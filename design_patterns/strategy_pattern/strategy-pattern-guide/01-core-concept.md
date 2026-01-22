# Core Concept: What Strategy Means in Linux Kernel

## The Problem Strategy Solves

```
+=============================================================================+
|                    THE FUNDAMENTAL PROBLEM                                   |
+=============================================================================+

    THE KERNEL MUST MAKE DECISIONS, BUT THE BEST DECISION VARIES

    Example: CPU Scheduling
    =======================

    Workload A (Desktop)           Workload B (Real-time)
    +---------------------+        +---------------------+
    | User: browsing      |        | User: audio system  |
    | Wants: responsive   |        | Wants: guarantees   |
    | Best: CFS fairness  |        | Best: RT priority   |
    +---------------------+        +---------------------+

    Workload C (Server)            Workload D (HPC)
    +---------------------+        +---------------------+
    | User: web server    |        | User: computation   |
    | Wants: throughput   |        | Wants: efficiency   |
    | Best: batching      |        | Best: NUMA-aware    |
    +---------------------+        +---------------------+

    PROBLEM: Hardcoding one scheduling algorithm would be wrong
             for 75% of use cases

    SOLUTION: Make the scheduling POLICY pluggable
              Keep the scheduling MECHANISM in the core
```

**中文说明：**

内核必须做决策，但最佳决策因工作负载而异。以CPU调度为例：桌面用户想要响应性，最佳选择是CFS公平调度；实时系统需要保证，最佳选择是RT优先级；服务器需要吞吐量，最佳选择是批处理；HPC需要效率，最佳选择是NUMA感知。问题：硬编码一种调度算法会在75%的场景下是错误的。解决方案：使调度策略可插拔，将调度机制保留在核心中。

---

## What "Algorithm Replacement" Means at Kernel Scale

In the Linux kernel, Strategy means:

1. **Complete algorithm is encapsulated**: All related functions form one unit
2. **Core does not know algorithm internals**: Just knows interface
3. **Strategies are interchangeable**: Same interface, different behavior
4. **Selection happens at well-defined points**: Boot time, runtime, per-object

```
    ALGORITHM ENCAPSULATION:

    +-------------------------------------------------------------------+
    |  struct sched_class (Strategy Interface)                          |
    +-------------------------------------------------------------------+
    |                                                                   |
    |   .enqueue_task       <-- How to add task to ready queue          |
    |   .dequeue_task       <-- How to remove task                      |
    |   .pick_next_task     <-- How to select next task to run          |
    |   .put_prev_task      <-- How to handle preempted task            |
    |   .check_preempt_curr <-- When to preempt current task            |
    |   .task_tick          <-- What to do on timer tick                |
    |                                                                   |
    |   ALL THESE WORK TOGETHER AS ONE COHERENT SCHEDULING ALGORITHM    |
    |                                                                   |
    +-------------------------------------------------------------------+

    CFS Implementation              RT Implementation
    +---------------------+         +---------------------+
    | enqueue: add to RB  |         | enqueue: add to     |
    |          tree       |         |          prio array |
    | pick_next: leftmost |         | pick_next: highest  |
    |            in tree  |         |            priority |
    | task_tick: update   |         | task_tick: check    |
    |            vruntime |         |            time     |
    +---------------------+         +---------------------+

    COMPLETELY DIFFERENT ALGORITHMS, SAME INTERFACE
```

**中文说明：**

在Linux内核中，策略意味着：完整算法被封装——所有相关函数形成一个单元；核心不知道算法内部——只知道接口；策略可互换——相同接口，不同行为；选择发生在明确定义的点。以`sched_class`为例，它包含`enqueue_task`（如何添加任务）、`pick_next_task`（如何选择下一个任务）等多个函数，这些函数协同工作形成一个完整的调度算法。CFS和RT实现完全不同的算法，但使用相同的接口。

---

## Why Strategy is About POLICY, Not LIFECYCLE

