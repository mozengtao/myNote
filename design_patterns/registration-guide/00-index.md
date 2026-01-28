# Registration Pattern in Linux Kernel (v3.2)

Subsystem registration for drivers, filesystems, and protocols.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-driver-case.md](03-driver-case.md) | Case 1: Driver Registration |
| [04-filesystem-case.md](04-filesystem-case.md) | Case 2: Filesystem Registration |
| [05-unified-skeleton.md](05-unified-skeleton.md) | Unified Skeleton |
| [06-antipatterns.md](06-antipatterns.md) | Anti-Patterns |
| [07-reading-guide.md](07-reading-guide.md) | Source Reading Guide |
| [08-mental-model.md](08-mental-model.md) | Final Mental Model |

---

## Overview

Components register with subsystems: pci_register_driver, register_filesystem.

**中文说明：**

注册模式：组件向子系统注册自己。

---

## Version

This guide targets **Linux kernel v3.2**.
