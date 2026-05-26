# TCP 收包流程 - 内核执行路径详解

> 🎯 **学习目标**: 深入理解 Linux TCP/IP 协议栈的数据接收路径，掌握从网卡硬件到应用程序的完整数据流

---

## 🧩 第一部分：宏观架构图

```
Network Hardware Layer
├── NIC (Network Interface Card)
│   ├── DMA Engine                             [Zero-copy Direct Memory Access]
│   ├── RX Queue                               [Hardware Buffer]
│   ├── Interrupt Generation                   [Packet Arrival IRQ]
│   └── Multi-queue Support (RSS)
└── PCIe Bus → Memory

════════════════════════════════════════════════════════════════════════════════

Kernel Interrupt Handling Layer
├── Hard IRQ                                   [NIC IRQ Handler]
│   ├── Disable IRQ, Minimize Handler Time
│   ├── DMA Packet from NIC to Memory
│   ├── Trigger Soft IRQ (NET_RX_SOFTIRQ)
│   └── Re-enable IRQ
├── Soft IRQ                                   [Network Packet Processing]
│   ├── net_rx_action()                        [Soft IRQ Handler]
│   ├── NAPI Polling                           [Efficient Batch Processing]
│   │   ├── napi_schedule()                    [Schedule NAPI]
│   │   ├── driver->poll()                     [Batch Receive]
│   │   └── napi_complete()                    [Complete NAPI Poll]
│   └── Deliver Packet to Protocol Stack

════════════════════════════════════════════════════════════════════════════════

Network Protocol Stack Layer
├── Data Link Layer
│   ├── netif_receive_skb()                    [Receive sk_buff]
│   ├── Ethernet Frame Parsing                 [Frame Header]
│   ├── VLAN Tag Processing                    [Virtual LAN]
│   └── Pass to Network Layer
├── Network Layer (IP)
│   ├── ip_rcv()                               [IP Packet Entry]
│   ├── IP Header Validation                   [Version, Checksum]
│   ├── Route Lookup                           [Local/Forward Decision]
│   ├── Fragment Reassembly (if needed)        [IP Fragmentation]
│   ├── ip_local_deliver()                     [Local Delivery]
│   └── Pass to Transport Layer
├── Transport Layer (TCP)
│   ├── tcp_v4_rcv()                           [TCP Entry]
│   ├── TCP Header Validation                  [Checksum, Port Match]
│   ├── Socket Lookup                          [Find by 4-tuple]
│   │   ├── __inet_lookup_skb()                [Hash Table Lookup]
│   │   └── Match (src_ip, src_port, dst_ip, dst_port)
│   ├── TCP State Machine                      [State-based Handling]
│   │   ├── LISTEN → SYN_RECV                  [Connection Setup]
│   │   ├── ESTABLISHED → Data Transfer        [Normal Data]
│   │   └── FIN_WAIT/CLOSE_WAIT                [Connection Close]
│   ├── Sequence Number Check                  [TCP Reliability]
│   ├── Sliding Window Management              [Flow Control]
│   ├── Enqueue to Socket Receive Buffer       [sk_receive_queue]
│   └── Wake Waiting User Process              [sk_data_ready()]

════════════════════════════════════════════════════════════════════════════════

Socket Subsystem Layer
├── struct sock Management
│   ├── Receive Buffer (sk_receive_queue)      [Packet Queue]
│   ├── Wait Queue (sk_sleep)                [Blocked Processes]
│   ├── Callback (sk_data_ready)             [Data Ready Notification]
│   └── Buffer Management (sk_rmem_alloc)      [Memory Quota]
├── Wake-up Mechanism
│   ├── sk_data_ready() Call                   [Triggered by TCP Layer]
│   ├── wake_up_interruptible()                [Wake Waiting Process]
│   ├── epoll_poll_callback()                  [Trigger epoll Event]
│   └── Process State: TASK_INTERRUPTIBLE → TASK_RUNNING

════════════════════════════════════════════════════════════════════════════════

User Space Application Layer
├── Syscall Interface
│   ├── recv(sockfd, buffer, len, flags)      [Receive Data]
│   ├── read(sockfd, buffer, len)               [Generic Read]
│   └── epoll_wait() Returns Readable Event     [I/O Multiplexing]
├── Data Copy Path
│   ├── Kernel sk_buff → User Buffer           [Data Copy]
│   ├── Zero-copy Option (MSG_ZEROCOPY)        [High Performance]
│   └── Memory Mapping Optimization
└── Application Processing
    ├── Protocol Parsing (HTTP, protobuf...)    [Application Protocol]
    ├── Business Logic                          [Application Logic]
    └── Response Generation
```

*图表说明：TCP 收包从网卡 DMA 经硬/软中断和 NAPI 进入协议栈，链路层→IP 层→TCP 层逐层处理后入队 Socket 接收缓冲区，通过 sk_data_ready 唤醒用户进程；用户态通过 recv/read/epoll_wait 读取数据并完成 copy_to_user。*

---

## 🔬 第二部分：内核执行路径

### 2.1 硬件中断到软中断

```c
// 网卡数据包到达，触发硬件中断
// 中断处理程序 (以 e1000e 驱动为例)

// 硬中断处理程序
static irqreturn_t e1000_intr(int irq, void *data)
{
    struct net_device *netdev = data;
    struct e1000_adapter *adapter = netdev_priv(netdev);
    
    ├── 读取中断状态寄存器                    // 确认中断源
    ├── 清除中断标志                         // 防止重复中断
    ├── if (likely(napi_schedule_prep(&adapter->napi))) // 检查 NAPI 状态
    │   ├── __napi_schedule(&adapter->napi)  // 🔥 调度软中断处理
    │   │   ├── list_add_tail(&napi->poll_list, &__get_cpu_var(softnet_data).poll_list)
    │   │   └── __raise_softirq_irqoff(NET_RX_SOFTIRQ) // 🔥 触发软中断
    │   └── 禁用网卡中断                     // 避免中断风暴
    └── return IRQ_HANDLED                   // 硬中断处理完成

// 软中断处理 - NET_RX_SOFTIRQ
static __latent_entropy void net_rx_action(struct softirq_action *h)
{
    struct softnet_data *sd = this_cpu_ptr(&softnet_data);
    unsigned long time_limit = jiffies + usecs_to_jiffies(netdev_budget_usecs);
    int budget = netdev_budget;
    
    ├── local_irq_disable()                 // 禁用本地中断
    ├── while (!list_empty(&sd->poll_list)) // 处理所有待处理的 NAPI 设备
    │   ├── struct napi_struct *n = list_first_entry(...) // 取出第一个 NAPI
    │   ├── 🔥 work = n->poll(n, weight)     // 调用驱动的 poll 函数
    │   │   └── e1000_clean_rx_irq()         // 驱动特定的接收处理
    │   │       ├── 从 DMA 环形缓冲区读取描述符
    │   │       ├── 分配 sk_buff 结构
    │   │       ├── skb = netdev_alloc_skb_ip_align(netdev, length)
    │   │       ├── 从网卡内存拷贝数据到 sk_buff
    │   │       ├── skb_put(skb, length)     // 设置数据长度
    │   │       ├── skb->protocol = eth_type_trans(skb, netdev) // 确定协议类型
    │   │       ├── 🔥 netif_receive_skb(skb) // 传递给协议栈
    │   │       └── 更新接收统计信息
    │   ├── budget -= work                   // 更新预算
    │   └── if (work < weight) napi_complete(n) // 如果处理完成，重新启用中断
    └── 🔥 传递数据包到上层协议栈
}
```

