# Case 4: work_struct in Workqueues

## Subsystem Background

```
+=============================================================================+
|                    WORKQUEUE AND WORK_STRUCT                                 |
+=============================================================================+

    WORKQUEUE: Deferred Execution Mechanism
    =======================================

    Problem: Some work cannot be done in interrupt context.
             Need to defer to a kernel thread.

    Solution: Workqueues
              - Queue work items
              - Worker threads execute them later
              - Works in process context (can sleep)


    WORK_STRUCT EMBEDDING:
    ======================

    struct work_struct {
        atomic_long_t data;
        struct list_head entry;
        work_func_t func;        /* The function to execute */
    };

    struct my_device {
        /* ... device fields ... */
        struct work_struct work;  /* EMBEDDED work item */
        /* ... more fields ... */
    };

    WORKFLOW:
    =========

    1. my_device embeds work_struct
    2. work_struct.func is set to a handler function
    3. Handler receives work_struct pointer
    4. container_of recovers the my_device
```

**中文说明：**

工作队列是Linux内核的延迟执行机制。某些工作不能在中断上下文中完成，需要延迟到内核线程。work_struct被嵌入到用户结构体中，当工作执行时，处理函数接收work_struct指针，通过container_of恢复到外层结构体。

---

## container_of in Workqueue Callbacks

```
    THE CALLBACK PATTERN:
    =====================

    struct my_device {
        char name[32];
        int pending_work;
        struct work_struct work;   /* EMBEDDED */
    };

    /* Work handler - receives work_struct pointer */
    void my_work_handler(struct work_struct *work)
    {
        /* Recover my_device from work_struct */
        struct my_device *dev = container_of(work, struct my_device, work);
        
        /* Now can access dev->name, dev->pending_work, etc. */
        printf("Processing work for device: %s\n", dev->name);
        dev->pending_work = 0;
    }

    /* Setup */
    void init_my_device(struct my_device *dev)
    {
        INIT_WORK(&dev->work, my_work_handler);
    }

    /* Schedule work */
    void trigger_work(struct my_device *dev)
    {
        dev->pending_work = 1;
        schedule_work(&dev->work);
    }
```

**中文说明：**

工作队列回调模式：work_struct嵌入在用户结构体中，INIT_WORK设置处理函数，schedule_work将工作项加入队列。当工作执行时，处理函数接收work_struct指针，通过container_of恢复到外层结构体访问设备状态。

---

## Minimal C Code Simulation

