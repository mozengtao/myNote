# HOW｜架构策略

## 1. Linux 调度的指导原则

```
GUIDING PRINCIPLES OF LINUX SCHEDULING
+=============================================================================+
|                                                                              |
|  PRINCIPLE 1: FAIRNESS OVER STRICT PRIORITY (公平优于严格优先级)              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Strict priority (problematic):                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Priority 100: ████████████████████████████████████████████████  │    │ |
|  │  │  Priority 50:  (waits until 100 sleeps)                          │    │ |
|  │  │  Priority 10:  (starves)                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: Lower priorities may never run                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Fairness (Linux CFS approach):                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Weight 1024 (nice 0):   ████████████████████████████            │    │ |
|  │  │  Weight 512  (nice 5):   ██████████████                          │    │ |
|  │  │  Weight 256  (nice 10):  ████████                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  All tasks run proportionally to their weight                    │    │ |
|  │  │  No starvation - everyone gets CPU time                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  The CFS fairness mechanism:                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  vruntime = actual_runtime * (NICE_0_WEIGHT / task_weight)       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example:                                                        │    │ |
|  │  │  • Task A (nice 0, weight 1024): runs 10ms → vruntime += 10ms   │    │ |
|  │  │  • Task B (nice 5, weight 512):  runs 10ms → vruntime += 20ms   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Lower weight tasks accumulate vruntime faster                   │    │ |
|  │  │  → They get picked less often                                    │    │ |
|  │  │  → But they still run!                                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PRINCIPLE 2: SEPARATION OF POLICY AND MECHANISM (策略与机制分离)            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                        schedule() core                           │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │    MECHANISM (fixed):                                       │ │    │ |
|  │  │  │    • Run queue management                                   │ │    │ |
|  │  │  │    • Context switch                                         │ │    │ |
|  │  │  │    • Preemption infrastructure                              │ │    │ |
|  │  │  │    • Load balancing framework                               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                             │                                    │    │ |
|  │  │                             │ calls                              │    │ |
|  │  │                             ▼                                    │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │    POLICY (pluggable via sched_class):                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │    ┌──────────────┐ ┌──────────────┐ ┌──────────────┐      │ │    │ |
|  │  │  │    │  stop_sched  │ │  dl_sched    │ │  rt_sched    │      │ │    │ |
|  │  │  │    │  (highest)   │ │  (deadline)  │ │  (realtime)  │      │ │    │ |
|  │  │  │    └──────────────┘ └──────────────┘ └──────────────┘      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │    ┌──────────────┐ ┌──────────────┐                       │ │    │ |
|  │  │  │    │  fair_sched  │ │  idle_sched  │                       │ │    │ |
|  │  │  │    │  (CFS)       │ │  (lowest)    │                       │ │    │ |
|  │  │  │    └──────────────┘ └──────────────┘                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Benefits:                                                               │ |
|  │  • New scheduling policies without changing core                        │ |
|  │  • Each policy optimized for specific workload                          │ |
|  │  • Clear interfaces between layers                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**原则 1：公平优于严格优先级**
- 严格优先级问题：低优先级任务可能永远不会运行（饥饿）
- CFS 公平性方法：所有任务按权重比例运行，无饥饿
- CFS 机制：`vruntime = actual_runtime * (NICE_0_WEIGHT / task_weight)`
  - 低权重任务 vruntime 累积更快 → 被选中频率更低 → 但仍会运行

**原则 2：策略与机制分离**
- 机制（固定）：运行队列管理、上下文切换、抢占基础设施、负载均衡框架
- 策略（可插拔）：通过 `sched_class` 实现不同调度策略
- 好处：
  - 新策略无需修改核心
  - 每个策略针对特定工作负载优化
  - 层次间接口清晰

---

## 2. 进程如何被抽象

```
PROCESS ABSTRACTION: PROCESS vs THREAD vs TASK
+=============================================================================+
|                                                                              |
|  THE LINUX APPROACH: EVERYTHING IS A TASK                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Traditional OS view:                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  PROCESS                          THREAD                         │    │ |
|  │  │  ┌───────────────────────┐       ┌──────────────────────────┐   │    │ |
|  │  │  │ • Own address space   │       │ • Shared address space   │   │    │ |
|  │  │  │ • Own file descriptors│       │ • Shared file descriptors│   │    │ |
|  │  │  │ • Own signal handlers │       │ • Shared signal handlers │   │    │ |
|  │  │  │ • Heavy to create     │       │ • Light to create        │   │    │ |
|  │  │  └───────────────────────┘       └──────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Different kernel objects, different APIs                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Linux view:                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │         TASK_STRUCT (the universal execution context)            │    │ |
|  │  │         ┌─────────────────────────────────────────────────────┐  │    │ |
|  │  │         │                                                      │  │    │ |
|  │  │         │   ┌──────────────────────────────────────────────┐   │  │    │ |
|  │  │         │   │  pid, tgid (thread group id)                 │   │  │    │ |
|  │  │         │   │  state (RUNNING, INTERRUPTIBLE, etc.)        │   │  │    │ |
|  │  │         │   │  sched_entity (scheduling info)              │   │  │    │ |
|  │  │         │   │  thread_info (arch-specific, stack)          │   │  │    │ |
|  │  │         │   └──────────────────────────────────────────────┘   │  │    │ |
|  │  │         │                         │                            │  │    │ |
|  │  │         │         ┌───────────────┼───────────────┐            │  │    │ |
|  │  │         │         │               │               │            │  │    │ |
|  │  │         │         ▼               ▼               ▼            │  │    │ |
|  │  │         │  ┌────────────┐ ┌────────────┐ ┌────────────┐        │  │    │ |
|  │  │         │  │ mm_struct  │ │files_struct│ │sighand_struc│       │  │    │ |
|  │  │         │  │ (memory)   │ │  (files)   │ │  (signals)  │       │  │    │ |
|  │  │         │  └────────────┘ └────────────┘ └────────────┘        │  │    │ |
|  │  │         │         ▲               ▲               ▲            │  │    │ |
|  │  │         │         │               │               │            │  │    │ |
|  │  │         │    Can be SHARED or PRIVATE (clone flags)            │  │    │ |
|  │  │         │                                                      │  │    │ |
|  │  │         └─────────────────────────────────────────────────────┘  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  HOW CLONE() CREATES DIFFERENT ENTITIES                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  clone() syscall with different flags:                                   │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  FORK (new process):                                             │    │ |
|  │  │  clone(SIGCHLD)                                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  Parent task_struct          Child task_struct                   │    │ |
|  │  │  ┌──────────────────┐       ┌──────────────────┐                │    │ |
|  │  │  │ mm   ──────────────────► │ mm   (COW copy)  │                │    │ |
|  │  │  │ files ─────────────────► │ files (dup'd)    │                │    │ |
|  │  │  │ signal ────────────────► │ signal (copy)    │                │    │ |
|  │  │  └──────────────────┘       └──────────────────┘                │    │ |
|  │  │                              (separate copies)                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  PTHREAD_CREATE (new thread):                                    │    │ |
|  │  │  clone(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |       │    │ |
|  │  │        CLONE_THREAD | CLONE_PARENT_SETTID | ...)                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  Parent task_struct          Child task_struct                   │    │ |
|  │  │  ┌──────────────────┐       ┌──────────────────┐                │    │ |
|  │  │  │ mm   ────────┬─────────► │ mm  ─────────────┘ (SHARED)       │    │ |
|  │  │  │ files ───────┼─────────► │ files ───────────┘ (SHARED)       │    │ |
|  │  │  │ signal ──────┼─────────► │ signal ──────────┘ (SHARED)       │    │ |
|  │  │  └──────────────┼───┘       └──────────────────┘                │    │ |
|  │  │                 └── same pointers, shared resources             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  KEY INSIGHT: Process vs Thread is just different clone() flags         │ |
|  │               The scheduler sees only task_struct                        │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**Linux 方法：一切都是任务（task）**

传统操作系统将进程和线程视为不同的内核对象，但 Linux 采用统一方法：
- `task_struct` 是通用的执行上下文
- 包含：pid、tgid（线程组 ID）、状态、调度实体、线程信息
- 资源（内存、文件、信号）可以共享或私有，由 `clone()` 标志决定

**clone() 创建不同实体的方式**：
- `fork`（新进程）：`clone(SIGCHLD)` → 所有资源都是独立副本（COW）
- `pthread_create`（新线程）：`clone(CLONE_VM|CLONE_FILES|...)` → 共享资源

**关键洞察**：进程与线程的区别只是不同的 `clone()` 标志，调度器只看到 `task_struct`。

---

## 3. 调度策略如何隔离

```
SCHEDULING POLICY ISOLATION: THE SCHED_CLASS HIERARCHY
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  SCHED_CLASS: OPS-TABLE POLYMORPHISM FOR SCHEDULING                      │ |
|  │                                                                          │ |
|  │  struct sched_class {                                                    │ |
|  │      const struct sched_class *next;   /* priority chain */             │ |
|  │                                                                          │ |
|  │      void (*enqueue_task)(rq, task, flags);                             │ |
|  │      void (*dequeue_task)(rq, task, flags);                             │ |
|  │      void (*check_preempt_curr)(rq, task, flags);                       │ |
|  │      struct task_struct *(*pick_next_task)(rq);                         │ |
|  │      void (*put_prev_task)(rq, task);                                   │ |
|  │      void (*task_tick)(rq, task, queued);                               │ |
|  │      ...                                                                 │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  CLASS PRIORITY CHAIN (highest to lowest):                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────┐                                               │    │ |
|  │  │  │ stop_sched   │ ◄── CPU stopper (migration, etc.)             │    │ |
|  │  │  │ class        │     Absolutely highest priority               │    │ |
|  │  │  └──────┬───────┘                                               │    │ |
|  │  │         │ next                                                  │    │ |
|  │  │         ▼                                                       │    │ |
|  │  │  ┌──────────────┐                                               │    │ |
|  │  │  │ dl_sched     │ ◄── SCHED_DEADLINE (EDF algorithm)            │    │ |
|  │  │  │ class        │     Hard real-time with deadlines             │    │ |
|  │  │  └──────┬───────┘                                               │    │ |
|  │  │         │ next                                                  │    │ |
|  │  │         ▼                                                       │    │ |
|  │  │  ┌──────────────┐                                               │    │ |
|  │  │  │ rt_sched     │ ◄── SCHED_FIFO, SCHED_RR                      │    │ |
|  │  │  │ class        │     Soft real-time, priority-based            │    │ |
|  │  │  └──────┬───────┘                                               │    │ |
|  │  │         │ next                                                  │    │ |
|  │  │         ▼                                                       │    │ |
|  │  │  ┌──────────────┐                                               │    │ |
|  │  │  │ fair_sched   │ ◄── SCHED_NORMAL, SCHED_BATCH                 │    │ |
|  │  │  │ class (CFS)  │     Default, fairness-based                   │    │ |
|  │  │  └──────┬───────┘                                               │    │ |
|  │  │         │ next                                                  │    │ |
|  │  │         ▼                                                       │    │ |
|  │  │  ┌──────────────┐                                               │    │ |
|  │  │  │ idle_sched   │ ◄── SCHED_IDLE                                │    │ |
|  │  │  │ class        │     Runs only when nothing else can           │    │ |
|  │  │  └──────────────┘                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  HOW PICK_NEXT_TASK USES THE CHAIN                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  for_each_class(class) {                                                 │ |
|  │      p = class->pick_next_task(rq);                                     │ |
|  │      if (p)                                                             │ |
|  │          return p;                                                      │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  Flow:                                                                   │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                   │   │ |
|  │  │  1. stop_sched.pick_next_task(rq) → NULL (no stopper tasks)      │   │ |
|  │  │                │                                                  │   │ |
|  │  │                ▼                                                  │   │ |
|  │  │  2. dl_sched.pick_next_task(rq) → NULL (no deadline tasks)       │   │ |
|  │  │                │                                                  │   │ |
|  │  │                ▼                                                  │   │ |
|  │  │  3. rt_sched.pick_next_task(rq) → NULL (no RT tasks runnable)    │   │ |
|  │  │                │                                                  │   │ |
|  │  │                ▼                                                  │   │ |
|  │  │  4. fair_sched.pick_next_task(rq) → task_A ← RETURN THIS         │   │ |
|  │  │                                                                   │   │ |
|  │  │  First class with runnable task wins                              │   │ |
|  │  │                                                                   │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**调度策略隔离：sched_class 层次结构**

`sched_class` 是调度策略的操作表（ops-table）多态接口：
- 包含函数指针：`enqueue_task`、`dequeue_task`、`pick_next_task`、`task_tick` 等
- 通过 `next` 指针形成优先级链

**调度类优先级链（从高到低）**：
1. `stop_sched_class`：CPU 停止器（迁移等），绝对最高优先级
2. `dl_sched_class`：SCHED_DEADLINE（EDF 算法），硬实时
3. `rt_sched_class`：SCHED_FIFO/RR，软实时
4. `fair_sched_class`：SCHED_NORMAL/BATCH（CFS），默认公平调度
5. `idle_sched_class`：SCHED_IDLE，只在无其他任务时运行

**`pick_next_task` 遍历链**：按优先级顺序调用每个类的 `pick_next_task()`，第一个返回非 NULL 的任务获胜。

---

## 4. 生命周期转换如何管理

```
LIFECYCLE TRANSITIONS: FORK / EXEC / EXIT
+=============================================================================+
|                                                                              |
|  TASK LIFECYCLE STATE MACHINE                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │                          ┌────────────────────────────────────┐          │ |
|  │                          │                                    │          │ |
|  │  ┌─────────┐   fork()    │   ┌─────────────────────────────┐  │          │ |
|  │  │ NONEXIST│────────────►│   │      TASK_RUNNING           │  │          │ |
|  │  └─────────┘             │   │   (runnable or running)     │  │          │ |
|  │                          │   └──────────┬──────────────────┘  │          │ |
|  │                          │              │                     │          │ |
|  │                          │              │ schedule()          │          │ |
|  │                          │              ▼                     │          │ |
|  │                          │   ┌─────────────────────────────┐  │          │ |
|  │                          │   │     Running on CPU          │◄─┼──┐       │ |
|  │                          │   └──────────┬──────────────────┘  │  │       │ |
|  │                          │              │                     │  │       │ |
|  │                          │  ┌───────────┼───────────┐         │  │       │ |
|  │                          │  │           │           │         │  │       │ |
|  │                          │  ▼           ▼           ▼         │  │       │ |
|  │                          │ wait     preempt    syscall        │  │       │ |
|  │                          │  │           │      (sleep)        │  │       │ |
|  │                          │  ▼           │           │         │  │       │ |
|  │  ┌─────────────────────┐ │              │           │         │  │       │ |
|  │  │TASK_INTERRUPTIBLE   │◄┼──────────────┘           │         │  │       │ |
|  │  │ (waiting, can wake) │ │                          │         │  │       │ |
|  │  └─────────┬───────────┘ │                          │         │  │       │ |
|  │            │             │                          ▼         │  │       │ |
|  │            │             │  ┌─────────────────────────────┐   │  │       │ |
|  │            │             │  │TASK_UNINTERRUPTIBLE         │   │  │       │ |
|  │  signal or │             │  │ (D state, waiting for I/O)  │   │  │       │ |
|  │  event     │             │  └──────────┬──────────────────┘   │  │       │ |
|  │            │             │             │                      │  │       │ |
|  │            │             │             │ I/O complete         │  │       │ |
|  │            │             │             │                      │  │       │ |
|  │            └─────────────┼─────────────┴──────────────────────┘  │       │ |
|  │                          │                                       │       │ |
|  │                          │                     wake_up()         │       │ |
|  │                          │◄──────────────────────────────────────┘       │ |
|  │                          │                                               │ |
|  │                          └──────────────────────────────────────┐        │ |
|  │                                                                  │        │ |
|  │  ┌─────────────────────┐                                        │        │ |
|  │  │   __TASK_STOPPED    │◄──────── SIGSTOP                       │        │ |
|  │  │   (stopped by sig)  │────────► SIGCONT ──────────────────────┘        │ |
|  │  └─────────────────────┘                                                 │ |
|  │                                                                          │ |
|  │  ┌─────────────────────┐                                                 │ |
|  │  │     EXIT_ZOMBIE     │◄──────── do_exit()                             │ |
|  │  │  (waiting parent)   │                                                 │ |
|  │  └─────────┬───────────┘                                                 │ |
|  │            │ wait() by parent                                            │ |
|  │            ▼                                                             │ |
|  │  ┌─────────────────────┐                                                 │ |
|  │  │     EXIT_DEAD       │                                                 │ |
|  │  │   (finally freed)   │                                                 │ |
|  │  └─────────────────────┘                                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**任务生命周期状态机**：

1. **NONEXIST → TASK_RUNNING**：通过 `fork()` 创建
2. **TASK_RUNNING**：可运行或正在运行
3. **TASK_INTERRUPTIBLE**：等待事件，可被信号唤醒
4. **TASK_UNINTERRUPTIBLE**（D 状态）：等待 I/O，不可被信号中断
5. **__TASK_STOPPED**：被 SIGSTOP 停止
6. **EXIT_ZOMBIE**：已退出，等待父进程 `wait()`
7. **EXIT_DEAD**：最终释放

---

```
FORK() INTERNALS
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  fork() → clone() → do_fork() → copy_process()                           │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  copy_process() (kernel/fork.c):                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. dup_task_struct()          ◄── Allocate new task_struct     │    │ |
|  │  │         │                          Copy parent's task_struct     │    │ |
|  │  │         │                          Allocate kernel stack         │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  2. copy_creds()               ◄── Copy credentials             │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  3. sched_fork()               ◄── Initialize scheduling        │    │ |
|  │  │         │                          Reset vruntime               │    │ |
|  │  │         │                          Set sched_class               │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  4. copy_files()               ◄── Clone or share files_struct  │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  5. copy_mm()                  ◄── Clone or share mm_struct     │    │ |
|  │  │         │                          Set up COW for pages          │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  6. copy_thread()              ◄── Set up kernel stack           │    │ |
|  │  │         │                          Save return address           │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  7. wake_up_new_task()         ◄── Add to run queue              │    │ |
|  │  │                                    (if not CLONE_STOPPED)        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  EXEC() PATH                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  execve() → do_execve() → search_binary_handler()                │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Open executable file                                         │    │ |
|  │  │  2. Read ELF headers                                             │    │ |
|  │  │  3. flush_old_exec()           ◄── Drop old mm, signals          │    │ |
|  │  │  4. setup_new_exec()           ◄── New program name, etc.        │    │ |
|  │  │  5. load_elf_binary()          ◄── Map text, data, bss          │    │ |
|  │  │  6. Set up stack (argv, envp)                                    │    │ |
|  │  │  7. start_thread()             ◄── Set instruction pointer       │    │ |
|  │  │                                    Return to userspace           │    │ |
|  │  │                                                                  │    │ |
|  │  │  Key: Same task_struct, completely new address space            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  EXIT() PATH                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  exit() → do_exit()                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. exit_signals()             ◄── Block all signals             │    │ |
|  │  │  2. exit_mm()                  ◄── Release memory                │    │ |
|  │  │  3. exit_files()               ◄── Close file descriptors        │    │ |
|  │  │  4. exit_fs()                  ◄── Release filesystem info       │    │ |
|  │  │  5. exit_notify()              ◄── Notify parent (SIGCHLD)       │    │ |
|  │  │                                    Reparent children to init     │    │ |
|  │  │  6. Set state = EXIT_ZOMBIE                                      │    │ |
|  │  │  7. schedule()                 ◄── Never returns!                │    │ |
|  │  │                                                                  │    │ |
|  │  │  Parent wait():                                                  │    │ |
|  │  │  • Reads exit code                                               │    │ |
|  │  │  • release_task() → free task_struct                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**fork() 内部流程**：
1. `dup_task_struct()`：分配新的 `task_struct`，复制父进程的，分配内核栈
2. `copy_creds()`：复制凭证
3. `sched_fork()`：初始化调度信息，重置 vruntime
4. `copy_files()`：克隆或共享 `files_struct`
5. `copy_mm()`：克隆或共享 `mm_struct`，设置 COW
6. `copy_thread()`：设置内核栈，保存返回地址
7. `wake_up_new_task()`：加入运行队列

**exec() 路径**：
- 打开可执行文件 → 读取 ELF 头 → 刷新旧执行环境 → 加载新二进制 → 设置栈 → 设置指令指针
- 关键：相同的 `task_struct`，完全新的地址空间

**exit() 路径**：
1. 阻止信号
2. 释放内存、文件、文件系统信息
3. 通知父进程，重新指定子进程的父进程为 init
4. 设置状态为 `EXIT_ZOMBIE`
5. `schedule()` 永不返回
6. 父进程 `wait()` 读取退出码并释放 `task_struct`

---

## 5. 调度器如何控制运行队列、上下文切换和抢占

```
SCHEDULER CONTROL MECHANISMS
+=============================================================================+
|                                                                              |
|  RUN QUEUES (运行队列)                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Each CPU has its own run queue (struct rq):                             │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct rq (per-CPU):                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  raw_spinlock_t lock        ◄── Protects the run queue   │   │    │ |
|  │  │  │  unsigned int nr_running    ◄── Number of runnable tasks │   │    │ |
|  │  │  │  struct task_struct *curr   ◄── Currently running task   │   │    │ |
|  │  │  │  struct task_struct *idle   ◄── Idle task for this CPU   │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  struct cfs_rq cfs          ◄── CFS run queue            │   │    │ |
|  │  │  │  struct rt_rq  rt           ◄── RT run queue             │   │    │ |
|  │  │  │  struct dl_rq  dl           ◄── Deadline run queue       │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  CFS RUN QUEUE (Red-Black Tree):                                 │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  struct cfs_rq:                                           │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │              ┌────┐                                       │   │    │ |
|  │  │  │              │ 50 │ (vruntime)                            │   │    │ |
|  │  │  │            ┌─┴────┴─┐                                     │   │    │ |
|  │  │  │          ┌─┘        └─┐                                   │   │    │ |
|  │  │  │        ┌────┐      ┌────┐                                 │   │    │ |
|  │  │  │        │ 30 │      │ 70 │                                 │   │    │ |
|  │  │  │      ┌─┴────┴─┐   ┌┴────┴┐                                │   │    │ |
|  │  │  │    ┌────┐  ┌────┐     ┌────┐                              │   │    │ |
|  │  │  │    │ 20 │  │ 40 │     │ 90 │                              │   │    │ |
|  │  │  │    └────┘  └────┘     └────┘                              │   │    │ |
|  │  │  │      ▲                                                    │   │    │ |
|  │  │  │      │                                                    │   │    │ |
|  │  │  │    rb_leftmost ◄── Next task to run (O(1) access)        │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  min_vruntime: 20 (tracks fair progress)                  │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CONTEXT SWITCH (上下文切换)                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  context_switch(rq, prev, next):                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌───────────────────┐      ┌───────────────────┐               │    │ |
|  │  │  │   prev task       │      │   next task       │               │    │ |
|  │  │  │                   │      │                   │               │    │ |
|  │  │  │  ┌─────────────┐  │      │  ┌─────────────┐  │               │    │ |
|  │  │  │  │ mm_struct   │  │      │  │ mm_struct   │  │               │    │ |
|  │  │  │  │ (address sp)│──┼──────┼──│ (address sp)│  │               │    │ |
|  │  │  │  └─────────────┘  │      │  └─────────────┘  │               │    │ |
|  │  │  │         │         │      │         ▲         │               │    │ |
|  │  │  │         │         │      │         │         │               │    │ |
|  │  │  │  ┌──────▼──────┐  │  1.  │  ┌──────┴──────┐  │               │    │ |
|  │  │  │  │ CPU page    │  │switch│  │ CPU page    │  │               │    │ |
|  │  │  │  │ table ptr   │──┼─mm───┼──│ table ptr   │  │               │    │ |
|  │  │  │  │ (CR3/TTBR)  │  │      │  │ (CR3/TTBR)  │  │               │    │ |
|  │  │  │  └─────────────┘  │      │  └─────────────┘  │               │    │ |
|  │  │  │                   │      │                   │               │    │ |
|  │  │  │  ┌─────────────┐  │      │  ┌─────────────┐  │               │    │ |
|  │  │  │  │ CPU regs    │  │  2.  │  │ CPU regs    │  │               │    │ |
|  │  │  │  │ (saved)     │──┼switch┼──│ (restored)  │  │               │    │ |
|  │  │  │  │             │  │ regs │  │             │  │               │    │ |
|  │  │  │  └─────────────┘  │      │  └─────────────┘  │               │    │ |
|  │  │  │                   │      │                   │               │    │ |
|  │  │  │  ┌─────────────┐  │      │  ┌─────────────┐  │               │    │ |
|  │  │  │  │ kernel stack│  │  3.  │  │ kernel stack│  │               │    │ |
|  │  │  │  │ (saved)     │──┼switch┼──│ (restored)  │  │               │    │ |
|  │  │  │  │             │  │ stack│  │             │  │               │    │ |
|  │  │  │  └─────────────┘  │      │  └─────────────┘  │               │    │ |
|  │  │  │                   │      │                   │               │    │ |
|  │  │  └───────────────────┘      └───────────────────┘               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Steps:                                                          │    │ |
|  │  │  1. switch_mm() - Switch address space (page tables)             │    │ |
|  │  │  2. switch_to() - Save/restore CPU registers                     │    │ |
|  │  │  3. Kernel stack pointer updated                                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PREEMPTION (抢占)                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  When can preemption happen?                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. VOLUNTARY (cooperative):                                     │    │ |
|  │  │     • Task calls schedule() explicitly                           │    │ |
|  │  │     • Task sleeps (wait_event, mutex_lock, etc.)                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. INVOLUNTARY (preemptive):                                    │    │ |
|  │  │     • Timer interrupt (time slice expired)                       │    │ |
|  │  │     • Higher priority task wakes up                              │    │ |
|  │  │     • Return from interrupt/syscall with TIF_NEED_RESCHED        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  The TIF_NEED_RESCHED flag:                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  set_tsk_need_resched(task)                                      │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  task->thread_info.flags |= TIF_NEED_RESCHED                     │    │ |
|  │  │         │                                                        │    │ |
|  │  │         │  (flag is checked at:)                                 │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├──► Return from syscall                                 │    │ |
|  │  │         ├──► Return from interrupt                               │    │ |
|  │  │         └──► preempt_enable() (if CONFIG_PREEMPT)               │    │ |
|  │  │                      │                                           │    │ |
|  │  │                      ▼                                           │    │ |
|  │  │               schedule()                                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Preemption configurations:                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  CONFIG_PREEMPT_NONE:    Only voluntary preemption               │    │ |
|  │  │                          (server workloads, max throughput)      │    │ |
|  │  │                                                                  │    │ |
|  │  │  CONFIG_PREEMPT_VOLUNTARY: Add explicit preemption points        │    │ |
|  │  │                            (desktop, balanced)                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  CONFIG_PREEMPT:         Preempt anywhere except critical        │    │ |
|  │  │                          sections (low latency)                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  CONFIG_PREEMPT_RT:      Real-time patches                       │    │ |
|  │  │                          (deterministic latency)                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**运行队列（Run Queues）**：
- 每个 CPU 有自己的运行队列（`struct rq`）
- 包含：自旋锁、可运行任务数、当前任务、空闲任务
- 分离的子队列：CFS（红黑树）、RT、Deadline
- CFS 运行队列使用红黑树按 vruntime 排序，最左节点（最小 vruntime）是下一个运行的任务

**上下文切换（Context Switch）**：
1. `switch_mm()`：切换地址空间（页表）
2. `switch_to()`：保存/恢复 CPU 寄存器
3. 更新内核栈指针

**抢占（Preemption）**：
- **自愿抢占**：任务显式调用 `schedule()` 或睡眠
- **非自愿抢占**：定时器中断（时间片过期）、高优先级任务唤醒、从中断/系统调用返回时检查 `TIF_NEED_RESCHED`

**抢占配置**：
- `CONFIG_PREEMPT_NONE`：仅自愿抢占（服务器工作负载，最大吞吐量）
- `CONFIG_PREEMPT_VOLUNTARY`：添加显式抢占点（桌面，平衡）
- `CONFIG_PREEMPT`：除临界区外随处可抢占（低延迟）
- `CONFIG_PREEMPT_RT`：实时补丁（确定性延迟）
