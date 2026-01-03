# Section 7: FSM Patterns Observed in Wireshark

## 7.1 Implicit FSMs

### Definition

```
+------------------------------------------------------------------+
|                      IMPLICIT FSM PATTERN                         |
+------------------------------------------------------------------+
|                                                                   |
|  State is NOT stored as an explicit enum variable.                |
|  Instead, state is determined by:                                 |
|    - Combination of flags                                         |
|    - Presence/absence of data                                     |
|    - Values of accumulated fields                                 |
|    - Guard conditions in parsing logic                            |
|                                                                   |
+------------------------------------------------------------------+
```

### Example: TCP State Implicit in Completeness Flags

```c
// TCP connection "state" is derived from flags, not stored explicitly
#define TCP_COMPLETENESS_SYNSENT   0x01
#define TCP_COMPLETENESS_SYNACK    0x02
#define TCP_COMPLETENESS_ACK       0x04
#define TCP_COMPLETENESS_DATA      0x08
#define TCP_COMPLETENESS_FIN       0x10
#define TCP_COMPLETENESS_RST       0x20

// Derived states:
// CLOSED:      completeness == 0
// SYN_SENT:    completeness & SYNSENT
// ESTABLISHED: completeness & (SYNSENT | SYNACK | ACK)
// DATA_SENT:   completeness & DATA
// CLOSING:     completeness & FIN
// RESET:       completeness & RST

static const char*
get_tcp_state_string(uint8_t completeness) {
    // State derived from flag combination
    if (completeness & TCP_COMPLETENESS_RST) return "Reset";
    if ((completeness & 0x07) == 0x07) return "Established";
    if (completeness & TCP_COMPLETENESS_SYNACK) return "SYN-ACK Received";
    if (completeness & TCP_COMPLETENESS_SYNSENT) return "SYN Sent";
    return "Unknown";
}
```

### Example: TLS State Implicit in Key Availability

```c
// TLS "state" derived from what keys/data are available
if (ssl->state & SSL_HAVE_SESSION_KEY) {
    // Implicitly in "application data" state
    decrypt_record(ssl, tvb);
} else if (ssl->state & SSL_MASTER_SECRET) {
    // Implicitly in "key derivation possible" state
    derive_keys(ssl);
} else if (ssl->state & SSL_PRE_MASTER_SECRET) {
    // Implicitly in "pre-master known" state
    compute_master_secret(ssl);
} else {
    // Implicitly in "handshake in progress" state
    continue_handshake_parsing(ssl, tvb);
}
```

---

## 7.2 Table-Driven Parsing

### Definition

```
+------------------------------------------------------------------+
|                  TABLE-DRIVEN PARSING PATTERN                     |
+------------------------------------------------------------------+
|                                                                   |
|  Instead of switch/if-else chains, use lookup tables to:          |
|    - Map message types to handlers                                |
|    - Map field IDs to parsing functions                           |
|    - Define state transitions                                     |
|                                                                   |
|  Benefits:                                                        |
|    - Cleaner code                                                 |
|    - Easier to extend                                             |
|    - Better cache utilization                                     |
|                                                                   |
+------------------------------------------------------------------+
```

### Example: SMB2 Command Dispatch Table

```c
// From packet-smb2.c - Table-driven command dispatch
typedef struct {
    int (*request)(tvbuff_t *tvb, packet_info *pinfo, 
                   proto_tree *tree, int offset, smb2_info_t *si);
    int (*response)(tvbuff_t *tvb, packet_info *pinfo, 
                    proto_tree *tree, int offset, smb2_info_t *si);
} smb2_function;

static const smb2_function smb2_dissector[] = {
    /* 0x00 */ {dissect_smb2_negotiate_request, dissect_smb2_negotiate_response},
    /* 0x01 */ {dissect_smb2_session_setup_request, dissect_smb2_session_setup_response},
    /* 0x02 */ {dissect_smb2_logoff_request, dissect_smb2_logoff_response},
    /* 0x03 */ {dissect_smb2_tree_connect_request, dissect_smb2_tree_connect_response},
    /* 0x04 */ {dissect_smb2_tree_disconnect_request, dissect_smb2_tree_disconnect_response},
    /* 0x05 */ {dissect_smb2_create_request, dissect_smb2_create_response},
    /* 0x06 */ {dissect_smb2_close_request, dissect_smb2_close_response},
    // ... more commands
};

static int
dissect_smb2_command(tvbuff_t *tvb, packet_info *pinfo, 
                     proto_tree *tree, smb2_info_t *si)
{
    uint16_t cmd = si->opcode;
    
    // Table lookup instead of switch
    if (cmd < array_length(smb2_dissector)) {
        if (si->flags & SMB2_FLAGS_RESPONSE) {
            return smb2_dissector[cmd].response(tvb, pinfo, tree, 0, si);
        } else {
            return smb2_dissector[cmd].request(tvb, pinfo, tree, 0, si);
        }
    }
    return 0;
}
```

