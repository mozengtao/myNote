/*
 * Timer Library Example / Test Program
 *
 * This file demonstrates how to use the timer wheel library.
 *
 * Compilation:
 *   gcc -o timer_test main.c timer.c -lrt
 *
 * The -lrt flag links the real-time library needed for clock_gettime().
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include "timer.h"

/*
 * Global timer context
 * In a real application, this might be part of a larger application context.
 */
static tmr_ctx_t g_tmr_ctx;

/*
 * Counter to track how many times each timer has fired
 */
static int heartbeat_count = 0;
static int oneshot_count = 0;
static int fast_count = 0;

/*
 * Example 1: Repeating timer callback (heartbeat)
 *
 * This callback is called every 1 second. It demonstrates a typical
 * periodic timer pattern where the timer restarts itself.
 */
static void
heartbeat_callback(tmr_t *t, void *opaque, int id)
{
    tmr_ctx_t *ctx = (tmr_ctx_t *)opaque;

    heartbeat_count++;
    printf("[HEARTBEAT] Timer '%s' (id=%d) fired! count=%d\n",
           t->name, id, heartbeat_count);

    /*
     * Restart the timer to make it repeat.
     * This is a common pattern for periodic timers.
     */
    tmr_restart(ctx, t);

    /*
     * Stop after 5 heartbeats to end the demo
     */
    if (heartbeat_count >= 5) {
        printf("[HEARTBEAT] Stopping after %d beats\n", heartbeat_count);
        tmr_stop(ctx, t);
    }
}

/*
 * Example 2: One-shot timer callback
 *
 * This callback fires once and does NOT restart the timer.
 * Useful for delayed actions, timeouts, etc.
 */
static void
oneshot_callback(tmr_t *t, void *opaque, int id)
{
    (void)opaque;  /* Unused in this example */

    oneshot_count++;
    printf("[ONESHOT] Timer '%s' (id=%d) fired! This only happens once.\n",
           t->name, id);

    /*
     * Note: We do NOT call tmr_restart() here.
     * The timer will not fire again unless explicitly restarted.
     */
}

/*
 * Example 3: Fast timer callback (100ms interval)
 *
 * This demonstrates a faster timer. We'll stop it after a few iterations.
 */
static void
fast_callback(tmr_t *t, void *opaque, int id)
{
    tmr_ctx_t *ctx = (tmr_ctx_t *)opaque;

    fast_count++;
    printf("[FAST] Timer '%s' (id=%d) fired! count=%d\n",
           t->name, id, fast_count);

    /*
     * Stop after 10 fast ticks
     */
    if (fast_count < 10) {
        tmr_restart(ctx, t);
    } else {
        printf("[FAST] Stopping fast timer after %d ticks\n", fast_count);
    }
}

/*
 * Example 4: Timer with custom data
 *
 * This demonstrates passing custom data through the opaque pointer.
 */
typedef struct {
    const char *message;
    int         max_count;
    int         current_count;
} custom_data_t;

static void
custom_callback(tmr_t *t, void *opaque, int id)
{
    custom_data_t *data = (custom_data_t *)opaque;

    data->current_count++;
    printf("[CUSTOM] Timer '%s' (id=%d): %s (count=%d/%d)\n",
           t->name, id, data->message, data->current_count, data->max_count);

    /*
     * Note: We need the timer context to restart.
     * In this example, we use the global context.
     * A better design might include the context in custom_data_t.
     */
    if (data->current_count < data->max_count) {
        tmr_restart(&g_tmr_ctx, t);
    } else {
        printf("[CUSTOM] Custom timer completed\n");
    }
}

/*
 * Main function - demonstrates timer library usage
 */