### 2.2 网络协议栈处理路径

```c
// 数据包进入协议栈
int netif_receive_skb(struct sk_buff *skb)
├── 各种预处理和过滤
├── __netif_receive_skb_core(skb, false)     // 核心接收处理
│   ├── 处理 VLAN 标签
│   ├── 根据 skb->protocol 分发到不同协议    // ETH_P_IP, ETH_P_IPV6...
│   └── deliver_skb(skb, pt_prev, orig_dev)  // 交付给协议处理器
│       └── pt_prev->func(skb, skb->dev, pt_prev, orig_dev)
│           └── ip_rcv() // 🔥 进入 IP 层处理

// IP 层处理
int ip_rcv(struct sk_buff *skb, struct net_device *dev,
           struct packet_type *pt, struct net_device *orig_dev)
├── IP 包基本检查
│   ├── pskb_may_pull(skb, sizeof(struct iphdr)) // 确保有完整 IP 头
│   ├── 检查 IP 版本 (必须是 4)
│   ├── IP 头长度检查
│   ├── IP 校验和验证
│   └── skb_trim(skb, ntohs(iph->tot_len))       // 修剪到实际长度
├── 🔥 NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING, ..., ip_rcv_finish) // netfilter 钩子
└── ip_rcv_finish()                              // IP 接收完成处理
    ├── 路由查找决策
    │   ├── ip_route_input_noref()               // 路由表查找
    │   └── 根据路由决定：本地交付 vs 转发
    ├── 如果是本地包：
    │   └── ip_local_deliver()                   // 🔥 本地交付
    │       ├── 处理 IP 分片重组 (如果需要)
    │       ├── NF_HOOK(..., ip_local_deliver_finish) // 再次 netfilter
    │       └── ip_local_deliver_finish()        // 交付给传输层
    │           ├── 根据 protocol 字段分发       // IPPROTO_TCP, IPPROTO_UDP...
    │           ├── ipprot = rcu_dereference(inet_protos[protocol])
    │           └── ret = ipprot->handler(skb)   // 调用传输层处理器
    │               └── tcp_v4_rcv() // 🔥 进入 TCP 层
    └── 如果是转发包：
        └── ip_forward()                         // 转发处理

// TCP 层处理
int tcp_v4_rcv(struct sk_buff *skb)
├── TCP 包基本检查
│   ├── pskb_may_pull(skb, sizeof(struct tcphdr)) // 确保有完整 TCP 头
│   ├── TCP 校验和验证
│   ├── skb_trim(skb, len)                       // 修剪到 TCP 数据长度
│   └── 提取四元组信息 (src_ip, src_port, dst_ip, dst_port)
├── 🔥 Socket 查找
│   ├── sk = __inet_lookup_skb(&tcp_hashinfo, skb, ...) // 哈希表查找
│   │   ├── 首先查找 ESTABLISHED socket          // 最常见情况
│   │   ├── inet_lookup_established()            // 查找已建立连接
│   │   │   ├── 计算哈希值: hash = inet_ehashfn(daddr, dport, saddr, sport)
│   │   │   ├── 在哈希桶中查找匹配的 socket
│   │   │   └── 匹配四元组和网络命名空间
│   │   └── 如果未找到，查找 LISTEN socket       // 新连接请求
│   │       └── inet_lookup_listener()
│   └── 如果都未找到，发送 RST 并丢弃包
├── 🔥 Socket 状态检查和处理
│   ├── if (sk->sk_state == TCP_TIME_WAIT)       // TIME_WAIT 状态特殊处理
│   │   └── tcp_timewait_state_process()
│   ├── if (sk->sk_state == TCP_NEW_SYN_RECV)    // SYN_RECV 状态 (半连接)
│   │   └── 处理三次握手中的 ACK
│   └── 🔥 正常处理：tcp_v4_do_rcv(sk, skb)     // 主要数据处理路径
│       ├── if (sk->sk_state == TCP_ESTABLISHED)  // 🔥 ESTABLISHED 状态
│       │   └── tcp_rcv_established(sk, skb)      // 数据接收主函数
│       └── else: tcp_rcv_state_process(sk, skb)  // 其他状态处理
└── 清理和统计更新
```

### 2.3 TCP 数据接收和 Socket 通知

```c
// TCP ESTABLISHED 状态的数据接收 (最重要的路径)
void tcp_rcv_established(struct sock *sk, struct sk_buff *skb)
{
    struct tcp_sock *tp = tcp_sk(sk);
    
    ├── 🔥 快速路径检查 (Fast Path)
    │   ├── 检查数据包是否符合快速处理条件：
    │   │   ├── 序列号正确 (th->seq == tp->rcv_nxt)
    │   │   ├── 无 TCP 选项
    │   │   ├── 窗口未更新
    │   │   └── 纯数据包 (不是控制包)
    │   └── if (快速路径条件满足)
    │       ├── tcp_queue_rcv(sk, skb, &fragstolen) // 🔥 直接入队
    │       │   ├── __skb_queue_tail(&sk->sk_receive_queue, skb) // 添加到接收队列
    │       │   ├── tp->rcv_nxt += TCP_SKB_CB(skb)->end_seq - TCP_SKB_CB(skb)->seq
    │       │   ├── sk_mem_charge(sk, skb->truesize)  // 内存记账
    │       │   └── tcp_data_ready(sk)               // 🔥🔥🔥 通知数据就绪
    │       ├── 发送 ACK (如果需要)
    │       └── return // 快速路径完成
    ├── 🔥 慢速路径 (Slow Path) - 处理复杂情况
    │   ├── TCP 选项处理
    │   ├── 序列号检查和处理
    │   │   ├── 如果序列号不连续，可能需要缓存
    │   │   └── tcp_try_rmem_schedule() // 检查接收缓冲区空间
    │   ├── 窗口更新处理
    │   ├── 处理紧急数据 (URG)
    │   ├── 数据入队和重排序
    │   │   ├── tcp_try_coalesce() // 尝试合并相邻数据
    │   │   ├── tcp_queue_rcv() 或 tcp_data_queue() // 数据入队
    │   │   └── tcp_ofo_queue() // 乱序队列处理
    │   ├── ACK 处理 (确认之前发送的数据)
    │   │   ├── tcp_ack() // 处理 ACK
    │   │   ├── 更新发送窗口
    │   │   └── 释放已确认的发送缓冲区
    │   └── 🔥 tcp_data_ready(sk) // 通知数据就绪
    └── tcp_rcv_space_adjust(sk) // 调整接收窗口大小
}

// 数据就绪通知链
static void tcp_data_ready(struct sock *sk)
├── sk->sk_data_ready(sk) // 🔥 调用 socket 的数据就绪回调
    └── sock_def_readable(sk) // 默认的可读通知函数
        ├── rcu_read_lock()
        ├── wq = rcu_dereference(sk->sk_wq) // 获取等待队列
        ├── if (skwq_has_sleeper(wq)) // 检查是否有进程在等待
        │   └── 🔥 wake_up_interruptible_sync_poll(&wq->wait, 
        │           EPOLLIN | EPOLLPRI | EPOLLRDNORM | EPOLLRDBAND)
        │       ├── __wake_up_sync_key() // 唤醒等待的进程
        │       ├── 遍历等待队列中的所有等待项
        │       ├── 对每个等待项调用 wake_func()
        │       │   ├── 如果是普通进程等待：default_wake_function()
        │       │   │   └── try_to_wake_up() → 将进程状态改为 TASK_RUNNING
        │       │   └── 如果是 epoll 等待：ep_poll_callback() // 🔥 触发 epoll 事件
        │       │       ├── 将对应的 epitem 添加到就绪链表
        │       │       └── 唤醒 epoll_wait 中的进程
        │       └── 返回唤醒的进程数量
        ├── sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_IN) // 异步通知 (SIGIO)
        └── rcu_read_unlock()

// epoll 回调详细过程 (从 TCP 层触发)
static int ep_poll_callback(wait_queue_entry_t *wait, unsigned mode, int sync, void *key)
├── 获取对应的 epitem 和 eventpoll
├── 检查事件掩码匹配
├── 将 epitem 添加到就绪链表 (如果尚未添加)
├── 🔥 唤醒 epoll_wait 中等待的进程
│   └── wake_up(&ep->wq) // 唤醒用户进程
└── 返回成功
```