### Example: TLS Extension Table

```c
// Table of extension handlers
static const ssl_ext_dissector_t ssl_ext_dissectors[] = {
    {SSL_HND_HELLO_EXT_SERVER_NAME, dissect_ssl_ext_server_name},
    {SSL_HND_HELLO_EXT_STATUS_REQUEST, dissect_ssl_ext_status_request},
    {SSL_HND_HELLO_EXT_SUPPORTED_GROUPS, dissect_ssl_ext_supported_groups},
    {SSL_HND_HELLO_EXT_SIGNATURE_ALGORITHMS, dissect_ssl_ext_sig_algs},
    // ... more extensions
    {0, NULL}  // Sentinel
};

static void
dissect_ssl_extensions(tvbuff_t *tvb, proto_tree *tree, 
                       SslDecryptSession *ssl)
{
    while (offset < end) {
        uint16_t ext_type = tvb_get_ntohs(tvb, offset);
        
        // Table lookup for handler
        for (int i = 0; ssl_ext_dissectors[i].dissector; i++) {
            if (ssl_ext_dissectors[i].type == ext_type) {
                ssl_ext_dissectors[i].dissector(tvb, tree, ssl);
                break;
            }
        }
    }
}
```

---

## 7.3 Flag-Based Sub-States

### Definition

```
+------------------------------------------------------------------+
|                   FLAG-BASED SUB-STATES PATTERN                   |
+------------------------------------------------------------------+
|                                                                   |
|  Main state + flags for orthogonal concerns:                      |
|                                                                   |
|  struct state {                                                   |
|      enum main_state phase;     // Primary FSM state              |
|      uint32_t flags;            // Orthogonal properties          |
|  };                                                               |
|                                                                   |
|  Allows tracking:                                                 |
|    - Main protocol phase                                          |
|    - Encryption status                                            |
|    - Optional features enabled                                    |
|    - Error conditions                                             |
|                                                                   |
+------------------------------------------------------------------+
```

### Example: TCP Flow State with Flags

```c
typedef struct _tcp_flow_t {
    // Base state (sequence tracking)
    uint32_t base_seq;
    uint32_t nextseq;
    
    // Static flags (set once)
    uint8_t static_flags;
    #define TCP_S_BASE_SEQ_SET  0x01
    #define TCP_S_SAW_SYN       0x02
    #define TCP_S_SAW_FIN       0x04
    
    // Dynamic flags (change per packet)
    uint32_t lastsegmentflags;
    #define TCP_A_RETRANSMISSION   0x0001
    #define TCP_A_OUT_OF_ORDER     0x0002
    #define TCP_A_ZERO_WINDOW      0x0004
    #define TCP_A_KEEP_ALIVE       0x0008
    #define TCP_A_DUPLICATE_ACK    0x0010
    
    // Feature flags
    uint16_t flags;
    #define TCP_FLOW_REASSEMBLE_UNTIL_FIN  0x0001
    
} tcp_flow_t;
```

### Example: HTTP/2 Stream with Multiple Flag Types

```c
typedef struct {
    // Stream state (explicit enum)
    enum http2_stream_state state;
    
    // Content flags
    bool is_stream_http_connect;
    
    // Data tracking
    uint32_t request_in_frame_num;
    uint32_t response_in_frame_num;
    
    // Per-direction flags (indexed by flow direction)
    struct {
        bool is_window_initialized;
        int32_t current_window_size;
    } oneway_stream_info[2];
    
} http2_stream_info_t;
```

---

## 7.4 Partial Decoding FSMs

### Definition

```
+------------------------------------------------------------------+
|                 PARTIAL DECODING FSM PATTERN                      |
+------------------------------------------------------------------+
|                                                                   |
|  FSM tracks what CAN be decoded, not protocol state:              |
|                                                                   |
|  - Decryption keys available?                                     |
|  - Enough data for reassembly?                                    |
|  - Required context from previous packets?                        |
|  - Decompression dictionary ready?                                |
|                                                                   |
|  Allows partial dissection when full state unknown.               |
|                                                                   |
+------------------------------------------------------------------+
```

### Example: TLS Partial Decoding

