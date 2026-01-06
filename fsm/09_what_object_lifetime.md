# WHAT | Modeling Object Lifetime with FSMs

## 1. Lifecycle FSMs

```
THE UNIVERSAL OBJECT LIFECYCLE
==============================

Every non-trivial object goes through phases:

  +----------+     +---------------+     +---------+
  | CREATED  | --> | INITIALIZING  | --> | RUNNING |
  +----------+     +---------------+     +---------+
                                              |
       +----------------------------------+   |
       |                                  |   |
       v                                  v   v
  +-----------+     +-----------+     +----------+
  | DESTROYED | <-- |  STOPPED  | <-- | STOPPING |
  +-----------+     +-----------+     +----------+


This is NOT just for network connections.
This applies to:
  - Database handles
  - File handles
  - Thread pool instances
  - Plugin objects
  - Hardware interfaces
  - Any resource with setup/teardown
```

```
LIFECYCLE STATES EXPLAINED
==========================

CREATED:
  - Memory allocated
  - Initial values set (zero or defaults)
  - NO external resources acquired
  - NOT ready for use
  
INITIALIZING:
  - Acquiring external resources
  - Opening files/sockets
  - Allocating buffers
  - May fail → ERROR state

RUNNING (or READY/ACTIVE):
  - Fully operational
  - Can process requests
  - Resources fully acquired
  - Normal operation mode

STOPPING (or SHUTTING_DOWN):
  - Received shutdown signal
  - Finishing pending work
  - Rejecting new requests
  - Graceful wind-down

STOPPED:
  - No pending work
  - Ready for cleanup
  - Resources still held

DESTROYED:
  - All resources released
  - Memory freed
  - Object no longer usable
```

