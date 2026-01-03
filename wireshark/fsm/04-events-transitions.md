# Section 4: Events and Transitions in Wireshark FSMs

## 4.1 What Constitutes an Event

```
+------------------------------------------------------------------+
|                    EVENT SOURCES IN WIRESHARK                     |
+------------------------------------------------------------------+
|                                                                   |
|  PACKET ARRIVAL                                                   |
|  ==============                                                   |
|  - New packet received                                            |
|  - Packet belongs to tracked conversation                         |
|  - dissect_xxx() function called                                  |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  FRAME/MESSAGE TYPE                                               |
|  =================                                                |
|  - TCP flags: SYN, ACK, FIN, RST                                  |
|  - TLS record type: Handshake, Application, Alert                 |
|  - HTTP/2 frame type: HEADERS, DATA, SETTINGS, RST_STREAM         |
|  - SMB2 command: CREATE, READ, WRITE, CLOSE                       |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  FIELD VALUES                                                     |
|  ============                                                     |
|  - TLS handshake type: ClientHello, ServerHello, Finished         |
|  - HTTP/2 stream ID                                               |
|  - Sequence numbers                                               |
|  - Status/error codes                                             |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  DERIVED CONDITIONS                                               |
|  ==================                                               |
|  - Sequence gap detected                                          |
|  - Retransmission identified                                      |
|  - All fragments received                                         |
|  - Decryption keys available                                      |
|                                                                   |
+------------------------------------------------------------------+
```

### Event Processing Flow

```
                          +------------------+
                          |  Incoming Packet |
                          +--------+---------+
                                   |
                                   v
                          +------------------+
                          | Find/Create      |
                          | Conversation     |
                          +--------+---------+
                                   |
                                   v
                          +------------------+
                          | Get Protocol     |
                          | State            |
                          +--------+---------+
                                   |
                                   v
                          +------------------+
                          | Parse Packet     |
                          | (Extract Event)  |
                          +--------+---------+
                                   |
                                   v
                          +------------------+
                          | Process Event    |
                          | (State Transition)|
                          +--------+---------+
                                   |
                                   v
                          +------------------+
                          | Update State     |
                          | Store Results    |
                          +------------------+
```

---

## 4.2 How State Transitions Are Triggered

### Example: TWAMP Explicit FSM Transitions

```c
// From packet-twamp.c
static void
dissect_twamp_control(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    twamp_control_transaction_t *ct;
    
    // Get or create state
    ct = get_twamp_control_transaction(pinfo);
    
    // Determine event and transition based on current state
    switch (ct->last_state) {
        case CONTROL_STATE_UNKNOWN:
            // First packet - must be greeting
            ct->last_state = CONTROL_STATE_GREETING;
            break;
            
        case CONTROL_STATE_GREETING:
            // After greeting comes setup response
            ct->last_state = CONTROL_STATE_SETUP_RESPONSE;
            break;
            
        case CONTROL_STATE_SETUP_RESPONSE:
            ct->last_state = CONTROL_STATE_SERVER_START;
            break;
            
        case CONTROL_STATE_SERVER_START:
            // Command determines next state
            command = tvb_get_uint8(tvb, offset);
            if (command == 5) {
                ct->last_state = CONTROL_STATE_REQUEST_TW_SESSION;
            } else if (command == 2) {
                ct->last_state = CONTROL_STATE_START_SESSIONS;
            }
            break;
        // ... more states
    }
}
```

### Example: TCP Implicit Transitions via Flags

```c
// From packet-tcp.c - Transitions happen through flag checking
static void
tcp_analyze_sequence(struct tcp_analysis *tcpd, packet_info *pinfo, 
                     uint16_t flags, uint32_t seq, uint32_t ack)
{
    // Transition based on flags (implicit FSM)
    if (flags & TH_SYN) {
        if (flags & TH_ACK) {
            // SYN-ACK: transition from SYN_SENT to SYN_RECEIVED
            tcpd->conversation_completeness |= TCP_COMPLETENESS_SYNACK;
        } else {
            // SYN: transition from CLOSED to SYN_SENT
            tcpd->conversation_completeness |= TCP_COMPLETENESS_SYNSENT;
        }
    }
    
    if ((flags & TH_ACK) && 
        (tcpd->conversation_completeness & TCP_COMPLETENESS_SYNACK)) {
        // ACK after SYN-ACK: transition to ESTABLISHED
        tcpd->conversation_completeness |= TCP_COMPLETENESS_ACK;
    }
    
    if (flags & TH_FIN) {
        tcpd->conversation_completeness |= TCP_COMPLETENESS_FIN;
    }
    
    if (flags & TH_RST) {
        tcpd->conversation_completeness |= TCP_COMPLETENESS_RST;
    }
}
```

---

## 4.3 Implicit FSMs in Decoding Logic

### Pattern: State Encoded in Conditionals

```c
// TLS implicit state machine in dissection
static int
dissect_ssl3_record(tvbuff_t *tvb, packet_info *pinfo,
                    proto_tree *tree, SslDecryptSession *ssl)
{
    int content_type = tvb_get_uint8(tvb, offset);
    
    switch (content_type) {
        case SSL_ID_HANDSHAKE:
            // Parse handshake, update ssl->state based on handshake_type
            dissect_ssl3_handshake(tvb, pinfo, tree, ssl);
            break;
            
        case SSL_ID_CHG_CIPHER_SPEC:
            // State transition: activate new cipher
            if (ssl->state & SSL_HAVE_SESSION_KEY) {
                ssl_change_cipher(ssl, server);
            }
            break;
            
        case SSL_ID_APP_DATA:
            // Only valid if we're past handshake
            if (ssl->state & SSL_HAVE_SESSION_KEY) {
                decrypt_application_data(tvb, ssl);
            }
            break;
            
        case SSL_ID_ALERT:
            // May indicate session termination
            dissect_ssl3_alert(tvb, pinfo, tree, ssl);
            break;
    }
}
```

