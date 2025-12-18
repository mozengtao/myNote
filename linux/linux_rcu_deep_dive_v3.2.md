# Linux 内核 RCU (Read-Copy-Update) 深度解析 (v3.2)

## 目录
1. [High-level Motivation & Mental Model](#1-high-level-motivation--mental-model)
2. [Core RCU Guarantees and Rules](#2-core-rcu-guarantees-and-rules)
3. [Fundamental APIs and Semantics](#3-fundamental-apis-and-semantics)
4. [RCU Grace Period Mechanics](#4-rcu-grace-period-mechanics)
5. [RCU Flavors and Their Differences](#5-rcu-flavors-and-their-differences)
6. [Update-side Lifecycle Walkthrough](#6-update-side-lifecycle-walkthrough)
7. [Reader-side Execution Model](#7-reader-side-execution-model)
8. [Memory Ordering & Barriers](#8-memory-ordering--barriers)
9. [Real Kernel Usage Examples](#9-real-kernel-usage-examples)
10. [Common RCU Bugs and Anti-patterns](#10-common-rcu-bugs-and-anti-patterns)
11. [Minimal Self-contained Example](#11-minimal-self-contained-example)
12. [Design Philosophy & Intuition](#12-design-philosophy--intuition)
13. [Summary and Learning Checklist](#13-summary-and-learning-checklist)
14. [RCU + Reference Counting: Why They Must Be Combined](#14-rcu--reference-counting-why-they-must-be-combined)

---

## 1. High-level Motivation & Mental Model

### 1.1 What Problem RCU Solves

RCU solves the fundamental tension between **read performance** and **update correctness** in concurrent data structures.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                   THE PROBLEM: Read-Mostly Workloads                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Scenario: Routing table with 1000 reads/second, 1 update/minute            │
│                                                                             │
│  Traditional Locks:                                                         │
│  ─────────────────                                                          │
│    Reader 1 ──► [acquire lock] ──► [read] ──► [release lock]               │
│    Reader 2 ──► [acquire lock] ──► [read] ──► [release lock]               │
│    Reader 3 ──► [acquire lock] ──► [read] ──► [release lock]               │
│         ...                                                                 │
│    Writer   ──► [acquire lock] ──► [write] ──► [release lock]              │
│                                                                             │
│  Problems:                                                                  │
│    • Lock acquisition = memory barrier + atomic operation + cache bounce   │
│    • Even reader-writer locks require atomic counter increment             │
│    • Lock contention on multi-core systems kills scalability               │
│    • 99.9% reads paying penalty for 0.1% writes                            │
│                                                                             │
│  RCU Solution:                                                              │
│  ─────────────                                                              │
│    Reader 1 ──► [直接读取，无锁]                                             │
│    Reader 2 ──► [直接读取，无锁]                                             │
│    Reader 3 ──► [直接读取，无锁]                                             │
│    Writer   ──► [复制→修改→发布指针→等待宽限期→释放旧数据]                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**为什么传统锁无法解决这个问题？**

- **读写锁 (rwlock)**: 即使只是读取，也需要原子操作来增减读者计数
- **自旋锁 (spinlock)**: 所有访问者互斥，读者之间也会竞争
- **顺序锁 (seqlock)**: 写者可能无限次重试读者

**RCU 的关键洞察**：如果能让读者完全无需同步操作，只让写者承担同步开销，那么对于读多写少的场景，整体性能将大幅提升。

### 1.2 RCU vs Spinlock vs RWLock

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SEMANTIC COMPARISON (Not API)                            │
├─────────────────┬───────────────┬───────────────┬───────────────────────────┤
│                 │   Spinlock    │   RWLock      │   RCU                     │
├─────────────────┼───────────────┼───────────────┼───────────────────────────┤
│ Reader blocks   │     Yes       │     No*       │     Never                 │
│ on reader       │               │               │                           │
├─────────────────┼───────────────┼───────────────┼───────────────────────────┤
│ Reader blocks   │     Yes       │     Yes       │     Never                 │
│ on writer       │               │               │                           │
├─────────────────┼───────────────┼───────────────┼───────────────────────────┤
│ Writer blocks   │     Yes       │     Yes       │     Yes (on old readers)  │
│ on reader       │               │               │                           │
├─────────────────┼───────────────┼───────────────┼───────────────────────────┤
│ Read-side       │   Expensive   │   Moderate    │   Near-zero               │
│ overhead        │  (atomic op)  │  (atomic op)  │   (preempt_disable)       │
├─────────────────┼───────────────┼───────────────┼───────────────────────────┤
│ Reader sees     │   Current     │   Current     │   Might see old data      │
│ which data?     │   data        │   data        │   (but consistent!)       │
├─────────────────┼───────────────┼───────────────┼───────────────────────────┤
│ Space overhead  │     None      │     None      │   Extra copy during       │
│                 │               │               │   update                  │
└─────────────────┴───────────────┴───────────────┴───────────────────────────┘

* RWLock readers don't block each other, but atomic counter ops still cause
  cache-line bouncing on SMP systems.
```

**语义差异的关键点**:

1. **RCU 读者可能看到旧数据** - 但保证看到的是一致的旧数据或新数据，永远不会是"一半旧一半新"
2. **RCU 写者需要额外空间** - 因为旧数据在宽限期内不能释放
3. **RCU 写者延迟释放** - 这是性能换取读者零开销的代价

### 1.3 What "Grace Period" Really Means

```
═══════════════════════════════════════════════════════════════════════════════
                    PRECISE DEFINITION OF GRACE PERIOD
═══════════════════════════════════════════════════════════════════════════════

A grace period is a time interval during which EVERY CPU has passed through
at least one QUIESCENT STATE.

Quiescent State (静止状态) =
  A CPU state where we KNOW the CPU cannot be holding an RCU reference

In classic (non-preemptible) RCU:
  • Context switch (schedule())
  • Return to user mode  
  • CPU idle loop entry
  • Explicit rcu_read_unlock() completion

Why does passing through a quiescent state matter?
─────────────────────────────────────────────────
  RCU read-side critical sections cannot span quiescent states.
  
  Therefore, if CPU X passes through a quiescent state after time T,
  any RCU read-side critical section that started before time T
  must have completed.

  If ALL CPUs pass through quiescent states after time T,
  then ALL pre-existing read-side critical sections have completed.
  
═══════════════════════════════════════════════════════════════════════════════


                     GRACE PERIOD TIMELINE
═══════════════════════════════════════════════════════════════════════════════

 Time ────────────────────────────────────────────────────────────────────────►
      │                                                                        
      │   T0: call_rcu() registers callback                                   
      │    │                                                                   
      │    ▼                                                                   
      │   ┌───────────────────────────────────────────────────────────────┐   
      │   │                   GRACE PERIOD                                │   
      │   │                                                               │   
      │   │ CPU 0: ═══[RCU read]═══╗                                      │   
      │   │                        ║ context switch (QS)                  │   
      │   │                        ╚══════════════════════════════►       │   
      │   │                                                               │   
      │   │ CPU 1: ════════════════╗ returns to usermode (QS)             │   
      │   │                        ╚══════════════════════════════►       │   
      │   │                                                               │   
      │   │ CPU 2: ═══════════════════════╗ idle entry (QS)               │   
      │   │                               ╚═══════════════════════►       │   
      │   │                               │                               │   
      │   │                               ▼                               │   
      │   │                        Last CPU reports QS                    │   
      │   └───────────────────────────────┬───────────────────────────────┘   
      │                                   │                                    
      │                                   ▼                                    
      │                            T1: Grace period ends                       
      │                                   │                                    
      │                                   ▼                                    
      │                            callback() invoked                          
      │                            kfree(old_data) safe                        
      │                                                                        
═══════════════════════════════════════════════════════════════════════════════
```

**类比理解宽限期**:

想象一个图书馆，书架上有一本旧书要换成新版:

1. **管理员**: 先把新版放上书架 (rcu_assign_pointer)
2. **等待期**: 确保所有正在阅读旧版的读者都还回去了 (grace period)
3. **销毁旧版**: 确认没人在读了，才能销毁旧版 (kfree after grace period)

关键是：新来的读者可能拿到新版或旧版（取决于时机），但管理员只需要等待"当时"正在阅读的读者。

---

## 2. Core RCU Guarantees and Rules

### 2.1 Reader-side Guarantees

```c
/*
 * RCU 对读者的承诺:
 *
 * 1. 零阻塞保证 (Zero Blocking)
 *    - 读者永远不会因为写者或其他读者而阻塞
 *    - rcu_read_lock() 不是真正的锁
 *
 * 2. 一致性保证 (Consistency)
 *    - 读者要么看到完整的旧数据，要么看到完整的新数据
 *    - 永远不会看到中间状态
 *
 * 3. 存在性保证 (Existence)
 *    - 在 RCU 读临界区内，通过 rcu_dereference() 获取的指针指向的数据
 *      保证在整个临界区内有效，不会被释放
 */
 
rcu_read_lock();                           /* 进入 RCU 读临界区 */
p = rcu_dereference(global_ptr);           /* 安全获取指针 */
if (p) {
    /* p 指向的数据在整个临界区内保证有效 */
    /* 可以安全读取 p->field1, p->field2, ... */
    do_something_with(p);
}
rcu_read_unlock();                         /* 离开 RCU 读临界区 */
/* 此时 p 可能已经无效，不能再使用！ */
```

### 2.2 Writer-side Guarantees

```c
/*
 * RCU 对写者的承诺:
 *
 * 1. 发布-订阅保证 (Publish-Subscribe)
 *    - rcu_assign_pointer() 之后，新读者可能看到新数据
 *    - 保证新数据的所有初始化在指针发布之前完成
 *
 * 2. 宽限期保证 (Grace Period)
 *    - synchronize_rcu() 返回后，所有在调用之前开始的读临界区都已结束
 *    - call_rcu() 的回调在宽限期后执行
 *
 * 3. 内存排序保证 (Memory Ordering)
 *    - rcu_assign_pointer() 包含写屏障
 *    - synchronize_rcu() 包含完整内存屏障
 */

/* 更新端代码模式 */
struct foo *new_fp = kmalloc(sizeof(*new_fp), GFP_KERNEL);
new_fp->field1 = value1;                   /* 初始化新数据 */
new_fp->field2 = value2;
/* ↓ smp_wmb() 隐含在 rcu_assign_pointer() 中 */
old_fp = rcu_dereference_protected(global_ptr, lockdep_is_held(&my_lock));
rcu_assign_pointer(global_ptr, new_fp);    /* 发布新指针 */
synchronize_rcu();                         /* 等待宽限期 */
/* 此时所有旧读者都已退出 */
kfree(old_fp);                             /* 安全释放旧数据 */
```

### 2.3 What RCU Does NOT Guarantee

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    RCU 不保证以下事项                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 1. 读者立即看到更新                                                          │
│    ───────────────────                                                      │
│    X  rcu_assign_pointer(p, new) 之后，读者可能仍看到旧值                    │
│    ✓  最终一致性: 新读者最终会看到新值                                       │
│                                                                             │
│ 2. 写者之间的互斥                                                            │
│    ─────────────────                                                        │
│    X  RCU 不提供写者之间的同步                                               │
│    ✓  需要额外的锁 (spinlock/mutex) 来保护更新操作                          │
│                                                                             │
│ 3. 读者修改数据                                                              │
│    ─────────────                                                            │
│    X  RCU 读临界区内不能修改 RCU 保护的数据                                  │
│    ✓  只能读取                                                               │
│                                                                             │
│ 4. 跨宽限期的指针有效性                                                      │
│    ─────────────────────                                                    │
│    X  在 rcu_read_unlock() 后，之前获取的指针可能无效                        │
│    ✓  指针只在获取它的 RCU 读临界区内有效                                    │
│                                                                             │
│ 5. 实时性保证                                                                │
│    ───────────                                                              │
│    X  宽限期的长度不确定                                                     │
│    ✓  通常是毫秒级，但无硬性保证                                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.4 Formal RCU Invariants

```
═══════════════════════════════════════════════════════════════════════════════
                        RCU INVARIANTS (不变量)
═══════════════════════════════════════════════════════════════════════════════

INVARIANT 1: Read-side Critical Section Rules
─────────────────────────────────────────────
  • rcu_read_lock() 和 rcu_read_unlock() 必须配对
  • 在非抢占 RCU 中，临界区内不能睡眠
  • 临界区可以嵌套
  • 通过 rcu_dereference() 获取的指针只在临界区内有效

INVARIANT 2: Update-side Rules  
──────────────────────────────
  • 新数据必须在 rcu_assign_pointer() 之前完全初始化
  • 旧数据必须在宽限期之后才能释放
  • 多个写者需要外部同步 (如 spinlock)

INVARIANT 3: Memory Ordering Rules
──────────────────────────────────
  • rcu_assign_pointer(p, v):
      smp_wmb();  // 确保 v 的初始化先于 p 的赋值
      ACCESS_ONCE(p) = v;
      
  • rcu_dereference(p):
      tmp = ACCESS_ONCE(p);
      smp_read_barrier_depends();  // Alpha 架构需要
      return tmp;

INVARIANT 4: Grace Period Property
──────────────────────────────────
  对于任意时刻 T 调用的 synchronize_rcu():
  • 在 T 之前开始的所有 RCU 读临界区必须在 synchronize_rcu() 返回前结束
  • synchronize_rcu() 返回后开始的 RCU 读临界区不受影响
  
═══════════════════════════════════════════════════════════════════════════════
```

---

## 3. Fundamental APIs and Semantics

### 3.1 rcu_read_lock() / rcu_read_unlock()

```c
/* include/linux/rcupdate.h */

/**
 * rcu_read_lock() - 标记 RCU 读临界区的开始
 *
 * 关键点: 这不是一个真正的锁！
 */
static inline void rcu_read_lock(void)
{
    __rcu_read_lock();      /* 实际实现取决于 RCU 类型 */
    __acquire(RCU);         /* Sparse 静态检查标记 */
    rcu_read_acquire();     /* lockdep 调试支持 */
}

/* 在非抢占 RCU (CONFIG_TREE_RCU) 中: */
static inline void __rcu_read_lock(void)
{
    preempt_disable();      /* 仅仅禁用抢占！ */
}

static inline void __rcu_read_unlock(void)
{
    preempt_enable();       /* 仅仅启用抢占！ */
}
```

**为什么 rcu_read_lock() 不是真正的锁？**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│              rcu_read_lock() vs 真正的锁                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  真正的锁 (spinlock):                                                       │
│  ────────────────────                                                       │
│    spin_lock(&lock);                                                        │
│    │                                                                        │
│    ├─ 1. 原子操作尝试获取锁                                                  │
│    ├─ 2. 失败则自旋等待                                                      │
│    ├─ 3. 获取后修改全局锁状态                                                │
│    └─ 4. 包含完整内存屏障                                                    │
│                                                                             │
│  rcu_read_lock() (非抢占内核):                                              │
│  ───────────────────────────                                                │
│    rcu_read_lock();                                                         │
│    │                                                                        │
│    └─ preempt_disable();  // 仅仅增加 per-CPU 的 preempt_count              │
│                           // 没有原子操作                                    │
│                           // 没有全局状态修改                                │
│                           // 没有自旋等待                                    │
│                           // 超快！                                          │
│                                                                             │
│  开销对比 (大约):                                                            │
│    spin_lock:      100+ cycles (有竞争时更多)                               │
│    rcu_read_lock:  2-3 cycles  (纯本地操作)                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 rcu_dereference()

```c
/* include/linux/rcupdate.h */

#define __rcu_dereference_check(p, c, space) \
    ({ \
        typeof(*p) *_________p1 = (typeof(*p)*__force )ACCESS_ONCE(p); \
        rcu_lockdep_assert(c, "suspicious rcu_dereference_check()" \
                              " usage"); \
        rcu_dereference_sparse(p, space); \
        smp_read_barrier_depends(); \    /* Alpha 架构需要 */
        ((typeof(*p) __force __kernel *)(_________p1)); \
    })

#define rcu_dereference(p) rcu_dereference_check(p, 0)
```

**为什么需要 rcu_dereference() 而不是直接读指针？**

```c
/* 错误示例: 没有使用 rcu_dereference() */
rcu_read_lock();
p = global_ptr;                    /* 编译器可能优化掉这个读取！ */
x = p->field;                      /* 可能读到过期值 */
rcu_read_unlock();

/* 正确示例 */
rcu_read_lock();
p = rcu_dereference(global_ptr);   /* ACCESS_ONCE 防止编译器优化 */
                                   /* smp_read_barrier_depends 保证 Alpha 正确性 */
x = p->field;                      /* 安全 */
rcu_read_unlock();
```

### 3.3 rcu_assign_pointer()

```c
/* include/linux/rcupdate.h */

#define __rcu_assign_pointer(p, v, space) \
    ({ \
        smp_wmb();   /* 确保 v 的初始化在赋值之前完成 */  \
        (p) = (typeof(*v) __force space *)(v); \
    })

#define rcu_assign_pointer(p, v) __rcu_assign_pointer((p), (v), __rcu)
```

**为什么需要 smp_wmb()？**

```
═══════════════════════════════════════════════════════════════════════════════
                没有 smp_wmb() 会发生什么？
═══════════════════════════════════════════════════════════════════════════════

Writer (CPU 0):                     Reader (CPU 1):
─────────────────────────────────   ─────────────────────────────────────────
new = kmalloc(...);
new->data = 42;
global_ptr = new;                   p = global_ptr;
                                    x = p->data;   /* 可能读到 0 或垃圾值！ */

问题: CPU/编译器可能重排序，导致指针先写入，数据后初始化


有 smp_wmb() 时:
─────────────────────────────────   ─────────────────────────────────────────
new = kmalloc(...);
new->data = 42;
smp_wmb();    /* ← 写屏障: 上面的写操作必须在下面之前对其他 CPU 可见 */
global_ptr = new;                   p = rcu_dereference(global_ptr);
                                    x = p->data;   /* 一定是 42 */
═══════════════════════════════════════════════════════════════════════════════
```

### 3.4 synchronize_rcu()

```c
/* kernel/rcutree.c */

/**
 * synchronize_sched - 等待 RCU-sched 宽限期结束
 *
 * 返回时保证: 所有在调用之前开始的 RCU 读临界区都已结束
 */
void synchronize_sched(void)
{
    if (rcu_blocking_is_gp())    /* 单 CPU 系统优化 */
        return;
    wait_rcu_gp(call_rcu_sched);
}
```

```
synchronize_rcu() 的行为:
═══════════════════════════════════════════════════════════════════════════════

    Thread A                        Thread B (RCU Reader)
    ─────────────────────────       ──────────────────────────────────────────
    old = global_ptr;
    rcu_assign_pointer(             rcu_read_lock();
        global_ptr, new);           p = rcu_dereference(global_ptr);
                                    // p 可能是 old 或 new
    synchronize_rcu();              do_something(p);
    │                               rcu_read_unlock();
    │ (blocks until Thread B's
    │  critical section ends)
    ▼
    kfree(old);                     // 此时 Thread B 已经退出临界区
                                    // kfree(old) 是安全的
═══════════════════════════════════════════════════════════════════════════════
```

### 3.5 call_rcu()

```c
/* kernel/rcutree.c */

static void
__call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *rcu),
           struct rcu_state *rsp)
{
    unsigned long flags;
    struct rcu_data *rdp;

    debug_rcu_head_queue(head);
    head->func = func;              /* 保存回调函数 */
    head->next = NULL;

    smp_mb(); /* 确保 RCU 更新在回调注册之前对其他 CPU 可见 */

    local_irq_save(flags);
    rdp = this_cpu_ptr(rsp->rda);

    /* 添加回调到当前 CPU 的回调链表 */
    *rdp->nxttail[RCU_NEXT_TAIL] = head;
    rdp->nxttail[RCU_NEXT_TAIL] = &head->next;
    rdp->qlen++;
    
    /* ... 可能触发宽限期 ... */
    
    local_irq_restore(flags);
}

void call_rcu_sched(struct rcu_head *head, void (*func)(struct rcu_head *rcu))
{
    __call_rcu(head, func, &rcu_sched_state);
}
```

**synchronize_rcu() vs call_rcu()**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│           synchronize_rcu() vs call_rcu() 对比                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  synchronize_rcu():                                                         │
│  ──────────────────                                                         │
│    • 同步等待宽限期结束                                                      │
│    • 调用者会阻塞                                                            │
│    • 适用于可以睡眠的上下文                                                   │
│    • 代码更简单                                                              │
│                                                                             │
│    old = rcu_dereference_protected(ptr, ...);                               │
│    rcu_assign_pointer(ptr, new);                                            │
│    synchronize_rcu();     /* 阻塞直到宽限期结束 */                           │
│    kfree(old);                                                              │
│                                                                             │
│  call_rcu():                                                                │
│  ───────────                                                                │
│    • 异步，立即返回                                                          │
│    • 调用者不阻塞                                                            │
│    • 适用于中断上下文或性能敏感路径                                           │
│    • 回调在软中断上下文执行                                                   │
│                                                                             │
│    struct my_struct {                                                       │
│        struct rcu_head rcu;                                                 │
│        int data;                                                            │
│    };                                                                       │
│                                                                             │
│    void my_free(struct rcu_head *rcu) {                                     │
│        struct my_struct *p = container_of(rcu, struct my_struct, rcu);      │
│        kfree(p);                                                            │
│    }                                                                        │
│                                                                             │
│    old = rcu_dereference_protected(ptr, ...);                               │
│    rcu_assign_pointer(ptr, new);                                            │
│    call_rcu(&old->rcu, my_free);  /* 立即返回，回调稍后执行 */               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. RCU Grace Period Mechanics

### 4.1 Hierarchical RCU (Tree RCU) Architecture

```
═══════════════════════════════════════════════════════════════════════════════
                  RCU Hierarchical Structure (以 16 CPU 为例)
═══════════════════════════════════════════════════════════════════════════════

                        ┌─────────────┐
                        │  rcu_state  │  全局 RCU 状态
                        │  ─────────  │
                        │ gpnum: N    │  当前宽限期编号
                        │ completed:  │  已完成的宽限期编号
                        │    N-1      │
                        └──────┬──────┘
                               │
                 Level 0       ▼
                        ┌─────────────┐
                        │  rcu_node   │  Root Node
                        │  (root)     │
                        │ qsmask:     │  需要报告 QS 的子节点掩码
                        │ 0b0011      │
                        └──────┬──────┘
                               │
                 Level 1   ┌───┴───┐
                           │       │
                      ┌────▼───┐ ┌─▼──────┐
                      │rcu_node│ │rcu_node│
                      │qsmask: │ │qsmask: │
                      │0b1111  │ │0b1111  │
                      └───┬────┘ └───┬────┘
                          │          │
                 Level 2  │          │
           ┌──────┬───────┼──────┐   └───────┬───────┬───────┐
           │      │       │      │           │       │       │
           ▼      ▼       ▼      ▼           ▼       ▼       ▼
        ┌─────┐┌─────┐┌─────┐┌─────┐     ┌─────┐┌─────┐┌─────┐┌─────┐
        │CPU 0││CPU 1││CPU 2││CPU 3│ ... │CPU12││CPU13││CPU14││CPU15│
        │     ││     ││     ││     │     │     ││     ││     ││     │
        │rcu_ ││rcu_ ││rcu_ ││rcu_ │     │rcu_ ││rcu_ ││rcu_ ││rcu_ │
        │data ││data ││data ││data │     │data ││data ││data ││data │
        └─────┘└─────┘└─────┘└─────┘     └─────┘└─────┘└─────┘└─────┘

═══════════════════════════════════════════════════════════════════════════════
```

### 4.2 Core Data Structures

```c
/* kernel/rcutree.h */

/*
 * Per-CPU RCU 数据
 */
struct rcu_data {
    /* 1) 静止状态和宽限期处理 */
    unsigned long completed;        /* 已完成的 GP 编号 */
    unsigned long gpnum;            /* 当前已知的 GP 编号 */
    unsigned long passed_quiesce_gpnum;  /* 报告 QS 时的 GP 编号 */
    bool passed_quiesce;            /* 是否已通过静止状态 */
    bool qs_pending;                /* 是否需要报告静止状态 */
    struct rcu_node *mynode;        /* 所属的叶子 rcu_node */
    unsigned long grpmask;          /* 在 rcu_node 中的位掩码 */

    /* 2) 回调链表 */
    struct rcu_head *nxtlist;       /* 回调链表头 */
    struct rcu_head **nxttail[RCU_NEXT_SIZE];  /* 分段尾指针 */
    long qlen;                      /* 回调数量 */
    
    int cpu;
    struct rcu_state *rsp;
};

/*
 * rcu_node - 层次结构中的节点
 */
struct rcu_node {
    raw_spinlock_t lock;
    unsigned long gpnum;            /* 当前 GP 编号 */
    unsigned long completed;        /* 已完成的 GP 编号 */
    unsigned long qsmask;           /* 需要报告 QS 的 CPU/子节点掩码 */
    unsigned long qsmaskinit;       /* GP 开始时的初始掩码 */
    unsigned long grpmask;          /* 在父节点中的位掩码 */
    struct rcu_node *parent;
    
    /* Preemptible RCU 相关 */
    struct list_head blkd_tasks;    /* 被阻塞的任务列表 */
};
```

### 4.3 Quiescent State Detection

```c
/* kernel/rcutree.c */

/*
 * 记录静止状态
 * 调用时必须禁用抢占
 */
void rcu_sched_qs(int cpu)
{
    struct rcu_data *rdp = &per_cpu(rcu_sched_data, cpu);

    rdp->passed_quiesce_gpnum = rdp->gpnum;  /* 记录 QS 时的 GP */
    barrier();
    if (rdp->passed_quiesce == 0)
        trace_rcu_grace_period("rcu_sched", rdp->gpnum, "cpuqs");
    rdp->passed_quiesce = 1;
}

/*
 * 上下文切换时调用
 * 对于 RCU-sched，上下文切换就是静止状态
 */
void rcu_note_context_switch(int cpu)
{
    trace_rcu_utilization("Start context switch");
    rcu_sched_qs(cpu);                       /* 报告 RCU-sched QS */
    rcu_preempt_note_context_switch(cpu);    /* 处理 preemptible RCU */
    trace_rcu_utilization("End context switch");
}
```

### 4.4 Grace Period State Machine

```
═══════════════════════════════════════════════════════════════════════════════
                    GRACE PERIOD STATE TRANSITIONS
═══════════════════════════════════════════════════════════════════════════════

    ┌──────────────────────────────────────────────────────────────────────┐
    │                                                                      │
    │  ┌─────────────┐    call_rcu()     ┌─────────────┐                  │
    │  │             │ ────────────────► │             │                  │
    │  │  GP_IDLE    │                   │  GP_INIT    │                  │
    │  │             │                   │             │                  │
    │  │ gpnum == N  │                   │ gpnum = N+1 │                  │
    │  │ completed   │                   │ initialize  │                  │
    │  │   == N     │                   │ qsmask bits │                  │
    │  └─────────────┘                   └──────┬──────┘                  │
    │        ▲                                  │                         │
    │        │                                  │ init done               │
    │        │                                  ▼                         │
    │        │                          ┌─────────────┐                   │
    │        │                          │             │                   │
    │        │                          │ FORCE_QS    │ ◄─── jiffies      │
    │        │                          │             │      timeout      │
    │        │                          │ send IPIs   │                   │
    │        │                          │ to laggard  │                   │
    │        │                          │ CPUs        │                   │
    │        │                          └──────┬──────┘                   │
    │        │                                 │                          │
    │        │                                 │ all CPUs reported QS     │
    │        │                                 ▼                          │
    │        │                          ┌─────────────┐                   │
    │        │                          │             │                   │
    │        └────────────────────────  │ GP_CLEANUP  │                   │
    │              completed = gpnum    │             │                   │
    │              invoke callbacks     │ advance     │                   │
    │                                   │ callbacks   │                   │
    │                                   └─────────────┘                   │
    │                                                                      │
    └──────────────────────────────────────────────────────────────────────┘
    
    
    Quiescent State Reporting (向上传播):
    ═══════════════════════════════════════
    
    CPU 3 reports QS:
    
        rcu_data[3].passed_quiesce = 1
                    │
                    ▼
        rcu_report_qs_rdp(cpu=3)
                    │
                    ▼
        leaf rcu_node.qsmask &= ~(1 << 3)
                    │
                    │ if (qsmask == 0)
                    ▼
        parent rcu_node.qsmask &= ~child_bit
                    │
                    │ if (qsmask == 0 && parent == NULL)
                    ▼
        rcu_report_qs_rsp()  /* GP 完成！ */
    
═══════════════════════════════════════════════════════════════════════════════
```

---

## 5. RCU Flavors and Their Differences

### 5.1 RCU Flavor Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        RCU 变体对比表                                        │
├──────────────┬──────────────┬──────────────┬──────────────┬─────────────────┤
│              │ RCU-sched    │ RCU-bh       │ RCU          │ SRCU            │
│              │              │              │ (preempt)    │                 │
├──────────────┼──────────────┼──────────────┼──────────────┼─────────────────┤
│ Read lock    │ preempt_     │ local_bh_    │ preempt_     │ srcu_read_      │
│              │ disable()    │ disable()    │ disable() +  │ lock(srcu)      │
│              │              │              │ nesting cnt  │                 │
├──────────────┼──────────────┼──────────────┼──────────────┼─────────────────┤
│ Can sleep    │ No           │ No           │ No (但可被   │ Yes             │
│ in critical  │              │              │ 抢占)        │                 │
│ section      │              │              │              │                 │
├──────────────┼──────────────┼──────────────┼──────────────┼─────────────────┤
│ Quiescent    │ Context      │ BH enable    │ Context      │ Explicit        │
│ state        │ switch       │              │ switch +     │ unlock          │
│              │              │              │ idle         │                 │
├──────────────┼──────────────┼──────────────┼──────────────┼─────────────────┤
│ Use case     │ Scheduler,   │ Networking,  │ General      │ Need to sleep   │
│              │ interrupt    │ softirq      │ kernel code  │ in reader       │
│              │ handlers     │ handlers     │              │                 │
├──────────────┼──────────────┼──────────────┼──────────────┼─────────────────┤
│ GP wait      │ synchronize_ │ synchronize_ │ synchronize_ │ synchronize_    │
│              │ sched()      │ rcu_bh()     │ rcu()        │ srcu(srcu)      │
└──────────────┴──────────────┴──────────────┴──────────────┴─────────────────┘
```

### 5.2 RCU-sched

```c
/* 用于保护 scheduler 和中断处理程序访问的数据 */

static inline void rcu_read_lock_sched(void)
{
    preempt_disable();      /* 禁用抢占即进入临界区 */
}

static inline void rcu_read_unlock_sched(void)
{
    preempt_enable();
}

/* 
 * 特点:
 * - 任何禁用抢占的代码都是隐式的 RCU-sched 读临界区
 * - local_irq_disable() 也算
 * - 用于需要与 NMI 或中断处理程序共享数据的场景
 */
```

### 5.3 RCU-bh (Bottom Half)

```c
/* 用于与 softirq 共享数据的场景 */

static inline void rcu_read_lock_bh(void)
{
    local_bh_disable();     /* 禁用软中断 */
}

static inline void rcu_read_unlock_bh(void)
{
    local_bh_enable();
}

/*
 * 特点:
 * - softirq 完成即静止状态
 * - 比普通 RCU 有更短的宽限期
 * - 主要用于网络子系统
 */
```

### 5.4 SRCU (Sleepable RCU)

```c
/* include/linux/srcu.h */

struct srcu_struct {
    int completed;
    struct srcu_struct_array __percpu *per_cpu_ref;
    struct mutex mutex;
};

int srcu_read_lock(struct srcu_struct *sp);
void srcu_read_unlock(struct srcu_struct *sp, int idx);
void synchronize_srcu(struct srcu_struct *sp);

/*
 * SRCU 特点:
 * - 读临界区内可以睡眠
 * - 每个 srcu_struct 独立，有自己的宽限期
 * - 开销比普通 RCU 高
 * - 用于需要在读端睡眠的场景
 */
```

---

## 6. Update-side Lifecycle Walkthrough

### 6.1 Complete Update Scenario

```c
/* 完整的 RCU 更新示例 */

struct my_data {
    int value;
    char name[32];
    struct rcu_head rcu;    /* 用于 call_rcu() */
};

static DEFINE_SPINLOCK(my_lock);        /* 保护写者之间的互斥 */
static struct my_data __rcu *global_data; /* RCU 保护的指针 */

/* 
 * 更新函数
 */
int update_data(int new_value, const char *new_name)
{
    struct my_data *new_data, *old_data;
    
    /* Step 1: 分配新数据 */
    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
    if (!new_data)
        return -ENOMEM;
    
    /* Step 2: 初始化新数据 (在发布之前必须完成) */
    new_data->value = new_value;
    strncpy(new_data->name, new_name, sizeof(new_data->name));
    
    /* Step 3: 获取更新锁 (写者之间互斥) */
    spin_lock(&my_lock);
    
    /* Step 4: 保存旧指针 */
    old_data = rcu_dereference_protected(global_data,
                                         lockdep_is_held(&my_lock));
    
    /* Step 5: 发布新指针 (包含 smp_wmb) */
    rcu_assign_pointer(global_data, new_data);
    
    spin_unlock(&my_lock);
    
    /* Step 6: 等待宽限期 (所有旧读者退出) */
    synchronize_rcu();
    
    /* Step 7: 安全释放旧数据 */
    kfree(old_data);
    
    return 0;
}
```

### 6.2 Timeline Diagram

```
═══════════════════════════════════════════════════════════════════════════════
                    UPDATE OPERATION TIMELINE
═══════════════════════════════════════════════════════════════════════════════

Time ─────────────────────────────────────────────────────────────────────────►

Writer Thread:
│
├─ new = kmalloc()
├─ new->value = 42
├─ new->name = "hello"
│
├─ spin_lock(&my_lock)
├─ old = global_data
├─ rcu_assign_pointer(global_data, new)    ◄─── 发布点
├─ spin_unlock(&my_lock)                         │
│                                                │
├─ synchronize_rcu()  ────────────────────┐      │
│   │                                     │      │
│   │  (等待所有预先存在的读者)           │      │
│   │         │                           │      │
│   │         ▼                           │      │
│   └─────────────────────────────────────┘      │
│                                                │
├─ kfree(old)  ◄─── 此时安全                     │
▼                                                │
                                                 │
Reader Thread A (发布前开始):                     │
├─ rcu_read_lock()                              │
├─ p = rcu_dereference(global_data)  ──► old    │
├─ use(p)  // 使用旧数据，安全                   │
├─ rcu_read_unlock()  ◄─── synchronize 等待这个  │
▼                                                │
                                                 │
Reader Thread B (发布后开始):                     │
                   ├─ rcu_read_lock()            │
                   ├─ p = rcu_dereference(...) ──► new (可能)
                   ├─ use(p)  // 使用新数据      │
                   ├─ rcu_read_unlock()          │
                   ▼                             │

═══════════════════════════════════════════════════════════════════════════════
```

---

## 7. Reader-side Execution Model

### 7.1 Per-CPU Behavior

```
═══════════════════════════════════════════════════════════════════════════════
                    RCU READER PER-CPU MODEL
═══════════════════════════════════════════════════════════════════════════════

CPU 0                     CPU 1                     CPU 2
──────────────────────    ──────────────────────    ──────────────────────

[process context]         [softirq context]         [idle]
      │                         │                         │
      ▼                         ▼                         ▼
┌────────────────┐        ┌────────────────┐        ┌────────────────┐
│ rcu_read_lock()│        │ (隐式 RCU      │        │ (idle 是 QS)   │
│ preempt_       │        │  临界区)       │        │                │
│ disable()      │        │                │        │ 宽限期可以     │
│                │        │ softirq        │        │ 立即从这个     │
│ ... 读取 ...   │        │ handler        │        │ CPU 角度完成   │
│                │        │ running        │        │                │
│ rcu_read_      │        │                │        │                │
│ unlock()       │        │                │        │                │
│ preempt_       │        │ softirq done   │        │                │
│ enable()       │        │ (QS for bh)    │        │                │
└────────────────┘        └────────────────┘        └────────────────┘
      │                         │                         │
      ▼                         ▼                         ▼
 [context switch]          [return to                [wake up]
 (QS for sched)            process ctx]              
                           (QS for sched)

═══════════════════════════════════════════════════════════════════════════════

Key insight: 
- RCU 不跟踪每个读者
- 只跟踪每个 CPU 是否经过静止状态
- 这是 RCU 可扩展性的关键
```

### 7.2 Why Readers Must Not Sleep

```
═══════════════════════════════════════════════════════════════════════════════
                  为什么 RCU 读临界区不能睡眠？
═══════════════════════════════════════════════════════════════════════════════

问题场景 (如果允许睡眠):
────────────────────────

CPU 0:                                    CPU 1:
──────                                    ──────
rcu_read_lock();                         rcu_assign_pointer(ptr, new);
p = rcu_dereference(ptr); // old         synchronize_rcu();
                                              │
/* 睡眠！*/                                   │ 等待 CPU 0 报告 QS
schedule();  ───────────────────────────────────►
│                                              │
│  (其他任务运行)                              │ CPU 0 报告了 QS
│  context switch = QS                         │ (因为发生了调度)
│                                              │
│                                              ▼
│                                         kfree(old); // 释放了！
│
▼
/* 醒来，继续执行 */
use(p); // p 指向已释放的内存！💥 UAF BUG!
rcu_read_unlock();


解决方案:
─────────
1. 非抢占 RCU: rcu_read_lock() = preempt_disable()
   - 不能被调度出去，因此不会有上述问题
   
2. Preemptible RCU: 跟踪被抢占的任务
   - 维护 blkd_tasks 链表
   - 宽限期等待这些任务
   
3. SRCU: 允许睡眠
   - 使用不同的机制跟踪读者
   - 开销更大

═══════════════════════════════════════════════════════════════════════════════
```

---

## 8. Memory Ordering & Barriers

### 8.1 RCU Memory Ordering Requirements

```
═══════════════════════════════════════════════════════════════════════════════
                    RCU MEMORY ORDERING
═══════════════════════════════════════════════════════════════════════════════

Publisher (Writer):                     Subscriber (Reader):
───────────────────                     ────────────────────

/* 初始化新对象 */                      
new->a = 1;                            
new->b = 2;                            
        │                               
        │ smp_wmb() [隐含在               
        │ rcu_assign_pointer 中]          
        ▼                                       rcu_read_lock();
rcu_assign_pointer(ptr, new);          ───────► p = rcu_dereference(ptr);
                                                │
                                                │ smp_read_barrier_depends()
                                                │ [隐含在 rcu_dereference 中]
                                                │ [大多数架构上是空操作]
                                                │ [Alpha 需要]
                                                ▼
                                                x = p->a;  // 保证看到 1
                                                y = p->b;  // 保证看到 2
                                                rcu_read_unlock();

关键点:
1. smp_wmb() 确保初始化在指针赋值之前完成
2. smp_read_barrier_depends() 确保指针读取在解引用之前完成
3. 在大多数架构上，数据依赖本身就提供了排序保证
4. Alpha 是例外，需要显式屏障

═══════════════════════════════════════════════════════════════════════════════
```

### 8.2 rcu_dereference vs READ_ONCE

```c
/*
 * READ_ONCE(p):
 * - 防止编译器优化 (如缓存到寄存器)
 * - 不提供任何内存屏障
 * - 用于避免编译器优化导致的问题
 *
 * rcu_dereference(p):
 * - 包含 READ_ONCE 的功能
 * - 额外包含 smp_read_barrier_depends() (Alpha 需要)
 * - 包含 lockdep 检查 (调试时)
 * - 包含 sparse 类型检查
 */

/* 错误用法 */
rcu_read_lock();
p = global_ptr;           // 编译器可能优化！
x = p->field;
rcu_read_unlock();

/* 稍好但仍不完整 */
rcu_read_lock();
p = READ_ONCE(global_ptr);  // Alpha 上可能出问题
x = p->field;
rcu_read_unlock();

/* 正确用法 */
rcu_read_lock();
p = rcu_dereference(global_ptr);  // 完整的 RCU 语义
x = p->field;
rcu_read_unlock();
```

---

## 9. Real Kernel Usage Examples

### 9.1 Network Subsystem: RPS Flow Tables

```c
/* net/core/dev.c - Receive Packet Steering */

/*
 * 为什么用 RCU?
 * - 每个收到的包都要查询流表
 * - 流表偶尔更新
 * - 典型的读多写少场景
 */

static u16 get_rps_cpu(struct net_device *dev, struct sk_buff *skb,
                       struct rps_dev_flow **rflowp)
{
    struct netdev_rx_queue *rxqueue;
    struct rps_map *map;
    struct rps_dev_flow_table *flow_table;
    
    /* RCU 读临界区 */
    rxqueue = dev->_rx + rxq_index;
    
    /* RCU 保护的流表查询 */
    flow_table = rcu_dereference(rxqueue->rps_flow_table);
    if (!flow_table)
        goto out;
    
    /* RCU 保护的 CPU 映射查询 */
    map = rcu_dereference(rxqueue->rps_map);
    if (map) {
        if (map->len == 1 && map->cpus[0] == raw_smp_processor_id())
            goto done;
        cpu = map->cpus[reciprocal_scale(hash, map->len)];
    }
    
    /* ... */
}

/*
 * 更新流表 (需要 rtnl_lock 保护写者互斥)
 */
static int store_rps_map(struct netdev_rx_queue *queue,
                         const char *buf, size_t len)
{
    struct rps_map *old_map, *map;
    
    map = kzalloc(/* ... */);
    /* ... 初始化 map ... */
    
    /* rtnl_lock 已持有 */
    old_map = rtnl_dereference(queue->rps_map);
    rcu_assign_pointer(queue->rps_map, map);
    
    if (old_map)
        kfree_rcu(old_map, rcu);  /* 等价于 call_rcu + kfree */
    
    return 0;
}
```

### 9.2 VFS: Dentry Cache (dcache)

```c
/* fs/dcache.c - Directory Entry Cache */

/*
 * 为什么用 RCU?
 * - 路径查找是最频繁的文件系统操作
 * - dcache 是只读查找的热点
 * - 使用 RCU 可以避免在查找路径上加锁
 */

/*
 * RCU-walk 模式的路径查找
 * - 不获取任何锁
 * - 使用 rcu_read_lock 保护
 * - 失败时回退到 REF-walk
 */
static int lookup_fast(struct nameidata *nd,
                       struct path *path, struct inode **inode,
                       unsigned *seqp)
{
    struct vfsmount *mnt = nd->path.mnt;
    struct dentry *dentry, *parent = nd->path.dentry;
    
    /* RCU-walk 模式 */
    if (nd->flags & LOOKUP_RCU) {
        /* 使用 RCU 查找 dentry */
        dentry = __d_lookup_rcu(parent, &nd->last, &seq);
        if (!dentry)
            return 0;
            
        /* 验证 dentry 仍然有效 */
        *inode = dentry->d_inode;
        if (read_seqcount_retry(&dentry->d_seq, seq))
            return -ECHILD;  /* 需要重试或回退 */
            
        /* ... */
    }
}
```

---

## 10. Common RCU Bugs and Anti-patterns

### 10.1 Use-After-Free

```c
/* BUG: 没有等待宽限期就释放 */

void buggy_update(struct foo *new)
{
    struct foo *old;
    
    spin_lock(&my_lock);
    old = global_ptr;
    rcu_assign_pointer(global_ptr, new);
    spin_unlock(&my_lock);
    
    kfree(old);  // BUG! 可能有读者还在使用 old
}

/* 正确版本 */
void correct_update(struct foo *new)
{
    struct foo *old;
    
    spin_lock(&my_lock);
    old = global_ptr;
    rcu_assign_pointer(global_ptr, new);
    spin_unlock(&my_lock);
    
    synchronize_rcu();  // 等待所有旧读者退出
    kfree(old);
}
```

### 10.2 Sleeping in Read Critical Section

```c
/* BUG: 在 RCU 读临界区内睡眠 */

void buggy_reader(void)
{
    rcu_read_lock();
    p = rcu_dereference(global_ptr);
    
    kmalloc(SIZE, GFP_KERNEL);  // BUG! 可能睡眠
    
    use(p);
    rcu_read_unlock();
}

/* 正确版本 1: 使用 GFP_ATOMIC */
void correct_reader_v1(void)
{
    rcu_read_lock();
    p = rcu_dereference(global_ptr);
    
    kmalloc(SIZE, GFP_ATOMIC);  // OK: 不会睡眠
    
    use(p);
    rcu_read_unlock();
}

/* 正确版本 2: 使用 SRCU */
void correct_reader_v2(void)
{
    int idx = srcu_read_lock(&my_srcu);
    p = srcu_dereference(global_ptr, &my_srcu);
    
    kmalloc(SIZE, GFP_KERNEL);  // OK: SRCU 允许睡眠
    
    use(p);
    srcu_read_unlock(&my_srcu, idx);
}
```

### 10.3 Missing rcu_dereference

```c
/* BUG: 直接访问 RCU 指针 */

void buggy_reader(void)
{
    rcu_read_lock();
    p = global_ptr;           // BUG! 可能被编译器优化
    x = p->field;             // 可能读到过期值
    rcu_read_unlock();
}

/* 正确版本 */
void correct_reader(void)
{
    rcu_read_lock();
    p = rcu_dereference(global_ptr);  // 正确
    x = p->field;
    rcu_read_unlock();
}
```

### 10.4 Mixing Locks and RCU Incorrectly

```c
/* BUG: 锁和 RCU 使用不一致 */

void buggy_code(void)
{
    /* Reader 使用 RCU */
    rcu_read_lock();
    p = rcu_dereference(list_head);
    /* ... 遍历 ... */
    rcu_read_unlock();
    
    /* Writer 没有使用锁保护写者互斥 */
    new_node->next = list_head;
    rcu_assign_pointer(list_head, new_node);  // BUG: 多个写者竞争!
}

/* 正确版本 */
void correct_code(void)
{
    /* Reader 使用 RCU (不变) */
    rcu_read_lock();
    p = rcu_dereference(list_head);
    /* ... 遍历 ... */
    rcu_read_unlock();
    
    /* Writer 使用锁保护 */
    spin_lock(&list_lock);
    new_node->next = list_head;
    rcu_assign_pointer(list_head, new_node);
    spin_unlock(&list_lock);
}
```

---

## 11. Minimal Self-contained Example

```c
/*
 * 最小化 RCU 示例
 * 
 * 场景: 维护一个 RCU 保护的配置结构体
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

/* RCU 保护的数据结构 */
struct config {
    int timeout;
    char server[64];
    struct rcu_head rcu;    /* call_rcu 需要 */
};

static DEFINE_SPINLOCK(config_lock);  /* 写者互斥 */
static struct config __rcu *current_config;

/*
 * 读取配置 (可以在任何上下文调用)
 */
void read_config(int *timeout, char *server)
{
    struct config *cfg;
    
    rcu_read_lock();
    cfg = rcu_dereference(current_config);
    if (cfg) {
        *timeout = cfg->timeout;
        strncpy(server, cfg->server, 64);
    }
    rcu_read_unlock();
}

/*
 * RCU 回调: 释放旧配置
 */
static void config_free_rcu(struct rcu_head *head)
{
    struct config *cfg = container_of(head, struct config, rcu);
    pr_info("Freeing old config: timeout=%d\n", cfg->timeout);
    kfree(cfg);
}

/*
 * 更新配置
 */
int update_config(int new_timeout, const char *new_server)
{
    struct config *new_cfg, *old_cfg;
    
    /* 1. 分配新配置 */
    new_cfg = kmalloc(sizeof(*new_cfg), GFP_KERNEL);
    if (!new_cfg)
        return -ENOMEM;
    
    /* 2. 初始化新配置 */
    new_cfg->timeout = new_timeout;
    strncpy(new_cfg->server, new_server, sizeof(new_cfg->server));
    
    /* 3. 原子替换 (需要锁保护写者互斥) */
    spin_lock(&config_lock);
    old_cfg = rcu_dereference_protected(current_config,
                                        lockdep_is_held(&config_lock));
    rcu_assign_pointer(current_config, new_cfg);
    spin_unlock(&config_lock);
    
    /* 4. 延迟释放旧配置 */
    if (old_cfg)
        call_rcu(&old_cfg->rcu, config_free_rcu);
    
    return 0;
}

/*
 * 模块初始化
 */
static int __init rcu_example_init(void)
{
    pr_info("RCU example loaded\n");
    
    /* 设置初始配置 */
    update_config(30, "server1.example.com");
    
    return 0;
}

/*
 * 模块卸载
 */
static void __exit rcu_example_exit(void)
{
    struct config *cfg;
    
    spin_lock(&config_lock);
    cfg = rcu_dereference_protected(current_config,
                                    lockdep_is_held(&config_lock));
    rcu_assign_pointer(current_config, NULL);
    spin_unlock(&config_lock);
    
    /* 同步等待所有读者，然后释放 */
    synchronize_rcu();
    kfree(cfg);
    
    pr_info("RCU example unloaded\n");
}

module_init(rcu_example_init);
module_exit(rcu_example_exit);
MODULE_LICENSE("GPL");
```

---

## 12. Design Philosophy & Intuition

### 12.1 Why RCU Scales

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    RCU 可扩展性的秘密                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 1. 读者完全独立                                                              │
│    ─────────────                                                            │
│    • 没有共享的写操作 (原子计数器、锁等)                                     │
│    • 没有缓存行竞争                                                          │
│    • 每个 CPU 独立执行，不受其他 CPU 影响                                    │
│                                                                             │
│ 2. 工作转移到写者                                                            │
│    ─────────────────                                                        │
│    • 复制数据的开销由写者承担                                                │
│    • 等待宽限期的延迟由写者承担                                              │
│    • 读者从不为写者买单                                                      │
│                                                                             │
│ 3. 批量处理宽限期                                                            │
│    ─────────────────                                                        │
│    • 多个 call_rcu() 可以共享一个宽限期                                      │
│    • 回调批量执行                                                            │
│    • 减少了宽限期的开销                                                      │
│                                                                             │
│                                                                             │
│       CPU 数量增加时的扩展性:                                                │
│       ──────────────────────                                                │
│                                                                             │
│       吞吐量                                                                 │
│          ▲                                                                  │
│          │                    RCU 读操作                                    │
│          │               ╱──────────────────                                │
│          │              ╱                                                   │
│          │             ╱                                                    │
│          │            ╱                                                     │
│          │           ╱                                                      │
│          │          ╱   rwlock 读操作                                       │
│          │         ╱   ╱────────────────                                    │
│          │        ╱  ╱                                                      │
│          │       ╱ ╱                                                        │
│          │      ╱╱                                                          │
│          │     ╱                                                            │
│          │    ╱                                                             │
│          │  ╱                                                               │
│          │╱──────────────────────────────────────────► CPU 数量             │
│          1     4      16      64     256                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 12.2 Why RCU is Hard but Powerful

RCU 的难点:
1. **内存模型**: 需要理解弱序内存架构和屏障
2. **宽限期**: 需要理解什么是静止状态
3. **生命周期**: 指针只在特定范围内有效
4. **写者同步**: RCU 不保护写者，需要额外的锁

RCU 的强大之处:
1. **读性能**: 几乎零开销的读操作
2. **可扩展性**: 读者数量增加不影响性能
3. **无死锁**: 读者永不阻塞
4. **广泛适用**: Linux 内核中到处都是 RCU

---

## 13. Summary and Learning Checklist

### 13.1 Key Invariants (必须牢记)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      RCU 核心不变量                                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 1. rcu_dereference() 返回的指针只在获取它的 RCU 读临界区内有效              │
│                                                                             │
│ 2. 新数据必须在 rcu_assign_pointer() 之前完全初始化                         │
│                                                                             │
│ 3. 旧数据必须在宽限期后才能释放                                              │
│                                                                             │
│ 4. RCU 不保护写者之间的互斥，需要外部锁                                      │
│                                                                             │
│ 5. 非抢占 RCU 的读临界区内不能睡眠                                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 13.2 When to Use RCU

✅ **适合使用 RCU**:
- 读操作远多于写操作 (10:1 或更高)
- 可以容忍读取到稍旧的数据
- 数据通过指针访问
- 需要高性能的并发读取
- 例子: 路由表、配置数据、只读缓存

### 13.3 When NOT to Use RCU

❌ **不适合使用 RCU**:
- 读写比例接近 1:1
- 需要读取最新数据
- 数据频繁更新
- 需要在读临界区内睡眠 (除非用 SRCU)
- 空间受限 (RCU 更新需要额外空间)

### 13.4 Learning Checklist

```
□ 理解为什么 rcu_read_lock() 不是真正的锁
□ 理解宽限期的精确定义
□ 能解释 rcu_dereference() 和 rcu_assign_pointer() 的内存屏障作用
□ 知道何时使用 synchronize_rcu() vs call_rcu()
□ 理解 RCU-sched, RCU-bh, SRCU 的区别和适用场景
□ 能识别常见的 RCU bug (UAF, 睡眠, 缺少 dereference)
□ 能写出正确的 RCU 读写代码
□ 理解 RCU 的可扩展性优势
```

### 13.5 Further Reading

1. **Paul McKenney 的 RCU 论文和文档**
   - `Documentation/RCU/` 目录下的所有文档
   - "What is RCU, Fundamentally?"

2. **内核源码**
   - `include/linux/rcupdate.h` - API 定义
   - `kernel/rcutree.c` - Tree RCU 实现
   - `kernel/rcutree.h` - 数据结构定义

3. **LWN 文章**
   - https://lwn.net/Articles/262464/ (RCU part 1)
   - https://lwn.net/Articles/263130/ (RCU part 2)
   - https://lwn.net/Articles/264090/ (RCU part 3)

---

## 14. RCU + Reference Counting: Why They Must Be Combined

### 14.1 核心问题：RCU 保护的局限性

RCU 只保证在读临界区内对象不会被释放，但 **不能保证临界区外对象仍然有效**。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    RCU 单独使用的局限性                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  问题场景:                                                                   │
│  ─────────                                                                  │
│  假设我们需要在 RCU 临界区外长时间使用查找到的对象                            │
│                                                                             │
│    Thread A (Reader):              Thread B (Updater):                      │
│    ───────────────────             ────────────────────                     │
│    rcu_read_lock();                                                         │
│    obj = rcu_dereference(table);   /* obj->refcnt = 1 */                   │
│    /* 找到了目标对象 */                                                      │
│    rcu_read_unlock();                                                       │
│    │                               rcu_assign_pointer(table, new_obj);      │
│    │                               synchronize_rcu();                       │
│    │                               kfree(obj);  /* 对象被释放！ */          │
│    ▼                                                                        │
│    use(obj);  /* 💥 UAF! 使用已释放的内存 */                                │
│                                                                             │
│  根本原因:                                                                   │
│  ─────────                                                                  │
│  • RCU 只保护"临界区内"的访问                                                │
│  • 一旦离开 rcu_read_unlock()，对象的生命周期不再受 RCU 保护                 │
│  • 如果需要长期持有对象，必须有其他机制保证对象存活                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 14.2 RCU 和 Reference Counting 各自解决什么问题？

```
═══════════════════════════════════════════════════════════════════════════════
                    两种机制的职责划分
═══════════════════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  RCU 解决的问题:                         Reference Counting 解决的问题:     │
│  ─────────────────                       ─────────────────────────────      │
│                                                                             │
│  1. 读者无锁访问                          1. 跨临界区的对象生命周期管理       │
│     • 高性能并发读取                         • 只要有人持有引用，对象就不释放 │
│     • 读者之间不竞争                                                        │
│                                                                             │
│  2. 短期存在性保证                        2. 长期存在性保证                   │
│     • 仅在 RCU 临界区内有效                  • 不限于特定代码区域             │
│     • 离开临界区后无效                       • 显式获取和释放                 │
│                                                                             │
│  3. 延迟释放                              3. 最后一个引用释放时销毁           │
│     • 宽限期后统一释放                       • 精确控制释放时机               │
│     • 不需要精确跟踪每个读者                                                 │
│                                                                             │
│                                                                             │
│  组合使用的模式:                                                             │
│  ────────────────                                                           │
│                                                                             │
│    RCU 保护"查找过程"  ───►  refcnt 保护"使用过程"                           │
│                                                                             │
│    ┌───────────────────────┐     ┌───────────────────────────────┐         │
│    │   RCU Read Section    │     │    Reference Counted Usage    │         │
│    │                       │     │                               │         │
│    │  rcu_read_lock()      │     │                               │         │
│    │  obj = lookup(...)    │────►│  /* refcnt 已增加 */          │         │
│    │  refcnt_inc(obj)      │     │  do_long_operation(obj);      │         │
│    │  rcu_read_unlock()    │     │  refcnt_dec(obj);             │         │
│    │                       │     │                               │         │
│    │  短暂、高频            │     │  可能很长、可能睡眠            │         │
│    └───────────────────────┘     └───────────────────────────────┘         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

### 14.3 为什么必须使用 atomic_inc_not_zero？

这是整个模式中**最关键、最容易出错**的地方。

```c
/* 错误用法: 使用普通的 atomic_inc */

rcu_read_lock();
obj = rcu_dereference(global_table);
if (obj) {
    atomic_inc(&obj->refcnt);    /* BUG! 可能增加一个正在释放的对象 */
}
rcu_read_unlock();
```

**为什么 `atomic_inc()` 是错误的？**

```
═══════════════════════════════════════════════════════════════════════════════
                 atomic_inc() 的竞态条件
═══════════════════════════════════════════════════════════════════════════════

时间线:
───────────────────────────────────────────────────────────────────────────────►

  Thread A (Reader)                    Thread B (Updater)
  ─────────────────                    ──────────────────
  
  rcu_read_lock();                    
  obj = rcu_dereference(table);       // refcnt = 1
                                      
                                      // 开始删除流程
                                      spin_lock(&table_lock);
                                      list_del_rcu(&obj->list);
                                      spin_unlock(&table_lock);
                                      
                                      // 释放最后一个引用
                                      if (atomic_dec_and_test(&obj->refcnt)) {
                                          // refcnt 变成 0！
                                          call_rcu(&obj->rcu, free_obj);
                                      }
  
  if (obj) {
      atomic_inc(&obj->refcnt);       // refcnt: 0 → 1，复活了一个即将释放的对象！
  }
  rcu_read_unlock();
  
                                      // RCU 回调执行
                                      kfree(obj);  // 释放！
  
  use(obj);  // 💥 UAF!

═══════════════════════════════════════════════════════════════════════════════

问题分析:
─────────
1. Thread B 已经将 refcnt 减到 0，准备释放对象
2. Thread A 在 RCU 临界区内看到了这个对象（因为 RCU 保护，还能看到）
3. Thread A 用 atomic_inc() 将 refcnt 从 0 增加到 1
4. 这是"复活"一个已经决定要释放的对象！
5. Thread B 的 call_rcu 回调最终会释放这个对象
6. Thread A 持有的引用指向已释放的内存

核心问题: atomic_inc() 不检查 refcnt 是否已经是 0
```

**正确用法: `atomic_inc_not_zero()`**

```c
/* 正确用法: 使用 atomic_inc_not_zero */

rcu_read_lock();
obj = rcu_dereference(global_table);
if (obj && atomic_inc_not_zero(&obj->refcnt)) {
    /* 成功获取引用 */
    rcu_read_unlock();
    
    /* 可以安全地长期使用 obj */
    do_something(obj);
    
    /* 用完后释放引用 */
    if (atomic_dec_and_test(&obj->refcnt))
        kfree_rcu(obj, rcu);
} else {
    /* 对象正在被释放，获取引用失败 */
    rcu_read_unlock();
    /* 必须处理查找失败的情况 */
}
```

```
═══════════════════════════════════════════════════════════════════════════════
                 atomic_inc_not_zero() 如何解决问题
═══════════════════════════════════════════════════════════════════════════════

atomic_inc_not_zero(&refcnt) 的语义:
─────────────────────────────────────
  if (refcnt != 0) {
      refcnt++;
      return 1;  // 成功
  } else {
      return 0;  // 失败，refcnt 已经是 0
  }
  
  整个操作是原子的！


正确的时间线:
───────────────────────────────────────────────────────────────────────────────►

  Thread A (Reader)                    Thread B (Updater)
  ─────────────────                    ──────────────────
  
  rcu_read_lock();                    
  obj = rcu_dereference(table);       // refcnt = 1
                                      
                                      // 开始删除
                                      spin_lock(&table_lock);
                                      list_del_rcu(&obj->list);
                                      spin_unlock(&table_lock);
                                      
                                      if (atomic_dec_and_test(&obj->refcnt)) {
                                          // refcnt 变成 0
                                          call_rcu(&obj->rcu, free_obj);
                                      }
  
  if (obj && atomic_inc_not_zero(&obj->refcnt)) {
      // atomic_inc_not_zero 返回 0！
      // 因为 refcnt 已经是 0
      // 不会"复活"对象
  }
  // 条件为 false，不会进入 if 分支
  
  rcu_read_unlock();
  
                                      // RCU 回调安全执行
                                      kfree(obj);
  
  // Thread A 正确地知道获取引用失败
  // 不会使用 obj

═══════════════════════════════════════════════════════════════════════════════
```

### 14.4 完整的 Code Pattern 分析

```c
/*
 * 标准的 RCU + refcount 查找模式
 */

rcu_read_lock();                              /* 步骤 1: 进入 RCU 临界区 */

obj = rcu_dereference(global_table);          /* 步骤 2: RCU 安全地获取指针 */

if (obj && refcount_inc_not_zero(&obj->refcnt)) {  /* 步骤 3: 尝试获取引用 */
    /* 
     * 成功路径:
     * - obj 非空
     * - refcnt 不为 0，成功增加
     * - 现在我们"拥有"了一个引用
     */
}

rcu_read_unlock();                            /* 步骤 4: 离开 RCU 临界区 */

/* 步骤 5: 安全的长期使用 */
do_long_operation(obj);                       /* 可以睡眠、可以很长 */

refcount_dec(&obj->refcnt);                   /* 步骤 6: 释放引用 */
                                              /* 注意：这里可能触发对象释放 */
```

```
═══════════════════════════════════════════════════════════════════════════════
                    每个步骤的作用和必要性
═══════════════════════════════════════════════════════════════════════════════

步骤 1: rcu_read_lock()
───────────────────────
  • 进入 RCU 读临界区
  • 保证在临界区内，通过 RCU 指针访问的对象不会被释放
  • 即使对象已经从数据结构中删除，内存仍然有效

步骤 2: rcu_dereference(global_table)
─────────────────────────────────────
  • 安全地读取 RCU 保护的指针
  • 包含必要的内存屏障
  • 返回的指针只在 RCU 临界区内保证有效

步骤 3: refcount_inc_not_zero(&obj->refcnt)
───────────────────────────────────────────
  • 这是关键的"交接"操作
  • 尝试将生命周期管理从 RCU 转交给 refcount
  • 如果失败（返回 0），说明对象即将被释放，放弃使用
  • 如果成功（返回非0），我们拥有了长期引用

步骤 4: rcu_read_unlock()
─────────────────────────
  • 离开 RCU 临界区
  • 此后 RCU 不再保护对象
  • 但我们已经持有 refcount，对象不会被释放

步骤 5: 长期使用
────────────────
  • 因为持有引用，对象保证存活
  • 可以执行耗时操作
  • 可以睡眠（如果在进程上下文）
  • 可以跨多个函数传递对象

步骤 6: refcount_dec(&obj->refcnt)
──────────────────────────────────
  • 释放我们持有的引用
  • 如果是最后一个引用，可能触发对象释放
  • 释放后不能再使用 obj

═══════════════════════════════════════════════════════════════════════════════
```

### 14.5 真实内核代码分析：`__inet_lookup_established()`

这是 Linux 内核网络子系统中查找 TCP socket 的核心函数。

```c
/* net/ipv4/inet_hashtables.c */

struct sock *__inet_lookup_established(struct net *net,
                  struct inet_hashinfo *hashinfo,
                  const __be32 saddr, const __be16 sport,
                  const __be32 daddr, const u16 hnum,
                  const int dif)
{
    INET_ADDR_COOKIE(acookie, saddr, daddr)
    const __portpair ports = INET_COMBINED_PORTS(sport, hnum);
    struct sock *sk;
    const struct hlist_nulls_node *node;
    unsigned int hash = inet_ehashfn(net, daddr, hnum, saddr, sport);
    unsigned int slot = hash & hashinfo->ehash_mask;
    struct inet_ehash_bucket *head = &hashinfo->ehash[slot];

    rcu_read_lock();                              /* ← 步骤1: 进入 RCU 临界区 */
begin:
    sk_nulls_for_each_rcu(sk, node, &head->chain) {  /* ← RCU 安全遍历 */
        if (INET_MATCH(sk, net, hash, acookie,
                    saddr, daddr, ports, dif)) {
            /*
             * 找到匹配的 socket！
             * 但是，这个 socket 可能正在被关闭...
             */
            if (unlikely(!atomic_inc_not_zero(&sk->sk_refcnt)))
                goto begintw;                     /* ← 步骤3: 获取引用失败，跳过 */
            /*
             * 成功获取引用后，必须重新验证匹配条件！
             * 因为在我们获取引用的过程中，socket 可能已经改变
             */
            if (unlikely(!INET_MATCH(sk, net, hash, acookie,
                saddr, daddr, ports, dif))) {
                sock_put(sk);                     /* ← 验证失败，释放引用 */
                goto begin;                       /* ← 重新开始查找 */
            }
            goto out;                             /* ← 成功！ */
        }
    }
    /* ... TIME_WAIT socket 处理 ... */
    sk = NULL;
out:
    rcu_read_unlock();                            /* ← 步骤4: 离开 RCU 临界区 */
    return sk;                                    /* ← 返回带引用的 socket */
}
```

**代码分析图解**:

```
═══════════════════════════════════════════════════════════════════════════════
            __inet_lookup_established() 执行流程
═══════════════════════════════════════════════════════════════════════════════

                          开始
                            │
                            ▼
                    ┌───────────────┐
                    │ rcu_read_lock │
                    └───────┬───────┘
                            │
              ┌─────────────▼─────────────┐
     begin:   │  遍历 hash 链表           │◄───────┐
              │  sk_nulls_for_each_rcu()  │        │
              └─────────────┬─────────────┘        │
                            │                      │
                    ┌───────▼───────┐              │
                    │ INET_MATCH?   │              │
                    └───────┬───────┘              │
                   No │           │ Yes            │
                      │           ▼                │
                      │   ┌───────────────────┐    │
                      │   │atomic_inc_not_zero│    │
                      │   │  (&sk->sk_refcnt) │    │
                      │   └─────────┬─────────┘    │
                      │             │              │
                      │    返回0    │    返回1     │
                      │    (失败)   │    (成功)    │
                      │      │      │              │
                      │      ▼      ▼              │
                      │   ┌─────┐ ┌─────────────┐  │
                      │   │goto │ │ 重新验证    │  │
                      │   │begin│ │ INET_MATCH  │  │
                      │   │ tw  │ └──────┬──────┘  │
                      │   └──┬──┘        │         │
                      │      │    失败   │  成功   │
                      │      │     │     │         │
                      │      │     ▼     ▼         │
                      │      │  ┌─────┐ ┌────┐     │
                      │      │  │sock_│ │goto│     │
                      │      │  │put()│ │out │     │
                      │      │  └──┬──┘ └─┬──┘     │
                      │      │     │      │        │
                      │      │     └──────┼────────┘
                      │      │            │
                      └──────┴────────────┘
                                          │
                                          ▼
                                  ┌───────────────┐
                            out:  │rcu_read_unlock│
                                  └───────┬───────┘
                                          │
                                          ▼
                                  ┌───────────────┐
                                  │  return sk    │
                                  │ (带引用 或   │
                                  │  NULL)        │
                                  └───────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

**为什么需要"double-check"（重新验证）？**

```
═══════════════════════════════════════════════════════════════════════════════
              为什么 atomic_inc_not_zero 后还要重新验证？
═══════════════════════════════════════════════════════════════════════════════

竞态场景:
─────────

  Thread A (Lookup)                    Thread B (Socket移动)
  ─────────────────                    ──────────────────────
  
  rcu_read_lock();
  
  遍历找到 sk
  if (INET_MATCH(sk, ...)) {          
      // sk 匹配！准备获取引用
                                       // 另一个 CPU 重新 hash 这个 socket
                                       spin_lock(&hash_lock);
                                       sk_nulls_del_node_init_rcu(sk);
                                       // sk 的参数可能改变
                                       // 然后加入到另一个 hash bucket
                                       sk_nulls_add_node_rcu(sk, &other_head);
                                       spin_unlock(&hash_lock);
      
      atomic_inc_not_zero(&sk->refcnt);  // 成功，refcnt++
      // 但是！sk 可能已经不匹配我们要找的了
      
      if (!INET_MATCH(sk, ...)) {      // 重新验证：失败！
          sock_put(sk);                 // 释放引用
          goto begin;                   // 重新查找
      }
  }
  
  rcu_read_unlock();


关键洞察:
─────────
• RCU 保证我们看到的指针指向有效内存
• 但不保证对象的状态没有改变
• socket 可能被重新 hash 到另一个位置
• socket 的参数（端口、地址）可能已经改变
• 必须在持有引用后重新验证

═══════════════════════════════════════════════════════════════════════════════
```

### 14.6 另一个真实例子：`nf_conntrack` 连接跟踪

```c
/* net/netfilter/nf_conntrack_core.c */

/*
 * 查找连接跟踪条目
 */
struct nf_conntrack_tuple_hash *
nf_conntrack_find_get(struct net *net, u16 zone,
                      const struct nf_conntrack_tuple *tuple)
{
    struct nf_conntrack_tuple_hash *h;
    struct nf_conn *ct;

    rcu_read_lock();
begin:
    h = __nf_conntrack_find(net, zone, tuple);
    if (h) {
        ct = nf_ct_tuplehash_to_ctrack(h);
        /*
         * 检查连接是否正在被销毁
         * 以及尝试获取引用
         */
        if (unlikely(nf_ct_is_dying(ct) ||
                     !atomic_inc_not_zero(&ct->ct_general.use)))
            h = NULL;                          /* 获取失败 */
            /* 注意：这里没有 goto begin
             * 因为 dying 状态意味着连接确实要被删除了
             */
    }
    rcu_read_unlock();

    return h;
}
```

**这个例子展示了额外的检查：`nf_ct_is_dying(ct)`**

```
═══════════════════════════════════════════════════════════════════════════════
              额外的状态检查：nf_ct_is_dying()
═══════════════════════════════════════════════════════════════════════════════

为什么除了 atomic_inc_not_zero 还要检查 dying 标志？
───────────────────────────────────────────────────────

场景: 连接正在被有序关闭

  时刻 T1: refcnt = 2 (有其他引用)
           dying = false
           
  时刻 T2: 决定关闭连接
           dying = true     ← 设置 dying 标志
           refcnt = 2       ← refcnt 还不是 0
           
  时刻 T3: 其他引用释放
           refcnt = 1
           
  时刻 T4: 新的查找发生
           • atomic_inc_not_zero 会成功 (refcnt 是 1，不是 0)
           • 但这个连接已经"逻辑上死亡"了
           • 不应该再被新的使用者引用


dying 标志的作用:
─────────────────
• 在 refcnt 变成 0 之前就表明对象"不再接受新引用"
• 提供更清晰的生命周期管理
• 避免在对象即将销毁时还获取到它

模式:
────
if (nf_ct_is_dying(ct) ||               // 检查逻辑状态
    !atomic_inc_not_zero(&ct->use))     // 检查物理状态
    return NULL;

═══════════════════════════════════════════════════════════════════════════════
```

### 14.7 总结：RCU + Refcount 的不变量

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                RCU + Refcount 组合使用的 5 条不变量                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. RCU 临界区内获取的指针只在临界区内有效                                   │
│     • 离开 rcu_read_unlock() 后，指针可能指向已释放的内存                   │
│     • 如果需要长期使用，必须持有引用计数                                     │
│                                                                             │
│  2. 必须使用 atomic_inc_not_zero()，不能使用 atomic_inc()                   │
│     • atomic_inc() 会"复活"正在被释放的对象                                 │
│     • atomic_inc_not_zero() 会检测并拒绝获取 refcnt=0 的对象                │
│                                                                             │
│  3. atomic_inc_not_zero() 必须在 RCU 临界区内调用                           │
│     • RCU 保证我们读到的指针指向有效内存                                     │
│     • 在临界区外，对象可能已经被释放，atomic 操作会访问无效内存              │
│                                                                             │
│  4. 获取引用成功后可能需要重新验证对象状态                                   │
│     • 对象可能在我们检查和获取引用之间发生变化                               │
│     • 特别是当对象可以被移动或修改时                                         │
│                                                                             │
│  5. 离开 RCU 临界区后，生命周期完全由引用计数管理                            │
│     • RCU 不再提供任何保护                                                   │
│     • 最后一个 refcount_dec() 会触发释放                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 14.8 完整的对比表

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    RCU 与 Refcount 的对比                                    │
├─────────────────┬─────────────────────────┬─────────────────────────────────┤
│     特性        │         RCU             │       Reference Counting        │
├─────────────────┼─────────────────────────┼─────────────────────────────────┤
│ 保护范围        │ 仅 RCU 临界区内         │ 持有引用期间                    │
├─────────────────┼─────────────────────────┼─────────────────────────────────┤
│ 获取开销        │ 极低 (preempt_disable)  │ 原子操作                        │
├─────────────────┼─────────────────────────┼─────────────────────────────────┤
│ 释放时机        │ 宽限期后批量            │ 最后引用释放时立即              │
├─────────────────┼─────────────────────────┼─────────────────────────────────┤
│ 可以睡眠?       │ 不能 (普通RCU)          │ 可以                            │
├─────────────────┼─────────────────────────┼─────────────────────────────────┤
│ 跨函数传递?     │ 不能 (临界区受限)       │ 可以                            │
├─────────────────┼─────────────────────────┼─────────────────────────────────┤
│ 精确跟踪?       │ 不跟踪读者              │ 精确计数                        │
├─────────────────┼─────────────────────────┼─────────────────────────────────┤
│ 适合场景         │ 短暂的只读查找           │ 长期持有和操作                  │
└─────────────────┴─────────────────────────┴─────────────────────────────────┘
```

### 14.9 RCU + refcount 时间轴
```
# RCU 负责“发现对象那一瞬间是安全的”
RCU 保护的区间
    rcu_read_lock()
    ...
    rcu_read_unlock()
    
    只保护 lookup
    时间非常短
    不允许睡眠
    不保证后续安全

    👉 这是 “瞬间安全”

# refcount 负责“对象被使用期间不会死”
refcount 从哪里开始接力
    refcount_inc_not_zero()

在 RCU 区间内
把“对象的生命”接管过来
防止对象被释放

👉 这是 “长期安全”

# Simplified:

Time →
RCU:      [ lookup safe ]
refcount:        [ safe usage ------------------ ]


# Detaild:

Time  ─────────────────────────────────────────────────────────→

Reader CPU (RX path)
│
│  rcu_read_lock()
│  │
│  │   obj = rcu_dereference(global_ptr)
│  │   if (obj && refcount_inc_not_zero(&obj->refcnt))
│  │
│  rcu_read_unlock()
│
│  ─────────────── safe long-term usage ───────────────
│                  (sleep / work / callbacks)
│
│  refcount_dec(&obj->refcnt)
│
│
│
Writer CPU (delete/update path)
│
│               remove obj from global_ptr
│               (no new readers can find it)
│
│               wait for grace period
│               (synchronize_rcu / call_rcu)
│
│                                       refcount reaches zero
│                                       free(obj)
│

```

---

**文档版本**: 基于 Linux Kernel v3.2  
**最后更新**: 针对 rcutree.c 和 rcupdate.h 的分析，增加 RCU + Refcount 组合模式

