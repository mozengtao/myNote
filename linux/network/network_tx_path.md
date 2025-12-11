# Linux Network Transmit (TX) Path - Complete Call Chain

## 1. Overview ASCII Diagram

```
+=============================================================================+
||                    LINUX NETWORK TRANSMIT PATH                            ||
||                   (Socket to Wire - Complete Stack)                       ||
+=============================================================================+

+-----------------------------------------------------------------------------+
|                           USER SPACE                                        |
+-----------------------------------------------------------------------------+
|                                                                             |
|   Application                                                               |
|   +-----------------+                                                       |
|   | write()         |  <-- 写入文件描述符                                   |
|   | send()          |  <-- 发送数据（无标志）                               |
|   | sendto()        |  <-- 发送数据到指定地址（UDP常用）                    |
|   | sendmsg()       |  <-- 发送消息（支持辅助数据、多缓冲区）               |
|   +-----------------+                                                       |
|           |                                                                 |
|           | (System Call via glibc wrapper)                                 |
|           v                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                        SYSTEM CALL LAYER                                    |
|                       (arch/x86/kernel/entry_*.S, net/socket.c)             |
+-----------------------------------------------------------------------------+
|                                                                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------+                                                        |
|   | sys_sendto()   |  <-- 系统调用入口，处理sendto()                        |
|   +-------+--------+                                                        |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------+                                                        |
|   | sys_sendmsg()  |  <-- 系统调用入口，处理sendmsg()                       |
|   +-------+--------+                                                        |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | __sys_sendmsg()    |  <-- 内部实现，复制用户空间消息头                  |
|   +-------+------------+                                                    |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | ___sys_sendmsg()   |  <-- 构建内核消息结构 msghdr                       |
|   +-------+------------+                                                    |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                         SOCKET LAYER                                        |
|                        (net/socket.c)                                       |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+-----------+                                                     |
|   | sock_sendmsg()    |  <-- Socket发送消息入口                             |
|   +-------+-----------+     检查安全模块(LSM)权限                           |
|           |                                                                 |
|           v                                                                 |
|   +-------+-----------+                                                     |
|   | __sock_sendmsg()  |  <-- 调用security_socket_sendmsg()                  |
|   +-------+-----------+                                                     |
|           |                                                                 |
|           v                                                                 |
|   +-------+-------------------+                                             |
|   | __sock_sendmsg_nosec()    |  <-- 设置sock_iocb，调用协议层              |
|   +-------+-------------------+                                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+-------------------+                                             |
|   | sock->ops->sendmsg()      |  <-- 调用协议特定的sendmsg                  |
|   +---------------------------+     (inet_sendmsg for TCP/UDP)              |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                    PROTOCOL FAMILY LAYER                                    |
|                   (net/ipv4/af_inet.c)                                      |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | inet_sendmsg()     |  <-- AF_INET协议族发送入口                         |
|   +-------+------------+     自动绑定端口(如果需要)                         |
|           |                                                                 |
|           v                                                                 |
|   +-------+-------------------+                                             |
|   | sk->sk_prot->sendmsg()    |  <-- 调用传输层协议sendmsg                  |
|   +---------------------------+     tcp_sendmsg / udp_sendmsg               |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                       TRANSPORT LAYER (TCP)                                 |
|                      (net/ipv4/tcp.c, tcp_output.c)                         |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | tcp_sendmsg()      |  <-- TCP发送消息主函数                             |
|   +-------+------------+     等待连接建立(如果需要)                         |
|           |                  计算MSS，分配SKB                               |
|           |                  将数据复制到SKB                                |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | tcp_push()         |  <-- 触发TCP发送流程                               |
|   +-------+------------+     设置PSH标志(如果需要)                          |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | __tcp_push_pending |  <-- 推送挂起的数据                                |
|   | _frames()          |                                                    |
|   +-------+------------+                                                    |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | tcp_write_xmit()   |  <-- TCP写发送队列                                 |
|   +-------+------------+     处理拥塞控制窗口                               |
|           |                  检查发送窗口                                   |
|           |                  TSO/GSO分段                                    |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | tcp_transmit_skb() |  <-- 构建TCP头部并发送单个SKB                      |
|   +-------+------------+     设置序列号、确认号                             |
|           |                  计算校验和                                     |
|           |                  启动重传定时器                                 |
|           v                                                                 |
|   +-------+-------------------+                                             |
|   | icsk->icsk_af_ops->       |  <-- 调用IP层发送队列                       |
|   |   queue_xmit()            |     (ip_queue_xmit)                         |
|   +---------------------------+                                             |
|                                                                             |
+-----------------------------------------------------------------------------+
|                       TRANSPORT LAYER (UDP)                                 |
|                      (net/ipv4/udp.c)                                       |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | udp_sendmsg()      |  <-- UDP发送消息主函数                             |
|   +-------+------------+     获取目标地址                                   |
|           |                  路由查找                                       |
|           |                  cork模式处理                                   |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | udp_send_skb()     |  <-- 构建UDP头部                                   |
|   +-------+------------+     计算校验和                                     |
|           |                                                                 |
|           v                                                                 |
|   +-------+-------------------+                                             |
|   | ip_send_skb()             |  <-- 调用IP层发送                           |
|   +---------------------------+                                             |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                         IP LAYER (Network Layer)                            |
|                        (net/ipv4/ip_output.c)                               |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | ip_queue_xmit()    |  <-- IP发送队列（TCP路径）                         |
|   +-------+------------+     查找/缓存路由                                  |
|           |                  设置IP选项                                     |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | ip_local_out()     |  <-- 本地输出处理                                  |
|   +-------+------------+     调用Netfilter OUTPUT链                         |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | __ip_local_out()     |  <-- 设置IP头部字段                              |
|   +-------+--------------+     计算IP头校验和                               |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+    +-----------------------+                     |
|   | nf_hook(NF_INET_     |--->| Netfilter OUTPUT Hook |                     |
|   |   LOCAL_OUT)         |    | (iptables OUTPUT链)   |                     |
|   +-------+--------------+    +-----------------------+                     |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | dst_output()       |  <-- 目标输出函数                                  |
|   +-------+------------+     调用路由缓存中的output函数                     |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | ip_output()        |  <-- IP输出处理                                    |
|   +-------+------------+     调用Netfilter POSTROUTING链                    |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+    +---------------------------+                 |
|   | nf_hook(NF_INET_     |--->| Netfilter POSTROUTING Hook|                 |
|   |   POST_ROUTING)      |    | (iptables POSTROUTING链)  |                 |
|   +-------+--------------+    +---------------------------+                 |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | ip_finish_output()   |  <-- 完成IP输出处理                              |
|   +-------+--------------+     检查是否需要分片                             |
|           |                                                                 |
|           +-------------------+                                             |
|           |                   |                                             |
|           v                   v                                             |
|   +-------+-------+   +-------+--------+                                    |
|   | (skb->len <=  |   | ip_fragment()  |  <-- IP分片（如果需要）            |
|   |  mtu)         |   +-------+--------+     大包分成小片                   |
|   +-------+-------+           |                                             |
|           |                   |                                             |
|           +-------------------+                                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | ip_finish_output2()  |  <-- 准备L2发送                                  |
|   +-------+--------------+     邻居子系统查找                               |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | dst_neigh_output() |  <-- 邻居输出                                      |
|   +-------+------------+     调用邻居子系统发送                             |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                     NEIGHBOUR SUBSYSTEM (ARP/NDP)                           |
|                    (net/core/neighbour.c)                                   |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------+                                                |
|   | neigh_resolve_output() |  <-- 解析邻居MAC地址                           |
|   +-------+----------------+     如果没有缓存则发送ARP请求                  |
|           |                      获取目标MAC后调用设备发送                  |
|           |                                                                 |
|           v                                                                 |
|   +-------+------------+                                                    |
|   | neigh_hh_output()  |  <-- 使用缓存的硬件头发送                          |
|   +-------+------------+     快速路径，复制缓存的L2头                       |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | dev_queue_xmit()     |  <-- 设备发送队列入口                            |
|   +-----------------------+                                                 |
|                                                                             |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                       TRAFFIC CONTROL (QOS)                                 |
|                      (net/sched/sch_generic.c)                              |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | dev_queue_xmit()     |  <-- 设备传输队列                                |
|   +-------+--------------+     选择TX队列                                   |
|           |                    获取队列锁                                   |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | __dev_queue_xmit()   |  <-- 内部发送实现                                |
|   +-------+--------------+     处理Qdisc（队列规则）                        |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+    +------------------------+                    |
|   | q->enqueue()         |--->| Traffic Control/QoS    |                    |
|   +-------+--------------+    | (htb, pfifo, etc.)     |                    |
|           |                   +------------------------+                    |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | __qdisc_run()        |  <-- 运行队列规则                                |
|   +-------+--------------+     从队列取出数据包                             |
|           |                                                                 |
|           v                                                                 |
|   +-------+--------------+                                                  |
|   | sch_direct_xmit()    |  <-- 直接发送到设备                              |
|   +-------+--------------+                                                  |
|           |                                                                 |
|           v                                                                 |
|   +-------+-------------------+                                             |
|   | dev_hard_start_xmit()     |  <-- 硬件发送起始                           |
|   +-------+-------------------+     调用驱动ndo_start_xmit                  |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                       DEVICE DRIVER LAYER                                   |
|                     (drivers/net/ethernet/...)                              |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | ndo_start_xmit()             |  <-- 驱动发送函数入口                    |
|   | (e.g., e1000_xmit_frame,     |     网卡驱动的发送回调                   |
|   |  ixgbe_xmit_frame)           |                                          |
|   +-------+----------------------+                                          |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | Driver TX Processing         |  <-- 驱动内部处理                        |
|   +------------------------------+                                          |
|   | - Map SKB to DMA buffers     |     将SKB映射到DMA缓冲区                 |
|   | - Setup TX descriptors       |     设置发送描述符                       |
|   | - TSO/GSO offload            |     TSO/GSO卸载到硬件                    |
|   | - Checksum offload           |     校验和卸载到硬件                     |
|   +-------+----------------------+                                          |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | Write to TX Ring             |  <-- 写入发送环形缓冲区                  |
|   +-------+----------------------+     更新tail指针通知硬件                 |
|           |                                                                 |
+=============================================================================+

+-----------------------------------------------------------------------------+
|                          HARDWARE (NIC)                                     |
+-----------------------------------------------------------------------------+
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | NIC DMA Engine               |  <-- 网卡DMA引擎                         |
|   +-------+----------------------+     从内存读取数据包                     |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | NIC TX Processing            |  <-- 网卡发送处理                        |
|   +------------------------------+                                          |
|   | - Add Ethernet FCS           |     添加以太网帧校验序列                 |
|   | - Apply hardware offloads    |     应用硬件卸载功能                     |
|   +-------+----------------------+                                          |
|           |                                                                 |
|           v                                                                 |
|   +-------+----------------------+                                          |
|   | PHY / MAC Layer              |  <-- 物理层/MAC层                        |
|   +-------+----------------------+     编码、调制                           |
|           |                                                                 |
|           v                                                                 |
|   +------------------------------+                                          |
|   |        NETWORK WIRE          |  <-- 网络线路                            |
|   +------------------------------+     数据包发送到网络                     |
|                                                                             |
+=============================================================================+
```