### Pattern: Guard Conditions as State Checks

```c
// HTTP/2 frame processing
static int
dissect_http2_frame(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    http2_session_t *h2session = get_http2_session(pinfo);
    http2_stream_info_t *stream_info;
    
    uint8_t type = tvb_get_uint8(tvb, offset);
    uint32_t stream_id = tvb_get_ntohl(tvb, offset + 5) & 0x7FFFFFFF;
    
    // Get stream state
    stream_info = get_stream_info(pinfo, h2session, stream_id);
    
    switch (type) {
        case HTTP2_HEADERS:
            // Only valid on idle or reserved streams
            if (stream_info->state == STREAM_STATE_IDLE) {
                stream_info->state = STREAM_STATE_OPEN;
            }
            break;
            
        case HTTP2_DATA:
            // Only valid on open or half-closed streams
            if (stream_info->state != STREAM_STATE_OPEN &&
                stream_info->state != STREAM_STATE_HALF_CLOSED_LOCAL) {
                // Protocol error
                expert_add_info(pinfo, item, &ei_http2_stream_error);
            }
            break;
            
        case HTTP2_RST_STREAM:
            // Transition to closed
            stream_info->state = STREAM_STATE_CLOSED;
            break;
    }
}
```

---

## 4.4 Trade-offs: Implicit vs Explicit FSMs

```
+------------------------------------------------------------------+
|                    EXPLICIT FSM                                   |
+------------------------------------------------------------------+
| ADVANTAGES:                        | DISADVANTAGES:               |
| - Clear state diagram              | - Rigid structure            |
| - Easy to verify correctness       | - Hard to handle edge cases  |
| - Self-documenting                 | - Complex for multi-flow     |
| - Protocol violations obvious      | - State explosion possible   |
+------------------------------------------------------------------+
| EXAMPLE: TWAMP, simple request-response protocols                 |
+------------------------------------------------------------------+

+------------------------------------------------------------------+
|                    IMPLICIT FSM                                   |
+------------------------------------------------------------------+
| ADVANTAGES:                        | DISADVANTAGES:               |
| - Flexible                         | - Hard to understand         |
| - Handles partial captures         | - Difficult to verify        |
| - Graceful degradation             | - State scattered in code    |
| - Handles protocol variations      | - May miss violations        |
+------------------------------------------------------------------+
| EXAMPLE: TCP, TLS, HTTP/2, most Wireshark dissectors              |
+------------------------------------------------------------------+
```

### When to Use Each Approach

```
USE EXPLICIT FSM:                    USE IMPLICIT FSM:
================                     ================
- Simple protocols                   - Complex protocols
- Linear state progression           - Multiple concurrent states
- Need strict validation             - Partial capture handling
- Protocol compliance testing        - Real-world traffic analysis
- Few states (< 10)                  - Many states or state combinations
```

---

## State Transition Summary

```
+------------------------------------------------------------------+
|              STATE TRANSITION PATTERNS IN WIRESHARK               |
+------------------------------------------------------------------+
|                                                                   |
|  1. SWITCH-BASED EXPLICIT FSM                                     |
|  ============================                                     |
|  switch(current_state) {                                          |
|      case STATE_A:                                                |
|          if (event == X) new_state = STATE_B;                     |
|          break;                                                   |
|  }                                                                |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  2. FLAG-BASED IMPLICIT FSM                                       |
|  ==========================                                       |
|  if (event_X_occurred) flags |= FLAG_X;                           |
|  if ((flags & (FLAG_X | FLAG_Y)) == (FLAG_X | FLAG_Y)) {          |
|      // Combined state reached                                    |
|  }                                                                |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  3. GUARD-BASED IMPLICIT FSM                                      |
|  ===========================                                      |
|  if (can_decrypt() && is_application_data()) {                    |
|      decrypt_and_dissect();                                       |
|  }                                                                |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 中文解释

### 事件与状态转换

**1. 什么构成事件**
- 数据包到达：触发 `dissect_xxx()` 函数
- 帧/消息类型：TCP标志、TLS记录类型、HTTP/2帧类型
- 字段值：握手类型、流ID、序列号
- 派生条件：序列间隙检测、重传识别、解密密钥可用

**2. 状态转换触发方式**
- 显式FSM：使用 `switch(current_state)` 和明确的状态枚举
- 隐式FSM：通过标志位检查和条件判断

**3. 解码逻辑中的隐式FSM**
- 状态编码在条件分支中
- 守卫条件作为状态检查
- 例如：`if (ssl->state & SSL_HAVE_SESSION_KEY)` 检查是否可以解密

**4. 隐式与显式FSM的权衡**
- 显式FSM：
  - 优点：清晰、易验证、自文档化
  - 缺点：僵硬、难处理边缘情况
  - 适用于：简单协议、线性状态进展
  
- 隐式FSM：
  - 优点：灵活、处理部分捕获、优雅降级
  - 缺点：难理解、状态分散在代码中
  - 适用于：复杂协议、多并发状态、实际流量分析
