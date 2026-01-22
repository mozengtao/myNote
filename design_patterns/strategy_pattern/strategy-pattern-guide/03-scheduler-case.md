# Case 1: CPU Scheduler Classes

## Subsystem Background

```
+=============================================================================+
|                    CPU SCHEDULER ARCHITECTURE                                |
+=============================================================================+

                          SCHEDULER CORE
                          ==============

    +------------------------------------------------------------------+
    |                     kernel/sched.c                                |
    |                                                                   |
    |   MECHANISM (Fixed):                                              |
    |   - Timer interrupts (scheduler_tick)                             |
    |   - Context switch (context_switch)                               |
    |   - CPU selection (select_task_rq)                                |
    |   - Load balancing infrastructure                                 |
    |   - Run queue management                                          |
    |                                                                   |
    +------------------------------------------------------------------+
                                |
                                | delegates POLICY to
                                v
    +------------------------------------------------------------------+
    |                   SCHEDULING CLASSES                              |
    |                   (Strategy Pattern)                              |
    |                                                                   |
    |   +------------------+  +------------------+  +------------------+|
    |   |   stop_class     |  |    rt_class      |  |   fair_class     ||
    |   | (highest prio)   |  | (real-time)      |  | (CFS - normal)   ||
    |   +------------------+  +------------------+  +------------------+|
    |                                                                   |
    |   +------------------+  +------------------+                      |
    |   |   idle_class     |  | (more classes    |                      |
    |   | (lowest prio)    |  |  possible)       |                      |
    |   +------------------+  +------------------+                      |
    |                                                                   |
    +------------------------------------------------------------------+

    KEY INSIGHT:
    - Scheduler CORE knows WHEN to schedule (timer tick, sleep, wakeup)
    - Scheduler CLASS knows HOW to schedule (which task, how long)
```

**中文说明：**

CPU调度器架构：核心（`kernel/sched.c`）负责机制——定时器中断、上下文切换、CPU选择、负载均衡基础设施、运行队列管理。核心将策略委托给调度类（策略模式）：`stop_class`（最高优先级）、`rt_class`（实时）、`fair_class`（CFS，普通任务）、`idle_class`（最低优先级）。关键洞察：调度器核心知道何时调度（定时器滴答、睡眠、唤醒），调度类知道如何调度（哪个任务、多长时间）。

---

## The Strategy Interface: struct sched_class

### Components

| Component | Role |
|-----------|------|
| **Strategy Interface** | `struct sched_class` |
| **Replaceable Algorithm** | CFS, RT, Stop, Idle (complete scheduling policies) |
| **Selection Mechanism** | Per-task: `task->sched_class` based on policy |

### The Interface

```c
struct sched_class {
    const struct sched_class *next;  /* Priority chain */

    /* ===== ENQUEUEING/DEQUEUEING ===== */
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)(struct rq *rq);

    /* ===== PREEMPTION ===== */
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

    /* ===== TASK SELECTION ===== */
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);

    /* ===== LOAD BALANCING ===== */
    int (*select_task_rq)(struct task_struct *p, int sd_flag, int flags);
    void (*migrate_task_rq)(struct task_struct *p, int next_cpu);

    /* ===== TIMER HANDLING ===== */
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)(struct task_struct *p);

    /* ===== PRIORITY ===== */
    void (*prio_changed)(struct rq *rq, struct task_struct *p, int oldprio);
    void (*switched_to)(struct rq *rq, struct task_struct *p);

    /* ... more operations ... */
};
```

### Control Flow: How Core Uses Strategy