---

## 2. Key Source Files

| Layer | File | Description |
|-------|------|-------------|
| **System Call** | `net/socket.c` | Socket系统调用实现 |
| **Socket** | `net/socket.c` | sock_sendmsg()等函数 |
| **AF_INET** | `net/ipv4/af_inet.c` | inet_sendmsg() |
| **TCP** | `net/ipv4/tcp.c` | tcp_sendmsg() |
| **TCP Output** | `net/ipv4/tcp_output.c` | tcp_transmit_skb() |
| **UDP** | `net/ipv4/udp.c` | udp_sendmsg() |
| **IP Output** | `net/ipv4/ip_output.c` | ip_queue_xmit(), ip_output() |
| **Neighbour** | `net/core/neighbour.c` | neigh_resolve_output() |
| **Core Dev** | `net/core/dev.c` | dev_queue_xmit() |
| **Qdisc** | `net/sched/sch_generic.c` | 流量控制 |
| **Driver** | `drivers/net/ethernet/...` | 各网卡驱动 |

---

## 3. Key Data Structures

### 3.1 struct sk_buff (Socket Buffer)

```c
/* 位置: include/linux/skbuff.h */
/* 核心网络数据缓冲区，贯穿整个网络栈 */

struct sk_buff {
    /* 链表指针，用于队列管理 */
    struct sk_buff      *next;
    struct sk_buff      *prev;
    
    /* 时间戳 */
    ktime_t             tstamp;
    
    /* 所属socket */
    struct sock         *sk;
    
    /* 关联的网络设备 */
    struct net_device   *dev;
    
    /* 路由/目的信息 */
    unsigned long       _skb_refdst;
    
    /* 数据指针 */
    unsigned char       *head;      /* 缓冲区开始 */
    unsigned char       *data;      /* 数据开始 */
    unsigned char       *tail;      /* 数据结束 */
    unsigned char       *end;       /* 缓冲区结束 */
    
    unsigned int        len;        /* 数据长度 */
    unsigned int        data_len;   /* 分页数据长度 */
    
    /* 协议信息 */
    __be16              protocol;   /* 以太网协议类型 */
    
    /* 校验和相关 */
    __u8                ip_summed;
    /* ... 更多字段 ... */
};
```

