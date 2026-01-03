# Section 5: Error Handling and Resynchronization

## 5.1 How Malformed Packets Are Handled

```
+------------------------------------------------------------------+
|                 MALFORMED PACKET HANDLING FLOW                    |
+------------------------------------------------------------------+
|                                                                   |
|  +-------------+                                                  |
|  |   Packet    |                                                  |
|  +------+------+                                                  |
|         |                                                         |
|         v                                                         |
|  +-------------+    NO     +------------------+                   |
|  | Valid       |---------->| Mark as Malformed|                   |
|  | Header?     |           | Add Expert Info  |                   |
|  +------+------+           | Continue Parsing |                   |
|         | YES              +------------------+                   |
|         v                                                         |
|  +-------------+    NO     +------------------+                   |
|  | Valid       |---------->| Truncate Display |                   |
|  | Length?     |           | Show What We Can |                   |
|  +------+------+           +------------------+                   |
|         | YES                                                     |
|         v                                                         |
|  +-------------+    NO     +------------------+                   |
|  | Valid       |---------->| Flag Violation   |                   |
|  | State?      |           | Don't Crash      |                   |
|  +------+------+           | Best-Effort Parse|                   |
|         | YES              +------------------+                   |
|         v                                                         |
|  +-------------+                                                  |
|  | Normal      |                                                  |
|  | Processing  |                                                  |
|  +-------------+                                                  |
|                                                                   |
+------------------------------------------------------------------+
```

### Expert Info System

```c
// From packet-tcp.c - Expert information for analysis
static expert_field ei_tcp_analysis_retransmission;
static expert_field ei_tcp_analysis_fast_retransmission;
static expert_field ei_tcp_analysis_spurious_retransmission;
static expert_field ei_tcp_analysis_out_of_order;
static expert_field ei_tcp_analysis_reused_ports;
static expert_field ei_tcp_analysis_lost_packet;
static expert_field ei_tcp_analysis_ack_lost_packet;
static expert_field ei_tcp_analysis_window_update;
static expert_field ei_tcp_analysis_window_full;
static expert_field ei_tcp_analysis_keep_alive;
static expert_field ei_tcp_analysis_duplicate_ack;
static expert_field ei_tcp_analysis_zero_window;
static expert_field ei_tcp_suboption_malformed;

// Usage
if (ta->flags & TCP_A_RETRANSMISSION) {
    expert_add_info(pinfo, flags_item, &ei_tcp_analysis_retransmission);
}
```

### Malformed Packet Patterns

```c
// Pattern 1: Length validation
static int
dissect_protocol(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    // Check minimum length
    if (tvb_captured_length(tvb) < MIN_HEADER_LEN) {
        expert_add_info(pinfo, item, &ei_packet_short);
        return tvb_captured_length(tvb);  // Return what we have
    }
    
    uint32_t len = tvb_get_ntohl(tvb, offset);
    
    // Sanity check length field
    if (len > MAX_REASONABLE_LENGTH) {
        proto_tree_add_expert(tree, pinfo, &ei_bad_length, tvb, offset, 4);
        // Continue with clamped length
        len = tvb_captured_length_remaining(tvb, offset);
    }
    
    // ...
}

// Pattern 2: State validation
static int
dissect_tls_handshake(tvbuff_t *tvb, packet_info *pinfo, 
                      proto_tree *tree, SslDecryptSession *ssl)
{
    uint8_t handshake_type = tvb_get_uint8(tvb, offset);
    
    // Validate handshake message sequence
    switch (handshake_type) {
        case SSL_HND_SERVER_HELLO:
            if (!(ssl->state & SSL_CLIENT_RANDOM)) {
                // ServerHello without ClientHello - flag but continue
                expert_add_info(pinfo, item, &ei_tls_unexpected_message);
            }
            break;
            
        case SSL_HND_FINISHED:
            if (!(ssl->state & SSL_HAVE_SESSION_KEY)) {
                // Finished without key material
                expert_add_info(pinfo, item, &ei_tls_unexpected_message);
            }
            break;
    }
    
    // Continue parsing regardless
    // ...
}
```

---

## 5.2 Recovery from Invalid State

### Strategy 1: Best-Effort Parsing

```c
// Parse what we can, flag what we can't
static int
dissect_http2_data(tvbuff_t *tvb, packet_info *pinfo, 
                   proto_tree *tree, http2_session_t *h2session)
{
    http2_stream_info_t *stream = get_stream_info(pinfo, h2session);
    
    // Stream might be unknown (capture started mid-connection)
    if (!stream) {
        // Create dummy stream info
        stream = wmem_new0(wmem_file_scope(), http2_stream_info_t);
        stream->stream_id = stream_id;
        // Mark as synthetic
        stream->state = STREAM_STATE_UNKNOWN;
        
        expert_add_info(pinfo, item, &ei_http2_unknown_stream);
    }
    
    // Continue parsing with whatever info we have
    // ...
}
```

### Strategy 2: State Reset on Error

```c
// Reset state when unrecoverable error detected
static void
handle_protocol_error(protocol_state_t *state, packet_info *pinfo)
{
    // Log the error
    expert_add_info(pinfo, NULL, &ei_protocol_error);
    
    // Reset to safe state
    state->phase = PHASE_UNKNOWN;
    state->pending_operations = NULL;
    
    // Mark all subsequent packets as potentially affected
    state->error_frame = pinfo->num;
}
```

### Strategy 3: Graceful Degradation

