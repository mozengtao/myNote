# Case 3: I/O Scheduler (Elevator)

## Subsystem Background

```
+=============================================================================+
|                    I/O SCHEDULER ARCHITECTURE                                |
+=============================================================================+

                          BLOCK LAYER CORE
                          ================

    +------------------------------------------------------------------+
    |                     block/*.c                                     |
    |                                                                   |
    |   MECHANISM (Fixed):                                              |
    |   - Request queue management                                      |
    |   - Bio to request conversion                                     |
    |   - Plug/unplug handling                                          |
    |   - Hardware dispatch                                             |
    |   - Request completion                                            |
    |                                                                   |
    +------------------------------------------------------------------+
                                |
                                | delegates ORDERING POLICY to
                                v
    +------------------------------------------------------------------+
    |                   I/O SCHEDULER STRATEGIES                        |
    |                   (Elevator - Strategy Pattern)                   |
    |                                                                   |
    |   +------------------+  +------------------+  +------------------+|
    |   |      NOOP        |  |    Deadline      |  |       CFQ        ||
    |   | (no reordering)  |  | (latency-bound)  |  | (fair queuing)   ||
    |   +------------------+  +------------------+  +------------------+|
    |                                                                   |
    |   +------------------+                                            |
    |   |    Anticipatory  |  (deprecated in later kernels)            |
    |   | (read anticipate)|                                            |
    |   +------------------+                                            |
    |                                                                   |
    +------------------------------------------------------------------+

    KEY INSIGHT:
    - Block layer knows WHEN to dispatch I/O (queue processing)
    - Elevator knows HOW to order requests (minimize seek, ensure fairness)
```

**中文说明：**

I/O调度器架构：块层核心（`block/*.c`）负责机制——请求队列管理、bio到request的转换、插塞处理、硬件分发、请求完成。核心将排序策略委托给I/O调度器（电梯）：NOOP（无重排序）、Deadline（延迟限制）、CFQ（公平排队）、Anticipatory（读预测，后来废弃）。关键洞察：块层知道何时分发I/O，电梯知道如何排序请求（最小化寻道、确保公平）。

---

## The Strategy Interface: struct elevator_ops

### Components

| Component | Role |
|-----------|------|
| **Strategy Interface** | `struct elevator_ops` |
| **Replaceable Algorithm** | NOOP, Deadline, CFQ (complete scheduling policies) |
| **Selection Mechanism** | Per-queue, boot param, sysfs |

### The Interface

```c
struct elevator_ops {
    /* === MERGING === */
    elevator_merge_fn *elevator_merge_fn;
    elevator_merged_fn *elevator_merged_fn;
    elevator_merge_req_fn *elevator_merge_req_fn;
    elevator_allow_merge_fn *elevator_allow_merge_fn;

    /* === REQUEST HANDLING === */
    elevator_dispatch_fn *elevator_dispatch_fn;
    elevator_add_req_fn *elevator_add_req_fn;

    /* === QUEUE MANAGEMENT === */
    elevator_queue_empty_fn *elevator_queue_empty_fn;
    elevator_completed_req_fn *elevator_completed_req_fn;

    /* === LIFECYCLE === */
    elevator_init_fn *elevator_init_fn;
    elevator_exit_fn *elevator_exit_fn;

    /* ... more operations ... */
};

struct elevator_type {
    struct list_head list;
    struct elevator_ops ops;
    char elevator_name[ELV_NAME_MAX];
    struct module *owner;
};
```

### Control Flow: How Core Uses Strategy

```
    __elv_add_request() - Adding Request to Queue
    =============================================

    +----------------------------------+
    |  New I/O request arrives         |
    +----------------------------------+
                   |
                   v
    +----------------------------------+
    |  Block layer validates request   |  MECHANISM
    |  (sector range, flags, etc.)     |  (Core)
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  elevator->ops->elevator_merge_fn()    ||  STRATEGY
    ||  (Can this merge with existing req?)   ||  (decides merge)
    +==========================================+
                   |
            +------+------+
            |             |
            v             v
    +------------+  +------------------+
    |  Merged    |  | No merge         |
    +------------+  +------------------+
                          |
                          v
    +==========================================+
    ||  elevator->ops->elevator_add_req_fn()  ||  STRATEGY
    ||  (Add to scheduler's internal queue)   ||  (decides position)
    +==========================================+


    blk_peek_request() - Dispatching Request
    ========================================

    +----------------------------------+
    |  Driver ready for next request   |
    +----------------------------------+
                   |
                   v
    +==========================================+
    ||  elevator->ops->elevator_dispatch_fn() ||  STRATEGY
    ||  (Select which request to dispatch)    ||  (decides order)
    +==========================================+
                   |
                   v
    +----------------------------------+
    |  Send to hardware                |  MECHANISM
    +----------------------------------+
```