```
+=============================================================================+
|                    POLICY vs LIFECYCLE                                       |
+=============================================================================+

    LIFECYCLE (Template Method Domain)     POLICY (Strategy Domain)
    ==================================     =========================

    "When do we do things?"                "How do we make decisions?"

    +---------------------------+          +---------------------------+
    | device_add() sequence:    |          | schedule() decision:      |
    |   1. register kobject     |          |   Which task runs next?   |
    |   2. create sysfs         |          |                           |
    |   3. add to bus           |          | tcp_cong_avoid() decision:|
    |   4. call probe           |          |   How fast to grow window?|
    |   5. send uevent          |          |                           |
    +---------------------------+          | elv_dispatch() decision:  |
    ORDER is FIXED                         |   Which I/O request next? |
    Framework CONTROLS                     +---------------------------+
                                           ALGORITHM varies
                                           Strategy DECIDES


    LIFECYCLE:                             POLICY:
    - Framework owns entry point           - Core owns entry point
    - Framework wraps with pre/post        - Core delegates entire decision
    - Implementation provides one step     - Strategy provides full algorithm
    - Cannot skip framework steps          - Algorithm is self-contained

    Template Method: "You do X, I handle the rest"
    Strategy: "You tell me what X should be"
```

**中文说明：**

策略与生命周期的区别：生命周期（模板方法领域）回答"何时做事？"，如`device_add()`的序列是固定的，框架控制顺序。策略（策略模式领域）回答"如何做决策？"，如`schedule()`决定哪个任务下一个运行，`tcp_cong_avoid()`决定多快增长窗口，`elv_dispatch()`决定哪个I/O请求下一个处理。生命周期中，框架拥有入口点并用前后步骤包装；策略中，核心拥有入口点但将整个决策委托给策略。模板方法说"你做X，我处理其余的"；策略说"你告诉我X应该是什么"。

---

## How Strategy Differs from Template Method

```
    TEMPLATE METHOD                        STRATEGY
    ===============                        ========

    CONTROL FLOW:                          CONTROL FLOW:
    +-------------------+                  +-------------------+
    | vfs_read() {      |                  | schedule() {      |
    |   check_perms();  |  Framework       |   ...             |
    |   f_op->read();   |  wraps hook      |   class->         |
    |   fsnotify();     |                  |    pick_next();   |  Core delegates
    | }                 |                  |   class->         |  entire algo
    +-------------------+                  |    put_prev();    |
                                           | }                 |
                                           +-------------------+

    ONE HOOK CALLED:                       MULTIPLE OPS CALLED:
    - f_op->read is one function           - pick_next, put_prev, enqueue,
    - Framework handles everything else      dequeue all work together as
                                             one coherent algorithm

    FRAMEWORK INVARIANTS:                  NO FRAMEWORK WRAPPING:
    - Lock held during hook                - Strategy manages own state
    - Post-processing guaranteed           - Strategy decides everything
    - Implementation cannot skip           - Core just asks for decision

    IMPLEMENTATION IS PASSIVE:             STRATEGY IS ACTIVE:
    - Just fills in one piece              - Controls entire algorithm
    - Does not manage state machine        - Owns its own state machine
```

**中文说明：**

模板方法与策略的根本区别：

控制流：模板方法中，框架函数（如`vfs_read()`）用前后步骤包装一个钩子调用；策略中，核心函数（如`schedule()`）将整个算法委托给策略。

调用方式：模板方法调用一个钩子函数（`f_op->read`）；策略调用多个协同工作的ops函数（`pick_next`、`put_prev`、`enqueue`、`dequeue`）。

框架控制：模板方法有框架不变量（锁、后处理保证）；策略没有框架包装，策略管理自己的状态。

实现角色：模板方法中实现是被动的，只填充一个部分；策略中策略是主动的，控制整个算法。

---

## Why Strategy is Explicitly Opt-In

