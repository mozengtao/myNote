# Linux Kernel Block Layer Request Model (v3.2)

## Overview

This document explains **block request queue architecture** in Linux kernel v3.2, focusing on batching, fairness, and throughput vs latency tradeoffs.

---

## Request Queue Architecture

```
+------------------------------------------------------------------+
|  BLOCK LAYER OVERVIEW                                            |
+------------------------------------------------------------------+

    User Space
         │
         │ read()/write()
         ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                      VFS Layer                               │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                │ submit_bio()
                                ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                    Block Layer                               │
    │  ┌─────────────────────────────────────────────────────────┐│
    │  │              Request Queue (per device)                 ││
    │  │  ┌─────────────────────────────────────────────────────┐││
    │  │  │              I/O Scheduler (Elevator)               │││
    │  │  │  ┌─────────────────────────────────────────────────┐│││
    │  │  │  │  Sorted/Merged Requests                        ││││
    │  │  │  │  [req1][req2][req3][req4][req5]                ││││
    │  │  │  └─────────────────────────────────────────────────┘│││
    │  │  └─────────────────────────────────────────────────────┘││
    │  └─────────────────────────────────────────────────────────┘│
    └───────────────────────────────┬─────────────────────────────┘
                                    │
                                    │ dispatch
                                    ▼
    ┌─────────────────────────────────────────────────────────────┐
    │                  Block Device Driver                         │
    │                      (e.g., SCSI)                           │
    └───────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
                           Hardware
```

**中文解释：**
- 块层架构：VFS → 块层（请求队列 + I/O调度器）→ 设备驱动 → 硬件
- 请求队列：每个设备一个，包含排序/合并后的请求
- I/O调度器（电梯）：优化请求顺序

---

## Request Lifecycle

```
+------------------------------------------------------------------+
|  REQUEST LIFECYCLE                                               |
+------------------------------------------------------------------+

    BIO (Block I/O Unit)
         │
         │ submit_bio()
         ▼
    ┌─────────────────────┐
    │ Create/Merge Request│
    │   - New request?    │
    │   - Merge with      │
    │     existing?       │
    └──────────┬──────────┘
               │
               ▼
    ┌─────────────────────┐
    │ Add to Elevator     │
    │   - Sort by sector  │
    │   - Apply policy    │
    └──────────┬──────────┘
               │
               ▼
    ┌─────────────────────┐
    │ Wait in Queue       │
    │   - May be reordered│
    │   - May be merged   │
    └──────────┬──────────┘
               │
               │ elv_dispatch()
               ▼
    ┌─────────────────────┐
    │ Dispatch to Driver  │
    │   - Remove from     │
    │     elevator        │
    │   - Add to dispatch │
    │     queue           │
    └──────────┬──────────┘
               │
               │ request_fn()
               ▼
    ┌─────────────────────┐
    │ Driver Processes    │
    │   - Send to HW      │
    │   - Wait for IRQ    │
    └──────────┬──────────┘
               │
               │ completion IRQ
               ▼
    ┌─────────────────────┐
    │ Complete Request    │
    │   - Notify waiter   │
    │   - Free request    │
    └─────────────────────┘
```

**中文解释：**
- 请求生命周期：submit_bio → 创建/合并请求 → 加入电梯 → 等待 → 分发 → 驱动处理 → 完成
- 合并：相邻扇区的请求可合并为一个
- 排序：按扇区号排序减少磁头移动

---

## I/O Schedulers (Elevators)

```
+------------------------------------------------------------------+
|  I/O SCHEDULER COMPARISON (v3.2)                                 |
+------------------------------------------------------------------+

    1. NOOP (No-Operation)
    +----------------------------------------------------------+
    | - FIFO queue, no reordering                               |
    | - Merge only                                              |
    | - Best for: SSD, RAM disk, virtual machines               |
    | - Latency: Lowest                                         |
    | - Throughput: Depends on device                           |
    +----------------------------------------------------------+
    
    2. DEADLINE
    +----------------------------------------------------------+
    | - Sorted queue + deadline queues                          |
    | - Prevents starvation with expiration times               |
    | - Best for: Databases, latency-sensitive                  |
    | - Latency: Bounded (deadline guarantee)                   |
    | - Throughput: Good                                        |
    |                                                           |
    | Queue structure:                                          |
    | ┌─────────────────────────────────────┐                  |
    | │ Sorted Queue (by sector)            │                  |
    | │ [sect 100][sect 200][sect 300]      │                  |
    | └─────────────────────────────────────┘                  |
    | ┌─────────────────┐ ┌─────────────────┐                  |
    | │ Read Deadline   │ │ Write Deadline  │                  |
    | │ [oldest first]  │ │ [oldest first]  │                  |
    | └─────────────────┘ └─────────────────┘                  |
    +----------------------------------------------------------+
    
    3. CFQ (Completely Fair Queuing)
    +----------------------------------------------------------+
    | - Per-process queues                                      |
    | - Fair time slices                                        |
    | - Best for: Desktop, mixed workloads                      |
    | - Latency: Variable                                       |
    | - Throughput: Fair distribution                           |
    |                                                           |
    | Queue structure:                                          |
    | ┌─────────────────┐                                      |
    | │ Process A queue │──▶ [req][req][req]                   |
    | └─────────────────┘                                      |
    | ┌─────────────────┐                                      |
    | │ Process B queue │──▶ [req][req]                        |
    | └─────────────────┘                                      |
    | ┌─────────────────┐                                      |
    | │ Process C queue │──▶ [req][req][req][req]              |
    | └─────────────────┘                                      |
    |                                                           |
    | Round-robin with time slices                              |
    +----------------------------------------------------------+
```