```c
/* Lifecycle FSM Example: Database Connection */

typedef enum {
    DB_STATE_CREATED,      /* Object exists, not initialized */
    DB_STATE_CONNECTING,   /* Connecting to database */
    DB_STATE_READY,        /* Can execute queries */
    DB_STATE_BUSY,         /* Query in progress */
    DB_STATE_STOPPING,     /* Shutdown initiated */
    DB_STATE_STOPPED,      /* Stopped, resources held */
    DB_STATE_ERROR,        /* Error state */
    DB_STATE_COUNT
} db_state_t;

typedef enum {
    DB_EVENT_INIT,         /* Start initialization */
    DB_EVENT_CONNECTED,    /* Connection established */
    DB_EVENT_QUERY,        /* Execute query request */
    DB_EVENT_QUERY_DONE,   /* Query completed */
    DB_EVENT_SHUTDOWN,     /* Start shutdown */
    DB_EVENT_STOPPED,      /* Shutdown complete */
    DB_EVENT_DESTROY,      /* Release all resources */
    DB_EVENT_ERROR,        /* Error occurred */
    DB_EVENT_COUNT
} db_event_t;

typedef struct {
    db_state_t state;
    int socket;
    char *host;
    int port;
    void *query_result;
    int error_code;
} db_conn_t;

/* Creation (not an FSM event, just allocation) */
db_conn_t *db_create(const char *host, int port) {
    db_conn_t *db = calloc(1, sizeof(*db));
    if (!db) return NULL;
    
    db->state = DB_STATE_CREATED;
    db->socket = -1;
    db->host = strdup(host);
    db->port = port;
    
    return db;  /* In CREATED state, NOT connected */
}

/* Destruction (after FSM reaches STOPPED) */
void db_destroy(db_conn_t *db) {
    if (db->state != DB_STATE_STOPPED) {
        /* Must be stopped before destroy */
        db_dispatch(db, DB_EVENT_SHUTDOWN);
        /* Wait for STOPPED state... */
    }
    
    free(db->host);
    free(db->query_result);
    free(db);
}

/* Actions */
static int act_connect(void *ctx) {
    db_conn_t *db = ctx;
    db->socket = socket_connect(db->host, db->port);
    return (db->socket >= 0) ? 0 : -1;
}

static int act_disconnect(void *ctx) {
    db_conn_t *db = ctx;
    if (db->socket >= 0) {
        close(db->socket);
        db->socket = -1;
    }
    return 0;
}

static int act_cleanup(void *ctx) {
    db_conn_t *db = ctx;
    free(db->query_result);
    db->query_result = NULL;
    return 0;
}

/* Transition table */
static const transition_t db_transitions[DB_STATE_COUNT][DB_EVENT_COUNT] = {
/*                  INIT                       CONNECTED                QUERY                    QUERY_DONE             SHUTDOWN                   STOPPED                 DESTROY     ERROR                    */
[DB_STATE_CREATED]   = {{DB_STATE_CONNECTING, act_connect}, ILLEGAL,  ILLEGAL,                 ILLEGAL,               ILLEGAL,                   ILLEGAL,                ILLEGAL,    ILLEGAL                  },
[DB_STATE_CONNECTING]= {ILLEGAL, {DB_STATE_READY, NULL},              ILLEGAL,                 ILLEGAL,               {DB_STATE_STOPPING, act_disconnect}, ILLEGAL,     ILLEGAL,    {DB_STATE_ERROR, NULL}   },
[DB_STATE_READY]     = {ILLEGAL, ILLEGAL,                             {DB_STATE_BUSY, NULL},   ILLEGAL,               {DB_STATE_STOPPING, NULL}, ILLEGAL,                ILLEGAL,    {DB_STATE_ERROR, NULL}   },
[DB_STATE_BUSY]      = {ILLEGAL, ILLEGAL,                             ILLEGAL,                 {DB_STATE_READY, NULL},{DB_STATE_STOPPING, NULL}, ILLEGAL,                ILLEGAL,    {DB_STATE_ERROR, NULL}   },
[DB_STATE_STOPPING]  = {ILLEGAL, ILLEGAL,                             ILLEGAL,                 ILLEGAL,               ILLEGAL,                   {DB_STATE_STOPPED, act_disconnect}, ILLEGAL, ILLEGAL        },
[DB_STATE_STOPPED]   = {ILLEGAL, ILLEGAL,                             ILLEGAL,                 ILLEGAL,               ILLEGAL,                   ILLEGAL,   {DB_STATE_STOPPED, act_cleanup}, ILLEGAL        },
[DB_STATE_ERROR]     = {ILLEGAL, ILLEGAL,                             ILLEGAL,                 ILLEGAL,               {DB_STATE_STOPPING, NULL}, ILLEGAL,                ILLEGAL,    ILLEGAL                  },
};
```

---

## 2. Resource Ownership

```
ALLOCATION AND RELEASE TIED TO STATES
=====================================

Resource Ownership by State:

State            | Socket | Buffer | Timer | Query Result |
-----------------+--------+--------+-------+--------------+
CREATED          |   -    |   -    |   -   |      -       |
CONNECTING       |   ✓    |   -    |   ✓   |      -       |
READY            |   ✓    |   -    |   -   |      -       |
BUSY             |   ✓    |   ✓    |   ✓   |      -       |
STOPPING         |   ✓    |   -    |   -   |      -       |
STOPPED          |   -    |   -    |   -   |      -       |
ERROR            |   ?    |   -    |   -   |      -       |

Legend:
  ✓  = Resource is allocated and valid
  -  = Resource is NOT allocated
  ?  = Resource may or may not be valid (cleanup needed)

INVARIANT: In each state, we know EXACTLY which resources exist.
```

