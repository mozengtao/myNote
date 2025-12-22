# Linux Kernel Atomic Operations (v3.2)

## Overview

This document explains how atomic operations are used in Linux kernel v3.2, focusing on **architectural intent, correctness contracts, and performance tradeoffs** — and how these ideas translate to user-space C/C++ applications.

---

## Phase 1 — Why Atomic Operations Exist

### The Problem Atomics Solve

```
+------------------------------------------------------------------+
|  THE PROBLEM: NON-ATOMIC READ-MODIFY-WRITE                       |
+------------------------------------------------------------------+

    Plain C code: counter++;
    
    Compiles to (non-atomic):
    
    CPU 1                       CPU 2
    ─────────────────────────   ─────────────────────────
    load counter → reg         load counter → reg
    add reg, 1                  add reg, 1
    store reg → counter        store reg → counter
    
    TIMELINE (race condition):
    
    Time │ CPU 1              │ CPU 2              │ counter
    ─────┼────────────────────┼────────────────────┼─────────
      1  │ load (gets 5)      │                    │    5
      2  │                    │ load (gets 5)      │    5
      3  │ add (reg = 6)      │                    │    5
      4  │                    │ add (reg = 6)      │    5
      5  │ store (counter=6)  │                    │    6
      6  │                    │ store (counter=6)  │    6
    
    EXPECTED: 7   ACTUAL: 6   ← LOST UPDATE!
```

**中文解释：**
- 普通的 C 语言 `counter++` 不是原子操作
- 编译成多条指令：加载、增加、存储
- 两个 CPU 并发执行时可能产生竞争条件
- 结果：更新丢失（Lost Update）

### Why Plain Loads/Stores are Insufficient

```
+------------------------------------------------------------------+
|  PROBLEMS WITH PLAIN MEMORY ACCESS                               |
+------------------------------------------------------------------+

    1. TORN READS/WRITES (unaligned or >word size)
    +----------------------------------------------------------+
    | 64-bit value on 32-bit system:                            |
    | - Write of high 32 bits and low 32 bits is NOT atomic     |
    | - Reader may see half-old, half-new value                 |
    +----------------------------------------------------------+
    
    2. COMPILER REORDERING
    +----------------------------------------------------------+
    | flag = 1;                May become:   data = ...; 
    | data = ...;                            flag = 1;
    |                                                           |
    | Compiler optimizes for single-threaded correctness only   |
    +----------------------------------------------------------+
    
    3. CPU REORDERING
    +----------------------------------------------------------+
    | Store buffer, write combining, out-of-order execution     |
    | CPU may complete stores in different order than issued    |
    +----------------------------------------------------------+
    
    4. CACHE COHERENCY DELAYS
    +----------------------------------------------------------+
    | Write to one CPU's cache may not be visible to others     |
    | immediately without proper synchronization                |
    +----------------------------------------------------------+
```

**中文解释：**
- **撕裂读写**：大于字长的数据可能读到一半新一半旧
- **编译器重排序**：编译器为优化可能改变指令顺序
- **CPU 重排序**：现代 CPU 乱序执行，存储可能延迟
- **缓存一致性延迟**：一个 CPU 的写入可能不会立即对其他 CPU 可见

### Why Atomics are NOT "Lightweight Locks"

```
+------------------------------------------------------------------+
|  ATOMICS vs LOCKS: FUNDAMENTAL DIFFERENCE                        |
+------------------------------------------------------------------+

    LOCK:
    +----------------------------------------------------------+
    | - Provides MUTUAL EXCLUSION                               |
    | - Protects a CRITICAL SECTION (multiple operations)       |
    | - Guarantees only one thread executes protected code      |
    | - Can protect compound invariants                         |
    +----------------------------------------------------------+
    
    ATOMIC:
    +----------------------------------------------------------+
    | - Provides SINGLE OPERATION atomicity only                |
    | - NO mutual exclusion                                     |
    | - NO protection for multi-step invariants                 |
    | - Each atomic op is independent                           |
    +----------------------------------------------------------+
    
    WRONG MENTAL MODEL:
    
        "atomic_inc is like a tiny lock around counter++"
        
    CORRECT MENTAL MODEL:
    
        "atomic_inc is a SINGLE indivisible operation
         that cannot be interrupted or observed mid-flight"
    
    EXAMPLE OF INCORRECT USE:
    
        /* WRONG: Two atomics don't form a transaction! */
        if (atomic_read(&available) > 0) {   /* Check */
            atomic_dec(&available);           /* Act */
        }
        /* Race: Another CPU may have decremented between check and act */
```

**中文解释：**
- 锁（Lock）：提供互斥，保护临界区，可保护多步不变量
- 原子操作：只保证单个操作的原子性，无互斥，无法保护多步不变量
- 错误心智模型："原子操作是小型锁"
- 正确心智模型："原子操作是不可分割的单一操作"
- 两个原子操作之间没有事务保证

### When Atomics Should NOT Be Used

```
+------------------------------------------------------------------+
|  WHEN NOT TO USE ATOMICS                                         |
+------------------------------------------------------------------+

    1. COMPOUND INVARIANTS
    +----------------------------------------------------------+
    | If correctness depends on multiple values being           |
    | consistent with each other → USE LOCK                     |
    |                                                           |
    | Example: Updating both head and tail of a queue           |
    +----------------------------------------------------------+
    
    2. COMPLEX CRITICAL SECTIONS
    +----------------------------------------------------------+
    | If operation involves multiple steps that must all        |
    | complete together → USE LOCK                              |
    |                                                           |
    | Example: Updating linked list pointers                    |
    +----------------------------------------------------------+
    
    3. WHEN LOCK CONTENTION IS LOW
    +----------------------------------------------------------+
    | Locks have low overhead when uncontended                  |
    | Don't optimize prematurely with atomics                   |
    +----------------------------------------------------------+
    
    4. WHEN CORRECTNESS IS SUBTLE
    +----------------------------------------------------------+
    | Lock-free code is notoriously difficult to get right      |
    | If you're not sure → USE LOCK first                       |
    +----------------------------------------------------------+
```

**中文解释：**
- 不应使用原子操作的场景：
  1. 复合不变量：多个值需要一致性 → 使用锁
  2. 复杂临界区：多步操作必须一起完成 → 使用锁
  3. 锁竞争低时：无竞争锁开销很低，不要过早优化
  4. 正确性微妙时：无锁代码难以正确实现

---

## Phase 2 — Atomic Types in Linux Kernel

### atomic_t

From `include/linux/types.h`:

```c
typedef struct {
    int counter;
} atomic_t;
```

```
+------------------------------------------------------------------+
|  atomic_t SEMANTICS                                              |
+------------------------------------------------------------------+

    PURPOSE:
    +----------------------------------------------------------+
    | - 32-bit atomic integer                                   |
    | - Used for reference counting, statistics, flags          |
    | - Architecture-independent abstraction                    |
    +----------------------------------------------------------+
    
    WHAT IS GUARANTEED:
    +----------------------------------------------------------+
    | - Single read/write is atomic (no torn values)            |
    | - RMW operations (inc, dec, add) are atomic               |
    | - Operations complete before function returns             |
    +----------------------------------------------------------+
    
    WHAT IS NOT GUARANTEED:
    +----------------------------------------------------------+
    | - NO memory ordering (unless specified)                   |
    | - Multiple atomics are NOT transactional                  |
    | - atomic_read may return stale cache value                |
    +----------------------------------------------------------+
    
    TYPICAL USE CASES:
    +----------------------------------------------------------+
    | 1. Reference counting (i_count, users, refcount)          |
    | 2. Statistics counters                                    |
    | 3. Simple state flags                                     |
    | 4. Sequence numbers                                       |
    +----------------------------------------------------------+
    
    COMMON MISUSE:
    +----------------------------------------------------------+
    | ✗ Using for pointer-and-count pairs                       |
    | ✗ Checking then acting without cmpxchg                    |
    | ✗ Assuming ordering between different atomics             |
    +----------------------------------------------------------+
```

