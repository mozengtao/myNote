# TCP 发包流程 - 内核执行路径详解

> 🎯 **学习目标**: 深入理解 Linux TCP/IP 协议栈的数据发送路径，掌握从应用程序到网卡硬件的完整数据流

---

## 🧩 第一部分：宏观架构图

```
User Space Application Layer
├── Application
│   ├── send(sockfd, data, len, flags)
│   ├── write(sockfd, data, len)
│   └── sendmsg(sockfd, &msg, flags)          [Vector I/O / Zero-copy Entry]
├── glibc Syscall Wrapper
│   └── syscall(__NR_sendto / __NR_write)
└── User Buffer
    └── copy_from_user() Target                [Major CPU Overhead]

════════════════════════════════════════════════════════════════════════════════

Kernel Space - Syscall Layer
├── sys_send() / sys_write() / sys_sendmsg()   [Syscall Entry]
├── sock_sendmsg()                             [Socket Layer Entry]
│   ├── security_socket_sendmsg()              [LSM Security Check]
│   └── sock->ops->sendmsg()                   [Protocol Family Dispatch]
├── inet_sendmsg()                             [IPv4/IPv6 inet Layer]
│   └── sk->sk_prot->sendmsg()                 [Transport Layer Protocol]
└── tcp_sendmsg() / tcp_sendmsg_locked()       [TCP Send Core]

════════════════════════════════════════════════════════════════════════════════

TCP Protocol Layer
├── Send Buffer Management
│   ├── sk_write_queue                           [Pending skb Queue]
│   ├── sk_send_head                             [Current Send Queue Head]
│   ├── sk_wmem_queued / sk_sndbuf               [Memory Quota Control]
│   └── sk_stream_wait_memory()                  [Block when Buffer Full]
├── Data Copy & skb Construction
│   ├── sk_stream_alloc_skb()                    [Allocate sk_buff]
│   ├── skb_entail()                             [Enqueue sk_write_queue]
│   ├── skb_copy_to_page_nocache()               [User Data → skb Page]
│   └── tcp_sendpage()                           [Zero-copy Page Send]
├── Send Decision (Push / Nagle / Cork)
│   ├── tcp_push()                               [Immediate Send Decision]
│   ├── tcp_nagle_check()                        [Nagle Algorithm]
│   ├── TCP_NODELAY / TCP_CORK                   [User-controlled Strategy]
│   └── __tcp_push_pending_frames()              [Push Pending Frames]
├── Segmentation & Congestion Control
│   ├── tcp_write_xmit()                         [Actual Send Loop]
│   ├── tcp_cwnd_test()                          [Congestion Window Check]
│   ├── tcp_init_tso_segs()                      [TSO/GSO Segmentation]
│   ├── tcp_transmit_skb()                       [Single Segment Send]
│   └── tcp_retransmit_timer()                   [Timeout Retransmit]
├── TCP Header Construction
│   ├── tcp_init_nondata_skb()                   [Pure ACK / Control Packet]
│   ├── Sequence: snd_nxt, snd_una, write_seq
│   ├── Window: tcp_select_window()
│   └── tcp_v4_send_check()                      [Checksum, HW Offload]
└── Pass to IP Layer: ip_queue_xmit()

════════════════════════════════════════════════════════════════════════════════

IP Protocol Layer
├── Route Lookup
│   ├── __sk_dst_check()                         [Route Cache]
│   └── ip_route_output_ports()                  [Routing Table Lookup]
├── IP Header Construction + Netfilter
│   ├── NF_INET_LOCAL_OUT                        [Local Output Hook]
│   └── ip_finish_output() → ip_finish_output2()
└── dev_queue_xmit()                             [Enter Link Layer]

════════════════════════════════════════════════════════════════════════════════

Data Link Layer & Driver
├── Traffic Control (qdisc)
│   ├── __dev_queue_xmit()                       [Device Send Entry]
│   ├── qdisc_run()                              [Queue Discipline Scheduler]
│   └── Multi-queue: select_queue()              [XPS/RPS Load Balancing]
├── Neighbor Subsystem (ARP/ND)
│   ├── dst_neigh_output()                       [L2 Address Resolution]
│   └── neigh_hh_output()                          [Fill MAC Header]
├── NIC Driver
│   ├── ndo_start_xmit()                         [Driver Send Function]
│   ├── DMA Mapping (dma_map_single/page)        [Memory → Device Address]
│   ├── TX Ring Descriptor Fill                  [Hardware Queue]
│   └── Write Doorbell / Trigger DMA             [Notify NIC to Send]
└── sk_write_space() Callback                    [Wake User after Send Complete]

════════════════════════════════════════════════════════════════════════════════

Network Hardware Layer
├── NIC
│   ├── DMA Read skb Data from Memory
│   ├── TSO: Hardware TCP Segmentation
│   ├── Checksum Offload: Hardware Checksum
│   └── Physical Layer Transmit (Electrical/Optical)
└── TX Complete IRQ → NET_TX_SOFTIRQ → Free skb → sk_write_space()
```

