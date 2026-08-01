/*
 * Generic Timer Library Demonstration
 * Shows typical usage patterns for network protocols and real-time systems
 */

#define _GNU_SOURCE  /* For usleep() and other functions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>

#include "timer.h"

/* Global state for demo */
static timer_context_t *g_timer_ctx = NULL;
static int g_running = 1;

/*
 * Timer callback functions for different scenarios
 */

/* Heartbeat timer - typical keep-alive mechanism */
void heartbeat_callback(gtimer_t *timer, void *user_data)
{
    int *counter = (int *)user_data;

    printf("[%ld] Heartbeat #%d - sending keep-alive packet\n",
           timer_get_time() / 1000000, (*counter)++);

    /* Restart for next heartbeat */
    timer_restart(g_timer_ctx, timer);
}

/* Connection timeout - simulates network protocol timeout */
void connection_timeout_callback(gtimer_t *timer, void *user_data)
{
    const char *connection_name = (const char *)user_data;
    (void)timer;  /* Suppress unused parameter warning */

    printf("[%ld] Connection timeout: %s - closing connection\n",
           timer_get_time() / 1000000, connection_name);

    /* In real application, would close the connection here */
}

/* Retry timer - exponential backoff pattern */
void retry_callback(gtimer_t *timer, void *user_data)
{
    static int retry_count = 0;
    (void)user_data;  /* Suppress unused parameter warning */

    retry_count++;
    printf("[%ld] Retry attempt #%d - retransmitting packet\n",
           timer_get_time() / 1000000, retry_count);

    if (retry_count < 5) {
        /* Exponential backoff: double the interval each time */
        timer_time_t new_interval = timer->interval * 2;
        timer->interval = new_interval;
        timer_restart(g_timer_ctx, timer);
        printf("    Next retry in %ld ms\n", new_interval / 1000);
    } else {
        printf("    Max retries reached, giving up\n");
    }
}

/* Periodic maintenance timer */
void maintenance_callback(gtimer_t *timer, void *user_data)
{
    static int maintenance_count = 0;
    (void)user_data;  /* Suppress unused parameter warning */

    maintenance_count++;
    printf("[%ld] Maintenance task #%d - cleaning up resources\n",
           timer_get_time() / 1000000, maintenance_count);

    /* Restart for next maintenance cycle */
    timer_restart(g_timer_ctx, timer);
}

/* One-shot timer for delayed operations */
void delayed_operation_callback(gtimer_t *timer, void *user_data)
{
    const char *operation = (const char *)user_data;
    (void)timer;  /* Suppress unused parameter warning */

    printf("[%ld] Executing delayed operation: %s\n",
           timer_get_time() / 1000000, operation);
}

/*
 * Signal handler for graceful shutdown
 */
void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down...\n", sig);
    g_running = 0;
}

/*
 * Simulate network event processing with select()
 */
void run_event_loop(void)
{
    fd_set readfds;
    struct timeval timeout;
    int max_fd = 0;  /* No real FDs in this demo */
    int result;

    printf("Starting event loop with timer integration...\n");
    printf("Press Ctrl+C to exit\n\n");

    while (g_running) {
        /* Prepare for select() */
        FD_ZERO(&readfds);

        /* Get timeout from timer system */
        timer_time_t next_timeout = timer_get_next_timeout(g_timer_ctx);

        if (next_timeout < 0) {
            /* No active timers, use default timeout */
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
        } else {
            /* Convert microseconds to timeval */
            timeout.tv_sec = next_timeout / 1000000;
            timeout.tv_usec = next_timeout % 1000000;

            /* Ensure minimum timeout to prevent spinning */
            if (timeout.tv_sec == 0 && timeout.tv_usec < 1000) {
                timeout.tv_usec = 1000;  /* 1ms minimum */
            }
        }

        /* Wait for events or timeout */
        result = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (result < 0) {
            if (errno == EINTR) {
                continue;  /* Interrupted by signal */
            }
            perror("select");
            break;
        }

        /* Execute expired timers */
        timer_exec(g_timer_ctx);

        /* In a real application, would process network events here */
        if (result > 0) {
            printf("Network events would be processed here\n");
        }
    }
}

/*
 * Demo: Multiple timer types working together
 */