**中文解释：**
- `atomic_t`：32位原子整数类型
- **保证**：单个读写原子、RMW 操作原子
- **不保证**：内存顺序、多原子操作事务性
- 典型用途：引用计数、统计计数器、简单状态标志
- 常见误用：检查后操作、假设不同原子间有顺序

### atomic64_t

From `include/linux/types.h`:

```c
#ifdef CONFIG_64BIT
typedef struct {
    long counter;
} atomic64_t;
#endif
```

```
+------------------------------------------------------------------+
|  atomic64_t SEMANTICS                                            |
+------------------------------------------------------------------+

    PURPOSE:
    +----------------------------------------------------------+
    | - 64-bit atomic integer                                   |
    | - Required when 32 bits is insufficient                   |
    | - On 32-bit systems, uses lock or cmpxchg8b               |
    +----------------------------------------------------------+
    
    WHY EXISTS:
    +----------------------------------------------------------+
    | - Large counters (bytes transferred, timestamps)          |
    | - 64-bit pointers on 64-bit systems                       |
    | - Overflow-resistant counters                             |
    +----------------------------------------------------------+
    
    ARCHITECTURE DIFFERENCES:
    +----------------------------------------------------------+
    | 64-bit system: Native 64-bit atomic instructions          |
    | 32-bit system: May use cmpxchg8b or locks (expensive!)    |
    +----------------------------------------------------------+
```

**中文解释：**
- `atomic64_t`：64位原子整数
- 用途：大计数器、64位指针、防溢出计数器
- 架构差异：64位系统用原生指令，32位系统可能使用锁

### kref (Reference Counting Wrapper)

From `include/linux/kref.h`:

```c
struct kref {
    atomic_t refcount;
};
```

From `lib/kref.c`:

```c
void kref_init(struct kref *kref)
{
    atomic_set(&kref->refcount, 1);
    smp_mb();  /* Ensure initialization is visible */
}

void kref_get(struct kref *kref)
{
    WARN_ON(!atomic_read(&kref->refcount));  /* Catch use-after-free */
    atomic_inc(&kref->refcount);
    smp_mb__after_atomic_inc();              /* Memory barrier */
}

int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    if (atomic_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;
    }
    return 0;
}
```

```
+------------------------------------------------------------------+
|  kref PATTERN                                                    |
+------------------------------------------------------------------+

    PURPOSE:
    +----------------------------------------------------------+
    | - Standard refcounting pattern                            |
    | - Enforces correct memory barriers                        |
    | - Includes debugging checks (WARN_ON)                     |
    +----------------------------------------------------------+
    
    CONTRACT:
    +----------------------------------------------------------+
    | 1. kref_init() before any use                             |
    | 2. kref_get() only when you KNOW refcount > 0             |
    | 3. kref_put() calls release() when count hits 0           |
    | 4. After kref_put(), object may be freed                  |
    +----------------------------------------------------------+
    
    WHY NOT JUST atomic_t:
    +----------------------------------------------------------+
    | - kref adds memory barriers automatically                 |
    | - kref includes debugging assertions                      |
    | - kref enforces the release callback pattern              |
    +----------------------------------------------------------+
```

**中文解释：**
- `kref`：引用计数的标准包装器
- 自动添加内存屏障、调试断言
- 契约：init 初始化、get 仅当计数 > 0、put 减到 0 时调用释放函数

---

## Phase 3 — Atomic Operations & Semantics

### atomic_read / atomic_set

From `arch/x86/include/asm/atomic.h`:

```c
static inline int atomic_read(const atomic_t *v)
{
    return (*(volatile int *)&(v)->counter);
}

static inline void atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}
```

```
+------------------------------------------------------------------+
|  atomic_read / atomic_set SEMANTICS                              |
+------------------------------------------------------------------+

    atomic_read(v):
    +----------------------------------------------------------+
    | - Reads the current value atomically                      |
    | - Uses volatile to prevent compiler caching               |
    | - NO memory barrier                                       |
    | - May return stale value from CPU cache                   |
    +----------------------------------------------------------+
    
    atomic_set(v, i):
    +----------------------------------------------------------+
    | - Sets the value atomically                               |
    | - Simple aligned store (atomic on most architectures)     |
    | - NO memory barrier                                       |
    | - Other CPUs may not see it immediately                   |
    +----------------------------------------------------------+
    
    WHY volatile:
    +----------------------------------------------------------+
    | - Prevents compiler from optimizing away repeated reads   |
    | - Ensures load is actually performed                      |
    | - Does NOT prevent CPU reordering!                        |
    +----------------------------------------------------------+
    
    COMMON MISUSE:
    +----------------------------------------------------------+
    | ✗ Assuming atomic_read sees latest value                  |
    | ✗ Using atomic_set to "publish" data to other CPUs        |
    |   (need smp_wmb() before, smp_rmb() after on reader side) |
    +----------------------------------------------------------+
```

**中文解释：**
- `atomic_read`：原子读取当前值，用 volatile 防止编译器缓存，无内存屏障
- `atomic_set`：原子设置值，简单对齐存储，无内存屏障
- volatile 只防止编译器优化，不防止 CPU 重排序
- 常见误用：假设读到最新值、用 set 发布数据给其他 CPU

### atomic_inc / atomic_dec

From `arch/x86/include/asm/atomic.h`:

```c
static inline void atomic_inc(atomic_t *v)
{
    asm volatile(LOCK_PREFIX "incl %0"
                 : "+m" (v->counter));
}

static inline void atomic_dec(atomic_t *v)
{
    asm volatile(LOCK_PREFIX "decl %0"
                 : "+m" (v->counter));
}
```

```
+------------------------------------------------------------------+
|  atomic_inc / atomic_dec SEMANTICS                               |
+------------------------------------------------------------------+

    WHAT HAPPENS:
    +----------------------------------------------------------+
    | 1. CPU issues LOCK prefix (x86)                           |
    | 2. Bus/cache lock acquired                                |
    | 3. Read-modify-write completes atomically                 |
    | 4. Lock released                                          |
    +----------------------------------------------------------+
    
    GUARANTEES:
    +----------------------------------------------------------+
    | - No other CPU can observe intermediate state             |
    | - On x86, LOCK also acts as full memory barrier           |
    | - Increment/decrement never lost                          |
    +----------------------------------------------------------+
    
    NO RETURN VALUE:
    +----------------------------------------------------------+
    | - Cannot know what the new value is                       |
    | - Use atomic_inc_return() if you need the new value       |
    | - Use atomic_dec_and_test() for refcounting               |
    +----------------------------------------------------------+
```

**中文解释：**
- x86 使用 LOCK 前缀保证原子性
- 保证：其他 CPU 无法观察到中间状态
- 在 x86 上，LOCK 同时作为完整内存屏障
- 无返回值，需要返回值用 `atomic_inc_return()` 或 `atomic_dec_and_test()`

