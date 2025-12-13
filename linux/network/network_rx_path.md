# Linux Network Receive (RX) Path - Complete Call Chain

## 1. Overview ASCII Diagram

```
+=============================================================================+
||                    LINUX NETWORK RECEIVE PATH                             ||
||                   (Wire to Socket - Complete Stack)                       ||
+=============================================================================+

+-----------------------------------------------------------------------------+
|                          HARDWARE (NIC)                                     |
+-----------------------------------------------------------------------------+
|                                                                             |
|   +------------------------------+                                          |
|   |        NETWORK WIRE          |  <-- 网络线路接收数据                    |
|   +-------+----------------------+                                          |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | PHY / MAC Layer              |  <-- 物理层/MAC层                        |
|   +-------+----------------------+     解调、解码                           |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | NIC RX Processing            |  <-- 网卡接收处理                        |
|   +------------------------------+                                          |
|   | - Validate Ethernet FCS      |     验证以太网帧校验序列                 |
|   | - RSS (Receive Side Scaling) |     接收端缩放，多队列分发               |
|   | - Checksum offload           |     硬件校验和验证                       |
|   | - VLAN tag extraction        |     VLAN标签提取                         |
|   +-------+----------------------+                                          |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | NIC DMA Engine               |  <-- 网卡DMA引擎                         |
|   +-------+----------------------+     写入预分配的环形缓冲区               |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | RX Ring Buffer               |  <-- 接收环形缓冲区                      |
|   +-------+----------------------+     存放接收到的数据包                   |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | Generate Hardware Interrupt  |  <-- 产生硬件中断                        |
|   +-------+----------------------+     或使用中断合并                       |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                     DEVICE DRIVER LAYER                                     |
|                    (drivers/net/ethernet/...)                               |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+-----------------------+                                         |
|   | Interrupt Handler             |  <-- 硬件中断处理程序                   |
|   | (e.g., e1000_intr,            |     快速响应，最小化中断处理时间        |
|   |  ixgbe_msix_clean_rings)      |                                         |
|   +-------+-----------------------+                                         |
|           |                                                                 |
|           v                                                                 |
|   +-------+-----------------------+                                         |
|   | napi_schedule()               |  <-- 调度NAPI轮询                       |
|   +-------+-----------------------+     将设备加入轮询列表                  |
|           |                        设置软中断标志                           |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                       NAPI (New API) LAYER                                  |
|                      (net/core/dev.c)                                       |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           | (软中断上下文 NET_RX_SOFTIRQ)                                   |
|           v                                                                 |
|   +-------+-----------------------+                                         |
|   | net_rx_action()               |  <-- 网络接收软中断处理                 |
|   +-------+-----------------------+     遍历轮询列表                        |
|           |                                                                 |
|           v                                                                 |
|   +-------+-----------------------+                                         |
|   | napi_poll()                   |  <-- NAPI轮询调度器                     |
|   +-------+-----------------------+                                         |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------------+                                            |
|   | Driver's poll function     |  <-- 驱动轮询函数                          |
|   | (e.g., e1000_clean,        |     从环形缓冲区读取数据包                 |
|   |  ixgbe_clean_rx_irq)       |     构建sk_buff                            |
|   +-------+--------------------+                                            |
|           |                                                                 |
|           | For each received packet:                                       |
|           |   - Allocate/reuse sk_buff                                      |
|           |   - Copy data or setup DMA mapping                              |
|           |   - Set protocol, checksum status                               |
|           |   - Call napi_gro_receive() or netif_receive_skb()              |
|           v                                                                 |
|   +-------+-----------------------+                                         |
|   | napi_gro_receive()            |  <-- GRO接收入口                        |
|   +-------+-----------------------+     尝试聚合小包                        |
|           |                                                                 |
|           v                                                                 |
|   +-------+-----------------------+                                         |
|   | dev_gro_receive()             |  <-- GRO处理                            |
|   +-------+-----------------------+     按协议类型聚合                      |
|           |                                                                 |
|           +-------------------+                                             |
|           |                   |                                             |
|           v                   v                                             |
|   [Packet aggregated]   [Flush to stack]                                    |
|           |                   |                                             |
|           |                   v                                             |
|           |          +--------+--------------+                              |
|           +--------->| napi_skb_finish()     |                              |
|                      +--------+--------------+                              |
|                               |                                             |
|                               v                                             |
|                      +--------+--------------+                              |
|                      | netif_receive_skb()   |  <-- 数据包接收主入口        |
|                      +--------+--------------+     进入协议栈               |
|                               |                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                    GENERIC RECEIVE LAYER                                    |
|                   (net/core/dev.c)                                          |
+-----------------------------------------------------------------------------+
|                               |                                             |
|                               v                                             |
|                      +--------+--------------+                              |
|                      | netif_receive_skb()   |  <-- 通用接收入口            |
|                      +--------+--------------+                              |
|                               |                                             |
|                               v                                             |
|                      +--------+------------------+                          |
|                      | __netif_receive_skb()     |  <-- 内部实现            |
|                      +--------+------------------+     时间戳、RPS处理      |
|                               |                                             |
|                               v                                             |
|                      +--------+------------------+                          |
|                      | __netif_receive_skb_core()|  <-- 核心分发逻辑        |
|                      +--------+------------------+                          |
|                               |                                             |
|   +---------------------------+----------------------------+                |
|   |                           |                            |                |
|   v                           v                            v                |
| [ptype_all]             [rx_handler]                [ptype_base]            |
| (AF_PACKET,             (bridge,                    (协议处理)              |
|  tcpdump)               bonding, etc.)                                      |
|   |                           |                            |                |
|   v                           v                            v                |
| deliver_skb()           skb->dev->rx_handler()      deliver_skb()           |
| (原始包抓取)            (如br_handle_frame)         (协议栈)                |
|                               |                            |                |
|                               v                            |                |
|                      [May modify skb->dev]                 |                |
|                      [Re-enter __netif_receive_skb]        |                |
|                               |                            |                |
|                               +----------------------------+                |
|                               |                                             |
|                               v                                             |
|                      +--------+------------------+                          |
|                      | Protocol Handler          |  <-- 协议处理函数        |
|                      | (based on skb->protocol)  |     根据以太网类型分发   |
|                      +--------+------------------+                          |
|                               |                                             |
|           +-------------------+-------------------+                         |
|           |                   |                   |                         |
|           v                   v                   v                         |
|     [ETH_P_IP]          [ETH_P_ARP]        [ETH_P_IPV6]                     |
|     ip_rcv()            arp_rcv()          ipv6_rcv()                       |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                         IP LAYER (Network Layer)                            |
|                        (net/ipv4/ip_input.c)                                |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | ip_rcv()           |  <-- IP接收主函数                                  |
|   +-------+------------+     基本验证：版本、校验和、长度                   |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+    +---------------------------+                 |
|   | NF_HOOK(NF_INET_     |--->| Netfilter PREROUTING Hook |                 |
|   |   PRE_ROUTING)       |    | (iptables PREROUTING链)   |                 |
|   +-------+--------------+    | DNAT在此处理              |                 |
|           |                   +---------------------------+                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | ip_rcv_finish()    |  <-- 完成IP接收处理                                |
|   +-------+------------+     路由查找                                       |
|           |                  处理IP选项                                     |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | ip_route_input()   |  <-- 路由输入决策                                  |
|   +-------+------------+     确定本地/转发/丢弃                             |
|           |                                                                 |
|           +-------------------+                                             |
|           |                   |                                             |
|           v                   v                                             |
|   [本地递送]            [转发]                                              |
|   ip_local_deliver()   ip_forward()                                         |
|           |                   |                                             |
|           |                   v                                             |
|           |            +------+-------+                                     |
|           |            | NF_HOOK(     |                                     |
|           |            |  FORWARD)    |                                     |
|           |            +------+-------+                                     |
|           |                   |                                             |
|           |                   v                                             |
|           |            ip_forward_finish()                                  |
|           |                   |                                             |
|           |                   v                                             |
|           |            dst_output() --> (TX Path)                           |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------+                                                |
|   | ip_local_deliver()     |  <-- 本地递送                                  |
|   +-------+----------------+     处理IP分片重组                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+    +-----------------------+                     |
|   | NF_HOOK(NF_INET_     |--->| Netfilter INPUT Hook  |                     |
|   |   LOCAL_IN)          |    | (iptables INPUT链)    |                     |
|   +-------+--------------+    +-----------------------+                     |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | ip_local_deliver_finish()|  <-- 完成本地递送                            |
|   +-------+------------------+     分发到传输层                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | ipprot->handler()        |  <-- 调用传输层处理函数                      |
|   +--------------------------+     tcp_v4_rcv / udp_rcv / icmp_rcv          |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                       TRANSPORT LAYER (TCP)                                 |
|                      (net/ipv4/tcp_ipv4.c, tcp_input.c)                     |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | tcp_v4_rcv()       |  <-- TCP接收入口（IPv4）                           |
|   +-------+------------+     校验和验证                                     |
|           |                  查找对应socket                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | __inet_lookup_skb()      |  <-- Socket查找                              |
|   +-------+------------------+     根据四元组查找                           |
|           |                        (src_ip, dst_ip, src_port, dst_port)     |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | (Socket found?)          |                                              |
|   +-------+------------------+                                              |
|           |                                                                 |
|           +-------------------+                                             |
|           |                   |                                             |
|           v                   v                                             |
|   [sock_owned_by_user?]  [Socket被用户态占用]                               |
|           |                   |                                             |
|           v                   v                                             |
|   tcp_v4_do_rcv()       sk_add_backlog()  <-- 加入backlog队列               |
|           |                   |               稍后处理                      |
|           |                   |                                             |
|           v                   |                                             |
|   +-------+------------------+|                                             |
|   | (TCP State Machine)      ||                                             |
|   +-------+------------------+|                                             |
|           |                   |                                             |
|           +-------------------+                                             |
|           |                                                                 |
|   +-------+------+--------+-------+                                         |
|   |              |                |                                         |
|   v              v                v                                         |
| [ESTABLISHED]  [LISTEN]       [Other States]                                |
|   |              |                |                                         |
|   v              v                v                                         |
| tcp_rcv_       tcp_v4_         tcp_rcv_                                     |
| established()  hnd_req()       state_process()                              |
|   |              |                |                                         |
|   |              v                |                                         |
|   |        tcp_child_process()   |                                         |
|   |        (新连接处理)           |                                         |
|   |              |                |                                         |
|   +-------+------+--------+-------+                                         |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | TCP Data Processing      |  <-- TCP数据处理                             |
|   +-------+------------------+                                              |
|   | - Sequence number check  |     序列号检查                               |
|   | - ACK processing         |     确认处理                                 |
|   | - Window update          |     窗口更新                                 |
|   | - Out-of-order handling  |     乱序处理                                 |
|   | - SACK processing        |     选择性确认处理                           |
|   +-------+------------------+                                              |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | tcp_queue_rcv()          |  <-- 加入接收队列                            |
|   +-------+------------------+     或预队列                                 |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | sk->sk_data_ready()      |  <-- 通知应用层数据就绪                      |
|   +-------+------------------+     唤醒等待的进程                           |
|           |                        (sock_def_readable)                      |
|                                                                             |
+-----------------------------------------------------------------------------+
|                       TRANSPORT LAYER (UDP)                                 |
|                      (net/ipv4/udp.c)                                       |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | udp_rcv()          |  <-- UDP接收入口                                   |
|   +-------+------------+                                                    |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | __udp4_lib_rcv()   |  <-- UDP接收实现                                   |
|   +-------+------------+     校验和验证                                     |
|           |                  Socket查找                                     |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | udp_queue_rcv_skb()      |  <-- 队列接收                                |
|   +-------+------------------+     Socket过滤                               |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | sock_queue_rcv_skb()     |  <-- 加入接收队列                            |
|   +-------+------------------+                                              |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | sk->sk_data_ready()      |  <-- 通知应用层                              |
|   +--------------------------+                                              |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                         SOCKET LAYER                                        |
|                        (net/socket.c)                                       |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | sk_receive_queue         |  <-- Socket接收队列                          |
|   +-------+------------------+     数据等待被读取                           |
|           |                                                                 |
|           | (用户调用read/recv/recvmsg)                                     |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | sock_recvmsg()           |  <-- Socket接收消息                          |
|   +-------+------------------+     安全检查                                 |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | __sock_recvmsg()         |  <-- 内部实现                                |
|   +-------+------------------+                                              |
|           |                                                                 |
|           v                                                                 |
|   +-------+-------------------+                                             |
|   | sock->ops->recvmsg()      |  <-- 协议特定recvmsg                        |
|   +---------------------------+     inet_recvmsg                            |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | sk->sk_prot->recvmsg()   |  <-- 传输层recvmsg                           |
|   +-------+------------------+     tcp_recvmsg / udp_recvmsg                |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                    PROTOCOL SPECIFIC RECEIVE                                |
|                   (net/ipv4/tcp.c, udp.c)                                   |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+  (TCP Path)                                  |
|   | tcp_recvmsg()            |  <-- TCP接收消息                             |
|   +-------+------------------+                                              |
|   | - Wait for data          |     等待数据                                 |
|   | - Copy from receive queue|     从接收队列复制                           |
|   | - Handle urgent data     |     处理紧急数据                             |
|   | - Update receive window  |     更新接收窗口                             |
|   +-------+------------------+                                              |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | skb_copy_datagram_iovec()|  <-- 复制数据到用户空间                      |
|   +-------+------------------+     或使用零拷贝                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+  (UDP Path)                                  |
|   | udp_recvmsg()            |  <-- UDP接收消息                             |
|   +-------+------------------+                                              |
|   | - Dequeue from sk_receive|     从接收队列取出                           |
|   |   _queue                 |                                              |
|   | - Copy to user           |     复制到用户空间                           |
|   | - Return sender info     |     返回发送者信息                           |
|   +-------+------------------+                                              |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                        SYSTEM CALL LAYER                                    |
|                       (net/socket.c)                                        |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | sys_recvmsg()            |  <-- 系统调用返回                            |
|   +-------+------------------+                                              |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------------+                                              |
|   | copy_to_user()           |  <-- 复制数据到用户空间                      |
|   +--------------------------+                                              |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                           USER SPACE                                        |
+-----------------------------------------------------------------------------+
|                                                                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+-----------------+                                               |
|   | Application             |                                               |
|   +-----------------+-------+                                               |
|   | read()          |  <-- 读取文件描述符                                   |
|   | recv()          |  <-- 接收数据（无标志）                               |
|   | recvfrom()      |  <-- 接收数据及发送者地址（UDP常用）                  |
|   | recvmsg()       |  <-- 接收消息（支持辅助数据）                         |
|   +-----------------+                                                       |
|                                                                             |
+=============================================================================+
```

