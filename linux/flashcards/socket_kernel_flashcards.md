# Socket Related Structures and API Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] Linux内核中socket的分层架构是什么？
A: Socket采用分层架构分离关注点：
```
+---------------------------+
|     用户空间应用          |
+------------+--------------+
             | socket系统调用
             v
+---------------------------+
|   BSD Socket Layer        |  struct socket
|   (net/socket.c)          |  struct proto_ops
+------------+--------------+
             |
             v
+---------------------------+
|   Protocol Layer          |  struct sock
|   (AF_INET, AF_UNIX...)   |  struct proto
+------------+--------------+
             |
             v
+---------------------------+
|   Transport Layer         |  TCP/UDP协议
|   (tcp.c, udp.c)          |  struct inet_sock
+------------+--------------+
             |
             v
+---------------------------+
|   Network Layer           |  IP协议
|   (ip_output.c)           |  路由选择
+---------------------------+
```

Q: [Basic] socket和sock结构体的关系是什么？
A: socket和sock是两个不同层次的抽象：
```
struct socket (BSD层)        struct sock (协议层)
+-----------------+         +------------------+
| state           |         | __sk_common      |
| type            |         | sk_receive_queue |
| flags           |         | sk_write_queue   |
| ops (proto_ops) |-------->| sk_prot (proto)  |
| file            |         | sk_socket -------|---+
| sk -------------|-------->| ...              |   |
| wq              |         +------------------+   |
+-----------------+                                |
       ^                                           |
       +-------------------------------------------+

关系：
- socket: 面向用户的BSD socket接口抽象
- sock: 协议无关的网络层socket表示
- 一对一关系：socket->sk指向sock，sock->sk_socket指回socket
```

Q: [Basic] 常用的socket类型(sock_type)有哪些？
A: socket类型定义在net.h中：
```c
/* include/linux/net.h */
enum sock_type {
    SOCK_STREAM    = 1,  // 流式socket (TCP)
    SOCK_DGRAM     = 2,  // 数据报socket (UDP)
    SOCK_RAW       = 3,  // 原始socket
    SOCK_RDM       = 4,  // 可靠传递消息
    SOCK_SEQPACKET = 5,  // 顺序数据包
    SOCK_DCCP      = 6,  // DCCP协议
    SOCK_PACKET    = 10, // 链路层packet
};

/* socket类型与协议对应 */
SOCK_STREAM  + AF_INET  → TCP
SOCK_DGRAM   + AF_INET  → UDP
SOCK_RAW     + AF_INET  → Raw IP
SOCK_STREAM  + AF_UNIX  → Unix Stream
SOCK_DGRAM   + AF_UNIX  → Unix Datagram
```

Q: [Basic] socket状态(socket_state)有哪些？
A: socket状态定义了连接的生命周期：
```c
/* include/linux/net.h */
typedef enum {
    SS_FREE = 0,        // 未分配
    SS_UNCONNECTED,     // 未连接到任何socket
    SS_CONNECTING,      // 连接进行中
    SS_CONNECTED,       // 已连接
    SS_DISCONNECTING    // 断开连接中
} socket_state;

/* 状态转换图 */
SS_FREE
   │ sock_alloc()
   v
SS_UNCONNECTED
   │ connect()开始
   v
SS_CONNECTING ──────────┐
   │                    │ 连接失败
   │ 连接成功           v
   v               SS_UNCONNECTED
SS_CONNECTED
   │ shutdown()
   v
SS_DISCONNECTING
   │ close()
   v
SS_FREE
```

---

## struct socket (BSD Socket)

Q: [Intermediate] struct socket的完整结构是什么？
A: struct socket是BSD层的socket表示：
```c
/* include/linux/net.h */
struct socket {
    socket_state        state;   // 连接状态
    
    kmemcheck_bitfield_begin(type);
    short               type;    // socket类型(SOCK_STREAM等)
    kmemcheck_bitfield_end(type);
    
    unsigned long       flags;   // socket标志
    
    struct socket_wq __rcu *wq;  // 等待队列
    
    struct file        *file;    // 关联的文件对象
    struct sock        *sk;      // 协议层socket
    const struct proto_ops *ops; // 协议操作函数表
};

/* flags常用值 */
#define SOCK_ASYNC_NOSPACE  0  // 发送缓冲区满
#define SOCK_ASYNC_WAITDATA 1  // 等待数据
#define SOCK_NOSPACE        2  // 没有发送空间
#define SOCK_PASSCRED       3  // 传递凭证
#define SOCK_PASSSEC        4  // 传递安全标签
```

Q: [Intermediate] struct socket_wq的作用是什么？
A: socket_wq用于实现等待和异步通知：
```c
/* include/linux/net.h */
struct socket_wq {
    /* wait必须是第一个字段 */
    wait_queue_head_t wait;         // 等待队列头
    struct fasync_struct *fasync_list; // 异步通知链表
    struct rcu_head rcu;            // RCU回收
} ____cacheline_aligned_in_smp;

/* 使用场景 */
// 1. poll/select/epoll等待
sock_poll() → poll_wait(file, &sock->wq->wait, wait);

// 2. 阻塞读写等待
do {
    prepare_to_wait(&sk->sk_wq->wait, &wait, TASK_INTERRUPTIBLE);
    if (有数据可读)
        break;
    schedule();
} while (!signal_pending(current));

// 3. fasync异步信号通知
sock_fasync() → fasync_helper(fd, filp, on, &sock->wq->fasync_list);
kill_fasync(&sock->wq->fasync_list, SIGIO, POLL_IN);
```

---

## struct proto_ops (协议操作)

Q: [Intermediate] struct proto_ops定义了哪些操作？
A: proto_ops是BSD层协议操作的抽象：
```c
/* include/linux/net.h */
struct proto_ops {
    int     family;           // 协议族
    struct module *owner;     // 所属模块
    
    /* 生命周期管理 */
    int (*release)(struct socket *sock);
    
    /* 连接管理 */
    int (*bind)(struct socket *sock, struct sockaddr *myaddr,
                int sockaddr_len);
    int (*connect)(struct socket *sock, struct sockaddr *vaddr,
                   int sockaddr_len, int flags);
    int (*socketpair)(struct socket *sock1, struct socket *sock2);
    int (*accept)(struct socket *sock, struct socket *newsock, int flags);
    int (*listen)(struct socket *sock, int len);
    int (*shutdown)(struct socket *sock, int flags);
    
    /* 地址获取 */
    int (*getname)(struct socket *sock, struct sockaddr *addr,
                   int *sockaddr_len, int peer);
    
    /* I/O操作 */
    unsigned int (*poll)(struct file *file, struct socket *sock,
                         struct poll_table_struct *wait);
    int (*ioctl)(struct socket *sock, unsigned int cmd, unsigned long arg);
    
    /* 数据传输 */
    int (*sendmsg)(struct kiocb *iocb, struct socket *sock,
                   struct msghdr *m, size_t total_len);
    int (*recvmsg)(struct kiocb *iocb, struct socket *sock,
                   struct msghdr *m, size_t total_len, int flags);
    
    /* 选项设置 */
    int (*setsockopt)(struct socket *sock, int level,
                      int optname, char __user *optval, unsigned int optlen);
    int (*getsockopt)(struct socket *sock, int level,
                      int optname, char __user *optval, int __user *optlen);
    
    /* 高级I/O */
    int (*mmap)(struct file *file, struct socket *sock,
                struct vm_area_struct *vma);
    ssize_t (*sendpage)(struct socket *sock, struct page *page,
                        int offset, size_t size, int flags);
    ssize_t (*splice_read)(struct socket *sock, loff_t *ppos,
                           struct pipe_inode_info *pipe, size_t len,
                           unsigned int flags);
};
```

