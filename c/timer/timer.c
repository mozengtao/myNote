/*
 * Standalone Timer Wheel Implementation
 *
 * Implements a "hashed timing wheel" algorithm for efficient O(1) timer
 * operations. See timer.h for detailed documentation.
 *
 * Note: Uses 'tmr_' prefix to avoid conflicts with POSIX timer_t/timer_create.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "timer.h"

/*
 * Helper macro for array element count
 */
#define NELEMS(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * Get current time in microseconds
 *
 * Uses CLOCK_MONOTONIC for monotonically increasing time that
 * is not affected by system clock changes.
 */
int64_t
tmr_now(void)
{
    struct timespec ts;

    /*
     * CLOCK_MONOTONIC: monotonic time since some unspecified starting point.
     * Not affected by discontinuous jumps in system time (e.g., NTP adjustments).
     */
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/*
 * Compare two timestamps
 *
 * @return  < 0 if a < b, 0 if a == b, > 0 if a > b
 */
static int
tmr_cmp(int64_t a, int64_t b)
{
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

/*
 * Remove a timer from the wheel (internal function)
 *
 * Walks the linked list at the timer's wheel position and removes it.
 * Sets wheel_pos to -1 to indicate timer is not running.
 *
 * @param ctx   Timer context
 * @param t     Timer to remove
 * @return      TMR_OK on success, TMR_ERR_INVALID if not in wheel
 */
static int
wheel_remove(tmr_ctx_t *ctx, tmr_t *t)
{
    tmr_t *cur, **prev_next;

    /* Timer not in wheel */
    if (t->wheel_pos == -1)
        return TMR_ERR_INVALID;

    /*
     * Walk the linked list at this wheel position.
     * prev_next points to the 'next' pointer that points to current node,
     * allowing us to unlink the node when found.
     */
    prev_next = &ctx->wheel[t->wheel_pos];
    for (cur = *prev_next; cur != NULL; cur = cur->next) {
        if (cur == t) {
            /* Found it - unlink by updating previous node's next pointer */
            *prev_next = cur->next;
            break;
        }
        prev_next = &cur->next;
    }

    /* Mark as not running */
    t->wheel_pos = -1;

    return TMR_OK;
}

/*
 * Insert a timer into the wheel (internal function)
 *
 * Calculates which wheel slot the timer should go into based on its
 * expiry time, then inserts it in sorted order within that slot.
 *
 * @param ctx   Timer context
 * @param t     Timer to insert
 */
static void
wheel_insert(tmr_ctx_t *ctx, tmr_t *t)
{
    int64_t offset;
    int slot;
    tmr_t *cur, **insert_point;

    /* Sanity check: don't insert if already running */
    if (t->wheel_pos != -1) {
        printf("[tmr] ERROR: inserting already-running timer: %p-%s\n",
               (void *)t, t->name);
        return;
    }

    /*
     * Calculate wheel slot for this timer:
     * 1. Compute time offset from current wheel time
     * 2. Convert to number of ticks
     * 3. Add current position and wrap around
     */
    offset = t->when - ctx->wheel_time;
    if (offset < 0)
        offset = 0;  /* Already expired, put in current slot */

    offset /= TMR_WHEEL_TICK_US;          /* Convert to ticks */
    slot = (ctx->current_pos + offset) % TMR_WHEEL_SIZE;

    /*
     * Insert into the slot's linked list in sorted order (by expiry time).
     * This ensures timers in the same slot fire in the correct order.
     */
    insert_point = &ctx->wheel[slot];
    for (;;) {
        cur = *insert_point;
        if (cur == NULL)
            break;

        /* Insert before first timer that expires later than us */
        if (tmr_cmp(cur->when, t->when) >= 0)
            break;

        insert_point = &cur->next;
    }

    /* Link into the list */
    t->next = *insert_point;
    *insert_point = t;

    /* Remember which slot we're in */
    t->wheel_pos = slot;
}

/*
 * Initialize a timer context
 */
int
tmr_ctx_init(tmr_ctx_t *ctx)
{
    if (ctx == NULL)
        return TMR_ERR_INVALID;

    memset(ctx->wheel, 0, sizeof(ctx->wheel));
    ctx->current_pos = 0;
    ctx->wheel_time = tmr_now();

    return TMR_OK;
}

/*
 * Shutdown timer context and free all timers
 */
int
tmr_ctx_shutdown(tmr_ctx_t *ctx)
{
    int i;
    tmr_t *t;

    if (ctx == NULL)
        return TMR_ERR_INVALID;

    /*
     * Walk all wheel slots and free all timers
     */
    for (i = 0; i < (int)NELEMS(ctx->wheel); i++) {
        while ((t = ctx->wheel[i]) != NULL) {
            wheel_remove(ctx, t);
            free(t);
        }
    }

    return TMR_OK;
}

/*
 * Restart a timer using its current interval
 */
int
tmr_restart(tmr_ctx_t *ctx, tmr_t *t)
{
    if (t == NULL)
        return TMR_ERR_INVALID;

    /* Remove from wheel if currently running */
    wheel_remove(ctx, t);

    /* Calculate new expiry time: now + interval */
    t->when = tmr_now() + t->interval;

    /* Insert back into the wheel */
    wheel_insert(ctx, t);

    return TMR_OK;
}

/*
 * Stop a running timer (does not free it)
 */
int
tmr_stop(tmr_ctx_t *ctx, tmr_t *t)
{
    if (t == NULL)
        return TMR_OK;

    return wheel_remove(ctx, t);
}

/*
 * Delete a timer and free its memory
 */
int
tmr_delete(tmr_ctx_t *ctx, tmr_t *t)
{
    if (t == NULL)
        return TMR_OK;

    wheel_remove(ctx, t);
    free(t);

    return TMR_OK;
}

/*
 * Start a timer with a new interval
 */
int
tmr_start(tmr_ctx_t *ctx, tmr_t *t, int64_t interval)
{
    if (t == NULL) {
        printf("[tmr] ERROR: timer not created, failed to start\n");
        return TMR_ERR_INVALID;
    }

    t->interval = interval;
    return tmr_restart(ctx, t);
}

/*
 * Create a new timer
 */
int
tmr_create(tmr_ctx_t *ctx, tmr_t **tp, const char *name,
           int64_t interval, tmr_callback_fn callback, void *opaque, int id)
{
    tmr_t *t;

    t = malloc(sizeof(tmr_t));
    if (t == NULL) {
        printf("[tmr] ERROR: memory allocation failed for timer '%s'\n", name);
        return TMR_ERR_NOMEM;
    }

    /* Initialize timer fields */
    t->callback = callback;
    t->name = name;
    t->interval = interval;
    t->opaque = opaque;
    t->id = id;
    t->wheel_pos = -1;  /* Not running yet */
    t->next = NULL;
    t->when = 0;

    *tp = t;

    /*
     * If interval is 0, just create without starting.
     * Otherwise, start the timer immediately.
     */
    if (interval == 0)
        return TMR_OK;

    return tmr_restart(ctx, t);
}

/*
 * Execute expired timers
 *
 * This is the main timer processing function. It should be called
 * periodically (e.g., from an event loop).
 *
 * Algorithm:
 * 1. Check all timers at the current wheel position
 * 2. Fire any that have expired
 * 3. Advance the wheel if enough time has passed
 */
int
tmr_exec(tmr_ctx_t *ctx)
{
    tmr_t *t;
    int64_t now, next_tick;

    now = tmr_now();

    /*
     * Process timers at current wheel position.
     * Timers are sorted by expiry time within each slot.
     */
    for (;;) {
        t = ctx->wheel[ctx->current_pos];
        if (t == NULL)
            break;

        /* Stop if this timer hasn't expired yet */
        if (tmr_cmp(t->when, now) > 0)
            break;

        /*
         * Timer has expired - remove from wheel and fire callback.
         * Note: We remove before calling callback so the callback
         * can safely restart the same timer if needed.
         */
        ctx->wheel[ctx->current_pos] = t->next;
        t->next = NULL;
        t->wheel_pos = -1;

        /* Fire the callback */
        t->callback(t, t->opaque, t->id);
    }

    /*
     * Advance the wheel if we've passed the next tick boundary.
     * This rotates the wheel to process the next time slot.
     */
    next_tick = ctx->wheel_time + TMR_WHEEL_TICK_US;
    if (tmr_cmp(now, next_tick) > 0) {
        ctx->wheel_time = next_tick;
        ctx->current_pos++;
        if (ctx->current_pos >= (int)NELEMS(ctx->wheel))
            ctx->current_pos = 0;  /* Wrap around */
    }

    return TMR_OK;
}

/*
 * Get the current wheel time (for debugging)
 */
int64_t
tmr_wheel_time(tmr_ctx_t *ctx)
{
    return ctx->wheel_time;
}

/*
 * Get timeout value for select() as a timeval
 *
 * Calculates time until the next timer expires, suitable for use
 * as a select() timeout.
 */
struct timeval *
tmr_select_timeout(tmr_ctx_t *ctx, struct timeval *tv)
{
    tmr_t *t;
    int64_t remaining, next_when;

    /* Default: wait until next wheel tick */
    next_when = ctx->wheel_time + TMR_WHEEL_TICK_US;

    /* Check if there's a timer at current position that expires sooner */
    t = ctx->wheel[ctx->current_pos];
    if (t != NULL && tmr_cmp(t->when, next_when) < 0)
        next_when = t->when;

    /* Calculate remaining time */
    remaining = next_when - tmr_now();
    if (remaining < 0) {
        /* Timer already expired - return minimal timeout */
        tv->tv_sec = 0;
        tv->tv_usec = 1;
    } else {
        tv->tv_sec = remaining / 1000000;
        tv->tv_usec = remaining - tv->tv_sec * 1000000;
    }

    return tv;
}

/*
 * Get timeout value for poll() in milliseconds
 *
 * Returns the time until the next timer expires.
 * Returns 0 if a timer has already expired.
 */
int
tmr_poll_timeout(tmr_ctx_t *ctx)
{
    tmr_t *t;
    int64_t remaining, next_when;

    /* Default: wait until next wheel tick */
    next_when = ctx->wheel_time + TMR_WHEEL_TICK_US;

    /* Check if there's a timer at current position that expires sooner */
    t = ctx->wheel[ctx->current_pos];
    if (t != NULL && tmr_cmp(t->when, next_when) < 0)
        next_when = t->when;

    /* Calculate remaining time in milliseconds */
    remaining = next_when - tmr_now();
    if (remaining < 0)
        remaining = 0;

    return remaining / 1000;  /* Convert microseconds to milliseconds */
}

/*
 * Dump all timers for debugging
 *
 * Prints the state of all timers in the wheel, showing:
 * - Wheel position
 * - Whether timer is expired (E) or running (R)
 * - Timing information
 * - Timer name
 */
void
tmr_dump(tmr_ctx_t *ctx)
{
    tmr_t *t;
    int i, pos;
    int64_t slot_time, now;

    printf("[tmr] DUMP: current_pos=%d wheel_time=%ld\n",
           ctx->current_pos, (long)ctx->wheel_time);

    pos = ctx->current_pos;
    slot_time = ctx->wheel_time;
    now = tmr_now();

    /*
     * Walk through all wheel slots starting from current position
     */
    for (i = 0; i < (int)NELEMS(ctx->wheel); i++) {
        for (t = ctx->wheel[pos]; t != NULL; t = t->next) {
            printf("[tmr]   slot=%d %c time=%ld when=%ld delta=%ld name=%s\n",
                   pos,
                   (now > t->when) ? 'E' : 'R',  /* E=expired, R=running */
                   (long)slot_time,
                   (long)t->when,
                   (long)(t->when - slot_time),
                   t->name);
        }

        slot_time += TMR_WHEEL_TICK_US;
        pos++;
        if (pos >= (int)NELEMS(ctx->wheel))
            pos = 0;
    }
}
