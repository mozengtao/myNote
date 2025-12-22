# Linux Kernel Scheduler Core Architecture (v3.2)

## Overview

This document explains the **core scheduling mechanisms** in Linux kernel v3.2, focusing on architecture rather than policies.

---

## What the Scheduler Core Is Responsible For

```
+------------------------------------------------------------------+
|  SCHEDULER CORE RESPONSIBILITIES                                 |
+------------------------------------------------------------------+

    +----------------------------------------------------------+
    | MECHANISM (Scheduler Core)   | POLICY (Scheduler Class)  |
    |------------------------------|---------------------------|
    | Task state management        | Which task to pick next   |
    | Runqueue management          | Priority calculations     |
    | Context switching            | Fairness rules            |
    | Preemption boundaries        | Time slice decisions      |
    | Timer tick handling          | Load balancing strategy   |
    | CPU assignment               | Wake-up preemption rules  |
    +----------------------------------------------------------+
    
    CORE INVARIANT:
    +----------------------------------------------------------+
    | At any moment, exactly ONE task runs on each CPU          |
    | (or the idle task if no runnable tasks exist)             |
    +----------------------------------------------------------+
```

**中文解释：**
- 调度器核心负责：任务状态管理、运行队列管理、上下文切换、抢占边界、定时器处理
- 调度策略负责：选择下一个任务、优先级计算、公平性规则
- 核心不变量：每个 CPU 在任何时刻恰好运行一个任务

---

## Task States and Transitions

From `include/linux/sched.h`:

```c
#define TASK_RUNNING        0
#define TASK_INTERRUPTIBLE  1
#define TASK_UNINTERRUPTIBLE 2
#define __TASK_STOPPED      4
#define __TASK_TRACED       8
#define TASK_DEAD           64
#define TASK_WAKEKILL       128
#define TASK_WAKING         256
```

```
+------------------------------------------------------------------+
|  TASK STATE MACHINE                                              |
+------------------------------------------------------------------+

                    fork()
                      │
                      ▼
               ┌─────────────┐
               │ TASK_RUNNING │◄──────────────────────────────┐
               │  (runnable)  │                               │
               └──────┬───────┘                               │
                      │                                       │
         ┌────────────┼────────────┐                         │
         │            │            │                         │
    schedule()   sleep APIs   stop/trace                     │
         │            │            │                         │
         ▼            ▼            ▼                         │
    ┌─────────┐  ┌─────────────┐  ┌──────────┐              │
    │ RUNNING │  │INTERRUPTIBLE│  │ STOPPED  │              │
    │(on CPU) │  │   (sleep)   │  │ (debug)  │              │
    └────┬────┘  └──────┬──────┘  └────┬─────┘              │
         │              │              │                     │
         │         wake_up()      SIGCONT                   │
         │              │              │                     │
         │              └──────────────┴─────────────────────┘
         │
         ▼
    exit() ─────► TASK_DEAD ─────► (freed)

    KEY TRANSITIONS:
    +----------------------------------------------------------+
    | RUNNING → INTERRUPTIBLE:  Explicit sleep (wait_event)     |
    | INTERRUPTIBLE → RUNNING:  wake_up() or signal received    |
    | RUNNING → UNINTERRUPTIBLE: I/O wait (not killable)        |
    | Any → DEAD:               exit() or killed                |
    +----------------------------------------------------------+
```

**中文解释：**
- TASK_RUNNING：可运行（在队列中或正在CPU上执行）
- TASK_INTERRUPTIBLE：可中断睡眠（可被信号唤醒）
- TASK_UNINTERRUPTIBLE：不可中断睡眠（I/O等待）
- 关键转换：显式睡眠、wake_up 唤醒、信号唤醒、退出

---

## Runqueue Architecture