**中文说明：** `sk_buff`是Linux网络栈中最重要的数据结构，它封装了网络数据包及其元数据。数据包从用户空间到网卡的整个发送过程中，都通过这个结构传递。

### 3.2 struct socket

```c
/* 位置: include/linux/net.h */
/* BSD socket接口层表示 */

struct socket {
    socket_state        state;      /* 连接状态 */
    short               type;       /* SOCK_STREAM, SOCK_DGRAM等 */
    unsigned long       flags;      /* 标志位 */
    
    struct socket_wq    *wq;        /* 等待队列 */
    struct file         *file;      /* 关联的文件描述符 */
    struct sock         *sk;        /* 网络层socket */
    
    /* 协议操作函数表 */
    const struct proto_ops *ops;    /* inet_stream_ops, inet_dgram_ops等 */
};
```

**中文说明：** `struct socket`是用户空间与内核网络栈之间的接口层，通过ops指向的协议操作表调用底层协议实现。

### 3.3 struct sock

```c
/* 位置: include/net/sock.h */
/* 网络层socket，包含协议状态信息 */

struct sock {
    struct sock_common  __sk_common;
    
    /* 发送/接收缓冲区管理 */
    atomic_t            sk_rmem_alloc;  /* 接收缓冲区已用 */
    atomic_t            sk_wmem_alloc;  /* 发送缓冲区已用 */
    int                 sk_sndbuf;      /* 发送缓冲区大小 */
    int                 sk_rcvbuf;      /* 接收缓冲区大小 */
    
    /* 队列 */
    struct sk_buff_head sk_receive_queue;  /* 接收队列 */
    struct sk_buff_head sk_write_queue;    /* 发送队列 */
    
    /* 协议操作 */
    struct proto        *sk_prot;       /* tcp_prot, udp_prot等 */
    
    /* 状态 */
    unsigned char       sk_state;       /* TCP状态等 */
    
    /* 回调函数 */
    void (*sk_state_change)(struct sock *sk);
    void (*sk_data_ready)(struct sock *sk, int bytes);
    void (*sk_write_space)(struct sock *sk);
    /* ... */
};
```

