# Extracting FRR Architecture - Part 3: WHAT

## Concrete Architectural Building Blocks to Extract

This document identifies the minimum viable building blocks needed for
an FRR-inspired C architecture and explains how to design each.

---

## 1. Mandatory Modules Mapping

```
+===========================================================================+
|                    FRR → GENERIC MODULE MAPPING                           |
+===========================================================================+

  +------------------------------------------------------------------+
  |  FRR Module  |  Generic Name     |  Responsibility              |
  +------------------------------------------------------------------+
  |  lib/        |  core/            |  Runtime infrastructure      |
  |  zebra/      |  state/           |  State authority             |
  |  bgpd,ospfd  |  engines/         |  Decision engines            |
  |  netlink     |  backend/         |  Execution backend           |
  |  vtysh,mgmtd |  mgmt/            |  Management plane            |
  +------------------------------------------------------------------+


                    MINIMUM VIABLE ARCHITECTURE
                    ===========================

  +------------------------------------------------------------------------+
  |                                                                        |
  |   mgmt/         - CLI, config, monitoring                              |
  |      |                                                                 |
  |      v                                                                 |
  |   +--------------------+                                               |
  |   |   CORE RUNTIME     |  <-- core/                                    |
  |   |                    |                                               |
  |   | - Event loop       |                                               |
  |   | - Memory mgmt      |                                               |
  |   | - Logging          |                                               |
  |   | - Timers           |                                               |
  |   +--------------------+                                               |
  |            |                                                           |
  |            v                                                           |
  |   +--------------------+        +--------------------+                 |
  |   |   STATE MANAGER    |<------>|  DECISION ENGINES  |                 |
  |   |                    |        |                    |                 |
  |   | - Authoritative    |  state | - Plugin A         |                 |
  |   |   state store      | queries| - Plugin B         |                 |
  |   | - Request queue    |        | - Plugin C         |                 |
  |   | - Conflict resolver|        |                    |                 |
  |   +--------------------+        +--------------------+                 |
  |            |                                                           |
  |            | Execution commands                                        |
  |            v                                                           |
  |   +--------------------+                                               |
  |   | EXECUTION BACKEND  |  <-- backend/                                 |
  |   |                    |                                               |
  |   | - Actual I/O       |                                               |
  |   | - System calls     |                                               |
  |   | - External comms   |                                               |
  |   +--------------------+                                               |
  |                                                                        |
  +------------------------------------------------------------------------+


  MODULE RESPONSIBILITIES (Minimum Viable):
  =========================================

  core/ - Runtime Infrastructure
  +------------------------------------------------------------------+
  | MUST HAVE:                                                       |
  | - Event loop (poll-based)                                        |
  | - Timer management                                               |
  | - Logging framework                                              |
  | - Memory allocation wrappers                                     |
  |                                                                  |
  | NICE TO HAVE:                                                    |
  | - RCU for safe cleanup (like frrcu.h)                           |
  | - Debug/trace infrastructure                                     |
  | - Profiling hooks                                                |
  +------------------------------------------------------------------+

  state/ - State Manager
  +------------------------------------------------------------------+
  | MUST HAVE:                                                       |
  | - Central state store                                            |
  | - Request/intent queue                                           |
  | - State change notification                                      |
  |                                                                  |
  | NICE TO HAVE:                                                    |
  | - Transaction support                                            |
  | - Snapshot/rollback                                              |
  | - Persistence                                                    |
  +------------------------------------------------------------------+

  engines/ - Decision Engines
  +------------------------------------------------------------------+
  | MUST HAVE:                                                       |
  | - Standard engine interface                                      |
  | - Registration mechanism                                         |
  | - Lifecycle management (init/shutdown)                           |
  |                                                                  |
  | NICE TO HAVE:                                                    |
  | - Hot reload                                                     |
  | - Health monitoring                                              |
  | - Resource limits                                                |
  +------------------------------------------------------------------+

  backend/ - Execution Backend
  +------------------------------------------------------------------+
  | MUST HAVE:                                                       |
  | - Command queue                                                  |
  | - Sync/async execution                                           |
  | - Result notification                                            |
  |                                                                  |
  | NICE TO HAVE:                                                    |
  | - Batching                                                       |
  | - Retry logic                                                    |
  | - Multiple backend types                                         |
  +------------------------------------------------------------------+

  mgmt/ - Management Plane
  +------------------------------------------------------------------+
  | MUST HAVE:                                                       |
  | - Configuration interface                                        |
  | - Status queries                                                 |
  | - Basic monitoring                                               |
  |                                                                  |
  | NICE TO HAVE:                                                    |
  | - CLI                                                            |
  | - REST/gRPC API                                                  |
  | - Metrics export                                                 |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 3.1 模块职责最小化定义

**core/ - 运行时基础设施**

```c
// 必须有：事件循环
struct event_loop {
    int epoll_fd;
    struct event *events;
    int running;
};

