/*
 * shm_ring.h
 *
 * A bounded, blocking, multi-producer/multi-consumer (MPMC) ring buffer
 * built on top of POSIX shared memory (shm_open/mmap), a process-shared
 * mutex and two process-shared condition variables.
 *
 * This is a learning/reference implementation. See docs/SHM_RING_BUFFER.md
 * for the full design rationale, memory layout diagrams, and usage guide.
 */

#ifndef SHM_RING_H
#define SHM_RING_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHMRING_MAGIC        0x53484D52u /* "SHMR" */
#define SHMRING_NAME_MAX      64U
#define SHMRING_MIN_CAPACITY   1U

/* Return codes for shmring_push()/shmring_pop() and lifecycle calls. */
enum {
    SHMRING_OK = 0,
    SHMRING_ERR_INVAL = 1,  /* bad arguments */
    SHMRING_ERR_FULL = 2,   /* non-blocking push: ring is full */
    SHMRING_ERR_EMPTY = 3,  /* non-blocking pop: ring is empty */
    SHMRING_ERR_CLOSED = 4, /* ring was shut down while waiting */
    SHMRING_ERR_SYS = 5,    /* system call failure; see errno */
    SHMRING_ERR_TOOBIG = 6, /* payload larger than max_payload */
};

/*
 * One message slot as stored inside the shared memory region.
 *
 * Slots are NOT a fixed C array (their size depends on max_payload, which
 * is only known at ring-creation time), so shmring_hdr_t stores them as a
 * flat byte array; shm_ring.c indexes into it via an internal stride-based
 * helper (slot_stride bytes per slot). This header does not expose that
 * helper -- it is purely an implementation detail of the library.
 */
typedef struct {
    uint64_t seq;          /* monotonically increasing message number */
    struct timespec ts;    /* producer-side timestamp (CLOCK_REALTIME) */
    uint32_t len;           /* actual payload length in bytes */
    uint8_t payload[];      /* up to hdr->max_payload bytes */
} shmring_slot_t;

/*
 * Control block mapped at the start of the shared memory segment.
 *
 * Layout in shared memory:
 *
 *   +--------------------------------------------------+
 *   | shmring_hdr_t (fixed-size header fields)          |
 *   |  magic, capacity, max_payload, slot_stride         |
 *   |  lock, not_full, not_empty                         |
 *   |  head, tail, count, closed                         |
 *   |  next_seq, total_pushed, total_popped              |
 *   +--------------------------------------------------+
 *   | slots[0]  = shmring_slot_t (seq/ts/len/payload)    |
 *   | slots[1]  = shmring_slot_t                         |
 *   | ...                                                |
 *   | slots[capacity-1]                                  |
 *   +--------------------------------------------------+
 *
 * head/tail/count implement a classic bounded-buffer: producers block on
 * not_full while count == capacity, consumers block on not_empty while
 * count == 0. Both sides advance their index modulo capacity.
 */
typedef struct {
    uint32_t magic;
    uint32_t capacity;      /* number of slots, fixed at creation time */
    uint32_t max_payload;   /* max payload bytes per slot, fixed at creation */
    uint32_t slot_stride;   /* bytes per slot = align8(sizeof(shmring_slot_t) + max_payload) */

    pthread_mutex_t lock;    /* PTHREAD_PROCESS_SHARED + PTHREAD_MUTEX_ROBUST */
    pthread_cond_t not_full;  /* signaled by consumers, waited on by producers */
    pthread_cond_t not_empty; /* signaled by producers, waited on by consumers */

    uint32_t head;   /* index of next slot to pop */
    uint32_t tail;   /* index of next slot to push */
    uint32_t count;  /* number of occupied slots, 0..capacity */
    uint32_t closed; /* set by shmring_shutdown(); wakes all waiters */

    uint64_t next_seq;      /* next sequence number handed out by push() */
    uint64_t total_pushed;  /* lifetime counters, protected by lock */
    uint64_t total_popped;

    uint8_t slots[]; /* capacity * slot_stride bytes */
} shmring_hdr_t;

/* Process-local handle returned by shmring_create()/shmring_attach(). */
typedef struct {
    shmring_hdr_t *hdr;
    size_t map_size;
    char name[SHMRING_NAME_MAX];
} shmring_t;

/*
 * Create a new ring buffer named `name` with `capacity` slots, each able to
 * hold up to `max_payload` bytes of user data.
 *
 * If the shared memory object already exists (e.g. another writer process
 * created it first), this call transparently falls back to attaching to
 * the existing ring instead of failing -- multiple writer processes can be
 * started in any order. Returns SHMRING_OK on success.
 */
int shmring_create(const char *name, uint32_t capacity, uint32_t max_payload, shmring_t **out);

/*
 * Attach to an existing ring buffer named `name`. Fails with
 * SHMRING_ERR_SYS/ENOENT if no writer has created it yet.
 */
int shmring_attach(const char *name, shmring_t **out);

/* Unmap and free the local handle. Does not affect the shared memory object
 * itself, so other attached processes are unaffected. */
void shmring_close(shmring_t *ring);

/* Remove the underlying shared memory object (shm_unlink). Should be called
 * exactly once, by whichever process owns the ring's lifecycle, after all
 * other processes have stopped using it. */
int shmring_destroy(const char *name);

/*
 * Push one message into the ring. `out_seq` is optional (may be NULL) and
 * receives the message's ring-assigned sequence number -- useful since
 * with multiple writers, the caller's own send count is NOT the same as
 * the shared, globally monotonic sequence number stamped on the slot.
 *
 * If `block` is non-zero and the ring is full, the caller blocks until
 * space is available or the ring is shut down (SHMRING_ERR_CLOSED).
 * If `block` is zero, returns SHMRING_ERR_FULL immediately when full.
 */
int shmring_push(shmring_t *ring, const void *data, uint32_t len, uint64_t *out_seq, int block);

/*
 * Pop one message from the ring into `buf` (capacity `buf_cap` bytes).
 * `out_len`, `out_seq` and `out_ts` are optional (may be NULL): they receive
 * the message's actual length, sequence number, and producer-side
 * timestamp (useful for measuring end-to-end latency), respectively.
 *
 * Blocking semantics mirror shmring_push().
 */
int shmring_pop(shmring_t *ring, void *buf, uint32_t buf_cap, uint32_t *out_len, uint64_t *out_seq,
                 struct timespec *out_ts, int block);

/*
 * Mark the ring as closed and wake every thread/process currently blocked
 * in shmring_push()/shmring_pop(). Safe to call from a signal handler's
 * "please stop" flag pattern (i.e. call it from normal control flow after
 * observing a signal, not from inside the handler itself).
 */
void shmring_shutdown(shmring_t *ring);

/* Current number of occupied slots (best-effort snapshot). */
uint32_t shmring_count(shmring_t *ring);

/* Lifetime push/pop counters, for monitoring/demo purposes. */
void shmring_stats(shmring_t *ring, uint64_t *out_total_pushed, uint64_t *out_total_popped);

#ifdef __cplusplus
}
#endif

#endif /* SHM_RING_H */
