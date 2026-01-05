# collectd Process Model and Main Event Loop

## Part 2: Execution Flow and Thread Management

### 2.1 Main Function and Initialization Sequence

The `main()` function in collectd follows a clear initialization → run → shutdown lifecycle:

```
main() [src/daemon/cmd.c]
│
├── init_config(argc, argv)
│   ├── read_cmdline()           // Parse -C, -t, -T, -P, -f, -B flags
│   ├── plugin_init_ctx()        // Initialize thread-local plugin context
│   └── configure_collectd()
│       ├── cf_read(configfile)  // Parse collectd.conf
│       │   └── dispatch_loadplugin()  // Load each plugin .so
│       │       └── plugin_load()
│       │           └── dlopen() + module_register()
│       ├── change_basedir()     // chdir to BaseDir
│       └── init_global_variables()
│           ├── interval_g = cf_get_default_interval()
│           ├── timeout_g = atoi(global_option_get("Timeout"))
│           └── hostname_g = init_hostname()
│
├── daemonize() [if COLLECT_DAEMON && !-f]
│   ├── fork() twice
│   ├── setsid()
│   └── redirect stdin/stdout/stderr
│
└── run_loop(test_readall, notify_func)
    ├── do_init()
    │   ├── uc_init()                // Initialize value cache
    │   ├── plugin_init_all()        // Call all init callbacks
    │   │   └── for each registered init callback:
    │   │       callback()
    │   ├── start_write_threads(N)   // Create write thread pool
    │   └── start_read_threads(N)    // Create read thread pool
    │
    ├── do_loop()                    // Main event loop
    │   └── while (loop == 0):
    │       ├── plugin_read_all()    // Check cache timeouts
    │       ├── calculate wait_until
    │       └── nanosleep(interval)
    │
    └── do_shutdown()
        └── plugin_shutdown_all()
            ├── stop_read_threads()
            ├── stop_write_threads()
            ├── plugin_flush(NULL, 0, NULL)
            └── for each shutdown callback:
                callback()
```

**初始化流程（中文解释）：**
collectd 的启动过程分为以下几个阶段：
1. **命令行解析**：处理 `-C`（配置文件）、`-f`（前台运行）等参数
2. **配置加载**：解析 `collectd.conf`，加载插件动态库（`.so` 文件）
3. **守护进程化**：fork 两次创建后台进程，重定向标准输入输出
4. **初始化**：调用所有插件的 init 回调，创建读写线程池
5. **主循环**：周期性触发读取操作，处理超时
6. **关闭**：优雅地停止线程，刷新缓冲区，调用插件清理回调

---

### 2.2 The Main Event Loop (do_loop)

```c
// From src/daemon/collectd.c:268-301
static int do_loop(void) {
  cdtime_t interval = cf_get_default_interval();
  cdtime_t wait_until = cdtime() + interval;

  while (loop == 0) {
    /* Issue all plugins */
    plugin_read_all();   // Actually just checks cache timeouts!

    cdtime_t now = cdtime();
    if (now >= wait_until) {
      WARNING("Not sleeping because next interval is in the past!");
      wait_until = now + interval;
      continue;
    }

    struct timespec ts_wait = CDTIME_T_TO_TIMESPEC(wait_until - now);
    wait_until = wait_until + interval;

    while ((loop == 0) && (nanosleep(&ts_wait, &ts_wait) != 0)) {
      if (errno != EINTR) {
        ERROR("nanosleep failed: %s", STRERRNO);
        return -1;
      }
    }
  }
  return 0;
}
```

**重要澄清**：`plugin_read_all()` 的名字具有误导性。它实际上只是调用 `uc_check_timeout()` 来检查过期的缓存条目。真正的读取操作由独立的读线程执行！

**主循环解析（中文解释）：**
主循环的核心职责是**维持全局心跳**。它并不直接执行读取操作（那是读线程的工作），而是：
1. 周期性调用 `plugin_read_all()` 检查缓存超时
2. 计算下一个唤醒时间点
3. 使用 `nanosleep` 精确睡眠到下一个间隔
4. 处理信号中断（`EINTR`）以支持优雅停止