### atomic_add_return

From `arch/x86/include/asm/atomic.h`:

```c
static inline int atomic_add_return(int i, atomic_t *v)
{
    /* Modern 486+ processor */
    return i + xadd(&v->counter, i);
}
```

```
+------------------------------------------------------------------+
|  atomic_add_return SEMANTICS                                     |
+------------------------------------------------------------------+

    WHY RETURN VARIANT EXISTS:
    +----------------------------------------------------------+
    | Common pattern: "add and check result"                    |
    |                                                           |
    | WRONG:                                                    |
    | atomic_add(1, &v);                                        |
    | if (atomic_read(&v) == threshold) { ... } /* RACE! */     |
    |                                                           |
    | CORRECT:                                                  |
    | if (atomic_add_return(1, &v) == threshold) { ... }        |
    +----------------------------------------------------------+
    
    GUARANTEES:
    +----------------------------------------------------------+
    | - Returns the NEW value after addition                    |
    | - Addition and return are atomic together                 |
    | - No other CPU can increment between add and read         |
    +----------------------------------------------------------+
    
    VARIATIONS:
    +----------------------------------------------------------+
    | atomic_inc_return(v)  = atomic_add_return(1, v)           |
    | atomic_dec_return(v)  = atomic_add_return(-1, v)          |
    | atomic_sub_return(i,v)= atomic_add_return(-i, v)          |
    +----------------------------------------------------------+
```

**中文解释：**
- 返回变体存在原因：避免"加后检查"的竞争条件
- 保证：返回加法后的新值，加法和返回是原子的
- 变体：`atomic_inc_return`、`atomic_dec_return`、`atomic_sub_return`

### atomic_cmpxchg (Compare-And-Exchange)

From `arch/x86/include/asm/atomic.h`:

```c
static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
    return cmpxchg(&v->counter, old, new);
}
```

```
+------------------------------------------------------------------+
|  atomic_cmpxchg SEMANTICS (CAS)                                  |
+------------------------------------------------------------------+

    OPERATION:
    +----------------------------------------------------------+
    | Atomically:                                               |
    |   if (*v == old) {                                        |
    |       *v = new;                                           |
    |       return old;  /* Success */                          |
    |   } else {                                                |
    |       return *v;   /* Failure: return current value */    |
    |   }                                                       |
    +----------------------------------------------------------+
    
    THE FUNDAMENTAL BUILDING BLOCK:
    +----------------------------------------------------------+
    | - All other atomic RMW can be built from cmpxchg          |
    | - Enables lock-free algorithms                            |
    | - Foundation of wait-free data structures                 |
    +----------------------------------------------------------+
    
    TYPICAL CAS LOOP PATTERN:
    +----------------------------------------------------------+
    | int old, new;                                             |
    | do {                                                      |
    |     old = atomic_read(v);                                 |
    |     new = transform(old);  /* Compute desired new value */|
    | } while (atomic_cmpxchg(v, old, new) != old);             |
    |                                                           |
    | If cmpxchg fails (someone else changed v), retry.         |
    +----------------------------------------------------------+
    
    EXAMPLE: atomic_add_unless
    +----------------------------------------------------------+
    | /* Add 'a' unless value equals 'u' */                     |
    | static inline int __atomic_add_unless(atomic_t *v,        |
    |                                        int a, int u) {    |
    |     int c, old;                                           |
    |     c = atomic_read(v);                                   |
    |     for (;;) {                                            |
    |         if (unlikely(c == u))                             |
    |             break;                                        |
    |         old = atomic_cmpxchg(v, c, c + a);                |
    |         if (likely(old == c))                             |
    |             break;                                        |
    |         c = old;  /* Retry with new value */              |
    |     }                                                     |
    |     return c;                                             |
    | }                                                         |
    +----------------------------------------------------------+
```

**中文解释：**
- `cmpxchg`（CAS）：比较并交换，原子的"检查-更新"操作
- 如果当前值等于 old，则设置为 new 并返回 old（成功）
- 如果当前值不等于 old，返回当前值（失败）
- 是所有无锁算法的基础，所有其他原子 RMW 都可用 cmpxchg 构建
- 典型模式：CAS 循环，失败时重试

---

## Phase 4 — Memory Ordering & Barriers (Critical)

### Why Memory Ordering Matters

```
+------------------------------------------------------------------+
|  THE MEMORY ORDERING PROBLEM                                     |
+------------------------------------------------------------------+

    SCENARIO: Producer-Consumer with flag
    
    Producer (CPU 0):              Consumer (CPU 1):
    ─────────────────              ─────────────────
    data = 42;                     while (!flag) ;
    flag = 1;                      print(data);
    
    EXPECTED: print 42
    
    BUT WITHOUT BARRIERS:
    
    CPU 0 may reorder:             CPU 1 may reorder:
    flag = 1;                      load data (gets 0!)
    data = 42;                     load flag (sees 1)
                                   print 0  ← WRONG!
    
    OR:
    
    CPU 0 writes to store buffer   CPU 1 sees flag=1 from cache
    flag visible before data       data still 0 in cache
```

```
+------------------------------------------------------------------+
|  MEMORY ORDERING GUARANTEES                                      |
+------------------------------------------------------------------+

    NO ORDERING (default for atomic_t):
    +----------------------------------------------------------+
    | atomic_set/read provide NO ordering with other memory     |
    | operations. Compiler and CPU may reorder freely.          |
    +----------------------------------------------------------+
    
    FULL BARRIER (smp_mb):
    +----------------------------------------------------------+
    | All memory operations before the barrier complete         |
    | before any operations after the barrier start.            |
    |                                                           |
    | LOAD  ─┐                                                  |
    | STORE ─┤                                                  |
    |        ├─ smp_mb() ─────────────────────────────          |
    | LOAD  ─┤                        │                         |
    | STORE ─┘                        ▼                         |
    |                          All must complete                |
    |                          before these start               |
    +----------------------------------------------------------+
    
    WRITE BARRIER (smp_wmb):
    +----------------------------------------------------------+
    | All STORES before the barrier complete before             |
    | any STORES after the barrier.                             |
    | Does NOT order loads.                                     |
    +----------------------------------------------------------+
    
    READ BARRIER (smp_rmb):
    +----------------------------------------------------------+
    | All LOADS before the barrier complete before              |
    | any LOADS after the barrier.                              |
    | Does NOT order stores.                                    |
    +----------------------------------------------------------+
```

**中文解释：**
- **问题**：CPU 和编译器可能重排序内存操作
- **无顺序**：默认 atomic_t 操作不保证与其他内存操作的顺序
- **完整屏障 smp_mb()**：屏障前所有操作完成后，屏障后操作才开始
- **写屏障 smp_wmb()**：仅排序写操作
- **读屏障 smp_rmb()**：仅排序读操作

### Barrier Definitions for x86

From `arch/x86/include/asm/system.h`:

```c
#define mb()   asm volatile("mfence":::"memory")
#define rmb()  asm volatile("lfence":::"memory")
#define wmb()  asm volatile("sfence" ::: "memory")

#ifdef CONFIG_SMP
#define smp_mb()  mb()
#define smp_rmb() barrier()  /* x86 doesn't reorder reads */
#define smp_wmb() barrier()  /* x86 doesn't reorder writes */
#else
#define smp_mb()  barrier()
#define smp_rmb() barrier()
#define smp_wmb() barrier()
#endif
```