### 2.4 用户态数据接收

```c
// 用户进程调用 recv() 接收数据
// 系统调用路径

SYSCALL_DEFINE4(recv, int, fd, void __user *, ubuf, size_t, size, unsigned int, flags)
├── sock_recvmsg() 
├── security_socket_recvmsg()
└── sock->ops->recvmsg() // 调用协议特定的接收函数
    └── inet_recvmsg() // inet socket 接收
        └── sk->sk_prot->recvmsg() // TCP 协议接收
            └── tcp_recvmsg() // 🔥 TCP 数据接收主函数
                ├── lock_sock(sk) // 获取 socket 锁
                ├── timeo = sock_rcvtimeo(sk, flags & MSG_DONTWAIT) // 超时设置
                ├── 🔥 主接收循环
                │   └── while (len > 0) {
                │       ├── 🔥 检查接收队列
                │       │   ├── skb_peek(&sk->sk_receive_queue) // 查看队列头
                │       │   └── if (队列为空 && 非阻塞) return -EAGAIN
                │       ├── 🔥 如果队列为空，进入等待
                │       │   ├── sk_wait_data(sk, &timeo, last) // 等待数据到达
                │       │   │   ├── prepare_to_wait() // 准备等待
                │       │   │   ├── set_bit(SOCK_ASYNC_WAITDATA, &sk->sk_socket->flags)
                │       │   │   ├── release_sock(sk) // 释放锁，允许其他操作
                │       │   │   ├── timeo = schedule_timeout(timeo) // 🔥 进程睡眠等待
                │       │   │   ├── lock_sock(sk) // 重新获取锁
                │       │   │   └── clear_bit(SOCK_ASYNC_WAITDATA, &sk->sk_socket->flags)
                │       │   └── 如果被 tcp_data_ready() 唤醒，继续执行
                │       ├── 🔥 从队列取出数据包
                │       │   ├── skb = __skb_dequeue(&sk->sk_receive_queue) // 出队
                │       │   ├── used = skb->len // 数据长度
                │       │   ├── 🔥 拷贝数据到用户空间
                │       │   │   ├── if (!(flags & MSG_TRUNC))
                │       │   │   └── err = skb_copy_datagram_msg(skb, offset, msg, used)
                │       │   │       └── copy_to_user() // 🔥 内核到用户态数据拷贝
                │       │   ├── *seq += used // 更新序列号
                │       │   ├── 释放 skb: consume_skb(skb) // 释放内核缓冲区
                │       │   └── 更新接收窗口
                │       └── len -= used // 更新剩余接收长度
                ├── 清理和统计更新
                ├── release_sock(sk) // 释放锁
                └── 返回实际接收的字节数

// 进程唤醒过程 (tcp_data_ready → sock_def_readable → wake_up)
// 当 TCP 数据到达时，sk_data_ready() 最终会调用：
wake_up_interruptible_sync_poll(&sk->sk_wq->wait, EPOLLIN)
├── __wake_up_common()
├── 遍历 socket 等待队列上的所有 wait_queue_entry_t
├── 调用每个 wait_entry 的 func() 函数
│   ├── 对于普通进程等待 (recv/read 阻塞)：
│   │   └── default_wake_function() → try_to_wake_up()
│   │       ├── 将进程状态从 TASK_INTERRUPTIBLE 改为 TASK_RUNNING
│   │       ├── 将进程加入运行队列
│   │       └── 触发进程调度
│   └── 对于 epoll 等待：
│       └── ep_poll_callback() → 触发 epoll 事件通知
└── 被唤醒的进程从 schedule_timeout() 返回，继续执行 tcp_recvmsg()
```

---

## 🧱 第三部分：核心数据结构

### 3.1 struct sk_buff - 网络数据包核心结构

