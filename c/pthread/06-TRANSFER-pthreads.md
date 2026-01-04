# TRANSFER | Using pthreads Correctly in Real Projects

## Decision Framework: pthreads vs Alternatives

```
+------------------------------------------------------------------+
|                    CONCURRENCY MODEL SELECTION                   |
+------------------------------------------------------------------+
|                                                                  |
|   PTHREADS vs EVENT LOOP                                         |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Choose PTHREADS when:                                    | |
|   |   +------------------------------------------------------+ | |
|   |   | - CPU-bound parallel work                            | | |
|   |   | - Need to utilize multiple cores                     | | |
|   |   | - Shared mutable state is unavoidable                | | |
|   |   | - Long-running tasks per request                     | | |
|   |   | - Blocking APIs that can't be avoided                | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   |   Choose EVENT LOOP when:                                  | |
|   |   +------------------------------------------------------+ | |
|   |   | - I/O-bound with many concurrent connections         | | |
|   |   | - Short request processing time                      | | |
|   |   | - 10K+ concurrent connections needed                 | | |
|   |   | - Latency-sensitive (avoid context switch)           | | |
|   |   | - Single-threaded simplicity valued                  | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   |   HYBRID (memcached approach):                             | |
|   |   +------------------------------------------------------+ | |
|   |   | - N worker threads (N = CPU cores)                   | | |
|   |   | - Each thread runs its own event loop                | | |
|   |   | - Best of both: multi-core + high concurrency        | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   PTHREADS vs PROCESSES                                          |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Choose PTHREADS when:                                    | |
|   |   +------------------------------------------------------+ | |
|   |   | - Frequent data sharing needed                       | | |
|   |   | - Low-latency communication required                 | | |
|   |   | - Memory efficiency matters                          | | |
|   |   | - Shared caches/data structures                      | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   |   Choose PROCESSES when:                                   | |
|   |   +------------------------------------------------------+ | |
|   |   | - Fault isolation critical (crash doesn't kill all)  | | |
|   |   | - Security isolation needed                          | | |
|   |   | - Different privilege levels                         | | |
|   |   | - Language/runtime diversity                         | | |
|   |   | - Fork-exec pattern (child runs different program)   | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**pthreads vs 事件循环**：pthreads 适合 CPU 密集、需要多核、共享可变状态、阻塞 API 的场景。事件循环适合 I/O 密集、高并发连接、低延迟的场景。memcached 采用混合模式：多个工作线程，每个线程内运行事件循环。

**pthreads vs 进程**：pthreads 适合频繁数据共享、低延迟通信、共享缓存的场景。进程适合需要故障隔离、安全隔离、不同权限级别的场景。

---

## Recommended Architectural Rules

```
+------------------------------------------------------------------+
|                    ARCHITECTURAL PRINCIPLES                      |
+------------------------------------------------------------------+
|                                                                  |
|   RULE 1: Minimize Shared Mutable State                          |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   PREFER:                                                  | |
|   |   +------------------------------------------------------+ | |
|   |   | - Immutable shared data (read-only after init)       | | |
|   |   | - Thread-local data (no sharing)                     | | |
|   |   | - Message passing (copy data, not share)             | | |
|   |   | - One owner at a time (explicit transfer)            | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   |   AVOID:                                                   | |
|   |   +------------------------------------------------------+ | |
|   |   | - Global mutable variables                           | | |
|   |   | - Fine-grained sharing without clear ownership       | | |
|   |   | - "Anyone can touch this" data structures            | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   RULE 2: Define Strict Ownership                                |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   For EVERY piece of shared data, document:               | |
|   |                                                            | |
|   |   /* lru_crawler_lock protects:                            | |
|   |    *   - do_run_lru_crawler_thread                         | |
|   |    *   - crawler_count                                     | |
|   |    *   - active_crawler_mod state                          | |
|   |    *                                                       | |
|   |    * Lock ordering: Must be acquired BEFORE lru_locks[i]   | |
|   |    *                                                       | |
|   |    * Held by: main thread (start/stop), crawler thread     | |
|   |    */                                                      | |
|   |   static pthread_mutex_t lru_crawler_lock = ...;           | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   RULE 3: Centralize Locking Policy                              |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   GOOD: Lock inside the data structure                     | |
|   |   +------------------------------------------------------+ | |
|   |   |   typedef struct {                                   | | |
|   |   |       pthread_mutex_t lock;                          | | |
|   |   |       item_t* items;                                 | | |
|   |   |   } safe_list_t;                                     | | |
|   |   |                                                      | | |
|   |   |   void safe_list_add(safe_list_t* list, item_t* it) {| | |
|   |   |       pthread_mutex_lock(&list->lock);               | | |
|   |   |       // add item                                    | | |
|   |   |       pthread_mutex_unlock(&list->lock);             | | |
|   |   |   }                                                  | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   |   BAD: Callers manage locking                              | |
|   |   +------------------------------------------------------+ | |
|   |   |   // Caller 1:                                       | | |
|   |   |   lock(some_lock);                                   | | |
|   |   |   list_add(list, item);  // Did I use right lock?    | | |
|   |   |   unlock(some_lock);                                 | | |
|   |   |                                                      | | |
|   |   |   // Caller 2:                                       | | |
|   |   |   list_add(list, item);  // Forgot to lock!          | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**规则 1：最小化共享可变状态**。优先使用不可变共享数据、线程本地数据、消息传递、明确的所有权转移。避免全局可变变量。

