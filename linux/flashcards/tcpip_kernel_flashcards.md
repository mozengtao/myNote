# TCP/IP Networking Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel internals, data structures, call paths, and APIs
> **Language**: English terms with Chinese explanations
> **Total Cards**: 80+

---

## 1. Core Data Structures (核心数据结构)

---

Q: What is `struct sk_buff` and why is it central to Linux networking?
A: `sk_buff` (socket buffer) 是Linux网络栈的核心数据结构，用于表示一个网络数据包。它贯穿从用户空间到网卡驱动的整个路径，包含数据指针、协议元数据、路由信息等。每个经过内核的数据包都被封装在一个sk_buff中。
[Basic]

---

Q: In `struct sk_buff`, what do the four pointers `head`, `data`, `tail`, and `end` represent?
A: 
```
head  --> +------------------+  缓冲区开始（固定）
          |   headroom       |  预留空间（添加协议头）
data  --> +------------------+  实际数据开始
          |   packet data    |  数据包内容
tail  --> +------------------+  实际数据结束
          |   tailroom       |  尾部预留空间
end   --> +------------------+  缓冲区结束（固定）
```
`head`/`end`固定不变，`data`/`tail`随协议层处理移动。
[Basic]

---

Q: What is the difference between `skb->len` and `skb->data_len`?
A: 
- `skb->len`: 数据包的总长度（包括线性区域+分页数据）
- `skb->data_len`: 分页数据（paged data/frags）的长度
- 线性数据长度 = `skb->len - skb->data_len` = `skb_headlen(skb)`

当数据包使用scatter-gather时，部分数据存储在`skb_shinfo(skb)->frags[]`中。
[Intermediate]

---

Q: What is `skb_shinfo(skb)` and what does it contain?
A: `skb_shinfo(skb)`返回`struct skb_shared_info`指针，存储在skb缓冲区末尾（end指针处）。包含：
```c
struct skb_shared_info {
    __u8 nr_frags;              // 分页片段数量
    __u8 gso_type;              // GSO类型
    unsigned short gso_size;     // GSO分段大小
    skb_frag_t frags[MAX_SKB_FRAGS]; // 分页数据数组
    struct sk_buff *frag_list;  // SKB链表（IP分片）
    struct skb_shared_hwtstamps hwtstamps; // 硬件时间戳
};
```
[Intermediate]

---

Q: [Cloze] Linux socket层使用`struct ______`作为BSD socket接口的抽象，而网络层使用`struct ______`存储协议状态。
A: `socket` 和 `sock`

`struct socket`是用户空间可见的抽象，`struct sock`是网络层内部的协议控制块，包含连接状态、缓冲区、回调函数等。
[Basic]

---

Q: What is `struct tcp_sock` and how does it relate to `struct sock`?
A: `tcp_sock`是TCP协议专用的socket结构，通过嵌入继承实现：
```
struct tcp_sock {
    struct inet_connection_sock  inet_conn;
        struct inet_sock         inet;
            struct sock          sk;      // 基类
}
```
使用`tcp_sk(sk)`宏可以从`sock*`转换为`tcp_sock*`。这是C语言实现面向对象继承的典型模式。
[Intermediate]

---

Q: What key TCP state variables are stored in `struct tcp_sock`?
A: 
| 字段 | 含义 |
|------|------|
| `snd_una` | Send Unacknowledged - 第一个未确认的序列号 |
| `snd_nxt` | Send Next - 下一个要发送的序列号 |
| `rcv_nxt` | Receive Next - 下一个期望接收的序列号 |
| `snd_wnd` | Send Window - 对端通告的发送窗口 |
| `rcv_wnd` | Receive Window - 本地接收窗口 |
| `snd_cwnd` | Congestion Window - 拥塞窗口 |
| `snd_ssthresh` | Slow Start Threshold - 慢启动阈值 |
| `mss_cache` | Maximum Segment Size - 缓存的MSS值 |
[Intermediate]

---

Q: What is `struct dst_entry` and what does it represent?
A: `dst_entry`是路由缓存条目，存储数据包的路由/目的地信息：
```c
struct dst_entry {
    struct net_device *dev;       // 出口设备
    int (*input)(struct sk_buff *);   // 本地递送函数 (ip_local_deliver)
    int (*output)(struct sk_buff *);  // 发送函数 (ip_output)
    unsigned long expires;        // 过期时间
    int error;                    // ICMP错误码
    struct neighbour *neighbour;  // 邻居条目
};
```
skb通过`skb_dst(skb)`获取关联的路由信息。
[Advanced]

---

Q: What is `struct neighbour` used for in the Linux kernel?
A: `neighbour`结构表示邻居子系统中的一个邻居节点（L2地址解析）：
```c
struct neighbour {
    struct net_device *dev;       // 出口设备
    unsigned char ha[MAX_ADDR_LEN]; // 硬件地址(MAC)
    __u8 nud_state;               // 邻居状态 (NUD_REACHABLE等)
    int (*output)(struct neighbour *, struct sk_buff *);
    struct hh_cache hh;           // 缓存的L2头
    struct timer_list timer;      // 超时定时器
};
```
用于ARP/NDP解析，缓存IP到MAC的映射。
[Advanced]

---

