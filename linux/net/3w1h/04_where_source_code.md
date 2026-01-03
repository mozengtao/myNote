# WHERE｜源码落点（在真实工程中在哪里）

## 1. 入口文件和目录

```
SOURCE CODE ENTRY POINTS
+=============================================================================+
|                                                                              |
|  FILE SYSTEM LAYOUT                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  linux/                                                                  │ |
|  │  ├── include/                                                            │ |
|  │  │   ├── linux/                                                          │ |
|  │  │   │   ├── netdevice.h     ★ net_device, net_device_ops               │ |
|  │  │   │   ├── skbuff.h        ★ sk_buff, skb_* functions                 │ |
|  │  │   │   ├── socket.h        ★ socket structures                        │ |
|  │  │   │   └── tcp.h           ★ TCP constants and options                │ |
|  │  │   │                                                                   │ |
|  │  │   └── net/                                                            │ |
|  │  │       ├── sock.h          ★ struct sock, sock_* functions            │ |
|  │  │       ├── tcp.h           ★ tcp_sock, TCP internals                  │ |
|  │  │       └── ip.h            ★ IP layer definitions                     │ |
|  │  │                                                                       │ |
|  │  └── net/                    ★ Network Stack Implementation             │ |
|  │      ├── core/                                                           │ |
|  │      │   ├── dev.c           ★ netif_receive_skb, dev_queue_xmit        │ |
|  │      │   ├── sock.c          ★ socket layer core                        │ |
|  │      │   ├── skbuff.c        ★ sk_buff allocation/operations            │ |
|  │      │   └── filter.c          packet filtering                          │ |
|  │      │                                                                   │ |
|  │      ├── ipv4/                                                           │ |
|  │      │   ├── tcp.c           ★ TCP socket operations                    │ |
|  │      │   ├── tcp_input.c     ★ TCP receive path                         │ |
|  │      │   ├── tcp_output.c    ★ TCP send path                            │ |
|  │      │   ├── tcp_ipv4.c      ★ TCP/IPv4 glue                            │ |
|  │      │   ├── tcp_timer.c       TCP timers                                │ |
|  │      │   ├── tcp_cong.c        congestion control                        │ |
|  │      │   ├── udp.c             UDP implementation                        │ |
|  │      │   ├── ip_input.c        IP receive                                │ |
|  │      │   ├── ip_output.c       IP send                                   │ |
|  │      │   └── route.c           routing table                             │ |
|  │      │                                                                   │ |
|  │      ├── ipv6/                 IPv6 implementation (parallel to ipv4)    │ |
|  │      │                                                                   │ |
|  │      ├── socket.c            ★ socket system call entry                 │ |
|  │      │                                                                   │ |
|  │      └── sched/                traffic control (qdisc)                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ★ = Primary entry point                                                     |
|                                                                              |
+=============================================================================+
```

**中文说明：**

源码主要分布在两个区域：
- **`include/`**：头文件，包含核心数据结构定义
  - `linux/netdevice.h`：网络设备抽象
  - `linux/skbuff.h`：网络缓冲区
  - `net/sock.h`：socket 核心结构
  - `net/tcp.h`：TCP 内部结构

- **`net/`**：实现代码
  - `net/core/`：核心网络功能
  - `net/ipv4/`：IPv4 协议栈
  - `net/socket.c`：系统调用入口

---

## 2. 核心架构 Struct / Class

