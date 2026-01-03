# Section 9: User-Space Design Lessons

## 9.1 When Implicit FSMs Are Acceptable

### Good Use Cases for Implicit FSMs

```
+------------------------------------------------------------------+
|              WHEN TO USE IMPLICIT FSMs                            |
+------------------------------------------------------------------+
|                                                                   |
|  ✓ PASSIVE ANALYSIS / MONITORING                                  |
|    - Network traffic analysis                                     |
|    - Log processing                                               |
|    - Protocol debugging                                           |
|    - Traffic recording                                            |
|                                                                   |
|  ✓ HANDLING INCOMPLETE DATA                                       |
|    - Capture starts mid-session                                   |
|    - Packet loss expected                                         |
|    - Partial protocol traces                                      |
|                                                                   |
|  ✓ MULTIPLE ORTHOGONAL CONCERNS                                   |
|    - Independent state dimensions                                 |
|    - Feature flags                                                |
|    - Capability tracking                                          |
|                                                                   |
|  ✓ MAXIMUM INFORMATION EXTRACTION                                 |
|    - Show what you can                                            |
|    - Graceful degradation                                         |
|    - Best-effort parsing                                          |
|                                                                   |
+------------------------------------------------------------------+
```

### Design Checklist for Implicit FSMs

```c
// Example: Implicit FSM for protocol analyzer

typedef struct {
    // Use flags for observed events
    uint32_t observed;
    #define OBS_HANDSHAKE_START  (1 << 0)
    #define OBS_HANDSHAKE_DONE   (1 << 1)
    #define OBS_DATA_SENT        (1 << 2)
    #define OBS_ERROR_SEEN       (1 << 3)
    
    // Use flags for derived capabilities
    uint32_t can_do;
    #define CAN_DECODE_HEADER    (1 << 0)
    #define CAN_DECODE_BODY      (1 << 1)
    #define CAN_CORRELATE        (1 << 2)
    
    // Per-direction state
    struct flow_info flows[2];
    
} analyzer_state_t;

// Derive "state" from flags
static const char* get_phase_name(analyzer_state_t *s) {
    if (s->observed & OBS_ERROR_SEEN) return "Error";
    if (s->observed & OBS_DATA_SENT) return "Data Transfer";
    if (s->observed & OBS_HANDSHAKE_DONE) return "Established";
    if (s->observed & OBS_HANDSHAKE_START) return "Handshaking";
    return "Unknown";
}
```

---

## 9.2 When Explicit FSMs Are Necessary

### Good Use Cases for Explicit FSMs

```
+------------------------------------------------------------------+
|              WHEN TO USE EXPLICIT FSMs                            |
+------------------------------------------------------------------+
|                                                                   |
|  ✓ PROTOCOL IMPLEMENTATION                                        |
|    - Must enforce state machine                                   |
|    - Reject invalid sequences                                     |
|    - Security-critical code                                       |
|                                                                   |
|  ✓ SIMPLE, LINEAR PROTOCOLS                                       |
|    - Clear state progression                                      |
|    - Few states (< 10)                                            |
|    - No optional features                                         |
|                                                                   |
|  ✓ COMPLIANCE TESTING                                             |
|    - Verify correct protocol implementation                       |
|    - Detect state violations                                      |
|    - RFC compliance checking                                      |
|                                                                   |
|  ✓ DOCUMENTATION VALUE                                            |
|    - Clear specification                                          |
|    - Self-documenting code                                        |
|    - Easy to verify against spec                                  |
|                                                                   |
+------------------------------------------------------------------+
```

### Design Pattern for Explicit FSM

```c
// Example: Explicit FSM for protocol server

typedef enum {
    STATE_IDLE,
    STATE_GREETING,
    STATE_AUTHENTICATING,
    STATE_READY,
    STATE_PROCESSING,
    STATE_CLOSING,
    STATE_ERROR
} server_state_t;

typedef enum {
    EVENT_CONNECT,
    EVENT_AUTH_REQUEST,
    EVENT_AUTH_SUCCESS,
    EVENT_AUTH_FAIL,
    EVENT_COMMAND,
    EVENT_COMMAND_DONE,
    EVENT_DISCONNECT,
    EVENT_TIMEOUT,
    EVENT_COUNT
} server_event_t;

// Transition table
typedef struct {
    server_state_t next_state;
    void (*action)(server_ctx_t *ctx);
} transition_t;

static const transition_t fsm_table[STATE_ERROR][EVENT_COUNT] = {
    // STATE_IDLE
    [STATE_IDLE] = {
        [EVENT_CONNECT] = {STATE_GREETING, send_greeting},
        // All other events invalid in this state
    },
    // STATE_GREETING
    [STATE_GREETING] = {
        [EVENT_AUTH_REQUEST] = {STATE_AUTHENTICATING, start_auth},
        [EVENT_TIMEOUT] = {STATE_CLOSING, send_timeout},
    },
    // ... more states
};

void fsm_handle(server_ctx_t *ctx, server_event_t event) {
    transition_t *t = &fsm_table[ctx->state][event];
    
    if (t->next_state == 0 && t->action == NULL) {
        // Invalid transition
        handle_protocol_error(ctx);
        return;
    }
    
    ctx->state = t->next_state;
    if (t->action) t->action(ctx);
}
```

---

## 9.3 How to Evolve FSMs Safely as Protocols Grow

### Strategy 1: Version-Based Extension

