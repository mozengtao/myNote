# collectd Dispatch Path and Value Cache

## Part 5: Data Flow from Read to Write

### 5.1 The Complete Dispatch Pipeline

```
Read Plugin                                                    Write Plugin
+-----------+                                                  +-----------+
| cpu_read()|                                                  | rrd_write |
+-----------+                                                  +-----------+
      |                                                              ^
      | plugin_dispatch_values(&vl)                                  |
      v                                                              |
+------------------------------------------------------------------+ |
|                    DISPATCH PIPELINE                              | |
+------------------------------------------------------------------+ |
| 1. check_drop_value()  <-- Backpressure (queue full?)            | |
|    |                                                              | |
|    v                                                              | |
| 2. plugin_write_enqueue(vl)                                      | |
|    |-- plugin_value_list_clone()  <-- Deep copy                  | |
|    |-- queue.append(clone)                                       | |
|    |-- pthread_cond_signal()                                     | |
|    v                                                              | |
| [Write Thread]                                                   | |
| 3. plugin_write_dequeue()                                        | |
|    v                                                              | |
| 4. plugin_dispatch_values_internal(vl)                           | |
|    |                                                              | |
|    |-- escape_slashes() on all identifier fields                 | |
|    |                                                              | |
|    |-- Pre-Cache Chain (if configured)                           | |
|    |   fc_process_chain(ds, vl, pre_cache_chain)                 | |
|    |   [Match/Target rules can STOP, RETURN, or CONTINUE]        | |
|    |                                                              | |
|    |-- Value Cache Update                                        | |
|    |   uc_update(ds, vl)                                         | |
|    |   [Calculate rates for COUNTER/DERIVE/ABSOLUTE]             | |
|    |   [Dispatch cache events: NEW, UPDATE]                      | |
|    |                                                              | |
|    |-- Post-Cache Chain (if configured)                          | |
|    |   fc_process_chain(ds, vl, post_cache_chain)                | |
|    |                                                              | |
|    |-- Default Action (if no chain or chain returns CONTINUE)    | |
|    |   fc_default_action(ds, vl)                                 | |
|    |   └── plugin_write(NULL, ds, vl)  <-- All writers   --------+
|    |                                                              |
+------------------------------------------------------------------+
```

**数据流解析（中文解释）：**

从读插件到写插件的完整路径：

1. **背压检查**：如果写队列过长，按概率丢弃数据
2. **入队**：深拷贝 `value_list` 并放入写队列
3. **出队**：写线程从队列取出数据
4. **内部分发**：
   - 转义标识符中的斜杠
   - 执行 Pre-Cache 链（可过滤/修改数据）
   - 更新值缓存（计算速率）
   - 执行 Post-Cache 链
   - 调用默认动作（写入所有注册的写插件）

---

### 5.2 The Value Cache (utils_cache.c)

The cache serves multiple purposes:

1. **Rate calculation** for COUNTER, DERIVE, ABSOLUTE types
2. **Value history** for anomaly detection
3. **State tracking** for missing value detection
4. **Metadata storage** per metric

```c
// From src/daemon/utils_cache.c

typedef struct cache_entry_s {
  char name[6 * DATA_MAX_NAME_LEN];  // Full identifier string
  size_t values_num;                  // Number of data sources
  gauge_t *values_gauge;              // Calculated rates (for all types)
  value_t *values_raw;                // Raw values (for rate calculation)
  cdtime_t last_time;                 // Timestamp of last value
  cdtime_t last_update;               // Local time of last update
  cdtime_t interval;                  // Collection interval
  int state;                          // STATE_UNKNOWN, STATE_OKAY, STATE_MISSING
  int hits;                           // Access counter
  gauge_t *history;                   // Circular buffer for history
  size_t history_index;               // Current position in history
  size_t history_length;              // History buffer size
  meta_data_t *meta;                  // Metadata
  unsigned long callbacks_mask;       // Subscribed cache event callbacks
} cache_entry_t;

// Cache is an AVL tree keyed by identifier string
static c_avl_tree_t *cache_tree;
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
```