Q: [Intermediate] inet_stream_ops和inet_dgram_ops的区别是什么？
A: TCP和UDP使用不同的proto_ops实现：
```c
/* net/ipv4/af_inet.c */
const struct proto_ops inet_stream_ops = {
    .family        = PF_INET,
    .owner         = THIS_MODULE,
    .release       = inet_release,
    .bind          = inet_bind,
    .connect       = inet_stream_connect,  // 阻塞等待三次握手
    .accept        = inet_accept,          // TCP特有
    .listen        = inet_listen,          // TCP特有
    .shutdown      = inet_shutdown,
    .setsockopt    = sock_common_setsockopt,
    .getsockopt    = sock_common_getsockopt,
    .sendmsg       = inet_sendmsg,
    .recvmsg       = inet_recvmsg,
    .mmap          = sock_no_mmap,
    .sendpage      = inet_sendpage,
    /* ... */
};

const struct proto_ops inet_dgram_ops = {
    .family        = PF_INET,
    .owner         = THIS_MODULE,
    .release       = inet_release,
    .bind          = inet_bind,
    .connect       = inet_dgram_connect,   // 只记录目标地址
    .accept        = sock_no_accept,       // UDP不支持
    .listen        = sock_no_listen,       // UDP不支持
    .shutdown      = inet_shutdown,
    .sendmsg       = inet_sendmsg,
    .recvmsg       = inet_recvmsg,
    /* ... */
};
```

---

## struct sock (协议Socket)

Q: [Intermediate] struct sock的核心字段有哪些？
A: struct sock是协议层socket的核心表示：
```c
/* include/net/sock.h */
struct sock {
    struct sock_common  __sk_common;  // 共享部分
    
    /* 便捷宏定义 */
    #define sk_node        __sk_common.skc_node
    #define sk_refcnt      __sk_common.skc_refcnt
    #define sk_family      __sk_common.skc_family
    #define sk_state       __sk_common.skc_state
    #define sk_reuse       __sk_common.skc_reuse
    #define sk_prot        __sk_common.skc_prot
    #define sk_net         __sk_common.skc_net
    
    /* 锁 */
    socket_lock_t       sk_lock;
    
    /* 接收队列 */
    struct sk_buff_head sk_receive_queue;
    
    /* backlog队列（软中断写入） */
    struct {
        atomic_t        rmem_alloc;
        int             len;
        struct sk_buff  *head;
        struct sk_buff  *tail;
    } sk_backlog;
    
    /* 缓冲区大小 */
    int                 sk_rcvbuf;     // 接收缓冲区
    int                 sk_sndbuf;     // 发送缓冲区
    
    /* 发送队列 */
    struct sk_buff_head sk_write_queue;
    atomic_t            sk_wmem_alloc; // 已分配的发送内存
    
    /* 目标缓存 */
    struct dst_entry    *sk_dst_cache;
    
    /* 选项和标志 */
    unsigned long       sk_flags;
    unsigned int        sk_shutdown : 2;
    unsigned int        sk_protocol : 8;
    unsigned int        sk_type : 16;
    
    /* 错误处理 */
    int                 sk_err;
    int                 sk_err_soft;
    struct sk_buff_head sk_error_queue;
    
    /* 回调函数 */
    void (*sk_state_change)(struct sock *sk);
    void (*sk_data_ready)(struct sock *sk, int bytes);
    void (*sk_write_space)(struct sock *sk);
    void (*sk_error_report)(struct sock *sk);
    int  (*sk_backlog_rcv)(struct sock *sk, struct sk_buff *skb);
    void (*sk_destruct)(struct sock *sk);
    
    /* 关联的socket */
    struct socket       *sk_socket;
};
```

Q: [Intermediate] struct sock_common的作用是什么？
A: sock_common是sock的最小网络层表示，用于各种哈希查找：
```c
/* include/net/sock.h */
struct sock_common {
    /* IPv4地址（用于快速匹配） */
    __be32          skc_daddr;       // 目的IPv4地址
    __be32          skc_rcv_saddr;   // 本地IPv4地址
    
    /* 哈希值 */
    union {
        unsigned int    skc_hash;        // 协议查找哈希
        __u16           skc_u16hashes[2]; // UDP用
    };
    
    /* 基本属性 */
    unsigned short  skc_family;      // 地址族
    volatile unsigned char skc_state; // 连接状态
    unsigned char   skc_reuse;       // SO_REUSEADDR
    int             skc_bound_dev_if; // 绑定的网络设备
    
    /* 哈希表节点 */
    union {
        struct hlist_node       skc_bind_node;    // 绑定哈希
        struct hlist_nulls_node skc_portaddr_node; // 端口地址哈希
    };
    
    /* 协议和命名空间 */
    struct proto    *skc_prot;   // 协议处理函数
    struct net      *skc_net;    // 网络命名空间
    
    /* 主哈希节点 */
    union {
        struct hlist_node       skc_node;
        struct hlist_nulls_node skc_nulls_node;
    };
    
    /* 引用计数 */
    atomic_t        skc_refcnt;
};

/* 用于TCP/UDP快速查找 */
// INET_MATCH()宏使用skc_daddr和skc_rcv_saddr做快速比较
```

Q: [Advanced] socket_lock_t的锁机制是什么？
A: socket_lock_t实现了自旋锁和伪信号量的组合：
```c
/* include/net/sock.h */
typedef struct {
    spinlock_t      slock;   // 自旋锁
    int             owned;   // 拥有标志
    wait_queue_head_t wq;    // 等待队列
#ifdef CONFIG_DEBUG_LOCK_ALLOC
    struct lockdep_map dep_map;
#endif
} socket_lock_t;

/* 使用方式 */
// 1. 软中断上下文（快速路径）
bh_lock_sock(sk);    // spin_lock(&sk->sk_lock.slock)
if (sock_owned_by_user(sk)) {
    // 用户进程持有锁，加入backlog
    __sk_add_backlog(sk, skb);
} else {
    // 直接处理
    sk_backlog_rcv(sk, skb);
}
bh_unlock_sock(sk);

// 2. 进程上下文
lock_sock(sk);       // 获取"owned"锁，可能睡眠
/* ... 可以睡眠的操作 ... */
release_sock(sk);    // 释放并处理backlog

/* lock_sock实现 */
void lock_sock(struct sock *sk)
{
    lock_sock_nested(sk, 0);
}

void lock_sock_nested(struct sock *sk, int subclass)
{
    might_sleep();
    spin_lock_bh(&sk->sk_lock.slock);
    if (sk->sk_lock.owned)
        __lock_sock(sk);  // 等待owned变为0
    sk->sk_lock.owned = 1;
    spin_unlock(&sk->sk_lock.slock);
    /* 注意：这里没有解锁BH，因为在release_sock中会处理 */
}
```

---

## struct proto (协议处理)

