# FRR lib/ Infrastructure: API | Reusable Interfaces

## Overview Diagram

```
+-----------------------------------------------------------------------------+
|                                                                             |
|                    API STABILITY AND REUSABILITY                            |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  API CLASSIFICATION                                                         |
|  ==================                                                         |
|                                                                             |
|  +-----------------------------------+-----------------------------------+   |
|  |     STABLE / EXTERNAL USE         |      INTERNAL / DAEMON-SPECIFIC   |   |
|  +-----------------------------------+-----------------------------------+   |
|  |                                   |                                   |   |
|  |  Event System:                    |  Zebra Client:                    |   |
|  |  - event_add_read/write/timer     |  - zclient internals              |   |
|  |  - event_cancel                   |  - zapi_* encoding details        |   |
|  |  - event_master_create            |                                   |   |
|  |                                   |  VRF Internals:                   |   |
|  |  Memory System:                   |  - vrf_init callbacks             |   |
|  |  - XMALLOC/XCALLOC/XFREE          |  - vrf_bitmap internals           |   |
|  |  - DECLARE/DEFINE_MTYPE           |                                   |   |
|  |                                   |  Northbound:                      |   |
|  |  Logging:                         |  - nb_* internal APIs             |   |
|  |  - zlog_err/warn/info/debug       |  - YANG model internals           |   |
|  |  - flog_err/flog_warn             |                                   |   |
|  |                                   |  Command System:                  |   |
|  |  Data Structures:                 |  - defun internals                |   |
|  |  - typesafe.h containers          |  - vty internal state             |   |
|  |  - hash.h (legacy)                |                                   |   |
|  |  - linklist.h (legacy)            |  Protocol Helpers:                |   |
|  |                                   |  - bgp_* internals                |   |
|  |  Prefix/Address:                  |  - ospf_* internals               |   |
|  |  - str2prefix, prefix2str         |  - isis_* internals               |   |
|  |  - prefix_match, prefix_same      |                                   |   |
|  |                                   |                                   |   |
|  |  Stream:                          |                                   |   |
|  |  - stream_new/free                |                                   |   |
|  |  - stream_put*/get*               |                                   |   |
|  |                                   |                                   |   |
|  +-----------------------------------+-----------------------------------+   |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  STABLE APIs FOR EXTERNAL USE                                               |
|  ===========================                                                |
|                                                                             |
|  1. EVENT SYSTEM API                                                        |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  // Create event loop                                               |    |
|  |  struct event_loop *master = event_master_create("myapp");          |    |
|  |                                                                     |    |
|  |  // Add events (all return void, store handle in last param)        |    |
|  |  event_add_read(master, callback, arg, fd, &event_handle);          |    |
|  |  event_add_write(master, callback, arg, fd, &event_handle);         |    |
|  |  event_add_timer(master, callback, arg, seconds, &event_handle);    |    |
|  |  event_add_timer_msec(master, callback, arg, msec, &event_handle);  |    |
|  |  event_add_event(master, callback, arg, val, &event_handle);        |    |
|  |                                                                     |    |
|  |  // Cancel event                                                    |    |
|  |  event_cancel(&event_handle);  // Sets handle to NULL               |    |
|  |                                                                     |    |
|  |  // Main loop                                                       |    |
|  |  struct event event;                                                |    |
|  |  while (event_fetch(master, &event))                                |    |
|  |      event_call(&event);                                            |    |
|  |                                                                     |    |
|  |  // Cleanup                                                         |    |
|  |  event_master_free(master);                                         |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  2. MEMORY SYSTEM API                                                       |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  // Define memory types (in header)                                 |    |
|  |  DECLARE_MGROUP(MYAPP);                                             |    |
|  |  DECLARE_MTYPE(MYAPP_CONTEXT);                                      |    |
|  |                                                                     |    |
|  |  // Define memory types (in source)                                 |    |
|  |  DEFINE_MGROUP(MYAPP, "My Application");                            |    |
|  |  DEFINE_MTYPE(MYAPP, MYAPP_CONTEXT, "Application context");         |    |
|  |                                                                     |    |
|  |  // Allocation                                                      |    |
|  |  ptr = XMALLOC(MTYPE_MYAPP_CONTEXT, sizeof(*ptr));                  |    |
|  |  ptr = XCALLOC(MTYPE_MYAPP_CONTEXT, sizeof(*ptr));  // zeroed       |    |
|  |  str = XSTRDUP(MTYPE_MYAPP_STRING, "hello");                        |    |
|  |  ptr = XREALLOC(MTYPE_MYAPP_CONTEXT, ptr, new_size);                |    |
|  |                                                                     |    |
|  |  // Deallocation (ALWAYS sets ptr to NULL!)                         |    |
|  |  XFREE(MTYPE_MYAPP_CONTEXT, ptr);  // ptr == NULL after             |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  3. LOGGING API                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  // Basic logging (no error code)                                   |    |
|  |  zlog_err("Critical error: %s", error_msg);                         |    |
|  |  zlog_warn("Warning condition: %d", code);                          |    |
|  |  zlog_info("Information: processing %s", item);                     |    |
|  |  zlog_notice("Daemon started");                                     |    |
|  |  zlog_debug("Debug: state=%d", state);                              |    |
|  |                                                                     |    |
|  |  // With error code (for structured error handling)                 |    |
|  |  flog_err(EC_MYAPP_FAILED, "Operation failed");                     |    |
|  |  flog_warn(EC_MYAPP_RETRY, "Will retry operation");                 |    |
|  |                                                                     |    |
|  |  // FRR-specific format extensions                                  |    |
|  |  zlog_info("Prefix: %pFX", &prefix);     // IP prefix               |    |
|  |  zlog_info("Address: %pI4", &addr);      // IPv4 address            |    |
|  |  zlog_info("Address: %pI6", &addr6);     // IPv6 address            |    |
|  |  zlog_info("Sockunion: %pSU", &su);      // Socket address          |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  4. DATA STRUCTURE API (typesafe.h)                                         |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  // Double-linked list                                              |    |
|  |  PREDECL_DLIST(item_list);                                          |    |
|  |  struct item {                                                      |    |
|  |      struct item_list_item entry;                                   |    |
|  |      int value;                                                     |    |
|  |  };                                                                 |    |
|  |  DECLARE_DLIST(item_list, struct item, entry);                      |    |
|  |                                                                     |    |
|  |  struct item_list_head head;                                        |    |
|  |  item_list_init(&head);                                             |    |
|  |  item_list_add_tail(&head, new_item);                               |    |
|  |  item_list_del(&head, item);                                        |    |
|  |  frr_each(item_list, &head, item) { ... }                           |    |
|  |  frr_each_safe(item_list, &head, item) { ... }  // allows delete    |    |
|  |  item_list_fini(&head);                                             |    |
|  |                                                                     |    |
|  |  // Hash table                                                      |    |
|  |  PREDECL_HASH(item_hash);                                           |    |
|  |  struct item { struct item_hash_item hentry; ... };                 |    |
|  |  DECLARE_HASH(item_hash, struct item, hentry, cmpfn, hashfn);       |    |
|  |                                                                     |    |
|  |  // Min-heap (priority queue)                                       |    |
|  |  PREDECL_HEAP(timer_heap);                                          |    |
|  |  struct timer { struct timer_heap_item hentry; ... };               |    |
|  |  DECLARE_HEAP(timer_heap, struct timer, hentry, cmpfn);             |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  5. PREFIX/ADDRESS API                                                      |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  // Parse                                                           |    |
|  |  struct prefix p;                                                   |    |
|  |  str2prefix("10.0.0.0/8", &p);                                      |    |
|  |  str2prefix_ipv4("192.168.1.0/24", (struct prefix_ipv4 *)&p);       |    |
|  |  str2prefix_ipv6("2001:db8::/32", (struct prefix_ipv6 *)&p);        |    |
|  |                                                                     |    |
|  |  // Format                                                          |    |
|  |  char buf[PREFIX_STRLEN];                                           |    |
|  |  prefix2str(&p, buf, sizeof(buf));                                  |    |
|  |                                                                     |    |
|  |  // Compare                                                         |    |
|  |  prefix_same(&p1, &p2)           // exact match                     |    |
|  |  prefix_match(&network, &host)   // network contains host           |    |
|  |  prefix_cmp(&p1, &p2)            // -1, 0, 1                        |    |
|  |                                                                     |    |
|  |  // Manipulate                                                      |    |
|  |  apply_mask(&p);                 // zero host bits                  |    |
|  |  prefix_copy(&dst, &src);                                           |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
|  6. STREAM API                                                              |    
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  // Create                                                          |    |
|  |  struct stream *s = stream_new(4096);                               |    |
|  |                                                                     |    |
|  |  // Write (network byte order)                                      |    |
|  |  stream_putc(s, byte);           // 1 byte                          |    |
|  |  stream_putw(s, word);           // 2 bytes                         |    |
|  |  stream_putl(s, dword);          // 4 bytes                         |    |
|  |  stream_putq(s, qword);          // 8 bytes                         |    |
|  |  stream_put(s, data, len);       // raw bytes                       |    |
|  |  stream_put_prefix(s, &prefix);  // IP prefix                       |    |
|  |                                                                     |    |
|  |  // Read (network byte order)                                       |    |
|  |  byte = stream_getc(s);                                             |    |
|  |  word = stream_getw(s);                                             |    |
|  |  dword = stream_getl(s);                                            |    |
|  |  qword = stream_getq(s);                                            |    |
|  |  stream_get(s, buf, len);                                           |    |
|  |                                                                     |    |
|  |  // Control                                                         |    |
|  |  stream_reset(s);                // reset for reuse                 |    |
|  |  STREAM_DATA(s)                  // get data pointer                |    |
|  |  stream_get_endp(s)              // get data length                 |    |
|  |                                                                     |    |
|  |  // Cleanup                                                         |    |
|  |  stream_free(s);                                                    |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  COMMON MISUSE PATTERNS                                                     |
|  ======================                                                     |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  MISTAKE 1: Forgetting event_cancel before object destruction       |    |
|  |  -----------------------------------------------------------        |    |
|  |                                                                     |    |
|  |  BAD:                                                               |    |
|  |  void destroy_peer(struct peer *p) {                                |    |
|  |      XFREE(MTYPE_PEER, p);  // Timer still references p!            |    |
|  |  }                                                                  |    |
|  |                                                                     |    |
|  |  GOOD:                                                              |    |
|  |  void destroy_peer(struct peer *p) {                                |    |
|  |      event_cancel(&p->keepalive_timer);  // Cancel first            |    |
|  |      event_cancel(&p->hold_timer);                                  |    |
|  |      XFREE(MTYPE_PEER, p);                                          |    |
|  |  }                                                                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  MISTAKE 2: Using wrong MTYPE for allocation                        |    |
|  |  -----------------------------------------------                    |    |
|  |                                                                     |    |
|  |  BAD:                                                               |    |
|  |  peer = XMALLOC(MTYPE_TMP, sizeof(*peer));  // Wrong type!          |    |
|  |  XFREE(MTYPE_BGP_PEER, peer);               // Mismatched!          |    |
|  |                                                                     |    |
|  |  GOOD:                                                              |    |
|  |  peer = XMALLOC(MTYPE_BGP_PEER, sizeof(*peer));                     |    |
|  |  XFREE(MTYPE_BGP_PEER, peer);               // Matched              |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  MISTAKE 3: Not checking stream bounds                              |    |
|  |  ---------------------------------------                            |    |
|  |                                                                     |    |
|  |  BAD:                                                               |    |
|  |  value = stream_getl(s);  // May read past end!                     |    |
|  |                                                                     |    |
|  |  GOOD:                                                              |    |
|  |  if (STREAM_READABLE(s) >= 4)                                       |    |
|  |      value = stream_getl(s);                                        |    |
|  |  else                                                               |    |
|  |      return ERROR;                                                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  MISTAKE 4: Blocking in event callbacks                             |    |
|  |  ------------------------------------------                         |    |
|  |                                                                     |    |
|  |  BAD:                                                               |    |
|  |  void my_callback(struct event *e) {                                |    |
|  |      sleep(5);  // Blocks entire event loop!                        |    |
|  |      result = sync_dns_lookup(name);  // Also blocking!             |    |
|  |  }                                                                  |    |
|  |                                                                     |    |
|  |  GOOD:                                                              |    |
|  |  void my_callback(struct event *e) {                                |    |
|  |      // Quick processing only                                       |    |
|  |      // Use async operations or reschedule                          |    |
|  |      event_add_timer(master, retry_later, ctx, 5, &t);              |    |
|  |  }                                                                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  MISTAKE 5: Modifying container while iterating                     |    |
|  |  -----------------------------------------------                    |    |
|  |                                                                     |    |
|  |  BAD:                                                               |    |
|  |  frr_each(item_list, &head, item) {                                 |    |
|  |      if (should_delete(item))                                       |    |
|  |          item_list_del(&head, item);  // Corrupts iterator!         |    |
|  |  }                                                                  |    |
|  |                                                                     |    |
|  |  GOOD:                                                              |    |
|  |  frr_each_safe(item_list, &head, item) {  // Safe version           |    |
|  |      if (should_delete(item))                                       |    |
|  |          item_list_del(&head, item);  // OK                         |    |
|  |  }                                                                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细解释

### API 分类

FRR 的 lib/ API 可以分为两类：

**稳定/外部使用 API：**
- 设计用于被多个守护进程使用
- 接口较少变化
- 有文档或明确的使用模式
- 适合在外部项目中重用

**内部/守护进程特定 API：**
- 仅在 FRR 内部使用
- 可能随版本变化
- 依赖 FRR 特定的上下文
- 不应在外部项目中直接使用

### 1. 事件系统 API

事件系统是最容易重用的 API：

```c
// 创建事件循环
struct event_loop *master = event_master_create("myapp");

