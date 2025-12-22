# Linux Kernel Policy vs Mechanism Separation (v3.2)

## Overview

This document explains how the Linux kernel **separates policy from mechanism**, enabling long-term maintainability and extensible architecture design.

---

## Definition: Policy vs Mechanism

```
+------------------------------------------------------------------+
|  POLICY vs MECHANISM                                             |
+------------------------------------------------------------------+

    MECHANISM:
    +----------------------------------------------------------+
    | "How to do something"                                    |
    |                                                          |
    | - The implementation of a capability                     |
    | - Stable, rarely changes                                 |
    | - Provides primitives and operations                     |
    | - Does NOT make decisions                                |
    +----------------------------------------------------------+
    
    POLICY:
    +----------------------------------------------------------+
    | "When and what to do"                                    |
    |                                                          |
    | - The decision logic                                     |
    | - Changes based on requirements                          |
    | - Uses mechanisms to achieve goals                       |
    | - Can be swapped without changing mechanism              |
    +----------------------------------------------------------+

+------------------------------------------------------------------+
|  ANALOGY: HIGHWAY SYSTEM                                         |
+------------------------------------------------------------------+

    MECHANISM:                      POLICY:
    +------------------+            +------------------+
    | - Roads          |            | - Speed limits   |
    | - Traffic lights |            | - Lane rules     |
    | - Signs          |            | - Rush hour rules|
    | - Bridges        |            | - Toll rates     |
    +------------------+            +------------------+
           |                                |
           | Fixed infrastructure           | Changeable rules
           | Expensive to change            | Easy to change
```

**中文解释：**
- **机制**："如何做" — 实现能力、稳定、提供原语
- **策略**："何时做什么" — 决策逻辑、可变、使用机制实现目标
- 类比：高速公路系统 — 道路是机制，限速规则是策略

---

## Scheduler Classes: Policy/Mechanism Example

### The Mechanism: Core Scheduler

```
+------------------------------------------------------------------+
|  SCHEDULER ARCHITECTURE                                          |
+------------------------------------------------------------------+

    +-----------------------------------------------------------+
    |                    SCHEDULER CORE (Mechanism)              |
    +-----------------------------------------------------------+
    | - Run queue management                                     |
    | - Context switching                                        |
    | - Timer tick handling                                      |
    | - CPU selection                                            |
    | - Load balancing infrastructure                            |
    +-----------------------------------------------------------+
                              |
                              | Calls policy through ops
                              v
    +------------+  +------------+  +------------+  +------------+
    |   STOP     |  |   RT       |  |   FAIR     |  |   IDLE     |
    |  (policy)  |  |  (policy)  |  |  (policy)  |  |  (policy)  |
    +------------+  +------------+  +------------+  +------------+
    | Migration  |  | Real-time  |  | CFS        |  | Run when   |
    | stopper    |  | FIFO/RR    |  | completely |  | nothing    |
    | threads    |  | priorities |  | fair       |  | else runs  |
    +------------+  +------------+  +------------+  +------------+
```

### The Policy Interface: sched_class

From `include/linux/sched.h`:

```c
struct sched_class {
    const struct sched_class *next;  /* Priority chain */
    
    /* Policy operations */
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)(struct rq *rq);
    
    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);
    
    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);
    
    void (*set_curr_task)(struct rq *rq);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)(struct task_struct *p);
    /* ... */
};
```

### Policy Implementations

```c
/* Real-time scheduler policy */
const struct sched_class rt_sched_class = {
    .next           = &fair_sched_class,
    .enqueue_task   = enqueue_task_rt,
    .dequeue_task   = dequeue_task_rt,
    .pick_next_task = pick_next_task_rt,
    /* ... */
};

/* Completely Fair Scheduler policy */
const struct sched_class fair_sched_class = {
    .next           = &idle_sched_class,
    .enqueue_task   = enqueue_task_fair,
    .dequeue_task   = dequeue_task_fair,
    .pick_next_task = pick_next_task_fair,
    /* ... */
};
```