**中文解释：**
- NOOP：无重排序，仅合并，适合 SSD/虚拟机
- DEADLINE：有界延迟，防止饥饿，适合数据库
- CFQ：每进程队列，公平时间片，适合桌面

---

## Merge and Dispatch

```
+------------------------------------------------------------------+
|  REQUEST MERGING                                                 |
+------------------------------------------------------------------+

    BACK MERGE (most common):
    
    Existing request:     New BIO:
    [sector 100-199]  +   [sector 200-299]
                          │
                          ▼
    Merged request:   [sector 100-299]
    
    FRONT MERGE (less common):
    
    Existing request:     New BIO:
    [sector 200-299]  +   [sector 100-199]
                          │
                          ▼
    Merged request:   [sector 100-299]

    MERGE BENEFITS:
    +----------------------------------------------------------+
    | 1. Fewer requests → less queue overhead                   |
    | 2. Larger I/O → better device efficiency                  |
    | 3. Sequential access → minimal seek time                  |
    +----------------------------------------------------------+

+------------------------------------------------------------------+
|  DISPATCH DECISION                                               |
+------------------------------------------------------------------+

    Dispatch criteria:
    
    ┌─────────────────────────────────────────────────────────────┐
    │ 1. Device has capacity (not full)                           │
    │ 2. Elevator selects next request                            │
    │    - Deadline: Expired request first                        │
    │    - CFQ: Current process's queue, time slice               │
    │ 3. Unplug triggered (batching complete)                     │
    └─────────────────────────────────────────────────────────────┘

    PLUGGING/UNPLUGGING:
    
    Submit BIO 1 ──┐
    Submit BIO 2 ──┼──▶ [PLUGGED: Accumulating]
    Submit BIO 3 ──┘
                       │
                       │ unplug (timer or explicit)
                       ▼
                   [Dispatch all at once]
    
    +----------------------------------------------------------+
    | Plugging delays dispatch to allow more merging            |
    | Unplugging sends accumulated requests to device           |
    +----------------------------------------------------------+
```

**中文解释：**
- 合并类型：后合并（常见）、前合并
- 合并好处：更少请求、更大I/O、顺序访问
- 插拔机制：累积请求（plugged）→ 批量分发（unplug）

---

## Throughput vs Latency Tradeoff

```
+------------------------------------------------------------------+
|  THROUGHPUT vs LATENCY ANALYSIS                                  |
+------------------------------------------------------------------+

    MAXIMIZE THROUGHPUT:
    +----------------------------------------------------------+
    | Strategy: Large batches, heavy reordering                 |
    |                                                           |
    | - Long plug intervals (accumulate more requests)          |
    | - Aggressive merging                                      |
    | - Sort by sector (minimize seek)                          |
    |                                                           |
    | Tradeoff: Individual request latency increases            |
    +----------------------------------------------------------+

    MINIMIZE LATENCY:
    +----------------------------------------------------------+
    | Strategy: Immediate dispatch, minimal reordering          |
    |                                                           |
    | - Short/no plug intervals                                 |
    | - FIFO or deadline-based                                  |
    | - NOOP scheduler                                          |
    |                                                           |
    | Tradeoff: Overall throughput may decrease                 |
    +----------------------------------------------------------+

    WORKLOAD-BASED DECISION:
    
    ┌────────────────────┬──────────────┬──────────────────────┐
    │ Workload           │ Priority     │ Scheduler            │
    ├────────────────────┼──────────────┼──────────────────────┤
    │ Sequential backup  │ Throughput   │ NOOP or CFQ          │
    │ Database OLTP      │ Latency      │ Deadline             │
    │ Desktop usage      │ Fairness     │ CFQ                  │
    │ SSD storage        │ Latency      │ NOOP                 │
    │ Batch processing   │ Throughput   │ CFQ (ionice)         │
    └────────────────────┴──────────────┴──────────────────────┘

    HDD vs SSD BEHAVIOR:
    
    HDD (seek matters):
    ┌─────────────────────────────────────────────────────────────┐
    │ Random I/O: 100 IOPS     Sequential: 200 MB/s               │
    │ Reordering helps A LOT                                      │
    │ Seek time: 5-10 ms dominates                                │
    └─────────────────────────────────────────────────────────────┘

    SSD (no seek penalty):
    ┌─────────────────────────────────────────────────────────────┐
    │ Random I/O: 100,000 IOPS  Sequential: 500 MB/s              │
    │ Reordering helps LITTLE                                     │
    │ Just dispatch immediately                                   │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 最大化吞吐量：长批处理间隔、激进合并、按扇区排序（延迟增加）
- 最小化延迟：立即分发、最少重排序、NOOP 调度器（吞吐量可能下降）
- HDD：重排序帮助大（寻道时间主导）
- SSD：重排序帮助小（无寻道惩罚）

---

## User-Space Batching System

```c
/* User-space batching system inspired by block layer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

/* Request structure */
struct request {
    int id;
    int priority;           /* Lower = higher priority */
    void (*process)(struct request *);
    void *data;
    struct request *next;
};

