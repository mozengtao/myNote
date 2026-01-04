# WHERE | How pthreads Appear in Real Codebases

## Typical Architectural Patterns

```
+------------------------------------------------------------------+
|                    THREAD-PER-CONNECTION                         |
+------------------------------------------------------------------+
|                                                                  |
|   +--------+     +---------+     +---------+     +---------+     |
|   | Client |     | Client  |     | Client  |     | Client  |     |
|   |   1    |     |    2    |     |    3    |     |    N    |     |
|   +---+----+     +----+----+     +----+----+     +----+----+     |
|       |              |              |              |             |
|       v              v              v              v             |
|   +---+----+     +----+----+     +----+----+     +----+----+     |
|   | Thread |     | Thread  |     | Thread  |     | Thread  |     |
|   |   1    |     |    2    |     |    3    |     |    N    |     |
|   +--------+     +---------+     +---------+     +---------+     |
|                                                                  |
|   PROS:                                                          |
|   - Simple programming model                                     |
|   - Blocking I/O is fine                                         |
|   - Good for long-lived connections                              |
|                                                                  |
|   CONS:                                                          |
|   - Doesn't scale beyond ~10K connections                        |
|   - High memory usage (stack per thread)                         |
|   - Context switch overhead                                      |
|                                                                  |
|   EXAMPLE: Traditional database connections, SSH servers         |
|                                                                  |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                    WORKER THREAD POOL                            |
+------------------------------------------------------------------+
|                                                                  |
|   Clients         Work Queue           Thread Pool               |
|   +------+                                                       |
|   |Client|--+     +-----+-----+     +--------+--------+          |
|   +------+  |     |Task1|Task2|     |Worker 1|Worker 2|          |
|   +------+  +---->+-----+-----+---->+--------+--------+          |
|   |Client|--+     |Task3|Task4|     |Worker 3|Worker 4|          |
|   +------+  +---->+-----+-----+---->+--------+--------+          |
|   +------+  |     |  .  |  .  |                                  |
|   |Client|--+     +-----+-----+     Fixed number of workers      |
|   +------+                          (usually = CPU cores)        |
|                                                                  |
|   STRUCTURE:                                                     |
|   +------------------------------------------------------------+ |
|   |   Main Thread: Accept connections, create tasks            | |
|   |   Work Queue:  Thread-safe queue (mutex + condvar)         | |
|   |   Workers:     Dequeue and process tasks                   | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   MEMCACHED PATTERN:                                             |
|   +------------------------------------------------------------+ |
|   |   - Worker threads pre-created at startup                  | |
|   |   - Each worker has its own event loop (libevent)          | |
|   |   - Main thread accepts, distributes to workers            | |
|   |   - Workers handle multiple connections via events         | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   PROS:                                                          |
|   - Bounded resource usage                                       |
|   - Good cache locality                                          |
|   - Scales with CPU cores                                        |
|                                                                  |
|   CONS:                                                          |
|   - More complex than thread-per-connection                      |
|   - Queue can become bottleneck                                  |
|                                                                  |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                    PIPELINE STAGES                               |
+------------------------------------------------------------------+
|                                                                  |
|   Input --> [Stage 1] --> [Stage 2] --> [Stage 3] --> Output     |
|               Thread        Thread        Thread                 |
|                                                                  |
|   +--------+    +--------+    +--------+    +--------+           |
|   | Parse  |--->| Decode |--->|Compress|--->| Write  |           |
|   | Thread |    | Thread |    | Thread |    | Thread |           |
|   +--------+    +--------+    +--------+    +--------+           |
|        |             |             |             |               |
|        v             v             v             v               |
|      Queue         Queue         Queue        Output             |
|                                                                  |
|   PROS:                                                          |
|   - Natural for streaming data                                   |
|   - Each stage can be tuned independently                        |
|   - Good parallelism if stages balanced                          |
|                                                                  |
|   CONS:                                                          |
|   - Latency = sum of all stages                                  |
|   - Slowest stage determines throughput                          |
|   - Complex error handling                                       |
|                                                                  |
|   EXAMPLE: Video encoding, network packet processing             |
|                                                                  |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                    BACKGROUND MAINTENANCE THREADS                |
+------------------------------------------------------------------+
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                     MAIN APPLICATION                       | |
|   |                                                            | |
|   |   [Worker 1] [Worker 2] [Worker 3] [Worker 4]              | |
|   |       |           |           |           |                | |
|   |       +-----+-----+-----+-----+                            | |
|   |             |                                              | |
|   |             v                                              | |
|   |   +------------------+                                     | |
|   |   | Shared Data      |                                     | |
|   |   | - Cache entries  |                                     | |
|   |   | - Statistics     |                                     | |
|   |   | - Configuration  |                                     | |
|   |   +------------------+                                     | |
|   |             ^                                              | |
|   |             |                                              | |
|   +-------------|----------------------------------------------+ |
|                 |                                                |
|   BACKGROUND    |                                                |
|   THREADS:      |                                                |
|   +-------------+----------------------------------------------+ |
|   |                                                            | |
|   |   [LRU Crawler]    [Stats Aggregator]    [Config Reloader] | |
|   |   - Scans cache    - Periodic snapshots  - Watches files   | |
|   |   - Expires items  - Logs metrics        - Hot reload      | |
|   |   - Low priority   - Non-blocking        - Signal handler  | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   MEMCACHED EXAMPLE (crawler.c):                                 |
|   +------------------------------------------------------------+ |
|   |   item_crawler_thread:                                     | |
|   |   - Started by start_item_crawler_thread()                 | |
|   |   - Waits on condition variable for work                   | |
|   |   - Crawls LRU lists to expire items                       | |
|   |   - Yields periodically (usleep) to not starve workers     | |
|   |   - Stopped gracefully via stop_item_crawler_thread()      | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
四种常见的线程架构模式：

1. **每连接一线程**：简单但不可扩展，适合长连接、低并发场景。

2. **工作线程池**：固定数量的工作线程从队列取任务处理。memcached 使用这种模式，但每个工作线程内部用事件循环处理多个连接。

3. **流水线**：数据流经多个处理阶段，每个阶段一个线程。适合流式处理，但延迟是各阶段之和。

4. **后台维护线程**：执行定期维护任务（如 memcached 的 LRU crawler）。独立于主业务逻辑，通常低优先级运行。

---

## Where pthread Logic Should Live

```
+------------------------------------------------------------------+
|                    CODE ORGANIZATION                             |
+------------------------------------------------------------------+
|                                                                  |
|   GOOD: Dedicated concurrency layer                              |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   project/                                                 | |
|   |   +-- main.c              # startup, config                | |
|   |   +-- business/           # pure business logic            | |
|   |   |   +-- cache.c         # no pthread calls here!         | |
|   |   |   +-- protocol.c      # no pthread calls here!         | |
|   |   +-- concurrency/        # all pthread code here          | |
|   |   |   +-- thread_pool.c   # worker management              | |
|   |   |   +-- sync.c          # mutex wrappers, queues         | |
|   |   |   +-- background.c    # maintenance threads            | |
|   |   +-- include/                                             | |
|   |       +-- thread_pool.h   # clean interface                | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   BAD: Scattered across business logic                           |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   void process_request(request_t* req) {                   | |
|   |       pthread_mutex_lock(&cache_lock);    // mixed in!     | |
|   |       item_t* item = cache_lookup(key);                    | |
|   |       if (item == NULL) {                                  | |
|   |           pthread_mutex_unlock(&cache_lock);               | |
|   |           pthread_mutex_lock(&network_lock);  // another!  | |
|   |           fetch_from_backend(...);                         | |
|   |           pthread_mutex_unlock(&network_lock);             | |
|   |           pthread_mutex_lock(&cache_lock);    // again!    | |
|   |           cache_insert(...);                               | |
|   |       }                                                    | |
|   |       pthread_mutex_unlock(&cache_lock);                   | |
|   |   }                                                        | |
|   |                                                            | |
|   |   Problems:                                                | |
|   |   - Lock ordering hard to verify                           | |
|   |   - Business logic obscured by locking                     | |
|   |   - Easy to forget unlock on error paths                   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   MEMCACHED APPROACH:                                            |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   thread.c    - Thread pool, connection dispatch           | |
|   |   crawler.c   - LRU crawler (separate concern)             | |
|   |   assoc.c     - Hash table with its own locking policy     | |
|   |   items.c     - Item ops, knows about item_lock            | |
|   |   slabs.c     - Slab allocator, has slabs_lock             | |
|   |                                                            | |
|   |   Each module owns its synchronization policy              | |
|   |   Clear boundaries between modules                         | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**好的做法**：将线程相关代码集中在专门的并发层。业务逻辑模块不直接调用 pthread 函数，而是使用并发层提供的抽象接口。