```
+------------------------------------------------------------------+
|  PER-CPU RUNQUEUE DESIGN                                         |
+------------------------------------------------------------------+

    CPU 0             CPU 1             CPU 2             CPU 3
    ┌─────────┐       ┌─────────┐       ┌─────────┐       ┌─────────┐
    │  rq[0]  │       │  rq[1]  │       │  rq[2]  │       │  rq[3]  │
    ├─────────┤       ├─────────┤       ├─────────┤       ├─────────┤
    │ lock    │       │ lock    │       │ lock    │       │ lock    │
    │ nr_run  │       │ nr_run  │       │ nr_run  │       │ nr_run  │
    │ curr    │       │ curr    │       │ curr    │       │ curr    │
    │ idle    │       │ idle    │       │ idle    │       │ idle    │
    │ cfs_rq  │       │ cfs_rq  │       │ cfs_rq  │       │ cfs_rq  │
    │ rt_rq   │       │ rt_rq   │       │ rt_rq   │       │ rt_rq   │
    └─────────┘       └─────────┘       └─────────┘       └─────────┘
    
    WHY PER-CPU:
    +----------------------------------------------------------+
    | - Eliminates global lock contention                       |
    | - Cache locality (task data stays on same CPU)            |
    | - Scalability: O(1) operations don't depend on CPU count  |
    +----------------------------------------------------------+

    struct rq (simplified):
    +----------------------------------------------------------+
    | struct rq {                                               |
    |     raw_spinlock_t lock;     /* Protects this rq */       |
    |     unsigned int nr_running; /* Runnable tasks */         |
    |     struct task_struct *curr;/* Currently running */      |
    |     struct task_struct *idle;/* Idle task */              |
    |     struct cfs_rq cfs;       /* CFS sub-runqueue */       |
    |     struct rt_rq rt;         /* RT sub-runqueue */        |
    |     struct task_struct *stop;/* Stop task */              |
    | };                                                        |
    +----------------------------------------------------------+
```

**中文解释：**
- 每个 CPU 有独立的运行队列（rq）
- 优势：消除全局锁竞争、缓存局部性、可扩展性
- 运行队列包含：锁、可运行任务数、当前任务、空闲任务、CFS/RT 子队列

---

## Enqueue and Dequeue Operations

```
+------------------------------------------------------------------+
|  ENQUEUE/DEQUEUE FLOW                                            |
+------------------------------------------------------------------+

    ENQUEUE (task becomes runnable):
    
    wake_up_new_task() / try_to_wake_up()
                │
                ▼
    ┌───────────────────────┐
    │ activate_task(rq, p)  │
    └───────────┬───────────┘
                │
                ▼
    ┌───────────────────────┐
    │ enqueue_task(rq, p)   │
    └───────────┬───────────┘
                │
                ▼
    ┌───────────────────────┐
    │ p->sched_class->      │  ← Policy hook
    │   enqueue_task(rq, p) │
    └───────────┬───────────┘
                │
                ▼
    ┌───────────────────────┐
    │ inc_nr_running(rq)    │
    └───────────────────────┘

    DEQUEUE (task leaves runqueue):
    
    schedule() / task blocks
                │
                ▼
    ┌───────────────────────┐
    │ deactivate_task(rq,p) │
    └───────────┬───────────┘
                │
                ▼
    ┌───────────────────────┐
    │ dequeue_task(rq, p)   │
    └───────────┬───────────┘
                │
                ▼
    ┌───────────────────────┐
    │ p->sched_class->      │  ← Policy hook
    │   dequeue_task(rq, p) │
    └───────────┬───────────┘
                │
                ▼
    ┌───────────────────────┐
    │ dec_nr_running(rq)    │
    └───────────────────────┘
```

**中文解释：**
- 入队：任务变为可运行时，通过 activate_task → enqueue_task → 策略类入队
- 出队：任务离开队列时，通过 deactivate_task → dequeue_task → 策略类出队
- 策略钩子：每个调度类实现自己的入队/出队逻辑

---

## How Policies Plug Into Mechanisms

