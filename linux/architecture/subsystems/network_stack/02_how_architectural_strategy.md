# HOW｜架构策略

## 1. 包流如何建模

```
PACKET FLOW MODEL
+=============================================================================+
|                                                                              |
|  THE PIPELINE METAPHOR                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux network stack as an ASSEMBLY LINE:                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Each packet flows through stages, each stage does ONE thing:    │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │                    RECEIVE PATH (RX)                        ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  Wire                                                       ││    │ |
|  │  │  │    │                                                        ││    │ |
|  │  │  │    ▼                                                        ││    │ |
|  │  │  │  ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐  ││    │ |
|  │  │  │  │ NIC  │───►│ GRO  │───►│  IP  │───►│ TCP  │───►│Socket│  ││    │ |
|  │  │  │  │Driver│    │      │    │Input │    │Input │    │Queue │  ││    │ |
|  │  │  │  └──────┘    └──────┘    └──────┘    └──────┘    └──────┘  ││    │ |
|  │  │  │      │           │           │           │           │      ││    │ |
|  │  │  │      │           │           │           │           ▼      ││    │ |
|  │  │  │  alloc      aggregate    route       process     wake up    ││    │ |
|  │  │  │  sk_buff    packets      lookup      headers     recv()     ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │                    TRANSMIT PATH (TX)                       ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  User                                                       ││    │ |
|  │  │  │    │                                                        ││    │ |
|  │  │  │    ▼                                                        ││    │ |
|  │  │  │  ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐    ┌──────┐  ││    │ |
|  │  │  │  │Socket│───►│ TCP  │───►│  IP  │───►│Qdisc │───►│ NIC  │  ││    │ |
|  │  │  │  │send()│    │Output│    │Output│    │Queue │    │Driver│  ││    │ |
|  │  │  │  └──────┘    └──────┘    └──────┘    └──────┘    └──────┘  ││    │ |
|  │  │  │      │           │           │           │           │      ││    │ |
|  │  │  │      ▼           ▼           ▼           ▼           ▼      ││    │ |
|  │  │  │   copy       segment      add IP      schedule    DMA to    ││    │ |
|  │  │  │   data       + headers    header      + shape     hardware  ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  KEY PROPERTIES:                                                         │ |
|  │  • Each stage has ONE responsibility                                     │ |
|  │  • sk_buff carries packet through all stages                             │ |
|  │  • Stages connected by function calls (not queues, except at ends)       │ |
|  │  • Hooks allow injection at each stage (netfilter)                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**包流模型**：Linux 网络栈作为流水线

**接收路径（RX）**：
Wire → NIC Driver（分配 sk_buff）→ GRO（聚合包）→ IP Input（路由查找）→ TCP Input（处理头部）→ Socket Queue（唤醒 recv()）

**发送路径（TX）**：
User send() → Socket（复制数据）→ TCP Output（分段 + 头部）→ IP Output（添加 IP 头）→ Qdisc Queue（调度 + 整形）→ NIC Driver（DMA 到硬件）

关键属性：
- 每个阶段有单一职责
- sk_buff 携带包通过所有阶段
- 阶段通过函数调用连接（除了端点外无队列）
- 钩子允许在每个阶段注入（netfilter）

---

```
PACKET FLOW: DETAILED RX PATH
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  INCOMING PACKET: Full path from wire to application                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  [Wire]                                                          │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 1. NIC DMA: Hardware writes frame to ring buffer         │   │    │ |
|  │  │  │    • Pre-allocated sk_buff or XDP frame                  │   │    │ |
|  │  │  │    • NIC raises interrupt                                │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 2. IRQ Handler: napi_schedule()                          │   │    │ |
|  │  │  │    • Disable IRQ for this queue                          │   │    │ |
|  │  │  │    • Schedule NAPI poll                                  │   │    │ |
|  │  │  │    • Return immediately                                  │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 3. NAPI Poll (softirq context)                           │   │    │ |
|  │  │  │    • Process up to budget packets (default 64)           │   │    │ |
|  │  │  │    • For each: driver->napi_poll() → napi_gro_receive()  │   │    │ |
|  │  │  │    • If < budget, re-enable IRQ                          │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 4. GRO (Generic Receive Offload)                         │   │    │ |
|  │  │  │    • Try to merge with existing flow                     │   │    │ |
|  │  │  │    • napi_gro_receive() → gro_receive_cb()               │   │    │ |
|  │  │  │    • Eventually: napi_gro_complete() → netif_receive_skb()│   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 5. netif_receive_skb() - Core RX function                │   │    │ |
|  │  │  │    • RPS: Redirect to another CPU if configured          │   │    │ |
|  │  │  │    • Packet type dispatch: ip_rcv, arp_rcv, etc.         │   │    │ |
|  │  │  │    • Calls registered packet handlers                    │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 6. ip_rcv() → ip_rcv_finish()                            │   │    │ |
|  │  │  │    • Validate IP header                                  │   │    │ |
|  │  │  │    • Netfilter PREROUTING hook                           │   │    │ |
|  │  │  │    • Route lookup: ip_route_input()                      │   │    │ |
|  │  │  │    • Decide: local delivery or forward                   │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼ (if local)                                                 │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 7. ip_local_deliver() → ip_local_deliver_finish()        │   │    │ |
|  │  │  │    • Netfilter INPUT hook                                │   │    │ |
|  │  │  │    • Protocol dispatch: tcp_v4_rcv, udp_rcv, etc.        │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 8. tcp_v4_rcv()                                          │   │    │ |
|  │  │  │    • Lookup socket: __inet_lookup_skb()                  │   │    │ |
|  │  │  │    • Validate TCP header                                 │   │    │ |
|  │  │  │    • tcp_v4_do_rcv() → tcp_rcv_established()             │   │    │ |
|  │  │  │    • Add to socket receive queue                         │   │    │ |
|  │  │  │    • Wake up blocked reader                              │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │     │                                                            │    │ |
|  │  │     ▼                                                            │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ 9. User recv() returns                                   │   │    │ |
|  │  │  │    • Data copied from socket buffer to user buffer       │   │    │ |
|  │  │  │    • sk_buff freed (or kept for zero-copy)               │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**详细接收路径**：