Q: [Intermediate] struct proto定义了哪些协议级操作？
A: struct proto是传输层协议的操作抽象：
```c
/* include/net/sock.h */
struct proto {
    /* 连接生命周期 */
    void    (*close)(struct sock *sk, long timeout);
    int     (*connect)(struct sock *sk, struct sockaddr *uaddr, int addr_len);
    int     (*disconnect)(struct sock *sk, int flags);
    struct sock *(*accept)(struct sock *sk, int flags, int *err);
    
    /* 初始化和销毁 */
    int     (*init)(struct sock *sk);
    void    (*destroy)(struct sock *sk);
    void    (*shutdown)(struct sock *sk, int how);
    
    /* 选项 */
    int     (*setsockopt)(struct sock *sk, int level, int optname,
                          char __user *optval, unsigned int optlen);
    int     (*getsockopt)(struct sock *sk, int level, int optname,
                          char __user *optval, int __user *option);
    int     (*ioctl)(struct sock *sk, int cmd, unsigned long arg);
    
    /* 数据传输 */
    int     (*sendmsg)(struct kiocb *iocb, struct sock *sk,
                       struct msghdr *msg, size_t len);
    int     (*recvmsg)(struct kiocb *iocb, struct sock *sk,
                       struct msghdr *msg, size_t len,
                       int noblock, int flags, int *addr_len);
    int     (*sendpage)(struct sock *sk, struct page *page,
                        int offset, size_t size, int flags);
    int     (*bind)(struct sock *sk, struct sockaddr *uaddr, int addr_len);
    
    /* backlog处理 */
    int     (*backlog_rcv)(struct sock *sk, struct sk_buff *skb);
    
    /* 哈希表操作 */
    void    (*hash)(struct sock *sk);
    void    (*unhash)(struct sock *sk);
    void    (*rehash)(struct sock *sk);
    int     (*get_port)(struct sock *sk, unsigned short snum);
    
    /* 内存管理 */
    void    (*enter_memory_pressure)(struct sock *sk);
    atomic_long_t     *memory_allocated;
    struct percpu_counter *sockets_allocated;
    int               *memory_pressure;
    long              *sysctl_mem;
    int               *sysctl_wmem;
    int               *sysctl_rmem;
    
    /* slab缓存 */
    struct kmem_cache *slab;
    unsigned int      obj_size;
    
    /* 元信息 */
    char              name[32];
    struct list_head  node;
};
```

Q: [Advanced] TCP协议的struct proto是什么样的？
A: tcp_prot定义了TCP协议的操作：
```c
/* net/ipv4/tcp_ipv4.c */
struct proto tcp_prot = {
    .name           = "TCP",
    .owner          = THIS_MODULE,
    .close          = tcp_close,
    .connect        = tcp_v4_connect,
    .disconnect     = tcp_disconnect,
    .accept         = inet_csk_accept,
    .ioctl          = tcp_ioctl,
    .init           = tcp_v4_init_sock,
    .destroy        = tcp_v4_destroy_sock,
    .shutdown       = tcp_shutdown,
    .setsockopt     = tcp_setsockopt,
    .getsockopt     = tcp_getsockopt,
    .recvmsg        = tcp_recvmsg,
    .sendmsg        = tcp_sendmsg,
    .sendpage       = tcp_sendpage,
    .backlog_rcv    = tcp_v4_do_rcv,
    .hash           = inet_hash,
    .unhash         = inet_unhash,
    .get_port       = inet_csk_get_port,
    .enter_memory_pressure = tcp_enter_memory_pressure,
    .memory_allocated = &tcp_memory_allocated,
    .sockets_allocated = &tcp_sockets_allocated,
    .orphan_count   = &tcp_orphan_count,
    .memory_pressure = &tcp_memory_pressure,
    .sysctl_mem     = sysctl_tcp_mem,
    .sysctl_wmem    = sysctl_tcp_wmem,
    .sysctl_rmem    = sysctl_tcp_rmem,
    .max_header     = MAX_TCP_HEADER,
    .obj_size       = sizeof(struct tcp_sock),
    .slab_flags     = SLAB_DESTROY_BY_RCU,
    /* ... */
};
```

---

## struct inet_sock (INET Socket)

Q: [Intermediate] struct inet_sock扩展了什么？
A: inet_sock是IPv4/IPv6 socket的基础：
```c
/* include/net/inet_sock.h */
struct inet_sock {
    /* sock必须是第一个成员 */
    struct sock     sk;
    
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
    struct ipv6_pinfo *pinet6;  // IPv6信息
#endif

    /* 便捷宏 - 直接访问sock_common中的字段 */
    #define inet_daddr     sk.__sk_common.skc_daddr
    #define inet_rcv_saddr sk.__sk_common.skc_rcv_saddr
    
    /* 端口信息 */
    __be16          inet_dport;    // 目的端口
    __u16           inet_num;      // 本地端口（主机序）
    __be32          inet_saddr;    // 源地址
    __be16          inet_sport;    // 源端口
    
    /* IP选项 */
    __s16           uc_ttl;        // 单播TTL
    __u16           cmsg_flags;    // 控制消息标志
    __u16           inet_id;       // IP ID
    struct ip_options_rcu __rcu *inet_opt;  // IP选项
    
    /* 选项标志 */
    __u8            tos;           // 服务类型
    __u8            min_ttl;       // 最小TTL
    __u8            mc_ttl;        // 组播TTL
    __u8            pmtudisc;      // PMTU发现
    __u8            recverr:1,     // 接收错误
                    is_icsk:1,     // 是否是inet_connection_sock
                    freebind:1,    // 自由绑定
                    hdrincl:1,     // 用户提供IP头
                    mc_loop:1,     // 组播环回
                    transparent:1, // 透明代理
                    mc_all:1,      // 接收所有组播
                    nodefrag:1;    // 禁止分片
    
    /* 组播 */
    int             mc_index;      // 组播出接口
    __be32          mc_addr;       // 组播地址
    struct ip_mc_socklist __rcu *mc_list; // 已加入的组播组
    
    /* cork用于UDP分片聚合 */
    struct inet_cork_full cork;
};

/* 类型转换宏 */
static inline struct inet_sock *inet_sk(const struct sock *sk)
{
    return (struct inet_sock *)sk;
}
```

Q: [Intermediate] inet_connection_sock添加了什么？
A: inet_connection_sock用于面向连接的协议(TCP)：
```c
/* include/net/inet_connection_sock.h */
struct inet_connection_sock {
    struct inet_sock      icsk_inet;   // 必须是第一个成员
    
    /* 请求队列 */
    struct request_sock_queue icsk_accept_queue;  // accept队列
    
    /* 绑定信息 */
    struct inet_bind_bucket *icsk_bind_hash;
    
    /* 超时控制 */
    unsigned long         icsk_timeout;      // 超时时间
    struct timer_list     icsk_retransmit_timer;  // 重传定时器
    struct timer_list     icsk_delack_timer;      // 延迟ACK定时器
    
    /* 拥塞控制 */
    const struct tcp_congestion_ops *icsk_ca_ops;
    void                 *icsk_ca_priv;      // 私有数据
    
    /* 连接管理 */
    __u32                icsk_rto;           // 重传超时
    __u32                icsk_pmtu_cookie;   // PMTU
    __u8                 icsk_retransmits;   // 重传次数
    __u8                 icsk_pending;       // 待处理操作
    __u8                 icsk_backoff;       // 退避指数
    __u8                 icsk_syn_retries;   // SYN重传次数
    
    /* MTU探测 */
    struct {
        __u8             enabled : 1;
        __u8             search_high_set : 1;
        __u8             search_low_set : 1;
        __u8             probe_size : 5;
    } icsk_mtup;
    
    /* ACK控制 */
    struct {
        __u8             pending;     // ACK待处理标志
        __u8             quick;       // 快速ACK计数
        __u8             pingpong;    // pingpong标志
        __u8             blocked;     // 是否阻塞
        __u32            ato;         // 估计的延迟ACK超时
        unsigned long    timeout;     // 当前超时
        __u32            lrcvtime;    // 上次接收时间
        __u16            last_seg_size; // 上个段大小
        __u16            rcv_mss;     // 接收MSS
    } icsk_ack;
};
```