Q: What is `struct net_device` and what key fields does it contain?
A: `net_device`表示一个网络接口（物理或虚拟）：
```c
struct net_device {
    char name[IFNAMSIZ];          // 设备名 (eth0, lo)
    unsigned char dev_addr[MAX_ADDR_LEN]; // MAC地址
    unsigned int mtu;             // 最大传输单元
    unsigned int flags;           // IFF_UP, IFF_PROMISC等
    const struct net_device_ops *netdev_ops; // 设备操作函数
    struct netdev_queue *_tx;     // 发送队列
    unsigned int num_tx_queues;   // 发送队列数
    netdev_features_t features;   // 硬件特性 (TSO, checksum offload)
    struct Qdisc *qdisc;          // 流量控制
};
```
[Intermediate]

---

Q: What is `struct inet_hashinfo` and what hash tables does it contain?
A: `inet_hashinfo`管理TCP连接的哈希表：
```c
struct inet_hashinfo {
    struct inet_ehash_bucket *ehash;      // Established连接哈希表
    spinlock_t *ehash_locks;              // ehash锁数组
    unsigned int ehash_mask;              // 哈希掩码
    
    struct inet_listen_hashbucket *lhash; // Listening socket哈希表
    
    struct inet_bind_hashbucket *bhash;   // 端口绑定哈希表
    unsigned int bhash_size;
};
```
全局实例`tcp_hashinfo`。接收包时用四元组在ehash中查找对应socket。
[Advanced]

---

Q: What is `struct napi_struct` used for?
A: `napi_struct`是NAPI轮询的核心结构：
```c
struct napi_struct {
    struct list_head poll_list;  // 加入softnet_data轮询列表
    unsigned long state;          // NAPI_STATE_SCHED等
    int weight;                   // 每次轮询处理的最大包数(通常64)
    int (*poll)(struct napi_struct *, int); // 驱动轮询函数
    struct net_device *dev;       // 关联网络设备
    struct sk_buff *gro_list;     // GRO聚合列表
    struct sk_buff *skb;          // 当前处理的SKB
};
```
每个网卡队列对应一个napi_struct实例。
[Advanced]

---

Q: What is `struct softnet_data` and why is it per-CPU?
A: `softnet_data`是每CPU的网络处理数据结构：
```c
DECLARE_PER_CPU_ALIGNED(struct softnet_data, softnet_data);

struct softnet_data {
    struct list_head poll_list;       // NAPI轮询列表
    struct sk_buff_head input_pkt_queue; // 输入包队列
    struct napi_struct backlog;       // 后备NAPI
    unsigned int processed;           // 已处理包数
    unsigned int dropped;             // 丢弃包数
};
```
Per-CPU设计避免多核竞争，提高并行处理能力。
[Advanced]

---

## 2. SKB Operations (SKB操作函数)

---

Q: What function adds space at the beginning of an `sk_buff` for protocol headers?
A: `skb_push(skb, len)` - 将`skb->data`指针向前移动len字节，为添加协议头腾出空间，同时增加`skb->len`。
```c
// 添加TCP头
th = (struct tcphdr *)skb_push(skb, sizeof(struct tcphdr));
```
反向操作是`skb_pull()`，用于剥离协议头。
[Basic]

---

Q: What is the difference between `skb_put()` and `skb_push()`?
A: 
```
skb_push(skb, len):    在头部添加空间
  data向前移动，len增加
  用于添加协议头

skb_put(skb, len):     在尾部添加空间  
  tail向后移动，len增加
  用于追加数据

skb_pull(skb, len):    从头部移除数据
  data向后移动，len减少
  用于剥离协议头

skb_trim(skb, len):    裁剪到指定长度
  修改tail和len
  用于截断数据
```
[Basic]

---

Q: What is the purpose of `skb_reserve()`?
A: `skb_reserve(skb, len)`在新分配的skb中预留头部空间：
```c
skb = alloc_skb(size, GFP_KERNEL);
skb_reserve(skb, NET_IP_ALIGN + ETH_HLEN + sizeof(struct iphdr) + sizeof(struct tcphdr));
```
移动`data`和`tail`指针，为后续`skb_push()`添加协议头预留空间。必须在添加任何数据之前调用。
[Basic]

---

Q: [Code] What does this kernel code do?
```c
skb_reserve(skb, NET_IP_ALIGN);
```
A: 在skb头部预留`NET_IP_ALIGN`（通常2字节）的空间，使IP头部对齐到4字节边界。

原因：以太网头14字节，加2字节后IP头从16字节偏移开始，实现4字节对齐，提高CPU访问效率。
[Intermediate]

---

Q: What is the purpose of `skb_clone()` vs `skb_copy()`?
A: 
| 函数 | 操作 | 数据共享 | 使用场景 |
|------|------|----------|----------|
| `skb_clone()` | 浅拷贝 | 共享数据缓冲区 | 广播/多播，数据只读 |
| `skb_copy()` | 深拷贝 | 独立缓冲区 | 需要修改数据 |
| `pskb_copy()` | 部分拷贝 | 仅拷贝线性部分 | 需要修改头部 |

`skb_clone()`时使用`skb_shared_info->dataref`引用计数管理共享。
[Intermediate]

---

Q: What does `skb_linearize()` do and when is it needed?
A: `skb_linearize(skb)`将分散的数据（paged frags）合并到连续的线性缓冲区：
```c
if (skb_is_nonlinear(skb)) {
    if (skb_linearize(skb))
        goto drop;  // 内存分配失败
}
// 现在可以安全地用skb->data访问所有数据
```
当需要直接访问完整数据、但数据分散在多个页面时使用。开销较大，应尽量避免。
[Intermediate]