// 添加读事件
void read_callback(struct event *e) {
    int fd = EVENT_FD(e);
    void *data = EVENT_ARG(e);
    // 处理读取
}
event_add_read(master, read_callback, mydata, sockfd, &read_event);

// 添加定时器（10秒后触发）
void timer_callback(struct event *e) {
    // 定时器逻辑
}
event_add_timer(master, timer_callback, mydata, 10, &timer_event);

// 主循环
struct event event;
while (event_fetch(master, &event))
    event_call(&event);

// 清理
event_master_free(master);
```

**要点：**
- `event_add_*` 函数通过最后一个参数返回事件句柄
- 在销毁关联对象前必须调用 `event_cancel`
- 事件句柄被取消或执行后自动置为 NULL

### 2. 内存系统 API

内存系统提供类型化分配和泄漏检测：

```c
// 定义内存类型
DECLARE_MGROUP(MYAPP);
DECLARE_MTYPE(MYAPP_CONN);

DEFINE_MGROUP(MYAPP, "My Application");
DEFINE_MTYPE(MYAPP, MYAPP_CONN, "Connection structure");

// 分配
struct connection *conn;
conn = XMALLOC(MTYPE_MYAPP_CONN, sizeof(*conn));
// 或者零初始化
conn = XCALLOC(MTYPE_MYAPP_CONN, sizeof(*conn));

