# Linux Kernel Context-Aware Programming (v3.2)

## Overview

This document explains how the Linux kernel enforces **context-aware programming**, where code behavior is constrained by the execution context in which it runs.

---

## Kernel Execution Contexts

```
+------------------------------------------------------------------+
|  KERNEL EXECUTION CONTEXTS                                       |
+------------------------------------------------------------------+

    +-----------------+     +-----------------+     +-----------------+
    | PROCESS CONTEXT |     | SOFTIRQ CONTEXT |     |  IRQ CONTEXT    |
    +-----------------+     +-----------------+     +-----------------+
    |                 |     |                 |     |                 |
    | - Syscall       |     | - Network RX    |     | - Hardware IRQ  |
    | - Workqueue     |     | - Timers        |     | - NMI           |
    | - kthread       |     | - Tasklet       |     |                 |
    |                 |     | - RCU callbacks |     |                 |
    +-----------------+     +-----------------+     +-----------------+
    |                 |     |                 |     |                 |
    | CAN sleep       |     | CANNOT sleep    |     | CANNOT sleep    |
    | CAN allocate    |     | Atomic alloc    |     | Atomic only     |
    | Preemptible     |     | Not preemptible |     | Not preemptible |
    | Has current     |     | Has current     |     | current invalid |
    |                 |     |                 |     |                 |
    +-----------------+     +-----------------+     +-----------------+
            |                       |                       |
            v                       v                       v
    LEAST RESTRICTED        RESTRICTED             MOST RESTRICTED
```

### Context Detection

From `include/linux/hardirq.h`:

```c
#define in_irq()        (hardirq_count())     /* Hardware IRQ */
#define in_softirq()    (softirq_count())     /* Softirq/BH */
#define in_interrupt()  (irq_count())         /* IRQ or softirq */
#define in_atomic()     (preempt_count() != 0) /* Cannot sleep */

/* Check if we're in process context */
#define in_task()       (!(in_interrupt() || in_atomic()))
```

**中文解释：**
- **进程上下文**：系统调用、工作队列、内核线程 — 可睡眠、可分配
- **软中断上下文**：网络 RX、定时器、tasklet — 不可睡眠、原子分配
- **硬中断上下文**：硬件中断处理 — 限制最严格

---

## Context Comparison Table

```
+------------------------------------------------------------------+
|  WHAT IS ALLOWED IN EACH CONTEXT                                 |
+------------------------------------------------------------------+

    Operation              | Process | Softirq |   IRQ   |
    -----------------------+---------+---------+---------+
    Sleep / schedule()     |   YES   |   NO    |   NO    |
    mutex_lock()           |   YES   |   NO    |   NO    |
    down() / semaphore     |   YES   |   NO    |   NO    |
    GFP_KERNEL allocation  |   YES   |   NO    |   NO    |
    GFP_ATOMIC allocation  |   YES   |   YES   |   YES   |
    spin_lock()            |   YES   |   YES   |   YES   |
    spin_lock_irq()        |   YES   |   YES   |   NO    |
    spin_lock_irqsave()    |   YES   |   YES   |   YES   |
    copy_to/from_user()    |   YES   |   NO    |   NO    |
    Access current->       |   YES   |   YES*  |   NO    |
    kmalloc(GFP_KERNEL)    |   YES   |   NO    |   NO    |
    kmalloc(GFP_ATOMIC)    |   YES   |   YES   |   YES   |
    kfree()                |   YES   |   YES   |   YES   |
    
    * current is valid in softirq but may not be meaningful
```

**中文解释：**
- 进程上下文：所有操作都允许
- 软中断上下文：不能睡眠、不能使用可睡眠锁、只能原子分配
- 硬中断上下文：限制最严，不能访问用户空间，`current` 无效

---

## IRQ Handlers

