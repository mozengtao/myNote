# Object Pool (SLAB) Pattern in Linux Kernel (v3.2)

Pre-allocated object caching for efficient frequent allocations.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-slab-case.md](03-slab-case.md) | Case 1: SLAB Allocator |
| [04-mempool-case.md](04-mempool-case.md) | Case 2: Memory Pool |
| [05-unified-skeleton.md](05-unified-skeleton.md) | Unified Skeleton |
| [06-antipatterns.md](06-antipatterns.md) | Anti-Patterns |
| [07-reading-guide.md](07-reading-guide.md) | Source Reading Guide |
| [08-mental-model.md](08-mental-model.md) | Final Mental Model |

---

## Overview

SLAB allocator provides efficient same-size allocations: kmem_cache_create, kmem_cache_alloc, kmem_cache_free.

**中文说明：**

SLAB分配器提供高效的相同大小分配。

---

## Version

This guide targets **Linux kernel v3.2**.
