# Section 3: State Representation in Wireshark Dissectors

## 3.1 State Structures Used by Dissectors

### TCP State Structure (packet-tcp.h)

```c
// Per-direction flow state
typedef struct _tcp_flow_t {
    uint32_t base_seq;              // Base sequence number
    uint32_t fin;                   // Frame number of FIN
    uint32_t window;                // Last seen window
    int16_t  win_scale;             // Window scaling factor
    uint16_t flags;                 // State flags
    wmem_tree_t *multisegment_pdus; // PDUs spanning segments
    wmem_list_t *ooo_segments;      // Out-of-order segments
    // ... additional fields
} tcp_flow_t;

// Per-connection state
struct tcp_analysis {
    tcp_flow_t flow1;               // Direction 1
    tcp_flow_t flow2;               // Direction 2
    tcp_flow_t *fwd;                // Current forward flow
    tcp_flow_t *rev;                // Current reverse flow
    struct tcp_acked *ta;           // ACK analysis data
    nstime_t ts_first;              // First packet timestamp
    uint32_t stream;                // Stream number
    uint8_t conversation_completeness; // SYN/FIN tracking
};
```

### TLS State Structure (packet-tls-utils.h)

```c
typedef struct _SslDecryptSession {
    unsigned char _master_secret[48];
    StringInfo session_id;
    StringInfo session_ticket;
    StringInfo server_random;
    StringInfo client_random;
    StringInfo master_secret;
    
    int state;                      // Current handshake state
    const SslCipherSuite *cipher_suite;
    SslDecoder *server;             // Server decoder
    SslDecoder *client;             // Client decoder
    SslSession session;             // Session parameters
} SslDecryptSession;
```

### HTTP/2 State Structure (packet-http2.c)

```c
typedef struct {
    wmem_queue_t *settings_queue[2];     // SETTINGS per direction
    nghttp2_hd_inflater *hd_inflater[2]; // HPACK inflaters
    wmem_map_t *per_stream_info;         // Stream -> info map
    uint32_t current_stream_id;          // Current stream
    tcp_flow_t *fwd_flow;                // Flow direction
    uint32_t initial_new_stream_window_size[2];
    int32_t current_connection_window_size[2];
} http2_session_t;

typedef struct {
    http2_oneway_stream_info_t oneway_stream_info[2];
    uint32_t stream_id;
    uint32_t request_in_frame_num;
    uint32_t response_in_frame_num;
    char *scheme;
    char *authority;
    char *path;
} http2_stream_info_t;
```

---

## 3.2 Per-Connection and Per-Stream State Allocation

```
ALLOCATION HIERARCHY:
=====================

wmem_file_scope()
    │
    ├── conversation_t
    │       │
    │       └── proto_data (via conversation_add_proto_data)
    │               │
    │               ├── tcp_analysis (per TCP connection)
    │               │       ├── flow1, flow2 (per direction)
    │               │       └── acked_table (per packet analysis)
    │               │
    │               ├── SslDecryptSession (per TLS session)
    │               │       ├── SslDecoder *server
    │               │       └── SslDecoder *client
    │               │
    │               └── http2_session_t (per HTTP/2 connection)
    │                       └── per_stream_info (wmem_map_t)
    │                               └── http2_stream_info_t (per stream)
    │
    └── reassembly_table
            └── fragment_head (per reassembly)
```

### Allocation Example (HTTP/2)

```c
// From packet-http2.c
static http2_session_t*
get_http2_session(packet_info *pinfo, conversation_t* conversation)
{
    http2_session_t *h2session;
    
    // Try to get existing session
    h2session = (http2_session_t*)conversation_get_proto_data(
        conversation, proto_http2);
    
    if (!h2session) {
        // Allocate new session with file scope (persists across packets)
        h2session = wmem_new0(wmem_file_scope(), http2_session_t);
        
        // Initialize per-stream map
        h2session->per_stream_info = wmem_map_new(
            wmem_file_scope(), g_direct_hash, g_direct_equal);
        
        // Store in conversation
        conversation_add_proto_data(conversation, proto_http2, h2session);
    }
    
    return h2session;
}
```

---

## 3.3 Why Wireshark Avoids Single Enum FSMs

```
TEXTBOOK FSM (Single Enum):           WIRESHARK FSM (Composite State):
============================           =============================

enum tcp_state {                      struct tcp_analysis {
    STATE_CLOSED,                         // Multiple orthogonal states:
    STATE_LISTEN,                         
    STATE_SYN_SENT,                       uint8_t conversation_completeness;
    STATE_SYN_RCVD,                       // BITS: SYN_SENT | SYN_ACK_RECV | 
    STATE_ESTABLISHED,                    //       ACK_RECV | DATA_SENT | ...
    STATE_FIN_WAIT_1,                     
    STATE_FIN_WAIT_2,                     tcp_flow_t flow1, flow2;
    STATE_CLOSE_WAIT,                     // Each flow has its own state
    STATE_CLOSING,                        
    STATE_LAST_ACK,                       struct tcp_acked *ta;
    STATE_TIME_WAIT                       // Per-packet analysis flags
};                                    };
```