// 释放
XFREE(MTYPE_MYAPP_CONN, conn);  // conn 现在是 NULL
```

**要点：**
- XFREE 自动将指针置 NULL，防止悬空指针
- 分配和释放必须使用相同的 MTYPE
- 关闭时 log_memstats() 报告泄漏

### 3. 日志 API

日志系统支持多种输出目标：

```c
// 基本日志
zlog_err("Connection failed: %s", strerror(errno));
zlog_warn("Timeout occurred, retrying");
zlog_info("Processing request from %s", client);
zlog_debug("Internal state: %d", state);

// 带错误码的日志
flog_err(EC_MYAPP_CONN_FAILED, "Connection to %s failed", host);

// FRR 格式扩展
struct prefix p;
zlog_info("Route: %pFX", &p);  // 输出前缀

struct in_addr addr;
zlog_info("Address: %pI4", &addr);  // 输出 IPv4 地址
```

**要点：**
- 日志级别应遵循 syslog 约定
- 使用 `flog_*` 进行结构化错误报告
- FRR 扩展了 printf 格式（%pFX, %pI4 等）

### 4. 数据结构 API

typesafe.h 提供类型安全的容器：

```c
// 定义双向链表
PREDECL_DLIST(peer_list);
struct peer {
    struct peer_list_item entry;  // 嵌入节点
    char *name;
};
DECLARE_DLIST(peer_list, struct peer, entry);