void demo_multiple_timers(void)
{
    gtimer_t *heartbeat_timer;
    gtimer_t *connection_timer1, *connection_timer2;
    gtimer_t *retry_timer;
    gtimer_t *maintenance_timer;
    gtimer_t *delayed_timer;

    static int heartbeat_counter = 1;

    printf("=== Multiple Timer Types Demo ===\n");

    /* Create various types of timers */

    /* 1. Heartbeat timer - every 3 seconds */
    heartbeat_timer = timer_create("heartbeat", heartbeat_callback,
                                   &heartbeat_counter, 1);
    if (heartbeat_timer) {
        timer_start(g_timer_ctx, heartbeat_timer, 3 * 1000000);  /* 3 seconds */
        printf("Started heartbeat timer (3s interval)\n");
    }

    /* 2. Connection timeout timers - different timeouts */
    connection_timer1 = timer_create("conn_timeout", connection_timeout_callback,
                                     (void *)"Client-A", 2);
    if (connection_timer1) {
        timer_start(g_timer_ctx, connection_timer1, 15 * 1000000);  /* 15 seconds */
        printf("Started connection timeout for Client-A (15s)\n");
    }

    connection_timer2 = timer_create("conn_timeout", connection_timeout_callback,
                                     (void *)"Client-B", 3);
    if (connection_timer2) {
        timer_start(g_timer_ctx, connection_timer2, 25 * 1000000);  /* 25 seconds */
        printf("Started connection timeout for Client-B (25s)\n");
    }

    /* 3. Retry timer with exponential backoff - starts at 1 second */
    retry_timer = timer_create("retry", retry_callback, NULL, 4);
    if (retry_timer) {
        timer_start(g_timer_ctx, retry_timer, 1 * 1000000);  /* 1 second */
        printf("Started retry timer (1s initial, exponential backoff)\n");
    }

    /* 4. Maintenance timer - every 10 seconds */
    maintenance_timer = timer_create("maintenance", maintenance_callback, NULL, 5);
    if (maintenance_timer) {
        timer_start(g_timer_ctx, maintenance_timer, 10 * 1000000);  /* 10 seconds */
        printf("Started maintenance timer (10s interval)\n");
    }

    /* 5. One-shot delayed operation - 7 seconds */
    delayed_timer = timer_create("delayed_op", delayed_operation_callback,
                                (void *)"Database backup", 6);
    if (delayed_timer) {
        timer_start(g_timer_ctx, delayed_timer, 7 * 1000000);  /* 7 seconds */
        printf("Scheduled delayed operation (7s delay)\n");
    }

    printf("\nRunning timers for demonstration...\n\n");

    /* Run the event loop */
    run_event_loop();

    /* Cleanup (timers will be freed by context destroy) */
    printf("\nDemo completed.\n");
}

/*
 * Demo: Timer start/stop operations
 */
void demo_timer_operations(void)
{
    gtimer_t *test_timer;
    int i;

    printf("\n=== Timer Operations Demo ===\n");

    test_timer = timer_create("test", delayed_operation_callback,
                             (void *)"Test operation", 100);

    if (!test_timer) {
        printf("Failed to create test timer\n");
        return;
    }

    /* Demonstrate start/stop/restart */
    printf("1. Starting timer for 2 seconds...\n");
    timer_start(g_timer_ctx, test_timer, 2 * 1000000);

    sleep(1);
    printf("2. Stopping timer after 1 second...\n");
    timer_stop(g_timer_ctx, test_timer);

    printf("3. Restarting timer for 1 second...\n");
    timer_start(g_timer_ctx, test_timer, 1 * 1000000);

    /* Wait for timer to expire */
    for (i = 0; i < 15; i++) {
        timer_exec(g_timer_ctx);
        usleep(100000);  /* 100ms */
    }

    timer_delete(test_timer);
    printf("Timer operations demo completed.\n");
}

/*
 * Main function
 */
int main(int argc, char *argv[])
{
    (void)argc;  /* Suppress unused parameter warning */
    (void)argv;  /* Suppress unused parameter warning */

    printf("Generic Timer Library Demonstration\n");
    printf("===================================\n\n");

    /* Install signal handler for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Create timer context */
    g_timer_ctx = timer_context_create();
    if (!g_timer_ctx) {
        printf("Failed to create timer context\n");
        return EXIT_FAILURE;
    }

    printf("Timer context created successfully\n");

    /* Run demonstrations */
    demo_timer_operations();
    demo_multiple_timers();

    /* Cleanup */
    printf("\nCleaning up...\n");
    timer_context_destroy(g_timer_ctx);
    g_timer_ctx = NULL;

    printf("Timer library demonstration completed.\n");
    return EXIT_SUCCESS;
}