```
+------------------------------------------------------------------+
|  SCHEDULER CLASS HIERARCHY                                       |
+------------------------------------------------------------------+

    Priority (highest to lowest):
    
    ┌─────────────────┐
    │   stop_sched    │  ← Highest: migration, stop-machine
    └────────┬────────┘
             │ fallthrough if no task
             ▼
    ┌─────────────────┐
    │   rt_sched      │  ← Real-time: SCHED_FIFO, SCHED_RR
    └────────┬────────┘
             │ fallthrough if no task
             ▼
    ┌─────────────────┐
    │   fair_sched    │  ← CFS: SCHED_NORMAL, SCHED_BATCH
    └────────┬────────┘
             │ fallthrough if no task
             ▼
    ┌─────────────────┐
    │   idle_sched    │  ← SCHED_IDLE tasks
    └─────────────────┘

    struct sched_class (mechanism contract):
    +----------------------------------------------------------+
    | struct sched_class {                                      |
    |     const struct sched_class *next;  /* Fallback */       |
    |                                                           |
    |     /* Policy must implement: */                          |
    |     void (*enqueue_task)(rq, task, flags);                |
    |     void (*dequeue_task)(rq, task, flags);                |
    |     void (*check_preempt_curr)(rq, task, flags);          |
    |     struct task_struct *(*pick_next_task)(rq);            |
    |     void (*put_prev_task)(rq, task);                      |
    |     void (*task_tick)(rq, task, queued);                  |
    | };                                                        |
    +----------------------------------------------------------+
```

**中文解释：**
- 调度类按优先级链接：stop > rt > fair > idle
- 机制调用策略钩子：入队、出队、检查抢占、选择下一个任务
- 策略实现自己的逻辑，机制调用契约接口

---

## Preemption Boundaries

```
+------------------------------------------------------------------+
|  PREEMPTION MODEL                                                |
+------------------------------------------------------------------+

    WHEN PREEMPTION CAN OCCUR:
    +----------------------------------------------------------+
    | 1. Returning from interrupt to user space                 |
    | 2. Returning from interrupt to kernel (if preempt enabled)|
    | 3. Explicitly calling schedule()                          |
    | 4. Blocking operations (sleep, wait)                      |
    +----------------------------------------------------------+
    
    WHEN PREEMPTION CANNOT OCCUR:
    +----------------------------------------------------------+
    | 1. Holding spinlock (preempt_disable())                   |
    | 2. In interrupt context                                   |
    | 3. preempt_count > 0                                      |
    +----------------------------------------------------------+

    PREEMPT_COUNT STRUCTURE:
    
    ┌───────────────────────────────────────────────────────────┐
    │ 31        20 19      16 15        8 7          0          │
    │ ┌──────────┬──────────┬───────────┬───────────────┐       │
    │ │ PREEMPT  │ SOFTIRQ  │  HARDIRQ  │    PREEMPT    │       │
    │ │  ACTIVE  │  COUNT   │   COUNT   │    COUNT      │       │
    │ └──────────┴──────────┴───────────┴───────────────┘       │
    └───────────────────────────────────────────────────────────┘
    
    preempt_count() > 0 means preemption is disabled

    INVARIANT:
    +----------------------------------------------------------+
    | Schedule must never be called with preempt_count > 0      |
    | (except for explicit schedule() which checks this)        |
    +----------------------------------------------------------+
```

**中文解释：**
- 可抢占：从中断返回用户态、从中断返回内核（如果启用）、显式调用 schedule、阻塞操作
- 不可抢占：持有自旋锁、在中断上下文、preempt_count > 0
- preempt_count 编码：抢占计数 + 硬中断计数 + 软中断计数

---

## Invariants That Must Never Be Violated

```
+------------------------------------------------------------------+
|  SCHEDULER INVARIANTS                                            |
+------------------------------------------------------------------+

    INVARIANT 1: Single Current Task
    +----------------------------------------------------------+
    | Each CPU has exactly one current task at any time         |
    | rq->curr is always valid (may be idle task)               |
    +----------------------------------------------------------+
    
    INVARIANT 2: Runqueue Consistency
    +----------------------------------------------------------+
    | task on_rq == 1 ⟺ task is in some runqueue                |
    | task on_rq == 0 ⟺ task is not in any runqueue             |
    +----------------------------------------------------------+
    
    INVARIANT 3: State-Queue Coupling
    +----------------------------------------------------------+
    | TASK_RUNNING → must be in runqueue (or currently running) |
    | TASK_INTERRUPTIBLE → must NOT be in runqueue              |
    | TASK_DEAD → must NOT be in runqueue                       |
    +----------------------------------------------------------+
    
    INVARIANT 4: Lock Ordering
    +----------------------------------------------------------+
    | Never hold rq->lock of multiple runqueues simultaneously  |
    | (except during migration with careful ordering)           |
    +----------------------------------------------------------+
    
    INVARIANT 5: Context Switch Atomicity
    +----------------------------------------------------------+
    | Context switch must complete atomically with respect to   |
    | the task being switched to/from                           |
    +----------------------------------------------------------+
```

