# Source Reading Guide: Composite Pattern

A guided path through Linux kernel v3.2 source code.

---

## Reading Path Overview

```
    PHASE 1: Kobject Core
    =====================
    include/linux/kobject.h    <- Structures
    lib/kobject.c              <- Implementation
    
    PHASE 2: Device Model
    =====================
    include/linux/device.h     <- struct device
    drivers/base/core.c        <- device_add/del
    
    PHASE 3: Sysfs Integration
    ==========================
    fs/sysfs/                  <- Sysfs filesystem
```

---

## Phase 1: Kobject Core

### File: include/linux/kobject.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct kobject:
    - parent pointer
    - kset pointer
    - kref (reference count)
    
    struct kset:
    - list (children)
    - kobj (embedded kobject)
    
    struct kobj_type:
    - release function
    - sysfs_ops
```

### File: lib/kobject.c

```
    WHAT TO LOOK FOR:
    =================
    
    kobject_init():
    - How kobject is initialized
    
    kobject_add():
    - How parent is set
    - How sysfs entry is created
    
    kobject_put():
    - Reference counting
    - When release is called
```

**中文说明：**

阶段1：Kobject核心。在kobject.h中查找kobject、kset、kobj_type结构定义。在kobject.c中学习初始化、添加、引用计数如何工作。

---

## Phase 2: Device Model

### File: include/linux/device.h

```
    WHAT TO LOOK FOR:
    =================
    
    struct device:
    - parent pointer
    - kobj (embedded kobject)
    - klist_children
```

### File: drivers/base/core.c

```
    WHAT TO LOOK FOR:
    =================
    
    device_add():
    - How device joins hierarchy
    - kobject_add call
    
    device_del():
    - How device leaves hierarchy
```

---

## Key Functions to Trace

| Function | File | Purpose |
|----------|------|---------|
| `kobject_init()` | lib/kobject.c | Initialize kobject |
| `kobject_add()` | lib/kobject.c | Add to hierarchy |
| `kobject_del()` | lib/kobject.c | Remove from hierarchy |
| `kobject_put()` | lib/kobject.c | Release reference |
| `device_add()` | drivers/base/core.c | Add device |

---

## Tracing Exercise

```
    TRACE: Device Addition
    ======================
    
    1. Start at device_add() in drivers/base/core.c
    
    2. Find where parent is set
    
    3. Find kobject_add() call
    
    4. Trace sysfs directory creation
    
    5. Understand the complete hierarchy formation
```

---

## Reading Checklist

```
    [ ] Read struct kobject definition
    [ ] Read kobject_init implementation
    [ ] Read kobject_add implementation
    [ ] Read struct device definition
    [ ] Trace device_add hierarchy setup
```

---

## Version

This reading guide is for **Linux kernel v3.2**.
