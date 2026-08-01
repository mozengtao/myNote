/*
 * Generic Timer Library - Time Wheel Implementation
 * Based on DPL timer design for high-performance network and real-time systems
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Configuration constants
 */
#define TIMER_WHEEL_SIZE    8192        /* Number of wheel slots */
#define TIMER_WHEEL_TICK    1000000     /* Microseconds per slot (1 second) */

/*
 * Return codes
 */
enum {
    TIMER_OK = 0,
    TIMER_ERROR_INVALID = -1,
    TIMER_ERROR_NOMEM = -2,
    TIMER_ERROR_NULL_POINTER = -3
};

/*
 * Type definitions
 */
typedef int64_t timer_time_t;           /* Time in microseconds */

/* Forward declarations */
struct timer;
struct timer_context;

/*
 * Timer callback function type
 * @param timer: The expired timer
 * @param user_data: User-provided data
 */
typedef void (*timer_callback_t)(struct timer *timer, void *user_data);

/*
 * Timer object
 */
typedef struct timer {
    timer_time_t when;                  /* Absolute expiration time (microseconds) */
    timer_time_t interval;              /* Timer interval in microseconds */
    struct timer *next;                 /* Next timer in wheel slot */
    int wheel_slot;                     /* Current wheel slot (-1 if not active) */
    const char *name;                   /* Timer name for debugging */
    timer_callback_t callback;          /* Expiration callback function */
    void *user_data;                    /* User data passed to callback */
    int timer_id;                       /* User-defined timer ID */
} gtimer_t;

/*
 * Timer wheel context
 */
typedef struct timer_context {
    int current_slot;                   /* Current wheel position */
    timer_time_t base_time;             /* Time base at current slot */
    gtimer_t *wheel[TIMER_WHEEL_SIZE];  /* Timer wheel slots */
} timer_context_t;

/*
 * Context Management Functions
 */

/**
 * Create a timer wheel context
 * @return Pointer to timer context, or NULL on failure
 */
timer_context_t *timer_context_create(void);

/**
 * Destroy timer context and all associated timers
 * @param ctx: Timer context to destroy
 */
void timer_context_destroy(timer_context_t *ctx);

/*
 * Timer Lifecycle Functions
 */

/**
 * Create a timer object
 * @param name: Timer name for debugging (can be NULL)
 * @param callback: Function to call when timer expires
 * @param user_data: User data to pass to callback
 * @param timer_id: User-defined timer identifier
 * @return Pointer to timer object, or NULL on failure
 */
gtimer_t *timer_create(const char *name, timer_callback_t callback, 
                       void *user_data, int timer_id);

/**
 * Start a timer with specified interval
 * @param ctx: Timer context
 * @param timer: Timer object to start
 * @param interval_us: Timer interval in microseconds
 * @return TIMER_OK on success, negative error code on failure
 */
int timer_start(timer_context_t *ctx, gtimer_t *timer, timer_time_t interval_us);

/**
 * Stop a running timer
 * @param ctx: Timer context
 * @param timer: Timer object to stop
 * @return TIMER_OK on success, negative error code on failure
 */
int timer_stop(timer_context_t *ctx, gtimer_t *timer);

/**
 * Restart an existing timer (stop and start with current interval)
 * @param ctx: Timer context
 * @param timer: Timer object to restart
 * @return TIMER_OK on success, negative error code on failure
 */
int timer_restart(timer_context_t *ctx, gtimer_t *timer);

/**
 * Delete timer object and free memory
 * @param timer: Timer object to delete
 */
void timer_delete(gtimer_t *timer);

/*
 * Timer Execution Functions
 */

/**
 * Execute all expired timers
 * Should be called regularly from the main event loop
 * @param ctx: Timer context
 * @return TIMER_OK on success, negative error code on failure
 */
int timer_exec(timer_context_t *ctx);

/**
 * Get time until next timer expiration
 * Useful for select() and poll() timeout values
 * @param ctx: Timer context
 * @return Microseconds until next expiration, or -1 if no active timers
 */
timer_time_t timer_get_next_timeout(timer_context_t *ctx);

/*
 * Utility Functions
 */

/**
 * Get current system time in microseconds
 * @return Current time in microseconds since epoch
 */
timer_time_t timer_get_time(void);

/**
 * Compare two timer times (handles wraparound)
 * @param a: First time
 * @param b: Second time  
 * @return Positive if a > b, negative if a < b, zero if equal
 */
static inline int64_t timer_time_cmp(timer_time_t a, timer_time_t b)
{
    return a - b;
}

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H */