---

## Socket创建流程 (Socket Creation)

Q: [Intermediate] socket系统调用的内核实现流程是什么？
A: socket()系统调用的完整流程：
```c
/* net/socket.c */
SYSCALL_DEFINE3(socket, int, family, int, type, int, protocol)
{
    int retval;
    struct socket *sock;
    int flags;

    /* 提取flags（SOCK_CLOEXEC, SOCK_NONBLOCK） */
    flags = type & ~SOCK_TYPE_MASK;
    type &= SOCK_TYPE_MASK;

    /* 创建socket */
    retval = sock_create(family, type, protocol, &sock);
    if (retval < 0)
        return retval;

    /* 映射到文件描述符 */
    retval = sock_map_fd(sock, flags);
    if (retval < 0)
        sock_release(sock);

    return retval;
}

/* 创建流程详解 */
sock_create()
    │
    ├─→ security_socket_create()  // LSM安全检查
    │
    └─→ __sock_create()
            │
            ├─→ sock_alloc()      // 分配socket结构
            │       │
            │       ├─→ new_inode()  // 创建inode
            │       └─→ 初始化socket字段
            │
            └─→ net_families[family]->create()  // 协议族创建
                    │
                    │  (以AF_INET为例)
                    └─→ inet_create()
                            │
                            ├─→ sk_alloc()     // 分配sock结构
                            ├─→ sock_init_data() // 初始化sock
                            ├─→ sk->sk_prot->init() // 协议初始化
                            └─→ 设置proto_ops
```

Q: [Intermediate] sock_alloc()和sock_release()做什么？
A: sock_alloc分配socket，sock_release释放：
```c
/* net/socket.c */
static struct socket *sock_alloc(void)
{
    struct inode *inode;
    struct socket *sock;

    /* 从socket文件系统分配inode */
    inode = new_inode_pseudo(sock_mnt->mnt_sb);
    if (!inode)
        return NULL;

    /* 获取socket（嵌入在inode中） */
    sock = SOCKET_I(inode);

    /* 设置inode属性 */
    inode->i_ino = get_next_ino();
    inode->i_mode = S_IFSOCK | S_IRWXUGO;
    inode->i_uid = current_fsuid();
    inode->i_gid = current_fsgid();
    inode->i_op = &sockfs_inode_ops;

    /* 统计计数 */
    this_cpu_add(sockets_in_use, 1);
    return sock;
}

void sock_release(struct socket *sock)
{
    if (sock->ops) {
        struct module *owner = sock->ops->owner;
        
        sock->ops->release(sock);  // 协议层释放
        sock->ops = NULL;
        module_put(owner);
    }

    if (sock->wq->fasync_list)
        pr_err("sock_release: fasync list not empty!\n");

    this_cpu_sub(sockets_in_use, 1);
    if (!sock->file) {
        iput(SOCK_INODE(sock));  // 释放inode
        return;
    }
    sock->file = NULL;
}
```

Q: [Advanced] 协议族是如何注册和查找的？
A: net_proto_family注册机制：
```c
/* include/linux/net.h */
struct net_proto_family {
    int             family;   // 协议族号
    int (*create)(struct net *net, struct socket *sock,
                  int protocol, int kern);  // 创建函数
    struct module  *owner;
};

/* net/socket.c */
static const struct net_proto_family __rcu *net_families[NPROTO];

/* 注册协议族 */
int sock_register(const struct net_proto_family *ops)
{
    int err;

    if (ops->family >= NPROTO)
        return -ENOBUFS;

    spin_lock(&net_family_lock);
    if (rcu_dereference_protected(net_families[ops->family], ...))
        err = -EEXIST;  // 已存在
    else {
        rcu_assign_pointer(net_families[ops->family], ops);
        err = 0;
    }
    spin_unlock(&net_family_lock);
    return err;
}

/* 常见协议族注册 */
// AF_INET (IPv4)
static const struct net_proto_family inet_family_ops = {
    .family = PF_INET,
    .create = inet_create,
    .owner  = THIS_MODULE,
};
sock_register(&inet_family_ops);

// AF_UNIX
static const struct net_proto_family unix_family_ops = {
    .family = PF_UNIX,
    .create = unix_create,
    .owner  = THIS_MODULE,
};
sock_register(&unix_family_ops);

// AF_NETLINK
static const struct net_proto_family netlink_family_ops = {
    .family = PF_NETLINK,
    .create = netlink_create,
    .owner  = THIS_MODULE,
};
sock_register(&netlink_family_ops);
```

---

## Socket文件操作 (Socket File Operations)

Q: [Intermediate] socket的file_operations是什么？
A: socket文件使用专用的file_operations：
```c
/* net/socket.c */
static const struct file_operations socket_file_ops = {
    .owner         = THIS_MODULE,
    .llseek        = no_llseek,        // 不支持seek
    .aio_read      = sock_aio_read,    // 异步读
    .aio_write     = sock_aio_write,   // 异步写
    .poll          = sock_poll,        // poll/select/epoll
    .unlocked_ioctl = sock_ioctl,      // ioctl
    .mmap          = sock_mmap,        // 内存映射
    .open          = sock_no_open,     // 禁止通过/proc打开
    .release       = sock_close,       // 关闭
    .fasync        = sock_fasync,      // 异步通知
    .sendpage      = sock_sendpage,    // sendfile支持
    .splice_write  = generic_splice_sendpage,
    .splice_read   = sock_splice_read,
#ifdef CONFIG_COMPAT
    .compat_ioctl  = compat_sock_ioctl,
#endif
};

/* sock_poll实现 */
static unsigned int sock_poll(struct file *file, poll_table *wait)
{
    struct socket *sock = file->private_data;
    unsigned int mask = 0;

    if (sock->ops->poll)
        mask = sock->ops->poll(file, sock, wait);

    return mask;
}

/* TCP的poll实现会检查 */
// POLLIN:  有数据可读或连接关闭
// POLLOUT: 可写（发送缓冲区有空间）
// POLLERR: 有错误
// POLLHUP: 对端关闭
// POLLPRI: 有紧急数据(OOB)
```