*图表说明：TCP 发包从用户态 send/write 经 copy_from_user 进入内核，TCP 层构造 skb 并入队发送缓冲区，经拥塞控制和分段后传递给 IP 层和链路层，最终由网卡 DMA 发送；发送完成后通过 sk_write_space 回调唤醒用户进程。*

**与收包路径的对称关系**:

```
Receive: NIC → Hard IRQ → NAPI → ip_rcv → tcp_v4_rcv → sk_receive_queue → sk_data_ready → recv()
Send:    send() → tcp_sendmsg → sk_write_queue → tcp_write_xmit → ip_queue_xmit → dev_queue_xmit → NIC
                                                                                              ↓
                                              ACK Arrives → tcp_clean_rtx_queue → sk_write_space
```

*图表说明：收包与发包路径的对称关系——收包由 sk_data_ready 唤醒 recv，发包由 ACK 触发 tcp_clean_rtx_queue 后通过 sk_write_space 释放发送缓冲区。*

---

## 🔬 第二部分：内核执行路径

### 2.1 用户态到内核态：系统调用入口

```c
// 用户态调用
ssize_t send(int sockfd, const void *buf, size_t len, int flags);

// 内核路径展开
SYSCALL_DEFINE4(send, int, fd, void __user *, buff, size_t, len, unsigned int, flags)
├── sockfd_lookup_light(fd, &err, &fput_needed)  // fd → struct socket
├── sock_sendmsg(sock, &msg, len)
│   ├── security_socket_sendmsg(sock, msg, size) // SELinux 等检查
│   └── sock->ops->sendmsg(sock, msg, size)      // 协议族分发
│       └── inet_sendmsg()
│           ├── msg->msg_name = NULL             // 已连接 socket 无需地址
│           └── sk->sk_prot->sendmsg(sk, msg, size)
│               └── tcp_sendmsg()                // 🔥 TCP 发送主函数

// write() 路径 (socket 也是 file)
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf, size_t, count)
├── ksys_write()
├── vfs_write()
│   └── file->f_op->write_iter()                 // socket 的 write_iter
│       └── sock_write_iter()
│           └── sock_sendmsg()                     // 最终汇入同一路径
```

**send() vs sendmsg() vs write()**:

| 接口 | 特点 | 内核路径 |
|------|------|----------|
| `send()` | 简单，已连接 socket | → sock_sendmsg |
| `sendmsg()` | 支持 iovec、辅助数据 | → sock_sendmsg (msg->msg_iter) |
| `write()` | 通用 fd 写入 | → sock_write_iter → sock_sendmsg |

### 2.2 tcp_sendmsg() - TCP 发送核心

```c
int tcp_sendmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct sk_buff *skb;
    int flags, err, copied = 0;
    long timeo;

    ├── lock_sock(sk)                              // 🔒 获取 socket 锁
    ├── 连接状态检查
    │   ├── if (sk->sk_state != TCP_ESTABLISHED)   // 必须已连接
    │   │   └── err = tcp_sendmsg_fastopen() 或返回错误
    │   └── 检查 sk->sk_shutdown & SEND_SHUTDOWN   // 对端已关闭写
    ├── flags = msg->msg_flags
    ├── timeo = sock_sndtimeo(sk, flags & MSG_DONTWAIT) // 发送超时
    ├── 🔥 主发送循环: while (msg_data_left(msg))
    │   ├── 🔥 等待发送缓冲区空间 (背压机制)
    │   │   └── while (!sk_stream_memory_free(sk)) {
    │   │       ├── if (非阻塞) return -EAGAIN
    │   │       └── sk_stream_wait_memory(sk, &timeo)  // 🔥 进程睡眠等待
    │   │           ├── prepare_to_wait_exclusive(&sk->sk_wq->wait, ...)
    │   │           ├── release_sock(sk)               // 释放锁，允许 ACK 处理
    │   │           ├── schedule_timeout(timeo)        // 进程阻塞
    │   │           │   └── 被 sk_write_space() 唤醒 ← ACK 释放缓冲区后
    │   │           └── lock_sock(sk)                  // 重新获取锁
    │   │       }
    │   ├── 🔥 分配 skb 并拷贝数据
    │   │   ├── copy = min_t(int, msg_data_left(msg), mss_now)
    │   │   ├── if (需要新 skb)
    │   │   │   ├── skb = sk_stream_alloc_skb(sk, select_size, ...)
    │   │   │   ├── skb_entail(sk, skb)                // 加入 sk_write_queue
    │   │   │   └── TCP_SKB_CB(skb)->seq = tp->write_seq
    │   │   ├── copy = skb_copy_to_page_nocache(sk, skb, ... msg, copy)
    │   │   │   └── copy_from_user()                   // 🔥 用户态 → 内核态
    │   │   ├── tp->write_seq += copy
    │   │   ├── TCP_SKB_CB(skb)->end_seq += copy
    │   │   └── copied += copy
    │   ├── 🔥 发送决策 (Nagle / Push)
    │   │   └── if (forced_push(tp) || tcp_should_push(sk, flags))
    │   │       └── tcp_push(sk, flags, mss_now, tp->nonagle, size_goal)
    │   └── 继续处理剩余数据
    ├── 🔥 最终 push (确保尾部数据发送)
    │   └── if (copied) tcp_push(sk, flags, ...)
    ├── release_sock(sk)
    └── return copied > 0 ? copied : err
}
```