```c
/*
 * CONTAINER_OF WITH WORK_STRUCT SIMULATION
 * Demonstrates deferred work pattern
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/* ==========================================================
 * KERNEL-STYLE DEFINITIONS
 * ========================================================== */

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* ==========================================================
 * WORK_STRUCT AND WORKQUEUE (Simplified)
 * ========================================================== */

struct work_struct;
typedef void (*work_func_t)(struct work_struct *work);

struct work_struct {
    work_func_t func;
    struct work_struct *next;  /* For queue linking */
    int pending;
};

#define INIT_WORK(work, handler) do {   \
    (work)->func = (handler);           \
    (work)->next = NULL;                \
    (work)->pending = 0;                \
} while (0)

/* Simple workqueue (single-threaded for demo) */
struct workqueue {
    struct work_struct *head;
    struct work_struct *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int running;
    pthread_t worker;
};

static struct workqueue global_wq;

void queue_work(struct workqueue *wq, struct work_struct *work)
{
    pthread_mutex_lock(&wq->lock);
    
    if (work->pending) {
        pthread_mutex_unlock(&wq->lock);
        return;  /* Already queued */
    }
    
    work->pending = 1;
    work->next = NULL;
    
    if (wq->tail) {
        wq->tail->next = work;
        wq->tail = work;
    } else {
        wq->head = wq->tail = work;
    }
    
    pthread_cond_signal(&wq->cond);
    pthread_mutex_unlock(&wq->lock);
}

void schedule_work(struct work_struct *work)
{
    queue_work(&global_wq, work);
}

void *worker_thread(void *arg)
{
    struct workqueue *wq = arg;
    
    while (wq->running) {
        pthread_mutex_lock(&wq->lock);
        
        while (!wq->head && wq->running) {
            pthread_cond_wait(&wq->cond, &wq->lock);
        }
        
        if (!wq->running) {
            pthread_mutex_unlock(&wq->lock);
            break;
        }
        
        struct work_struct *work = wq->head;
        wq->head = work->next;
        if (!wq->head) wq->tail = NULL;
        work->pending = 0;
        
        pthread_mutex_unlock(&wq->lock);
        
        /* Execute work */
        printf("[WORKER] Executing work at %p\n", (void *)work);
        work->func(work);
    }
    
    return NULL;
}

void init_workqueue(void)
{
    global_wq.head = NULL;
    global_wq.tail = NULL;
    global_wq.running = 1;
    pthread_mutex_init(&global_wq.lock, NULL);
    pthread_cond_init(&global_wq.cond, NULL);
    pthread_create(&global_wq.worker, NULL, worker_thread, &global_wq);
}

void destroy_workqueue(void)
{
    pthread_mutex_lock(&global_wq.lock);
    global_wq.running = 0;
    pthread_cond_signal(&global_wq.cond);
    pthread_mutex_unlock(&global_wq.lock);
    pthread_join(global_wq.worker, NULL);
    pthread_mutex_destroy(&global_wq.lock);
    pthread_cond_destroy(&global_wq.cond);
}

/* ==========================================================
 * USER STRUCTURE: Network Device with Deferred Work
 * ========================================================== */

struct net_device {
    char name[16];
    int rx_packets;
    int tx_packets;
    int link_status;
    
    /* Embedded work items for deferred processing */
    struct work_struct rx_work;     /* Deferred RX processing */
    struct work_struct link_work;   /* Deferred link handling */
};

/* ==========================================================
 * WORK HANDLERS - Use container_of to get device
 * ========================================================== */

void rx_work_handler(struct work_struct *work)
{
    /* 
     * We receive work_struct pointer.
     * Need to get back to net_device to access rx_packets, etc.
     */
    struct net_device *dev = container_of(work, struct net_device, rx_work);
    
    printf("[RX_WORK] Processing RX for device '%s'\n", dev->name);
    printf("  work_struct at:  %p\n", (void *)work);
    printf("  net_device at:   %p\n", (void *)dev);
    printf("  offsetof(rx_work) = %zu\n", 
           offsetof(struct net_device, rx_work));
    
    /* Simulate processing */
    dev->rx_packets += 100;
    printf("  Processed packets, rx_packets now = %d\n", dev->rx_packets);
}

void link_work_handler(struct work_struct *work)
{
    struct net_device *dev = container_of(work, struct net_device, link_work);
    
    printf("[LINK_WORK] Handling link change for '%s'\n", dev->name);
    printf("  work_struct at:  %p\n", (void *)work);
    printf("  net_device at:   %p\n", (void *)dev);
    printf("  offsetof(link_work) = %zu\n", 
           offsetof(struct net_device, link_work));
    
    /* Simulate link handling */
    dev->link_status = !dev->link_status;
    printf("  Link status now = %s\n", 
           dev->link_status ? "UP" : "DOWN");
}

/* ==========================================================
 * DEVICE OPERATIONS
 * ========================================================== */

struct net_device *alloc_netdev(const char *name)
{
    struct net_device *dev = malloc(sizeof(*dev));
    if (!dev) return NULL;
    
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->rx_packets = 0;
    dev->tx_packets = 0;
    dev->link_status = 0;
    
    /* Initialize work items with handlers */
    INIT_WORK(&dev->rx_work, rx_work_handler);
    INIT_WORK(&dev->link_work, link_work_handler);
    
    return dev;
}

/* Simulate interrupt - schedules deferred work */
void simulate_rx_interrupt(struct net_device *dev)
{
    printf("\n[IRQ] RX interrupt on '%s' - scheduling deferred work\n", 
           dev->name);
    schedule_work(&dev->rx_work);
}

void simulate_link_interrupt(struct net_device *dev)
{
    printf("\n[IRQ] Link interrupt on '%s' - scheduling deferred work\n", 
           dev->name);
    schedule_work(&dev->link_work);
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("CONTAINER_OF WITH WORK_STRUCT DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    init_workqueue();
    
    /* Create network device */
    printf("--- Creating network device ---\n");
    struct net_device *eth0 = alloc_netdev("eth0");
    
    printf("net_device '%s' at: %p\n", eth0->name, (void *)eth0);
    printf("  .rx_work at:   %p (offset %zu)\n", 
           (void *)&eth0->rx_work,
           offsetof(struct net_device, rx_work));
    printf("  .link_work at: %p (offset %zu)\n", 
           (void *)&eth0->link_work,
           offsetof(struct net_device, link_work));
    
    /* Simulate interrupts that trigger deferred work */
    printf("\n--- Simulating interrupts ---\n");
    
    simulate_rx_interrupt(eth0);
    usleep(100000);  /* Let worker process */
    
    simulate_link_interrupt(eth0);
    usleep(100000);
    
    simulate_rx_interrupt(eth0);
    usleep(100000);
    
    /* Show final state */
    printf("\n--- Final device state ---\n");
    printf("eth0: rx_packets=%d, tx_packets=%d, link=%s\n",
           eth0->rx_packets, eth0->tx_packets,
           eth0->link_status ? "UP" : "DOWN");
    
    /* Demonstrate container_of explicitly */
    printf("\n--- container_of demonstration ---\n");
    struct work_struct *w = &eth0->rx_work;
    printf("Given work_struct at %p\n", (void *)w);
    printf("container_of calculation:\n");
    printf("  %p - %zu = %p\n",
           (void *)w,
           offsetof(struct net_device, rx_work),
           (void *)container_of(w, struct net_device, rx_work));
    printf("Result matches eth0? %s\n",
           container_of(w, struct net_device, rx_work) == eth0 ? "YES" : "NO");
    
    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    destroy_workqueue();
    free(eth0);
    
    printf("\n=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- work_struct is embedded in device structure\n");
    printf("- Work handler receives only work_struct pointer\n");
    printf("- container_of recovers the device to access state\n");
    printf("- Same device can have multiple work_struct members\n");
    printf("- This pattern enables deferred execution with context\n");
    printf("=================================================\n");
    
    return 0;
}
```

