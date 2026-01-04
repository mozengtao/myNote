# WHAT | pthread Building Blocks - Part 3: Advanced Primitives

## Read-Write Locks

```
+------------------------------------------------------------------+
|                    READ-WRITE LOCKS                              |
+------------------------------------------------------------------+
|                                                                  |
|   PURPOSE: Allow multiple readers OR one writer                  |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   State Machine:                                           | |
|   |                                                            | |
|   |   +--------+   rd_lock   +------------+                    | |
|   |   |  IDLE  |------------>|  READERS   |                    | |
|   |   |        |<------------|  (N >= 1)  |                    | |
|   |   +---+----+   rd_unlock +------------+                    | |
|   |       |                        |                           | |
|   |       | wr_lock                | wr_lock (blocks)          | |
|   |       v                        v                           | |
|   |   +--------+            +-------------+                    | |
|   |   | WRITER |            | WRITER WAITS|                    | |
|   |   | (N = 1)|            +-------------+                    | |
|   |   +--------+                                               | |
|   |       |                                                    | |
|   |       | wr_unlock                                          | |
|   |       v                                                    | |
|   |   +--------+                                               | |
|   |   |  IDLE  |                                               | |
|   |   +--------+                                               | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   API:                                                           |
|   +------------------------------------------------------------+ |
|   |   pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;    | |
|   |                                                            | |
|   |   // Reader                                                | |
|   |   pthread_rwlock_rdlock(&rwlock);                          | |
|   |   read_shared_data();  // multiple readers OK              | |
|   |   pthread_rwlock_unlock(&rwlock);                          | |
|   |                                                            | |
|   |   // Writer                                                | |
|   |   pthread_rwlock_wrlock(&rwlock);                          | |
|   |   modify_shared_data();  // exclusive access               | |
|   |   pthread_rwlock_unlock(&rwlock);                          | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   WHEN RW LOCKS HELP:                                            |
|   +------------------------------------------------------------+ |
|   |   - Read-heavy workloads (10:1 or higher read:write ratio) | |
|   |   - Long critical sections (readers don't block each other)| |
|   |   - Many concurrent readers                                | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   WHEN RW LOCKS HURT:                                            |
|   +------------------------------------------------------------+ |
|   |   - Balanced read/write workloads                          | |
|   |   - Short critical sections (rwlock overhead dominates)    | |
|   |   - Write-heavy workloads (writers constantly waiting)     | |
|   |   - RWlock has higher overhead than mutex!                 | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   WRITER STARVATION:                                             |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Time -->                                                 | |
|   |   Reader 1: [=============]                                | |
|   |   Reader 2:     [=============]                            | |
|   |   Reader 3:         [=============]                        | |
|   |   Reader 4:             [=============]                    | |
|   |   Writer:   ..............................[==] finally!    | |
|   |                                                            | |
|   |   If new readers keep arriving, writer waits forever       | |
|   |   Solution: PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
读写锁允许多个读者同时访问，但写者独占。状态机有三种状态：空闲、有读者、有写者。

**适用场景**：读多写少（10:1 或更高）、临界区较长、并发读者多。

**不适用场景**：读写均衡、临界区短（rwlock 开销大于 mutex）、写多读少。

**写者饥饿问题**：如果读者不断到来，写者可能永远等待。解决方案是使用写者优先属性 PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP。

---

## Barriers

```
+------------------------------------------------------------------+
|                    BARRIERS                                      |
+------------------------------------------------------------------+
|                                                                  |
|   PURPOSE: Synchronization point where ALL threads must arrive   |
|            before ANY can proceed                                |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread 0    Thread 1    Thread 2    Thread 3             | |
|   |      |           |           |           |                 | |
|   |      v           v           v           v                 | |
|   |   [Phase 1]   [Phase 1]   [Phase 1]   [Phase 1]            | |
|   |      |           |           |           |                 | |
|   |      v           v           v           v                 | |
|   |   ======= BARRIER (count=4) ========                       | |
|   |      |           |           |           |                 | |
|   |   arrives     arrives     arrives     arrives              | |
|   |   (waits)     (waits)     (waits)     (last one!)          | |
|   |      |           |           |           |                 | |
|   |      +-----+-----+-----+-----+           |                 | |
|   |            |                             |                 | |
|   |            +----- ALL released <---------+                 | |
|   |            |           |           |           |           | |
|   |            v           v           v           v           | |
|   |        [Phase 2]   [Phase 2]   [Phase 2]   [Phase 2]       | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   API:                                                           |
|   +------------------------------------------------------------+ |
|   |   pthread_barrier_t barrier;                               | |
|   |   pthread_barrier_init(&barrier, NULL, NUM_THREADS);       | |
|   |                                                            | |
|   |   // In each thread:                                       | |
|   |   do_phase1_work();                                        | |
|   |   pthread_barrier_wait(&barrier);  // blocks until all     | |
|   |   do_phase2_work();                // guaranteed all done  | |
|   |                                                            | |
|   |   pthread_barrier_destroy(&barrier);                       | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   USE CASES:                                                     |
|   +------------------------------------------------------------+ |
|   |   - Parallel algorithms with phases                        | |
|   |   - Matrix computation (all rows before columns)           | |
|   |   - Simulations with time steps                            | |
|   |   - Testing: ensure all threads reach certain point        | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
屏障（Barrier）是一个同步点，所有线程必须到达后才能继续。适用于分阶段并行算法，如矩阵计算（所有行处理完再处理列）、物理模拟的时间步等。初始化时指定线程数，每个线程调用 barrier_wait() 阻塞，直到所有线程都到达，然后同时释放。