---

## 2. Key Source Files

| Layer | File | Description |
|-------|------|-------------|
| **Driver** | `drivers/net/ethernet/...` | 各网卡驱动 |
| **Core Dev** | `net/core/dev.c` | netif_receive_skb(), NAPI |
| **IP Input** | `net/ipv4/ip_input.c` | ip_rcv(), ip_local_deliver() |
| **TCP Input** | `net/ipv4/tcp_input.c` | tcp_rcv_established() |
| **TCP IPv4** | `net/ipv4/tcp_ipv4.c` | tcp_v4_rcv() |
| **UDP** | `net/ipv4/udp.c` | udp_rcv() |
| **Socket** | `net/socket.c` | sock_recvmsg() |
| **Neighbour** | `net/core/neighbour.c` | ARP处理 |

---

## 3. Key Data Structures

### 3.1 struct napi_struct

```c
/* 位置: include/linux/netdevice.h */
/* NAPI轮询结构 */

struct napi_struct {
    struct list_head    poll_list;      /* 轮询列表链接 */
    
    unsigned long       state;          /* NAPI状态 */
    int                 weight;         /* 每次轮询处理的最大包数 */
    
    int                 (*poll)(struct napi_struct *, int);  /* 轮询函数 */
    
    struct net_device   *dev;           /* 关联的网络设备 */
    struct sk_buff      *gro_list;      /* GRO聚合列表 */
    struct sk_buff      *skb;           /* 当前处理的SKB */
    /* ... */
};
```

