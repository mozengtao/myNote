# Linux Kernel Deferred Execution Models (v3.2)

## Overview

This document explains **softirqs, tasklets, and workqueues** in Linux kernel v3.2, focusing on when and why to use each.

---

## Why Deferred Work Exists

```
+------------------------------------------------------------------+
|  THE PROBLEM: INTERRUPT CONTEXT CONSTRAINTS                      |
+------------------------------------------------------------------+

    HARD IRQ HANDLER:
    +----------------------------------------------------------+
    | - Must complete FAST (microseconds)                       |
    | - Cannot sleep or block                                   |
    | - Cannot acquire sleeping locks (mutex)                   |
    | - Interrupts may be disabled                              |
    +----------------------------------------------------------+
    
    BUT WORK TO DO:
    +----------------------------------------------------------+
    | - Process network packets (complex parsing)               |
    | - Update data structures (may need locks)                 |
    | - Interact with user-space (may sleep)                    |
    +----------------------------------------------------------+
    
    SOLUTION: DEFER THE WORK
    +----------------------------------------------------------+
    | Hard IRQ:    Quick ACK, queue work, return               |
    | Deferred:    Do heavy processing later, safer context    |
    +----------------------------------------------------------+

    ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
    │  Hard IRQ    │────▶│   Softirq/   │────▶│  Process     │
    │  (fast ACK)  │     │   Tasklet    │     │  Context     │
    └──────────────┘     └──────────────┘     └──────────────┘
         │                      │                     │
         │                      │                     │
    Interrupts          Interrupts ON          Full sleeping
    may be OFF          No sleeping            allowed
```

**中文解释：**
- 硬中断限制：必须快速完成、不能睡眠、不能获取睡眠锁
- 但有复杂工作：处理网络包、更新数据结构、与用户态交互
- 解决方案：延迟工作 — 硬中断快速确认后排队，稍后在更安全上下文处理

---

## Softirq, Tasklet, Workqueue Comparison

```
+------------------------------------------------------------------+
|  DEFERRED EXECUTION COMPARISON TABLE                             |
+------------------------------------------------------------------+

    ┌────────────┬─────────────┬─────────────┬─────────────────────┐
    │  Property  │   Softirq   │   Tasklet   │     Workqueue       │
    ├────────────┼─────────────┼─────────────┼─────────────────────┤
    │ Context    │ Softirq     │ Softirq     │ Process (kworker)   │
    ├────────────┼─────────────┼─────────────┼─────────────────────┤
    │ May sleep? │ NO          │ NO          │ YES                 │
    ├────────────┼─────────────┼─────────────┼─────────────────────┤
    │ Preemptible│ NO          │ NO          │ YES                 │
    ├────────────┼─────────────┼─────────────┼─────────────────────┤
    │ Latency    │ Lowest      │ Low         │ Higher              │
    ├────────────┼─────────────┼─────────────┼─────────────────────┤
    │ Concurrency│ Same softirq│ Serialized  │ Parallel workers    │
    │            │ runs on all │ per-tasklet │                     │
    │            │ CPUs        │             │                     │
    ├────────────┼─────────────┼─────────────┼─────────────────────┤
    │ Use case   │ Networking  │ Driver      │ I/O, user-space     │
    │            │ Block I/O   │ bottom-half │ interaction         │
    ├────────────┼─────────────┼─────────────┼─────────────────────┤
    │ Dynamic?   │ NO (fixed)  │ YES         │ YES                 │
    └────────────┴─────────────┴─────────────┴─────────────────────┘
```

**中文解释：**
| 属性 | Softirq | Tasklet | Workqueue |
|------|---------|---------|-----------|
| 上下文 | 软中断 | 软中断 | 进程（kworker）|
| 可睡眠 | 否 | 否 | 是 |
| 可抢占 | 否 | 否 | 是 |
| 延迟 | 最低 | 低 | 较高 |
| 并发 | 所有CPU并行 | 每tasklet串行 | 并行工作者 |
| 动态创建 | 否 | 是 | 是 |