// 使用
struct peer_list_head peers;
peer_list_init(&peers);

// 添加
struct peer *p = create_peer();
peer_list_add_tail(&peers, p);

// 遍历
struct peer *item;
frr_each(peer_list, &peers, item) {
    printf("Peer: %s\n", item->name);
}

// 安全删除（遍历时）
frr_each_safe(peer_list, &peers, item) {
    if (should_delete(item)) {
        peer_list_del(&peers, item);
        destroy_peer(item);
    }
}
```

### 5. 前缀/地址 API

处理 IP 前缀的核心工具：

```c
struct prefix p;

// 从字符串解析
if (str2prefix("10.0.0.0/8", &p) == 0) {
    zlog_err("Invalid prefix");
}

// 转换为字符串
char buf[PREFIX_STRLEN];
prefix2str(&p, buf, sizeof(buf));

// 比较
if (prefix_same(&p1, &p2)) {
    // 完全相同
}

// 匹配
struct prefix network, host;
str2prefix("10.0.0.0/8", &network);
str2prefix("10.1.2.3/32", &host);
if (prefix_match(&network, &host)) {
    // host 在 network 范围内
}
```

### 6. Stream API

用于构建和解析网络消息：

```c
// 创建
struct stream *s = stream_new(4096);

// 写入（自动网络字节序）
stream_putw(s, htons(MSG_TYPE));  // 不需要，putw 已经是网络字节序
stream_putw(s, MSG_TYPE);         // 正确方式
stream_putl(s, length);
stream_put(s, payload, payload_len);

