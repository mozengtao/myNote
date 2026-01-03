# WHERE｜源代码地图

## 1. net/ 目录结构

```
NET/ DIRECTORY STRUCTURE
+=============================================================================+
|                                                                              |
|  net/                                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  CORE NETWORKING (核心网络)                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  net/                                                            │    │ |
|  │  │  ├── core/             ◄── Core networking infrastructure        │    │ |
|  │  │  │   ├── dev.c         ◄── netif_receive_skb, dev_queue_xmit     │    │ |
|  │  │  │   ├── skbuff.c      ◄── sk_buff allocation and manipulation   │    │ |
|  │  │  │   ├── sock.c        ◄── Generic socket layer                  │    │ |
|  │  │  │   ├── filter.c      ◄── Socket filters, BPF                   │    │ |
|  │  │  │   ├── flow_dissector.c ◄── Packet parsing for RSS/RPS         │    │ |
|  │  │  │   ├── neighbour.c   ◄── ARP/ND neighbor cache                 │    │ |
|  │  │  │   ├── rtnetlink.c   ◄── Netlink for routing                   │    │ |
|  │  │  │   └── net_namespace.c ◄── Network namespace                   │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── socket.c          ◄── Socket syscall entry points           │    │ |
|  │  │  └── sysctl_net_core.c ◄── /proc/sys/net/core tunables           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  IPv4 STACK (IPv4 协议栈)                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  net/ipv4/                                                       │    │ |
|  │  │  ├── af_inet.c         ◄── AF_INET socket family                 │    │ |
|  │  │  ├── ip_input.c        ◄── ip_rcv, IP receive path               │    │ |
|  │  │  ├── ip_output.c       ◄── ip_output, IP transmit path           │    │ |
|  │  │  ├── ip_forward.c      ◄── IP forwarding                         │    │ |
|  │  │  ├── ip_fragment.c     ◄── IP fragmentation/reassembly           │    │ |
|  │  │  ├── route.c           ◄── Routing table and lookup              │    │ |
|  │  │  ├── fib_*.c           ◄── Forwarding Information Base           │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── tcp.c             ◄── TCP socket interface                  │    │ |
|  │  │  ├── tcp_input.c       ◄── TCP receive processing                │    │ |
|  │  │  ├── tcp_output.c      ◄── TCP transmit processing               │    │ |
|  │  │  ├── tcp_ipv4.c        ◄── TCP over IPv4 specifics               │    │ |
|  │  │  ├── tcp_timer.c       ◄── TCP timers (retransmit, etc.)         │    │ |
|  │  │  ├── tcp_cong.c        ◄── Congestion control framework          │    │ |
|  │  │  ├── tcp_cubic.c       ◄── CUBIC congestion control              │    │ |
|  │  │  │                                                               │    │ |
|  │  │  ├── udp.c             ◄── UDP implementation                    │    │ |
|  │  │  ├── raw.c             ◄── Raw sockets                           │    │ |
|  │  │  ├── icmp.c            ◄── ICMP handling                         │    │ |
|  │  │  ├── arp.c             ◄── ARP protocol                          │    │ |
|  │  │  └── ping.c            ◄── ICMP ping sockets                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  IPv6 STACK (IPv6 协议栈)                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  net/ipv6/                                                       │    │ |
|  │  │  ├── af_inet6.c        ◄── AF_INET6 socket family                │    │ |
|  │  │  ├── ip6_input.c       ◄── IPv6 receive                          │    │ |
|  │  │  ├── ip6_output.c      ◄── IPv6 transmit                         │    │ |
|  │  │  ├── tcp_ipv6.c        ◄── TCP over IPv6                         │    │ |
|  │  │  ├── udp.c             ◄── UDP over IPv6                         │    │ |
|  │  │  ├── route.c           ◄── IPv6 routing                          │    │ |
|  │  │  └── ndisc.c           ◄── Neighbor Discovery                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  NETFILTER (包过滤框架)                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  net/netfilter/                                                  │    │ |
|  │  │  ├── core.c            ◄── Hook registration, NF_HOOK            │    │ |
|  │  │  ├── nf_conntrack_*.c  ◄── Connection tracking                   │    │ |
|  │  │  ├── nf_nat_*.c        ◄── Network Address Translation           │    │ |
|  │  │  ├── nf_tables_*.c     ◄── nftables implementation               │    │ |
|  │  │  └── x_tables.c        ◄── iptables core                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  net/ipv4/netfilter/                                             │    │ |
|  │  │  ├── ip_tables.c       ◄── iptables for IPv4                     │    │ |
|  │  │  ├── nf_nat_*.c        ◄── IPv4 NAT                              │    │ |
|  │  │  └── ipt_*.c           ◄── iptables modules                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  OTHER PROTOCOLS (其他协议)                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  net/unix/             ◄── Unix domain sockets                   │    │ |
|  │  │  net/packet/           ◄── AF_PACKET (raw access)                │    │ |
|  │  │  net/netlink/          ◄── Netlink protocol                      │    │ |
|  │  │  net/xdp/              ◄── XDP (eXpress Data Path)               │    │ |
|  │  │  net/sched/            ◄── Traffic control (tc), qdiscs          │    │ |
|  │  │  net/bridge/           ◄── Ethernet bridging                     │    │ |
|  │  │  net/ethernet/         ◄── Ethernet protocol handling            │    │ |
|  │  │  net/sctp/             ◄── SCTP protocol                         │    │ |
|  │  │  net/dccp/             ◄── DCCP protocol                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DRIVERS (驱动程序)                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  drivers/net/                                                            │ |
|  │  ├── ethernet/            ◄── Ethernet NIC drivers                      │ |
|  │  │   ├── intel/           ◄── Intel NICs (e1000, igb, ixgbe, i40e)      │ |
|  │  │   ├── mellanox/        ◄── Mellanox NICs (mlx4, mlx5)                │ |
|  │  │   ├── broadcom/        ◄── Broadcom NICs                             │ |
|  │  │   └── ...                                                             │ |
|  │  ├── wireless/            ◄── Wireless NIC drivers                      │ |
|  │  ├── virtio_net.c         ◄── Virtio network driver                     │ |
|  │  ├── loopback.c           ◄── Loopback device                           │ |
|  │  └── veth.c               ◄── Virtual ethernet pairs                    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**net/ 目录结构**：

**核心网络**（net/core/）：
- `dev.c`：netif_receive_skb、dev_queue_xmit（核心 RX/TX）
- `skbuff.c`：sk_buff 分配和操作
- `sock.c`：通用 socket 层
- `neighbour.c`：ARP/ND 邻居缓存

**IPv4 协议栈**（net/ipv4/）：
- `ip_input.c`：ip_rcv，IP 接收路径
- `ip_output.c`：ip_output，IP 发送路径
- `tcp.c/tcp_input.c/tcp_output.c`：TCP 实现
- `udp.c`：UDP 实现
- `route.c/fib_*.c`：路由表

**IPv6 协议栈**（net/ipv6/）：类似 IPv4

**Netfilter**（net/netfilter/）：
- `core.c`：钩子注册，NF_HOOK
- `nf_conntrack_*.c`：连接跟踪
- `nf_nat_*.c`：NAT

**驱动程序**（drivers/net/）：
- `ethernet/intel/`：Intel 网卡（e1000、igb、ixgbe）
- `ethernet/mellanox/`：Mellanox 网卡

---

## 2. 架构锚点：struct sk_buff

```
ARCHITECTURAL ANCHOR: STRUCT SK_BUFF
+=============================================================================+
|                                                                              |
|  WHERE TO FIND SK_BUFF                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Definition: include/linux/skbuff.h                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Line ~500-800 (varies by version):                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct sk_buff {                                                │    │ |
|  │  │      /* These two members must be first. */                      │    │ |
|  │  │      struct sk_buff *next;                                       │    │ |
|  │  │      struct sk_buff *prev;                                       │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Implementation: net/core/skbuff.c                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Key functions:                                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  __alloc_skb()           - Core allocation                       │    │ |
|  │  │  __netdev_alloc_skb()    - Driver allocation (with headroom)     │    │ |
|  │  │  kfree_skb()             - Free (error path)                     │    │ |
|  │  │  consume_skb()           - Free (normal path)                    │    │ |
|  │  │  skb_clone()             - Clone (share data)                    │    │ |
|  │  │  skb_copy()              - Full copy                             │    │ |
|  │  │  pskb_copy()             - Partial copy (headers only)           │    │ |
|  │  │  skb_push()              - Prepend header                        │    │ |
|  │  │  skb_pull()              - Strip header                          │    │ |
|  │  │  skb_put()               - Extend tail                           │    │ |
|  │  │  skb_trim()              - Reduce length                         │    │ |
|  │  │  skb_queue_tail()        - Add to queue tail                     │    │ |
|  │  │  skb_dequeue()           - Remove from queue head                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SK_BUFF USAGE PATTERN                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  RX: Driver allocates → stack processes → socket queues → user reads    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Driver:                                                         │    │ |
|  │  │    skb = netdev_alloc_skb(dev, len + NET_IP_ALIGN);              │    │ |
|  │  │    skb_reserve(skb, NET_IP_ALIGN);  // Align IP header           │    │ |
|  │  │    skb_put(skb, len);               // Mark data length          │    │ |
|  │  │    // DMA fills skb->data                                        │    │ |
|  │  │    napi_gro_receive(&napi, skb);                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  IP layer:                                                       │    │ |
|  │  │    iph = ip_hdr(skb);               // Get IP header             │    │ |
|  │  │    skb_pull(skb, iph->ihl * 4);     // Strip IP header           │    │ |
|  │  │    skb_reset_transport_header(skb); // Mark transport start      │    │ |
|  │  │                                                                  │    │ |
|  │  │  TCP layer:                                                      │    │ |
|  │  │    th = tcp_hdr(skb);               // Get TCP header            │    │ |
|  │  │    skb_pull(skb, th->doff * 4);     // Strip TCP header          │    │ |
|  │  │    // Now skb->data points to payload                            │    │ |
|  │  │    skb_queue_tail(&sk->sk_receive_queue, skb);                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TX: User writes → socket layer → stack → driver sends                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Socket layer:                                                   │    │ |
|  │  │    skb = sock_alloc_send_skb(sk, size, ...);                     │    │ |
|  │  │    skb_reserve(skb, MAX_TCP_HEADER); // Room for headers         │    │ |
|  │  │    // Copy user data                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  TCP layer:                                                      │    │ |
|  │  │    th = skb_push(skb, sizeof(*th)); // Add TCP header            │    │ |
|  │  │    // Fill TCP header fields                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  IP layer:                                                       │    │ |
|  │  │    iph = skb_push(skb, sizeof(*iph)); // Add IP header           │    │ |
|  │  │    // Fill IP header fields                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Driver:                                                         │    │ |
|  │  │    // DMA map skb->data                                          │    │ |
|  │  │    // Send to hardware                                           │    │ |
|  │  │    // Free on completion                                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct sk_buff 位置**：
- 定义：`include/linux/skbuff.h`
- 实现：`net/core/skbuff.c`

**关键函数**：
- `__alloc_skb()`：核心分配
- `kfree_skb()/consume_skb()`：释放
- `skb_clone()/skb_copy()`：克隆/复制
- `skb_push()/skb_pull()`：前置/剥离头部
- `skb_queue_tail()/skb_dequeue()`：队列操作

**使用模式**：
- RX：驱动分配 → 协议栈处理 → socket 队列 → 用户读取
- TX：用户写入 → socket 层 → 协议栈 → 驱动发送

---

## 3. 控制中心：netif_receive_skb()

```
CONTROL HUB: NETIF_RECEIVE_SKB()
+=============================================================================+
|                                                                              |
|  LOCATION: net/core/dev.c                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  int netif_receive_skb(struct sk_buff *skb)                              │ |
|  │  {                                                                       │ |
|  │      /* Entry point for all received packets */                          │ |
|  │      return netif_receive_skb_internal(skb);                             │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  static int netif_receive_skb_internal(struct sk_buff *skb)              │ |
|  │  {                                                                       │ |
|  │      /* RPS: Redirect to another CPU if configured */                    │ |
|  │      if (static_key_false(&rps_needed)) {                                │ |
|  │          // May enqueue to other CPU's backlog                           │ |
|  │      }                                                                   │ |
|  │      return __netif_receive_skb(skb);                                    │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  static int __netif_receive_skb_core(struct sk_buff *skb, bool pfmemalloc)│ |
|  │  {                                                                       │ |
|  │      struct packet_type *ptype;                                          │ |
|  │      __be16 type;                                                        │ |
|  │                                                                          │ |
|  │      /* 1. Generic XDP */                                                │ |
|  │      if (static_key_false(&generic_xdp_needed))                          │ |
|  │          do_xdp_generic(skb);                                            │ |
|  │                                                                          │ |
|  │      /* 2. Packet taps (tcpdump, etc.) */                                │ |
|  │      list_for_each_entry_rcu(ptype, &ptype_all, list) {                  │ |
|  │          deliver_skb(skb, ptype, ...);                                   │ |
|  │      }                                                                   │ |
|  │                                                                          │ |
|  │      /* 3. Bridge check */                                               │ |
|  │      if (skb->dev->rx_handler)                                           │ |
|  │          return rx_handler(skb);                                         │ |
|  │                                                                          │ |
|  │      /* 4. Protocol dispatch based on skb->protocol */                   │ |
|  │      type = skb->protocol;                                               │ |
|  │      list_for_each_entry_rcu(ptype, &ptype_base[type & 15], list) {      │ |
|  │          if (ptype->type == type)                                        │ |
|  │              return ptype->func(skb, ...);  // ip_rcv, arp_rcv, etc.     │ |
|  │      }                                                                   │ |
|  │                                                                          │ |
|  │      return NET_RX_DROP;                                                 │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER CONTROL HUBS                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  net/core/dev.c:                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  netif_receive_skb()     - RX entry point                        │    │ |
|  │  │  dev_queue_xmit()        - TX entry point                        │    │ |
|  │  │  napi_gro_receive()      - GRO entry point                       │    │ |
|  │  │  register_netdev()       - Register network device               │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  net/ipv4/ip_input.c:                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  ip_rcv()                - IP receive entry                      │    │ |
|  │  │  ip_rcv_finish()         - After prerouting hook                 │    │ |
|  │  │  ip_local_deliver()      - For local delivery                    │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  net/ipv4/ip_output.c:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  ip_output()             - IP output                             │    │ |
|  │  │  ip_queue_xmit()         - TCP uses this                         │    │ |
|  │  │  ip_finish_output()      - Final output processing               │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  net/ipv4/tcp_ipv4.c:                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  tcp_v4_rcv()            - TCP receive for IPv4                  │    │ |
|  │  │  tcp_v4_connect()        - TCP connect                           │    │ |
|  │  │  tcp_v4_do_rcv()         - Core TCP receive                      │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  net/ipv4/tcp_input.c:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  tcp_rcv_established()   - Fast path for established TCP         │    │ |
|  │  │  tcp_rcv_state_process() - TCP state machine                     │    │ |
|  │  │  tcp_data_queue()        - Queue data for user                   │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  net/ipv4/tcp_output.c:                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  tcp_transmit_skb()      - Send TCP segment                      │    │ |
|  │  │  tcp_write_xmit()        - Main TX function                      │    │ |
|  │  │  __tcp_push_pending_frames() - Push pending data                 │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制中心：netif_receive_skb()**（net/core/dev.c）

处理流程：
1. RPS：如果配置，可能重定向到其他 CPU
2. Generic XDP
3. 包 tap（tcpdump 等）
4. 桥接检查
5. 协议分发：根据 `skb->protocol` 调用 `ip_rcv`、`arp_rcv` 等

**其他控制中心**：
- `net/core/dev.c`：`dev_queue_xmit()`（TX 入口）、`napi_gro_receive()`
- `net/ipv4/ip_input.c`：`ip_rcv()`、`ip_local_deliver()`
- `net/ipv4/ip_output.c`：`ip_output()`、`ip_queue_xmit()`
- `net/ipv4/tcp_ipv4.c`：`tcp_v4_rcv()`
- `net/ipv4/tcp_input.c`：`tcp_rcv_established()`
- `net/ipv4/tcp_output.c`：`tcp_transmit_skb()`

---

## 4. 路径追踪策略

```
PATH TRACING STRATEGY
+=============================================================================+
|                                                                              |
|  METHOD 1: FTRACE                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Trace RX path                                                         │ |
|  │  echo 'netif_receive_skb' > /sys/kernel/debug/tracing/set_graph_function │ |
|  │  echo function_graph > /sys/kernel/debug/tracing/current_tracer          │ |
|  │  echo 1 > /sys/kernel/debug/tracing/tracing_on                           │ |
|  │  # Generate traffic                                                      │ |
|  │  ping -c 1 8.8.8.8                                                       │ |
|  │  cat /sys/kernel/debug/tracing/trace                                     │ |
|  │                                                                          │ |
|  │  Output:                                                                 │ |
|  │   0)               |  netif_receive_skb() {                              │ |
|  │   0)               |    __netif_receive_skb() {                          │ |
|  │   0)               |      __netif_receive_skb_core() {                   │ |
|  │   0)   0.123 us    |        ip_rcv() {                                   │ |
|  │   0)               |          NF_HOOK() {                                │ |
|  │   0)               |            ip_rcv_finish() {                        │ |
|  │   0)               |              ip_local_deliver() {                   │ |
|  │   0)               |                icmp_rcv() {                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 2: TCPDUMP / WIRESHARK                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Capture on interface                                                  │ |
|  │  tcpdump -i eth0 -n -vv port 80                                          │ |
|  │                                                                          │ |
|  │  # Capture with timestamps                                               │ |
|  │  tcpdump -i eth0 -tttt port 80                                           │ |
|  │                                                                          │ |
|  │  # Save to file for Wireshark                                            │ |
|  │  tcpdump -i eth0 -w capture.pcap                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 3: PERF + TRACEPOINTS                                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # List network tracepoints                                              │ |
|  │  perf list 'net:*'                                                       │ |
|  │  perf list 'tcp:*'                                                       │ |
|  │  perf list 'skb:*'                                                       │ |
|  │                                                                          │ |
|  │  # Trace TCP events                                                      │ |
|  │  perf record -e 'tcp:*' -a -- sleep 10                                   │ |
|  │  perf script                                                             │ |
|  │                                                                          │ |
|  │  # Trace skb allocation                                                  │ |
|  │  perf record -e 'skb:kfree_skb' -a -g -- sleep 10                        │ |
|  │  perf report                                                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 4: BPF/EBPF TRACING                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Using bpftrace                                                        │ |
|  │  bpftrace -e 'kprobe:tcp_v4_rcv { @[comm] = count(); }'                  │ |
|  │                                                                          │ |
|  │  # Trace TCP connect latency                                             │ |
|  │  bpftrace -e '                                                           │ |
|  │    kprobe:tcp_v4_connect { @start[tid] = nsecs; }                        │ |
|  │    kretprobe:tcp_v4_connect /@start[tid]/ {                              │ |
|  │        @latency = hist(nsecs - @start[tid]);                             │ |
|  │        delete(@start[tid]);                                              │ |
|  │    }'                                                                    │ |
|  │                                                                          │ |
|  │  # Using BCC tools                                                       │ |
|  │  tcpconnect    # Trace TCP connect()                                     │ |
|  │  tcpaccept     # Trace TCP accept()                                      │ |
|  │  tcplife       # Summarize TCP session lifetimes                         │ |
|  │  tcpretrans    # Trace TCP retransmits                                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 5: /PROC AND /SYS                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Network statistics                                                    │ |
|  │  cat /proc/net/dev          # Interface stats                            │ |
|  │  cat /proc/net/tcp          # TCP connection states                      │ |
|  │  cat /proc/net/snmp         # Protocol statistics                        │ |
|  │  cat /proc/net/netstat      # Extended TCP stats                         │ |
|  │                                                                          │ |
|  │  # Socket info                                                           │ |
|  │  ss -tunap                  # TCP/UDP sockets                            │ |
|  │  ss -s                      # Socket summary                             │ |
|  │                                                                          │ |
|  │  # Interface info                                                        │ |
|  │  ethtool -S eth0            # NIC-level statistics                       │ |
|  │  ethtool -k eth0            # Offload status                             │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**路径追踪策略**：

**方法 1：ftrace**
- 使用 function_graph 追踪器
- 设置 `set_graph_function` 到 `netif_receive_skb`
- 可视化调用层次

**方法 2：tcpdump / Wireshark**
- 接口级抓包
- 保存为 pcap 文件用 Wireshark 分析

**方法 3：perf + tracepoints**
- `perf list 'net:*'` 列出网络追踪点
- `perf record -e 'tcp:*'` 记录 TCP 事件

**方法 4：BPF/eBPF 追踪**
- bpftrace 脚本追踪
- BCC 工具：tcpconnect、tcpretrans

**方法 5：/proc 和 /sys**
- `/proc/net/dev`：接口统计
- `/proc/net/tcp`：TCP 连接状态
- `ss -tunap`：socket 信息

---

## 5. 阅读顺序

```
RECOMMENDED READING ORDER
+=============================================================================+
|                                                                              |
|  LEVEL 1: DATA STRUCTURES (数据结构)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. include/linux/skbuff.h                                               │ |
|  │     • struct sk_buff - the packet carrier                                │ |
|  │     • Understand head/data/tail/end pointers                             │ |
|  │     • skb_push/skb_pull operations                                       │ |
|  │                                                                          │ |
|  │  2. include/linux/netdevice.h                                            │ |
|  │     • struct net_device - the interface                                  │ |
|  │     • struct net_device_ops - driver operations                          │ |
|  │     • NAPI structures                                                    │ |
|  │                                                                          │ |
|  │  3. include/net/sock.h                                                   │ |
|  │     • struct sock - generic socket                                       │ |
|  │     • struct proto - protocol operations                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 2: RX PATH (接收路径)                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  4. net/core/dev.c                                                       │ |
|  │     • netif_receive_skb() - core RX function                             │ |
|  │     • napi_gro_receive() - GRO entry                                     │ |
|  │     • __netif_receive_skb_core() - protocol dispatch                     │ |
|  │                                                                          │ |
|  │  5. net/ipv4/ip_input.c                                                  │ |
|  │     • ip_rcv() - IP receive                                              │ |
|  │     • ip_local_deliver() - local delivery                                │ |
|  │                                                                          │ |
|  │  6. net/ipv4/tcp_ipv4.c                                                  │ |
|  │     • tcp_v4_rcv() - TCP receive for IPv4                                │ |
|  │                                                                          │ |
|  │  7. net/ipv4/tcp_input.c                                                 │ |
|  │     • tcp_rcv_established() - fast path                                  │ |
|  │     • tcp_data_queue() - data queuing                                    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 3: TX PATH (发送路径)                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  8. net/ipv4/tcp_output.c                                                │ |
|  │     • tcp_transmit_skb() - build and send TCP segment                    │ |
|  │     • tcp_write_xmit() - main TX logic                                   │ |
|  │                                                                          │ |
|  │  9. net/ipv4/ip_output.c                                                 │ |
|  │     • ip_queue_xmit() - add IP header                                    │ |
|  │     • ip_output() - output processing                                    │ |
|  │                                                                          │ |
|  │  10. net/core/dev.c                                                      │ |
|  │      • dev_queue_xmit() - TX entry point                                 │ |
|  │      • dev_hard_start_xmit() - call driver                               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 4: EXTENSIONS (扩展)                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  11. net/netfilter/core.c                                                │ |
|  │      • nf_hook_slow() - hook invocation                                  │ |
|  │      • nf_register_net_hook() - registration                             │ |
|  │                                                                          │ |
|  │  12. net/sched/sch_generic.c                                             │ |
|  │      • qdisc infrastructure                                              │ |
|  │      • Traffic control basics                                            │ |
|  │                                                                          │ |
|  │  13. A simple NIC driver (e.g., drivers/net/virtio_net.c)                │ |
|  │      • See how NAPI works in practice                                    │ |
|  │      • Understand ring buffer management                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  READING STRATEGY                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. TRACE A SIMPLE PING                                                  │ |
|  │     Use ftrace to follow ICMP echo request/reply                         │ |
|  │     Understand the full path                                             │ |
|  │                                                                          │ |
|  │  2. TRACE TCP CONNECTION                                                 │ |
|  │     Follow connect() → accept() → data transfer → close()                │ |
|  │     Understand state transitions                                         │ |
|  │                                                                          │ |
|  │  3. FOCUS ON sk_buff MANIPULATION                                        │ |
|  │     Watch how skb->data moves through the stack                          │ |
|  │     Understand when copies happen vs. pointer manipulation               │ |
|  │                                                                          │ |
|  │  4. USE PERF TO FIND HOT PATHS                                           │ |
|  │     perf top under network load                                          │ |
|  │     Shows which functions matter most                                    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**第 1 层：数据结构**
1. `include/linux/skbuff.h`：sk_buff，理解 head/data/tail/end 指针
2. `include/linux/netdevice.h`：net_device，net_device_ops
3. `include/net/sock.h`：sock，proto

**第 2 层：RX 路径**
4. `net/core/dev.c`：netif_receive_skb，协议分发
5. `net/ipv4/ip_input.c`：ip_rcv，ip_local_deliver
6. `net/ipv4/tcp_ipv4.c`：tcp_v4_rcv
7. `net/ipv4/tcp_input.c`：tcp_rcv_established

**第 3 层：TX 路径**
8. `net/ipv4/tcp_output.c`：tcp_transmit_skb
9. `net/ipv4/ip_output.c`：ip_queue_xmit
10. `net/core/dev.c`：dev_queue_xmit

**第 4 层：扩展**
11. `net/netfilter/core.c`：钩子调用
12. `net/sched/sch_generic.c`：qdisc
13. 简单 NIC 驱动（如 virtio_net.c）

**阅读策略**：
1. 追踪简单 ping
2. 追踪 TCP 连接
3. 关注 sk_buff 操作
4. 使用 perf 找热路径
