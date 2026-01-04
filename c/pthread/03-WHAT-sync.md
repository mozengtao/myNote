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
