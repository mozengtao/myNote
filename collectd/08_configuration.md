# collectd Configuration System

## Part 8: Configuration Parsing and Plugin Configuration

### 8.1 Configuration File Structure

```apache
# /etc/collectd/collectd.conf

# Global options
Hostname    "server01"
FQDNLookup   true
Interval     10
Timeout      2
ReadThreads  5
WriteThreads 5

# Type definitions
TypesDB     "/usr/share/collectd/types.db"
TypesDB     "/etc/collectd/my_types.db"

# Plugin loading and configuration
LoadPlugin cpu
LoadPlugin memory
LoadPlugin interface

<LoadPlugin "write_http">
  Interval 30
  FlushInterval 60
</LoadPlugin>

<Plugin cpu>
  ReportByCpu true
  ValuesPercentage false
</Plugin>

<Plugin interface>
  Interface "eth0"
  IgnoreSelected false
</Plugin>

<Plugin write_http>
  <Node "influxdb">
    URL "http://influxdb:8086/write"
    Format "JSON"
  </Node>
</Plugin>

# Filter chains
<Chain "PostCache">
  <Rule "drop_unwanted">
    <Match "regex">
      Plugin "^debug$"
    </Match>
    <Target "stop">
    </Target>
  </Rule>
  <Target "write">
  </Target>
</Chain>
```

**配置文件结构（中文解释）：**

collectd 配置文件采用类 Apache 语法，分为几个部分：

1. **全局选项**：如 `Hostname`、`Interval`
2. **类型定义**：`TypesDB` 指定 types.db 文件
3. **插件加载**：`LoadPlugin` 或 `<LoadPlugin>` 块
4. **插件配置**：`<Plugin>` 块
5. **过滤链**：`<Chain>` 块

---

### 8.2 oconfig Data Structures

```c
// From src/liboconfig/oconfig.h

// Configuration value types
typedef union {
  char *string;
  double number;
  int boolean;
} oconfig_value_t;

// A single configuration option
typedef struct {
  char *key;                    // Option name
  oconfig_value_t *values;      // Array of values
  int values_num;               // Number of values
  struct oconfig_item_s *parent;// Parent block
  struct oconfig_item_s *children; // Child items (for blocks)
  int children_num;             // Number of children
} oconfig_item_t;
```

```
Example: <Plugin interface>
           Interface "eth0"
           IgnoreSelected false
         </Plugin>

Parsed into:
+------------------------+
| oconfig_item_t         |
+------------------------+
| key: "Plugin"          |
| values: ["interface"]  |
| values_num: 1          |
| children_num: 2        |
| children ──────────────────┐
+------------------------+   │
                             v
        +------------------------+  +------------------------+
        | oconfig_item_t         |  | oconfig_item_t         |
        +------------------------+  +------------------------+
        | key: "Interface"       |  | key: "IgnoreSelected"  |
        | values: ["eth0"]       |  | values: [false]        |
        | values_num: 1          |  | values_num: 1          |
        | children_num: 0        |  | children_num: 0        |
        +------------------------+  +------------------------+
```

**配置数据结构（中文解释）：**

`oconfig_item_t` 是一个树形结构：
- `key`：选项名或块名
- `values`：值数组（字符串、数字或布尔）
- `children`：子项数组（对于块）

类型系统：
- `OCONFIG_TYPE_STRING`：带引号的字符串
- `OCONFIG_TYPE_NUMBER`：浮点数
- `OCONFIG_TYPE_BOOLEAN`：true/false

---

### 8.3 Global Options

```c
// From src/daemon/configfile.c

static cf_global_option_t cf_global_options[] = {
  {"BaseDir",            NULL, 0, PKGLOCALSTATEDIR},
  {"PIDFile",            NULL, 0, PIDFILE},
  {"Hostname",           NULL, 0, NULL},
  {"FQDNLookup",         NULL, 0, "true"},
  {"Interval",           NULL, 0, NULL},
  {"ReadThreads",        NULL, 0, "5"},
  {"WriteThreads",       NULL, 0, "5"},
  {"WriteQueueLimitHigh", NULL, 0, NULL},
  {"WriteQueueLimitLow",  NULL, 0, NULL},
  {"Timeout",            NULL, 0, "2"},
  {"AutoLoadPlugin",     NULL, 0, "false"},
  {"CollectInternalStats", NULL, 0, "false"},
  {"PreCacheChain",      NULL, 0, "PreCache"},
  {"PostCacheChain",     NULL, 0, "PostCache"},
  {"MaxReadInterval",    NULL, 0, "86400"}
};

// Access global options
const char *global_option_get(const char *option);
int global_option_set(const char *option, const char *value, bool from_cli);
long global_option_get_long(const char *option, long default_value);
cdtime_t global_option_get_time(const char *name, cdtime_t default_value);
```