---

Q: What is `skb_headroom()` and `skb_tailroom()`?
A: 
```c
skb_headroom(skb) = skb->data - skb->head   // 头部可用空间
skb_tailroom(skb) = skb->end - skb->tail    // 尾部可用空间
```
用于检查是否有足够空间添加协议头/尾：
```c
if (skb_headroom(skb) < hdr_len) {
    // 需要重新分配更大的缓冲区
    skb = skb_realloc_headroom(skb, hdr_len);
}
```
[Basic]

---

Q: What does `pskb_may_pull()` do?
A: `pskb_may_pull(skb, len)`确保skb的线性区域至少有len字节：
```c
if (!pskb_may_pull(skb, sizeof(struct iphdr)))
    goto drop;

// 现在可以安全访问IP头
iph = ip_hdr(skb);
```
如果数据在分页区域，会将其拷贝到线性区域。协议处理前必须调用，确保头部可访问。
[Intermediate]

---

## 3. Transmit Path (发送路径)

---

Q: What is the call path from `send()` syscall to the NIC driver in Linux?
A: 
```
send() / sendmsg()           [用户空间]
    |
    v
sys_sendto() / sys_sendmsg() [系统调用入口]
    |
    v
sock_sendmsg()               [Socket层，LSM检查]
    |
    v
inet_sendmsg()               [AF_INET协议族]
    |
    v
tcp_sendmsg()                [TCP传输层，分段、发送缓冲]
    |
    v
tcp_write_xmit()             [TCP发送队列处理]
    |
    v
tcp_transmit_skb()           [构建TCP头，计算校验和]
    |
    v
ip_queue_xmit()              [IP层，路由查找]
    |
    v
ip_local_out()               [Netfilter OUTPUT]
    |
    v
ip_output()                  [Netfilter POSTROUTING]
    |
    v
ip_finish_output()           [分片检查]
    |
    v
neigh_resolve_output()       [邻居解析，ARP]
    |
    v
dev_queue_xmit()             [设备层，Qdisc]
    |
    v
ndo_start_xmit()             [驱动发送]
```
[Intermediate]

---

Q: What is the purpose of `sk->sk_prot` in `struct sock`?
A: `sk->sk_prot`指向`struct proto`，定义了传输层协议的操作函数表：
```c
struct proto tcp_prot = {
    .name       = "TCP",
    .close      = tcp_close,
    .connect    = tcp_v4_connect,
    .accept     = inet_csk_accept,
    .sendmsg    = tcp_sendmsg,
    .recvmsg    = tcp_recvmsg,
    .backlog_rcv = tcp_v4_do_rcv,
    .hash       = inet_hash,
    .unhash     = inet_unhash,
    .init       = tcp_v4_init_sock,
};
```
这是Strategy模式在内核中的典型应用，UDP使用`udp_prot`。
[Intermediate]

---

Q: What is the difference between `struct proto_ops` and `struct proto`?
A: 
| 结构体 | 位置 | 功能 | 示例 |
|--------|------|------|------|
| `proto_ops` | `socket->ops` | BSD socket接口层操作 | `inet_stream_ops` |
| `proto` | `sk->sk_prot` | 传输层协议操作 | `tcp_prot` |

调用链：
```
socket->ops->sendmsg()      = inet_sendmsg()
    |
    v
sk->sk_prot->sendmsg()      = tcp_sendmsg()
```
`proto_ops`是统一接口，`proto`是具体协议实现。
[Advanced]

---

Q: What happens in `tcp_sendmsg()`?
A: `tcp_sendmsg()`是TCP发送的核心函数：
1. 获取socket锁 `lock_sock(sk)`
2. 等待连接建立（如果需要）
3. 计算MSS和发送窗口
4. 循环处理用户数据：
   - 分配或复用skb
   - 从用户空间拷贝数据到skb
   - 加入发送队列 `sk->sk_write_queue`
5. 调用`tcp_push()`触发发送
6. 释放锁 `release_sock(sk)`
[Intermediate]

---

Q: What happens when `tcp_sendmsg()` is called but the send buffer is full?
A: 当发送缓冲区满时（`!sk_stream_memory_free(sk)`）：
1. 如果设置`MSG_DONTWAIT`，立即返回`-EAGAIN`
2. 否则调用`sk_stream_wait_memory()`阻塞等待
3. 等待条件：`sk_stream_wspace(sk) >= sk_stream_min_wspace(sk)`
4. 当对端ACK释放缓冲区空间时，`sk->sk_write_space()`回调被调用
5. 等待进程被唤醒继续发送

缓冲区大小可通过`SO_SNDBUF`或`net.core.wmem_*`调整。
[Advanced]

---

Q: What is the purpose of `tcp_transmit_skb()`?
A: `tcp_transmit_skb()`构建TCP头并发送单个skb：
```c
int tcp_transmit_skb(struct sock *sk, struct sk_buff *skb,
                     int clone_it, gfp_t gfp_mask)
{
    // 1. 如果需要克隆skb（重传时保留原件）
    if (clone_it)
        skb = skb_clone(skb, gfp_mask);
    
    // 2. 构建TCP头
    th = tcp_hdr(skb);
    th->source = inet->inet_sport;
    th->dest = inet->inet_dport;
    th->seq = htonl(tcb->seq);
    th->ack_seq = htonl(tp->rcv_nxt);
    
    // 3. 计算校验和
    tcp_options_write((__be32 *)(th + 1), tp, &opts);
    
    // 4. 调用IP层发送
    err = icsk->icsk_af_ops->queue_xmit(skb);
}
```
[Advanced]