int event_loop_run(struct event_loop *loop);
int event_add_fd(struct event_loop *loop, int fd, int events, 
                 event_handler_t handler, void *data);
int event_add_timer(struct event_loop *loop, int ms, 
                    timer_handler_t handler, void *data);

// 必须有：日志
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_error(const char *fmt, ...);

// 必须有：内存管理（带追踪）
void *mem_alloc(size_t size, const char *type);
void mem_free(void *ptr);
void mem_stats(void);  // 打印内存使用统计
```

**state/ - 状态管理器**

```c
// 必须有：状态存储
struct state_manager;

struct state_manager *state_manager_create(void);
void state_manager_destroy(struct state_manager *sm);

// 必须有：请求/意图队列
int state_submit_request(struct state_manager *sm, 
                         const struct request *req);

// 必须有：变更通知
typedef void (*state_change_cb)(const struct state_delta *delta, void *arg);
int state_subscribe(struct state_manager *sm, state_change_cb cb, void *arg);
```

**engines/ - 决策引擎**

```c
// 必须有：标准接口
struct engine_ops {
    int (*init)(struct engine *e, const struct engine_config *cfg);
    int (*shutdown)(struct engine *e);
    int (*on_state_change)(struct engine *e, const struct state_delta *d);
};

// 必须有：注册机制
int engine_register(const char *name, const struct engine_ops *ops);
int engine_start(const char *name);
int engine_stop(const char *name);
```

**backend/ - 执行后端**

```c
// 必须有：命令队列
int backend_submit(struct backend *b, const struct command *cmd);

// 必须有：结果通知
typedef void (*result_cb)(const struct command *cmd, int result, void *arg);
int backend_set_callback(struct backend *b, result_cb cb, void *arg);
```

---

## 2. Core Data Structure Design

```
+===========================================================================+
|                    DATA STRUCTURE PATTERNS                                |
+===========================================================================+

                    LONG-LIVED STATE OBJECTS
                    ========================

  // Pattern: Lifecycle-aware objects
  struct resource {
      // Identity
      uint64_t id;
      char name[MAX_NAME];
      
      // Lifecycle state
      enum resource_state state;
      time_t created_at;
      time_t updated_at;
      
      // Ownership
      uint32_t owner_engine_id;
      
      // For RCU or refcounting
      union {
          struct rcu_head rcu;
          atomic_int refcount;
      };
      
      // Linkage (for containers)
      struct list_node node;
      
      // Domain-specific data
      struct resource_data data;
  };


                    OWNERSHIP-AWARE STRUCTURES
                    ==========================

  // Who owns this object?
  enum ownership {
      OWNER_NONE,          // Orphaned (error state)
      OWNER_STATE_MGR,     // State manager owns
      OWNER_ENGINE,        // Specific engine owns
      OWNER_BACKEND,       // Backend owns (during execution)
      OWNER_TRANSITIONAL   // Being transferred
  };

  struct owned_object {
      enum ownership owner;
      uint32_t owner_id;   // e.g., engine ID
      
      // Ownership transfer tracking
      uint64_t transfer_id;
      time_t transfer_start;
  };


                    REFERENCE COUNTING PATTERN
                    ==========================

  struct refcounted {
      atomic_int refcount;
      void (*destructor)(struct refcounted *);
  };

  static inline void ref_get(struct refcounted *r) {
      atomic_fetch_add(&r->refcount, 1);
  }

  static inline void ref_put(struct refcounted *r) {
      if (atomic_fetch_sub(&r->refcount, 1) == 1) {
          r->destructor(r);
      }
  }


                    RCU PATTERN (from frrcu.h)
                    ==========================

  // For read-heavy, write-rare data
  struct rcu_protected {
      struct rcu_head rcu;
      // ... data ...
  };

  // Reading (no locks, just mark "I'm reading")
  rcu_read_lock();
  struct rcu_protected *p = atomic_load(&global_ptr);
  // use p safely
  rcu_read_unlock();

  // Writing (deferred free)
  struct rcu_protected *old = atomic_exchange(&global_ptr, new);
  rcu_free(MTYPE, old, rcu);  // Freed after all readers done


                    INTENT VS STATE SEPARATION
                    ==========================

  +------------------------------------------------------------------+
  |  INTENT (immutable)                                              |
  +------------------------------------------------------------------+
  |  - What the user/engine WANTS                                    |
  |  - Stored as-is from configuration                               |
  |  - Never modified during runtime                                 |
  |  - Can always be replayed                                        |
  |                                                                  |
  |  struct intent {                                                 |
  |      uint64_t id;                                                |
  |      time_t submitted_at;                                        |
  |      uint32_t source_engine;                                     |
  |      struct intent_data data;  // Immutable                      |
  |  };                                                              |
  +------------------------------------------------------------------+

  +------------------------------------------------------------------+
  |  STATE (mutable, derived)                                        |
  +------------------------------------------------------------------+
  |  - Current actual state                                          |
  |  - Derived from applying intents                                 |
  |  - May differ from intent (during reconciliation)                |
  |  - Discardable (can rebuild from intents)                        |
  |                                                                  |
  |  struct state {                                                  |
  |      uint64_t intent_id;      // Which intent this came from     |
  |      enum reconcile_status status;  // pending/applied/failed    |
  |      struct state_data data;  // Current actual state            |
  |  };                                                              |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 3.2 核心数据结构设计

