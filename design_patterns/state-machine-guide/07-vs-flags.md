# State Machine vs Boolean Flags

Understanding when to use state machines versus simple boolean flags.

---

## Core Distinction

```
+=============================================================================+
|                    STATE MACHINE vs FLAGS                                    |
+=============================================================================+

    BOOLEAN FLAGS:                      STATE MACHINE:
    ==============                      ==============
    
    struct device {                     enum device_state {
        int is_powered;                     STATE_OFF,
        int is_initialized;                 STATE_POWERED,
        int is_running;                     STATE_INITIALIZED,
        int has_error;                      STATE_RUNNING,
    };                                      STATE_ERROR,
                                        };
    
    /* 2^4 = 16 combinations */         /* 5 discrete states */
    /* How many are valid? */           /* All are valid */
```

**中文说明：**

布尔标志vs状态机：使用标志时，4个布尔值有16种组合，不清楚哪些有效；使用状态机时，5个离散状态，所有状态都是有效的。

---

## Comparison Table

| Aspect | Boolean Flags | State Machine |
|--------|---------------|---------------|
| **Valid states** | Any combination | Only defined states |
| **Transitions** | Any flag change | Only valid paths |
| **Atomic update** | Multiple writes | Single write |
| **Code complexity** | Combinatorial | Linear |
| **Debugging** | Check N flags | Check 1 state |

---

## When to Use Flags

```c
/* USE FLAGS for truly independent properties */
struct device {
    unsigned int readable:1;
    unsigned int writable:1;
    unsigned int seekable:1;
    /* These are independent! All combinations valid */
};
```

---

## When to Use State Machine

```c
/* USE STATE MACHINE for lifecycle/protocol */
enum connection_state {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    DISCONNECTING,
};
/* States are mutually exclusive */
/* Specific transitions required */
```

---

## Decision Flowchart

```
    Are the properties independent?
            |
    +-------+-------+
   YES              NO
    |               |
    v               v
  FLAGS          STATE MACHINE
```

---

## Version

Based on **Linux kernel v3.2** design patterns.