```
STATE-BASED RESOURCE MANAGEMENT
===============================

Entry Actions: Acquire resources when entering state
Exit Actions: Release resources when leaving state

+---------------+
|  READY state  |
+---------------+
      |
      | EVENT_QUERY
      |
      v
+-----+---------+--------+
| Entry Action:          |
|   buffer = alloc(...)  |
|   timer = start(...)   |
+------------------------+
      |
      | (in BUSY state)
      |
      v
+------------------------+
| Exit Action:           |
|   free(buffer)         |
|   timer_cancel(timer)  |
+------------------------+
      |
      | EVENT_QUERY_DONE
      v
+---------------+
|  READY state  |
+---------------+


Code Pattern:
  void on_enter_busy(context_t *ctx) {
      ctx->buffer = malloc(BUFFER_SIZE);
      ctx->timer = timer_start(QUERY_TIMEOUT);
  }
  
  void on_exit_busy(context_t *ctx) {
      free(ctx->buffer);
      ctx->buffer = NULL;
      timer_cancel(ctx->timer);
      ctx->timer = NULL;
  }
```

```
PREVENTING USE-AFTER-FREE VIA STATE
===================================

Common Bug: Use-after-free

  free(buffer);
  // ... later ...
  memcpy(buffer, data, len);  // CRASH!


FSM Prevention:

  1. Buffer only valid in BUSY state
  2. BUSY state requires buffer != NULL
  3. Leaving BUSY state frees buffer
  4. Cannot re-enter BUSY without new allocation

State transitions ENFORCE resource validity.


Example: Safe Buffer Access

  int handle_data(context_t *ctx, const void *data, size_t len) {
      if (ctx->state != STATE_BUSY) {
          /* Buffer doesn't exist in this state */
          return -EINVAL;
      }
      
      /* State guarantees buffer is valid */
      assert(ctx->buffer != NULL);
      memcpy(ctx->buffer, data, len);
      return 0;
  }


The state check replaces the NULL check:
  - NULL check: "Is this pointer valid?"
  - State check: "Is this OPERATION valid?"
  
State check is more powerful:
  - Covers multiple resources at once
  - Expresses INTENT, not just validity
  - Documents allowed operations
```

```c
/* Use-After-Free Prevention Example */

typedef struct {
    state_t state;
    
    /* Resource: only valid in certain states */
    char *buffer;       /* Valid in: BUSY */
    int socket;         /* Valid in: CONNECTED, BUSY */
    timer_t *timer;     /* Valid in: CONNECTING, BUSY */
} connection_t;

/* Safe access patterns */

int conn_send(connection_t *c, const void *data, size_t len) {
    /* State check replaces resource check */
    if (c->state != STATE_CONNECTED && c->state != STATE_BUSY) {
        return -EINVAL;  /* Can't send in this state */
    }
    
    /* State guarantees socket is valid */
    return send(c->socket, data, len, 0);
}

int conn_read_to_buffer(connection_t *c) {
    /* Only BUSY state has a buffer */
    if (c->state != STATE_BUSY) {
        return -EINVAL;
    }
    
    /* State guarantees both socket and buffer are valid */
    return recv(c->socket, c->buffer, BUFFER_SIZE, 0);
}

/* Entry/exit actions manage resources */
void enter_busy(connection_t *c) {
    assert(c->buffer == NULL);  /* Sanity check */
    c->buffer = malloc(BUFFER_SIZE);
    c->timer = timer_start(OPERATION_TIMEOUT);
}

void exit_busy(connection_t *c) {
    free(c->buffer);
    c->buffer = NULL;
    timer_cancel(c->timer);
    c->timer = NULL;
}
```

---

## 3. Lifetime FSM vs Protocol FSM

