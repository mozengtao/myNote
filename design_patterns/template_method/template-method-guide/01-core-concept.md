# Core Concept: What Template Method Means in Linux Kernel

## The Problem Template Method Solves

```
+=============================================================================+
|                    THE FUNDAMENTAL KERNEL PROBLEM                           |
+=============================================================================+

    WITHOUT TEMPLATE METHOD                 WITH TEMPLATE METHOD
    ======================                  ====================

    +----------------+                      +----------------+
    |    Driver A    |                      |   CORE LAYER   |
    +----------------+                      +----------------+
    | 1. lock()      |                      | 1. lock()      |
    | 2. check()     |  Each driver         | 2. check()     |
    | 3. do_work()   |  reimplements        | 3. hook()  ----+---> driver_work()
    | 4. unlock()    |  everything          | 4. unlock()    |
    +----------------+                      +----------------+
                                                    |
    +----------------+                              |
    |    Driver B    |                      SINGLE SOURCE OF TRUTH
    +----------------+                      FOR CONTROL LOGIC
    | 1. lock()      |  <- Different
    | 2. do_work()   |  <- order!
    | 3. check()     |  <- BUG!
    | 4. unlock()    |
    +----------------+

    PROBLEM:                                SOLUTION:
    - Duplicated control logic              - Control logic in one place
    - Inconsistent ordering                 - Guaranteed ordering
    - Security holes                        - Centralized security
    - Maintenance nightmare                 - Easy to audit
```

**中文说明：**

上图对比了没有模板方法和使用模板方法的情况。左侧展示了问题：每个驱动程序独立实现全部控制逻辑，导致代码重复、顺序不一致（例如驱动B在检查前就执行了工作，这是Bug）、安全漏洞和维护困难。右侧展示了解决方案：核心层统一管理控制逻辑，驱动程序仅提供实际工作函数。这确保了单一事实来源、保证执行顺序、集中安全检查、易于审计。

---

## What Template Method Means in Kernel Terms

In the Linux kernel, **Template Method** is a structural pattern where:

1. **Core layer** defines a fixed algorithm skeleton
2. **Implementation layer** provides specific behavior at designated points
3. **Control flow never transfers to implementation**—the implementation is called, does work, and returns

This is fundamentally about **control flow ownership**:

| Aspect | Core Layer Owns | Implementation Provides |
|--------|-----------------|------------------------|
| Entry point | Yes | No |
| Locking | Yes | No |
| Permission checks | Yes | No |
| Pre-conditions | Yes | No |
| Post-conditions | Yes | No |
| Error recovery | Yes | Limited |
| Actual work | No | Yes |

---

## Why Linux Kernel Needs Template Method

### 1. Untrusted Code Boundary

```
+=============================================================================+
|                         TRUST BOUNDARY                                       |
+=============================================================================+

                    KERNEL CORE (Trusted)
    +----------------------------------------------------------+
    |                                                          |
    |   +------------------+    +------------------+            |
    |   |   VFS Core       |    |   Net Core       |            |
    |   +------------------+    +------------------+            |
    |           |                       |                      |
    |           v                       v                      |
    |   +-----------------+    +-----------------+             |
    |   | Security Hooks  |    | Packet Checks   |             |
    |   +-----------------+    +-----------------+             |
    |           |                       |                      |
    +-----------|-----------------------|----------------------+
                |                       |
    ============|=======================|======================  TRUST BOUNDARY
                |                       |
    +-----------|-----------------------|----------------------+
    |           v                       v                      |
    |   +------------------+    +------------------+            |
    |   |  Filesystem      |    |  Network Driver  |            |
    |   |  (ext4, nfs...)  |    |  (e1000, rtl...) |            |
    |   +------------------+    +------------------+            |
    |                                                          |
    |                 MODULES (Less Trusted)                   |
    +----------------------------------------------------------+
```

**中文说明：**

Linux内核存在信任边界。核心层（VFS、网络核心等）是受信任的，由内核开发者维护。模块层（文件系统、网络驱动等）信任度较低，可能由第三方编写。模板方法确保核心层在调用不太受信任的代码前后执行安全检查，防止模块代码绕过安全机制。

Drivers and filesystems may be:
- Written by third parties
- Less rigorously reviewed
- Potentially buggy or malicious

Template Method ensures:
- Security checks happen BEFORE driver code runs
- Cleanup happens AFTER driver code returns
- Driver code cannot skip mandatory steps

### 2. Locking Correctness