```
+------------------------------------------------------------------+
|  IRQ HANDLER CONSTRAINTS                                         |
+------------------------------------------------------------------+

    +-------------------+
    |   Hardware IRQ    |
    | (CPU interrupted) |
    +--------+----------+
             |
             v
    +--------+----------+
    |  IRQ Handler      |
    |                   |
    | CONSTRAINTS:      |
    | - Must be fast    |
    | - Cannot sleep    |
    | - Minimal work    |
    | - Ack hardware    |
    +--------+----------+
             |
             | Schedule deferred work
             v
    +--------+----------+
    |  Softirq/Tasklet  |
    |  or Workqueue     |
    +-------------------+
```

### IRQ Handler Example

```c
/* Minimal IRQ handler */
static irqreturn_t my_irq_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;
    u32 status;
    
    /* Read and ack hardware */
    status = readl(dev->regs + STATUS);
    writel(status, dev->regs + ACK);
    
    if (!(status & MY_IRQ_MASK))
        return IRQ_NONE;  /* Not our interrupt */
    
    /* Cannot do heavy work here! */
    /* Cannot sleep! */
    /* Cannot allocate with GFP_KERNEL! */
    
    /* Schedule deferred work */
    tasklet_schedule(&dev->tasklet);
    /* or: */
    napi_schedule(&dev->napi);
    /* or: */
    queue_work(dev->workqueue, &dev->work);
    
    return IRQ_HANDLED;
}
```

**中文解释：**
- IRQ 处理程序必须：
  1. 快速执行
  2. 确认硬件中断
  3. 调度延迟工作
- IRQ 处理程序不能：
  1. 睡眠
  2. 做大量工作
  3. 使用可睡眠分配

---

## Softirqs and Tasklets

```
+------------------------------------------------------------------+
|  SOFTIRQ CONTEXT                                                 |
+------------------------------------------------------------------+

    After IRQ returns:
    
    +-------------------+
    |   do_softirq()    |
    +--------+----------+
             |
             v
    +--------+----------+
    |  NET_RX_SOFTIRQ   |  Network packet processing
    +-------------------+
    
    +--------+----------+
    |  TIMER_SOFTIRQ    |  Timer callbacks
    +-------------------+
    
    +--------+----------+
    |  TASKLET_SOFTIRQ  |  Tasklet execution
    +-------------------+
    
    CONSTRAINTS:
    +----------------------------------------------------------+
    | - Cannot sleep (no mutex, no GFP_KERNEL)                 |
    | - Cannot block on I/O                                    |
    | - Should complete quickly                                |
    | - Can be interrupted by hardware IRQs                    |
    | - Same softirq can run on multiple CPUs simultaneously   |
    +----------------------------------------------------------+
```

### Softirq vs Tasklet vs Workqueue

```
+------------------------------------------------------------------+
|  DEFERRED WORK MECHANISMS                                        |
+------------------------------------------------------------------+

    +-------------+     +-------------+     +-------------+
    |   SOFTIRQ   |     |   TASKLET   |     |  WORKQUEUE  |
    +-------------+     +-------------+     +-------------+
    |             |     |             |     |             |
    | Atomic      |     | Atomic      |     | Process     |
    | context     |     | context     |     | context     |
    |             |     |             |     |             |
    | Runs on     |     | Serial per  |     | Can sleep   |
    | all CPUs    |     | tasklet     |     |             |
    | concurrently|     |             |     | Can block   |
    |             |     | Cannot      |     |             |
    | Hard to use |     | run on      |     | Easy to use |
    | correctly   |     | multiple    |     |             |
    |             |     | CPUs        |     |             |
    +-------------+     +-------------+     +-------------+
           |                   |                   |
           v                   v                   v
    High performance     Medium             Can sleep
    High complexity      complexity         Lower perf
```

**中文解释：**
- **Softirq**：高性能、高复杂度、可多 CPU 并行
- **Tasklet**：中等复杂度、同一 tasklet 串行执行
- **Workqueue**：可睡眠、易用、性能较低

---

## Process Context

```
+------------------------------------------------------------------+
|  PROCESS CONTEXT                                                 |
+------------------------------------------------------------------+

    User space:         write(fd, buf, len)
                               |
                               v syscall
    +-------------------+
    |  Process Context  |
    |                   |
    | - current valid   |
    | - Can sleep       |
    | - Can page fault  |
    | - Preemptible     |
    +--------+----------+
             |
             | copy_from_user()
             | mutex_lock()
             | kmalloc(GFP_KERNEL)
             | schedule() - may sleep!
             |
             v
    +-------------------+
    |  Driver Code      |
    +-------------------+
             |
             v
    Return to user space
```

