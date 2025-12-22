# Linux Kernel Lock Ordering & Deadlock Avoidance (v3.2)

## Overview

This document explains **lock ordering rules and lockdep** in Linux kernel v3.2, focusing on deadlock prevention and detection.

---

## Lock Ordering Rules

```
+------------------------------------------------------------------+
|  THE DEADLOCK PROBLEM                                            |
+------------------------------------------------------------------+

    CLASSIC DEADLOCK (ABBA):
    
    Thread 1                    Thread 2
    ─────────                   ─────────
    lock(A)                     lock(B)
         │                           │
         ▼                           ▼
    lock(B) ─── BLOCKED ──────▶ lock(A) ─── BLOCKED
         │                           │
         │◀────────────────────────▶│
         │     DEADLOCK!            │
    
    TIMELINE:
    
    Time    Thread 1        Thread 2        Locks held
    ────    ────────        ────────        ──────────
    T1      lock(A)                         A: T1
    T2                      lock(B)         A: T1, B: T2
    T3      lock(B) WAIT    lock(A) WAIT    DEADLOCK!

    SOLUTION: CONSISTENT ORDERING
    +----------------------------------------------------------+
    | Rule: Always acquire locks in the SAME order              |
    |                                                           |
    | If order is: A before B                                   |
    |                                                           |
    | Thread 1: lock(A) → lock(B) ✓                             |
    | Thread 2: lock(A) → lock(B) ✓                             |
    |                                                           |
    | No ABBA possible!                                         |
    +----------------------------------------------------------+
```

**中文解释：**
- 经典死锁（ABBA）：线程1持A等B，线程2持B等A
- 解决方案：一致的锁顺序 — 总是以相同顺序获取锁
- 如果顺序是 A 在 B 之前，所有线程都必须先锁A再锁B

---

## Kernel Lock Hierarchy

```
+------------------------------------------------------------------+
|  KERNEL LOCK ORDERING CONVENTION                                 |
+------------------------------------------------------------------+

    GENERAL HIERARCHY (outer to inner):
    
    ┌─────────────────────────────────────────────────────────────┐
    │  Outermost (acquire first)                                   │
    │                                                              │
    │  ├── Large granularity locks (subsystem level)               │
    │  │   ├── i_mutex (inode)                                    │
    │  │   ├── sb_lock (superblock)                               │
    │  │   └── ...                                                │
    │  │                                                          │
    │  ├── Medium granularity locks                                │
    │  │   ├── page_lock                                          │
    │  │   ├── address_space lock                                 │
    │  │   └── ...                                                │
    │  │                                                          │
    │  └── Fine granularity locks (innermost, acquire last)        │
    │      ├── rq->lock (per-CPU runqueue)                        │
    │      ├── atomic bitops                                      │
    │      └── ...                                                │
    │                                                              │
    │  Innermost (acquire last)                                    │
    └─────────────────────────────────────────────────────────────┘

    SPECIFIC EXAMPLES:
    +----------------------------------------------------------+
    | VFS:    i_mutex → i_lock → address_space->tree_lock       |
    | MM:     mmap_sem → page_lock → zone->lock                 |
    | Net:    socket_lock → sk_lock → queue_lock               |
    | Sched:  rq[cpu0]->lock → rq[cpu1]->lock (by CPU number)  |
    +----------------------------------------------------------+
```

**中文解释：**
- 锁层次结构：从外到内 — 大粒度锁先获取，小粒度锁后获取
- VFS：i_mutex → i_lock → tree_lock
- 调度器：按 CPU 编号顺序获取 rq->lock

---

## Lockdep Model

```
+------------------------------------------------------------------+
|  LOCKDEP: AUTOMATIC DEADLOCK DETECTION                           |
+------------------------------------------------------------------+

    WHAT LOCKDEP DOES:
    +----------------------------------------------------------+
    | 1. Tracks lock acquisition ORDER at runtime               |
    | 2. Builds DEPENDENCY GRAPH                                |
    | 3. Detects CYCLES = potential deadlock                    |
    | 4. Reports BEFORE actual deadlock occurs                  |
    +----------------------------------------------------------+

    DEPENDENCY GRAPH:
    
    Normal operation (no problem):
    
    lock(A) → lock(B) → unlock(B) → unlock(A)
    
    Creates edge: A → B (A before B)
    
    
    Later, somewhere else:
    
    lock(B) → lock(A) → ...  ← LOCKDEP WARNING!
    
    Would create edge: B → A
    But A → B already exists
    CYCLE DETECTED!

    GRAPH VISUALIZATION:
    
         ┌───┐         ┌───┐
         │ A │────────▶│ B │
         └───┘         └───┘
           ▲             │
           │             │  ← Adding this edge
           └─────────────┘    creates a cycle!

    LOCKDEP OUTPUT EXAMPLE:
    +----------------------------------------------------------+
    | ======================================================    |
    | [ INFO: possible circular locking dependency detected ]   |
    | 3.2.0-kernel #1                                          |
    | -------------------------------------------------------   |
    | process/1234 is trying to acquire lock:                  |
    |  (&lock_A){+.+.+.}, at: [<ffffffff81234567>] func_B      |
    |                                                          |
    | but task is already holding lock:                        |
    |  (&lock_B){+.+.+.}, at: [<ffffffff81234568>] func_A      |
    |                                                          |
    | which lock already depends on the new lock.              |
    ======================================================     |
    +----------------------------------------------------------+
```