**发送缓冲区背压流程**:

```
send() 写入数据
    ↓
sk_wmem_queued >= sk_sndbuf ?
    ↓ 是
sk_stream_wait_memory() → 进程睡眠 (TASK_INTERRUPTIBLE)
    ↓
对端 ACK 到达 → tcp_ack() → tcp_clean_rtx_queue()
    ↓
释放已确认 skb → sk_wmem_queued 减少
    ↓
sk_write_space() → wake_up(&sk->sk_wq->wait)
    ↓
send() 被唤醒，继续拷贝数据
```

### 2.3 tcp_push() 与 Nagle 算法

```c
// 决定是否立即发送待发数据
static void tcp_push(struct sock *sk, int flags, int mss_now,
                     int nonagle, int size_goal)
├── __tcp_push_pending_frames(sk, mss_now, nonagle)
    └── tcp_write_xmit(sk, mss_now, nonagle, 0, sk_gfp_mask(sk))

// Nagle 算法核心检查
static bool tcp_nagle_check(bool partial, const struct tcp_sock *tp,
                            int nonagle)
├── if (nonagle & TCP_NAGLE_OFF) return false      // TCP_NODELAY 已禁用 Nagle
├── if (nonagle & TCP_NAGLE_CORK) return true    // TCP_CORK 强制延迟
├── if (tp->packets_out == 0) return false       // 无未确认包，可发送
├── if (tcp_minshall_check(tp)) return false     // Minshall 算法
└── return true                                  // 🔥 延迟发送，等待 ACK

// Nagle 算法行为:
// 1. 有未确认数据时，延迟发送小数据包
// 2. 收到 ACK 后 (tcp_ack → tcp_push_one) 再发送
// 3. 目的: 减少小包数量，提高带宽利用率
// 4. 代价: 增加延迟 (典型 40ms，一个 RTT)
```

**Nagle 与 TCP_NODELAY 对比**:

```c
// 默认: Nagle 开启 (适合 bulk 传输)
send(sockfd, data, len, 0);

// 禁用 Nagle (适合低延迟交互，如游戏、SSH)
int flag = 1;
setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

// TCP_CORK: 强制合并所有数据到一个大包 (适合 HTTP 响应)
setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
// ... 多次 write/send ...
flag = 0;  // 解除 cork，一次性发送
setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(flag));
```

### 2.4 tcp_write_xmit() - 实际发送循环

```c
static bool tcp_write_xmit(struct sock *sk, unsigned int mss_now, int nonagle,
                           int push_one, gfp_t gfp)
{
    struct tcp_sock *tp = tcp_sk(sk);
    struct sk_buff *skb;
    unsigned int tso_segs, sent_pkts = 0;
    int cwnd_quota;

    ├── while ((skb = tcp_send_head(sk))) {       // 遍历 sk_send_head 起的队列
    │   ├── 🔥 拥塞控制检查
    │   │   ├── cwnd_quota = tcp_cwnd_test(tp, skb)
    │   │   │   └── 检查: inflight < snd_cwnd (拥塞窗口)
    │   │   ├── if (!cwnd_quota) break           // 窗口已满，停止发送
    │   │   └── tcp_pacing_check()               // 发送 pacing (BBR 等)
    │   ├── 🔥 接收窗口检查
    │   │   └── if (!tcp_snd_wnd_test(tp, skb, mss_now)) break
    │   ├── 🔥 TSO/GSO 分段
    │   │   ├── tso_segs = tcp_init_tso_segs(skb, mss_now)
    │   │   ├── if (tso_segs > 1)
    │   │   │   └── 网卡硬件分段 (TSO) 或软件 GSO
    │   │   └── tcp_set_skb_tso_segs(skb, mss_now)
    │   ├── 🔥 发送单个段
    │   │   └── if (tcp_transmit_skb(sk, skb, 1, gfp) < 0) break
    │   │       ├── 构造 TCP 头 (seq, ack_seq, window, flags)
    │   │       ├── tcp_options_write()          // 写入 TCP 选项 (SACK, TS...)
    │   │       ├── tcp_v4_send_check()          // 校验和
    │   │       ├── tp->snd_nxt += tcp_skb_pcount(skb) * mss_now
    │   │       └── ip_queue_xmit(sk, skb, &inet->cork.fl) // → IP 层
    │   ├── tcp_advance_send_head(sk, skb)       // 推进 sk_send_head
    │   ├── sent_pkts += tcp_skb_pcount(skb)
    │   └── if (push_one) break
    ├── tcp_cwnd_validate(sk, ...)               // 更新拥塞窗口
    └── return sent_pkts > 0
}
```

### 2.5 tcp_transmit_skb() - 单段发送与 IP 层交接