**中文说明：** NAPI（New API）是Linux网络栈的核心机制，它结合了中断和轮询的优点。在高负载时使用轮询减少中断开销，低负载时使用中断减少延迟。每个支持NAPI的网卡驱动都会注册一个poll函数。

### 3.2 struct softnet_data

```c
/* 位置: include/linux/netdevice.h */
/* 每CPU的网络处理数据 */

struct softnet_data {
    struct Qdisc        *output_queue;      /* 输出队列 */
    struct Qdisc        **output_queue_tailp;
    
    struct list_head    poll_list;          /* NAPI轮询列表 */
    
    struct sk_buff      *completion_queue;  /* 完成队列 */
    struct sk_buff_head process_queue;      /* 处理队列 */
    
    /* 接收处理 */
    struct sk_buff_head input_pkt_queue;    /* 输入包队列 */
    struct napi_struct  backlog;            /* 后备NAPI */
    
    /* 统计 */
    unsigned int        processed;          /* 已处理包数 */
    unsigned int        time_squeeze;       /* 时间片耗尽次数 */
    unsigned int        cpu_collision;      /* CPU冲突次数 */
    unsigned int        received_rps;       /* RPS接收包数 */
    unsigned int        dropped;            /* 丢弃包数 */
    /* ... */
};

DECLARE_PER_CPU_ALIGNED(struct softnet_data, softnet_data);
```

**中文说明：** `softnet_data`是每个CPU私有的网络处理数据结构，包含NAPI轮询列表、输入队列等。这种设计避免了多CPU之间的锁竞争，提高了网络处理的并行性。

### 3.3 struct packet_type

```c
/* 位置: include/linux/netdevice.h */
/* 协议类型处理器 */

struct packet_type {
    __be16              type;           /* 以太网类型（ETH_P_IP等） */
    struct net_device   *dev;           /* 关联设备（NULL=所有） */
    
    /* 协议处理函数 */
    int (*func)(struct sk_buff *skb,
                struct net_device *dev,
                struct packet_type *pt,
                struct net_device *orig_dev);
    
    struct sk_buff *(*gro_receive)(struct list_head *head,
                                   struct sk_buff *skb);
    int (*gro_complete)(struct sk_buff *skb, int nhoff);
    
    struct list_head    list;           /* 链表指针 */
};
```

**中文说明：** `packet_type`定义了协议处理器，根据以太网帧的类型字段（如0x0800表示IP）调用相应的处理函数。IP协议在启动时注册`ip_rcv`作为处理函数。

### 3.4 struct inet_hashinfo

```c
/* 位置: include/net/inet_hashtables.h */
/* TCP/UDP连接哈希表 */

struct inet_hashinfo {
    /* 已建立连接的哈希表 */
    struct inet_ehash_bucket *ehash;
    spinlock_t               *ehash_locks;
    unsigned int             ehash_mask;
    unsigned int             ehash_locks_mask;
    
    /* 监听socket的哈希表 */
    struct inet_listen_hashbucket *listening_hash;
    
    /* 绑定端口的哈希表 */
    struct inet_bind_hashbucket *bhash;
    unsigned int                bhash_size;
    /* ... */
};
```

**中文说明：** `inet_hashinfo`维护TCP连接的哈希表，用于快速查找对应的socket。接收数据包时，根据四元组（源IP、目的IP、源端口、目的端口）在哈希表中查找socket。

### 3.5 struct sk_buff (接收相关字段)

```c
/* 接收路径相关的sk_buff字段 */
struct sk_buff {
    /* ... 基本字段 ... */
    
    /* 接收时设置的字段 */
    __u8    pkt_type;       /* PACKET_HOST, PACKET_BROADCAST等 */
    __u8    ip_summed;      /* 校验和状态 */
                            /* CHECKSUM_NONE: 未校验 */
                            /* CHECKSUM_UNNECESSARY: 硬件已校验 */
                            /* CHECKSUM_COMPLETE: 硬件提供了校验和 */
    
    __be16  protocol;       /* 以太网协议类型 */
    
    /* 时间戳 */
    ktime_t tstamp;         /* 接收时间戳 */
    
    /* 哈希值（用于RPS/RFS） */
    __u32   hash;
    __u32   rxhash;
    
    /* VLAN信息 */
    __u16   vlan_tci;
    /* ... */
};
```

---

## 4. Key Function Descriptions