```
    STRATEGY IS NOT THE DEFAULT:

    +-------------------------------------------------------------------+
    |  The kernel does NOT use Strategy pattern everywhere              |
    |                                                                   |
    |  Strategy is ONLY used when:                                      |
    |  1. Multiple valid algorithms exist for the same problem          |
    |  2. No single algorithm is universally best                       |
    |  3. The choice is a POLICY decision, not a MECHANISM requirement  |
    |  4. Algorithms are self-contained (no cross-algorithm state)      |
    +-------------------------------------------------------------------+

    WHERE STRATEGY IS USED:               WHERE STRATEGY IS NOT USED:

    +-------------------------+           +---------------------------+
    | Scheduler policies      |           | System call interface     |
    | TCP congestion control  |           | Device lifecycle          |
    | I/O schedulers          |           | VFS read/write path       |
    | Memory policies         |           | Network transmit path     |
    | Security modules        |           | Interrupt handling        |
    +-------------------------+           +---------------------------+

    Policy decisions                      Mechanism / Lifecycle
    (HOW to decide)                       (WHAT must happen)
```

**中文说明：**

策略模式不是默认选择。内核只在以下情况使用策略：(1) 同一问题存在多个有效算法；(2) 没有算法是普遍最优的；(3) 选择是策略决策而非机制要求；(4) 算法是自包含的（无跨算法状态）。策略模式用于：调度策略、TCP拥塞控制、I/O调度器、内存策略、安全模块——这些是策略决策（如何决定）。不使用策略的地方：系统调用接口、设备生命周期、VFS读写路径、网络发送路径——这些是机制/生命周期（必须发生什么）。

---

## Policy vs Mechanism Separation

```
+=============================================================================+
|                    POLICY vs MECHANISM                                       |
+=============================================================================+

    THE FUNDAMENTAL UNIX/LINUX DESIGN PRINCIPLE:
    
    "Separate mechanism from policy"
    
    MECHANISM                              POLICY
    =========                              ======
    
    - The WHAT and WHEN                    - The HOW and WHY
    - Fixed, stable                        - Variable, configurable
    - In kernel core                       - In strategy/module
    - Provides capability                  - Makes decisions
    
    SCHEDULER EXAMPLE:
    
    +---------------------------+          +---------------------------+
    |       MECHANISM           |          |         POLICY            |
    +---------------------------+          +---------------------------+
    | - Timer interrupts        |          | - Which task next? (CFS)  |
    | - Context switch code     |          | - How long to run? (CFS)  |
    | - CPU accounting          |          | - When to preempt? (RT)   |
    | - Run queue structure     |          | - Priority calc (RT)      |
    +---------------------------+          +---------------------------+
    
    MECHANISM is in kernel/sched.c         POLICY is in sched_class
    Fixed for all systems                  Configurable per system/task

    TCP EXAMPLE:
    
    +---------------------------+          +---------------------------+
    |       MECHANISM           |          |         POLICY            |
    +---------------------------+          +---------------------------+
    | - Send/receive packets    |          | - Window size calc        |
    | - ACK processing          |          | - Congestion detection    |
    | - Retransmission logic    |          | - Rate adjustment         |
    | - Connection state        |          | - Loss recovery           |
    +---------------------------+          +---------------------------+
    
    MECHANISM is in tcp_*.c                POLICY is in tcp_congestion_ops
```

**中文说明：**

策略与机制分离是Unix/Linux的基本设计原则。机制提供"什么"和"何时"——固定、稳定、在核心中、提供能力。策略提供"如何"和"为什么"——可变、可配置、在策略/模块中、做决策。

以调度器为例：机制包括定时器中断、上下文切换、CPU统计、运行队列结构，在`kernel/sched.c`中，对所有系统固定。策略包括选择下一个任务、运行多长时间、何时抢占、优先级计算，在`sched_class`中，可按系统/任务配置。

以TCP为例：机制包括发送/接收数据包、ACK处理、重传逻辑、连接状态。策略包括窗口大小计算、拥塞检测、速率调整、丢包恢复，在`tcp_congestion_ops`中。

---

## Pluggable Algorithm Characteristics

