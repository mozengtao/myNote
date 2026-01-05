# Writing collectd Plugins: Step-by-Step Guide

## Part 9: Practical Plugin Development

### 9.1 Minimal Read Plugin (Template)

```c
/**
 * my_plugin.c - A minimal collectd read plugin
 */

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

static int my_read(void) {
  value_list_t vl = VALUE_LIST_INIT;
  value_t values[1];
  
  // Collect your metric
  double my_value = 42.0;  // Replace with actual data collection
  
  values[0].gauge = my_value;
  vl.values = values;
  vl.values_len = 1;
  
  sstrncpy(vl.plugin, "my_plugin", sizeof(vl.plugin));
  sstrncpy(vl.type, "gauge", sizeof(vl.type));
  // vl.host, vl.time, vl.interval are auto-filled
  
  plugin_dispatch_values(&vl);
  
  return 0;
}

void module_register(void) {
  plugin_register_read("my_plugin", my_read);
}
```

**最简读插件模板（中文解释）：**

这是一个最简单的读插件：
1. 定义读回调函数
2. 在 `module_register()` 中注册
3. 采集数据并调用 `plugin_dispatch_values()`

必须设置的字段：
- `plugin`：插件名
- `type`：类型名（必须在 types.db 中定义）
- `values` 和 `values_len`：值数组

---

### 9.2 Read Plugin with Configuration

```c
/**
 * my_plugin.c - Read plugin with configuration
 */

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

// Configuration state
static char *my_option = NULL;
static bool my_flag = false;

static int my_config(oconfig_item_t *ci) {
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    
    if (strcasecmp("Option", child->key) == 0) {
      if (cf_util_get_string(child, &my_option) != 0)
        return -1;
    }
    else if (strcasecmp("Flag", child->key) == 0) {
      if (cf_util_get_boolean(child, &my_flag) != 0)
        return -1;
    }
    else {
      ERROR("my_plugin: Unknown config option: %s", child->key);
      return -1;
    }
  }
  
  // Validate configuration
  if (my_option == NULL) {
    ERROR("my_plugin: `Option` is required");
    return -1;
  }
  
  return 0;
}

static int my_init(void) {
  // One-time initialization
  INFO("my_plugin: Initialized with option=%s, flag=%s",
       my_option, my_flag ? "true" : "false");
  return 0;
}

static int my_read(void) {
  value_list_t vl = VALUE_LIST_INIT;
  value_t values[1];
  
  // Use configuration
  if (my_flag) {
    // Different behavior based on flag
  }
  
  values[0].gauge = 42.0;
  vl.values = values;
  vl.values_len = 1;
  
  sstrncpy(vl.plugin, "my_plugin", sizeof(vl.plugin));
  sstrncpy(vl.type, "gauge", sizeof(vl.type));
  
  plugin_dispatch_values(&vl);
  return 0;
}

static int my_shutdown(void) {
  // Cleanup
  sfree(my_option);
  return 0;
}

void module_register(void) {
  plugin_register_complex_config("my_plugin", my_config);
  plugin_register_init("my_plugin", my_init);
  plugin_register_read("my_plugin", my_read);
  plugin_register_shutdown("my_plugin", my_shutdown);
}
```

**带配置的读插件（中文解释）：**

增加了：
1. **复杂配置回调**：解析 `<Plugin my_plugin>` 块
2. **初始化回调**：在主循环开始前执行一次
3. **关闭回调**：释放资源

配置文件示例：
```apache
<Plugin my_plugin>
  Option "some_value"
  Flag true
</Plugin>
```

---

### 9.3 Stateful Write Plugin