**中文说明：** `struct sock`是传输层的核心结构，包含协议状态、缓冲区、发送/接收队列等。TCP、UDP等协议都扩展这个基本结构。

### 3.4 struct tcp_sock

```c
/* 位置: include/linux/tcp.h */
/* TCP协议特定信息 */

struct tcp_sock {
    struct inet_connection_sock inet_conn;
    
    /* 序列号管理 */
    u32     snd_una;        /* 第一个未确认的序列号 */
    u32     snd_nxt;        /* 下一个要发送的序列号 */
    u32     rcv_nxt;        /* 下一个期望接收的序列号 */
    
    /* 窗口管理 */
    u32     snd_wnd;        /* 发送窗口 */
    u32     rcv_wnd;        /* 接收窗口 */
    
    /* 拥塞控制 */
    u32     snd_cwnd;       /* 拥塞窗口 */
    u32     snd_ssthresh;   /* 慢启动阈值 */
    
    /* MSS */
    u16     mss_cache;      /* 缓存的MSS值 */
    
    /* 重传 */
    struct timer_list retransmit_timer;
    /* ... */
};
```

**中文说明：** `tcp_sock`扩展了基本的`sock`结构，添加了TCP协议特有的字段，如序列号、窗口大小、拥塞控制参数等。

### 3.5 struct net_device

```c
/* 位置: include/linux/netdevice.h */
/* 网络设备抽象 */

struct net_device {
    char            name[IFNAMSIZ];     /* 设备名如eth0 */
    
    /* 硬件地址 */
    unsigned char   dev_addr[MAX_ADDR_LEN];
    
    /* MTU */
    unsigned int    mtu;
    
    /* 发送队列 */
    struct netdev_queue *_tx;
    unsigned int    num_tx_queues;
    
    /* 设备操作函数表 */
    const struct net_device_ops *netdev_ops;
    
    /* 流量控制 */
    struct Qdisc    *qdisc;
    
    /* 特性标志（GSO、TSO、校验和卸载等） */
    netdev_features_t features;
    /* ... */
};
```

**中文说明：** `net_device`代表一个网络接口（物理或虚拟），包含设备操作函数、硬件特性、发送队列等。

### 3.6 struct net_device_ops

```c
/* 位置: include/linux/netdevice.h */
/* 网络设备操作函数表 */

struct net_device_ops {
    int  (*ndo_open)(struct net_device *dev);
    int  (*ndo_stop)(struct net_device *dev);
    
    /* 核心发送函数 - 驱动必须实现 */
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                  struct net_device *dev);
    
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int  (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int  (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    /* ... */
};
```