### How Policy is Swapped

```c
/* Mechanism code calls policy through ops */
static inline struct task_struct *
pick_next_task(struct rq *rq)
{
    const struct sched_class *class;
    struct task_struct *p;
    
    /* Try each class in priority order */
    for_each_class(class) {
        p = class->pick_next_task(rq);  /* Policy decision */
        if (p)
            return p;
    }
    
    BUG(); /* Should never happen - idle class always has a task */
}

/* Adding a new scheduling policy:
 * 1. Create new sched_class
 * 2. Implement the ops
 * 3. Link into class chain
 * NO changes to pick_next_task()!
 */
```

**中文解释：**
- 调度器核心是机制：运行队列、上下文切换、负载均衡
- 调度类是策略：RT、CFS、IDLE 各有不同决策逻辑
- 通过 `sched_class` ops 表切换策略，无需修改核心机制

---

## VFS: Policy/Mechanism Example

```
+------------------------------------------------------------------+
|  VFS ARCHITECTURE                                                |
+------------------------------------------------------------------+

    +-----------------------------------------------------------+
    |                      VFS CORE (Mechanism)                  |
    +-----------------------------------------------------------+
    | - Inode/dentry cache management                            |
    | - Path lookup                                              |
    | - File descriptor management                               |
    | - Permission checking framework                            |
    | - Buffer/page cache                                        |
    +-----------------------------------------------------------+
                              |
                              | Calls policy through ops
                              v
    +------------+  +------------+  +------------+  +------------+
    |    ext4    |  |    NFS     |  |   procfs   |  |   tmpfs    |
    |  (policy)  |  |  (policy)  |  |  (policy)  |  |  (policy)  |
    +------------+  +------------+  +------------+  +------------+
    | Disk       |  | Network    |  | Kernel     |  | Memory     |
    | layout     |  | protocol   |  | state      |  | only       |
    | Journaling |  | Caching    |  | Dynamic    |  | Volatile   |
    +------------+  +------------+  +------------+  +------------+
```

### VFS Policy Interface

```c
struct file_operations {
    /* Policy: How this filesystem implements read */
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    /* ... */
};

struct inode_operations {
    /* Policy: How this filesystem creates files */
    int (*create)(struct inode *, struct dentry *, int, struct nameidata *);
    struct dentry *(*lookup)(struct inode *, struct dentry *, struct nameidata *);
    /* ... */
};
```

### VFS Mechanism Calls Policy

```c
/* VFS mechanism - same for all filesystems */
ssize_t vfs_read(struct file *file, char __user *buf, 
                 size_t count, loff_t *pos)
{
    /* Mechanism: Common validation */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || !file->f_op->read)
        return -EINVAL;
    
    /* Mechanism: Permission check */
    ret = rw_verify_area(READ, file, pos, count);
    if (ret)
        return ret;
    
    /* Policy: Filesystem-specific read */
    return file->f_op->read(file, buf, count, pos);
}
```

**中文解释：**
- VFS 核心是机制：缓存管理、路径查找、权限检查
- 文件系统是策略：ext4 用磁盘、NFS 用网络、procfs 用内核数据
- 通过 `file_operations`/`inode_operations` 切换策略

---

## Block Layer: Policy/Mechanism Example

```
+------------------------------------------------------------------+
|  BLOCK LAYER ARCHITECTURE                                        |
+------------------------------------------------------------------+

    +-----------------------------------------------------------+
    |                   BLOCK CORE (Mechanism)                   |
    +-----------------------------------------------------------+
    | - Request queue management                                 |
    | - Bio/request handling                                     |
    | - Plugging/unplugging                                      |
    | - DMA mapping                                              |
    +-----------------------------------------------------------+
                              |
                              | Calls policy through elevator_ops
                              v
    +------------+  +------------+  +------------+  +------------+
    |    NOOP    |  |    CFQ     |  |  DEADLINE  |  |    BFQ     |
    |  (policy)  |  |  (policy)  |  |  (policy)  |  |  (policy)  |
    +------------+  +------------+  +------------+  +------------+
    | No         |  | Fair       |  | Deadline   |  | Budget     |
    | scheduling |  | queuing    |  | based      |  | fair       |
    | FIFO       |  | per-process|  | Latency    |  | queuing    |
    +------------+  +------------+  +------------+  +------------+
```

