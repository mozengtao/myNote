# State Machine Anti-Patterns

Common mistakes to avoid when implementing state machines.

---

## Anti-Pattern 1: Missing Transition Validation

```c
/* BAD: No validation */
void set_state(struct device *dev, int new_state)
{
    dev->state = new_state;
}

/* CORRECT: Validate */
int set_state(struct device *dev, int new_state)
{
    if (!valid_transition[dev->state][new_state])
        return -EINVAL;
    dev->state = new_state;
    return 0;
}
```

---

## Anti-Pattern 2: Flag-Based State

```c
/* BAD: Multiple flags for lifecycle */
struct device {
    int is_on;
    int is_ready;
    int is_error;
};

/* CORRECT: Single state variable */
enum state { OFF, READY, ERROR };
struct device {
    enum state state;
};
```

---

## Anti-Pattern 3: Race Conditions

```c
/* BAD: Non-atomic check-and-change */
if (dev->state == STATE_READY) {
    /* Race window! */
    dev->state = STATE_BUSY;
}

/* CORRECT: Use locking */
spin_lock(&dev->lock);
if (dev->state == STATE_READY)
    dev->state = STATE_BUSY;
spin_unlock(&dev->lock);
```

---

## Anti-Pattern 4: Implicit States

```c
/* BAD: Derived state */
int get_state(struct device *dev)
{
    if (dev->power && dev->driver && !dev->error)
        return STATE_READY;
    /* Complex logic */
}

/* CORRECT: Explicit state */
struct device {
    enum state state;  /* Single source of truth */
};
```

---

## Summary

```
    [X] Validate all state transitions
    [X] Use single state variable
    [X] Protect with locks
    [X] State is explicit
    [X] Include entry/exit actions
```

---

## Version

Based on **Linux kernel v3.2** state machine patterns.