```
CORE ARCHITECTURAL STRUCTURES
+=============================================================================+
|                                                                              |
|  STRUCT NET_DEVICE (include/linux/netdevice.h)                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct net_device {                                                     │ |
|  │      // 设备标识                                                         │ |
|  │      char            name[IFNAMSIZ];     // "eth0", "lo", etc.          │ |
|  │      unsigned int    ifindex;             // interface index             │ |
|  │                                                                          │ |
|  │      // 设备属性                                                         │ |
|  │      unsigned int    mtu;                 // Maximum Transfer Unit       │ |
|  │      unsigned int    flags;               // IFF_UP, IFF_RUNNING, ...   │ |
|  │      unsigned char   dev_addr[MAX_ADDR_LEN];  // MAC address            │ |
|  │                                                                          │ |
|  │      // 核心操作表 ★                                                     │ |
|  │      const struct net_device_ops *netdev_ops;                           │ |
|  │                                                                          │ |
|  │      // 统计信息                                                         │ |
|  │      struct net_device_stats stats;                                     │ |
|  │                                                                          │ |
|  │      // 流量控制                                                         │ |
|  │      struct Qdisc    *qdisc;              // queue discipline           │ |
|  │                                                                          │ |
|  │      // 命名空间                                                         │ |
|  │      struct net      *nd_net;             // network namespace          │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  struct net_device_ops {                                                 │ |
|  │      int  (*ndo_open)(struct net_device *dev);                          │ |
|  │      int  (*ndo_stop)(struct net_device *dev);                          │ |
|  │      netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,                 │ |
|  │                                     struct net_device *dev);            │ |
|  │      struct net_device_stats* (*ndo_get_stats)(struct net_device *dev); │ |
|  │      int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);   │ |
|  │      int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);       │ |
|  │      // ... many more operations                                        │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  Location: include/linux/netdevice.h:800-1200                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT SK_BUFF (include/linux/skbuff.h)                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct sk_buff {                                                        │ |
|  │      // 链表管理                                                         │ |
|  │      struct sk_buff      *next, *prev;                                  │ |
|  │                                                                          │ |
|  │      // 关联的 socket 和设备                                             │ |
|  │      struct sock         *sk;             // owner socket               │ |
|  │      struct net_device   *dev;            // device                     │ |
|  │                                                                          │ |
|  │      // 数据指针 ★                                                       │ |
|  │      unsigned char       *head;           // buffer start               │ |
|  │      unsigned char       *data;           // data start                 │ |
|  │      unsigned int        tail;            // data end (offset)          │ |
|  │      unsigned int        end;             // buffer end (offset)        │ |
|  │      unsigned int        len;             // data length                │ |
|  │                                                                          │ |
|  │      // 协议信息                                                         │ |
|  │      __be16              protocol;        // packet type (ETH_P_IP)     │ |
|  │      __u16               transport_header;                              │ |
|  │      __u16               network_header;                                │ |
|  │      __u16               mac_header;                                    │ |
|  │                                                                          │ |
|  │      // 引用计数                                                         │ |
|  │      atomic_t            users;           // reference count            │ |
|  │                                                                          │ |
|  │      // 校验和                                                           │ |
|  │      __u8                ip_summed:2;     // checksum status            │ |
|  │      __wsum              csum;            // checksum value             │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  Key operations (include/linux/skbuff.h):                                │ |
|  │  • alloc_skb()     - allocate sk_buff                                   │ |
|  │  • kfree_skb()     - free sk_buff                                       │ |
|  │  • skb_put()       - add data to tail                                   │ |
|  │  • skb_push()      - add data to head (prepend header)                  │ |
|  │  • skb_pull()      - remove data from head                              │ |
|  │  • skb_reserve()   - reserve headroom                                   │ |
|  │  • skb_clone()     - create shared clone                                │ |
|  │                                                                          │ |
|  │  Location: include/linux/skbuff.h:300-600                               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT SOCK (include/net/sock.h)                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct sock {                                                           │ |
|  │      // Socket 状态 ★                                                    │ |
|  │      unsigned char       sk_state;        // TCP_ESTABLISHED, ...       │ |
|  │      unsigned short      sk_family;       // AF_INET, AF_INET6          │ |
|  │      unsigned char       sk_protocol;     // IPPROTO_TCP, ...           │ |
|  │                                                                          │ |
|  │      // 接收/发送队列                                                    │ |
|  │      struct sk_buff_head sk_receive_queue;                              │ |
|  │      struct sk_buff_head sk_write_queue;                                │ |
|  │                                                                          │ |
|  │      // 缓冲区限制                                                       │ |
|  │      int                 sk_sndbuf;       // send buffer limit          │ |
|  │      int                 sk_rcvbuf;       // receive buffer limit       │ |
|  │                                                                          │ |
|  │      // 锁 ★                                                             │ |
|  │      socket_lock_t       sk_lock;         // socket lock                │ |
|  │      atomic_t            sk_refcnt;       // reference count            │ |
|  │                                                                          │ |
|  │      // 回调函数 ★                                                       │ |
|  │      void (*sk_data_ready)(struct sock *sk, int len);                   │ |
|  │      void (*sk_state_change)(struct sock *sk);                          │ |
|  │      void (*sk_write_space)(struct sock *sk);                           │ |
|  │                                                                          │ |
|  │      // 协议操作表 ★                                                     │ |
|  │      struct proto        *sk_prot;                                      │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  Location: include/net/sock.h:200-500                                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT TCP_SOCK (include/linux/tcp.h + include/net/tcp.h)                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct tcp_sock {                                                       │ |
|  │      struct inet_connection_sock inet_conn;  // base                    │ |
|  │                                                                          │ |
|  │      // 序列号 ★                                                         │ |
|  │      u32 snd_una;      // oldest unacknowledged seq                     │ |
|  │      u32 snd_nxt;      // next seq to send                              │ |
|  │      u32 rcv_nxt;      // expected next seq                             │ |
|  │      u32 copied_seq;   // seq copied to user space                      │ |
|  │                                                                          │ |
|  │      // 窗口 ★                                                           │ |
|  │      u32 snd_wnd;      // send window                                   │ |
|  │      u32 rcv_wnd;      // receive window                                │ |
|  │                                                                          │ |
|  │      // 拥塞控制 ★                                                       │ |
|  │      u32 snd_cwnd;     // congestion window                             │ |
|  │      u32 snd_ssthresh; // slow start threshold                          │ |
|  │                                                                          │ |
|  │      // RTT 测量                                                         │ |
|  │      u32 srtt;         // smoothed RTT (scaled)                         │ |
|  │      u32 mdev;         // RTT deviation (scaled)                        │ |
|  │                                                                          │ |
|  │      // 选项                                                             │ |
|  │      u16 mss_cache;    // cached MSS                                    │ |
|  │      u8  nonagle:4;    // Nagle algorithm control                       │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  Location: include/linux/tcp.h + net/ipv4/tcp.c                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT NET (include/net/net_namespace.h)                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct net {                                                            │ |
|  │      // 设备列表                                                         │ |
|  │      struct list_head    dev_base_head;   // all devices in namespace   │ |
|  │                                                                          │ |
|  │      // 协议特定数据                                                     │ |
|  │      struct netns_ipv4   ipv4;            // IPv4 namespace             │ |
|  │      struct netns_ipv6   ipv6;            // IPv6 namespace             │ |
|  │                                                                          │ |
|  │      // loopback 设备                                                    │ |
|  │      struct net_device   *loopback_dev;                                 │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  Purpose: Network namespace for container isolation                      │ |
|  │  Location: include/net/net_namespace.h                                  │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

核心架构结构体的位置：
- **`net_device`**：`include/linux/netdevice.h`，网络设备抽象
- **`sk_buff`**：`include/linux/skbuff.h`，网络数据包容器
- **`sock`**：`include/net/sock.h`，通用 socket 结构
- **`tcp_sock`**：`include/linux/tcp.h` + `include/net/tcp.h`，TCP 特有数据
- **`net`**：`include/net/net_namespace.h`，网络命名空间

---

## 3. 枢纽路径函数

```
CRITICAL PATH FUNCTIONS (枢纽路径函数)
+=============================================================================+
|                                                                              |
|  RECEIVE PATH (接收路径)                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  netif_receive_skb() - 接收路径入口                                      │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/core/dev.c:3100-3200                               │  │ |
|  │  │                                                                    │  │ |
|  │  │  int netif_receive_skb(struct sk_buff *skb)                       │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      // Set timestamp                                              │  │ |
|  │  │      net_timestamp_check(netdev_tstamp_prequeue, skb);            │  │ |
|  │  │                                                                    │  │ |
|  │  │      // RPS (Receive Packet Steering) if enabled                  │  │ |
|  │  │      if (static_key_false(&rps_needed))                           │  │ |
|  │  │          return netif_receive_skb_internal(skb);                  │  │ |
|  │  │                                                                    │  │ |
|  │  │      return __netif_receive_skb(skb);                             │  │ |
|  │  │  }                                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  Called by: NAPI poll handlers                                     │  │ |
|  │  │  Calls: Protocol handlers (ip_rcv, arp_rcv, etc.)                 │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  ip_rcv() - IP 层接收                                                    │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/ipv4/ip_input.c:350-420                            │  │ |
|  │  │                                                                    │  │ |
|  │  │  int ip_rcv(struct sk_buff *skb, struct net_device *dev,          │  │ |
|  │  │             struct packet_type *pt, struct net_device *orig_dev)  │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      // Validate IP header                                         │  │ |
|  │  │      iph = ip_hdr(skb);                                            │  │ |
|  │  │      if (iph->version != 4 || iph->ihl < 5)                       │  │ |
|  │  │          goto drop;                                                │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Pass through Netfilter                                    │  │ |
|  │  │      return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING, skb,       │  │ |
|  │  │                     dev, NULL, ip_rcv_finish);                    │  │ |
|  │  │  }                                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  Registered with: dev_add_pack(&ip_packet_type)                   │  │ |
|  │  │  Calls: ip_rcv_finish → ip_local_deliver                          │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  tcp_v4_rcv() - TCP 接收                                                 │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/ipv4/tcp_ipv4.c:1600-1800                          │  │ |
|  │  │                                                                    │  │ |
|  │  │  int tcp_v4_rcv(struct sk_buff *skb)                              │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      th = tcp_hdr(skb);                                           │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Find socket                                                │  │ |
|  │  │      sk = __inet_lookup(net, &tcp_hashinfo, iph->saddr,           │  │ |
|  │  │                         th->source, iph->daddr, th->dest, ...);   │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Process by state                                           │  │ |
|  │  │      if (sk->sk_state == TCP_ESTABLISHED)                          │  │ |
|  │  │          tcp_rcv_established(sk, skb, th, skb->len);              │  │ |
|  │  │      else                                                          │  │ |
|  │  │          tcp_rcv_state_process(sk, skb, th, skb->len);            │  │ |
|  │  │  }                                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  Registered with: inet_add_protocol(&tcp_protocol, IPPROTO_TCP)   │  │ |
|  │  │  Calls: tcp_rcv_established / tcp_rcv_state_process               │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  SEND PATH (发送路径)                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  tcp_sendmsg() - TCP 发送入口                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/ipv4/tcp.c:920-1150                                │  │ |
|  │  │                                                                    │  │ |
|  │  │  int tcp_sendmsg(struct kiocb *iocb, struct sock *sk,             │  │ |
|  │  │                  struct msghdr *msg, size_t size)                 │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      lock_sock(sk);                                                │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Check state                                                │  │ |
|  │  │      if ((1 << sk->sk_state) & ~(TCPF_ESTABLISHED | TCPF_CLOSE_WAIT)) │
|  │  │          goto out_err;                                             │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Copy data to send buffer                                   │  │ |
|  │  │      while (msg_data_left(msg)) {                                  │  │ |
|  │  │          skb = sk_stream_alloc_skb(sk, ...);                      │  │ |
|  │  │          skb_entail(sk, skb);                                      │  │ |
|  │  │          skb_copy_to_page(sk, msg, skb, ...);                     │  │ |
|  │  │      }                                                             │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Push data                                                  │  │ |
|  │  │      tcp_push(sk, flags, mss_now, ...);                           │  │ |
|  │  │                                                                    │  │ |
|  │  │      release_sock(sk);                                             │  │ |
|  │  │  }                                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  Called by: sock_sendmsg (socket layer)                           │  │ |
|  │  │  Calls: tcp_push → tcp_write_xmit                                 │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  dev_queue_xmit() - 设备层发送                                           │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/core/dev.c:2500-2700                               │  │ |
|  │  │                                                                    │  │ |
|  │  │  int dev_queue_xmit(struct sk_buff *skb)                          │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      dev = skb->dev;                                               │  │ |
|  │  │      txq = netdev_pick_tx(dev, skb);                              │  │ |
|  │  │      q = rcu_dereference_bh(txq->qdisc);                          │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Enqueue to qdisc                                           │  │ |
|  │  │      rc = q->enqueue(skb, q);                                     │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Run qdisc                                                  │  │ |
|  │  │      __qdisc_run(q);                                               │  │ |
|  │  │  }                                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  Called by: ip_local_out → dst_output                             │  │ |
|  │  │  Calls: ndo_start_xmit (driver)                                   │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  REGISTRATION FUNCTIONS (注册函数)                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  dev_add_pack() / dev_remove_pack() - 协议类型注册                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/core/dev.c:350-420                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  void dev_add_pack(struct packet_type *pt)                        │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      spin_lock(&ptype_lock);                                       │  │ |
|  │  │      if (pt->type == htons(ETH_P_ALL))                            │  │ |
|  │  │          list_add_rcu(&pt->list, &ptype_all);                     │  │ |
|  │  │      else                                                          │  │ |
|  │  │          list_add_rcu(&pt->list,                                  │  │ |
|  │  │                       &ptype_base[ntohs(pt->type) & PTYPE_HASH_MASK]);│
|  │  │      spin_unlock(&ptype_lock);                                     │  │ |
|  │  │  }                                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  Usage: dev_add_pack(&ip_packet_type);  // register IP handler    │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  register_netdevice() / unregister_netdevice() - 设备注册                │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/core/dev.c:5000-5200                               │  │ |
|  │  │                                                                    │  │ |
|  │  │  int register_netdevice(struct net_device *dev)                   │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      // Assign ifindex                                             │  │ |
|  │  │      dev->ifindex = dev_new_index(net);                           │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Add to device list                                         │  │ |
|  │  │      list_add_tail_rcu(&dev->dev_list, &net->dev_base_head);      │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Initialize sysfs                                           │  │ |
|  │  │      netdev_register_kobject(dev);                                │  │ |
|  │  │  }                                                                 │  │ |
|  │  │                                                                    │  │ |
|  │  │  Usage: register_netdev(dev);  // driver init                     │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  │  tcp_v4_connect() - TCP 连接                                             │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐  │ |
|  │  │  Location: net/ipv4/tcp_ipv4.c:150-250                            │  │ |
|  │  │                                                                    │  │ |
|  │  │  int tcp_v4_connect(struct sock *sk, struct sockaddr *uaddr,      │  │ |
|  │  │                     int addr_len)                                 │  │ |
|  │  │  {                                                                 │  │ |
|  │  │      // Route lookup                                               │  │ |
|  │  │      rt = ip_route_connect(fl4, ...);                             │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Set addresses                                              │  │ |
|  │  │      inet->inet_daddr = usin->sin_addr.s_addr;                    │  │ |
|  │  │      inet->inet_dport = usin->sin_port;                           │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Change state to SYN_SENT                                   │  │ |
|  │  │      tcp_set_state(sk, TCP_SYN_SENT);                              │  │ |
|  │  │                                                                    │  │ |
|  │  │      // Send SYN                                                   │  │ |
|  │  │      tcp_connect(sk);                                              │  │ |
|  │  │  }                                                                 │  │ |
|  │  └──────────────────────────────────────────────────────────────────┘  │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