```
    schedule() - Main Scheduler Entry
    ==================================

    +----------------------------------+
    |  schedule() called               |
    |  (timer tick, sleep, yield, etc) |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  Disable preemption              |  MECHANISM
    |  Lock runqueue                   |  (Core)
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  prev->sched_class->put_prev_task()    ||  STRATEGY
    ||  (Handle the task being switched out)  ||
    +==========================================+
                   |
                   v
    +==========================================+
    ||  next = pick_next_task(rq)             ||  STRATEGY
    ||  (Select next task to run)             ||  (iterates through classes)
    +==========================================+
                   |
                   v
    +----------------------------------+
    |  if (next != prev)               |  MECHANISM
    |      context_switch(prev, next)  |  (Core)
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  Unlock runqueue                 |  MECHANISM
    |  Enable preemption               |  (Core)
    +----------------------------------+
```

**中文说明：**

`schedule()`的控制流：核心负责禁用抢占、锁定运行队列（机制），然后委托给策略——调用`put_prev_task()`处理被切换出的任务、调用`pick_next_task()`选择下一个运行的任务，最后核心执行上下文切换、解锁（机制）。注意策略调用没有被框架前后步骤包装，策略控制选择逻辑。

---

## Why Strategy is Required Here

### 1. Different Workloads Need Different Policies

```
    WORKLOAD                BEST SCHEDULING POLICY
    ========                ======================

    Desktop Interactive     CFS with low latency
    +-------------------+   - Responsive UI
    | User clicks mouse |   - Fair time sharing
    | App should respond|   - No starvation
    | immediately       |
    +-------------------+

    Real-Time Audio         RT (SCHED_FIFO)
    +-------------------+   - Guaranteed CPU time
    | Audio buffer must |   - Predictable latency
    | be filled every   |   - Priority-based
    | 10ms              |
    +-------------------+

    Batch Server            CFS with throughput tuning
    +-------------------+   - Maximize throughput
    | Process millions  |   - Batch similar tasks
    | of requests       |   - Less context switches
    +-------------------+

    SINGLE ALGORITHM CANNOT SATISFY ALL WORKLOADS
    STRATEGY PATTERN ALLOWS RIGHT ALGORITHM PER TASK
```

### 2. Coexistence is Required

```
    MULTIPLE POLICIES ACTIVE SIMULTANEOUSLY:

    CPU 0 Run Queue
    +--------------------------------------------------+
    |  stop_sched_class    [empty]                     |
    |  rt_sched_class      [RT tasks: audio, control]  |
    |  fair_sched_class    [Normal tasks: browser, vim]|
    |  idle_sched_class    [idle task]                 |
    +--------------------------------------------------+

    pick_next_task() iterates through classes:
    1. Check stop_class: any stop tasks? No -> continue
    2. Check rt_class: any RT tasks? Yes -> return RT task
       OR No -> continue  
    3. Check fair_class: any normal tasks? Yes -> return CFS task
    4. Check idle_class: return idle task

    DIFFERENT ALGORITHMS COEXIST ON SAME CPU
```

**中文说明：**

为什么需要策略：(1) 不同工作负载需要不同策略——桌面交互需要CFS低延迟、实时音频需要RT保证、批处理服务器需要吞吐量优化，单一算法无法满足所有工作负载。(2) 需要共存——多个策略同时活跃在同一CPU上，`pick_next_task()`按优先级遍历类：stop -> RT -> fair -> idle。

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL SCHEDULER STRATEGY PATTERN SIMULATION
 * 
 * Demonstrates how scheduler classes work as strategies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct task_struct;
struct rq;
struct sched_class;

/* ==========================================================
 * TASK STRUCTURE
 * ========================================================== */
struct task_struct {
    int pid;
    const char *name;
    int policy;                    /* SCHED_NORMAL, SCHED_FIFO, etc */
    int prio;                      /* Priority */
    const struct sched_class *sched_class;  /* Strategy */
    
    /* CFS-specific (when using fair_sched_class) */
    unsigned long vruntime;
    
    /* RT-specific (when using rt_sched_class) */
    int rt_priority;
    
    struct task_struct *next;      /* Linked list */
};

#define SCHED_NORMAL  0
#define SCHED_FIFO    1
#define SCHED_RR      2

/* ==========================================================
 * RUN QUEUE (Per-CPU)
 * ========================================================== */