```c
/**
 * write_my_backend.c - Stateful write plugin
 */

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

typedef struct {
  char *host;
  int port;
  void *connection;
  pthread_mutex_t lock;
  char *buffer;
  size_t buffer_size;
  size_t buffer_fill;
} my_context_t;

static my_context_t *ctx = NULL;

static int my_connect(my_context_t *c) {
  if (c->connection != NULL)
    return 0;
  
  c->connection = connect_to_backend(c->host, c->port);
  if (c->connection == NULL) {
    ERROR("write_my_backend: Connection failed");
    return -1;
  }
  
  return 0;
}

static int my_write(const data_set_t *ds, const value_list_t *vl,
                    user_data_t __attribute__((unused)) *ud) {
  if (ctx == NULL)
    return -1;
  
  pthread_mutex_lock(&ctx->lock);
  
  // Lazy connect
  if (my_connect(ctx) != 0) {
    pthread_mutex_unlock(&ctx->lock);
    return -1;
  }
  
  // Format data
  char line[512];
  int len = format_line(line, sizeof(line), ds, vl);
  
  // Buffer or send
  if (ctx->buffer_fill + len > ctx->buffer_size) {
    // Flush buffer first
    send_data(ctx->connection, ctx->buffer, ctx->buffer_fill);
    ctx->buffer_fill = 0;
  }
  
  memcpy(ctx->buffer + ctx->buffer_fill, line, len);
  ctx->buffer_fill += len;
  
  pthread_mutex_unlock(&ctx->lock);
  return 0;
}

static int my_flush(cdtime_t timeout,
                    const char __attribute__((unused)) *identifier,
                    user_data_t __attribute__((unused)) *ud) {
  if (ctx == NULL)
    return -1;
  
  pthread_mutex_lock(&ctx->lock);
  
  if (ctx->buffer_fill > 0 && ctx->connection != NULL) {
    send_data(ctx->connection, ctx->buffer, ctx->buffer_fill);
    ctx->buffer_fill = 0;
  }
  
  pthread_mutex_unlock(&ctx->lock);
  return 0;
}

static int my_config(oconfig_item_t *ci) {
  ctx = calloc(1, sizeof(*ctx));
  if (ctx == NULL)
    return -1;
  
  pthread_mutex_init(&ctx->lock, NULL);
  ctx->buffer_size = 65536;
  ctx->buffer = malloc(ctx->buffer_size);
  
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    
    if (strcasecmp("Host", child->key) == 0)
      cf_util_get_string(child, &ctx->host);
    else if (strcasecmp("Port", child->key) == 0)
      cf_util_get_int(child, &ctx->port);
  }
  
  return 0;
}

static int my_shutdown(void) {
  if (ctx == NULL)
    return 0;
  
  pthread_mutex_lock(&ctx->lock);
  
  // Final flush
  if (ctx->buffer_fill > 0 && ctx->connection != NULL) {
    send_data(ctx->connection, ctx->buffer, ctx->buffer_fill);
  }
  
  // Close connection
  if (ctx->connection != NULL) {
    close_connection(ctx->connection);
  }
  
  pthread_mutex_unlock(&ctx->lock);
  pthread_mutex_destroy(&ctx->lock);
  
  sfree(ctx->host);
  sfree(ctx->buffer);
  sfree(ctx);
  ctx = NULL;
  
  return 0;
}

void module_register(void) {
  plugin_register_complex_config("write_my_backend", my_config);
  plugin_register_write("write_my_backend", my_write, NULL);
  plugin_register_flush("write_my_backend", my_flush, NULL);
  plugin_register_shutdown("write_my_backend", my_shutdown);
}
```

**有状态写插件（中文解释）：**

关键设计点：

1. **连接管理**：
   - 延迟连接（首次写入时）
   - 自动重连（连接丢失时）

2. **缓冲机制**：
   - 累积数据直到缓冲区满
   - flush 回调强制发送

3. **线程安全**：
   - 互斥锁保护状态
   - 写线程可能并发调用

4. **资源清理**：
   - shutdown 回调中刷新缓冲区
   - 释放所有内存和连接

---

### 9.4 Multi-Instance Plugin