---

### 2.3 Thread Model

collectd uses multiple thread pools for concurrent execution:

```
+-------------------+     +-----------------+     +------------------+
| Main Thread       |     | Read Threads    |     | Write Threads    |
+-------------------+     +-----------------+     +------------------+
| - Initialization  |     | - plugin_read   |     | - plugin_write   |
| - Global timer    |     | - Heap-based    |     | - Queue-based    |
| - Cache timeout   |     |   scheduling    |     |   consumption    |
| - Signal handling |     | - Per-plugin    |     | - Parallel       |
+-------------------+     |   backoff       |     |   dispatch       |
        |                 +-----------------+     +------------------+
        |                        ^                       ^
        |                        |                       |
        v                        |                       |
   +----+------------------------+---+              +----+----+
   |        Shared State             |              |  Queue  |
   |  - read_heap (min-heap)         |              | write_  |
   |  - read_list (lookup)           |  --------->  | queue   |
   |  - cache_tree (AVL)             |   enqueue    +---------+
   +----------------------------------+
```

**线程模型（中文解释）：**
collectd 采用多线程模型来提高并发性能：

1. **主线程**：负责初始化、全局定时和信号处理
2. **读线程池**（默认 5 个）：
   - 使用**最小堆**调度下一个要执行的读回调
   - 每个读操作独立调度，互不阻塞
   - 失败时自动退避（间隔翻倍）
3. **写线程池**（默认 5 个）：
   - 使用**阻塞队列**消费待写数据
   - 支持背压机制（队列满时丢弃数据）

---

### 2.4 Read Thread Implementation

```c
// Simplified from src/daemon/plugin.c:450-603
static void *plugin_read_thread(void *args) {
  while (read_loop != 0) {
    read_func_t *rf;
    
    // Get next read function from heap (sorted by next_read time)
    pthread_mutex_lock(&read_lock);
    rf = c_heap_get_root(read_heap);
    if (rf == NULL) {
      pthread_cond_wait(&read_cond, &read_lock);
      pthread_mutex_unlock(&read_lock);
      continue;
    }
    pthread_mutex_unlock(&read_lock);

    // Sleep until it's time to execute
    pthread_mutex_lock(&read_lock);
    while (cdtime() < rf->rf_next_read)
      pthread_cond_timedwait(&read_cond, &read_lock, ...);
    pthread_mutex_unlock(&read_lock);

    // Execute the read callback
    if (rf->rf_type == RF_SIMPLE) {
      status = ((int (*)(void))rf->rf_callback)();
    } else {
      status = ((plugin_read_cb)rf->rf_callback)(&rf->rf_udata);
    }

    // Handle failure: exponential backoff
    if (status != 0) {
      rf->rf_effective_interval *= 2;
      if (rf->rf_effective_interval > max_read_interval)
        rf->rf_effective_interval = max_read_interval;
    } else {
      rf->rf_effective_interval = rf->rf_interval;
    }

    // Schedule next read
    rf->rf_next_read += rf->rf_effective_interval;
    c_heap_insert(read_heap, rf);
  }
  return NULL;
}
```

**读线程调度机制（中文解释）：**
读线程使用最小堆实现高效调度：
1. 从堆顶取出下一个要执行的读函数（`rf_next_read` 最小的）
2. 如果还没到执行时间，使用条件变量睡眠等待
3. 执行读回调（简单类型或复杂类型）
4. **失败退避**：如果读取失败，将间隔翻倍（最大 86400 秒）
5. 重新计算下次执行时间，插回堆中

这种设计的优势：
- O(log n) 的插入和取出操作
- 精确的时间调度
- 自动故障恢复

---

### 2.5 Write Thread and Queue