---

## Softirq Details

```
+------------------------------------------------------------------+
|  SOFTIRQ ARCHITECTURE                                            |
+------------------------------------------------------------------+

    STATIC SOFTIRQ TYPES (kernel/softirq.c):
    
    enum {
        HI_SOFTIRQ = 0,        /* High-priority tasklets */
        TIMER_SOFTIRQ,         /* Timer expiration */
        NET_TX_SOFTIRQ,        /* Network transmit */
        NET_RX_SOFTIRQ,        /* Network receive */
        BLOCK_SOFTIRQ,         /* Block device */
        BLOCK_IOPOLL_SOFTIRQ,  /* Block I/O polling */
        TASKLET_SOFTIRQ,       /* Regular tasklets */
        SCHED_SOFTIRQ,         /* Scheduler */
        HRTIMER_SOFTIRQ,       /* High-res timers */
        RCU_SOFTIRQ,           /* RCU processing */
        NR_SOFTIRQS
    };

    EXECUTION MODEL:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  Hardware IRQ                                                │
    │       │                                                      │
    │       ▼                                                      │
    │  raise_softirq(NET_RX_SOFTIRQ)  ← Mark softirq pending      │
    │       │                                                      │
    │       ▼                                                      │
    │  irq_exit() or ksoftirqd                                     │
    │       │                                                      │
    │       ▼                                                      │
    │  do_softirq()                                                │
    │       │                                                      │
    │       ▼                                                      │
    │  __do_softirq()                                              │
    │       │                                                      │
    │       ├──▶ HI_SOFTIRQ action                                │
    │       ├──▶ TIMER_SOFTIRQ action                             │
    │       ├──▶ NET_TX_SOFTIRQ action ← e.g., net_tx_action()    │
    │       ├──▶ NET_RX_SOFTIRQ action ← e.g., net_rx_action()    │
    │       └──▶ ... other pending softirqs                       │
    └─────────────────────────────────────────────────────────────┘
    
    FORBIDDEN OPERATIONS IN SOFTIRQ:
    +----------------------------------------------------------+
    | ✗ Sleep (schedule())                                      |
    | ✗ Mutex acquisition                                       |
    | ✗ User-space copy (copy_to_user)                          |
    | ✗ Long-running computation                                |
    +----------------------------------------------------------+
```

**中文解释：**
- Softirq 是静态定义的（编译时固定数量）
- 类型：高优先级tasklet、定时器、网络收发、块设备、调度器、RCU
- 执行模型：硬中断触发 → raise_softirq → irq_exit 或 ksoftirqd 执行
- 禁止操作：睡眠、互斥锁、用户空间拷贝、长时间计算

---

## Tasklet Details

```
+------------------------------------------------------------------+
|  TASKLET ARCHITECTURE                                            |
+------------------------------------------------------------------+

    struct tasklet_struct {
        struct tasklet_struct *next;
        unsigned long state;        /* TASKLET_STATE_* */
        atomic_t count;             /* 0 = enabled */
        void (*func)(unsigned long);
        unsigned long data;
    };

    STATE FLAGS:
    +----------------------------------------------------------+
    | TASKLET_STATE_SCHED  = scheduled (pending)                |
    | TASKLET_STATE_RUN    = currently running                  |
    +----------------------------------------------------------+

    SERIALIZATION GUARANTEE:
    
    CPU 0                          CPU 1
    ─────                          ─────
    tasklet_schedule(&t)           tasklet_schedule(&t)
         │                              │
         ▼                              ▼
    ┌─────────────┐              ┌─────────────┐
    │ Check RUN   │              │ Check RUN   │
    │ flag        │              │ flag        │
    └──────┬──────┘              └──────┬──────┘
           │                            │
           │  If RUN set, re-schedule   │
           │  ◄─────────────────────────┤
           ▼                            
    ┌─────────────┐              
    │ Set RUN     │   ← Only ONE CPU executes
    │ Execute     │              
    │ Clear RUN   │              
    └─────────────┘              

    GUARANTEE:
    +----------------------------------------------------------+
    | Same tasklet NEVER runs concurrently on multiple CPUs     |
    | This simplifies locking within the tasklet                |
    +----------------------------------------------------------+
```