```
TWO KINDS OF FSMs
=================

LIFETIME FSM:
  - Models object existence
  - States: created, initialized, running, stopped, destroyed
  - Events: init, start, stop, destroy
  - Purpose: Resource management, safe cleanup

PROTOCOL FSM:
  - Models communication behavior
  - States: idle, handshaking, authenticated, transferring
  - Events: message_received, timeout, error
  - Purpose: Protocol correctness


They can be COMBINED or SEPARATE:

Option 1: Single Combined FSM
+---------------------------------------------------------------+
|  CREATED -> CONNECTING -> HANDSHAKING -> AUTHENTICATED -> ... |
|     ^                                                    |    |
|     |                    <- DESTROYING <- STOPPING <-----+    |
+---------------------------------------------------------------+
- Simpler: one state variable
- Harder: many states, complex transitions


Option 2: Two Separate FSMs
+---------------------------------------------------------------+
|  Lifetime FSM: CREATED -> RUNNING -> STOPPING -> DESTROYED    |
|                              |                                |
|                              v                                |
|  Protocol FSM: IDLE -> HANDSHAKING -> AUTHENTICATED -> ...    |
|                (only active when Lifetime FSM is RUNNING)     |
+---------------------------------------------------------------+
- Cleaner: separation of concerns
- More complex: need to coordinate FSMs
```

```
WHEN TO USE EACH
================

Use LIFETIME FSM when:
  - Managing resource allocation/deallocation
  - Ensuring proper cleanup order
  - Preventing use-after-free
  - Tracking initialization status

Use PROTOCOL FSM when:
  - Implementing communication protocol
  - Handling message sequences
  - Managing authentication flow
  - Tracking conversation state

Use BOTH (separate) when:
  - Object has complex lifecycle AND protocol
  - Lifecycle events (shutdown) can interrupt protocol
  - Want to reuse protocol FSM with different lifecycles


COMPOSITION PATTERN:
+---------------------------------------------------------------+
|  typedef struct {                                             |
|      lifecycle_fsm_t lifecycle;  /* Created/Running/Stopped */|
|      protocol_fsm_t protocol;    /* Idle/Auth/Transfer */    |
|                                                               |
|      /* Shared resources */                                   |
|      int socket;                                              |
|      buffer_t *buf;                                           |
|  } connection_t;                                              |
|                                                               |
|  /* Protocol FSM only active when lifecycle is RUNNING */     |
|  int conn_handle_event(connection_t *c, event_t e) {          |
|      if (is_lifecycle_event(e)) {                             |
|          return lifecycle_dispatch(&c->lifecycle, e);         |
|      }                                                        |
|                                                               |
|      if (c->lifecycle.state != LIFECYCLE_RUNNING) {           |
|          return -EINVAL;  /* Protocol events ignored */       |
|      }                                                        |
|                                                               |
|      return protocol_dispatch(&c->protocol, e);               |
|  }                                                            |
+---------------------------------------------------------------+
```

```
LIFECYCLE FSM TEMPLATE
======================

/* Standard lifecycle states */
typedef enum {
    LIFECYCLE_CREATED,
    LIFECYCLE_INITIALIZING,
    LIFECYCLE_RUNNING,
    LIFECYCLE_STOPPING,
    LIFECYCLE_STOPPED,
    LIFECYCLE_ERROR,
    LIFECYCLE_COUNT
} lifecycle_state_t;

/* Standard lifecycle events */
typedef enum {
    LIFECYCLE_INIT,
    LIFECYCLE_INIT_DONE,
    LIFECYCLE_STOP,
    LIFECYCLE_STOP_DONE,
    LIFECYCLE_ERROR_EVENT,
    LIFECYCLE_DESTROY,
    LIFECYCLE_EVENT_COUNT
} lifecycle_event_t;

/* Standard transitions */
static const transition_t lifecycle_table[LIFECYCLE_COUNT][LIFECYCLE_EVENT_COUNT] = {
/*                    INIT                            INIT_DONE                 STOP                          STOP_DONE                   ERROR                         DESTROY                    */
[LIFECYCLE_CREATED]     = {{LIFECYCLE_INITIALIZING, start_init}, ILLEGAL,       ILLEGAL,                      ILLEGAL,                    {LIFECYCLE_ERROR, NULL},      ILLEGAL                    },
[LIFECYCLE_INITIALIZING]= {ILLEGAL,                              {LIFECYCLE_RUNNING, NULL}, {LIFECYCLE_STOPPING, abort_init}, ILLEGAL,      {LIFECYCLE_ERROR, NULL},      ILLEGAL                    },
[LIFECYCLE_RUNNING]     = {ILLEGAL,                              ILLEGAL,       {LIFECYCLE_STOPPING, start_stop}, ILLEGAL,                 {LIFECYCLE_ERROR, NULL},      ILLEGAL                    },
[LIFECYCLE_STOPPING]    = {ILLEGAL,                              ILLEGAL,       ILLEGAL,                      {LIFECYCLE_STOPPED, NULL},  {LIFECYCLE_ERROR, NULL},      ILLEGAL                    },
[LIFECYCLE_STOPPED]     = {ILLEGAL,                              ILLEGAL,       ILLEGAL,                      ILLEGAL,                    ILLEGAL,                      {LIFECYCLE_STOPPED, cleanup}},
[LIFECYCLE_ERROR]       = {ILLEGAL,                              ILLEGAL,       {LIFECYCLE_STOPPING, start_stop}, ILLEGAL,                 ILLEGAL,                      ILLEGAL                    },
};

/* Reusable for any object that needs lifecycle management */
```