---

Q: What is the function of `dev_queue_xmit()`?
A: `dev_queue_xmit()`是数据包进入设备层的入口：
1. 选择TX队列（多队列网卡）
2. 获取队列锁
3. 调用流量控制Qdisc入队：`q->enqueue(skb, q)`
4. 运行Qdisc：`__qdisc_run(q)`
5. 调用`sch_direct_xmit()`直接发送或排队
6. 最终调用`dev_hard_start_xmit()` → `ndo_start_xmit()`

如果驱动返回`NETDEV_TX_BUSY`，skb会被重新入队。
[Intermediate]

---

Q: What is TSO/GSO and how does it work in the kernel?
A: 
**TSO** (TCP Segmentation Offload): 硬件分段
**GSO** (Generic Segmentation Offload): 软件分段

工作流程：
```
tcp_sendmsg()
    |
    v
tcp_write_xmit() -- 创建大skb (64KB)
    |               skb_shinfo(skb)->gso_size = mss
    |               skb_shinfo(skb)->gso_type = SKB_GSO_TCPV4
    v
dev_queue_xmit()
    |
    +---> 硬件支持TSO?
    |         |
    |    YES  |  NO
    |         v
    |    dev_gso_segment() -> skb_segment()
    |         |
    |         v
    |    分成多个mss大小的skb
    v
ndo_start_xmit() -- TSO: 硬件分段
                    GSO: 发送多个小skb
```
[Intermediate]

---

## 4. Receive Path (接收路径)

---

Q: What is the complete receive path from NIC to application?
A: 
```
[NIC Hardware]
    |
    v
Hardware Interrupt → napi_schedule()     [禁用网卡中断]
    |
    v
NET_RX_SOFTIRQ → net_rx_action()         [软中断]
    |
    v
napi_poll() → driver's poll()            [NAPI轮询]
    |
    v
napi_gro_receive()                        [GRO聚合]
    |
    v
netif_receive_skb()                       [通用接收]
    |
    v
__netif_receive_skb_core()               [协议分发]
    |
    +---> ptype_all (tcpdump)
    +---> rx_handler (bridge)
    +---> ptype_base[protocol]
    |
    v
ip_rcv()                                  [IP层]
    |
    v
NF_INET_PRE_ROUTING → ip_rcv_finish()    [Netfilter]
    |
    v
ip_route_input() → ip_local_deliver()    [路由决策]
    |
    v
NF_INET_LOCAL_IN → ip_local_deliver_finish()
    |
    v
tcp_v4_rcv()                              [TCP层]
    |
    v
tcp_v4_do_rcv() → tcp_rcv_established()  [TCP状态机]
    |
    v
sk->sk_data_ready()                       [唤醒应用]
    |
    v
tcp_recvmsg() → copy_to_user()           [用户读取]
```
[Intermediate]

---

Q: What is NAPI and why was it introduced?
A: NAPI (New API) 结合中断和轮询的网络接收机制：

**传统中断模式问题**：每个包一次中断，高负载时中断风暴

**NAPI工作流程**：
1. 第一个包触发硬件中断
2. 中断处理程序：禁用网卡中断 + `napi_schedule()`
3. 软中断调用`net_rx_action()`
4. 轮询网卡，批量处理数据包
5. 队列空后`napi_complete()` + 重新启用中断

**优点**：高负载减少中断，低负载保持低延迟
[Intermediate]

---

Q: What is GRO and how does it differ from LRO?
A: 
| 特性 | LRO (Large Receive Offload) | GRO (Generic Receive Offload) |
|------|---------------------------|------------------------------|
| 实现位置 | 硬件/固件 | 软件（内核） |
| 保留包边界 | 否（可能破坏） | 是（可还原） |
| 转发支持 | 不适合 | 支持 |
| 灵活性 | 低 | 高（协议可扩展） |

GRO合并条件（TCP）：
- 相同流（四元组）
- 连续序列号
- 相同标志（除PSH外）
- 合并后不超过64KB
[Intermediate]

---

Q: What is the difference between `netif_rx()` and `netif_receive_skb()`?
A: 
| 函数 | 上下文 | 使用场景 |
|------|--------|----------|
| `netif_rx()` | 中断上下文 | 传统驱动，入队到`input_pkt_queue`后返回 |
| `netif_receive_skb()` | 软中断/NAPI上下文 | NAPI驱动，直接处理进协议栈 |
| `napi_gro_receive()` | NAPI轮询 | 支持GRO聚合，现代驱动推荐 |

现代驱动应使用NAPI + `napi_gro_receive()`。
[Intermediate]

---

Q: What are the three queues in a TCP socket for receiving data?
A: 
```
+------------------+     +------------------+     +------------------+
|  Receive Queue   |     |  Backlog Queue   |     |    Prequeue      |
|------------------|     |------------------|     |------------------|
| sk->sk_receive_  |     | sk->sk_backlog   |     | tp->ucopy.       |
|     queue        |     |                  |     |     prequeue     |
+------------------+     +------------------+     +------------------+
        |                        |                        |
        v                        v                        v
  按序、可读取的数据       socket被用户锁定时       直接复制到用户空间
                          暂存的数据包             的优化路径
```
数据到达时根据`sock_owned_by_user(sk)`选择队列。
[Advanced]

