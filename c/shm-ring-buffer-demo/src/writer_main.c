/*
 * writer_main.c
 *
 * Demo producer for the shm_ring library. Creates (or attaches to, if
 * another writer already created it) a shared-memory ring buffer and
 * pushes text messages into it at a configurable rate.
 *
 * Usage:
 *   shm_writer <name> <capacity> <max_payload> <count> [interval_ms]
 */

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <shm_ring.h>

static volatile sig_atomic_t g_stop = 0;

static void
on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <name> <capacity> <max_payload> <count> [interval_ms]\n", prog);
    fprintf(stderr, "  name         POSIX shared memory object name, e.g. /demo_ring\n");
    fprintf(stderr, "  capacity     number of slots in the ring\n");
    fprintf(stderr, "  max_payload  max bytes per message\n");
    fprintf(stderr, "  count        number of messages to push (0 = run until Ctrl-C)\n");
    fprintf(stderr, "  interval_ms  delay between pushes in milliseconds (default 200)\n");
}

int
main(int argc, char **argv)
{
    const char *name;
    uint32_t capacity, max_payload;
    uint64_t count, sent = 0;
    long interval_ms;
    struct sigaction sa;
    shmring_t *ring = NULL;
    char msg[256];
    int rc;

    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }
    name = argv[1];
    capacity = (uint32_t)strtoul(argv[2], NULL, 10);
    max_payload = (uint32_t)strtoul(argv[3], NULL, 10);
    count = strtoull(argv[4], NULL, 10);
    interval_ms = (argc > 5) ? strtol(argv[5], NULL, 10) : 200;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);

    rc = shmring_create(name, capacity, max_payload, &ring);
    if (rc != SHMRING_OK) {
        fprintf(stderr, "shmring_create(%s) failed: rc=%d errno=%s\n", name, rc, strerror(errno));
        return 1;
    }
    printf("[writer pid=%d] ring '%s' ready (capacity=%u, max_payload=%u)\n", (int)getpid(), name, capacity, max_payload);

    /*
     * NOTE: if the ring is full, shmring_push(block=1) blocks inside
     * pthread_cond_wait(), which is not woken up merely by SIGINT being
     * delivered. g_stop is only checked between pushes, so a writer stuck
     * on a full ring may need one item consumed before it notices Ctrl-C.
     * See docs/SHM_RING_BUFFER.md "常见陷阱" for the full explanation.
     */
    while (!g_stop && (count == 0 || sent < count)) {
        int len = snprintf(msg, sizeof(msg), "hello #%" PRIu64 " from pid %d", sent, (int)getpid());

        if (len < 0)
            break;

        uint64_t seq = 0;

        rc = shmring_push(ring, msg, (uint32_t)len, &seq, /*block=*/1);
        if (rc == SHMRING_ERR_CLOSED) {
            printf("[writer] ring was shut down, stopping\n");
            break;
        }
        if (rc != SHMRING_OK) {
            fprintf(stderr, "[writer] push failed: rc=%d\n", rc);
            break;
        }

        /* `seq` is the ring's shared, globally monotonic sequence number
         * (also seen by the reader); it is NOT the same as `sent`, which
         * is just this process's own local send count -- the two only
         * coincide when there is exactly one writer. */
        printf("[writer] pushed seq=%" PRIu64 " len=%d \"%s\" (ring_count=%u)\n", seq, len, msg, shmring_count(ring));
        sent++;

        if (interval_ms > 0)
            (void)usleep((useconds_t)interval_ms * 1000);
    }

    {
        uint64_t total_pushed, total_popped;

        shmring_stats(ring, &total_pushed, &total_popped);
        printf("[writer] stopping. total_pushed=%" PRIu64 " total_popped(all readers)=%" PRIu64 "\n", total_pushed, total_popped);
    }

    /* Wake any readers currently blocked in shmring_pop() so they don't
     * hang forever once we go away. Does not remove the shm object --
     * other readers/writers may still want to attach to it. */
    shmring_shutdown(ring);
    shmring_close(ring);
    return 0;
}