```c
// Support multiple protocol versions with FSM evolution

typedef struct {
    uint8_t version;
    
    union {
        struct v1_state {
            enum v1_state_t state;
        } v1;
        
        struct v2_state {
            enum v2_state_t state;
            uint32_t extension_flags;  // New in v2
        } v2;
    };
} protocol_state_t;

void handle_packet(protocol_state_t *s, packet_t *pkt) {
    switch (s->version) {
        case 1:
            handle_v1(s, pkt);
            break;
        case 2:
            handle_v2(s, pkt);
            break;
    }
}
```

### Strategy 2: Capability-Based Extension

```c
// Add capabilities without breaking existing states

typedef struct {
    // Core state (never changes)
    enum core_state_t core_state;
    
    // Extension flags (added over time)
    uint64_t capabilities;
    #define CAP_BASIC         (1ULL << 0)   // Original
    #define CAP_COMPRESSION   (1ULL << 1)   // Added v1.1
    #define CAP_ENCRYPTION    (1ULL << 2)   // Added v1.2
    #define CAP_MULTIPLEXING  (1ULL << 3)   // Added v2.0
    
    // Optional sub-states (added as needed)
    void *compression_state;  // NULL if not enabled
    void *encryption_state;   // NULL if not enabled
    void *mux_state;          // NULL if not enabled
    
} extensible_state_t;

void enable_feature(extensible_state_t *s, uint64_t cap) {
    s->capabilities |= cap;
    
    // Allocate sub-state if needed
    switch (cap) {
        case CAP_COMPRESSION:
            s->compression_state = alloc_compression_state();
            break;
        // ...
    }
}
```

### Strategy 3: State Machine Composition

```c
// Compose multiple simple FSMs rather than one complex one

typedef struct {
    // Connection-level FSM
    struct {
        enum conn_state_t state;
    } connection;
    
    // Authentication FSM (orthogonal)
    struct {
        enum auth_state_t state;
        int attempts;
    } auth;
    
    // Per-stream FSM (multiple instances)
    struct {
        uint32_t stream_id;
        enum stream_state_t state;
    } *streams;
    int num_streams;
    
} composed_state_t;

// Each FSM evolved independently
void handle_conn_event(composed_state_t *s, event_t *e);
void handle_auth_event(composed_state_t *s, event_t *e);
void handle_stream_event(composed_state_t *s, uint32_t stream, event_t *e);
```

---

## 9.4 Reusable Principles Summary

```
+------------------------------------------------------------------+
|              PROTOCOL FSM DESIGN PRINCIPLES                       |
+------------------------------------------------------------------+

1. CHOOSE THE RIGHT ABSTRACTION
   ============================
   - Analysis tool? -> Implicit FSM with flags
   - Protocol server? -> Explicit FSM with transitions
   - Hybrid? -> Composed FSMs

2. DESIGN FOR PARTIAL INFORMATION
   ===============================
   - What if capture starts mid-session?
   - What if packets are lost?
   - What if malformed data received?
   
   SOLUTION: Track "what we know" not "what state we're in"

3. SEPARATE CONCERNS
   =================
   - Connection state vs Stream state
   - Authentication vs Data transfer
   - Encryption vs Application logic
   
   SOLUTION: Multiple orthogonal state machines

4. PLAN FOR EVOLUTION
   ==================
   - Reserve flag bits for future use
   - Use capability negotiation
   - Version your state structures
   
   SOLUTION: Extensible state representation

5. PRIORITIZE ROBUSTNESS
   =====================
   - Never crash on bad input
   - Always show something useful
   - Flag errors, don't reject
   
   SOLUTION: Defensive programming, expert info

6. OPTIMIZE HOT PATHS
   ==================
   - Cache frequently accessed state
   - Use O(1) lookups
   - Minimize allocations
   
   SOLUTION: Hash tables, memory pools, state caching

+------------------------------------------------------------------+
```

### Decision Flowchart

```
                    Start
                      │
                      v
            ┌─────────────────┐
            │ Are you         │
            │ implementing    │──YES──> Use Explicit FSM
            │ the protocol?   │         with strict transitions
            └────────┬────────┘
                     │ NO
                     v
            ┌─────────────────┐
            │ Is state linear │
            │ and simple      │──YES──> Use Simple Explicit FSM
            │ (< 5 states)?   │         (enum + switch)
            └────────┬────────┘
                     │ NO
                     v
            ┌─────────────────┐
            │ Multiple        │
            │ orthogonal      │──YES──> Use Flag-Based Implicit FSM
            │ concerns?       │         with composed states
            └────────┬────────┘
                     │ NO
                     v
            ┌─────────────────┐
            │ Must handle     │
            │ partial/damaged │──YES──> Use Partial Decoding FSM
            │ data?           │         with capability flags
            └────────┬────────┘
                     │ NO
                     v
              Use Table-Driven
              Explicit FSM
```

---

## 中文解释

### 用户空间设计经验

**1. 何时使用隐式FSM**
- 被动分析/监控场景
- 处理不完整数据
- 多个正交关注点
- 最大化信息提取

**2. 何时使用显式FSM**
- 协议实现（必须执行状态机）
- 简单线性协议
- 合规性测试
- 需要文档价值

**3. 如何安全演进FSM**
- 策略1：基于版本的扩展
- 策略2：基于能力的扩展
- 策略3：状态机组合

**4. 可复用原则**
- 选择正确的抽象级别
- 为部分信息设计
- 分离关注点
- 规划演进
- 优先考虑健壮性
- 优化热路径

**5. 决策流程**
- 实现协议？→ 显式FSM
- 状态简单？→ 简单显式FSM
- 多个正交关注？→ 标志位隐式FSM
- 处理损坏数据？→ 部分解码FSM
- 否则 → 表驱动显式FSM