**中文解释：**
- Lockdep：自动死锁检测
  1. 运行时跟踪锁获取顺序
  2. 构建依赖图
  3. 检测循环 = 潜在死锁
  4. 在实际死锁发生前报告
- 依赖图：锁A→B 表示先A后B，如果后来出现 B→A，形成循环

---

## Real Deadlock Scenarios

```
+------------------------------------------------------------------+
|  SCENARIO 1: Simple ABBA                                         |
+------------------------------------------------------------------+

    /* Thread 1 */              /* Thread 2 */
    mutex_lock(&lock_a);        mutex_lock(&lock_b);
    mutex_lock(&lock_b);        mutex_lock(&lock_a);
         ↓ DEADLOCK                  ↓

    FIX: Establish order (a before b)
    
    /* Both threads */
    mutex_lock(&lock_a);
    mutex_lock(&lock_b);
    ...
    mutex_unlock(&lock_b);
    mutex_unlock(&lock_a);

+------------------------------------------------------------------+
|  SCENARIO 2: Lock and Interrupt                                  |
+------------------------------------------------------------------+

    /* Process context */        /* IRQ handler */
    spin_lock(&my_lock);        spin_lock(&my_lock);
         │                           │
         │ ◀── IRQ occurs ───────────┘
         │      while holding lock
         ↓
    DEADLOCK (self-deadlock)

    FIX: Disable interrupts when taking lock in process context
    
    spin_lock_irqsave(&my_lock, flags);
    ...
    spin_unlock_irqrestore(&my_lock, flags);

+------------------------------------------------------------------+
|  SCENARIO 3: Recursive Mutex Attempt                             |
+------------------------------------------------------------------+

    mutex_lock(&my_mutex);
    some_function();
         │
         └──▶ mutex_lock(&my_mutex);  ← SAME MUTEX!
              ↓
         DEADLOCK (self-deadlock)

    FIX: Track lock ownership or use trylock

+------------------------------------------------------------------+
|  SCENARIO 4: Memory Allocation Under Lock                        |
+------------------------------------------------------------------+

    spin_lock(&my_lock);
    ptr = kmalloc(size, GFP_KERNEL);  ← CAN SLEEP!
         │
         │ Memory pressure → reclaim → needs same lock
         ↓
    DEADLOCK

    FIX: Use GFP_ATOMIC or allocate outside lock
    
    spin_lock(&my_lock);
    ptr = kmalloc(size, GFP_ATOMIC);  /* Won't sleep */
    spin_unlock(&my_lock);
```

**中文解释：**
- 场景1（ABBA）：建立固定顺序
- 场景2（锁和中断）：进程上下文关中断
- 场景3（递归）：跟踪锁所有权或使用 trylock
- 场景4（锁下分配）：用 GFP_ATOMIC 或在锁外分配

---

## Violation Detection

```
+------------------------------------------------------------------+
|  LOCKDEP ANNOTATIONS                                             |
+------------------------------------------------------------------+

    LOCK CLASS:
    +----------------------------------------------------------+
    | Each lock has a "class" - identified by code location     |
    | Locks of same class are considered equivalent             |
    |                                                           |
    | struct foo foos[100];                                     |
    | mutex_init(&foos[i].lock);  /* All same class */          |
    +----------------------------------------------------------+

    SUBCLASS (Nesting):
    +----------------------------------------------------------+
    | Sometimes same lock class needs different ordering        |
    |                                                           |
    | /* Acquiring child inode lock while holding parent */     |
    | mutex_lock(&parent->i_mutex);                             |
    | mutex_lock_nested(&child->i_mutex, I_MUTEX_CHILD);        |
    |                                                           |
    | Subclass tells lockdep this is expected nesting           |
    +----------------------------------------------------------+

    COMMON ANNOTATIONS:
    +----------------------------------------------------------+
    | lockdep_assert_held(&lock)  - Assert lock is held         |
    | lockdep_set_class(&lock, &key) - Set custom class         |
    | mutex_lock_nested(&lock, subclass) - Nested acquisition   |
    | lock_acquire() / lock_release() - Manual tracking         |
    +----------------------------------------------------------+
```

