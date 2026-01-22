# Final Mental Model

## The Key Question

> **"When I see a function pointer table in Linux kernel C code, how do I decide whether this is Strategy?"**

---

## The Decision Process

```
+=============================================================================+
|                    STRATEGY PATTERN DECISION FLOWCHART                       |
+=============================================================================+

                    +----------------------------+
                    | See function pointer table |
                    | (struct xxx_ops)           |
                    +----------------------------+
                               |
                               v
              +----------------------------------+
              | Does a single core operation     |
              | call MULTIPLE ops from this      |
              | table?                           |
              +----------------------------------+
                     |                |
                    YES               NO
                     |                |
                     v                v
    +------------------+    +------------------------+
    | Are the ops      |    | Is each op wrapped by  |
    | called WITHOUT   |    | a framework function   |
    | framework        |    | with pre/post steps?   |
    | pre/post steps?  |    +------------------------+
    +------------------+           |        |
           |       |              YES       NO
          YES      NO              |        |
           |       |               v        v
           v       v          +---------+ +--------+
    +----------+ +---------+  |TEMPLATE | |Unclear |
    | Does the | |Probably |  | METHOD  | |pattern |
    | impl own | |Template |  +---------+ +--------+
    | its own  | |Method   |
    | state?   | +---------+
    +----------+
       |     |
      YES    NO
       |     |
       v     v
    +--------+  +------------+
    |STRATEGY|  | Might be   |
    +--------+  | either     |
                +------------+
```

**中文说明：**

决策流程图：看到函数指针表时，首先问"单个核心操作是否调用此表中的多个ops？"如果是，继续问"ops是否不被框架前后步骤包装？"如果是，再问"实现是否拥有自己的状态？"如果都是，则是策略模式。如果单个核心操作只调用一个op，且被框架函数包装，则是模板方法。

---

## The One-Paragraph Mental Model

When examining a function pointer table (ops structure) in the Linux kernel, determine whether it implements Strategy by asking: **"Does the core delegate an entire algorithm to this ops table, calling multiple functions that work together as a coherent unit, without wrapping them in mandatory pre/post framework steps?"** If yes, and if the implementation maintains its own state machine and can be selected/replaced at runtime, you have found Strategy. The key insight is that **Strategy is about policy delegation** — the core knows WHEN to act but delegates the HOW to an interchangeable algorithm. If instead you see a single hook called per framework function, with mandatory pre/post steps enforced by the framework, you have Template Method, which is about **lifecycle control** — the framework owns the control flow and just allows the implementation to fill in one piece.

**中文说明：**

检查内核中的函数指针表时，通过以下问题判断是否是策略模式：**"核心是否将整个算法委托给此ops表，调用多个作为一个整体工作的函数，而不用强制的框架前后步骤包装它们？"** 如果是，且实现维护自己的状态机，可以在运行时选择/替换，则是策略模式。关键洞察：策略是关于策略委托——核心知道何时行动但将如何行动委托给可互换的算法。如果每个框架函数只调用一个钩子，且有框架强制的前后步骤，则是模板方法——框架拥有控制流，只允许实现填充一个部分。

---

## Visual Mental Model

```
+=============================================================================+
|                    THE STRATEGY PATTERN MENTAL MODEL                         |
+=============================================================================+

    STRATEGY = Policy Delegation
    ============================

                         CORE
                      (Mechanism)
                          |
                          | "I know WHEN, you decide HOW"
                          |
                          v
    +------------------------------------------+
    |            STRATEGY (Policy)             |
    +------------------------------------------+
    |                                          |
    |  +--------+  +--------+  +--------+      |
    |  | op_a() |  | op_b() |  | op_c() |      |
    |  +--------+  +--------+  +--------+      |
    |       \          |          /            |
    |        \         |         /             |
    |         \        |        /              |
    |          +-------+-------+               |
    |                  |                       |
    |         COMPLETE ALGORITHM               |
    |         (all ops work together)          |
    +------------------------------------------+


    THE THREE QUESTIONS:
    ====================

    +----------------------------------------------------------+
    |                                                          |
    |  1. "Does core call MULTIPLE ops for one operation?"     |
    |                                                          |
    |     YES --> Might be Strategy                            |
    |     NO  --> Probably Template Method                     |
    |                                                          |
    +----------------------------------------------------------+
    |                                                          |
    |  2. "Is there framework wrapping (pre/post steps)?"      |
    |                                                          |
    |     NO  --> Might be Strategy                            |
    |     YES --> Probably Template Method                     |
    |                                                          |
    +----------------------------------------------------------+
    |                                                          |
    |  3. "Does implementation maintain its own state?"        |
    |                                                          |
    |     YES --> Likely Strategy                              |
    |     NO  --> Likely Template Method                       |
    |                                                          |
    +----------------------------------------------------------+
```