```c
// TLS decoding state - tracks decoding capability, not protocol state
typedef struct _SslDecryptSession {
    int state;  // Bit flags for what we can do
    
    // State bits determine decoding capability
    #define SSL_CLIENT_RANDOM       (1<<0)  // Can correlate to keylog
    #define SSL_SERVER_RANDOM       (1<<1)  // Can correlate to keylog
    #define SSL_CIPHER              (1<<2)  // Know how to decrypt
    #define SSL_HAVE_SESSION_KEY    (1<<3)  // Can decrypt now
    #define SSL_MASTER_SECRET       (1<<5)  // Can derive keys
    #define SSL_PRE_MASTER_SECRET   (1<<6)  // Can compute master
    
} SslDecryptSession;

// Decoding decision based on available info
static int
dissect_ssl3_record(SslDecryptSession *ssl, tvbuff_t *tvb)
{
    if (ssl->state & SSL_HAVE_SESSION_KEY) {
        // Full decryption possible
        return decrypt_and_dissect_record(ssl, tvb);
    } else if (ssl->state & SSL_CIPHER) {
        // Know cipher but no keys - show metadata
        return dissect_encrypted_record_metadata(ssl, tvb);
    } else {
        // Minimal info - show raw record
        return dissect_raw_record(tvb);
    }
}
```

### Example: TCP Reassembly Partial State

```c
// Reassembly can proceed with partial data
struct tcp_multisegment_pdu {
    uint32_t seq;
    uint32_t nxtpdu;
    uint32_t first_frame;
    uint32_t last_frame;
    
    // Partial state flags
    uint32_t flags;
    #define MSP_FLAGS_REASSEMBLE_ENTIRE_SEGMENT  0x0001
    #define MSP_FLAGS_GOT_ALL_SEGMENTS           0x0002
    #define MSP_FLAGS_MISSING_FIRST_SEGMENT      0x0004
};

// Can show partial reassembly even without all segments
if (msp->flags & MSP_FLAGS_GOT_ALL_SEGMENTS) {
    // Complete - full dissection
    dissect_complete_pdu(msp);
} else if (!(msp->flags & MSP_FLAGS_MISSING_FIRST_SEGMENT)) {
    // Have start - can show header at least
    dissect_partial_pdu_header(msp);
} else {
    // Missing start - show fragment only
    show_fragment_data(msp);
}
```

---

## Pattern Summary

```
+------------------------------------------------------------------+
|               FSM PATTERNS IN WIRESHARK SUMMARY                   |
+------------------------------------------------------------------+
|                                                                   |
|  IMPLICIT FSM                                                     |
|  ===========                                                      |
|  State derived from flags/data, not explicit variable             |
|  Pro: Flexible, handles partial captures                          |
|  Con: Hard to trace state, scattered logic                        |
|  Example: TCP completeness, TLS key availability                  |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  TABLE-DRIVEN                                                     |
|  ============                                                     |
|  Lookup tables map types to handlers                              |
|  Pro: Clean code, easy to extend, cache-friendly                  |
|  Con: Less flexibility in handler selection                       |
|  Example: SMB2 commands, TLS extensions                           |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  FLAG-BASED SUB-STATES                                            |
|  ======================                                           |
|  Main state + orthogonal flags                                    |
|  Pro: Tracks multiple concerns independently                      |
|  Con: Can be complex to reason about                              |
|  Example: TCP flow flags, HTTP/2 stream flags                     |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PARTIAL DECODING                                                 |
|  ================                                                 |
|  Track decoding capability, not protocol state                    |
|  Pro: Graceful degradation, robust to missing data                |
|  Con: May produce incomplete output                               |
|  Example: TLS without keys, TCP with gaps                         |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 中文解释

### Wireshark中观察到的FSM模式

**1. 隐式FSM**
- 状态不存储为显式枚举变量
- 状态从标志位、数据存在性、字段值推导
- 优点：灵活，处理部分捕获
- 缺点：难以追踪状态，逻辑分散
- 示例：TCP完整性标志，TLS密钥可用性

**2. 表驱动解析**
- 使用查找表映射类型到处理函数
- 优点：代码清晰，易扩展，缓存友好
- 缺点：处理函数选择灵活性较低
- 示例：SMB2命令分发表，TLS扩展表

**3. 基于标志的子状态**
- 主状态 + 正交关注点的标志
- 优点：独立追踪多个关注点
- 缺点：推理可能复杂
- 示例：TCP流标志，HTTP/2流标志

**4. 部分解码FSM**
- 追踪解码能力而非协议状态
- 优点：优雅降级，对缺失数据健壮
- 缺点：可能产生不完整输出
- 示例：无密钥的TLS，有间隙的TCP