```c
static int tcp_transmit_skb(struct sock *sk, struct sk_buff *skb,
                            int clone_it, gfp_t gfp_mask)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct tcp_sock *tp = tcp_sk(sk);
    struct tcp_skb_cb *tcb = TCP_SKB_CB(skb);
    struct tcp_outgoing *tcp_header;

    ├── 🔥 构造 TCP 头部
    │   ├── th = tcp_hdr(skb)
    │   ├── th->source = inet->inet_sport
    │   ├── th->dest = inet->inet_dport
    │   ├── th->seq = htonl(tcb->seq)            // 🔥 发送序列号
    │   ├── th->ack_seq = htonl(tp->rcv_nxt)     // 捎带 ACK
    │   ├── th->doff = (tcp_header_len >> 2)     // 头部长度
    │   ├── th->window = htons(tcp_select_window(sk)) // 通告窗口
    │   ├── th->check = 0
    │   └── 设置 flags: TCPHDR_ACK | TCPHDR_PSH...
    ├── tcp_options_write()                        // SACK, Timestamp 等
    ├── tcp_v4_send_check()                        // 校验和 (可 offload)
    ├── 🔥 重传队列管理
    │   ├── if (clone_it) skb = skb_clone(skb)   // 重传需要保留副本
    │   ├── tcp_add_retrans_queue()              // 加入重传队列
    │   └── tcp_retransmit_timer()               // 启动/重置重传定时器
    ├── 统计: tp->segs_out++, tp->data_segs_out++
    └── 🔥 传递给 IP 层
        └── err = ip_queue_xmit(sk, skb, &inet->cork.fl)
```

### 2.6 IP 层到链路层

```c
int ip_queue_xmit(struct sock *sk, struct sk_buff *skb, struct flowi *fl)
├── 路由查找 (缓存或 ip_route_output_ports)
├── 构造 IP 头 (saddr, daddr, ttl, protocol=IPPROTO_TCP, tot_len...)
├── ip_send_check(iph)                           // IP 头校验和
├── NF_HOOK(NFPROTO_IPV4, NF_INET_LOCAL_OUT, ..., ip_finish_output)
└── ip_finish_output()
    └── ip_finish_output2()
        ├── if (skb->len > mtu && !skb_is_gso(skb))
        │   └── ip_fragment()                    // IP 分片 (尽量避免)
        └── dst_neigh_output()
            └── neigh_hh_output() / dev_queue_xmit() // 🔥 进入设备层

// 设备发送
int __dev_queue_xmit(struct sk_buff *skb, struct net_device *dev, ...)
├── skb_update_prio()                            // QoS 优先级
├── qdisc = rcu_dereference(dev->qdisc)          // 获取队列规则
├── q->enqueue(skb, q)                           // 入队 qdisc
├── __qdisc_run(q)                               // 调度发送
│   └── sch_direct_xmit()
│       └── dev_hard_start_xmit(skb, dev, txq)
│           └── ops->ndo_start_xmit()            // 🔥 驱动发送函数
│               ├── DMA 映射 skb 数据
│               ├── 填充 TX ring 描述符
│               └── writel(doorbell)             // 通知网卡
└── 返回 (异步: 发送完成中断释放 skb)
```

### 2.7 重传机制与 sk_write_space 回调

```c
// 超时重传定时器
void tcp_retransmit_timer(struct sock *sk)
├── if (icsk->icsk_retransmits == 0)
│   └── 首次超时: 记录 dup_ack 等
├── tcp_write_timeout_handler()
│   ├── tcp_retransmit_skb()                     // 重传最早未确认段
│   │   ├── 从重传队列取出 skb
│   │   ├── 更新 seq 为 snd_una (重传序列号)
│   │   └── tcp_transmit_skb(sk, skb, 0, gfp)   // 重新发送
│   ├── 拥塞窗口减半 (拥塞避免)
│   ├── icsk->icsk_retransmits++                 // 重传计数
│   └── 指数退避: icsk->icsk_rto *= 2           // RTO 翻倍
└── 重新启动定时器

// ACK 到达时的清理
void tcp_ack(struct sock *sk, const struct sk_buff *skb, int flag)
├── tcp_clean_rtx_queue()                        // 清理已确认的重传队列
│   ├── 遍历重传队列，移除 snd_una 之前的 skb
│   ├── sk_wmem_queued -= freed                  // 释放发送缓冲区
│   └── kfree_skb()                              // 释放 skb
├── tcp_cong_control()                           // 拥塞控制算法更新 cwnd
└── 🔥 通知应用层发送缓冲区有空间
    └── sk->sk_write_space(sk)
        └── sock_def_write_space()
            └── wake_up_interruptible_sync_poll(&sk->sk_wq->wait, EPOLLOUT)
                ├── 唤醒阻塞在 send() 的进程
                └── 触发 epoll EPOLLOUT 事件

// 快速重传 (收到 3 个重复 ACK)
void tcp_fastretrans_alert()
├── tcp_enter_fast_recovery()                    // 进入快速恢复
├── tcp_retransmit_skb()                         // 重传丢失段
└── 拥塞窗口调整
```

### 2.8 关键内核子系统协作

1. **Socket 层**: 统一 sendmsg 接口，管理 fd 和权限
2. **TCP 层**: 缓冲区管理、分段、拥塞控制、可靠性
3. **IP 层**: 路由、分片、Netfilter 过滤
4. **邻居子系统**: ARP 解析 MAC 地址
5. **qdisc**: 流量整形和队列调度
6. **网卡驱动**: DMA 传输，硬件卸载
7. **进程调度**: send 阻塞/唤醒，epoll 事件通知