| Function | Layer | Description |
|----------|-------|-------------|
| `napi_schedule()` | Driver | 调度NAPI轮询，设置软中断 |
| `net_rx_action()` | Softirq | 网络接收软中断处理函数 |
| `napi_poll()` | NAPI | 调用驱动的poll函数 |
| `napi_gro_receive()` | NAPI | GRO接收入口，聚合小包 |
| `netif_receive_skb()` | Core | 通用接收入口，分发到协议栈 |
| `__netif_receive_skb_core()` | Core | 核心分发逻辑 |
| `ip_rcv()` | Network | IP接收主函数，基本验证 |
| `ip_rcv_finish()` | Network | 完成IP接收，路由查找 |
| `ip_local_deliver()` | Network | 本地递送，处理分片 |
| `ip_local_deliver_finish()` | Network | 分发到传输层 |
| `tcp_v4_rcv()` | Transport | TCP接收入口 |
| `tcp_v4_do_rcv()` | Transport | TCP状态机处理 |
| `tcp_rcv_established()` | Transport | 已建立连接的快速路径 |
| `tcp_rcv_state_process()` | Transport | TCP状态转换处理 |
| `udp_rcv()` | Transport | UDP接收入口 |
| `sock_queue_rcv_skb()` | Socket | 加入socket接收队列 |
| `tcp_recvmsg()` | Socket | TCP接收消息，复制到用户 |
| `udp_recvmsg()` | Socket | UDP接收消息 |

---

## 5. NAPI and Interrupt Coalescing

```
+------------------------------------------------------------------+
|                    NAPI PROCESSING MODEL                          |
+------------------------------------------------------------------+
|                                                                   |
|   +-------------------+                                           |
|   |   NIC Hardware    |                                           |
|   +--------+----------+                                           |
|            |                                                      |
|            v                                                      |
|   +--------+----------+                                           |
|   |  Hardware IRQ     |  <-- 硬件中断                             |
|   +--------+----------+     最小化处理时间                        |
|            |                                                      |
|            v                                                      |
|   +--------+------------------+                                   |
|   | Interrupt Handler         |                                   |
|   +---------------------------+                                   |
|   | 1. Disable NIC interrupts |  <-- 禁用网卡中断                 |
|   | 2. napi_schedule()        |  <-- 调度NAPI轮询                 |
|   +--------+------------------+                                   |
|            |                                                      |
|            | raise NET_RX_SOFTIRQ                                 |
|            v                                                      |
|   +--------+------------------+                                   |
|   |  Softirq Context          |  <-- 软中断上下文                 |
|   +--------+------------------+                                   |
|            |                                                      |
|            v                                                      |
|   +--------+------------------+                                   |
|   |  net_rx_action()          |                                   |
|   +---------------------------+                                   |
|   |  while (poll_list) {      |                                   |
|   |    napi = list_first();   |                                   |
|   |    work = napi->poll();   |  <-- 调用驱动poll函数             |
|   |    if (work < budget) {   |                                   |
|   |      napi_complete();     |  <-- 完成轮询                     |
|   |      enable_irq();        |  <-- 重新启用中断                 |
|   |    }                      |                                   |
|   |  }                        |                                   |
|   +---------------------------+                                   |
|                                                                   |
|   +--------------------------+                                    |
|   |   Driver's poll()        |  <-- 驱动轮询函数                  |
|   +--------------------------+                                    |
|   | while (budget > 0) {     |                                    |
|   |   desc = get_rx_desc();  |  <-- 从环形缓冲区获取描述符        |
|   |   if (!desc) break;      |                                    |
|   |   skb = build_skb();     |  <-- 构建SKB                       |
|   |   napi_gro_receive(skb); |  <-- 提交到协议栈                  |
|   |   budget--;              |                                    |
|   | }                        |                                    |
|   | return processed;        |                                    |
|   +--------------------------+                                    |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** NAPI的核心思想是：当收到第一个包的中断后，禁用网卡中断并切换到轮询模式。在轮询模式下，持续从接收队列取包直到队列为空或达到预算限制。这样可以在高负载时批量处理数据包，显著减少中断开销。

---

## 6. GRO (Generic Receive Offload)

```
+------------------------------------------------------------------+
|                    GRO AGGREGATION FLOW                           |
+------------------------------------------------------------------+
|                                                                   |
|   napi_gro_receive(skb)                                           |
|        |                                                          |
|        v                                                          |
|   +----+----+                                                     |
|   |         |                                                     |
|   | (Check protocol GRO support)                                  |
|   |         |                                                     |
|   +----+----+                                                     |
|        |                                                          |
|        v                                                          |
|   dev_gro_receive()                                               |
|        |                                                          |
|        v                                                          |
|   +----+----+----+----+                                           |
|   |    |    |    |    |                                           |
|   v    v    v    v    v                                           |
|  IP   TCP  UDP  GRE  VXLAN    <-- 各协议的gro_receive             |
|   |    |    |    |    |                                           |
|   +----+----+----+----+                                           |
|        |                                                          |
|        v                                                          |
|   +----+------------------------+                                 |
|   | Can merge with existing?   |  <-- 检查是否可以合并            |
|   +----+------------------------+                                 |
|        |                                                          |
|        +-------------------+                                      |
|        |                   |                                      |
|        v                   v                                      |
|   [YES: Merge]        [NO: Add to list]                           |
|        |                   |                                      |
|        v                   |                                      |
|   NAPI_GRO_CB(p)->        |                                      |
|   count++                  |                                      |
|        |                   |                                      |
|        +-------------------+                                      |
|        |                                                          |
|        | (flush条件: 不同流、满了、超时)                          |
|        v                                                          |
|   napi_gro_complete()                                             |
|        |                                                          |
|        v                                                          |
|   netif_receive_skb()  <-- 聚合后的大包提交到协议栈               |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|   GRO Merge Conditions (TCP example):                             |
|   +-------------------------------------------------------+       |
|   | - Same flow (src/dst IP, src/dst port)                |       |
|   | - Consecutive sequence numbers                         |       |
|   | - Same flags (except PSH)                              |       |
|   | - Total size doesn't exceed 65535                      |       |
|   +-------------------------------------------------------+       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** GRO（通用接收卸载）是LRO（大接收卸载）的软件实现。它将多个小的TCP段合并成一个大的段，减少协议栈的处理开销。合并条件包括：相同流、连续序列号、相同标志等。

---

## 7. Netfilter Hooks in RX Path

