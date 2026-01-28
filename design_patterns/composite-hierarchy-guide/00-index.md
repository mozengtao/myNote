# Composite/Hierarchy Pattern in Linux Kernel (v3.2)

Tree structures for hierarchical object relationships.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-kobject-case.md](03-kobject-case.md) | Case 1: kobject/kset |
| [04-device-case.md](04-device-case.md) | Case 2: Device Hierarchy |
| [05-unified-skeleton.md](05-unified-skeleton.md) | Unified Skeleton |
| [06-antipatterns.md](06-antipatterns.md) | Anti-Patterns |
| [07-reading-guide.md](07-reading-guide.md) | Source Reading Guide |
| [08-mental-model.md](08-mental-model.md) | Final Mental Model |

---

## Overview

kobject/kset creates tree hierarchy with parent pointers.

**中文说明：**

kobject/kset用parent指针创建树层次结构。

---

## Version

This guide targets **Linux kernel v3.2**.