---

## 🧱 第三部分：核心数据结构

### 3.1 struct sock - 发送相关字段

```c
struct sock {
    /* 发送缓冲区 */
    int                     sk_sndbuf;        // 🔥 发送缓冲区上限 (SO_SNDBUF)
    atomic_t                sk_wmem_alloc;    // 已分配发送内存
    struct sk_buff_head     sk_write_queue;   // 🔥 待发送 skb 队列
    struct sk_buff          *sk_send_head;    // 🔥 当前可发送位置 (重传队列头)

    /* 回调函数 */
    void                    (*sk_write_space)(struct sock *sk);  // 🔥 发送空间可用
    void                    (*sk_state_change)(struct sock *sk); // 状态变化
    void                    (*sk_error_report)(struct sock *sk); // 错误报告

    struct socket_wq __rcu  *sk_wq;           // 等待队列 (send 阻塞在此)
};
```

**sk_write_queue vs 重传队列**:
- `sk_write_queue`: 所有待发送和已发送未确认的 skb
- `sk_send_head`: 指向第一个尚未成功发送或需要重传的 skb
- 已确认 (snd_una 之前) 的 skb 被释放，`sk_wmem_queued` 减少

### 3.2 struct tcp_sock - TCP 发送控制

```c
struct tcp_sock {
    /* 序列号管理 */
    __u32   snd_nxt;        // 🔥 下一个发送序列号
    __u32   snd_una;        // 🔥 最早未确认的序列号
    __u32   write_seq;      // 应用写入的最高序列号
    __u32   pushed_seq;     // 已 push 的序列号

    /* 窗口与拥塞控制 */
    __u32   snd_wnd;        // 🔥 对端通告的接收窗口
    __u32   snd_cwnd;       // 🔥 拥塞窗口 (拥塞控制算法维护)
    __u32   snd_ssthresh;   // 慢启动阈值
    __u32   mss_cache;      // 🔥 MSS (Maximum Segment Size)

    /* 发送队列 */
    __u32   packets_out;    // 未确认的包数量
    __u32   retrans_out;    // 重传中的包数量
    __u32   lost_out;       // 丢失的包数量

    /* Nagle 控制 */
    u8      nonagle:4;      // TCP_NAGLE_OFF / TCP_NAGLE_CORK 等

    /* RTT 与重传 */
    __u32   srtt_us;        // 平滑 RTT
    __u32   rttvar_us;      // RTT 变化量
    /* icsk->icsk_rto 在 inet_connection_sock 中 */
};
```

### 3.3 struct sk_buff - 发送路径中的数据演变

```c
// 阶段 1: tcp_sendmsg 分配后 (仅数据)
// skb->data → [用户数据...]
// skb->len = 数据长度

// 阶段 2: tcp_transmit_skb 添加 TCP 头
// skb->data → [TCP Header | 用户数据...]
// transport_header 指向 TCP 头

// 阶段 3: ip_queue_xmit 添加 IP 头
// skb->data → [IP Header | TCP Header | 用户数据...]
// network_header 指向 IP 头

// 阶段 4: dev_queue_xmit 添加以太网头
// skb->data → [Eth Header | IP Header | TCP Header | 用户数据...]
// mac_header 指向以太网头

// 零拷贝路径 (sendfile/tcp_sendpage):
struct skb_shared_info {
    unsigned char   nr_frags;               // 分片数量
    skb_frag_t      frags[MAX_SKB_FRAGS];   // 🔥 直接引用页缓存页面
    /*
    skb_frag_t {
        struct page *p;     // 页缓存页面 (无 copy)
        __u32 page_offset; // 页内偏移
        __u32 size;        // 分片大小
    };
    */
};
```

### 3.4 struct tcp_skb_cb - TCP 控制信息

```c
struct tcp_skb_cb {
    __u32   seq;            // 🔥 此 skb 的起始序列号
    __u32   end_seq;        // 结束序列号 (seq + data_len)
    __u8    tcp_flags;      // TCP 标志 (ACK, PSH, FIN...)
    __u8    sacked;         // SACK 状态
    union {
        struct {
            __u16   tcp_gso_segs;   // GSO 段数
            __u16   tcp_gso_size;   // GSO 每段大小
        };
    };
    /* 发送路径 tx 字段 */
    struct {
        __u32   when;           // 发送时间 (重传用)
        __u32   ack_seq;        // 期望的 ACK 序列号
    } tx;
};
```

---

## ⚙️ 第四部分：最小可运行实验

### 4.1 TCP 发送性能测试