int
main(int argc, char *argv[])
{
    tmr_t *heartbeat_timer;
    tmr_t *oneshot_timer;
    tmr_t *fast_timer;
    tmr_t *custom_timer;
    custom_data_t custom_data;
    int timeout_ms;
    int running;

    (void)argc;
    (void)argv;

    printf("=== Timer Wheel Library Demo ===\n\n");

    /*
     * Step 1: Initialize the timer context
     *
     * This sets up the timer wheel data structure.
     * You typically do this once at application startup.
     */
    printf("Initializing timer context...\n");
    if (tmr_ctx_init(&g_tmr_ctx) != TMR_OK) {
        fprintf(stderr, "Failed to initialize timer context\n");
        return 1;
    }

    /*
     * Step 2: Create timers
     *
     * tmr_create() allocates and optionally starts a timer.
     * Parameters:
     *   - ctx:      Timer context
     *   - tp:       Output pointer to the created timer
     *   - name:     Human-readable name (for debugging)
     *   - interval: Interval in microseconds (0 = create but don't start)
     *   - callback: Function to call when timer expires
     *   - opaque:   User data passed to callback
     *   - id:       User-defined ID passed to callback
     */

    /* Timer 1: Heartbeat timer - fires every 1 second */
    printf("Creating heartbeat timer (1 second interval)...\n");
    if (tmr_create(&g_tmr_ctx, &heartbeat_timer, "heartbeat",
                   1000000,            /* 1 second = 1,000,000 microseconds */
                   heartbeat_callback,
                   &g_tmr_ctx,         /* Pass context so callback can restart */
                   1) != TMR_OK) {
        fprintf(stderr, "Failed to create heartbeat timer\n");
        return 1;
    }

    /* Timer 2: One-shot timer - fires once after 2.5 seconds */
    printf("Creating oneshot timer (2.5 second delay)...\n");
    if (tmr_create(&g_tmr_ctx, &oneshot_timer, "oneshot",
                   2500000,            /* 2.5 seconds */
                   oneshot_callback,
                   NULL,               /* No opaque data needed */
                   2) != TMR_OK) {
        fprintf(stderr, "Failed to create oneshot timer\n");
        return 1;
    }

    /* Timer 3: Fast timer - fires every 100ms */
    printf("Creating fast timer (100ms interval)...\n");
    if (tmr_create(&g_tmr_ctx, &fast_timer, "fast",
                   100000,             /* 100ms = 100,000 microseconds */
                   fast_callback,
                   &g_tmr_ctx,
                   3) != TMR_OK) {
        fprintf(stderr, "Failed to create fast timer\n");
        return 1;
    }

    /* Timer 4: Custom data timer - demonstrates passing custom data */
    printf("Creating custom data timer (500ms interval)...\n");
    custom_data.message = "Hello from custom timer!";
    custom_data.max_count = 3;
    custom_data.current_count = 0;

    if (tmr_create(&g_tmr_ctx, &custom_timer, "custom",
                   500000,             /* 500ms */
                   custom_callback,
                   &custom_data,       /* Pass custom data structure */
                   4) != TMR_OK) {
        fprintf(stderr, "Failed to create custom timer\n");
        return 1;
    }

    /*
     * Step 3: Dump timer state (for debugging)
     */
    printf("\n--- Initial timer state ---\n");
    tmr_dump(&g_tmr_ctx);
    printf("---------------------------\n\n");

    /*
     * Step 4: Event loop
     *
     * This is the main application loop. In a real application,
     * you would typically use poll() or select() to wait for:
     *   - Timer events
     *   - Network I/O
     *   - User input
     *   - etc.
     *
     * tmr_poll_timeout() returns the time (in ms) until the next
     * timer expires, which can be used as the poll() timeout.
     */
    printf("Starting event loop...\n\n");
    running = 1;

    while (running) {
        /*
         * Get timeout for poll()
         *
         * This tells us how long we can sleep before the next timer fires.
         * Using this as the poll() timeout ensures we wake up in time.
         */
        timeout_ms = tmr_poll_timeout(&g_tmr_ctx);

        /*
         * Wait for events or timeout
         *
         * In a real application, you would add file descriptors to poll().
         * Here we just use it as a sleep mechanism.
         */
        poll(NULL, 0, timeout_ms);

        /*
         * Process expired timers
         *
         * This checks all timers and fires callbacks for any that have expired.
         * IMPORTANT: Must be called regularly for timers to work!
         */
        tmr_exec(&g_tmr_ctx);

        /*
         * Check if we should exit
         *
         * For this demo, we exit when heartbeat timer has stopped.
         */
        if (heartbeat_count >= 5 && fast_count >= 10) {
            printf("\nAll timers completed. Exiting event loop.\n");
            running = 0;
        }
    }

    /*
     * Step 5: Cleanup
     *
     * Delete individual timers (optional if shutting down context)
     */
    printf("\nCleaning up timers...\n");
    tmr_delete(&g_tmr_ctx, heartbeat_timer);
    tmr_delete(&g_tmr_ctx, oneshot_timer);
    tmr_delete(&g_tmr_ctx, fast_timer);
    tmr_delete(&g_tmr_ctx, custom_timer);

    /*
     * Shutdown timer context
     *
     * This frees any remaining timers and cleans up.
     */
    tmr_ctx_shutdown(&g_tmr_ctx);

    printf("\n=== Demo Complete ===\n");
    printf("Summary:\n");
    printf("  Heartbeat fired: %d times\n", heartbeat_count);
    printf("  Oneshot fired:   %d times\n", oneshot_count);
    printf("  Fast fired:      %d times\n", fast_count);
    printf("  Custom fired:    %d times\n", custom_data.current_count);

    return 0;
}

/*
 * Alternative event loop using select() instead of poll()
 *
 * This is commented out but shows how to use tmr_select_timeout().
 */
#if 0
void
event_loop_with_select(void)
{
    fd_set readfds;
    struct timeval tv;
    int ret;

    while (1) {
        /* Get timeout for select() */
        tmr_select_timeout(&g_tmr_ctx, &tv);

        /* Set up file descriptors to watch (empty in this example) */
        FD_ZERO(&readfds);

        /* Wait for events or timeout */
        ret = select(0, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select");
            break;
        }

        /* Process expired timers */
        tmr_exec(&g_tmr_ctx);
    }
}
#endif
