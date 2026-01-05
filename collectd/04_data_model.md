# collectd Core Data Model

## Part 4: Data Structures, Ownership, and Lifetime

### 4.1 The Type System: data_set_t and data_source_t

collectd uses a **type system** defined in `types.db` to describe metric schemas:

```c
// From src/daemon/plugin.h

struct data_source_s {
  char name[DATA_MAX_NAME_LEN];  // Data source name (e.g., "value", "rx", "tx")
  int type;                       // DS_TYPE_COUNTER, GAUGE, DERIVE, ABSOLUTE
  double min;                     // Minimum valid value (NAN = unbounded)
  double max;                     // Maximum valid value (NAN = unbounded)
};
typedef struct data_source_s data_source_t;

struct data_set_s {
  char type[DATA_MAX_NAME_LEN];   // Type name (e.g., "cpu", "if_octets")
  size_t ds_num;                   // Number of data sources
  data_source_t *ds;               // Array of data sources
};
typedef struct data_set_t data_set_t;
```

```
Example: types.db entry
-----------------------
if_octets    rx:DERIVE:0:U, tx:DERIVE:0:U

Parsed into:
+-----------------+
| data_set_t      |
+-----------------+
| type: "if_octets"|
| ds_num: 2       |
| ds: ───────────────────────┐
+-----------------+          │
                             v
                   +------------------------+
                   | data_source_t[0]       |
                   +------------------------+
                   | name: "rx"             |
                   | type: DS_TYPE_DERIVE   |
                   | min: 0                 |
                   | max: NAN               |
                   +------------------------+
                   | data_source_t[1]       |
                   +------------------------+
                   | name: "tx"             |
                   | type: DS_TYPE_DERIVE   |
                   | min: 0                 |
                   | max: NAN               |
                   +------------------------+
```

**类型系统说明（中文解释）：**

`types.db` 是 collectd 的**模式定义文件**，类似于数据库的 schema：

1. **data_set_t**：描述一种指标类型（如 `cpu`、`if_octets`）
2. **data_source_t**：描述类型中的单个值（如 CPU 的 `user`、`system`）

四种数据源类型：
| 类型 | 含义 | 示例 |
|------|------|------|
| COUNTER | 单调递增计数器 | 网络包数 |
| GAUGE | 瞬时值 | 温度、负载 |
| DERIVE | 可增可减的导数 | 网络字节数 |
| ABSOLUTE | 每次重置的绝对值 | 系统启动后的运行时间 |

---

### 4.2 Value Types: value_t Union

```c
// From src/daemon/plugin.h

typedef unsigned long long counter_t;  // Monotonic counter
typedef double gauge_t;                 // Instantaneous value
typedef int64_t derive_t;               // Signed derivative
typedef uint64_t absolute_t;            // Absolute value (reset each read)

union value_u {
  counter_t counter;
  gauge_t gauge;
  derive_t derive;
  absolute_t absolute;
};
typedef union value_u value_t;
```

**Type Safety**: The `value_t` union requires external knowledge of the type (from `data_set_t`) to interpret correctly. This is a deliberate trade-off for memory efficiency.

```c
// Correct usage:
void process_value(const data_set_t *ds, const value_t *v, size_t i) {
  switch (ds->ds[i].type) {
    case DS_TYPE_COUNTER:
      printf("counter: %llu\n", v->counter);
      break;
    case DS_TYPE_GAUGE:
      printf("gauge: %f\n", v->gauge);
      break;
    case DS_TYPE_DERIVE:
      printf("derive: %" PRId64 "\n", v->derive);
      break;
    case DS_TYPE_ABSOLUTE:
      printf("absolute: %" PRIu64 "\n", v->absolute);
      break;
  }
}
```

**值类型说明（中文解释）：**

`value_t` 是一个**联合体**，可以存储四种不同类型的值：
- 所有类型共享同一块内存（8 字节）
- 必须通过 `data_set_t` 知道实际类型
- 这是为了内存效率的设计权衡