```
+------------------------------------------------------------------+
|  x86 MEMORY MODEL (Total Store Ordering)                        |
+------------------------------------------------------------------+

    x86 IS RELATIVELY WELL-BEHAVED:
    +----------------------------------------------------------+
    | - Stores are NOT reordered with other stores              |
    | - Loads are NOT reordered with other loads                |
    | - Loads may be reordered with earlier stores              |
    |   (Store → Load reordering IS allowed)                    |
    +----------------------------------------------------------+
    
    WHY smp_wmb() = barrier() on x86:
    +----------------------------------------------------------+
    | x86 hardware already guarantees store-store ordering      |
    | Only need compiler barrier to prevent compiler reordering |
    +----------------------------------------------------------+
    
    WHEN FULL BARRIER IS NEEDED:
    +----------------------------------------------------------+
    | When you need Store → Load ordering:                      |
    |                                                           |
    | *ptr = value;   /* Store */                               |
    | smp_mb();       /* Needed! */                             |
    | flag = 1;       /* Store that others will load */         |
    +----------------------------------------------------------+
    
    LOCK PREFIX INCLUDES BARRIER:
    +----------------------------------------------------------+
    | On x86, atomic RMW operations (inc, dec, xchg, cmpxchg)   |
    | with LOCK prefix are FULL memory barriers.                |
    |                                                           |
    | This is why smp_mb__before_atomic_dec() is just barrier() |
    +----------------------------------------------------------+
```

**中文解释：**
- x86 内存模型（TSO）相对有序：
  - 写不会与写重排序
  - 读不会与读重排序
  - 读可能与之前的写重排序（Store→Load 可重排）
- 在 x86 上 `smp_wmb()` 只是编译器屏障
- LOCK 前缀的原子操作自带完整内存屏障

### Barrier Placement Rules

```
+------------------------------------------------------------------+
|  BARRIER PLACEMENT RULES                                         |
+------------------------------------------------------------------+

    RULE 1: PUBLISH PATTERN
    +----------------------------------------------------------+
    | /* Producer */                                            |
    | initialize(object);                                       |
    | smp_wmb();           /* Ensure init visible before ptr */ |
    | shared_ptr = object;                                      |
    |                                                           |
    | /* Consumer */                                            |
    | ptr = shared_ptr;                                         |
    | smp_rmb();           /* Ensure ptr read before contents */|
    | use(ptr);                                                 |
    +----------------------------------------------------------+
    
    RULE 2: FLAG PATTERN
    +----------------------------------------------------------+
    | /* Writer */                                              |
    | data = value;                                             |
    | smp_wmb();           /* Data before flag */               |
    | flag = 1;                                                 |
    |                                                           |
    | /* Reader */                                              |
    | while (!flag) cpu_relax();                                |
    | smp_rmb();           /* Flag before data */               |
    | use(data);                                                |
    +----------------------------------------------------------+
    
    RULE 3: PAIRED BARRIERS
    +----------------------------------------------------------+
    | Write barrier on writer side MUST be paired with          |
    | read barrier on reader side.                              |
    |                                                           |
    | Single-sided barrier is usually a BUG.                    |
    +----------------------------------------------------------+
```

**中文解释：**
- **发布模式**：初始化数据 → 写屏障 → 发布指针；获取指针 → 读屏障 → 使用数据
- **标志模式**：写数据 → 写屏障 → 设置标志；等待标志 → 读屏障 → 读数据
- **配对规则**：写屏障必须与读屏障配对，单边屏障通常是 bug

---

## Phase 5 — Real Kernel Usage Patterns

### Pattern 1: Reference Counting (struct file)

From `fs/file_table.c`:

```c
/* In struct file (fs.h): atomic_long_t f_count; */

/* Get a reference */
static inline struct file *get_file(struct file *f)
{
    atomic_long_inc(&f->f_count);
    return f;
}

/* Release a reference */
void fput(struct file *file)
{
    if (atomic_long_dec_and_test(&file->f_count)) {
        /* Last reference - schedule cleanup */
        call_rcu(&file->f_u.fu_rcuhead, file_free_rcu);
    }
}
```

```
+------------------------------------------------------------------+
|  PATTERN: REFERENCE COUNTING                                     |
+------------------------------------------------------------------+

    WHY ATOMIC:
    +----------------------------------------------------------+
    | - Multiple CPUs may hold references simultaneously        |
    | - inc/dec must not lose updates                           |
    | - dec_and_test must be atomic to prevent double-free      |
    +----------------------------------------------------------+
    
    WHAT WOULD BREAK:
    +----------------------------------------------------------+
    | Plain int:                                                |
    | - Lost updates → premature free or memory leak            |
    | - Race in check-then-free → double free                   |
    |                                                           |
    | Lock instead:                                             |
    | - Works but higher overhead for simple operations         |
    | - Every reference get/put would need to acquire lock      |
    +----------------------------------------------------------+
    
    INVARIANT:
    +----------------------------------------------------------+
    | refcount > 0 implies object is valid                      |
    | refcount == 0 triggers deallocation (once!)               |
    +----------------------------------------------------------+
```

**中文解释：**
- 引用计数使用原子操作：多 CPU 并发持有引用
- `dec_and_test` 必须原子以防止双重释放
- 普通 int：丢失更新 → 过早释放或内存泄漏
- 用锁代替：可行但开销高

### Pattern 2: sk_buff Reference Counting

From `include/linux/skbuff.h`:

```c
struct sk_buff {
    ...
    atomic_t    users;      /* Users count */
};

static inline struct sk_buff *skb_get(struct sk_buff *skb)
{
    atomic_inc(&skb->users);
    return skb;
}

/* In net/core/skbuff.c */
void kfree_skb(struct sk_buff *skb)
{
    if (unlikely(!skb))
        return;
    if (likely(atomic_dec_and_test(&skb->users)))
        __kfree_skb(skb);
}
```

```
+------------------------------------------------------------------+
|  PATTERN: NETWORK BUFFER REFCOUNTING                             |
+------------------------------------------------------------------+

    WHY ATOMIC:
    +----------------------------------------------------------+
    | - skb passed between protocol layers, queues, CPUs        |
    | - Multiple consumers may hold reference                   |
    | - Fast path must be lock-free                             |
    +----------------------------------------------------------+
    
    INVARIANT:
    +----------------------------------------------------------+
    | users >= 1 when skb is in active use                      |
    | users == 1 means sole owner, can modify freely            |
    | users > 1 means shared, must clone before modify          |
    +----------------------------------------------------------+
```

**中文解释：**
- skb 在协议层、队列、CPU 之间传递
- 多个消费者可能持有引用
- 快速路径必须无锁
- users > 1 表示共享，修改前必须克隆

### Pattern 3: Lock-Free Statistics Counters

```c
/* Simple statistics - no ordering needed */
struct my_stats {
    atomic_long_t packets_received;
    atomic_long_t bytes_received;
    atomic_long_t errors;
};

/* Fast path - just increment */
void receive_packet(struct sk_buff *skb)
{
    atomic_long_inc(&stats.packets_received);
    atomic_long_add(skb->len, &stats.bytes_received);
    /* No barriers needed - stats are independent */
}
```

