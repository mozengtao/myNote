# Observer Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                     OBSERVER PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    +-------------------+         +-------------------+            |
|    |     Subject       |         |     Observer      |            |
|    +-------------------+  notifies  +-------------------+            |
|    | - observers[]     |-------->| + update(event)   |            |
|    | + attach(obs)     |         +--------+----------+            |
|    | + detach(obs)     |                  ^                       |
|    | + notify()        |                  |                       |
|    +-------------------+         +--------+----------+            |
|                                  |        |          |            |
|                                  v        v          v            |
|                            +------+ +------+ +------+             |
|                            |Obs A | |Obs B | |Obs C |             |
|                            +------+ +------+ +------+             |
|                            |update| |update| |update|             |
|                            +------+ +------+ +------+             |
|                                                                   |
|    Subject notifies all registered observers when state changes   |
|    Observers can be added/removed at runtime                      |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 观察者模式定义对象间依赖关系，被观察者状态变化时自动通知所有观察者。在Linux内核中，通知链(notifier chain)是观察者模式的典型实现。内核子系统（如电源管理、网络设备）发生事件时，通过通知链通知所有注册的监听者。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Notifier Chains

```c
/* From: include/linux/notifier.h */

/**
 * struct notifier_block - Observer structure
 *
 * Represents an observer that wants to be notified of events.
 */
struct notifier_block {
    /* Callback function - called when event occurs */
    int (*notifier_call)(struct notifier_block *nb,
                         unsigned long action, void *data);
    struct notifier_block __rcu *next;  /* Next observer in chain */
    int priority;                        /* Notification priority */
};

/**
 * struct atomic_notifier_head - Subject (for atomic context)
 *
 * Maintains list of observers, notifies them on events.
 */
struct atomic_notifier_head {
    spinlock_t lock;
    struct notifier_block __rcu *head;  /* List of observers */
};

/**
 * struct blocking_notifier_head - Subject (for process context)
 */
struct blocking_notifier_head {
    struct rw_semaphore rwsem;
    struct notifier_block __rcu *head;
};

/* Macros to initialize notifier heads */
#define ATOMIC_NOTIFIER_HEAD(name) \
    struct atomic_notifier_head name = ATOMIC_NOTIFIER_INIT(name)

#define BLOCKING_NOTIFIER_HEAD(name) \
    struct blocking_notifier_head name = BLOCKING_NOTIFIER_INIT(name)
```

### 2.2 Kernel Example: Notifier Chain Operations

```c
/* From: kernel/notifier.c */

/**
 * notifier_chain_register - Attach observer to subject
 * @nl: Pointer to head of notifier chain
 * @n: New observer to add
 *
 * Adds observer to chain, sorted by priority.
 */
static int notifier_chain_register(struct notifier_block **nl,
                                   struct notifier_block *n)
{
    while ((*nl) != NULL) {
        if (n->priority > (*nl)->priority)
            break;
        nl = &((*nl)->next);
    }
    n->next = *nl;
    rcu_assign_pointer(*nl, n);
    return 0;
}

/**
 * notifier_chain_unregister - Detach observer from subject
 * @nl: Pointer to head of notifier chain
 * @n: Observer to remove
 */
static int notifier_chain_unregister(struct notifier_block **nl,
                                     struct notifier_block *n)
{
    while ((*nl) != NULL) {
        if ((*nl) == n) {
            rcu_assign_pointer(*nl, n->next);
            return 0;
        }
        nl = &((*nl)->next);
    }
    return -ENOENT;
}

/**
 * notifier_call_chain - Notify all observers
 * @nl: Pointer to head of notifier chain
 * @val: Event value
 * @v: Event data
 * @nr_to_call: Max observers to notify (-1 for all)
 * @nr_calls: Output - number of observers called
 *
 * Walks the chain, calling each observer's notifier_call().
 */
static int notifier_call_chain(struct notifier_block **nl,
                               unsigned long val, void *v,
                               int nr_to_call, int *nr_calls)
{
    int ret = NOTIFY_DONE;
    struct notifier_block *nb, *next_nb;

    nb = rcu_dereference_raw(*nl);

    while (nb && nr_to_call) {
        next_nb = rcu_dereference_raw(nb->next);
        
        /* Call observer's callback */
        ret = nb->notifier_call(nb, val, v);

        if (nr_calls)
            (*nr_calls)++;

        /* Stop if observer requests it */
        if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
            break;
        nb = next_nb;
        nr_to_call--;
    }
    return ret;
}

/**
 * atomic_notifier_chain_register - Register atomic notifier
 */
int atomic_notifier_chain_register(struct atomic_notifier_head *nh,
                                   struct notifier_block *n)
{
    unsigned long flags;
    int ret;

    spin_lock_irqsave(&nh->lock, flags);
    ret = notifier_chain_register(&nh->head, n);
    spin_unlock_irqrestore(&nh->lock, flags);
    return ret;
}

/**
 * atomic_notifier_call_chain - Call atomic notifier chain
 */
int atomic_notifier_call_chain(struct atomic_notifier_head *nh,
                               unsigned long val, void *v)
{
    int ret;

    rcu_read_lock();
    ret = notifier_call_chain(&nh->head, val, v, -1, NULL);
    rcu_read_unlock();
    return ret;
}
```