1. **NIC DMA**：硬件写帧到环形缓冲区，触发中断
2. **IRQ Handler**：`napi_schedule()` 禁用 IRQ，调度 NAPI poll
3. **NAPI Poll**：处理最多 budget 个包（默认 64），调用 `napi_gro_receive()`
4. **GRO**：尝试与现有流合并，最终调用 `netif_receive_skb()`
5. **netif_receive_skb()**：核心 RX 函数，RPS 重定向，包类型分发
6. **ip_rcv()**：验证 IP 头，Netfilter PREROUTING 钩子，路由查找
7. **ip_local_deliver()**：Netfilter INPUT 钩子，协议分发
8. **tcp_v4_rcv()**：socket 查找，验证 TCP 头，添加到接收队列，唤醒读者
9. **User recv()**：数据从 socket 缓冲区复制到用户缓冲区

---

## 2. sk_buff 的角色

```
ROLE OF SK_BUFF
+=============================================================================+
|                                                                              |
|  SK_BUFF: THE PACKET CARRIER                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct sk_buff is NOT just a buffer, it's:                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. METADATA CONTAINER                                           │    │ |
|  │  │     • Source/destination device                                  │    │ |
|  │  │     • Protocol information                                       │    │ |
|  │  │     • Timestamps                                                 │    │ |
|  │  │     • Checksum status                                            │    │ |
|  │  │     • Queue mapping                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. DATA POINTER MANAGEMENT                                      │    │ |
|  │  │     • head, data, tail, end pointers                             │    │ |
|  │  │     • Efficient header push/pull                                 │    │ |
|  │  │     • No data copy when adding headers                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. SHARED DATA SUPPORT                                          │    │ |
|  │  │     • Reference counting for data                                │    │ |
|  │  │     • Clone without copy (skb_clone)                             │    │ |
|  │  │     • Copy-on-write semantics                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. FRAGMENTATION SUPPORT                                        │    │ |
|  │  │     • Linear data + paged fragments                              │    │ |
|  │  │     • Scatter-gather for zero-copy                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SK_BUFF MEMORY LAYOUT                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct sk_buff (control structure, ~200 bytes):                 │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ struct sk_buff *next, *prev;   // Queue linkage          │   │    │ |
|  │  │  │ struct sock *sk;                // Owner socket          │   │    │ |
|  │  │  │ struct net_device *dev;         // Device                │   │    │ |
|  │  │  │ unsigned char *head;            // Start of buffer       │   │    │ |
|  │  │  │ unsigned char *data;            // Start of data         │   │    │ |
|  │  │  │ unsigned char *tail;            // End of data           │   │    │ |
|  │  │  │ unsigned char *end;             // End of buffer         │   │    │ |
|  │  │  │ unsigned int len;               // Data length           │   │    │ |
|  │  │  │ __u16 protocol;                 // L3 protocol           │   │    │ |
|  │  │  │ __u8 pkt_type;                  // Packet type           │   │    │ |
|  │  │  │ ... transport_header, network_header, mac_header ...     │   │    │ |
|  │  │  │ struct sk_buff_data_ref *shinfo; // Shared info          │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  Data buffer:                                                    │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  head                                              end    │   │    │ |
|  │  │  │    │                                                 │    │   │    │ |
|  │  │  │    ▼                                                 ▼    │   │    │ |
|  │  │  │  ┌─────┬───────────────────────────────────────┬─────┐   │   │    │ |
|  │  │  │  │head │          packet data                   │tail │   │   │    │ |
|  │  │  │  │room │  ◄─────────── len ──────────────►     │room │   │   │    │ |
|  │  │  │  └─────┴───────────────────────────────────────┴─────┘   │   │    │ |
|  │  │  │        ▲                                       ▲         │   │    │ |
|  │  │  │        │                                       │         │   │    │ |
|  │  │  │      data                                    tail        │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  headroom: space to prepend headers (Ethernet, IP, TCP)   │   │    │ |
|  │  │  │  tailroom: space to append data or padding                │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SK_BUFF OPERATIONS                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  skb_push(skb, len)   - Prepend header (decrease data ptr)       │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  Before: [headroom][  data  ][tailroom]                     ││    │ |
|  │  │  │                    ▲                                        ││    │ |
|  │  │  │                  data                                       ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  After:  [head][HDR][  data  ][tailroom]                    ││    │ |
|  │  │  │               ▲                                             ││    │ |
|  │  │  │             data (moved back by len)                        ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  │  skb_pull(skb, len)   - Strip header (increase data ptr)         │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  Before: [head][ETH][IP][TCP][payload][tail]                ││    │ |
|  │  │  │               ▲                                             ││    │ |
|  │  │  │             data                                            ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  After:  [head][ETH][IP][TCP][payload][tail]                ││    │ |
|  │  │  │                   ▲                                         ││    │ |
|  │  │  │                 data (moved forward, ETH "stripped")        ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  │  skb_clone(skb)       - Create reference (no data copy)          │    │ |
|  │  │  skb_copy(skb)        - Full copy (including data)               │    │ |
|  │  │  pskb_copy(skb)       - Partial copy (headers only)              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**sk_buff 的角色**：

sk_buff 不仅仅是缓冲区，它是：
1. **元数据容器**：源/目标设备、协议信息、时间戳、校验和状态、队列映射
2. **数据指针管理**：head/data/tail/end 指针，高效头部 push/pull，添加头部无需复制
3. **共享数据支持**：数据引用计数，无复制克隆（skb_clone），写时复制语义
4. **分片支持**：线性数据 + 分页片段，scatter-gather 零拷贝

**内存布局**：
- headroom：预留空间用于前置头部（Ethernet、IP、TCP）
- data → tail：实际数据区域
- tailroom：用于追加数据或填充

**操作**：
- `skb_push()`：前置头部（减少 data 指针）
- `skb_pull()`：剥离头部（增加 data 指针）
- `skb_clone()`：创建引用（无数据复制）

---

## 3. 协议隔离策略

```
PROTOCOL ISOLATION STRATEGY
+=============================================================================+
|                                                                              |
|  HOW LINUX ISOLATES PROTOCOL IMPLEMENTATIONS                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  LAYER 1: PROTOCOL FAMILY (PF_INET, PF_INET6, etc.)                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  socket(AF_INET, SOCK_STREAM, 0)                                 │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ net_families[] - registered protocol families              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ [PF_UNIX]  → unix_family_ops    (AF_UNIX sockets)           │ │    │ |
|  │  │  │ [PF_INET]  → inet_family_ops    (IPv4)                      │ │    │ |
|  │  │  │ [PF_INET6] → inet6_family_ops   (IPv6)                      │ │    │ |
|  │  │  │ [PF_PACKET]→ packet_family_ops  (raw packets)               │ │    │ |
|  │  │  │ [PF_NETLINK]→netlink_family_ops (kernel messaging)          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LAYER 2: PROTOCOL TYPE (TCP, UDP, SCTP, etc.)                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  inet_family_ops.create() looks up:                              │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ inetsw[] - registered inet protocols                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ [SOCK_STREAM, IPPROTO_TCP] → tcp_prot                       │ │    │ |
|  │  │  │ [SOCK_DGRAM,  IPPROTO_UDP] → udp_prot                       │ │    │ |
|  │  │  │ [SOCK_RAW,    IPPROTO_RAW] → raw_prot                       │ │    │ |
|  │  │  │ [SOCK_DGRAM,  IPPROTO_ICMP]→ ping_prot                      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  LAYER 3: PROTOCOL OPERATIONS                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct proto (protocol-specific operations):                    │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  struct proto tcp_prot = {                                  │ │    │ |
|  │  │  │      .name           = "TCP",                               │ │    │ |
|  │  │  │      .connect        = tcp_v4_connect,                      │ │    │ |
|  │  │  │      .disconnect     = tcp_disconnect,                      │ │    │ |
|  │  │  │      .accept         = inet_csk_accept,                     │ │    │ |
|  │  │  │      .close          = tcp_close,                           │ │    │ |
|  │  │  │      .sendmsg        = tcp_sendmsg,                         │ │    │ |
|  │  │  │      .recvmsg        = tcp_recvmsg,                         │ │    │ |
|  │  │  │      .backlog_rcv    = tcp_v4_do_rcv,                       │ │    │ |
|  │  │  │      .hash           = inet_hash,                           │ │    │ |
|  │  │  │      .unhash         = inet_unhash,                         │ │    │ |
|  │  │  │      .get_port       = inet_csk_get_port,                   │ │    │ |
|  │  │  │      .setsockopt     = tcp_setsockopt,                      │ │    │ |
|  │  │  │      .getsockopt     = tcp_getsockopt,                      │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  struct proto udp_prot = {                                  │ │    │ |
|  │  │  │      .name           = "UDP",                               │ │    │ |
|  │  │  │      .connect        = ip4_datagram_connect,                │ │    │ |
|  │  │  │      .sendmsg        = udp_sendmsg,                         │ │    │ |
|  │  │  │      .recvmsg        = udp_recvmsg,                         │ │    │ |
|  │  │  │      ...                                                    │ │    │ |
|  │  │  │  };                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  RESULT: PLUGGABLE PROTOCOLS                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  • TCP and UDP share IP layer, have different proto ops                 │ |
|  │  • IPv4 and IPv6 share transport layer, have different family ops       │ |
|  │  • New protocol (SCTP) can plug in without changing existing code       │ |
|  │  • Loadable as kernel modules                                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**协议隔离策略**：

**层次 1：协议族**（PF_INET、PF_INET6 等）
- `net_families[]` 注册协议族
- PF_UNIX → unix_family_ops
- PF_INET → inet_family_ops
- PF_INET6 → inet6_family_ops

**层次 2：协议类型**（TCP、UDP、SCTP 等）
- `inetsw[]` 注册 inet 协议
- SOCK_STREAM + IPPROTO_TCP → tcp_prot
- SOCK_DGRAM + IPPROTO_UDP → udp_prot

**层次 3：协议操作**
- `struct proto` 包含协议特定操作：connect、sendmsg、recvmsg...
- tcp_prot 和 udp_prot 有各自的实现

**结果**：可插拔协议
- TCP 和 UDP 共享 IP 层，有不同的 proto ops
- 新协议（SCTP）可以插入而不改变现有代码
- 可作为内核模块加载

---

## 4. 包的生命周期

```
LIFECYCLE OF A PACKET
+=============================================================================+
|                                                                              |
|  PACKET LIFECYCLE: FROM ALLOCATION TO FREE                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │   RECEIVE PATH LIFECYCLE                                         │    │ |
|  │  │   ───────────────────────                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │ ALLOCATE│ ◄── Driver allocates from slab cache               │    │ |
|  │  │   │  sk_buff │    netdev_alloc_skb() or napi_alloc_skb()         │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │  FILL   │ ◄── NIC DMA fills buffer with frame data           │    │ |
|  │  │   │         │                                                    │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │ PROCESS │ ◄── Pass through protocol layers                   │    │ |
|  │  │   │         │    GRO → netif_receive_skb → ip_rcv → tcp_rcv      │    │ |
|  │  │   │         │    skb->data pointer adjusted at each layer        │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │  QUEUE  │ ◄── Add to socket receive queue                    │    │ |
|  │  │   │         │    skb_queue_tail(&sk->sk_receive_queue, skb)      │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │ CONSUME │ ◄── User recv() copies data                        │    │ |
|  │  │   │         │    skb_copy_datagram_msg()                         │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │  FREE   │ ◄── Return to slab cache                           │    │ |
|  │  │   │         │    kfree_skb() or consume_skb()                    │    │ |
|  │  │   └─────────┘                                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │   TRANSMIT PATH LIFECYCLE                                        │    │ |
|  │  │   ────────────────────────                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │ ALLOCATE│ ◄── Socket layer allocates                         │    │ |
|  │  │   │  sk_buff │    sock_alloc_send_skb()                          │    │ |
|  │  │   └────┬────┘    (with memory accounting)                        │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │  COPY   │ ◄── User data copied to skb                        │    │ |
|  │  │   │  DATA   │    skb_copy_datagram_from_iter()                   │    │ |
|  │  │   └────┬────┘    or zerocopy via MSG_ZEROCOPY                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │  BUILD  │ ◄── Add protocol headers                           │    │ |
|  │  │   │ HEADERS │    TCP: tcp_transmit_skb() adds TCP header         │    │ |
|  │  │   │         │    IP: ip_queue_xmit() adds IP header              │    │ |
|  │  │   │         │    Uses skb_push() - no data copy!                 │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │  QUEUE  │ ◄── Enter qdisc layer                              │    │ |
|  │  │   │         │    dev_queue_xmit() → qdisc enqueue                │    │ |
|  │  │   │         │    Traffic shaping, scheduling                     │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │ TRANSMIT│ ◄── Driver sends to NIC                            │    │ |
|  │  │   │         │    ndo_start_xmit() → DMA to hardware              │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │ COMPLETE│ ◄── NIC completion interrupt                       │    │ |
|  │  │   │         │    Free skb after DMA complete                     │    │ |
|  │  │   └────┬────┘                                                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │   ┌─────────┐                                                    │    │ |
|  │  │   │  FREE   │ ◄── Return to slab cache                           │    │ |
|  │  │   │         │    consume_skb() or dev_kfree_skb()                │    │ |
|  │  │   └─────────┘                                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  REFERENCE COUNTING                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  skb->users: reference count                                            │ |
|  │                                                                          │ |
|  │  skb_get(skb)     - increment refcount                                  │ |
|  │  kfree_skb(skb)   - decrement, free if zero (error path)                │ |
|  │  consume_skb(skb) - decrement, free if zero (normal path)               │ |
|  │                                                                          │ |
|  │  skb_clone(skb)   - share data, separate sk_buff structs                │ |
|  │                     both point to same data, separate refcounts         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**包的生命周期**：

**接收路径生命周期**：
1. **ALLOCATE**：驱动从 slab 缓存分配（`netdev_alloc_skb()`）
2. **FILL**：NIC DMA 填充帧数据
3. **PROCESS**：通过协议层（GRO → netif_receive_skb → ip_rcv → tcp_rcv）
4. **QUEUE**：添加到 socket 接收队列
5. **CONSUME**：用户 recv() 复制数据
6. **FREE**：返回 slab 缓存（`kfree_skb()`）

**发送路径生命周期**：
1. **ALLOCATE**：socket 层分配（`sock_alloc_send_skb()`）
2. **COPY DATA**：用户数据复制到 skb
3. **BUILD HEADERS**：添加协议头（TCP、IP），使用 `skb_push()` 无数据复制
4. **QUEUE**：进入 qdisc 层，流量整形
5. **TRANSMIT**：驱动发送到 NIC
6. **COMPLETE**：NIC 完成中断
7. **FREE**：返回 slab 缓存

**引用计数**：
- `skb_get()`：增加引用
- `kfree_skb()`：减少，零时释放（错误路径）
- `consume_skb()`：减少，零时释放（正常路径）
- `skb_clone()`：共享数据，分离 sk_buff 结构
