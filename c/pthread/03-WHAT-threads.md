# WHAT | pthread Building Blocks - Part 1: Threads

## Thread Lifecycle

```
+------------------------------------------------------------------+
|                    THREAD STATE MACHINE                          |
+------------------------------------------------------------------+
|                                                                  |
|                      pthread_create()                            |
|                            |                                     |
|                            v                                     |
|   +---------------+   +---------+   +---------------+            |
|   |               |   |         |   |               |            |
|   |   CREATED     |-->| RUNNING |-->|  TERMINATED   |            |
|   |               |   |         |   |               |            |
|   +---------------+   +----+----+   +-------+-------+            |
|                            |                |                    |
|                            |                |                    |
|              +-------------+-------------+  |                    |
|              |             |             |  |                    |
|              v             v             v  |                    |
|         +---------+  +---------+  +---------+                    |
|         | BLOCKED |  | WAITING |  | SLEEPING|                    |
|         | (mutex) |  | (cond)  |  | (sleep) |                    |
|         +---------+  +---------+  +---------+                    |
|              |             |             |                       |
|              +-------------+-------------+                       |
|                            |                                     |
|                            v                                     |
|                       +---------+                                |
|                       | RUNNING |                                |
|                       +---------+                                |
|                                                                  |
|   TERMINATION PATHS:                                             |
|   +------------------------------------------------------------+ |
|   | 1. Return from thread function                             | |
|   | 2. Call pthread_exit(value)                                | |
|   | 3. Cancelled by pthread_cancel()                           | |
|   | 4. Process exits (all threads terminated)                  | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
线程的生命周期包含几个状态：创建（CREATED）、运行（RUNNING）、阻塞（BLOCKED，等待互斥锁）、等待（WAITING，等待条件变量）、睡眠（SLEEPING）、终止（TERMINATED）。线程可以通过四种方式终止：从线程函数返回、调用 pthread_exit()、被 pthread_cancel() 取消、或进程退出。

---

## Join vs Detach: Resource Reclamation

```
+------------------------------------------------------------------+
|                    JOINABLE vs DETACHED                          |
+------------------------------------------------------------------+
|                                                                  |
|   JOINABLE (default)                                             |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Main Thread                Thread                        | |
|   |   +--------+                 +--------+                    | |
|   |   |        |  create         |        |                    | |
|   |   |        |---------------->|  run   |                    | |
|   |   |        |                 |   .    |                    | |
|   |   |        |                 |   .    |                    | |
|   |   |        |                 | return |                    | |
|   |   |        |                 +---+----+                    | |
|   |   |        |                     |                         | |
|   |   |        |         [ZOMBIE STATE - resources held]       | |
|   |   |        |                     |                         | |
|   |   | join() |<--------------------+                         | |
|   |   |        | (blocks until thread terminates)              | |
|   |   |        |                                               | |
|   |   | gets   | [Resources freed, return value obtained]      | |
|   |   | result |                                               | |
|   |   +--------+                                               | |
|   |                                                            | |
|   |   pthread_join(tid, &retval);                              | |
|   |   - Waits for thread to terminate                          | |
|   |   - Retrieves thread's return value                        | |
|   |   - Releases thread resources (TCB, stack)                 | |
|   |   - MUST be called for each joinable thread!               | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   DETACHED                                                       |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Main Thread                Thread                        | |
|   |   +--------+                 +--------+                    | |
|   |   |        |  create         |        |                    | |
|   |   |        |---------------->|  run   |                    | |
|   |   | detach |                 |   .    |                    | |
|   |   |        |                 |   .    |                    | |
|   |   |  ...   |                 | return |                    | |
|   |   |  ...   |                 +---+----+                    | |
|   |   |  ...   |                     |                         | |
|   |   +--------+     [Resources freed automatically]           | |
|   |                                                            | |
|   |   pthread_detach(tid);                                     | |
|   |   - Thread resources freed on termination                  | |
|   |   - No way to get return value                             | |
|   |   - No way to wait for completion                          | |
|   |   - Cannot be joined!                                      | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   CHOOSING:                                                      |
|   +------------------------------------------------------------+ |
|   | Joinable: Need result, need to wait, need coordination     | |
|   | Detached: Fire-and-forget, daemon threads, background work | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**可连接线程（Joinable）**：默认状态。线程终止后进入「僵尸状态」，资源不会释放，直到其他线程调用 pthread_join()。join 会阻塞直到目标线程终止，然后获取返回值并释放资源。每个可连接线程必须被 join，否则会造成资源泄漏。

**分离线程（Detached）**：调用 pthread_detach() 后，线程终止时资源自动释放。无法获取返回值，无法等待其完成。适用于后台任务、守护线程。memcached 的 LRU crawler 使用可连接线程，因为需要协调启动和关闭。

---

## memcached's Thread Start Pattern (The Lock Dance)