struct rq {
    int cpu;
    struct task_struct *curr;      /* Currently running task */
    
    /* CFS run queue (simplified as linked list) */
    struct task_struct *cfs_tasks;
    int cfs_nr_running;
    
    /* RT run queue (simplified) */
    struct task_struct *rt_tasks;
    int rt_nr_running;
    
    /* Idle task */
    struct task_struct *idle;
};

/* ==========================================================
 * SCHED_CLASS: Strategy Interface
 * ========================================================== */
struct sched_class {
    const char *name;
    const struct sched_class *next;  /* Priority chain */
    
    /* Enqueue task to run queue */
    void (*enqueue_task)(struct rq *rq, struct task_struct *p);
    
    /* Dequeue task from run queue */
    void (*dequeue_task)(struct rq *rq, struct task_struct *p);
    
    /* Pick next task to run (THE KEY DECISION) */
    struct task_struct *(*pick_next_task)(struct rq *rq);
    
    /* Handle task being preempted */
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    
    /* Timer tick */
    void (*task_tick)(struct rq *rq, struct task_struct *p);
};

/* ==========================================================
 * CFS STRATEGY IMPLEMENTATION
 * Complete Fair Scheduler - fair time sharing
 * ========================================================== */

static void cfs_enqueue_task(struct rq *rq, struct task_struct *p)
{
    printf("  [CFS] Enqueue task %d (%s) with vruntime=%lu\n",
           p->pid, p->name, p->vruntime);
    
    /* Add to CFS run queue (simplified: just append) */
    p->next = rq->cfs_tasks;
    rq->cfs_tasks = p;
    rq->cfs_nr_running++;
}

static void cfs_dequeue_task(struct rq *rq, struct task_struct *p)
{
    printf("  [CFS] Dequeue task %d (%s)\n", p->pid, p->name);
    
    /* Remove from list (simplified) */
    struct task_struct **pp = &rq->cfs_tasks;
    while (*pp) {
        if (*pp == p) {
            *pp = p->next;
            break;
        }
        pp = &(*pp)->next;
    }
    rq->cfs_nr_running--;
}

static struct task_struct *cfs_pick_next_task(struct rq *rq)
{
    if (rq->cfs_nr_running == 0)
        return NULL;
    
    /* CFS: pick task with smallest vruntime */
    struct task_struct *best = NULL;
    struct task_struct *p = rq->cfs_tasks;
    
    while (p) {
        if (!best || p->vruntime < best->vruntime)
            best = p;
        p = p->next;
    }
    
    if (best) {
        printf("  [CFS] Pick next: task %d (%s) vruntime=%lu\n",
               best->pid, best->name, best->vruntime);
    }
    return best;
}

static void cfs_put_prev_task(struct rq *rq, struct task_struct *p)
{
    /* Update vruntime based on how long it ran */
    p->vruntime += 10;  /* Simplified: add fixed amount */
    printf("  [CFS] Put prev: task %d (%s) new vruntime=%lu\n",
           p->pid, p->name, p->vruntime);
}

static void cfs_task_tick(struct rq *rq, struct task_struct *p)
{
    p->vruntime += 1;
    printf("  [CFS] Tick: task %d vruntime now %lu\n", p->pid, p->vruntime);
}

static const struct sched_class fair_sched_class = {
    .name = "CFS",
    .next = NULL,  /* Will be set to idle_sched_class */
    .enqueue_task = cfs_enqueue_task,
    .dequeue_task = cfs_dequeue_task,
    .pick_next_task = cfs_pick_next_task,
    .put_prev_task = cfs_put_prev_task,
    .task_tick = cfs_task_tick,
};

/* ==========================================================
 * RT STRATEGY IMPLEMENTATION
 * Real-Time Scheduler - strict priority
 * ========================================================== */

