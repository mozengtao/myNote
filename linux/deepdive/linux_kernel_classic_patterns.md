# Linux Kernel Classic Design Patterns

## A Comprehensive Guide to Kernel Programming Patterns

This document covers the essential design patterns used throughout the Linux kernel, beyond the Operations Structure pattern.

---

## Table of Contents

1. [container_of Pattern](#1-container_of-pattern)
2. [Reference Counting (kref)](#2-reference-counting-kref)
3. [Linked List (list_head)](#3-linked-list-list_head)
4. [RCU (Read-Copy-Update)](#4-rcu-read-copy-update)
5. [Notifier Chain (Observer Pattern)](#5-notifier-chain-observer-pattern)
6. [Workqueue (Deferred Work)](#6-workqueue-deferred-work)
7. [Wait Queue](#7-wait-queue)
8. [Completion](#8-completion)
9. [Error Handling (goto cleanup)](#9-error-handling-goto-cleanup)
10. [Per-CPU Variables](#10-per-cpu-variables)
11. [Slab/Object Caching](#11-slobobject-caching)
12. [Kobject/Kset Hierarchy](#12-kobjectkset-hierarchy)
13. [Pattern Summary](#13-pattern-summary)

---

## 1. container_of Pattern

### 1.1 Core Concept

```
+============================================================================+
|                        CONTAINER_OF PATTERN                                 |
+============================================================================+
|                                                                             |
|   PURPOSE: Get pointer to containing structure from a member pointer        |
|                                                                             |
|   PROBLEM:                                                                  |
|   ========                                                                  |
|   You have a pointer to a MEMBER of a structure, but you need              |
|   a pointer to the WHOLE structure.                                        |
|                                                                             |
|   +---------------------------+                                             |
|   | struct my_device          |                                             |
|   |   +-------------------+   |                                             |
|   |   | other_field_1     |   |                                             |
|   |   +-------------------+   |                                             |
|   |   | other_field_2     |   |                                             |
|   |   +-------------------+   |                                             |
|   |   | list  <-----------+---|--- You have pointer to THIS                |
|   |   +-------------------+   |                                             |
|   |   | other_field_3     |   |                                             |
|   +---------------------------+                                             |
|   ^                                                                         |
|   |                                                                         |
|   +-- But you need pointer to THIS (the whole structure)                   |
|                                                                             |
|   SOLUTION:                                                                 |
|   =========                                                                 |
|   container_of(ptr, type, member)                                          |
|   - ptr:    pointer to the member                                          |
|   - type:   type of the containing structure                               |
|   - member: name of the member within the structure                        |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **问题**：你有一个指向结构体成员的指针，但需要整个结构体的指针
- **解决方案**：`container_of()` 通过计算偏移量反推出容器结构体的地址
- 这是内核中最基础的模式之一，几乎无处不在

### 1.2 Implementation

```c
/* include/linux/kernel.h */

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:    the pointer to the member.
 * @type:   the type of the container struct this is embedded in.
 * @member: the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* How it works:
 * 
 * 1. offsetof(type, member) - calculates byte offset of member within type
 * 2. (char *)ptr - cast to char* for byte-level arithmetic
 * 3. Subtract offset to get start of container structure
 */
```

### 1.3 Visual Example

```
+============================================================================+
|                    CONTAINER_OF CALCULATION                                 |
+============================================================================+
|                                                                             |
|   struct task_struct {                                                      |
|       pid_t pid;                    /* offset 0 */                         |
|       char comm[16];                /* offset 4 */                         |
|       struct list_head tasks;       /* offset 20 */  <-- You have this    |
|       struct mm_struct *mm;         /* offset 36 */                        |
|   };                                                                        |
|                                                                             |
|   Memory Layout:                                                            |
|   +--------+----------------+------------------+-----------+               |
|   |  pid   |     comm       |      tasks       |    mm     |               |
|   | (4B)   |    (16B)       |      (16B)       |   (8B)    |               |
|   +--------+----------------+------------------+-----------+               |
|   ^                         ^                                               |
|   |                         |                                               |
|   0x1000                    0x1014 (ptr to tasks)                          |
|   (start of struct)                                                         |
|                                                                             |
|   container_of(tasks_ptr, struct task_struct, tasks)                       |
|   = (struct task_struct *)((char *)0x1014 - 20)                            |
|   = (struct task_struct *)0x1000                                           |
|   = pointer to the whole task_struct!                                      |
|                                                                             |
+============================================================================+
```

### 1.4 Common Usage

```c
/* Example: Walking a linked list */
struct my_device {
    char name[32];
    int id;
    struct list_head list;  /* Embedded list node */
    void *data;
};

struct list_head device_list;

/* Iterate through all devices */
struct list_head *pos;
list_for_each(pos, &device_list) {
    /* 'pos' points to the 'list' member, not the device! */
    struct my_device *dev = container_of(pos, struct my_device, list);
    /* Now we can access dev->name, dev->id, etc. */
    printk("Device: %s\n", dev->name);
}
```

---

## 2. Reference Counting (kref)

### 2.1 Core Concept

```
+============================================================================+
|                     REFERENCE COUNTING (kref)                               |
+============================================================================+
|                                                                             |
|   PURPOSE: Manage object lifetime with shared ownership                     |
|                                                                             |
|   PROBLEM:                                                                  |
|   ========                                                                  |
|   Multiple users hold pointers to the same object.                         |
|   When should the object be freed?                                         |
|                                                                             |
|        User A ----+                                                         |
|                   |                                                         |
|        User B ----+---> Object (when to free?)                             |
|                   |                                                         |
|        User C ----+                                                         |
|                                                                             |
|   SOLUTION:                                                                 |
|   =========                                                                 |
|   Keep a counter. Each user increments on acquire, decrements on release.  |
|   Free when counter reaches zero.                                          |
|                                                                             |
|   +------------------+                                                      |
|   | Object           |                                                      |
|   | +-------------+  |                                                      |
|   | | kref (cnt=3)|  |  <- 3 users holding reference                       |
|   | +-------------+  |                                                      |
|   | | data...     |  |                                                      |
|   +------------------+                                                      |
|                                                                             |
|   User A releases: cnt = 2                                                  |
|   User B releases: cnt = 1                                                  |
|   User C releases: cnt = 0 -> OBJECT FREED!                                |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **问题**：多个用户持有同一对象的指针，何时释放？
- **解决方案**：引用计数 - 获取时增加，释放时减少，归零时释放对象
- 内核使用 `kref` 结构和相关 API 实现

### 2.2 Implementation

```c
/* include/linux/kref.h */

struct kref {
    atomic_t refcount;
};

/* Initialize to 1 (creator holds first reference) */
static inline void kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);
}

/* Acquire a reference */
static inline void kref_get(struct kref *kref)
{
    atomic_inc(&kref->refcount);
}

/* Release a reference - calls 'release' when count reaches 0 */
static inline int kref_put(struct kref *kref, 
                           void (*release)(struct kref *kref))
{
    if (atomic_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;  /* Object was freed */
    }
    return 0;  /* Object still alive */
}
```

### 2.3 Usage Example

```c
struct my_object {
    struct kref refcount;    /* Reference counter */
    char *name;
    void *data;
};

/* Release function - called when refcount hits 0 */
static void my_object_release(struct kref *kref)
{
    struct my_object *obj = container_of(kref, struct my_object, refcount);
    kfree(obj->name);
    kfree(obj->data);
    kfree(obj);
}

/* Create object */
struct my_object *my_object_create(const char *name)
{
    struct my_object *obj = kzalloc(sizeof(*obj), GFP_KERNEL);
    kref_init(&obj->refcount);  /* refcount = 1 */
    obj->name = kstrdup(name, GFP_KERNEL);
    return obj;
}

/* Get a reference */
void my_object_get(struct my_object *obj)
{
    kref_get(&obj->refcount);
}

/* Release a reference */
void my_object_put(struct my_object *obj)
{
    kref_put(&obj->refcount, my_object_release);
}

/* Usage */
struct my_object *obj = my_object_create("test");  /* refcount = 1 */
my_object_get(obj);  /* refcount = 2 */
my_object_get(obj);  /* refcount = 3 */

my_object_put(obj);  /* refcount = 2 */
my_object_put(obj);  /* refcount = 1 */
my_object_put(obj);  /* refcount = 0 -> my_object_release() called */
```

---

## 3. Linked List (list_head)

### 3.1 Core Concept

```
+============================================================================+
|                      LINUX LINKED LIST PATTERN                              |
+============================================================================+
|                                                                             |
|   UNIQUE DESIGN: The list node is EMBEDDED in the data structure           |
|                                                                             |
|   Traditional approach:                 Linux approach:                     |
|   +-------------+                       +-------------------+              |
|   | list_node   |                       | struct my_data    |              |
|   | - data *    |---> data              |   +------------+  |              |
|   | - next *    |                       |   | list_head  |  | <-- embedded |
|   | - prev *    |                       |   +------------+  |              |
|   +-------------+                       |   | actual data|  |              |
|                                         +-------------------+              |
|                                                                             |
|   BENEFITS:                                                                 |
|   - No separate allocation for list node                                   |
|   - One data item can be on MULTIPLE lists                                 |
|   - Cache-friendly (node is part of data)                                  |
|                                                                             |
|   struct my_task {                                                          |
|       char name[32];                                                        |
|       struct list_head run_list;     /* Can be on run queue */             |
|       struct list_head wait_list;    /* AND on wait queue */               |
|       struct list_head all_tasks;    /* AND on all-tasks list */           |
|   };                                                                        |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **独特设计**：链表节点嵌入到数据结构中，而不是指向数据
- **优点**：无需额外分配，一个对象可同时在多个链表上，缓存友好
- 使用 `container_of()` 从节点获取完整结构体

### 3.2 Implementation

```c
/* include/linux/list.h */

struct list_head {
    struct list_head *next, *prev;
};

/* Initialize a list head */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/* Add to list */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    head->next->prev = new;
    new->next = head->next;
    new->prev = head;
    head->next = new;
}

/* Remove from list */
static inline void list_del(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

/* Iterate through list */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* Iterate with container_of built in */
#define list_for_each_entry(pos, head, member)                  \
    for (pos = container_of((head)->next, typeof(*pos), member);\
         &pos->member != (head);                                 \
         pos = container_of(pos->member.next, typeof(*pos), member))
```

### 3.3 Visual Representation

```
+============================================================================+
|                      DOUBLY LINKED LIST STRUCTURE                           |
+============================================================================+
|                                                                             |
|   List Head (anchor, contains no data):                                    |
|                                                                             |
|       +------------+                                                        |
|       | list_head  |                                                        |
|       | next  prev |                                                        |
|       +--+------+--+                                                        |
|          |      |                                                           |
|          v      |                                                           |
|   +---------------+      +---------------+      +---------------+          |
|   | struct item_1 |      | struct item_2 |      | struct item_3 |          |
|   | +----------+  |<---->| +----------+  |<---->| +----------+  |          |
|   | |list_head |  |      | |list_head |  |      | |list_head |  |          |
|   | |next  prev|  |      | |next  prev|  |      | |next  prev|  |          |
|   | +----------+  |      | +----------+  |      | +----------+  |          |
|   | |  data    |  |      | |  data    |  |      | |  data    |  |          |
|   +---------------+      +---------------+      +---------------+          |
|          ^                                              |                   |
|          |                                              |                   |
|          +----------------------------------------------+                   |
|                           (circular)                                        |
|                                                                             |
+============================================================================+
```

---

## 4. RCU (Read-Copy-Update)

### 4.1 Core Concept

```
+============================================================================+
|                     RCU (READ-COPY-UPDATE)                                  |
+============================================================================+
|                                                                             |
|   PURPOSE: Lock-free read access to shared data                            |
|                                                                             |
|   PROBLEM:                                                                  |
|   ========                                                                  |
|   Many readers, few writers. Locking is expensive for readers.             |
|                                                                             |
|   Traditional locking:              RCU approach:                          |
|   +----------------+                +----------------+                     |
|   | Reader 1: WAIT |                | Reader 1: READ | (no waiting!)       |
|   | Reader 2: WAIT |                | Reader 2: READ |                     |
|   | Reader 3: WAIT |                | Reader 3: READ |                     |
|   | Writer:   LOCK |                | Writer:  COPY, |                     |
|   +----------------+                |          UPDATE |                     |
|                                     +----------------+                     |
|                                                                             |
|   HOW IT WORKS:                                                             |
|   =============                                                             |
|                                                                             |
|   1. READERS: Read without locks (just disable preemption briefly)         |
|   2. WRITERS: Make a COPY, modify copy, atomically swap pointer            |
|   3. GRACE PERIOD: Wait for all readers to finish before freeing old       |
|                                                                             |
|   Timeline:                                                                 |
|   ----------------------------------------------------------->  time       |
|   [Old Data]  Writer: swap    [New Data]                                   |
|      ^        pointer            ^                                         |
|      |           |               |                                         |
|   Reader A  Reader B starts   Reader C                                     |
|   still on     (sees new)     (sees new)                                   |
|   old data                                                                  |
|      |                                                                      |
|      +-- Must wait for A to finish before freeing old data                 |
|          (this is the "grace period")                                       |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **问题**：多读少写场景，锁对读者开销太大
- **解决方案**：读者无锁访问，写者复制-修改-交换指针
- **优雅期**：在释放旧数据前，等待所有读者完成
- 适用于读多写少的场景，如路由表、文件系统缓存等

### 4.2 Implementation Pattern

```c
/* RCU-protected pointer */
struct data {
    int value;
    char name[32];
};

struct data __rcu *global_ptr;  /* RCU-protected pointer */

/* READER - no locks! */
void reader(void)
{
    struct data *p;
    
    rcu_read_lock();        /* Disable preemption */
    p = rcu_dereference(global_ptr);  /* Get pointer safely */
    if (p)
        printk("value = %d\n", p->value);  /* Use data */
    rcu_read_unlock();      /* Re-enable preemption */
    /* DO NOT use 'p' after rcu_read_unlock()! */
}

/* WRITER */
void writer(int new_value)
{
    struct data *old, *new;
    
    new = kmalloc(sizeof(*new), GFP_KERNEL);
    new->value = new_value;
    
    old = rcu_dereference_protected(global_ptr, 
                                     lockdep_is_held(&writer_lock));
    
    rcu_assign_pointer(global_ptr, new);  /* Atomic swap */
    
    synchronize_rcu();      /* Wait for all readers */
    kfree(old);             /* Safe to free old now */
}
```

---

## 5. Notifier Chain (Observer Pattern)

### 5.1 Core Concept

```
+============================================================================+
|                     NOTIFIER CHAIN (OBSERVER)                               |
+============================================================================+
|                                                                             |
|   PURPOSE: Notify multiple interested parties of events                    |
|                                                                             |
|   Also known as: Observer Pattern, Publish-Subscribe                       |
|                                                                             |
|   STRUCTURE:                                                                |
|   ==========                                                                |
|                                                                             |
|           +-------------------+                                             |
|           | Event Source      |                                             |
|           | (e.g., netdevice) |                                             |
|           +--------+----------+                                             |
|                    |                                                        |
|                    | "Interface went DOWN"                                  |
|                    v                                                        |
|           +-------------------+                                             |
|           | Notifier Chain    |                                             |
|           +-------------------+                                             |
|                    |                                                        |
|        +-----------+-----------+                                            |
|        |           |           |                                            |
|        v           v           v                                            |
|   +--------+  +--------+  +--------+                                        |
|   |Callback|  |Callback|  |Callback|                                        |
|   |  (IP)  |  |(route) |  |(filter)|                                        |
|   +--------+  +--------+  +--------+                                        |
|                                                                             |
|   Each subsystem registers interest, gets notified when event occurs.      |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **目的**：事件发生时通知所有感兴趣的模块
- **实现**：回调函数链表，事件发生时遍历调用
- **典型用例**：网络设备状态变化、CPU 热插拔、电源管理等

### 5.2 Implementation

```c
/* include/linux/notifier.h */

struct notifier_block {
    int (*notifier_call)(struct notifier_block *nb,
                         unsigned long action, void *data);
    struct notifier_block *next;
    int priority;
};

/* Example: Network device notifier */
static struct notifier_block my_netdev_notifier = {
    .notifier_call = my_netdev_event,
    .priority = 0,
};

static int my_netdev_event(struct notifier_block *this,
                           unsigned long event, void *ptr)
{
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);
    
    switch (event) {
    case NETDEV_UP:
        printk("Interface %s is UP\n", dev->name);
        break;
    case NETDEV_DOWN:
        printk("Interface %s is DOWN\n", dev->name);
        break;
    }
    return NOTIFY_DONE;
}

/* Register */
register_netdevice_notifier(&my_netdev_notifier);

/* Unregister */
unregister_netdevice_notifier(&my_netdev_notifier);
```

---

## 6. Workqueue (Deferred Work)

### 6.1 Core Concept

```
+============================================================================+
|                     WORKQUEUE (DEFERRED WORK)                               |
+============================================================================+
|                                                                             |
|   PURPOSE: Defer work from interrupt context to process context            |
|                                                                             |
|   PROBLEM:                                                                  |
|   ========                                                                  |
|   Interrupt handlers must be fast. They cannot:                            |
|   - Sleep                                                                   |
|   - Take mutexes (only spinlocks)                                          |
|   - Do I/O that might block                                                |
|   - Allocate memory with GFP_KERNEL                                        |
|                                                                             |
|   SOLUTION:                                                                 |
|   =========                                                                 |
|   Schedule work to run later in process context (kernel thread)            |
|                                                                             |
|   Interrupt Context          Process Context                               |
|   +---------------+          +-------------------+                         |
|   | IRQ Handler   |          | Kernel Worker     |                         |
|   |               |          | Thread            |                         |
|   | - Quick work  |          |                   |                         |
|   | - schedule_   |--------->| - Slow work       |                         |
|   |   work()      |          | - Can sleep       |                         |
|   |               |          | - Can use mutexes |                         |
|   +---------------+          +-------------------+                         |
|        ^                            |                                       |
|        |                            v                                       |
|   Hardware                    File I/O, Network,                           |
|   Interrupt                   Complex processing                           |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **问题**：中断处理必须快速，不能睡眠或做耗时操作
- **解决方案**：将工作推迟到进程上下文中执行
- workqueue 由内核工作线程执行，可以睡眠、分配内存等

### 6.2 Implementation

```c
/* Define work item */
struct my_device {
    struct work_struct work;     /* For regular work */
    struct delayed_work dwork;   /* For delayed work */
    int data;
};

/* Work function - runs in process context */
static void my_work_func(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, work);
    
    /* Can sleep here! */
    mutex_lock(&some_mutex);
    /* ... do complex processing ... */
    mutex_unlock(&some_mutex);
}

/* Initialize */
struct my_device *dev = kzalloc(sizeof(*dev), GFP_KERNEL);
INIT_WORK(&dev->work, my_work_func);
INIT_DELAYED_WORK(&dev->dwork, my_delayed_work_func);

/* Schedule from interrupt handler */
irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    
    /* Quick handling... */
    
    /* Defer heavy work */
    schedule_work(&dev->work);
    
    /* Or defer with delay (100 jiffies) */
    schedule_delayed_work(&dev->dwork, 100);
    
    return IRQ_HANDLED;
}
```

---

## 7. Wait Queue

### 7.1 Core Concept

```
+============================================================================+
|                          WAIT QUEUE                                         |
+============================================================================+
|                                                                             |
|   PURPOSE: Put a process to sleep until a condition becomes true           |
|                                                                             |
|   MECHANISM:                                                                |
|   ==========                                                                |
|                                                                             |
|   WAITER (blocking):                  WAKER (e.g., IRQ handler):           |
|   ==================                  ======================                |
|                                                                             |
|   wait_event(wq, condition);          /* Data is ready */                  |
|        |                              wake_up(&wq);                        |
|        v                                   |                                |
|   +------------+                           |                                |
|   | Check cond |                           |                                |
|   | FALSE      |                           |                                |
|   +-----+------+                           |                                |
|         |                                  |                                |
|         v                                  |                                |
|   +------------+                           |                                |
|   | SLEEP      |<--------------------------+                                |
|   +------------+                                                            |
|         |                                                                   |
|         v                                                                   |
|   +------------+                                                            |
|   | WAKE UP    |                                                            |
|   +------------+                                                            |
|         |                                                                   |
|         v                                                                   |
|   +------------+                                                            |
|   | Check cond |                                                            |
|   | TRUE       |                                                            |
|   +-----+------+                                                            |
|         |                                                                   |
|         v                                                                   |
|   Continue execution                                                        |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **目的**：让进程睡眠等待某个条件成立
- **机制**：进程加入等待队列睡眠，条件满足时被唤醒
- 避免忙等待，节省 CPU 资源

### 7.2 Implementation

```c
#include <linux/wait.h>

/* Declare and initialize wait queue head */
DECLARE_WAIT_QUEUE_HEAD(my_wait_queue);

/* Or dynamically */
wait_queue_head_t my_wait_queue;
init_waitqueue_head(&my_wait_queue);

/* WAITER: Sleep until condition is true */
int my_read(char *buf, size_t len)
{
    /* Wait until data_available becomes true */
    wait_event(my_wait_queue, data_available);
    
    /* Or with timeout */
    ret = wait_event_timeout(my_wait_queue, data_available, HZ * 5);
    if (ret == 0)
        return -ETIMEDOUT;
    
    /* Or interruptible (can be interrupted by signals) */
    if (wait_event_interruptible(my_wait_queue, data_available))
        return -ERESTARTSYS;
    
    /* Data is now available, process it */
    return copy_to_user(buf, data, len);
}

/* WAKER: Wake up waiting processes */
irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    /* ... receive data ... */
    data_available = true;
    
    /* Wake up one waiter */
    wake_up(&my_wait_queue);
    
    /* Or wake up all waiters */
    wake_up_all(&my_wait_queue);
    
    return IRQ_HANDLED;
}
```

---

## 8. Completion

### 8.1 Core Concept

```
+============================================================================+
|                          COMPLETION                                         |
+============================================================================+
|                                                                             |
|   PURPOSE: One-shot synchronization - wait for something to complete       |
|                                                                             |
|   USE CASE:                                                                 |
|   =========                                                                 |
|   Thread A starts an operation and must wait for Thread B to finish it.    |
|                                                                             |
|   Thread A                              Thread B                            |
|   ========                              ========                            |
|   start_operation();                                                        |
|        |                                                                    |
|        |                                do_work();                          |
|        v                                    |                               |
|   wait_for_completion(&done);               |                               |
|        |                                    v                               |
|    [BLOCKED]                           complete(&done);                     |
|        |                                                                    |
|        v                                                                    |
|   [RESUMED - operation is done]                                            |
|                                                                             |
|   DIFFERENCE FROM WAIT_QUEUE:                                               |
|   - Completion is for one-time events                                      |
|   - Wait queue is for conditions that may change repeatedly                |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **目的**：等待某个操作完成（一次性同步）
- **用例**：线程 A 发起操作，等待线程 B 完成
- 与 wait_queue 区别：completion 用于一次性事件，wait_queue 用于反复变化的条件

### 8.2 Implementation

```c
#include <linux/completion.h>

/* Declare and initialize */
DECLARE_COMPLETION(my_completion);

/* Or dynamically */
struct completion my_completion;
init_completion(&my_completion);

/* Example: Waiting for firmware load */
struct my_device {
    struct completion fw_loaded;
    void *firmware;
};

int load_firmware(struct my_device *dev)
{
    /* Request firmware asynchronously */
    request_firmware_nowait(/*...callback=*/fw_callback, dev);
    
    /* Wait for it to complete */
    wait_for_completion(&dev->fw_loaded);
    
    /* Or with timeout */
    ret = wait_for_completion_timeout(&dev->fw_loaded, HZ * 10);
    if (ret == 0)
        return -ETIMEDOUT;
    
    /* Firmware is now loaded */
    return 0;
}

void fw_callback(const struct firmware *fw, void *context)
{
    struct my_device *dev = context;
    
    dev->firmware = process_firmware(fw);
    
    /* Signal completion */
    complete(&dev->fw_loaded);
}
```

---

## 9. Error Handling (goto cleanup)

### 9.1 Core Concept

```
+============================================================================+
|                     GOTO CLEANUP PATTERN                                    |
+============================================================================+
|                                                                             |
|   PURPOSE: Clean, structured error handling with proper resource cleanup   |
|                                                                             |
|   PROBLEM:                                                                  |
|   ========                                                                  |
|   Multiple resources allocated, any step can fail.                         |
|   Need to clean up already-allocated resources on failure.                 |
|                                                                             |
|   alloc A -----> alloc B -----> alloc C -----> SUCCESS                    |
|      |              |              |                                        |
|      |              |              +---> free C, free B, free A            |
|      |              +---> free B, free A                                   |
|      +---> free A                                                          |
|                                                                             |
|   BAD APPROACH (nested ifs):            KERNEL APPROACH (goto):            |
|   ==========================            =======================             |
|                                                                             |
|   if (alloc_a()) {                      a = alloc_a();                     |
|       if (alloc_b()) {                  if (!a) goto err_a;                |
|           if (alloc_c()) {              b = alloc_b();                     |
|               /* success */             if (!b) goto err_b;                |
|           } else {                      c = alloc_c();                     |
|               free_b();                 if (!c) goto err_c;                |
|               free_a();                 return 0;                          |
|           }                                                                 |
|       } else {                          err_c: free_b();                   |
|           free_a();                     err_b: free_a();                   |
|       }                                 err_a: return -ENOMEM;             |
|   }                                                                         |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **问题**：多个资源需要分配，任何一步可能失败，需要正确清理
- **解决方案**：使用 goto 跳转到清理代码，按逆序释放资源
- 这是内核中唯一推荐使用 goto 的场景

### 9.2 Implementation

```c
int my_device_init(struct my_device *dev)
{
    int ret;
    
    /* Step 1: Allocate memory */
    dev->buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!dev->buffer) {
        ret = -ENOMEM;
        goto err_buffer;
    }
    
    /* Step 2: Request IRQ */
    ret = request_irq(dev->irq, my_irq_handler, 0, "mydev", dev);
    if (ret) {
        goto err_irq;
    }
    
    /* Step 3: Create workqueue */
    dev->wq = create_singlethread_workqueue("mydev_wq");
    if (!dev->wq) {
        ret = -ENOMEM;
        goto err_wq;
    }
    
    /* Step 4: Register with subsystem */
    ret = register_device(dev);
    if (ret) {
        goto err_register;
    }
    
    return 0;  /* SUCCESS */

/* Error handling - in reverse order! */
err_register:
    destroy_workqueue(dev->wq);
err_wq:
    free_irq(dev->irq, dev);
err_irq:
    kfree(dev->buffer);
err_buffer:
    return ret;
}

void my_device_cleanup(struct my_device *dev)
{
    /* Same order as error cleanup, but all steps */
    unregister_device(dev);
    destroy_workqueue(dev->wq);
    free_irq(dev->irq, dev);
    kfree(dev->buffer);
}
```

---

## 10. Per-CPU Variables

### 10.1 Core Concept

```
+============================================================================+
|                       PER-CPU VARIABLES                                     |
+============================================================================+
|                                                                             |
|   PURPOSE: Each CPU has its own copy of a variable (no locking needed)     |
|                                                                             |
|   PROBLEM:                                                                  |
|   ========                                                                  |
|   Multiple CPUs access same variable -> need locking                       |
|   Locking causes cache line bouncing -> poor performance                   |
|                                                                             |
|   Shared variable:                      Per-CPU variables:                 |
|   +------------+                        +------+------+------+------+      |
|   | counter    |<-- All CPUs            | CPU0 | CPU1 | CPU2 | CPU3 |      |
|   +------------+    contend             |  10  |  20  |  15  |  25  |      |
|                                         +------+------+------+------+      |
|   Each access needs lock                Each CPU has own copy              |
|   Cache line bounces                    No locking needed!                 |
|                                                                             |
|   USE CASES:                                                                |
|   - Per-CPU counters (statistics)                                          |
|   - Per-CPU caches (reduce contention)                                     |
|   - Per-CPU queues (lockless access)                                       |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **目的**：每个 CPU 有自己的变量副本，无需加锁
- **优点**：避免锁竞争，避免缓存行颠簸
- **用例**：统计计数器、CPU 本地缓存、CPU 本地队列

### 10.2 Implementation

```c
#include <linux/percpu.h>

/* Static per-CPU variable */
DEFINE_PER_CPU(int, my_counter);

/* Dynamic per-CPU variable */
int __percpu *my_dynamic_counter;
my_dynamic_counter = alloc_percpu(int);

/* Access per-CPU variable (must disable preemption) */
void increment_counter(void)
{
    /* get_cpu() disables preemption and returns CPU id */
    int cpu = get_cpu();
    
    /* Access this CPU's copy */
    per_cpu(my_counter, cpu)++;
    
    /* Or use this_cpu_* helpers (preferred) */
    this_cpu_inc(my_counter);  /* Atomic increment */
    
    /* Re-enable preemption */
    put_cpu();
}

/* Sum all CPUs' counters */
int get_total_count(void)
{
    int total = 0;
    int cpu;
    
    for_each_possible_cpu(cpu) {
        total += per_cpu(my_counter, cpu);
    }
    return total;
}

/* Free dynamic per-CPU variable */
free_percpu(my_dynamic_counter);
```

---

## 11. Slab/Object Caching

### 11.1 Core Concept

```
+============================================================================+
|                      SLAB ALLOCATOR / OBJECT CACHE                          |
+============================================================================+
|                                                                             |
|   PURPOSE: Efficient allocation of same-size objects                       |
|                                                                             |
|   PROBLEM:                                                                  |
|   ========                                                                  |
|   Allocating many objects of same type (e.g., inodes, task_structs)       |
|   - kmalloc overhead for each allocation                                   |
|   - Object construction/initialization cost                                |
|                                                                             |
|   SOLUTION:                                                                 |
|   =========                                                                 |
|   Pre-allocate pools of objects, reuse freed objects                       |
|                                                                             |
|   +--------------------------------------------+                            |
|   | kmem_cache "task_struct_cache"             |                            |
|   +--------------------------------------------+                            |
|   |  +-----------+  +-----------+  +-----------+|                           |
|   |  |task_struct|  |task_struct|  |task_struct|| <-- Pre-allocated        |
|   |  |(free)     |  |(in use)   |  |(free)     ||     objects              |
|   |  +-----------+  +-----------+  +-----------+|                           |
|   |  +-----------+  +-----------+  +-----------+|                           |
|   |  |task_struct|  |task_struct|  |task_struct||                           |
|   |  |(in use)   |  |(free)     |  |(in use)   ||                           |
|   |  +-----------+  +-----------+  +-----------+|                           |
|   +--------------------------------------------+                            |
|                                                                             |
|   - Allocation: grab free object from pool (very fast)                     |
|   - Deallocation: return to pool (not freed, can be reused)               |
|   - Objects can be pre-constructed                                         |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **问题**：频繁分配同类型对象，kmalloc 开销大
- **解决方案**：预分配对象池，重用已释放的对象
- 分配极快（从池中取），释放极快（归还池中）

### 11.2 Implementation

```c
#include <linux/slab.h>

/* Define object structure */
struct my_object {
    int id;
    char name[32];
    struct list_head list;
};

/* Create the cache */
static struct kmem_cache *my_cache;

int __init my_init(void)
{
    my_cache = kmem_cache_create(
        "my_object_cache",          /* Name (appears in /proc/slabinfo) */
        sizeof(struct my_object),   /* Object size */
        0,                          /* Alignment */
        SLAB_HWCACHE_ALIGN,         /* Flags */
        my_object_ctor              /* Constructor (optional) */
    );
    if (!my_cache)
        return -ENOMEM;
    return 0;
}

/* Optional constructor - called when object first allocated */
static void my_object_ctor(void *obj)
{
    struct my_object *o = obj;
    memset(o, 0, sizeof(*o));
    INIT_LIST_HEAD(&o->list);
}

/* Allocate object from cache */
struct my_object *my_object_alloc(void)
{
    return kmem_cache_alloc(my_cache, GFP_KERNEL);
}

/* Free object back to cache */
void my_object_free(struct my_object *obj)
{
    kmem_cache_free(my_cache, obj);
}

/* Cleanup */
void __exit my_exit(void)
{
    kmem_cache_destroy(my_cache);
}
```

---

## 12. Kobject/Kset Hierarchy

### 12.1 Core Concept

```
+============================================================================+
|                     KOBJECT/KSET HIERARCHY                                  |
+============================================================================+
|                                                                             |
|   PURPOSE: Unified object model with sysfs representation                  |
|                                                                             |
|   KOBJECT provides:                                                         |
|   - Reference counting                                                      |
|   - Sysfs representation (directories in /sys)                             |
|   - Parent-child relationships                                             |
|   - Hotplug event generation                                               |
|                                                                             |
|   HIERARCHY:                                                                |
|   ==========                                                                |
|                                                                             |
|   /sys/                                                                     |
|   +-- devices/               <-- kset                                      |
|   |   +-- pci0000:00/        <-- kobject (device)                         |
|   |   |   +-- 0000:00:1f.0/  <-- kobject (device)                         |
|   |   |       +-- driver     <-- attribute                                 |
|   |   |       +-- resource   <-- attribute                                 |
|   +-- bus/                   <-- kset                                      |
|   |   +-- pci/               <-- kobject (bus type)                       |
|   |       +-- drivers/       <-- kset                                      |
|   |       +-- devices/       <-- kset                                      |
|   +-- class/                 <-- kset                                      |
|       +-- net/               <-- kobject (class)                          |
|           +-- eth0/          <-- kobject (device)                         |
|                                                                             |
+============================================================================+
```

**中文说明**：
- **kobject**：内核对象的基础结构，提供引用计数和 sysfs 表示
- **kset**：kobject 的集合，表示目录
- 统一的对象模型，实现设备模型的层次结构

### 12.2 Relationship to Device Model

```c
/* kobject is usually embedded in a larger structure */
struct device {
    struct kobject kobj;        /* Embedded kobject */
    struct device *parent;
    const char *init_name;
    const struct device_type *type;
    struct bus_type *bus;
    struct device_driver *driver;
    /* ... */
};

/* The device model is built on kobject/kset */
/*
 * kobject -> embedded in device, driver, bus, class
 * kset    -> containers (devices_kset, drivers_kset)
 * 
 * This creates the entire /sys hierarchy!
 */
```

---

## 13. Pattern Summary

```
+============================================================================+
|                         PATTERN SUMMARY                                     |
+============================================================================+
|                                                                             |
|   +-------------------+--------------------------------------------------+  |
|   | Pattern           | Purpose                                          |  |
|   +-------------------+--------------------------------------------------+  |
|   | container_of      | Get containing struct from member pointer        |  |
|   +-------------------+--------------------------------------------------+  |
|   | kref              | Reference counting for object lifetime           |  |
|   +-------------------+--------------------------------------------------+  |
|   | list_head         | Generic doubly linked list                       |  |
|   +-------------------+--------------------------------------------------+  |
|   | RCU               | Lock-free read access, rare updates             |  |
|   +-------------------+--------------------------------------------------+  |
|   | Notifier Chain    | Observer pattern for events                      |  |
|   +-------------------+--------------------------------------------------+  |
|   | Workqueue         | Defer work to process context                    |  |
|   +-------------------+--------------------------------------------------+  |
|   | Wait Queue        | Sleep until condition true                       |  |
|   +-------------------+--------------------------------------------------+  |
|   | Completion        | One-shot synchronization                         |  |
|   +-------------------+--------------------------------------------------+  |
|   | goto cleanup      | Structured error handling                        |  |
|   +-------------------+--------------------------------------------------+  |
|   | Per-CPU           | Per-CPU data, no locking                        |  |
|   +-------------------+--------------------------------------------------+  |
|   | Slab              | Efficient same-size object allocation           |  |
|   +-------------------+--------------------------------------------------+  |
|   | kobject/kset      | Unified object model with sysfs                 |  |
|   +-------------------+--------------------------------------------------+  |
|   | ops structure     | Polymorphism via function pointers              |  |
|   +-------------------+--------------------------------------------------+  |
|                                                                             |
+============================================================================+
```

**中文说明**：

| 模式 | 用途 |
|------|------|
| container_of | 从成员指针获取容器结构体 |
| kref | 引用计数管理对象生命周期 |
| list_head | 通用双向链表 |
| RCU | 无锁读访问，偶尔更新 |
| Notifier Chain | 事件的观察者模式 |
| Workqueue | 将工作推迟到进程上下文 |
| Wait Queue | 睡眠等待条件成立 |
| Completion | 一次性同步 |
| goto cleanup | 结构化错误处理 |
| Per-CPU | CPU 本地数据，无需锁 |
| Slab | 高效的同类型对象分配 |
| kobject/kset | 统一对象模型与 sysfs |
| ops structure | 通过函数指针实现多态 |

---

## References

1. Linux Kernel Source - `include/linux/list.h`
2. Linux Kernel Source - `include/linux/kref.h`
3. Linux Kernel Source - `include/linux/rcupdate.h`
4. Linux Kernel Source - `include/linux/workqueue.h`
5. Linux Kernel Source - `include/linux/wait.h`
6. "Linux Kernel Development, 3rd Edition" - Robert Love
7. "Understanding the Linux Kernel, 3rd Edition" - Bovet & Cesati