---

Q: [Code] What does this code check?
```c
if (sk->sk_state == TCP_ESTABLISHED) {
    if (tcp_rcv_established(sk, skb, tcp_hdr(skb), skb->len))
        goto reset;
}
```
A: 检查TCP连接是否处于ESTABLISHED状态，如果是则调用快速路径`tcp_rcv_established()`处理数据包。

这是TCP接收的"fast path"优化——对于已建立连接的正常数据传输，跳过完整的状态机检查，直接处理数据和ACK，提高性能。
[Advanced]

---

Q: What is the purpose of `sk->sk_data_ready` callback?
A: `sk->sk_data_ready`是socket的数据就绪回调函数：
```c
// 当有数据到达接收队列时被调用
sk->sk_data_ready(sk, skb_len);
```
默认实现`sock_def_readable()`：
```c
void sock_def_readable(struct sock *sk, int len)
{
    if (sk_has_sleeper(sk))
        wake_up_interruptible_sync_poll(sk_sleep(sk),
            POLLIN | POLLRDNORM | POLLRDBAND);
}
```
唤醒在socket上等待的进程（`read()`/`recv()`/`poll()`/`epoll`）。
[Advanced]

---

Q: What does `skb->protocol` field contain and when is it set?
A: `skb->protocol`包含以太网帧类型（网络字节序）：
```c
#define ETH_P_IP    0x0800  // IPv4
#define ETH_P_IPV6  0x86DD  // IPv6
#define ETH_P_ARP   0x0806  // ARP
#define ETH_P_8021Q 0x8100  // VLAN
```
设置时机：
- 驱动在接收时设置
- 或由`eth_type_trans(skb, dev)`从以太网头解析

`__netif_receive_skb_core()`根据此字段在`ptype_base[]`哈希表中查找协议处理函数。
[Basic]

---

## 5. Netfilter and Routing (Netfilter与路由)

---

Q: What are the five Netfilter hooks in IPv4?
A: 
```
                              [FORWARD]
                                  ^
                                  |
[PREROUTING] --> Routing --> [LOCAL_IN] --> Local Process
     |            Decision
     |                \
     v                 \--> [LOCAL_OUT] --> [POSTROUTING] --> Out
   DNAT                            |               |
                                   v               v
                               OUTPUT链        SNAT/Masquerade
```
| Hook | 时机 | 典型用途 |
|------|------|----------|
| `NF_INET_PRE_ROUTING` | 路由前 | DNAT, conntrack入口 |
| `NF_INET_LOCAL_IN` | 本地递送 | INPUT防火墙 |
| `NF_INET_FORWARD` | 转发 | 转发过滤 |
| `NF_INET_LOCAL_OUT` | 本地产生 | OUTPUT防火墙 |
| `NF_INET_POST_ROUTING` | 路由后 | SNAT, conntrack出口 |
[Intermediate]

---

Q: How is the NF_HOOK macro used in the kernel?
A: 
```c
// net/ipv4/ip_input.c
return NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING, skb, dev, NULL,
               ip_rcv_finish);
```
宏展开逻辑：
1. 遍历该hook点注册的所有回调函数
2. 回调返回值：
   - `NF_ACCEPT`: 继续下一个回调
   - `NF_DROP`: 丢弃数据包
   - `NF_STOLEN`: 回调接管skb
   - `NF_QUEUE`: 排队到用户空间
3. 所有回调通过后调用`ip_rcv_finish`
[Advanced]

---

Q: What is the routing decision process in `ip_rcv_finish()`?
A: 
```c
static int ip_rcv_finish(struct sk_buff *skb)
{
    // 1. 如果没有路由缓存，进行路由查找
    if (skb_dst(skb) == NULL) {
        int err = ip_route_input_noref(skb, iph->daddr, iph->saddr,
                                       iph->tos, skb->dev);
        if (err)
            goto drop;
    }
    
    // 2. 根据路由类型调用不同处理函数
    return dst_input(skb);  // 调用 dst->input()
}

// 路由类型决定input函数:
// RTN_LOCAL:     dst->input = ip_local_deliver
// RTN_UNICAST:   dst->input = ip_forward  
// RTN_BROADCAST: dst->input = ip_forward (或local)
// RTN_MULTICAST: dst->input = ip_mr_input
```
[Advanced]

---

## 6. TCP State Machine (TCP状态机)

---

Q: [Diagram] Draw the TCP state machine for connection establishment.
A: 
```
    Client                          Server
      |                               |
      |  CLOSED                  LISTEN (被动打开)
      |                               |
      +-------- SYN seq=x --------->  |
      |                               |
   SYN_SENT                      SYN_RCVD
      |                               |
      |<---- SYN+ACK seq=y,ack=x+1 ---+
      |                               |
      +-------- ACK ack=y+1 -------->  |
      |                               |
  ESTABLISHED                   ESTABLISHED
```
三次握手防止历史连接干扰，确保双方序列号同步。
[Basic]

---