static void rt_enqueue_task(struct rq *rq, struct task_struct *p)
{
    printf("  [RT] Enqueue task %d (%s) with rt_priority=%d\n",
           p->pid, p->name, p->rt_priority);
    
    /* Add to RT run queue */
    p->next = rq->rt_tasks;
    rq->rt_tasks = p;
    rq->rt_nr_running++;
}

static void rt_dequeue_task(struct rq *rq, struct task_struct *p)
{
    printf("  [RT] Dequeue task %d (%s)\n", p->pid, p->name);
    
    struct task_struct **pp = &rq->rt_tasks;
    while (*pp) {
        if (*pp == p) {
            *pp = p->next;
            break;
        }
        pp = &(*pp)->next;
    }
    rq->rt_nr_running--;
}

static struct task_struct *rt_pick_next_task(struct rq *rq)
{
    if (rq->rt_nr_running == 0)
        return NULL;
    
    /* RT: pick highest priority task */
    struct task_struct *best = NULL;
    struct task_struct *p = rq->rt_tasks;
    
    while (p) {
        if (!best || p->rt_priority > best->rt_priority)
            best = p;
        p = p->next;
    }
    
    if (best) {
        printf("  [RT] Pick next: task %d (%s) rt_priority=%d\n",
               best->pid, best->name, best->rt_priority);
    }
    return best;
}

static void rt_put_prev_task(struct rq *rq, struct task_struct *p)
{
    printf("  [RT] Put prev: task %d (%s)\n", p->pid, p->name);
    /* RT doesn't need to update anything on preemption */
}

static void rt_task_tick(struct rq *rq, struct task_struct *p)
{
    printf("  [RT] Tick: task %d (no vruntime for RT)\n", p->pid);
    /* SCHED_FIFO: runs until it yields or higher prio arrives */
}

static const struct sched_class rt_sched_class = {
    .name = "RT",
    .next = &fair_sched_class,  /* RT is higher priority than CFS */
    .enqueue_task = rt_enqueue_task,
    .dequeue_task = rt_dequeue_task,
    .pick_next_task = rt_pick_next_task,
    .put_prev_task = rt_put_prev_task,
    .task_tick = rt_task_tick,
};

/* ==========================================================
 * IDLE STRATEGY IMPLEMENTATION
 * Runs when nothing else to do
 * ========================================================== */

static struct task_struct *idle_pick_next_task(struct rq *rq)
{
    printf("  [IDLE] Pick next: idle task\n");
    return rq->idle;
}

static void idle_put_prev_task(struct rq *rq, struct task_struct *p)
{
    printf("  [IDLE] Put prev: idle task\n");
}

static const struct sched_class idle_sched_class = {
    .name = "IDLE",
    .next = NULL,
    .enqueue_task = NULL,
    .dequeue_task = NULL,
    .pick_next_task = idle_pick_next_task,
    .put_prev_task = idle_put_prev_task,
    .task_tick = NULL,
};

/* Set up the priority chain */
static void init_sched_classes(void)
{
    /* rt -> fair -> idle */
    ((struct sched_class *)&fair_sched_class)->next = &idle_sched_class;
}

/* ==========================================================
 * SCHEDULER CORE (MECHANISM)
 * This is the fixed part that uses strategies
 * ========================================================== */

/* The sched_class priority chain */
#define for_each_class(class) \
    for (class = &rt_sched_class; class; class = class->next)

/* Core: pick_next_task - iterates through strategy chain */
static struct task_struct *pick_next_task(struct rq *rq)
{
    const struct sched_class *class;
    struct task_struct *p;

    printf("[CORE] pick_next_task: iterating through sched classes\n");
    
    /* Iterate through classes by priority */
    for_each_class(class) {
        printf("[CORE] Checking %s class...\n", class->name);
        p = class->pick_next_task(rq);
        if (p)
            return p;
    }
    
    /* Should never reach here (idle always has a task) */
    return NULL;
}

