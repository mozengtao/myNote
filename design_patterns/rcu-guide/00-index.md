# RCU (Read-Copy-Update) Pattern in Linux Kernel (v3.2)

Lock-free read-side access for read-heavy concurrent data structures.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-dcache-case.md](03-dcache-case.md) | Case 1: dcache |
| [04-routing-case.md](04-routing-case.md) | Case 2: Routing Table |
| [05-unified-skeleton.md](05-unified-skeleton.md) | Unified Skeleton |
| [06-antipatterns.md](06-antipatterns.md) | Anti-Patterns |
| [07-reading-guide.md](07-reading-guide.md) | Source Reading Guide |
| [08-mental-model.md](08-mental-model.md) | Final Mental Model |

---

## Overview

RCU: readers no lock, writers copy-modify-swap-wait-free.

**中文说明：**

RCU：读者无锁，写者复制修改替换等待释放。

---

## Version

This guide targets **Linux kernel v3.2**.