---

## pthread_once

```
+------------------------------------------------------------------+
|                    PTHREAD_ONCE                                  |
+------------------------------------------------------------------+
|                                                                  |
|   PURPOSE: Ensure initialization runs EXACTLY ONCE               |
|            even if called from multiple threads simultaneously   |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   WRONG (race condition):                                  | |
|   |                                                            | |
|   |   static int initialized = 0;                              | |
|   |   static resource_t* resource;                             | |
|   |                                                            | |
|   |   void ensure_init() {                                     | |
|   |       if (!initialized) {      // Thread A checks          | |
|   |           // Thread B also checks (still 0)                | |
|   |           resource = create_resource();  // A creates      | |
|   |           // B also creates (duplicate!)                   | |
|   |           initialized = 1;                                 | |
|   |       }                                                    | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   CORRECT (using pthread_once):                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   static pthread_once_t init_once = PTHREAD_ONCE_INIT;     | |
|   |   static resource_t* resource;                             | |
|   |                                                            | |
|   |   void do_init() {                                         | |
|   |       resource = create_resource();                        | |
|   |   }                                                        | |
|   |                                                            | |
|   |   void ensure_init() {                                     | |
|   |       pthread_once(&init_once, do_init);                   | |
|   |       // Guaranteed: do_init runs exactly once             | |
|   |       // All threads see initialized state after return    | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   BEHAVIOR:                                                      |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread A        Thread B        Thread C                 | |
|   |      |               |               |                     | |
|   |   once()          once()          once()                   | |
|   |      |               |               |                     | |
|   |   [runs           [blocks]        [blocks]                 | |
|   |    do_init()]        |               |                     | |
|   |      |               |               |                     | |
|   |   [done]---------->[unblocks]----->[unblocks]              | |
|   |      |               |               |                     | |
|   |   returns         returns         returns                  | |
|   |                                                            | |
|   |   All three see the same initialized state                 | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
pthread_once 确保初始化函数只执行一次，即使多个线程同时调用。第一个到达的线程执行初始化函数，其他线程阻塞等待，初始化完成后所有线程继续执行并看到一致的状态。这比「双重检查锁定」（DCLP）模式更安全可靠。

---

## Memory Visibility and Happens-Before

```
+------------------------------------------------------------------+
|                    MEMORY VISIBILITY                             |
+------------------------------------------------------------------+
|                                                                  |
|   PROBLEM: Modern CPUs reorder memory operations                 |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   CPU 0 writes:          CPU 1 reads:                      | |
|   |   data = 42;             if (ready) {                      | |
|   |   ready = 1;                 x = data;  // might see 0!    | |
|   |                          }                                 | |
|   |                                                            | |
|   |   Reason: CPU caches, store buffers, compiler reordering   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   PTHREAD GUARANTEES (happens-before relationships):             |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   1. MUTEX UNLOCK --> MUTEX LOCK                           | |
|   |   +--------------------------------------------------+     | |
|   |   |                                                  |     | |
|   |   |  Thread A:                Thread B:              |     | |
|   |   |  data = 42;               pthread_mutex_lock();  |     | |
|   |   |  pthread_mutex_unlock();  x = data;  // sees 42  |     | |
|   |   |         |                     ^                  |     | |
|   |   |         +----happens-before---+                  |     | |
|   |   |                                                  |     | |
|   |   +--------------------------------------------------+     | |
|   |                                                            | |
|   |   2. COND_SIGNAL --> COND_WAIT RETURN                      | |
|   |   +--------------------------------------------------+     | |
|   |   |                                                  |     | |
|   |   |  Thread A:                Thread B:              |     | |
|   |   |  data = 42;               pthread_cond_wait();   |     | |
|   |   |  pthread_cond_signal();   // after return:       |     | |
|   |   |         |                 x = data;  // sees 42  |     | |
|   |   |         +----happens-before---+                  |     | |
|   |   |                                                  |     | |
|   |   +--------------------------------------------------+     | |
|   |                                                            | |
|   |   3. PTHREAD_CREATE --> THREAD START                       | |
|   |   +--------------------------------------------------+     | |
|   |   |                                                  |     | |
|   |   |  Main:                    New thread:            |     | |
|   |   |  data = 42;               // start:              |     | |
|   |   |  pthread_create();        x = data;  // sees 42  |     | |
|   |   |         |                     ^                  |     | |
|   |   |         +----happens-before---+                  |     | |
|   |   |                                                  |     | |
|   |   +--------------------------------------------------+     | |
|   |                                                            | |
|   |   4. THREAD END --> PTHREAD_JOIN RETURN                    | |
|   |   +--------------------------------------------------+     | |
|   |   |                                                  |     | |
|   |   |  Thread:                  Main:                  |     | |
|   |   |  data = 42;               pthread_join();        |     | |
|   |   |  return;                  // after join:         |     | |
|   |   |         |                 x = data;  // sees 42  |     | |
|   |   |         +----happens-before---+                  |     | |
|   |   |                                                  |     | |
|   |   +--------------------------------------------------+     | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   WHAT PTHREAD DOES NOT GUARANTEE:                               |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   - Visibility without synchronization primitives          | |
|   |   - Atomicity of non-atomic operations                     | |
|   |   - Order of independent operations                        | |
|   |                                                            | |
|   |   // WRONG: no happens-before relationship                 | |
|   |   Thread A: flag = 1;                                      | |
|   |   Thread B: while (!flag);  // may loop forever!           | |
|   |                             // compiler may optimize out   | |
|   |                                                            | |
|   |   // CORRECT: use mutex or atomic                          | |
|   |   Thread A: lock(); flag = 1; unlock();                    | |
|   |   Thread B: lock(); while (!flag) { unlock(); lock(); }    | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
现代 CPU 会重排内存操作以提高性能，这可能导致一个线程的写入对另一个线程不可见。pthread 通过同步原语建立「happens-before」关系来保证内存可见性：

1. **互斥锁 unlock → lock**：unlock 之前的写入对 lock 之后的读取可见
2. **条件变量 signal → wait 返回**：signal 之前的写入对 wait 返回后的读取可见
3. **pthread_create → 线程开始**：create 之前的写入对新线程可见
4. **线程结束 → pthread_join 返回**：线程结束前的写入对 join 返回后可见

**不保证**：没有同步原语时的可见性。裸的共享变量可能永远看不到更新，编译器甚至可能优化掉循环！