```
Cache Entry Example:
====================
Key: "server01/interface-eth0/if_octets"

+---------------------+
| cache_entry_t       |
+---------------------+
| name: "server01/..."| 
| values_num: 2       |     (rx and tx)
| values_gauge ───────────> [1234.5, 5678.9]  (rates in bytes/sec)
| values_raw ─────────────> [counter_rx, counter_tx]  (raw counters)
| last_time: 1699234567|     (from value_list.time)
| last_update: ...    |     (from cdtime())
| interval: 10s       |
| state: STATE_OKAY   |
+---------------------+
```

**值缓存机制（中文解释）：**

缓存的核心功能是**速率计算**：

| 类型 | 原始值 | 缓存计算 |
|------|--------|----------|
| GAUGE | 42.5 | 直接存储 42.5 |
| COUNTER | 12345678 | (当前值 - 上次值) / 时间差 |
| DERIVE | -1000 | (当前值 - 上次值) / 时间差（可为负） |
| ABSOLUTE | 1000 | 当前值 / 时间差（每次重置） |

缓存结构：
- 使用 AVL 树存储，按标识符字符串排序
- 每个缓存条目保存原始值和计算后的速率
- 支持历史记录（用于趋势分析）
- 支持元数据附加

---

### 5.3 Rate Calculation in uc_update

```c
// Simplified from src/daemon/utils_cache.c

int uc_update(const data_set_t *ds, const value_list_t *vl) {
  char name[6 * DATA_MAX_NAME_LEN];
  FORMAT_VL(name, sizeof(name), vl);  // Build identifier string
  
  pthread_mutex_lock(&cache_lock);
  
  cache_entry_t *ce;
  if (c_avl_get(cache_tree, name, (void *)&ce) != 0) {
    // First time seeing this metric
    uc_insert(ds, vl, name);
    pthread_mutex_unlock(&cache_lock);
    plugin_dispatch_cache_event(CE_VALUE_NEW, 0, name, vl);
    return 0;
  }
  
  // Check for out-of-order values
  if (ce->last_time >= vl->time) {
    pthread_mutex_unlock(&cache_lock);
    NOTICE("Value too old: %s", name);
    return -1;
  }
  
  // Calculate rates for each data source
  for (size_t i = 0; i < ds->ds_num; i++) {
    cdtime_t diff_time = vl->time - ce->last_time;
    
    switch (ds->ds[i].type) {
      case DS_TYPE_COUNTER: {
        // Handle counter wrap-around
        counter_t diff = counter_diff(ce->values_raw[i].counter,
                                       vl->values[i].counter);
        ce->values_gauge[i] = (double)diff / CDTIME_T_TO_DOUBLE(diff_time);
        ce->values_raw[i].counter = vl->values[i].counter;
        break;
      }
      
      case DS_TYPE_GAUGE:
        ce->values_gauge[i] = vl->values[i].gauge;
        ce->values_raw[i].gauge = vl->values[i].gauge;
        break;
        
      case DS_TYPE_DERIVE: {
        derive_t diff = vl->values[i].derive - ce->values_raw[i].derive;
        ce->values_gauge[i] = (double)diff / CDTIME_T_TO_DOUBLE(diff_time);
        ce->values_raw[i].derive = vl->values[i].derive;
        break;
      }
      
      case DS_TYPE_ABSOLUTE:
        ce->values_gauge[i] = (double)vl->values[i].absolute /
                              CDTIME_T_TO_DOUBLE(diff_time);
        ce->values_raw[i].absolute = vl->values[i].absolute;
        break;
    }
  }
  
  // Range check (discard values outside min/max)
  uc_check_range(ds, ce);
  
  ce->last_time = vl->time;
  ce->last_update = cdtime();
  
  pthread_mutex_unlock(&cache_lock);
  
  plugin_dispatch_cache_event(CE_VALUE_UPDATE, ce->callbacks_mask, name, vl);
  return 0;
}
```

**速率计算详解（中文解释）：**

速率计算公式：`rate = (current - previous) / time_diff`

特殊处理：
1. **COUNTER 回绕**：`counter_diff()` 处理 32/64 位计数器溢出
2. **范围检查**：超出 `min`/`max` 的值设为 NaN
3. **乱序值**：时间戳早于缓存的值被拒绝