```
+=============================================================================+
|                      LOCKING GUARANTEE                                       |
+=============================================================================+

    vfs_read() Template                    WHY THIS MATTERS
    ==================                     ================

    +---------------------------+
    |  1. mutex_lock(&inode)    |  <-- Framework acquires lock
    +---------------------------+
              |
              v
    +---------------------------+
    |  2. security_file_read()  |  <-- Security check under lock
    +---------------------------+
              |
              v
    +---------------------------+
    |  3. f_op->read()          |  <-- Driver runs under lock
    +---------------------------+
              |
              v
    +---------------------------+
    |  4. mutex_unlock(&inode)  |  <-- Framework releases lock
    +---------------------------+

    DRIVER CANNOT:                         IF DRIVER CONTROLLED LOCKING:
    - Forget to lock                       - Might forget to lock
    - Unlock too early                     - Might unlock wrong lock
    - Double lock                          - Might deadlock
    - Skip security check                  - Might skip checks
```

**中文说明：**

上图展示了模板方法如何保证锁的正确性。框架负责获取和释放锁，驱动程序在持锁状态下运行。驱动程序无法忘记加锁、提前解锁、重复加锁或跳过安全检查。如果让驱动程序控制锁，可能出现忘记加锁、锁错误的锁、死锁或跳过检查等问题。

### 3. Lifecycle Guarantees

The kernel manages complex object lifecycles:

```
    DEVICE LIFECYCLE (Framework Controlled)
    =======================================

    device_add()
        |
        +---> kobject_add()           [framework]
        |
        +---> bus_add_device()        [framework]
        |
        +---> device_create_file()    [framework]
        |
        +---> bus_probe_device()
                |
                +---> drv->probe()    [HOOK: driver code]
        |
        +---> uevent notification     [framework]

    DRIVER CANNOT CHANGE THIS ORDER
    DRIVER CANNOT SKIP STEPS
    DRIVER ONLY IMPLEMENTS probe()
```

**中文说明：**

设备生命周期由框架严格控制。`device_add()`依次调用kobject注册、总线添加、sysfs文件创建，然后才调用驱动的`probe()`钩子，最后发送uevent通知。驱动程序不能改变这个顺序，不能跳过步骤，只能实现`probe()`函数。

---

## Template Method vs Related Patterns

### Template Method vs Strategy

```
+=============================================================================+
|                   TEMPLATE METHOD vs STRATEGY                                |
+=============================================================================+

    TEMPLATE METHOD                        STRATEGY
    ===============                        ========

    +------------------+                   +------------------+
    |  framework_func  |                   |     caller       |
    +------------------+                   +------------------+
    | 1. pre_step()    |                   |                  |
    | 2. hook() -------+-> impl           | strategy->algo() |---> algo_impl
    | 3. post_step()   |                   |                  |
    +------------------+                   +------------------+

    FRAMEWORK CONTROLS:                    STRATEGY CONTROLS:
    - When hook runs                       - Entire algorithm
    - What happens before                  - Everything
    - What happens after

    USE WHEN:                              USE WHEN:
    - Order matters                        - Entire algorithm varies
    - Must enforce invariants              - No ordering constraints
    - Core owns lifecycle                  - Algorithms are independent

    EXAMPLE:                               EXAMPLE:
    - vfs_read() with f_op->read           - Scheduler policies
    - dev_queue_xmit() with ndo_xmit       - Compression algorithms
```

**中文说明：**

模板方法与策略模式的核心区别：模板方法中，框架控制钩子何时运行、前后发生什么；策略模式中，策略控制整个算法。模板方法用于顺序重要、必须强制执行不变量、核心层拥有生命周期的场景；策略模式用于整个算法可变、无顺序约束、算法相互独立的场景。

### Template Method vs Pure Callbacks

```
    PURE CALLBACK                          TEMPLATE METHOD
    =============                          ===============

    register_callback(my_func)             ops->hook = my_func
           |                                     |
           v                                     v
    event occurs                           framework_function() {
           |                                   pre_work()
           v                                   ops->hook()
    my_func() runs                             post_work()
    (no ordering guarantee)                }

    PROBLEM WITH PURE CALLBACKS:
    - No guaranteed ordering
    - No pre/post guarantees
    - Callback controls too much
```

**中文说明：**

纯回调与模板方法的区别：纯回调只是事件发生时调用函数，没有顺序保证，没有前置/后置保证，回调函数控制太多。模板方法将钩子嵌入到框架函数中，保证了执行顺序和前后处理。

### Template Method vs Function Pointer Tables

Not every ops table is Template Method:

```
    OPS TABLE WITHOUT TEMPLATE METHOD       OPS TABLE WITH TEMPLATE METHOD
    =================================       ==============================

    /* Just a vtable */                     /* Embedded in template */
    struct my_ops {                         void core_function(ctx) {
        void (*func_a)(ctx);                    lock(ctx)
        void (*func_b)(ctx);                    check_permissions(ctx)
    };                                          ops->hook(ctx)  // HOOK
                                               unlock(ctx)
    /* Caller decides order */              }
    ops->func_a(ctx);
    ops->func_b(ctx);                       /* Framework decides order */
```