**中文解释：**
- Tasklet 是动态创建的软中断处理单元
- 状态标志：SCHED（已调度）、RUN（正在运行）
- 串行化保证：同一 tasklet 不会在多个 CPU 上并发运行
- 简化了 tasklet 内部的锁定需求

---

## Workqueue Details

```
+------------------------------------------------------------------+
|  WORKQUEUE ARCHITECTURE                                          |
+------------------------------------------------------------------+

    struct work_struct {
        atomic_long_t data;      /* Workqueue pointer + flags */
        struct list_head entry;  /* Linked in queue */
        work_func_t func;        /* The work function */
    };

    EXECUTION MODEL:
    
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  queue_work(wq, &work)                                       │
    │         │                                                    │
    │         ▼                                                    │
    │  ┌─────────────────────────────────────────────────┐        │
    │  │              Per-CPU Work Pools                  │        │
    │  ├────────────┬────────────┬────────────┬──────────┤        │
    │  │   CPU 0    │   CPU 1    │   CPU 2    │   ...    │        │
    │  │  ┌──────┐  │  ┌──────┐  │  ┌──────┐  │          │        │
    │  │  │work1 │  │  │work3 │  │  │work5 │  │          │        │
    │  │  │work2 │  │  │work4 │  │  │      │  │          │        │
    │  │  └──────┘  │  └──────┘  │  └──────┘  │          │        │
    │  └────────────┴────────────┴────────────┴──────────┘        │
    │         │              │              │                      │
    │         ▼              ▼              ▼                      │
    │  ┌──────────┐   ┌──────────┐   ┌──────────┐                 │
    │  │ kworker/0│   │ kworker/1│   │ kworker/2│  ← Kernel       │
    │  │ (thread) │   │ (thread) │   │ (thread) │    threads      │
    │  └──────────┘   └──────────┘   └──────────┘                 │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    WORKQUEUE TYPES:
    +----------------------------------------------------------+
    | system_wq          - General purpose, shared              |
    | system_highpri_wq  - High priority work                   |
    | system_long_wq     - May take longer to complete          |
    | system_unbound_wq  - Not bound to specific CPU            |
    | system_freezable_wq- Can be frozen during suspend         |
    +----------------------------------------------------------+

    ALLOWED OPERATIONS:
    +----------------------------------------------------------+
    | ✓ Sleep (schedule())                                      |
    | ✓ Mutex acquisition                                       |
    | ✓ User-space copy                                         |
    | ✓ Memory allocation with GFP_KERNEL                       |
    | ✓ Long-running computation (but be considerate)           |
    +----------------------------------------------------------+
```

**中文解释：**
- Workqueue 在进程上下文（kworker 线程）中执行
- 每个 CPU 有工作池和 kworker 线程
- 可以睡眠、获取互斥锁、用户空间拷贝、内存分配
- 类型：通用、高优先级、长时间、非绑定、可冻结

---

## Real Kernel Examples

