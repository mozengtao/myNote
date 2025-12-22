# Linux Kernel Wait Queues and Completions (v3.2)

## Overview

This document explains **wait queues and completions** in Linux kernel v3.2, focusing on blocking semantics and wakeup protocols.

---

## Wait Queue Design

From `include/linux/wait.h`:

```c
struct __wait_queue_head {
    spinlock_t lock;
    struct list_head task_list;
};
typedef struct __wait_queue_head wait_queue_head_t;

struct __wait_queue {
    unsigned int flags;
    void *private;           /* Usually current task */
    wait_queue_func_t func;  /* Wakeup function */
    struct list_head task_list;
};
typedef struct __wait_queue wait_queue_t;
```

```
+------------------------------------------------------------------+
|  WAIT QUEUE ARCHITECTURE                                         |
+------------------------------------------------------------------+

    wait_queue_head_t (event source)
    ┌─────────────────────────────────────────────────────────────┐
    │  spinlock_t lock          ← Protects the list               │
    │  ┌─────────────────────────────────────────────────────┐    │
    │  │  task_list (circular doubly-linked)                 │    │
    │  │                                                     │    │
    │  │  ┌──────────┐   ┌──────────┐   ┌──────────┐        │    │
    │  │  │ waiter 1 │◀─▶│ waiter 2 │◀─▶│ waiter 3 │        │    │
    │  │  │ (task A) │   │ (task B) │   │ (task C) │        │    │
    │  │  └──────────┘   └──────────┘   └──────────┘        │    │
    │  └─────────────────────────────────────────────────────┘    │
    └─────────────────────────────────────────────────────────────┘

    WAITER STRUCTURE:
    +----------------------------------------------------------+
    | unsigned int flags:                                       |
    |   WQ_FLAG_EXCLUSIVE - Only wake one waiter               |
    |                                                          |
    | void *private:                                           |
    |   Pointer to task_struct of sleeping task                |
    |                                                          |
    | wait_queue_func_t func:                                  |
    |   Function to call on wakeup (usually default_wake_func) |
    +----------------------------------------------------------+
```

**中文解释：**
- wait_queue_head_t：事件源，包含自旋锁和等待者列表
- 等待者结构：标志（独占唤醒）、私有数据（任务指针）、唤醒函数
- 等待者通过双向链表链接

---

## Sleep vs Spin Decision

```
+------------------------------------------------------------------+
|  WHEN TO SLEEP vs SPIN                                           |
+------------------------------------------------------------------+

    SPIN (busy-wait):
    +----------------------------------------------------------+
    | while (!condition)                                        |
    |     cpu_relax();  /* Burn CPU cycles */                   |
    |                                                           |
    | Use when:                                                 |
    | - Expected wait is VERY short (< few microseconds)        |
    | - In interrupt context (cannot sleep)                     |
    | - Holding spinlock                                        |
    |                                                           |
    | Cost: Wastes CPU, prevents other work                     |
    +----------------------------------------------------------+

    SLEEP (block):
    +----------------------------------------------------------+
    | wait_event(wq, condition);  /* Give up CPU */             |
    |                                                           |
    | Use when:                                                 |
    | - Expected wait is longer (> milliseconds)                |
    | - In process context                                      |
    | - Not holding spinlocks                                   |
    |                                                           |
    | Cost: Context switch overhead (~microseconds)             |
    +----------------------------------------------------------+

    DECISION DIAGRAM:
    
    ┌──────────────────┐
    │ Need to wait?    │
    └────────┬─────────┘
             │
    ┌────────┴─────────┐
    │ In interrupt     │─── YES ──▶ SPIN (no choice)
    │ context?         │
    └────────┬─────────┘
             │ NO
             ▼
    ┌──────────────────┐
    │ Holding          │─── YES ──▶ SPIN (cannot sleep)
    │ spinlock?        │
    └────────┬─────────┘
             │ NO
             ▼
    ┌──────────────────┐
    │ Wait time        │─── < 10μs ──▶ SPIN (cheaper)
    │ expected?        │
    └────────┬─────────┘
             │ > 10μs
             ▼
         SLEEP (efficient)
```

