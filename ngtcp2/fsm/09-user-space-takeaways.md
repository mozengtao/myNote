# Section 9: User-Space Takeaways

## 9.1 How to Design FSMs for Async Systems

### Principle 1: Separate State from I/O

```
┌─────────────────────────────────────────────────────────────────┐
│                    SEPARATION OF CONCERNS                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────────┐      ┌─────────────────────────────┐   │
│  │   I/O Layer         │      │   State Machine Layer        │   │
│  │                     │      │                              │   │
│  │  • Socket read      │      │  • State transitions         │   │
│  │  • Socket write     │      │  • Timer logic               │   │
│  │  • Timer scheduling │      │  • Protocol logic            │   │
│  │                     │      │  • Error handling            │   │
│  │  (Application)      │      │  (Library)                   │   │
│  └──────────┬──────────┘      └──────────────┬───────────────┘   │
│             │                                │                   │
│             │    Events (data, time)         │                   │
│             └───────────────────────────────▶│                   │
│             │                                │                   │
│             │    Actions (send, callback)    │                   │
│             │◀───────────────────────────────┘                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Principle 2: Timestamp Injection

```c
// BAD: Internal time calls
void process_event(struct conn *conn) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);  // Hidden dependency
  // ...
}

// GOOD: Injected timestamps (ngtcp2 pattern)
void process_event(struct conn *conn, uint64_t ts) {
  // All time decisions use passed timestamp
  if (ts >= conn->expiry) {
    // Handle expiry
  }
}

// Benefits:
// - Testable (control time in tests)
// - Consistent (all logic uses same time)
// - Reproducible (deterministic behavior)
```

### Principle 3: Timer Aggregation

```c
// Instead of multiple independent timers:
// timer_for_loss, timer_for_idle, timer_for_ack, timer_for_keep_alive...

// Use single aggregated expiry (ngtcp2 pattern):
uint64_t get_expiry(struct conn *conn) {
  uint64_t expiry = UINT64_MAX;
  
  expiry = min(expiry, conn->loss_timer);
  expiry = min(expiry, conn->idle_timer);
  expiry = min(expiry, conn->ack_timer);
  expiry = min(expiry, conn->keep_alive_timer);
  // ... add all timer sources
  
  return expiry;
}

// Application schedules single timer for earliest expiry
// On expiry, call handle_expiry() to process all expired timers
```

### Principle 4: Non-Blocking Returns

```c
// BAD: Blocking call
ssize_t send_data(struct conn *conn, void *data, size_t len) {
  while (conn->state != STATE_READY) {
    usleep(1000);  // Blocking!
  }
  return do_send(conn, data, len);
}

// GOOD: Immediate return with status (ngtcp2 pattern)
ssize_t send_data(struct conn *conn, void *data, size_t len, uint64_t ts) {
  if (conn->state != STATE_READY) {
    return ERR_WOULD_BLOCK;  // Caller handles
  }
  return do_send(conn, data, len);
}
```

---

## 9.2 How to Evolve FSMs Safely

### Strategy 1: Flag-Based Extension

```c
// Adding new behavior without changing existing states

// Step 1: Add flag
#define CONN_FLAG_NEW_FEATURE 0x100u

// Step 2: Guard new behavior with flag
if (conn->flags & CONN_FLAG_NEW_FEATURE) {
  handle_new_feature(conn);
}

// Step 3: Existing code continues to work
// Old behavior: Flag not set, new code path not taken
// New behavior: Set flag to enable feature
```

### Strategy 2: Layered Sub-FSMs

```c
// Adding new functionality as independent sub-FSM

// Step 1: Define sub-FSM structure
struct new_subsystem {
  enum { NEW_STATE_A, NEW_STATE_B, NEW_STATE_C } state;
  uint64_t timer;
  // Sub-FSM specific data
};

// Step 2: Add to connection (nullable)
struct conn {
  // ... existing fields
  struct new_subsystem *new_sub;  // NULL if not active
};

// Step 3: Integrate at appropriate points
void process_event(struct conn *conn, uint64_t ts) {
  // Existing logic...
  
  // New subsystem processing (if active)
  if (conn->new_sub) {
    process_new_subsystem(conn->new_sub, ts);
  }
}
```

### Strategy 3: Version-Safe Callbacks

```c
// Allow callback structure evolution

struct callbacks_v1 {
  void (*on_connect)(struct conn *);
  void (*on_data)(struct conn *, void *, size_t);
};

