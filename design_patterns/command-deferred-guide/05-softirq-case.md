# Case 3: Softirqs

Softirqs are highest-priority deferred execution for kernel subsystems.

---

## Overview

```
    SOFTIRQS (v3.2):
    - HI_SOFTIRQ (tasklets)
    - TIMER_SOFTIRQ
    - NET_TX_SOFTIRQ
    - NET_RX_SOFTIRQ
    - BLOCK_SOFTIRQ
    - TASKLET_SOFTIRQ
```

**中文说明：**

Softirq是最高优先级的延迟执行，用于网络、块I/O等核心子系统。

---

## Key Functions

```c
/* Register (at init) */
open_softirq(nr, handler);

/* Raise (from IRQ) */
raise_softirq(nr);

/* Processing */
do_softirq();
```

---

## Softirq vs Tasklet

| Aspect | Softirq | Tasklet |
|--------|---------|---------|
| Definition | Compile-time | Dynamic |
| Parallelism | Multi-CPU | Serialized |
| Use case | Kernel subsystems | Drivers |

---

## Version

Based on **Linux kernel v3.2**.