**中文解释：**
- 自旋：等待时间极短、中断上下文、持有自旋锁
- 睡眠：等待时间较长、进程上下文、未持有自旋锁
- 决策：中断上下文→自旋、持有锁→自旋、等待>10μs→睡眠

---

## Wait Event Pattern

```c
/* Standard wait pattern */
wait_event(wq, condition);

/* Expands to approximately: */
while (!condition) {
    DEFINE_WAIT(wait);
    prepare_to_wait(&wq, &wait, TASK_UNINTERRUPTIBLE);
    if (!condition)
        schedule();
    finish_wait(&wq, &wait);
}
```

```
+------------------------------------------------------------------+
|  WAIT_EVENT TIMELINE                                             |
+------------------------------------------------------------------+

    Waiting Task                     Waking Task
    ─────────────                    ─────────────
    
    prepare_to_wait()
    │  - Add to wait queue
    │  - Set state = TASK_UNINTERRUPTIBLE
    │
    ▼
    check condition ────── FALSE
    │
    ▼
    schedule()
    │  - Context switch away
    │  - Task is sleeping
    │                                condition = true
    │                                │
    │                                ▼
    │                                wake_up(&wq)
    │                                │  - Find waiting tasks
    │                                │  - Set state = TASK_RUNNING
    │◀────────────────────────────────  - Schedule if needed
    │
    ▼
    finish_wait()
    │  - Remove from wait queue
    │
    ▼
    check condition ────── TRUE
    │
    ▼
    Continue execution

    CRITICAL ORDERING:
    +----------------------------------------------------------+
    | 1. Add to wait queue BEFORE checking condition            |
    | 2. Set sleeping state BEFORE checking condition           |
    | 3. Why? Avoid lost wakeup race!                           |
    +----------------------------------------------------------+
```

**中文解释：**
- 等待时间线：准备等待 → 检查条件 → 调度 → 被唤醒 → 完成等待 → 条件成立
- 关键顺序：先加入等待队列，再检查条件（避免丢失唤醒）

---

## Wakeup Ordering

```
+------------------------------------------------------------------+
|  WAKEUP SEMANTICS                                                |
+------------------------------------------------------------------+

    wake_up(&wq):
    +----------------------------------------------------------+
    | - Wakes ALL non-exclusive waiters                         |
    | - Wakes ONE exclusive waiter (first in queue)             |
    +----------------------------------------------------------+

    wake_up_all(&wq):
    +----------------------------------------------------------+
    | - Wakes ALL waiters (exclusive and non-exclusive)         |
    +----------------------------------------------------------+

    wake_up_interruptible(&wq):
    +----------------------------------------------------------+
    | - Only wakes TASK_INTERRUPTIBLE waiters                   |
    | - Ignores TASK_UNINTERRUPTIBLE waiters                    |
    +----------------------------------------------------------+

    EXCLUSIVE vs NON-EXCLUSIVE:
    
    ┌─────────────────────────────────────────────────────────────┐
    │  Wait Queue                                                  │
    │  ┌──────────┬──────────┬──────────┬──────────┬──────────┐  │
    │  │ Waiter 1 │ Waiter 2 │ Waiter 3 │ Waiter 4 │ Waiter 5 │  │
    │  │ (excl)   │ (excl)   │ (normal) │ (normal) │ (excl)   │  │
    │  └──────────┴──────────┴──────────┴──────────┴──────────┘  │
    │                                                              │
    │  wake_up():                                                  │
    │  - Wake Waiter 3 (normal) ✓                                 │
    │  - Wake Waiter 4 (normal) ✓                                 │
    │  - Wake Waiter 1 (excl) ✓   ← First exclusive               │
    │  - Stop after first exclusive                               │
    │                                                              │
    │  wake_up_all():                                              │
    │  - Wake ALL waiters ✓ ✓ ✓ ✓ ✓                               │
    └─────────────────────────────────────────────────────────────┘

    THUNDERING HERD PREVENTION:
    +----------------------------------------------------------+
    | Problem: 100 tasks wait for one event, all wake up        |
    | Solution: Exclusive waiters - only one wakes              |
    +----------------------------------------------------------+
```

