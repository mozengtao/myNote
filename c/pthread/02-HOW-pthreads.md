# HOW | Core Design Model Behind pthreads

## The pthread Execution Model

```
+------------------------------------------------------------------+
|                    PROCESS MEMORY LAYOUT                         |
+------------------------------------------------------------------+
|                                                                  |
|   HIGH ADDRESS                                                   |
|   +----------------------------------------------------------+   |
|   |                    KERNEL SPACE                          |   |
|   +----------------------------------------------------------+   |
|   |                    STACK (Thread 0 - Main)               |   |
|   |                    grows downward                        |   |
|   +----------------------------------------------------------+   |
|   |                    STACK (Thread 1)                      |   |
|   +----------------------------------------------------------+   |
|   |                    STACK (Thread 2)                      |   |
|   +----------------------------------------------------------+   |
|   |                         ...                              |   |
|   +----------------------------------------------------------+   |
|   |                    MEMORY MAPPED REGION                  |   |
|   |              (shared libraries, mmap files)              |   |
|   +----------------------------------------------------------+   |
|   |                         HEAP                             |   |
|   |                    grows upward                          |   |
|   +----------------------------------------------------------+   |
|   |                    BSS (uninitialized data)              |   |
|   +----------------------------------------------------------+   |
|   |                    DATA (initialized globals)            |   |
|   +----------------------------------------------------------+   |
|   |                    TEXT (code)                           |   |
|   +----------------------------------------------------------+   |
|   LOW ADDRESS                                                    |
|                                                                  |
|   SHARED between threads: TEXT, DATA, BSS, HEAP, MMAP            |
|   PRIVATE to each thread: STACK, registers, TLS                  |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
pthread 的执行模型基于进程内共享地址空间。每个线程都运行在同一个进程的内存布局中。代码段（TEXT）、全局数据（DATA/BSS）、堆内存（HEAP）和内存映射区域在所有线程间共享。只有栈空间、寄存器和线程本地存储（TLS）是每个线程私有的。这种设计使得线程间通信极其高效（直接读写内存），但也要求程序员必须正确处理同步。

---

## Thread as Execution Context

```
+------------------------------------------------------------------+
|                    THREAD EXECUTION CONTEXT                      |
+------------------------------------------------------------------+
|                                                                  |
|   PROCESS                                                        |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread 0 (main)        Thread 1           Thread 2       | |
|   |   +--------------+      +--------------+   +--------------+| |
|   |   | PC (EIP/RIP) |      | PC (EIP/RIP) |   | PC (EIP/RIP) || |
|   |   | SP (ESP/RSP) |      | SP (ESP/RSP) |   | SP (ESP/RSP) || |
|   |   | Registers    |      | Registers    |   | Registers    || |
|   |   | Stack        |      | Stack        |   | Stack        || |
|   |   | TLS pointer  |      | TLS pointer  |   | TLS pointer  || |
|   |   | Signal mask  |      | Signal mask  |   | Signal mask  || |
|   |   | errno        |      | errno        |   | errno        || |
|   |   +--------------+      +--------------+   +--------------+| |
|   |          |                     |                  |        | |
|   |          +----------+----------+------------------+        | |
|   |                     |                                      | |
|   |                     v                                      | |
|   |   +------------------------------------------------+       | |
|   |   |              SHARED RESOURCES                  |       | |
|   |   |  - Virtual memory mappings                     |       | |
|   |   |  - File descriptor table                       |       | |
|   |   |  - Signal handlers                             |       | |
|   |   |  - Current working directory                   |       | |
|   |   |  - User/group IDs                              |       | |
|   |   +------------------------------------------------+       | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
每个线程是一个独立的执行上下文，包含：程序计数器（PC，指向当前执行的指令）、栈指针（SP）、通用寄存器、栈空间、TLS 指针、信号掩码和线程私有的 errno。所有线程共享进程级资源：虚拟内存映射、文件描述符表、信号处理程序、当前工作目录、用户/组 ID。这意味着一个线程打开的文件，其他线程也可以访问。