```
+------------------------------------------------------------------+
|              MEMCACHED THREAD START PATTERN                      |
|              (crawler.c lines 734-765)                           |
+------------------------------------------------------------------+
|                                                                  |
|   "Lock dance to block until thread is waiting on condition"    |
|                                                                  |
|   Main Thread                       Crawler Thread               |
|   +-----------+                     +-----------+                |
|   |           |                     |           |                |
|   | lock(mtx) |                     |           |                |
|   |     |     |                     |           |                |
|   |     v     |                     |           |                |
|   | set flag  |                     |           |                |
|   | =1        |                     |           |                |
|   |     |     |                     |           |                |
|   |     v     |     create()        |           |                |
|   | pthread_  |-------------------->| (blocked  |                |
|   | create()  |                     |  on mtx)  |                |
|   |     |     |                     |     |     |                |
|   |     v     |                     |     |     |                |
|   | cond_wait |  unlock(mtx)        |     v     |                |
|   | (releases |-------------------->| lock(mtx) |                |
|   |  mtx)     |                     |     |     |                |
|   |     |     |                     |     v     |                |
|   |     |     |    signal(cond)     | cond_     |                |
|   |     |     |<--------------------| signal()  |                |
|   |     |     |                     |     |     |                |
|   | (wakes up |  reacquire(mtx)     |     v     |                |
|   |  but mtx  |<--(blocked)-------->| cond_wait |                |
|   |  held by  |                     | (releases |                |
|   |  crawler) |                     |  mtx)     |                |
|   |     |     |                     |     |     |                |
|   |     v     |  acquire(mtx)       |     |     |                |
|   | unlock(mtx)|                    | (waiting) |                |
|   |     |     |                     |     |     |                |
|   |     v     |                     |     |     |                |
|   | return 0  |                     | (ready)   |                |
|   +-----------+                     +-----------+                |
|                                                                  |
|   GUARANTEE: When start_item_crawler_thread() returns,           |
|              the crawler thread is WAITING on lru_crawler_cond   |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
这是 memcached 中经典的「锁舞」模式（见代码注释 734-743 行）。目的是确保当 start_item_crawler_thread() 返回时，新线程已经准备好接收工作（正在等待条件变量）。

流程：
1. 主线程持有锁，设置标志，创建线程
2. 新线程阻塞在锁上
3. 主线程调用 cond_wait（释放锁，开始等待）
4. 新线程获取锁，发送信号，进入自己的 cond_wait（释放锁）
5. 主线程被唤醒，获取锁，解锁，返回

这保证了返回时新线程处于安全的等待状态。

---

## Thread Cancellation Model

```
+------------------------------------------------------------------+
|                    THREAD CANCELLATION                           |
+------------------------------------------------------------------+
|                                                                  |
|   pthread_cancel(thread_id)                                      |
|        |                                                         |
|        v                                                         |
|   +------------------------------------------------------------+ |
|   |                  CANCELLATION TYPES                        | |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   PTHREAD_CANCEL_DEFERRED (default)                        | |
|   |   +------------------------------------------------------+ | |
|   |   |                                                      | | |
|   |   |   Thread continues until CANCELLATION POINT:         | | |
|   |   |   - pthread_testcancel()                             | | |
|   |   |   - Most blocking syscalls (read, write, sleep...)   | | |
|   |   |   - pthread_cond_wait()                              | | |
|   |   |   - pthread_join()                                   | | |
|   |   |                                                      | | |
|   |   |   Thread can cleanup before terminating              | | |
|   |   |                                                      | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   |   PTHREAD_CANCEL_ASYNCHRONOUS (dangerous!)                 | |
|   |   +------------------------------------------------------+ | |
|   |   |                                                      | | |
|   |   |   Thread can be cancelled at ANY point               | | |
|   |   |   - No guaranteed cleanup                            | | |
|   |   |   - May leave locks held                             | | |
|   |   |   - May leave data structures corrupted              | | |
|   |   |   - ALMOST NEVER USE THIS                            | | |
|   |   |                                                      | | |
|   |   +------------------------------------------------------+ | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   CLEANUP HANDLERS:                                              |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread_cleanup_push(cleanup_func, arg);                 | |
|   |   // ... code that might be cancelled ...                  | |
|   |   pthread_mutex_lock(&mutex);                              | |
|   |   pthread_cleanup_push(unlock_mutex, &mutex);              | |
|   |   // ... work with locked mutex ...                        | |
|   |   pthread_cleanup_pop(1);  // 1 = execute cleanup          | |
|   |   pthread_cleanup_pop(0);  // 0 = don't execute            | |
|   |                                                            | |
|   |   If cancelled, cleanup handlers run in LIFO order         | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   RECOMMENDATION: Avoid cancellation. Use flags instead!         |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Better pattern (as in memcached):                     | |
|   |   volatile int do_run_thread = 1;  // shared flag          | |
|   |                                                            | |
|   |   // Requester:                                            | |
|   |   do_run_thread = 0;                                       | |
|   |   pthread_cond_signal(&cond);                              | |
|   |   pthread_join(tid, NULL);                                 | |
|   |                                                            | |
|   |   // Thread:                                               | |
|   |   while (do_run_thread) {                                  | |
|   |       pthread_cond_wait(&cond, &mutex);                    | |
|   |       // ... do work ...                                   | |
|   |   }                                                        | |
|   |   // cleanup here, then return                             | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
线程取消机制允许一个线程请求终止另一个线程。有两种取消类型：

1. **延迟取消（默认）**：线程继续运行直到到达取消点（如 read、write、cond_wait 等阻塞调用或显式的 pthread_testcancel）。

2. **异步取消**：线程可以在任意点被取消。极其危险——可能留下锁被持有、数据结构损坏。几乎不应该使用。

**推荐做法**：避免使用 pthread_cancel！使用共享标志（如 memcached 的 do_run_lru_crawler_thread）来协调关闭。线程在循环中检查标志，收到关闭信号后执行清理再退出。这是更安全、更可控的方式。
