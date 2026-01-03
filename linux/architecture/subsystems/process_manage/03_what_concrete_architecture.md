# WHAT｜具体架构

## 1. 使用的架构模式

```
ARCHITECTURAL PATTERNS IN LINUX SCHEDULING
+=============================================================================+
|                                                                              |
|  PATTERN 1: CLASS-BASED SCHEDULING (基于类的调度)                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Problem: Different workloads need different scheduling algorithms       │ |
|  │                                                                          │ |
|  │  Solution: Scheduler classes with uniform interface                      │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                      schedule() core                             │    │ |
|  │  │                           │                                      │    │ |
|  │  │          ┌────────────────┼────────────────┐                     │    │ |
|  │  │          │                │                │                     │    │ |
|  │  │          ▼                ▼                ▼                     │    │ |
|  │  │  ┌──────────────┐ ┌──────────────┐ ┌──────────────┐             │    │ |
|  │  │  │  dl_sched    │ │  rt_sched    │ │  fair_sched  │             │    │ |
|  │  │  │  ───────────  │ │  ───────────  │ │  ───────────  │           │    │ |
|  │  │  │ EDF algorithm │ │ Fixed prio   │ │ CFS vruntime │             │    │ |
|  │  │  │ Deadline guar │ │ FIFO or RR   │ │ Weighted fair│             │    │ |
|  │  │  └──────────────┘ └──────────────┘ └──────────────┘             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Each class implements:                                          │    │ |
|  │  │  • enqueue_task()   - Add task to class's run queue              │    │ |
|  │  │  • dequeue_task()   - Remove task from run queue                 │    │ |
|  │  │  • pick_next_task() - Select best candidate                      │    │ |
|  │  │  • check_preempt()  - Should new task preempt current?           │    │ |
|  │  │  • task_tick()      - Timer tick processing                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Benefit: New scheduling algorithms without touching core               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 2: OPS-TABLE POLYMORPHISM (操作表多态)                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct sched_class {                                                    │ |
|  │      const struct sched_class *next;                                    │ |
|  │                                                                          │ |
|  │      void (*enqueue_task)(rq, task, flags);     /* vtable entry */      │ |
|  │      void (*dequeue_task)(rq, task, flags);     /* vtable entry */      │ |
|  │      struct task_struct *(*pick_next_task)(rq); /* vtable entry */      │ |
|  │      void (*task_tick)(rq, task, queued);       /* vtable entry */      │ |
|  │      ...                                                                 │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Dispatch example:                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  task->sched_class->pick_next_task(rq)                           │    │ |
|  │  │         │                │                                       │    │ |
|  │  │         │                └── Calls the function pointer          │    │ |
|  │  │         │                                                        │    │ |
|  │  │         └── Points to the class (e.g., fair_sched_class)         │    │ |
|  │  │                                                                  │    │ |
|  │  │  This is runtime polymorphism in C:                              │    │ |
|  │  │  • task_struct has pointer to sched_class                        │    │ |
|  │  │  • sched_class has function pointers                             │    │ |
|  │  │  • Core code calls through function pointers                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  Equivalent C++:                                                 │    │ |
|  │  │  class SchedClass {                                              │    │ |
|  │  │      virtual task_struct* pick_next_task(rq* rq) = 0;            │    │ |
|  │  │  };                                                              │    │ |
|  │  │  class FairSchedClass : public SchedClass { ... };               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 3: PER-CPU DATA STRUCTURES (per-CPU 数据结构)                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Problem: Global locks don't scale on SMP                                │ |
|  │                                                                          │ |
|  │  Solution: Give each CPU its own copy of critical data                   │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Bad: Global run queue                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │               Global Run Queue                            │   │    │ |
|  │  │  │                    [lock]                                 │   │    │ |
|  │  │  │  ┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐        │   │    │ |
|  │  │  │  │ T1 ││ T2 ││ T3 ││ T4 ││ T5 ││ T6 ││ T7 ││ T8 │        │   │    │ |
|  │  │  │  └────┘└────┘└────┘└────┘└────┘└────┘└────┘└────┘        │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │        ▲         ▲         ▲         ▲                          │    │ |
|  │  │        │         │         │         │  ALL CPUs contend        │    │ |
|  │  │      CPU0      CPU1      CPU2      CPU3                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Good: Per-CPU run queues                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │    CPU 0           CPU 1           CPU 2           CPU 3         │    │ |
|  │  │  ┌────────┐      ┌────────┐      ┌────────┐      ┌────────┐     │    │ |
|  │  │  │ rq[0]  │      │ rq[1]  │      │ rq[2]  │      │ rq[3]  │     │    │ |
|  │  │  │ [lock] │      │ [lock] │      │ [lock] │      │ [lock] │     │    │ |
|  │  │  │ T1, T5 │      │ T2, T6 │      │ T3, T7 │      │ T4, T8 │     │    │ |
|  │  │  └────────┘      └────────┘      └────────┘      └────────┘     │    │ |
|  │  │       │               │               │               │         │    │ |
|  │  │       └───────────────┴───────────────┴───────────────┘         │    │ |
|  │  │                           │                                      │    │ |
|  │  │                  Load balancing (periodic)                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Definition in code:                                                     │ |
|  │  DEFINE_PER_CPU(struct rq, runqueues);                                  │ |
|  │                                                                          │ |
|  │  Access:                                                                 │ |
|  │  struct rq *rq = cpu_rq(cpu);      /* get specific CPU's rq */         │ |
|  │  struct rq *rq = this_rq();        /* get current CPU's rq */          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PATTERN 4: RED-BLACK TREE FOR O(log n) OPERATIONS                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  CFS uses red-black tree keyed by vruntime:                              │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │               ┌──────────────────────────────────────────┐       │    │ |
|  │  │               │              Root                        │       │    │ |
|  │  │               │        vruntime = 1000                   │       │    │ |
|  │  │               └───────────────┬──────────────────────────┘       │    │ |
|  │  │                    ┌──────────┴──────────┐                       │    │ |
|  │  │                    ▼                     ▼                       │    │ |
|  │  │          ┌──────────────┐       ┌──────────────┐                 │    │ |
|  │  │          │ vruntime=500 │       │ vruntime=1500│                 │    │ |
|  │  │          └───────┬──────┘       └──────────────┘                 │    │ |
|  │  │           ┌──────┴──────┐                                        │    │ |
|  │  │           ▼             ▼                                        │    │ |
|  │  │   ┌────────────┐ ┌────────────┐                                  │    │ |
|  │  │   │vruntime=200│ │vruntime=700│                                  │    │ |
|  │  │   └────────────┘ └────────────┘                                  │    │ |
|  │  │         ▲                                                        │    │ |
|  │  │         │                                                        │    │ |
|  │  │    rb_leftmost (cached) ◄── O(1) access to next task            │    │ |
|  │  │                                                                  │    │ |
|  │  │  Operations:                                                     │    │ |
|  │  │  • Insert:  O(log n)                                             │    │ |
|  │  │  • Delete:  O(log n)                                             │    │ |
|  │  │  • Minimum: O(1) (cached leftmost)                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式 1：基于类的调度**
- 问题：不同工作负载需要不同的调度算法
- 解决方案：具有统一接口的调度器类
- 每个类实现：`enqueue_task()`、`dequeue_task()`、`pick_next_task()`、`check_preempt()`、`task_tick()`
- 好处：新调度算法无需触及核心代码

**模式 2：操作表多态**
- `sched_class` 结构包含函数指针（vtable 条目）
- 调度分发：`task->sched_class->pick_next_task(rq)`
- 这是 C 语言中的运行时多态，等价于 C++ 虚函数

**模式 3：per-CPU 数据结构**
- 问题：全局锁在 SMP 上不可扩展
- 解决方案：每个 CPU 有自己的运行队列副本
- 定义：`DEFINE_PER_CPU(struct rq, runqueues)`
- 访问：`cpu_rq(cpu)` 或 `this_rq()`

**模式 4：红黑树实现 O(log n) 操作**
- CFS 使用按 vruntime 排序的红黑树
- 插入/删除：O(log n)
- 最小值（下一个任务）：O(1)（缓存的最左节点）

---

## 2. 核心数据结构

```
CORE DATA STRUCTURES
+=============================================================================+
|                                                                              |
|  STRUCT TASK_STRUCT (include/linux/sched.h)                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  The task_struct is the CENTRAL data structure (~1.7KB in 3.2):          │ |
|  │                                                                          │ |
|  │  struct task_struct {                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* STATE SECTION */                                             │    │ |
|  │  │  volatile long state;        /* TASK_RUNNING, TASK_INTERRUPTIBLE */│   │ |
|  │  │  void *stack;                /* kernel stack pointer */          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* SCHEDULING SECTION */                                        │    │ |
|  │  │  int prio, static_prio, normal_prio;  /* priority values */      │    │ |
|  │  │  unsigned int rt_priority;            /* RT priority (0-99) */   │    │ |
|  │  │  const struct sched_class *sched_class;  /* ops table */         │    │ |
|  │  │  struct sched_entity se;              /* CFS scheduling entity */│    │ |
|  │  │  struct sched_rt_entity rt;           /* RT scheduling entity */ │    │ |
|  │  │  unsigned int policy;                 /* SCHED_NORMAL, etc. */   │    │ |
|  │  │  cpumask_t cpus_allowed;              /* CPU affinity mask */    │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* IDENTITY SECTION */                                          │    │ |
|  │  │  pid_t pid;                   /* process ID */                   │    │ |
|  │  │  pid_t tgid;                  /* thread group ID (= main pid) */ │    │ |
|  │  │  struct task_struct *real_parent;  /* biological parent */       │    │ |
|  │  │  struct task_struct *parent;       /* recipient of SIGCHLD */    │    │ |
|  │  │  struct list_head children;        /* list of children */        │    │ |
|  │  │  struct list_head sibling;         /* linkage in parent's list */│    │ |
|  │  │                                                                  │    │ |
|  │  │  /* RESOURCES SECTION */                                         │    │ |
|  │  │  struct mm_struct *mm;        /* address space */                │    │ |
|  │  │  struct mm_struct *active_mm; /* mm used for lazy TLB */         │    │ |
|  │  │  struct files_struct *files;  /* file descriptor table */        │    │ |
|  │  │  struct fs_struct *fs;        /* filesystem info (cwd, root) */  │    │ |
|  │  │  struct nsproxy *nsproxy;     /* namespaces */                   │    │ |
|  │  │  struct signal_struct *signal;/* signal handlers (shared) */     │    │ |
|  │  │  struct sighand_struct *sighand; /* signal handlers */           │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* CREDENTIALS SECTION */                                       │    │ |
|  │  │  const struct cred *cred;     /* credentials */                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* TIMING SECTION */                                            │    │ |
|  │  │  cputime_t utime, stime;      /* user/system time */             │    │ |
|  │  │  struct timespec start_time;  /* boot-based start time */        │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* MANY MORE FIELDS... (~250 total) */                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  KEY INSIGHT: task_struct is the IDENTITY of execution unit             │ |
|  │               All subsystems reference it                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT SCHED_ENTITY (调度实体)                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct sched_entity {                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct load_weight load;      /* weight for proportional share */│   │ |
|  │  │  struct rb_node run_node;      /* position in RB tree */         │    │ |
|  │  │  struct list_head group_node;  /* cgroup hierarchy */            │    │ |
|  │  │  unsigned int on_rq;           /* currently on run queue? */     │    │ |
|  │  │                                                                  │    │ |
|  │  │  u64 exec_start;               /* when execution started */      │    │ |
|  │  │  u64 sum_exec_runtime;         /* total runtime (ns) */          │    │ |
|  │  │  u64 vruntime;                 /* virtual runtime (key!) */      │    │ |
|  │  │  u64 prev_sum_exec_runtime;    /* for wait time calc */          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* for group scheduling */                                      │    │ |
|  │  │  struct sched_entity *parent;                                    │    │ |
|  │  │  struct cfs_rq *cfs_rq;        /* rq on which entity is */       │    │ |
|  │  │  struct cfs_rq *my_q;          /* rq "owned" by entity (group) */│    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  VRUNTIME CALCULATION:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  vruntime += delta_exec * (NICE_0_LOAD / load.weight)            │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example:                                                        │    │ |
|  │  │  • nice 0 (weight 1024): run 10ms → vruntime += 10ms            │    │ |
|  │  │  • nice 5 (weight 512):  run 10ms → vruntime += 20ms            │    │ |
|  │  │  • nice -5 (weight 2048): run 10ms → vruntime += 5ms            │    │ |
|  │  │                                                                  │    │ |
|  │  │  Lower nice (higher priority) = slower vruntime growth           │    │ |
|  │  │  = runs more often                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT RQ (运行队列)                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct rq {                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* LOCKING */                                                   │    │ |
|  │  │  raw_spinlock_t lock;          /* protects this rq */            │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* TASK TRACKING */                                             │    │ |
|  │  │  unsigned int nr_running;      /* runnable tasks on this CPU */  │    │ |
|  │  │  struct task_struct *curr;     /* currently running task */      │    │ |
|  │  │  struct task_struct *idle;     /* idle task for this CPU */      │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* SCHEDULER CLASS QUEUES */                                    │    │ |
|  │  │  struct cfs_rq cfs;            /* CFS run queue (embedded) */    │    │ |
|  │  │  struct rt_rq  rt;             /* RT run queue (embedded) */     │    │ |
|  │  │  struct dl_rq  dl;             /* Deadline run queue */          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* TIMING */                                                    │    │ |
|  │  │  u64 clock;                    /* rq-local clock (ns) */         │    │ |
|  │  │  u64 clock_task;               /* task-visible clock */          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* LOAD TRACKING */                                             │    │ |
|  │  │  struct load_weight load;      /* total load on this CPU */      │    │ |
|  │  │  unsigned long cpu_load[CPU_LOAD_IDX_MAX]; /* historical load */ │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* MIGRATION */                                                 │    │ |
|  │  │  int cpu;                      /* CPU this rq belongs to */      │    │ |
|  │  │  struct sched_domain *sd;      /* scheduling domain */           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  PER-CPU DEFINITION:                                                     │ |
|  │  DEFINE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT CFS_RQ (CFS 运行队列)                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct cfs_rq {                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct load_weight load;      /* total weight of tasks */       │    │ |
|  │  │  unsigned int nr_running;      /* number of runnable entities */ │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* THE RED-BLACK TREE */                                        │    │ |
|  │  │  struct rb_root tasks_timeline; /* RB tree root */               │    │ |
|  │  │  struct rb_node *rb_leftmost;   /* cached minimum (O(1)) */      │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* VIRTUAL TIME */                                              │    │ |
|  │  │  u64 min_vruntime;             /* monotonic virtual clock */     │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* CURRENT ENTITY */                                            │    │ |
|  │  │  struct sched_entity *curr;    /* currently running entity */    │    │ |
|  │  │  struct sched_entity *next;    /* hint for pick_next */          │    │ |
|  │  │  struct sched_entity *skip;    /* hint to skip entity */         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct task_struct**（~1.7KB）：
- **状态部分**：`state`（运行状态）、`stack`（内核栈指针）
- **调度部分**：`prio`、`sched_class`（操作表）、`sched_entity`（CFS 调度实体）、`policy`（调度策略）
- **身份部分**：`pid`、`tgid`（线程组 ID）、`parent`、`children`
- **资源部分**：`mm`（地址空间）、`files`（文件描述符表）、`fs`（文件系统信息）、`nsproxy`（命名空间）
- **凭证部分**：`cred`（凭证）
- **关键洞察**：`task_struct` 是执行单元的身份，所有子系统都引用它

**struct sched_entity**：
- `load`：权重
- `run_node`：红黑树节点
- `vruntime`：虚拟运行时间（关键！）
- vruntime 计算：`vruntime += delta_exec * (NICE_0_LOAD / weight)`

**struct rq**：
- per-CPU 运行队列
- 包含：锁、运行任务数、当前任务、空闲任务
- 嵌入子队列：`cfs_rq`、`rt_rq`、`dl_rq`

**struct cfs_rq**：
- 红黑树（`tasks_timeline`）按 vruntime 排序
- `rb_leftmost`：缓存的最小值（O(1) 访问）
- `min_vruntime`：单调虚拟时钟

---

## 3. 控制流

```
CONTROL FLOW: SCHEDULE() AND PICK_NEXT_TASK()
+=============================================================================+
|                                                                              |
|  SCHEDULE() - THE HEART OF SCHEDULING (kernel/sched/core.c)                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  schedule():                                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. preempt_disable()           ◄── Prevent nested preemption   │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  2. __schedule()                 ◄── Main scheduling logic       │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── rq = this_rq()       ◄── Get current CPU's run queue │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── prev = rq->curr      ◄── Get current task            │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── rq_lock(rq)          ◄── Lock run queue              │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── if (prev->state != TASK_RUNNING)                     │    │ |
|  │  │         │       deactivate_task(rq, prev)  ◄── Remove from rq    │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── put_prev_task(rq, prev)        ◄── Class callback    │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── next = pick_next_task(rq)      ◄── SELECT NEXT!      │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── if (prev != next)                                    │    │ |
|  │  │         │       context_switch(rq, prev, next) ◄── SWITCH!       │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ├── rq_unlock(rq)                                        │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  3. sched_preempt_enable_no_resched()                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PICK_NEXT_TASK() - SELECTING THE NEXT TASK                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  pick_next_task(rq):                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Fast path: only CFS tasks? */                                │    │ |
|  │  │  if (likely(rq->nr_running == rq->cfs.nr_running)) {             │    │ |
|  │  │      p = fair_sched_class.pick_next_task(rq);                    │    │ |
|  │  │      if (likely(p))                                              │    │ |
|  │  │          return p;                                               │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Slow path: iterate through classes */                        │    │ |
|  │  │  for_each_class(class) {                                         │    │ |
|  │  │      p = class->pick_next_task(rq);                              │    │ |
|  │  │      if (p)                                                      │    │ |
|  │  │          return p;                                               │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Should never reach here */                                   │    │ |
|  │  │  BUG();                                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  CLASS ITERATION ORDER:                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. stop_sched_class.pick_next_task(rq)    → stopper or NULL    │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  2. dl_sched_class.pick_next_task(rq)      → deadline or NULL   │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  3. rt_sched_class.pick_next_task(rq)      → realtime or NULL   │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  4. fair_sched_class.pick_next_task(rq)    → CFS task or NULL   │    │ |
|  │  │              │                                                   │    │ |
|  │  │              ▼                                                   │    │ |
|  │  │  5. idle_sched_class.pick_next_task(rq)    → idle task (always) │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CFS PICK_NEXT_ENTITY() - SELECTING FROM RED-BLACK TREE                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  __pick_first_entity(cfs_rq):                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  if (!cfs_rq->rb_leftmost)                                       │    │ |
|  │  │      return NULL;                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  return rb_entry(cfs_rq->rb_leftmost,                            │    │ |
|  │  │                  struct sched_entity,                            │    │ |
|  │  │                  run_node);                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │                    Red-Black Tree                         │   │    │ |
|  │  │  │                         50                                │   │    │ |
|  │  │  │                      ┌──┴──┐                              │   │    │ |
|  │  │  │                     25    75                              │   │    │ |
|  │  │  │                   ┌─┴─┐                                   │   │    │ |
|  │  │  │                  10  35                                   │   │    │ |
|  │  │  │                  ▲                                        │   │    │ |
|  │  │  │                  │                                        │   │    │ |
|  │  │  │           rb_leftmost (cached)                            │   │    │ |
|  │  │  │           vruntime = 10                                   │   │    │ |
|  │  │  │           THIS TASK RUNS NEXT (O(1))                      │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**schedule() - 调度的核心**：
1. `preempt_disable()`：防止嵌套抢占
2. `__schedule()`：主调度逻辑
   - 获取当前 CPU 的运行队列
   - 获取当前任务
   - 锁定运行队列
   - 如果当前任务不再运行，从队列中移除
   - 调用 `put_prev_task()`（类回调）
   - 调用 `pick_next_task()` 选择下一个任务
   - 如果不同，调用 `context_switch()` 切换
3. 重新启用抢占

**pick_next_task() - 选择下一个任务**：
- **快速路径**：如果只有 CFS 任务，直接调用 CFS 的 `pick_next_task()`
- **慢路径**：按优先级遍历所有类
  1. `stop_sched_class` → 2. `dl_sched_class` → 3. `rt_sched_class` → 4. `fair_sched_class` → 5. `idle_sched_class`

**CFS pick_next_entity()**：
- 返回 `rb_leftmost`（缓存的最左节点）
- O(1) 访问最小 vruntime 的任务

---

## 4. 扩展点

```
EXTENSION POINTS: SCHED_CLASS
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  THE SCHED_CLASS INTERFACE                                               │ |
|  │                                                                          │ |
|  │  struct sched_class {                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* PRIORITY CHAIN */                                            │    │ |
|  │  │  const struct sched_class *next;                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* QUEUE MANAGEMENT */                                          │    │ |
|  │  │  void (*enqueue_task)(struct rq *rq, struct task_struct *p,     │    │ |
|  │  │                       int flags);                                │    │ |
|  │  │  void (*dequeue_task)(struct rq *rq, struct task_struct *p,     │    │ |
|  │  │                       int flags);                                │    │ |
|  │  │  void (*yield_task)(struct rq *rq);                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* PREEMPTION */                                                │    │ |
|  │  │  void (*check_preempt_curr)(struct rq *rq, struct task_struct *p,│   │ |
|  │  │                             int flags);                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* SELECTION */                                                 │    │ |
|  │  │  struct task_struct *(*pick_next_task)(struct rq *rq);          │    │ |
|  │  │  void (*put_prev_task)(struct rq *rq, struct task_struct *p);   │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* MIGRATION */                                                 │    │ |
|  │  │  void (*set_curr_task)(struct rq *rq);                          │    │ |
|  │  │  void (*task_tick)(struct rq *rq, struct task_struct *p,        │    │ |
|  │  │                    int queued);                                  │    │ |
|  │  │  void (*task_fork)(struct task_struct *p);                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* PRIORITY */                                                  │    │ |
|  │  │  void (*prio_changed)(struct rq *rq, struct task_struct *p,     │    │ |
|  │  │                       int oldprio);                              │    │ |
|  │  │  void (*switched_from)(struct rq *rq, struct task_struct *p);   │    │ |
|  │  │  void (*switched_to)(struct rq *rq, struct task_struct *p);     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  EXISTING IMPLEMENTATIONS:                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  stop_sched_class (kernel/sched/stop_task.c)                     │    │ |
|  │  │  ├── Purpose: CPU stopper for migration                          │    │ |
|  │  │  └── Priority: Highest                                           │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  dl_sched_class (kernel/sched/deadline.c) [newer kernels]        │    │ |
|  │  │  ├── Purpose: SCHED_DEADLINE (EDF)                               │    │ |
|  │  │  └── Priority: Above RT                                          │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  rt_sched_class (kernel/sched/rt.c)                              │    │ |
|  │  │  ├── Purpose: SCHED_FIFO, SCHED_RR                               │    │ |
|  │  │  ├── Algorithm: Fixed priority with optional round-robin         │    │ |
|  │  │  └── Priority: Above CFS                                         │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  fair_sched_class (kernel/sched/fair.c)                          │    │ |
|  │  │  ├── Purpose: SCHED_NORMAL, SCHED_BATCH                          │    │ |
|  │  │  ├── Algorithm: CFS (vruntime-based fairness)                    │    │ |
|  │  │  └── Priority: Default                                           │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  idle_sched_class (kernel/sched/idle_task.c)                     │    │ |
|  │  │  ├── Purpose: SCHED_IDLE, idle task                              │    │ |
|  │  │  └── Priority: Lowest                                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  HOW TO ADD A NEW SCHEDULER CLASS (conceptually):                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Define new_sched_class structure                             │    │ |
|  │  │  2. Implement all required callbacks                             │    │ |
|  │  │  3. Insert into priority chain (set 'next' pointers)             │    │ |
|  │  │  4. Define new SCHED_* policy constant                           │    │ |
|  │  │  5. Update sched_setscheduler() to accept new policy            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**sched_class 接口**：
- **队列管理**：`enqueue_task()`、`dequeue_task()`、`yield_task()`
- **抢占**：`check_preempt_curr()`
- **选择**：`pick_next_task()`、`put_prev_task()`
- **迁移/tick**：`set_curr_task()`、`task_tick()`、`task_fork()`
- **优先级**：`prio_changed()`、`switched_from()`、`switched_to()`

**现有实现**：
1. `stop_sched_class`：CPU 停止器（迁移），最高优先级
2. `dl_sched_class`：SCHED_DEADLINE（EDF），高于 RT
3. `rt_sched_class`：SCHED_FIFO/RR，固定优先级
4. `fair_sched_class`：SCHED_NORMAL/BATCH（CFS），默认
5. `idle_sched_class`：SCHED_IDLE，最低优先级

**添加新调度类**：定义结构 → 实现回调 → 插入链 → 定义策略常量 → 更新 `sched_setscheduler()`

---

## 5. 代价与限制

```
COSTS AND LIMITS OF THE SCHEDULER
+=============================================================================+
|                                                                              |
|  SCHEDULING OVERHEAD (调度开销)                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  CONTEXT SWITCH COST:                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  Direct costs (measured):                                  │  │    │ |
|  │  │  │  • Save/restore registers: ~100-500 ns                     │  │    │ |
|  │  │  │  • Switch page tables (TLB flush): ~200-1000 ns            │  │    │ |
|  │  │  │  • Pipeline flush: ~50-200 ns                              │  │    │ |
|  │  │  │  • Total direct: ~500-2000 ns                              │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  Indirect costs (often larger):                            │  │    │ |
|  │  │  │  • TLB misses after switch: 10-100 µs warmup               │  │    │ |
|  │  │  │  • L1/L2 cache misses: 10-100 µs warmup                    │  │    │ |
|  │  │  │  • L3 cache pollution: variable                            │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  └───────────────────────────────────────────────────────────┘  │    │ |
|  │  │                                                                  │    │ |
|  │  │  PICK_NEXT_TASK COST:                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  CFS (common case):                                        │  │    │ |
|  │  │  │  • rb_leftmost access: O(1)                                │  │    │ |
|  │  │  │  • Actually ~tens of nanoseconds                           │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  RT class:                                                 │  │    │ |
|  │  │  │  • Bitmap scan: O(1) with constant = 100 priority levels   │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │  Enqueue/Dequeue:                                          │  │    │ |
|  │  │  │  • RB tree insert/delete: O(log n)                         │  │    │ |
|  │  │  │  • With n = 1000 tasks: ~10 comparisons                    │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  └───────────────────────────────────────────────────────────┘  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LATENCY TRADE-OFFS (延迟权衡)                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  THROUGHPUT ◄──────────── tension ──────────► LATENCY           │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Longer time slices (e.g., 100ms):                          │ │    │ |
|  │  │  │  + Fewer context switches                                   │ │    │ |
|  │  │  │  + Better cache utilization                                 │ │    │ |
|  │  │  │  + Higher throughput                                        │ │    │ |
|  │  │  │  - Worse response time                                      │ │    │ |
|  │  │  │  - UI feels sluggish                                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Shorter time slices (e.g., 1ms):                           │ │    │ |
|  │  │  │  + Better response time                                     │ │    │ |
|  │  │  │  + UI feels snappy                                          │ │    │ |
|  │  │  │  - More context switches                                    │ │    │ |
|  │  │  │  - Cache thrashing                                          │ │    │ |
|  │  │  │  - Lower throughput                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  CFS APPROACH: Dynamic time slices based on load                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  sched_latency_ns = 6ms (target scheduling period)          │ │    │ |
|  │  │  │  sched_min_granularity_ns = 0.75ms (minimum time slice)     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  If n tasks:                                                │ │    │ |
|  │  │  │  • time_slice = max(latency/n, min_granularity)             │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Examples:                                                  │ │    │ |
|  │  │  │  • 2 tasks: 6ms/2 = 3ms each                                │ │    │ |
|  │  │  │  • 8 tasks: 6ms/8 = 0.75ms each (minimum)                   │ │    │ |
|  │  │  │  • 16 tasks: 0.75ms each (clamped), period = 12ms           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ARCHITECTURAL LIMITS (架构限制)                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. SINGLE DISPATCHER ABSTRACTION                                        │ |
|  │     • All tasks go through same schedule() path                         │ |
|  │     • Can't easily bypass for ultra-low-latency                          │ |
|  │                                                                          │ |
|  │  2. KERNEL SPACE SCHEDULING                                              │ |
|  │     • Scheduling decisions require kernel entry                          │ |
|  │     • User-space threading (M:N) largely abandoned                       │ |
|  │     • Modern approach: 1:1 threading with futex                          │ |
|  │                                                                          │ |
|  │  3. GLOBAL ORDERING (within a class)                                     │ |
|  │     • CFS imposes global vruntime ordering                               │ |
|  │     • Load balancing tries to maintain fairness across CPUs              │ |
|  │     • Can conflict with NUMA locality                                    │ |
|  │                                                                          │ |
|  │  4. COMPLEXITY OF LOAD BALANCING                                         │ |
|  │     • Scheduling domains (core, socket, NUMA node)                       │ |
|  │     • Periodic rebalancing can cause latency spikes                      │ |
|  │     • Wake-affine heuristics can misfire                                 │ |
|  │                                                                          │ |
|  │  5. RT VS CFS BOUNDARY                                                   │ |
|  │     • RT tasks can completely starve CFS                                 │ |
|  │     • rt_bandwidth tries to limit this (but not on by default)           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**调度开销**：
- **上下文切换成本**：
  - 直接成本：保存/恢复寄存器 ~100-500ns，切换页表 ~200-1000ns，流水线刷新 ~50-200ns
  - 间接成本：TLB 缺失 10-100µs，缓存缺失 10-100µs（通常更大）
- **pick_next_task 成本**：
  - CFS：O(1)（访问 rb_leftmost）
  - 入队/出队：O(log n)

**延迟权衡**：
- 更长时间片（100ms）：更高吞吐量，更差响应时间
- 更短时间片（1ms）：更好响应时间，更低吞吐量
- CFS 方法：基于负载的动态时间片
  - `sched_latency_ns = 6ms`（目标调度周期）
  - `sched_min_granularity_ns = 0.75ms`（最小时间片）
  - `time_slice = max(latency/n, min_granularity)`

**架构限制**：
1. 单一调度器抽象：所有任务经过同一 `schedule()` 路径
2. 内核空间调度：调度决策需要进入内核
3. 全局排序：CFS 施加全局 vruntime 排序，可能与 NUMA 局部性冲突
4. 负载均衡复杂性：调度域、周期性重平衡可能导致延迟尖峰
5. RT vs CFS 边界：RT 任务可能完全饿死 CFS