```
+------------------------------------------------------------------+
|                NETFILTER HOOKS IN RX PATH                         |
+------------------------------------------------------------------+
|                                                                   |
|   ip_rcv()                                                        |
|        |                                                          |
|        v                                                          |
|   +----+------------------------+                                 |
|   | NF_INET_PRE_ROUTING         |  <-- PREROUTING链               |
|   | (iptables -t nat/mangle/raw |      所有进入的数据包           |
|   |  -A PREROUTING)             |      DNAT在此处理               |
|   +----+------------------------+      连接跟踪入口               |
|        |                                                          |
|        v                                                          |
|   ip_rcv_finish() --> 路由决策                                    |
|        |                                                          |
|        +-------------------+                                      |
|        |                   |                                      |
|        v                   v                                      |
|   [本地递送]          [转发]                                      |
|        |                   |                                      |
|        |                   v                                      |
|        |            +------+-------+                              |
|        |            | NF_INET_     |  <-- FORWARD链               |
|        |            | FORWARD      |      转发的数据包            |
|        |            +------+-------+      防火墙主要过滤点        |
|        |                   |                                      |
|        |                   v                                      |
|        |            (转到TX Path)                                 |
|        |                                                          |
|        v                                                          |
|   ip_local_deliver()                                              |
|        |                                                          |
|        v                                                          |
|   +----+------------------------+                                 |
|   | NF_INET_LOCAL_IN            |  <-- INPUT链                    |
|   | (iptables -t filter/mangle  |      发往本机的数据包           |
|   |  -A INPUT)                  |      主机防火墙                 |
|   +----+------------------------+                                 |
|        |                                                          |
|        v                                                          |
|   ip_local_deliver_finish()                                       |
|        |                                                          |
|        v                                                          |
|   传输层处理 (tcp_v4_rcv/udp_rcv)                                 |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Netfilter在接收路径提供三个主要钩子点：
1. **PREROUTING**: 所有进入的数据包都经过此点，DNAT和连接跟踪在此处理
2. **FORWARD**: 需要转发的数据包经过此点，主要的转发过滤
3. **INPUT**: 发往本机的数据包经过此点，主机防火墙的主要检查点

---

## 8. RPS/RFS (Receive Packet Steering / Receive Flow Steering)

```
+------------------------------------------------------------------+
|                    RPS/RFS PROCESSING                             |
+------------------------------------------------------------------+
|                                                                   |
|   netif_receive_skb()                                             |
|        |                                                          |
|        v                                                          |
|   +----+------------------------+                                 |
|   | get_rps_cpu()              |  <-- 计算目标CPU                 |
|   +----+------------------------+                                 |
|        |                                                          |
|        | RPS: 基于包头哈希选择CPU                                 |
|        | RFS: 基于流表选择处理该流应用的CPU                       |
|        v                                                          |
|   +----+------------------------+                                 |
|   | (Same CPU as current?)     |                                  |
|   +----+------------------------+                                 |
|        |                                                          |
|        +-------------------+                                      |
|        |                   |                                      |
|        v                   v                                      |
|   [YES: 本地处理]     [NO: IPI到目标CPU]                          |
|        |                   |                                      |
|        |                   v                                      |
|        |            enqueue_to_backlog()                          |
|        |                   |                                      |
|        |                   v                                      |
|        |            (目标CPU的softnet_data)                       |
|        |                   |                                      |
|        |                   v                                      |
|        |            ____napi_schedule(backlog)                    |
|        |                   |                                      |
|        +-------------------+                                      |
|        |                                                          |
|        v                                                          |
|   __netif_receive_skb()                                           |
|                                                                   |
+------------------------------------------------------------------+
|                                                                   |
|   RPS Hash Calculation:                                           |
|   +-------------------------------------------------------+       |
|   | hash = jhash_3words(saddr, daddr, ports)              |       |
|   | cpu = hash % num_cpus_in_rps_map                      |       |
|   +-------------------------------------------------------+       |
|                                                                   |
|   RFS Flow Table:                                                 |
|   +-------------------------------------------------------+       |
|   | flow_table[hash] = {                                  |       |
|   |   .cpu = last_cpu_that_processed_this_flow,           |       |
|   |   .filter = flow_id                                   |       |
|   | }                                                     |       |
|   +-------------------------------------------------------+       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 
- **RPS（接收包导向）**: 软件实现的多队列，将数据包分发到不同CPU处理，基于包头哈希选择CPU
- **RFS（接收流导向）**: 在RPS基础上，将同一流的数据包导向到处理该流应用所在的CPU，提高缓存命中率

---

## 9. DMA Ring Buffer Configuration (网卡如何知道缓冲区地址)

这是网络接收路径中非常关键的底层细节：软件如何告诉网卡DMA缓冲区的地址。

```
+===========================================================================+
||                 DMA RING BUFFER CONFIGURATION                           ||
||               (How NIC Knows Buffer Addresses)                          ||
+===========================================================================+

                         SYSTEM MEMORY
    +------------------------------------------------------------------+
    |                                                                  |
    |   +--------------------------+    +-------------------------+    |
    |   |  Descriptor Ring (RX)    |    |  Data Buffers (SKBs)    |    |
    |   |  (DMA Coherent Memory)   |    |  (DMA Mapped Memory)    |    |
    |   +--------------------------+    +-------------------------+    |
    |   |                          |    |                         |    |
    |   | Desc[0]: pkt_addr -------|----+-> Buffer 0 (2KB/4KB)    |    |
    |   |          hdr_addr        |    |                         |    |
    |   |          status/length   |    |                         |    |
    |   +--------------------------+    +-------------------------+    |
    |   | Desc[1]: pkt_addr -------|----+-> Buffer 1              |    |
    |   |          hdr_addr        |    |                         |    |
    |   |          status/length   |    |                         |    |
    |   +--------------------------+    +-------------------------+    |
    |   | Desc[2]: pkt_addr -------|----+-> Buffer 2              |    |
    |   |          ...             |    |   ...                   |    |
    |   +--------------------------+    +-------------------------+    |
    |   | ...                      |    |                         |    |
    |   +--------------------------+    +-------------------------+    |
    |   | Desc[N-1]: pkt_addr -----|----+-> Buffer N-1            |    |
    |   |             (wrap)       |    |                         |    |
    |   +--------------------------+    +-------------------------+    |
    |           ^                                                      |
    |           |                                                      |
    |   DMA Physical Address                                           |
    |   (Written to NIC Register)                                      |
    |                                                                  |
    +------------------------------------------------------------------+
              |                           ^
              |  PCI/PCIe Bus             |
              v                           |
    +------------------------------------------------------------------+
    |                        NIC HARDWARE                              |
    |                                                                  |
    |   +------------------------+    +---------------------------+    |
    |   | Configuration Registers|    | DMA Engine                |    |
    |   |------------------------|    |---------------------------|    |
    |   | RDBAL: Ring Base Low   |    | 1. Read RDBAL/RDBAH       |    |
    |   | RDBAH: Ring Base High  |--->| 2. Read Desc at Head      |    |
    |   | RDLEN: Ring Length     |    | 3. DMA packet to pkt_addr |    |
    |   | RDH:   Head Pointer    |    | 4. Write status to Desc   |    |
    |   | RDT:   Tail Pointer    |    | 5. Increment Head         |    |
    |   +------------------------+    +---------------------------+    |
    |                                                                  |
    +------------------------------------------------------------------+
```

### 9.1 Step-by-Step Configuration Process

