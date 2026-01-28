# Unified Observer/Notifier Skeleton

A generic C skeleton capturing the Observer/Notifier pattern as used in the Linux kernel.

---

## Complete Skeleton

```c
/*
 * Generic Observer/Notifier Pattern Skeleton
 * Based on Linux kernel notifier chain implementation
 */

#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * PART 1: NOTIFIER INFRASTRUCTURE
 * ================================================================ */

/* Return values from notifier callbacks */
#define NOTIFY_DONE     0x0000  /* Continue, not interested */
#define NOTIFY_OK       0x0001  /* Continue, processed */
#define NOTIFY_STOP     0x8000  /* Stop chain */
#define NOTIFY_BAD      (NOTIFY_STOP | 0x0002)  /* Stop, error */

/* Convert notifier return to errno */
#define NOTIFY_STOP_MASK  0x8000
#define notifier_to_errno(ret) \
    ((ret) & NOTIFY_STOP_MASK ? -((ret) & ~NOTIFY_STOP_MASK) : 0)

/* Notifier block - represents one subscriber */
struct notifier_block {
    int (*notifier_call)(struct notifier_block *nb,
                         unsigned long event,
                         void *data);
    struct notifier_block *next;
    int priority;  /* Higher = called first */
};

/* Notifier head - represents the chain */
struct notifier_head {
    struct notifier_block *head;
    /* In kernel: also has lock (rwsem, spinlock, or RCU) */
};

/* Initialize a notifier head */
#define NOTIFIER_HEAD_INIT { .head = NULL }
#define DEFINE_NOTIFIER_HEAD(name) \
    struct notifier_head name = NOTIFIER_HEAD_INIT

/* ================================================================
 * PART 2: CHAIN OPERATIONS
 * ================================================================ */

/*
 * Register a notifier to the chain
 * Inserts by priority (higher priority = earlier in list)
 */
int notifier_chain_register(struct notifier_head *nh,
                            struct notifier_block *nb)
{
    struct notifier_block **p;
    
    /* Find insertion point by priority */
    for (p = &nh->head; *p != NULL; p = &(*p)->next) {
        if (nb->priority > (*p)->priority)
            break;
    }
    
    /* Insert into list */
    nb->next = *p;
    *p = nb;
    
    return 0;
}

/*
 * Unregister a notifier from the chain
 */
int notifier_chain_unregister(struct notifier_head *nh,
                              struct notifier_block *nb)
{
    struct notifier_block **p;
    
    for (p = &nh->head; *p != NULL; p = &(*p)->next) {
        if (*p == nb) {
            *p = nb->next;
            return 0;
        }
    }
    return -1;  /* Not found */
}

/*
 * Call all notifiers in the chain
 * Returns: NOTIFY_OK if all OK, or first error
 */
int notifier_call_chain(struct notifier_head *nh,
                        unsigned long event,
                        void *data)
{
    struct notifier_block *nb;
    int ret = NOTIFY_DONE;
    
    for (nb = nh->head; nb != NULL; nb = nb->next) {
        ret = nb->notifier_call(nb, event, data);
        
        /* Check if chain should stop */
        if (ret & NOTIFY_STOP_MASK)
            break;
    }
    
    return ret;
}

/* ================================================================
 * PART 3: EVENT SOURCE (PUBLISHER)
 * ================================================================ */

/* Define events for this subsystem */
enum my_events {
    MY_EVENT_CREATED = 1,
    MY_EVENT_MODIFIED = 2,
    MY_EVENT_DESTROYED = 3,
};

/* The notifier chain for this subsystem */
DEFINE_NOTIFIER_HEAD(my_notifier_chain);

/* Public API for subscribers */
int register_my_notifier(struct notifier_block *nb)
{
    return notifier_chain_register(&my_notifier_chain, nb);
}

int unregister_my_notifier(struct notifier_block *nb)
{
    return notifier_chain_unregister(&my_notifier_chain, nb);
}

/* Internal function to broadcast events */
static int call_my_notifiers(unsigned long event, void *data)
{
    return notifier_call_chain(&my_notifier_chain, event, data);
}

/* ================================================================
 * PART 4: EXAMPLE EVENT SOURCE
 * ================================================================ */

struct my_object {
    int id;
    char name[32];
};

/* Create object - broadcasts CREATED event */
struct my_object *create_my_object(int id, const char *name)
{
    struct my_object *obj = malloc(sizeof(*obj));
    if (!obj)
        return NULL;
    
    obj->id = id;
    snprintf(obj->name, sizeof(obj->name), "%s", name);
    
    /* Notify all subscribers */
    call_my_notifiers(MY_EVENT_CREATED, obj);
    
    return obj;
}

/* Modify object - broadcasts MODIFIED event */
void modify_my_object(struct my_object *obj, const char *new_name)
{
    snprintf(obj->name, sizeof(obj->name), "%s", new_name);
    
    /* Notify all subscribers */
    call_my_notifiers(MY_EVENT_MODIFIED, obj);
}

/* Destroy object - broadcasts DESTROYED event */
void destroy_my_object(struct my_object *obj)
{
    /* Notify all subscribers BEFORE destruction */
    call_my_notifiers(MY_EVENT_DESTROYED, obj);
    
    free(obj);
}

/* ================================================================
 * PART 5: EXAMPLE SUBSCRIBERS
 * ================================================================ */

/* Subscriber 1: Logger */
int logger_notify(struct notifier_block *nb,
                  unsigned long event, void *data)
{
    struct my_object *obj = data;
    
    switch (event) {
    case MY_EVENT_CREATED:
        printf("[LOGGER] Object %d created: %s\n", obj->id, obj->name);
        break;
    case MY_EVENT_MODIFIED:
        printf("[LOGGER] Object %d modified: %s\n", obj->id, obj->name);
        break;
    case MY_EVENT_DESTROYED:
        printf("[LOGGER] Object %d destroyed\n", obj->id);
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block logger_nb = {
    .notifier_call = logger_notify,
    .priority = 100,  /* High priority - log first */
};

/* Subscriber 2: Statistics */
static int stats_create_count = 0;
static int stats_destroy_count = 0;

int stats_notify(struct notifier_block *nb,
                 unsigned long event, void *data)
{
    switch (event) {
    case MY_EVENT_CREATED:
        stats_create_count++;
        break;
    case MY_EVENT_DESTROYED:
        stats_destroy_count++;
        break;
    }
    return NOTIFY_OK;
}

static struct notifier_block stats_nb = {
    .notifier_call = stats_notify,
    .priority = 0,  /* Default priority */
};

/* ================================================================
 * PART 6: USAGE EXAMPLE
 * ================================================================ */

int main(void)
{
    struct my_object *obj1, *obj2;
    
    printf("=== OBSERVER/NOTIFIER PATTERN SKELETON ===\n\n");
    
    /* Subscribe to events */
    printf("--- Registering Subscribers ---\n");
    register_my_notifier(&logger_nb);
    register_my_notifier(&stats_nb);
    
    /* Generate events */
    printf("\n--- Creating Objects ---\n");
    obj1 = create_my_object(1, "First");
    obj2 = create_my_object(2, "Second");
    
    printf("\n--- Modifying Objects ---\n");
    modify_my_object(obj1, "First-Modified");
    
    printf("\n--- Destroying Objects ---\n");
    destroy_my_object(obj1);
    destroy_my_object(obj2);
    
    /* Show stats */
    printf("\n--- Statistics ---\n");
    printf("Created: %d, Destroyed: %d\n", 
           stats_create_count, stats_destroy_count);
    
    /* Cleanup */
    unregister_my_notifier(&logger_nb);
    unregister_my_notifier(&stats_nb);
    
    return 0;
}
```