常见错误：
```c
// 错误：不知道类型就访问
value_t v;
printf("%f", v.gauge);  // 可能是 counter，会输出垃圾！

// 正确：先检查类型
if (ds->ds[i].type == DS_TYPE_GAUGE) {
  printf("%f", v.gauge);
}
```

---

### 4.3 The Value List: value_list_t

The `value_list_t` is the **unit of currency** in collectd — the fundamental data exchange format:

```c
struct value_list_s {
  value_t *values;                      // Array of values
  size_t values_len;                    // Number of values (must match ds->ds_num)
  cdtime_t time;                        // Timestamp (0 = auto-fill with current time)
  cdtime_t interval;                    // Sampling interval (0 = auto-fill from context)
  char host[DATA_MAX_NAME_LEN];         // Hostname (empty = auto-fill from hostname_g)
  char plugin[DATA_MAX_NAME_LEN];       // Plugin name
  char plugin_instance[DATA_MAX_NAME_LEN]; // Plugin instance (optional)
  char type[DATA_MAX_NAME_LEN];         // Type name (from types.db)
  char type_instance[DATA_MAX_NAME_LEN];   // Type instance (optional)
  meta_data_t *meta;                    // Optional metadata
};
typedef struct value_list_s value_list_t;

#define VALUE_LIST_INIT { .values = NULL, .meta = NULL }
```

```
Identifier Format:
==================
host/plugin-plugin_instance/type-type_instance

Example:
server01/cpu-0/cpu-idle
└──────┘ └───┘ └──────┘
  host   plugin  type
         └───────────────────────┘
         plugin-plugin_instance

Memory Layout:
==============
+----------------------+
| value_list_t         |
+----------------------+
| values ──────────────────> [value_t, value_t, ...]
| values_len: 2        |
| time: 1699234567...  |     (cdtime_t = 64-bit)
| interval: 10s        |
| host: "server01"     |     (fixed 64-byte buffer)
| plugin: "cpu"        |
| plugin_instance: "0" |
| type: "cpu"          |
| type_instance: "idle"|
| meta ────────────────────> (optional key-value pairs)
+----------------------+
```

**value_list_t 详解（中文解释）：**

`value_list_t` 是 collectd 中最重要的数据结构，表示一次采集的完整数据：

1. **标识符字段**：
   - `host`：主机名（自动填充为 `hostname_g`）
   - `plugin`：插件名（如 "cpu"、"memory"）
   - `plugin_instance`：插件实例（如 CPU 编号 "0"）
   - `type`：类型名（必须在 types.db 中定义）
   - `type_instance`：类型实例（如 "idle"、"user"）

2. **时间字段**：
   - `time`：采集时间戳（0 表示自动填充当前时间）
   - `interval`：采集间隔（0 表示使用插件上下文中的值）

3. **值数组**：
   - `values`：指向值数组的指针
   - `values_len`：值的数量（必须与 `data_set_t.ds_num` 匹配）

4. **元数据**：
   - `meta`：可选的键值对，用于传递额外信息

---

### 4.4 Value List Initialization and Dispatch

```c
// Standard pattern for dispatching values:
static void submit_values(gauge_t value) {
  value_list_t vl = VALUE_LIST_INIT;  // Always initialize!
  
  // Allocate values array (stack is fine for simple cases)
  value_t values[1] = { { .gauge = value } };
  vl.values = values;
  vl.values_len = 1;
  
  // Set identifiers
  sstrncpy(vl.plugin, "my_plugin", sizeof(vl.plugin));
  sstrncpy(vl.type, "gauge", sizeof(vl.type));
  // host, time, interval will be auto-filled
  
  plugin_dispatch_values(&vl);
  // vl.values can be freed/reused after dispatch
}

// Multi-value dispatch:
static void submit_cpu_values(derive_t user, derive_t system) {
  value_list_t vl = VALUE_LIST_INIT;
  value_t values[2] = {
    { .derive = user },
    { .derive = system }
  };
  
  vl.values = values;
  vl.values_len = 2;
  sstrncpy(vl.plugin, "cpu", sizeof(vl.plugin));
  sstrncpy(vl.plugin_instance, "0", sizeof(vl.plugin_instance));
  sstrncpy(vl.type, "cpu", sizeof(vl.type));  // Must match types.db
  
  plugin_dispatch_values(&vl);
}
```