**中文说明：**

添加请求的控制流：新I/O请求到达，块层验证请求（机制），然后调用策略的`elevator_merge_fn()`判断是否能与现有请求合并，如果不能合并则调用`elevator_add_req_fn()`添加到调度器的内部队列。分发请求时，调用策略的`elevator_dispatch_fn()`选择哪个请求先分发，策略决定顺序。

---

## Why Strategy is Required Here

### 1. Different Storage Media Need Different Strategies

```
    STORAGE TYPE           BEST I/O SCHEDULER
    ============           ==================

    HDD (Rotational)       Deadline / CFQ
    +-------------------+  - Minimize seek time
    | Seek time: 10ms   |  - Reorder to reduce head movement
    | Sequential fast   |  - Starve prevention important
    +-------------------+

    SSD (Flash)            NOOP / Deadline
    +-------------------+  - No seek penalty
    | Seek time: ~0     |  - Don't waste CPU reordering
    | Random ~= Seq     |  - Simple FIFO often best
    +-------------------+

    Virtual (VM guest)     NOOP
    +-------------------+  - Host already schedules
    | Host does I/O sched| - Double scheduling wasteful
    +-------------------+

    Database Workload      Deadline
    +-------------------+  - Latency guarantees needed
    | Latency-sensitive |  - Can't starve reads
    +-------------------+
```

### 2. Scheduler Selection Per Device

```
    SELECTION MECHANISMS:

    BOOT PARAMETER:
    +-------------------------------------------------------+
    | elevator=deadline                                     |
    | (System-wide default)                                 |
    +-------------------------------------------------------+

    SYSFS (PER-DEVICE):
    +-------------------------------------------------------+
    | $ cat /sys/block/sda/queue/scheduler                  |
    | noop [deadline] cfq                                   |
    |                                                       |
    | $ echo cfq > /sys/block/sda/queue/scheduler           |
    | (Change scheduler for specific device)                |
    +-------------------------------------------------------+

    UDEV RULES:
    +-------------------------------------------------------+
    | # Set scheduler based on device type                  |
    | ACTION=="add|change", KERNEL=="sd[a-z]",              |
    |   ATTR{queue/rotational}=="0",                        |
    |   ATTR{queue/scheduler}="noop"                        |
    +-------------------------------------------------------+

    DIFFERENT DEVICES CAN USE DIFFERENT SCHEDULERS
    - /dev/sda (SSD): noop
    - /dev/sdb (HDD): deadline
    - /dev/vda (virtual): noop
```

**中文说明：**

为什么需要策略：(1) 不同存储介质需要不同策略——HDD需要Deadline/CFQ最小化寻道时间，SSD适合NOOP因为没有寻道惩罚，虚拟机适合NOOP因为主机已经调度。(2) 可以为每个设备选择调度器——通过启动参数设置默认、通过sysfs为每个设备单独设置、通过udev规则根据设备类型自动设置。

---

## Minimal C Code Simulation