**中文说明：**

视觉心智模型：策略=策略委托。核心（机制）说"我知道何时，你决定如何"，然后将决策委托给策略。策略包含多个协同工作的ops函数，形成完整算法。三个问题：(1) 核心是否为一个操作调用多个ops？是则可能是策略。(2) 是否有框架包装（前后步骤）？没有则可能是策略。(3) 实现是否维护自己的状态？是则可能是策略。

---

## Quick Reference Card

```
+=============================================================================+
|                    STRATEGY PATTERN QUICK REFERENCE                          |
+=============================================================================+

    IS IT STRATEGY?

    [Y] Multiple ops called together for one operation
    [Y] No mandatory pre/post framework wrapping
    [Y] Implementation maintains its own state
    [Y] Runtime algorithm selection supported
    [Y] Different implementations completely different

    NOT STRATEGY IF:

    [X] Each framework function calls exactly one op
    [X] Framework wraps with mandatory pre/post steps
    [X] Framework owns the data structures
    [X] No selection mechanism (ops fixed at object creation)
    [X] Ops are independent (don't work together)

    COMMON STRATEGIES:

    Scheduler:    sched_class       (CFS, RT, IDLE)
    TCP CC:       tcp_congestion_ops (CUBIC, Reno, Vegas)
    I/O Sched:    elevator_ops      (deadline, cfq, noop)
    Memory:       mempolicy modes   (default, bind, interleave)
    Security:     security_ops      (SELinux, AppArmor)

    NOT STRATEGY:

    VFS:          file_operations   (Template Method - wrapped)
    Network TX:   net_device_ops    (Template Method - wrapped)
    Block IO:     make_request_fn   (Template Method - wrapped)
    Device:       device_driver     (Template Method - lifecycle)
```

---

## The Essence

```
+=============================================================================+
|                                                                             |
|  STRATEGY IN LINUX KERNEL =                                                 |
|                                                                             |
|      COMPLETE ALGORITHM IS REPLACEABLE                                      |
|      +                                                                      |
|      CORE PROVIDES MECHANISM (WHEN)                                         |
|      +                                                                      |
|      STRATEGY PROVIDES POLICY (HOW)                                         |
|      +                                                                      |
|      NO FRAMEWORK WRAPPING                                                  |
|                                                                             |
|  This is about POLICY vs MECHANISM separation.                              |
|  The core doesn't know HOW to make decisions.                               |
|  The strategy encapsulates the entire decision-making algorithm.            |
|                                                                             |
+=============================================================================+
```

**中文说明：**

Linux内核中策略模式的本质 = 完整算法可替换 + 核心提供机制（何时）+ 策略提供策略（如何）+ 没有框架包装。这是关于策略与机制的分离。核心不知道如何做决策，策略封装整个决策算法。

---

## Final Checklist

Before concluding "this is Strategy", verify:

1. **Multiple ops work together**: Not independent hooks
2. **No framework wrapping**: Core delegates, doesn't wrap
3. **Strategy owns state**: Private data structures
4. **Runtime selectable**: Can switch algorithms
5. **Complete algorithm**: All decision logic in ops table

If all five are true, you have identified Strategy in the Linux kernel.

---

## The Contrast

| Ask | Strategy | Template Method |
|-----|----------|-----------------|
| Who owns control flow? | Core (just calls) | Framework (wraps) |
| How many ops per call? | Multiple | One |
| Pre/post steps? | None | Mandatory |
| State ownership? | Strategy | Framework |
| What varies? | Entire algorithm | One hook |
| Selection? | Runtime | Object creation |

---

## Remember

> **Strategy is about separating policy from mechanism.**
>
> The mechanism (core) provides the WHEN — when to schedule, when to send, when to dispatch.
>
> The policy (strategy) provides the HOW — how to pick next task, how to grow window, how to order requests.
>
> When you see ops tables where multiple functions work together as one algorithm, with no framework wrapping, and the implementation can be swapped at runtime — you have found Strategy in the Linux kernel.