Q: [Intermediate] sock_map_fd()如何将socket映射到fd？
A: sock_map_fd创建文件并关联fd：
```c
/* net/socket.c */
int sock_map_fd(struct socket *sock, int flags)
{
    struct file *newfile;
    int fd;

    /* 分配文件描述符 */
    fd = get_unused_fd_flags(flags);
    if (unlikely(fd < 0))
        return fd;

    /* 创建file结构 */
    newfile = sock_alloc_file(sock, flags, NULL);
    if (likely(!IS_ERR(newfile))) {
        fd_install(fd, newfile);  // 安装到fd表
        return fd;
    }

    put_unused_fd(fd);
    return PTR_ERR(newfile);
}

struct file *sock_alloc_file(struct socket *sock, int flags, const char *dname)
{
    struct qstr name = { .name = "" };
    struct path path;
    struct file *file;

    if (dname) {
        name.name = dname;
        name.len = strlen(name.name);
    } else {
        /* 使用协议名 */
        name.name = sock->sk ? sock->sk->sk_prot_creator->name : "";
        name.len = strlen(name.name);
    }

    /* 创建dentry */
    path.dentry = d_alloc_pseudo(sock_mnt->mnt_sb, &name);
    if (!path.dentry)
        return ERR_PTR(-ENOMEM);
    path.mnt = mntget(sock_mnt);

    /* 关联inode */
    d_instantiate(path.dentry, SOCK_INODE(sock));

    /* 分配file */
    file = alloc_file(&path, FMODE_READ | FMODE_WRITE,
                      &socket_file_ops);
    if (IS_ERR(file)) {
        path_put(&path);
        return file;
    }

    sock->file = file;
    file->f_flags = O_RDWR | (flags & O_NONBLOCK);
    file->private_data = sock;
    return file;
}
```

---

## Socket系统调用实现 (System Call Implementation)

Q: [Intermediate] bind系统调用的实现流程是什么？
A: bind()将socket绑定到地址：
```c
/* net/socket.c */
SYSCALL_DEFINE3(bind, int, fd, struct sockaddr __user *, umyaddr, int, addrlen)
{
    struct socket *sock;
    struct sockaddr_storage address;
    int err, fput_needed;

    /* 查找socket */
    sock = sockfd_lookup_light(fd, &err, &fput_needed);
    if (sock) {
        /* 复制地址到内核 */
        err = move_addr_to_kernel(umyaddr, addrlen, &address);
        if (err >= 0) {
            /* 安全检查 */
            err = security_socket_bind(sock, (struct sockaddr *)&address, addrlen);
            if (!err)
                /* 调用协议层bind */
                err = sock->ops->bind(sock, (struct sockaddr *)&address, addrlen);
        }
        fput_light(sock->file, fput_needed);
    }
    return err;
}

/* inet_bind实现(IPv4) */
int inet_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
    struct sockaddr_in *addr = (struct sockaddr_in *)uaddr;
    struct sock *sk = sock->sk;
    struct inet_sock *inet = inet_sk(sk);
    unsigned short snum;
    int chk_addr_ret;
    int err;

    /* 检查地址长度 */
    if (addr_len < sizeof(struct sockaddr_in))
        return -EINVAL;

    /* 检查地址族 */
    if (addr->sin_family != AF_INET)
        return -EAFNOSUPPORT;

    chk_addr_ret = inet_addr_type(sock_net(sk), addr->sin_addr.s_addr);

    snum = ntohs(addr->sin_port);

    /* 如果有协议特定的bind */
    if (sk->sk_prot->bind) {
        err = sk->sk_prot->bind(sk, uaddr, addr_len);
        if (err)
            return err;
    }

    lock_sock(sk);

    /* 检查socket状态 */
    err = -EINVAL;
    if (sk->sk_state != TCP_CLOSE || inet->inet_num)
        goto out_release_sock;

    /* 设置本地地址 */
    inet->inet_rcv_saddr = inet->inet_saddr = addr->sin_addr.s_addr;

    /* 获取端口 */
    if (snum || !(inet->bind_address_no_port)) {
        if (sk->sk_prot->get_port(sk, snum)) {
            inet->inet_saddr = inet->inet_rcv_saddr = 0;
            err = -EADDRINUSE;
            goto out_release_sock;
        }
    }

    inet->inet_sport = htons(inet->inet_num);
    inet->inet_daddr = 0;
    inet->inet_dport = 0;
    sk_dst_reset(sk);
    err = 0;

out_release_sock:
    release_sock(sk);
    return err;
}
```

Q: [Intermediate] listen系统调用做了什么？
A: listen()将socket转为监听状态：
```c
/* net/socket.c */
SYSCALL_DEFINE2(listen, int, fd, int, backlog)
{
    struct socket *sock;
    int err, fput_needed;
    int somaxconn;

    sock = sockfd_lookup_light(fd, &err, &fput_needed);
    if (sock) {
        /* 限制backlog大小 */
        somaxconn = sock_net(sock->sk)->core.sysctl_somaxconn;
        if ((unsigned int)backlog > somaxconn)
            backlog = somaxconn;

        err = security_socket_listen(sock, backlog);
        if (!err)
            err = sock->ops->listen(sock, backlog);

        fput_light(sock->file, fput_needed);
    }
    return err;
}

/* inet_listen实现 */
int inet_listen(struct socket *sock, int backlog)
{
    struct sock *sk = sock->sk;
    int err = -EINVAL;

    lock_sock(sk);

    /* 检查socket类型和状态 */
    if (sock->state != SS_UNCONNECTED || sock->type != SOCK_STREAM)
        goto out;

    /* 如果还没listen过 */
    if (sk->sk_state != TCP_LISTEN) {
        /* 初始化accept队列 */
        err = inet_csk_listen_start(sk, backlog);
        if (err)
            goto out;
    }
    
    /* 更新backlog */
    sk->sk_max_ack_backlog = backlog;
    err = 0;

out:
    release_sock(sk);
    return err;
}

/* inet_csk_listen_start */
int inet_csk_listen_start(struct sock *sk, int nr_table_entries)
{
    struct inet_connection_sock *icsk = inet_csk(sk);

    /* 分配SYN请求哈希表 */
    reqsk_queue_alloc(&icsk->icsk_accept_queue);

    /* 设置状态为TCP_LISTEN */
    sk->sk_state = TCP_LISTEN;
    
    /* 将socket加入监听哈希表 */
    if (!sk->sk_prot->get_port(sk, inet_sk(sk)->inet_num)) {
        inet_sk(sk)->inet_sport = htons(inet_sk(sk)->inet_num);
        sk_dst_reset(sk);
        sk->sk_prot->hash(sk);
        return 0;
    }

    sk->sk_state = TCP_CLOSE;
    return -EADDRINUSE;
}
```

