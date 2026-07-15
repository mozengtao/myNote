/*
 * shm_ring.c
 *
 * Implementation of the bounded, blocking MPMC shared-memory ring buffer
 * declared in shm_ring.h. See docs/SHM_RING_BUFFER.md for the design.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <shm_ring.h>

/* Number of times shmring_create() retries attaching after losing the
 * shm_open(O_CREAT|O_EXCL) race, and the delay between retries. This only
 * matters for the brief window between the winning creator's shm_open()
 * and the moment it finishes ftruncate()+init_shared_header(). */
#define SHMRING_CREATE_RACE_RETRIES 100
#define SHMRING_CREATE_RACE_DELAY_US 10000U /* 10ms */

static uint32_t
align_up8(uint32_t n)
{
    return (n + 7u) & ~7u;
}

static inline shmring_slot_t *
shmring_slot_at(shmring_hdr_t *hdr, uint32_t idx)
{
    return (shmring_slot_t *)(hdr->slots + (size_t)idx * hdr->slot_stride);
}

/*
 * Lock the ring's mutex, transparently recovering from a previous holder
 * having died while the lock was held (EOWNERDEAD). Our critical sections
 * only ever touch plain counters/indices in a way that leaves them in a
 * valid state even if a producer/consumer was killed mid-update, so simply
 * marking the mutex consistent and continuing is safe here.
 */
static int
lock_ring(shmring_hdr_t *hdr)
{
    int rc = pthread_mutex_lock(&hdr->lock);

    if (rc == EOWNERDEAD) {
        (void)pthread_mutex_consistent(&hdr->lock);
        return 0;
    }
    return rc;
}

static int
init_shared_header(shmring_hdr_t *hdr, uint32_t capacity, uint32_t max_payload)
{
    pthread_mutexattr_t mattr;
    pthread_condattr_t cattr;
    int rc;

    /* Zero everything first; the magic is published last (with a release
     * store) so that any process observing magic == SHMRING_MAGIC via an
     * acquire load is guaranteed to see every field written below it. */
    memset(hdr, 0, sizeof(*hdr));
    hdr->capacity = capacity;
    hdr->max_payload = max_payload;
    hdr->slot_stride = align_up8((uint32_t)sizeof(shmring_slot_t) + max_payload);

    rc = pthread_mutexattr_init(&mattr);
    if (rc != 0)
        return rc;
    rc = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    if (rc == 0)
        rc = pthread_mutexattr_setrobust(&mattr, PTHREAD_MUTEX_ROBUST);
    if (rc == 0)
        rc = pthread_mutex_init(&hdr->lock, &mattr);
    (void)pthread_mutexattr_destroy(&mattr);
    if (rc != 0)
        return rc;

    rc = pthread_condattr_init(&cattr);
    if (rc != 0)
        return rc;
    rc = pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    if (rc == 0)
        rc = pthread_cond_init(&hdr->not_full, &cattr);
    if (rc == 0)
        rc = pthread_cond_init(&hdr->not_empty, &cattr);
    (void)pthread_condattr_destroy(&cattr);
    if (rc != 0)
        return rc;

    __atomic_store_n(&hdr->magic, SHMRING_MAGIC, __ATOMIC_RELEASE);
    return 0;
}

static int
create_and_init(int fd, const char *name, uint32_t capacity, uint32_t max_payload, shmring_t **out)
{
    uint32_t slot_stride = align_up8((uint32_t)sizeof(shmring_slot_t) + max_payload);
    size_t full_size = sizeof(shmring_hdr_t) + (size_t)capacity * (size_t)slot_stride;
    void *base;
    shmring_hdr_t *hdr;
    shmring_t *ring;

    if (ftruncate(fd, (off_t)full_size) != 0)
        return SHMRING_ERR_SYS;

    base = mmap(NULL, full_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED)
        return SHMRING_ERR_SYS;

    hdr = (shmring_hdr_t *)base;
    if (init_shared_header(hdr, capacity, max_payload) != 0) {
        (void)munmap(base, full_size);
        return SHMRING_ERR_SYS;
    }

    ring = (shmring_t *)calloc(1, sizeof(*ring));
    if (ring == NULL) {
        (void)munmap(base, full_size);
        return SHMRING_ERR_SYS;
    }
    ring->hdr = hdr;
    ring->map_size = full_size;
    (void)snprintf(ring->name, sizeof(ring->name), "%s", name);

    *out = ring;
    return SHMRING_OK;
}

