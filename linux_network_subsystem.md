# Linux 网络子系统框架深入讲解

基于 Linux 3.2 内核源码分析

![linux 网络发送路径](./linux/network/network_tx_path.md)  
![linux 网络接收路径](./linux/network/network_rx_path.md)  
![]()  
![]()  

---

## 目录

- [网络子系统架构概述](#网络子系统架构概述)
- [核心数据结构](#核心数据结构)
- [Socket 层](#socket-层)
- [传输层 (TCP/UDP)](#传输层-tcpudp)
- [网络层 (IP)](#网络层-ip)
- [链路层与设备驱动](#链路层与设备驱动)
- [数据包收发流程](#数据包收发流程)
- [关键源码文件](#关键源码文件)

---

## 网络子系统架构概述

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            用户空间 (User Space)                             │
│                                                                              │
│    socket()  bind()  listen()  accept()  connect()  send()  recv()          │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │ 系统调用
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           BSD Socket 接口层                                  │
│                                                                              │
│   sys_socket() ──► sock_create() ──► inet_create()                         │
│   sys_sendto() ──► sock_sendmsg() ──► inet_sendmsg()                       │
│                                                                              │
│   struct socket ──► struct sock                                             │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           传输层 (Transport Layer)                           │
│                                                                              │
│   ┌─────────────────────────────┐   ┌─────────────────────────────┐         │
│   │           TCP               │   │           UDP               │         │
│   │                             │   │                             │         │
│   │   tcp_sendmsg()             │   │   udp_sendmsg()             │         │
│   │   tcp_rcv()                 │   │   udp_rcv()                 │         │
│   │   tcp_transmit_skb()        │   │   udp_send_skb()            │         │
│   │                             │   │                             │         │
│   │   连接管理/可靠传输/拥塞控制 │   │   无连接/不可靠/简单快速    │         │
│   └─────────────────────────────┘   └─────────────────────────────┘         │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           网络层 (Network Layer)                             │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                          IPv4 / IPv6                                │   │
│   │                                                                     │   │
│   │   ip_queue_xmit() ──► ip_output() ──► ip_finish_output()           │   │
│   │   ip_rcv() ──► ip_rcv_finish() ──► ip_local_deliver()              │   │
│   │                                                                     │   │
│   │   路由查找 (fib_lookup)                                             │   │
│   │   分片/重组                                                         │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                     Netfilter (iptables)                            │   │
│   │   PREROUTING ──► INPUT/FORWARD ──► POSTROUTING                     │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                       邻居子系统 (Neighbour Subsystem)                       │
│                                                                              │
│   ARP (IPv4) / NDP (IPv6)                                                   │
│   MAC 地址解析                                                               │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          设备无关层 (Device Layer)                           │
│                                                                              │
│   dev_queue_xmit() ──► qdisc (流量控制) ──► dev_hard_start_xmit()          │
│   netif_receive_skb() ◄── NAPI ◄── 中断                                     │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          网络设备驱动层 (Drivers)                            │
│                                                                              │
│   e1000_xmit_frame()  /  e1000_clean_rx_irq()                               │
│   ixgbe_xmit_frame()  /  ixgbe_poll()                                       │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              网络硬件                                        │
│                          网卡 (NIC)                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 协议族与地址族

```c
// 协议族
#define AF_UNIX     1   // Unix 域
#define AF_INET     2   // IPv4
#define AF_INET6    10  // IPv6
#define AF_NETLINK  16  // Netlink
#define AF_PACKET   17  // 原始包

// Socket 类型
#define SOCK_STREAM    1   // TCP (面向连接)
#define SOCK_DGRAM     2   // UDP (数据报)
#define SOCK_RAW       3   // 原始 Socket
#define SOCK_SEQPACKET 5   // 有序可靠数据报
```

---

## 核心数据结构

### 1. struct sk_buff - 网络数据包

```c
// include/linux/skbuff.h
struct sk_buff {
    struct sk_buff      *next;          // 链表指针
    struct sk_buff      *prev;
    
    struct sock         *sk;            // 所属 socket
    struct net_device   *dev;           // 网络设备
    
    // 时间戳
    ktime_t             tstamp;
    
    // 数据指针
    unsigned char       *head;          // 缓冲区起始
    unsigned char       *data;          // 数据起始
    unsigned char       *tail;          // 数据结束
    unsigned char       *end;           // 缓冲区结束
    
    unsigned int        len;            // 数据长度
    unsigned int        data_len;       // 分片数据长度
    
    // 协议信息
    __u16               protocol;       // 链路层协议
    __u16               transport_header; // 传输层头偏移
    __u16               network_header;   // 网络层头偏移
    __u16               mac_header;       // MAC 头偏移
    
    // 校验和
    __u8                ip_summed;
    __wsum              csum;
    
    // 标志
    __u8                pkt_type;       // 包类型
    __u8                fclone;
    __u8                cloned;
    
    // 控制块 (各层私有数据)
    char                cb[48];
    
    // ...
};
```

### sk_buff 内存布局

```
                    sk_buff 结构
                         │
     ┌───────────────────┴───────────────────┐
     │                                       │
     ▼                                       ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ head │ headroom │ MAC 头 │ IP 头 │ TCP/UDP 头 │    数据    │ tailroom │ end │
└──┬────────────────┬─────────┬───────┬────────────┬──────────────┬───────┬───┘
   │                │         │       │            │              │       │
   head             │    mac_header   │      transport_header     tail   end
                    data              network_header
                    
headroom: 用于添加协议头 (发送时)
tailroom: 用于添加尾部数据
```

### 2. struct socket - BSD Socket

```c
// include/linux/net.h
struct socket {
    socket_state            state;      // 连接状态
    short                   type;       // SOCK_STREAM, SOCK_DGRAM
    unsigned long           flags;
    struct socket_wq        *wq;        // 等待队列
    struct file             *file;      // 关联的文件
    struct sock             *sk;        // 底层 sock
    const struct proto_ops  *ops;       // 协议操作
};

// 状态
typedef enum {
    SS_FREE = 0,            // 未分配
    SS_UNCONNECTED,         // 未连接
    SS_CONNECTING,          // 连接中
    SS_CONNECTED,           // 已连接
    SS_DISCONNECTING        // 断开中
} socket_state;
```

### 3. struct sock - 通用 Socket

```c
// include/net/sock.h
struct sock {
    struct sock_common      __sk_common;
    
    socket_lock_t           sk_lock;
    struct sk_buff_head     sk_receive_queue;   // 接收队列
    struct sk_buff_head     sk_write_queue;     // 发送队列
    
    int                     sk_rcvbuf;          // 接收缓冲区大小
    int                     sk_sndbuf;          // 发送缓冲区大小
    
    atomic_t                sk_rmem_alloc;      // 已分配接收内存
    atomic_t                sk_wmem_alloc;      // 已分配发送内存
    
    int                     sk_forward_alloc;   // 预分配内存
    
    struct dst_entry        *sk_dst_cache;      // 路由缓存
    
    struct sk_filter        *sk_filter;         // BPF 过滤器
    
    unsigned long           sk_flags;
    unsigned long           sk_lingertime;      // SO_LINGER 时间
    
    struct socket           *sk_socket;         // 关联的 socket
    
    void                    (*sk_state_change)(struct sock *sk);
    void                    (*sk_data_ready)(struct sock *sk, int bytes);
    void                    (*sk_write_space)(struct sock *sk);
    void                    (*sk_error_report)(struct sock *sk);
    
    struct proto            *sk_prot;           // 协议操作
    
    // ...
};
```

### 4. struct net_device - 网络设备

```c
// include/linux/netdevice.h
struct net_device {
    char                    name[IFNAMSIZ];     // 设备名 (eth0)
    
    unsigned long           state;              // 设备状态
    
    struct net_device_stats stats;              // 统计信息
    
    unsigned int            flags;              // IFF_UP, IFF_BROADCAST
    unsigned int            mtu;                // 最大传输单元
    unsigned short          type;               // 硬件类型
    unsigned short          hard_header_len;    // 硬件头长度
    
    unsigned char           dev_addr[MAX_ADDR_LEN]; // MAC 地址
    
    struct netdev_queue     *_tx;               // 发送队列
    unsigned int            num_tx_queues;
    
    struct netdev_rx_queue  *_rx;               // 接收队列
    unsigned int            num_rx_queues;
    
    const struct net_device_ops *netdev_ops;    // 设备操作
    const struct ethtool_ops *ethtool_ops;      // ethtool 操作
    
    struct Qdisc            *qdisc;             // 流量控制
    
    // NAPI
    struct list_head        napi_list;
    
    // ...
};

struct net_device_ops {
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);
    int (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    // ...
};
```

---

## Socket 层

### Socket 创建流程

```
socket(AF_INET, SOCK_STREAM, 0)
            │
            ▼
      sys_socket()
            │
            ▼
      sock_create()
            │
            ├── 分配 struct socket
            │
            └── __sock_create()
                    │
                    └── 查找协议族: net_families[AF_INET]
                            │
                            ▼
                    inet_create()
                            │
                            ├── 分配 struct sock (inet_sock)
                            │
                            ├── 根据 type 设置协议:
                            │   SOCK_STREAM → TCP
                            │   SOCK_DGRAM  → UDP
                            │
                            └── 初始化 sock 结构
            │
            ▼
      sock_map_fd()
            │
            └── 分配文件描述符
            │
            ▼
      返回 fd
```

### 协议操作函数表

```c
// TCP 协议操作
const struct proto tcp_prot = {
    .name           = "TCP",
    .close          = tcp_close,
    .connect        = tcp_v4_connect,
    .accept         = inet_csk_accept,
    .sendmsg        = tcp_sendmsg,
    .recvmsg        = tcp_recvmsg,
    .sendpage       = tcp_sendpage,
    .backlog_rcv    = tcp_v4_do_rcv,
    .hash           = inet_hash,
    .unhash         = inet_unhash,
    // ...
};

// UDP 协议操作
const struct proto udp_prot = {
    .name           = "UDP",
    .close          = udp_lib_close,
    .connect        = ip4_datagram_connect,
    .sendmsg        = udp_sendmsg,
    .recvmsg        = udp_recvmsg,
    .hash           = udp_lib_hash,
    .unhash         = udp_lib_unhash,
    // ...
};
```

---

## 传输层 (TCP/UDP)

### TCP 连接状态机

```
                              ┌─────────────────┐
                              │     CLOSED      │
                              └────────┬────────┘
                                       │
          ┌────────────────────────────┼────────────────────────────┐
          │                            │                            │
    被动打开 (listen)            主动打开 (connect)                   │
          │                            │                            │
          ▼                            ▼                            │
   ┌─────────────┐              ┌─────────────┐                     │
   │   LISTEN    │              │  SYN_SENT   │                     │
   └──────┬──────┘              └──────┬──────┘                     │
          │                            │                            │
    收到 SYN                     收到 SYN+ACK                        │
    发送 SYN+ACK                 发送 ACK                            │
          │                            │                            │
          ▼                            ▼                            │
   ┌─────────────┐              ┌─────────────┐                     │
   │  SYN_RCVD   │              │ ESTABLISHED │◄────────────────────┘
   └──────┬──────┘              └──────┬──────┘     收到 ACK
          │                            │
    收到 ACK                     主动关闭 (close)
          │                     发送 FIN
          ▼                            │
   ┌─────────────┐                     ▼
   │ ESTABLISHED │              ┌─────────────┐
   └─────────────┘              │  FIN_WAIT_1 │
                                └──────┬──────┘
                                       │
                          ┌────────────┼────────────┐
                          │            │            │
                    收到 ACK     收到 FIN+ACK    收到 FIN
                          │       发送 ACK      发送 ACK
                          ▼            │            │
                   ┌─────────────┐     │            ▼
                   │  FIN_WAIT_2 │     │     ┌─────────────┐
                   └──────┬──────┘     │     │   CLOSING   │
                          │            │     └──────┬──────┘
                    收到 FIN           │      收到 ACK
                    发送 ACK           │            │
                          │            │            │
                          ▼            ▼            ▼
                   ┌─────────────────────────────────────┐
                   │              TIME_WAIT              │
                   │           (2*MSL 超时)              │
                   └──────────────────┬──────────────────┘
                                      │
                                      ▼
                               ┌─────────────┐
                               │   CLOSED    │
                               └─────────────┘
```

### TCP 发送流程

```
tcp_sendmsg()
      │
      ├── 等待连接建立 (如果需要)
      │
      ├── 分配 sk_buff
      │
      ├── skb_copy_to_page_nocache() ── 复制用户数据
      │
      └── __tcp_push_pending_frames()
              │
              ▼
        tcp_write_xmit()
              │
              ├── 拥塞窗口检查
              │
              ├── 发送窗口检查
              │
              └── tcp_transmit_skb()
                      │
                      ├── 构造 TCP 头
                      │
                      ├── 计算校验和
                      │
                      └── ip_queue_xmit()
                              │
                              ▼
                          IP 层处理
```

### UDP 发送流程

```
udp_sendmsg()
      │
      ├── 获取目的地址
      │
      ├── 路由查找 (ip_route_output_flow)
      │
      ├── 分配 sk_buff
      │
      ├── 复制用户数据
      │
      └── udp_send_skb()
              │
              ├── 构造 UDP 头
              │
              ├── 计算校验和 (可选)
              │
              └── ip_send_skb()
                      │
                      ▼
                  IP 层处理
```

---

## 网络层 (IP)

### IP 发送流程

```
ip_queue_xmit() 或 ip_push_pending_frames()
      │
      ▼
ip_local_out()
      │
      ├── Netfilter: LOCAL_OUT hook
      │
      └── dst_output()
              │
              ▼
        ip_output()
              │
              ├── Netfilter: POST_ROUTING hook
              │
              └── ip_finish_output()
                      │
                      ├── 分片 (如果需要)
                      │   ip_fragment()
                      │
                      └── ip_finish_output2()
                              │
                              ├── 邻居解析 (ARP)
                              │
                              └── dst_neigh_output()
                                      │
                                      ▼
                                  dev_queue_xmit()
```

### IP 接收流程

```
netif_receive_skb()
      │
      ▼
__netif_receive_skb()
      │
      ├── 协议分发: ptype_all, ptype_base
      │
      └── ip_rcv() (IPv4)
              │
              ├── 校验 IP 头
              │
              ├── Netfilter: PRE_ROUTING hook
              │
              └── ip_rcv_finish()
                      │
                      ├── 路由查找
                      │
                      └── dst_input()
                              │
                      ┌───────┴───────┐
                      ▼               ▼
              ip_local_deliver()  ip_forward()
              (本地接收)          (转发)
                      │               │
                      ├── Netfilter   ├── Netfilter
                      │   INPUT       │   FORWARD
                      │               │
                      ▼               ▼
              ip_local_deliver_finish() ip_forward_finish()
                      │               │
                      ▼               ▼
              传输层处理         ip_output()
              (tcp_v4_rcv/udp_rcv)
```

### 路由查找

```c
// net/ipv4/fib_frontend.c
int fib_lookup(struct net *net, struct flowi4 *flp, struct fib_result *res)
{
    // 查找路由表
    // FIB (Forwarding Information Base)
}

// 路由缓存 (已废弃，3.6 版本后移除)
// 现在使用 FIB Trie
```

---

## 链路层与设备驱动

### 发送流程

```
dev_queue_xmit()
      │
      ├── __dev_queue_xmit()
      │       │
      │       ├── 获取发送队列
      │       │
      │       └── __dev_xmit_skb()
      │               │
      │               ├── qdisc (流量控制)
      │               │   如: pfifo_fast, htb, tbf
      │               │
      │               └── sch_direct_xmit()
      │                       │
      │                       └── dev_hard_start_xmit()
      │                               │
      │                               └── ndo_start_xmit()
      │                                       │
      │                                       ▼
      │                               驱动发送函数
      │                               e1000_xmit_frame()
      │
      └── 返回发送状态
```

### 接收流程 (NAPI)

```
网卡中断
    │
    ▼
e1000_intr() (驱动中断处理)
    │
    ├── 禁用网卡中断
    │
    └── napi_schedule()
            │
            ▼
        __napi_schedule()
            │
            └── 将 napi 添加到 poll_list
                触发软中断 NET_RX_SOFTIRQ

软中断处理
    │
    ▼
net_rx_action()
    │
    └── for each napi in poll_list:
            │
            └── napi->poll() ── 如 e1000_clean()
                    │
                    ├── 从 ring buffer 读取数据包
                    │
                    ├── 分配 sk_buff
                    │
                    └── napi_gro_receive() 或 netif_receive_skb()
                            │
                            ▼
                        协议栈处理
```

### NAPI 架构

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              NAPI 机制                                       │
│                                                                              │
│   传统中断模式:                                                               │
│   每个包一个中断 ──► 高频率中断 ──► CPU 开销大                                 │
│                                                                              │
│   NAPI 模式:                                                                  │
│   ┌───────┐    ┌───────────┐    ┌───────────┐    ┌───────────┐             │
│   │ 中断  │───►│ 禁用中断  │───►│  轮询模式  │───►│ 启用中断  │             │
│   └───────┘    └───────────┘    │   (poll)  │    └───────────┘             │
│                                 │           │                                │
│                                 │ 批量处理  │                                │
│                                 │  多个包   │                                │
│                                 └───────────┘                                │
│                                                                              │
│   优点:                                                                       │
│   - 高负载时减少中断次数                                                      │
│   - 批量处理提高效率                                                          │
│   - 自适应: 低负载时仍使用中断                                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 数据包收发流程

### 完整发送流程

```
用户空间: send(fd, buf, len, 0)
                │
                ▼
┌───────────────────────────────────────────────────────────────┐
│ Socket 层                                                      │
│   sys_sendto() → sock_sendmsg() → inet_sendmsg()              │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 传输层 (TCP)                                                   │
│   tcp_sendmsg()                                                │
│   - 分段 (MSS)                                                 │
│   - 构建 TCP 头                                                │
│   - 计算校验和                                                 │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 网络层 (IP)                                                    │
│   ip_queue_xmit() → ip_local_out() → ip_output()              │
│   - 构建 IP 头                                                 │
│   - 路由查找                                                   │
│   - 分片 (如果 > MTU)                                          │
│   - Netfilter 处理                                             │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 邻居子系统                                                     │
│   neigh_resolve_output()                                       │
│   - ARP 解析 MAC 地址                                          │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 设备层                                                         │
│   dev_queue_xmit() → qdisc → dev_hard_start_xmit()            │
│   - 流量控制                                                   │
│   - 构建以太网帧头                                             │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 驱动层                                                         │
│   ndo_start_xmit() → DMA → 网卡                               │
└───────────────────────────────────────────────────────────────┘
```

### 完整接收流程

```
网卡接收数据 → 中断
                │
                ▼
┌───────────────────────────────────────────────────────────────┐
│ 驱动层                                                         │
│   硬中断 → napi_schedule() → 软中断                           │
│   napi_poll() → 从 ring buffer 读取                           │
│   napi_gro_receive()                                          │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 设备层                                                         │
│   netif_receive_skb()                                         │
│   - 协议类型分发                                               │
│   - 抓包 (tcpdump)                                             │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 网络层 (IP)                                                    │
│   ip_rcv() → ip_rcv_finish()                                  │
│   - 验证 IP 头                                                 │
│   - 路由决策 (本地/转发)                                       │
│   - 重组分片                                                   │
│   - Netfilter 处理                                             │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ 传输层 (TCP)                                                   │
│   tcp_v4_rcv()                                                │
│   - 验证校验和                                                 │
│   - 查找对应 socket                                            │
│   - 处理 TCP 状态机                                            │
│   - 放入接收队列                                               │
│   - 唤醒等待进程                                               │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
┌───────────────────────────────────────────────────────────────┐
│ Socket 层                                                      │
│   sock_recvmsg() → tcp_recvmsg()                              │
│   - 从接收队列复制数据到用户空间                                │
└───────────────────────────────────┬───────────────────────────┘
                                    │
                                    ▼
用户空间: recv() 返回
```

---

## 关键源码文件

### Socket 层

| 文件 | 功能 |
|------|------|
| `net/socket.c` | BSD Socket 接口 |
| `net/core/sock.c` | sock 操作 |

### 传输层

| 文件 | 功能 |
|------|------|
| `net/ipv4/tcp.c` | TCP 核心 |
| `net/ipv4/tcp_input.c` | TCP 接收 |
| `net/ipv4/tcp_output.c` | TCP 发送 |
| `net/ipv4/tcp_ipv4.c` | TCP over IPv4 |
| `net/ipv4/udp.c` | UDP |

### 网络层

| 文件 | 功能 |
|------|------|
| `net/ipv4/ip_input.c` | IP 接收 |
| `net/ipv4/ip_output.c` | IP 发送 |
| `net/ipv4/ip_forward.c` | IP 转发 |
| `net/ipv4/route.c` | 路由 |
| `net/ipv4/fib_*.c` | 路由表 |
| `net/ipv4/arp.c` | ARP |

### 设备层

| 文件 | 功能 |
|------|------|
| `net/core/dev.c` | 设备核心 |
| `net/core/skbuff.c` | sk_buff 管理 |
| `net/sched/sch_*.c` | 流量控制 |

### Netfilter

| 文件 | 功能 |
|------|------|
| `net/netfilter/core.c` | Netfilter 核心 |
| `net/netfilter/nf_conntrack*.c` | 连接跟踪 |
| `net/netfilter/xt_*.c` | iptables 模块 |

---

## 总结

### 网络子系统核心机制

1. **分层设计**: Socket → 传输 → 网络 → 设备 → 驱动
2. **sk_buff**: 贯穿整个协议栈的数据结构
3. **NAPI**: 高效的数据包接收机制
4. **Netfilter**: 灵活的包过滤框架

### 设计亮点

1. **零拷贝优化**: splice, sendfile
2. **GRO/GSO**: 减少协议栈处理次数
3. **多队列**: 支持多核并行处理
4. **协议无关**: 统一的 socket 接口

---

*本文档基于 Linux 3.2 内核源码分析*