```c
// Reduce functionality when state is uncertain
static int
dissect_encrypted_record(tvbuff_t *tvb, packet_info *pinfo,
                         proto_tree *tree, SslDecryptSession *ssl)
{
    if (ssl->state & SSL_HAVE_SESSION_KEY) {
        // Full decryption
        return decrypt_and_dissect(tvb, pinfo, tree, ssl);
    } else {
        // Partial analysis without decryption
        proto_tree_add_item(tree, hf_encrypted_data, tvb, 0, -1, ENC_NA);
        col_append_str(pinfo->cinfo, COL_INFO, " (encrypted)");
        return tvb_captured_length(tvb);
    }
}
```

---

## 5.3 FSM Robustness vs Strictness Trade-offs

```
STRICTNESS SPECTRUM:
====================

STRICT                                                        LENIENT
  |                                                              |
  v                                                              v
+----------+     +-----------+     +------------+     +----------+
| Reject   |     | Flag &    |     | Best-Effort|     | Accept   |
| Invalid  |     | Continue  |     | Parse      |     | Anything |
+----------+     +-----------+     +------------+     +----------+
     |                |                 |                  |
     v                v                 v                  v
Protocol         Wireshark         Wireshark         Fuzzing
Compliance       Default           Analysis          Tools
Testing          Behavior          Mode
```

### Trade-off Matrix

| Approach | Advantages | Disadvantages | Use Case |
|----------|-----------|---------------|----------|
| **Strict** | Protocol compliance, clear errors | Fragile, loses data | Compliance testing |
| **Flag & Continue** | Best of both worlds | Complex implementation | Production dissectors |
| **Best-Effort** | Maximum data extraction | May misinterpret | Analysis of damaged captures |
| **Accept All** | Never crashes | Garbage output | Fuzz testing |

### Wireshark's Philosophy

```
+------------------------------------------------------------------+
|                 WIRESHARK ERROR HANDLING PHILOSOPHY               |
+------------------------------------------------------------------+
|                                                                   |
|  1. NEVER CRASH                                                   |
|     - Buffer overrun checks everywhere                            |
|     - Recursion depth limits                                      |
|     - Memory allocation limits                                    |
|                                                                   |
|  2. SHOW WHAT YOU CAN                                             |
|     - Partial dissection better than nothing                      |
|     - Display raw bytes when parsing fails                        |
|     - Truncated display for truncated packets                     |
|                                                                   |
|  3. FLAG DON'T REJECT                                             |
|     - Expert info marks problems                                  |
|     - Continue parsing after errors                               |
|     - Let user decide if data is valid                            |
|                                                                   |
|  4. DEFENSIVE PARSING                                             |
|     - Validate lengths before use                                 |
|     - Bounds-check all array accesses                             |
|     - Limit loop iterations                                       |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 5.4 Resynchronization Mechanisms

### TCP Stream Reassembly Resync

```c
// From reassemble.c - Handling gaps in reassembly
fragment_head *
fragment_add_check(reassembly_table *table, tvbuff_t *tvb, int offset,
                   packet_info *pinfo, uint32_t id, const void *data,
                   uint32_t frag_offset, uint32_t frag_data_len,
                   bool more_frags)
{
    fragment_head *fd_head;
    
    // Check for gaps
    if (frag_offset > fd_head->contiguous_len) {
        // Gap detected - store fragment for later
        // Don't fail, wait for missing piece
        fd_head->flags |= FD_PARTIAL_REASSEMBLY;
    }
    
    // Check for overlap conflicts
    if (/* overlap with different data */) {
        fd_head->flags |= FD_OVERLAPCONFLICT;
        expert_add_info(pinfo, NULL, &ei_reassembly_overlap);
    }
    
    return fd_head;
}
```

### Protocol Resync Points

```
+------------------------------------------------------------------+
|               COMMON RESYNCHRONIZATION POINTS                     |
+------------------------------------------------------------------+
|                                                                   |
|  TCP:                                                             |
|  - SYN packet (new connection)                                    |
|  - RST packet (connection reset)                                  |
|  - Known sequence number                                          |
|                                                                   |
|  TLS:                                                             |
|  - Record boundaries (length-prefixed)                            |
|  - Alert messages                                                 |
|  - New handshake                                                  |
|                                                                   |
|  HTTP/2:                                                          |
|  - Frame boundaries (9-byte header)                               |
|  - GOAWAY frame                                                   |
|  - Connection preface                                             |
|                                                                   |
|  SMB2:                                                            |
|  - SMB2 magic number (0xFE 'S' 'M' 'B')                          |
|  - Session setup                                                  |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 中文解释

### 错误处理与重同步

**1. 畸形数据包处理**
- 永不崩溃：到处都有缓冲区溢出检查
- 显示能显示的：部分解析胜于无
- 标记而非拒绝：使用Expert Info标记问题
- 防御性解析：验证长度、边界检查、限制循环

**2. 从无效状态恢复**
- 策略1-尽力解析：解析能解析的，标记不能的
- 策略2-错误时重置状态：检测到不可恢复错误时重置
- 策略3-优雅降级：状态不确定时减少功能

**3. 健壮性与严格性的权衡**
- 严格模式：协议合规测试
- 标记并继续：Wireshark默认行为
- 尽力解析：分析损坏的捕获
- 接受所有：模糊测试

**4. 重同步机制**
- TCP：SYN包、RST包、已知序列号
- TLS：记录边界、Alert消息、新握手
- HTTP/2：帧边界、GOAWAY帧、连接前言
- 分片重组：处理间隙、冲突检测
