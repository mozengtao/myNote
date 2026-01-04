# PITFALLS | Common pthread Bugs and Failure Modes

## Data Races

```
+------------------------------------------------------------------+
|                    DATA RACE ANATOMY                             |
+------------------------------------------------------------------+
|                                                                  |
|   DEFINITION: Two threads access same memory,                    |
|               at least one is a write,                           |
|               no synchronization between them                    |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |   Time -->                                                 | |
|   |                                                            | |
|   |   Thread A:    read(x)     write(x=1)                      | |
|   |                    \           /                           | |
|   |                     \         /                            | |
|   |                      \       /                             | |
|   |                       [RACE!]                              | |
|   |                      /       \                             | |
|   |                     /         \                            | |
|   |   Thread B:    write(x=2)    read(x)                       | |
|   |                                                            | |
|   |   Result: UNDEFINED - could be 0, 1, 2, or garbage!        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   SUBTLE RACES:                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Looks atomic but ISN'T                                | |
|   |   counter++;  // Actually: tmp = counter; tmp++; counter=tmp|
|   |                                                            | |
|   |   Thread A:   load(counter) -> 5                           | |
|   |   Thread B:   load(counter) -> 5                           | |
|   |   Thread A:   store(counter, 6)                            | |
|   |   Thread B:   store(counter, 6)  // Lost update!           | |
|   |                                                            | |
|   |   Expected: 7,  Actual: 6                                  | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   FIX:                                                           |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   // Option 1: Mutex                                       | |
|   |   pthread_mutex_lock(&counter_lock);                       | |
|   |   counter++;                                               | |
|   |   pthread_mutex_unlock(&counter_lock);                     | |
|   |                                                            | |
|   |   // Option 2: Atomic (C11)                                | |
|   |   atomic_fetch_add(&counter, 1);                           | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
数据竞争的定义：两个线程访问同一内存位置，至少一个是写操作，且它们之间没有同步。结果是未定义行为——可能得到任意值。

特别隐蔽的是 `counter++`，它看起来是原子的，实际上是「读取-修改-写回」三步操作，两个线程交叉执行会导致丢失更新。

解决方案：使用互斥锁或原子操作（C11 atomics）。

---

## Deadlocks

```
+------------------------------------------------------------------+
|                    DEADLOCK SCENARIOS                            |
+------------------------------------------------------------------+
|                                                                  |
|   SCENARIO 1: Lock Ordering Inversion                            |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread A:              Thread B:                         | |
|   |   lock(mutex1);          lock(mutex2);                     | |
|   |   lock(mutex2);  <-WAITS | WAITS->  lock(mutex1);          | |
|   |        |                       |                           | |
|   |        +-------DEADLOCK--------+                           | |
|   |                                                            | |
|   |   Both threads wait forever for locks held by each other   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   FIX: Establish global lock ordering                            |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   RULE: Always acquire mutex1 before mutex2                | |
|   |                                                            | |
|   |   Thread A:              Thread B:                         | |
|   |   lock(mutex1);          lock(mutex1);  // same order!     | |
|   |   lock(mutex2);          lock(mutex2);                     | |
|   |   ...                    ...                               | |
|   |   unlock(mutex2);        unlock(mutex2);                   | |
|   |   unlock(mutex1);        unlock(mutex1);                   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   SCENARIO 2: Forgotten Unlock on Error Path                     |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   void process() {                                         | |
|   |       pthread_mutex_lock(&mtx);                            | |
|   |       if (error_condition) {                               | |
|   |           return;  // BUG! mutex still locked!             | |
|   |       }                                                    | |
|   |       // ... more code ...                                 | |
|   |       pthread_mutex_unlock(&mtx);                          | |
|   |   }                                                        | |
|   |                                                            | |
|   |   FIX: goto cleanup pattern                                | |
|   |   void process() {                                         | |
|   |       pthread_mutex_lock(&mtx);                            | |
|   |       if (error_condition) {                               | |
|   |           goto cleanup;                                    | |
|   |       }                                                    | |
|   |       // ... more code ...                                 | |
|   |   cleanup:                                                 | |
|   |       pthread_mutex_unlock(&mtx);                          | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   SCENARIO 3: Self-Deadlock (recursive call)                     |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   void recursive_op() {                                    | |
|   |       pthread_mutex_lock(&mtx);                            | |
|   |       if (condition) {                                     | |
|   |           recursive_op();  // DEADLOCK on normal mutex!    | |
|   |       }                                                    | |
|   |       pthread_mutex_unlock(&mtx);                          | |
|   |   }                                                        | |
|   |                                                            | |
|   |   FIX: Restructure or use recursive mutex (last resort)    | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**场景 1：锁顺序反转**：线程 A 持有锁 1 等待锁 2，线程 B 持有锁 2 等待锁 1。解决方案：建立全局锁顺序，所有线程按同一顺序获取锁。

**场景 2：错误路径遗漏解锁**：在 if 语句中直接 return 忘记解锁。解决方案：使用 goto cleanup 模式确保解锁。

**场景 3：自死锁**：持有普通互斥锁时递归调用自己。解决方案：重构代码或使用递归互斥锁（最后手段）。

---

## Condition Variable Misuse

```
+------------------------------------------------------------------+
|                    CONDVAR MISUSE                                |
+------------------------------------------------------------------+
|                                                                  |
|   BUG 1: Missing Predicate Check (if instead of while)           |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   WRONG:                                                   | |
|   |   pthread_mutex_lock(&mtx);                                | |
|   |   if (queue_empty()) {          // BUG: should be while    | |
|   |       pthread_cond_wait(&cond, &mtx);                      | |
|   |   }                                                        | |
|   |   process(dequeue());  // May crash! Queue could be empty! | |
|   |   pthread_mutex_unlock(&mtx);                              | |
|   |                                                            | |
|   |   Why it fails:                                            | |
|   |   1. Spurious wakeup returns even though queue empty       | |
|   |   2. Multiple waiters, but only one item added             | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   BUG 2: Signaling Without Holding Mutex                         |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   WRONG:                                                   | |
|   |   // Thread A (producer)        // Thread B (consumer)     | |
|   |   pthread_mutex_lock(&mtx);     pthread_mutex_lock(&mtx);  | |
|   |   enqueue(item);                while (empty) {            | |
|   |   pthread_mutex_unlock(&mtx);       cond_wait(&c, &mtx);   | |
|   |   pthread_cond_signal(&c);      }                          | |
|   |   // ^^ RACE: signal may be     process(dequeue());        | |
|   |   //    lost if B not waiting   pthread_mutex_unlock(&mtx);| |
|   |                                                            | |
|   |   CORRECT: Signal while holding mutex                      | |
|   |   pthread_mutex_lock(&mtx);                                | |
|   |   enqueue(item);                                           | |
|   |   pthread_cond_signal(&c);  // While holding lock!         | |
|   |   pthread_mutex_unlock(&mtx);                              | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   BUG 3: Wrong Mutex Passed to cond_wait                         |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread_mutex_t mtx1, mtx2;                              | |
|   |   pthread_cond_t cond;                                     | |
|   |                                                            | |
|   |   pthread_mutex_lock(&mtx1);                               | |
|   |   pthread_cond_wait(&cond, &mtx2);  // BUG! wrong mutex    | |
|   |   // mtx1 still held, mtx2 not held                        | |
|   |   // Undefined behavior                                    | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**Bug 1：缺少谓词检查**：用 if 而非 while。虚假唤醒或多个等待者时会导致处理空队列。

**Bug 2：不持有锁时发送信号**：信号可能在消费者开始等待之前发送，导致信号丢失。正确做法：持有锁时发送信号。

**Bug 3：传入错误的互斥锁**：cond_wait 的第二个参数必须是当前持有的锁，否则是未定义行为。

---

## Lifetime Bugs

```
+------------------------------------------------------------------+
|                    LIFETIME BUGS                                 |
+------------------------------------------------------------------+
|                                                                  |
|   BUG 1: Passing Stack Memory to Thread                          |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   WRONG:                                                   | |
|   |   void start_worker(int id) {                              | |
|   |       int worker_id = id;                                  | |
|   |       pthread_create(&tid, NULL, worker_func, &worker_id); | |
|   |   }  // worker_id goes out of scope!                       | |
|   |                                                            | |
|   |   void* worker_func(void* arg) {                           | |
|   |       int id = *(int*)arg;  // Reading deallocated memory! | |
|   |       // May get garbage, wrong value, or crash            | |
|   |   }                                                        | |
|   |                                                            | |
|   |   CORRECT:                                                 | |
|   |   void start_worker(int id) {                              | |
|   |       int* worker_id = malloc(sizeof(int));                | |
|   |       *worker_id = id;                                     | |
|   |       pthread_create(&tid, NULL, worker_func, worker_id);  | |
|   |   }                                                        | |
|   |                                                            | |
|   |   void* worker_func(void* arg) {                           | |
|   |       int id = *(int*)arg;                                 | |
|   |       free(arg);  // Thread takes ownership and frees      | |
|   |   }                                                        | |
|   |                                                            | |
|   |   OR: Pass value directly (if it fits in void*)            | |
|   |   pthread_create(&tid, NULL, worker_func, (void*)(intptr_t)id);|
|   |   int id = (intptr_t)arg;                                  | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   BUG 2: Freeing Shared Object Too Early                         |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   WRONG:                                                   | |
|   |   shared_data_t* data = create_shared_data();              | |
|   |   pthread_create(&tid1, NULL, worker, data);               | |
|   |   pthread_create(&tid2, NULL, worker, data);               | |
|   |   free(data);  // BUG! Threads still using data!           | |
|   |                                                            | |
|   |   CORRECT:                                                 | |
|   |   pthread_create(&tid1, NULL, worker, data);               | |
|   |   pthread_create(&tid2, NULL, worker, data);               | |
|   |   pthread_join(tid1, NULL);  // Wait for threads           | |
|   |   pthread_join(tid2, NULL);  // to finish                  | |
|   |   free(data);                // Then free                  | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   BUG 3: Destroying Mutex While Held                             |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   pthread_mutex_lock(&mtx);                                | |
|   |   // ... some work ...                                     | |
|   |   pthread_mutex_destroy(&mtx);  // BUG! Still locked!      | |
|   |   // Undefined behavior                                    | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**Bug 1：传递栈内存给线程**：局部变量在函数返回后被销毁，新线程访问已释放的内存。解决方案：用 malloc 分配或直接将值（如果能放进 void*）转型传递。

**Bug 2：过早释放共享对象**：创建线程后立即释放共享数据。必须等待所有使用该数据的线程结束（join）后再释放。

**Bug 3：销毁被持有的互斥锁**：锁定状态下销毁互斥锁是未定义行为。

---

## Performance Traps

```
+------------------------------------------------------------------+
|                    PERFORMANCE TRAPS                             |
+------------------------------------------------------------------+
|                                                                  |
|   TRAP 1: False Sharing                                          |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   struct counters {                                        | |
|   |       int counter_a;  // Thread A writes here              | |
|   |       int counter_b;  // Thread B writes here              | |
|   |   };                                                        | |
|   |                                                            | |
|   |   Problem: Both counters in same cache line (64 bytes)     | |
|   |                                                            | |
|   |   CPU 0 cache line:  [counter_a | counter_b | ...]         | |
|   |   CPU 1 cache line:  [counter_a | counter_b | ...]         | |
|   |                            ^           ^                   | |
|   |   Thread A writes --------+           |                    | |
|   |   Thread B writes --------------------+                    | |
|   |                                                            | |
|   |   Cache coherence: Entire line bounces between CPUs!       | |
|   |   Performance: 10-100x slower than properly aligned        | |
|   |                                                            | |
|   |   FIX: Padding to separate cache lines                     | |
|   |   struct counters {                                        | |
|   |       int counter_a;                                       | |
|   |       char pad[60];  // Force separate cache lines         | |
|   |       int counter_b;                                       | |
|   |   };                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   TRAP 2: Over-Locking (lock granularity too coarse)             |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   WRONG: Global lock for everything                        | |
|   |   pthread_mutex_t global_lock;                             | |
|   |                                                            | |
|   |   void any_operation() {                                   | |
|   |       pthread_mutex_lock(&global_lock);   // Serialize     | |
|   |       // ... all work ...                 // everything!   | |
|   |       pthread_mutex_unlock(&global_lock);                  | |
|   |   }                                                        | |
|   |                                                            | |
|   |   BETTER: Fine-grained locking                             | |
|   |   struct bucket {                                          | |
|   |       pthread_mutex_t lock;  // Per-bucket lock            | |
|   |       item_t* items;                                       | |
|   |   };                                                        | |
|   |   bucket_t hash_table[NUM_BUCKETS];                        | |
|   |                                                            | |
|   |   void hash_op(int key) {                                  | |
|   |       int idx = key % NUM_BUCKETS;                         | |
|   |       pthread_mutex_lock(&hash_table[idx].lock);           | |
|   |       // ... work on this bucket only ...                  | |
|   |       pthread_mutex_unlock(&hash_table[idx].lock);         | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   TRAP 3: Excessive Thread Count                                 |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Threads: [1] [2] [3] [4] [5] [6] ... [1000]              | |
|   |   CPUs:    [0] [1] [2] [3]                                 | |
|   |                                                            | |
|   |   Problems:                                                | |
|   |   - Memory: 1000 threads * 8MB stack = 8GB!                | |
|   |   - Context switches: Constant CPU overhead                | |
|   |   - Cache thrashing: Each switch pollutes cache            | |
|   |                                                            | |
|   |   RULE OF THUMB:                                           | |
|   |   - CPU-bound: threads = CPU cores                         | |
|   |   - I/O-bound: threads = CPU cores * 2 (or use async)      | |
|   |   - Never: threads = number of clients                     | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**陷阱 1：伪共享**：两个线程写入同一缓存行的不同变量，导致缓存行在 CPU 之间反复失效。性能可能下降 10-100 倍。解决方案：添加填充使变量分布在不同缓存行。

**陷阱 2：过度锁定**：使用全局锁串行化所有操作。解决方案：细粒度锁（如每个哈希桶一个锁）。

**陷阱 3：线程数过多**：线程数远超 CPU 核数会导致内存浪费、上下文切换开销、缓存污染。经验法则：CPU 密集型任务线程数等于核数，I/O 密集型可以稍多，绝不应该等于客户端数。

---

## Debugging Difficulty

```
+------------------------------------------------------------------+
|                    DEBUGGING CHALLENGES                          |
+------------------------------------------------------------------+
|                                                                  |
|   HEISENBUGS: Bugs that disappear when you look for them         |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Adding debug output:                                     | |
|   |   - printf() changes timing                                | |
|   |   - May hide race conditions                               | |
|   |   - Or make them appear!                                   | |
|   |                                                            | |
|   |   Using debugger:                                          | |
|   |   - Breakpoints stop threads differently                   | |
|   |   - Changes relative timing                                | |
|   |   - Bug may not reproduce                                  | |
|   |                                                            | |
|   |   Running under Valgrind/sanitizers:                       | |
|   |   - 10-50x slowdown                                        | |
|   |   - Different timing reveals different bugs                | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   TOOLS FOR PTHREAD DEBUGGING:                                   |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   1. ThreadSanitizer (TSAN)                                | |
|   |      gcc -fsanitize=thread -g program.c                    | |
|   |      Detects: data races, deadlocks, lock order issues     | |
|   |                                                            | |
|   |   2. Helgrind (Valgrind tool)                              | |
|   |      valgrind --tool=helgrind ./program                    | |
|   |      Detects: races, lock misuse, condition misuse         | |
|   |                                                            | |
|   |   3. GDB thread commands                                   | |
|   |      info threads        # list all threads                | |
|   |      thread N            # switch to thread N              | |
|   |      thread apply all bt # backtrace all threads           | |
|   |                                                            | |
|   |   4. Stress testing                                        | |
|   |      Run with many threads, varied timing, for hours       | |
|   |      Use different CPU counts                              | |
|   |      Add artificial delays to widen race windows           | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   REPRODUCTION STRATEGIES:                                       |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   1. Add deliberate sched_yield() to widen race window     | |
|   |   2. Pin threads to specific CPUs                          | |
|   |   3. Use taskset to limit CPU affinity                     | |
|   |   4. Record and replay with rr (Mozilla recorder)          | |
|   |   5. Log lock acquisitions with timestamps                 | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**Heisenbug**：观察时消失的 bug。添加 printf 改变时序、使用调试器暂停线程、运行 sanitizer 都会改变行为，可能使 bug 消失或让不同 bug 出现。

**调试工具**：
1. **ThreadSanitizer**：编译时加 `-fsanitize=thread`，检测数据竞争和死锁
2. **Helgrind**：Valgrind 工具，检测竞争和锁误用
3. **GDB**：`info threads`、`thread N`、`thread apply all bt`
4. **压力测试**：多线程长时间运行，变化 CPU 数量

**复现策略**：添加 sched_yield() 扩大竞争窗口、绑定线程到特定 CPU、使用 rr 录制回放。