```c
/**
 * my_multi_plugin.c - Multi-instance read plugin
 */

#include "collectd.h"
#include "plugin.h"
#include "utils/common/common.h"

typedef struct {
  char *name;
  char *host;
  int port;
} instance_t;

static void instance_free(void *arg) {
  instance_t *inst = arg;
  if (inst == NULL)
    return;
  
  sfree(inst->name);
  sfree(inst->host);
  sfree(inst);
}

static int instance_read(user_data_t *ud) {
  instance_t *inst = ud->data;
  
  // Collect data from this specific instance
  double value = query_server(inst->host, inst->port);
  
  value_list_t vl = VALUE_LIST_INIT;
  value_t values[1] = { { .gauge = value } };
  
  vl.values = values;
  vl.values_len = 1;
  
  sstrncpy(vl.plugin, "my_multi_plugin", sizeof(vl.plugin));
  sstrncpy(vl.plugin_instance, inst->name, sizeof(vl.plugin_instance));
  sstrncpy(vl.type, "gauge", sizeof(vl.type));
  
  plugin_dispatch_values(&vl);
  return 0;
}

static int parse_instance(oconfig_item_t *ci) {
  if (ci->values_num != 1 || ci->values[0].type != OCONFIG_TYPE_STRING) {
    ERROR("my_multi_plugin: Instance needs exactly one name");
    return -1;
  }
  
  instance_t *inst = calloc(1, sizeof(*inst));
  if (inst == NULL)
    return ENOMEM;
  
  inst->name = strdup(ci->values[0].value.string);
  
  // Default values
  inst->port = 8080;
  
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    
    if (strcasecmp("Host", child->key) == 0)
      cf_util_get_string(child, &inst->host);
    else if (strcasecmp("Port", child->key) == 0)
      cf_util_get_int(child, &inst->port);
    else {
      ERROR("my_multi_plugin: Unknown option: %s", child->key);
      instance_free(inst);
      return -1;
    }
  }
  
  if (inst->host == NULL) {
    ERROR("my_multi_plugin: Instance %s: Host is required", inst->name);
    instance_free(inst);
    return -1;
  }
  
  // Register read callback with unique name
  user_data_t ud = {
    .data = inst,
    .free_func = instance_free
  };
  
  char callback_name[256];
  snprintf(callback_name, sizeof(callback_name),
           "my_multi_plugin/%s", inst->name);
  
  return plugin_register_complex_read(
    NULL,            // group
    callback_name,   // unique callback name
    instance_read,   // callback
    0,               // interval (0 = default)
    &ud              // user_data
  );
}

static int my_config(oconfig_item_t *ci) {
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    
    if (strcasecmp("Instance", child->key) == 0) {
      if (parse_instance(child) != 0)
        return -1;
    }
    else {
      ERROR("my_multi_plugin: Unknown option: %s", child->key);
      return -1;
    }
  }
  
  return 0;
}

void module_register(void) {
  plugin_register_complex_config("my_multi_plugin", my_config);
}
```

配置示例：
```apache
<Plugin my_multi_plugin>
  <Instance "server1">
    Host "10.0.0.1"
    Port 8080
  </Instance>
  <Instance "server2">
    Host "10.0.0.2"
    Port 9090
  </Instance>
</Plugin>
```

**多实例插件（中文解释）：**

每个 `<Instance>` 块创建：
1. 独立的 `instance_t` 结构
2. 独立的读回调（使用唯一名称）
3. 独立的 `user_data_t`

`plugin_instance` 字段用于区分不同实例的数据：
- `host/my_multi_plugin-server1/gauge`
- `host/my_multi_plugin-server2/gauge`

---

### 9.5 Memory Ownership Rules