**坏的做法**：锁操作散布在业务逻辑中。这导致锁顺序难以验证、业务逻辑被同步代码淹没、容易在错误路径上遗漏解锁。

**memcached 的做法**：每个模块拥有自己的同步策略。thread.c 处理线程池，crawler.c 是独立的后台任务，assoc.c/items.c/slabs.c 各自管理自己的锁。模块边界清晰，每个模块内部同步策略明确。

---

## Structuring Thread Lifecycle

```
+------------------------------------------------------------------+
|                    THREAD LIFECYCLE MANAGEMENT                   |
+------------------------------------------------------------------+
|                                                                  |
|   STARTUP PATTERN:                                               |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   int main() {                                             | |
|   |       // 1. Initialize all global data BEFORE threads      | |
|   |       init_config();                                       | |
|   |       init_data_structures();                              | |
|   |                                                            | |
|   |       // 2. Block signals before creating threads          | |
|   |       block_all_signals();                                 | |
|   |                                                            | |
|   |       // 3. Create threads in dependency order             | |
|   |       start_worker_threads();      // depends on nothing   | |
|   |       start_background_threads();  // may depend on workers| |
|   |                                                            | |
|   |       // 4. Main loop or signal handling                   | |
|   |       run_main_loop();                                     | |
|   |                                                            | |
|   |       // 5. Shutdown in reverse order                      | |
|   |       stop_background_threads();                           | |
|   |       stop_worker_threads();                               | |
|   |       cleanup_resources();                                 | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   SHUTDOWN PATTERN (from memcached crawler.c):                   |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   int stop_item_crawler_thread(bool wait) {                | |
|   |       pthread_mutex_lock(&lru_crawler_lock);               | |
|   |                                                            | |
|   |       // 1. Check if already stopped                       | |
|   |       if (do_run_lru_crawler_thread == 0) {                | |
|   |           pthread_mutex_unlock(&lru_crawler_lock);         | |
|   |           return 0;  // idempotent                         | |
|   |       }                                                    | |
|   |                                                            | |
|   |       // 2. Signal thread to stop                          | |
|   |       do_run_lru_crawler_thread = 0;                       | |
|   |       pthread_cond_signal(&lru_crawler_cond);              | |
|   |       pthread_mutex_unlock(&lru_crawler_lock);             | |
|   |                                                            | |
|   |       // 3. Wait for completion if requested               | |
|   |       if (wait) {                                          | |
|   |           pthread_join(item_crawler_tid, NULL);            | |
|   |       }                                                    | |
|   |       return 0;                                            | |
|   |   }                                                        | |
|   |                                                            | |
|   |   KEY PRINCIPLES:                                          | |
|   |   - Idempotent (safe to call multiple times)               | |
|   |   - Non-blocking option (wait=false)                       | |
|   |   - Signal, don't cancel                                   | |
|   |   - Join to ensure completion                              | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   ERROR HANDLING:                                                |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   if ((ret = pthread_create(&tid, NULL, func, arg)) != 0) {| |
|   |       // pthread_create returns error code, not -1         | |
|   |       fprintf(stderr, "pthread_create: %s\n", strerror(ret));|
|   |       // Don't use errno! Return value IS the error.       | |
|   |       return -1;                                           | |
|   |   }                                                        | |
|   |                                                            | |
|   |   COMMON MISTAKE:                                          | |
|   |   if (pthread_create(...) < 0) {  // WRONG!                | |
|   |       perror("pthread_create");   // errno not set!        | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**启动模式**：按顺序初始化——先全局数据，再阻塞信号，然后按依赖顺序创建线程。

**关闭模式**（见 memcached 的 stop_item_crawler_thread）：
1. 检查是否已停止（幂等性）
2. 设置停止标志并发送信号
3. 根据需要等待线程结束

关键原则：幂等（多次调用安全）、提供非阻塞选项、用信号而非取消、用 join 确保完成。

**错误处理**：pthread 函数返回错误码（不是 -1），不设置 errno。用 strerror(ret) 而非 perror()。

---

## Reading pthread-Heavy Code

```
+------------------------------------------------------------------+
|                    CODE ANALYSIS CHECKLIST                       |
+------------------------------------------------------------------+
|                                                                  |
|   1. IDENTIFY SHARED STATE                                       |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Look for:                                                | |
|   |   - Global variables                                       | |
|   |   - Static variables                                       | |
|   |   - Heap allocations shared between threads                | |
|   |   - File descriptors                                       | |
|   |                                                            | |
|   |   memcached crawler.c examples:                            | |
|   |   +------------------------------------------------------+ | |
|   |   | static crawler crawlers[LARGEST_ID];     // shared   | | |
|   |   | static volatile int do_run_lru_crawler_thread = 0;   | | |
|   |   | crawler_module_t active_crawler_mod;     // shared   | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   2. IDENTIFY LOCK ORDERING                                      |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Draw a graph of which locks can be held together:        | |
|   |                                                            | |
|   |   lru_crawler_lock                                         | |
|   |         |                                                  | |
|   |         v                                                  | |
|   |   lru_locks[i] (per-slab class)                           | |
|   |         |                                                  | |
|   |         v                                                  | |
|   |   item_lock (per-item hash lock)                          | |
|   |                                                            | |
|   |   RULE: Always acquire in this order to prevent deadlock   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   3. IDENTIFY LIFECYCLE BOUNDARIES                               |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Questions to answer:                                     | |
|   |   - When is each thread created? (startup, on-demand?)     | |
|   |   - When does it terminate? (on signal, on error, never?)  | |
|   |   - What resources does it own?                            | |
|   |   - What happens if it crashes?                            | |
|   |                                                            | |
|   |   memcached crawler lifecycle:                             | |
|   |   +------------------------------------------------------+ | |
|   |   | Created:  start_item_crawler_thread() at startup     | | |
|   |   | Runs:     loops waiting on lru_crawler_cond          | | |
|   |   | Stops:    when do_run_lru_crawler_thread = 0         | | |
|   |   | Cleanup:  releases crawler lock, thread exits        | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   4. TRACE CRITICAL SECTIONS                                     |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   For each mutex, list:                                    | |
|   |   - What data it protects                                  | |
|   |   - All code paths that acquire it                         | |
|   |   - How long it's typically held                           | |
|   |                                                            | |
|   |   lru_crawler_lock protects:                               | |
|   |   +------------------------------------------------------+ | |
|   |   | - do_run_lru_crawler_thread flag                     | | |
|   |   | - crawler_count                                      | | |
|   |   | - active_crawler_mod state                           | | |
|   |   | - Coordination between start/stop and thread loop    | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
阅读 pthread 密集的代码时，按以下步骤分析：

1. **识别共享状态**：全局变量、静态变量、堆内存、文件描述符。在 crawler.c 中，crawlers 数组、do_run_lru_crawler_thread 标志都是共享状态。

2. **识别锁顺序**：画出锁的获取顺序图。memcached 中：lru_crawler_lock → lru_locks[i] → item_lock。必须按此顺序获取以避免死锁。

3. **识别生命周期边界**：线程何时创建、何时终止、拥有什么资源、崩溃时发生什么。

4. **追踪临界区**：每个互斥锁保护什么数据、哪些代码路径获取它、通常持有多长时间。
