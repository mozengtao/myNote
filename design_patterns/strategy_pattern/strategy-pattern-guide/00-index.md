# Strategy Pattern in Linux Kernel (v3.2)

A comprehensive guide to understanding the Strategy design pattern as it is actually implemented in the Linux kernel — focusing on algorithm-level replaceability and policy separation.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept: What Strategy Means in Kernel |
| [02-identification-rules.md](02-identification-rules.md) | Kernel-Level Identification Rules |
| [03-scheduler-case.md](03-scheduler-case.md) | Case 1: CPU Scheduler Classes |
| [04-tcp-congestion-case.md](04-tcp-congestion-case.md) | Case 2: TCP Congestion Control |
| [05-io-scheduler-case.md](05-io-scheduler-case.md) | Case 3: I/O Scheduler (Elevator) |
| [06-memory-policy-case.md](06-memory-policy-case.md) | Case 4: Memory Allocation Policies |
| [07-lsm-case.md](07-lsm-case.md) | Case 5: Linux Security Modules |
| [08-unified-skeleton.md](08-unified-skeleton.md) | Minimal Unified Strategy Skeleton |
| [09-vs-template-method.md](09-vs-template-method.md) | Strategy vs Template Method |
| [10-antipatterns.md](10-antipatterns.md) | Common Kernel Anti-Patterns |
| [11-reading-guide.md](11-reading-guide.md) | Learning Path and Source Reading Guide |
| [12-mental-model.md](12-mental-model.md) | Final Mental Model |

---

## Overview Diagram

```
+=============================================================================+
|                      STRATEGY PATTERN IN LINUX KERNEL                        |
+=============================================================================+

                        POLICY vs MECHANISM
                        ====================

    +------------------------------------------------------------------+
    |                     KERNEL CORE (Mechanism)                       |
    |                                                                   |
    |   "I know WHEN to schedule, but not HOW to pick next task"        |
    |   "I know WHEN to send, but not HOW to control congestion"        |
    |   "I know WHEN to dispatch I/O, but not HOW to order requests"    |
    |                                                                   |
    +------------------------------------------------------------------+
                                |
                                | delegates to
                                v
    +------------------------------------------------------------------+
    |                  STRATEGY (Pluggable Policy)                      |
    |                                                                   |
    |   +-------------+    +-------------+    +-------------+           |
    |   | Strategy A  |    | Strategy B  |    | Strategy C  |           |
    |   | (CFS)       |    | (RT)        |    | (Deadline)  |           |
    |   +-------------+    +-------------+    +-------------+           |
    |                                                                   |
    |   +-------------+    +-------------+    +-------------+           |
    |   | (CUBIC)     |    | (Reno)      |    | (Vegas)     |           |
    |   +-------------+    +-------------+    +-------------+           |
    |                                                                   |
    |   +-------------+    +-------------+    +-------------+           |
    |   | (CFQ)       |    | (Deadline)  |    | (NOOP)      |           |
    |   +-------------+    +-------------+    +-------------+           |
    |                                                                   |
    +------------------------------------------------------------------+

    KEY INSIGHT:
    +------------------------------------------------------------------+
    |  Strategy = ENTIRE ALGORITHM is replaceable                       |
    |  The core provides the WHEN, the strategy provides the HOW        |
    |  Multiple ops work together as ONE coherent policy                |
    +------------------------------------------------------------------+
```

**中文说明：**

上图展示了Linux内核中策略模式的核心架构——策略与机制分离。内核核心（机制）知道"何时"做某事，但不知道"如何"做：它知道何时调度但不知道如何选择下一个任务、知道何时发送但不知道如何控制拥塞、知道何时分发I/O但不知道如何排序请求。策略（可插拔的算法）提供"如何"：调度策略（CFS、RT、Deadline）、拥塞控制算法（CUBIC、Reno、Vegas）、I/O调度器（CFQ、Deadline、NOOP）。关键洞察：策略意味着整个算法可替换，核心提供"何时"，策略提供"如何"，多个ops函数协同工作形成一个完整的策略。

---

## Strategy vs Template Method: Quick Comparison

```
    TEMPLATE METHOD                        STRATEGY
    ===============                        ========

    Framework OWNS control flow            Framework DELEGATES entire algorithm

    +-------------------+                  +-------------------+
    | framework_func()  |                  | core_func()       |
    |   pre_step()      |                  |                   |
    |   hook() ---------|-> one func       |   strategy->      |
    |   post_step()     |                  |     algo_a()      |
    +-------------------+                  |     algo_b()      |
                                           |     algo_c() -----|-> entire algo
                                           +-------------------+

    Examples:                              Examples:
    - vfs_read()                           - schedule()
    - dev_queue_xmit()                     - tcp_cong_avoid()
    - device_add()                         - elv_dispatch()
```

**中文说明：**

模板方法与策略模式的快速对比：模板方法中，框架拥有控制流，在固定的前后步骤之间调用一个钩子函数。策略模式中，框架将整个算法委托给策略，策略拥有多个协同工作的函数。模板方法示例：`vfs_read()`、`dev_queue_xmit()`、`device_add()`。策略模式示例：`schedule()`、`tcp_cong_avoid()`、`elv_dispatch()`。

---

## Prerequisites

This guide assumes familiarity with:

- C programming language
- Function pointers and ops tables
- Linux kernel subsystems (scheduler, networking, block layer)
- Reading kernel source code
- The Template Method pattern in kernel (see companion guide)

---

## Key Terminology

| Term | Kernel Meaning |
|------|----------------|
| **Strategy** | A complete, replaceable algorithm for a specific concern |
| **Policy** | The "how" of a decision (e.g., how to pick next task) |
| **Mechanism** | The "when" of an action (e.g., when to reschedule) |
| **Class** | A strategy implementation (e.g., `sched_class`) |
| **Algorithm** | A self-contained set of related operations |
| **Pluggable** | Can be selected/replaced at runtime or boot time |
| **Ops Table** | Structure containing the strategy's function pointers |

---

## Why Strategy Exists in Linux Kernel

The kernel must support diverse workloads:

| Workload | Scheduler Strategy | TCP Strategy | I/O Strategy |
|----------|-------------------|--------------|--------------|
| Desktop | CFS | CUBIC | CFQ |
| Server | CFS + RT | Various | Deadline |
| Real-time | RT/Deadline | - | NOOP |
| Embedded | Minimal | Reno | NOOP |

No single algorithm is optimal for all cases. Strategy pattern allows:
- **Runtime selection**: Choose algorithm based on workload
- **Coexistence**: Multiple strategies active simultaneously
- **Evolution**: Add new algorithms without changing core

---

## Version

This guide targets **Linux kernel v3.2**, released January 2012.

All source code references and examples are based on this version.
