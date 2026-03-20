# WHAT | pthread Building Blocks - Part 2: Synchronization Primitives

## Mutexes: What They Actually Protect

```
+------------------------------------------------------------------+
|                    MUTEX MENTAL MODEL                            |
+------------------------------------------------------------------+
|                                                                  |
|   COMMON MISCONCEPTION:                                          |
|   "A mutex protects CODE"                                        |
|                                                                  |
|   CORRECT UNDERSTANDING:                                         |
|   "A mutex protects DATA by serializing access to it"            |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |                    DATA INVARIANT                          | |
|   |                         |                                  | |
|   |            +------------+------------+                     | |
|   |            |                         |                     | |
|   |            v                         v                     | |
|   |   +----------------+        +----------------+             | |
|   |   |  account.bal   |        | account.hist   |             | |
|   |   |     = $100     |        |  [deposit $50] |             | |
|   |   +----------------+        +----------------+             | |
|   |                                                            | |
|   |   INVARIANT: balance == sum(history)                       | |
|   |                                                            | |
|   |   WITHOUT MUTEX:                                           | |
|   |   Thread A: balance += 50;   Thread B: balance += 30;      | |
|   |   Thread A: history.add(50); Thread B: history.add(30);    | |
|   |                                                            | |
|   |   Possible interleaving:                                   | |
|   |   A: balance = 100 + 50 = 150                              | |
|   |   B: balance = 100 + 30 = 130  (A's write lost!)           | |
|   |   A: history.add(50)                                       | |
|   |   B: history.add(30)                                       | |
|   |   Final: balance=130, history=[50,30] --> INVARIANT BROKEN | |
|   |                                                            | |
|   |   WITH MUTEX:                                              | |
|   |   lock(mtx);                                               | |
|   |   balance += amount;                                       | |
|   |   history.add(amount);                                     | |
|   |   unlock(mtx);                                             | |
|   |   --> Invariant always maintained                          | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
互斥锁保护的是**数据**，不是代码。更准确地说，互斥锁保护的是**数据不变量（invariant）**。在上面的例子中，不变量是「余额等于历史交易的总和」。没有互斥锁，两个线程的交叉执行可能破坏这个不变量。使用互斥锁后，每次修改都是原子的，不变量始终成立。

### `pthread_mutex_lock`

`pthread_mutex_lock`（互斥锁）的核心作用是通过 "互斥性" 实现临界区（critical section）的独占访问 —— 也就是让类似 counter++ 这段代码同一时间只能被一个线程执行

1. 核心逻辑：锁的 "状态独占" + 内核的调度干预
    - 互斥锁本身有一个原子状态（锁定 / 未锁定）
        - 当线程 A 调用 pthread_mutex_lock 时，若锁是 "未锁定" 状态，会原子性地将锁标记为 "锁定"，线程 A 直接进入临界区修改 counter
        - 若锁已被线程 B 锁定，线程 A 会被操作系统内核挂起（从 CPU 运行队列移除），不再参与 CPU 调度，直到线程 B 调用 pthread_mutex_unlock 释放锁
        - 锁释放后，内核会唤醒等待该锁的线程（如 A），让其中一个线程获得锁并执行临界区代码

2. 原子性保证：锁的 "加锁 / 解锁" 操作本身是原子的
    - 硬件是锁实现的底层基础—— 操作系统层面的互斥锁，最终必须依赖 CPU 提供的原子指令（不可被中断的硬件指令）才能实现
        1. 硬件原子指令：锁的 "最小依赖"
            CPU 提供了专门的原子指令，用于实现 "检查并设置""交换" 等不可打断的操作，常见的有：
            - test-and-set（测试并设置）：原子性地检查内存地址的值，并设置新值；
            - compare-and-swap (CAS)（比较并交换）：原子性地比较内存值和预期值，相等则替换；
            - fetch-and-add（取并加）：原子性地读取内存值并加 1
                ```c
                // 伪代码：pthread_mutex_lock 的底层原子操作核心
                int test_and_set(int *lock) {
                    // 硬件保证：这一步是原子的，不会被中断/其他CPU核心打断
                    int old_value = *lock;  // 读取锁的当前状态
                    *lock = 1;              // 将锁设为锁定状态
                    return old_value;       // 返回旧状态
                }

                // 加锁逻辑
                void mutex_lock(int *lock) {
                    // 循环直到成功获取锁（自旋锁简化版，pthread_mutex是更复杂的混合锁）
                    while (test_and_set(lock) == 1) {
                        // 锁已被占用，等待（内核会根据锁类型决定自旋/挂起）
                    }
                }
                ```
        2. 多核 CPU 的额外硬件支撑：缓存一致性协议(如 MESI)
            - 每个 CPU 核心有自己的缓存，counter 和锁的状态可能被缓存到不同核心；
            - 缓存一致性协议保证：当一个核心修改了锁的状态（如标记为锁定），其他核心能立即看到这个修改，避免 "核心 A 认为锁已释放，核心 B 还认为锁是锁定的" 这种不一致问题；
            - 没有这个硬件协议，多核场景下的锁会完全失效

3. pthread_mutex 的分层实现（从硬件到应用层）
    ```
    应用层：pthread_mutex_lock/unlock（你调用的API）
    ↓
    内核层：Linux内核的futex（快速用户态互斥体）机制
            - 首次加锁：用户态通过原子指令尝试加锁，避免内核态切换；
            - 锁被占用：调用内核态syscall，挂起当前线程，释放CPU；
            - 解锁时：内核唤醒等待线程，重新竞争锁。
    ↓
    硬件层：CPU原子指令（test-and-set/CAS） + 缓存一致性协议（MESI）
    ```

### CPU 的原子操作
原子操作的关键是：一个操作从开始到结束，中间不会被任何其他CPU核心/线程中断，也不会被拆分执行

- 硬件支持原子操作的核心机制
    1. 总线锁（Bus Lock）：最基础的"独占总线"方式
        - **原理**：CPU执行原子指令（如`test-and-set`）时，向CPU总线发送`LOCK#`信号，锁定整个系统总线；
        - **效果**：`LOCK#`有效期间，其他CPU核心无法通过总线访问内存/缓存，当前CPU的原子操作不会被打断；
        - **举例**：执行`lock addl %eax, (%%ebx)`（x86带lock前缀的加法指令）时，CPU先锁总线，完成"读内存→加值→写回"后释放总线；
        - **优缺点**：实现简单、原子性绝对可靠，但锁总线会阻塞所有核心的内存访问，多核性能极差，仅作为现代CPU的兜底方案。

    2. 缓存锁（Cache Lock）：现代CPU的主流方式
        为解决总线锁性能问题，现代CPU（x86/ARMv8+）基于缓存和缓存一致性协议实现"只锁缓存行，不锁总线"：
        - **核心前提**：目标内存数据需在CPU核心缓存中（且为MESI协议的Exclusive/Modified独占态）；
        - **执行逻辑**：
        1. 执行原子指令时，检查目标数据的缓存行状态，若为独占态则直接在缓存行执行"读-改-写"；
        2. 通过缓存一致性协议向其他核心发送"无效化请求"，让同地址缓存行失效；
        3. 操作完成前，其他核心访问该地址需等待当前核心释放缓存行；
        - **效果**：仅锁定目标数据所在的缓存行（通常64字节），性能远优于总线锁；
        - **限制**：若目标数据不在缓存/跨缓存行，CPU自动降级为总线锁。

    3. 专用原子指令集：硬件直接封装原子操作
        现代CPU提供专用原子指令，由硬件电路实现"不可打断"，无需手动加锁，不同架构指令如下：

        | 架构   | 典型原子指令                | 功能说明                     |
        |--------|-----------------------------|------------------------------|
        | x86/x64 | `XCHG`（交换）| 原子交换内存和寄存器的值     |
        | x86/x64 | `CMPXCHG`（CAS）| 比较并交换（原子）|
        | x86/x64 | `LOCK XADD`（fetch-and-add）| 原子读取并加值               |
        | ARMv8  | `LDXR/STXR`（加载/存储独占）| 组合实现CAS等原子操作        |
        | RISC-V | `amoadd.w`（原子加）| 原子加法操作                 |

    **示例（x86的CMPXCHG指令）**：
    ```asm
    # 伪汇编：实现 CAS(addr, old_val, new_val) 原子操作
    CMPXCHG [addr], %eax
    # 硬件原子执行逻辑：
    # 1. 比较 [addr]（内存值）和 %ecx（预期旧值）；
    # 2. 相等则将 %eax（新值）写入 [addr]，设置标志位；
    # 3. 不等则将 [addr] 实际值加载到 %ecx；
    # 4. 全程不可被中断，其他核心无法修改 [addr]。
    ```