**中文说明：**

并非所有ops表都是模板方法。左侧是单纯的虚函数表，调用者决定调用顺序。右侧是真正的模板方法，框架将钩子嵌入到控制逻辑中，框架决定顺序。区分标准：核心是否有包装函数定义固定的前后步骤。

---

## The Inversion of Control Principle

```
+=============================================================================+
|                    INVERSION OF CONTROL (IoC)                               |
+=============================================================================+

    TRADITIONAL (Library)                  IoC (Framework)
    =====================                  ================

    +------------------+                   +------------------+
    |   Application    |                   |    Framework     |
    +------------------+                   +------------------+
           |                                      |
           | calls                                | calls
           v                                      v
    +------------------+                   +------------------+
    |    Library       |                   |   Application    |
    +------------------+                   +------------------+

    "Don't call us,                        "Don't call us,
     we'll call you"                        we'll call you"

    LIBRARY:                               FRAMEWORK:
    - You call library                     - Framework calls you
    - You control flow                     - Framework controls flow
    - Library is passive                   - You are passive


    IN LINUX KERNEL:
    ================

    +------------------+                   +------------------+
    |    VFS Core      |                   |   Filesystem     |
    |    (Framework)   |  -- calls -->     |   (ext4, nfs)    |
    +------------------+                   +------------------+

    The VFS is NOT a library you call.
    The VFS is a framework that calls your filesystem.
```

**中文说明：**

控制反转（IoC）是模板方法的基础原则。传统库模式中，应用调用库，应用控制流程。框架模式中，框架调用应用，框架控制流程。在Linux内核中，VFS不是你调用的库，而是调用你的文件系统的框架。这就是"好莱坞原则"：别打电话给我们，我们会打给你。

---

## Why Template Method is NOT About Inheritance

In C++ or Java, Template Method typically uses inheritance:

```cpp
// C++ style (NOT how Linux does it)
class AbstractTemplate {
    void template_method() {
        pre_step();
        hook();      // virtual
        post_step();
    }
    virtual void hook() = 0;
};

class Concrete : public AbstractTemplate {
    void hook() override { /* implementation */ }
};
```

**Linux kernel uses composition, not inheritance:**

```c
// Linux kernel style
struct my_operations {
    int (*hook)(struct context *ctx);
};

void template_function(struct context *ctx, struct my_operations *ops) {
    pre_step(ctx);
    if (ops && ops->hook)
        ops->hook(ctx);
    post_step(ctx);
}
```

The pattern is the same—the mechanism differs:

| Aspect | OOP Inheritance | Linux Kernel |
|--------|-----------------|--------------|
| Variation mechanism | Virtual methods | Function pointers |
| Binding | Compile time or vtable | Runtime ops table |
| Fixed algorithm | In abstract class | In core function |
| Hook definition | Pure virtual method | Function pointer in struct |

**中文说明：**

C++/Java中模板方法通常使用继承和虚函数。Linux内核使用组合而非继承：通过函数指针结构体实现变化点。模式本质相同（固定算法+变化钩子），机制不同（虚函数表vs显式函数指针）。这是C语言实现多态的标准方式。

---

## Linux Kernel Vocabulary

| Term | Meaning in Template Method Context |
|------|-----------------------------------|
| **Core layer** | Code that defines the template method (VFS, net core, block core) |
| **Implementation layer** | Code that fills in the hooks (filesystems, drivers) |
| **Framework** | The core layer viewed as controlling entity |
| **Driver** | The implementation layer viewed as controlled entity |
| **Lifecycle** | Sequence of states; framework controls transitions |
| **Invariant** | Condition framework guarantees (e.g., "lock held during hook") |
| **Ordering constraint** | Requirement that steps happen in specific sequence |
| **Hook** | Function pointer called at variation point |
| **Ops table** | Structure containing hooks (`struct file_operations`) |

---

## Summary

Template Method in Linux kernel means:

1. **Core defines algorithm**: `vfs_read()`, `dev_queue_xmit()`, `submit_bio()`
2. **Core calls implementation at specific points**: `ops->read()`, `ops->ndo_start_xmit()`
3. **Core owns everything else**: locking, checking, lifecycle, ordering
4. **Implementation cannot change the algorithm**: only fill in specific behavior
5. **This is not about inheritance**: it is about control flow ownership

The key insight: **whoever owns the entry point owns the control flow.**

In Template Method, the framework always owns the entry point.