// 发送
write(sock, STREAM_DATA(s), stream_get_endp(s));

// 接收和解析
stream_reset(s);
read(sock, STREAM_DATA(s), 1024);
stream_set_endp(s, bytes_read);

uint16_t type = stream_getw(s);
uint32_t len = stream_getl(s);

// 清理
stream_free(s);
```

### 常见误用模式

**错误 1：忘记取消事件**

```c
// 错误：对象被释放但定时器仍然存在
void destroy_connection(struct conn *c) {
    XFREE(MTYPE_CONN, c);  // 定时器回调会崩溃！
}

// 正确：先取消所有事件
void destroy_connection(struct conn *c) {
    event_cancel(&c->read_event);
    event_cancel(&c->timeout);
    XFREE(MTYPE_CONN, c);
}
```

**错误 2：MTYPE 不匹配**

```c
// 错误：分配和释放使用不同类型
p = XMALLOC(MTYPE_TMP, sizeof(*p));
XFREE(MTYPE_PEER, p);  // 内存统计将不正确

// 正确：始终使用相同类型
p = XMALLOC(MTYPE_PEER, sizeof(*p));
XFREE(MTYPE_PEER, p);
```

**错误 3：不检查 stream 边界**

```c
// 错误：可能读取超出边界
value = stream_getl(s);

// 正确：检查可读字节数
if (STREAM_READABLE(s) >= 4)
    value = stream_getl(s);
else
    return -1;  // 错误处理
```

**错误 4：在回调中阻塞**

```c
// 错误：阻塞整个事件循环
void callback(struct event *e) {
    result = sync_http_request(url);  // 可能阻塞数秒
    sleep(1);  // 绝对不要这样做
}

// 正确：使用异步操作或重新调度
void callback(struct event *e) {
    // 快速处理
    // 如需延迟，使用定时器
    event_add_timer_msec(master, retry, ctx, 100, &t);
}
```

**错误 5：遍历时修改容器**

```c
// 错误：可能导致迭代器失效
frr_each(my_list, &head, item) {
    if (condition)
        my_list_del(&head, item);  // 迭代器损坏
}

// 正确：使用安全遍历
frr_each_safe(my_list, &head, item) {
    if (condition)
        my_list_del(&head, item);  // 安全
}
```

### API 稳定性总结

| API | 稳定性 | 外部使用 | 注意事项 |
|-----|--------|----------|----------|
| event_* | 高 | ✓ | 需要 libfrr |
| XMALLOC/XFREE | 高 | ✓ | 需要 libfrr |
| zlog_* | 高 | ✓ | 需要 libfrr |
| typesafe.h | 高 | ✓ | 头文件即可 |
| prefix_* | 高 | ✓ | 需要 libfrr |
| stream_* | 高 | ✓ | 需要 libfrr |
| zclient_* | 中 | ⚠ | 仅与 Zebra |
| vrf_* | 中 | ⚠ | 需要 Zebra |
| nb_* | 低 | ✗ | FRR 内部 |
| vty_* | 低 | ⚠ | 复杂依赖 |
