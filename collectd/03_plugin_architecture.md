# collectd Plugin Architecture

## Part 3: The Plugin Abstraction Layer

### 3.1 Plugin Types and Their Contracts

collectd defines several callback types, each with specific responsibilities:

| Plugin Type | Purpose | Callback Signature | When Called |
|------------|---------|-------------------|-------------|
| **config** | Parse plugin config | `int (*)(const char *key, const char *val)` | At config parse time |
| **complex_config** | Parse nested config | `int (*)(oconfig_item_t *)` | At config parse time |
| **init** | One-time setup | `int (*)(void)` | Before main loop starts |
| **read** | Produce metrics | `int (*)(void)` or `int (*)(user_data_t *)` | Periodically by read threads |
| **write** | Consume metrics | `int (*)(const data_set_t *, const value_list_t *, user_data_t *)` | For each dispatched value |
| **flush** | Force write | `int (*)(cdtime_t timeout, const char *id, user_data_t *)` | On flush command |
| **missing** | Handle stale data | `int (*)(const value_list_t *, user_data_t *)` | When metric times out |
| **cache_event** | React to cache changes | `int (*)(cache_event_t *, user_data_t *)` | On cache NEW/UPDATE/EXPIRED |
| **shutdown** | Cleanup | `int (*)(void)` | During shutdown |
| **log** | Handle log messages | `void (*)(int severity, const char *msg, user_data_t *)` | On log events |
| **notification** | Handle alerts | `int (*)(const notification_t *, user_data_t *)` | On notifications |

**插件类型说明（中文解释）：**

1. **config/complex_config**：配置解析回调，在配置文件加载时调用
2. **init**：初始化回调，用于分配资源、打开文件句柄等
3. **read**：读取回调，周期性调用以采集指标数据
4. **write**：写入回调，每当有新数据时调用，负责持久化
5. **flush**：刷新回调，强制将缓冲区数据写入后端
6. **missing**：缺失回调，当某个指标超时未更新时调用
7. **shutdown**：关闭回调，释放资源、关闭连接
8. **log**：日志回调，自定义日志处理（如写入 syslog）
9. **notification**：通知回调，处理告警事件

---

### 3.2 Plugin Registration APIs

```c
// From src/daemon/plugin.h

// Configuration callbacks
int plugin_register_config(const char *name,
                           int (*callback)(const char *key, const char *val),
                           const char **keys, int keys_num);
int plugin_register_complex_config(const char *type,
                                   int (*callback)(oconfig_item_t *));

// Lifecycle callbacks
int plugin_register_init(const char *name, plugin_init_cb callback);
int plugin_register_shutdown(const char *name, plugin_shutdown_cb callback);

// Read callbacks (two variants)
int plugin_register_read(const char *name, int (*callback)(void));
int plugin_register_complex_read(const char *group, const char *name,
                                 plugin_read_cb callback, cdtime_t interval,
                                 user_data_t const *user_data);

// Write/flush callbacks
int plugin_register_write(const char *name, plugin_write_cb callback,
                          user_data_t const *user_data);
int plugin_register_flush(const char *name, plugin_flush_cb callback,
                          user_data_t const *user_data);

// Other callbacks
int plugin_register_missing(const char *name, plugin_missing_cb callback,
                            user_data_t const *user_data);
int plugin_register_log(const char *name, plugin_log_cb callback,
                        user_data_t const *user_data);
int plugin_register_notification(const char *name,
                                 plugin_notification_cb callback,
                                 user_data_t const *user_data);
```

**注册 API 设计原则（中文解释）：**

1. **函数指针 ABI**：所有回调都是 C 函数指针，保证跨编译器兼容
2. **用户数据**：`user_data_t` 允许传递插件私有状态
3. **自动内存管理**：`user_data.free_func` 在注销时自动调用
4. **名称唯一性**：每个注册名只能有一个回调

---

### 3.3 The module_register Entry Point

Every plugin must export a `module_register` function:

```c
// This is called by dlopen() via plugin_load()
void module_register(void);
```

This is the **only required export** from a plugin. The function should register all callbacks the plugin needs:

```c
// Example: src/load.c
void module_register(void) {
  plugin_register_config("load", load_config, config_keys, config_keys_num);
  plugin_register_read("load", load_read);
}

// Example: src/write_log.c
void module_register(void) {
  plugin_register_complex_config("write_log", wl_config);
  plugin_register_write("write_log", wl_write, NULL);
}
```