Q: [Diagram] Draw the TCP state machine for connection termination.
A: 
```
    Active Close                    Passive Close
        |                               |
   ESTABLISHED                     ESTABLISHED
        |                               |
        +-------- FIN seq=x -------->   |
        |                               |
    FIN_WAIT_1                     CLOSE_WAIT
        |                               |
        |<------- ACK ack=x+1 ---------+
        |                               |
    FIN_WAIT_2                    (应用close)
        |                               |
        |<------- FIN seq=y -----------+
        |                               |
        +-------- ACK ack=y+1 ------->  |
        |                               |
    TIME_WAIT (2MSL)               LAST_ACK
        |                               |
      CLOSED                        CLOSED
```
四次挥手：因为TCP是全双工，每个方向需要单独关闭。
[Basic]

---

Q: What is the purpose of TIME_WAIT state and why is it 2*MSL?
A: TIME_WAIT状态（默认60秒 = 2*MSL）的目的：

1. **可靠终止连接**：确保最后的ACK被对端收到
   - 如果对端没收到ACK，会重传FIN
   - 本端可以重发ACK

2. **防止旧包干扰**：让网络中残留的数据包过期
   - MSL = Maximum Segment Lifetime（报文最大生存时间）
   - 2*MSL确保双向的旧包都过期

内核使用`inet_timewait_sock`轻量结构存储TIME_WAIT状态，减少内存。
[Intermediate]

---

Q: What is `struct request_sock` used for?
A: `request_sock`表示半连接（SYN_RCVD状态）：
```c
struct request_sock {
    struct sock_common __req_common;
    struct request_sock *dl_next;  // 链表
    u16 mss;                       // 对端MSS
    u8  num_retrans;              // SYN+ACK重传次数
    unsigned long expires;         // 超时时间
    const struct request_sock_ops *rsk_ops;
};
```
存储在`inet_connection_sock->icsk_accept_queue`中。SYN Flood攻击会消耗这些资源，SYN Cookie是防御手段。
[Advanced]

---

Q: What is the difference between `sk->sk_ack_backlog` and `sk->sk_max_ack_backlog`?
A: 
| 字段 | 含义 |
|------|------|
| `sk_max_ack_backlog` | 最大完成连接队列长度（`listen(fd, backlog)`设置） |
| `sk_ack_backlog` | 当前完成连接队列中的连接数 |

还有半连接队列（SYN队列）由`tcp_max_syn_backlog`控制。

当`sk_ack_backlog >= sk_max_ack_backlog`时，新连接会被丢弃或触发SYN Cookie。
[Intermediate]

---

## 7. Congestion Control (拥塞控制)

---

Q: What are the four phases of TCP congestion control?
A: 
```
cwnd
  ^
  |                              +-----------------
  |                             /
  |                            / Congestion Avoidance
  |                           /  (每RTT增加1 MSS)
  |          Slow Start      /
  |         (指数增长)       /
  |        /                /
  |       /    ssthresh ---+
  |      /        |
  |     /         |
  +----+---------+-----------------------> time
       |         |
    开始      超时/3个重复ACK
               ssthresh = cwnd/2
               cwnd = 1 (超时) 或 cwnd/2 (快速恢复)
```
1. **Slow Start**: cwnd从1开始，每收到ACK翻倍
2. **Congestion Avoidance**: cwnd >= ssthresh后线性增长
3. **Fast Retransmit**: 3个重复ACK触发立即重传
4. **Fast Recovery**: 快速恢复到ssthresh，避免回到slow start
[Basic]

---

Q: What TCP congestion control algorithms are available in Linux?
A: 
| 算法 | 特点 | 适用场景 |
|------|------|----------|
| `reno` | 经典AIMD，丢包驱动 | 传统网络 |
| `cubic` | Linux默认，三次函数增长 | 高BDP网络 |
| `bbr` | 基于带宽估计，非丢包驱动 | 高丢包/长延迟网络 |
| `vegas` | 基于RTT，主动避免拥塞 | 低延迟场景 |
| `westwood` | 基于带宽估计 | 无线网络 |

可通过`sysctl net.ipv4.tcp_congestion_control`或`TCP_CONGESTION`套接字选项设置。
[Intermediate]

---

Q: How is congestion control implemented in the kernel?
A: 通过`struct tcp_congestion_ops`策略模式实现：
```c
struct tcp_congestion_ops {
    char name[TCP_CA_NAME_MAX];
    
    void (*init)(struct sock *sk);
    void (*release)(struct sock *sk);
    
    // 核心回调
    u32  (*ssthresh)(struct sock *sk);           // 计算ssthresh
    void (*cong_avoid)(struct sock *sk, u32 ack, u32 acked); // 拥塞避免
    void (*set_state)(struct sock *sk, u8 new_state);
    void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);
    void (*pkts_acked)(struct sock *sk, u32 num_acked, s32 rtt_us);
};
```
使用`tcp_register_congestion_control()`注册算法。
[Advanced]

---

## 8. Socket Options and APIs (套接字选项与API)

---

Q: What are the key differences between `SO_REUSEADDR` and `SO_REUSEPORT`?
A: 
| 选项 | 功能 |
|------|------|
| `SO_REUSEADDR` | 允许绑定TIME_WAIT地址；允许0.0.0.0与具体IP共存 |
| `SO_REUSEPORT` | 允许多个socket绑定完全相同的IP:Port，内核负载均衡分发 |

```c
// SO_REUSEPORT典型用法：多进程服务器
for (i = 0; i < num_workers; i++) {
    if (fork() == 0) {
        int fd = socket(...);
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
        bind(fd, addr, addrlen);  // 多个进程绑定同一端口
        listen(fd, backlog);
        // 内核自动分发连接到不同worker
    }
}
```
[Intermediate]