```
+------------------------------------------------------------------+
|  LOCKDEP STATE BITS                                              |
+------------------------------------------------------------------+

    Lock state encoding: {+.+.+.}
    
    Position 1: hardirq context usage
    Position 2: softirq context usage
    Position 3: reclaim context usage
    Position 4: ever used (debug)
    
    '+' = used with irqs enabled
    '-' = used with irqs disabled
    '?' = unknown
    '.' = not applicable

    UNSAFE PATTERNS DETECTED:
    +----------------------------------------------------------+
    | {+...} → {...-}  Lock taken in IRQ, also taken with      |
    |                  IRQs enabled elsewhere                   |
    |                  → Deadlock if IRQ while holding lock     |
    +----------------------------------------------------------+
```

**中文解释：**
- 锁类：按代码位置识别，同类锁被视为等效
- 子类（嵌套）：同类锁需要不同顺序时使用 mutex_lock_nested
- 状态位：编码硬中断/软中断/回收上下文使用情况

---

## User-Space Lock Ordering

```c
/* User-space lock ordering discipline */

#include <pthread.h>
#include <assert.h>

/* Lock ordering hierarchy (lower number = acquire first) */
#define LOCK_ORDER_GLOBAL   1
#define LOCK_ORDER_TABLE    2
#define LOCK_ORDER_ROW      3

struct ordered_mutex {
    pthread_mutex_t mutex;
    int order;
    const char *name;
};

/* Thread-local: highest order lock currently held */
static __thread int current_lock_order = 0;

void ordered_mutex_init(struct ordered_mutex *m, int order, 
                        const char *name)
{
    pthread_mutex_init(&m->mutex, NULL);
    m->order = order;
    m->name = name;
}

void ordered_mutex_lock(struct ordered_mutex *m)
{
    /* Check ordering discipline */
    if (current_lock_order >= m->order) {
        fprintf(stderr, "LOCK ORDER VIOLATION: "
                "holding order %d, trying to acquire %s (order %d)\n",
                current_lock_order, m->name, m->order);
        assert(0);  /* Crash - like lockdep BUG */
    }
    
    pthread_mutex_lock(&m->mutex);
    current_lock_order = m->order;
}

void ordered_mutex_unlock(struct ordered_mutex *m)
{
    pthread_mutex_unlock(&m->mutex);
    /* Reset to 0 (simplification - real impl tracks stack) */
    current_lock_order = 0;
}

/* Example usage */
struct database {
    struct ordered_mutex global_lock;
    struct ordered_mutex table_locks[10];
    struct ordered_mutex row_locks[100];
};

void db_init(struct database *db)
{
    ordered_mutex_init(&db->global_lock, LOCK_ORDER_GLOBAL, "global");
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "table_%d", i);
        ordered_mutex_init(&db->table_locks[i], LOCK_ORDER_TABLE, 
                          strdup(name));
    }
    for (int i = 0; i < 100; i++) {
        char name[32];
        snprintf(name, sizeof(name), "row_%d", i);
        ordered_mutex_init(&db->row_locks[i], LOCK_ORDER_ROW, 
                          strdup(name));
    }
}

/* Correct usage */
void update_row(struct database *db, int table, int row)
{
    /* Acquire in order: global → table → row */
    ordered_mutex_lock(&db->global_lock);
    ordered_mutex_lock(&db->table_locks[table]);
    ordered_mutex_lock(&db->row_locks[row]);
    
    /* Do work... */
    
    /* Release in reverse order */
    ordered_mutex_unlock(&db->row_locks[row]);
    ordered_mutex_unlock(&db->table_locks[table]);
    ordered_mutex_unlock(&db->global_lock);
}

/* Wrong usage - will trigger assertion */
void wrong_update(struct database *db, int table, int row)
{
    ordered_mutex_lock(&db->row_locks[row]);
    ordered_mutex_lock(&db->table_locks[table]);  /* VIOLATION! */
}
```

**中文解释：**
- 用户态锁顺序约束：
  1. 每个锁有顺序号
  2. 线程跟踪当前最高顺序
  3. 获取锁时检查顺序约束
  4. 违反时断言失败（类似 lockdep BUG）
- 示例：数据库锁层次 — global → table → row