```
Rule 1: Stack-allocated value_list is safe
=========================================
int my_read(void) {
  value_list_t vl = VALUE_LIST_INIT;  // On stack
  value_t values[1];                   // On stack
  vl.values = values;
  plugin_dispatch_values(&vl);         // Cloned internally
  // vl and values go out of scope - OK!
}

Rule 2: user_data ownership transfers to collectd
=================================================
void module_register(void) {
  my_state_t *state = malloc(sizeof(*state));
  user_data_t ud = {
    .data = state,
    .free_func = my_state_free  // collectd will call this
  };
  plugin_register_complex_read(..., &ud);
  // DON'T free(state) - collectd owns it now!
}

Rule 3: String config options are caller-owned
==============================================
int my_config(oconfig_item_t *ci) {
  char *url = NULL;
  cf_util_get_string(child, &url);  // Allocates new string
  // Caller (you) must free(url) when done!
}

Rule 4: data_set_t and value_list_t in write callbacks are read-only
===================================================================
int my_write(const data_set_t *ds, const value_list_t *vl, ...) {
  // ds and vl are owned by collectd
  // DON'T free them
  // DON'T modify them (note the 'const')
}
```

**内存所有权规则（中文解释）：**

| 场景 | 所有权 | 谁释放 |
|------|--------|--------|
| 栈上的 value_list | 临时 | 自动（作用域结束） |
| user_data.data | 转移给 collectd | collectd（通过 free_func） |
| cf_util_get_string 返回值 | 调用者 | 调用者 |
| 写回调中的 ds/vl | collectd | collectd |

---

### 9.6 Performance Considerations

```c
// DON'T: Allocate memory in hot path
int slow_read(void) {
  char *buffer = malloc(4096);  // Allocation in every read!
  // ...
  free(buffer);
}

// DO: Pre-allocate in init
static char *buffer;

int my_init(void) {
  buffer = malloc(4096);
}

int fast_read(void) {
  // Use pre-allocated buffer
}

// DON'T: Expensive operations in read callback
int slow_read(void) {
  dns_lookup("hostname.example.com");  // Blocking DNS!
}

// DO: Cache expensive results
static struct sockaddr_in cached_addr;
static bool addr_cached = false;

int my_init(void) {
  dns_lookup_and_cache("hostname.example.com", &cached_addr);
  addr_cached = true;
}

// DON'T: Log excessively
int noisy_read(void) {
  DEBUG("Processing value %d", i);  // For every value!
}

// DO: Rate-limit logging
static cdtime_t last_log;

int quiet_read(void) {
  if (cdtime() - last_log > TIME_T_TO_CDTIME_T(60)) {
    INFO("Still running...");
    last_log = cdtime();
  }
}
```

**性能考量（中文解释）：**

| 避免 | 原因 | 替代方案 |
|------|------|---------|
| 读回调中分配内存 | GC 压力、碎片 | 初始化时预分配 |
| 读回调中 DNS 查询 | 阻塞、慢 | 初始化时缓存 |
| 过多日志 | I/O 开销 | 限流日志 |
| 阻塞 I/O 无超时 | 阻塞线程 | 设置 SO_RCVTIMEO |

---

### 9.7 Correct Shutdown Behavior

```c
static int my_shutdown(void) {
  // 1. Stop any background threads
  if (my_thread_running) {
    pthread_cancel(my_thread);
    pthread_join(my_thread, NULL);
  }
  
  // 2. Flush buffered data
  if (buffer_fill > 0) {
    send_data(buffer, buffer_fill);
  }
  
  // 3. Close connections
  if (connection != NULL) {
    close(connection);
    connection = NULL;
  }
  
  // 4. Free allocated memory
  sfree(my_config_option);
  sfree(buffer);
  
  // 5. Destroy synchronization primitives
  pthread_mutex_destroy(&my_lock);
  
  return 0;
}
```

**正确的关闭行为（中文解释）：**

关闭回调的职责（按顺序）：
1. 停止后台线程
2. 刷新缓冲数据
3. 关闭网络连接
4. 释放堆内存
5. 销毁锁和条件变量

注意：关闭回调**可能**被多次调用（虽然通常不会），应该是幂等的。

---

### 9.8 Learning Outcomes

After reading this section, you should be able to:

- [ ] Create a minimal read plugin
- [ ] Add configuration support to plugins
- [ ] Implement a stateful write plugin with buffering
- [ ] Design multi-instance plugins
- [ ] Follow memory ownership rules correctly
- [ ] Avoid common performance pitfalls
- [ ] Implement proper shutdown behavior