**module_register 机制（中文解释）：**

当 `plugin_load()` 加载插件时：
1. 调用 `dlopen()` 加载 `.so` 文件
2. 调用 `dlsym()` 查找 `module_register` 符号
3. 调用 `module_register()`，插件在其中注册回调
4. 所有回调被添加到全局链表/堆中

这种设计避免了：
- 复杂的对象系统
- 虚函数表
- C++ 异常处理

---

### 3.4 Simple vs. Complex Read Callbacks

collectd supports two read callback styles:

```c
// Style 1: Simple read (no user data)
int simple_read(void) {
  // Read some data
  value_list_t vl = VALUE_LIST_INIT;
  // Fill vl...
  plugin_dispatch_values(&vl);
  return 0;
}

// Registration:
plugin_register_read("my_plugin", simple_read);
```

```c
// Style 2: Complex read (with user data and custom interval)
int complex_read(user_data_t *ud) {
  my_state_t *state = (my_state_t *)ud->data;
  // Use state...
  return 0;
}

// Registration:
user_data_t ud = {
  .data = my_state,
  .free_func = my_state_free  // Called on unregister
};
plugin_register_complex_read(
  "my_group",           // Group name for batch unregister
  "my_plugin",          // Callback name
  complex_read,         // Callback function
  DOUBLE_TO_CDTIME_T(5.0),  // Custom interval (5 seconds)
  &ud                   // User data (copied internally)
);
```

**简单 vs 复杂读回调（中文解释）：**

| 特性 | 简单回调 | 复杂回调 |
|-----|---------|---------|
| 函数签名 | `int (*)(void)` | `int (*)(user_data_t *)` |
| 状态管理 | 全局变量 | `user_data_t` 封装 |
| 自定义间隔 | 不支持 | 支持 |
| 分组注销 | 不支持 | 支持 (`group` 参数) |
| 适用场景 | 简单插件 | 多实例、有状态插件 |

推荐使用复杂回调，因为：
1. 支持多实例（同一插件多个配置块）
2. 状态封装更清晰
3. 内存管理自动化

---

### 3.5 Callback Registration Internals

```c
// From src/daemon/plugin.c

// Callback wrapper structure
struct callback_func_s {
  void *cf_callback;      // Function pointer (cast to actual type)
  user_data_t cf_udata;   // User data (deep copy)
  plugin_ctx_t cf_ctx;    // Context at registration time
};

// Registration flow
int create_register_callback(llist_t **list, const char *name,
                             void *callback, user_data_t const *ud) {
  callback_func_t *cf = calloc(1, sizeof(*cf));
  cf->cf_callback = callback;
  if (ud != NULL) {
    cf->cf_udata = *ud;  // Shallow copy (caller owns data)
  }
  cf->cf_ctx = plugin_get_ctx();  // Capture current context!
  
  return register_callback(list, name, cf);
}
```

**关键设计点**：注册时捕获当前的 `plugin_ctx`，这样在回调执行时可以恢复正确的上下文（包括插件名和间隔设置）。

**回调注册内部机制（中文解释）：**

回调注册时发生的事情：
1. 分配 `callback_func_t` 结构
2. 保存函数指针
3. 复制 `user_data`（浅拷贝，数据由调用者管理）
4. **捕获当前插件上下文**（这点至关重要！）
5. 添加到相应的回调链表

当回调被调用时：
1. 保存当前上下文
2. 恢复注册时的上下文
3. 执行回调
4. 恢复原来的上下文

---

### 3.6 Plugin Context (plugin_ctx_t)

The plugin context is thread-local storage that provides:

```c
struct plugin_ctx_s {
  char *name;              // Current plugin name
  cdtime_t interval;       // Collection interval
  cdtime_t flush_interval; // Auto-flush interval  
  cdtime_t flush_timeout;  // Flush timeout
};

// API
void plugin_init_ctx(void);
plugin_ctx_t plugin_get_ctx(void);
plugin_ctx_t plugin_set_ctx(plugin_ctx_t ctx);
cdtime_t plugin_get_interval(void);
```

Context is used for:
1. **Logging**: `daemon_log()` uses `ctx.name` to prefix messages
2. **Interval inheritance**: `plugin_dispatch_values()` uses `ctx.interval`
3. **Flush scheduling**: When registering flush callbacks