---

## Mutex Lifecycle and Ownership

```
+------------------------------------------------------------------+
|                    MUTEX LIFECYCLE                               |
+------------------------------------------------------------------+
|                                                                  |
|   INITIALIZATION:                                                |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Static initialization (preferred for global mutexes)  | |
|   |   pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;         | |
|   |                                                            | |
|   |   // Dynamic initialization (for heap-allocated mutexes)   | |
|   |   pthread_mutex_t *mtx = malloc(sizeof(pthread_mutex_t));  | |
|   |   pthread_mutex_init(mtx, NULL);  // NULL = default attr   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   USAGE:                                                         |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread_mutex_lock(&mtx);    // blocks until acquired    | |
|   |   // ... critical section (access protected data) ...      | |
|   |   pthread_mutex_unlock(&mtx);  // release                  | |
|   |                                                            | |
|   |   // Non-blocking variant                                  | |
|   |   if (pthread_mutex_trylock(&mtx) == 0) {                  | |
|   |       // acquired                                          | |
|   |       pthread_mutex_unlock(&mtx);                          | |
|   |   } else {                                                 | |
|   |       // would block, do something else                    | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   DESTRUCTION:                                                   |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Only for dynamically initialized mutexes              | |
|   |   pthread_mutex_destroy(&mtx);                             | |
|   |   free(mtx);                                               | |
|   |                                                            | |
|   |   RULES:                                                   | |
|   |   - Must not destroy while locked                          | |
|   |   - Must not destroy while threads waiting                 | |
|   |   - Static mutexes need not be destroyed                   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   memcached example (crawler.c):                                 |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Static initialization at file scope                   | |
|   |   static pthread_mutex_t lru_crawler_lock =                | |
|   |       PTHREAD_MUTEX_INITIALIZER;                           | |
|   |                                                            | |
|   |   // Usage pattern in start_item_crawler_thread            | |
|   |   pthread_mutex_lock(&lru_crawler_lock);                   | |
|   |   do_run_lru_crawler_thread = 1;                           | |
|   |   pthread_create(...);                                     | |
|   |   pthread_cond_wait(&lru_crawler_cond, &lru_crawler_lock); | |
|   |   pthread_mutex_unlock(&lru_crawler_lock);                 | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
互斥锁的生命周期：

1. **初始化**：静态初始化用 PTHREAD_MUTEX_INITIALIZER（适用于全局/静态互斥锁），动态初始化用 pthread_mutex_init（适用于堆分配的互斥锁）。

2. **使用**：lock() 阻塞直到获取锁，unlock() 释放锁。trylock() 是非阻塞版本，获取失败返回 EBUSY。

3. **销毁**：动态初始化的互斥锁需要调用 pthread_mutex_destroy()。销毁前必须确保锁未被持有且无线程等待。memcached 使用静态初始化，这是最简单安全的方式。

---

## Normal vs Recursive Mutexes

```
+------------------------------------------------------------------+
|                    MUTEX TYPES                                   |
+------------------------------------------------------------------+
|                                                                  |
|   PTHREAD_MUTEX_NORMAL (default):                                |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread A:                                                | |
|   |   lock(mtx);       // acquired                             | |
|   |   lock(mtx);       // DEADLOCK! (or undefined behavior)    | |
|   |                                                            | |
|   |   - Double-lock by same thread = UNDEFINED                 | |
|   |   - Unlock by non-owner = UNDEFINED                        | |
|   |   - Fast (no ownership tracking)                           | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   PTHREAD_MUTEX_RECURSIVE:                                       |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread A:                                                | |
|   |   lock(mtx);       // acquired, count=1                    | |
|   |   lock(mtx);       // allowed, count=2                     | |
|   |   lock(mtx);       // allowed, count=3                     | |
|   |   unlock(mtx);     // count=2                              | |
|   |   unlock(mtx);     // count=1                              | |
|   |   unlock(mtx);     // count=0, released                    | |
|   |                                                            | |
|   |   Setup:                                                   | |
|   |   pthread_mutexattr_t attr;                                | |
|   |   pthread_mutexattr_init(&attr);                           | |
|   |   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);|
|   |   pthread_mutex_init(&mtx, &attr);                         | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   PTHREAD_MUTEX_ERRORCHECK:                                      |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread A:                                                | |
|   |   lock(mtx);       // acquired                             | |
|   |   lock(mtx);       // returns EDEADLK (error, not block)   | |
|   |                                                            | |
|   |   Thread B:                                                | |
|   |   unlock(mtx);     // returns EPERM (not owner)            | |
|   |                                                            | |
|   |   Useful for debugging, slower than NORMAL                 | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   RECOMMENDATION:                                                |
|   +------------------------------------------------------------+ |
|   | - Default (NORMAL): Use in most cases                      | |
|   | - RECURSIVE: Only when unavoidable (legacy code, callbacks)| |
|   |   Often indicates design problem                           | |
|   | - ERRORCHECK: Use during development/debugging             | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
互斥锁有三种类型：

