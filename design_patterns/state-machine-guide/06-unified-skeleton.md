# Unified State Machine Skeleton

A generic C skeleton capturing the State Machine pattern as used in the Linux kernel.

---

## Complete Skeleton

```c
/*
 * Generic State Machine Pattern Skeleton
 * Based on Linux kernel state machine implementations
 */

#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * PART 1: STATE DEFINITIONS
 * ================================================================ */

/* State enumeration - mutually exclusive states */
enum object_state {
    STATE_UNINITIALIZED = 0,
    STATE_INITIALIZED,
    STATE_ACTIVE,
    STATE_SUSPENDED,
    STATE_ERROR,
    STATE_MAX  /* For array sizing */
};

/* State names for debugging */
static const char *state_names[] = {
    "UNINITIALIZED",
    "INITIALIZED",
    "ACTIVE",
    "SUSPENDED",
    "ERROR"
};

static inline const char *state_name(enum object_state s)
{
    if (s < STATE_MAX)
        return state_names[s];
    return "UNKNOWN";
}

/* ================================================================
 * PART 2: TRANSITION TABLE
 * ================================================================ */

/* Valid transitions matrix [from][to] */
static const int valid_transitions[STATE_MAX][STATE_MAX] = {
    /*                 UNINIT INIT  ACTIVE SUSP  ERROR */
    /* UNINIT   */ {    0,    1,     0,     0,    0   },
    /* INIT     */ {    1,    0,     1,     0,    1   },
    /* ACTIVE   */ {    0,    1,     0,     1,    1   },
    /* SUSP     */ {    0,    0,     1,     0,    1   },
    /* ERROR    */ {    1,    0,     0,     0,    0   },
};

/* ================================================================
 * PART 3: OBJECT WITH STATE
 * ================================================================ */

struct my_object {
    enum object_state state;
    int id;
    /* Other fields... */
};

/* ================================================================
 * PART 4: STATE TRANSITION FUNCTION
 * ================================================================ */

/* Return codes */
#define STATE_OK        0
#define STATE_INVALID  -1

/* Exit actions - called when leaving a state */
static void exit_state(struct my_object *obj)
{
    switch (obj->state) {
    case STATE_ACTIVE:
        printf("  [EXIT] Stopping active operations\n");
        break;
    case STATE_SUSPENDED:
        printf("  [EXIT] Leaving suspend\n");
        break;
    default:
        break;
    }
}

/* Entry actions - called when entering a state */
static void enter_state(struct my_object *obj, enum object_state new)
{
    switch (new) {
    case STATE_INITIALIZED:
        printf("  [ENTER] Initialization complete\n");
        break;
    case STATE_ACTIVE:
        printf("  [ENTER] Starting operations\n");
        break;
    case STATE_SUSPENDED:
        printf("  [ENTER] Entering low-power mode\n");
        break;
    case STATE_ERROR:
        printf("  [ENTER] Error state - cleanup required\n");
        break;
    default:
        break;
    }
}

/* Main state transition function */
int set_state(struct my_object *obj, enum object_state new_state)
{
    enum object_state old_state = obj->state;
    
    /* Validate transition */
    if (!valid_transitions[old_state][new_state]) {
        printf("[STATE] INVALID: %s -> %s\n",
               state_name(old_state), state_name(new_state));
        return STATE_INVALID;
    }
    
    printf("[STATE] %s -> %s\n",
           state_name(old_state), state_name(new_state));
    
    /* Exit actions */
    exit_state(obj);
    
    /* Change state */
    obj->state = new_state;
    
    /* Entry actions */
    enter_state(obj, new_state);
    
    return STATE_OK;
}

/* ================================================================
 * PART 5: STATE QUERY FUNCTIONS
 * ================================================================ */

static inline int is_active(struct my_object *obj)
{
    return obj->state == STATE_ACTIVE;
}

static inline int is_usable(struct my_object *obj)
{
    return obj->state == STATE_ACTIVE ||
           obj->state == STATE_INITIALIZED;
}

static inline int is_error(struct my_object *obj)
{
    return obj->state == STATE_ERROR;
}

/* ================================================================
 * PART 6: STATE-DEPENDENT OPERATIONS
 * ================================================================ */

int do_operation(struct my_object *obj)
{
    /* Check state before operating */
    if (!is_active(obj)) {
        printf("[OP] Cannot operate: not active (state=%s)\n",
               state_name(obj->state));
        return -1;
    }
    
    printf("[OP] Performing operation on object %d\n", obj->id);
    return 0;
}

/* ================================================================
 * PART 7: LIFECYCLE FUNCTIONS
 * ================================================================ */

struct my_object *create_object(int id)
{
    struct my_object *obj = malloc(sizeof(*obj));
    if (!obj)
        return NULL;
    
    obj->state = STATE_UNINITIALIZED;
    obj->id = id;
    
    printf("[CREATE] Object %d created\n", id);
    return obj;
}

int init_object(struct my_object *obj)
{
    return set_state(obj, STATE_INITIALIZED);
}

int activate_object(struct my_object *obj)
{
    return set_state(obj, STATE_ACTIVE);
}

int suspend_object(struct my_object *obj)
{
    return set_state(obj, STATE_SUSPENDED);
}

int resume_object(struct my_object *obj)
{
    return set_state(obj, STATE_ACTIVE);
}

void destroy_object(struct my_object *obj)
{
    printf("[DESTROY] Object %d destroyed\n", obj->id);
    free(obj);
}

/* ================================================================
 * PART 8: USAGE EXAMPLE
 * ================================================================ */

int main(void)
{
    struct my_object *obj;
    
    printf("=== STATE MACHINE SKELETON ===\n\n");
    
    /* Create and initialize */
    obj = create_object(1);
    
    /* Try operation before active */
    printf("\n--- Try operation before active ---\n");
    do_operation(obj);
    
    /* Normal lifecycle */
    printf("\n--- Normal lifecycle ---\n");
    init_object(obj);
    activate_object(obj);
    do_operation(obj);
    
    /* Suspend and resume */
    printf("\n--- Suspend/Resume ---\n");
    suspend_object(obj);
    do_operation(obj);  /* Should fail */
    resume_object(obj);
    do_operation(obj);  /* Should work */
    
    /* Invalid transition */
    printf("\n--- Invalid transition ---\n");
    set_state(obj, STATE_UNINITIALIZED);  /* Not allowed from ACTIVE */
    
    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    set_state(obj, STATE_INITIALIZED);
    set_state(obj, STATE_UNINITIALIZED);
    destroy_object(obj);
    
    return 0;
}
```

---

## Mapping to Kernel Cases

```
    SKELETON              TCP                 USB                 NETDEV
    ========              ===                 ===                 ======
    
    object_state          tcp_state           usb_device_state    operstate
    valid_transitions     implicit in code    usb_set_device_     dev_open/
                                              state()             close rules
    set_state()           tcp_set_state()     usb_set_device_     update_
                                              state()             operstate()
    is_active()           TCP_ESTABLISHED     USB_STATE_          netif_
                                              CONFIGURED          running()
```

---

## Key Implementation Points

```
    1. STATE ENUMERATION
       - Define all states as enum
       - States are mutually exclusive
       - Include initial and terminal states
    
    2. TRANSITION VALIDATION
       - Matrix or switch-based validation
       - Reject invalid transitions
       - Return error or BUG_ON
    
    3. ENTRY/EXIT ACTIONS
       - Actions when leaving old state
       - Actions when entering new state
       - Resource allocation/cleanup
    
    4. STATE QUERIES
       - Helper functions for common checks
       - is_active(), is_usable(), etc.
    
    5. STATE-DEPENDENT OPERATIONS
       - Check state before operations
       - Return error if wrong state
```

---

## Version

Based on **Linux kernel v3.2** state machine implementations.