**中文解释：**
- wake_up：唤醒所有非独占等待者 + 第一个独占等待者
- wake_up_all：唤醒所有等待者
- wake_up_interruptible：只唤醒可中断等待者
- 独占等待者：防止惊群（thundering herd）

---

## Completion Structure

```c
struct completion {
    unsigned int done;
    wait_queue_head_t wait;
};

void init_completion(struct completion *c);
void wait_for_completion(struct completion *c);
void complete(struct completion *c);
void complete_all(struct completion *c);
```

```
+------------------------------------------------------------------+
|  COMPLETION: SIMPLIFIED WAIT/SIGNAL                              |
+------------------------------------------------------------------+

    COMPLETION vs WAIT_QUEUE:
    +----------------------------------------------------------+
    | Wait Queue:                                               |
    | - Generic, flexible                                       |
    | - User manages condition                                  |
    | - Must handle spurious wakeups                            |
    |                                                           |
    | Completion:                                               |
    | - Simple one-shot signal                                  |
    | - Built-in "done" counter                                 |
    | - No spurious wakeups                                     |
    | - Perfect for "wait for task to finish"                   |
    +----------------------------------------------------------+

    USAGE PATTERN:
    
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  DECLARE_COMPLETION(work_done);                              │
    │                                                              │
    │  Initiator                        Worker                     │
    │  ──────────                       ──────────                 │
    │  start_worker();                  do_work();                 │
    │       │                                │                     │
    │       ▼                                ▼                     │
    │  wait_for_completion(&work_done);  complete(&work_done);    │
    │       │                                │                     │
    │       │◀───────────────────────────────┘                    │
    │       │  (woken up)                                         │
    │       ▼                                                      │
    │  use_result();                                               │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    REINIT:
    +----------------------------------------------------------+
    | reinit_completion(&c);  /* Reset for reuse */             |
    +----------------------------------------------------------+
```

**中文解释：**
- Completion 比 wait_queue 更简单：内置完成计数、无虚假唤醒
- 使用模式：启动工作者 → 等待完成 → 工作者完成 → 唤醒发起者
- reinit_completion 可重置以重用

---

## Real Kernel Examples

```
+------------------------------------------------------------------+
|  KERNEL WAIT QUEUE EXAMPLES                                      |
+------------------------------------------------------------------+

    1. BLOCK I/O: Wait for I/O completion
    +----------------------------------------------------------+
    | wait_queue_head_t bh_waitq;                               |
    |                                                           |
    | /* In submit_bh() */                                      |
    | wait_event(bh_waitq, buffer_uptodate(bh));                |
    |                                                           |
    | /* In I/O completion interrupt */                         |
    | wake_up(&bh_waitq);                                       |
    +----------------------------------------------------------+
    
    2. PIPE: Reader waits for data
    +----------------------------------------------------------+
    | /* In pipe_read() */                                      |
    | wait_event_interruptible(pipe->wait,                      |
    |     pipe_empty(pipe));                                    |
    |                                                           |
    | /* In pipe_write() */                                     |
    | wake_up_interruptible(&pipe->wait);                       |
    +----------------------------------------------------------+
    
    3. KTHREAD: Wait for thread creation
    +----------------------------------------------------------+
    | DECLARE_COMPLETION(done);                                 |
    |                                                           |
    | kthread = kthread_create(thread_fn, data, "name");        |
    | /* Internally uses completion */                          |
    |                                                           |
    | wait_for_completion(&done);                               |
    +----------------------------------------------------------+
    
    4. MODULE: Wait for module users to exit
    +----------------------------------------------------------+
    | DECLARE_COMPLETION(done);                                 |
    |                                                           |
    | /* In module_put() when count reaches 0 */                |
    | complete(&module->exit_completion);                       |
    |                                                           |
    | /* In delete_module() */                                  |
    | wait_for_completion(&module->exit_completion);            |
    +----------------------------------------------------------+
```