1. **NORMAL（默认）**：同一线程重复加锁导致死锁或未定义行为，非所有者解锁也是未定义行为。最快，因为不跟踪所有权。

2. **RECURSIVE**：允许同一线程多次加锁，维护计数器，必须解锁相同次数。适用于回调函数或遗留代码。但通常表明设计有问题——如果代码需要递归锁，可能应该重构。

3. **ERRORCHECK**：同线程重复加锁返回 EDEADLK 错误，非所有者解锁返回 EPERM。适用于调试阶段。

建议：默认使用 NORMAL，开发时考虑 ERRORCHECK，尽量避免 RECURSIVE。

---

## Condition Variables: The Wait/Signal Pattern

```
+------------------------------------------------------------------+
|                    CONDITION VARIABLE MODEL                      |
+------------------------------------------------------------------+
|                                                                  |
|   PURPOSE: Waiting for a state change in shared data             |
|                                                                  |
|   WITHOUT CONDITION VARIABLE (busy waiting):                     |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   while (1) {                                              | |
|   |       pthread_mutex_lock(&mtx);                            | |
|   |       if (condition_is_true()) {                           | |
|   |           break;  // got it!                               | |
|   |       }                                                    | |
|   |       pthread_mutex_unlock(&mtx);                          | |
|   |       usleep(1000);  // waste CPU, add latency             | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   WITH CONDITION VARIABLE (efficient blocking):                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread_mutex_lock(&mtx);                                | |
|   |   while (!condition_is_true()) {   // MUST be while loop! | |
|   |       pthread_cond_wait(&cond, &mtx);  // releases mtx     | |
|   |   }                                                        | |
|   |   // condition is true, mtx is held                        | |
|   |   pthread_mutex_unlock(&mtx);                              | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   THE THREE-PART PATTERN:                                        |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   1. PREDICATE (boolean condition on shared state)         | |
|   |      Examples: queue_not_empty, work_available, done       | |
|   |                                                            | |
|   |   2. MUTEX (protects the predicate's data)                 | |
|   |      Must hold mutex when checking and when waiting        | |
|   |                                                            | |
|   |   3. CONDITION VARIABLE (notification mechanism)           | |
|   |      Provides efficient blocking/waking                    | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   memcached crawler example:                                     |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Predicate: crawler_count > 0 (work available)            | |
|   |              or do_run_lru_crawler_thread == 0 (shutdown)  | |
|   |   Mutex: lru_crawler_lock                                  | |
|   |   Condvar: lru_crawler_cond                                | |
|   |                                                            | |
|   |   // In item_crawler_thread (line 603):                    | |
|   |   while (do_run_lru_crawler_thread) {                      | |
|   |       pthread_cond_wait(&lru_crawler_cond, &lru_crawler_lock);|
|   |       // process work when signaled                        | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
条件变量用于高效地等待共享数据状态的变化。没有条件变量时，必须使用忙等待（busy waiting），浪费 CPU 且增加延迟。条件变量允许线程在条件不满足时阻塞（释放 CPU），条件满足时被唤醒。

三要素模式：
1. **谓词（Predicate）**：基于共享状态的布尔条件
2. **互斥锁（Mutex）**：保护谓词依赖的数据
3. **条件变量（Condvar）**：提供阻塞/唤醒机制

memcached 的 crawler 使用这个模式：谓词是「有工作要做或要关闭」，互斥锁是 lru_crawler_lock，条件变量是 lru_crawler_cond。

---

## Spurious Wakeups

```
+------------------------------------------------------------------+
|                    SPURIOUS WAKEUPS                              |
+------------------------------------------------------------------+
|                                                                  |
|   PROBLEM: cond_wait() can return WITHOUT being signaled!        |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread A (waiting)        Thread B (signaler)            | |
|   |   +----------------+        +----------------+             | |
|   |   | cond_wait()    |        |                |             | |
|   |   |    ...         |        |                |             | |
|   |   |  [wakes up?!]  |        | (did nothing!) |             | |
|   |   +----------------+        +----------------+             | |
|   |                                                            | |
|   |   Causes:                                                  | |
|   |   - Kernel implementation details                          | |
|   |   - Signal interrupts                                      | |
|   |   - Multi-processor race conditions                        | |
|   |   - cond_broadcast waking multiple when only one needed    | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   WHY WE NEED while() NOT if():                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   WRONG (will break on spurious wakeup):                   | |
|   |   pthread_mutex_lock(&mtx);                                | |
|   |   if (!ready) {             // <-- BUG: should be while    | |
|   |       pthread_cond_wait(&cond, &mtx);                      | |
|   |   }                                                        | |
|   |   process_data();  // might run with ready still false!    | |
|   |                                                            | |
|   |   CORRECT:                                                 | |
|   |   pthread_mutex_lock(&mtx);                                | |
|   |   while (!ready) {          // re-check after every wake   | |
|   |       pthread_cond_wait(&cond, &mtx);                      | |
|   |   }                                                        | |
|   |   process_data();  // guaranteed ready == true             | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   SIGNAL vs BROADCAST:                                           |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread_cond_signal(&cond):                              | |
|   |   - Wakes AT LEAST ONE waiting thread                      | |
|   |   - More efficient when only one can proceed               | |
|   |                                                            | |
|   |   pthread_cond_broadcast(&cond):                           | |
|   |   - Wakes ALL waiting threads                              | |
|   |   - Needed when multiple might proceed or state changed    | |
|   |   - Safer but potentially less efficient                   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**虚假唤醒（Spurious Wakeup）**：pthread_cond_wait() 可能在没有收到信号的情况下返回！原因包括内核实现细节、信号中断、多处理器竞争条件等。