```
+------------------------------------------------------------------+
|            DRIVER INITIALIZATION FLOW                             |
+------------------------------------------------------------------+
|                                                                   |
|   +-----------------------------------------------------------+   |
|   | STEP 1: Allocate Descriptor Ring (DMA Coherent)           |   |
|   +-----------------------------------------------------------+   |
|   |                                                           |   |
|   |   // 分配描述符环 - 使用DMA一致性内存                     |   |
|   |   // 返回: 虚拟地址 + 物理地址                            |   |
|   |   rx_ring->desc = dma_alloc_coherent(                     |   |
|   |       dev,                      // 设备                   |   |
|   |       rx_ring->size,            // 大小(count * desc_sz)  |   |
|   |       &rx_ring->dma,            // 输出: 物理地址         |   |
|   |       GFP_KERNEL);              // 分配标志               |   |
|   |                                                           |   |
|   |   // rx_ring->desc = 虚拟地址 (CPU访问用)                 |   |
|   |   // rx_ring->dma  = 物理/DMA地址 (网卡访问用)            |   |
|   |                                                           |   |
|   +-----------------------------------------------------------+   |
|                          |                                        |
|                          v                                        |
|   +-----------------------------------------------------------+   |
|   | STEP 2: Allocate Data Buffers (SKBs)                      |   |
|   +-----------------------------------------------------------+   |
|   |                                                           |   |
|   |   for (i = 0; i < rx_ring->count; i++) {                  |   |
|   |       // 分配SKB用于存放接收的数据包                      |   |
|   |       skb = netdev_alloc_skb(dev, buffer_size);           |   |
|   |       rx_ring->rx_buffer_info[i].skb = skb;               |   |
|   |                                                           |   |
|   |       // 为SKB数据区创建DMA映射                           |   |
|   |       dma_addr = dma_map_single(                          |   |
|   |           dev,                                            |   |
|   |           skb->data,            // 虚拟地址               |   |
|   |           buffer_size,                                    |   |
|   |           DMA_FROM_DEVICE);     // 数据从设备到内存       |   |
|   |                                                           |   |
|   |       rx_ring->rx_buffer_info[i].dma = dma_addr;          |   |
|   |   }                                                       |   |
|   |                                                           |   |
|   +-----------------------------------------------------------+   |
|                          |                                        |
|                          v                                        |
|   +-----------------------------------------------------------+   |
|   | STEP 3: Initialize Descriptors with Buffer Addresses      |   |
|   +-----------------------------------------------------------+   |
|   |                                                           |   |
|   |   for (i = 0; i < rx_ring->count; i++) {                  |   |
|   |       // 获取描述符指针                                   |   |
|   |       rx_desc = IXGBE_RX_DESC(rx_ring, i);                |   |
|   |                                                           |   |
|   |       // 将数据缓冲区的DMA地址写入描述符                  |   |
|   |       rx_desc->read.pkt_addr = cpu_to_le64(               |   |
|   |           rx_ring->rx_buffer_info[i].dma);                |   |
|   |                                                           |   |
|   |       // 清除状态字段(表示描述符可用)                     |   |
|   |       rx_desc->wb.upper.status_error = 0;                 |   |
|   |   }                                                       |   |
|   |                                                           |   |
|   +-----------------------------------------------------------+   |
|                          |                                        |
|                          v                                        |
|   +-----------------------------------------------------------+   |
|   | STEP 4: Program NIC Registers with Ring Address           |   |
|   +-----------------------------------------------------------+   |
|   |                                                           |   |
|   |   rdba = rx_ring->dma;  // 描述符环的物理地址             |   |
|   |                                                           |   |
|   |   // 写入描述符环基地址 (64位地址分两个32位寄存器)        |   |
|   |   IXGBE_WRITE_REG(hw, IXGBE_RDBAL(idx),                   |   |
|   |                   rdba & 0xFFFFFFFF);      // 低32位      |   |
|   |   IXGBE_WRITE_REG(hw, IXGBE_RDBAH(idx),                   |   |
|   |                   rdba >> 32);             // 高32位      |   |
|   |                                                           |   |
|   |   // 写入描述符环长度                                     |   |
|   |   IXGBE_WRITE_REG(hw, IXGBE_RDLEN(idx),                   |   |
|   |                   count * sizeof(union ixgbe_adv_rx_desc));|   |
|   |                                                           |   |
|   |   // 初始化Head和Tail指针                                 |   |
|   |   IXGBE_WRITE_REG(hw, IXGBE_RDH(idx), 0);  // Head = 0    |   |
|   |   IXGBE_WRITE_REG(hw, IXGBE_RDT(idx), 0);  // Tail = 0    |   |
|   |                                                           |   |
|   +-----------------------------------------------------------+   |
|                          |                                        |
|                          v                                        |
|   +-----------------------------------------------------------+   |
|   | STEP 5: Enable RX Queue and Update Tail                   |   |
|   +-----------------------------------------------------------+   |
|   |                                                           |   |
|   |   // 启用接收队列                                         |   |
|   |   rxdctl |= IXGBE_RXDCTL_ENABLE;                          |   |
|   |   IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(idx), rxdctl);         |   |
|   |                                                           |   |
|   |   // 更新Tail指针，告诉网卡有多少描述符可用               |   |
|   |   // Tail指向最后一个可用描述符的下一个位置               |   |
|   |   IXGBE_WRITE_REG(hw, IXGBE_RDT(idx), count - 1);         |   |
|   |                                                           |   |
|   |   // 现在网卡知道:                                        |   |
|   |   // 1. 描述符环在哪里 (RDBAL/RDBAH)                      |   |
|   |   // 2. 环有多大 (RDLEN)                                  |   |
|   |   // 3. 从哪里开始处理 (RDH)                              |   |
|   |   // 4. 可以处理到哪里 (RDT)                              |   |
|   |   // 5. 每个描述符里有数据缓冲区的地址                    |   |
|   |                                                           |   |
|   +-----------------------------------------------------------+   |
|                                                                   |
+------------------------------------------------------------------+
```

### 9.2 RX Descriptor Structure

```c
/* 位置: drivers/net/ethernet/intel/ixgbe/ixgbe_type.h */
/* 接收描述符结构 (Intel 10GbE) */

union ixgbe_adv_rx_desc {
    /* Read Format - 网卡读取此格式获取缓冲区地址 */
    struct {
        __le64 pkt_addr;    /* 数据包缓冲区的DMA物理地址 */
        __le64 hdr_addr;    /* 头部缓冲区地址(Header Split用) */
    } read;
    
    /* Write-Back Format - 网卡写回接收状态 */
    struct {
        struct {
            __le32 data;        /* RSS hash, packet type等 */
            __le16 pkt_info;    /* RSS type, packet type */
            __le16 hdr_info;    /* Header length等 */
        } lo_dword;
        struct {
            __le32 rss;         /* RSS hash */
            __le16 ip_id;       /* IP identification */
            __le16 csum;        /* Checksum */
        } hi_dword;
        __le32 status_error;    /* 状态和错误标志 */
        __le16 length;          /* 数据包长度 */
        __le16 vlan;            /* VLAN tag */
    } wb;  /* write-back */
};
```

**中文说明：** 接收描述符是一个union结构，有两种格式：
- **Read格式**: 驱动填写，包含数据缓冲区的DMA地址，网卡读取此地址进行DMA写入
- **Write-Back格式**: 网卡填写，包含接收状态、长度、RSS哈希等信息

### 9.3 Head/Tail Pointer Mechanism

