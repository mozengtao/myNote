# Template Method Pattern in Linux Kernel (v3.2)

A comprehensive guide to understanding the Template Method design pattern as it is actually implemented in the Linux kernel using C language constructs.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept: What Template Method Means in Kernel |
| [02-identification-rules.md](02-identification-rules.md) | Kernel-Level Identification Rules |
| [03-vfs-case.md](03-vfs-case.md) | Case 1: VFS File Operations |
| [04-netdev-case.md](04-netdev-case.md) | Case 2: Network Device Transmit Path |
| [05-napi-case.md](05-napi-case.md) | Case 3: NAPI Polling Framework |
| [06-block-case.md](06-block-case.md) | Case 4: Block Layer I/O Submission |
| [07-device-model-case.md](07-device-model-case.md) | Case 5: Device Model Lifecycle |
| [08-unified-skeleton.md](08-unified-skeleton.md) | Minimal Unified Template Method Skeleton |
| [09-vs-strategy.md](09-vs-strategy.md) | Template Method vs Strategy |
| [10-antipatterns.md](10-antipatterns.md) | Common Kernel Anti-Patterns |
| [11-reading-guide.md](11-reading-guide.md) | Learning Path and Source Reading Guide |
| [12-mental-model.md](12-mental-model.md) | Final Mental Model |

---

## Overview Diagram

```
+==============================================================================+
|                    TEMPLATE METHOD IN LINUX KERNEL                           |
+==============================================================================+

                          CONTROL FLOW DIRECTION
                          ======================

    +------------------+         calls          +------------------+
    |   CORE LAYER     | ---------------------->|  IMPLEMENTATION  |
    |   (Framework)    |                        |     LAYER        |
    +------------------+                        +------------------+
    |                  |                        |                  |
    | - vfs_read()     |   ops->read()          | - ext4_read()    |
    | - dev_queue_xmit |   ops->ndo_start_xmit  | - e1000_xmit()   |
    | - napi_poll()    |   napi->poll()         | - driver_poll()  |
    | - submit_bio()   |   ops->make_request    | - blk_request()  |
    | - device_add()   |   drv->probe()         | - my_probe()     |
    |                  |                        |                  |
    +------------------+                        +------------------+
           |                                           ^
           |                                           |
           |    CORE OWNS:                             |
           |    - Entry point                          |
           |    - Pre-conditions                       |
           |    - Post-conditions                      |
           |    - Lifecycle                            |
           |    - Locking                              |
           |    - Error handling                       |
           |                                           |
           +-------------------------------------------+
                    IMPLEMENTATION PROVIDES:
                    - Specific behavior
                    - Hardware interaction
                    - Algorithm variation

+------------------------------------------------------------------------------+
|  KEY INSIGHT: The framework CALLS the implementation, never the reverse.     |
|  The implementation is a "guest" in the framework's execution context.       |
+------------------------------------------------------------------------------+
```

**中文说明：**

上图展示了Linux内核中模板方法模式的核心架构。控制流始终从核心层（框架）流向实现层，而非相反。核心层拥有执行入口点、前置条件检查、后置条件处理、生命周期管理、锁机制和错误处理。实现层仅提供特定行为（如硬件交互或算法变体）。关键洞察：框架调用实现，实现绝不反向调用框架的控制逻辑。实现代码是框架执行上下文中的"客人"。

---

## Prerequisites

This guide assumes familiarity with:

- C programming language
- Function pointers and ops tables
- Linux kernel subsystems (VFS, networking, block layer, device model)
- Reading kernel source code
- Basic understanding of kernel locking primitives

---

## Key Terminology

| Term | Kernel Meaning |
|------|----------------|
| **Core Layer** | Framework code that owns control flow (e.g., VFS, net core) |
| **Implementation Layer** | Driver or filesystem code that provides specific behavior |
| **Template Method** | Core function that defines fixed algorithm with variation points |
| **Hook** | Function pointer called at specific point in template |
| **Ops Table** | Structure containing function pointers for polymorphism |
| **Lifecycle** | Sequence of states an object transitions through |
| **Invariant** | Condition that must always hold true |

---

## Why This Guide Exists

Most design pattern literature explains Template Method using object-oriented inheritance in Java or C++. The Linux kernel uses C, which has no native inheritance mechanism. Yet the kernel extensively uses Template Method through:

- Function pointer tables (`struct file_operations`, `struct net_device_ops`)
- Framework functions that call hooks at specific points
- Strict separation between core control and implementation variation

Understanding this pattern is essential for:

- Writing correct kernel drivers
- Understanding kernel architecture
- Avoiding common anti-patterns that lead to bugs

---

## Version

This guide targets **Linux kernel v3.2**, released January 2012.

All source code references and examples are based on this version.
