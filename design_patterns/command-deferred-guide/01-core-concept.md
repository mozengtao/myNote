# Core Concept: Command/Deferred Execution Pattern

What deferred execution means in kernel architecture and why it is essential for handling work that cannot run in the current context.

---

## What Problem Does Deferred Execution Solve?

```
+=============================================================================+
|                    THE DEFERRED EXECUTION PROBLEM                            |
+=============================================================================+

    INTERRUPT CONTEXT LIMITATIONS:
    ==============================

    When CPU receives hardware interrupt:
    - Must respond FAST
    - Cannot sleep
    - Cannot call blocking functions
    - Cannot hold mutexes

    But sometimes interrupt needs to:
    - Allocate memory (might sleep)
    - Acquire mutexes (might sleep)
    - Do lengthy processing


    THE SOLUTION: DEFERRED EXECUTION
    ================================

    +-------------+     schedule     +-------------+
    | IRQ Handler | --------------> | Deferred    |
    | (atomic)    |                 | Work        |
    +-------------+                 | (process)   |
                                    +-------------+
```

**中文说明：**

延迟执行解决的问题：中断上下文不能睡眠但有时需要做可能睡眠的操作。解决方案：中断处理程序调度工作，稍后在安全上下文执行。

---

## Kernel Mechanisms

```
    FOUR MECHANISMS:
    ================

    1. SOFTIRQ  - Highest priority, no sleep, kernel-defined
    2. TASKLET  - Built on softirq, dynamic, no sleep
    3. WORKQUEUE - Process context, CAN sleep
    4. TIMER    - Time-delayed, softirq context

    Mechanism   Context      Sleep   Use Case
    ---------   -------      -----   --------
    Softirq     Softirq      No      Net/Block
    Tasklet     Softirq      No      Drivers
    Workqueue   Process      Yes     General
    Timer       Softirq      No      Delays
```

---

## Command Pattern Connection

```
    DEFERRED EXECUTION IS COMMAND PATTERN:
    =====================================

    +------------------+     +------------------+
    | struct work_struct|    | Command Object   |
    +------------------+     +------------------+
    | work_func        |     | execute()        |
    | data             |     | data             |
    +------------------+     +------------------+

    - Encapsulate work in structure
    - Queue for later execution
    - Executor processes commands
```

---

## When to Use What

```
    Need to sleep?
        YES --> WORKQUEUE
        NO  --> Continue...
    
    Highest priority?
        YES --> SOFTIRQ
        NO  --> Continue...
    
    Per-device, dynamic?
        YES --> TASKLET
        NO  --> WORKQUEUE (default)
```

---

## Version

Based on **Linux kernel v3.2** deferred execution mechanisms.