```c
/*
 * MINIMAL I/O SCHEDULER STRATEGY SIMULATION
 * 
 * Demonstrates how I/O schedulers work as strategies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
struct request;
struct request_queue;
struct elevator_ops;

/* ==========================================================
 * REQUEST STRUCTURE
 * ========================================================== */
struct request {
    unsigned long sector;        /* Starting sector */
    unsigned int nr_sectors;     /* Number of sectors */
    int is_read;                 /* Read or write */
    unsigned long deadline;      /* For deadline scheduler */
    struct request *next;        /* List linkage */
};

/* ==========================================================
 * ELEVATOR OPS: Strategy Interface
 * ========================================================== */
struct elevator_ops {
    const char *name;
    
    /* Initialize scheduler state */
    void *(*init)(struct request_queue *q);
    
    /* Cleanup scheduler state */
    void (*exit)(void *elevator_data);
    
    /* Add request to scheduler queue */
    void (*add_request)(void *elevator_data, struct request *rq);
    
    /* Dispatch next request (return NULL if empty) */
    struct request *(*dispatch)(void *elevator_data);
    
    /* Check if queue is empty */
    int (*queue_empty)(void *elevator_data);
};

/* ==========================================================
 * REQUEST QUEUE
 * ========================================================== */
struct request_queue {
    const char *device_name;
    const struct elevator_ops *elevator;
    void *elevator_data;
    
    unsigned long dispatched;
};

/* ==========================================================
 * NOOP SCHEDULER - Simple FIFO
 * Best for SSDs, virtual devices
 * ========================================================== */

struct noop_data {
    struct request *queue;
    struct request *queue_tail;
};

static void *noop_init(struct request_queue *q)
{
    struct noop_data *nd = calloc(1, sizeof(*nd));
    printf("  [NOOP] Initialized: simple FIFO, no reordering\n");
    return nd;
}

static void noop_exit(void *elevator_data)
{
    free(elevator_data);
    printf("  [NOOP] Exited\n");
}

static void noop_add_request(void *elevator_data, struct request *rq)
{
    struct noop_data *nd = elevator_data;
    
    /* Simply append to tail (FIFO) */
    rq->next = NULL;
    if (nd->queue_tail)
        nd->queue_tail->next = rq;
    else
        nd->queue = rq;
    nd->queue_tail = rq;
    
    printf("  [NOOP] Added request: sector %lu (FIFO order)\n", rq->sector);
}

static struct request *noop_dispatch(void *elevator_data)
{
    struct noop_data *nd = elevator_data;
    struct request *rq;
    
    if (!nd->queue)
        return NULL;
    
    /* Dispatch from head (FIFO) */
    rq = nd->queue;
    nd->queue = rq->next;
    if (!nd->queue)
        nd->queue_tail = NULL;
    
    printf("  [NOOP] Dispatch: sector %lu (no reordering)\n", rq->sector);
    return rq;
}

static int noop_queue_empty(void *elevator_data)
{
    struct noop_data *nd = elevator_data;
    return nd->queue == NULL;
}

static const struct elevator_ops noop_ops = {
    .name = "noop",
    .init = noop_init,
    .exit = noop_exit,
    .add_request = noop_add_request,
    .dispatch = noop_dispatch,
    .queue_empty = noop_queue_empty,
};

/* ==========================================================
 * DEADLINE SCHEDULER - Latency guarantees
 * Prevents starvation, ensures bounded latency
 * ========================================================== */

struct deadline_data {
    struct request *sort_list;      /* Sorted by sector */
    struct request *read_fifo;      /* Reads by deadline */
    struct request *write_fifo;     /* Writes by deadline */
    
    int fifo_batch;                 /* Batching counter */
    int writes_starved;             /* Starvation counter */
};

static void *deadline_init(struct request_queue *q)
{
    struct deadline_data *dd = calloc(1, sizeof(*dd));
    dd->fifo_batch = 0;
    dd->writes_starved = 0;
    printf("  [DEADLINE] Initialized: sector-sorted with deadline guarantees\n");
    return dd;
}

static void deadline_exit(void *elevator_data)
{
    free(elevator_data);
    printf("  [DEADLINE] Exited\n");
}

static void deadline_add_request(void *elevator_data, struct request *rq)
{
    struct deadline_data *dd = elevator_data;
    struct request **pp;
    
    /* Add to sorted list (by sector for seek optimization) */
    pp = &dd->sort_list;
    while (*pp && (*pp)->sector < rq->sector)
        pp = &(*pp)->next;
    rq->next = *pp;
    *pp = rq;
    
    /* Set deadline */
    rq->deadline = 100;  /* Simplified: fixed deadline */
    
    printf("  [DEADLINE] Added request: sector %lu (sorted + deadline)\n", 
           rq->sector);
}

static struct request *deadline_dispatch(void *elevator_data)
{
    struct deadline_data *dd = elevator_data;
    struct request *rq;
    struct request **pp;
    
    if (!dd->sort_list)
        return NULL;
    
    /* Dispatch from sorted list (seek optimization) */
    /* Real deadline scheduler also checks FIFO deadlines */
    rq = dd->sort_list;
    dd->sort_list = rq->next;
    
    printf("  [DEADLINE] Dispatch: sector %lu (seek-optimized order)\n", 
           rq->sector);
    return rq;
}

static int deadline_queue_empty(void *elevator_data)
{
    struct deadline_data *dd = elevator_data;
    return dd->sort_list == NULL;
}

static const struct elevator_ops deadline_ops = {
    .name = "deadline",
    .init = deadline_init,
    .exit = deadline_exit,
    .add_request = deadline_add_request,
    .dispatch = deadline_dispatch,
    .queue_empty = deadline_queue_empty,
};

/* ==========================================================
 * CFQ SCHEDULER - Complete Fair Queuing
 * Fair I/O bandwidth between processes
 * ========================================================== */

struct cfq_data {
    struct request *queue;
    int round_robin_pos;
};

static void *cfq_init(struct request_queue *q)
{
    struct cfq_data *cd = calloc(1, sizeof(*cd));
    printf("  [CFQ] Initialized: fair queuing between processes\n");
    return cd;
}

static void cfq_exit(void *elevator_data)
{
    free(elevator_data);
    printf("  [CFQ] Exited\n");
}

static void cfq_add_request(void *elevator_data, struct request *rq)
{
    struct cfq_data *cd = elevator_data;
    
    /* Simplified: just add to queue */
    /* Real CFQ has per-process queues and time slices */
    rq->next = cd->queue;
    cd->queue = rq;
    
    printf("  [CFQ] Added request: sector %lu (would be per-process queued)\n",
           rq->sector);
}

static struct request *cfq_dispatch(void *elevator_data)
{
    struct cfq_data *cd = elevator_data;
    struct request *rq;
    
    if (!cd->queue)
        return NULL;
    
    /* Simplified: dispatch from queue */
    /* Real CFQ does round-robin between process queues */
    rq = cd->queue;
    cd->queue = rq->next;
    
    printf("  [CFQ] Dispatch: sector %lu (fair sharing)\n", rq->sector);
    return rq;
}

static int cfq_queue_empty(void *elevator_data)
{
    struct cfq_data *cd = elevator_data;
    return cd->queue == NULL;
}

static const struct elevator_ops cfq_ops = {
    .name = "cfq",
    .init = cfq_init,
    .exit = cfq_exit,
    .add_request = cfq_add_request,
    .dispatch = cfq_dispatch,
    .queue_empty = cfq_queue_empty,
};

/* ==========================================================
 * BLOCK LAYER CORE (MECHANISM)
 * ========================================================== */

/* Registry of available schedulers */
static const struct elevator_ops *available_schedulers[] = {
    &noop_ops,
    &deadline_ops,
    &cfq_ops,
    NULL,
};

/* Find elevator by name */
static const struct elevator_ops *elevator_find(const char *name)
{
    for (int i = 0; available_schedulers[i]; i++) {
        if (strcmp(available_schedulers[i]->name, name) == 0)
            return available_schedulers[i];
    }
    return NULL;
}

/* Set elevator for queue */
static int elevator_set(struct request_queue *q, const char *name)
{
    const struct elevator_ops *e = elevator_find(name);
    if (!e) {
        printf("[BLOCK CORE] Unknown scheduler: %s\n", name);
        return -1;
    }
    
    /* Exit old elevator if any */
    if (q->elevator && q->elevator->exit)
        q->elevator->exit(q->elevator_data);
    
    q->elevator = e;
    printf("[BLOCK CORE] Set scheduler to: %s\n", e->name);
    
    /* Initialize new elevator */
    if (e->init)
        q->elevator_data = e->init(q);
    
    return 0;
}

/* Core: Add request to queue */
static void blk_queue_request(struct request_queue *q, struct request *rq)
{
    printf("[BLOCK CORE] blk_queue_request: sector %lu\n", rq->sector);
    
    /* STRATEGY: Let elevator decide where to insert */
    if (q->elevator && q->elevator->add_request)
        q->elevator->add_request(q->elevator_data, rq);
}

/* Core: Get next request to dispatch */
static struct request *blk_peek_request(struct request_queue *q)
{
    struct request *rq = NULL;
    
    /* STRATEGY: Let elevator decide which request */
    if (q->elevator && q->elevator->dispatch)
        rq = q->elevator->dispatch(q->elevator_data);
    
    if (rq) {
        q->dispatched++;
        printf("[BLOCK CORE] Dispatched request #%lu\n", q->dispatched);
    }
    
    return rq;
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */
int main(void)
{
    printf("================================================\n");
    printf("I/O SCHEDULER STRATEGY PATTERN DEMONSTRATION\n");
    printf("================================================\n");

    /* Create request queue for two devices */
    struct request_queue ssd_queue = { .device_name = "/dev/sda (SSD)" };
    struct request_queue hdd_queue = { .device_name = "/dev/sdb (HDD)" };

    /* Set appropriate schedulers */
    printf("\n=== CONFIGURING SCHEDULERS ===\n");
    printf("\nSSD gets NOOP (no seek penalty):\n");
    elevator_set(&ssd_queue, "noop");
    
    printf("\nHDD gets DEADLINE (seek optimization):\n");
    elevator_set(&hdd_queue, "deadline");

    /* Create test requests (random sectors to show reordering) */
    struct request reqs[] = {
        { .sector = 1000, .nr_sectors = 8 },
        { .sector = 100,  .nr_sectors = 8 },
        { .sector = 5000, .nr_sectors = 8 },
        { .sector = 200,  .nr_sectors = 8 },
    };

    /* === SSD with NOOP === */
    printf("\n=== SSD WITH NOOP SCHEDULER ===\n");
    printf("\nAdding requests (random sector order):\n");
    for (int i = 0; i < 4; i++) {
        struct request *rq = malloc(sizeof(*rq));
        *rq = reqs[i];
        blk_queue_request(&ssd_queue, rq);
    }
    
    printf("\nDispatching (FIFO - same order as added):\n");
    while (!ssd_queue.elevator->queue_empty(ssd_queue.elevator_data)) {
        struct request *rq = blk_peek_request(&ssd_queue);
        if (rq) free(rq);
    }

    /* === HDD with DEADLINE === */
    printf("\n=== HDD WITH DEADLINE SCHEDULER ===\n");
    printf("\nAdding requests (random sector order):\n");
    for (int i = 0; i < 4; i++) {
        struct request *rq = malloc(sizeof(*rq));
        *rq = reqs[i];
        blk_queue_request(&hdd_queue, rq);
    }
    
    printf("\nDispatching (Sorted by sector - minimizes seeks):\n");
    while (!hdd_queue.elevator->queue_empty(hdd_queue.elevator_data)) {
        struct request *rq = blk_peek_request(&hdd_queue);
        if (rq) free(rq);
    }

    printf("\n================================================\n");
    printf("KEY OBSERVATIONS:\n");
    printf("1. NOOP dispatched in arrival order (FIFO)\n");
    printf("2. DEADLINE dispatched in sector order (seek-optimized)\n");
    printf("3. Different devices use different schedulers\n");
    printf("4. Block core doesn't know scheduling algorithm details\n");
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

    1. REQUEST ORDERING
       +-------------------------------------------------------+
       | NOOP: FIFO order (arrival order)                      |
       | Deadline: Sector-sorted (minimize seek)               |
       | CFQ: Round-robin between process queues               |
       +-------------------------------------------------------+

    2. MERGE POLICY
       +-------------------------------------------------------+
       | Which requests can be merged?                         |
       | Front merge vs back merge decisions                   |
       | Each scheduler has different merge logic              |
       +-------------------------------------------------------+

    3. BATCHING STRATEGY
       +-------------------------------------------------------+
       | How many requests to dispatch at once?                |
       | When to switch between reads and writes?              |
       | Deadline: read batches, write batches                 |
       +-------------------------------------------------------+

    4. FAIRNESS POLICY
       +-------------------------------------------------------+
       | CFQ: Fair bandwidth between processes                 |
       | Deadline: No fairness (latency-focused)               |
       | NOOP: No fairness (pure FIFO)                         |
       +-------------------------------------------------------+

    5. STARVATION PREVENTION
       +-------------------------------------------------------+
       | Deadline: FIFO with expiration prevents starvation    |
       | CFQ: Time slices prevent starvation                   |
       | NOOP: No starvation prevention                        |
       +-------------------------------------------------------+

    THE CORE ONLY PROVIDES:
    - Request queue infrastructure
    - Request allocation/deallocation
    - Hardware dispatch interface
    - Completion handling
```