| Option | Default | Description |
|--------|---------|-------------|
| BaseDir | `/var/lib/collectd` | Working directory |
| Interval | 10 | Default collection interval (seconds) |
| Timeout | 2 | Interval multiplier for timeout |
| ReadThreads | 5 | Number of read threads |
| WriteThreads | 5 | Number of write threads |
| WriteQueueLimitHigh | 0 (disabled) | Max queue length before dropping |
| AutoLoadPlugin | false | Auto-load plugins on `<Plugin>` block |

**全局选项说明（中文解释）：**

关键选项解释：
- `Interval`：默认采集间隔，可被 `<LoadPlugin>` 覆盖
- `Timeout`：超时判断公式 = `interval × timeout_g`
- `ReadThreads/WriteThreads`：线程池大小
- `WriteQueueLimitHigh/Low`：背压机制阈值

---

### 8.4 Simple Plugin Configuration

```c
// Pattern 1: Key-value config (legacy)

static const char *config_keys[] = {
  "ReportRelative",
  "ValuesPercentage"
};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static bool report_relative;

static int load_config(const char *key, const char *value) {
  if (strcasecmp(key, "ReportRelative") == 0) {
    report_relative = IS_TRUE(value);
    return 0;
  }
  return -1;  // Unknown key
}

void module_register(void) {
  plugin_register_config("load", load_config, config_keys, config_keys_num);
  plugin_register_read("load", load_read);
}
```

**简单配置模式（中文解释）：**

适用于简单的键值对配置：
- 注册关心的键名列表
- 回调接收键和值（都是字符串）
- 自己解析值类型

限制：
- 不支持嵌套块
- 值都是字符串，需要自己转换
- 不支持多值选项

---

### 8.5 Complex Plugin Configuration

```c
// Pattern 2: Complex config (recommended)

static int my_config(oconfig_item_t *ci) {
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    
    if (strcasecmp("URL", child->key) == 0) {
      char *url = NULL;
      if (cf_util_get_string(child, &url) != 0)
        return -1;
      set_url(url);
      free(url);
    }
    else if (strcasecmp("Interval", child->key) == 0) {
      cdtime_t interval;
      if (cf_util_get_cdtime(child, &interval) != 0)
        return -1;
      set_interval(interval);
    }
    else if (strcasecmp("Instance", child->key) == 0) {
      // Nested block
      if (parse_instance(child) != 0)
        return -1;
    }
    else {
      WARNING("Unknown config option: %s", child->key);
      return -1;
    }
  }
  return 0;
}

void module_register(void) {
  plugin_register_complex_config("my_plugin", my_config);
}
```

**复杂配置模式（中文解释）：**

适用于复杂的嵌套配置：
- 接收整个 `<Plugin>` 块
- 可以处理嵌套块
- 使用 `cf_util_*` 辅助函数

辅助函数：
| 函数 | 用途 |
|------|------|
| `cf_util_get_string` | 获取字符串值 |
| `cf_util_get_int` | 获取整数值 |
| `cf_util_get_double` | 获取浮点值 |
| `cf_util_get_boolean` | 获取布尔值 |
| `cf_util_get_cdtime` | 获取时间值 |
| `cf_util_get_port_number` | 获取端口号 |

---

### 8.6 Multi-Instance Configuration

```c
// Supporting multiple configuration blocks:
// <Plugin my_plugin>
//   <Instance "server1">
//     Host "10.0.0.1"
//   </Instance>
//   <Instance "server2">
//     Host "10.0.0.2"
//   </Instance>
// </Plugin>

typedef struct {
  char *name;
  char *host;
  int port;
} instance_t;

static int parse_instance(oconfig_item_t *ci) {
  instance_t *inst = calloc(1, sizeof(*inst));
  
  // Get instance name
  if (ci->values_num != 1 || ci->values[0].type != OCONFIG_TYPE_STRING) {
    ERROR("Instance needs a name");
    return -1;
  }
  inst->name = strdup(ci->values[0].value.string);
  
  // Parse children
  for (int i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;
    if (strcasecmp("Host", child->key) == 0)
      cf_util_get_string(child, &inst->host);
    else if (strcasecmp("Port", child->key) == 0)
      cf_util_get_int(child, &inst->port);
  }
  
  // Register read callback with user_data
  user_data_t ud = {
    .data = inst,
    .free_func = instance_free
  };
  
  char callback_name[256];
  snprintf(callback_name, sizeof(callback_name), "my_plugin/%s", inst->name);
  
  plugin_register_complex_read(
    NULL,           // group
    callback_name,  // unique name
    instance_read,  // callback
    0,              // interval (0 = use default)
    &ud             // user_data
  );
  
  return 0;
}
```

