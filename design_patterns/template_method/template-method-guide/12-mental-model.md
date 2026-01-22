# Final Mental Model

## The Key Question

> **"When I see a function pointer table in Linux kernel C code, how do I decide whether this is Template Method or not?"**

---

## The Decision Process

```
+=============================================================================+
|                    TEMPLATE METHOD DECISION FLOWCHART                        |
+=============================================================================+

                    +----------------------------+
                    | See function pointer table |
                    | (struct xxx_operations)    |
                    +----------------------------+
                               |
                               v
              +--------------------------------+
              | Is there a framework function  |
              | that wraps calls to this table?|
              | (e.g., vfs_read, dev_queue_    |
              | xmit, device_add)              |
              +--------------------------------+
                     |                |
                    YES               NO
                     |                |
                     v                v
    +------------------+    +------------------+
    | Does wrapper do  |    | NOT Template     |
    | PRE-hook work?   |    | Method           |
    | (lock, validate, |    | (raw dispatch or |
    | security check)  |    | Strategy pattern)|
    +------------------+    +------------------+
           |       |
          YES      NO
           |       |
           v       v
    +-----------+  +-----------+
    | Does wrap-|  | Probably   |
    | per do    |  | not full   |
    | POST-hook |  | Template   |
    | work?     |  | Method     |
    +-----------+  +-----------+
       |     |
      YES    NO
       |     |
       v     v
    +--------+  +------------+
    |TEMPLATE|  | Partial or |
    | METHOD |  | degenerate |
    +--------+  | Template   |
                +------------+
```

**中文说明：**

决策流程图：看到函数指针表时，首先问"是否有框架函数包装对此表的调用？"如果没有，则不是模板方法（是原始分发或策略模式）。如果有，继续问"包装函数是否做钩子前工作（加锁、验证、安全检查）？"如果做，再问"是否做钩子后工作？"如果两者都做，则是模板方法。如果只做部分，则是部分或退化的模板方法。

---

## The One-Paragraph Mental Model

When examining a function pointer table (ops structure) in the Linux kernel, determine whether it implements Template Method by asking: **"Does a framework function exist that (1) serves as the sole entry point, (2) performs mandatory setup before calling the hook, and (3) performs mandatory cleanup after the hook returns?"** If all three conditions are met, and the implementation cannot bypass any of these steps, you have found a Template Method. The pattern is about **control flow ownership**—the framework owns the algorithm skeleton (entry, locking, validation, notification, cleanup), while the implementation only fills in the specific work. If instead the implementation owns the entry point, or if the ops table methods are called directly without wrapping, or if the entire algorithm is delegated without pre/post steps, you have something other than Template Method (likely Strategy pattern or raw polymorphism).

**中文说明：**

检查内核中的函数指针表时，通过以下问题判断是否是模板方法：**"是否存在一个框架函数，它(1)作为唯一入口点，(2)在调用钩子前执行强制设置，(3)在钩子返回后执行强制清理？"** 如果三个条件都满足，且实现无法绕过任何步骤，则是模板方法。模式的核心是**控制流所有权**——框架拥有算法骨架（入口、锁、验证、通知、清理），实现只填充具体工作。如果实现拥有入口点、或ops表方法被直接调用而没有包装、或整个算法被委托而没有前后步骤，则是其他模式（可能是策略模式或原始多态）。

---

## Visual Mental Model

