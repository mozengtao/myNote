/*
 * reader_main.c
 *
 * Demo consumer for the shm_ring library. Attaches to a shared-memory ring
 * buffer created by a writer process (retrying until it appears), pops
 * messages, and prints their content plus end-to-end latency.
 *
 * Usage:
 *   shm_reader <name> [count]
 */

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    fprintf(stderr, "Usage: %s <name> [count]\n", prog);
    fprintf(stderr, "  name   POSIX shared memory object name (must match the writer)\n");
    fprintf(stderr, "  count  number of messages to pop (0 or omitted = run until Ctrl-C/closed)\n");
}

static double
ts_diff_ms(const struct timespec *from, const struct timespec *to)
{
    double ms = (double)(to->tv_sec - from->tv_sec) * 1000.0;

    ms += (double)(to->tv_nsec - from->tv_nsec) / 1.0e6;
    return ms;
}

int
main(int argc, char **argv)
{
    const char *name;
    uint64_t count, received = 0;
    struct sigaction sa;
    shmring_t *ring = NULL;
    uint8_t buf[4096];
    int rc;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    name = argv[1];
    count = (argc > 2) ? strtoull(argv[2], NULL, 10) : 0;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    (void)sigaction(SIGINT, &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);

    printf("[reader pid=%d] waiting for ring '%s'...\n", (int)getpid(), name);
    for (;;) {
        rc = shmring_attach(name, &ring);
        if (rc == SHMRING_OK)
            break;
        if (g_stop) {
            printf("[reader] interrupted while waiting for ring to appear\n");
            return 1;
        }
        (void)usleep(200000); /* 200ms: the writer hasn't created the ring yet */
    }
    printf("[reader] attached to '%s'\n", name);

    while (!g_stop && (count == 0 || received < count)) {
        uint32_t len = 0;
        uint64_t seq = 0;
        struct timespec produced_ts, now;

        rc = shmring_pop(ring, buf, sizeof(buf) - 1, &len, &seq, &produced_ts, /*block=*/1);
        if (rc == SHMRING_ERR_CLOSED) {
            printf("[reader] ring was shut down by writer, stopping\n");
            break;
        }
        if (rc != SHMRING_OK) {
            fprintf(stderr, "[reader] pop failed: rc=%d\n", rc);
            break;
        }

        if (len >= sizeof(buf))
            len = (uint32_t)sizeof(buf) - 1;
        buf[len] = '\0';

        (void)clock_gettime(CLOCK_REALTIME, &now);
        printf("[reader] popped seq=%" PRIu64 " len=%u \"%s\" latency=%.3fms (ring_count=%u)\n", seq, len, (char *)buf,
               ts_diff_ms(&produced_ts, &now), shmring_count(ring));
        received++;
    }

    {
        uint64_t total_pushed, total_popped;

        shmring_stats(ring, &total_pushed, &total_popped);
        printf("[reader] exiting. total_pushed(all writers)=%" PRIu64 " total_popped=%" PRIu64 "\n", total_pushed, total_popped);
    }

    /* Only unmap our local view; the shared memory object itself is left
     * alone so other readers/writers can keep using it. */
    shmring_close(ring);
    return 0;
}