```
+------------------------------------------------------------------+
|  PATTERN: STATISTICS COUNTERS                                    |
+------------------------------------------------------------------+

    WHY ATOMIC:
    +----------------------------------------------------------+
    | - Many CPUs update counters concurrently                  |
    | - Must not lose increments                                |
    | - No relationship between different counters              |
    +----------------------------------------------------------+
    
    WHY NO BARRIERS:
    +----------------------------------------------------------+
    | - Counters are independent                                |
    | - Slight staleness is acceptable                          |
    | - Ordering between counters doesn't matter                |
    +----------------------------------------------------------+
    
    WHAT WOULD BREAK:
    +----------------------------------------------------------+
    | Plain int: Lost updates (counter too low)                 |
    | Lock: Massive contention on hot path                      |
    +----------------------------------------------------------+
    
    BETTER ALTERNATIVE (percpu):
    +----------------------------------------------------------+
    | For very hot counters, use per-CPU counters               |
    | Sum on read, no contention on write                       |
    +----------------------------------------------------------+
```

**中文解释：**
- 统计计数器使用原子操作：多 CPU 并发更新
- 不需要屏障：计数器独立，轻微过期可接受
- 更好替代方案：per-CPU 计数器，读时求和

### Pattern 4: Fast-Path Reference Acquisition

```c
/* From security/keys/keyring.c */
if (!atomic_inc_not_zero(&keyring->usage))
    continue;  /* Object being destroyed, skip */

/* From net/sunrpc/xprt.c */
struct rpc_xprt *xprt_get(struct rpc_xprt *xprt)
{
    if (atomic_inc_not_zero(&xprt->count))
        return xprt;
    return NULL;
}
```

```
+------------------------------------------------------------------+
|  PATTERN: atomic_inc_not_zero FOR SAFE LOOKUP                    |
+------------------------------------------------------------------+

    THE PROBLEM:
    +----------------------------------------------------------+
    | Looking up object in list/hash while another CPU          |
    | may be freeing it.                                        |
    |                                                           |
    | Can't do: if (refcount > 0) refcount++;                   |
    | Race between check and increment!                         |
    +----------------------------------------------------------+
    
    THE SOLUTION:
    +----------------------------------------------------------+
    | atomic_inc_not_zero(&obj->refcount):                      |
    | - If count > 0: increment and return TRUE                 |
    | - If count == 0: return FALSE (don't increment)           |
    | - Atomic: no race between check and increment             |
    +----------------------------------------------------------+
    
    USAGE PATTERN:
    +----------------------------------------------------------+
    | rcu_read_lock();                                          |
    | obj = find_in_list(key);   /* RCU-protected lookup */     |
    | if (obj && atomic_inc_not_zero(&obj->refcount)) {         |
    |     /* Got valid reference */                             |
    |     rcu_read_unlock();                                    |
    |     return obj;                                           |
    | }                                                         |
    | rcu_read_unlock();                                        |
    | return NULL;  /* Object being destroyed */                |
    +----------------------------------------------------------+
```

**中文解释：**
- **问题**：在查找列表时，另一个 CPU 可能正在释放对象
- **解决**：`atomic_inc_not_zero` 原子地"检查并增加"
- 如果计数 > 0：增加并返回 TRUE
- 如果计数 == 0：返回 FALSE（正在销毁）
- 常与 RCU 配合使用

### Pattern 5: State Flags with CAS

```c
/* Atomic state transition */
#define STATE_INIT      0
#define STATE_RUNNING   1
#define STATE_STOPPED   2

int start_device(struct device *dev)
{
    /* Only transition INIT → RUNNING */
    if (atomic_cmpxchg(&dev->state, STATE_INIT, STATE_RUNNING) 
        != STATE_INIT) {
        return -EINVAL;  /* Already started or stopped */
    }
    /* Successfully transitioned */
    return 0;
}
```

```
+------------------------------------------------------------------+
|  PATTERN: STATE MACHINE WITH CAS                                 |
+------------------------------------------------------------------+

    WHY CAS:
    +----------------------------------------------------------+
    | - Enforce valid state transitions only                    |
    | - Prevent concurrent state changes                        |
    | - Atomic check-and-update                                 |
    +----------------------------------------------------------+
    
    INVARIANT:
    +----------------------------------------------------------+
    | Only valid transitions allowed:                           |
    | INIT → RUNNING → STOPPED                                  |
    | No other transitions possible                             |
    +----------------------------------------------------------+
```

**中文解释：**
- 使用 CAS 实现状态机转换
- 确保只有有效的状态转换
- 防止并发状态更改

### Pattern 6: IRQ-Safe Data Access

```c
/* Data shared between process context and IRQ */
struct shared_data {
    atomic_t sequence;
    int value1;
    int value2;
};

/* Writer (IRQ disabled or spinlock held) */
void update_data(struct shared_data *data, int v1, int v2)
{
    atomic_inc(&data->sequence);  /* Odd = updating */
    smp_wmb();
    data->value1 = v1;
    data->value2 = v2;
    smp_wmb();
    atomic_inc(&data->sequence);  /* Even = valid */
}

/* Reader (can run anytime) */
int read_data(struct shared_data *data, int *v1, int *v2)
{
    unsigned seq;
    do {
        seq = atomic_read(&data->sequence);
        if (seq & 1) continue;  /* Update in progress */
        smp_rmb();
        *v1 = data->value1;
        *v2 = data->value2;
        smp_rmb();
    } while (atomic_read(&data->sequence) != seq);
    return 0;
}
```

```
+------------------------------------------------------------------+
|  PATTERN: SEQCOUNT (Read-Optimized Synchronization)              |
+------------------------------------------------------------------+

    WHY THIS PATTERN:
    +----------------------------------------------------------+
    | - Reader never blocks                                     |
    | - Writer can update even during reads                     |
    | - Reader detects concurrent modification and retries      |
    +----------------------------------------------------------+
    
    INVARIANTS:
    +----------------------------------------------------------+
    | - Odd sequence = update in progress                       |
    | - Even sequence = data consistent                         |
    | - Sequence change implies data may have changed           |
    +----------------------------------------------------------+
```

**中文解释：**
- seqcount 模式：读优化的同步
- 读者永不阻塞，检测到并发修改时重试
- 奇数序列号 = 更新中，偶数 = 数据一致

### Pattern 7: Scheduler Wakeup Flags

```c
/* Task wakeup uses atomic test-and-set */
static inline int wake_up_state(struct task_struct *p, unsigned int state)
{
    return try_to_wake_up(p, state, 0);
}

/* Check if task needs resched */
static inline void set_tsk_need_resched(struct task_struct *tsk)
{
    set_tsk_thread_flag(tsk, TIF_NEED_RESCHED);
}
```

```
+------------------------------------------------------------------+
|  PATTERN: SCHEDULER FLAGS                                        |
+------------------------------------------------------------------+

    WHY ATOMIC:
    +----------------------------------------------------------+
    | - Multiple CPUs may try to wake same task                 |
    | - Must not lose wakeup                                    |
    | - test-and-set prevents duplicate work                    |
    +----------------------------------------------------------+
```

**中文解释：**
- 调度器标志使用原子操作
- 多 CPU 可能尝试唤醒同一任务
- test-and-set 防止重复工作

### Pattern 8: Atomic Combined with Lock