---

## Structure Diagram

```
+=============================================================================+
|                    NOTIFIER SKELETON STRUCTURE                               |
+=============================================================================+

    +------------------+
    | notifier_head    |
    +------------------+
    | head --------+   |
    +--------------|---+
                   |
                   v
    +------------------+     +------------------+     +------------------+
    | notifier_block   | --> | notifier_block   | --> | notifier_block   |
    +------------------+     +------------------+     +------------------+
    | notifier_call    |     | notifier_call    |     | notifier_call    |
    | next         ---------+| next         ---------+| next = NULL      |
    | priority = 100   |     | priority = 50    |     | priority = 0     |
    +------------------+     +------------------+     +------------------+
           |                        |                        |
           v                        v                        v
      logger_notify()         cache_notify()         stats_notify()


    CALL FLOW:
    ==========

    event_source()
         |
         | call_my_notifiers(event, data)
         v
    notifier_call_chain()
         |
         +---> nb1->notifier_call() --> NOTIFY_OK
         |
         +---> nb2->notifier_call() --> NOTIFY_OK
         |
         +---> nb3->notifier_call() --> NOTIFY_OK
         |
         v
    return NOTIFY_OK
```

**中文说明：**

通知器骨架结构：notifier_head是链表头，包含指向第一个notifier_block的指针。每个notifier_block包含回调函数指针、next指针、优先级。调用流程：事件源调用call_my_notifiers，内部调用notifier_call_chain遍历链表，依次调用每个订阅者的回调函数。