**中文解释：**
- 不变量1：每个 CPU 恰好有一个当前任务
- 不变量2：on_rq 标志与队列状态一致
- 不变量3：任务状态与队列成员关系耦合
- 不变量4：不能同时持有多个运行队列的锁
- 不变量5：上下文切换必须是原子的

---

## User-Space Thread Pool Design

```
+------------------------------------------------------------------+
|  TRANSLATING TO USER-SPACE                                       |
+------------------------------------------------------------------+

    KERNEL CONCEPT          →    USER-SPACE EQUIVALENT
    ─────────────────────────────────────────────────────
    Per-CPU runqueue        →    Per-thread work queue
    Scheduler class         →    Priority/scheduling policy
    Task state              →    Job state (pending/running/done)
    Context switch          →    Thread pool dispatch
    Preemption              →    Cooperative yield points
    
    USER-SPACE THREAD POOL DESIGN:
    
    ┌─────────────────────────────────────────────────────────┐
    │                    Job Submission                        │
    │                         │                                │
    │                         ▼                                │
    │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
    │  │   Queue 0   │  │   Queue 1   │  │   Queue N   │      │
    │  │  (worker 0) │  │  (worker 1) │  │  (worker N) │      │
    │  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘      │
    │         │                │                │              │
    │         ▼                ▼                ▼              │
    │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐      │
    │  │  Worker 0   │  │  Worker 1   │  │  Worker N   │      │
    │  │  (thread)   │  │  (thread)   │  │  (thread)   │      │
    │  └─────────────┘  └─────────────┘  └─────────────┘      │
    └─────────────────────────────────────────────────────────┘
    
    KEY DESIGN PRINCIPLES:
    +----------------------------------------------------------+
    | 1. Per-worker queues reduce contention                    |
    | 2. Work stealing for load balancing                       |
    | 3. State machine for job lifecycle                        |
    | 4. Separate mechanism (dispatch) from policy (priority)   |
    +----------------------------------------------------------+
```

```c
/* User-space thread pool inspired by kernel scheduler */

enum job_state {
    JOB_PENDING,    /* Equivalent to TASK_RUNNING (in queue) */
    JOB_RUNNING,    /* Currently executing */
    JOB_BLOCKED,    /* Waiting for something */
    JOB_COMPLETED   /* Finished */
};

struct job {
    enum job_state state;
    void (*func)(void *arg);
    void *arg;
    struct job *next;
};

struct worker_queue {
    pthread_mutex_t lock;
    struct job *head;
    struct job *tail;
    int count;
    pthread_cond_t not_empty;
};

/* Per-worker queue - kernel per-CPU runqueue equivalent */
struct worker {
    pthread_t thread;
    struct worker_queue queue;
    int id;
};

void enqueue_job(struct worker *w, struct job *j)
{
    pthread_mutex_lock(&w->queue.lock);
    j->state = JOB_PENDING;
    /* Add to queue */
    if (w->queue.tail) {
        w->queue.tail->next = j;
    } else {
        w->queue.head = j;
    }
    w->queue.tail = j;
    j->next = NULL;
    w->queue.count++;
    pthread_cond_signal(&w->queue.not_empty);
    pthread_mutex_unlock(&w->queue.lock);
}

struct job *dequeue_job(struct worker *w)
{
    pthread_mutex_lock(&w->queue.lock);
    while (w->queue.count == 0) {
        pthread_cond_wait(&w->queue.not_empty, &w->queue.lock);
    }
    struct job *j = w->queue.head;
    w->queue.head = j->next;
    if (!w->queue.head) w->queue.tail = NULL;
    w->queue.count--;
    j->state = JOB_RUNNING;
    pthread_mutex_unlock(&w->queue.lock);
    return j;
}
```

**中文解释：**
- 内核概念到用户态映射：每CPU队列→每线程队列、调度类→策略、任务状态→作业状态
- 设计原则：每工作者队列减少竞争、工作窃取做负载均衡、状态机管理生命周期、机制与策略分离