### Process Context Operations

```c
/* Process context - full freedom */
static ssize_t my_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    struct my_device *dev = file->private_data;
    char *kbuf;
    int ret;
    
    /* CAN sleep for allocation */
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
    
    /* CAN access user space */
    if (copy_from_user(kbuf, buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    
    /* CAN take sleeping locks */
    mutex_lock(&dev->mutex);
    
    /* CAN sleep waiting for I/O */
    ret = wait_event_interruptible(dev->wait,
                                   device_ready(dev));
    if (ret)
        goto unlock;
    
    /* Do the actual work */
    ret = do_write(dev, kbuf, count);
    
unlock:
    mutex_unlock(&dev->mutex);
out:
    kfree(kbuf);
    return ret;
}
```

**中文解释：**
- 进程上下文可以：
  1. 使用 `GFP_KERNEL` 分配（可睡眠）
  2. 访问用户空间（`copy_from_user`）
  3. 获取可睡眠锁（mutex）
  4. 等待事件（`wait_event`）

---

## might_sleep() and Debugging

```
+------------------------------------------------------------------+
|  might_sleep() - CONTEXT DEBUGGING                               |
+------------------------------------------------------------------+

    From include/linux/kernel.h:
    
    void __might_sleep(const char *file, int line, int preempt_offset);
    
    #define might_sleep() \
        do { __might_sleep(__FILE__, __LINE__, 0); } while (0)
    
    Purpose:
    +----------------------------------------------------------+
    | Called at the start of functions that MAY sleep          |
    | In debug builds: Warns if called from atomic context     |
    | Catches bugs BEFORE they cause deadlocks                 |
    +----------------------------------------------------------+
```

### Using might_sleep()

```c
/* Function that may sleep - document with might_sleep() */
void my_function_that_sleeps(void)
{
    might_sleep();  /* Debug assertion */
    
    mutex_lock(&my_mutex);  /* This sleeps */
    /* ... */
    mutex_unlock(&my_mutex);
}

/* What happens if called from wrong context */
irqreturn_t broken_irq_handler(int irq, void *dev_id)
{
    /* BUG: This will trigger might_sleep warning! */
    my_function_that_sleeps();
    
    /*
     * With CONFIG_DEBUG_ATOMIC_SLEEP:
     * "BUG: sleeping function called from invalid context"
     * Stack trace shows exactly where the bug is
     */
    
    return IRQ_HANDLED;
}
```

**中文解释：**
- `might_sleep()` 是调试断言
- 在原子上下文调用时会打印警告
- 帮助在问题发生前捕获错误

---

## Real Bugs Caused by Context Misuse

### Bug 1: Sleeping in IRQ Context

```c
/* BUG: GFP_KERNEL in IRQ handler */
irqreturn_t broken_handler(int irq, void *dev_id)
{
    char *buf = kmalloc(1024, GFP_KERNEL);  /* WILL DEADLOCK! */
    
    /*
     * GFP_KERNEL may sleep waiting for memory
     * But we're in IRQ context - cannot sleep
     * Result: System hangs
     */
    
    /* ... */
    kfree(buf);
    return IRQ_HANDLED;
}

/* CORRECT: Use GFP_ATOMIC */
irqreturn_t fixed_handler(int irq, void *dev_id)
{
    char *buf = kmalloc(1024, GFP_ATOMIC);  /* Non-sleeping */
    
    if (!buf)
        return IRQ_NONE;  /* Handle failure! */
    
    /* ... */
    kfree(buf);
    return IRQ_HANDLED;
}
```

### Bug 2: User Access in Softirq