/*
 * Single-attempt attach using the two-phase mmap technique: the final
 * mapping size depends on `capacity`/`slot_stride`, which are only known
 * once we can read the header, so we first map just the header, then
 * remap the whole segment once we know its real size.
 */
static int
attach_once(const char *name, shmring_t **out)
{
    int fd;
    struct stat st;
    void *hdr_map;
    shmring_hdr_t *tmp_hdr;
    uint32_t capacity, slot_stride;
    size_t full_size;
    void *full_map;
    shmring_t *ring;

    fd = shm_open(name, O_RDWR, 0660);
    if (fd < 0)
        return SHMRING_ERR_SYS;

    if (fstat(fd, &st) != 0 || (size_t)st.st_size < sizeof(shmring_hdr_t)) {
        (void)close(fd);
        errno = EAGAIN;
        return SHMRING_ERR_SYS;
    }

    hdr_map = mmap(NULL, sizeof(shmring_hdr_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (hdr_map == MAP_FAILED) {
        (void)close(fd);
        return SHMRING_ERR_SYS;
    }

    tmp_hdr = (shmring_hdr_t *)hdr_map;
    if (__atomic_load_n(&tmp_hdr->magic, __ATOMIC_ACQUIRE) != SHMRING_MAGIC) {
        (void)munmap(hdr_map, sizeof(shmring_hdr_t));
        (void)close(fd);
        errno = EAGAIN;
        return SHMRING_ERR_SYS;
    }

    capacity = tmp_hdr->capacity;
    slot_stride = tmp_hdr->slot_stride;
    (void)munmap(hdr_map, sizeof(shmring_hdr_t));

    full_size = sizeof(shmring_hdr_t) + (size_t)capacity * (size_t)slot_stride;
    if ((size_t)st.st_size < full_size) {
        (void)close(fd);
        errno = EAGAIN;
        return SHMRING_ERR_SYS;
    }

    full_map = mmap(NULL, full_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    (void)close(fd);
    if (full_map == MAP_FAILED)
        return SHMRING_ERR_SYS;

    ring = (shmring_t *)calloc(1, sizeof(*ring));
    if (ring == NULL) {
        (void)munmap(full_map, full_size);
        return SHMRING_ERR_SYS;
    }
    ring->hdr = (shmring_hdr_t *)full_map;
    ring->map_size = full_size;
    (void)snprintf(ring->name, sizeof(ring->name), "%s", name);

    *out = ring;
    return SHMRING_OK;
}

int
shmring_create(const char *name, uint32_t capacity, uint32_t max_payload, shmring_t **out)
{
    int fd, rc;

    if (name == NULL || out == NULL || capacity < SHMRING_MIN_CAPACITY || max_payload == 0)
        return SHMRING_ERR_INVAL;
    *out = NULL;

    fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0660);
    if (fd >= 0) {
        rc = create_and_init(fd, name, capacity, max_payload, out);
        (void)close(fd);
        if (rc != SHMRING_OK)
            (void)shm_unlink(name);
        return rc;
    }

    if (errno != EEXIST)
        return SHMRING_ERR_SYS;

    /* Another writer won the race to create the object. It may still be
     * between shm_open() and finishing ftruncate()+init, so retry attach
     * briefly instead of failing immediately. */
    for (int attempt = 0; attempt < SHMRING_CREATE_RACE_RETRIES; attempt++) {
        rc = attach_once(name, out);
        if (rc == SHMRING_OK)
            return SHMRING_OK;
        (void)usleep(SHMRING_CREATE_RACE_DELAY_US);
    }
    return rc;
}

int
shmring_attach(const char *name, shmring_t **out)
{
    if (name == NULL || out == NULL)
        return SHMRING_ERR_INVAL;
    *out = NULL;
    return attach_once(name, out);
}

void
shmring_close(shmring_t *ring)
{
    if (ring == NULL)
        return;
    if (ring->hdr != NULL)
        (void)munmap(ring->hdr, ring->map_size);
    free(ring);
}

int
shmring_destroy(const char *name)
{
    if (name == NULL)
        return SHMRING_ERR_INVAL;
    if (shm_unlink(name) != 0)
        return SHMRING_ERR_SYS;
    return SHMRING_OK;
}

int
shmring_push(shmring_t *ring, const void *data, uint32_t len, uint64_t *out_seq, int block)
{
    shmring_hdr_t *hdr;
    int rc = SHMRING_OK;

    if (ring == NULL || ring->hdr == NULL || (len != 0 && data == NULL))
        return SHMRING_ERR_INVAL;

    hdr = ring->hdr;
    if (len > hdr->max_payload)
        return SHMRING_ERR_TOOBIG;

    if (lock_ring(hdr) != 0)
        return SHMRING_ERR_SYS;

    while (hdr->count == hdr->capacity && !hdr->closed) {
        if (!block) {
            rc = SHMRING_ERR_FULL;
            goto out;
        }
        pthread_cond_wait(&hdr->not_full, &hdr->lock);
    }
    if (hdr->closed) {
        rc = SHMRING_ERR_CLOSED;
        goto out;
    }

    {
        shmring_slot_t *slot = shmring_slot_at(hdr, hdr->tail);

        slot->seq = hdr->next_seq++;
        (void)clock_gettime(CLOCK_REALTIME, &slot->ts);
        slot->len = len;
        if (len != 0)
            memcpy(slot->payload, data, len);

        hdr->tail = (hdr->tail + 1) % hdr->capacity;
        hdr->count++;
        hdr->total_pushed++;
        if (out_seq != NULL)
            *out_seq = slot->seq;
    }
    pthread_cond_signal(&hdr->not_empty);

out:
    (void)pthread_mutex_unlock(&hdr->lock);
    return rc;
}

int
shmring_pop(shmring_t *ring, void *buf, uint32_t buf_cap, uint32_t *out_len, uint64_t *out_seq,
            struct timespec *out_ts, int block)
{
    shmring_hdr_t *hdr;
    int rc = SHMRING_OK;

    if (ring == NULL || ring->hdr == NULL || (buf_cap != 0 && buf == NULL))
        return SHMRING_ERR_INVAL;

    hdr = ring->hdr;

    if (lock_ring(hdr) != 0)
        return SHMRING_ERR_SYS;

    while (hdr->count == 0 && !hdr->closed) {
        if (!block) {
            rc = SHMRING_ERR_EMPTY;
            goto out;
        }
        pthread_cond_wait(&hdr->not_empty, &hdr->lock);
    }
    if (hdr->count == 0 && hdr->closed) {
        rc = SHMRING_ERR_CLOSED;
        goto out;
    }

    {
        shmring_slot_t *slot = shmring_slot_at(hdr, hdr->head);
        uint32_t copy_len = slot->len;

        if (copy_len > buf_cap)
            copy_len = buf_cap;
        if (copy_len != 0)
            memcpy(buf, slot->payload, copy_len);
        if (out_len != NULL)
            *out_len = slot->len;
        if (out_seq != NULL)
            *out_seq = slot->seq;
        if (out_ts != NULL)
            *out_ts = slot->ts;

        hdr->head = (hdr->head + 1) % hdr->capacity;
        hdr->count--;
        hdr->total_popped++;
    }
    pthread_cond_signal(&hdr->not_full);

out:
    (void)pthread_mutex_unlock(&hdr->lock);
    return rc;
}

void
shmring_shutdown(shmring_t *ring)
{
    shmring_hdr_t *hdr;

    if (ring == NULL || ring->hdr == NULL)
        return;
    hdr = ring->hdr;

    if (lock_ring(hdr) != 0)
        return;
    hdr->closed = 1;
    pthread_cond_broadcast(&hdr->not_full);
    pthread_cond_broadcast(&hdr->not_empty);
    (void)pthread_mutex_unlock(&hdr->lock);
}

uint32_t
shmring_count(shmring_t *ring)
{
    if (ring == NULL || ring->hdr == NULL)
        return 0;
    return __atomic_load_n(&ring->hdr->count, __ATOMIC_RELAXED);
}

void
shmring_stats(shmring_t *ring, uint64_t *out_total_pushed, uint64_t *out_total_popped)
{
    if (ring == NULL || ring->hdr == NULL)
        return;
    if (out_total_pushed != NULL)
        *out_total_pushed = __atomic_load_n(&ring->hdr->total_pushed, __ATOMIC_RELAXED);
    if (out_total_popped != NULL)
        *out_total_popped = __atomic_load_n(&ring->hdr->total_popped, __ATOMIC_RELAXED);
}