```c
// demo_tcp_send_perf.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345

void measure_send_performance(int sockfd, size_t data_size, int iterations,
                              const char *label) {
    char *data = malloc(data_size);
    memset(data, 'A', data_size);

    struct timeval start, end;
    printf("\n=== %s ===\n", label);
    printf("数据大小: %zu 字节, 次数: %d\n", data_size, iterations);

    gettimeofday(&start, NULL);
    for (int i = 0; i < iterations; i++) {
        ssize_t sent = send(sockfd, data, data_size, 0);
        if (sent != (ssize_t)data_size) {
            printf("发送失败: %s\n", strerror(errno));
            break;
        }
    }
    gettimeofday(&end, NULL);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_usec - start.tv_usec) / 1000000.0;
    size_t total = data_size * iterations;
    printf("耗时: %.3f 秒, 吞吐量: %.2f MB/s\n",
           elapsed, (total / elapsed) / (1024 * 1024));
    free(data);
}

int main() {
    int sockfd;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        printf("请先启动: nc -l %d > /dev/null\n", SERVER_PORT);
        return 1;
    }
    printf("已连接 %s:%d\n", SERVER_IP, SERVER_PORT);

    measure_send_performance(sockfd, 1024, 1000, "小包 1KB x 1000");
    measure_send_performance(sockfd, 64*1024, 100, "中包 64KB x 100");
    measure_send_performance(sockfd, 1024*1024, 10, "大包 1MB x 10");

    close(sockfd);
    return 0;
}
```

### 4.2 Nagle 算法对比实验

```c
// demo_tcp_nagle.c - 观察 Nagle 对延迟的影响
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 12346

double send_small_packets(int sockfd, int count) {
    struct timeval start, end;
    char byte = 'X';

    gettimeofday(&start, NULL);
    for (int i = 0; i < count; i++) {
        send(sockfd, &byte, 1, 0);  // 每次 1 字节
    }
    gettimeofday(&end, NULL);

    return (end.tv_sec - start.tv_sec) * 1000000.0 +
           (end.tv_usec - start.tv_usec);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    printf("=== Nagle 算法对比 ===\n");
    printf("监听端口 %d，等待连接...\n", PORT);

    client_fd = accept(server_fd, (struct sockaddr*)&addr, &len);

    // 测试 1: Nagle 默认开启
    printf("\n1. Nagle 开启 (默认): 发送 100 个 1 字节包\n");
    double t1 = send_small_packets(client_fd, 100);
    printf("   耗时: %.0f 微秒\n", t1);

    // 测试 2: 禁用 Nagle
    int nodelay = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
    printf("\n2. TCP_NODELAY 开启: 发送 100 个 1 字节包\n");
    double t2 = send_small_packets(client_fd, 100);
    printf("   耗时: %.0f 微秒\n", t2);
    printf("\nNagle 延迟比: %.1fx\n", t1 / t2);

    close(client_fd);
    close(server_fd);
    return 0;
}
```

### 4.3 发送缓冲区背压实验

```c
// demo_tcp_send_backpressure.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define PORT 12347

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    // 设置小发送缓冲区
    int sndbuf = 4096;  // 4KB
    setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    printf("=== 发送缓冲区背压实验 ===\n");
    printf("SO_SNDBUF = %d 字节\n", sndbuf);
    printf("端口 %d，等待连接 (对端不读取数据)...\n", PORT);

    client_fd = accept(server_fd, (struct sockaddr*)&addr, &len);

    // 设置非阻塞
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    char buf[8192];
    memset(buf, 'B', sizeof(buf));

    printf("\n尝试发送 8KB 数据 (缓冲区仅 4KB)...\n");
    ssize_t sent = send(client_fd, buf, sizeof(buf), 0);
    if (sent < 0) {
        printf("非阻塞 send 返回: %s (预期 EAGAIN)\n", strerror(errno));
    } else {
        printf("首次 send 发送: %ld 字节\n", sent);
    }

    // 查看内核 TCP 发送队列
    printf("\n内核 TCP 发送队列状态:\n");
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ss -i -n sport = :%d 2>/dev/null", PORT);
    system(cmd);

    close(client_fd);
    close(server_fd);
    return 0;
}
```

### 4.4 编译和运行

```bash
gcc -o demo_tcp_send_perf demo_tcp_send_perf.c
gcc -o demo_tcp_nagle demo_tcp_nagle.c
gcc -o demo_tcp_send_backpressure demo_tcp_send_backpressure.c

# 终端 1: 接收端
nc -l 12345 > /dev/null &
nc -l 12346 > /dev/null &
nc -l 12347 > /dev/null &   # 不读数据，制造背压

# 终端 2: 运行测试
./demo_tcp_send_perf
./demo_tcp_nagle
./demo_tcp_send_backpressure
```

### 4.5 触发的内核行为

| 实验 | 触发的内核路径 |
|------|----------------|
| 性能测试 | tcp_sendmsg → copy_from_user → tcp_push → tcp_write_xmit → ip_queue_xmit |
| Nagle 对比 | tcp_nagle_check 延迟/立即 tcp_push |
| 背压测试 | sk_stream_wait_memory / 非阻塞 EAGAIN / sk_write_space 唤醒 |

---

## 🔍 第五部分：可观测性 & Debug 方法

### 5.1 strace 跟踪发送系统调用

```bash
# 跟踪 send/write 系统调用
strace -e trace=network,write -T ./demo_tcp_send_perf

# 观察阻塞行为 (背压)
strace -e trace=send,sendto -tt ./demo_tcp_send_backpressure
```

### 5.2 ss 观察 TCP 发送队列