为什么需要速率：
- COUNTER 的原始值（如网络包数 12345678）对人类不友好
- 速率（如 1234.5 包/秒）更有意义
- RRD 等后端需要 GAUGE 类型的值

---

### 5.4 Cache Timeout and Missing Value Detection

```c
// Called from main loop via plugin_read_all()
int uc_check_timeout(void) {
  cdtime_t now = cdtime();
  
  pthread_mutex_lock(&cache_lock);
  
  // Find expired entries
  c_avl_iterator_t *iter = c_avl_get_iterator(cache_tree);
  char *key;
  cache_entry_t *ce;
  
  while (c_avl_iterator_next(iter, (void *)&key, (void *)&ce) == 0) {
    // Entry is stale if not updated for (interval * timeout_g) time
    if ((now - ce->last_update) < (ce->interval * timeout_g))
      continue;
    
    // Mark for removal
    expired[expired_num++] = {key, ce->last_time, ce->interval, ce->callbacks_mask};
  }
  
  pthread_mutex_unlock(&cache_lock);
  
  // Dispatch missing callbacks (without lock held!)
  for (size_t i = 0; i < expired_num; i++) {
    value_list_t vl = {.time = expired[i].time, .interval = expired[i].interval};
    parse_identifier_vl(expired[i].key, &vl);
    
    plugin_dispatch_missing(&vl);
    plugin_dispatch_cache_event(CE_VALUE_EXPIRED, expired[i].callbacks_mask,
                                expired[i].key, &vl);
  }
  
  // Actually remove entries
  pthread_mutex_lock(&cache_lock);
  for (size_t i = 0; i < expired_num; i++) {
    c_avl_remove(cache_tree, expired[i].key, ...);
    cache_free(ce);
  }
  pthread_mutex_unlock(&cache_lock);
  
  return 0;
}
```

**缓存超时机制（中文解释）：**

超时判断：`(当前时间 - 最后更新时间) > (采集间隔 × 超时倍数)`

默认 `timeout_g = 2`，意味着：
- 10 秒间隔的指标超过 20 秒未更新则视为缺失
- 触发 "missing" 回调
- 触发 `CE_VALUE_EXPIRED` 缓存事件
- 从缓存中删除

用途：
- 检测停止的进程
- 告警服务不可用
- 清理陈旧数据

---

### 5.5 Filter Chains

Filter chains allow matching and transforming values before writing:

```
<Chain "PostCache">
  <Rule "drop_debug">
    <Match "regex">
      Plugin "^debug$"
    </Match>
    <Target "stop">
    </Target>
  </Rule>
  
  <Rule "rename_host">
    <Match "regex">
      Host "^old-name$"
    </Match>
    <Target "set">
      Host "new-name"
    </Target>
  </Rule>
  
  <Target "write">
    Plugin "rrdtool"
    Plugin "network"
  </Target>
</Chain>
```

```c
// From src/daemon/filter_chain.c

int fc_process_chain(const data_set_t *ds, value_list_t *vl,
                     fc_chain_t *chain) {
  for (fc_rule_t *rule = chain->rules; rule != NULL; rule = rule->next) {
    // Check all matches
    int status = FC_MATCH_MATCHES;
    for (fc_match_t *match = rule->matches; match != NULL; match = match->next) {
      status = match->proc.match(ds, vl, NULL, &match->user_data);
      if (status != FC_MATCH_MATCHES)
        break;
    }
    
    if (status != FC_MATCH_MATCHES)
      continue;  // Rule didn't match
    
    // Execute all targets
    for (fc_target_t *target = rule->targets; target != NULL; target = target->next) {
      status = target->proc.invoke(ds, vl, NULL, &target->user_data);
      
      if (status == FC_TARGET_STOP)
        return FC_TARGET_STOP;  // Stop all processing
      if (status == FC_TARGET_RETURN)
        return FC_TARGET_CONTINUE;  // Exit this chain
    }
  }
  
  // Execute default targets
  for (fc_target_t *target = chain->targets; target != NULL; target = target->next) {
    target->proc.invoke(ds, vl, NULL, &target->user_data);
  }
  
  return FC_TARGET_CONTINUE;
}
```