/* Request queue with batching */
struct request_queue {
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    
    struct request *queue;  /* Sorted by priority */
    int count;
    
    /* Batching (plugging) */
    int plugged;
    int batch_size;
    int batch_count;
    
    /* Stats */
    int total_processed;
    int merges;
};

/* Initialize queue */
void rq_init(struct request_queue *rq, int batch_size)
{
    pthread_mutex_init(&rq->lock, NULL);
    pthread_cond_init(&rq->not_empty, NULL);
    rq->queue = NULL;
    rq->count = 0;
    rq->plugged = 0;
    rq->batch_size = batch_size;
    rq->batch_count = 0;
}

/* Insert sorted by priority (like elevator sort) */
void rq_insert(struct request_queue *rq, struct request *req)
{
    pthread_mutex_lock(&rq->lock);
    
    struct request **pp = &rq->queue;
    while (*pp && (*pp)->priority <= req->priority) {
        pp = &(*pp)->next;
    }
    req->next = *pp;
    *pp = req;
    rq->count++;
    rq->batch_count++;
    
    /* Unplug if batch full or not plugged */
    if (!rq->plugged || rq->batch_count >= rq->batch_size) {
        pthread_cond_signal(&rq->not_empty);
    }
    
    pthread_mutex_unlock(&rq->lock);
}

/* Plug (start batching) */
void rq_plug(struct request_queue *rq)
{
    pthread_mutex_lock(&rq->lock);
    rq->plugged = 1;
    rq->batch_count = 0;
    pthread_mutex_unlock(&rq->lock);
}

/* Unplug (dispatch batch) */
void rq_unplug(struct request_queue *rq)
{
    pthread_mutex_lock(&rq->lock);
    rq->plugged = 0;
    if (rq->count > 0) {
        pthread_cond_signal(&rq->not_empty);
    }
    pthread_mutex_unlock(&rq->lock);
}

/* Dispatch request (worker thread) */
struct request *rq_dispatch(struct request_queue *rq)
{
    pthread_mutex_lock(&rq->lock);
    
    while (rq->count == 0 || rq->plugged) {
        pthread_cond_wait(&rq->not_empty, &rq->lock);
    }
    
    struct request *req = rq->queue;
    rq->queue = req->next;
    rq->count--;
    rq->total_processed++;
    
    pthread_mutex_unlock(&rq->lock);
    return req;
}

/* Worker thread */
void *worker(void *arg)
{
    struct request_queue *rq = arg;
    
    while (1) {
        struct request *req = rq_dispatch(rq);
        if (req->process) {
            req->process(req);
        }
        free(req);
    }
    return NULL;
}

/* Example: Batched I/O simulation */
void process_io(struct request *req)
{
    printf("Processing request %d (priority %d)\n", 
           req->id, req->priority);
    usleep(10000);  /* Simulate I/O */
}

int main(void)
{
    struct request_queue rq;
    rq_init(&rq, 10);  /* Batch size 10 */
    
    /* Start worker */
    pthread_t tid;
    pthread_create(&tid, NULL, worker, &rq);
    
    /* Submit requests with batching */
    rq_plug(&rq);  /* Start batching */
    
    for (int i = 0; i < 20; i++) {
        struct request *req = malloc(sizeof(*req));
        req->id = i;
        req->priority = rand() % 100;
        req->process = process_io;
        req->data = NULL;
        rq_insert(&rq, req);
        
        if (i == 9) {
            rq_unplug(&rq);  /* Dispatch first batch */
            usleep(100000);
            rq_plug(&rq);    /* Start second batch */
        }
    }
    
    rq_unplug(&rq);  /* Dispatch remaining */
    
    sleep(1);
    printf("Processed: %d\n", rq.total_processed);
    
    return 0;
}
```

**中文解释：**
- 用户态批处理系统：模拟块层请求队列
- 插入时按优先级排序（类似电梯排序）
- plug/unplug 控制批处理
- 工作线程从队列分发请求

