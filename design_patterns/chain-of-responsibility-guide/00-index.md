# Chain of Responsibility Pattern in Linux Kernel (v3.2)

Handler chains where each handler can process or pass along.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-netfilter-case.md](03-netfilter-case.md) | Case 1: Netfilter Hooks |
| [04-irq-case.md](04-irq-case.md) | Case 2: Shared IRQ Handling |
| [05-unified-skeleton.md](05-unified-skeleton.md) | Unified Skeleton |
| [06-vs-observer.md](06-vs-observer.md) | Chain vs Observer |
| [07-antipatterns.md](07-antipatterns.md) | Anti-Patterns |
| [08-reading-guide.md](08-reading-guide.md) | Source Reading Guide |
| [09-mental-model.md](09-mental-model.md) | Final Mental Model |

---

## Overview

```
+=============================================================================+
|                    CHAIN OF RESPONSIBILITY PATTERN                           |
+=============================================================================+

    Request --> Handler1 --> Handler2 --> Handler3
                   |            |
              pass along    HANDLE IT (stop)

    DIFFERS FROM OBSERVER:
    ======================

    Observer: ALL handlers called (broadcast)
    Chain:    ONE handler processes (first match wins)
```

**中文说明：**

责任链模式让多个处理器有机会处理请求。与观察者不同：观察者是广播，责任链是第一个能处理的处理器处理。

---

## Version

This guide targets **Linux kernel v3.2**.