```c
struct sk_buff {
    union {
        struct {
            /* 链表管理 */
            struct sk_buff      *next;      // 下一个 skb
            struct sk_buff      *prev;      // 前一个 skb
        };
        struct rb_node      rbnode;         // 红黑树节点 (TCP 乱序队列)
        struct list_head    list;           // 通用链表
    };

    union {
        struct sock         *sk;            // 关联的 socket
        int                 ip_defrag_offset;
    };

    union {
        ktime_t             tstamp;         // 时间戳
        u64                 skb_mstamp_ns;  // 纳秒级时间戳
    };

    /*
     * 这是控制缓冲区的核心字段
     * 它定义了有效数据在线性数据区域中的位置
     */
    char                    *head;          // 缓冲区起始地址 (分配的内存开始)
    char                    *data;          // 🔥 当前数据起始位置 (有效数据开始)
    sk_buff_data_t          tail;           // 🔥 当前数据结束位置
    sk_buff_data_t          end;            // 缓冲区结束位置 (分配的内存结束)
    unsigned char           *head_end;      // head 和 end 之间的实际结束

    unsigned int            len;            // 🔥 数据总长度 (包括分片)
    unsigned int            data_len;       // 非线性数据长度 (分片数据长度)
    __u16                   mac_len;        // MAC 头长度
    __u16                   hdr_len;        // 可写头长度

    /* 校验和相关 */
    union {
        __wsum              csum;           // 校验和
        struct {
            __u16           csum_start;     // 校验和开始偏移
            __u16           csum_offset;    // 校验和字段偏移
        };
    };

    __u32                   priority;       // 数据包优先级
    int                     skb_iif;        // 输入接口索引
    __u32                   hash;           // 数据包哈希值 (用于负载均衡)

    __be16                  vlan_proto;     // VLAN 协议
    __u16                   vlan_tci;       // VLAN 标签控制信息

    union {
        unsigned int        napi_id;        // NAPI 实例 ID
        unsigned int        sender_cpu;     // 发送 CPU
    };

    __u32                   secmark;        // 安全标记
    union {
        __u32               mark;           // 通用标记
        __u32               reserved_tailroom; // 尾部保留空间
    };

    union {
        __be16              inner_protocol; // 内层协议 (隧道)
        __u8                inner_ipproto;  // 内层 IP 协议
    };

    __u16                   inner_transport_header; // 内层传输层头偏移
    __u16                   inner_network_header;   // 内层网络层头偏移
    __u16                   inner_mac_header;       // 内层 MAC 头偏移

    __be16                  protocol;       // 🔥 协议类型 (ETH_P_IP, ETH_P_IPV6...)
    __u16                   transport_header; // 🔥 传输层头偏移 (TCP/UDP 头位置)
    __u16                   network_header;   // 🔥 网络层头偏移 (IP 头位置)  
    __u16                   mac_header;       // 🔥 MAC 层头偏移 (以太网头位置)

    /* 私有区域 - 各层协议的控制信息 */
    char                    cb[48] __aligned(8); // 🔥 控制缓冲区 (各层存储私有数据)

    unsigned long           _skb_refdst;    // 路由目标引用
    void                    (*destructor)(struct sk_buff *skb); // 析构函数

    struct sec_path         *sp;            // IPsec 路径
#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
    unsigned long           _nfct;          // netfilter 连接跟踪
#endif

    unsigned int            len;            // 数据长度 (重复声明用于调试)
    unsigned int            data_len;       // 分片数据长度

    /* 分片数据 (非线性数据) */
    struct skb_shared_info  *shinfo;       // 共享信息 (包含分片数组)
};

/* TCP 控制缓冲区 - 存储在 skb->cb 中 */
struct tcp_skb_cb {
    __u32                   seq;            // TCP 序列号
    __u32                   end_seq;        // 结束序列号
    union {
        __u32               tcp_tw_isn;     // TIME_WAIT ISN
        struct {
            u16             tcp_gso_segs;   // GSO 分段数
            u16             tcp_gso_size;   // GSO 大小
        };
    };
    __u8                    tcp_flags;      // TCP 标志位
    __u8                    sacked;         // SACK 信息
    __u8                    ip_dsfield;     // IP DSCP 字段
    __u8                    txstamp_ack:1,  // 时间戳 ACK
                            eor:1,          // 记录结束
                            has_rxtstamp:1, // 接收时间戳
                            unused:5;
    __u32                   ack_seq;        // ACK 序列号
    union {
        struct {
            __u32           when;           // 时间戳
            __u32           bytes_acked;    // 已确认字节数
        } tx;   /* only used for outgoing skbs */
        union {
            struct inet_skb_parm    h4;     // IPv4 参数
#if IS_ENABLED(CONFIG_IPV6)
            struct inet6_skb_parm   h6;     // IPv6 参数
#endif
        } header;  /* For incoming skbs */
    };
};
```

**作用**: 网络数据包的核心载体，贯穿整个网络协议栈  
**生命周期**: 网卡接收时分配 → 各层处理 → 用户态拷贝后释放  
**关键字段**:
- `data/tail`: 定义当前有效数据的范围
- `protocol`: 指示上层协议类型
- `cb[]`: 各层协议存储私有控制信息

### 3.2 struct sock - Socket 核心结构 (网络相关部分)

```c
struct sock {
    /*
     * Now struct inet_timewait_sock also uses sock_common, so please just
     * don't add nothing before this first member (__sk_common) --acme
     */
    struct sock_common      __sk_common;     // 公共字段 (地址、端口等)
#define sk_node                 __sk_common.skc_node       // 哈希表节点
#define sk_nulls_node           __sk_common.skc_nulls_node
#define sk_refcnt               __sk_common.skc_refcnt     // 引用计数
#define sk_tx_queue_mapping     __sk_common.skc_tx_queue_mapping
#define sk_dontcopy_begin       __sk_common.skc_dontcopy_begin
#define sk_dontcopy_end         __sk_common.skc_dontcopy_end
#define sk_hash                 __sk_common.skc_hash       // 哈希值
#define sk_portpair             __sk_common.skc_portpair   // 端口对
#define sk_num                  __sk_common.skc_num        // 本地端口
#define sk_dport                __sk_common.skc_dport      // 远程端口
#define sk_addrpair             __sk_common.skc_addrpair   // 地址对
#define sk_daddr                __sk_common.skc_daddr      // 远程地址
#define sk_rcv_saddr            __sk_common.skc_rcv_saddr  // 本地地址
#define sk_family               __sk_common.skc_family     // 地址族
#define sk_state                __sk_common.skc_state      // 🔥 连接状态
#define sk_reuse                __sk_common.skc_reuse      // 地址重用
#define sk_reuseport            __sk_common.skc_reuseport  // 端口重用
#define sk_ipv6only             __sk_common.skc_ipv6only
#define sk_net_refcnt           __sk_common.skc_net_refcnt
#define sk_bound_dev_if         __sk_common.skc_bound_dev_if // 绑定设备
#define sk_bind_node            __sk_common.skc_bind_node
#define sk_prot                 __sk_common.skc_prot       // 🔥 协议操作表
#define sk_net                  __sk_common.skc_net        // 网络命名空间
#define sk_v6_daddr             __sk_common.skc_v6_daddr   // IPv6 远程地址
#define sk_v6_rcv_saddr         __sk_common.skc_v6_rcv_saddr // IPv6 本地地址
#define sk_cookie               __sk_common.skc_cookie     // Socket cookie
#define sk_incoming_cpu         __sk_common.skc_incoming_cpu // 接收 CPU
#define sk_flags                __sk_common.skc_flags
#define sk_rxhash               __sk_common.skc_rxhash     // 接收哈希

    socket_lock_t           sk_lock;        // Socket 锁
    atomic_t                sk_drops;       // 丢包计数
    int                     sk_rcvlowat;    // 接收低水位标记
    struct sk_buff_head     sk_error_queue; // 错误队列
    struct sk_buff_head     sk_receive_queue; // 🔥🔥🔥 接收队列 (应用数据)
    /*
     * The backlog contains packets processed by the BH.
     * It is used to delay processing from softirq context to process context.
     */
    struct {
        atomic_t            rmem_alloc;     // 已分配接收内存
        int                 len;            // 队列长度  
        struct sk_buff      *head;          // 队列头
        struct sk_buff      *tail;          // 队列尾
    } sk_backlog;                          // 🔥 backlog 队列 (延迟处理)

#define sk_rmem_alloc           sk_backlog.rmem_alloc

    int                     sk_forward_alloc; // 预分配内存
    __u32                   sk_txhash;        // 发送哈希
    unsigned int            sk_napi_id;       // NAPI ID
    unsigned int            sk_ll_usec;       // 低延迟微秒数
    atomic_t                sk_refcnt;        // 引用计数
    int                     sk_rcvbuf;        // 🔥 接收缓冲区大小
    struct sk_filter __rcu  *sk_filter;      // BPF 过滤器
    union {
        struct socket_wq __rcu      *sk_wq; // 🔥🔥🔥 等待队列 (用户进程等待)
        struct socket_wq            *sk_wq_raw;
    };
#ifdef CONFIG_XFRM
    struct xfrm_policy __rcu *sk_policy[2];   // IPsec 策略
#endif
    struct dst_entry __rcu  *sk_dst_cache;   // 路由缓存
    atomic_t                sk_omem_alloc;    // 其他内存分配
    int                     sk_sndbuf;        // 发送缓冲区大小
    
    /* 🔥 回调函数 - 关键的事件通知机制 */
    void                    (*sk_state_change)(struct sock *sk);         // 状态变化回调
    void                    (*sk_data_ready)(struct sock *sk);           // 🔥🔥🔥 数据就绪回调
    void                    (*sk_write_space)(struct sock *sk);          // 写空间可用回调  
    void                    (*sk_error_report)(struct sock *sk);         // 错误报告回调
    int                     (*sk_backlog_rcv)(struct sock *sk,           // backlog 处理回调
                                              struct sk_buff *skb);
    void                    (*sk_destruct)(struct sock *sk);             // 析构回调

    struct sock_reuseport __rcu     *sk_reuseport_cb;   // 端口重用回调
    struct bpf_sk_storage __rcu     *sk_bpf_storage;    // BPF 存储
    struct rcu_head         sk_rcu;                     // RCU 头
    struct inet_sock        inet;                       // inet 特定字段
};

/* socket 等待队列 - 用户进程在此等待 */
struct socket_wq {
    /* Note: wait MUST be first field of socket_wq */
    wait_queue_head_t       wait;           // 🔥🔥🔥 等待队列头 (recv/epoll_wait 在此等待)
    struct fasync_struct    *fasync_list;   // 异步通知链表 (SIGIO)
    unsigned long           flags;          // 等待标志
    struct rcu_head         rcu;            // RCU 头
} ____cacheline_aligned_in_smp;
```