```
    WHAT MAKES AN ALGORITHM "PLUGGABLE"?

    1. SELF-CONTAINED STATE
       +-------------------------------------------------------+
       |  Each strategy maintains its own state                |
       |  No shared mutable state between strategies           |
       |                                                       |
       |  CFS: vruntime, rb-tree                               |
       |  RT:  priority array, bitmaps                         |
       |  CUBIC: cwnd, ssthresh, cubic state                   |
       +-------------------------------------------------------+

    2. COHERENT INTERFACE
       +-------------------------------------------------------+
       |  All operations needed for the algorithm              |
       |  Interface is complete (no missing pieces)            |
       |                                                       |
       |  sched_class: enqueue, dequeue, pick_next, put_prev...|
       |  tcp_cong_ops: ssthresh, cong_avoid, undo_cwnd...     |
       +-------------------------------------------------------+

    3. SELECTION POINT
       +-------------------------------------------------------+
       |  Well-defined moment when strategy is chosen          |
       |                                                       |
       |  Scheduler: per-task (task->sched_class)              |
       |  TCP CC: per-socket (icsk->icsk_ca_ops)               |
       |  I/O Sched: per-queue (q->elevator)                   |
       +-------------------------------------------------------+

    4. NO CROSS-ALGORITHM CALLS
       +-------------------------------------------------------+
       |  Strategy A never calls Strategy B                    |
       |  Core mediates if coordination needed                 |
       |                                                       |
       |  CFS doesn't call RT functions                        |
       |  CUBIC doesn't call Reno functions                    |
       +-------------------------------------------------------+
```

**中文说明：**

可插拔算法的特征：

(1) 自包含状态——每个策略维护自己的状态，策略之间没有共享可变状态。CFS有vruntime和红黑树，RT有优先级数组和位图，CUBIC有cwnd、ssthresh和cubic状态。

(2) 完整接口——算法所需的所有操作，接口是完整的。`sched_class`有`enqueue`、`dequeue`、`pick_next`等，`tcp_cong_ops`有`ssthresh`、`cong_avoid`等。

(3) 选择点——明确定义的策略选择时刻。调度器是每任务（`task->sched_class`），TCP CC是每socket（`icsk->icsk_ca_ops`），I/O调度是每队列（`q->elevator`）。

(4) 无跨算法调用——策略A从不调用策略B，如果需要协调由核心中介。CFS不调用RT函数，CUBIC不调用Reno函数。

---

## Runtime Selection

```
    STRATEGY SELECTION MECHANISMS:

    BOOT TIME:
    +-------------------------------------------------------+
    |  Kernel command line: elevator=deadline               |
    |  Compiled-in defaults: CONFIG_DEFAULT_IOSCHED         |
    +-------------------------------------------------------+

    SYSCTL / PROCFS:
    +-------------------------------------------------------+
    |  /proc/sys/net/ipv4/tcp_congestion_control = cubic    |
    |  echo "cubic" > /proc/sys/net/...                     |
    +-------------------------------------------------------+

    PER-OBJECT ASSIGNMENT:
    +-------------------------------------------------------+
    |  Per-task: sched_setscheduler(pid, SCHED_FIFO, ...)   |
    |  Per-socket: setsockopt(fd, TCP_CONGESTION, "reno")   |
    |  Per-queue: echo deadline > /sys/block/sda/queue/scheduler |
    +-------------------------------------------------------+

    AUTOMATIC SELECTION:
    +-------------------------------------------------------+
    |  Scheduler: RT tasks get rt_sched_class               |
    |             Normal tasks get fair_sched_class         |
    +-------------------------------------------------------+
```

---

## Summary

Strategy in Linux kernel means:

1. **Entire algorithm is replaceable**: Not just one function, but complete policy
2. **Policy is separated from mechanism**: Core provides when, strategy provides how
3. **Multiple ops work together**: All functions in ops table form one coherent unit
4. **Self-contained state**: Each strategy manages its own data
5. **Explicit selection**: Strategy is chosen at well-defined points
6. **No framework wrapping**: Strategy owns its algorithm completely

The key insight: **Strategy is about giving the implementation ownership of an entire algorithm, not about filling in one piece of a framework-controlled flow.**