### I/O Scheduler Interface

```c
struct elevator_ops {
    /* Policy: Decide request order */
    elevator_dispatch_fn *elevator_dispatch_fn;
    
    /* Policy: Merge requests */
    elevator_merge_fn *elevator_merge_fn;
    elevator_merge_req_fn *elevator_merge_req_fn;
    
    /* Policy: Add new request */
    elevator_add_req_fn *elevator_add_req_fn;
    
    /* Lifecycle */
    elevator_init_fn *elevator_init_fn;
    elevator_exit_fn *elevator_exit_fn;
};
```

### Runtime Policy Selection

```bash
# View available schedulers
$ cat /sys/block/sda/queue/scheduler
noop deadline [cfq]

# Switch policy at runtime!
$ echo deadline > /sys/block/sda/queue/scheduler
$ cat /sys/block/sda/queue/scheduler
noop [deadline] cfq

# No reboot, no mechanism change, just policy swap
```

**中文解释：**
- Block 核心是机制：请求队列、DMA 映射
- I/O 调度器是策略：NOOP、CFQ、DEADLINE
- 可在运行时切换策略，无需修改机制

---

## How Policy is Swapped Without Touching Mechanism

```
+------------------------------------------------------------------+
|  SWAPPING POLICY WITHOUT MECHANISM CHANGE                        |
+------------------------------------------------------------------+

    STEP 1: Define policy interface (ops table)
    +----------------------------------------------------------+
    | struct xxx_policy_ops {                                  |
    |     int (*decide)(struct context *ctx);                  |
    |     void (*update)(struct context *ctx, int result);     |
    | };                                                       |
    +----------------------------------------------------------+
    
    STEP 2: Mechanism uses interface
    +----------------------------------------------------------+
    | void mechanism_work(struct context *ctx) {               |
    |     int decision = ctx->policy->decide(ctx);             |
    |     do_work(decision);                                   |
    |     ctx->policy->update(ctx, result);                    |
    | }                                                        |
    +----------------------------------------------------------+
    
    STEP 3: Multiple policies implement interface
    +----------------------------------------------------------+
    | struct xxx_policy_ops policy_a = { .decide = a_decide }; |
    | struct xxx_policy_ops policy_b = { .decide = b_decide }; |
    +----------------------------------------------------------+
    
    STEP 4: Swap at runtime
    +----------------------------------------------------------+
    | ctx->policy = &policy_b;  /* Mechanism unchanged! */     |
    +----------------------------------------------------------+
```

### Example: Congestion Control

```c
/* TCP congestion control - classic policy/mechanism split */

/* Mechanism: TCP core */
void tcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    const struct tcp_congestion_ops *ca = inet_csk(sk)->icsk_ca_ops;
    
    /* Policy makes the decision */
    ca->cong_avoid(sk, ack, in_flight);
}

/* Policy A: Cubic */
static void cubictcp_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    /* Cubic algorithm */
}

/* Policy B: Reno */
static void tcp_reno_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
    /* Reno algorithm */
}

/* Swap congestion control per-connection:
 * setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, "cubic", 5);
 * setsockopt(fd, IPPROTO_TCP, TCP_CONGESTION, "reno", 4);
 */
```

**中文解释：**
- 切换策略的步骤：
  1. 定义策略接口（ops 表）
  2. 机制通过接口调用策略
  3. 多个策略实现同一接口
  4. 运行时切换策略指针
- TCP 拥塞控制是典型例子：Cubic、Reno 等可动态切换

---

## User-Space Application