Q: [Intermediate] accept系统调用的实现是什么？
A: accept()从监听socket接受连接：
```c
/* net/socket.c */
SYSCALL_DEFINE4(accept4, int, fd, struct sockaddr __user *, upeer_sockaddr,
                int __user *, upeer_addrlen, int, flags)
{
    struct socket *sock, *newsock;
    struct file *newfile;
    int err, len, newfd, fput_needed;
    struct sockaddr_storage address;

    /* 查找监听socket */
    sock = sockfd_lookup_light(fd, &err, &fput_needed);
    if (!sock)
        goto out;

    err = -ENFILE;
    /* 创建新socket */
    newsock = sock_alloc();
    if (!newsock)
        goto out_put;

    newsock->type = sock->type;
    newsock->ops = sock->ops;

    /* 分配新的fd和file */
    newfd = get_unused_fd_flags(flags);
    if (unlikely(newfd < 0)) {
        err = newfd;
        sock_release(newsock);
        goto out_put;
    }
    newfile = sock_alloc_file(newsock, flags, sock->sk->sk_prot_creator->name);
    if (IS_ERR(newfile)) {
        err = PTR_ERR(newfile);
        put_unused_fd(newfd);
        sock_release(newsock);
        goto out_put;
    }

    /* 安全检查 */
    err = security_socket_accept(sock, newsock);
    if (err)
        goto out_fd;

    /* 调用协议层accept（可能阻塞） */
    err = sock->ops->accept(sock, newsock, sock->file->f_flags);
    if (err < 0)
        goto out_fd;

    /* 返回对端地址 */
    if (upeer_sockaddr) {
        err = newsock->ops->getname(newsock, (struct sockaddr *)&address, &len, 2);
        if (err < 0)
            goto out_fd;
        err = move_addr_to_user(&address, len, upeer_sockaddr, upeer_addrlen);
        if (err < 0)
            goto out_fd;
    }

    fd_install(newfd, newfile);
    err = newfd;
    goto out_put;

out_fd:
    fput(newfile);
    put_unused_fd(newfd);
out_put:
    fput_light(sock->file, fput_needed);
out:
    return err;
}

/* inet_csk_accept - TCP accept实现 */
struct sock *inet_csk_accept(struct sock *sk, int flags, int *err)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct request_sock_queue *queue = &icsk->icsk_accept_queue;
    struct sock *newsk;
    int error;

    lock_sock(sk);

    /* 检查socket状态 */
    error = -EINVAL;
    if (sk->sk_state != TCP_LISTEN)
        goto out_err;

    /* 如果accept队列为空 */
    if (reqsk_queue_empty(queue)) {
        /* 非阻塞直接返回 */
        if (flags & O_NONBLOCK) {
            error = -EAGAIN;
            goto out_err;
        }
        /* 阻塞等待新连接 */
        error = inet_csk_wait_for_connect(sk, timeo);
        if (error)
            goto out_err;
    }

    /* 从accept队列取出一个已完成三次握手的连接 */
    newsk = reqsk_queue_get_child(queue, sk);

out:
    release_sock(sk);
    return newsk;
out_err:
    newsk = NULL;
    *err = error;
    goto out;
}
```

---

## Socket缓冲区管理 (Buffer Management)

Q: [Intermediate] socket的接收和发送缓冲区如何工作？
A: socket使用sk_buff队列管理缓冲区：
```c
/* 接收缓冲区 */
struct sock {
    struct sk_buff_head sk_receive_queue;  // 接收队列
    int                 sk_rcvbuf;          // 缓冲区大小上限
    atomic_t            sk_rmem_alloc;      // 已分配大小
    
    /* backlog队列（软中断使用） */
    struct {
        atomic_t        rmem_alloc;
        int             len;
        struct sk_buff  *head;
        struct sk_buff  *tail;
    } sk_backlog;
};

/* 发送缓冲区 */
struct sock {
    struct sk_buff_head sk_write_queue;    // 发送队列
    int                 sk_sndbuf;          // 缓冲区大小上限
    atomic_t            sk_wmem_alloc;      // 已分配大小
    int                 sk_wmem_queued;     // 队列中的数据量
};

/* 接收数据流程 */
网卡中断
   │
   v
软中断(NET_RX_SOFTIRQ)
   │
   v
tcp_v4_rcv()
   │
   ├─→ 如果sock被用户进程持有
   │      sk_add_backlog(sk, skb)  // 加入backlog
   │
   └─→ 否则
          tcp_v4_do_rcv(sk, skb)
             │
             v
          tcp_rcv_established()
             │
             v
          __skb_queue_tail(&sk->sk_receive_queue, skb)
             │
             v
          sk->sk_data_ready(sk)  // 唤醒等待进程
```

Q: [Advanced] SO_SNDBUF和SO_RCVBUF如何影响缓冲区？
A: socket选项控制缓冲区大小：
```c
/* 设置缓冲区大小 */
// 用户调用
setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

/* 内核处理 - net/core/sock.c */
case SO_SNDBUF:
    /* 不能超过系统最大值的两倍 */
    val = min_t(u32, val, sysctl_wmem_max);
    sk->sk_userlocks |= SOCK_SNDBUF_LOCK;
    sk->sk_sndbuf = max_t(u32, val * 2, SOCK_MIN_SNDBUF);
    sk->sk_write_space(sk);  // 通知写空间变化
    break;

case SO_RCVBUF:
    val = min_t(u32, val, sysctl_rmem_max);
    sk->sk_userlocks |= SOCK_RCVBUF_LOCK;
    sk->sk_rcvbuf = max_t(u32, val * 2, SOCK_MIN_RCVBUF);
    break;

/* 相关sysctl参数 */
net.core.rmem_default = 212992   // 默认接收缓冲区
net.core.rmem_max = 212992       // 最大接收缓冲区
net.core.wmem_default = 212992   // 默认发送缓冲区
net.core.wmem_max = 212992       // 最大发送缓冲区

/* TCP特定参数 */
net.ipv4.tcp_rmem = 4096 87380 6291456  // min default max
net.ipv4.tcp_wmem = 4096 16384 4194304  // min default max
```

---

## Socket回调函数 (Socket Callbacks)

Q: [Intermediate] socket的回调函数是如何设置和使用的？
A: sock结构体中定义了多个回调：
```c
struct sock {
    /* 状态变化回调 */
    void (*sk_state_change)(struct sock *sk);
    
    /* 数据就绪回调 */
    void (*sk_data_ready)(struct sock *sk, int bytes);
    
    /* 写空间可用回调 */
    void (*sk_write_space)(struct sock *sk);
    
    /* 错误报告回调 */
    void (*sk_error_report)(struct sock *sk);
    
    /* backlog处理回调 */
    int (*sk_backlog_rcv)(struct sock *sk, struct sk_buff *skb);
    
    /* 销毁回调 */
    void (*sk_destruct)(struct sock *sk);
};

/* 默认回调 - sock_init_data()中设置 */
void sock_init_data(struct socket *sock, struct sock *sk)
{
    /* ... */
    sk->sk_state_change = sock_def_wakeup;
    sk->sk_data_ready   = sock_def_readable;
    sk->sk_write_space  = sock_def_write_space;
    sk->sk_error_report = sock_def_error_report;
    sk->sk_destruct     = sock_def_destruct;
    /* ... */
}

/* 默认唤醒实现 */
static void sock_def_wakeup(struct sock *sk)
{
    struct socket_wq *wq = rcu_dereference(sk->sk_wq);
    if (wq_has_sleeper(wq))
        wake_up_interruptible_all(&wq->wait);
}

static void sock_def_readable(struct sock *sk, int bytes)
{
    struct socket_wq *wq = rcu_dereference(sk->sk_wq);
    if (wq_has_sleeper(wq))
        wake_up_interruptible_sync_poll(&wq->wait, POLLIN | POLLPRI);
    sk_wake_async(sk, SOCK_WAKE_WAITD, POLL_IN);
}
```

Q: [Advanced] 如何自定义socket回调？
A: 内核模块可以替换默认回调：
```c
/* 示例：自定义data_ready回调 */
static void my_data_ready(struct sock *sk, int bytes)
{
    struct my_private *priv = sk->sk_user_data;
    
    /* 自定义处理 */
    pr_info("Data ready: %d bytes\n", bytes);
    
    /* 设置标志 */
    set_bit(DATA_READY, &priv->flags);
    
    /* 唤醒等待者 */
    wake_up(&priv->waitq);
    
    /* 可选：调用原始回调 */
    priv->orig_data_ready(sk, bytes);
}

/* 安装回调 */
static int my_sock_init(struct sock *sk)
{
    struct my_private *priv;
    
    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;
    
    /* 保存原始回调 */
    priv->orig_data_ready = sk->sk_data_ready;
    priv->orig_state_change = sk->sk_state_change;
    
    /* 安装自定义回调 */
    sk->sk_user_data = priv;
    sk->sk_data_ready = my_data_ready;
    
    return 0;
}

/* 恢复回调 */
static void my_sock_cleanup(struct sock *sk)
{
    struct my_private *priv = sk->sk_user_data;
    
    if (priv) {
        sk->sk_data_ready = priv->orig_data_ready;
        sk->sk_user_data = NULL;
        kfree(priv);
    }
}
```