```c
/* Reference count with lock for complex operations */
struct my_object {
    atomic_t refcount;
    spinlock_t lock;
    struct list_head list;
    /* ... */
};

/* Simple reference: atomic only */
void get_ref(struct my_object *obj)
{
    atomic_inc(&obj->refcount);
}

/* Complex operation: lock required */
void remove_from_list(struct my_object *obj)
{
    spin_lock(&global_lock);
    list_del(&obj->list);
    spin_unlock(&global_lock);
    
    /* Now drop reference */
    if (atomic_dec_and_test(&obj->refcount))
        free_object(obj);
}
```

```
+------------------------------------------------------------------+
|  PATTERN: ATOMIC + LOCK COMBINATION                              |
+------------------------------------------------------------------+

    DESIGN PRINCIPLE:
    +----------------------------------------------------------+
    | - Atomic for SIMPLE, SINGLE-VALUE operations              |
    | - Lock for COMPLEX, MULTI-VALUE operations                |
    | - Combine when needed                                     |
    +----------------------------------------------------------+
    
    RULES:
    +----------------------------------------------------------+
    | 1. Refcount is always atomic (simple inc/dec)             |
    | 2. List operations protected by lock                      |
    | 3. Final free happens outside lock                        |
    +----------------------------------------------------------+
```

**中文解释：**
- 设计原则：简单单值用原子，复杂多值用锁
- 引用计数始终是原子的
- 列表操作受锁保护
- 最终释放在锁外进行

---

## Phase 6 — Atomics vs Locks vs RCU

```
+------------------------------------------------------------------+
|  SYNCHRONIZATION PRIMITIVES COMPARISON                           |
+------------------------------------------------------------------+

    +-------------------+----------+----------+----------+----------+
    | Property          | Atomic   | Spinlock | Mutex    | RCU      |
    +-------------------+----------+----------+----------+----------+
    | Latency           | Lowest   | Low      | Higher   | Lowest*  |
    | (uncontended)     | (~10 ns) | (~20 ns) | (~100 ns)| (reader) |
    +-------------------+----------+----------+----------+----------+
    | Scalability       | High     | Low      | Low      | Highest  |
    | (many CPUs)       | (no lock)| (serial) | (serial) | (readers)|
    +-------------------+----------+----------+----------+----------+
    | Scope             | Single   | Critical | Critical | Read     |
    |                   | value    | section  | section  | section  |
    +-------------------+----------+----------+----------+----------+
    | May sleep?        | No       | No       | Yes      | No*      |
    +-------------------+----------+----------+----------+----------+
    | Protects          | One var  | Any code | Any code | Data     |
    |                   |          |          |          | only     |
    +-------------------+----------+----------+----------+----------+
    | Correctness       | Hard     | Easy     | Easy     | Medium   |
    | difficulty        |          |          |          |          |
    +-------------------+----------+----------+----------+----------+

    * RCU readers are essentially free (just preempt_disable on some configs)
    * RCU read sections must not sleep
```

```
+------------------------------------------------------------------+
|  DECISION TABLE: WHEN TO USE WHAT                                |
+------------------------------------------------------------------+

    USE ATOMIC WHEN:
    +----------------------------------------------------------+
    | ✓ Single value manipulation                               |
    | ✓ Reference counting                                      |
    | ✓ Statistics counters                                     |
    | ✓ Simple flags                                            |
    | ✓ Performance critical and lock contention is high        |
    +----------------------------------------------------------+
    
    USE SPINLOCK WHEN:
    +----------------------------------------------------------+
    | ✓ Critical section is short                               |
    | ✓ Cannot sleep (IRQ context)                              |
    | ✓ Multiple values must be consistent                      |
    | ✓ Correctness more important than peak performance        |
    +----------------------------------------------------------+
    
    USE MUTEX WHEN:
    +----------------------------------------------------------+
    | ✓ Critical section may take long time                     |
    | ✓ May need to sleep (allocate, I/O)                       |
    | ✓ Only process context                                    |
    +----------------------------------------------------------+
    
    USE RCU WHEN:
    +----------------------------------------------------------+
    | ✓ Read-mostly workload                                    |
    | ✓ Readers must not block                                  |
    | ✓ Updates can be delayed                                  |
    | ✓ Pointer-based data structures                           |
    +----------------------------------------------------------+
```

**中文解释：**
| 属性 | 原子 | 自旋锁 | 互斥锁 | RCU |
|------|------|--------|--------|-----|
| 延迟 | 最低 | 低 | 较高 | 最低(读) |
| 可扩展性 | 高 | 低 | 低 | 最高 |
| 范围 | 单值 | 临界区 | 临界区 | 读区间 |
| 可睡眠 | 否 | 否 | 是 | 否 |
| 正确性难度 | 难 | 易 | 易 | 中 |

---

## Phase 7 — Common Atomic Bugs

### Bug 1: Atomic Without Memory Ordering

```c
/* BUG: Missing barrier */
void producer(void)
{
    buffer[idx] = data;      /* Store data */
    atomic_inc(&count);       /* "Publish" */
    /* Problem: data may not be visible when count is! */
}

void consumer(void)
{
    if (atomic_read(&count) > 0) {
        use(buffer[idx]);    /* May see stale data! */
        atomic_dec(&count);
    }
}
```

```
+------------------------------------------------------------------+
|  BUG: MISSING BARRIERS                                           |
+------------------------------------------------------------------+

    FIX:
    +----------------------------------------------------------+
    | void producer(void) {                                     |
    |     buffer[idx] = data;                                   |
    |     smp_wmb();              /* Barrier before publish */  |
    |     atomic_inc(&count);                                   |
    | }                                                         |
    |                                                           |
    | void consumer(void) {                                     |
    |     if (atomic_read(&count) > 0) {                        |
    |         smp_rmb();          /* Barrier after check */     |
    |         use(buffer[idx]);                                 |
    |         atomic_dec(&count);                               |
    |     }                                                     |
    | }                                                         |
    +----------------------------------------------------------+
```

**中文解释：**
- 问题：原子操作不保证与其他内存操作的顺序
- 修复：在发布前加写屏障，在检查后加读屏障

### Bug 2: Atomic Used for Compound Invariants

```c
/* BUG: Two atomics don't form a transaction */
struct account {
    atomic_t balance;
    atomic_t pending;
};

void transfer(struct account *from, struct account *to, int amount)
{
    atomic_sub(amount, &from->balance);
    /* Crash here → money disappeared! */
    atomic_add(amount, &to->balance);
}
```

```
+------------------------------------------------------------------+
|  BUG: COMPOUND INVARIANTS                                        |
+------------------------------------------------------------------+

    PROBLEM:
    +----------------------------------------------------------+
    | - Invariant: total money must be conserved                |
    | - Two separate atomics don't protect this                 |
    | - Failure between them violates invariant                 |
    +----------------------------------------------------------+
    
    FIX: USE LOCK
    +----------------------------------------------------------+
    | void transfer(struct account *from, struct account *to,   |
    |               int amount) {                               |
    |     spin_lock(&bank_lock);                                |
    |     from->balance -= amount;                              |
    |     to->balance += amount;                                |
    |     spin_unlock(&bank_lock);                              |
    | }                                                         |
    +----------------------------------------------------------+
```

**中文解释：**
- 问题：两个原子操作不构成事务
- 不变量：总金额必须守恒
- 两者之间的失败违反不变量
- 修复：使用锁保护整个操作

### Bug 3: ABA Problem