**中文说明：**

核心不控制的内容：(1) 请求顺序——NOOP用FIFO，Deadline用扇区排序，CFQ用进程间轮询；(2) 合并策略——哪些请求可以合并、前合并还是后合并；(3) 批处理策略——一次分发多少请求、何时在读写之间切换；(4) 公平策略——CFQ有进程间公平，Deadline和NOOP没有；(5) 防饥饿——Deadline用FIFO加过期时间，CFQ用时间片，NOOP无防饥饿。核心只提供：请求队列基础设施、请求分配/释放、硬件分发接口、完成处理。

---

## Real Kernel Code Reference (v3.2)

### struct elevator_ops in include/linux/elevator.h

```c
struct elevator_ops {
    elevator_merge_fn *elevator_merge_fn;
    elevator_merged_fn *elevator_merged_fn;
    elevator_dispatch_fn *elevator_dispatch_fn;
    elevator_add_req_fn *elevator_add_req_fn;
    /* ... more operations ... */
};
```

### Sysfs interface in block/elevator.c

```c
/* Change elevator via sysfs */
static ssize_t elv_iosched_store(struct request_queue *q, 
                                  const char *name, size_t count)
{
    elevator_switch(q, name);
    return count;
}
```

---

## Key Takeaways

1. **Complete algorithm encapsulation**: Each elevator is a complete I/O ordering policy
2. **Per-device selection**: Different devices can use different schedulers
3. **Runtime switchable**: Scheduler can be changed via sysfs while running
4. **Self-contained state**: Each scheduler maintains its own queues and data structures
5. **No core involvement in ordering**: Core only triggers add/dispatch, scheduler decides order