---

## Socket选项 (Socket Options)

Q: [Intermediate] SOL_SOCKET级别有哪些常用选项？
A: 通用socket选项定义在asm/socket.h：
```c
/* 常用SOL_SOCKET选项 */
SO_DEBUG       1   // 启用调试
SO_REUSEADDR   2   // 允许地址重用
SO_TYPE        3   // 获取socket类型
SO_ERROR       4   // 获取并清除错误
SO_DONTROUTE   5   // 不使用路由
SO_BROADCAST   6   // 允许发送广播
SO_SNDBUF      7   // 发送缓冲区大小
SO_RCVBUF      8   // 接收缓冲区大小
SO_KEEPALIVE   9   // 保持连接活跃
SO_OOBINLINE  10   // 带外数据内联
SO_LINGER     13   // 关闭时延迟
SO_RCVTIMEO   20   // 接收超时
SO_SNDTIMEO   21   // 发送超时
SO_BINDTODEVICE 25 // 绑定到设备
SO_REUSEPORT  15   // 允许端口重用(3.9+)

/* 设置示例 */
int optval = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

struct linger lg = {.l_onoff = 1, .l_linger = 5};
setsockopt(fd, SOL_SOCKET, SO_LINGER, &lg, sizeof(lg));
```

Q: [Advanced] SO_REUSEADDR和SO_REUSEPORT的区别是什么？
A: 两者解决不同的端口复用问题：
```c
/* SO_REUSEADDR */
作用：
1. 允许bind到TIME_WAIT状态的地址
2. 允许bind到0.0.0.0即使已有特定地址bind
3. 快速重启服务

/* SO_REUSEPORT (Linux 3.9+) */
作用：
1. 允许多个socket bind到完全相同的地址和端口
2. 内核在这些socket间做负载均衡
3. 用于多进程/多线程服务器

/* 示例场景 */
// SO_REUSEADDR: 服务器重启时快速绑定
Server crash → restart → bind()失败(端口在TIME_WAIT)
使用SO_REUSEADDR → bind()成功

// SO_REUSEPORT: 多工作进程
Worker1: bind(8080) with SO_REUSEPORT ✓
Worker2: bind(8080) with SO_REUSEPORT ✓
Worker3: bind(8080) with SO_REUSEPORT ✓
// 内核负载均衡incoming连接到3个worker

/* 内核实现 */
// net/ipv4/inet_connection_sock.c
inet_csk_get_port()检查:
- sk_reuseport && sk2->sk_reuseport → 允许
- sk_reuse && sk2->sk_reuse && sk2->sk_state != TCP_LISTEN → 允许
```

---

## 内核Socket API (Kernel Socket API)

Q: [Intermediate] 内核模块如何创建和使用socket？
A: 内核提供kernel_*系列函数：
```c
/* include/linux/net.h */

/* 创建内核socket */
int sock_create_kern(int family, int type, int proto, struct socket **res);

/* 绑定地址 */
int kernel_bind(struct socket *sock, struct sockaddr *addr, int addrlen);

/* 监听 */
int kernel_listen(struct socket *sock, int backlog);

/* 接受连接 */
int kernel_accept(struct socket *sock, struct socket **newsock, int flags);

/* 连接 */
int kernel_connect(struct socket *sock, struct sockaddr *addr,
                   int addrlen, int flags);

/* 发送数据 */
int kernel_sendmsg(struct socket *sock, struct msghdr *msg,
                   struct kvec *vec, size_t num, size_t len);

/* 接收数据 */
int kernel_recvmsg(struct socket *sock, struct msghdr *msg,
                   struct kvec *vec, size_t num, size_t len, int flags);

/* 设置选项 */
int kernel_setsockopt(struct socket *sock, int level, int optname,
                      char *optval, unsigned int optlen);

/* 关闭连接 */
int kernel_sock_shutdown(struct socket *sock, enum sock_shutdown_cmd how);

/* 示例：内核TCP客户端 */
static int kernel_tcp_connect(const char *ip, int port)
{
    struct socket *sock;
    struct sockaddr_in addr;
    int ret;

    /* 创建socket */
    ret = sock_create_kern(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
    if (ret < 0)
        return ret;

    /* 设置服务器地址 */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = in_aton(ip);

    /* 连接 */
    ret = kernel_connect(sock, (struct sockaddr *)&addr, sizeof(addr), 0);
    if (ret < 0) {
        sock_release(sock);
        return ret;
    }

    /* 发送数据 */
    struct kvec iov = {
        .iov_base = "Hello",
        .iov_len = 5,
    };
    struct msghdr msg = {
        .msg_flags = MSG_DONTWAIT,
    };
    ret = kernel_sendmsg(sock, &msg, &iov, 1, 5);

    /* 清理 */
    kernel_sock_shutdown(sock, SHUT_RDWR);
    sock_release(sock);
    return ret;
}
```

Q: [Intermediate] sockfd_lookup和fput的使用方法？
A: 从fd获取socket结构：
```c
/* 从用户空间fd获取socket */

/* 方法1：简单查找 */
struct socket *sockfd_lookup(int fd, int *err)
{
    struct file *file = fget(fd);
    struct socket *sock;

    if (!file) {
        *err = -EBADF;
        return NULL;
    }

    sock = sock_from_file(file, err);
    if (!sock)
        fput(file);
    return sock;
}

/* 使用后必须调用sockfd_put */
#define sockfd_put(sock) fput(sock->file)

/* 方法2：轻量级查找（更高效） */
struct socket *sockfd_lookup_light(int fd, int *err, int *fput_needed)
{
    struct file *file = fget_light(fd, fput_needed);
    struct socket *sock;

    if (file) {
        sock = sock_from_file(file, err);
        if (sock)
            return sock;
        fput_light(file, *fput_needed);
    }
    return NULL;
}

/* 使用示例 */
int my_ioctl_handler(int fd, ...)
{
    struct socket *sock;
    int err, fput_needed;

    sock = sockfd_lookup_light(fd, &err, &fput_needed);
    if (!sock)
        return err;

    /* 操作socket */
    err = do_something(sock);

    /* 释放引用 */
    fput_light(sock->file, fput_needed);
    return err;
}
```

---

## I/O多路复用 (I/O Multiplexing)

