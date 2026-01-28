# Factory Pattern in Linux Kernel (v3.2)

Centralized object creation with encapsulated allocation logic.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-netdev-case.md](03-netdev-case.md) | Case 1: alloc_netdev() |
| [04-skb-case.md](04-skb-case.md) | Case 2: alloc_skb() |
| [05-unified-skeleton.md](05-unified-skeleton.md) | Unified Skeleton |
| [06-antipatterns.md](06-antipatterns.md) | Anti-Patterns |
| [07-reading-guide.md](07-reading-guide.md) | Source Reading Guide |
| [08-mental-model.md](08-mental-model.md) | Final Mental Model |

---

## Overview

Factory functions encapsulate object creation: alloc_netdev(), alloc_skb(), alloc_disk().

**中文说明：**

工厂模式封装对象创建。

---

## Version

This guide targets **Linux kernel v3.2**.
