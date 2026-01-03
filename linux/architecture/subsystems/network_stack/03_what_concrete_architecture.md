# WHAT｜具体架构

## 1. 模式：流水线与钩子

```
PATTERN 1: PIPELINE
+=============================================================================+
|                                                                              |
|  NETWORK STACK AS PIPELINE                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  RX Pipeline (stages connected by function calls):                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐          │    │ |
|  │  │  │   NIC   │──►│  NAPI   │──►│   GRO   │──►│  L2/L3  │          │    │ |
|  │  │  │  Driver │   │  Poll   │   │         │   │ Dispatch│          │    │ |
|  │  │  └─────────┘   └─────────┘   └─────────┘   └────┬────┘          │    │ |
|  │  │                                                  │               │    │ |
|  │  │         ┌────────────────────────────────────────┤               │    │ |
|  │  │         │                                        │               │    │ |
|  │  │         ▼                                        ▼               │    │ |
|  │  │    ┌─────────┐                              ┌─────────┐          │    │ |
|  │  │    │  ARP    │                              │   IP    │          │    │ |
|  │  │    │ Handler │                              │  Input  │          │    │ |
|  │  │    └─────────┘                              └────┬────┘          │    │ |
|  │  │                                                  │               │    │ |
|  │  │                            ┌─────────────────────┤               │    │ |
|  │  │                            │                     │               │    │ |
|  │  │                            ▼                     ▼               │    │ |
|  │  │                       ┌─────────┐           ┌─────────┐          │    │ |
|  │  │                       │  ICMP   │           │TCP/UDP  │          │    │ |
|  │  │                       │ Handler │           │ Input   │          │    │ |
|  │  │                       └─────────┘           └────┬────┘          │    │ |
|  │  │                                                  │               │    │ |
|  │  │                                                  ▼               │    │ |
|  │  │                                             ┌─────────┐          │    │ |
|  │  │                                             │ Socket  │          │    │ |
|  │  │                                             │  Queue  │          │    │ |
|  │  │                                             └─────────┘          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PIPELINE CHARACTERISTICS:                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. SINGLE PACKET, SINGLE PATH                                   │    │ |
|  │  │     Each packet follows one path through the pipeline            │    │ |
|  │  │     No parallel processing of same packet                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. IN-PLACE TRANSFORMATION                                      │    │ |
|  │  │     sk_buff modified in place (header stripping)                 │    │ |
|  │  │     Minimal copying                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. FAST-PATH OPTIMIZATION                                       │    │ |
|  │  │     Common case: direct function calls                           │    │ |
|  │  │     Exception handling: queue and defer                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. DEMULTIPLEXING AT BRANCH POINTS                              │    │ |
|  │  │     Protocol field → function pointer dispatch                   │    │ |
|  │  │     Hash tables for socket lookup                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式 1：流水线**

RX 流水线：阶段通过函数调用连接
NIC Driver → NAPI Poll → GRO → L2/L3 Dispatch → IP Input → TCP/UDP Input → Socket Queue

流水线特征：
1. **单包单路径**：每个包走一条路径，不并行处理同一包
2. **原地转换**：sk_buff 原地修改（剥离头部），最小复制
3. **快路径优化**：常见情况直接函数调用，异常处理排队延迟
4. **分支点解复用**：协议字段 → 函数指针分发，socket 查找用哈希表

---

```
PATTERN 2: HOOKS (NETFILTER)
+=============================================================================+
|                                                                              |
|  NETFILTER HOOK ARCHITECTURE                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Hooks inserted at strategic points in packet path:                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │            ┌──────────────────────────────────────────┐          │    │ |
|  │  │            │            LOCAL PROCESS                 │          │    │ |
|  │  │            └──────────────────────────────────────────┘          │    │ |
|  │  │                    ▲                      │                      │    │ |
|  │  │                    │                      │                      │    │ |
|  │  │               [INPUT]                [OUTPUT]                    │    │ |
|  │  │                    │                      │                      │    │ |
|  │  │                    │                      ▼                      │    │ |
|  │  │  ┌─────────┐   ┌───┴───┐           ┌──────────┐   ┌─────────┐   │    │ |
|  │  │  │Incoming │──►│Routing│           │ Routing  │──►│Outgoing │   │    │ |
|  │  │  │ Packet  │   │Decision│           │ Decision │   │ Packet  │   │    │ |
|  │  │  └─────────┘   └───┬───┘           └────┬─────┘   └─────────┘   │    │ |
|  │  │       │            │                    │              ▲        │    │ |
|  │  │  [PREROUTING]      │                    │        [POSTROUTING]  │    │ |
|  │  │       │            │                    │              │        │    │ |
|  │  │       ▼            │                    │              │        │    │ |
|  │  │                    └────────────────────┘              │        │    │ |
|  │  │                            │                           │        │    │ |
|  │  │                       [FORWARD]                        │        │    │ |
|  │  │                            │                           │        │    │ |
|  │  │                            └───────────────────────────┘        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  HOOK POINTS (IPv4):                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  NF_INET_PRE_ROUTING   - After sanity checks, before routing    │    │ |
|  │  │  NF_INET_LOCAL_IN      - After routing, destined for local      │    │ |
|  │  │  NF_INET_FORWARD       - After routing, not for local           │    │ |
|  │  │  NF_INET_LOCAL_OUT     - Locally generated, before routing      │    │ |
|  │  │  NF_INET_POST_ROUTING  - After routing, before leaving          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  HOOK IMPLEMENTATION:                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct nf_hook_ops {                                            │    │ |
|  │  │      nf_hookfn       *hook;      // Callback function            │    │ |
|  │  │      struct module   *owner;                                     │    │ |
|  │  │      u_int8_t        pf;         // Protocol family              │    │ |
|  │  │      unsigned int    hooknum;    // Which hook point             │    │ |
|  │  │      int             priority;   // Order of execution           │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Hook return values:                                             │    │ |
|  │  │  NF_ACCEPT  - Continue processing                                │    │ |
|  │  │  NF_DROP    - Drop packet, free sk_buff                          │    │ |
|  │  │  NF_STOLEN  - Handler took ownership, don't free                 │    │ |
|  │  │  NF_QUEUE   - Queue to userspace (NFQUEUE)                       │    │ |
|  │  │  NF_REPEAT  - Call this hook again                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  USE CASES:                                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • iptables/nftables: Firewall rules                             │    │ |
|  │  │  • NAT: Address translation at PREROUTING/POSTROUTING            │    │ |
|  │  │  • Connection tracking: State machine for connections            │    │ |
|  │  │  • Load balancing: IPVS at INPUT hook                            │    │ |
|  │  │  • Traffic accounting: Count bytes/packets                       │    │ |
|  │  │  • Packet mangling: Modify headers (TTL, TOS, etc.)              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式 2：钩子（Netfilter）**

钩子点（IPv4）：
- **NF_INET_PRE_ROUTING**：完整性检查后，路由前
- **NF_INET_LOCAL_IN**：路由后，目标是本地
- **NF_INET_FORWARD**：路由后，不是本地
- **NF_INET_LOCAL_OUT**：本地生成，路由前
- **NF_INET_POST_ROUTING**：路由后，离开前

钩子返回值：
- NF_ACCEPT：继续处理
- NF_DROP：丢弃包
- NF_STOLEN：处理器接管，不释放
- NF_QUEUE：排队到用户空间

用例：iptables 防火墙、NAT 地址转换、连接跟踪、IPVS 负载均衡、流量统计

---

## 2. 核心数据结构

```
CORE DATA STRUCTURES
+=============================================================================+
|                                                                              |
|  STRUCT SK_BUFF (include/linux/skbuff.h)                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct sk_buff {                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Queue linkage (first cache line) */                          │    │ |
|  │  │  struct sk_buff      *next, *prev;                               │    │ |
|  │  │  struct sock         *sk;           // Owner socket              │    │ |
|  │  │  ktime_t             tstamp;        // Timestamp                 │    │ |
|  │  │  struct net_device   *dev;          // Device arrived/leaving on│    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Data pointers (critical for performance) */                  │    │ |
|  │  │  unsigned char       *head;         // Start of buffer           │    │ |
|  │  │  unsigned char       *data;         // Start of data             │    │ |
|  │  │  unsigned int        tail;          // End of data (offset)      │    │ |
|  │  │  unsigned int        end;           // End of buffer (offset)    │    │ |
|  │  │  unsigned int        len;           // Length of data            │    │ |
|  │  │  unsigned int        data_len;      // Length in fragments       │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Protocol headers (offsets from head) */                      │    │ |
|  │  │  __u16               transport_header;  // TCP/UDP header        │    │ |
|  │  │  __u16               network_header;    // IP header             │    │ |
|  │  │  __u16               mac_header;        // Ethernet header       │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Protocol info */                                             │    │ |
|  │  │  __be16              protocol;      // ETH_P_IP, ETH_P_IPV6...   │    │ |
|  │  │  __u8                pkt_type;      // PACKET_HOST, BROADCAST... │    │ |
|  │  │  __u8                ip_summed;     // Checksum status           │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Queue mapping */                                             │    │ |
|  │  │  __u16               queue_mapping; // TX queue                  │    │ |
|  │  │  __u8                cloned;        // Is this a clone?          │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Reference counting */                                        │    │ |
|  │  │  atomic_t            users;         // Reference count           │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Destructor */                                                │    │ |
|  │  │  void (*destructor)(struct sk_buff *skb);                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Control buffer (protocol private) */                         │    │ |
|  │  │  char                cb[48];        // Private protocol data     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  MEMORY LAYOUT:                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────┐    │    │ |
|  │  │  │                    sk_buff struct                        │    │    │ |
|  │  │  │                    (~200 bytes)                          │    │    │ |
|  │  │  └─────────────────────────────────────────────────────────┘    │    │ |
|  │  │                               │                                  │    │ |
|  │  │                               │ head pointer                     │    │ |
|  │  │                               ▼                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────┐    │    │ |
|  │  │  │ headroom │ MAC │ IP │ TCP │   payload    │ tailroom     │    │    │ |
|  │  │  └─────────────────────────────────────────────────────────┘    │    │ |
|  │  │  ▲          ▲     ▲    ▲     ▲              ▲               ▲   │    │ |
|  │  │  │          │     │    │     │              │               │   │    │ |
|  │  │ head    mac_hdr net_hdr trans data         tail           end   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct sk_buff**（include/linux/skbuff.h）

关键字段：
- **队列链接**：next/prev（双向链表）、sk（所属 socket）、dev（设备）
- **数据指针**：head（缓冲区起始）、data（数据起始）、tail（数据结束）、end（缓冲区结束）、len（数据长度）
- **协议头偏移**：transport_header（TCP/UDP）、network_header（IP）、mac_header（Ethernet）
- **协议信息**：protocol（ETH_P_IP 等）、pkt_type（主机/广播）、ip_summed（校验和状态）
- **引用计数**：users（原子引用计数）
- **控制缓冲区**：cb[48]（协议私有数据）

---

```
STRUCT NET_DEVICE (include/linux/netdevice.h)
+=============================================================================+
|                                                                              |
|  struct net_device {                                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  /* Device identity */                                                   │ |
|  │  char                  name[IFNAMSIZ];    // "eth0", "lo", etc.          │ |
|  │  unsigned int          ifindex;           // Unique interface index      │ |
|  │  unsigned int          flags;             // IFF_UP, IFF_RUNNING...      │ |
|  │  unsigned int          mtu;               // Maximum transmission unit   │ |
|  │                                                                          │ |
|  │  /* Hardware address */                                                  │ |
|  │  unsigned char         dev_addr[ETH_ALEN]; // MAC address                │ |
|  │  unsigned char         broadcast[ETH_ALEN];// Broadcast address          │ |
|  │                                                                          │ |
|  │  /* Stats */                                                             │ |
|  │  struct net_device_stats stats;           // RX/TX counters              │ |
|  │                                                                          │ |
|  │  /* OPERATIONS TABLE (key abstraction) */                                │ |
|  │  const struct net_device_ops *netdev_ops; // Driver operations           │ |
|  │  const struct ethtool_ops   *ethtool_ops; // Ethtool interface           │ |
|  │                                                                          │ |
|  │  /* TX queues */                                                         │ |
|  │  unsigned int          num_tx_queues;     // Number of TX queues         │ |
|  │  struct netdev_queue   *_tx;              // TX queue array              │ |
|  │                                                                          │ |
|  │  /* RX handling */                                                       │ |
|  │  unsigned int          num_rx_queues;                                    │ |
|  │  struct netdev_rx_queue *_rx;                                            │ |
|  │                                                                          │ |
|  │  /* Qdisc (traffic control) */                                           │ |
|  │  struct Qdisc          *qdisc;            // Root qdisc                  │ |
|  │                                                                          │ |
|  │  /* NAPI */                                                              │ |
|  │  struct list_head      napi_list;         // NAPI contexts               │ |
|  │                                                                          │ |
|  │  /* Features and offloads */                                             │ |
|  │  netdev_features_t     features;          // NETIF_F_TSO, CSUM...        │ |
|  │  netdev_features_t     hw_features;       // Hardware capabilities       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|  };                                                                          |
|                                                                              |
|  struct net_device_ops (driver implements this):                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  const struct net_device_ops my_driver_ops = {                           │ |
|  │      .ndo_open            = my_open,         // ifconfig up              │ |
|  │      .ndo_stop            = my_stop,         // ifconfig down            │ |
|  │      .ndo_start_xmit      = my_xmit,         // Transmit packet          │ |
|  │      .ndo_get_stats64     = my_get_stats,    // Get statistics           │ |
|  │      .ndo_set_mac_address = my_set_mac,      // Change MAC               │ |
|  │      .ndo_change_mtu      = my_change_mtu,   // Change MTU               │ |
|  │      .ndo_set_rx_mode     = my_set_rx_mode,  // Multicast/promisc        │ |
|  │      .ndo_validate_addr   = eth_validate_addr,                           │ |
|  │  };                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct net_device**（include/linux/netdevice.h）

关键字段：
- **设备身份**：name（"eth0"）、ifindex（唯一索引）、flags（IFF_UP 等）、mtu
- **硬件地址**：dev_addr（MAC 地址）、broadcast（广播地址）
- **统计**：stats（RX/TX 计数器）
- **操作表**（关键抽象）：netdev_ops（驱动操作）、ethtool_ops（ethtool 接口）
- **TX 队列**：num_tx_queues、_tx（队列数组）
- **RX 处理**：num_rx_queues、_rx、napi_list
- **Qdisc**：qdisc（流量控制）
- **特性**：features（NETIF_F_TSO、CSUM 等）

驱动实现 `net_device_ops`：ndo_open、ndo_stop、ndo_start_xmit 等

---

## 3. 控制流：RX/TX 路径

```
CONTROL FLOW: RX PATH
+=============================================================================+
|                                                                              |
|  DETAILED RX CONTROL FLOW                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  [Hardware IRQ] ◄── NIC signals packet arrival                   │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  driver_irq_handler()                                            │    │ |
|  │  │        │ napi_schedule(&napi)                                    │    │ |
|  │  │        │ • Disable NIC IRQ                                       │    │ |
|  │  │        │ • Set NAPI_STATE_SCHED                                  │    │ |
|  │  │        │ • Raise NET_RX_SOFTIRQ                                  │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  [Return from IRQ]                                               │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  [NET_RX_SOFTIRQ runs]                                           │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  net_rx_action() ◄── Softirq handler                             │    │ |
|  │  │        │                                                         │    │ |
|  │  │        │ for each scheduled napi:                                │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  napi_poll(&napi, budget)                                        │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  driver->napi_poll() ◄── Driver-specific poll                    │    │ |
|  │  │        │                                                         │    │ |
|  │  │        │ for each packet in ring buffer:                         │    │ |
|  │  │        │   skb = build_skb(data)                                 │    │ |
|  │  │        │   napi_gro_receive(&napi, skb)                          │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  napi_gro_receive()                                              │    │ |
|  │  │        │ • Try to merge with existing flow                       │    │ |
|  │  │        │ • If merged, return                                     │    │ |
|  │  │        │ • If not, flush and call netif_receive_skb              │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  netif_receive_skb(skb) ◄── Core RX entry point                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  __netif_receive_skb_core()                                      │    │ |
|  │  │        │ • Generic XDP                                           │    │ |
|  │  │        │ • Packet taps (tcpdump)                                 │    │ |
|  │  │        │ • Bridge check                                          │    │ |
|  │  │        │ • Protocol handler dispatch                             │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ptype->func(skb) ◄── e.g., ip_rcv for ETH_P_IP                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ip_rcv(skb, dev, ptype, orig_dev)                               │    │ |
|  │  │        │ • NF_INET_PRE_ROUTING hook                              │    │ |
|  │  │        │ • Route lookup                                          │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ip_local_deliver() [if dst is local]                            │    │ |
|  │  │        │ • NF_INET_LOCAL_IN hook                                 │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  tcp_v4_rcv() [if IPPROTO_TCP]                                   │    │ |
|  │  │        │ • Socket lookup                                         │    │ |
|  │  │        │ • tcp_v4_do_rcv()                                       │    │ |
|  │  │        │ • tcp_rcv_established() [fast path]                     │    │ |
|  │  │        │ • Add to sk->sk_receive_queue                           │    │ |
|  │  │        │ • Wake up sk->sk_wq                                     │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  [User recv() returns]                                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**RX 控制流**：

1. **硬件 IRQ**：NIC 信号包到达
2. **driver_irq_handler()**：napi_schedule()，禁用 NIC IRQ，触发 NET_RX_SOFTIRQ
3. **net_rx_action()**：softirq 处理器
4. **napi_poll()**：调用驱动的 poll 函数
5. **driver->napi_poll()**：从环形缓冲区取包，调用 napi_gro_receive()
6. **napi_gro_receive()**：尝试合并流，否则调用 netif_receive_skb()
7. **netif_receive_skb()**：核心 RX 入口点
8. **__netif_receive_skb_core()**：XDP、包 tap、桥接检查、协议处理器分发
9. **ip_rcv()**：PRE_ROUTING 钩子，路由查找
10. **ip_local_deliver()**：LOCAL_IN 钩子
11. **tcp_v4_rcv()**：socket 查找，添加到接收队列，唤醒等待者

---

```
CONTROL FLOW: TX PATH
+=============================================================================+
|                                                                              |
|  DETAILED TX CONTROL FLOW                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  [User send()/write()] ◄── Application sends data                │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  sys_sendto() / sys_write()                                      │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  sock_sendmsg()                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  tcp_sendmsg() ◄── Protocol-specific send                        │    │ |
|  │  │        │ • Copy data from user to kernel                         │    │ |
|  │  │        │ • Build sk_buff with payload                            │    │ |
|  │  │        │ • tcp_push() to start sending                           │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  tcp_write_xmit()                                                │    │ |
|  │  │        │ • Congestion control check                              │    │ |
|  │  │        │ • Flow control (window)                                 │    │ |
|  │  │        │ • Segmentation (MSS)                                    │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  tcp_transmit_skb()                                              │    │ |
|  │  │        │ • Add TCP header                                        │    │ |
|  │  │        │ • Calculate checksum (or defer to HW)                   │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ip_queue_xmit()                                                 │    │ |
|  │  │        │ • Route lookup (if not cached)                          │    │ |
|  │  │        │ • NF_INET_LOCAL_OUT hook                                │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ip_output()                                                     │    │ |
|  │  │        │ • Add IP header                                         │    │ |
|  │  │        │ • NF_INET_POST_ROUTING hook                             │    │ |
|  │  │        │ • Fragment if needed                                    │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ip_finish_output()                                              │    │ |
|  │  │        │ • Neighbor lookup (ARP)                                 │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  dev_queue_xmit()                                                │    │ |
|  │  │        │ • Select TX queue                                       │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  __dev_queue_xmit()                                              │    │ |
|  │  │        │ • Qdisc enqueue (traffic shaping)                       │    │ |
|  │  │        │ • Try direct xmit if qdisc allows                       │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  dev_hard_start_xmit()                                           │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ndo_start_xmit() ◄── Driver transmit                            │    │ |
|  │  │        │ • Add MAC header                                        │    │ |
|  │  │        │ • DMA map sk_buff data                                  │    │ |
|  │  │        │ • Write to NIC TX ring                                  │    │ |
|  │  │        │ • Ring doorbell                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  [Hardware sends frame]                                          │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  [TX completion IRQ]                                             │    │ |
|  │  │        │ • Free sk_buff                                          │    │ |
|  │  │        │ • Update stats                                          │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  [Done]                                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**TX 控制流**：

1. **User send()**：应用发送数据
2. **sys_sendto()** → **sock_sendmsg()**
3. **tcp_sendmsg()**：从用户复制数据，构建 sk_buff
4. **tcp_write_xmit()**：拥塞控制、流控、分段
5. **tcp_transmit_skb()**：添加 TCP 头，计算校验和
6. **ip_queue_xmit()**：路由查找，LOCAL_OUT 钩子
7. **ip_output()**：添加 IP 头，POST_ROUTING 钩子，分片
8. **ip_finish_output()**：邻居查找（ARP）
9. **dev_queue_xmit()**：选择 TX 队列
10. **__dev_queue_xmit()**：Qdisc 入队（流量整形）
11. **dev_hard_start_xmit()** → **ndo_start_xmit()**：添加 MAC 头，DMA 映射，写 NIC TX ring
12. **TX 完成 IRQ**：释放 sk_buff

---

## 4. 扩展点：Netfilter

```
EXTENSION POINTS: NETFILTER
+=============================================================================+
|                                                                              |
|  HOW TO REGISTER A NETFILTER HOOK                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  static unsigned int my_hook_fn(void *priv,                              │ |
|  │                                  struct sk_buff *skb,                    │ |
|  │                                  const struct nf_hook_state *state)      │ |
|  │  {                                                                       │ |
|  │      struct iphdr *iph = ip_hdr(skb);                                    │ |
|  │                                                                          │ |
|  │      // Inspect or modify packet                                         │ |
|  │      if (should_drop(iph))                                               │ |
|  │          return NF_DROP;                                                 │ |
|  │                                                                          │ |
|  │      return NF_ACCEPT;                                                   │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  static struct nf_hook_ops my_hook_ops = {                               │ |
|  │      .hook     = my_hook_fn,                                             │ |
|  │      .pf       = NFPROTO_IPV4,                                           │ |
|  │      .hooknum  = NF_INET_PRE_ROUTING,                                    │ |
|  │      .priority = NF_IP_PRI_FIRST,                                        │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  static int __init my_init(void)                                         │ |
|  │  {                                                                       │ |
|  │      return nf_register_net_hook(&init_net, &my_hook_ops);               │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  static void __exit my_exit(void)                                        │ |
|  │  {                                                                       │ |
|  │      nf_unregister_net_hook(&init_net, &my_hook_ops);                    │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**注册 Netfilter 钩子**：

1. 定义钩子函数：接收 sk_buff，返回 NF_DROP/NF_ACCEPT
2. 填充 `nf_hook_ops`：hook 函数、协议族、钩子点、优先级
3. 调用 `nf_register_net_hook()` 注册
4. 模块退出时调用 `nf_unregister_net_hook()` 注销

---

## 5. 代价：缓存未命中与内存压力

```
COSTS: CACHE MISSES AND MEMORY PRESSURE
+=============================================================================+
|                                                                              |
|  COST 1: CACHE MISSES                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Per-packet cache access pattern:                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  COLD DATA (cache miss likely):                                  │    │ |
|  │  │  • sk_buff struct (~200 bytes, spans 3-4 cache lines)            │    │ |
|  │  │  • Packet data (64-1500+ bytes)                                  │    │ |
|  │  │  • Socket struct (if not recently used)                          │    │ |
|  │  │  • Routing cache entry                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  IMPACT:                                                         │    │ |
|  │  │  • L3 cache miss: ~40 cycles                                     │    │ |
|  │  │  • DRAM access: ~200 cycles                                      │    │ |
|  │  │  • At 14.88 Mpps, that's ~3 billion cache misses/sec possible    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  MITIGATIONS:                                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. PREFETCHING                                                  │    │ |
|  │  │     • prefetch(skb->data) before processing                      │    │ |
|  │  │     • Driver prefetches next descriptor                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. CACHE-LINE ALIGNED STRUCTURES                                │    │ |
|  │  │     • Hot fields grouped in first cache line of sk_buff          │    │ |
|  │  │     • ____cacheline_aligned_in_smp macros                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. BATCH PROCESSING                                             │    │ |
|  │  │     • NAPI processes multiple packets                            │    │ |
|  │  │     • Amortizes cache warmup over batch                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. PER-CPU CACHES                                               │    │ |
|  │  │     • sk_buff allocation from per-CPU cache                      │    │ |
|  │  │     • Reduces cache-line bouncing                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  COST 2: MEMORY PRESSURE                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Memory usage scenarios:                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  NORMAL OPERATION:                                               │    │ |
|  │  │  • 10K connections × ~8KB socket buffer = 80 MB                  │    │ |
|  │  │  • Ring buffers: 256 entries × 2KB × 4 queues = 2 MB per NIC     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ATTACK SCENARIO (SYN flood):                                    │    │ |
|  │  │  • Attacker sends millions of SYN packets                        │    │ |
|  │  │  • Each needs memory for connection tracking                     │    │ |
|  │  │  • Can exhaust memory quickly                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  SLOW RECEIVER:                                                  │    │ |
|  │  │  • Application doesn't call recv() fast enough                   │    │ |
|  │  │  • Socket buffers fill up                                        │    │ |
|  │  │  • Backpressure to TCP (zero window)                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  MITIGATIONS:                                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. SOCKET BUFFER LIMITS                                         │    │ |
|  │  │     • net.core.rmem_max, net.core.wmem_max                       │    │ |
|  │  │     • Per-socket: SO_RCVBUF, SO_SNDBUF                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. MEMORY ACCOUNTING                                            │    │ |
|  │  │     • sk->sk_wmem_alloc, sk->sk_rmem_alloc                       │    │ |
|  │  │     • Block allocation when over limit                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. SYN COOKIES                                                  │    │ |
|  │  │     • Don't allocate memory for SYN_RECV sockets                 │    │ |
|  │  │     • Reconstruct state from ACK                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. GRO/LRO                                                      │    │ |
|  │  │     • Merge packets → fewer sk_buffs                             │    │ |
|  │  │     • Reduces per-packet memory overhead                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  5. DROP ON PRESSURE                                             │    │ |
|  │  │     • If memory critically low, drop incoming packets            │    │ |
|  │  │     • net.ipv4.tcp_mem thresholds                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**代价 1：缓存未命中**

冷数据（可能缓存未命中）：
- sk_buff 结构（~200 字节，跨 3-4 个缓存行）
- 包数据（64-1500+ 字节）
- Socket 结构（如果最近未使用）
- 路由缓存条目

影响：L3 缓存未命中 ~40 周期，DRAM 访问 ~200 周期

缓解措施：
1. **预取**：`prefetch(skb->data)`
2. **缓存行对齐结构**：热字段分组到第一个缓存行
3. **批处理**：NAPI 处理多个包，摊销缓存预热
4. **Per-CPU 缓存**：sk_buff 从 per-CPU 缓存分配

**代价 2：内存压力**

场景：
- 正常运行：10K 连接 × 8KB socket 缓冲 = 80 MB
- 攻击场景（SYN flood）：可快速耗尽内存
- 慢接收者：socket 缓冲填满

缓解措施：
1. **Socket 缓冲限制**：rmem_max/wmem_max
2. **内存统计**：sk->sk_wmem_alloc，超限时阻塞分配
3. **SYN Cookies**：不为 SYN_RECV 分配内存
4. **GRO/LRO**：合并包减少 sk_buff 数量
5. **压力时丢弃**：内存紧张时丢弃入站包