### 2.3 Kernel Example: Reboot Notifier Usage

```c
/* From: kernel/notifier.c */

/* Subject: Reboot notifier chain */
BLOCKING_NOTIFIER_HEAD(reboot_notifier_list);

/**
 * register_reboot_notifier - Register observer for reboot events
 * @nb: Notifier block to register
 *
 * Registers a function to be called when system reboots.
 */
int register_reboot_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&reboot_notifier_list, nb);
}

/**
 * unregister_reboot_notifier - Unregister reboot observer
 */
int unregister_reboot_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&reboot_notifier_list, nb);
}
```

### 2.4 Kernel Example: ACPI Notifier

```c
/* From: drivers/acpi/event.c */

/* Subject: ACPI event notifier chain */
static BLOCKING_NOTIFIER_HEAD(acpi_chain_head);

/**
 * acpi_notifier_call_chain - Notify observers of ACPI event
 * @dev: ACPI device
 * @type: Event type
 * @data: Event data
 */
int acpi_notifier_call_chain(struct acpi_device *dev, u32 type, u32 data)
{
    struct acpi_bus_event event;

    strcpy(event.device_class, dev->pnp.device_class);
    strcpy(event.bus_id, dev->pnp.bus_id);
    event.type = type;
    event.data = data;
    
    /* Notify all observers */
    return (blocking_notifier_call_chain(&acpi_chain_head, 0, (void *)&event)
            == NOTIFY_BAD) ? -EINVAL : 0;
}

/**
 * register_acpi_notifier - Register observer for ACPI events
 */
int register_acpi_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_register(&acpi_chain_head, nb);
}

int unregister_acpi_notifier(struct notifier_block *nb)
{
    return blocking_notifier_chain_unregister(&acpi_chain_head, nb);
}
```

