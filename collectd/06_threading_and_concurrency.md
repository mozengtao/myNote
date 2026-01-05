# collectd Threading Model and Concurrency

## Part 6: Thread Safety, Synchronization, and Plugin Author Responsibilities

### 6.1 Thread Architecture Overview

```
+-----------------+
|   Main Thread   |  (1 thread)
+-----------------+
| - Signal handler|
| - Cache timeout |
| - Global timer  |
+--------+--------+
         |
         |  spawns
         v
+--------+--------+--------+--------+--------+
|  Read Thread 0  |  Read Thread 1  |  ...   |  (default: 5 threads)
+-----------------+-----------------+--------+
| - plugin_read_thread()                      |
| - Heap-based scheduling                     |
| - Parallel execution of read callbacks      |
+--------------------+------------------------+
                     |
                     |  enqueue
                     v
            +------------------+
            |   Write Queue    |
            +------------------+
                     |
                     |  dequeue
                     v
+--------+--------+--------+--------+--------+
| Write Thread 0  | Write Thread 1  |  ...   |  (default: 5 threads)
+-----------------+-----------------+--------+
| - plugin_write_thread()                     |
| - Queue-based consumption                   |
| - Parallel execution of write callbacks     |
+---------------------------------------------+
```

**线程架构总览（中文解释）：**

collectd 使用三类线程：

1. **主线程**（1 个）：
   - 处理信号
   - 检查缓存超时
   - 维持全局心跳

2. **读线程池**（默认 5 个）：
   - 从最小堆获取下一个要执行的读回调
   - 并行执行多个读插件

3. **写线程池**（默认 5 个）：
   - 从队列消费数据
   - 并行调用写插件

---

### 6.2 What Runs in Parallel

```
Parallel Execution Example (5 read threads, 10 plugins):

Time  Thread-0   Thread-1   Thread-2   Thread-3   Thread-4
----  ---------  ---------  ---------  ---------  ---------
 0ms  cpu_read   mem_read   disk_read  net_read   load_read
50ms  cpu_done   mem_done   disk_work  net_work   load_done
                                          |          |
100ms swap_read  df_read    disk_done  net_done   vmem_read
                                          |
150ms swap_done  df_work    proc_read  uptime_rd  vmem_done
                     |
200ms [idle]     df_done    proc_done  uptime_dn  [idle]
```

**Key Points**:
1. Multiple read plugins execute **concurrently**
2. A single read plugin instance runs on **one thread at a time**
3. Same plugin can run on different threads across intervals
4. Write plugins are called in parallel for different value lists

**并行执行规则（中文解释）：**

**同时运行的**：
- 不同插件的读回调（cpu 和 memory 可以同时执行）
- 不同数据的写操作（写 cpu 和写 disk 的数据可以同时进行）

**不会同时运行的**：
- 同一个插件的同一个读回调（不会有两个线程同时执行 `cpu_read`）
- 这是因为读函数从堆中取出后才执行，执行完才放回堆

---

### 6.3 Serialization Points

```c
// Read scheduling is serialized through the read_lock mutex
static pthread_mutex_t read_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_cond = PTHREAD_COND_INITIALIZER;

// Write queue access is serialized through write_lock
static pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t write_cond = PTHREAD_COND_INITIALIZER;

// Cache access is serialized through cache_lock
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
```

```
Lock Acquisition Order (to prevent deadlocks):
==============================================

1. read_lock  - For read heap/list access
2. write_lock - For write queue access  
3. cache_lock - For cache tree access

These locks should NOT be held simultaneously.
If you need multiple, acquire in the order above.
```

**同步点（中文解释）：**

collectd 使用三个主要锁：

| 锁 | 保护的资源 | 持有时机 |
|----|-----------|---------|
| `read_lock` | 读堆和读列表 | 取出/放回读函数时 |
| `write_lock` | 写队列 | 入队/出队时 |
| `cache_lock` | 缓存树 | 读写缓存时 |