```
+------------------------------------------------------------------+
|                HEAD/TAIL POINTER MECHANISM                        |
+------------------------------------------------------------------+
|                                                                   |
|   Descriptor Ring (Circular Buffer):                              |
|                                                                   |
|   +---+---+---+---+---+---+---+---+---+---+---+---+               |
|   | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |10 |11 | ...          |
|   +---+---+---+---+---+---+---+---+---+---+---+---+               |
|         ^                       ^                                 |
|         |                       |                                 |
|        HEAD                    TAIL                               |
|     (Hardware)              (Software)                            |
|                                                                   |
|   +---------------------------------------------------------------+
|   |                                                               |
|   |  HEAD (RDH) - 网卡维护:                                       |
|   |    - 指向下一个要处理的描述符                                 |
|   |    - 网卡DMA完成后自动递增                                    |
|   |    - 只读(对软件而言)                                         |
|   |                                                               |
|   |  TAIL (RDT) - 驱动维护:                                       |
|   |    - 指向最后一个可用描述符的下一个位置                       |
|   |    - 驱动补充缓冲区后更新                                     |
|   |    - 驱动写入，网卡读取                                       |
|   |                                                               |
|   |  可用描述符: HEAD到TAIL之间的描述符                           |
|   |  (TAIL - HEAD) mod RING_SIZE = 可用描述符数                   |
|   |                                                               |
|   +---------------------------------------------------------------+
|                                                                   |
|   OPERATION FLOW:                                                 |
|                                                                   |
|   1. 初始状态: HEAD=0, TAIL=N-1 (所有描述符可用)                  |
|                                                                   |
|      +===+===+===+===+===+===+===+===+                            |
|      | A | A | A | A | A | A | A | A |  A=Available               |
|      +===+===+===+===+===+===+===+===+                            |
|        ^                           ^                              |
|       HEAD                        TAIL                            |
|                                                                   |
|   2. 网卡接收包后: HEAD递增, 描述符变为"已使用"                   |
|                                                                   |
|      +===+===+===+===+===+===+===+===+                            |
|      | U | U | U | A | A | A | A | A |  U=Used                    |
|      +===+===+===+===+===+===+===+===+                            |
|                ^                   ^                              |
|               HEAD                TAIL                            |
|                                                                   |
|   3. 驱动处理完成后: 重新分配缓冲区, 更新TAIL                     |
|                                                                   |
|      +===+===+===+===+===+===+===+===+                            |
|      | A | A | A | A | A | A | A | A |                            |
|      +===+===+===+===+===+===+===+===+                            |
|                ^   ^                                              |
|               HEAD TAIL (wrap around)                             |
|                                                                   |
+------------------------------------------------------------------+
```

### 9.4 Key Kernel Code (Intel ixgbe Driver)

```c
/* 文件: drivers/net/ethernet/intel/ixgbe/ixgbe_main.c */

/* 配置接收队列 - 将DMA地址写入网卡寄存器 */
void ixgbe_configure_rx_ring(struct ixgbe_adapter *adapter,
                             struct ixgbe_ring *ring)
{
    struct ixgbe_hw *hw = &adapter->hw;
    u64 rdba = ring->dma;  /* 描述符环的DMA物理地址 */
    u8 reg_idx = ring->reg_idx;

    /* 写入描述符环基地址到网卡寄存器 */
    IXGBE_WRITE_REG(hw, IXGBE_RDBAL(reg_idx), 
                    (rdba & DMA_BIT_MASK(32)));  /* 低32位 */
    IXGBE_WRITE_REG(hw, IXGBE_RDBAH(reg_idx), 
                    (rdba >> 32));               /* 高32位 */
    
    /* 写入描述符环长度 */
    IXGBE_WRITE_REG(hw, IXGBE_RDLEN(reg_idx),
                    ring->count * sizeof(union ixgbe_adv_rx_desc));
    
    /* 初始化Head/Tail指针 */
    IXGBE_WRITE_REG(hw, IXGBE_RDH(reg_idx), 0);  /* Head = 0 */
    IXGBE_WRITE_REG(hw, IXGBE_RDT(reg_idx), 0);  /* Tail = 0 */
    
    /* 保存Tail寄存器地址供后续更新 */
    ring->tail = hw->hw_addr + IXGBE_RDT(reg_idx);
    
    /* 启用接收队列 */
    rxdctl |= IXGBE_RXDCTL_ENABLE;
    IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(reg_idx), rxdctl);
    
    /* 分配接收缓冲区并更新Tail */
    ixgbe_alloc_rx_buffers(ring, ixgbe_desc_unused(ring));
}

/* 分配接收缓冲区并填充描述符 */
void ixgbe_alloc_rx_buffers(struct ixgbe_ring *rx_ring, u16 cleaned_count)
{
    union ixgbe_adv_rx_desc *rx_desc;
    struct ixgbe_rx_buffer *bi;
    u16 i = rx_ring->next_to_use;

    while (cleaned_count--) {
        rx_desc = IXGBE_RX_DESC(rx_ring, i);
        bi = &rx_ring->rx_buffer_info[i];

        /* 分配SKB */
        bi->skb = netdev_alloc_skb(rx_ring->netdev, rx_ring->rx_buf_len);
        
        /* 创建DMA映射 */
        bi->dma = dma_map_single(rx_ring->dev, bi->skb->data,
                                 rx_ring->rx_buf_len, DMA_FROM_DEVICE);
        
        /* 将DMA地址写入描述符 */
        rx_desc->read.pkt_addr = cpu_to_le64(bi->dma);
        
        i++;
        if (i == rx_ring->count)
            i = 0;  /* 环形缓冲区回绕 */
    }
    
    rx_ring->next_to_use = i;
    
    /* 更新Tail寄存器，告诉网卡新的可用描述符 */
    writel(i, rx_ring->tail);
}
```

### 9.5 NIC Register Summary

| Register | Name | Description |
|----------|------|-------------|
| **RDBAL** | RX Descriptor Base Address Low | 描述符环基地址低32位 |
| **RDBAH** | RX Descriptor Base Address High | 描述符环基地址高32位 |
| **RDLEN** | RX Descriptor Length | 描述符环总长度(字节) |
| **RDH** | RX Descriptor Head | Head指针(网卡维护) |
| **RDT** | RX Descriptor Tail | Tail指针(驱动更新) |
| **RXDCTL** | RX Descriptor Control | 接收描述符控制(启用等) |

### 9.6 DMA Memory Types

```
+------------------------------------------------------------------+
|                    DMA MEMORY TYPES                               |
+------------------------------------------------------------------+
|                                                                   |
|   +---------------------------+----------------------------------+|
|   |  Memory Type              |  Use Case                        ||
|   +---------------------------+----------------------------------+|
|   |                           |                                  ||
|   |  dma_alloc_coherent()     |  描述符环                        ||
|   |  (一致性DMA内存)          |  - CPU和设备都需要频繁访问       ||
|   |                           |  - 硬件自动保持缓存一致性        ||
|   |                           |  - 不需要显式sync操作            ||
|   |                           |                                  ||
|   +---------------------------+----------------------------------+|
|   |                           |                                  ||
|   |  dma_map_single()         |  数据缓冲区(SKB)                 ||
|   |  (流式DMA映射)            |  - 单向数据传输                  ||
|   |                           |  - DMA_FROM_DEVICE: 接收         ||
|   |                           |  - DMA_TO_DEVICE: 发送           ||
|   |                           |  - 需要在访问前后sync            ||
|   |                           |                                  ||
|   +---------------------------+----------------------------------+|
|   |                           |                                  ||
|   |  dma_map_page()           |  大缓冲区(分页数据)              ||
|   |  (页面DMA映射)            |  - 用于jumbo frame               ||
|   |                           |  - 避免大内存连续分配            ||
|   |                           |                                  ||
|   +---------------------------+----------------------------------+|
|                                                                   |
+------------------------------------------------------------------+
```

