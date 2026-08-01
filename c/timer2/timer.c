/*
 * Generic Timer Library - Time Wheel Implementation
 * Ported from DPL timer system for high-performance applications
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "timer.h"

/*
 * Internal helper macros
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * Get current system time in microseconds
 */
timer_time_t
timer_get_time(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }

    return (timer_time_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/*
 * Internal function: Remove timer from wheel
 * Based on DPL tdelete()
 */
static int
timer_remove_internal(timer_context_t *ctx, gtimer_t *timer)
{
    gtimer_t *current, **prev_link;

    if (!ctx || !timer) {
        return TIMER_ERROR_NULL_POINTER;
    }

    if (timer->wheel_slot == -1) {
        return TIMER_ERROR_INVALID;  /* Timer not in wheel */
    }

    prev_link = &ctx->wheel[timer->wheel_slot];
    for (current = *prev_link; current != NULL; current = current->next) {
        if (current == timer) {
            *prev_link = timer->next;
            break;
        }
        prev_link = &current->next;
    }

    timer->wheel_slot = -1;
    timer->next = NULL;

    return TIMER_OK;
}

/*
 * Internal function: Insert timer into wheel with sorted order
 * Based on DPL tinsert()
 */
static void
timer_insert_internal(timer_context_t *ctx, gtimer_t *timer)
{
    int64_t offset;
    gtimer_t **insert_point, *current;

    if (!ctx || !timer) {
        return;
    }

    if (timer->wheel_slot != -1) {
        /* Timer already in wheel - this shouldn't happen */
        printf("Warning: inserting timer that's already active: %s\n",
               timer->name ? timer->name : "unnamed");
        return;
    }

    /* Calculate wheel slot based on expiration time */
    offset = timer->when - ctx->base_time;
    if (offset < 0) {
        offset = 0;  /* Timer already expired, put in current slot */
    }

    offset /= TIMER_WHEEL_TICK;
    offset += ctx->current_slot;
    offset %= TIMER_WHEEL_SIZE;

    /* Find insertion point in sorted order */
    insert_point = &ctx->wheel[offset];
    for (;;) {
        current = *insert_point;
        if (current == NULL) {
            break;  /* End of list */
        }

        if (timer_time_cmp(current->when, timer->when) >= 0) {
            break;  /* Found insertion point */
        }

        insert_point = &current->next;
    }

    /* Insert timer */
    timer->next = *insert_point;
    *insert_point = timer;
    timer->wheel_slot = offset;
}

/*
 * Create timer wheel context
 */
timer_context_t *
timer_context_create(void)
{
    timer_context_t *ctx;

    ctx = malloc(sizeof(timer_context_t));
    if (ctx == NULL) {
        return NULL;
    }

    memset(ctx, 0, sizeof(timer_context_t));
    ctx->current_slot = 0;
    ctx->base_time = timer_get_time();

    return ctx;
}

/*
 * Destroy context and cleanup all timers
 */
void
timer_context_destroy(timer_context_t *ctx)
{
    int slot;
    gtimer_t *timer, *next_timer;

    if (ctx == NULL) {
        return;
    }

    /* Free all timers in all slots */
    for (slot = 0; slot < (int)ARRAY_SIZE(ctx->wheel); slot++) {
        timer = ctx->wheel[slot];
        while (timer != NULL) {
            next_timer = timer->next;
            free(timer);
            timer = next_timer;
        }
    }

    free(ctx);
}

/*
 * Create timer object
 */
gtimer_t *
timer_create(const char *name, timer_callback_t callback, void *user_data, int timer_id)
{
    gtimer_t *timer;

    if (callback == NULL) {
        return NULL;
    }

    timer = malloc(sizeof(gtimer_t));
    if (timer == NULL) {
        return NULL;
    }

    memset(timer, 0, sizeof(gtimer_t));
    timer->name = name;
    timer->callback = callback;
    timer->user_data = user_data;
    timer->timer_id = timer_id;
    timer->wheel_slot = -1;  /* Not active */

    return timer;
}

/*
 * Start timer with specified interval
 */
int
timer_start(timer_context_t *ctx, gtimer_t *timer, timer_time_t interval_us)
{
    if (!ctx || !timer) {
        return TIMER_ERROR_NULL_POINTER;
    }

    if (timer->callback == NULL) {
        return TIMER_ERROR_INVALID;
    }

    timer->interval = interval_us;
    return timer_restart(ctx, timer);
}

/*
 * Stop timer
 */
int
timer_stop(timer_context_t *ctx, gtimer_t *timer)
{
    if (!ctx || !timer) {
        return TIMER_ERROR_NULL_POINTER;
    }

    return timer_remove_internal(ctx, timer);
}

/*
 * Restart timer (stop and start with current interval)
 */
int
timer_restart(timer_context_t *ctx, gtimer_t *timer)
{
    if (!ctx || !timer) {
        return TIMER_ERROR_NULL_POINTER;
    }

    /* Remove from wheel if currently active */
    timer_remove_internal(ctx, timer);

    /* Set new expiration time */
    timer->when = timer_get_time() + timer->interval;

    /* Insert into wheel */
    timer_insert_internal(ctx, timer);

    return TIMER_OK;
}

/*
 * Delete timer object
 */
void
timer_delete(gtimer_t *timer)
{
    if (timer != NULL) {
        free(timer);
    }
}

/*
 * Execute expired timers
 * Based on DPL dpl_timerexec()
 */
int
timer_exec(timer_context_t *ctx)
{
    gtimer_t *timer;
    timer_time_t now, next_slot_time;

    if (ctx == NULL) {
        return TIMER_ERROR_NULL_POINTER;
    }

    now = timer_get_time();

    /* Execute all expired timers in current slot */
    for (;;) {
        timer = ctx->wheel[ctx->current_slot];
        if (timer == NULL) {
            break;  /* No more timers in this slot */
        }

        if (timer_time_cmp(timer->when, now) > 0) {
            break;  /* Timer not yet expired */
        }

        /* Remove timer from wheel */
        ctx->wheel[ctx->current_slot] = timer->next;
        timer->next = NULL;
        timer->wheel_slot = -1;

        /* Execute callback */
        timer->callback(timer, timer->user_data);
    }

    /* Advance wheel position if time has passed */
    next_slot_time = ctx->base_time + TIMER_WHEEL_TICK;
    if (timer_time_cmp(now, next_slot_time) > 0) {
        ctx->base_time = next_slot_time;
        ctx->current_slot++;
        if (ctx->current_slot >= TIMER_WHEEL_SIZE) {
            ctx->current_slot = 0;
        }
    }

    return TIMER_OK;
}

/*
 * Get time until next timer expiration
 */
timer_time_t
timer_get_next_timeout(timer_context_t *ctx)
{
    gtimer_t *timer;
    timer_time_t when, now;

    if (ctx == NULL) {
        return -1;
    }

    when = ctx->base_time + TIMER_WHEEL_TICK;

    /* Check if there's an earlier timer in current slot */
    timer = ctx->wheel[ctx->current_slot];
    if (timer != NULL && timer_time_cmp(timer->when, when) < 0) {
        when = timer->when;
    }

    now = timer_get_time();
    timer_time_t timeout = when - now;

    if (timeout < 0) {
        return 0;  /* Already expired, return immediately */
    }

    return timeout;
}