# WHY｜为什么需要进程管理

## 1. 进程管理解决的核心系统问题

```
CORE PROBLEMS SOLVED BY PROCESS MANAGEMENT
+=============================================================================+
|                                                                              |
|  THE FUNDAMENTAL ILLUSION: INFINITE CPUs FOR FINITE HARDWARE                 |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Physical Reality:                                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │    CPU 0        CPU 1        CPU 2        CPU 3                   │   │ |
|  │  │   ┌─────┐      ┌─────┐      ┌─────┐      ┌─────┐                  │   │ |
|  │  │   │     │      │     │      │     │      │     │   4 cores only   │   │ |
|  │  │   └─────┘      └─────┘      └─────┘      └─────┘                  │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │                          SCHEDULER                                       │ |
|  │                             │                                            │ |
|  │                    ┌────────┴────────┐                                   │ |
|  │                    ▼                 ▼                                   │ |
|  │                 MULTIPLEXES      TIME-SLICES                             │ |
|  │                    │                 │                                   │ |
|  │                    └────────┬────────┘                                   │ |
|  │                             ▼                                            │ |
|  │  Process Illusion:                                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  Process 1   Process 2   Process 3   Process 4   Process 5   ...  │   │ |
|  │  │  ┌─────┐     ┌─────┐     ┌─────┐     ┌─────┐     ┌─────┐          │   │ |
|  │  │  │"Own │     │"Own │     │"Own │     │"Own │     │"Own │          │   │ |
|  │  │  │ CPU"│     │ CPU"│     │ CPU"│     │ CPU"│     │ CPU"│          │   │ |
|  │  │  └─────┘     └─────┘     └─────┘     └─────┘     └─────┘          │   │ |
|  │  │                                                                    │   │ |
|  │  │  Thousands of processes, each believing it has dedicated CPU      │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

进程管理解决的根本问题是**用有限的硬件创造无限 CPU 的假象**：
- 物理现实：只有 4-64 个 CPU 核心
- 进程假象：数千个进程，每个都认为自己拥有专用 CPU
- 调度器通过时间分片和多路复用实现这一假象

---

```
PROBLEM 1: CPU TIME SHARING (CPU 时间共享)
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Without time sharing:                                                   │ |
|  │                                                                          │ |
|  │  Time ──────────────────────────────────────────────────────────►       │ |
|  │                                                                          │ |
|  │  CPU 0 │████████████████████████████████████████████████████████│       │ |
|  │        │           Process A runs forever                       │       │ |
|  │        │                                                        │       │ |
|  │                                                                          │ |
|  │  Process B: Waiting forever... 😞                                       │ |
|  │  Process C: Waiting forever... 😞                                       │ |
|  │                                                                          │ |
|  │  ─────────────────────────────────────────────────────────────────────  │ |
|  │                                                                          │ |
|  │  With time sharing (preemptive multitasking):                            │ |
|  │                                                                          │ |
|  │  Time ──────────────────────────────────────────────────────────►       │ |
|  │                                                                          │ |
|  │  CPU 0 │██A██│██B██│██C██│██A██│██B██│██C██│██A██│██B██│██C██│ ...      │ |
|  │        │ 10ms│ 10ms│ 10ms│ 10ms│ 10ms│ 10ms│ 10ms│ 10ms│ 10ms│          │ |
|  │                                                                          │ |
|  │  All processes make progress!                                            │ |
|  │                                                                          │ |
|  │  Key mechanisms:                                                         │ |
|  │  • Timer interrupt (HZ = 100-1000)                                      │ |
|  │  • Context switch                                                        │ |
|  │  • Run queue management                                                  │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**CPU 时间共享**是进程管理的第一个核心问题：
- 没有时间共享：一个进程会永远运行，其他进程永远等待
- 有时间共享：通过抢占式多任务，每个进程轮流获得 CPU 时间片（如 10ms）
- 关键机制：定时器中断（HZ=100-1000）、上下文切换、运行队列管理

---

