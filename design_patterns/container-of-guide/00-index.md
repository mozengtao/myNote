# Container-of Pattern in Linux Kernel (v3.2)

The most fundamental pattern in Linux kernel programming — navigating from an embedded member to its containing structure.

## Table of Contents

| File | Topic |
|------|-------|
| [01-core-concept.md](01-core-concept.md) | Core Concept: Embedded Structure Navigation |
| [02-identification-rules.md](02-identification-rules.md) | Identification Rules |
| [03-list-head-case.md](03-list-head-case.md) | Case 1: list_head and Linked Lists |
| [04-hlist-case.md](04-hlist-case.md) | Case 2: hlist for Hash Tables |
| [05-kobject-case.md](05-kobject-case.md) | Case 3: kobject Embedding |
| [06-work-struct-case.md](06-work-struct-case.md) | Case 4: work_struct in Workqueues |
| [07-unified-skeleton.md](07-unified-skeleton.md) | Unified Skeleton |
| [08-vs-inheritance.md](08-vs-inheritance.md) | Container-of vs OOP Inheritance |
| [09-antipatterns.md](09-antipatterns.md) | Anti-Patterns |
| [10-reading-guide.md](10-reading-guide.md) | Source Reading Guide |
| [11-mental-model.md](11-mental-model.md) | Final Mental Model |

---

## Overview Diagram

```
+=============================================================================+
|                    CONTAINER_OF PATTERN                                      |
+=============================================================================+

    THE PROBLEM:
    ============

    +---------------------------+
    | struct my_device          |
    |   +-------------------+   |
    |   | name[32]          |   |
    |   +-------------------+   |
    |   | id                |   |
    |   +-------------------+   |
    |   | list  <-----------+---|--- You have pointer to THIS member
    |   +-------------------+   |
    |   | data              |   |
    +---------------------------+
    ^
    |
    +-- But you NEED pointer to the WHOLE structure


    THE SOLUTION:
    =============

    container_of(list_ptr, struct my_device, list)
    
    +---------------------------+
    | struct my_device          |  <-- Returns pointer HERE
    |   +-------------------+   |
    |   | name[32]          |   |  offset = 0
    |   +-------------------+   |
    |   | id                |   |  offset = 32
    |   +-------------------+   |
    |   | list  <-----------+---|  offset = 36 (list_ptr points here)
    |   +-------------------+   |
    |   | data              |   |  offset = 52
    +---------------------------+

    CALCULATION:
    container_start = list_ptr - offsetof(struct my_device, list)
                    = list_ptr - 36
```

**中文说明：**

container_of模式解决的问题：你有一个指向结构体成员的指针，但需要整个结构体的指针。解决方案：`container_of(ptr, type, member)`通过计算成员在结构体中的偏移量，从成员指针反推出容器结构体的起始地址。这是Linux内核中最基础的模式，几乎所有链表、哈希表、设备模型都依赖于此模式。

---

## Why This Pattern Exists

### The C Language Constraint

```
    C HAS NO INHERITANCE
    ====================

    In C++/Java:                    In C (Linux Kernel):
    ==============                  ====================

    class Device {                  struct device {
        // base class                   // just data
    };                              };

    class MyDevice : Device {       struct my_device {
        // inherits from Device         struct device dev;  // EMBED it
    };                                  // my own fields
                                    };

    MyDevice* d = ...;              struct my_device *md = ...;
    Device* base = d;               struct device *base = &md->dev;
    // Automatic!                   // Manual embedding

    // But how to go back?          // How to go back?
    MyDevice* d = (MyDevice*)base;  // container_of() !
    // Works only if correct type
```

### The Kernel's Solution

```
    EMBED AND RECOVER:

    1. EMBED the generic structure inside the specific one
    2. Use container_of() to RECOVER the specific structure

    This gives C the power of:
    - Polymorphism (via function pointers)
    - Type hierarchies (via embedding)
    - Generic algorithms (via container_of)
```

**中文说明：**

C语言没有继承机制。在C++/Java中可以通过继承实现基类到子类的转换。在C语言的Linux内核中，通过"嵌入"（embedding）实现类似功能：将通用结构体嵌入到特定结构体中，然后用`container_of()`从通用结构体指针恢复到特定结构体指针。这种方式给C语言带来了多态性、类型层次和通用算法的能力。

---

## Prerequisites

This guide assumes familiarity with:

- C pointer arithmetic
- Structure layout and padding
- `offsetof()` macro
- Linux kernel linked list basics

---

## Key Terminology

| Term | Meaning |
|------|---------|
| **Container** | The outer/enclosing structure |
| **Member** | The embedded/inner structure |
| **Embedding** | Placing one struct inside another (not via pointer) |
| **Intrusive** | Data structure where nodes are embedded in user structs |
| **offsetof** | Macro to compute byte offset of member in struct |

---

## The Macro Definition

```c
/* include/linux/kernel.h */

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})
```

### How It Works

1. `typeof(((type *)0)->member)` — Get the type of the member
2. `const ... *__mptr = (ptr)` — Type-check: ensure ptr matches member type
3. `(char *)__mptr` — Cast to char* for byte-level arithmetic
4. `offsetof(type, member)` — Get byte offset of member in type
5. Subtract offset to get start of container

---

## Version

This guide targets **Linux kernel v3.2**.
