# Command/Deferred Execution Pattern in Linux Kernel (v3.2)

Deferred and queued execution for work that cannot run in current context.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-workqueue-case.md](03-workqueue-case.md) | Case 1: Workqueues |
| [04-tasklet-case.md](04-tasklet-case.md) | Case 2: Tasklets |
| [05-softirq-case.md](05-softirq-case.md) | Case 3: Softirqs |
| [06-unified-skeleton.md](06-unified-skeleton.md) | Unified Skeleton |
| [07-vs-direct.md](07-vs-direct.md) | Deferred vs Direct |
| [08-antipatterns.md](08-antipatterns.md) | Anti-Patterns |
| [09-reading-guide.md](09-reading-guide.md) | Source Reading Guide |
| [10-mental-model.md](10-mental-model.md) | Final Mental Model |

---

## Overview

```
    IRQ Handler (can't sleep)  -->  schedule  -->  Worker (can sleep)
```

**中文说明：**

延迟执行：中断处理程序调度工作，在安全上下文中执行。

---

## Version

This guide targets **Linux kernel v3.2**.