**中文解释：**
- 块 I/O：等待 I/O 完成
- 管道：读者等待数据
- 内核线程：等待线程创建
- 模块：等待模块用户退出

---

## User-Space Condition Variable

```c
/* User-space wait/signal inspired by kernel wait queues */

#include <pthread.h>
#include <stdbool.h>

/* Equivalent to wait_queue_head_t */
struct wait_queue {
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

#define WAIT_QUEUE_INIT { PTHREAD_MUTEX_INITIALIZER, \
                          PTHREAD_COND_INITIALIZER }

/* Equivalent to wait_event() */
#define wait_event(wq, condition) do {              \
    pthread_mutex_lock(&(wq)->lock);                \
    while (!(condition)) {                          \
        pthread_cond_wait(&(wq)->cond,              \
                          &(wq)->lock);             \
    }                                               \
    pthread_mutex_unlock(&(wq)->lock);              \
} while (0)

/* Equivalent to wake_up() */
#define wake_up(wq) do {                            \
    pthread_mutex_lock(&(wq)->lock);                \
    pthread_cond_signal(&(wq)->cond);               \
    pthread_mutex_unlock(&(wq)->lock);              \
} while (0)

/* Equivalent to wake_up_all() */
#define wake_up_all(wq) do {                        \
    pthread_mutex_lock(&(wq)->lock);                \
    pthread_cond_broadcast(&(wq)->cond);            \
    pthread_mutex_unlock(&(wq)->lock);              \
} while (0)

/* Equivalent to completion */
struct completion {
    bool done;
    struct wait_queue wq;
};

#define COMPLETION_INIT { false, WAIT_QUEUE_INIT }

void init_completion(struct completion *c)
{
    c->done = false;
    pthread_mutex_init(&c->wq.lock, NULL);
    pthread_cond_init(&c->wq.cond, NULL);
}

void wait_for_completion(struct completion *c)
{
    pthread_mutex_lock(&c->wq.lock);
    while (!c->done) {
        pthread_cond_wait(&c->wq.cond, &c->wq.lock);
    }
    pthread_mutex_unlock(&c->wq.lock);
}

void complete(struct completion *c)
{
    pthread_mutex_lock(&c->wq.lock);
    c->done = true;
    pthread_cond_signal(&c->wq.cond);
    pthread_mutex_unlock(&c->wq.lock);
}

void complete_all(struct completion *c)
{
    pthread_mutex_lock(&c->wq.lock);
    c->done = true;
    pthread_cond_broadcast(&c->wq.cond);
    pthread_mutex_unlock(&c->wq.lock);
}

void reinit_completion(struct completion *c)
{
    pthread_mutex_lock(&c->wq.lock);
    c->done = false;
    pthread_mutex_unlock(&c->wq.lock);
}

/* Example: Producer-consumer with wait queue */
struct buffer {
    int data[10];
    int count;
    struct wait_queue not_empty;
    struct wait_queue not_full;
};

void producer(struct buffer *buf, int item)
{
    wait_event(&buf->not_full, buf->count < 10);
    
    pthread_mutex_lock(&buf->not_full.lock);
    buf->data[buf->count++] = item;
    pthread_mutex_unlock(&buf->not_full.lock);
    
    wake_up(&buf->not_empty);
}

int consumer(struct buffer *buf)
{
    wait_event(&buf->not_empty, buf->count > 0);
    
    pthread_mutex_lock(&buf->not_empty.lock);
    int item = buf->data[--buf->count];
    pthread_mutex_unlock(&buf->not_empty.lock);
    
    wake_up(&buf->not_full);
    return item;
}
```

**中文解释：**
- 用户态等待队列：使用 pthread_cond 实现
- wait_event：条件变量等待
- wake_up：pthread_cond_signal
- wake_up_all：pthread_cond_broadcast
- Completion：简化的一次性信号