---

Q: What is `skb->ip_summed` and what values can it have?
A: `ip_summed`表示skb的校验和状态：
| 值 | 含义 | 发送/接收 |
|----|------|-----------|
| `CHECKSUM_NONE` | 未校验，需软件计算 | 接收 |
| `CHECKSUM_UNNECESSARY` | 硬件已验证正确 | 接收 |
| `CHECKSUM_COMPLETE` | 硬件提供校验和值 | 接收 |
| `CHECKSUM_PARTIAL` | 需硬件计算（offload） | 发送 |

用于实现checksum offload功能，减少CPU开销。
[Advanced]

---

Q: What is the difference between blocking and non-blocking socket I/O?
A: 
| 模式 | 行为 | 设置方式 |
|------|------|----------|
| 阻塞 | 操作未完成时进程睡眠等待 | 默认 |
| 非阻塞 | 无数据时立即返回`-EAGAIN` | `fcntl(fd, F_SETFL, O_NONBLOCK)` |

内核实现：
```c
// tcp_recvmsg()
timeo = sock_rcvtimeo(sk, flags & MSG_DONTWAIT);
// ...
if (skb_queue_empty(&sk->sk_receive_queue)) {
    if (!timeo) {  // 非阻塞
        err = -EAGAIN;
        break;
    }
    sk_wait_data(sk, &timeo);  // 阻塞等待
}
```
[Basic]

---

Q: How does `epoll` work in the kernel?
A: 
```c
// 1. epoll_create 创建eventpoll结构
struct eventpoll {
    struct rb_root rbr;        // 红黑树存储监控的fd
    struct list_head rdllist;  // 就绪队列
    wait_queue_head_t wq;      // 等待队列
};

// 2. epoll_ctl 添加fd到红黑树
struct epitem {
    struct rb_node rbn;        // 红黑树节点
    struct list_head rdllink;  // 就绪链表节点
    struct file *ffd;          // 关联的file
    struct eventpoll *ep;      // 所属eventpoll
    struct epoll_event event;  // 监控的事件
};

// 3. Socket设置回调
// ep_poll_callback() 被加入 sk->sk_wq
// 当sk->sk_data_ready()触发时，回调将epitem加入rdllist

// 4. epoll_wait 检查rdllist，有事件则返回
```
边缘触发(ET)只在状态变化时通知，水平触发(LT)只要条件满足持续通知。
[Advanced]

---

## 9. DMA and Ring Buffers (DMA与环形缓冲区)

---

Q: How does the NIC know where to DMA received packets?
A: 驱动通过以下步骤配置网卡DMA地址：

1. **分配描述符环**（DMA一致性内存）：
```c
rx_ring->desc = dma_alloc_coherent(dev, ring_size, &rx_ring->dma, GFP_KERNEL);
```

2. **将描述符环地址写入网卡寄存器**：
```c
IXGBE_WRITE_REG(hw, IXGBE_RDBAL(idx), rdba & 0xFFFFFFFF);  // 低32位
IXGBE_WRITE_REG(hw, IXGBE_RDBAH(idx), rdba >> 32);         // 高32位
IXGBE_WRITE_REG(hw, IXGBE_RDLEN(idx), ring_size);          // 环长度
```

3. **描述符中存储数据缓冲区DMA地址**：
```c
rx_desc->read.pkt_addr = cpu_to_le64(buffer_dma_addr);
```

网卡从RDBAL/RDBAH读取描述符环地址，再从描述符读取缓冲区地址进行DMA。
[Advanced]

---

Q: What is the difference between coherent and streaming DMA?
A: 
| 类型 | 函数 | 缓存 | 一致性 | 用途 |
|------|------|------|--------|------|
| Coherent | `dma_alloc_coherent()` | 禁用 | 硬件自动 | 描述符环 |
| Streaming | `dma_map_single()` | 启用 | 需手动sync | 数据缓冲区 |

```c
// Coherent: CPU和设备都可随时访问，自动一致
desc_ring = dma_alloc_coherent(dev, size, &dma_addr, GFP_KERNEL);

// Streaming: 访问前需同步
dma_addr = dma_map_single(dev, buf, len, DMA_FROM_DEVICE);
// ...设备DMA写入...
dma_sync_single_for_cpu(dev, dma_addr, len, DMA_FROM_DEVICE);
// 现在CPU可以读取
```
[Advanced]

---

Q: What is the Head/Tail pointer mechanism in NIC ring buffers?
A: 
```
+---+---+---+---+---+---+---+---+
| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
+---+---+---+---+---+---+---+---+
      ^               ^
      |               |
     HEAD            TAIL
   (Hardware)      (Software)

HEAD (RDH): 网卡维护，指向下一个要处理的描述符
            网卡DMA完成后自动递增
            
TAIL (RDT): 驱动维护，指向最后一个可用描述符
            驱动补充缓冲区后更新

可用描述符 = (TAIL - HEAD) mod RING_SIZE

驱动通过写RDT告诉网卡有新的缓冲区可用
网卡通过读HEAD决定从哪里开始DMA
```
[Intermediate]

---

## 10. Debugging and Tracing (调试与追踪)

---

