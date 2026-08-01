# Generic Timer Library API Design

## Core Data Structures

```c
/* Time type - microseconds since epoch */
typedef int64_t timer_time_t;

/* Timer callback function */
typedef void (*timer_callback_t)(struct timer *timer, void *user_data);

/* Individual timer object */
typedef struct timer {
    timer_time_t when;           /* Absolute expiration time */
    timer_time_t interval;       /* Timer interval in microseconds */
    struct timer *next;          /* Next timer in wheel slot */
    int wheel_slot;              /* Current wheel slot (-1 if not active) */
    const char *name;            /* Timer name for debugging */
    timer_callback_t callback;   /* Expiration callback function */
    void *user_data;             /* User data passed to callback */
    int timer_id;                /* User-defined timer ID */
} timer_t;

/* Time wheel context */
typedef struct timer_context {
    int current_slot;                    /* Current wheel position */
    timer_time_t base_time;              /* Time base at current slot */
    timer_t *wheel[TIMER_WHEEL_SIZE];    /* Timer wheel slots */
} timer_context_t;
```

## Constants and Configuration

```c
#define TIMER_WHEEL_SIZE    8192        /* Number of wheel slots */
#define TIMER_WHEEL_TICK    1000000     /* Microseconds per slot (1 second) */

/* Return codes */
enum {
    TIMER_OK = 0,
    TIMER_ERROR_INVALID = -1,
    TIMER_ERROR_NOMEM = -2,
    TIMER_ERROR_NULL_POINTER = -3
};
```

## Public API Functions

### Context Management
```c
/* Create timer wheel context */
timer_context_t *timer_context_create(void);

/* Destroy context and all timers */
void timer_context_destroy(timer_context_t *ctx);
```

### Timer Lifecycle  
```c
/* Create a timer object */
timer_t *timer_create(const char *name, timer_callback_t callback, void *user_data, int timer_id);

/* Start timer with specified interval */
int timer_start(timer_context_t *ctx, timer_t *timer, timer_time_t interval_us);

/* Stop a running timer */
int timer_stop(timer_context_t *ctx, timer_t *timer);

/* Delete timer object */
void timer_delete(timer_t *timer);
```

### Timer Execution
```c
/* Execute expired timers */
int timer_exec(timer_context_t *ctx);

/* Get timeout for next timer expiration (for select/poll) */
timer_time_t timer_get_next_timeout(timer_context_t *ctx);
```

### Utility Functions
```c
/* Get current system time in microseconds */
timer_time_t timer_get_time(void);

/* Restart an existing timer */
int timer_restart(timer_context_t *ctx, timer_t *timer);
```

## Key Design Decisions

1. **Simplified Error Handling**: Use simple integer return codes instead of complex error system
2. **Clean Separation**: Timer objects separate from context for better memory management  
3. **Standard Types**: Use standard C types and naming conventions
4. **Platform Abstraction**: `timer_get_time()` can be implemented per platform
5. **Thread Safety**: Single-threaded by design, wrapper can add mutex if needed
6. **Memory Management**: Caller responsible for timer object lifecycle