### 2.5 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL OBSERVER PATTERN                       |
|                   (Notifier Chains)                               |
+------------------------------------------------------------------+
|                                                                   |
|    Subject (Notifier Head)                                        |
|    +---------------------------+                                  |
|    | reboot_notifier_list      |                                  |
|    +---------------------------+                                  |
|    | head ----+                |                                  |
|    +----------+----------------+                                  |
|               |                                                   |
|               v                                                   |
|    +----------+--------+     +------------------+                 |
|    | notifier_block    |---->| notifier_block   |----> ...        |
|    +-------------------+     +------------------+                 |
|    | notifier_call     |     | notifier_call    |                 |
|    | = driver_shutdown |     | = fs_sync        |                 |
|    | priority = 100    |     | priority = 50    |                 |
|    +-------------------+     +------------------+                 |
|                                                                   |
|    Event: System Reboot                                           |
|    +-------------------+                                          |
|    | kernel_restart()  |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|             v                                                     |
|    +--------+------------------+                                  |
|    | blocking_notifier_call_   |                                  |
|    | chain(&reboot_notifier_   |                                  |
|    | list, SYS_RESTART, NULL)  |                                  |
|    +--------+------------------+                                  |
|             |                                                     |
|    +--------+--------+--------+                                   |
|    |        |        |        |                                   |
|    v        v        v        v                                   |
| +------+ +------+ +------+ +------+                               |
| |call 1| |call 2| |call 3| |call 4|  (Notify all observers)       |
| +------+ +------+ +------+ +------+                               |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux内核的通知链是观察者模式的经典实现。以重启通知为例：reboot_notifier_list是被观察者（Subject），维护一个notifier_block链表。各驱动和子系统通过register_reboot_notifier()注册观察者。当系统重启时，内核调用blocking_notifier_call_chain()遍历链表，依次调用每个观察者的notifier_call回调函数。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Loose Coupling** | Subject and observers are loosely coupled |
| **Dynamic Registration** | Observers can be added/removed at runtime |
| **Broadcast Communication** | One event notifies many observers |
| **Priority Support** | Observers called in priority order |
| **Extensibility** | New observers don't require subject changes |
| **Modularity** | Each observer handles events independently |

**中文说明：** 观察者模式的优势包括：松耦合（被观察者和观察者松散耦合）、动态注册（运行时添加/移除观察者）、广播通信（一个事件通知多个观察者）、优先级支持（按优先级顺序调用观察者）、可扩展性（添加新观察者无需修改被观察者）、模块化（每个观察者独立处理事件）。

---

## 4. User-Space Implementation Example