**值提交模式（中文解释）：**

提交值的标准流程：

1. **初始化**：使用 `VALUE_LIST_INIT` 宏
2. **设置值**：分配 `values` 数组并填充
3. **设置标识符**：至少设置 `plugin` 和 `type`
4. **调用 dispatch**：`plugin_dispatch_values()` 会深拷贝数据

**自动填充规则**：
| 字段 | 如果为空/0 |
|------|-----------|
| `host` | 填充 `hostname_g` |
| `time` | 填充 `cdtime()` |
| `interval` | 填充 `plugin_get_interval()` |

---

### 4.5 Value List Cloning and Ownership

```c
// From src/daemon/plugin.c

// Deep clone with auto-fill
static value_list_t *plugin_value_list_clone(value_list_t const *vl_orig) {
  value_list_t *vl = malloc(sizeof(*vl));
  memcpy(vl, vl_orig, sizeof(*vl));
  
  // Auto-fill host if empty
  if (vl->host[0] == 0)
    sstrncpy(vl->host, hostname_g, sizeof(vl->host));
  
  // Deep copy values array
  vl->values = calloc(vl_orig->values_len, sizeof(*vl->values));
  memcpy(vl->values, vl_orig->values, vl_orig->values_len * sizeof(*vl->values));
  
  // Deep copy metadata
  vl->meta = meta_data_clone(vl->meta);
  
  // Auto-fill time and interval
  if (vl->time == 0)
    vl->time = cdtime();
  if (vl->interval == 0)
    vl->interval = plugin_get_interval();
  
  return vl;
}

// Free cloned value list
static void plugin_value_list_free(value_list_t *vl) {
  if (vl == NULL) return;
  meta_data_destroy(vl->meta);
  sfree(vl->values);
  sfree(vl);
}
```

**Ownership Model**:
```
Read Plugin                      Write Queue                    Write Plugin
+-----------+                    +-----------+                  +-----------+
| vl (stack)|  -- clone -->     | vl (heap) |  -- passed -->  | vl (ref)  |
| values[]  |                    | values[]  |                  |           |
+-----------+                    +-----------+                  +-----------+
     |                                 |                              |
  [auto-free]                    [queue owns]                   [read-only]
```

**所有权模型（中文解释）：**

collectd 的所有权规则：

1. **读插件**：在栈上创建 `value_list_t`，dispatch 后可以立即重用
2. **队列**：`plugin_write_enqueue()` 会**深拷贝**整个结构
3. **写插件**：收到的是拷贝的**只读引用**，不应修改或释放

关键点：
- 读插件不需要管理堆内存
- 写插件不需要释放传入的数据
- 队列负责内存生命周期管理

---

### 4.6 The types.db File

```
# types.db format:
# type_name  ds_name:type:min:max[, ds_name:type:min:max, ...]

# Single-value types
gauge        value:GAUGE:U:U
counter      value:COUNTER:U:U
derive       value:DERIVE:U:U
absolute     value:ABSOLUTE:U:U

# Multi-value types
cpu          value:DERIVE:0:U
if_octets    rx:DERIVE:0:U, tx:DERIVE:0:U
if_packets   rx:DERIVE:0:U, tx:DERIVE:0:U
load         shortterm:GAUGE:0:5000, midterm:GAUGE:0:5000, longterm:GAUGE:0:5000
memory       value:GAUGE:0:281474976710656
df           used:GAUGE:0:U, free:GAUGE:0:U
```

**Parsing Flow** (from `src/daemon/types_list.c`):