```c
/* BUG: copy_to_user in softirq */
void broken_softirq_handler(unsigned long data)
{
    struct request *req = (struct request *)data;
    
    /* BUG: Cannot access user space from softirq! */
    copy_to_user(req->user_buf, kernel_data, len);
    
    /*
     * Problems:
     * 1. May page fault - cannot handle in atomic
     * 2. Wrong process context - current not meaningful
     * 3. Security: Wrong address space!
     */
}

/* CORRECT: Defer to workqueue */
void fixed_softirq_handler(unsigned long data)
{
    struct request *req = (struct request *)data;
    
    /* Queue work that runs in process context */
    queue_work(my_wq, &req->work);
}

void workqueue_handler(struct work_struct *work)
{
    struct request *req = container_of(work, struct request, work);
    
    /* NOW we can access user space */
    copy_to_user(req->user_buf, kernel_data, len);
}
```

### Bug 3: Mutex in Atomic Context

```c
/* BUG: mutex_lock with spinlock held */
void broken_function(void)
{
    spin_lock(&my_spinlock);      /* Disables preemption */
    
    mutex_lock(&my_mutex);        /* BUG: May sleep! */
    
    /*
     * mutex_lock may sleep if contended
     * But spinlock disables preemption
     * Result: Deadlock
     */
    
    mutex_unlock(&my_mutex);
    spin_unlock(&my_spinlock);
}

/* CORRECT: No sleeping locks inside spinlock */
void fixed_function(void)
{
    /* Take mutex first (can sleep) */
    mutex_lock(&my_mutex);
    
    /* Then spinlock (cannot sleep, but okay) */
    spin_lock(&my_spinlock);
    
    /* ... critical section ... */
    
    spin_unlock(&my_spinlock);
    mutex_unlock(&my_mutex);
}
```

### Bug 4: Sleeping with IRQs Disabled

```c
/* BUG: Sleeping with IRQs disabled */
void broken_irq_handling(void)
{
    unsigned long flags;
    
    spin_lock_irqsave(&my_lock, flags);  /* Disables IRQs */
    
    msleep(100);  /* BUG: Cannot sleep! */
    
    /*
     * IRQs disabled = timer IRQs blocked
     * Timer IRQs needed for scheduler
     * msleep needs scheduler
     * Result: System frozen
     */
    
    spin_unlock_irqrestore(&my_lock, flags);
}
```

**中文解释：**
- **Bug 1**：IRQ 中使用 `GFP_KERNEL` → 死锁
- **Bug 2**：软中断中访问用户空间 → 页错误处理失败
- **Bug 3**：持有自旋锁时使用 mutex → 死锁
- **Bug 4**：禁用中断时睡眠 → 系统冻结

---

## User-Space Async System Translation