```
PROBLEM 2: FAIRNESS VS LATENCY (公平性 vs 延迟)
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  THE FUNDAMENTAL TENSION                                                 │ |
|  │                                                                          │ |
|  │  ┌───────────────────────────────────────────────────────────────────┐  │ |
|  │  │                                                                     │  │ |
|  │  │        FAIRNESS ◄───────── tension ─────────► LATENCY              │  │ |
|  │  │                                                                     │  │ |
|  │  │  "Every task gets         vs        "Interactive tasks             │  │ |
|  │  │   equal CPU time"                    respond immediately"           │  │ |
|  │  │                                                                     │  │ |
|  │  └───────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  Example scenario:                                                       │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Running: video encoder (CPU-bound, needs 100% CPU for hours)    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Waiting: terminal (interactive, needs response in < 10ms)       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Problem: If perfectly fair, terminal waits 50% of time          │    │ |
|  │  │           → UI feels sluggish                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Sleepers get bonus                                    │    │ |
|  │  │            Interactive tasks get priority boost                  │    │ |
|  │  │            But still bounded by fairness                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Linux CFS approach:                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Track "virtual runtime" (vruntime) for each task              │    │ |
|  │  │  • Task with smallest vruntime runs next                         │    │ |
|  │  │  • Sleepers don't accumulate vruntime                            │    │ |
|  │  │    → They wake up with "credit" (lower vruntime)                 │    │ |
|  │  │    → They run immediately                                        │    │ |
|  │  │  • But their vruntime catches up quickly                         │    │ |
|  │  │    → Can't starve CPU-bound tasks forever                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**公平性与延迟的矛盾**是调度器设计的核心张力：
- 公平性要求：每个任务获得相等的 CPU 时间
- 延迟要求：交互任务需要立即响应
- 例子：视频编码器（CPU 密集型）vs 终端（交互式）
- Linux CFS 解决方案：
  - 跟踪每个任务的"虚拟运行时间"（vruntime）
  - vruntime 最小的任务下一个运行
  - 睡眠者不累积 vruntime → 醒来时有"信用" → 立即运行
  - 但 vruntime 会很快追上 → 不会永远饿死 CPU 密集型任务

---

```
PROBLEM 3: ISOLATION BETWEEN TASKS (任务间隔离)
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Without isolation:                                                      │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Process A        Process B        Process C                     │    │ |
|  │  │  ┌─────────┐     ┌─────────┐      ┌─────────┐                   │    │ |
|  │  │  │ while(1)│     │         │      │         │                   │    │ |
|  │  │  │   ;     │ ──► │ STARVED │      │ STARVED │                   │    │ |
|  │  │  └─────────┘     └─────────┘      └─────────┘                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  A buggy process starves everyone else                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  With isolation (scheduler protection):                                  │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                    SCHEDULER                                 ││    │ |
|  │  │  │                                                              ││    │ |
|  │  │  │  "Each task gets fair share, regardless of behavior"         ││    │ |
|  │  │  │                                                              ││    │ |
|  │  │  │  • Timer interrupt forces preemption                         ││    │ |
|  │  │  │  • No task can monopolize CPU                                ││    │ |
|  │  │  │  • Priority can't be infinite                                ││    │ |
|  │  │  │                                                              ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  │  Process A        Process B        Process C                     │    │ |
|  │  │  ┌─────────┐     ┌─────────┐      ┌─────────┐                   │    │ |
|  │  │  │ while(1)│     │ normal  │      │ normal  │                   │    │ |
|  │  │  │   ;     │     │ work    │      │ work    │                   │    │ |
|  │  │  └─────────┘     └─────────┘      └─────────┘                   │    │ |
|  │  │  gets ~33%       gets ~33%        gets ~33%                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Additional isolation mechanisms:                                        │ |
|  │  • Memory: each process has own address space (mm_struct)               │ |
|  │  • Files: each process has own file descriptors (files_struct)          │ |
|  │  • Signals: each process has own signal handlers (sighand_struct)       │ |
|  │  • Credentials: each process has own uid/gid (cred)                     │ |
|  │  • Namespaces: containerization (nsproxy)                               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**任务间隔离**是进程管理的第三个核心问题：
- 没有隔离：一个死循环进程会饿死其他所有进程
- 有隔离：调度器强制公平分配 CPU 时间
  - 定时器中断强制抢占
  - 没有任务能独占 CPU
  - 优先级不能无限高
- 额外隔离机制：
  - 内存隔离（`mm_struct`）
  - 文件描述符隔离（`files_struct`）
  - 信号隔离（`sighand_struct`）
  - 凭证隔离（`cred`）
  - 命名空间隔离（`nsproxy`）

---

## 2. 没有结构化调度器会发生什么

