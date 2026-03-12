# Linux Kernel v3.2 — Clean Architecture Analysis

## Overview

This document series analyzes the Linux kernel v3.2.0 ("Saber-toothed Squirrel")
source code through the lens of **Clean Architecture** (Robert C. Martin). The
analysis maps real kernel constructs — structs, function pointer tables, execution
paths, and include relationships — to the four concentric layers of Clean
Architecture.

The kernel was never designed using Clean Architecture terminology, yet it
converges on the same core principles: **stable policy lives at the center;
volatile implementation lives at the periphery; dependencies point inward**.

---

## Clean Architecture Layer Model Applied to the Kernel

```
+--------------------------------------------------------------+
|  OUTER LAYER (Volatile Implementation)                       |
|  Drivers, Filesystems, Arch code, NIC drivers, SELinux impl  |
+--------------------------------------------------------------+
|  INTERFACE ADAPTERS                                          |
|  file_operations, sched_class, proto_ops, net_device_ops,    |
|  vm_operations_struct, irq_chip, security_operations,        |
|  clocksource, bus_type                                       |
+--------------------------------------------------------------+
|  USE CASES (Subsystem Policy Logic)                          |
|  schedule(), vfs_read(), handle_mm_fault(), ip_rcv(),        |
|  driver_register(), generic_handle_irq(), run_local_timers() |
+--------------------------------------------------------------+
|  ENTITIES (Most Stable Core)                                 |
|  task_struct, inode, sk_buff, page, vm_area_struct,          |
|  irq_desc, device, timer_list                                |
+--------------------------------------------------------------+
```

### Dependency Rule

In the kernel, the Dependency Rule manifests as:

- **Outer layers include inner-layer headers**, never the reverse.
- `fs/ext4/file.c` includes `linux/fs.h` (where `file_operations` is defined).
- `linux/fs.h` does **not** include anything from `fs/ext4/`.
- `drivers/net/e1000/` includes `linux/netdevice.h` (where `net_device_ops` lives).
- `linux/netdevice.h` has no knowledge of any specific driver.

This is the C-language equivalent of the Dependency Inversion Principle: the
inner layer defines the interface (the struct of function pointers); the outer
layer provides the concrete implementation by filling in those function pointers.

### Kernel's Architectural Technique: Function Pointer Structs

The kernel's primary mechanism for dependency inversion is the **struct of
function pointers**. This is the C equivalent of an abstract interface / vtable:

| Interface Struct            | Defined In                    | Implemented By                     |
|-----------------------------|-------------------------------|------------------------------------|
| `struct file_operations`    | `include/linux/fs.h`         | ext4, xfs, tmpfs, procfs, ...      |
| `struct sched_class`        | `include/linux/sched.h`      | CFS, RT, idle, stop                |
| `struct vm_operations_struct` | `include/linux/mm.h`       | filemap, shmem, device drivers     |
| `struct proto_ops`          | `include/linux/net.h`        | inet_stream_ops, inet_dgram_ops    |
| `struct net_device_ops`     | `include/linux/netdevice.h`  | e1000, ixgbe, virtio-net, ...      |
| `struct security_operations`| `include/linux/security.h`   | SELinux, AppArmor, SMACK           |
| `struct irq_chip`           | `include/linux/irq.h`        | APIC, GIC, PIC, ...               |
| `struct clocksource`        | `include/linux/clocksource.h`| TSC, HPET, ACPI PM timer, ...     |
| `struct bus_type`           | `include/linux/device.h`     | PCI, USB, platform, I2C, ...      |

---

## Document Structure

The full analysis is split into two parts:

1. **[Part 1](clean_architecture_analysis_part1.md)** — Process Management,
   Memory Management, Virtual File System, Network Stack
2. **[Part 2](clean_architecture_analysis_part2.md)** — Device Driver Model,
   Security Framework, Interrupt Handling, Time Management

Each subsystem analysis follows this structure:

1. Subsystem Overview
2. Entities (Stable Layer)
3. Use Cases (Policy Logic)
4. Interface Adapters (Dependency Inversion Points)
5. Outer Implementation Layer
6. Execution Flow Analysis
7. Dependency Flow Analysis
8. Architecture Diagram
9. Clean Architecture Insights

---

## Key Findings Summary

### Where the Kernel Aligns with Clean Architecture

1. **Dependency Inversion is pervasive.** Every major subsystem uses function
   pointer structs to decouple policy from mechanism.

2. **The Entity layer is remarkably stable.** `task_struct`, `inode`, `sk_buff`,
   and `page` have existed since the earliest Linux versions and change slowly.

3. **Plug-in architecture is the norm.** Filesystems, schedulers, security
   modules, drivers, and network protocols all register themselves into the
   kernel through well-defined interfaces — acting as interchangeable plugins.

4. **Execution flows outward; dependencies point inward.** A `read()` syscall
   flows from VFS core → `file_operations` → ext4, but ext4 depends on VFS
   headers, not the reverse.

### Where the Kernel Diverges from Strict Clean Architecture

1. **Performance shortcuts.** The kernel occasionally bypasses abstraction
   layers for performance. The CFS scheduler fast-path in `pick_next_task()`
   skips the class chain iteration when all tasks are CFS tasks.

2. **Circular awareness.** Some entity structs (like `task_struct`) contain
   fields related to many subsystems (scheduling, memory, signals, credentials),
   creating a god-object that Clean Architecture would frown upon.

3. **Include-based compilation units.** The scheduler uses `#include "sched_fair.c"`
   to compose a single translation unit, breaking the usual file-boundary
   separation assumed by Clean Architecture's module concept.

4. **Global state.** Per-CPU variables, global singletons like `security_ops`,
   and the process table are architectural necessities that violate strict
   layering.

5. **No runtime swappability for some interfaces.** While filesystems and
   drivers can be loaded/unloaded, the scheduler class and LSM module are
   typically fixed at boot time.

---

*Analysis based on Linux kernel source tree at `/home/morrism/repos/linux`,
version 3.2.0.*