```c
/* user_space_context_aware.c */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>

/*---------------------------------------------------------
 * Simulate kernel contexts in user space
 *---------------------------------------------------------*/

/* Thread-local context tracking */
__thread enum {
    CTX_NORMAL,      /* Like process context */
    CTX_SIGNAL,      /* Like IRQ context */
    CTX_ATOMIC       /* Like softirq - in critical section */
} current_context = CTX_NORMAL;

__thread int atomic_nesting = 0;

#define enter_atomic() do { \
    atomic_nesting++; \
    if (atomic_nesting == 1) \
        current_context = CTX_ATOMIC; \
} while (0)

#define exit_atomic() do { \
    atomic_nesting--; \
    if (atomic_nesting == 0) \
        current_context = CTX_NORMAL; \
} while (0)

/*---------------------------------------------------------
 * Context-aware operations
 *---------------------------------------------------------*/
void might_sleep(const char *func)
{
    if (current_context != CTX_NORMAL) {
        fprintf(stderr, 
                "BUG: %s called from atomic context!\n", func);
        /* In real code: abort() or log */
    }
}

void *safe_malloc(size_t size)
{
    might_sleep(__func__);  /* Check context */
    return malloc(size);
}

/* Non-blocking allocation - like GFP_ATOMIC */
void *atomic_malloc(size_t size)
{
    /* In real code: use memory pool */
    return malloc(size);  /* May fail, but won't block */
}

/*---------------------------------------------------------
 * Example: Event processing with contexts
 *---------------------------------------------------------*/
typedef void (*event_handler_t)(void *data);

struct event {
    event_handler_t handler;
    void *data;
};

/* Fast path - like softirq */
void process_event_atomic(struct event *e)
{
    enter_atomic();
    
    /* Cannot call sleeping functions here! */
    e->handler(e->data);
    
    exit_atomic();
}

/* Slow path - like workqueue */
void process_event_normal(struct event *e)
{
    /* Can sleep here */
    might_sleep(__func__);  /* Always passes in normal context */
    
    e->handler(e->data);
}

/*---------------------------------------------------------
 * Signal handler - like IRQ handler
 *---------------------------------------------------------*/
volatile sig_atomic_t pending_work = 0;

void signal_handler(int sig)
{
    /* Very restricted - like IRQ */
    /* Cannot call most functions! */
    /* Just set flag */
    pending_work = 1;
    
    /* DO NOT: malloc, printf, mutex_lock, etc. */
}

void main_loop(void)
{
    while (1) {
        if (pending_work) {
            pending_work = 0;
            
            /* Handle in process context */
            printf("Processing deferred work\n");
            void *buf = safe_malloc(1024);  /* OK here */
            free(buf);
        }
        
        /* ... other work ... */
        usleep(10000);
    }
}

/*---------------------------------------------------------
 * Demo
 *---------------------------------------------------------*/
void demo_handler(void *data)
{
    const char *msg = data;
    printf("Handler: %s\n", msg);
}

int main(void)
{
    struct event e = {
        .handler = demo_handler,
        .data = "test event"
    };
    
    printf("Normal context:\n");
    process_event_normal(&e);
    
    printf("\nAtomic context:\n");
    process_event_atomic(&e);
    
    printf("\nBug demonstration:\n");
    enter_atomic();
    safe_malloc(100);  /* This will warn! */
    exit_atomic();
    
    return 0;
}
```

**中文解释：**
- 用户态上下文模拟：
  1. 线程局部变量跟踪当前上下文
  2. `might_sleep()` 检查上下文
  3. 信号处理程序类似 IRQ（限制严格）
  4. 延迟工作在正常上下文处理

---

## Summary

```
+------------------------------------------------------------------+
|  CONTEXT-AWARE PROGRAMMING SUMMARY                               |
+------------------------------------------------------------------+

    CONTEXT HIERARCHY (most to least restricted):
    +----------------------------------------------------------+
    | 1. NMI Context     - Almost nothing allowed              |
    | 2. Hardware IRQ    - Minimal work, defer everything      |
    | 3. Softirq/Tasklet - Atomic alloc, no sleep              |
    | 4. Spinlock held   - No sleep, must be fast              |
    | 5. Process Context - Full freedom                        |
    +----------------------------------------------------------+
    
    KEY RULES:
    +----------------------------------------------------------+
    | Rule 1: Never sleep in atomic context                    |
    | Rule 2: Use GFP_ATOMIC in non-process context            |
    | Rule 3: Defer work from IRQ to softirq or workqueue      |
    | Rule 4: User space access only in process context        |
    | Rule 5: Document context requirements in comments        |
    +----------------------------------------------------------+
    
    DEBUGGING:
    +----------------------------------------------------------+
    | - Enable CONFIG_DEBUG_ATOMIC_SLEEP                       |
    | - Use might_sleep() at start of sleeping functions       |
    | - Watch for "scheduling while atomic" messages           |
    | - lockdep helps find lock ordering issues                |
    +----------------------------------------------------------+
    
    TRANSLATION TO USER SPACE:
    +----------------------------------------------------------+
    | - Signal handlers = IRQ handlers (very restricted)       |
    | - Critical sections = atomic context                     |
    | - Normal execution = process context                     |
    | - Use context tracking for debugging                     |
    +----------------------------------------------------------+
```

**中文总结：**
上下文感知编程是内核正确性的关键：
1. 上下文层次：NMI > 硬中断 > 软中断 > 自旋锁 > 进程上下文
2. 核心规则：原子上下文不睡眠、使用正确的分配标志、延迟工作
3. 调试工具：`CONFIG_DEBUG_ATOMIC_SLEEP`、`might_sleep()`、lockdep
4. 用户态映射：信号处理 = IRQ、临界区 = 原子上下文