**关键设计**：回调执行时**不持有锁**，这意味着：
- 读回调可以长时间运行而不阻塞其他操作
- 写回调可以安全地访问缓存（通过 `uc_*` API）

---

### 6.4 Plugin Author Responsibilities

#### 6.4.1 Thread Safety Requirements

```c
// WRONG: Global state without synchronization
static int counter = 0;

int my_read(void) {
  counter++;  // Data race if called from multiple threads!
  // ...
}

// RIGHT: Use user_data for per-callback state
typedef struct {
  pthread_mutex_t lock;
  int counter;
} my_state_t;

int my_read(user_data_t *ud) {
  my_state_t *state = ud->data;
  pthread_mutex_lock(&state->lock);
  state->counter++;
  pthread_mutex_unlock(&state->lock);
  // ...
}
```

**插件作者的线程安全责任（中文解释）：**

collectd **不保护插件内部状态**。插件作者必须：

1. **避免全局可变状态**：使用 `user_data_t` 封装状态
2. **如果必须使用全局状态**：自己加锁
3. **知道回调可能并发执行**：多实例插件尤其需要注意

---

#### 6.4.2 What NOT to Do Inside Callbacks

```c
// DON'T: Block for a long time
int bad_read(void) {
  sleep(60);  // Blocks a read thread for 60 seconds!
  return 0;
}

// DON'T: Call blocking I/O without timeout
int bad_read(void) {
  // This could hang forever if server is unresponsive
  recv(socket, buffer, size, 0);
  return 0;
}

// DON'T: Acquire locks and then call collectd APIs
int bad_read(void) {
  pthread_mutex_lock(&my_lock);
  plugin_dispatch_values(&vl);  // May acquire other locks - deadlock risk!
  pthread_mutex_unlock(&my_lock);
}

// DO: Use timeouts
int good_read(void) {
  struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  int ret = recv(socket, buffer, size, 0);
  if (ret < 0 && errno == EAGAIN) {
    // Timeout, return and try again later
    return -1;
  }
}

// DO: Release locks before calling dispatch
int good_read(void) {
  pthread_mutex_lock(&my_lock);
  // ... prepare data ...
  value_list_t vl = ...;
  pthread_mutex_unlock(&my_lock);  // Release BEFORE dispatch
  
  plugin_dispatch_values(&vl);
}
```

**回调中的禁忌（中文解释）：**

| 禁止 | 原因 |
|------|------|
| 长时间阻塞 | 占用读/写线程，影响其他插件 |
| 无超时的网络 I/O | 可能永久阻塞线程 |
| 持有锁时调用 dispatch | 可能死锁（dispatch 会获取其他锁） |
| 调用 `exit()` 或 `abort()` | 不优雅的终止，数据丢失 |
| 大量内存分配 | 可能导致内存碎片或耗尽 |

---

### 6.5 Thread-Local Storage: Plugin Context

```c
// Plugin context is thread-local
static pthread_key_t plugin_ctx_key;

// Each thread has its own context
plugin_ctx_t plugin_get_ctx(void) {
  plugin_ctx_t *ctx = pthread_getspecific(plugin_ctx_key);
  if (ctx == NULL) {
    ctx = plugin_ctx_create();  // Create for this thread
  }
  return *ctx;
}

plugin_ctx_t plugin_set_ctx(plugin_ctx_t ctx) {
  plugin_ctx_t *c = pthread_getspecific(plugin_ctx_key);
  plugin_ctx_t old = *c;
  *c = ctx;
  return old;
}
```

```
Context Switching During Read:
==============================

Read Thread 0                    Read Thread 1
+---------------------------+    +---------------------------+
| pthread_getspecific()     |    | pthread_getspecific()     |
| -> ctx_thread_0           |    | -> ctx_thread_1           |
+---------------------------+    +---------------------------+
           |                                |
           v                                v
+---------------------------+    +---------------------------+
| Execute cpu_read()        |    | Execute mem_read()        |
| ctx.name = "cpu"          |    | ctx.name = "memory"       |
| ctx.interval = 10s        |    | ctx.interval = 30s        |
+---------------------------+    +---------------------------+
           |                                |
           v                                v
+---------------------------+    +---------------------------+
| plugin_dispatch_values()  |    | plugin_dispatch_values()  |
| vl.interval = 10s (auto)  |    | vl.interval = 30s (auto)  |
+---------------------------+    +---------------------------+
```