```c
/* user_space_policy_mechanism.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------
 * Mechanism: Generic job processing
 *---------------------------------------------------------*/
struct job {
    int id;
    int priority;
    int size;
};

struct job_queue {
    struct job **jobs;
    int count;
    int capacity;
    const struct scheduling_policy *policy;  /* Swappable! */
};

/*---------------------------------------------------------
 * Policy interface
 *---------------------------------------------------------*/
struct scheduling_policy {
    const char *name;
    void (*init)(struct job_queue *q);
    void (*add_job)(struct job_queue *q, struct job *job);
    struct job *(*get_next)(struct job_queue *q);
    void (*cleanup)(struct job_queue *q);
};

/*---------------------------------------------------------
 * Mechanism functions (use policy through ops)
 *---------------------------------------------------------*/
struct job_queue *queue_create(const struct scheduling_policy *policy)
{
    struct job_queue *q = calloc(1, sizeof(*q));
    q->capacity = 100;
    q->jobs = calloc(q->capacity, sizeof(struct job *));
    q->policy = policy;
    
    if (q->policy->init)
        q->policy->init(q);
    
    printf("[Mechanism] Created queue with policy: %s\n", policy->name);
    return q;
}

void queue_add(struct job_queue *q, struct job *job)
{
    /* Mechanism: Store job */
    if (q->count < q->capacity)
        q->jobs[q->count++] = job;
    
    /* Policy: May reorder */
    q->policy->add_job(q, job);
}

struct job *queue_next(struct job_queue *q)
{
    /* Policy decides which job is next */
    return q->policy->get_next(q);
}

/*---------------------------------------------------------
 * Policy A: FIFO (First In First Out)
 *---------------------------------------------------------*/
static void fifo_add(struct job_queue *q, struct job *job)
{
    /* FIFO: Jobs stay in order */
    printf("[FIFO] Added job %d\n", job->id);
}

static struct job *fifo_next(struct job_queue *q)
{
    if (q->count == 0)
        return NULL;
    
    /* FIFO: Return first job */
    struct job *job = q->jobs[0];
    
    /* Shift remaining */
    for (int i = 0; i < q->count - 1; i++)
        q->jobs[i] = q->jobs[i + 1];
    q->count--;
    
    printf("[FIFO] Selected job %d\n", job->id);
    return job;
}

static const struct scheduling_policy fifo_policy = {
    .name = "FIFO",
    .add_job = fifo_add,
    .get_next = fifo_next,
};

/*---------------------------------------------------------
 * Policy B: Priority-based
 *---------------------------------------------------------*/
static void priority_add(struct job_queue *q, struct job *job)
{
    /* Priority: Sort on add */
    int i = q->count - 1;
    while (i > 0 && q->jobs[i - 1]->priority < job->priority) {
        q->jobs[i] = q->jobs[i - 1];
        i--;
    }
    q->jobs[i] = job;
    printf("[Priority] Added job %d (priority %d)\n", 
           job->id, job->priority);
}

static struct job *priority_next(struct job_queue *q)
{
    if (q->count == 0)
        return NULL;
    
    /* Priority: First is highest priority */
    struct job *job = q->jobs[0];
    
    for (int i = 0; i < q->count - 1; i++)
        q->jobs[i] = q->jobs[i + 1];
    q->count--;
    
    printf("[Priority] Selected job %d (priority %d)\n",
           job->id, job->priority);
    return job;
}

static const struct scheduling_policy priority_policy = {
    .name = "Priority",
    .add_job = priority_add,
    .get_next = priority_next,
};

/*---------------------------------------------------------
 * Policy C: Shortest Job First
 *---------------------------------------------------------*/
static void sjf_add(struct job_queue *q, struct job *job)
{
    /* SJF: Sort by size */
    int i = q->count - 1;
    while (i > 0 && q->jobs[i - 1]->size > job->size) {
        q->jobs[i] = q->jobs[i - 1];
        i--;
    }
    q->jobs[i] = job;
    printf("[SJF] Added job %d (size %d)\n", job->id, job->size);
}

static struct job *sjf_next(struct job_queue *q)
{
    if (q->count == 0)
        return NULL;
    
    struct job *job = q->jobs[0];
    
    for (int i = 0; i < q->count - 1; i++)
        q->jobs[i] = q->jobs[i + 1];
    q->count--;
    
    printf("[SJF] Selected job %d (size %d)\n", job->id, job->size);
    return job;
}

static const struct scheduling_policy sjf_policy = {
    .name = "SJF (Shortest Job First)",
    .add_job = sjf_add,
    .get_next = sjf_next,
};

/*---------------------------------------------------------
 * Demo: Same mechanism, different policies
 *---------------------------------------------------------*/
int main(void)
{
    struct job jobs[] = {
        {1, 5, 100},
        {2, 10, 50},
        {3, 1, 200},
        {4, 8, 25},
    };
    int n = sizeof(jobs) / sizeof(jobs[0]);
    
    printf("=== Testing FIFO Policy ===\n");
    struct job_queue *q1 = queue_create(&fifo_policy);
    for (int i = 0; i < n; i++)
        queue_add(q1, &jobs[i]);
    printf("Processing:\n");
    while (queue_next(q1));
    
    printf("\n=== Testing Priority Policy ===\n");
    struct job_queue *q2 = queue_create(&priority_policy);
    for (int i = 0; i < n; i++)
        queue_add(q2, &jobs[i]);
    printf("Processing:\n");
    while (queue_next(q2));
    
    printf("\n=== Testing SJF Policy ===\n");
    struct job_queue *q3 = queue_create(&sjf_policy);
    for (int i = 0; i < n; i++)
        queue_add(q3, &jobs[i]);
    printf("Processing:\n");
    while (queue_next(q3));
    
    return 0;
}
```