```c
// Simplified parsing logic
static void parse_line(char *buf) {
  char *fields[64];
  size_t fields_num = strsplit(buf, fields, 64);
  
  data_set_t ds = {0};
  sstrncpy(ds.type, fields[0], sizeof(ds.type));
  ds.ds_num = fields_num - 1;
  ds.ds = calloc(ds.ds_num, sizeof(*ds.ds));
  
  for (size_t i = 0; i < ds.ds_num; i++) {
    parse_ds(ds.ds + i, fields[i + 1], strlen(fields[i + 1]));
  }
  
  plugin_register_data_set(&ds);  // Register globally
  sfree(ds.ds);  // ds is copied internally
}
```

**types.db 详解（中文解释）：**

`types.db` 是 collectd 的**类型定义文件**，定义了所有合法的指标类型：

格式：`类型名 数据源名:类型:最小值:最大值[, ...]`

- `U` 表示无限制（NaN）
- 多个数据源用逗号分隔

为什么需要 types.db：
1. **类型安全**：确保 `values_len` 与类型定义匹配
2. **速率计算**：缓存需要知道类型来计算 COUNTER/DERIVE 的速率
3. **范围检查**：剔除超出 min/max 范围的值

---

### 4.7 Notification Structure

```c
typedef struct notification_s {
  int severity;                         // NOTIF_FAILURE, NOTIF_WARNING, NOTIF_OKAY
  cdtime_t time;                        // Timestamp
  char message[NOTIF_MAX_MSG_LEN];      // Human-readable message (256 bytes)
  char host[DATA_MAX_NAME_LEN];         // Identifier fields (same as value_list)
  char plugin[DATA_MAX_NAME_LEN];
  char plugin_instance[DATA_MAX_NAME_LEN];
  char type[DATA_MAX_NAME_LEN];
  char type_instance[DATA_MAX_NAME_LEN];
  notification_meta_t *meta;            // Key-value metadata
} notification_t;

// Severity levels
#define NOTIF_FAILURE 1
#define NOTIF_WARNING 2
#define NOTIF_OKAY    4
```

**通知结构（中文解释）：**

`notification_t` 用于告警和状态变更通知：
- 与 `value_list_t` 共享相同的标识符字段
- 添加了 `severity` 和 `message` 字段
- 支持元数据附加

---

### 4.8 cdtime_t: The Time Representation

```c
// collectd uses a 64-bit fixed-point time representation
// with nanosecond precision

// 1 second = 2^30 cdtime units (approximately 1.07 billion)
#define CDTIME_T_TO_DOUBLE(t) \
  (((double)(t)) / 1073741824.0)

#define DOUBLE_TO_CDTIME_T(d) \
  ((cdtime_t)((d) * 1073741824.0))

#define CDTIME_T_TO_TIMESPEC(t) \
  (struct timespec) { \
    .tv_sec = (time_t)((t) >> 30), \
    .tv_nsec = (long)((((t) & 0x3FFFFFFF) * 1000000000) >> 30) \
  }

cdtime_t cdtime(void);  // Current time in cdtime units
```

**为什么使用 cdtime_t（中文解释）：**

collectd 使用自定义的时间表示（而非 `time_t` 或 `struct timespec`）：

1. **单一类型**：64 位整数，无需分开处理秒和纳秒
2. **亚秒精度**：约 1ns 精度（实际约 0.93ns）
3. **高效比较**：直接比较整数
4. **溢出安全**：可表示到公元 2514 年

转换公式：
- cdtime → 秒：`cdtime >> 30` 或 `cdtime / 1073741824.0`
- 秒 → cdtime：`seconds * 1073741824` 或 `seconds << 30`

---

### 4.9 Learning Outcomes

After reading this section, you should be able to:

- [ ] Explain the relationship between `data_set_t` and `types.db`
- [ ] Describe the four data source types and when to use each
- [ ] Construct a `value_list_t` and dispatch it correctly
- [ ] Explain the ownership model for value lists
- [ ] Understand why `value_t` is a union and how to use it safely
- [ ] Convert between `cdtime_t` and standard time representations