**过滤链机制（中文解释）：**

过滤链类似于 iptables 规则：

1. **Chain**：规则链（如 PreCache、PostCache）
2. **Rule**：规则，包含匹配器和目标
3. **Match**：匹配条件（如正则表达式匹配插件名）
4. **Target**：动作（如 stop、return、write、set）

内置目标：
| 目标 | 作用 |
|------|------|
| `stop` | 停止所有处理，不写入任何后端 |
| `return` | 退出当前链，继续后续处理 |
| `jump` | 跳转到另一个链 |
| `write` | 写入指定的写插件 |

---

### 5.6 The plugin_write Function

```c
// From src/daemon/plugin.c

int plugin_write(const char *plugin, const data_set_t *ds,
                 const value_list_t *vl) {
  if (plugin == NULL) {
    // Write to ALL registered writers
    int success = 0;
    int failure = 0;
    
    for (llentry_t *le = llist_head(list_write); le != NULL; le = le->next) {
      callback_func_t *cf = le->value;
      plugin_write_cb callback = cf->cf_callback;
      
      // Switch context to writer's context
      plugin_ctx_t old_ctx = plugin_get_ctx();
      plugin_ctx_t ctx = old_ctx;
      ctx.name = cf->cf_ctx.name;
      plugin_set_ctx(ctx);
      
      int status = callback(ds, vl, &cf->cf_udata);
      
      plugin_set_ctx(old_ctx);
      
      if (status != 0)
        failure++;
      else
        success++;
    }
    
    // Return error only if ALL writers failed
    return (success == 0 && failure != 0) ? -1 : 0;
  } else {
    // Write to specific plugin only
    // ... find plugin by name and call ...
  }
}
```

**写入机制（中文解释）：**

`plugin_write()` 的行为：

1. 如果 `plugin == NULL`：调用所有注册的写插件
2. 如果指定了插件名：只调用该插件

错误处理：
- 单个插件失败不影响其他插件
- 只有所有插件都失败才返回错误
- 使用 "complain" 机制避免重复日志

---

### 5.7 Backpressure: Write Queue Limits

```c
// From src/daemon/plugin.c

static bool check_drop_value(void) {
  if (write_limit_high == 0)
    return false;  // No limit configured
  
  double p = get_drop_probability();
  if (p == 0.0)
    return false;
  
  if (p == 1.0)
    return true;  // Always drop
  
  // Probabilistic drop
  double q = cdrand_d();
  return (p > q);
}

static double get_drop_probability(void) {
  long wql = write_queue_length;
  
  if (wql < write_limit_low)
    return 0.0;  // Queue OK
  if (wql >= write_limit_high)
    return 1.0;  // Queue full, drop everything
  
  // Linear interpolation between low and high
  long pos = 1 + wql - write_limit_low;
  long size = 1 + write_limit_high - write_limit_low;
  return (double)pos / (double)size;
}
```

```
Write Queue Backpressure:
========================

Queue Length
     ^
     |
high |--------------------------- 100% drop
     |                         /
     |                       /
     |                     /  probabilistic
     |                   /    drop region
     |                 /
 low |---------------/----------- 0% drop
     |
     +-------------------------->
                              time
```

**背压机制（中文解释）：**

当写入速度跟不上读取速度时：

1. 队列长度低于 `WriteQueueLimitLow`：正常处理
2. 队列长度在 Low 和 High 之间：按概率丢弃
3. 队列长度超过 `WriteQueueLimitHigh`：丢弃所有新数据

配置示例：
```
WriteQueueLimitHigh 1000000
WriteQueueLimitLow   800000
```

这种设计的优势：
- 防止内存耗尽
- 渐进式降级
- 可预测的行为

---

### 5.8 Learning Outcomes

After reading this section, you should be able to:

- [ ] Trace the path of a value from `plugin_dispatch_values` to write plugins
- [ ] Explain how the cache calculates rates for different data source types
- [ ] Describe the timeout mechanism for detecting missing values
- [ ] Configure filter chains to route/transform data
- [ ] Explain the write queue backpressure mechanism
- [ ] Understand why dispatch is asynchronous (queue-based)