**生命周期感知对象：**

```c
// 每个重要对象都应该有：
struct object {
    // 1. 身份标识
    uint64_t id;           // 唯一 ID
    char name[64];         // 人类可读名称
    
    // 2. 生命周期状态
    enum state {
        STATE_CREATING,    // 正在创建
        STATE_ACTIVE,      // 正常使用
        STATE_UPDATING,    // 正在更新
        STATE_DELETING,    // 正在删除
        STATE_DELETED      // 已删除（等待清理）
    } state;
    
    // 3. 时间追踪
    time_t created_at;
    time_t updated_at;
    
    // 4. 所有权
    uint32_t owner_id;     // 谁拥有这个对象
    
    // 5. 内存管理辅助
    atomic_int refcount;   // 引用计数
    // 或
    struct rcu_head rcu;   // RCU 延迟释放
};
```

**意图 vs 状态分离：**

```
意图（Intent）：
- 用户/引擎想要什么
- 配置时记录，不可变
- 例如："我想要路由 10.0.0.0/8 经过 192.168.1.1"

状态（State）：
- 当前实际情况
- 运行时变化
- 例如："路由 10.0.0.0/8 已安装到内核"

为什么分离？
1. 可以知道"期望 vs 实际"
2. 可以重放意图重建状态
3. 可以检测偏差并调和
```

---

## 3. Message-Based Interaction Model