To compile: `gcc -o work_demo work_demo.c -pthread`

---

## Multiple Work Items

```
+=============================================================================+
|              MULTIPLE WORK ITEMS IN ONE STRUCTURE                            |
+=============================================================================+

    struct complex_device {
        char name[32];
        
        /* Multiple embedded work items */
        struct work_struct init_work;     /* For initialization */
        struct work_struct rx_work;       /* For RX processing */
        struct work_struct tx_work;       /* For TX processing */
        struct work_struct error_work;    /* For error handling */
        struct work_struct cleanup_work;  /* For cleanup */
    };

    Each work handler uses container_of with the CORRECT member name:

    void init_handler(struct work_struct *work)
    {
        struct complex_device *dev = 
            container_of(work, struct complex_device, init_work);
    }

    void rx_handler(struct work_struct *work)
    {
        struct complex_device *dev = 
            container_of(work, struct complex_device, rx_work);
    }

    SAME device structure, DIFFERENT offsets, SAME result!
```

---

## Real Kernel Examples

### Network Driver (drivers/net/*)

```c
struct e1000_adapter {
    /* ... */
    struct work_struct reset_task;
    struct work_struct watchdog_task;
    /* ... */
};

static void e1000_reset_task(struct work_struct *work)
{
    struct e1000_adapter *adapter = 
        container_of(work, struct e1000_adapter, reset_task);
    /* Now can access adapter->netdev, adapter->hw, etc. */
}
```

### USB Core (drivers/usb/core/*)

```c
struct usb_device {
    /* ... */
    struct work_struct autosuspend_work;
    struct work_struct autoresume_work;
    /* ... */
};
```

---

## Key Takeaways

1. **work_struct carries no user data**: Only function pointer and queue linkage
2. **Embedding provides context**: Device state is in the containing structure
3. **Handler uses container_of**: From work_struct back to device
4. **Multiple work items possible**: Same struct can have many work_struct members
5. **Pattern enables deferred execution**: With full access to device context