---

## Mapping to Kernel Cases

```
    SKELETON ELEMENT          BLOCKING NOTIFIER     NETDEV NOTIFIER
    ================          =================     ===============
    
    struct notifier_head      blocking_notifier_    RAW_NOTIFIER_
                              HEAD                  HEAD
    
    notifier_chain_register   blocking_notifier_    raw_notifier_
                              chain_register        chain_register
    
    notifier_call_chain       blocking_notifier_    raw_notifier_
                              call_chain            call_chain
    
    my_notifier_chain         reboot_notifier_      netdev_chain
                              list
    
    MY_EVENT_*                SYS_RESTART, etc.     NETDEV_UP, etc.
    
    register_my_notifier      register_reboot_      register_netdevice_
                              notifier              notifier
```

---

## Key Implementation Points

```
    1. PRIORITY ORDERING
    ====================
    - Higher priority = earlier in chain
    - Insert in sorted order during registration
    - Allows control over callback order
    
    2. RETURN VALUE PROTOCOL
    ========================
    - NOTIFY_DONE: Not interested, continue
    - NOTIFY_OK: Processed, continue
    - NOTIFY_STOP: Stop chain (no more callbacks)
    - NOTIFY_BAD: Stop chain, error occurred
    
    3. LOCKING (in real kernel)
    ===========================
    - BLOCKING: rwsem protects chain
    - ATOMIC: RCU protects chain
    - RAW: Caller manages locking
    
    4. DATA PASSING
    ===============
    - Event type via unsigned long
    - Context data via void pointer
    - Subscribers cast to correct type
```

---

## Adapting for Real Projects

```c
/* For single-threaded applications: use as-is */

/* For multi-threaded applications: add locking */
struct notifier_head_threadsafe {
    pthread_rwlock_t lock;
    struct notifier_block *head;
};

int notifier_chain_register_safe(struct notifier_head_threadsafe *nh,
                                 struct notifier_block *nb)
{
    pthread_rwlock_wrlock(&nh->lock);
    /* ... registration logic ... */
    pthread_rwlock_unlock(&nh->lock);
    return 0;
}

int notifier_call_chain_safe(struct notifier_head_threadsafe *nh,
                             unsigned long event, void *data)
{
    pthread_rwlock_rdlock(&nh->lock);
    /* ... call chain logic ... */
    pthread_rwlock_unlock(&nh->lock);
    return ret;
}
```

---

## Version

Based on **Linux kernel v3.2** notifier chain implementation.

Key source files:
- `include/linux/notifier.h` - data structures and macros
- `kernel/notifier.c` - chain operations implementation