```
+=============================================================================+
|                    THE TEMPLATE METHOD MENTAL MODEL                          |
+=============================================================================+

              USER/CALLER
                   |
                   | calls
                   v
         +-----------------+
         |  FRAMEWORK      |     <-- LOOK FOR THIS
         |  ENTRY POINT    |
         +-----------------+
                   |
        +----------+----------+
        |                     |
        v                     |
    +-------+                 |
    | PRE   |  <-- AND THIS   |
    | STEPS |                 |
    +-------+                 |
        |                     |
        v                     |
    +=======+                 |
    || HOOK || --> impl()     |    IF ALL THREE EXIST:
    +=======+                 |    TEMPLATE METHOD
        |                     |
        v                     |
    +-------+                 |
    | POST  |  <-- AND THIS   |
    | STEPS |                 |
    +-------+                 |
        |                     |
        +---------+-----------+
                  |
                  v
              RESULT


    THE QUESTION TO ASK:

    +----------------------------------------------------------+
    |                                                          |
    |  "Who owns the entry point?"                             |
    |                                                          |
    |  Framework owns it  -->  Likely Template Method          |
    |  Implementation owns it  -->  NOT Template Method        |
    |                                                          |
    +----------------------------------------------------------+

    THE SECOND QUESTION:

    +----------------------------------------------------------+
    |                                                          |
    |  "Can implementation skip pre or post steps?"            |
    |                                                          |
    |  No, framework enforces them  -->  Template Method       |
    |  Yes, implementation controls  -->  NOT Template Method  |
    |                                                          |
    +----------------------------------------------------------+
```

**中文说明：**

视觉心智模型：寻找(1)框架入口点、(2)前置步骤、(3)后置步骤。如果三者都存在，则是模板方法。要问的第一个问题："谁拥有入口点？"框架拥有则可能是模板方法，实现拥有则不是。第二个问题："实现能否跳过前置或后置步骤？"不能则是模板方法，能则不是。

---

## Quick Reference Card

```
+=============================================================================+
|                    TEMPLATE METHOD QUICK REFERENCE                           |
+=============================================================================+

    IS IT TEMPLATE METHOD?
    
    [Y] Framework function wraps ops table calls
    [Y] Pre-hook: locking, validation, security
    [Y] Post-hook: cleanup, notification, statistics
    [Y] Implementation cannot bypass framework
    [Y] Single hook per framework function

    NOT TEMPLATE METHOD IF:

    [X] Implementation is called directly
    [X] No framework wrapper exists
    [X] Implementation controls entry point
    [X] Multiple hooks compose algorithm (Strategy)
    [X] No mandatory pre/post steps

    COMMON TEMPLATE METHODS:

    VFS:     vfs_read()      wraps f_op->read
    VFS:     vfs_write()     wraps f_op->write
    NET TX:  dev_queue_xmit  wraps ndo_start_xmit
    NAPI:    net_rx_action   wraps napi->poll
    BLOCK:   submit_bio      wraps make_request_fn
    DEVICE:  device_add      leads to drv->probe

    NOT TEMPLATE METHOD:

    Scheduler:  schedule() uses multiple sched_class ops
    TCP CC:     Various calls to tcp_congestion_ops
    I/O Sched:  elevator_ops methods called directly
```

---

## The Essence

```
+=============================================================================+
|                                                                             |
|  TEMPLATE METHOD IN LINUX KERNEL =                                          |
|                                                                             |
|      FRAMEWORK OWNS CONTROL FLOW                                            |
|      +                                                                      |
|      IMPLEMENTATION PROVIDES SPECIFIC BEHAVIOR                              |
|      +                                                                      |
|      FRAMEWORK ENFORCES INVARIANTS                                          |
|                                                                             |
|  This is NOT about inheritance.                                             |
|  This is about WHO CONTROLS WHAT HAPPENS.                                   |
|                                                                             |
+=============================================================================+
```

**中文说明：**

Linux内核中模板方法的本质 = 框架拥有控制流 + 实现提供特定行为 + 框架强制执行不变量。这不是关于继承，而是关于**谁控制发生什么**。

---

## Final Checklist

Before concluding "this is Template Method", verify:

1. **Entry point ownership**: Framework function is what callers invoke
2. **Pre-hook phase**: Framework does setup before calling ops
3. **Single hook**: One ops function called per framework function
4. **Post-hook phase**: Framework does cleanup after ops returns
5. **Invariant enforcement**: Implementation cannot bypass framework steps

If all five are true, you have identified Template Method in the Linux kernel.

---

## Remember

> **Template Method is not a coding technique—it is an architectural decision about who owns the control flow.**
>
> In the Linux kernel, the answer is always: **the framework owns the control flow, the implementation provides the variation.**
>
> This is why Linux kernel code is maintainable, secure, and consistent across thousands of drivers and filesystems.