struct callbacks_v2 {
  void (*on_connect)(struct conn *);
  void (*on_data)(struct conn *, void *, size_t);
  void (*on_new_event)(struct conn *);  // Added in v2
};

// Create connection with version
struct conn *conn_new(int cb_version, void *callbacks) {
  if (cb_version == 1) {
    // Handle v1 callbacks
  } else if (cb_version == 2) {
    // Handle v2 callbacks, including new event
  }
}
```

### Strategy 4: Guard Condition Strengthening

```c
// Safely add preconditions to transitions

// Before: Simple guard
if (conn->state == STATE_READY) {
  do_action(conn);
}

// After: Strengthened guard (backward compatible)
if (conn->state == STATE_READY && 
    conn_is_validated(conn) &&      // New condition
    !(conn->flags & FLAG_PAUSED)) { // New condition
  do_action(conn);
}

// Existing code that satisfies new conditions continues to work
// New conditions prevent edge cases
```

---

## 9.3 How to Avoid State Explosion

### Technique 1: Hierarchical State Organization

```
┌─────────────────────────────────────────────────────────────────┐
│              HIERARCHICAL STATE ORGANIZATION                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  WRONG: Flat state space                                         │
│  ────────────────────────                                        │
│  States = {A1, A2, A3, B1, B2, B3, C1, C2, C3}                  │
│  = 9 states (3 × 3 combinations)                                 │
│                                                                  │
│  As features add: 3 × 3 × 3 × 3 × ... = exponential!            │
│                                                                  │
│  RIGHT: Hierarchical organization                                │
│  ────────────────────────────────                                │
│  Layer 1: Major Phase    = {A, B, C}           (3 states)       │
│  Layer 2: Sub-state      = {1, 2, 3}           (3 values)       │
│  Total: 3 + 3 = 6 values to manage, same expressiveness         │
│                                                                  │
│  struct state {                                                  │
│    enum { A, B, C } phase;                                       │
│    enum { S1, S2, S3 } sub_state;                                │
│  };                                                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Technique 2: Orthogonal Concerns as Flags

```c
// Instead of STATE_A_ENCRYPTED, STATE_A_UNENCRYPTED, 
// STATE_B_ENCRYPTED, STATE_B_UNENCRYPTED, ...

// Use orthogonal dimensions:
struct conn {
  enum { STATE_A, STATE_B, STATE_C } state;  // Primary FSM
  
  // Orthogonal concerns as flags
  unsigned encrypted : 1;
  unsigned authenticated : 1;
  unsigned key_updated : 1;
  // ...
};

// Decision logic combines state + flags:
if (conn->state == STATE_A && conn->encrypted) {
  // Handle encrypted STATE_A
}
```

### Technique 3: Sub-FSM Containment

```c
// Complex subsystems get their own FSM, not combined with main

// WRONG: Combine everything
enum state {
  IDLE,
  CONNECTING,
  CONNECTING_PATH_VALIDATING,  // Explosion starts here
  CONNECTING_PATH_VALIDATED,
  CONNECTED,
  CONNECTED_PATH_VALIDATING,
  CONNECTED_PATH_VALIDATED,
  // ... more combinations
};

// RIGHT: Separate FSMs (ngtcp2 pattern)
struct conn {
  enum { IDLE, CONNECTING, CONNECTED } state;  // Main FSM
  struct path_validation *pv;  // Sub-FSM (nullable)
  struct key_update *ku;       // Sub-FSM (nullable)
};

// Sub-FSMs have their own lifecycle, independent of main
```

### Technique 4: Event-Based vs State-Based Guards

```c
// Sometimes the guard is on the event, not the state

// State-based (can lead to explosion):
switch (state) {
case STATE_A_CAN_RECEIVE:
  process_data(data);
  break;
case STATE_A_CANNOT_RECEIVE:
  // Separate state just for this condition
  break;
}

// Event-based (avoids explosion):
switch (state) {
case STATE_A:
  if (can_receive(conn)) {  // Guard on event
    process_data(data);
  } else {
    queue_data(data);  // Handle gracefully
  }
  break;
}
```

---

## 9.4 Summary: FSM Design Checklist