**中文说明：** `net_device_ops`定义了网络设备驱动必须实现的操作函数，其中`ndo_start_xmit`是数据包发送的入口点。

---

## 4. Key Function Descriptions

| Function | Layer | Description |
|----------|-------|-------------|
| `sys_sendmsg()` | Syscall | 系统调用入口，处理sendmsg() |
| `sock_sendmsg()` | Socket | Socket层发送，调用LSM检查 |
| `inet_sendmsg()` | AF_INET | IPv4协议族发送入口 |
| `tcp_sendmsg()` | Transport | TCP发送主函数，管理发送缓冲区和分段 |
| `tcp_transmit_skb()` | Transport | 构建TCP头并发送单个SKB |
| `udp_sendmsg()` | Transport | UDP发送主函数 |
| `ip_queue_xmit()` | Network | IP层发送队列，处理路由 |
| `ip_local_out()` | Network | 本地输出，调用Netfilter OUTPUT |
| `ip_output()` | Network | IP输出，调用Netfilter POSTROUTING |
| `ip_finish_output()` | Network | 完成IP输出，检查分片 |
| `neigh_resolve_output()` | Neighbour | 解析目标MAC地址 |
| `dev_queue_xmit()` | Device | 设备发送队列入口 |
| `dev_hard_start_xmit()` | Device | 调用驱动发送函数 |
| `ndo_start_xmit()` | Driver | 驱动发送函数，写入硬件 |

---

## 5. GSO/TSO Offload Path

```
+------------------------------------------------------------------+
|                  GSO/TSO OFFLOAD PATH                             |
+------------------------------------------------------------------+
|                                                                   |
|   tcp_sendmsg()                                                   |
|        |                                                          |
|        v                                                          |
|   tcp_write_xmit()                                                |
|        |                                                          |
|        | (大数据块作为单个大SKB)                                  |
|        v                                                          |
|   tcp_transmit_skb()                                              |
|        |                                                          |
|        | skb_shinfo(skb)->gso_size = mss                          |
|        | skb_shinfo(skb)->gso_type = SKB_GSO_TCPV4                 |
|        v                                                          |
|   ip_queue_xmit() --> ip_output()                                 |
|        |                                                          |
|        v                                                          |
|   +----+----+                                                     |
|   |         |                                                     |
|   | (Hardware supports TSO?)                                      |
|   |         |                                                     |
|   +----+----+----+                                                |
|        |         |                                                |
|        v         v                                                |
|   [YES: TSO]  [NO: GSO Software Segmentation]                     |
|        |         |                                                |
|        |         v                                                |
|        |    dev_gso_segment()                                     |
|        |         |                                                |
|        |         v                                                |
|        |    skb_segment()  <-- 在软件中分段                       |
|        |         |                                                |
|        +----+----+                                                |
|             |                                                     |
|             v                                                     |
|   dev_hard_start_xmit()                                           |
|             |                                                     |
|             v                                                     |
|   ndo_start_xmit()  <-- TSO: 硬件分段                             |
|                         GSO: 发送多个小SKB                        |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** TSO（TCP Segmentation Offload）允许内核将大的TCP段交给网卡硬件分段。如果硬件不支持，则使用GSO（Generic Segmentation Offload）在发送前由软件分段。这大大减少了CPU开销。

---

## 6. Netfilter Hooks in TX Path

```
+------------------------------------------------------------------+
|                NETFILTER HOOKS IN TX PATH                         |
+------------------------------------------------------------------+
|                                                                   |
|   ip_local_out()                                                  |
|        |                                                          |
|        v                                                          |
|   +----+------------------------+                                 |
|   | NF_INET_LOCAL_OUT           |  <-- OUTPUT链                   |
|   | (iptables -t filter/nat     |      本地产生的数据包           |
|   |  -A OUTPUT)                 |                                 |
|   +----+------------------------+                                 |
|        |                                                          |
|        v                                                          |
|   dst_output() --> ip_output()                                    |
|        |                                                          |
|        v                                                          |
|   +----+------------------------+                                 |
|   | NF_INET_POST_ROUTING        |  <-- POSTROUTING链              |
|   | (iptables -t nat/mangle     |      离开主机前的最后处理       |
|   |  -A POSTROUTING)            |      SNAT在此处理               |
|   +----+------------------------+                                 |
|        |                                                          |
|        v                                                          |
|   ip_finish_output()                                              |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Netfilter在发送路径中提供两个钩子点：LOCAL_OUT用于处理本机产生的数据包（如防火墙过滤），POST_ROUTING用于数据包离开主机前的最后处理（如SNAT地址转换）。

