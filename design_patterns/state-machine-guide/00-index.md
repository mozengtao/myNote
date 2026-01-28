# State Machine Pattern in Linux Kernel (v3.2)

Explicit state transitions for managing object lifecycles and protocols.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-netdev-states-case.md](03-netdev-states-case.md) | Case 1: Network Device States |
| [04-tcp-states-case.md](04-tcp-states-case.md) | Case 2: TCP Connection States |
| [05-usb-states-case.md](05-usb-states-case.md) | Case 3: USB Device States |
| [06-unified-skeleton.md](06-unified-skeleton.md) | Unified Skeleton |
| [07-vs-flags.md](07-vs-flags.md) | State Machine vs Boolean Flags |
| [08-antipatterns.md](08-antipatterns.md) | Anti-Patterns |
| [09-reading-guide.md](09-reading-guide.md) | Source Reading Guide |
| [10-mental-model.md](10-mental-model.md) | Final Mental Model |

---

## Overview

```
    STATE MACHINE:
    ==============
    - Object has defined states
    - Only certain transitions allowed
    - Actions triggered on transitions
    - Invalid transitions rejected
```

**中文说明：**

状态机模式管理对象的生命周期。对象有定义的状态，只允许特定转换，转换时触发动作。

---

## Version

This guide targets **Linux kernel v3.2**.