---

## How pthread Concurrency Differs from Other Models

```
+------------------------------------------------------------------+
|                    CONCURRENCY MODEL COMPARISON                  |
+------------------------------------------------------------------+
|                                                                  |
|   1. PTHREADS (Shared Memory)                                    |
|   +------------------------------------------------------------+ |
|   |  Process                                                   | |
|   |  +--------+  +--------+  +--------+                        | |
|   |  |Thread 1|  |Thread 2|  |Thread 3|                        | |
|   |  +---+----+  +----+---+  +----+---+                        | |
|   |      |            |           |                            | |
|   |      +------------+-----------+                            | |
|   |                   |                                        | |
|   |              [SHARED DATA]                                 | |
|   |                                                            | |
|   |  + Fast communication (just read/write memory)             | |
|   |  - Must synchronize explicitly (mutexes, etc.)             | |
|   |  - Bugs affect entire process                              | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   2. PROCESSES (Isolated Memory)                                 |
|   +------------------------------------------------------------+ |
|   |  +----------+    +----------+    +----------+              | |
|   |  | Process1 |    | Process2 |    | Process3 |              | |
|   |  | [memory] |    | [memory] |    | [memory] |              | |
|   |  +----+-----+    +-----+----+    +----+-----+              | |
|   |       |                |              |                    | |
|   |       +-------+--------+-------+------+                    | |
|   |               |                |                           | |
|   |          [PIPE/SHM]       [SOCKET]                         | |
|   |                                                            | |
|   |  + Isolation (crash doesn't kill others)                   | |
|   |  + Different privilege levels possible                     | |
|   |  - Slow IPC (syscalls, copying)                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   3. EVENT LOOP (Single Thread, Multiplexed I/O)                 |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |  +---> [Check I/O ready] ---> [Handle Event 1] ---+        | |
|   |  |                                                |        | |
|   |  +---- [Handle Event 3] <--- [Handle Event 2] <---+        | |
|   |                                                            | |
|   |  + No synchronization needed                               | |
|   |  + Simple mental model                                     | |
|   |  - Can't use multiple CPUs                                 | |
|   |  - Blocking operations freeze everything                   | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   4. ASYNC/AWAIT (Cooperative Scheduling)                        |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |  async fn task1() {           async fn task2() {           | |
|   |      let x = fetch().await;       let y = query().await;   | |
|   |      process(x);                  store(y);                | |
|   |  }                            }                            | |
|   |         |                              |                   | |
|   |         +------> [EXECUTOR] <----------+                   | |
|   |                      |                                     | |
|   |                [Schedules tasks]                           | |
|   |                                                            | |
|   |  + Lightweight (no kernel thread per task)                 | |
|   |  + Explicit yield points                                   | |
|   |  - Runtime overhead                                        | |
|   |  - Colored function problem                                | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
四种并发模型对比：

1. **pthreads**：共享内存，通信快但需要显式同步，一个线程的 bug 会影响整个进程
2. **进程**：隔离内存，一个进程崩溃不影响其他进程，但 IPC 开销大
3. **事件循环**：单线程多路复用，无需同步，但无法利用多核
4. **async/await**：协作式调度，轻量级，但有运行时开销和「函数着色」问题

memcached 采用混合模式：工作线程使用 pthreads，每个线程内使用事件循环（libevent/epoll）处理多个连接。

---

## Stack Per Thread

```
+------------------------------------------------------------------+
|                    THREAD STACK MANAGEMENT                       |
+------------------------------------------------------------------+
|                                                                  |
|   DEFAULT STACK SIZE: 8MB (ulimit -s), but pthread can override  |
|                                                                  |
|   HIGH ADDRESS                                                   |
|   +----------------------------------------------------------+   |
|   |  Guard Page (PROT_NONE) - causes SIGSEGV on overflow     |   |
|   +----------------------------------------------------------+   |
|   |                                                          |   |
|   |                    STACK SPACE                           |   |
|   |                                                          |   |
|   |  +----------------------------------------------------+  |   |
|   |  | Local variables for current function               |  |   |
|   |  | Return address                                     |  |   |
|   |  | Saved registers                                    |  |   |
|   |  +----------------------------------------------------+  |   |
|   |  | Local variables for caller                         |  |   |
|   |  | Return address                                     |  |   |
|   |  | ...                                                |  |   |
|   |  +----------------------------------------------------+  |   |
|   |                                                          |   |
|   |                    (grows downward)                      |   |
|   |                          |                               |   |
|   |                          v                               |   |
|   |                                                          |   |
|   +----------------------------------------------------------+   |
|   LOW ADDRESS (stack pointer)                                    |
|                                                                  |
|   OWNERSHIP RULE:                                                |
|   +----------------------------------------------------------+   |
|   | Stack memory is OWNED by the thread                      |   |
|   | When thread exits, stack is DEALLOCATED                  |   |
|   | NEVER pass pointers to stack memory to other threads!    |   |
|   +----------------------------------------------------------+   |
|                                                                  |
|   EXAMPLE (DANGEROUS):                                           |
|                                                                  |
|   void* thread_func(void* arg) {                                 |
|       int local_data = 42;         // on THIS thread's stack     |
|       pass_to_other_thread(&local_data);  // DANGER!             |
|       return NULL;  // stack deallocated, other thread crashes   |
|   }                                                              |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
每个线程有独立的栈空间，默认 8MB（可通过 pthread_attr_setstacksize 修改）。栈顶有一个保护页（guard page），访问时会触发 SIGSEGV，用于检测栈溢出。关键的所有权规则：栈内存归线程所有，线程退出时栈被释放。永远不要将栈上变量的指针传递给其他线程！这是一个极其常见的错误——线程退出后，其他线程访问该指针会导致未定义行为。