/* Core: schedule - the main scheduler function */
static void schedule(struct rq *rq)
{
    struct task_struct *prev, *next;

    printf("\n[CORE] === schedule() called ===\n");
    
    prev = rq->curr;
    
    /* === STRATEGY CALL: put_prev_task === */
    if (prev && prev->sched_class->put_prev_task) {
        printf("[CORE] Calling prev->sched_class->put_prev_task()\n");
        prev->sched_class->put_prev_task(rq, prev);
    }
    
    /* === STRATEGY CALL: pick_next_task (through chain) === */
    next = pick_next_task(rq);
    
    /* === MECHANISM: context switch === */
    if (next != prev) {
        printf("[CORE] Context switch: %s -> %s\n",
               prev ? prev->name : "none",
               next ? next->name : "none");
        rq->curr = next;
    } else {
        printf("[CORE] No context switch needed\n");
    }
    
    printf("[CORE] === schedule() complete ===\n");
}

/* Core: scheduler_tick - called on timer interrupt */
static void scheduler_tick(struct rq *rq)
{
    struct task_struct *curr = rq->curr;
    
    printf("\n[CORE] === scheduler_tick() ===\n");
    
    if (curr && curr->sched_class->task_tick) {
        /* === STRATEGY CALL: task_tick === */
        curr->sched_class->task_tick(rq, curr);
    }
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("================================================\n");
    printf("SCHEDULER STRATEGY PATTERN DEMONSTRATION\n");
    printf("================================================\n");

    init_sched_classes();

    /* Create run queue */
    struct rq rq = {
        .cpu = 0,
        .curr = NULL,
        .cfs_tasks = NULL,
        .cfs_nr_running = 0,
        .rt_tasks = NULL,
        .rt_nr_running = 0,
    };

    /* Create idle task */
    struct task_struct idle_task = {
        .pid = 0,
        .name = "idle",
        .policy = SCHED_NORMAL,
        .sched_class = &idle_sched_class,
    };
    rq.idle = &idle_task;
    rq.curr = &idle_task;

    /* Create normal tasks (CFS) */
    struct task_struct task1 = {
        .pid = 1,
        .name = "browser",
        .policy = SCHED_NORMAL,
        .sched_class = &fair_sched_class,
        .vruntime = 100,
    };

    struct task_struct task2 = {
        .pid = 2,
        .name = "vim",
        .policy = SCHED_NORMAL,
        .sched_class = &fair_sched_class,
        .vruntime = 50,
    };

    /* Create RT task */
    struct task_struct task3 = {
        .pid = 3,
        .name = "audio",
        .policy = SCHED_FIFO,
        .sched_class = &rt_sched_class,
        .rt_priority = 99,
    };

    /* === Scenario 1: Add CFS tasks and schedule === */
    printf("\n=== SCENARIO 1: CFS Tasks Only ===\n");
    
    printf("\nEnqueueing CFS tasks:\n");
    fair_sched_class.enqueue_task(&rq, &task1);
    fair_sched_class.enqueue_task(&rq, &task2);
    
    schedule(&rq);
    
    /* Simulate some timer ticks */
    scheduler_tick(&rq);
    scheduler_tick(&rq);
    
    /* Schedule again */
    schedule(&rq);

    /* === Scenario 2: Add RT task (preempts CFS) === */
    printf("\n=== SCENARIO 2: RT Task Arrives ===\n");
    
    printf("\nEnqueueing RT task:\n");
    rt_sched_class.enqueue_task(&rq, &task3);
    
    /* RT should preempt CFS */
    schedule(&rq);
    
    /* RT task runs */
    scheduler_tick(&rq);
    
    /* === Scenario 3: RT task leaves === */
    printf("\n=== SCENARIO 3: RT Task Completes ===\n");
    
    printf("\nDequeuing RT task:\n");
    rt_sched_class.dequeue_task(&rq, &task3);
    
    /* Back to CFS */
    schedule(&rq);

    printf("\n================================================\n");
    printf("KEY OBSERVATIONS:\n");
    printf("1. Core (schedule) doesn't know CFS or RT internals\n");
    printf("2. Each class implements complete scheduling algorithm\n");
    printf("3. Classes coexist - RT has priority over CFS\n");
    printf("4. Adding new class requires no core changes\n");
    printf("================================================\n");

    return 0;
}
```

---

## What the Kernel Core Does NOT Control

```
+=============================================================================+
|              WHAT CORE DOES NOT CONTROL (Strategy Owns)                      |
+=============================================================================+

    THE CORE DOES NOT DECIDE:

    1. WHICH TASK TO RUN NEXT
       +-------------------------------------------------------+
       | Core calls pick_next_task()                           |
       | Strategy decides based on its algorithm:              |
       |   CFS: smallest vruntime                              |
       |   RT: highest priority                                |
       |   Core doesn't know these details                     |
       +-------------------------------------------------------+

    2. HOW LONG A TASK RUNS
       +-------------------------------------------------------+
       | Strategy's task_tick() decides when to request        |
       | rescheduling:                                         |
       |   CFS: when vruntime exceeds ideal runtime            |
       |   RT FIFO: never (runs until blocked/preempted)       |
       +-------------------------------------------------------+

    3. WHAT DATA STRUCTURES TO USE
       +-------------------------------------------------------+
       | CFS: red-black tree indexed by vruntime               |
       | RT: priority arrays with bitmaps                      |
       | Core only sees the sched_class interface              |
       +-------------------------------------------------------+

    4. PREEMPTION POLICY
       +-------------------------------------------------------+
       | Strategy's check_preempt_curr() decides               |
       |   RT: always preempt lower priority                   |
       |   CFS: preempt if vruntime delta too large            |
       +-------------------------------------------------------+

    5. LOAD BALANCING DECISIONS
       +-------------------------------------------------------+
       | Strategy's select_task_rq() picks target CPU          |
       | Each class has its own load balancing logic           |
       +-------------------------------------------------------+

    THE CORE ONLY PROVIDES:
    - When to call schedule() (timer, sleep, wakeup)
    - Context switch mechanics
    - Run queue infrastructure
    - Priority chain iteration