```
WHAT BREAKS WITHOUT A STRUCTURED SCHEDULER
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  FAILURE 1: STARVATION (饥饿)                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Without fairness:                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  High priority task ███████████████████████████████████████     │    │ |
|  │  │                                                                  │    │ |
|  │  │  Medium priority    ▓ (rarely runs)                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Low priority       (never runs - STARVED)                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Result: Low priority tasks never complete                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  FAILURE 2: PRIORITY INVERSION (优先级反转)                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Classic case: Mars Pathfinder bug (1997)                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  Low priority task holds lock ───────────┐                       │    │ |
|  │  │                                           │                      │    │ |
|  │  │  High priority task waits for lock ◄──────┘                      │    │ |
|  │  │                                           │                      │    │ |
|  │  │  Medium priority task runs ◄──────────────┤ preempts low         │    │ |
|  │  │                                           │                      │    │ |
|  │  │  Result: High priority blocked by medium! │                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Priority inheritance (rt_mutex in Linux)              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  FAILURE 3: UNBOUNDED LATENCY (无界延迟)                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Without preemption:                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Task A runs kernel code ────────────────────────────────────    │    │ |
|  │  │                          │ 50ms │ 100ms │ 200ms │ ???ms │         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Interrupt arrives ───► │                                        │    │ |
|  │  │                         │ waiting... waiting... waiting...       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Result: Real-time deadline missed                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solutions in Linux:                                             │    │ |
|  │  │  • CONFIG_PREEMPT (preemptible kernel)                          │    │ |
|  │  │  • CONFIG_PREEMPT_RT (real-time patches)                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  FAILURE 4: CACHE THRASHING (缓存抖动)                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Without affinity awareness:                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  Task A on CPU 0 ─────► Task A on CPU 1 ─────► Task A on CPU 2  │    │ |
|  │  │       │                      │                      │            │    │ |
|  │  │       └── cache warm         └── cache cold         └── cold    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Result: Performance degrades due to constant cache misses       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Linux solution: CPU affinity, per-CPU run queues                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

没有结构化调度器会导致四种失败：

1. **饥饿**：低优先级任务永远无法运行
2. **优先级反转**：高优先级任务被中等优先级阻塞（火星探路者 bug）
3. **无界延迟**：没有抢占时，实时截止时间无法保证
4. **缓存抖动**：任务在 CPU 之间迁移导致缓存失效

---

## 3. 这个子系统中占主导的复杂度

```
DOMINANT COMPLEXITIES IN SCHEDULER SUBSYSTEM
+=============================================================================+
|                                                                              |
|  COMPLEXITY 1: CONCURRENCY (并发)                                Priority: ★★★★★|
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  The scheduler itself runs concurrently on multiple CPUs:                │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  CPU 0                    CPU 1                    CPU 2         │    │ |
|  │  │  ─────                    ─────                    ─────         │    │ |
|  │  │  schedule()               schedule()               schedule()    │    │ |
|  │  │     │                        │                        │          │    │ |
|  │  │     ▼                        ▼                        ▼          │    │ |
|  │  │  pick_next_task()         pick_next_task()         pick_next_task()│  │ |
|  │  │     │                        │                        │          │    │ |
|  │  │     └────────────────────────┼────────────────────────┘          │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │              SHARED DATA: task list, run queues                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Locking strategy:                                                       │ |
|  │  • Per-CPU run queue locks (rq->lock)                                   │ |
|  │  • RCU for task list traversal                                          │ |
|  │  • Careful lock ordering to avoid deadlock                              │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  COMPLEXITY 2: SCALABILITY (SMP 可扩展性)                       Priority: ★★★★★|
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Challenge: O(1) scheduling decision with 1000s of tasks                 │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Global run queue (bad for scalability):                         │    │ |
|  │  │                                                                  │    │ |
|  │  │      ┌───────────────────────────────────────────────┐           │    │ |
|  │  │      │            Global Run Queue                    │           │    │ |
|  │  │      │    [T1] [T2] [T3] [T4] [T5] ... [T1000]       │           │    │ |
|  │  │      └───────────────────────────────────────────────┘           │    │ |
|  │  │                         │                                        │    │ |
|  │  │        ┌────────────────┼────────────────┐                       │    │ |
|  │  │        ▼                ▼                ▼                       │    │ |
|  │  │     CPU 0            CPU 1            CPU 2                      │    │ |
|  │  │        │                │                │                       │    │ |
|  │  │        └── all contend for single lock ──┘                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Per-CPU run queues (good for scalability):                      │    │ |
|  │  │                                                                  │    │ |
|  │  │     CPU 0              CPU 1              CPU 2                  │    │ |
|  │  │   ┌─────────┐       ┌─────────┐       ┌─────────┐                │    │ |
|  │  │   │  rq[0]  │       │  rq[1]  │       │  rq[2]  │                │    │ |
|  │  │   │ [T1][T4]│       │ [T2][T5]│       │ [T3][T6]│                │    │ |
|  │  │   └─────────┘       └─────────┘       └─────────┘                │    │ |
|  │  │        │                │                │                       │    │ |
|  │  │        └── each has own lock, no contention ─┘                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  + Load balancing between CPUs (periodic + idle)                 │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  COMPLEXITY 3: PREDICTABILITY (可预测性)                        Priority: ★★★★☆|
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Different workloads need different guarantees:                          │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Workload Type          Requirement          Scheduler Class     │    │ |
|  │  │  ─────────────          ───────────          ───────────────     │    │ |
|  │  │  Desktop/Server         Fairness             SCHED_NORMAL (CFS)  │    │ |
|  │  │  Audio/Video            Bounded latency      SCHED_FIFO/RR       │    │ |
|  │  │  Industrial control     Hard deadline        SCHED_DEADLINE      │    │ |
|  │  │  Batch processing       Low priority         SCHED_BATCH         │    │ |
|  │  │  Background tasks       Minimal CPU          SCHED_IDLE          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Linux provides multiple scheduler classes to meet different needs      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