```c
/* BUG: ABA problem in lock-free stack */
struct node *pop(void)
{
    struct node *old_head, *new_head;
    do {
        old_head = head;
        if (!old_head) return NULL;
        new_head = old_head->next;
    } while (cmpxchg(&head, old_head, new_head) != old_head);
    return old_head;
}

/* Problem: head could be A → B → C
 * Thread 1: reads head=A, next=B, preempted
 * Thread 2: pops A, pops B, pushes A back
 * Thread 1: cmpxchg sees head=A, succeeds!
 * But now head→next is wrong! */
```

```
+------------------------------------------------------------------+
|  BUG: ABA PROBLEM                                                |
+------------------------------------------------------------------+

    TIMELINE:
    +----------------------------------------------------------+
    | Time | Thread 1           | Thread 2        | head       |
    |------|--------------------|-----------------|------------|
    |  1   | read head=A, B     |                 | A→B→C      |
    |  2   | (preempted)        | pop A           | B→C        |
    |  3   |                    | pop B           | C          |
    |  4   |                    | push A (reused) | A→C        |
    |  5   | cmpxchg A→B        |                 | B (!wrong!)|
    +----------------------------------------------------------+
    
    FIX:
    +----------------------------------------------------------+
    | 1. Use generation counter with pointer                    |
    | 2. Use hazard pointers                                    |
    | 3. Use RCU (common in kernel)                             |
    +----------------------------------------------------------+
```

**中文解释：**
- ABA 问题：值从 A 变成 B 再变回 A
- cmpxchg 看到 A 就成功，但内部状态已改变
- 修复：使用代数计数器、hazard pointers 或 RCU

### Bug 4: Refcount Overflow/Underflow

```c
/* BUG: Overflow wraps to 0, triggering early free */
void get_many(struct object *obj)
{
    for (int i = 0; i < UINT_MAX; i++) {
        atomic_inc(&obj->refcount);
        /* Eventually wraps to 0! */
    }
}

/* BUG: Double-decrement → negative → underflow */
void double_put(struct object *obj)
{
    put(obj);
    put(obj);  /* refcount goes negative, then huge positive! */
}
```

```
+------------------------------------------------------------------+
|  BUG: REFCOUNT OVERFLOW/UNDERFLOW                                |
+------------------------------------------------------------------+

    KERNEL DEFENSE (modern kernels have refcount_t):
    +----------------------------------------------------------+
    | - WARN_ON if incrementing from 0                          |
    | - WARN_ON if decrementing to negative                     |
    | - Saturating arithmetic (stop at max, don't wrap)         |
    +----------------------------------------------------------+
    
    kref_get():
    +----------------------------------------------------------+
    | void kref_get(struct kref *kref)                          |
    | {                                                         |
    |     WARN_ON(!atomic_read(&kref->refcount));  /* Catch! */ |
    |     atomic_inc(&kref->refcount);                          |
    |     smp_mb__after_atomic_inc();                           |
    | }                                                         |
    +----------------------------------------------------------+
```

**中文解释：**
- 溢出：增加太多次，回绕到 0，触发提前释放
- 下溢：双重减少，变负，然后变成巨大正数
- 内核防御：从 0 增加时警告，减到负数时警告，饱和算术

### Bug 5: Mixing Atomic and Non-Atomic Access

```c
/* BUG: Mixed access */
void writer_atomic(struct data *d)
{
    atomic_set(&d->value, 42);
}

void writer_plain(struct data *d)
{
    d->value.counter = 42;  /* Plain access! */
}

/* Data race: compiler may optimize plain access */
/* No guarantee of atomicity */
```

```
+------------------------------------------------------------------+
|  BUG: MIXED ATOMIC/NON-ATOMIC ACCESS                             |
+------------------------------------------------------------------+

    PROBLEM:
    +----------------------------------------------------------+
    | - Compiler doesn't know about concurrency intent          |
    | - May cache, reorder, or optimize away plain access       |
    | - Undefined behavior in C11 memory model                  |
    +----------------------------------------------------------+
    
    RULE:
    +----------------------------------------------------------+
    | If ANY access is atomic, ALL accesses must be atomic      |
    | (or protected by a lock)                                  |
    +----------------------------------------------------------+
```

**中文解释：**
- 问题：混合原子和非原子访问
- 编译器不知道并发意图，可能缓存、重排序或优化
- 规则：如果有任何原子访问，所有访问都必须是原子的

---

## Phase 8 — User-Space Transfer

### C11 Atomics vs Kernel Atomics

```c
/* Kernel style */
atomic_t count = ATOMIC_INIT(0);
atomic_inc(&count);
int val = atomic_read(&count);

/* C11 style */
#include <stdatomic.h>
atomic_int count = ATOMIC_VAR_INIT(0);
atomic_fetch_add(&count, 1);
int val = atomic_load(&count);
```

```
+------------------------------------------------------------------+
|  C11 ATOMICS MAPPING                                             |
+------------------------------------------------------------------+

    +------------------------+----------------------------------+
    | Kernel                 | C11                              |
    +------------------------+----------------------------------+
    | atomic_t               | atomic_int                       |
    | atomic64_t             | atomic_llong                     |
    | atomic_read(v)         | atomic_load(v)                   |
    | atomic_set(v, i)       | atomic_store(v, i)               |
    | atomic_inc(v)          | atomic_fetch_add(v, 1)           |
    | atomic_dec(v)          | atomic_fetch_sub(v, 1)           |
    | atomic_cmpxchg(v,o,n)  | atomic_compare_exchange_*(v,&o,n)|
    | smp_mb()               | atomic_thread_fence(memory_order_seq_cst) |
    | smp_wmb()              | atomic_thread_fence(memory_order_release) |
    | smp_rmb()              | atomic_thread_fence(memory_order_acquire) |
    +------------------------+----------------------------------+
    
    C11 MEMORY ORDERS:
    +----------------------------------------------------------+
    | memory_order_relaxed  - No ordering (like atomic_set)     |
    | memory_order_acquire  - Like smp_rmb (no loads before)    |
    | memory_order_release  - Like smp_wmb (no stores after)    |
    | memory_order_acq_rel  - Both acquire and release          |
    | memory_order_seq_cst  - Full sequential consistency       |
    +----------------------------------------------------------+
```

**中文解释：**
- C11 原子操作与内核原子操作的对应关系
- C11 内存顺序：relaxed（无序）、acquire（获取）、release（释放）、seq_cst（顺序一致）

### User-Space Example: Reference-Counted Object

```c
/* refcounted_object.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <pthread.h>

struct refcounted_object {
    atomic_int refcount;
    char *data;
};

struct refcounted_object *object_create(const char *data)
{
    struct refcounted_object *obj = malloc(sizeof(*obj));
    if (!obj) return NULL;
    
    obj->data = strdup(data);
    atomic_store(&obj->refcount, 1);  /* Initial reference */
    return obj;
}

void object_get(struct refcounted_object *obj)
{
    /* Must only call when refcount > 0 */
    int old = atomic_fetch_add(&obj->refcount, 1);
    if (old == 0) {
        fprintf(stderr, "BUG: get on dead object!\n");
        abort();
    }
}

void object_put(struct refcounted_object *obj)
{
    int old = atomic_fetch_sub(&obj->refcount, 1);
    if (old == 1) {
        /* Was 1, now 0 - we're the last reference */
        free(obj->data);
        free(obj);
    } else if (old <= 0) {
        fprintf(stderr, "BUG: put on already-freed object!\n");
        abort();
    }
}

/* Thread-safe lookup with atomic_compare_exchange */
int object_try_get(struct refcounted_object *obj)
{
    int old = atomic_load(&obj->refcount);
    do {
        if (old == 0) return 0;  /* Object is dead */
    } while (!atomic_compare_exchange_weak(&obj->refcount, &old, old + 1));
    return 1;  /* Successfully got reference */
}
```