```c
// Context switching during callback execution
void execute_callback(callback_func_t *cf) {
  plugin_ctx_t old_ctx = plugin_set_ctx(cf->cf_ctx);
  
  // Inside callback, plugin_get_ctx() returns cf->cf_ctx
  cf->cf_callback(...);
  
  plugin_set_ctx(old_ctx);  // Restore previous context
}
```

**插件上下文机制（中文解释）：**

插件上下文使用**线程本地存储**（pthread_key_t）实现，每个线程有独立的上下文副本。

主要用途：
1. **日志前缀**：`P_ERROR()` 等宏会自动添加插件名
2. **间隔继承**：`value_list` 自动获取当前插件的间隔
3. **正确的属性传播**：确保写插件知道是哪个读插件产生的数据

上下文切换过程：
```
读线程执行 cpu 插件读回调：
  1. 保存当前上下文（可能是空或其他插件）
  2. 设置 ctx.name = "cpu", ctx.interval = 10s
  3. 执行 cpu_read()
  4. cpu_read() 调用 plugin_dispatch_values()
     - value_list.interval 自动设为 10s
  5. 恢复之前的上下文
```

---

### 3.7 Why C and Function Pointers (Not C++ Objects)

collectd deliberately avoids C++ and object-oriented patterns:

| Design Choice | Rationale |
|--------------|-----------|
| C function pointers | Stable ABI across compilers |
| No vtables | No binary compatibility issues |
| No exceptions | Predictable control flow |
| No RTTI | Smaller binary, no hidden costs |
| Explicit memory | Clear ownership semantics |

This enables:
- Plugins compiled with different compilers
- Plugins using different C++ standard library versions
- Easier debugging (no hidden code generation)
- Portable to embedded systems

**为什么使用 C 而非 C++（中文解释）：**

collectd 选择纯 C 的原因：

1. **ABI 稳定性**：C 函数调用约定在所有编译器间一致，而 C++ 的名称修饰和虚函数表可能不兼容

2. **二进制兼容**：插件可以用不同版本的编译器编译，只要遵循 C ABI

3. **可预测性**：
   - 没有异常（控制流清晰）
   - 没有 RTTI（无隐藏开销）
   - 没有构造函数/析构函数的隐式调用

4. **可移植性**：可以在嵌入式系统上运行

5. **调试友好**：gdb 调试更直观

---

### 3.8 Callback List Management

```c
// Internal callback storage uses linked lists
static llist_t *list_init;      // Init callbacks
static llist_t *list_write;     // Write callbacks
static llist_t *list_flush;     // Flush callbacks
static llist_t *list_shutdown;  // Shutdown callbacks
static llist_t *list_log;       // Log callbacks
static llist_t *list_notification;  // Notification callbacks

// Read callbacks use a min-heap for scheduling
static c_heap_t *read_heap;
static llist_t *read_list;  // For lookup by name
```

```
Callback Storage Layout:
========================

list_write:
+-------+    +-------+    +-------+
| name  | -> | name  | -> | name  |
| "rrd" |    |"kafka"|    | "log" |
+-------+    +-------+    +-------+
| cb_fn |    | cb_fn |    | cb_fn |
| udata |    | udata |    | udata |
| ctx   |    | ctx   |    | ctx   |
+-------+    +-------+    +-------+

read_heap (min-heap by next_read time):
         [cpu: 10:00:05]
        /              \
[disk: 10:00:08]  [load: 10:00:10]
      /
[mem: 10:00:15]
```

**回调存储结构（中文解释）：**

不同类型的回调使用不同的存储结构：

1. **链表**（init、write、flush、shutdown、log、notification）：
   - 按注册顺序遍历
   - O(n) 查找，但回调数量通常很少

2. **最小堆**（read）：
   - 按 `rf_next_read` 时间排序
   - O(log n) 插入和取出
   - 保证下一个要执行的回调总在堆顶

3. **查找链表**（read_list）：
   - 用于按名称查找和注销
   - 与堆并行维护

---

### 3.9 Learning Outcomes

After reading this section, you should be able to:

- [ ] List all plugin callback types and their purposes
- [ ] Explain the difference between simple and complex read callbacks
- [ ] Describe what `module_register` does and when it's called
- [ ] Explain why plugin context is important
- [ ] Articulate why collectd uses C function pointers instead of C++ objects
- [ ] Describe how callbacks are stored and retrieved
