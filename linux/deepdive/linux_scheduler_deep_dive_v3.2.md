# Linux Kernel Scheduler Deep Dive (v3.2)
## A Code-Level Walkthrough of the CFS Scheduler

---

## Table of Contents
1. [Subsystem Context (Big Picture)](#1-subsystem-context-big-picture)
2. [Directory & File Map](#2-directory--file-map)
3. [Core Data Structures](#3-core-data-structures)
4. [Entry Points & Call Paths](#4-entry-points--call-paths)
5. [Core Workflows](#5-core-workflows)
6. [Important Algorithms & Mechanisms](#6-important-algorithms--mechanisms)
7. [Concurrency & Synchronization](#7-concurrency--synchronization)
8. [Performance Considerations](#8-performance-considerations)
9. [Common Pitfalls & Bugs](#9-common-pitfalls--bugs)
10. [How to Read This Code Yourself](#10-how-to-read-this-code-yourself)
11. [Summary & Mental Model](#11-summary--mental-model)
12. [What to Study Next](#12-what-to-study-next)

---

## 1. Subsystem Context (Big Picture)

### What Kernel Subsystem Are We Studying?

The **Process Scheduler** is the core component responsible for deciding which task runs on which CPU and for how long. In Linux 3.2, the default scheduler for normal (SCHED_NORMAL) tasks is the **Completely Fair Scheduler (CFS)**, introduced in kernel 2.6.23 by Ingo Molnar.

### What Problem Does It Solve?

1. **CPU Time Allocation**: Fairly distributes CPU time among competing tasks
2. **Responsiveness**: Ensures interactive tasks get timely CPU access
3. **Throughput**: Maximizes overall system work done
4. **SMP Load Balancing**: Distributes work across multiple CPUs
5. **Real-time Guarantees**: Provides deterministic scheduling for RT tasks

### Where It Sits in the Overall Kernel Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              USER SPACE                                      │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐            │
│  │   Process   │ │   Process   │ │   Thread    │ │   Thread    │            │
│  │     A       │ │     B       │ │     C       │ │     D       │            │
│  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘ └──────┬──────┘            │
└─────────┼───────────────┼───────────────┼───────────────┼───────────────────┘
          │               │               │               │
          │ System Calls: fork(), sched_setscheduler(), yield(), nanosleep()
          ▼               ▼               ▼               ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              KERNEL SPACE                                    │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                         SCHEDULER SUBSYSTEM                            │ │
│  │  ┌──────────────┐                                                      │ │
│  │  │ Scheduling   │  stop_sched_class → rt_sched_class → fair_sched_class│ │
│  │  │   Classes    │                                      → idle_sched_class│
│  │  └──────────────┘                                                      │ │
│  │                                                                        │ │
│  │  ┌──────────────────────────────────────────────────────────────────┐  │ │
│  │  │                    PER-CPU RUNQUEUES                             │  │ │
│  │  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐              │  │ │
│  │  │  │  CPU 0  │  │  CPU 1  │  │  CPU 2  │  │  CPU 3  │   ...        │  │ │
│  │  │  │   rq    │  │   rq    │  │   rq    │  │   rq    │              │  │ │
│  │  │  │┌──────┐ │  │┌──────┐ │  │┌──────┐ │  │┌──────┐ │              │  │ │
│  │  │  ││cfs_rq│ │  ││cfs_rq│ │  ││cfs_rq│ │  ││cfs_rq│ │              │  │ │
│  │  │  ││rt_rq │ │  ││rt_rq │ │  ││rt_rq │ │  ││rt_rq │ │              │  │ │
│  │  │  │└──────┘ │  │└──────┘ │  │└──────┘ │  │└──────┘ │              │  │ │
│  │  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘              │  │ │
│  │  └──────────────────────────────────────────────────────────────────┘  │ │
│  │                               ▲                                        │ │
│  │                               │ Load Balancing                         │ │
│  │                     ┌─────────┴─────────┐                              │ │
│  │                     │  Scheduler Domains │                              │ │
│  │                     │   (sched_domain)   │                              │ │
│  │                     └───────────────────┘                              │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                    │                                         │
│         ┌──────────────────────────┼──────────────────────────┐             │
│         │                          │                          │             │
│         ▼                          ▼                          ▼             │
│  ┌─────────────┐           ┌─────────────┐           ┌─────────────┐        │
│  │   Timer     │           │  Interrupt  │           │   Memory    │        │
│  │ Subsystem   │           │  Subsystem  │           │ Management  │        │
│  │ (hrtimer)   │           │ (softirq)   │           │    (mm)     │        │
│  └─────────────┘           └─────────────┘           └─────────────┘        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

**图解说明：**
- 调度器是内核的核心子系统，位于用户空间进程和硬件之间
- 每个CPU都有独立的运行队列(runqueue)，包含CFS运行队列和RT运行队列
- 调度类按优先级链式组织：stop > rt > fair > idle
- 调度器与定时器子系统（负责时间片）、中断子系统（触发重调度）、内存管理（进程上下文切换）紧密交互
- SMP系统中，调度域(sched_domain)用于跨CPU负载均衡

### How This Subsystem Interacts with Others

| Subsystem | Interaction |
|-----------|-------------|
| **Timer (hrtimer)** | Provides tick-based accounting, preemption timer |
| **Interrupts** | Timer interrupts trigger scheduler_tick(), wakeups from IRQ handlers |
| **Memory (mm)** | Context switch requires switching mm_struct (page tables) |
| **Process Management** | fork() creates new tasks, exit() removes them |
| **SMP/CPU Hotplug** | Load balancing, CPU online/offline events |
| **cgroups** | Resource limits, group scheduling |

---

## 2. Directory & File Map

In Linux 3.2, the scheduler code lives primarily in `kernel/`. Note that in modern kernels (4.x+), these files have been reorganized into `kernel/sched/`.

```
kernel/
│
├── sched.c              → Main scheduler: runqueue, schedule(), context_switch()
│                          Core data structures (struct rq, sched_class interface)
│                          ~9800 lines - the "heart" of the scheduler
│
├── sched_fair.c         → CFS (Completely Fair Scheduler) implementation
│                          Red-black tree operations, vruntime calculations
│                          Load balancing for fair class (~5100 lines)
│
├── sched_rt.c           → Real-time scheduler (SCHED_FIFO, SCHED_RR)
│                          Priority arrays, RT bandwidth control (~1850 lines)
│
├── sched_idletask.c     → Idle task scheduler class (~100 lines)
│                          Always returns idle task when nothing else to run
│
├── sched_stoptask.c     → Stop-task scheduler class (~120 lines)
│                          Highest priority, for CPU hotplug, stop_machine
│
├── sched_cpupri.c/h     → CPU priority tracking for RT load balancing
│
├── sched_features.h     → Runtime-tunable scheduler features (debug flags)
│
├── sched_stats.h        → Statistics collection infrastructure
│
├── sched_debug.c        → /proc/sched_debug interface
│
├── sched_clock.c        → High-resolution scheduler clock
│
├── sched_autogroup.c/h  → Automatic task grouping for desktop responsiveness
│
└── fork.c               → Process creation, copy_process() initializes sched data

include/linux/
│
├── sched.h              → Core scheduling structures:
│                          - struct task_struct (the process descriptor)
│                          - struct sched_class (scheduler class interface)
│                          - struct sched_entity, sched_rt_entity
│                          - struct sched_domain (SMP load balancing)
│                          - Task states (TASK_RUNNING, etc.)
│
└── rbtree.h             → Red-black tree used by CFS
```

### Why Is the Code Split This Way?

1. **sched.c** - The monolithic core handles the scheduler class abstraction, runqueue management, and the critical `schedule()` path. It's large because it must be fast (cache-locality matters).

2. **sched_fair.c / sched_rt.c / sched_idle.c** - Each scheduling policy is a separate "class" implementing the `sched_class` interface. This allows:
   - Different algorithms for different workloads
   - Clean separation of concerns
   - Easy addition of new scheduling classes

3. **Header vs Source separation** - Structures in `sched.h` are needed by many kernel subsystems (fork, signals, etc.), but implementation stays in `kernel/`.

---

## 3. Core Data Structures

### 3.1 The Scheduling Class Interface

```c
// include/linux/sched.h, lines 1076-1127

struct sched_class {
    const struct sched_class *next;  // 链表指向下一个低优先级调度类

    void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task) (struct rq *rq);

    void (*check_preempt_curr) (struct rq *rq, struct task_struct *p, int flags);

    struct task_struct * (*pick_next_task) (struct rq *rq);
    void (*put_prev_task) (struct rq *rq, struct task_struct *p);

#ifdef CONFIG_SMP
    int (*select_task_rq)(struct task_struct *p, int sd_flag, int flags);
    void (*rq_online)(struct rq *rq);
    void (*rq_offline)(struct rq *rq);
#endif

    void (*set_curr_task) (struct rq *rq);
    void (*task_tick) (struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork) (struct task_struct *p);
    // ... more operations
};
```

**字段解释：**

| 字段 | 用途 |
|------|------|
| `next` | 指向下一个调度类，形成优先级链：stop→rt→fair→idle |
| `enqueue_task` | 将任务加入运行队列 |
| `dequeue_task` | 将任务从运行队列移除 |
| `pick_next_task` | 选择下一个要运行的任务（核心！） |
| `put_prev_task` | 当前任务被切换出去前的清理 |
| `check_preempt_curr` | 检查新唤醒的任务是否应该抢占当前任务 |
| `task_tick` | 每个时钟滴答调用，更新时间统计 |
| `select_task_rq` | SMP：为任务选择目标CPU |

### 3.2 The Per-CPU Runqueue

```c
// kernel/sched.c, lines 591-708

struct rq {
    /* runqueue lock: protects most rq fields */
    raw_spinlock_t lock;

    /*
     * nr_running and cpu_load should be in the same cacheline because
     * remote CPUs use both these fields when doing load calculation.
     */
    unsigned long nr_running;              // 该CPU上可运行任务数
    unsigned long cpu_load[CPU_LOAD_IDX_MAX]; // 负载历史数组

    /* capture load from *all* tasks on this cpu: */
    struct load_weight load;               // 总权重
    unsigned long nr_load_updates;
    u64 nr_switches;                       // 上下文切换计数

    struct cfs_rq cfs;                     // CFS运行队列（内嵌）
    struct rt_rq rt;                       // RT运行队列（内嵌）

#ifdef CONFIG_FAIR_GROUP_SCHED
    struct list_head leaf_cfs_rq_list;     // 用于cgroup的cfs_rq链表
#endif

    struct task_struct *curr, *idle, *stop; // 当前/空闲/停止任务指针
    unsigned long next_balance;             // 下次负载均衡时间
    struct mm_struct *prev_mm;              // 延迟释放的mm

    u64 clock;                              // 运行队列时钟
    u64 clock_task;                         // 任务时钟（排除irq时间）

#ifdef CONFIG_SMP
    struct root_domain *rd;                 // 根域（调度域树的根）
    struct sched_domain *sd;                // 调度域
    unsigned long cpu_power;                // CPU算力
    int cpu;                                // 该rq所属的CPU编号
    int online;                             // CPU是否在线
    // ... active balancing fields
#endif
};
```

**内存布局图：**

```
                    Per-CPU Runqueue (struct rq)
┌────────────────────────────────────────────────────────────────────────┐
│  lock (raw_spinlock_t)         ← 保护整个runqueue                       │
├────────────────────────────────────────────────────────────────────────┤
│  nr_running = 5                ← 可运行任务总数                          │
│  cpu_load[0..4]                ← 负载平均值（用于均衡）                   │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                     cfs_rq (embedded)                            │  │
│  │  nr_running = 4              ← CFS任务数                          │  │
│  │  min_vruntime = 1000000      ← 最小虚拟运行时间                    │  │
│  │  tasks_timeline ─────────────► Red-Black Tree                    │  │
│  │                                     ┌───┐                        │  │
│  │                                ┌────│ B │────┐                   │  │
│  │                                │    └───┘    │                   │  │
│  │                              ┌─┴─┐        ┌──┴──┐                │  │
│  │                              │ A │        │  C  │                │  │
│  │                              └───┘        └──┬──┘                │  │
│  │  rb_leftmost ──────────────────►             │                   │  │
│  │                                           ┌──┴──┐                │  │
│  │                                           │  D  │                │  │
│  │                                           └─────┘                │  │
│  │  curr → (current sched_entity)                                   │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                        │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                      rt_rq (embedded)                            │  │
│  │  rt_nr_running = 1           ← RT任务数                           │  │
│  │  active.bitmap[]             ← 优先级位图                          │  │
│  │  active.queue[0..99]         ← 100个优先级队列                     │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                        │
│  curr ────────────────────────► current task_struct                   │
│  idle ────────────────────────► idle task (swapper)                   │
│  clock = 123456789             ← 单调递增时钟                          │
└────────────────────────────────────────────────────────────────────────┘
```

**所有权与生命周期：**
- `struct rq` 是per-CPU静态分配：`DEFINE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues)`
- 生命周期与CPU相同（从boot到CPU offline）
- `cfs_rq`和`rt_rq`内嵌在`rq`中，不单独分配
- 访问必须持有`rq->lock`或在禁用抢占的情况下

### 3.3 The CFS Runqueue

```c
// kernel/sched.c, lines 336-414

struct cfs_rq {
    struct load_weight load;        // 该队列所有实体的总权重
    unsigned long nr_running;       // 可运行实体数
    unsigned long h_nr_running;     // 层次化任务计数（含子组）

    u64 exec_clock;                 // 执行时间累计
    u64 min_vruntime;               // 最小虚拟运行时间（关键！）

    struct rb_root tasks_timeline;  // 红黑树根（按vruntime排序）
    struct rb_node *rb_leftmost;    // 缓存的最左节点（下一个运行）

    struct sched_entity *curr;      // 当前运行的调度实体
    struct sched_entity *next;      // 明确要求下一个运行的
    struct sched_entity *last;      // 刚运行过的（buddy机制）
    struct sched_entity *skip;      // 要跳过的（yield）

#ifdef CONFIG_FAIR_GROUP_SCHED
    struct rq *rq;                  // 所属的runqueue
    struct task_group *tg;          // 所属的任务组
    struct list_head leaf_cfs_rq_list; // 叶子cfs_rq链表
#endif

#ifdef CONFIG_CFS_BANDWIDTH
    int runtime_enabled;            // 是否启用带宽控制
    s64 runtime_remaining;          // 剩余运行时间配额
    int throttled;                  // 是否被限流
#endif
};
```

**不变量（Invariants）：**
1. `min_vruntime` 只能单调递增（永不回退）
2. 所有在红黑树中的实体，其`vruntime >= min_vruntime`
3. `rb_leftmost`始终指向vruntime最小的实体（O(1)获取）
4. `nr_running == 红黑树中的节点数 + (curr在树外 ? 1 : 0)`

### 3.4 The Scheduling Entity (sched_entity)

```c
// include/linux/sched.h, lines 1169-1193

struct sched_entity {
    struct load_weight  load;           // 权重（由nice值决定）
    struct rb_node      run_node;       // 红黑树节点
    struct list_head    group_node;     // 组内链表节点
    unsigned int        on_rq;          // 是否在运行队列上

    u64                 exec_start;     // 开始执行的时间戳
    u64                 sum_exec_runtime; // 实际运行时间累计
    u64                 vruntime;       // 虚拟运行时间（核心！）
    u64                 prev_sum_exec_runtime; // 上次累计（用于计算delta）

    u64                 nr_migrations;  // 迁移次数

#ifdef CONFIG_FAIR_GROUP_SCHED
    struct sched_entity *parent;        // 父调度实体（层次调度）
    struct cfs_rq       *cfs_rq;        // 该实体所在的cfs_rq
    struct cfs_rq       *my_q;          // 该实体"拥有"的cfs_rq（如果是组）
#endif
};
```

**关键字段图解：**

```
task_struct
┌────────────────────────┐
│  ...                   │
│  const struct          │
│  sched_class *sched_class ──► fair_sched_class
│                        │
│  struct sched_entity se │
│  ┌──────────────────┐  │
│  │ load.weight=1024 │  │  ← nice 0 的权重
│  │ load.inv_weight  │  │  ← 权重的倒数（优化除法）
│  │                  │  │
│  │ vruntime=1500000 │  │  ← 虚拟运行时间（纳秒）
│  │                  │  │
│  │ run_node ────────┼──┼──► 嵌入到cfs_rq的红黑树
│  │                  │  │
│  │ on_rq = 1        │  │  ← 正在运行队列上
│  │                  │  │
│  │ exec_start       │  │  ← 开始执行时的clock_task
│  │ sum_exec_runtime │  │  ← 实际CPU时间
│  └──────────────────┘  │
│                        │
│  struct sched_rt_entity rt │
│  └──────────────────┘  │
│  ...                   │
└────────────────────────┘
```

### 3.5 The task_struct (Scheduler-Related Fields)

```c
// include/linux/sched.h, lines 1220-1240 (excerpt)

struct task_struct {
    volatile long state;      // -1:unrunnable, 0:runnable, >0:stopped

    int on_rq;                // 是否在runqueue上（0或1）
    int on_cpu;               // 是否正在CPU上执行（SMP）

    int prio;                 // 动态优先级（考虑PI后）
    int static_prio;          // 静态优先级（由nice设置）
    int normal_prio;          // 正常优先级（不考虑临时boost）
    unsigned int rt_priority; // RT优先级（0-99）

    const struct sched_class *sched_class; // 指向调度类
    struct sched_entity se;   // CFS调度实体
    struct sched_rt_entity rt; // RT调度实体

    unsigned int policy;      // SCHED_NORMAL, SCHED_FIFO, SCHED_RR等
    cpumask_t cpus_allowed;   // CPU亲和性掩码

    // ... 其他字段
};
```

**任务状态转换图：**

```
                    ┌─────────────────────────────────────────┐
                    │                                         │
                    ▼                                         │
     ┌──────────────────────────┐                             │
     │    TASK_RUNNING (R)      │                             │
     │    on_rq=1, on_cpu=0/1   │                             │
     │    在运行队列上/正在执行   │                             │
     └────────────┬─────────────┘                             │
                  │                                           │
       ┌──────────┼───────────┐                               │
       │          │           │                               │
       ▼          ▼           ▼                               │
  schedule()  do_exit()   wait_event()/                       │
  (preempt)               sleep_on()                          │
       │          │           │                               │
       │          │           ▼                               │
       │          │    ┌──────────────────────────┐           │
       │          │    │ TASK_INTERRUPTIBLE (S)   │           │
       │          │    │ TASK_UNINTERRUPTIBLE (D) │──────────►│
       │          │    │       on_rq=0            │  wake_up()│
       │          │    │   等待I/O或事件           │           │
       │          │    └──────────────────────────┘           │
       │          │                                           │
       │          ▼                                           │
       │   ┌──────────────────────────┐                       │
       │   │     EXIT_ZOMBIE (Z)      │                       │
       │   │   等待父进程wait()        │                       │
       │   └────────────┬─────────────┘                       │
       │                │ wait() by parent                    │
       │                ▼                                     │
       │   ┌──────────────────────────┐                       │
       │   │     EXIT_DEAD (X)        │                       │
       │   │   资源已释放              │                       │
       │   └──────────────────────────┘                       │
       │                                                      │
       └──────────────────────────────────────────────────────┘
```

**优先级映射：**

```
                Priority Number Line
      0                                                    139
      │◄────────── RT Priorities ──────────►│◄─── Nice ───►│
      │   (SCHED_FIFO/RR: 1-99 maps to 0-99)│(-20..+19)   │
      ├────────────────────────────────────┼──────────────┤
      0        ...        99              100    ...     139
      │                    │                │              │
      MAX_RT_PRIO-1        │           nice=-20      nice=+19
      (highest RT)         │          (highest normal)
                      MAX_RT_PRIO
                       (=100)

Nice to static_prio:  static_prio = MAX_RT_PRIO + nice + 20
                      static_prio = 100 + nice + 20
                      nice=0  → static_prio=120
                      nice=-20 → static_prio=100
                      nice=+19 → static_prio=139
```

---

## 4. Entry Points & Call Paths

### 4.1 Key Entry Points

| Entry Point | Trigger | Purpose |
|-------------|---------|---------|
| `schedule()` | Voluntary yield, wakeup, preemption | Main scheduler entry |
| `scheduler_tick()` | Timer interrupt (HZ times/sec) | Time accounting, preemption check |
| `try_to_wake_up()` | I/O completion, signal, mutex unlock | Wake sleeping task |
| `sched_fork()` | fork() / clone() | Initialize new task's sched data |
| `do_exit()` | Process termination | Clean up scheduler state |
| `sched_setscheduler()` | syscall | Change scheduling policy |
| `wake_up_process()` | Kernel internal | Simple wakeup wrapper |

### 4.2 The Main schedule() Call Path

```
┌───────────────────────────────────────────────────────────────────────────┐
│                           SCHEDULE() ENTRY POINTS                         │
└───────────────────────────────────────────────────────────────────────────┘
                    │
    ┌───────────────┼───────────────┬───────────────┐
    │               │               │               │
    ▼               ▼               ▼               ▼
Voluntary       Timer IRQ      Preemption      Wakeup
schedule()      check          Point           Preempt
    │               │               │               │
    │               │               │               │
    └───────────────┴───────────────┴───────────────┘
                            │
                            ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  schedule()                                          [kernel/sched.c:4486] │
│  ├── sched_submit_work()         ← 提交pending的块I/O                      │
│  └── __schedule()                ← 核心调度函数                            │
│       │                                                                   │
│       ├── preempt_disable()      ← 禁止抢占                               │
│       ├── cpu = smp_processor_id()                                       │
│       ├── rq = cpu_rq(cpu)       ← 获取当前CPU的runqueue                   │
│       ├── prev = rq->curr        ← 保存当前任务指针                        │
│       │                                                                   │
│       ├── raw_spin_lock_irq(&rq->lock)  ← 获取runqueue锁                  │
│       │                                                                   │
│       ├── if (prev->state && !(preempt_count() & PREEMPT_ACTIVE))        │
│       │   └── deactivate_task(rq, prev)  ← 移出运行队列                   │
│       │                                                                   │
│       ├── put_prev_task(rq, prev)     ← 让出CPU前的清理                   │
│       │   └── prev->sched_class->put_prev_task(rq, prev)                 │
│       │                                                                   │
│       ├── next = pick_next_task(rq)   ← 选择下一个任务（核心！）           │
│       │   │                                                              │
│       │   │  ┌─────────────────────────────────────────────────────┐     │
│       │   │  │ Optimization: if all tasks are CFS:                 │     │
│       │   │  │   if (rq->nr_running == rq->cfs.h_nr_running)       │     │
│       │   │  │     return fair_sched_class.pick_next_task(rq)      │     │
│       │   │  │                                                     │     │
│       │   │  │ Otherwise, iterate through sched classes:           │     │
│       │   │  │   for_each_class(class)                             │     │
│       │   │  │     if (p = class->pick_next_task(rq))             │     │
│       │   │  │       return p;                                     │     │
│       │   │  └─────────────────────────────────────────────────────┘     │
│       │   │                                                              │
│       │   └── Returns: task_struct * (never NULL due to idle)           │
│       │                                                                   │
│       ├── if (prev != next)                                              │
│       │   ├── rq->curr = next                                            │
│       │   └── context_switch(rq, prev, next)  ← 上下文切换！              │
│       │       │                                                          │
│       │       ├── prepare_task_switch()                                  │
│       │       ├── switch_mm(oldmm, mm, next)  ← 切换地址空间              │
│       │       ├── switch_to(prev, next, prev) ← 切换寄存器/栈            │
│       │       └── finish_task_switch()        ← 完成清理                 │
│       │                                                                   │
│       ├── raw_spin_unlock_irq(&rq->lock)     （在context_switch中释放）  │
│       │                                                                   │
│       └── if (need_resched())                                            │
│           └── goto need_resched;  ← 可能循环再次调度                      │
└───────────────────────────────────────────────────────────────────────────┘
```

### 4.3 The Wakeup Path (try_to_wake_up)

```
I/O Completion / Signal / Mutex Unlock
                │
                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  try_to_wake_up(p, state, wake_flags)               [kernel/sched.c:2821] │
│       │                                                                   │
│       ├── raw_spin_lock_irqsave(&p->pi_lock)                             │
│       ├── if (!(p->state & state)) goto out;  ← 状态检查                  │
│       │                                                                   │
│       ├── #ifdef CONFIG_SMP                                              │
│       │   ├── while (p->on_cpu) cpu_relax();  ← 等待任务离开CPU          │
│       │   ├── p->state = TASK_WAKING                                     │
│       │   ├── if (p->sched_class->task_waking)                          │
│       │   │       p->sched_class->task_waking(p);                        │
│       │   ├── cpu = select_task_rq(p, ...)   ← 选择目标CPU               │
│       │   └── set_task_cpu(p, cpu)           ← 设置任务的CPU             │
│       │   #endif                                                         │
│       │                                                                   │
│       ├── ttwu_queue(p, cpu)                 ← 入队                       │
│       │   ├── ttwu_activate(rq, p, ...)                                  │
│       │   │   └── activate_task(rq, p, ...)                              │
│       │   │       └── enqueue_task(rq, p, flags)                         │
│       │   │           └── p->sched_class->enqueue_task(rq, p, flags)    │
│       │   │                                                              │
│       │   └── ttwu_do_wakeup(rq, p, ...)                                │
│       │       ├── p->state = TASK_RUNNING                                │
│       │       └── check_preempt_curr(rq, p)  ← 检查是否要抢占            │
│       │           └── rq->curr->sched_class->check_preempt_curr(rq, p)  │
│       │               │                                                  │
│       │               └── CFS: check_preempt_wakeup()                   │
│       │                   if (should_preempt)                            │
│       │                       resched_task(curr) ← 设置TIF_NEED_RESCHED │
│       │                                                                   │
│       └── raw_spin_unlock_irqrestore(&p->pi_lock)                        │
│                                                                           │
│  Returns: 1 if woken, 0 if already running                               │
└───────────────────────────────────────────────────────────────────────────┘
```

### 4.4 Timer Tick Path

```
Hardware Timer Interrupt
        │
        ▼
  timer_interrupt()
        │
        ▼
  update_process_times()
        │
        ▼
┌───────────────────────────────────────────────────────────────────────────┐
│  scheduler_tick()                                   [kernel/sched.c:3974] │
│       │                                                                   │
│       ├── raw_spin_lock(&rq->lock)                                       │
│       │                                                                   │
│       ├── update_rq_clock(rq)            ← 更新runqueue时钟              │
│       │                                                                   │
│       ├── update_cpu_load_active(rq)     ← 更新CPU负载统计               │
│       │                                                                   │
│       ├── curr->sched_class->task_tick(rq, curr, 0)                      │
│       │   │                                                              │
│       │   └── CFS: task_tick_fair()                                      │
│       │       └── entity_tick(cfs_rq, se)                                │
│       │           ├── update_curr(cfs_rq)     ← 更新vruntime             │
│       │           └── check_preempt_tick(cfs_rq, curr)                   │
│       │               │                                                  │
│       │               │  if (delta_exec > ideal_runtime)                 │
│       │               │      resched_task(curr);                         │
│       │               │  // 如果已用时间超过理想份额，标记重调度          │
│       │                                                                   │
│       ├── raw_spin_unlock(&rq->lock)                                     │
│       │                                                                   │
│       └── trigger_load_balance(rq)       ← 触发负载均衡                  │
│           └── raise_softirq(SCHED_SOFTIRQ)                               │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Core Workflows

### 5.1 Initialization (sched_init)

**触发时机**: 内核启动早期，由`start_kernel()`调用

```c
// kernel/sched.c:8196 - sched_init()

void __init sched_init(void)
{
    // 1. 分配per-cpu数据结构
    for_each_possible_cpu(i) {
        struct rq *rq = cpu_rq(i);
        
        raw_spin_lock_init(&rq->lock);
        rq->nr_running = 0;
        rq->calc_load_active = 0;
        
        // 初始化CFS runqueue
        init_cfs_rq(&rq->cfs);
        // 初始化RT runqueue  
        init_rt_rq(&rq->rt, rq);
        
#ifdef CONFIG_FAIR_GROUP_SCHED
        // 初始化根任务组
        root_task_group.shares = root_task_group_load;
        INIT_LIST_HEAD(&rq->leaf_cfs_rq_list);
        rq->cfs.rq = rq;
        rq->cfs.tg = &root_task_group;
#endif
        rq->cpu = i;
        rq->online = 0;
    }
    
    // 2. 设置idle进程
    set_load_weight(&init_task);  // init_task是第一个进程(PID 0)
    
    // 3. 初始化调度域（SMP）
    init_defrootdomain();
    init_rt_bandwidth(&def_rt_bandwidth, ...);
}
```

**状态变化**:
- 系统从无调度器状态 → 调度器就绪
- 每个CPU的runqueue被初始化
- idle进程(swapper)被设置为各CPU的初始任务

### 5.2 Fork Path (Creating New Task)

**触发时机**: fork(), clone(), kernel_thread()

```c
// kernel/fork.c (调用) → kernel/sched.c

copy_process()
    └── sched_fork(p)  // kernel/sched.c
        │
        ├── p->state = TASK_RUNNING
        │   // 新进程初始状态设为RUNNING，但尚未入队
        │
        ├── p->prio = current->normal_prio
        │   // 继承父进程优先级
        │
        ├── p->sched_class = &fair_sched_class
        │   // 默认使用CFS（除非是内核线程或设置了RT）
        │
        ├── p->se.vruntime = 0
        │   // 初始化调度实体
        │
        └── task_fork_fair(p)  // CFS特定初始化
            │
            ├── update_curr(cfs_rq)
            │   // 更新父进程的运行时间
            │
            ├── p->se.vruntime = cfs_rq->min_vruntime
            │   // 子进程vruntime从当前最小值开始
            │
            └── place_entity(cfs_rq, &p->se, 1)
                // 可能给新任务一个惩罚（START_DEBIT特性）
                // 防止fork炸弹获得不公平优势
```

```
wake_up_new_task()  // 新进程首次被唤醒
    │
    ├── activate_task(rq, p, 0)
    │   └── enqueue_task(rq, p, flags)
    │       └── enqueue_task_fair(rq, p, flags)
    │           └── enqueue_entity(cfs_rq, se, flags)
    │               └── __enqueue_entity(cfs_rq, se)
    │                   // 插入红黑树
    │
    └── check_preempt_curr(rq, p, WF_FORK)
        // 检查是否应该抢占父进程
```

### 5.3 The Fast Path: pick_next_task_fair

这是调度器最热的路径（被`schedule()`频繁调用）：

```c
// kernel/sched_fair.c:2679

static struct task_struct *pick_next_task_fair(struct rq *rq)
{
    struct cfs_rq *cfs_rq = &rq->cfs;
    struct sched_entity *se;
    
    // 如果没有CFS任务，返回NULL让其他类处理
    if (!cfs_rq->nr_running)
        return NULL;
    
    // 层次调度：从顶层cfs_rq向下遍历
    do {
        se = pick_next_entity(cfs_rq);   // 选择vruntime最小的
        set_next_entity(cfs_rq, se);     // 设置为当前运行实体
        cfs_rq = group_cfs_rq(se);       // 如果是组，进入其cfs_rq
    } while (cfs_rq);
    
    // 到达叶子节点，获取task_struct
    struct task_struct *p = task_of(se);
    
    return p;
}

// pick_next_entity的核心：
static struct sched_entity *pick_next_entity(struct cfs_rq *cfs_rq)
{
    // 优化：rb_leftmost已缓存，O(1)获取
    struct sched_entity *left = __pick_first_entity(cfs_rq);
    struct sched_entity *se;
    
    // Buddy机制：优先选择next/last buddy
    se = left;  // 默认选最左（vruntime最小）
    
    // 但可能因为cache locality选择buddy
    if (cfs_rq->next && wakeup_preempt_entity(left, cfs_rq->next) < 1)
        se = cfs_rq->next;
    if (cfs_rq->last && wakeup_preempt_entity(left, cfs_rq->last) < 1)
        se = cfs_rq->last;
        
    // skip用于yield()
    if (cfs_rq->skip == se)
        se = pick_next_entity(...);
        
    return se;
}
```

**性能关键点**:
- `rb_leftmost`缓存使得"找最小vruntime"是O(1)
- Buddy机制提高cache locality
- 大部分情况下，`rq->nr_running == rq->cfs.h_nr_running`优化成立

### 5.4 The Slow Path: Load Balancing

**触发时机**: 周期性（scheduler_tick触发的softirq）或idle时

```
scheduler_tick()
    └── trigger_load_balance(rq)
        └── raise_softirq(SCHED_SOFTIRQ)
                │
                ▼
        run_rebalance_domains()  // softirq handler
                │
┌───────────────┴───────────────────────────────────────────────────────────┐
│  rebalance_domains(cpu)                                                   │
│       │                                                                   │
│       for_each_domain(cpu, sd) {  // 遍历调度域层次                       │
│           │                                                               │
│           ├── 检查是否到达balance_interval                                │
│           │                                                               │
│           └── load_balance(cpu, rq, sd, idle)                            │
│               │                                                          │
│               ├── find_busiest_group(sd, cpu, &imbalance, ...)          │
│               │   // 找到最繁忙的调度组                                   │
│               │   // 计算不平衡量imbalance                               │
│               │                                                          │
│               ├── find_busiest_queue(sd, group, ...)                    │
│               │   // 在最繁忙组中找最繁忙CPU                             │
│               │                                                          │
│               └── move_tasks(this_rq, this_cpu, busiest, imbalance)     │
│                   │                                                      │
│                   while (imbalance > 0) {                               │
│                       p = pick a task from busiest                       │
│                       deactivate_task(busiest, p)                       │
│                       set_task_cpu(p, this_cpu)                          │
│                       activate_task(this_rq, p)                         │
│                       imbalance -= task_load                             │
│                   }                                                      │
│       }                                                                   │
└───────────────────────────────────────────────────────────────────────────┘
```

**调度域层次**:

```
                        Root Domain (all CPUs)
                               │
              ┌────────────────┴────────────────┐
              │                                 │
        NUMA Node 0                       NUMA Node 1
         Domain                             Domain
              │                                 │
    ┌─────────┴─────────┐             ┌─────────┴─────────┐
    │                   │             │                   │
  Package 0          Package 1      Package 2          Package 3
   Domain             Domain         Domain             Domain
    │                   │             │                   │
  ┌─┴─┐               ┌─┴─┐         ┌─┴─┐               ┌─┴─┐
  │   │               │   │         │   │               │   │
CPU0 CPU1           CPU2 CPU3     CPU4 CPU5           CPU6 CPU7
 (SMT siblings)
```

---

## 6. Important Algorithms & Mechanisms

### 6.1 Virtual Runtime (vruntime) - CFS的核心

**核心公式**:
```
                     NICE_0_LOAD        1024
delta_vruntime = ───────────────── × delta_exec = ────── × delta_exec
                   task_weight                     weight
```

**权重表** (kernel/sched.c):
```c
static const int prio_to_weight[40] = {
 /* -20 */ 88761, 71755, 56483, 46273, 36291,
 /* -15 */ 29154, 23254, 18705, 14949, 11916,
 /* -10 */  9548,  7620,  6100,  4904,  3906,
 /*  -5 */  3121,  2501,  1991,  1586,  1277,
 /*   0 */  1024,   820,   655,   526,   423,  ← nice 0 = 1024
 /*   5 */   335,   272,   215,   172,   137,
 /*  10 */   110,    87,    70,    56,    45,
 /*  15 */    36,    29,    23,    18,    15,
};
```

**示例计算**:
```
Task A: nice=0,  weight=1024, 运行 10ms → vruntime += 10ms × 1 = 10ms
Task B: nice=5,  weight=335,  运行 10ms → vruntime += 10ms × 3.06 ≈ 30.6ms
Task C: nice=-5, weight=3121, 运行 10ms → vruntime += 10ms × 0.33 ≈ 3.3ms
```

**图示**:
```
vruntime视角下的"公平时间"：

nice值       权重      运行10ms后的vruntime增量      相对速度
  -5        3121            ~3.3ms                   慢（可多跑）
   0        1024            10.0ms                   基准
  +5         335            ~30.6ms                  快（少跑）

┌─────────────────────────────────────────────────────────────────────┐
│  vruntime时间线（所有任务目标：相同的vruntime）                      │
│                                                                     │
│  0        50ms      100ms      150ms      200ms      250ms          │
│  ├─────────┼─────────┼──────────┼──────────┼──────────┤             │
│                                                                     │
│  Task C (nice=-5):  ████████████████████░░░░░░░░░░░░░ (75ms real)  │
│  Task A (nice=0):   ████████░░░░░░░░░░░░░░░░░░░░░░░░░ (25ms real)  │
│  Task B (nice=+5):  ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ (8ms real)   │
│                     ↑                                               │
│                all reach same vruntime ≈ 25ms                       │
│                                                                     │
│  结果：高权重任务获得更多实际CPU时间，但vruntime增长相同             │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.2 The Red-Black Tree Operations

```c
// kernel/sched_fair.c:368 - __enqueue_entity

static void __enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
    struct rb_node **link = &cfs_rq->tasks_timeline.rb_node;
    struct rb_node *parent = NULL;
    struct sched_entity *entry;
    int leftmost = 1;  // 假设是最左

    // 标准红黑树插入
    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct sched_entity, run_node);
        
        if (entity_before(se, entry)) {  // se->vruntime < entry->vruntime ?
            link = &parent->rb_left;
        } else {
            link = &parent->rb_right;
            leftmost = 0;  // 不是最左了
        }
    }

    // 维护rb_leftmost缓存
    if (leftmost)
        cfs_rq->rb_leftmost = &se->run_node;

    rb_link_node(&se->run_node, parent, link);
    rb_insert_color(&se->run_node, &cfs_rq->tasks_timeline);
}
```

**红黑树可视化**:
```
                    Red-Black Tree (by vruntime)
                    
                         ┌───────────────┐
                         │  vruntime=100 │ (BLACK)
                         │    Task B     │
                         └───────┬───────┘
                    ┌────────────┴────────────┐
                    ▼                         ▼
           ┌───────────────┐         ┌───────────────┐
           │ vruntime=80   │ (RED)   │ vruntime=150  │ (RED)
           │   Task A      │         │    Task C     │
           └───────┬───────┘         └───────┬───────┘
              ┌────┴────┐               ┌────┴────┐
              ▼         ▼               ▼         ▼
           ┌─────┐   ┌─────┐         NULL     ┌─────┐
           │ 60  │   │ 90  │                  │ 200 │
           │  D  │   │  E  │                  │  F  │
           └─────┘   └─────┘                  └─────┘
              ↑
         rb_leftmost (cached)
         
Operations:
  - pick_next: O(1) via rb_leftmost
  - enqueue:   O(log n) insert + rebalance
  - dequeue:   O(log n) delete + rebalance
```

### 6.3 Time Slice Calculation

CFS没有固定时间片，而是根据任务数动态计算"理想运行时间"：

```c
// kernel/sched_fair.c:495

// 调度周期：所有任务都应该运行一次的时间
static u64 __sched_period(unsigned long nr_running)
{
    u64 period = sysctl_sched_latency;  // 默认6ms
    unsigned long nr_latency = sched_nr_latency;  // 默认8

    // 如果任务太多，每个任务至少保证min_granularity
    if (unlikely(nr_running > nr_latency)) {
        period = sysctl_sched_min_granularity;  // 0.75ms
        period *= nr_running;
    }

    return period;
}

// 任务的理想时间片
static u64 sched_slice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
    u64 slice = __sched_period(cfs_rq->nr_running);
    
    // 按权重比例分配
    // slice = period × (se->weight / total_weight)
    slice = calc_delta_mine(slice, se->load.weight, &cfs_rq->load);
    
    return slice;
}
```

**示例**:
```
假设3个任务，权重分别为1024, 512, 512，总权重2048
调度周期 period = 6ms

Task A (1024): slice = 6ms × (1024/2048) = 3ms
Task B (512):  slice = 6ms × (512/2048)  = 1.5ms
Task C (512):  slice = 6ms × (512/2048)  = 1.5ms

时间线：
|--A(3ms)--|--B(1.5ms)--|--C(1.5ms)--|--A(3ms)--|...
└────────────── 6ms period ──────────────┘
```

### 6.4 Preemption Decision (check_preempt_wakeup)

```c
// kernel/sched_fair.c:2611

static void check_preempt_wakeup(struct rq *rq, struct task_struct *p, int wake_flags)
{
    struct sched_entity *se = &curr->se, *pse = &p->se;
    
    // RT任务总是抢占CFS任务
    if (test_tsk_need_resched(curr))
        return;
        
    // 同一层级比较vruntime
    find_matching_se(&se, &pse);
    
    // 核心判断：新任务的vruntime是否足够小
    if (wakeup_preempt_entity(se, pse) == 1) {
        resched_task(curr);  // 设置TIF_NEED_RESCHED
    }
}

static int wakeup_preempt_entity(struct sched_entity *curr, struct sched_entity *se)
{
    s64 gran, vdiff = curr->vruntime - se->vruntime;
    
    if (vdiff <= 0)
        return -1;  // se的vruntime更大，不抢占
        
    // 只有差距超过wakeup_granularity才抢占
    gran = sysctl_sched_wakeup_granularity;  // 默认1ms
    if (vdiff > gran)
        return 1;  // 抢占！
        
    return 0;  // 差距太小，不值得切换
}
```

---

## 7. Concurrency & Synchronization

### 7.1 Locking Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SCHEDULER LOCKING HIERARCHY                         │
└─────────────────────────────────────────────────────────────────────────────┘

Priority (must acquire in this order to avoid deadlock):

1. p->pi_lock (per-task)          ← 保护优先级继承、wakeup
   │
   └──► 2. rq->lock (per-cpu)     ← 保护runqueue的所有字段
          │
          ├──► 3. rq->lock (another CPU)  ← 双锁用于任务迁移
          │    (always lock lower CPU first to avoid ABBA deadlock)
          │
          └──► 4. Various per-cfs_rq locks
                  (rt_runtime_lock, cfs_bandwidth->lock)

特殊情况：
- tasklist_lock: 保护进程树遍历（读写锁）
- sched_domains_mutex: 保护调度域拓扑变更
```

### 7.2 The rq->lock Spinlock

```c
// 典型的获取模式
static inline void __schedule(void)
{
    // ... 
    raw_spin_lock_irq(&rq->lock);  // 禁中断 + 获取锁
    
    // ... 访问rq的所有字段 ...
    
    if (likely(prev != next)) {
        context_switch(rq, prev, next);
        // 注意：context_switch内部会释放锁！
        // 这是合法的，因为新任务会继续持有"逻辑"锁
    } else {
        raw_spin_unlock_irq(&rq->lock);
    }
}
```

**为什么用raw_spinlock？**
- `raw_spinlock_t`在RT-Preempt内核中也不会转为mutex
- 调度路径必须是原子的，不能被抢占

### 7.3 双锁与任务迁移

```c
// 当需要在两个CPU间迁移任务时
static void double_rq_lock(struct rq *rq1, struct rq *rq2)
{
    // 按CPU编号排序，避免死锁
    if (rq1 < rq2) {
        raw_spin_lock(&rq1->lock);
        raw_spin_lock_nested(&rq2->lock, SINGLE_DEPTH_NESTING);
    } else {
        raw_spin_lock(&rq2->lock);
        raw_spin_lock_nested(&rq1->lock, SINGLE_DEPTH_NESTING);
    }
}
```

### 7.4 RCU在调度中的应用

```c
// 调度域的读取（无锁）
for_each_domain(cpu, sd) {
    // sd通过RCU保护
    // 在preempt_disable()期间安全访问
}

// 调度域的更新（需要同步）
detach_destroy_domains()
{
    // ...
    synchronize_sched();  // 等待所有CPU完成RCU宽限期
    // 现在可以安全释放旧的调度域
}
```

### 7.5 若同步出错会怎样？

| 场景 | 后果 |
|------|------|
| rq->lock不持有时修改nr_running | 数据竞争，可能导致任务丢失或计数错误 |
| 双锁顺序错误 | ABBA死锁，系统hang |
| 唤醒时不检查on_cpu | 可能在任务还在另一个CPU上运行时就入队 |
| 不禁止中断获取rq->lock | 中断处理程序可能死锁 |
| 调度路径中睡眠 | 无限递归调度 |

---

## 8. Performance Considerations

### 8.1 Hot Paths vs Cold Paths

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  HOT PATHS (性能关键，频繁执行)                                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  schedule() → __schedule() → pick_next_task() → context_switch()           │
│       ↑                                                                     │
│       │ 每秒可能调用数千次                                                   │
│       │                                                                     │
│  scheduler_tick() → task_tick_fair() → update_curr()                       │
│       ↑                                                                     │
│       │ 每秒HZ次（通常100-1000次）                                          │
│       │                                                                     │
│  try_to_wake_up() → enqueue_task_fair() → __enqueue_entity()               │
│       ↑                                                                     │
│       │ 每次I/O完成、信号、锁释放都会触发                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│  COLD PATHS (较少执行)                                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  load_balance() → move_tasks()                                              │
│       ↑                                                                     │
│       │ 周期性（毫秒级），或idle时                                           │
│                                                                             │
│  sched_fork() / sched_setscheduler() / sched_setaffinity()                 │
│       ↑                                                                     │
│       │ 进程创建/策略变更时                                                  │
│                                                                             │
│  sched_init() / partition_sched_domains()                                  │
│       ↑                                                                     │
│       │ 启动时或CPU热插拔                                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Cacheline Considerations

```c
// kernel/sched.c - struct rq定义中的注释

struct rq {
    raw_spinlock_t lock;
    
    /*
     * nr_running and cpu_load should be in the same cacheline because
     * remote CPUs use both these fields when doing load calculation.
     */
    unsigned long nr_running;
    unsigned long cpu_load[CPU_LOAD_IDX_MAX];
    // ...
};

// DEFINE_PER_CPU_SHARED_ALIGNED确保每个rq在独立cacheline
```

**关键优化**:
- rq是per-CPU的，避免false sharing
- `rb_leftmost`缓存避免每次遍历树
- `cpu_load`数组按时间衰减，避免每次重算

### 8.3 O(1) vs O(log n)

| 操作 | 复杂度 | 实现 |
|------|--------|------|
| pick_next_task (CFS) | O(1) | rb_leftmost缓存 |
| enqueue_task (CFS) | O(log n) | 红黑树插入 |
| dequeue_task (CFS) | O(log n) | 红黑树删除 |
| pick_next_task (RT) | O(1) | bitmap + 优先级队列 |
| 调度类遍历 | O(k) | k=4（stop/rt/fair/idle） |

### 8.4 Linux 3.2 的可伸缩性限制

1. **单一rq->lock**：每个CPU一把大锁，高并发wakeup可能争用
2. **全局负载均衡**：大型NUMA系统上开销大
3. **CFS bandwidth throttling**：v3.2是新功能，可能有性能问题
4. **Autogroup**：桌面优化，服务器可能需要禁用

---

## 9. Common Pitfalls & Bugs

### 9.1 Typical Mistakes

| 错误 | 后果 | 正确做法 |
|------|------|----------|
| 在持有rq->lock时睡眠 | 死锁 | 使用原子操作或先释放锁 |
| 忘记更新nr_running | 任务"消失"或重复 | 确保enqueue/dequeue配对 |
| vruntime溢出 | 调度混乱 | 使用min_vruntime作为基准点 |
| 直接修改task->prio | 破坏优先级继承 | 使用set_user_nice() |
| 在中断上下文调用schedule() | panic | 只在进程上下文调度 |

### 9.2 Subtle Bugs This Code Avoids

**min_vruntime的单调性**:
```c
static void update_min_vruntime(struct cfs_rq *cfs_rq)
{
    // ...
    // 关键：只能增加，不能减少
    cfs_rq->min_vruntime = max_vruntime(cfs_rq->min_vruntime, vruntime);
}
```
若允许min_vruntime减少，长时间睡眠的任务醒来后会获得不公平的优势。

**on_cpu等待**:
```c
static int try_to_wake_up(...)
{
    // 必须等待任务完全离开CPU
    while (p->on_cpu) {
        cpu_relax();
    }
    // 现在安全入队
}
```
若不等待，可能同一个任务同时在两个CPU"运行"。

### 9.3 Historical Issues in v3.2

1. **CFS Bandwidth的throttle/unthrottle竞态**（后续版本修复）
2. **NOHZ（动态tick）与负载均衡的交互问题**
3. **cgroup迁移时的权重计算bug**

---

## 10. How to Read This Code Yourself

### 10.1 Recommended Reading Order

```
第一阶段：理解核心路径
1. kernel/sched.c: schedule() 和 __schedule()
2. kernel/sched.c: pick_next_task()
3. kernel/sched.c: context_switch()
4. include/linux/sched.h: struct sched_class 定义

第二阶段：深入CFS
5. kernel/sched_fair.c: pick_next_task_fair()
6. kernel/sched_fair.c: enqueue_task_fair() / dequeue_task_fair()
7. kernel/sched_fair.c: update_curr() - vruntime更新
8. kernel/sched_fair.c: __enqueue_entity() - 红黑树操作

第三阶段：理解数据结构
9.  kernel/sched.c: struct rq 定义及初始化
10. kernel/sched.c: struct cfs_rq 定义
11. include/linux/sched.h: struct task_struct 调度相关字段
12. include/linux/sched.h: struct sched_entity

第四阶段：SMP与负载均衡
13. kernel/sched_fair.c: load_balance()
14. kernel/sched_fair.c: find_busiest_group() / find_busiest_queue()
15. include/linux/sched.h: struct sched_domain

第五阶段：高级主题
16. kernel/sched_rt.c: RT调度类
17. kernel/sched_fair.c: CFS bandwidth控制
18. kernel/sched_autogroup.c: 自动分组
```

### 10.2 What to Ignore Initially

- `CONFIG_*` 条件编译分支（先理解默认配置）
- `schedstat_*` 统计宏
- `trace_*` 追踪点
- `SCHED_DEBUG` 相关代码
- `sched_debug.c` 整个文件
- 各种`_unlikely()`优化路径

### 10.3 Useful Grep/Cscope Commands

```bash
# 找调度类定义
grep -n "static const struct sched_class" kernel/sched*.c

# 找schedule入口
grep -n "^asmlinkage.*schedule" kernel/sched.c

# 找vruntime更新位置
grep -n "vruntime +=" kernel/sched*.c

# 找所有抢占检查点
grep -rn "resched_task\|set_tsk_need_resched" kernel/

# 使用cscope
# 在内核根目录: make cscope
# cscope -d
# 查找符号定义: Ctrl+\ s
# 查找调用者: Ctrl+\ c
```

### 10.4 Debugging Tips

```bash
# 查看运行时调度统计
cat /proc/schedstat
cat /proc/sched_debug

# 查看特定进程的调度信息
cat /proc/<pid>/sched

# 调整调度参数（需要root）
echo 10000000 > /proc/sys/kernel/sched_latency_ns
echo 1000000 > /proc/sys/kernel/sched_min_granularity_ns

# ftrace追踪调度事件
echo 1 > /sys/kernel/debug/tracing/events/sched/enable
cat /sys/kernel/debug/tracing/trace
```

---

## 11. Summary & Mental Model

### One-Paragraph Summary

Linux 3.2的调度器采用模块化设计，通过`sched_class`接口支持多种调度策略。默认的CFS调度器使用**虚拟运行时间(vruntime)**追踪每个任务的"公平份额"，任务按vruntime排序存储在红黑树中，调度器总是选择vruntime最小的任务运行。每个CPU有独立的运行队列(`struct rq`)，通过周期性的负载均衡在CPU间迁移任务。整个系统由一个中心调度循环(`schedule()`)驱动，该函数在任务主动让出CPU、被抢占、或时间片耗尽时被调用。

### Key Invariants

1. **vruntime单调递增**：`cfs_rq->min_vruntime`只增不减
2. **权重守恒**：`cfs_rq->load.weight == Σ(se->load.weight)` for all entities on rq
3. **树一致性**：红黑树中的实体按vruntime有序，rb_leftmost正确
4. **锁序**：`p->pi_lock` before `rq->lock` before `another rq->lock`
5. **状态一致**：`on_rq==1 ⟺ 任务在某个runqueue的红黑树或rt队列中`

### Mental Model

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  把调度器想象成一个"公平的银行排号系统"：                                     │
│                                                                             │
│  • 每个任务有一个"已服务时间"计数器(vruntime)                                │
│  • 权重高的任务(nice低)，计数器增长慢                                        │
│  • 权重低的任务(nice高)，计数器增长快                                        │
│  • 调度器总是服务"已服务时间最少"的任务                                       │
│  • 最终所有任务的计数器趋于相同，实现"公平"                                   │
│                                                                             │
│  类比：                                                                      │
│  - VIP客户(高权重)：柜员处理慢，可以久坐                                     │
│  - 普通客户(低权重)：柜员处理快，很快被叫下一个                               │
│  - 结果：VIP获得更多服务时间，但每个人的"等待体验"是公平的                   │
│                                                                             │
│  SMP负载均衡就像多个柜台，会把等待太久的客户转移到空闲柜台                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. What to Study Next

### Recommended Learning Order

| 顺序 | 子系统 | 与调度器的关系 |
|------|--------|----------------|
| 1 | **进程管理 (fork/exit)** | 任务创建和销毁时的调度交互 |
| 2 | **中断与软中断** | 调度的触发源 (timer_interrupt) |
| 3 | **内存管理 (mm)** | context_switch需要切换页表 |
| 4 | **同步原语 (锁)** | 等待/唤醒机制依赖调度 |
| 5 | **时间子系统 (timers)** | 时间片、高精度调度 |
| 6 | **Cgroups** | 资源限制，组调度 |
| 7 | **CPU热插拔** | 调度域动态变化 |
| 8 | **NUMA** | 跨节点负载均衡优化 |

### Related Subsystem Files

```
kernel/fork.c              - copy_process(), wake_up_new_task()
kernel/exit.c              - do_exit(), 任务终止
kernel/signal.c            - 信号与唤醒
kernel/timer.c             - 传统定时器
kernel/hrtimer.c           - 高精度定时器
kernel/softirq.c           - 软中断处理
arch/x86/kernel/entry_*.S  - 系统调用/中断入口
arch/x86/kernel/process*.c - context_switch底层实现
mm/mmap.c                  - 地址空间管理
```

---

## Appendix: Quick Reference

### A. Scheduling Policies

| Policy | Class | Priority Range | Behavior |
|--------|-------|----------------|----------|
| SCHED_NORMAL | fair | nice -20 to +19 | CFS, 基于vruntime的公平调度 |
| SCHED_BATCH | fair | nice -20 to +19 | CFS, 但不会抢占（适合批处理） |
| SCHED_IDLE | fair | nice -20 to +19 | 最低优先级，只在系统空闲时运行 |
| SCHED_FIFO | rt | 1-99 | 先入先出，不让出CPU直到阻塞/yield |
| SCHED_RR | rt | 1-99 | 时间片轮转，同优先级间共享CPU |

### B. Key sysctl Tunables

| Parameter | Default | Description |
|-----------|---------|-------------|
| sched_latency_ns | 6000000 (6ms) | 调度周期目标 |
| sched_min_granularity_ns | 750000 (0.75ms) | 最小时间片 |
| sched_wakeup_granularity_ns | 1000000 (1ms) | 唤醒抢占阈值 |
| sched_migration_cost_ns | 500000 (0.5ms) | 迁移代价（影响均衡决策） |
| sched_nr_migrate | 32 | 每次均衡最多迁移任务数 |

### C. Debug Interfaces

```bash
/proc/sched_debug          # 全局调度调试信息
/proc/<pid>/sched          # 进程调度统计
/proc/schedstat            # 调度统计汇总
/sys/kernel/debug/sched_features  # 调度特性开关
```

---

**Author**: Linux Kernel Study Guide  
**Kernel Version**: 3.2.0 ("Saber-toothed Squirrel")  
**Last Updated**: Based on kernel source analysis