### User-Space Example: Lock-Free Statistics

```c
/* lockfree_stats.c */

#include <stdatomic.h>

struct stats {
    atomic_ullong requests;
    atomic_ullong bytes_in;
    atomic_ullong bytes_out;
    atomic_ullong errors;
};

static struct stats global_stats = {0};

/* Called from multiple threads - no locks needed */
void record_request(size_t bytes_in, size_t bytes_out)
{
    atomic_fetch_add_explicit(&global_stats.requests, 1, 
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&global_stats.bytes_in, bytes_in,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&global_stats.bytes_out, bytes_out,
                              memory_order_relaxed);
}

void record_error(void)
{
    atomic_fetch_add_explicit(&global_stats.errors, 1,
                              memory_order_relaxed);
}

/* Reading stats - relaxed is fine, slight staleness OK */
void print_stats(void)
{
    printf("Requests: %llu\n", 
           atomic_load_explicit(&global_stats.requests, memory_order_relaxed));
    printf("Bytes in: %llu\n",
           atomic_load_explicit(&global_stats.bytes_in, memory_order_relaxed));
    printf("Bytes out: %llu\n",
           atomic_load_explicit(&global_stats.bytes_out, memory_order_relaxed));
    printf("Errors: %llu\n",
           atomic_load_explicit(&global_stats.errors, memory_order_relaxed));
}
```

### User-Space Example: State Flag with CAS

```c
/* state_machine.c */

#include <stdatomic.h>

typedef enum {
    STATE_INIT,
    STATE_STARTING,
    STATE_RUNNING,
    STATE_STOPPING,
    STATE_STOPPED
} service_state_t;

struct service {
    atomic_int state;
    /* ... other fields ... */
};

int service_start(struct service *svc)
{
    int expected = STATE_INIT;
    
    /* Atomically: if state == INIT, set to STARTING */
    if (!atomic_compare_exchange_strong(&svc->state, &expected, STATE_STARTING)) {
        /* Failed - state was not INIT */
        return -1;
    }
    
    /* Do actual startup... */
    
    /* Transition STARTING → RUNNING */
    atomic_store(&svc->state, STATE_RUNNING);
    return 0;
}

int service_stop(struct service *svc)
{
    int expected = STATE_RUNNING;
    
    if (!atomic_compare_exchange_strong(&svc->state, &expected, STATE_STOPPING)) {
        return -1;
    }
    
    /* Do actual shutdown... */
    
    atomic_store(&svc->state, STATE_STOPPED);
    return 0;
}
```

**中文解释：**
- 用户态示例：
  1. 引用计数对象：使用 C11 原子操作
  2. 无锁统计计数器：使用 `memory_order_relaxed`
  3. CAS 状态机：使用 `atomic_compare_exchange_strong`

---

## Phase 9 — Design Rules & Checklist

```
+------------------------------------------------------------------+
|  DESIGN RULES FOR SAFE ATOMIC USAGE                              |
+------------------------------------------------------------------+

    RULE 1: START WITH LOCKS
    +----------------------------------------------------------+
    | - Locks are easier to reason about                        |
    | - Only use atomics when locks are too slow                |
    | - Profile before optimizing                               |
    +----------------------------------------------------------+
    
    RULE 2: SINGLE VALUE ONLY
    +----------------------------------------------------------+
    | - Atomics protect ONE value, not relationships            |
    | - If invariant involves multiple values → use lock        |
    +----------------------------------------------------------+
    
    RULE 3: KNOW YOUR BARRIERS
    +----------------------------------------------------------+
    | - Atomic ops have NO ordering by default                  |
    | - Add explicit barriers when ordering matters             |
    | - Pair write barriers with read barriers                  |
    +----------------------------------------------------------+
    
    RULE 4: DOCUMENT CONCURRENCY
    +----------------------------------------------------------+
    | - Comment WHY atomic is used                              |
    | - Document ordering requirements                          |
    | - Explain the invariant being protected                   |
    +----------------------------------------------------------+
    
    RULE 5: TEST THOROUGHLY
    +----------------------------------------------------------+
    | - Race conditions are hard to reproduce                   |
    | - Use stress testing and TSan/helgrind                    |
    | - Model checking for complex algorithms                   |
    +----------------------------------------------------------+
```

```
+------------------------------------------------------------------+
|  WARNING SIGNS OF ATOMIC MISUSE                                  |
+------------------------------------------------------------------+

    🚩 Multiple atomics without locks
    +----------------------------------------------------------+
    | If you're doing: if (atomic_read(&a) && atomic_read(&b))  |
    | You probably need a lock.                                 |
    +----------------------------------------------------------+
    
    🚩 Check-then-act without CAS
    +----------------------------------------------------------+
    | if (atomic_read(&v) > 0)  /* Check */                     |
    |     atomic_dec(&v);        /* Act */                      |
    | This is a RACE. Use atomic_dec_if_positive() or CAS loop. |
    +----------------------------------------------------------+
    
    🚩 No barriers around publish/consume
    +----------------------------------------------------------+
    | If you're publishing a pointer for others to read,        |
    | you need write barrier before publish, read barrier after.|
    +----------------------------------------------------------+
    
    🚩 Refcount from unknown state
    +----------------------------------------------------------+
    | object_get(obj);  /* But is obj valid? */                 |
    | Need atomic_inc_not_zero or RCU protection.               |
    +----------------------------------------------------------+
```

```
+------------------------------------------------------------------+
|  ATOMIC USAGE CHECKLIST                                          |
+------------------------------------------------------------------+

    Before using atomics, answer:
    
    □ Is this a single-value invariant?
      → If no, use lock
    
    □ Do I need ordering with other memory operations?
      → If yes, add appropriate barriers
    
    □ Is there a check-then-act pattern?
      → If yes, use CAS or atomic_*_unless variants
    
    □ Am I acquiring a reference to possibly-dead object?
      → If yes, use atomic_inc_not_zero with RCU
    
    □ Is this on a hot path with high contention?
      → If no, just use a lock
    
    □ Have I documented the concurrency contract?
      → If no, add comments now
    
    □ Can I test this with TSan/stress tests?
      → If no, reconsider the design
```

**中文总结：**

设计规则：
1. **从锁开始**：锁更容易推理，只有锁太慢时才用原子
2. **仅单值**：原子只保护一个值，多值不变量用锁
3. **了解屏障**：默认无顺序，需要时显式添加屏障
4. **文档化并发**：注释为何用原子、顺序要求、不变量
5. **充分测试**：竞争条件难以重现，用压力测试和工具

警告信号：
- 🚩 多个原子无锁 → 可能需要锁
- 🚩 检查后操作无 CAS → 竞争条件
- 🚩 发布/消费无屏障 → 可见性问题
- 🚩 从未知状态获取引用 → 需要 atomic_inc_not_zero

检查清单：
- 单值不变量？多值用锁
- 需要内存顺序？添加屏障
- 检查后操作？用 CAS
- 获取可能死亡对象引用？用 atomic_inc_not_zero + RCU
- 热路径高竞争？不是就用锁
- 已文档化？添加注释
- 可测试？用 TSan/压力测试