**线程本地存储（中文解释）：**

每个线程有独立的插件上下文副本：

1. 读线程执行 cpu 回调时：`ctx.name = "cpu"`, `ctx.interval = 10s`
2. 同时另一个读线程执行 memory：`ctx.name = "memory"`, `ctx.interval = 30s`
3. `plugin_dispatch_values()` 自动使用当前线程的上下文设置间隔

这种设计确保了：
- 并行执行时上下文不会混淆
- 每个 value_list 获得正确的间隔
- 日志输出正确的插件名

---

### 6.6 Safe Patterns for Complex Plugins

```c
// Pattern 1: Connection pooling with locking
typedef struct {
  pthread_mutex_t lock;
  void *connections[MAX_CONN];
  int conn_count;
} pool_t;

int my_read(user_data_t *ud) {
  pool_t *pool = ud->data;
  void *conn;
  
  // Acquire connection from pool
  pthread_mutex_lock(&pool->lock);
  conn = pool_get_connection(pool);
  pthread_mutex_unlock(&pool->lock);
  
  if (conn == NULL) {
    ERROR("No available connections");
    return -1;
  }
  
  // Use connection (no lock held)
  query_and_dispatch(conn);
  
  // Return connection to pool
  pthread_mutex_lock(&pool->lock);
  pool_return_connection(pool, conn);
  pthread_mutex_unlock(&pool->lock);
  
  return 0;
}

// Pattern 2: Atomic counters (C11)
#include <stdatomic.h>

static atomic_uint_fast64_t total_reads = 0;

int my_read(void) {
  atomic_fetch_add(&total_reads, 1);  // Thread-safe without mutex
  // ...
}

// Pattern 3: Per-thread state via complex_read
int my_read(user_data_t *ud) {
  // Each registration gets its own user_data
  // No synchronization needed if each instance is independent
  my_instance_t *instance = ud->data;
  return instance_read(instance);
}
```

**安全的并发模式（中文解释）：**

1. **连接池**：短暂持有锁获取连接，释放锁后使用连接
2. **原子操作**：使用 C11 原子类型避免锁
3. **每实例状态**：每个配置块创建独立实例，互不干扰

---

### 6.7 Debugging Concurrency Issues

```bash
# Use ThreadSanitizer (TSan) to detect data races
$ make clean
$ CFLAGS="-fsanitize=thread -g" ./configure
$ make
$ ./src/collectd -f  # TSan will report races

# Use Helgrind for lock order violations
$ valgrind --tool=helgrind ./src/collectd -f

# Use GDB for deadlock debugging
$ gdb -p $(pidof collectd)
(gdb) info threads
(gdb) thread 3
(gdb) bt  # See where the thread is stuck
```

**调试并发问题（中文解释）：**

| 工具 | 用途 |
|------|------|
| ThreadSanitizer | 检测数据竞争 |
| Helgrind | 检测锁顺序违规 |
| GDB | 调试死锁（查看线程堆栈） |

常见症状和原因：
- collectd 卡死 → 死锁
- 间歇性崩溃 → 数据竞争
- 数据不一致 → 缺少同步

---

### 6.8 Learning Outcomes

After reading this section, you should be able to:

- [ ] Describe the thread model and which parts run in parallel
- [ ] List the main synchronization primitives and their purposes
- [ ] Explain plugin author responsibilities for thread safety
- [ ] Identify patterns that can cause deadlocks or data races
- [ ] Use tools to debug concurrency issues
- [ ] Implement thread-safe plugins using appropriate patterns