```bash
# 查看发送队列详细信息
ss -i -e -n dst :12345

# 关键字段:
#   cwnd: 拥塞窗口
#   rto:  重传超时
#   bytes_acked: 已确认字节
#   bytes_sent: 已发送字节
#   segs_out: 发送段数
#   retrans: 重传次数

# 实时监控
watch -n 0.5 "ss -i -n dst :12345"
```

### 5.3 tcpdump 观察发包

```bash
# 捕获 TCP 包，显示序列号
sudo tcpdump -i lo -n -S port 12345

# 观察 Nagle 效应 (小包合并)
sudo tcpdump -i lo -n -c 20 port 12346

# 观察重传
sudo tcpdump -i lo -n 'tcp port 12345 and (tcp[tcpflags] & tcp-push != 0)'
```

### 5.4 /proc 网络统计

```bash
# TCP 发送统计
cat /proc/net/snmp | grep "^Tcp:"
# OutSegs: 发送段数, RetransSegs: 重传段数

# 扩展统计
cat /proc/net/netstat | grep TcpExt
# TCPFastRetrans, TCPSlowStartRetrans, TCPTimeouts...

# 网卡发送统计
cat /proc/net/dev | awk 'NR<=2 || /lo|eth/ {print}'
```

### 5.5 perf 观察内核函数

```bash
# 记录 TCP 发送相关系统调用
sudo perf record -e syscalls:sys_enter_sendto,syscalls:sys_exit_sendto \
    -g ./demo_tcp_send_perf

# 记录内核函数
sudo perf record -e probe:tcp_sendmsg,probe:tcp_transmit_skb \
    -g ./demo_tcp_send_perf 2>/dev/null || \
sudo perf record -g ./demo_tcp_send_perf

sudo perf script | head -50
```

### 5.6 ftrace 跟踪发送路径

```bash
# 设置函数图跟踪
echo function_graph > /sys/kernel/debug/tracing/current_tracer
echo 'tcp_sendmsg tcp_push tcp_write_xmit tcp_transmit_skb ip_queue_xmit dev_queue_xmit' > \
    /sys/kernel/debug/tracing/set_ftrace_filter
echo 1 > /sys/kernel/debug/tracing/tracing_on

./demo_tcp_send_perf

cat /sys/kernel/debug/tracing/trace | head -100

# 清理
echo 0 > /sys/kernel/debug/tracing/tracing_on
echo > /sys/kernel/debug/tracing/set_ftrace_filter
```

### 5.7 使用 TCP_INFO 获取内核状态

```c
// 在应用中获取 TCP 内核状态
struct tcp_info info;
socklen_t len = sizeof(info);
getsockopt(sockfd, IPPROTO_TCP, TCP_INFO, &info, &len);

printf("rtt: %u us\n", info.tcpi_rtt);
printf("snd_cwnd: %u\n", info.tcpi_snd_cwnd);
printf("retrans: %u\n", info.tcpi_retrans);
printf("bytes_sent: %u\n", info.tcpi_bytes_sent);
printf("bytes_acked: %u\n", info.tcpi_bytes_acked);
```

---

## ⚡ 第六部分：性能与设计权衡

### 6.1 性能瓶颈分析

1. **copy_from_user 开销**
   - 每次 send() 都要拷贝用户数据到内核 skb
   - 大数据传输时 CPU 开销显著
   - 优化: sendfile、splice、MSG_ZEROCOPY

2. **系统调用频率**
   - 小数据频繁 send() → 系统调用开销大
   - Nagle 算法减少包数但增加延迟
   - 优化: writev/sendmsg 批量、TCP_CORK

3. **锁竞争**
   - lock_sock() 保护整个 tcp_sendmsg
   - 多线程同时 send 同一 socket 会串行化
   - 优化: SO_REUSEPORT 多 socket、每连接一线程

4. **重传开销**
   - 丢包导致 RTO 超时 (默认最小 200ms)
   - 快速重传 (3 dup ACK) 更快恢复
   - 监控: RetransSegs / OutSegs 比率

### 6.2 硬件卸载技术

| 技术 | 作用 | 检查方法 |
|------|------|----------|
| **TSO** (TCP Segmentation Offload) | 网卡硬件 TCP 分段 | `ethtool -k eth0 \| grep tcp-segmentation` |
| **GSO** (Generic Segmentation Offload) | 软件分段延迟到驱动 | 内核自动 |
| **GRO** (Generic Receive Offload) | 接收方向合并 | 收包路径 |
| **Checksum Offload** | 硬件计算 TCP/IP 校验和 | `ethtool -k eth0 \| grep tx-checksumming` |

```bash
# 查看网卡 offload 能力
ethtool -k eth0

# 启用 TSO (通常默认开启)
ethtool -K eth0 tso on gso on tx on
```

### 6.3 拥塞控制算法

| 算法 | 特点 | 设置 |
|------|------|------|
| **cubic** | Linux 默认，适合通用网络 | 默认 |
| **bbr** | 基于带宽和 RTT，高带宽低延迟 | `sysctl net.ipv4.tcp_congestion_control=bbr` |
| **reno** | 经典算法，保守 | 研究/对比用 |

```bash
# 查看当前算法
sysctl net.ipv4.tcp_congestion_control

# 查看可用算法
sysctl net.ipv4.tcp_available_congestion_control
```

### 6.4 内核参数调优

