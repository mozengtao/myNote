# Section 2: FSM Boundaries in Wireshark

## 2.1 Where Protocol State Is Stored

```
+-------------------------------------------------------------------------+
|                           WIRESHARK MEMORY MODEL                         |
+-------------------------------------------------------------------------+
|                                                                          |
|  +------------------+    +------------------+    +------------------+    |
|  | wmem_epan_scope  |    | wmem_file_scope  |    | wmem_packet_scope|    |
|  | (Application)    |    | (Capture File)   |    | (Single Packet)  |    |
|  +--------+---------+    +--------+---------+    +--------+---------+    |
|           |                       |                       |              |
|           v                       v                       v              |
|  +------------------+    +------------------+    +------------------+    |
|  | Protocol         |    | Conversation     |    | Per-Packet       |    |
|  | Registration     |    | State            |    | Temporary Data   |    |
|  | (Static)         |    | (Session Data)   |    |                  |    |
|  +------------------+    +------------------+    +------------------+    |
|                                                                          |
+-------------------------------------------------------------------------+
```

### State Storage Locations in Wireshark

1. **Conversation Layer** (`conversation_t`)
   - Hash tables keyed by (addr1, port1, addr2, port2)
   - Holds per-conversation data via `conversation_add_proto_data()`
   - Lives for the duration of the capture file

2. **Protocol-Specific Structures**
   ```c
   // Example: TLS state stored in SslDecryptSession
   typedef struct _SslDecryptSession {
       int state;                    // Current FSM state
       StringInfo master_secret;     // Derived key material
       SslDecoder *server;           // Server-side decoder state
       SslDecoder *client;           // Client-side decoder state
       SslSession session;           // Session parameters
   } SslDecryptSession;
   ```

3. **Per-Packet Data** (`proto_data`)
   - Temporary data for current packet processing
   - Used for multi-pass dissection

---

## 2.2 Separation of Parsing Logic from State Tracking

```
+---------------------+        +---------------------+
|   PARSING LOGIC     |        |   STATE TRACKING    |
+---------------------+        +---------------------+
|                     |        |                     |
| dissect_tcp()       |<------>| tcp_analysis        |
| - Read TCP header   |        | - seq numbers       |
| - Extract fields    |        | - ack tracking      |
| - Validate checksum |        | - retransmission    |
|                     |        |                     |
+---------------------+        +---------------------+
|                     |        |                     |
| dissect_tls()       |<------>| SslDecryptSession   |
| - Read record       |        | - handshake state   |
| - Parse handshake   |        | - cipher suite      |
| - Decrypt if ready  |        | - keys              |
|                     |        |                     |
+---------------------+        +---------------------+
```

### Code Pattern Example (from packet-tcp.h)

```c
// State structure (STATE TRACKING)
struct tcp_analysis {
    tcp_flow_t  flow1;              // One direction
    tcp_flow_t  flow2;              // Other direction
    tcp_flow_t  *fwd;               // Current forward flow
    tcp_flow_t  *rev;               // Current reverse flow
    struct tcp_acked *ta;           // ACK analysis
    wmem_tree_t *acked_table;       // Frame -> ACK info
    nstime_t    ts_first;           // First packet time
    uint32_t    stream;             // Stream number
    // ... more state fields
};

// Parsing logic retrieves state (PARSING)
struct tcp_analysis *get_tcp_conversation_data(
    conversation_t *conv, 
    packet_info *pinfo
);
```

---

## 2.3 Why State Must Persist Across Packets

```
Packet Timeline:
================

  Time ─────────────────────────────────────────────────────────────>

  Pkt 1        Pkt 2        Pkt 3        Pkt 4        Pkt 5
  [SYN]   ─>   [SYN-ACK]─>  [ACK]   ─>   [Data]  ─>   [ACK]
    │              │           │            │           │
    v              v           v            v           v
  ┌────────────────────────────────────────────────────────────┐
  │                 PERSISTENT STATE                            │
  ├────────────────────────────────────────────────────────────┤
  │ After Pkt 1: state = SYN_SENT, seq = ISN_client             │
  │ After Pkt 2: state = SYN_RECV, seq = ISN_server             │
  │ After Pkt 3: state = ESTABLISHED                            │
  │ After Pkt 4: bytes_sent++, track retransmissions            │
  │ After Pkt 5: bytes_acked++, calculate RTT                   │
  └────────────────────────────────────────────────────────────┘
```

