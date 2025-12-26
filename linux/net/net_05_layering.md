# Linux Networking Subsystem: Layering and PDD Mapping

## 1. PDD Mapping to Networking Stack

```
+------------------------------------------------------------------+
|  PRESENTATION-DOMAIN-DATA LAYERING IN NETWORKING                 |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │                    APPLICATION                              │
    └─────────────────────────────────────────────────────────────┘
                              │
    ┌─────────────────────────▼───────────────────────────────────┐
    │                PRESENTATION LAYER                           │
    │  ┌────────────────────────────────────────────────────────┐ │
    │  │  System Calls (user-kernel boundary)                   │ │
    │  │    sys_socket, sys_bind, sys_connect, sys_sendto       │ │
    │  │    sys_recvfrom, sys_listen, sys_accept, sys_close     │ │
    │  ├────────────────────────────────────────────────────────┤ │
    │  │  net/socket.c                                          │ │
    │  │    sock_create, sock_sendmsg, sock_recvmsg             │ │
    │  │    Parameter validation, user↔kernel data copy         │ │
    │  └────────────────────────────────────────────────────────┘ │
    │  Responsibility: User interface, validation, data copying   │
    └─────────────────────────────────────────────────────────────┘
                              │
    ┌─────────────────────────▼───────────────────────────────────┐
    │                   DOMAIN LAYER                              │
    │  ┌────────────────────────────────────────────────────────┐ │
    │  │  Protocol-Independent Core (net/core/)                 │ │
    │  │    • Socket management (sock.c)                        │ │
    │  │    • Buffer management (skbuff.c)                      │ │
    │  │    • Device management (dev.c)                         │ │
    │  │    • Routing cache (dst.c)                             │ │
    │  │    • Neighbor cache (neighbour.c)                      │ │
    │  ├────────────────────────────────────────────────────────┤ │
    │  │  Protocol Implementations                              │ │
    │  │    • net/ipv4/ - IPv4 stack                            │ │
    │  │    • net/ipv6/ - IPv6 stack                            │ │
    │  │    • net/unix/ - Unix domain sockets                   │ │
    │  └────────────────────────────────────────────────────────┘ │
    │  Responsibility: Protocol logic, packet processing          │
    └─────────────────────────────────────────────────────────────┘
                              │
    ┌─────────────────────────▼───────────────────────────────────┐
    │                    DATA LAYER                               │
    │  ┌────────────────────────────────────────────────────────┐ │
    │  │  Network Device Drivers (drivers/net/)                 │ │
    │  │    • Hardware abstraction                              │ │
    │  │    • DMA management                                    │ │
    │  │    • Interrupt handling                                │ │
    │  ├────────────────────────────────────────────────────────┤ │
    │  │  Hardware (NICs, WiFi adapters, etc.)                  │ │
    │  └────────────────────────────────────────────────────────┘ │
    │  Responsibility: Hardware interaction, physical I/O         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Layer Analysis: Ethernet (L2)

```
+------------------------------------------------------------------+
|  ETHERNET LAYER ANALYSIS                                         |
+------------------------------------------------------------------+

    WHAT STAYS IN CORE (net/ethernet/, net/core/dev.c):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Frame type dispatch (eth_type_trans)                    │
    │  • MAC address management                                  │
    │  • Ethernet header manipulation                            │
    │  • MTU handling                                            │
    │  • Statistics aggregation                                  │
    │                                                              │
    │  /* net/ethernet/eth.c */                                  │
    │  __be16 eth_type_trans(struct sk_buff *skb,                │
    │                        struct net_device *dev)              │
    │  {                                                          │
    │      struct ethhdr *eth = eth_hdr(skb);                    │
    │      skb->dev = dev;                                       │
    │      skb_reset_mac_header(skb);                            │
    │      skb_pull(skb, ETH_HLEN);  /* Strip Ethernet header */ │
    │      return eth->h_proto;       /* Return payload type */  │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    WHAT BELONGS TO DRIVER (drivers/net/ethernet/*/):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Hardware register programming                           │
    │  • DMA buffer management                                   │
    │  • Interrupt coalescing                                    │
    │  • Hardware checksum offload                               │
    │  • Link state detection                                    │
    │                                                              │
    │  /* Driver implements net_device_ops */                    │
    │  static const struct net_device_ops e1000_netdev_ops = {   │
    │      .ndo_open          = e1000_open,                      │
    │      .ndo_stop          = e1000_close,                     │
    │      .ndo_start_xmit    = e1000_xmit_frame,                │
    │      .ndo_set_rx_mode   = e1000_set_multi,                 │
    │      /* ... */                                             │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    BOUNDARY ENFORCEMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   Core (net/core/dev.c)           Driver (drivers/net/)    │
    │   ┌─────────────────────┐         ┌─────────────────────┐  │
    │   │ netif_receive_skb() │         │ e1000_clean_rx()    │  │
    │   │                     │◄────────│   napi_gro_receive()│  │
    │   │ Protocol dispatch   │         │   (passes skb)      │  │
    │   └─────────────────────┘         └─────────────────────┘  │
    │                                                              │
    │   ┌─────────────────────┐         ┌─────────────────────┐  │
    │   │ dev_queue_xmit()    │         │ e1000_xmit_frame()  │  │
    │   │                     │────────►│   (receives skb)    │  │
    │   │ Qdisc, TX queue     │         │   DMA to hardware   │  │
    │   └─────────────────────┘         └─────────────────────┘  │
    │                                                              │
    │   Interface: struct net_device_ops                         │
    │   Contract: skb ownership transfers at function call       │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Layer Analysis: IP (L3)

```
+------------------------------------------------------------------+
|  IP LAYER ANALYSIS                                               |
+------------------------------------------------------------------+

    WHAT STAYS IN CORE (net/core/):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Routing cache management (dst.c)                        │
    │  • Neighbor discovery cache (neighbour.c)                  │
    │  • Generic flow handling (flow.c)                          │
    │  • FIB rules framework (fib_rules.c)                       │
    │  • Device event notifications                              │
    │                                                              │
    │  These are SHARED across IPv4, IPv6, and other L3 protos   │
    └─────────────────────────────────────────────────────────────┘

    WHAT BELONGS TO IPv4 (net/ipv4/):
    ┌─────────────────────────────────────────────────────────────┐
    │  • IP header processing (ip_input.c, ip_output.c)          │
    │  • Fragmentation/reassembly (ip_fragment.c)                │
    │  • IPv4 routing (route.c, fib_*.c)                         │
    │  • ICMP (icmp.c)                                           │
    │  • ARP (arp.c)                                             │
    │  • Socket binding (af_inet.c)                              │
    │                                                              │
    │  /* Receive path */                                        │
    │  int ip_rcv(struct sk_buff *skb, ...)                      │
    │  {                                                          │
    │      /* Validate IP header */                              │
    │      iph = ip_hdr(skb);                                    │
    │      if (iph->ihl < 5 || iph->version != 4)                │
    │          goto drop;                                         │
    │                                                              │
    │      /* Pass through netfilter */                          │
    │      return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING,     │
    │                     skb, dev, NULL, ip_rcv_finish);         │
    │  }                                                          │
    │                                                              │
    │  /* Output path */                                         │
    │  int ip_output(struct sk_buff *skb)                        │
    │  {                                                          │
    │      /* Add IP header, fragment if needed */               │
    │      return NF_HOOK(NFPROTO_IPV4, NF_INET_POST_ROUTING,    │
    │                     skb, NULL, dev, ip_finish_output);      │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    BOUNDARY ENFORCEMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   Transport (TCP/UDP)              IP Layer                 │
    │   ┌─────────────────────┐         ┌─────────────────────┐  │
    │   │ tcp_transmit_skb() │────────►│ ip_queue_xmit()     │  │
    │   │                     │         │   • Route lookup    │  │
    │   │                     │         │   • Build IP header │  │
    │   └─────────────────────┘         └──────────┬──────────┘  │
    │                                              │              │
    │                                              ▼              │
    │   ┌─────────────────────┐         ┌─────────────────────┐  │
    │   │ tcp_v4_rcv()       │◄────────│ ip_local_deliver()  │  │
    │   │                     │         │   • Find protocol   │  │
    │   │                     │         │   • Dispatch        │  │
    │   └─────────────────────┘         └─────────────────────┘  │
    │                                                              │
    │   IP → Device Layer:                                       │
    │   ┌─────────────────────┐         ┌─────────────────────┐  │
    │   │ ip_finish_output() │────────►│ dst_output()        │  │
    │   │                     │         │   dev_queue_xmit()  │  │
    │   └─────────────────────┘         └─────────────────────┘  │
    │                                                              │
    │   Interface: Protocol handler registration                 │
    │   Contract: ip_local_deliver_finish() dispatches by proto  │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Layer Analysis: TCP (L4)

```
+------------------------------------------------------------------+
|  TCP LAYER ANALYSIS                                              |
+------------------------------------------------------------------+

    WHAT STAYS IN CORE (net/core/, include/net/):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Socket buffer management (sock.c)                       │
    │  • Generic connection tracking                             │
    │  • Wait queue handling                                     │
    │  • Socket option framework                                 │
    │                                                              │
    │  Generic struct proto operations shared by all transport:  │
    │  • Memory accounting                                       │
    │  • Slab cache for protocol sockets                         │
    └─────────────────────────────────────────────────────────────┘

    WHAT BELONGS TO TCP (net/ipv4/tcp*.c):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Connection state machine (tcp_input.c)                  │
    │  • Congestion control (tcp_cong.c, tcp_cubic.c)            │
    │  • Retransmission (tcp_output.c, tcp_timer.c)              │
    │  • Sequence number management                              │
    │  • Window management                                       │
    │  • SACK/FACK                                               │
    │  • TCP options parsing                                     │
    │                                                              │
    │  /* TCP-specific sock extension */                         │
    │  struct tcp_sock {                                         │
    │      struct inet_connection_sock inet_conn;                │
    │      /* TCP state machine */                               │
    │      __u32 snd_una;        /* First unacked seq */         │
    │      __u32 snd_nxt;        /* Next seq to send */          │
    │      __u32 rcv_nxt;        /* Expected next seq */         │
    │      /* Congestion control */                              │
    │      __u32 snd_cwnd;       /* Congestion window */         │
    │      __u32 snd_ssthresh;   /* Slow start threshold */      │
    │      /* ... 100+ more fields */                            │
    │  };                                                         │
    └─────────────────────────────────────────────────────────────┘

    BOUNDARY ENFORCEMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   Socket Layer                     TCP Layer                │
    │   ┌─────────────────────┐         ┌─────────────────────┐  │
    │   │ sock_sendmsg()     │         │ tcp_sendmsg()       │  │
    │   │                     │────────►│   • Segment data    │  │
    │   │ sock->ops->sendmsg │         │   • Update seq nums │  │
    │   └─────────────────────┘         │   • Push to queue   │  │
    │                                    └──────────┬──────────┘  │
    │                                               │              │
    │                                               ▼              │
    │                                    ┌─────────────────────┐  │
    │                                    │ tcp_transmit_skb() │  │
    │                                    │   • Build TCP hdr  │  │
    │                                    │   • ip_queue_xmit()│  │
    │                                    └─────────────────────┘  │
    │                                                              │
    │   TCP → IP:                                                │
    │   tcp_transmit_skb() → ip_queue_xmit()                     │
    │   skb ownership passes to IP layer                         │
    │                                                              │
    │   IP → TCP:                                                │
    │   ip_local_deliver_finish() → tcp_v4_rcv()                 │
    │   Dispatch via inet_protos[IPPROTO_TCP]                    │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    TCP STATE MACHINE (Domain Logic):
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │   CLOSED ──(connect)──► SYN_SENT ──(SYN+ACK)──► ESTABLISHED│
    │      │                                              │       │
    │   (listen)                                      (close)     │
    │      │                                              │       │
    │      ▼                                              ▼       │
    │   LISTEN ──(SYN)──► SYN_RCVD ──(ACK)──►     FIN_WAIT_1     │
    │                                                     │       │
    │                                                 (FIN+ACK)   │
    │                                                     │       │
    │                                                     ▼       │
    │                                              TIME_WAIT      │
    │                                                     │       │
    │                                              (timeout)      │
    │                                                     │       │
    │                                                     ▼       │
    │                                                 CLOSED      │
    │                                                              │
    │   All state transitions are TCP-specific DOMAIN LOGIC      │
    │   NOT in the socket layer or IP layer                      │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 5. Complete Layer Diagram

```
+------------------------------------------------------------------+
|  COMPLETE NETWORKING LAYER ARCHITECTURE                          |
+------------------------------------------------------------------+

    ┌─────────────────────────────────────────────────────────────┐
    │                    USER SPACE                               │
    │              Application (curl, nginx, etc.)                │
    └─────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │  SYSTEM CALLS     │
                    │  (PRESENTATION)   │
                    │  socket, send,    │
                    │  recv, connect    │
                    └─────────┬─────────┘
                              │
    ╔═════════════════════════▼═════════════════════════════════════╗
    ║                    SOCKET LAYER                               ║
    ║              net/socket.c (PRESENTATION)                      ║
    ║  ┌──────────────────────────────────────────────────────────┐ ║
    ║  │  sock_sendmsg()  sock_recvmsg()  sock_create()          │ ║
    ║  │  Parameter validation, user-space data copy              │ ║
    ║  └────────────────────────┬─────────────────────────────────┘ ║
    ╚═══════════════════════════╪═══════════════════════════════════╝
                                │
                    ┌───────────▼───────────┐
                    │ sock->ops->sendmsg()  │
                    │  proto_ops dispatch   │
                    └───────────┬───────────┘
                                │
    ╔═══════════════════════════▼═══════════════════════════════════╗
    ║                    TRANSPORT LAYER                            ║
    ║              net/ipv4/tcp.c, udp.c (DOMAIN)                   ║
    ║  ┌──────────────────────────────────────────────────────────┐ ║
    ║  │  TCP: State machine, congestion, retransmit              │ ║
    ║  │  UDP: Simple datagram handling                           │ ║
    ║  │  struct proto (tcp_prot, udp_prot)                       │ ║
    ║  └────────────────────────┬─────────────────────────────────┘ ║
    ╚═══════════════════════════╪═══════════════════════════════════╝
                                │
                    ┌───────────▼───────────┐
                    │ ip_queue_xmit()       │
                    │ tcp_v4_rcv()          │
                    └───────────┬───────────┘
                                │
    ╔═══════════════════════════▼═══════════════════════════════════╗
    ║                    NETWORK LAYER                              ║
    ║              net/ipv4/ (DOMAIN)                               ║
    ║  ┌──────────────────────────────────────────────────────────┐ ║
    ║  │  IP: Routing, fragmentation, ICMP                        │ ║
    ║  │  ip_rcv(), ip_output(), ip_forward()                     │ ║
    ║  │  Routing tables, FIB                                     │ ║
    ║  └────────────────────────┬─────────────────────────────────┘ ║
    ╚═══════════════════════════╪═══════════════════════════════════╝
                                │
                    ┌───────────▼───────────┐
                    │ dev_queue_xmit()      │
                    │ netif_receive_skb()   │
                    └───────────┬───────────┘
                                │
    ╔═══════════════════════════▼═══════════════════════════════════╗
    ║                    DEVICE LAYER                               ║
    ║              net/core/dev.c (DOMAIN/DATA boundary)            ║
    ║  ┌──────────────────────────────────────────────────────────┐ ║
    ║  │  Traffic control (qdisc), multiqueue, NAPI               │ ║
    ║  │  net_device management                                   │ ║
    ║  │  struct net_device_ops dispatch                          │ ║
    ║  └────────────────────────┬─────────────────────────────────┘ ║
    ╚═══════════════════════════╪═══════════════════════════════════╝
                                │
                    ┌───────────▼───────────┐
                    │ dev->netdev_ops->     │
                    │   ndo_start_xmit()    │
                    └───────────┬───────────┘
                                │
    ╔═══════════════════════════▼═══════════════════════════════════╗
    ║                    DRIVER LAYER                               ║
    ║              drivers/net/ (DATA)                              ║
    ║  ┌──────────────────────────────────────────────────────────┐ ║
    ║  │  Hardware-specific: DMA, registers, interrupts           │ ║
    ║  │  Intel e1000, Broadcom bnx2x, Realtek r8169, etc.        │ ║
    ║  └────────────────────────┬─────────────────────────────────┘ ║
    ╚═══════════════════════════╪═══════════════════════════════════╝
                                │
                    ┌───────────▼───────────┐
                    │      HARDWARE         │
                    │     NIC / WiFi        │
                    └───────────────────────┘
```

---

## 6. Boundary Enforcement Mechanisms

```
+------------------------------------------------------------------+
|  HOW BOUNDARIES ARE ENFORCED                                     |
+------------------------------------------------------------------+

    MECHANISM 1: Ops Tables (Contracts)
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  Each layer defines an interface (ops table):              │
    │                                                              │
    │  Socket ↔ Protocol:  struct proto_ops                      │
    │  Protocol ↔ IP:      Function calls (ip_queue_xmit)        │
    │  IP ↔ Device:        struct net_device_ops                 │
    │  Device ↔ Hardware:  Driver-specific                       │
    │                                                              │
    │  Layers CANNOT bypass these interfaces.                    │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    MECHANISM 2: Separate Directories
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  net/socket.c      → Socket layer only                     │
    │  net/core/         → Protocol-independent core             │
    │  net/ipv4/         → IPv4 specific                         │
    │  net/ipv6/         → IPv6 specific                         │
    │  drivers/net/      → Hardware drivers                      │
    │                                                              │
    │  Directory structure mirrors layer boundaries.             │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    MECHANISM 3: Header Separation
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  include/linux/    → Public APIs                           │
    │    • skbuff.h, netdevice.h, net.h                         │
    │                                                              │
    │  include/net/      → Internal protocol headers             │
    │    • sock.h, tcp.h, ip.h                                  │
    │                                                              │
    │  Lower layers don't include higher layer headers.          │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘

    MECHANISM 4: Ownership Transfer Rules
    ┌─────────────────────────────────────────────────────────────┐
    │                                                              │
    │  TX Path: skb ownership flows DOWN                         │
    │    sock_sendmsg → tcp_sendmsg → ip_queue_xmit → driver     │
    │                                                              │
    │  RX Path: skb ownership flows UP                           │
    │    driver → netif_receive_skb → ip_rcv → tcp_v4_rcv        │
    │                                                              │
    │  At each boundary, ownership transfers EXPLICITLY.         │
    │  Layer that receives skb is responsible for freeing it.    │
    │                                                              │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  LAYERING SUMMARY                                                |
+------------------------------------------------------------------+

    PDD MAPPING:
    ┌─────────────────────────────────────────────────────────────┐
    │  PRESENTATION: System calls + net/socket.c                 │
    │    → User interface, validation, data copying              │
    │                                                              │
    │  DOMAIN: net/core/ + net/ipv4/ + net/ipv6/                │
    │    → Protocol logic, state machines, routing               │
    │                                                              │
    │  DATA: drivers/net/                                        │
    │    → Hardware abstraction, DMA, interrupts                 │
    └─────────────────────────────────────────────────────────────┘

    KEY BOUNDARIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  Socket ↔ Transport:  proto_ops                            │
    │  Transport ↔ Network: Protocol handler registration        │
    │  Network ↔ Device:    net_device_ops                       │
    │  Device ↔ Hardware:   Driver implementation                │
    └─────────────────────────────────────────────────────────────┘

    BOUNDARY ENFORCEMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Ops tables define contracts                             │
    │  • Directory structure mirrors layers                      │
    │  • Header includes follow dependencies                     │
    │  • Ownership transfer is explicit                          │
    └─────────────────────────────────────────────────────────────┘
```

**中文总结：**
- **表示层**：系统调用+net/socket.c，负责用户界面和数据验证
- **领域层**：net/core/+net/ipv4/，包含协议逻辑和状态机
- **数据层**：drivers/net/，硬件抽象和DMA操作
- **边界机制**：ops表定义契约、目录结构镜像层次、头文件遵循依赖、所有权显式传递
- **Ethernet层**：核心处理帧类型分发，驱动处理硬件细节
- **IP层**：核心提供路由缓存，IPv4实现具体头处理和路由
- **TCP层**：核心提供套接字管理，TCP实现状态机和拥塞控制