```
┌─────────────────────────────────────────────────────────────────┐
│                    FSM DESIGN CHECKLIST                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  □ Async-Friendly Design                                         │
│    □ No blocking calls in FSM logic                              │
│    □ Timestamp injection for all time-dependent logic            │
│    □ Single aggregated timer expiry                              │
│    □ Clear separation of I/O and state logic                     │
│                                                                  │
│  □ State Organization                                            │
│    □ Primary states as enum (mutually exclusive)                 │
│    □ Orthogonal concerns as flags                                │
│    □ Complex features as sub-FSMs                                │
│    □ Hierarchical, not flat                                      │
│                                                                  │
│  □ Transition Safety                                             │
│    □ Guard conditions before transitions                         │
│    □ Entry/exit actions clearly defined                          │
│    □ No partial state updates                                    │
│    □ Terminal states are sticky                                  │
│                                                                  │
│  □ Evolution Planning                                            │
│    □ Flag space for future features                              │
│    □ Callback versioning                                         │
│    □ Sub-FSM extension points                                    │
│    □ Guard conditions can be strengthened                        │
│                                                                  │
│  □ Error Handling                                                │
│    □ Clear error categories (fatal vs recoverable)               │
│    □ Rich error information                                      │
│    □ Explicit terminal states (CLOSING, DRAINING)                │
│    □ Cleanup path well-defined                                   │
│                                                                  │
│  □ Testability                                                   │
│    □ State is inspectable                                        │
│    □ Time is injectable                                          │
│    □ No hidden global state                                      │
│    □ Callbacks for observation                                   │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 9.5 Anti-Patterns to Avoid

### Anti-Pattern 1: Hidden Time Dependencies

```c
// BAD
void check_timeout(struct conn *conn) {
  time_t now = time(NULL);  // Hidden dependency
  if (now - conn->last_activity > TIMEOUT) {
    close_conn(conn);
  }
}

// GOOD
void check_timeout(struct conn *conn, uint64_t now) {
  if (now - conn->last_activity > TIMEOUT) {
    close_conn(conn);
  }
}
```

### Anti-Pattern 2: State Polling

```c
// BAD
while (conn->state != STATE_READY) {
  poll_for_events(conn);
  usleep(1000);
}

// GOOD
// Event loop waits for events or timer
wait_for_event_or_timeout(fds, get_timeout(conn));
handle_events(conn);
```

### Anti-Pattern 3: Implicit State Transitions

```c
// BAD
void send_data(struct conn *conn, void *data) {
  // Hidden transition inside send
  if (check_some_condition(conn)) {
    conn->state = STATE_X;  // Surprise!
  }
  do_send(conn, data);
}

// GOOD
int send_data(struct conn *conn, void *data) {
  if (conn->state != STATE_READY) {
    return ERR_INVALID_STATE;  // Clear error
  }
  return do_send(conn, data);
}
// Transitions happen at clear points, visible to caller
```

### Anti-Pattern 4: State + Action Coupling

```c
// BAD
void transition_to_connected(struct conn *conn) {
  conn->state = STATE_CONNECTED;
  send_connected_notification();  // I/O in transition
  start_timer();                  // I/O in transition
}

// GOOD
void transition_to_connected(struct conn *conn) {
  conn->state = STATE_CONNECTED;
  // Return to caller; caller handles I/O
}

// In caller:
transition_to_connected(conn);
send_connected_notification();  // I/O after transition
// Timer handled by application event loop
```

---

## 中文解释 (Chinese Explanation)

### 异步系统 FSM 设计原则

1. **分离状态与 I/O**: FSM 库只处理状态逻辑，应用程序处理 I/O
2. **时间戳注入**: 所有时间相关逻辑使用传入的时间戳
3. **定时器聚合**: 单一聚合到期时间，应用程序调度单个定时器
4. **非阻塞返回**: 所有 API 立即返回，返回状态码

### 安全演进 FSM

1. **基于标志的扩展**: 添加新标志启用新功能，不改变现有状态
2. **分层子 FSM**: 新功能作为独立子 FSM 添加
3. **版本安全回调**: 回调结构支持版本演进
4. **守卫条件强化**: 安全添加新的前置条件

### 避免状态爆炸

1. **层次化状态组织**: 使用多层次而非扁平状态空间
2. **正交关注点作为标志**: 独立维度用标志表示
3. **子 FSM 封装**: 复杂子系统有自己的 FSM
4. **基于事件的守卫**: 有时守卫在事件上而非状态上

### 反模式

- 隐藏时间依赖
- 状态轮询
- 隐式状态转换
- 状态与动作耦合