**规则 2：定义严格的所有权**。为每个共享数据写文档，说明哪个锁保护它、锁顺序是什么、哪些线程会持有它。

**规则 3：集中锁策略**。锁应该封装在数据结构内部，而不是让调用者管理。这避免了调用者忘记加锁或用错锁的问题。

---

## Designing a Safe Thread Pool

```
+------------------------------------------------------------------+
|                    THREAD POOL DESIGN                            |
+------------------------------------------------------------------+
|                                                                  |
|   COMPONENTS:                                                    |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   typedef struct {                                         | |
|   |       // Thread management                                 | |
|   |       pthread_t* threads;                                  | |
|   |       int thread_count;                                    | |
|   |       volatile bool shutdown;                              | |
|   |                                                            | |
|   |       // Work queue                                        | |
|   |       pthread_mutex_t queue_lock;                          | |
|   |       pthread_cond_t work_available;                       | |
|   |       pthread_cond_t work_done;  // optional, for joins    | |
|   |       task_t* queue_head;                                  | |
|   |       task_t* queue_tail;                                  | |
|   |       int queue_size;                                      | |
|   |       int active_workers;  // for graceful shutdown        | |
|   |   } thread_pool_t;                                         | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   WORKER LOOP:                                                   |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   void* worker_thread(void* arg) {                         | |
|   |       thread_pool_t* pool = arg;                           | |
|   |                                                            | |
|   |       while (1) {                                          | |
|   |           pthread_mutex_lock(&pool->queue_lock);           | |
|   |                                                            | |
|   |           // Wait for work OR shutdown                     | |
|   |           while (pool->queue_size == 0 && !pool->shutdown) { |
|   |               pthread_cond_wait(&pool->work_available,     | |
|   |                                 &pool->queue_lock);        | |
|   |           }                                                | |
|   |                                                            | |
|   |           // Check for shutdown                            | |
|   |           if (pool->shutdown && pool->queue_size == 0) {   | |
|   |               pthread_mutex_unlock(&pool->queue_lock);     | |
|   |               break;  // Exit loop                         | |
|   |           }                                                | |
|   |                                                            | |
|   |           // Dequeue task                                  | |
|   |           task_t* task = dequeue(&pool->queue_head);       | |
|   |           pool->queue_size--;                              | |
|   |           pool->active_workers++;                          | |
|   |           pthread_mutex_unlock(&pool->queue_lock);         | |
|   |                                                            | |
|   |           // Execute task (outside lock!)                  | |
|   |           task->func(task->arg);                           | |
|   |           free(task);                                      | |
|   |                                                            | |
|   |           // Mark as done                                  | |
|   |           pthread_mutex_lock(&pool->queue_lock);           | |
|   |           pool->active_workers--;                          | |
|   |           if (pool->active_workers == 0) {                 | |
|   |               pthread_cond_signal(&pool->work_done);       | |
|   |           }                                                | |
|   |           pthread_mutex_unlock(&pool->queue_lock);         | |
|   |       }                                                    | |
|   |       return NULL;                                         | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   GRACEFUL SHUTDOWN:                                             |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   void thread_pool_destroy(thread_pool_t* pool) {          | |
|   |       pthread_mutex_lock(&pool->queue_lock);               | |
|   |       pool->shutdown = true;                               | |
|   |       pthread_cond_broadcast(&pool->work_available);       | |
|   |       pthread_mutex_unlock(&pool->queue_lock);             | |
|   |                                                            | |
|   |       // Wait for all threads                              | |
|   |       for (int i = 0; i < pool->thread_count; i++) {       | |
|   |           pthread_join(pool->threads[i], NULL);            | |
|   |       }                                                    | |
|   |                                                            | |
|   |       // Cleanup remaining tasks                           | |
|   |       // ...                                               | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
一个安全的线程池需要：
- **线程管理**：线程数组、关闭标志
- **工作队列**：互斥锁、条件变量、队列头尾指针
- **活跃计数**：用于优雅关闭

工作循环的关键点：
1. 用 while 检查队列空且未关闭
2. 先检查关闭条件再取任务
3. 任务执行在锁外部
4. 跟踪活跃工作者用于等待完成

优雅关闭：设置关闭标志、broadcast 唤醒所有线程、join 等待所有线程退出。

---

## Testing and Debugging pthread Code

```
+------------------------------------------------------------------+
|                    TESTING STRATEGIES                            |
+------------------------------------------------------------------+
|                                                                  |
|   1. STRESS TESTING                                              |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Run with varied parameters                            | |
|   |   for threads in 1 2 4 8 16 32 64; do                      | |
|   |       for iterations in 100 1000 10000; do                 | |
|   |           ./test --threads=$threads --iter=$iterations     | |
|   |       done                                                 | |
|   |   done                                                     | |
|   |                                                            | |
|   |   // Run for extended time                                 | |
|   |   timeout 1h ./stress_test --continuous                    | |
|   |                                                            | |
|   |   // Vary CPU affinity                                     | |
|   |   taskset -c 0 ./test      # single CPU                    | |
|   |   taskset -c 0-3 ./test    # 4 CPUs                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   2. SANITIZERS (Build-time)                                     |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   # Thread Sanitizer - detects data races                  | |
|   |   gcc -fsanitize=thread -g -O1 program.c                   | |
|   |                                                            | |
|   |   # Address Sanitizer - memory errors (some race aspects)  | |
|   |   gcc -fsanitize=address -g program.c                      | |
|   |                                                            | |
|   |   # Run with sanitizer                                     | |
|   |   TSAN_OPTIONS="history_size=7" ./program                  | |
|   |                                                            | |
|   |   Example TSAN output:                                     | |
|   |   WARNING: ThreadSanitizer: data race (pid=1234)           | |
|   |     Write of size 4 at 0x... by thread T1:                 | |
|   |       #0 worker_func program.c:42                          | |
|   |     Previous read of size 4 at 0x... by thread T2:         | |
|   |       #0 main_loop program.c:78                            | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   3. LOGGING STRATEGIES                                          |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Thread-safe logging with minimal lock time            | |
|   |   #define LOG(fmt, ...) do { \                             | |
|   |       char buf[256]; \                                     | |
|   |       int len = snprintf(buf, sizeof(buf), \               | |
|   |           "[T%lu] " fmt "\n", pthread_self(), ##__VA_ARGS__);\
|   |       write(log_fd, buf, len); /* atomic for small writes */ \
|   |   } while(0)                                               | |
|   |                                                            | |
|   |   // Log lock operations (debug mode only)                 | |
|   |   #ifdef DEBUG                                             | |
|   |   #define LOCK(m) do { \                                   | |
|   |       LOG("LOCK %s", #m); \                                | |
|   |       pthread_mutex_lock(m); \                             | |
|   |       LOG("LOCKED %s", #m); \                              | |
|   |   } while(0)                                               | |
|   |   #endif                                                   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**压力测试**：使用不同线程数、迭代次数、CPU 亲和性长时间运行。

**Sanitizers**：编译时加 `-fsanitize=thread` 检测数据竞争，输出会指明竞争的位置和涉及的线程。

**日志策略**：使用线程安全的日志（小写入在 write() 上是原子的），调试模式下记录锁操作帮助分析死锁。

---

## Transferable Concepts

```
+------------------------------------------------------------------+
|                    CONCEPTS THAT TRANSFER                        |
+------------------------------------------------------------------+
|                                                                  |
|   OWNERSHIP DISCIPLINE (transfers everywhere)                    |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread:  "Who owns this memory? Who frees it?"          | |
|   |   Rust:     Borrow checker enforces ownership              | |
|   |   Go:       Channel-based ownership transfer               | |
|   |   C++:      unique_ptr, shared_ptr, RAII                   | |
|   |                                                            | |
|   |   The CONCEPT is universal, only enforcement differs       | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   EXPLICIT LIFECYCLE MANAGEMENT (transfers everywhere)           |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread:  create -> run -> join/detach                   | |
|   |   Futures:  create -> pending -> resolved                  | |
|   |   Actors:   spawn -> mailbox -> terminate                  | |
|   |   Coroutines: create -> suspend/resume -> complete         | |
|   |                                                            | |
|   |   Every model needs lifecycle management                   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   CONCEPTS NOT TO COPY BLINDLY:                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   1. Low-level lock primitives                             | |
|   |      - Higher-level languages have better abstractions     | |
|   |      - Go: channels, sync.Mutex                            | |
|   |      - Java: synchronized, concurrent collections          | |
|   |                                                            | |
|   |   2. Manual memory management                              | |
|   |      - GC languages handle this                            | |
|   |      - Still need ownership for other resources            | |
|   |                                                            | |
|   |   3. Condition variable patterns                           | |
|   |      - Often replaced by channels or futures               | |
|   |      - But the CONCEPT (wait for state change) transfers   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**可迁移的概念**：
1. **所有权纪律**：谁拥有这块内存，谁负责释放。在 Rust 中由借用检查器强制执行，在 Go 中通过 channel 传递所有权，在 C++ 中用智能指针。概念是通用的，只是执行方式不同。

2. **显式生命周期管理**：创建→运行→终止的模式在所有并发模型中都存在。

**不应盲目复制的概念**：
1. 低级锁原语——高级语言有更好的抽象
2. 手动内存管理——GC 语言自动处理
3. 条件变量模式——常被 channel 或 future 替代

---

## Reference Projects Summary

```
+------------------------------------------------------------------+
|                    REFERENCE PROJECT READING GUIDE               |
+------------------------------------------------------------------+
|                                                                  |
|   MEMCACHED                                                      |
|   +------------------------------------------------------------+ |
|   |   Key files to study:                                      | |
|   |   - thread.c:   Worker thread pool, dispatch               | |
|   |   - crawler.c:  Background thread with condvar pattern     | |
|   |   - assoc.c:    Hash table with lock striping              | |
|   |   - items.c:    Item lifecycle and locking                 | |
|   |                                                            | |
|   |   Patterns demonstrated:                                   | |
|   |   - "Lock dance" for thread startup coordination           | |
|   |   - Flag-based shutdown (no pthread_cancel)                | |
|   |   - Module-level lock ownership                            | |
|   |   - Static mutex initialization                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   REDIS (background threads)                                     |
|   +------------------------------------------------------------+ |
|   |   Key files to study:                                      | |
|   |   - bio.c:      Background I/O threads                     | |
|   |   - lazyfree.c: Async memory freeing                       | |
|   |                                                            | |
|   |   Patterns demonstrated:                                   | |
|   |   - Hybrid event loop + pthread for background work        | |
|   |   - Offloading expensive ops to background                 | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   NGINX (why NOT pthreads)                                       |
|   +------------------------------------------------------------+ |
|   |   Key insight:                                             | |
|   |   - Multi-process, not multi-thread                        | |
|   |   - Each worker is a separate process                      | |
|   |   - No shared state = no locking = simpler                 | |
|   |                                                            | |
|   |   When to follow this pattern:                             | |
|   |   - Stateless request handling                             | |
|   |   - Need crash isolation                                   | |
|   |   - Simpler deployment/debugging                           | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**memcached**：研究 thread.c（工作线程池）、crawler.c（条件变量模式）、assoc.c（锁分片哈希表）。展示了锁舞模式、基于标志的关闭、模块级锁所有权。

**Redis**：研究 bio.c（后台 I/O 线程）。展示了事件循环 + pthread 混合模式，将昂贵操作卸载到后台。

**nginx**：展示了为什么有时不用 pthreads——多进程模型，无共享状态，无需锁，更简单。适用于无状态请求处理、需要崩溃隔离的场景。
