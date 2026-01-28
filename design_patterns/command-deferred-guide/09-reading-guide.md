# Source Reading Guide: Deferred Execution

A guided path through Linux kernel v3.2 source code for deferred execution.

---

## Reading Path

```
    PHASE 1: Workqueue
    ==================
    include/linux/workqueue.h  <- API
    kernel/workqueue.c         <- Implementation
    
    PHASE 2: Tasklet/Softirq
    ========================
    include/linux/interrupt.h  <- API
    kernel/softirq.c           <- Implementation
    
    PHASE 3: Example Usage
    ======================
    drivers/net/*.c            <- Network driver workqueues
    drivers/block/*.c          <- Block driver tasklets
```

---

## Key Functions

| Function | File | Purpose |
|----------|------|---------|
| `schedule_work()` | kernel/workqueue.c | Queue work |
| `worker_thread()` | kernel/workqueue.c | Worker loop |
| `tasklet_schedule()` | kernel/softirq.c | Queue tasklet |
| `tasklet_action()` | kernel/softirq.c | Run tasklets |
| `do_softirq()` | kernel/softirq.c | Process softirqs |

---

## Reading Checklist

```
    [ ] Read struct work_struct definition
    [ ] Read schedule_work() implementation
    [ ] Read struct tasklet_struct definition
    [ ] Read tasklet_schedule() implementation
    [ ] Find usage in a network driver
```

---

## Version

This guide is for **Linux kernel v3.2**.
