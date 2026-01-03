# Section 1: Why Protocol Parsers Need FSMs

## 1.1 Why Protocol Decoding Cannot Be Stateless

```
+------------------+     +------------------+     +------------------+
|   Packet 1       |     |   Packet 2       |     |   Packet 3       |
|  [SYN]           |---->|  [SYN-ACK]       |---->|  [ACK + Data]    |
|  ClientHello?    |     |  ServerHello?    |     |  Encrypted?      |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
+------------------------------------------------------------------+
|                     STATE IS REQUIRED                             |
|  - Which handshake phase are we in?                              |
|  - What cipher suite was negotiated?                             |
|  - What keys have been exchanged?                                |
|  - Is the connection encrypted now?                              |
+------------------------------------------------------------------+
```

### Key Reasons for Statefulness

1. **Multi-Packet Transactions**
   - A single logical operation (e.g., TLS handshake) spans multiple packets
   - Each packet's meaning depends on what came before
   - Cannot determine message type from a single packet in isolation

2. **Context-Dependent Parsing**
   - The same byte sequence means different things in different states
   - Example: In TLS, `0x17` after handshake = Application Data; before = invalid

3. **Cryptographic State**
   - Encryption keys are derived from handshake data
   - Cannot decrypt packet N without knowing keys from packets 1..N-1

4. **Session Correlation**
   - Must track which packets belong to which session/stream
   - TCP sequence numbers, HTTP/2 stream IDs, SMB session IDs

---

## 1.2 How FSMs Differ from Simple Packet Parsing

```
SIMPLE PARSING (Stateless):                FSM PARSING (Stateful):
+----------------+                         +----------------+
| Read Header    |                         | Read Header    |
| Parse Fields   |                         | Check State    |----+
| Output Data    |                         | Parse Fields   |    |
+----------------+                         | Update State   |<---+
                                           | Output Data    |
                                           +----------------+
```

### Simple Parsing
- Treats each packet independently
- Works for: ARP, ICMP Echo, simple UDP protocols
- No memory of previous packets

### FSM Parsing
- Maintains state across packets
- Transitions between states based on events
- Can validate protocol sequences
- Required for: TCP, TLS, HTTP/2, SMB, etc.

---

## 1.3 Stateful Protocols in Wireshark

| Protocol | State Tracked | Why Stateful |
|----------|--------------|--------------|
| TCP | Connection state, sequence numbers, flow direction | Ordered delivery, retransmission detection |
| TLS/SSL | Handshake phase, cipher suite, keys | Encryption, key exchange |
| HTTP/2 | Stream states, HPACK table | Multiplexed streams, header compression |
| SMB/SMB2 | Session, tree, file handles | Multi-step file operations |
| DNS | Transaction ID correlation | Request-response matching |
| SIP | Dialog state | Call setup/teardown |
| MPTCP | Subflow mapping, DSN tracking | Multi-path coordination |

---

## 1.4 Why TLS / HTTP/2 / SMB Require FSMs

### TLS Example FSM

```
                    +-------------+
                    |   UNKNOWN   |
                    +------+------+
                           |
                    ClientHello
                           v
                    +-------------+
                    |    HELLO    |
                    +------+------+
                           |
                    ServerHello
                           v
                    +-------------+
                    | KEY_EXCHG   |
                    +------+------+
                           |
                    Finished
                           v
                    +-------------+
                    | APPLICATION |
                    +------+------+
```

### HTTP/2 Stream State Machine

```
                              +--------+
                      HEADERS |        | PUSH_PROMISE
                         ,----+  idle  +----.
                        /     |        |     \
                       v      +--------+      v
                +----------+              +----------+
                |          |              |          |
        ,-------|  open    |--------------|reserved  |-------.
       /        |          |   HEADERS    | (local)  |        \
      v         +----------+              +----------+         v
+----------+         |                         |         +----------+
|   half   |         | RST_STREAM             | RST     |   half   |
|  closed  |         v                        v STREAM  |  closed  |
| (remote) |    +----------+            +----------+    |  (local) |
+----------+    |          |            |          |    +----------+
      |         |  closed  |<-----------|  closed  |         |
      |         |          |            |          |         |
      |         +----------+            +----------+         |
      |                                                      |
      `--------------------->  closed  <--------------------'
```

### SMB2 Session State

```
+-------------------+
| NO_SESSION        |
+--------+----------+
         |
    SESSION_SETUP (req)
         v
+-------------------+
| SESSION_PENDING   |
+--------+----------+
         |
    SESSION_SETUP (resp: SUCCESS)
         v
+-------------------+
| SESSION_VALID     |
+--------+----------+
         |
    LOGOFF / DISCONNECT
         v
+-------------------+
| SESSION_EXPIRED   |
+-------------------+
```

---

## 中文解释

### 为什么协议解析需要状态机

**1. 无状态解析的局限性**
- 每个数据包被独立处理，没有上下文信息
- 适用于简单协议（如ARP、ICMP）
- 无法处理跨多个数据包的事务

**2. 有状态协议的特点**
- TLS握手需要追踪：当前阶段、协商的密码套件、交换的密钥
- HTTP/2需要追踪：每个流的状态、HPACK动态表
- SMB需要追踪：会话ID、树连接、文件句柄

**3. 状态机的核心功能**
- 记住之前发生了什么
- 根据当前状态决定如何解析数据
- 验证协议消息的正确顺序
- 在状态之间进行转换

**4. Wireshark中的实现**
- 使用"conversation"（会话）机制存储状态
- 每个协议定义自己的状态结构体
- 状态在多个数据包之间持久化
- 支持跨包重组和解密