---

## Thread-Local Storage (TLS)

```
+------------------------------------------------------------------+
|                    THREAD-LOCAL STORAGE                          |
+------------------------------------------------------------------+
|                                                                  |
|   PURPOSE: Per-thread global variables                           |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                    PROCESS                                 | |
|   |                                                            | |
|   |   Global: int shared_counter = 0;  // SHARED               | |
|   |   TLS:    __thread int tls_counter = 0;  // PER-THREAD     | |
|   |                                                            | |
|   |   Thread 0            Thread 1            Thread 2         | |
|   |   +----------+        +----------+        +----------+     | |
|   |   |tls_counter|       |tls_counter|       |tls_counter|    | |
|   |   |   = 10    |       |   = 20    |       |   = 30    |    | |
|   |   +----------+        +----------+        +----------+     | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   IMPLEMENTATION (x86-64 Linux):                                 |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   %fs register --> Thread Control Block (TCB)              | |
|   |                         |                                  | |
|   |                         v                                  | |
|   |   +--------------------------------------------------+     | |
|   |   |  TLS Block for Thread                            |     | |
|   |   |  +--------+--------+--------+--------+           |     | |
|   |   |  | errno  | tls_var| tls_var| ...    |           |     | |
|   |   |  +--------+--------+--------+--------+           |     | |
|   |   +--------------------------------------------------+     | |
|   |                                                            | |
|   |   Access: mov %fs:offset, %rax  (very fast, no syscall)    | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   USE CASES:                                                     |
|   - errno (must be per-thread!)                                  |
|   - Thread-local caches                                          |
|   - Recursion depth counters                                     |
|   - Random number generator state                                |
|                                                                  |
|   SYNTAX:                                                        |
|   - GCC/Clang: __thread int var;                                 |
|   - C11: _Thread_local int var;                                  |
|   - C++11: thread_local int var;                                 |
|   - pthread API: pthread_key_create/pthread_setspecific          |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
线程本地存储（TLS）解决了「每个线程需要独立副本的全局变量」问题。最典型的例子是 errno——每个线程必须有自己的 errno，否则一个线程设置的错误码会被另一个线程覆盖。在 x86-64 Linux 上，TLS 通过 %fs 寄存器实现，访问速度非常快，无需系统调用。声明 TLS 变量可以使用 `__thread`（GCC 扩展）、`_Thread_local`（C11）或 `thread_local`（C++11）。

---

## Scheduling Interaction with the Kernel

```
+------------------------------------------------------------------+
|                    PTHREAD SCHEDULING MODEL                      |
+------------------------------------------------------------------+
|                                                                  |
|   USER SPACE              |            KERNEL SPACE              |
|                           |                                      |
|   pthread_create()        |                                      |
|         |                 |                                      |
|         v                 |                                      |
|   +----------+            |         +------------------+         |
|   | pthread  |  clone()   |         |   Kernel Thread  |         |
|   | library  |------------|-------->|   (task_struct)  |         |
|   +----------+            |         +--------+---------+         |
|                           |                  |                   |
|                           |                  v                   |
|                           |         +------------------+         |
|                           |         |   Run Queue      |         |
|                           |         | +----+ +----+    |         |
|                           |         | |T1  | |T2  |... |         |
|                           |         | +----+ +----+    |         |
|                           |         +--------+---------+         |
|                           |                  |                   |
|                           |                  v                   |
|                           |         +------------------+         |
|                           |         |  CFS Scheduler   |         |
|                           |         | (Completely Fair |         |
|                           |         |   Scheduler)     |         |
|                           |         +--------+---------+         |
|                           |                  |                   |
|                           |                  v                   |
|                           |            [CPU CORE]                |
|                                                                  |
|   SCHEDULING POLICIES:                                           |
|   +------------------------------------------------------------+ |
|   | SCHED_OTHER (default): CFS, fair time-sharing              | |
|   | SCHED_FIFO:  Real-time, first-in first-out                 | |
|   | SCHED_RR:    Real-time, round-robin with quantum           | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   CONTEXT SWITCH TRIGGERS:                                       |
|   +------------------------------------------------------------+ |
|   | 1. Thread calls blocking syscall (read, write, sleep)      | |
|   | 2. Thread's time slice expires                             | |
|   | 3. Higher priority thread becomes runnable                 | |
|   | 4. Thread explicitly yields (sched_yield)                  | |
|   | 5. Thread blocks on mutex/condvar                          | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
在 Linux NPTL 实现中，每个 pthread 对应一个内核线程（task_struct）。pthread_create 底层调用 clone() 系统调用创建内核线程。内核使用 CFS（完全公平调度器）管理所有线程。上下文切换发生在：线程调用阻塞系统调用、时间片用完、更高优先级线程就绪、线程主动让出 CPU、或线程在锁/条件变量上阻塞。默认调度策略是 SCHED_OTHER（普通时间片轮转），也可使用 SCHED_FIFO 或 SCHED_RR 实现实时调度。