**作用**: 表示一个网络连接的端点  
**生命周期**: socket() 创建 → connect/bind/listen → close 销毁  
**关键字段**:
- `sk_receive_queue`: 应用层可读的数据包队列
- `sk_wq->wait`: 用户进程等待队列，recv/epoll_wait 在此阻塞
- `sk_data_ready`: 数据到达时的回调函数，唤醒等待的进程

### 3.3 struct tcp_sock - TCP 特定扩展

```c
struct tcp_sock {
    /* inet_connection_sock has to be the first member of tcp_sock */
    struct inet_connection_sock     inet_conn;      // 继承 inet 连接 socket

    /* TCP 序列号管理 */
    __u32                   rcv_nxt;                // 🔥 期望接收的下一个序列号
    __u32                   copied_seq;             // 用户已拷贝的序列号 
    __u32                   rcv_wup;                // 接收窗口更新点
    __u32                   snd_nxt;                // 下一个发送序列号
    __u32                   snd_una;                // 未确认的最小序列号
    __u32                   snd_sml;                // 最后一个小包的序列号  
    __u32                   rcv_tstamp;             // 最后接收时间戳
    __u32                   lsndtime;               // 最后发送时间
    
    /* 拥塞控制 */
    __u32                   snd_wl1;                // 窗口更新的序列号
    __u32                   snd_wnd;                // 🔥 发送窗口大小
    __u32                   max_window;             // 最大窗口
    __u32                   mss_cache;              // 🔥 MSS 缓存 (最大段大小)
    
    /* 接收窗口管理 */  
    __u32                   window_clamp;           // 窗口限制
    __u32                   rcv_ssthresh;           // 接收慢启动阈值
    
    /* RTT 和超时 */
    __u32                   srtt_us;                // 平滑 RTT (微秒)
    __u32                   mdev_us;                // RTT 平均偏差
    __u32                   mdev_max_us;            // 最大 RTT 偏差
    __u32                   rttvar_us;              // RTT 变化量
    __u32                   rtt_seq;                // RTT 序列号
    
    struct {
        __u32               rtt_us;                 // 当前 RTT
        __u32               seq;                    // RTT 测量序列号
        __u32               time;                   // 时间戳
    } rtt_min;

    /* 拥塞控制算法相关 */
    __u32                   snd_ssthresh;           // 慢启动阈值
    __u32                   snd_cwnd;               // 🔥 拥塞窗口
    __u32                   snd_cwnd_cnt;           // 拥塞窗口计数
    __u32                   snd_cwnd_clamp;         // 拥塞窗口限制
    __u32                   snd_cwnd_used;          // 已使用拥塞窗口
    __u32                   snd_cwnd_stamp;         // 拥塞窗口时间戳
    __u32                   prior_cwnd;             // 之前的拥塞窗口
    __u32                   prr_delivered;          // PRR 已交付
    __u32                   prr_out;                // PRR 输出
    __u32                   delivered;              // 已交付字节数
    __u32                   delivered_ce;           // CE 标记的已交付
    __u32                   app_limited;            // 应用限制标记

    /* 接收缓冲区自动调整 */
    struct {
        __u32               space;                  // 接收缓冲区空间
        __u32               seq;                    // 序列号
        __u64               time;                   // 时间戳  
    } rcvq_space;                                  // 🔥 接收队列空间管理

    /* 乱序处理 */
    struct rb_root          out_of_order_queue;     // 🔥 乱序队列 (红黑树)
    struct sk_buff          *ooo_last_skb;          // 最后一个乱序包

    /* SACK (选择确认) 相关 */
    struct tcp_sack_block   duplicate_sack[1];      // 重复 SACK
    struct tcp_sack_block   selective_acks[4];      // 选择 ACK 块

    struct tcp_sack_block   recv_sack_cache[4];     // 接收 SACK 缓存

    struct sk_buff          *highest_sack;          // 最高 SACK 点
    int                     lost_cnt_hint;          // 丢失计数提示
    __u32                   prior_ssthresh;         // 之前的慢启动阈值  
    __u32                   high_seq;               // 高序列号
    __u32                   retrans_stamp;          // 重传时间戳
    __u32                   undo_marker;            // 撤销标记
    int                     undo_retrans;           // 撤销重传
    __u64                   bytes_received;         // 🔥 接收字节总数
    __u32                   segs_in;                // 输入段数
    __u32                   data_segs_in;           // 数据段输入数
    __u32                   segs_out;               // 输出段数
    __u32                   data_segs_out;          // 数据段输出数

    /* 快速恢复相关 */
    __u64                   bytes_acked;            // 已确认字节数
    __u32                   dsack_dups;             // DSACK 重复数
    __u32                   snd_wl1;                // 发送窗口左边界
    __u32                   snd_wl2;                // 发送窗口右边界
    __u32                   pred_flags;             // 🔥 预测标志 (快速路径优化)

    /* 时间戳选项 */
    __u32                   rx_opt_ts_recent;       // 最近接收时间戳
    __u32                   rx_opt_ts_recent_stamp; // 时间戳更新时间
    __u32                   tcp_time_stamp;         // TCP 时间戳

    /* 接收相关 */
    __u32                   rcv_wnd;                // 🔥 接收窗口大小
    __u32                   write_seq;              // 写序列号
    __u32                   notsent_lowat;          // 未发送低水位
    __u32                   pushed_seq;             // 推送序列号
    __u32                   lost_out;               // 丢失输出
    __u32                   sacked_out;             // SACK 输出
};
```