### 9.7 DMA Coherent Memory (DMA一致性内存)
```
- 问题背景：CPU缓存与DMA的冲突
    +------------------------------------------------------------------+
    |                    THE CACHE COHERENCY PROBLEM                    |
    +------------------------------------------------------------------+

    CPU                                  Device (NIC/DMA)
        |                                        |
        v                                        v
    +--------+                              +--------+
    | L1/L2  |                              |  DMA   |
    | Cache  |                              | Engine |
    +--------+                              +--------+
        |                                        |
        +---------------+  +--------------------+
                        |  |
                        v  v
                +-------------+
                |   Memory    |
                | (RAM)       |
                +-------------+

    问题场景：

    1. CPU写入数据到缓存，但未刷新到内存
    -> 设备DMA读取时看到的是旧数据 ❌

    2. 设备DMA写入数据到内存
    -> CPU从缓存读取时看到的是旧数据 ❌

- 两种DMA内存类型
    类型	            函数	                缓存一致性	    适用场景
    Coherent (一致性)	dma_alloc_coherent()	硬件自动保证	描述符环、控制结构
    Streaming (流式)	dma_map_single()	    需要手动sync	数据缓冲区

- DMA Coherent Memory 特点
+------------------------------------------------------------------+
|                    DMA COHERENT MEMORY                            |
+------------------------------------------------------------------+

分配方式:
    void *vaddr = dma_alloc_coherent(
        struct device *dev,     // 设备
        size_t size,            // 大小
        dma_addr_t *dma_handle, // 输出: 物理/总线地址
        gfp_t gfp               // 分配标志
    );

返回值:
    vaddr      = CPU使用的虚拟地址
    dma_handle = 设备使用的DMA/物理地址

特点:
+---------------------------------------------------------------+
|                                                               |
|  1. 禁用缓存 (Uncached/Write-Combined)                        |
|     - 内存区域被标记为不可缓存                                |
|     - CPU每次访问都直接读写内存                               |
|     - 设备DMA也直接读写内存                                   |
|     -> 双方看到的数据始终一致                                 |
|                                                               |
|  2. 无需手动同步                                              |
|     - 不需要调用 dma_sync_* 函数                              |
|     - 硬件自动保证一致性                                      |
|                                                               |
|  3. 性能开销                                                  |
|     - CPU访问较慢（绕过缓存）                                 |
|     - 适合小量、频繁更新的控制结构                            |
|                                                               |
+---------------------------------------------------------------+

- 对比：Streaming DMA
+------------------------------------------------------------------+
|                    STREAMING DMA MAPPING                          |
+------------------------------------------------------------------+

映射方式:
    dma_addr_t dma_addr = dma_map_single(
        struct device *dev,
        void *cpu_addr,         // 已分配内存的虚拟地址
        size_t size,
        enum dma_data_direction dir  // DMA_TO_DEVICE / DMA_FROM_DEVICE
    );

特点:
+---------------------------------------------------------------+
|                                                               |
|  1. 使用缓存 (Cached)                                         |
|     - 内存正常使用CPU缓存                                      |
|     - 访问速度快                                              |
|                                                               |
|  2. 需要手动同步                                              |
|                                                               |
|     DMA_TO_DEVICE (发送):                                     |
|     +-------------------+                                     |
|     | CPU写入数据       |                                     |
|     +-------------------+                                     |
|              |                                                |
|              v                                                |
|     +-------------------+                                     |
|     | dma_sync_single_  |  <-- 刷新CPU缓存到内存              |
|     | for_device()      |                                     |
|     +-------------------+                                     |
|              |                                                |
|              v                                                |
|     +-------------------+                                     |
|     | 设备DMA读取       |                                     |
|     +-------------------+                                     |
|                                                               |
|     DMA_FROM_DEVICE (接收):                                   |
|     +-------------------+                                     |
|     | 设备DMA写入       |                                     |
|     +-------------------+                                     |
|              |                                                |
|              v                                                |
|     +-------------------+                                     |
|     | dma_sync_single_  |  <-- 使CPU缓存无效                  |
|     | for_cpu()         |                                     |
|     +-------------------+                                     |
|              |                                                |
|              v                                                |
|     +-------------------+                                     |
|     | CPU读取数据       |                                     |
|     +-------------------+                                     |
|                                                               |
+---------------------------------------------------------------+

- 网卡驱动中的典型用法
/* 描述符环 - 使用 Coherent Memory */
/* 原因: CPU和网卡都需要频繁读写，需要实时一致 */
rx_ring->desc = dma_alloc_coherent(dev, 
                                   ring_size,
                                   &rx_ring->dma,  /* 物理地址 */
                                   GFP_KERNEL);

/* 数据缓冲区 - 使用 Streaming DMA */  
/* 原因: 大量数据传输，需要缓存提高性能 */
dma_addr = dma_map_single(dev, 
                          skb->data,
                          buffer_size,
                          DMA_FROM_DEVICE);  /* 网卡写入内存 */

/* 接收完成后，CPU读取前需要同步 */
dma_sync_single_for_cpu(dev, dma_addr, len, DMA_FROM_DEVICE);

总结
    特性	    Coherent           Memory	Streaming Memory
    缓存	    禁用/绕过	        启用
    一致性	    硬件自动	        需手动sync
    性能	    CPU访问慢	        CPU访问快
    适用	    描述符、控制结构	大数据缓冲区
    生命周期	长期持有	        短期映射/解映射

核心理解： DMA Coherent Memory通过禁用CPU缓存来保证CPU和设备看到的内存内容始终一致，代价是CPU访问速度变慢。适合小型、频繁访问的控制结构（如描述符环），而不适合大量数据传输。
```

**中文总结：**

网卡获取DMA缓冲区地址的完整流程：

1. **驱动分配描述符环**: 使用`dma_alloc_coherent()`分配一块连续的DMA一致性内存作为描述符环，同时获得虚拟地址和物理地址

2. **驱动分配数据缓冲区**: 为每个描述符分配SKB，并用`dma_map_single()`获取SKB数据区的DMA地址

3. **填充描述符**: 将每个数据缓冲区的DMA地址写入对应描述符的`pkt_addr`字段

4. **配置网卡寄存器**: 将描述符环的物理地址写入网卡的RDBAL/RDBAH寄存器，这样网卡就知道描述符环在哪里

5. **网卡接收数据**: 网卡从描述符中读取`pkt_addr`，通过DMA将数据包直接写入该地址对应的内存

6. **Tail指针更新**: 驱动通过更新RDT寄存器告诉网卡有多少描述符是可用的