---

## Manual Ownership Rules

```
+------------------------------------------------------------------+
|                    OWNERSHIP DISCIPLINES                         |
+------------------------------------------------------------------+
|                                                                  |
|   1. MEMORY OWNERSHIP                                            |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   RULE: Every allocation must have ONE clear owner         | |
|   |                                                            | |
|   |   PATTERN: Creator owns until explicit transfer            | |
|   |                                                            | |
|   |   // Thread A (producer)                                   | |
|   |   data_t* data = malloc(sizeof(data_t));  // A owns        | |
|   |   fill_data(data);                                         | |
|   |   enqueue(shared_queue, data);  // ownership transferred   | |
|   |   // A must NOT access data after this point               | |
|   |                                                            | |
|   |   // Thread B (consumer)                                   | |
|   |   data_t* data = dequeue(shared_queue);  // B now owns     | |
|   |   process(data);                                           | |
|   |   free(data);  // B frees                                  | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   2. LOCK OWNERSHIP                                              |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   RULE: Lock must be released by the same thread           | |
|   |         that acquired it (except for some special cases)   | |
|   |                                                            | |
|   |   CORRECT:                                                 | |
|   |   pthread_mutex_lock(&mutex);    // Thread A locks         | |
|   |   do_work();                                               | |
|   |   pthread_mutex_unlock(&mutex);  // Thread A unlocks       | |
|   |                                                            | |
|   |   WRONG:                                                   | |
|   |   // Thread A                     // Thread B              | |
|   |   pthread_mutex_lock(&mutex);     // waits...              | |
|   |   signal_thread_b();              // wakes up              | |
|   |   // exits without unlock         pthread_mutex_unlock();  | |
|   |                                   // UNDEFINED BEHAVIOR!   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   3. LIFETIME COUPLING                                           |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   RULE: Resources passed to threads must outlive           | |
|   |         the thread's use of them                           | |
|   |                                                            | |
|   |   memcached crawler.c example (lines 745-765):             | |
|   |                                                            | |
|   |   pthread_mutex_lock(&lru_crawler_lock);                   | |
|   |   do_run_lru_crawler_thread = 1;  // signal before create  | |
|   |   pthread_create(&tid, NULL, thread_func, NULL);           | |
|   |   pthread_cond_wait(&cond, &mutex);  // wait until ready   | |
|   |   pthread_mutex_unlock(&mutex);                            | |
|   |   // Thread is now guaranteed to be running                | |
|   |   // Main thread can safely proceed                        | |
|   |                                                            | |
|   |   The "lock dance" ensures proper lifecycle coordination   | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
pthreads 不提供自动的资源管理，程序员必须手动维护三种所有权规则：

1. **内存所有权**：每个分配必须有唯一的所有者。在多线程间传递数据时，必须明确所有权转移点。生产者将数据放入队列后，不应再访问该数据；消费者取出数据后负责释放。

2. **锁所有权**：锁必须由获取它的同一线程释放。一个线程加锁、另一个线程解锁是未定义行为。

3. **生命周期耦合**：传递给线程的资源必须比线程的使用周期更长。memcached 中的「lock dance」模式（见代码 745-765 行）就是确保线程正确启动后主线程才继续执行的经典模式。

---

## Composing with Signals, I/O, and fork/exec

```
+------------------------------------------------------------------+
|                    PTHREADS + SIGNALS                            |
+------------------------------------------------------------------+
|                                                                  |
|   PROBLEM: Signals + threads = complexity                        |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Signal arrives --> Which thread handles it?              | |
|   |                                                            | |
|   |   PROCESS-DIRECTED SIGNALS (SIGINT, SIGTERM, SIGKILL):     | |
|   |   - Delivered to ANY thread that doesn't block it          | |
|   |   - Usually the first thread found                         | |
|   |                                                            | |
|   |   THREAD-DIRECTED SIGNALS (pthread_kill):                  | |
|   |   - Delivered to specific thread                           | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   RECOMMENDED PATTERN: Dedicated signal-handling thread          |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   main() {                                                 | |
|   |       sigset_t set;                                        | |
|   |       sigfillset(&set);                                    | |
|   |       pthread_sigmask(SIG_BLOCK, &set, NULL);  // block all| |
|   |                                                            | |
|   |       // All spawned threads inherit blocked signals       | |
|   |       pthread_create(&worker1, ...);                       | |
|   |       pthread_create(&worker2, ...);                       | |
|   |                                                            | |
|   |       // Dedicated thread for signals                      | |
|   |       pthread_create(&sig_thread, signal_handler_thread);  | |
|   |   }                                                        | |
|   |                                                            | |
|   |   void* signal_handler_thread(void* arg) {                 | |
|   |       sigset_t set;                                        | |
|   |       sigemptyset(&set);                                   | |
|   |       sigaddset(&set, SIGINT);                             | |
|   |       sigaddset(&set, SIGTERM);                            | |
|   |                                                            | |
|   |       while (1) {                                          | |
|   |           int sig;                                         | |
|   |           sigwait(&set, &sig);  // synchronous wait        | |
|   |           handle_signal(sig);   // no async-signal-safety  | |
|   |       }                                                    | |
|   |   }                                                        | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                    PTHREADS + I/O                                |
+------------------------------------------------------------------+
|                                                                  |
|   FILE DESCRIPTORS: Shared across all threads!                   |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread A: fd = open("/file", O_RDWR);                    | |
|   |   Thread B: read(fd, buf, 100);   // Uses same fd!         | |
|   |   Thread C: close(fd);            // Closes for everyone!  | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   PITFALLS:                                                      |
|   +------------------------------------------------------------+ |
|   |   1. File position is SHARED                               | |
|   |      - Two threads reading sequentially will interleave    | |
|   |      - Use pread/pwrite for independent positions          | |
|   |                                                            | |
|   |   2. close() in one thread affects all                     | |
|   |      - Must coordinate fd lifecycle                        | |
|   |                                                            | |
|   |   3. Blocking I/O blocks ONE thread                        | |
|   |      - Other threads continue (unlike event loop)          | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                    PTHREADS + FORK                               |
+------------------------------------------------------------------+
|                                                                  |
|   DANGER: fork() in multithreaded process                        |
|                                                                  |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   BEFORE FORK:                 AFTER FORK:                 | |
|   |   +-----------+                +-----------+ +-----------+ | |
|   |   | Parent    |                | Parent    | | Child     | | |
|   |   | Thread 1  |     fork()     | Thread 1  | | Thread 1  | | |
|   |   | Thread 2  |  --------->    | Thread 2  | | (ONLY!)   | | |
|   |   | Thread 3  |                | Thread 3  | |           | | |
|   |   +-----------+                +-----------+ +-----------+ | |
|   |                                                            | |
|   |   Child process has ONLY the thread that called fork()    | |
|   |   BUT it inherits ALL mutexes in their current state!     | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   DEADLOCK SCENARIO:                                             |
|   +------------------------------------------------------------+ |
|   |                                                            | |
|   |   Thread 2 holds mutex M                                   | |
|   |   Thread 1 calls fork()                                    | |
|   |   Child inherits mutex M (still "locked" by Thread 2)      | |
|   |   Thread 2 doesn't exist in child!                         | |
|   |   Child tries to lock M --> DEADLOCK forever               | |
|   |                                                            | |
|   +------------------------------------------------------------+ |
|                                                                  |
|   SOLUTION:                                                      |
|   +------------------------------------------------------------+ |
|   |   1. Only call fork() before creating threads              | |
|   |   2. Use pthread_atfork() to lock/unlock around fork       | |
|   |   3. Immediately call exec() after fork (safest)           | |
|   +------------------------------------------------------------+ |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**  
**pthreads 与信号**：信号与多线程的交互很复杂。进程级信号（如 SIGINT）会被投递到任意一个没有阻塞该信号的线程。推荐模式：主线程阻塞所有信号，创建一个专门的信号处理线程使用 sigwait() 同步等待信号。

**pthreads 与 I/O**：文件描述符在所有线程间共享。一个线程关闭 fd 会影响所有线程。文件偏移量也是共享的，两个线程同时读写会互相干扰。使用 pread/pwrite 可以避免共享偏移量问题。

**pthreads 与 fork**：这是最危险的组合！fork() 只复制调用线程到子进程，但子进程继承了所有互斥锁的状态。如果其他线程持有锁，子进程中没有对应的线程来解锁，会导致永久死锁。解决方案：只在创建线程之前 fork，或者 fork 后立即 exec。