**作用**: TCP 协议特定的连接状态和控制信息  
**关键字段**:
- `rcv_nxt`: 期望接收的下一个序列号，用于检查数据包顺序
- `rcv_wnd`: 接收窗口，控制发送端的发送速度
- `out_of_order_queue`: 乱序数据包的红黑树管理

---

## ⚙️ 第四部分：最小可运行实验

### 4.1 TCP 收包观测实验

```c
// demo_tcp_recv_trace.c - TCP 收包路径跟踪
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>

#define PORT 9999
#define BUFFER_SIZE 8192

volatile int keep_running = 1;

void signal_handler(int sig) {
    keep_running = 0;
}

void print_socket_info(int sockfd) {
    struct sockaddr_in local, remote;
    socklen_t len = sizeof(local);
    
    if (getsockname(sockfd, (struct sockaddr*)&local, &len) == 0) {
        printf("本地地址: %s:%d\n", 
               inet_ntoa(local.sin_addr), ntohs(local.sin_port));
    }
    
    len = sizeof(remote);
    if (getpeername(sockfd, (struct sockaddr*)&remote, &len) == 0) {
        printf("远程地址: %s:%d\n", 
               inet_ntoa(remote.sin_addr), ntohs(remote.sin_port));
    }
    
    // 获取接收缓冲区大小
    int rcvbuf_size;
    len = sizeof(rcvbuf_size);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, &len) == 0) {
        printf("接收缓冲区大小: %d 字节\n", rcvbuf_size);
    }
    
    printf("Socket FD: %d\n", sockfd);
    printf("进程 PID: %d\n", getpid());
}

void print_recv_stats() {
    // 打印网络统计信息
    printf("\n=== 网络接收统计 ===\n");
    system("cat /proc/net/snmp | grep '^Tcp:' | tail -1 | awk '{print \"TCP InSegs: \" $10}'");
    system("cat /proc/net/netstat | grep 'TcpExt:' | tail -1 | awk '{print \"TCP DelayedACKs: \" $13}'");
    
    // 显示软中断统计
    printf("软中断统计:\n");
    system("cat /proc/softirqs | head -1; cat /proc/softirqs | grep NET_RX");
    
    // 显示网卡统计
    printf("网卡接收统计:\n");
    system("cat /proc/net/dev | head -2; cat /proc/net/dev | grep -E '(eth0|ens|enp)'");
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== TCP 收包路径跟踪实验 ===\n");
    
    // 创建服务器 socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }
    
    // 设置地址重用
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }
    
    // 开始监听
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }
    
    printf("TCP 服务器监听在端口 %d\n", PORT);
    printf("使用以下命令测试:\n");
    printf("  echo 'Hello TCP' | nc localhost %d\n", PORT);
    printf("  或者: telnet localhost %d\n", PORT);
    printf("\n观测命令 (在另一个终端):\n");
    printf("  sudo tcpdump -i lo port %d\n", PORT);
    printf("  ss -i dst :%d\n", PORT);
    printf("\n按 Ctrl+C 停止服务器\n\n");
    
    print_recv_stats(); // 初始统计
    
    while (keep_running) {
        printf("等待连接...\n");
        
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue; // 信号中断
            perror("accept");
            break;
        }
        
        printf("\n=== 新连接建立 ===\n");
        printf("客户端: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        print_socket_info(client_fd);
        
        // 数据接收循环
        int packet_count = 0;
        struct timeval recv_start, recv_end;
        
        while (keep_running) {
            gettimeofday(&recv_start, NULL);
            
            // 🔥 关键：recv() 系统调用 - 触发完整的TCP收包路径
            ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
            
            gettimeofday(&recv_end, NULL);
            
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    printf("客户端关闭连接\n");
                } else {
                    perror("recv");
                }
                break;
            }
            
            buffer[bytes_received] = '\0';
            packet_count++;
            
            // 计算接收延迟
            double recv_time = (recv_end.tv_sec - recv_start.tv_sec) * 1000000.0 +
                              (recv_end.tv_usec - recv_start.tv_usec);
            
            printf("\n--- 数据包 #%d ---\n", packet_count);
            printf("接收字节数: %ld\n", bytes_received);
            printf("接收延迟: %.2f 微秒\n", recv_time);
            printf("数据内容: %s", buffer);
            
            // 回显数据 (触发发送路径)
            send(client_fd, buffer, bytes_received, 0);
            
            // 显示当前 socket 状态
            char ss_cmd[256];
            snprintf(ss_cmd, sizeof(ss_cmd), 
                    "ss -i -n dst %s:%d 2>/dev/null | grep -v State || true",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            printf("Socket 状态:\n");
            system(ss_cmd);
        }
        
        close(client_fd);
        printf("\n=== 连接关闭 ===\n");
        
        if (packet_count > 0) {
            printf("总共处理 %d 个数据包\n", packet_count);
            print_recv_stats(); // 最终统计
        }
    }
    
    close(server_fd);
    printf("\n服务器已停止\n");
    
    return 0;
}
```

### 4.2 TCP 接收缓冲区管理实验

