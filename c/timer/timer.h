/*
 * Standalone Timer Wheel Implementation
 *
 * This implements a classic "timer wheel" (also known as "hashed timing wheel")
 * algorithm for efficient timer management with O(1) start/stop operations.
 *
 * The timer wheel concept:
 * - Time is divided into "ticks" (TMR_WHEEL_TICK_US microseconds each)
 * - A circular array (wheel) of timer lists, one slot per tick
 * - Timers are inserted into the slot corresponding to their expiry time
 * - The wheel rotates as time advances, firing timers in each slot
 *
 *    Wheel (circular array of timer lists):
 *    ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 *    │  0  │  1  │  2  │  3  │  4  │  5  │ ... │ N-1 │
 *    └──┬──┴──┬──┴─────┴─────┴─────┴─────┴─────┴─────┘
 *       │     │
 *       │     └─▶ [timer_a] → [timer_b] → NULL
 *       │
 *       └─▶ [timer_x] → NULL
 *              ▲
 *              │
 *           current position (advances with time)
 *
 * Note: Uses 'tmr_' prefix to avoid conflicts with POSIX timer_t/timer_create.
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <sys/time.h>

/*
 * Configuration constants
 */
#define TMR_WHEEL_SIZE      1024              /* Number of slots in the wheel */
#define TMR_WHEEL_TICK_US   1000              /* Microseconds per wheel tick (1ms) */

/*
 * Error codes
 */
#define TMR_OK              0
#define TMR_ERR_NOMEM       (-1)
#define TMR_ERR_INVALID     (-2)
#define TMR_ERR_RUNNING     (-3)

/*
 * Forward declarations
 */
typedef struct tmr_node tmr_t;
typedef struct tmr_ctx tmr_ctx_t;

/*
 * Timer callback function type
 *
 * @param t      The timer that expired
 * @param opaque User-provided opaque pointer
 * @param id     User-provided timer ID
 */
typedef void (*tmr_callback_fn)(tmr_t *t, void *opaque, int id);

/*
 * Timer structure - represents a single timer
 *
 * Users should treat this as opaque and use the API functions.
 */
struct tmr_node {
    tmr_callback_fn   callback;   /* Function to call when timer expires */
    const char       *name;       /* Human-readable name for debugging */
    int64_t           interval;   /* Timer interval in microseconds */
    int64_t           when;       /* Absolute expiry time (microseconds) */
    void             *opaque;     /* User-provided opaque data */
    int               id;         /* User-provided timer ID */
    int               wheel_pos;  /* Current position in wheel (-1 = not running) */
    tmr_t            *next;       /* Next timer in the same wheel slot */
};

/*
 * Timer context - holds the timer wheel and state
 *
 * One context can manage multiple timers.
 */
struct tmr_ctx {
    tmr_t    *wheel[TMR_WHEEL_SIZE];  /* The timer wheel (array of timer lists) */
    int       current_pos;             /* Current wheel position */
    int64_t   wheel_time;              /* Time corresponding to current position */
};

/*
 * Get current time in microseconds
 *
 * @return Current monotonic time in microseconds
 */
int64_t tmr_now(void);

/*
 * Get the current wheel time (for debugging)
 *
 * @param ctx   Timer context
 * @return      Current wheel time in microseconds
 */
int64_t tmr_wheel_time(tmr_ctx_t *ctx);

/*
 * Initialize a timer context
 *
 * @param ctx   Pointer to timer context to initialize
 * @return      TMR_OK on success
 */
int tmr_ctx_init(tmr_ctx_t *ctx);

/*
 * Shutdown timer context and free all timers
 *
 * @param ctx   Timer context
 * @return      TMR_OK on success
 */
int tmr_ctx_shutdown(tmr_ctx_t *ctx);

/*
 * Create a new timer
 *
 * @param ctx      Timer context
 * @param tp       Output: pointer to created timer
 * @param name     Human-readable name for debugging
 * @param interval Timer interval in microseconds (0 = create but don't start)
 * @param callback Function to call when timer expires
 * @param opaque   User data passed to callback
 * @param id       User-defined timer ID passed to callback
 * @return         TMR_OK on success, error code on failure
 */
int tmr_create(tmr_ctx_t *ctx, tmr_t **tp, const char *name,
               int64_t interval, tmr_callback_fn callback, void *opaque, int id);

/*
 * Delete a timer and free its memory
 *
 * @param ctx   Timer context
 * @param t     Timer to delete
 * @return      TMR_OK on success
 */
int tmr_delete(tmr_ctx_t *ctx, tmr_t *t);

/*
 * Start a timer with a new interval
 *
 * @param ctx      Timer context
 * @param t        Timer to start
 * @param interval Interval in microseconds
 * @return         TMR_OK on success, error code on failure
 */
int tmr_start(tmr_ctx_t *ctx, tmr_t *t, int64_t interval);

/*
 * Stop a running timer (does not free it)
 *
 * @param ctx   Timer context
 * @param t     Timer to stop
 * @return      TMR_OK on success
 */
int tmr_stop(tmr_ctx_t *ctx, tmr_t *t);

/*
 * Restart a timer using its current interval
 *
 * @param ctx   Timer context
 * @param t     Timer to restart
 * @return      TMR_OK on success, error code on failure
 */
int tmr_restart(tmr_ctx_t *ctx, tmr_t *t);

/*
 * Execute expired timers
 *
 * This should be called periodically (e.g., in an event loop).
 * It checks for and fires any expired timers.
 *
 * @param ctx   Timer context
 * @return      TMR_OK on success
 */
int tmr_exec(tmr_ctx_t *ctx);

/*
 * Get timeout value for poll() in milliseconds
 *
 * Returns the time until the next timer expires, suitable for
 * use as a poll() timeout.
 *
 * @param ctx   Timer context
 * @return      Timeout in milliseconds (0 = timer pending, -1 = no timers)
 */
int tmr_poll_timeout(tmr_ctx_t *ctx);

/*
 * Get timeout value for select() as a timeval
 *
 * @param ctx   Timer context
 * @param tv    Output: timeval structure to fill
 * @return      Pointer to tv
 */
struct timeval *tmr_select_timeout(tmr_ctx_t *ctx, struct timeval *tv);

/*
 * Dump all timers for debugging
 *
 * @param ctx   Timer context
 */
void tmr_dump(tmr_ctx_t *ctx);

#endif /* TIMER_H */