**这就是为什么必须用 while 而不是 if**：每次从 wait 返回后，必须重新检查谓词。如果谓词仍为假，继续等待。只有谓词为真时才离开循环。这是 pthread 编程中最重要的规则之一。

**signal vs broadcast**：signal 唤醒至少一个等待线程（不一定只唤醒一个），broadcast 唤醒所有等待线程。当不确定时，broadcast 更安全。

---

## Producer-Consumer Pattern

```
+------------------------------------------------------------------+
|                    PRODUCER-CONSUMER                             |
+------------------------------------------------------------------+
|                                                                  |
|   +--------+     +-------------+     +--------+                  |
|   |Producer|---->|  Bounded    |---->|Consumer|                  |
|   +--------+     |    Queue    |     +--------+                  |
|                  +-------------+                                 |
|                                                                  |
|   IMPLEMENTATION:                                                |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   typedef struct {                                         | |
|   |       pthread_mutex_t lock;                                | |
|   |       pthread_cond_t not_empty;  // consumer waits         | |
|   |       pthread_cond_t not_full;   // producer waits         | |
|   |       int items[MAX];                                      | |
|   |       int count, head, tail;                               | |
|   |   } queue_t;                                               | |
|   |                                                            | |
|   |   void produce(queue_t* q, int item) {                     | |
|   |       pthread_mutex_lock(&q->lock);                        | |
|   |       while (q->count == MAX) {        // queue full       | |
|   |           pthread_cond_wait(&q->not_full, &q->lock);       | |
|   |       }                                                    | |
|   |       q->items[q->tail] = item;                            | |
|   |       q->tail = (q->tail + 1) % MAX;                       | |
|   |       q->count++;                                          | |
|   |       pthread_cond_signal(&q->not_empty);                  | |
|   |       pthread_mutex_unlock(&q->lock);                      | |
|   |   }                                                        | |
|   |                                                            | |
|   |   int consume(queue_t* q) {                                | |
|   |       pthread_mutex_lock(&q->lock);                        | |
|   |       while (q->count == 0) {          // queue empty      | |
|   |           pthread_cond_wait(&q->not_empty, &q->lock);      | |
|   |       }                                                    | |
|   |       int item = q->items[q->head];                        | |
|   |       q->head = (q->head + 1) % MAX;                       | |
|   |       q->count--;                                          | |
|   |       pthread_cond_signal(&q->not_full);                   | |
|   |       pthread_mutex_unlock(&q->lock);                      | |
|   |       return item;                                         | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
生产者-消费者是最经典的多线程模式。使用一个互斥锁和两个条件变量：not_empty（消费者等待）和 not_full（生产者等待）。

生产者：获取锁→队列满则等待 not_full→添加元素→发送 not_empty 信号→解锁。
消费者：获取锁→队列空则等待 not_empty→取出元素→发送 not_full 信号→解锁。

注意 while 循环处理虚假唤醒，信号在解锁前发送（持有锁时发送更安全）。