```c
// demo_tcp_recv_buffer.c - TCP 接收缓冲区行为测试
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 10000

void print_buffer_info(int sockfd, const char* stage) {
    int rcvbuf, sndbuf;
    socklen_t len = sizeof(rcvbuf);
    
    printf("\n=== %s ===\n", stage);
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &len) == 0) {
        printf("SO_RCVBUF (接收缓冲区): %d 字节\n", rcvbuf);
    }
    
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &len) == 0) {
        printf("SO_SNDBUF (发送缓冲区): %d 字节\n", sndbuf);
    }
    
    // 获取 TCP 信息
    struct tcp_info info;
    len = sizeof(info);
    if (getsockopt(sockfd, IPPROTO_TCP, TCP_INFO, &info, &len) == 0) {
        printf("TCP_INFO:\n");
        printf("  状态: %u\n", info.tcpi_state);
        printf("  接收窗口: %u\n", info.tcpi_rcv_wnd);
        printf("  发送窗口: %u\n", info.tcpi_snd_wnd);
        printf("  MSS: %u\n", info.tcpi_snd_mss);
        printf("  RTT: %u 微秒\n", info.tcpi_rtt);
        printf("  重传数: %u\n", info.tcpi_retransmits);
    }
    
    // 显示 /proc/net/tcp 中的信息
    char proc_cmd[256];
    snprintf(proc_cmd, sizeof(proc_cmd), 
            "cat /proc/net/tcp | awk 'NR==1 || $2 ~ /:%04X/ {print}' 2>/dev/null || true", 
            PORT);
    printf("内核 TCP 状态:\n");
    system(proc_cmd);
}

void test_recv_scenarios(int client_fd) {
    char buffer[8192];
    
    printf("\n=== 测试不同接收场景 ===\n");
    
    // 1. 测试阻塞接收
    printf("\n1. 阻塞接收测试 (等待 5 秒数据):\n");
    printf("   发送一些数据到端口 %d 进行测试...\n", PORT);
    
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(client_fd, &readfds);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    int ready = select(client_fd + 1, &readfds, NULL, NULL, &timeout);
    if (ready > 0) {
        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("   阻塞接收成功: %ld 字节\n", bytes);
            printf("   数据: %s\n", buffer);
            
            print_buffer_info(client_fd, "阻塞接收后");
        }
    } else {
        printf("   阻塞接收超时\n");
    }
    
    // 2. 测试非阻塞接收
    printf("\n2. 非阻塞接收测试:\n");
    
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("   非阻塞接收成功: %ld 字节\n", bytes);
        printf("   数据: %s\n", buffer);
    } else if (bytes == -1 && errno == EAGAIN) {
        printf("   非阻塞接收: 无数据可读 (EAGAIN)\n");
    } else {
        printf("   非阻塞接收错误: %s\n", strerror(errno));
    }
    
    // 恢复阻塞模式
    fcntl(client_fd, F_SETFL, flags);
    
    // 3. 测试接收缓冲区调整
    printf("\n3. 接收缓冲区调整测试:\n");
    
    int old_rcvbuf, new_rcvbuf = 32768; // 32KB
    socklen_t len = sizeof(old_rcvbuf);
    getsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &old_rcvbuf, &len);
    
    printf("   调整前接收缓冲区: %d 字节\n", old_rcvbuf);
    
    if (setsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &new_rcvbuf, sizeof(new_rcvbuf)) == 0) {
        getsockopt(client_fd, SOL_SOCKET, SO_RCVBUF, &new_rcvbuf, &len);
        printf("   调整后接收缓冲区: %d 字节\n", new_rcvbuf);
    } else {
        printf("   接收缓冲区调整失败: %s\n", strerror(errno));
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    printf("=== TCP 接收缓冲区管理实验 ===\n");
    
    // 创建服务器 socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }
    
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }
    
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }
    
    printf("服务器监听在端口 %d\n", PORT);
    printf("使用以下命令测试:\n");
    printf("  echo 'Buffer Test Data' | nc localhost %d\n", PORT);
    printf("  dd if=/dev/zero bs=1024 count=10 | nc localhost %d\n", PORT);
    
    print_buffer_info(server_fd, "服务器 socket 创建后");
    
    printf("\n等待客户端连接...\n");
    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        exit(1);
    }
    
    printf("客户端连接: %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), 
           ntohs(client_addr.sin_port));
    
    print_buffer_info(client_fd, "连接建立后");
    
    // 执行各种接收测试
    test_recv_scenarios(client_fd);
    
    close(client_fd);
    close(server_fd);
    
    return 0;
}
```

### 4.3 编译和运行

```bash
# 编译
gcc -o demo_tcp_recv_trace demo_tcp_recv_trace.c
gcc -o demo_tcp_recv_buffer demo_tcp_recv_buffer.c

# 运行跟踪实验 (在终端1)
./demo_tcp_recv_trace

# 在另一个终端测试
echo "Hello TCP Kernel!" | nc localhost 9999

# 运行缓冲区实验 (在终端1)  
./demo_tcp_recv_buffer

# 在另一个终端测试
echo "Buffer test data" | nc localhost 10000
```

### 4.4 触发的内核行为

这些实验会触发：

1. **网卡中断处理** - 数据包到达时的硬中断和软中断
2. **TCP 协议栈处理** - tcp_v4_rcv() 到 tcp_rcv_established()
3. **Socket 缓冲区管理** - sk_receive_queue 的入队操作  
4. **进程唤醒机制** - sk_data_ready() → wake_up_interruptible()
5. **用户态数据拷贝** - tcp_recvmsg() 中的 copy_to_user()

---

## 🔍 第五部分：可观测性 & Debug 方法

### 5.1 使用 tcpdump 观察数据包

```bash
# 捕获指定端口的 TCP 流量
sudo tcpdump -i any -n port 9999

# 详细显示 TCP 头部信息
sudo tcpdump -i any -n -v port 9999

# 显示数据包内容 (十六进制 + ASCII)
sudo tcpdump -i any -n -X port 9999

# 保存到文件供后续分析
sudo tcpdump -i any -n -w tcp_recv.pcap port 9999
```

### 5.2 使用 ss 观察 TCP 连接状态

```bash
# 显示所有 TCP 连接的详细信息
ss -antip

# 显示指定端口的连接信息
ss -antip dst :9999

# 显示内核内存使用情况
ss -m dst :9999

# 实时监控连接状态变化
watch -n 1 "ss -antip dst :9999"
```

### 5.3 观察网络统计信息

```bash
# TCP 协议统计
cat /proc/net/snmp | grep ^Tcp

# 扩展 TCP 统计  
cat /proc/net/netstat | grep TcpExt

# 软中断统计
cat /proc/softirqs | grep NET_RX

# 网卡统计
cat /proc/net/dev

# 实时监控网络统计
watch -d "cat /proc/net/snmp | grep ^Tcp; echo; cat /proc/softirqs | grep NET_RX"
```

### 5.4 使用 strace 跟踪系统调用

```bash
# 跟踪 TCP 相关系统调用
strace -e trace=network,read,write ./demo_tcp_recv_trace

# 详细显示系统调用参数
strace -e trace=recv,send -v ./demo_tcp_recv_trace

# 显示系统调用时间
strace -e trace=recv -T ./demo_tcp_recv_trace
```

### 5.5 使用 perf 观察内核性能

```bash
# 记录 TCP 接收相关事件
sudo perf record -e syscalls:sys_enter_recv,syscalls:sys_exit_recv \
    -g ./demo_tcp_recv_trace

# 记录网络中断和软中断
sudo perf record -e irq:irq_handler_entry,irq:softirq_entry \
    -g ./demo_tcp_recv_trace

# 查看调用栈
sudo perf script

# 统计系统调用频率
sudo perf stat -e syscalls:sys_enter_recv,syscalls:sys_enter_send \
    ./demo_tcp_recv_trace
```