Q: How to trace the network packet path in Linux kernel?
A: 
| 工具 | 用途 | 示例 |
|------|------|------|
| `perf trace` | 系统调用追踪 | `perf trace -e 'net:*'` |
| `ftrace` | 函数追踪 | `echo tcp_sendmsg > set_ftrace_filter` |
| `kprobe` | 动态探针 | 在任意函数插入探测点 |
| `dropwatch` | 丢包追踪 | 显示内核丢包位置 |
| `ss -i` | socket统计 | 显示TCP内部状态 |
| `/proc/net/` | 运行时信息 | `tcp`, `udp`, `dev`, `snmp` |

```bash
# 追踪TCP发送路径
echo 'tcp_sendmsg tcp_write_xmit tcp_transmit_skb' > /sys/kernel/debug/tracing/set_ftrace_filter
echo function > /sys/kernel/debug/tracing/current_tracer
cat /sys/kernel/debug/tracing/trace_pipe
```
[Intermediate]

---

Q: What information is available in `/proc/net/tcp`?
A: 
```
sl  local_address rem_address   st tx_queue rx_queue ... uid  timeout inode
 0: 0100007F:0277 00000000:0000 0A 00000000:00000000 ...  0    0 12345 ...
```
| 字段 | 含义 |
|------|------|
| sl | 槽号 |
| local_address | 本地IP:Port（十六进制） |
| rem_address | 远端IP:Port |
| st | 状态（0A=LISTEN, 01=ESTABLISHED等） |
| tx_queue | 发送队列中的数据量 |
| rx_queue | 接收队列中的数据量 |
| uid | socket所有者UID |
| inode | socket inode号 |

使用`ss -tnp`可获得更可读的输出。
[Basic]

---

Q: How to check socket buffer usage and statistics?
A: 
```bash
# 1. 查看socket缓冲区配置
sysctl net.core.rmem_max
sysctl net.core.wmem_max
sysctl net.ipv4.tcp_rmem
sysctl net.ipv4.tcp_wmem

# 2. 查看当前socket状态
ss -m  # 显示内存使用
ss -i  # 显示内部TCP信息 (cwnd, rtt, etc.)

# 3. 查看协议统计
cat /proc/net/snmp    # SNMP MIB统计
cat /proc/net/netstat # 扩展TCP统计
netstat -s            # 汇总统计

# 4. 查看网卡队列
cat /proc/net/softnet_stat  # 每CPU软中断统计
ethtool -S eth0             # 网卡硬件统计
```
[Basic]

---

## 11. Common Misconceptions (常见误解)

---

Q: Is `close()` the same as `shutdown()` for sockets?
A: 不同！
| 函数 | 行为 |
|------|------|
| `close(fd)` | 减少引用计数，计数为0时关闭 |
| `shutdown(fd, SHUT_WR)` | 立即发送FIN，不影响读 |
| `shutdown(fd, SHUT_RD)` | 关闭读方向 |
| `shutdown(fd, SHUT_RDWR)` | 关闭双向 |

多进程共享socket时：
- `close()`不会发送FIN（引用计数>0）
- `shutdown()`立即影响所有引用者
[Intermediate]

---

Q: Does setting `SO_SNDBUF` guarantee that buffer size?
A: 不完全！
```c
int sndbuf = 65536;
setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
```
内核行为：
1. 实际分配大小 = 2 * 请求值（内核翻倍用于管理开销）
2. 受`net.core.wmem_max`上限限制
3. 可通过`SO_SNDBUFFORCE`绕过限制（需CAP_NET_ADMIN）

读取实际值：
```c
getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &actual, &len);
```
[Intermediate]

---

Q: Does TCP guarantee message boundaries?
A: 不！TCP是字节流协议，不保留消息边界。

发送：`send("Hello")` + `send("World")`
接收可能：
- `recv()` = "HelloWorld"
- `recv()` = "Hel" + `recv()` = "loWorld"
- 任意其他组合

解决方案：
1. 固定长度消息
2. 定界符（如换行符）
3. 长度前缀（先发送消息长度）
4. 使用SCTP（保留消息边界）
[Basic]

---

Q: Is `accept()` guaranteed to return connections in order?
A: 基本是，但不严格保证。

内核维护完成连接队列（`icsk_accept_queue`），`accept()`从队列头取连接。正常情况下FIFO。

但以下情况可能影响顺序：
1. 多线程同时`accept()`
2. SYN Cookie模式下的重建
3. `SO_REUSEPORT`时内核负载均衡

对于大多数应用，顺序不重要——每个连接独立处理。
[Intermediate]

---

## Summary Card (总结卡片)

---

Q: What are the key kernel structures in the TCP/IP stack?
A: 
```
User Space
    |
    v
+------------------+
|  struct socket   |  BSD socket接口
|    -> ops        |  proto_ops (inet_stream_ops)
+------------------+
    |
    v
+------------------+
|  struct sock     |  通用socket控制块
|    -> sk_prot    |  struct proto (tcp_prot)
+------------------+
    |
    v
+------------------+
|  struct tcp_sock |  TCP协议状态
|    -> snd_una    |  序列号、窗口、拥塞控制
|    -> snd_cwnd   |
+------------------+
    |
    v
+------------------+
|  struct sk_buff  |  数据包缓冲区
|    -> data       |  协议头和数据
|    -> protocol   |
+------------------+
    |
    v
+------------------+
| struct net_device|  网络接口
|    -> netdev_ops |  驱动操作函数
+------------------+
```
[Basic]

---

*Total: 80+ cards covering Linux kernel TCP/IP implementation*

