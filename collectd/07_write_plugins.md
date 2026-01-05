# collectd Write Plugins Deep Dive

## Part 7: Architecture Patterns for Different Backends

### 7.1 Write Plugin Callback Signature

```c
typedef int (*plugin_write_cb)(const data_set_t *ds,
                               const value_list_t *vl,
                               user_data_t *user_data);
```

A write callback receives:
- `ds`: The data set definition (type schema from types.db)
- `vl`: The value list with actual data
- `user_data`: Plugin-specific state (from registration)

**Must return**:
- `0` on success
- Non-zero on failure (will be logged, but won't stop other writers)

**写回调规范（中文解释）：**

写回调接收完整的数据描述：
- `ds` 提供类型信息（数据源名称、类型等）
- `vl` 提供实际数据（值、时间戳、标识符）
- `user_data` 提供插件私有状态（连接池、配置等）

返回值只用于日志记录，单个写插件失败不影响其他插件。

---

### 7.2 Write Plugin Archetypes

#### 7.2.1 Stateless/Immediate Write (e.g., write_log)

```c
// From src/write_log.c - simplest possible write plugin

static int wl_write(const data_set_t *ds, const value_list_t *vl,
                    __attribute__((unused)) user_data_t *user_data) {
  char buffer[WL_BUF_SIZE] = {0};
  
  // Format and output immediately
  format_graphite(buffer, sizeof(buffer), ds, vl, NULL, NULL, '_', 0);
  INFO("%s", buffer);
  
  return 0;
}

void module_register(void) {
  plugin_register_write("write_log", wl_write, NULL);  // No user_data
}
```

**Characteristics**:
- No buffering
- No connection state
- Immediate output (synchronous)
- Simplest pattern

**即时写入模式（中文解释）：**

特点：
- 无缓冲，每个值立即处理
- 无连接状态
- 同步执行
- 最简单的实现模式

适用场景：日志输出、调试、转发到另一个同步 API

---

#### 7.2.2 Buffered/Batched Write (e.g., write_http)

```c
// Simplified pattern from write_http

typedef struct {
  char *url;
  char *buffer;
  size_t buffer_size;
  size_t buffer_fill;
  cdtime_t last_flush;
  cdtime_t flush_interval;
  pthread_mutex_t lock;
} http_instance_t;

static int http_write(const data_set_t *ds, const value_list_t *vl,
                      user_data_t *ud) {
  http_instance_t *inst = ud->data;
  
  pthread_mutex_lock(&inst->lock);
  
  // Append to buffer
  size_t needed = format_json_size(ds, vl);
  if (inst->buffer_fill + needed > inst->buffer_size) {
    // Buffer full, flush first
    http_flush_nolock(inst);
  }
  
  format_json(inst->buffer + inst->buffer_fill, needed, ds, vl);
  inst->buffer_fill += needed;
  
  // Check if time-based flush is needed
  if (cdtime() - inst->last_flush > inst->flush_interval) {
    http_flush_nolock(inst);
  }
  
  pthread_mutex_unlock(&inst->lock);
  return 0;
}

static int http_flush(cdtime_t timeout, const char *identifier,
                      user_data_t *ud) {
  http_instance_t *inst = ud->data;
  
  pthread_mutex_lock(&inst->lock);
  int status = http_flush_nolock(inst);
  pthread_mutex_unlock(&inst->lock);
  
  return status;
}

static int http_flush_nolock(http_instance_t *inst) {
  if (inst->buffer_fill == 0)
    return 0;
  
  // Send HTTP POST
  curl_post(inst->url, inst->buffer, inst->buffer_fill);
  
  inst->buffer_fill = 0;
  inst->last_flush = cdtime();
  return 0;
}
```

**Characteristics**:
- Batches multiple values
- Reduces network/IO overhead
- Requires flush callback
- Need synchronization if multi-threaded access

**批量写入模式（中文解释）：**

特点：
- 累积多个值后一起发送
- 减少网络/IO 开销
- 需要实现 flush 回调
- 需要同步保护缓冲区

设计考量：
- 缓冲区大小：太小频繁刷新，太大内存浪费
- 刷新触发：大小阈值 + 时间阈值
- 线程安全：写线程并发调用，需要锁保护

---

#### 7.2.3 Connection-Based Write (e.g., write_redis)

```c
// Simplified pattern from write_redis

typedef struct {
  char *host;
  int port;
  redisContext *conn;
  pthread_mutex_t lock;
  cdtime_t last_connect_attempt;
} redis_instance_t;

static int redis_write(const data_set_t *ds, const value_list_t *vl,
                       user_data_t *ud) {
  redis_instance_t *inst = ud->data;
  int status = 0;
  
  pthread_mutex_lock(&inst->lock);
  
  // Lazy connection
  if (inst->conn == NULL) {
    inst->conn = redis_connect(inst->host, inst->port);
    if (inst->conn == NULL) {
      // Rate-limit connection attempts
      if (cdtime() - inst->last_connect_attempt < TIME_T_TO_CDTIME_T(10)) {
        pthread_mutex_unlock(&inst->lock);
        return -1;
      }
      inst->last_connect_attempt = cdtime();
      pthread_mutex_unlock(&inst->lock);
      return -1;
    }
  }
  
  // Format and send
  char key[256], value[256];
  format_redis_key(key, sizeof(key), vl);
  format_redis_value(value, sizeof(value), ds, vl);
  
  redisReply *reply = redisCommand(inst->conn, "SET %s %s", key, value);
  if (reply == NULL) {
    // Connection lost, reconnect next time
    redisFree(inst->conn);
    inst->conn = NULL;
    status = -1;
  } else {
    freeReplyObject(reply);
  }
  
  pthread_mutex_unlock(&inst->lock);
  return status;
}

static int redis_shutdown(void) {
  // Close all connections
  for (each instance) {
    pthread_mutex_lock(&inst->lock);
    if (inst->conn != NULL) {
      redisFree(inst->conn);
      inst->conn = NULL;
    }
    pthread_mutex_unlock(&inst->lock);
  }
  return 0;
}
```

**Characteristics**:
- Maintains persistent connections
- Lazy initialization
- Automatic reconnection
- Needs shutdown callback for cleanup

**连接型写入模式（中文解释）：**

特点：
- 维护持久连接
- 延迟初始化（首次写入时连接）
- 自动重连（连接丢失时）
- 需要 shutdown 回调清理连接

最佳实践：
- 连接重试限流（避免频繁重连）
- 锁保护连接状态
- 优雅关闭时释放连接

---

#### 7.2.4 Pull-Based (Prometheus Exporter)

```c
// Pattern from write_prometheus (exposes /metrics endpoint)

typedef struct {
  c_avl_tree_t *metrics;  // Stored metrics for scraping
  pthread_mutex_t lock;
} prom_context_t;

static int prom_write(const data_set_t *ds, const value_list_t *vl,
                      user_data_t *ud) {
  prom_context_t *ctx = ud->data;
  
  pthread_mutex_lock(&ctx->lock);
  
  // Store/update metrics in tree (overwrite old values)
  char name[256];
  format_metric_name(name, sizeof(name), vl);
  
  prom_metric_t *m = avl_get(ctx->metrics, name);
  if (m == NULL) {
    m = prom_metric_create(ds, vl);
    avl_insert(ctx->metrics, name, m);
  } else {
    prom_metric_update(m, ds, vl);
  }
  
  pthread_mutex_unlock(&ctx->lock);
  return 0;
}

// HTTP handler (called from Prometheus scraper)
static void prom_http_handler(void *ud, MHD_Connection *conn) {
  prom_context_t *ctx = ud;
  
  pthread_mutex_lock(&ctx->lock);
  
  char *body = prom_format_metrics(ctx->metrics);
  
  pthread_mutex_unlock(&ctx->lock);
  
  MHD_Response *response = MHD_create_response_from_buffer(...);
  MHD_queue_response(conn, MHD_HTTP_OK, response);
  free(body);
}
```

**Characteristics**:
- Stores latest values (not forwarded)
- Exposes HTTP endpoint for scraping
- Values can be overwritten
- Different threading model (HTTP threads)

**拉取模式（中文解释）：**

这种模式与传统"推送"不同：
- 不主动发送数据
- 存储最新值
- 外部系统（Prometheus）主动拉取

设计考量：
- 内存管理：需要限制存储的指标数量
- 过期处理：旧指标如何删除
- 并发：HTTP 线程和写线程需要同步

---

### 7.3 Time Alignment Issues

Write plugins must tolerate imperfect timing:

```
Expected (10s interval):     Actual:
t=0   t=10  t=20  t=30       t=0   t=10.1  t=19.9  t=30.2
  |-----|-----|-----|          |------|------|------|

Problems:
1. Slight variations due to scheduling jitter
2. Missed samples if read takes too long
3. Duplicate timestamps (clock skew, NTP adjustment)
```

```c
// Write plugins should handle:

// 1. Slightly irregular intervals
void handle_value(cdtime_t time, cdtime_t interval) {
  // Don't assume time is exactly (last_time + interval)
  // Allow for jitter
}

// 2. Missing values
void check_stale(cdtime_t expected_time, cdtime_t actual_time) {
  if ((actual_time - expected_time) > (interval * 2)) {
    // Value was missed, may need to insert NULL/NaN
  }
}

// 3. Out-of-order values (rare but possible)
void handle_ooo(cdtime_t time) {
  // The cache rejects these, but some backends need handling
}
```

**时间对齐问题（中文解释）：**

写插件必须处理以下时间问题：

1. **抖动**：实际间隔可能是 10.1s 或 9.9s，不是精确的 10s
2. **缺失**：读取超时或失败会导致缺失样本
3. **乱序**：NTP 调整可能导致时间戳回退

对 RRD 的特殊影响：
- RRD 要求固定间隔
- 时间戳必须对齐到 step 边界
- 乱序值会被拒绝

---

### 7.4 Write Plugin Comparison

| Plugin | Type | Buffering | Connection | Time Sensitivity |
|--------|------|-----------|------------|------------------|
| write_log | Immediate | No | No | None |
| write_graphite | Stream | Line-buffered | TCP | Low |
| write_influxdb_udp | Batched | Yes | UDP | Low |
| write_http | Batched | Yes | HTTP | Low |
| write_rrd | Immediate | No | File | High |
| write_prometheus | Pull | Store | HTTP server | Low |
| network | Stream | Yes | UDP/TCP | Low |

**写插件对比（中文解释）：**

| 维度 | 低要求 | 高要求 |
|------|--------|--------|
| 时间敏感度 | Graphite、InfluxDB | RRD |
| 连接管理 | write_log | Redis、MongoDB |
| 缓冲复杂度 | 即时写入 | HTTP 批量 |
| 多实例支持 | 单实例 | 多后端并行 |

---

### 7.5 Common Write Plugin Pitfalls

```c
// WRONG: Blocking forever on network write
int bad_write(const data_set_t *ds, const value_list_t *vl, user_data_t *ud) {
  send(socket, data, len, 0);  // May block forever!
}

// RIGHT: Use timeouts
int good_write(const data_set_t *ds, const value_list_t *vl, user_data_t *ud) {
  struct timeval tv = {.tv_sec = 5};
  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  if (send(socket, data, len, 0) < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return -1;  // Will retry later
  }
}

// WRONG: Modifying the value_list
int bad_write(const data_set_t *ds, value_list_t *vl, user_data_t *ud) {
  vl->time = cdtime();  // DON'T modify input!
}

// RIGHT: Copy if modification needed
int good_write(const data_set_t *ds, const value_list_t *vl, user_data_t *ud) {
  value_list_t vl_copy = *vl;
  vl_copy.time = cdtime();  // Modify copy
  // ...
}

// WRONG: Leaking memory on error
int bad_write(const data_set_t *ds, const value_list_t *vl, user_data_t *ud) {
  char *buffer = malloc(1024);
  if (send_failed)
    return -1;  // Memory leak!
  free(buffer);
  return 0;
}

// RIGHT: Cleanup on all paths
int good_write(const data_set_t *ds, const value_list_t *vl, user_data_t *ud) {
  char *buffer = malloc(1024);
  int status = 0;
  
  if (send_failed)
    status = -1;
  
  free(buffer);
  return status;
}
```

**常见陷阱（中文解释）：**

| 陷阱 | 后果 |
|------|------|
| 无超时的网络 I/O | 阻塞写线程 |
| 修改传入的 value_list | 影响其他写插件 |
| 错误路径内存泄漏 | 长期运行后内存耗尽 |
| 不处理连接断开 | 持续失败，日志爆炸 |

---

### 7.6 Learning Outcomes

After reading this section, you should be able to:

- [ ] Implement a simple stateless write plugin
- [ ] Design a buffered write plugin with proper flushing
- [ ] Handle connection management in write plugins
- [ ] Understand the difference between push and pull models
- [ ] Handle timing irregularities appropriately
- [ ] Avoid common write plugin pitfalls