枢纽路径函数位置：

**接收路径：**
- `netif_receive_skb()`：`net/core/dev.c`，设备层入口
- `ip_rcv()`：`net/ipv4/ip_input.c`，IP 层入口
- `tcp_v4_rcv()`：`net/ipv4/tcp_ipv4.c`，TCP 入口

**发送路径：**
- `tcp_sendmsg()`：`net/ipv4/tcp.c`，TCP 发送入口
- `dev_queue_xmit()`：`net/core/dev.c`，设备层发送

**注册函数：**
- `dev_add_pack()`：`net/core/dev.c`，协议处理器注册
- `register_netdevice()`：`net/core/dev.c`，设备注册
- `tcp_v4_connect()`：`net/ipv4/tcp_ipv4.c`，TCP 连接

---

## 4. 验证 WHY / HOW / WHAT 的方法

```
VERIFICATION METHODS
+=============================================================================+
|                                                                              |
|  METHOD 1: TRACE SK_BUFF FLOW (追踪 sk_buff 流动)                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  验证：分层架构和事件驱动                                                │ |
|  │                                                                          │ |
|  │  # 使用 ftrace 追踪 sk_buff 函数                                         │ |
|  │  echo 'netif_receive_skb' > /sys/kernel/debug/tracing/set_ftrace_filter│ |
|  │  echo 'ip_rcv' >> /sys/kernel/debug/tracing/set_ftrace_filter          │ |
|  │  echo 'tcp_v4_rcv' >> /sys/kernel/debug/tracing/set_ftrace_filter      │ |
|  │  echo function_graph > /sys/kernel/debug/tracing/current_tracer        │ |
|  │                                                                          │ |
|  │  Expected output:                                                        │ |
|  │  netif_receive_skb() {                                                  │ |
|  │      __netif_receive_skb_core() {                                       │ |
|  │          ip_rcv() {                                                     │ |
|  │              ip_rcv_finish() {                                          │ |
|  │                  ip_local_deliver() {                                   │ |
|  │                      tcp_v4_rcv() {                                     │ |
|  │                          tcp_rcv_established();                         │ |
|  │                      }                                                  │ |
|  │                  }                                                      │ |
|  │              }                                                          │ |
|  │          }                                                              │ |
|  │      }                                                                  │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  验证结论：清晰的分层调用链                                              │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 2: OBSERVE NET_DEVICE_OPS CALLBACKS (观察 net_device_ops 回调)       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  验证：抽象驱动接口                                                      │ |
|  │                                                                          │ |
|  │  # 查看特定驱动的 ops 定义                                               │ |
|  │  grep -A 30 'static const struct net_device_ops' drivers/net/ethernet/  │ |
|  │                                                                          │ |
|  │  Example from e1000e:                                                    │ |
|  │  static const struct net_device_ops e1000_netdev_ops = {                │ |
|  │      .ndo_open               = e1000_open,                              │ |
|  │      .ndo_stop               = e1000_close,                             │ |
|  │      .ndo_start_xmit         = e1000_xmit_frame,                        │ |
|  │      .ndo_get_stats          = e1000_get_stats,                         │ |
|  │      .ndo_set_rx_mode        = e1000_set_rx_mode,                       │ |
|  │      .ndo_change_mtu         = e1000_change_mtu,                        │ |
|  │      // ...                                                             │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  验证结论：所有驱动遵循相同接口                                          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 3: CHECK TCP FSM ON SK_BUFF (检查 TCP 状态机)                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  验证：状态机模式                                                        │ |
|  │                                                                          │ |
|  │  # 从 tcp.c 查看状态转换表                                               │ |
|  │  grep -A 20 'static const unsigned char new_state' net/ipv4/tcp.c      │ |
|  │                                                                          │ |
|  │  static const unsigned char new_state[16] = {                            │ |
|  │    /* current state:        new state:      action: */                  │ |
|  │    /* TCP_ESTABLISHED */ TCP_FIN_WAIT1 | TCP_ACTION_FIN,                │ |
|  │    /* TCP_SYN_SENT    */ TCP_CLOSE,                                     │ |
|  │    /* TCP_SYN_RECV    */ TCP_FIN_WAIT1 | TCP_ACTION_FIN,                │ |
|  │    /* TCP_FIN_WAIT1   */ TCP_FIN_WAIT1,                                 │ |
|  │    /* TCP_FIN_WAIT2   */ TCP_FIN_WAIT2,                                 │ |
|  │    /* TCP_TIME_WAIT   */ TCP_CLOSE,                                     │ |
|  │    /* TCP_CLOSE       */ TCP_CLOSE,                                     │ |
|  │    /* TCP_CLOSE_WAIT  */ TCP_LAST_ACK  | TCP_ACTION_FIN,                │ |
|  │    /* TCP_LAST_ACK    */ TCP_LAST_ACK,                                  │ |
|  │    /* TCP_LISTEN      */ TCP_CLOSE,                                     │ |
|  │    /* TCP_CLOSING     */ TCP_CLOSING,                                   │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  # 查看 tcp_set_state 函数                                               │ |
|  │  grep -A 30 'void tcp_set_state' net/ipv4/tcp.c                        │ |
|  │                                                                          │ |
|  │  验证结论：明确的状态转换表和统一入口                                    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 4: VERIFY NAPI POLL (验证 NAPI 轮询)                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  验证：延迟处理和并发性能                                                │ |
|  │                                                                          │ |
|  │  # 查看 NAPI 注册                                                        │ |
|  │  grep -r 'netif_napi_add' drivers/net/                                  │ |
|  │                                                                          │ |
|  │  Example from e1000e:                                                    │ |
|  │  netif_napi_add(netdev, &adapter->napi, e1000_clean, 64);               │ |
|  │  //                                      ^^^poll    ^^^budget           │ |
|  │                                                                          │ |
|  │  # 查看 NAPI poll 实现                                                   │ |
|  │  grep -A 50 'static int e1000_clean' drivers/net/ethernet/intel/e1000e/ │ |
|  │                                                                          │ |
|  │  static int e1000_clean(struct napi_struct *napi, int budget)           │ |
|  │  {                                                                       │ |
|  │      // Clean RX                                                         │ |
|  │      work_done = e1000_clean_rx_irq(adapter, &adapter->rx_ring,         │ |
|  │                                     &work_done, budget);                 │ |
|  │                                                                          │ |
|  │      // If not enough work, switch back to interrupt mode               │ |
|  │      if (work_done < budget) {                                          │ |
|  │          napi_complete(napi);                                           │ |
|  │          e1000_irq_enable(adapter);                                     │ |
|  │      }                                                                   │ |
|  │      return work_done;                                                   │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  验证结论：批量处理和自适应中断/轮询切换                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 5: TRACE LOCKING (追踪锁使用)                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  验证：并发保护                                                          │ |
|  │                                                                          │ |
|  │  # 搜索 lock_sock 使用                                                   │ |
|  │  grep -n 'lock_sock\|release_sock' net/ipv4/tcp.c                       │ |
|  │                                                                          │ |
|  │  Output:                                                                 │ |
|  │  net/ipv4/tcp.c:287:    lock_sock(sk);                                  │ |
|  │  net/ipv4/tcp.c:340:    release_sock(sk);                               │ |
|  │  net/ipv4/tcp.c:958:    lock_sock(sk);                                  │ |
|  │  net/ipv4/tcp.c:1100:   release_sock(sk);                               │ |
|  │  ...                                                                     │ |
|  │                                                                          │ |
|  │  Pattern: 所有用户上下文操作都在 lock_sock/release_sock 之间            │ |
|  │                                                                          │ |
|  │  验证结论：严格的锁保护模式                                              │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

验证方法：
1. **追踪 sk_buff 流动**：使用 ftrace 观察分层调用链
2. **观察 net_device_ops**：检查驱动接口一致性
3. **检查 TCP 状态机**：查看 `new_state[]` 表和 `tcp_set_state()`
4. **验证 NAPI**：查看驱动的 poll 函数和 budget 使用
5. **追踪锁使用**：搜索 `lock_sock`/`release_sock` 模式

---

## 5. 阅读顺序建议

```
RECOMMENDED READING ORDER
+=============================================================================+
|                                                                              |
|  PHASE 1: UNDERSTAND CORE ABSTRACTIONS (理解核心抽象)                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. include/linux/skbuff.h                                              │ |
|  │     └── struct sk_buff definition                                       │ |
|  │     └── skb_* inline functions                                          │ |
|  │     └── 理解数据包的基本容器                                             │ |
|  │                                                                          │ |
|  │  2. include/linux/netdevice.h                                           │ |
|  │     └── struct net_device definition                                    │ |
|  │     └── struct net_device_ops                                           │ |
|  │     └── 理解设备抽象                                                     │ |
|  │                                                                          │ |
|  │  3. include/net/sock.h                                                  │ |
|  │     └── struct sock definition                                          │ |
|  │     └── socket locking primitives                                       │ |
|  │     └── 理解 socket 核心                                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 2: TRACE DATA FLOW (追踪数据流)                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  4. net/core/dev.c                                                      │ |
|  │     └── netif_receive_skb() - receive path entry                        │ |
|  │     └── dev_queue_xmit() - send path entry                              │ |
|  │     └── 理解设备层如何分发数据包                                         │ |
|  │                                                                          │ |
|  │  5. net/ipv4/ip_input.c + net/ipv4/ip_output.c                          │ |
|  │     └── ip_rcv(), ip_rcv_finish()                                       │ |
|  │     └── ip_queue_xmit(), ip_local_out()                                 │ |
|  │     └── 理解 IP 层处理                                                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 3: UNDERSTAND PROTOCOL IMPLEMENTATION (理解协议实现)                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  6. net/ipv4/tcp.c ★                                                    │ |
|  │     └── tcp_sendmsg(), tcp_recvmsg()                                    │ |
|  │     └── tcp_close(), tcp_disconnect()                                   │ |
|  │     └── tcp_set_state(), new_state[] array                              │ |
|  │     └── 理解 TCP socket 操作和状态机                                     │ |
|  │                                                                          │ |
|  │  7. net/ipv4/tcp_input.c                                                │ |
|  │     └── tcp_rcv_established()                                           │ |
|  │     └── tcp_rcv_state_process()                                         │ |
|  │     └── 理解 TCP 接收处理                                                │ |
|  │                                                                          │ |
|  │  8. net/ipv4/tcp_output.c                                               │ |
|  │     └── tcp_transmit_skb()                                              │ |
|  │     └── tcp_write_xmit()                                                │ |
|  │     └── 理解 TCP 发送处理                                                │ |
|  │                                                                          │ |
|  │  9. net/ipv4/tcp_ipv4.c                                                 │ |
|  │     └── tcp_v4_connect()                                                │ |
|  │     └── tcp_v4_rcv()                                                    │ |
|  │     └── 理解 TCP/IPv4 绑定                                               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 4: UNDERSTAND ASYNC MECHANISMS (理解异步机制)                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  10. net/core/dev.c - NAPI section                                      │ |
|  │      └── napi_schedule(), napi_complete()                               │ |
|  │      └── __napi_poll()                                                  │ |
|  │      └── 理解中断/轮询切换                                               │ |
|  │                                                                          │ |
|  │  11. drivers/net/ethernet/intel/e1000e/                                 │ |
|  │      └── e1000_netdev_ops definition                                    │ |
|  │      └── e1000_clean() - NAPI poll                                      │ |
|  │      └── e1000_xmit_frame()                                             │ |
|  │      └── 理解真实驱动如何使用框架                                        │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  PHASE 5: UNDERSTAND EXTENSION MECHANISMS (理解扩展机制)                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  12. net/ipv4/tcp_cong.c                                                │ |
|  │      └── tcp_register_congestion_control()                              │ |
|  │      └── 理解算法插件机制                                                │ |
|  │                                                                          │ |
|  │  13. net/netfilter/                                                     │ |
|  │      └── nf_hook(), nf_register_hook()                                  │ |
|  │      └── 理解钩子机制                                                    │ |
|  │                                                                          │ |
|  │  14. net/sched/                                                         │ |
|  │      └── register_qdisc()                                               │ |
|  │      └── 理解流量控制扩展                                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  READING TIPS:                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  • 先读头文件了解数据结构，再读 .c 文件了解实现                          │ |
|  │  • 使用 cscope/ctags 快速跳转符号定义                                    │ |
|  │  • 使用 git log -p <file> 了解函数演进历史                               │ |
|  │  • 关注函数开头的注释，通常包含重要设计说明                              │ |
|  │  • 搜索 TODO/FIXME/XXX 了解已知问题                                      │ |
|  │  • 阅读相关 Documentation/networking/ 文档                               │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

推荐阅读顺序（从宏观到细节）：

**第一阶段：理解核心抽象**
1. `include/linux/skbuff.h` - 数据包容器
2. `include/linux/netdevice.h` - 设备抽象
3. `include/net/sock.h` - Socket 核心

**第二阶段：追踪数据流**
4. `net/core/dev.c` - 设备层入口
5. `net/ipv4/ip_input.c` + `ip_output.c` - IP 层

**第三阶段：理解协议实现**
6-9. TCP 相关文件（`tcp.c`, `tcp_input.c`, `tcp_output.c`, `tcp_ipv4.c`）

**第四阶段：理解异步机制**
10-11. NAPI 机制和驱动实现

**第五阶段：理解扩展机制**
12-14. 拥塞控制、Netfilter、流量控制