```bash
# 增大发送缓冲区
sysctl -w net.core.wmem_max=16777216
sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"

# 启用 TCP window scaling (高带宽延迟积网络)
sysctl -w net.ipv4.tcp_window_scaling=1

# 减少 TIME_WAIT (高并发短连接)
sysctl -w net.ipv4.tcp_tw_reuse=1
```

### 6.5 应用层优化策略

```c
// 1. 禁用 Nagle (低延迟场景)
int nodelay = 1;
setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

// 2. 增大发送缓冲区 (高吞吐)
int sndbuf = 256 * 1024;
setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

// 3. 使用 sendmsg 批量发送 (减少系统调用)
struct iovec iov[10];
struct msghdr msg = { .msg_iov = iov, .msg_iovlen = 10 };
sendmsg(fd, &msg, 0);

// 4. 零拷贝 (文件 → 网络)
sendfile(out_fd, in_fd, &offset, count);

// 5. 非阻塞 + epoll EPOLLOUT (高并发)
fcntl(fd, F_SETFL, O_NONBLOCK);
// epoll_wait 返回 EPOLLOUT 时再 send
```

---

## 🔗 第七部分：横向对比

### 7.1 TCP vs UDP 发送

| 特性 | TCP send() | UDP sendto() |
|------|------------|--------------|
| **可靠性** | 确认重传 | 无 |
| **流量控制** | 滑动窗口 + 拥塞控制 | 无 |
| **发送缓冲区** | sk_write_queue + 重传队列 | 简单队列 |
| **阻塞行为** | 缓冲区满时阻塞 | 通常立即返回或 EAGAIN |
| **复杂度** | 高 (tcp_sendmsg 数百行) | 低 (udp_sendmsg) |
| **适用场景** | 可靠传输 | 实时、广播 |

### 7.2 不同发送 API 对比

```c
// 1. send() - 最简单
send(sockfd, buf, len, 0);

// 2. write() - 通用 fd 接口
write(sockfd, buf, len);

// 3. sendmsg() - 向量 I/O + 辅助数据
struct msghdr msg;
sendmsg(sockfd, &msg, 0);

// 4. sendfile() - 文件到 socket 零拷贝
sendfile(sockfd, filefd, &offset, count);

// 5. splice() - fd 到 fd 零拷贝
splice(filefd, &off1, pipefd[1], NULL, len, SPLICE_F_MOVE);
splice(pipefd[0], NULL, sockfd, NULL, len, SPLICE_F_MOVE);
```

### 7.3 阻塞 vs 非阻塞发送

```c
// 阻塞 (默认): 缓冲区满时 sleep 直到 sk_write_space 唤醒
send(sockfd, buf, len, 0);

// 非阻塞: 缓冲区满时立即返回 EAGAIN
fcntl(sockfd, F_SETFL, O_NONBLOCK);
ssize_t n = send(sockfd, buf, len, 0);
if (n < 0 && errno == EAGAIN) {
    // 注册 EPOLLOUT，等待可写
    epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &(struct epoll_event){
        .events = EPOLLIN | EPOLLOUT, .data.fd = sockfd
    });
}

// MSG_DONTWAIT: 单次非阻塞
send(sockfd, buf, len, MSG_DONTWAIT);
```

### 7.4 内核态 vs 用户态协议栈 (DPDK)

| 特性 | 内核 TCP (send) | DPDK + 用户态 TCP |
|------|-----------------|-------------------|
| **系统调用** | 每次 send 一次 | 无 (轮询) |
| **拷贝** | copy_from_user | 预注册内存池 |
| **拥塞控制** | 内核完整实现 | 需自行实现 |
| **兼容性** | 标准 socket API | 专用 API |
| **性能** | ~10 Gbps | ~100 Gbps |

---

## 🧠 第八部分：一句话本质总结

> **TCP 发送的本质是将用户数据拷贝到内核发送缓冲区，经 Nagle 合并和拥塞控制调度后，逐层封装并通过 DMA 发送到网卡，同时维护重传队列保证可靠性，通过 sk_write_space 回调实现发送缓冲区的背压控制。**

---

## 📌 下一步学习

掌握了 TCP 发包流程后，建议继续学习：
1. **[sendfile 零拷贝](07-sendfile.md)** - 绕过 copy_from_user 的高性能传输
2. 对比 [TCP 收包流程](05-tcp-recv.md) - 理解完整的双向数据流

---

## 🔖 关键要点回顾

- ✅ send() 经 sock_sendmsg → tcp_sendmsg → tcp_write_xmit 到达网卡
- ✅ copy_from_user 是常规发送路径的主要 CPU 开销
- ✅ sk_sndbuf 满时 send() 阻塞，ACK 后 sk_write_space 唤醒
- ✅ Nagle 算法合并小包，TCP_NODELAY 禁用以降低延迟
- ✅ 拥塞窗口 (snd_cwnd) 和接收窗口 (snd_wnd) 限制发送速率
- ✅ TSO/GSO 将分段工作卸载到网卡，显著提升吞吐
- ✅ 重传队列保证可靠性，超时和快速重传是两种恢复机制
- ✅ 理解了从应用数据到网络传输的完整路径