```
+------------------------------------------------------------------+
|  SOFTIRQ EXAMPLE: Network RX                                     |
+------------------------------------------------------------------+

    /* net/core/dev.c */
    static void net_rx_action(struct softirq_action *h)
    {
        struct softnet_data *sd = &__get_cpu_var(softnet_data);
        
        while (!list_empty(&sd->poll_list)) {
            struct napi_struct *n;
            
            n = list_first_entry(&sd->poll_list,
                                 struct napi_struct, poll_list);
            
            /* Process packets - NO SLEEPING */
            work = n->poll(n, weight);
            ...
        }
    }

+------------------------------------------------------------------+
|  TASKLET EXAMPLE: USB Controller                                 |
+------------------------------------------------------------------+

    /* Tasklet for USB controller bottom-half */
    static void ehci_tasklet(unsigned long param)
    {
        struct ehci_hcd *ehci = (struct ehci_hcd *)param;
        
        /* Process completed transfers */
        /* Cannot sleep, but serialized per-controller */
    }
    
    /* In IRQ handler */
    tasklet_schedule(&ehci->tasklet);

+------------------------------------------------------------------+
|  WORKQUEUE EXAMPLE: Block Device Cleanup                         |
+------------------------------------------------------------------+

    /* Deferred block device work */
    static void blk_release_queue_work(struct work_struct *work)
    {
        struct request_queue *q = 
            container_of(work, struct request_queue, release_work);
        
        /* Can sleep - clean up resources */
        blk_free_flush_queue(q);
        elevator_exit(q->elevator);
        ...
    }
    
    /* Schedule work */
    INIT_WORK(&q->release_work, blk_release_queue_work);
    schedule_work(&q->release_work);
```

**中文解释：**
- Softirq 示例：网络接收（net_rx_action），处理 NAPI 轮询
- Tasklet 示例：USB 控制器底半部，串行化处理每个控制器
- Workqueue 示例：块设备清理，可睡眠、释放资源

---

## User-Space Async Task System

```
+------------------------------------------------------------------+
|  USER-SPACE TRANSLATION                                          |
+------------------------------------------------------------------+

    KERNEL CONCEPT      →    USER-SPACE EQUIVALENT
    ───────────────────────────────────────────────────
    Softirq            →    Signal handler / Fast callback
    Tasklet            →    Serialized async callback
    Workqueue          →    Thread pool
    raise_softirq()    →    Event notification
    schedule_work()    →    Submit to thread pool

    DESIGN PATTERN:
    
    ┌─────────────────────────────────────────────────────────────┐
    │                     Event Loop Model                         │
    │                                                              │
    │  ┌──────────────┐     ┌──────────────┐     ┌────────────┐  │
    │  │   I/O Event  │────▶│ Fast Handler │────▶│  Deferred  │  │
    │  │   (epoll)    │     │ (non-block)  │     │  Work Pool │  │
    │  └──────────────┘     └──────────────┘     └────────────┘  │
    │        ↑                     │                    │         │
    │        │                     │                    │         │
    │   Event Loop             Callbacks           Thread Pool    │
    │   (single thread)        (fast path)         (blocking OK)  │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

```c
/* User-space async task system */

/* Fast callback - like tasklet (no blocking) */
typedef void (*fast_callback_t)(void *arg);

/* Deferred work - like workqueue (blocking OK) */
typedef void (*work_func_t)(void *arg);

struct work_item {
    work_func_t func;
    void *arg;
    struct work_item *next;
};

struct work_pool {
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    struct work_item *head, *tail;
    pthread_t *workers;
    int num_workers;
    int shutdown;
};

/* Submit work - like schedule_work() */
void submit_work(struct work_pool *pool, work_func_t func, void *arg)
{
    struct work_item *item = malloc(sizeof(*item));
    item->func = func;
    item->arg = arg;
    item->next = NULL;
    
    pthread_mutex_lock(&pool->lock);
    if (pool->tail) {
        pool->tail->next = item;
    } else {
        pool->head = item;
    }
    pool->tail = item;
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->lock);
}

/* Worker thread - like kworker */
void *worker_thread(void *arg)
{
    struct work_pool *pool = arg;
    
    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (!pool->head && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }
        
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        
        struct work_item *item = pool->head;
        pool->head = item->next;
        if (!pool->head) pool->tail = NULL;
        pthread_mutex_unlock(&pool->lock);
        
        /* Execute work - CAN BLOCK */
        item->func(item->arg);
        free(item);
    }
    return NULL;
}
```

**中文解释：**
- 用户态映射：Softirq→信号处理/快速回调、Tasklet→串行化异步回调、Workqueue→线程池
- 设计模式：事件循环 + 快速处理器 + 延迟工作池
- submit_work 类似 schedule_work，工作线程类似 kworker