```
+===========================================================================+
|                    MESSAGE PASSING DESIGN                                 |
+===========================================================================+

                    WHY MESSAGE PASSING?
                    ====================

  Direct Call (Tight Coupling):
  +------------+          +------------+
  |  Module A  |---call-->|  Module B  |
  +------------+          +------------+
       |                        |
       | A knows B's interface  |
       | A blocks waiting for B |
       | A and B cannot be      |
       |   separated easily     |

  Message Passing (Loose Coupling):
  +------------+    +-------+    +------------+
  |  Module A  |--->| Queue |--->|  Module B  |
  +------------+    +-------+    +------------+
       |                              |
       | A only knows message format  |
       | A doesn't block              |
       | A and B can be separate      |
       |   processes later            |


                    MESSAGE QUEUE IMPLEMENTATION
                    ============================

  // Simple lock-free queue (single producer, single consumer)
  struct message_queue {
      struct message *buffer;
      size_t capacity;
      atomic_size_t head;  // Producer writes here
      atomic_size_t tail;  // Consumer reads here
  };

  int mq_send(struct message_queue *q, const struct message *msg) {
      size_t head = atomic_load(&q->head);
      size_t next = (head + 1) % q->capacity;
      
      if (next == atomic_load(&q->tail)) {
          return -EAGAIN;  // Queue full
      }
      
      q->buffer[head] = *msg;
      atomic_store(&q->head, next);
      return 0;
  }

  int mq_recv(struct message_queue *q, struct message *msg) {
      size_t tail = atomic_load(&q->tail);
      
      if (tail == atomic_load(&q->head)) {
          return -EAGAIN;  // Queue empty
      }
      
      *msg = q->buffer[tail];
      atomic_store(&q->tail, (tail + 1) % q->capacity);
      return 0;
  }


                    MESSAGE TYPES
                    =============

  // Generic message header
  struct msg_header {
      uint32_t type;       // Message type
      uint32_t len;        // Total length
      uint64_t seq;        // Sequence number
      uint64_t timestamp;  // When sent
      uint32_t src;        // Source module
      uint32_t dst;        // Destination module
  };

  // Example message types
  enum msg_type {
      MSG_REQUEST,         // "Please do X"
      MSG_RESPONSE,        // "X done (or failed)"
      MSG_NOTIFICATION,    // "Y happened"
      MSG_QUERY,           // "What is Z?"
      MSG_QUERY_RESPONSE,  // "Z is W"
  };

  // Request message
  struct msg_request {
      struct msg_header hdr;
      uint32_t request_type;
      uint8_t data[];      // Variable length
  };


                    ASYNC PATTERN
                    =============

  Sender:
  +------------------------------------------------------------------+
  |  struct msg_request req = {                                      |
  |      .hdr.type = MSG_REQUEST,                                    |
  |      .hdr.seq = next_seq++,                                      |
  |      .request_type = REQ_ADD_RESOURCE,                           |
  |  };                                                              |
  |  memcpy(req.data, &resource_data, sizeof(resource_data));        |
  |                                                                  |
  |  mq_send(state_mgr_queue, &req);                                 |
  |                                                                  |
  |  // Store pending request                                        |
  |  pending_add(req.hdr.seq, callback, context);                    |
  +------------------------------------------------------------------+

  Receiver (State Manager):
  +------------------------------------------------------------------+
  |  while (mq_recv(queue, &msg) == 0) {                             |
  |      switch (msg.hdr.type) {                                     |
  |      case MSG_REQUEST:                                           |
  |          result = handle_request(&msg);                          |
  |          send_response(msg.hdr.src, msg.hdr.seq, result);        |
  |          break;                                                  |
  |      // ...                                                      |
  |      }                                                           |
  |  }                                                               |
  +------------------------------------------------------------------+

  Sender (receiving response):
  +------------------------------------------------------------------+
  |  while (mq_recv(response_queue, &msg) == 0) {                    |
  |      struct pending *p = pending_find(msg.hdr.seq);              |
  |      if (p) {                                                    |
  |          p->callback(p->context, msg.result);                    |
  |          pending_remove(msg.hdr.seq);                            |
  |      }                                                           |
  |  }                                                               |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 3.3 消息交互模型

**为什么使用消息传递？**

```
直接调用的问题：
1. 紧耦合 - A 必须知道 B 的接口
2. 阻塞 - A 等待 B 完成
3. 难以分离 - 无法把 B 移到单独进程

消息传递的好处：
1. 松耦合 - A 只需知道消息格式
2. 异步 - A 发送后继续工作
3. 可扩展 - 可以把队列变成网络通信
```

**消息队列实现要点：**

```c
// 简单的 SPSC (Single Producer Single Consumer) 队列
// 特点：
// - 无锁（使用原子操作）
// - 固定大小缓冲区
// - 生产者和消费者可以在不同线程

struct ring_buffer {
    struct message *buffer;   // 消息数组
    size_t capacity;          // 容量（2的幂更好）
    _Atomic size_t head;      // 写入位置
    _Atomic size_t tail;      // 读取位置
};