### 5.6 使用 ftrace 深入内核

```bash
# 跟踪 TCP 接收函数
echo function_graph > /sys/kernel/debug/tracing/current_tracer
echo 'tcp_v4_rcv tcp_rcv_established tcp_data_ready' > \
    /sys/kernel/debug/tracing/set_ftrace_filter
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 运行测试
echo "test data" | nc localhost 9999

# 查看函数调用图
cat /sys/kernel/debug/tracing/trace

# 清理
echo 0 > /sys/kernel/debug/tracing/tracing_on
echo > /sys/kernel/debug/tracing/set_ftrace_filter
```

### 5.7 观察 Socket 内核状态

```bash
# 查看进程打开的文件描述符
ls -la /proc/$(pgrep demo_tcp)/fd/

# 查看 Socket 详细信息  
cat /proc/$(pgrep demo_tcp)/net/tcp

# 查看内存使用情况
cat /proc/$(pgrep demo_tcp)/status | grep -E "Vm|Rss"

# 查看 TCP 队列状态
ss -nltp | grep :9999
```

---

## ⚡ 第六部分：性能与设计权衡

### 6.1 性能瓶颈分析

1. **中断处理开销**
   - 硬中断上下文切换成本高
   - 软中断在高负载时可能成为瓶颈
   - 中断合并和 NAPI 可以优化

2. **内存拷贝开销**
   - 网卡 → 内核: DMA 零拷贝
   - 内核 → 用户态: copy_to_user() 开销
   - 大数据传输时拷贝成为主要开销

3. **锁竞争**
   - socket 锁保护接收队列
   - 多核环境下锁竞争加剧
   - RFS/RPS 可以改善 CPU 亲和性

### 6.2 TCP 接收设计权衡

**中断 vs 轮询**

| 模式 | 优势 | 劣势 | 适用场景 |
|------|------|------|----------|
| **中断模式** | 低延迟，低 CPU 使用 | 高负载时中断风暴 | 低负载场景 |
| **轮询模式 (NAPI)** | 高吞吐量，避免中断风暴 | 高 CPU 使用，高延迟 | 高负载场景 |
| **混合模式** | 自适应最优性能 | 复杂度高 | 生产环境 |

**缓冲区管理策略**
```c
// 小缓冲区 - 低内存使用，但系统调用频繁
setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &small_buf, sizeof(small_buf));

// 大缓冲区 - 高吞吐量，但内存使用多
setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &large_buf, sizeof(large_buf));
```

**接收窗口自动调整**
- **优势**: 自动适应网络条件，优化吞吐量
- **代价**: 算法复杂度，可能过度使用内存

### 6.3 优化策略

1. **硬件优化**
```bash
# 启用网卡多队列
ethtool -L eth0 combined 4

# 调整中断合并
ethtool -C eth0 rx-usecs 50

# 启用 RSS (Receive Side Scaling)
ethtool -K eth0 rxhash on
```

2. **内核参数优化**
```bash
# 增大接收缓冲区
echo 'net.core.rmem_max = 134217728' >> /etc/sysctl.conf
echo 'net.ipv4.tcp_rmem = 4096 87380 134217728' >> /etc/sysctl.conf

# 启用 TCP 窗口缩放
echo 'net.ipv4.tcp_window_scaling = 1' >> /etc/sysctl.conf

# 调整软中断预算
echo 'net.core.netdev_budget = 600' >> /etc/sysctl.conf
```

3. **应用层优化**
```c
// 使用大接收缓冲区
int rcvbuf = 1024 * 1024; // 1MB
setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

// 批量接收
char buffer[64 * 1024]; // 大缓冲区
ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);

// 使用 MSG_WAITALL 确保接收完整数据
recv(sockfd, buffer, expected_size, MSG_WAITALL);
```

---

## 🔗 第七部分：横向对比

### 7.1 TCP vs UDP 接收路径

| 特性 | TCP | UDP |
|------|-----|-----|
| **可靠性保证** | 序列号、重传、流量控制 | 无保证，尽力交付 |
| **接收复杂度** | 高 (状态机、缓冲管理) | 低 (直接交付) |
| **内存使用** | 接收缓冲区 + 乱序队列 | 仅接收缓冲区 |
| **延迟** | 较高 (协议开销) | 低 (直接处理) |
| **适用场景** | 可靠数据传输 | 实时应用 |

### 7.2 不同操作系统的 TCP 栈

**Linux vs FreeBSD**
- **Linux**: 更激进的优化，复杂的拥塞控制算法
- **FreeBSD**: 更保守的设计，稳定性优先

**Linux vs Windows**
- **Linux**: 开源，可定制性强
- **Windows**: 闭源，但有完善的工具链

### 7.3 用户态协议栈对比

**内核态 TCP vs 用户态协议栈 (DPDK)**

```c
// 内核态 TCP (传统方式)
int sockfd = socket(AF_INET, SOCK_STREAM, 0);
recv(sockfd, buffer, size, 0); // 系统调用开销

// 用户态协议栈 (DPDK + 自实现TCP)
struct rte_mbuf *pkts[32];
uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, pkts, 32); // 批量接收
// 用户态 TCP 处理...
```

**优势对比**:
- **内核态**: 成熟稳定，功能完整，易于使用
- **用户态**: 高性能，低延迟，完全控制

---

## 🧠 第八部分：一句话本质总结

> **TCP 收包的本质是从网卡硬件中断开始，通过分层的软中断处理和协议栈解析，最终通过回调机制唤醒用户进程，实现可靠的端到端数据传输。**

---

## 📌 下一步学习

掌握了 TCP 收包流程后，建议继续学习：
1. **[TCP 发包流程](06-tcp-send.md)** - 理解数据发送的完整路径
2. **[sendfile 零拷贝](07-sendfile.md)** - 高性能数据传输机制

### 收包与发包的对称关系

理解收包路径后，发包路径中的对称概念如下：

| 收包 (Receive) | 发包 (Send) |
|----------------|-------------|
| `sk_receive_queue` | `sk_write_queue` |
| `sk_data_ready()` 唤醒 recv | `sk_write_space()` 唤醒 send |
| `copy_to_user()` 内核→用户 | `copy_from_user()` 用户→内核 |
| `tcp_v4_rcv()` 协议栈入口 | `tcp_transmit_skb()` 协议栈出口 |
| NAPI 批量收包 | TSO/GSO 批量发包 |
| `EPOLLIN` 可读事件 | `EPOLLOUT` 可写事件 |
| 接收窗口 `rcv_wnd` | 发送窗口 `snd_wnd` |
| 序列号 `rcv_nxt` | 序列号 `snd_nxt` |

---

## 🔖 关键要点回顾

- ✅ TCP 收包涉及硬中断、软中断、协议栈多层处理
- ✅ NAPI 机制在高负载时从中断切换到轮询模式
- ✅ sk_data_ready 回调是唤醒用户进程的关键机制
- ✅ TCP 状态机和序列号管理保证数据可靠性
- ✅ 理解了从网卡到应用程序的完整数据流路径