---

## Summary: Object Lifetime FSMs

```
+----------------------------------------------------------+
|                 LIFECYCLE FSM BENEFITS                    |
+----------------------------------------------------------+
|                                                          |
|  RESOURCE SAFETY:                                        |
|    - Each state defines which resources exist            |
|    - Entry actions acquire, exit actions release         |
|    - State check replaces NULL check                     |
|                                                          |
|  USE-AFTER-FREE PREVENTION:                              |
|    - Resources only accessible in valid states           |
|    - State machine enforces access rules                 |
|    - Invalid access = illegal transition                 |
|                                                          |
|  PROPER CLEANUP:                                         |
|    - Shutdown is a STATE, not an afterthought            |
|    - Cleanup order enforced by transitions               |
|    - Cannot destroy before stopped                       |
|                                                          |
|  SEPARATION FROM PROTOCOL:                               |
|    - Lifecycle: "Does object exist?"                     |
|    - Protocol: "What is object doing?"                   |
|    - Can be combined or kept separate                    |
|                                                          |
+----------------------------------------------------------+
```

---

**中文解释（Chinese Explanation）**

**生命周期 FSM**

每个非平凡对象都经历生命周期阶段：
- **CREATED**：内存已分配，初始值设置，无外部资源，未准备好使用
- **INITIALIZING**：获取外部资源（打开文件/socket、分配缓冲区），可能失败
- **RUNNING**：完全可操作，可以处理请求
- **STOPPING**：收到关闭信号，完成待处理工作，拒绝新请求
- **STOPPED**：无待处理工作，准备清理，资源仍持有
- **DESTROYED**：所有资源释放，内存释放，对象不再可用

**资源所有权**

分配和释放与状态绑定。每个状态下，我们**精确知道**哪些资源存在。

- **入口动作**：进入状态时获取资源
- **出口动作**：离开状态时释放资源

**通过状态防止 use-after-free**

状态检查替代资源检查：
- NULL 检查："这个指针有效吗？"
- 状态检查："这个**操作**有效吗？"

状态检查更强大：一次覆盖多个资源、表达意图而非仅有效性、记录允许的操作。

**生命周期 FSM vs 协议 FSM**

- **生命周期 FSM**：建模对象存在（created, initialized, running, stopped, destroyed）
- **协议 FSM**：建模通信行为（idle, handshaking, authenticated, transferring）

可以合并为一个 FSM（更简单但状态多）或分离为两个 FSM（更清晰但需要协调）。

组合模式：对象包含两个 FSM，协议 FSM 只在生命周期 FSM 处于 RUNNING 状态时活跃。生命周期事件可以中断协议。