```c
/*
 * Observer Pattern - User Space Implementation
 * Mimics Linux Kernel's notifier chain mechanism
 * 
 * Compile: gcc -o observer observer.c -pthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ============================================================
 * Observer Interface
 * Similar to notifier_block in kernel
 * ============================================================ */

/* Event types */
enum event_type {
    EVENT_STARTUP = 0,
    EVENT_SHUTDOWN,
    EVENT_CONFIG_CHANGE,
    EVENT_ERROR,
    EVENT_MAX
};

/* Event data structure */
struct event_data {
    enum event_type type;
    const char *description;
    void *data;
    int data_size;
};

/* Forward declaration */
struct observer;

/* Observer callback type */
typedef int (*observer_callback)(struct observer *obs, 
                                 unsigned long event,
                                 void *data);

/* Notification results */
#define NOTIFY_OK       0
#define NOTIFY_DONE     0
#define NOTIFY_STOP     (1 << 0)
#define NOTIFY_BAD      (1 << 1)

/* Observer structure (notifier_block equivalent) */
struct observer {
    const char *name;
    observer_callback callback;
    struct observer *next;
    int priority;
    void *private_data;
};

/* ============================================================
 * Subject Interface
 * Similar to notifier_head in kernel
 * ============================================================ */

/* Subject structure (notifier_head equivalent) */
struct subject {
    const char *name;
    struct observer *head;
    pthread_mutex_t lock;
    int observer_count;
};

/* ============================================================
 * Subject Operations
 * ============================================================ */

/**
 * subject_init - Initialize a subject
 * @subj: Subject to initialize
 * @name: Subject name
 */
void subject_init(struct subject *subj, const char *name)
{
    subj->name = name;
    subj->head = NULL;
    pthread_mutex_init(&subj->lock, NULL);
    subj->observer_count = 0;
    printf("[Subject] Created: %s\n", name);
}

/**
 * subject_register - Attach observer to subject
 * @subj: Subject
 * @obs: Observer to attach
 *
 * Inserts observer into list sorted by priority (higher first).
 */
int subject_register(struct subject *subj, struct observer *obs)
{
    struct observer **pp;
    
    pthread_mutex_lock(&subj->lock);
    
    /* Find insertion point based on priority */
    pp = &subj->head;
    while (*pp != NULL) {
        if (obs->priority > (*pp)->priority)
            break;
        pp = &((*pp)->next);
    }
    
    /* Insert observer */
    obs->next = *pp;
    *pp = obs;
    subj->observer_count++;
    
    pthread_mutex_unlock(&subj->lock);
    
    printf("[Subject] %s: Registered observer '%s' (priority %d)\n",
           subj->name, obs->name, obs->priority);
    
    return 0;
}

/**
 * subject_unregister - Detach observer from subject
 * @subj: Subject
 * @obs: Observer to detach
 */
int subject_unregister(struct subject *subj, struct observer *obs)
{
    struct observer **pp;
    
    pthread_mutex_lock(&subj->lock);
    
    pp = &subj->head;
    while (*pp != NULL) {
        if (*pp == obs) {
            *pp = obs->next;
            obs->next = NULL;
            subj->observer_count--;
            pthread_mutex_unlock(&subj->lock);
            printf("[Subject] %s: Unregistered observer '%s'\n",
                   subj->name, obs->name);
            return 0;
        }
        pp = &((*pp)->next);
    }
    
    pthread_mutex_unlock(&subj->lock);
    return -1;
}

/**
 * subject_notify - Notify all observers of an event
 * @subj: Subject
 * @event: Event type
 * @data: Event data
 *
 * Calls all registered observers in priority order.
 */
int subject_notify(struct subject *subj, unsigned long event, void *data)
{
    struct observer *obs;
    int ret = NOTIFY_DONE;
    int call_count = 0;
    
    printf("\n[Subject] %s: Notifying event %lu\n", subj->name, event);
    
    pthread_mutex_lock(&subj->lock);
    
    obs = subj->head;
    while (obs != NULL) {
        printf("[Subject] %s: Calling observer '%s'\n", 
               subj->name, obs->name);
        
        /* Call observer's callback */
        ret = obs->callback(obs, event, data);
        call_count++;
        
        /* Check if observer wants to stop chain */
        if (ret & NOTIFY_STOP) {
            printf("[Subject] %s: Observer '%s' requested stop\n",
                   subj->name, obs->name);
            break;
        }
        
        obs = obs->next;
    }
    
    pthread_mutex_unlock(&subj->lock);
    
    printf("[Subject] %s: Notified %d observers\n", subj->name, call_count);
    
    return ret;
}

/**
 * subject_destroy - Cleanup subject
 */
void subject_destroy(struct subject *subj)
{
    pthread_mutex_destroy(&subj->lock);
    printf("[Subject] Destroyed: %s\n", subj->name);
}

/* ============================================================
 * Concrete Observers
 * ============================================================ */

/* Observer 1: Logger */
struct logger_data {
    FILE *log_file;
    int log_count;
};

int logger_callback(struct observer *obs, unsigned long event, void *data)
{
    struct logger_data *ld = obs->private_data;
    struct event_data *ed = data;
    
    ld->log_count++;
    printf("  [Logger] Event %lu: %s (log #%d)\n", 
           event, ed ? ed->description : "unknown", ld->log_count);
    
    return NOTIFY_OK;
}

/* Observer 2: Metrics Collector */
struct metrics_data {
    int events_received[EVENT_MAX];
};

int metrics_callback(struct observer *obs, unsigned long event, void *data)
{
    struct metrics_data *md = obs->private_data;
    
    if (event < EVENT_MAX) {
        md->events_received[event]++;
    }
    
    printf("  [Metrics] Recorded event %lu (total for type: %d)\n",
           event, md->events_received[event]);
    
    return NOTIFY_OK;
}

/* Observer 3: Alert System */
struct alert_data {
    int alert_threshold;
    int error_count;
};

int alert_callback(struct observer *obs, unsigned long event, void *data)
{
    struct alert_data *ad = obs->private_data;
    struct event_data *ed = data;
    
    if (event == EVENT_ERROR) {
        ad->error_count++;
        printf("  [Alert] ERROR detected! Count: %d\n", ad->error_count);
        
        if (ad->error_count >= ad->alert_threshold) {
            printf("  [Alert] CRITICAL: Error threshold reached!\n");
            return NOTIFY_STOP;  /* Stop notification chain */
        }
    } else if (event == EVENT_SHUTDOWN) {
        printf("  [Alert] Shutdown initiated: %s\n", 
               ed ? ed->description : "unknown");
    }
    
    return NOTIFY_OK;
}

/* Observer 4: State Machine */
struct state_data {
    int current_state;
};

int state_callback(struct observer *obs, unsigned long event, void *data)
{
    struct state_data *sd = obs->private_data;
    int old_state = sd->current_state;
    
    switch (event) {
    case EVENT_STARTUP:
        sd->current_state = 1;  /* Running */
        break;
    case EVENT_SHUTDOWN:
        sd->current_state = 0;  /* Stopped */
        break;
    case EVENT_ERROR:
        sd->current_state = -1; /* Error */
        break;
    default:
        break;
    }
    
    printf("  [State] State transition: %d -> %d\n", 
           old_state, sd->current_state);
    
    return NOTIFY_OK;
}

/* ============================================================
 * Helper Functions
 * ============================================================ */

void print_event_name(enum event_type type)
{
    const char *names[] = {
        "STARTUP", "SHUTDOWN", "CONFIG_CHANGE", "ERROR"
    };
    if (type < EVENT_MAX) {
        printf("%s", names[type]);
    }
}

/* ============================================================
 * Main - Demonstrate Observer Pattern
 * ============================================================ */

int main(void)
{
    struct subject system_events;
    struct event_data event;
    
    /* Observer structures */
    struct observer logger_obs;
    struct observer metrics_obs;
    struct observer alert_obs;
    struct observer state_obs;
    
    /* Observer private data */
    struct logger_data logger_data = { .log_file = stdout, .log_count = 0 };
    struct metrics_data metrics_data = { .events_received = {0} };
    struct alert_data alert_data = { .alert_threshold = 3, .error_count = 0 };
    struct state_data state_data = { .current_state = 0 };

    printf("=== Observer Pattern Demo (Notification System) ===\n\n");

    /* Initialize subject */
    subject_init(&system_events, "SystemEvents");

    /* Initialize observers */
    logger_obs.name = "Logger";
    logger_obs.callback = logger_callback;
    logger_obs.priority = 100;  /* High priority - logs first */
    logger_obs.private_data = &logger_data;
    
    metrics_obs.name = "Metrics";
    metrics_obs.callback = metrics_callback;
    metrics_obs.priority = 50;
    metrics_obs.private_data = &metrics_data;
    
    alert_obs.name = "Alert";
    alert_obs.callback = alert_callback;
    alert_obs.priority = 200;  /* Highest priority */
    alert_obs.private_data = &alert_data;
    
    state_obs.name = "StateMachine";
    state_obs.callback = state_callback;
    state_obs.priority = 75;
    state_obs.private_data = &state_data;

    /* Register observers */
    printf("--- Registering Observers ---\n");
    subject_register(&system_events, &logger_obs);
    subject_register(&system_events, &metrics_obs);
    subject_register(&system_events, &alert_obs);
    subject_register(&system_events, &state_obs);

    /* Trigger events */
    printf("\n--- Triggering Events ---\n");
    
    /* Event 1: Startup */
    event.type = EVENT_STARTUP;
    event.description = "System starting up";
    event.data = NULL;
    subject_notify(&system_events, EVENT_STARTUP, &event);
    
    /* Event 2: Config change */
    event.type = EVENT_CONFIG_CHANGE;
    event.description = "Configuration updated";
    subject_notify(&system_events, EVENT_CONFIG_CHANGE, &event);
    
    /* Event 3-5: Multiple errors (will trigger alert threshold) */
    event.type = EVENT_ERROR;
    event.description = "Database connection failed";
    subject_notify(&system_events, EVENT_ERROR, &event);
    
    event.description = "Network timeout";
    subject_notify(&system_events, EVENT_ERROR, &event);
    
    event.description = "Disk space low";
    subject_notify(&system_events, EVENT_ERROR, &event);  /* Should stop chain */

    /* Event 6: Try shutdown */
    event.type = EVENT_SHUTDOWN;
    event.description = "Graceful shutdown";
    subject_notify(&system_events, EVENT_SHUTDOWN, &event);

    /* Unregister an observer */
    printf("\n--- Unregistering Metrics Observer ---\n");
    subject_unregister(&system_events, &metrics_obs);
    
    /* Event after unregistration */
    event.type = EVENT_STARTUP;
    event.description = "System restart";
    subject_notify(&system_events, EVENT_STARTUP, &event);

    /* Print final statistics */
    printf("\n--- Final Statistics ---\n");
    printf("Logger: %d events logged\n", logger_data.log_count);
    printf("Metrics: Startup=%d, Shutdown=%d, Config=%d, Error=%d\n",
           metrics_data.events_received[EVENT_STARTUP],
           metrics_data.events_received[EVENT_SHUTDOWN],
           metrics_data.events_received[EVENT_CONFIG_CHANGE],
           metrics_data.events_received[EVENT_ERROR]);
    printf("Alerts: %d errors detected\n", alert_data.error_count);
    printf("State: Current state = %d\n", state_data.current_state);

    /* Cleanup */
    subject_destroy(&system_events);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Notification Flow Diagram

```
+------------------------------------------------------------------+
|                   NOTIFICATION FLOW                               |
+------------------------------------------------------------------+
|                                                                   |
|    Event Occurs: ERROR                                            |
|    +-------------------+                                          |
|    | subject_notify()  |                                          |
|    | (EVENT_ERROR)     |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|             v                                                     |
|    +--------+----------+                                          |
|    | Lock mutex        |                                          |
|    | obs = head        |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|    +--------v----------+     Priority: 200                        |
|    | Alert Observer    |<------------------------+                |
|    | alert_callback()  |                         |                |
|    +-------------------+                         |                |
|    | Check error count |                         |                |
|    | Increment count   |                         |                |
|    | return NOTIFY_OK  |                         |                |
|    +--------+----------+                         |                |
|             |                                    |                |
|    +--------v----------+     Priority: 100       | Sorted by      |
|    | Logger Observer   |                         | Priority       |
|    | logger_callback() |                         | (descending)   |
|    +-------------------+                         |                |
|    | Log event         |                         |                |
|    | return NOTIFY_OK  |                         |                |
|    +--------+----------+                         |                |
|             |                                    |                |
|    +--------v----------+     Priority: 75        |                |
|    | State Observer    |                         |                |
|    | state_callback()  |                         |                |
|    +-------------------+                         |                |
|    | Update state      |                         |                |
|    | return NOTIFY_OK  |                         |                |
|    +--------+----------+                         |                |
|             |                                    |                |
|    +--------v----------+     Priority: 50        |                |
|    | Metrics Observer  |<------------------------+                |
|    | metrics_callback()|                                          |
|    +-------------------+                                          |
|    | Record metrics    |                                          |
|    | return NOTIFY_OK  |                                          |
|    +--------+----------+                                          |
|             |                                                     |
|             v                                                     |
|    +--------+----------+                                          |
|    | Unlock mutex      |                                          |
|    | return result     |                                          |
|    +-------------------+                                          |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 通知流程：当事件发生时，subject_notify()遍历观察者链表，按优先级顺序（从高到低）调用每个观察者的回调函数。每个观察者处理事件后返回结果，如果返回NOTIFY_STOP则停止通知链。整个过程在互斥锁保护下进行，确保线程安全。

---

## 6. Key Implementation Points

1. **Callback Function**: Observer registers a function pointer for notifications
2. **Priority Ordering**: Observers sorted by priority in the chain
3. **Chain Termination**: Observer can return NOTIFY_STOP to halt chain
4. **Thread Safety**: Use mutex/spinlock to protect observer list
5. **Dynamic Registration**: Observers added/removed at any time
6. **Event Data**: Pass event-specific data to observers

**中文说明：** 实现观察者模式的关键点：回调函数（观察者注册函数指针接收通知）、优先级排序（观察者按优先级排序）、链终止（观察者可返回NOTIFY_STOP停止链）、线程安全（使用互斥锁保护观察者列表）、动态注册（随时添加/移除观察者）、事件数据（向观察者传递事件相关数据）。