调度器子系统的主导复杂度：

1. **并发**（★★★★★）：
   - 调度器本身在多个 CPU 上并发运行
   - 共享数据：任务列表、运行队列
   - 锁策略：per-CPU 运行队列锁、RCU 遍历

2. **可扩展性**（★★★★★）：
   - 挑战：数千任务时 O(1) 调度决策
   - 解决方案：per-CPU 运行队列（无争用） + 负载均衡

3. **可预测性**（★★★★☆）：
   - 不同工作负载需要不同保证
   - Linux 提供多种调度类：CFS（公平）、FIFO/RR（实时）、DEADLINE（硬截止期限）

---

## 4. UNIX 传承和 Linux 工作负载如何塑造调度器设计

```
UNIX HERITAGE AND LINUX WORKLOAD EVOLUTION
+=============================================================================+
|                                                                              |
|  UNIX SCHEDULER EVOLUTION                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1970s: Original UNIX (AT&T)                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • Simple priority-based scheduling                              │    │ |
|  │  │  • nice value (-20 to +19)                                       │    │ |
|  │  │  • Priority decay over time                                      │    │ |
|  │  │  • Designed for time-sharing mainframes                          │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  1990s: Linux 1.x/2.x                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • O(n) scheduler - scan all tasks                               │    │ |
|  │  │  • Global run queue                                              │    │ |
|  │  │  • Simple but doesn't scale                                      │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2002: O(1) Scheduler (Linux 2.6)                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • Constant-time scheduling                                      │    │ |
|  │  │  • Per-CPU run queues                                            │    │ |
|  │  │  • Active/expired arrays                                         │    │ |
|  │  │  • Heuristics for interactivity (fragile)                        │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2007: CFS - Completely Fair Scheduler (Linux 2.6.23)                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • Red-black tree ordered by vruntime                            │    │ |
|  │  │  • O(log n) pick_next but typically O(1) for leftmost           │    │ |
|  │  │  • Proportional fairness (weighted fair queuing)                 │    │ |
|  │  │  • No heuristics - pure fairness with sleeper bonus              │    │ |
|  │  │  • Modular sched_class design                                    │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LINUX WORKLOAD EVOLUTION                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1990s: Desktop/Server                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • Interactive: X11, terminals, editors                          │    │ |
|  │  │  • Server: Apache, Sendmail                                      │    │ |
|  │  │  • Batch: make, gcc                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Scheduler focus: Interactivity on desktop                       │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2000s: SMP Era                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • Multi-core CPUs become common                                 │    │ |
|  │  │  • NUMA architectures                                            │    │ |
|  │  │  • Web servers handling thousands of connections                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  Scheduler focus: Scalability, NUMA awareness                    │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2010s: Cloud and Containers                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • Virtual machines sharing CPUs                                 │    │ |
|  │  │  • Container orchestration (Docker, K8s)                         │    │ |
|  │  │  • Microservices with varying workloads                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  Scheduler focus: cgroups, CPU bandwidth control                 │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2020s: Heterogeneous Computing                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  • big.LITTLE (ARM), Hybrid (Intel)                              │    │ |
|  │  │  • Power efficiency critical                                     │    │ |
|  │  │  • Real-time in embedded/automotive                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Scheduler focus: Energy-aware, SCHED_DEADLINE                   │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**UNIX 调度器演进**：
- 1970s：简单优先级调度，nice 值 (-20 到 +19)
- 1990s Linux：O(n) 调度器，全局运行队列，不可扩展
- 2002 Linux 2.6：O(1) 调度器，per-CPU 队列，活跃/过期数组，但交互性启发式脆弱
- 2007 Linux 2.6.23：CFS（完全公平调度器），红黑树按 vruntime 排序，无启发式，纯公平性

**Linux 工作负载演进**：
- 1990s：桌面/服务器，关注交互性
- 2000s：SMP 时代，关注可扩展性和 NUMA
- 2010s：云和容器，关注 cgroups 和 CPU 带宽控制
- 2020s：异构计算，关注能效和实时性