**多实例配置（中文解释）：**

支持多实例的关键点：
1. 每个 `<Instance>` 块创建独立的状态
2. 使用 `user_data_t` 传递实例状态
3. 回调名称必须唯一（如 `my_plugin/server1`）
4. 设置 `free_func` 确保关闭时释放内存

这样可以实现：
- 同一插件监控多个目标
- 每个目标有独立的配置
- 每个目标可以有不同的采集间隔

---

### 8.7 Per-Plugin Interval Override

```c
// In LoadPlugin block:
<LoadPlugin my_plugin>
  Interval 30        # Plugin-specific interval
  FlushInterval 60   # Auto-flush interval
  FlushTimeout 10    # Flush timeout
</LoadPlugin>

// Parsed in dispatch_loadplugin():
static int dispatch_loadplugin(oconfig_item_t *ci) {
  plugin_ctx_t ctx = {
    .interval = cf_get_default_interval(),  // Start with global
    .name = strdup(plugin_name),
  };
  
  for (int i = 0; i < ci->children_num; ++i) {
    oconfig_item_t *child = ci->children + i;
    
    if (strcasecmp("Interval", child->key) == 0)
      cf_util_get_cdtime(child, &ctx.interval);
    else if (strcasecmp("FlushInterval", child->key) == 0)
      cf_util_get_cdtime(child, &ctx.flush_interval);
    else if (strcasecmp("FlushTimeout", child->key) == 0)
      cf_util_get_cdtime(child, &ctx.flush_timeout);
  }
  
  // Set context before loading plugin
  plugin_ctx_t old_ctx = plugin_set_ctx(ctx);
  
  // Load plugin (calls module_register)
  plugin_load(plugin_name, global);
  
  // Restore context
  plugin_set_ctx(old_ctx);
}
```

**插件级间隔覆盖（中文解释）：**

间隔的继承规则：
1. 默认使用全局 `Interval`
2. `<LoadPlugin>` 块可以覆盖
3. `plugin_register_complex_read()` 可以再次覆盖

流程：
```
全局 Interval = 10s
    ↓
<LoadPlugin> Interval = 30s  → 覆盖为 30s
    ↓
module_register() 调用时 ctx.interval = 30s
    ↓
plugin_register_read() 使用 ctx.interval = 30s
    ↓
plugin_register_complex_read(interval=0) 也使用 30s
plugin_register_complex_read(interval=60) 覆盖为 60s
```

---

### 8.8 Configuration Loading Flow

```
cf_read(configfile)
│
├── cf_read_generic(filename)
│   └── cf_read_file(filename)
│       └── oconfig_parse_file()  ──> liboconfig parser
│
├── cf_include_all()
│   └── Process <Include> directives recursively
│
└── for each child in config:
    │
    ├── dispatch_value(ci)         ──> Global options
    │   ├── dispatch_value_typesdb() ──> Load types.db
    │   ├── dispatch_value_plugindir() ──> Set plugin dir
    │   └── dispatch_global_option()
    │
    └── dispatch_block(ci)
        ├── dispatch_loadplugin()  ──> Load .so, call module_register
        ├── dispatch_block_plugin() ──> Call plugin config callback
        └── fc_configure()         ──> Configure filter chains
```

**配置加载流程（中文解释）：**

1. **解析阶段**：liboconfig 解析配置文件
2. **包含处理**：递归处理 `<Include>` 指令
3. **分发阶段**：
   - 全局选项 → `dispatch_value()`
   - `LoadPlugin` → 加载 `.so` 并调用 `module_register()`
   - `<Plugin>` → 调用插件的配置回调
   - `<Chain>` → 配置过滤链

执行顺序很重要：
- `LoadPlugin` 必须在对应的 `<Plugin>` 块之前
- `TypesDB` 应该在加载插件之前

---

### 8.9 Learning Outcomes

After reading this section, you should be able to:

- [ ] Understand the configuration file structure and syntax
- [ ] Implement simple key-value configuration callbacks
- [ ] Implement complex configuration with nested blocks
- [ ] Support multi-instance plugin configurations
- [ ] Use cf_util_* helper functions correctly
- [ ] Understand the configuration loading order