### Reasons for Avoiding Single Enum FSM

1. **Orthogonal State Dimensions**
   - TCP connection has multiple independent aspects
   - Sequence state, ACK state, window state, retransmission state
   - These change independently

2. **Bidirectional Flows**
   - Each direction has its own state
   - Cannot be captured in single enum

3. **Partial State Information**
   - Capture may start mid-connection
   - Cannot determine exact protocol state
   - Flags track "what we've seen" rather than "what state we're in"

4. **Analysis vs Protocol State**
   - Protocol spec defines states
   - Dissector tracks analysis state (different concern)
   - Need to handle malformed/unusual traffic

---

## 3.4 Partial States and Flags Instead of Enums

### Example: TCP Conversation Completeness

```c
// From packet-tcp.c
#define TCP_COMPLETENESS_SYNSENT        0x01  // SYN seen
#define TCP_COMPLETENESS_SYNACK         0x02  // SYN-ACK seen  
#define TCP_COMPLETENESS_ACK            0x04  // Completing ACK seen
#define TCP_COMPLETENESS_DATA           0x08  // Data seen
#define TCP_COMPLETENESS_FIN            0x10  // FIN seen
#define TCP_COMPLETENESS_RST            0x20  // RST seen

// State is a bitmask, not a single enum value
tcpd->conversation_completeness |= TCP_COMPLETENESS_SYNSENT;
```

### Example: TLS State Flags

```c
// From packet-tls-utils.h
#define SSL_CLIENT_RANDOM       (1<<0)
#define SSL_SERVER_RANDOM       (1<<1)
#define SSL_CIPHER              (1<<2)
#define SSL_HAVE_SESSION_KEY    (1<<3)
#define SSL_VERSION             (1<<4)
#define SSL_MASTER_SECRET       (1<<5)
#define SSL_PRE_MASTER_SECRET   (1<<6)
#define SSL_ENCRYPT_THEN_MAC    (1<<11)
#define SSL_SEEN_0RTT_APPDATA   (1<<12)

// Check state with bitwise operations
if (ssl->state & SSL_HAVE_SESSION_KEY) {
    // Can decrypt
}
```

### Example: TWAMP Control State (Explicit Enum)

```c
// From packet-twamp.c - This is a rare explicit FSM
enum twamp_control_state {
    CONTROL_STATE_UNKNOWN = 0,
    CONTROL_STATE_GREETING,
    CONTROL_STATE_SETUP_RESPONSE,
    CONTROL_STATE_SERVER_START,
    CONTROL_STATE_REQUEST_SESSION,
    CONTROL_STATE_ACCEPT_SESSION,
    CONTROL_STATE_START_SESSIONS,
    CONTROL_STATE_START_SESSIONS_ACK,
    CONTROL_STATE_TEST_RUNNING,
    CONTROL_STATE_STOP_SESSIONS,
    CONTROL_STATE_REQUEST_TW_SESSION
};
```

---

## State Representation Patterns Summary

```
+------------------------------------------------------------------+
|                    STATE REPRESENTATION PATTERNS                  |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 1: Bitmask Flags                                         |
|  ========================                                         |
|  uint16_t flags;                                                  |
|  flags |= FLAG_SEEN_X;                                            |
|  if (flags & FLAG_SEEN_Y) { ... }                                 |
|                                                                   |
|  USE WHEN: Multiple independent boolean states                    |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 2: Composite Structs                                     |
|  ============================                                     |
|  struct state {                                                   |
|      flow_state_t flow[2];                                        |
|      analysis_t analysis;                                         |
|      crypto_t crypto;                                             |
|  };                                                               |
|                                                                   |
|  USE WHEN: Multiple orthogonal state dimensions                   |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|  PATTERN 3: Explicit Enum                                         |
|  ========================                                         |
|  enum protocol_state { STATE_A, STATE_B, STATE_C };               |
|  switch(state) { case STATE_A: ... }                              |
|                                                                   |
|  USE WHEN: Simple linear protocol with clear states               |
|                                                                   |
+------------------------------------------------------------------+
```

---

## 中文解释

### 状态表示方法

**1. 解析器使用的状态结构**
- TCP：`tcp_analysis` 包含双向流状态、ACK分析、时间戳
- TLS：`SslDecryptSession` 包含握手状态、密钥材料、解码器
- HTTP/2：`http2_session_t` 包含设置队列、HPACK状态、流映射

**2. 状态分配策略**
- 使用 `wmem_file_scope()` 分配持久状态
- 通过 `conversation_add_proto_data()` 关联到会话
- 按需分配子状态（如每个流的状态）

**3. 为什么避免单一枚举FSM**
- 协议状态有多个正交维度（发送方向、接收方向、加密状态等）
- 捕获可能从连接中间开始
- 需要处理异常/恶意流量
- 分析状态与协议规范状态不同

**4. 使用标志位代替枚举**
- TCP使用 `conversation_completeness` 位掩码追踪看到的事件
- TLS使用状态标志追踪密钥材料可用性
- 位掩码允许独立追踪多个状态维度
- 更灵活、更健壮