**中文解释：**
- 用户态示例：作业调度系统
- 机制：队列管理、作业存储
- 策略：FIFO、优先级、最短作业优先
- 相同机制代码，切换策略即改变行为

---

## Summary

```
+------------------------------------------------------------------+
|  POLICY/MECHANISM SEPARATION SUMMARY                             |
+------------------------------------------------------------------+

    BENEFITS:
    +----------------------------------------------------------+
    | 1. EXTENSIBILITY                                          |
    |    Add new policies without changing mechanism            |
    |                                                          |
    | 2. MAINTAINABILITY                                        |
    |    Mechanism stable, policies can evolve                  |
    |                                                          |
    | 3. TESTABILITY                                            |
    |    Test mechanism once, test each policy separately       |
    |                                                          |
    | 4. FLEXIBILITY                                            |
    |    Swap policies at runtime                               |
    |                                                          |
    | 5. SEPARATION OF CONCERNS                                 |
    |    Different experts for mechanism vs policy              |
    +----------------------------------------------------------+
    
    IMPLEMENTATION PATTERN:
    +----------------------------------------------------------+
    | 1. Identify what is stable (mechanism)                    |
    | 2. Identify what varies (policy)                          |
    | 3. Define interface between them (ops table)              |
    | 4. Implement mechanism using interface                    |
    | 5. Implement multiple policies                            |
    | 6. Provide way to select/swap policies                    |
    +----------------------------------------------------------+
    
    KERNEL EXAMPLES:
    +----------------------------------------------------------+
    | Subsystem      | Mechanism          | Policy             |
    |----------------|--------------------|--------------------|
    | Scheduler      | Core scheduler     | sched_class        |
    | VFS            | File operations    | Filesystems        |
    | Block          | Request queue      | I/O schedulers     |
    | TCP            | TCP core           | Congestion control |
    | Memory         | Page allocator     | Reclaim policies   |
    +----------------------------------------------------------+
```

**中文总结：**
策略/机制分离的核心原则：
1. **可扩展性**：添加新策略无需修改机制
2. **可维护性**：机制稳定，策略可演化
3. **可测试性**：分别测试机制和策略
4. **灵活性**：运行时切换策略
5. **关注点分离**：不同专家负责不同部分

实现模式：识别稳定部分（机制）→ 识别变化部分（策略）→ 定义接口 → 机制使用接口 → 多策略实现