// 发送：写入 head，推进 head
// 接收：读取 tail，推进 tail
// 满：(head + 1) % cap == tail
// 空：head == tail
```

---

## 4. Configuration vs Runtime State

```
+===========================================================================+
|                    CONFIG VS RUNTIME SEPARATION                           |
+===========================================================================+

                    THE PROBLEM
                    ===========

  WRONG: Config directly mutates runtime
  +------------------------------------------------------------------+
  |  void set_config(const char *key, const char *value) {           |
  |      if (strcmp(key, "max_connections") == 0) {                  |
  |          global_max_conn = atoi(value);  // Direct mutation!     |
  |          // What about existing connections?                     |
  |          // What if value is invalid?                            |
  |          // How to rollback?                                     |
  |      }                                                           |
  |  }                                                               |
  +------------------------------------------------------------------+


                    THE SOLUTION
                    ============

  +------------------------------------------------------------------+
  |                                                                  |
  |  +----------------+                                              |
  |  | Configuration  |  <-- Declarative intent                      |
  |  | (What we want) |                                              |
  |  +----------------+                                              |
  |         |                                                        |
  |         | Validate & Parse                                       |
  |         v                                                        |
  |  +----------------+                                              |
  |  | Intent Store   |  <-- Immutable records                       |
  |  | (Parsed config)|                                              |
  |  +----------------+                                              |
  |         |                                                        |
  |         | Reconcile                                              |
  |         v                                                        |
  |  +----------------+                                              |
  |  | Runtime State  |  <-- Mutable, derived                        |
  |  | (Actual state) |                                              |
  |  +----------------+                                              |
  |                                                                  |
  +------------------------------------------------------------------+


                    RECONCILIATION PATTERN
                    ======================

  // Intent (from config)
  struct config_intent {
      int max_connections;
      int timeout_sec;
      char server_name[64];
  };

  // Runtime state
  struct runtime_state {
      int current_max_connections;
      int current_timeout;
      // ... plus transient state
      int active_connections;
      struct connection *connections;
  };

  // Reconciliation function
  void reconcile(struct runtime_state *rt, 
                 const struct config_intent *intent) {
      
      // Compare and update each field
      if (rt->current_max_connections != intent->max_connections) {
          log_info("Updating max_connections: %d -> %d",
                   rt->current_max_connections, intent->max_connections);
          
          // Handle active connections if reducing
          if (intent->max_connections < rt->active_connections) {
              graceful_close_excess(rt, intent->max_connections);
          }
          
          rt->current_max_connections = intent->max_connections;
      }
      
      if (rt->current_timeout != intent->timeout_sec) {
          log_info("Updating timeout: %d -> %d",
                   rt->current_timeout, intent->timeout_sec);
          rt->current_timeout = intent->timeout_sec;
          // Update active connection timers
          update_connection_timeouts(rt, intent->timeout_sec);
      }
  }


                    REPLAYABLE CONFIG
                    =================

  // Config is a sequence of intents
  struct config_entry {
      uint64_t sequence;    // Order
      time_t timestamp;     // When applied
      char key[64];
      char value[256];
  };

  // Full state can be rebuilt by replaying
  void rebuild_from_config(struct runtime_state *rt,
                           struct config_entry *entries,
                           size_t count) {
      // Clear state
      memset(rt, 0, sizeof(*rt));
      
      // Replay each config entry in order
      for (size_t i = 0; i < count; i++) {
          apply_config_entry(rt, &entries[i]);
      }
  }
```

---

## 中文解释 (Chinese Explanation)

### 3.4 配置与运行时状态分离

**错误做法：配置直接修改运行时**

```c
// 问题：
void set_config(const char *key, const char *value) {
    if (strcmp(key, "port") == 0) {
        server_port = atoi(value);  // 直接修改
        // 问题：
        // 1. 正在监听旧端口怎么办？
        // 2. value 无效怎么办？
        // 3. 需要回滚怎么办？
    }
}
```

**正确做法：配置 → 意图 → 调和 → 状态**

```c
// 1. 配置解析（只验证，不执行）
int parse_config(const char *file, struct config_intent *intent) {
    // 读取文件
    // 验证格式和值
    // 填充 intent 结构
    // 返回成功/失败
}

// 2. 意图存储（不可变）
void store_intent(struct intent_store *store, 
                  const struct config_intent *intent) {
    // 保存意图记录
    // 分配序列号
    // 记录时间戳
}

// 3. 调和（对比意图和实际，执行变更）
void reconcile(struct runtime *rt, 
               const struct config_intent *intent) {
    // 对比每个字段
    // 生成变更计划
    // 执行变更（可能异步）
    // 更新运行时状态
}
```

**可重放配置的好处：**

```
1. 可审计
   - 知道什么时候改了什么
   - 谁改的

2. 可回滚
   - 重放到某个时间点
   - 撤销某次变更

3. 可测试
   - 重放配置序列
   - 验证最终状态

4. 可诊断
   - 当前状态来自哪些配置
   - 配置变更历史
```

---

## 5. Summary: Minimum Viable Modules

| Module | Minimum Implementation | Lines of Code (est.) |
|--------|------------------------|----------------------|
| core/event.c | poll-based event loop | ~200 |
| core/log.c | Level-based logging | ~100 |
| core/memory.c | Tracked malloc/free | ~150 |
| state/state_mgr.c | Hash table + request queue | ~400 |
| engines/engine.c | Registration + lifecycle | ~200 |
| backend/backend.c | Command queue + execution | ~300 |
| mgmt/config.c | Simple key-value config | ~200 |

**Total minimal implementation: ~1500 lines of C**

This is your starting point. Add complexity only as needed.

---

## Next: Part 4 - WHERE (Mapping to Your Codebase)