Q: [Intermediate] socket的poll实现是什么？
A: poll检查socket的可读写状态：
```c
/* TCP socket的poll实现 */
unsigned int tcp_poll(struct file *file, struct socket *sock,
                      poll_table *wait)
{
    struct sock *sk = sock->sk;
    unsigned int mask;

    sock_poll_wait(file, sk_sleep(sk), wait);

    mask = 0;

    /* 连接状态检查 */
    if (sk->sk_state == TCP_LISTEN)
        return inet_csk_listen_poll(sk);

    /* 检查错误 */
    if (sk->sk_err || !skb_queue_empty(&sk->sk_error_queue))
        mask |= POLLERR;

    /* 对端关闭 */
    if (sk->sk_shutdown == SHUTDOWN_MASK || sk->sk_state == TCP_CLOSE)
        mask |= POLLHUP;

    /* 本端读关闭 */
    if (sk->sk_shutdown & RCV_SHUTDOWN)
        mask |= POLLIN | POLLRDNORM | POLLRDHUP;

    /* 可读 */
    if (!skb_queue_empty(&sk->sk_receive_queue))
        mask |= POLLIN | POLLRDNORM;

    /* 连接完成或失败 */
    if (sk->sk_state == TCP_SYN_SENT)
        return mask;

    /* 可写 */
    if (sk_stream_wspace(sk) >= sk_stream_min_wspace(sk))
        mask |= POLLOUT | POLLWRNORM;

    /* 紧急数据 */
    if (tp->urg_data & TCP_URG_VALID)
        mask |= POLLPRI;

    return mask;
}

/* poll_table操作 */
static inline void sock_poll_wait(struct file *filp,
                                   wait_queue_head_t *wait_address,
                                   poll_table *p)
{
    if (!poll_does_not_wait(p) && wait_address) {
        poll_wait(filp, wait_address, p);
        smp_mb();
    }
}
```

Q: [Advanced] epoll如何与socket交互？
A: epoll使用回调机制：
```c
/* 注册到epoll时 */
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)
   │
   v
ep_insert() / ep_modify()
   │
   v
ep_item_poll()  // 调用socket的poll
   │
   └─→ tcp_poll(file, sock, &pt)
          │
          └─→ poll_wait(file, sk_sleep(sk), pt)
                 │
                 └─→ 将epi加入socket的等待队列

/* 当数据到达时 */
tcp_data_ready(sk)
   │
   └─→ wake_up_interruptible_sync_poll(&wq->wait, POLLIN)
          │
          └─→ ep_poll_callback()  // 被唤醒的回调
                 │
                 ├─→ 将epi加入rdllist
                 └─→ 唤醒epoll_wait()

/* epoll_wait返回 */
epoll_wait(epfd, events, maxevents, timeout)
   │
   v
遍历rdllist
   │
   └─→ ep_item_poll(epi, NULL)  // 再次poll确认事件
          │
          └─→ 返回就绪的events
```

---

## 常见错误和调试 (Common Errors and Debugging)

Q: [Intermediate] socket编程中常见的内核态错误有哪些？
A: 常见错误及原因：
```c
/* 错误1：未持有锁访问socket */
// 错误
skb_queue_tail(&sk->sk_receive_queue, skb);

// 正确
lock_sock(sk);
skb_queue_tail(&sk->sk_receive_queue, skb);
release_sock(sk);

/* 错误2：在中断上下文睡眠 */
// 错误 - 在软中断中调用
lock_sock(sk);  // 可能睡眠！

// 正确
bh_lock_sock(sk);
if (sock_owned_by_user(sk)) {
    sk_add_backlog(sk, skb);
} else {
    /* 处理 */
}
bh_unlock_sock(sk);

/* 错误3：忘记增加引用计数 */
// 错误
struct sock *sk = some_lookup(key);
/* sk可能被其他上下文释放 */
sk->sk_xxx = value;

// 正确
struct sock *sk = some_lookup(key);
sock_hold(sk);
/* 安全使用 */
sk->sk_xxx = value;
sock_put(sk);

/* 错误4：不检查返回值 */
// 错误
skb = alloc_skb(len, GFP_KERNEL);
skb->data = ...;  // skb可能为NULL!

// 正确
skb = alloc_skb(len, GFP_KERNEL);
if (!skb)
    return -ENOMEM;
```

Q: [Intermediate] 如何调试socket问题？
A: socket调试方法：
```bash
# 1. 查看socket统计
$ ss -s
Total: 168 (kernel 0)
TCP:   12 (estab 5, closed 2, orphaned 0, synrecv 0, timewait 2/0)

# 2. 查看socket详细信息
$ ss -tanp
State  Recv-Q Send-Q Local:Port  Peer:Port
ESTAB  0      0      10.0.0.1:22 10.0.0.2:54321

# 3. 查看socket内存
$ cat /proc/net/sockstat
sockets: used 168
TCP: inuse 5 orphan 0 tw 2 alloc 7 mem 2
UDP: inuse 3 mem 0

# 4. ftrace跟踪
$ echo 'tcp_sendmsg' >> /sys/kernel/debug/tracing/set_ftrace_filter
$ echo function > /sys/kernel/debug/tracing/current_tracer
$ cat /sys/kernel/debug/tracing/trace

# 5. 使用bpftrace
$ bpftrace -e 'kprobe:tcp_sendmsg { @[comm] = count(); }'

# 6. 内核配置选项
CONFIG_NET_DEBUG=y
CONFIG_SOCK_DEBUG=y
```

---

## 最佳实践 (Best Practices)

Q: [Advanced] 如何正确实现一个协议族？
A: 实现新协议族的标准模式：
```c
/* 1. 定义proto_ops */
static const struct proto_ops my_proto_ops = {
    .family     = AF_MY,
    .owner      = THIS_MODULE,
    .release    = my_release,
    .bind       = my_bind,
    .connect    = my_connect,
    .accept     = my_accept,
    .listen     = my_listen,
    .sendmsg    = my_sendmsg,
    .recvmsg    = my_recvmsg,
    .poll       = my_poll,
    .ioctl      = my_ioctl,
    .getname    = my_getname,
    .shutdown   = my_shutdown,
    /* 不支持的操作使用sock_no_* */
    .socketpair = sock_no_socketpair,
    .mmap       = sock_no_mmap,
};

/* 2. 定义proto */
static struct proto my_proto = {
    .name       = "MY",
    .owner      = THIS_MODULE,
    .obj_size   = sizeof(struct my_sock),
    .init       = my_init,
    .close      = my_close,
    .connect    = my_connect_proto,
    .disconnect = my_disconnect,
    .sendmsg    = my_sendmsg_proto,
    .recvmsg    = my_recvmsg_proto,
};

/* 3. 实现create函数 */
static int my_create(struct net *net, struct socket *sock,
                     int protocol, int kern)
{
    struct sock *sk;

    if (sock->type != SOCK_DGRAM)
        return -ESOCKTNOSUPPORT;

    sock->ops = &my_proto_ops;

    sk = sk_alloc(net, AF_MY, GFP_KERNEL, &my_proto);
    if (!sk)
        return -ENOMEM;

    sock_init_data(sock, sk);
    sk->sk_protocol = protocol;

    return 0;
}

/* 4. 定义net_proto_family */
static const struct net_proto_family my_family = {
    .family = AF_MY,
    .create = my_create,
    .owner  = THIS_MODULE,
};

/* 5. 模块初始化 */
static int __init my_init_module(void)
{
    int ret;

    ret = proto_register(&my_proto, 1);
    if (ret)
        return ret;

    ret = sock_register(&my_family);
    if (ret) {
        proto_unregister(&my_proto);
        return ret;
    }

    return 0;
}

static void __exit my_exit_module(void)
{
    sock_unregister(AF_MY);
    proto_unregister(&my_proto);
}

module_init(my_init_module);
module_exit(my_exit_module);
```