```c
// From src/daemon/plugin.c
struct write_queue_s {
  value_list_t *vl;        // Cloned value list
  plugin_ctx_t ctx;        // Caller's context
  write_queue_t *next;     // Linked list
};

// Producer side (called from read thread context)
static int plugin_write_enqueue(value_list_t const *vl) {
  write_queue_t *q = malloc(sizeof(*q));
  q->vl = plugin_value_list_clone(vl);  // Deep copy!
  q->ctx = plugin_get_ctx();
  
  pthread_mutex_lock(&write_lock);
  // Append to tail
  if (write_queue_tail == NULL) {
    write_queue_head = q;
  } else {
    write_queue_tail->next = q;
  }
  write_queue_tail = q;
  write_queue_length++;
  pthread_cond_signal(&write_cond);
  pthread_mutex_unlock(&write_lock);
  return 0;
}

// Consumer side (write thread)
static void *plugin_write_thread(void *args) {
  while (write_loop) {
    value_list_t *vl = plugin_write_dequeue();  // Blocks if empty
    if (vl == NULL) continue;
    
    plugin_dispatch_values_internal(vl);  // Call all write plugins
    plugin_value_list_free(vl);
  }
  return NULL;
}
```

**写队列机制（中文解释）：**
写操作采用生产者-消费者模式：

**生产者**（读线程调用 `plugin_dispatch_values`）：
1. **深拷贝** value_list（避免读线程释放后使用）
2. 保存调用者的插件上下文
3. 追加到队列尾部
4. 唤醒等待的写线程

**消费者**（写线程）：
1. 从队列头部取出数据
2. 恢复原始插件上下文
3. 调用所有注册的写插件
4. 释放 value_list 内存

**背压机制**：当队列长度超过 `WriteQueueLimitHigh` 时，开始按概率丢弃数据。

---

### 2.6 Graceful Shutdown Sequence

```
Signal (SIGTERM/SIGINT)
│
└── stop_collectd()  // Sets loop = 1
    │
    └── do_loop() exits
        │
        └── do_shutdown()
            │
            ├── plugin_shutdown_all()
            │   │
            │   ├── stop_read_threads()
            │   │   ├── read_loop = 0
            │   │   ├── pthread_cond_broadcast()
            │   │   └── pthread_join() for each thread
            │   │
            │   ├── stop_write_threads()
            │   │   ├── write_loop = false
            │   │   ├── pthread_cond_broadcast()
            │   │   ├── pthread_join() for each thread
            │   │   └── Free remaining queue items
            │   │
            │   ├── plugin_flush(NULL, 0, NULL)
            │   │   // Ask all plugins to flush
            │   │
            │   └── for each shutdown callback:
            │       callback()
            │
            └── Cleanup data structures
```

**优雅关闭流程（中文解释）：**
collectd 的关闭过程确保数据不丢失：
1. 设置 `loop` 标志，主循环退出
2. 停止所有读线程（广播条件变量，等待线程结束）
3. 停止所有写线程（处理队列中剩余数据）
4. 调用所有插件的 flush 回调（确保缓冲区写入）
5. 调用所有插件的 shutdown 回调（释放资源）
6. 清理内部数据结构

---

### 2.7 Key Data Structures

```c
// Thread-local plugin context
struct plugin_ctx_s {
  char *name;              // Plugin name
  cdtime_t interval;       // Collection interval
  cdtime_t flush_interval; // Auto-flush interval
  cdtime_t flush_timeout;  // Flush timeout
};

// Read function descriptor (in min-heap)
struct read_func_s {
  callback_func_t rf_super;        // Base callback info
  char rf_group[DATA_MAX_NAME_LEN]; // Plugin group
  char *rf_name;                   // Callback name
  int rf_type;                     // RF_SIMPLE or RF_COMPLEX
  cdtime_t rf_interval;            // Configured interval
  cdtime_t rf_effective_interval;  // Current interval (with backoff)
  cdtime_t rf_next_read;           // Next execution time
};
```

---

### 2.8 Learning Outcomes

After reading this section, you should be able to:

- [ ] Trace the complete execution flow from `main()` to shutdown
- [ ] Explain the role of each thread type (main, read, write)
- [ ] Describe how the read heap implements fair scheduling
- [ ] Understand the write queue's producer-consumer pattern
- [ ] Explain the exponential backoff mechanism for failed reads
- [ ] Describe the graceful shutdown sequence