### Key Reasons for Persistence

1. **Sequence Tracking**
   - Must remember expected sequence numbers
   - Detect gaps, retransmissions, out-of-order

2. **Protocol Negotiation**
   - TLS cipher suite negotiated once, used for entire session
   - HTTP/2 SETTINGS affect all subsequent frames

3. **Reassembly**
   - Fragments arrive in multiple packets
   - Must buffer until complete PDU received

4. **Security Analysis**
   - Track anomalies across packet boundaries
   - Detect protocol violations

---

## 2.4 Sessions and Conversations as FSM Containers

```
                    +-----------------------+
                    |   CONVERSATION_TABLE  |
                    |   (Global Hash Map)   |
                    +-----------+-----------+
                                |
            +-------------------+-------------------+
            |                   |                   |
            v                   v                   v
    +---------------+   +---------------+   +---------------+
    | Conversation  |   | Conversation  |   | Conversation  |
    | Key: TCP      |   | Key: TCP      |   | Key: UDP      |
    | 10.0.0.1:4433 |   | 192.168.1.1:22|   | 8.8.8.8:53    |
    | 10.0.0.2:443  |   | 192.168.1.2:54321| | 10.0.0.1:12345|
    +-------+-------+   +-------+-------+   +-------+-------+
            |                   |                   |
            v                   v                   v
    +---------------+   +---------------+   +---------------+
    | Proto Data:   |   | Proto Data:   |   | Proto Data:   |
    | - TLS session |   | - SSH state   |   | - DNS xact ID |
    | - HTTP/2 sess |   | - Key exchg   |   | - Query cache |
    +---------------+   +---------------+   +---------------+
```

### Conversation API Pattern

```c
// From conversation.h

// Create or find a conversation
conversation_t *find_or_create_conversation(packet_info *pinfo);

// Attach protocol-specific state
void conversation_add_proto_data(
    conversation_t *conv, 
    int proto, 
    void *proto_data
);

// Retrieve protocol-specific state
void *conversation_get_proto_data(
    const conversation_t *conv, 
    int proto
);
```

### Multi-Layer State Example (HTTP/2 over TLS)

```
+------------------------------------------------------------------+
|                    TCP Conversation                               |
|  +------------------------------------------------------------+  |
|  |                  TLS Session State                          |  |
|  |  +------------------------------------------------------+  |  |
|  |  |               HTTP/2 Session State                    |  |  |
|  |  |  +------------------------------------------------+  |  |  |
|  |  |  |            HTTP/2 Stream State                  |  |  |  |
|  |  |  |  (per stream_id: headers, body, state)          |  |  |  |
|  |  |  +------------------------------------------------+  |  |  |
|  |  +------------------------------------------------------+  |  |
|  +------------------------------------------------------------+  |
+------------------------------------------------------------------+
```

---

## 中文解释

### FSM边界与状态存储

**1. 状态存储位置**
- Wireshark使用分层内存管理：应用级、文件级、包级
- 协议状态存储在"conversation"（会话）数据结构中
- 每个协议定义自己的状态结构体（如 `tcp_analysis`、`SslDecryptSession`）

**2. 解析逻辑与状态追踪的分离**
- 解析逻辑：读取字段、解码数据、验证格式
- 状态追踪：记录连接状态、序列号、协商参数
- 这种分离使代码更清晰、更易维护

**3. 状态必须跨包持久化的原因**
- TCP序列号追踪：检测重传、乱序
- TLS加密：握手时协商的密钥在整个会话中使用
- 重组：跨多个包的数据片段需要缓存

**4. 会话作为FSM容器**
- `conversation_t` 是Wireshark的核心抽象
- 通过地址/端口四元组标识
- 支持附加任意协议特定数据
- 层级结构：TCP会话 → TLS会话 → HTTP/2会话 → HTTP/2流