```

**中文说明：**

核心不控制的内容（策略拥有）：(1) 下一个运行哪个任务——核心调用`pick_next_task()`，策略根据算法决定；(2) 任务运行多长时间——策略的`task_tick()`决定何时请求重调度；(3) 使用什么数据结构——CFS用红黑树，RT用优先级数组，核心只看到`sched_class`接口；(4) 抢占策略——策略的`check_preempt_curr()`决定；(5) 负载均衡决策——策略的`select_task_rq()`选择目标CPU。核心只提供：何时调用`schedule()`、上下文切换机制、运行队列基础设施、优先级链迭代。

---

## Real Kernel Code Reference (v3.2)

### struct sched_class in kernel/sched.c

```c
struct sched_class {
    const struct sched_class *next;

    void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task) (struct rq *rq);
    bool (*yield_to_task) (struct rq *rq, struct task_struct *p, bool preempt);

    void (*check_preempt_curr) (struct rq *rq, struct task_struct *p, int flags);

    struct task_struct * (*pick_next_task) (struct rq *rq);
    void (*put_prev_task) (struct rq *rq, struct task_struct *p);
    /* ... more operations ... */
};
```

### The class chain in kernel/sched.c

```c
#define sched_class_highest (&stop_sched_class)
#define for_each_class(class) \
    for (class = sched_class_highest; class; class = class->next)

/* Priority: stop -> rt -> fair -> idle */
```

---

## Key Takeaways

1. **Complete algorithm encapsulation**: Each `sched_class` is a complete scheduling policy
2. **Multiple ops work together**: `enqueue`, `pick_next`, `put_prev`, `task_tick` form one algorithm
3. **Core provides mechanism only**: Timer handling, context switch, run queue iteration
4. **Strategies coexist**: RT and CFS active simultaneously, chained by priority
5. **No framework wrapping**: Core doesn't wrap strategy calls with mandatory pre/post steps
