# epoll 多路复用 - 内核执行路径详解

> 🎯 **学习目标**: 深入理解 Linux epoll 机制的内核实现，掌握从事件注册到通知的完整路径和高性能原理

---

## 🧩 第一部分：宏观架构图

```
User Space
├── Application (Web Server / Game Server / Proxy...)
│   ├── epoll_create1(EPOLL_CLOEXEC)
│   ├── epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event)
│   ├── epoll_wait(epfd, events, maxevents, timeout)
│   └── Handle Ready Events
├── glibc Syscall Wrapper
└── High-performance Event-driven Model
    ├── Reactor Pattern
    └── Async Non-blocking I/O

════════════════════════════════════════════════════════════════════════════════

Kernel Space - epoll Subsystem
├── epoll Instance Management
│   ├── struct eventpoll                     [epoll Core Structure]
│   ├── RB-Tree                              [Manage Monitored fds]
│   ├── Ready List                           [Store Ready Events]
│   └── Wait Queue                           [Blocked User Processes]
├── Syscall Handling
│   ├── sys_epoll_create1()                  [Create epoll Instance]
│   ├── sys_epoll_ctl()                      [Add/Modify/Delete fd]
│   │   ├── EPOLL_CTL_ADD                    [Add Monitor]
│   │   ├── EPOLL_CTL_MOD                    [Modify Events]
│   │   └── EPOLL_CTL_DEL                    [Remove Monitor]
│   └── sys_epoll_wait()                     [Wait for Ready Events]
│       ├── ep_poll()                        [Core Poll Logic]
│       ├── schedule_timeout()               [Process Scheduling Wait]
│       └── ep_send_events()                 [Deliver Events to Userspace]
├── Callback Mechanism
│   ├── ep_poll_callback()                   [Event Callback]
│   ├── wake_up_locked()                     [Wake Waiting Process]
│   └── Ready Event List Management

════════════════════════════════════════════════════════════════════════════════

File Subsystem Integration
├── struct file Extension
│   └── f_ep_links                           [Reverse Link to epoll]
├── Poll Ops for Various File Types
│   ├── socket_file_ops.poll()               [Network Socket]
│   │   └── sock_poll()                      [TCP/UDP State Check]
│   ├── pipe_poll()                          [Pipe]
│   ├── eventfd_poll()                       [eventfd]
│   ├── timerfd_poll()                       [timerfd]
│   └── signalfd_poll()                      [signalfd]
└── Event Notification
    ├── Data Arrival → Wake epoll
    ├── Connection Established → Wake epoll
    ├── Buffer Writable → Wake epoll
    └── Exception Condition → Wake epoll

════════════════════════════════════════════════════════════════════════════════

Networking Subsystem
├── TCP/UDP Protocol Stack
│   ├── Packet Arrival → sk_data_ready()     [Trigger Readable Event]
│   ├── Send Buffer Available → sk_write_space() [Trigger Writable Event]
│   └── Connection State Change → sk_state_change() [Trigger Conn Event]
├── Network Interrupt Handling
│   ├── Hardware IRQ → Soft IRQ (NET_RX_SOFTIRQ)
│   ├── Protocol Stack → Wake Waiting Socket
│   └── epoll_callback → Wake User Process
└── Performance Optimizations
    ├── NAPI (New API) Polling
    ├── Interrupt Coalescing
    └── CPU Affinity
```

*图表说明：epoll 在用户态通过 create/ctl/wait 管理事件监听；内核用红黑树管理 fd、就绪链表存储事件，文件/网络子系统通过 poll 回调和 sk_data_ready 等机制在 I/O 就绪时唤醒等待进程，实现 O(1) 复杂度的事件驱动模型。*

---

## 🔬 第二部分：内核执行路径

### 2.1 epoll_create1() - 创建 epoll 实例

```c
// 用户态调用
int epfd = epoll_create1(EPOLL_CLOEXEC);

// 内核路径展开
SYSCALL_DEFINE1(epoll_create1, int, flags)
├── ep_alloc(&ep)                            // 🔥 分配 eventpoll 结构
│   ├── kmem_cache_alloc(epi_cache)          // 从 slab 分配内存
│   ├── init_waitqueue_head(&ep->wq)         // 初始化等待队列
│   ├── init_waitqueue_head(&ep->poll_wait)  // 初始化轮询等待队列
│   ├── INIT_LIST_HEAD(&ep->rdllist)         // 初始化就绪链表
│   ├── RB_CLEAR_NODE(&ep->rbr)              // 初始化红黑树根
│   ├── ep->ovflist = EP_UNACTIVE_PTR        // 溢出链表初始化
│   └── 设置各种锁和计数器
├── anon_inode_getfile("eventpoll", &eventpoll_fops, ep, O_RDWR | flags)
│   ├── 创建匿名 inode                       // epoll 实例的文件表示
│   ├── 分配 struct file                     // 文件描述符封装
│   └── 设置 file_operations 为 eventpoll_fops
├── fd_install(fd, file)                     // 安装到进程 fd 表
└── 返回 epoll fd

// eventpoll 结构体初始化完成后的状态
struct eventpoll {
    wait_queue_head_t wq;           // 用户进程等待队列 (epoll_wait 阻塞在此)
    wait_queue_head_t poll_wait;    // 内部轮询等待队列
    struct list_head rdllist;       // 就绪事件链表 (空)
    struct rb_root_cached rbr;      // 红黑树根 (空)
    struct epitem *ovflist;         // 溢出链表 (初始为 EP_UNACTIVE_PTR)
    // ... 其他字段
};
```

### 2.2 epoll_ctl() - 管理监听的文件描述符

```c
// 用户态调用  
epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);

// 内核路径展开
SYSCALL_DEFINE4(epoll_ctl, int, epfd, int, op, int, fd,
                struct epoll_event __user *, event)
├── f = fdget(epfd)                          // 获取 epoll file 结构
├── tf = fdget(fd)                           // 获取目标 file 结构
├── 验证文件类型和权限
├── copy_from_user(&epds, event)             // 拷贝事件结构从用户态
├── ep = f.file->private_data                // 获取 eventpoll 结构
├── mutex_lock(&ep->mtx)                     // 获取 epoll 互斥锁
├── switch (op) {
│   ├── case EPOLL_CTL_ADD:                  // 🔥 添加监听 fd
│   │   ├── 检查是否已存在 (红黑树查找)
│   │   ├── ep_insert(ep, &epds, tf.file, fd, full_check) // 核心插入逻辑
│   │   │   ├── ep_item_alloc()              // 分配 epitem 结构
│   │   │   ├── 初始化 epitem 字段
│   │   │   │   ├── epi->ep = ep             // 反向指针
│   │   │   │   ├── epi->ffd.file = tfile    // 目标文件
│   │   │   │   ├── epi->ffd.fd = fd         // 文件描述符  
│   │   │   │   ├── epi->event = *event      // 事件掩码
│   │   │   │   └── INIT_LIST_HEAD(&epi->rdllink) // 就绪链表节点
│   │   │   ├── 🔥 关键：注册回调函数
│   │   │   │   ├── init_poll_funcptr(&pwq, ep_ptable_queue_proc)
│   │   │   │   ├── revents = ep_item_poll(epi, &pt, 1) // 调用文件 poll 
│   │   │   │   │   └── epi->ffd.file->f_op->poll(epi->ffd.file, &pt)
│   │   │   │   │       └── sock_poll() / pipe_poll() / ... // 具体文件类型的 poll
│   │   │   │   │           ├── poll_wait(file, &sk->sk_sleep, pt) // 注册到等待队列
│   │   │   │   │           │   └── pt->_qproc(file, &sk->sk_sleep, pt)
│   │   │   │   │           │       └── ep_ptable_queue_proc() // 🔥 epoll 队列处理函数
│   │   │   │   │           │           ├── 分配 eppoll_entry  
│   │   │   │   │           │           ├── init_waitqueue_func_entry(&pwq->wait, ep_poll_callback)
│   │   │   │   │           │           ├── add_wait_queue(whead, &pwq->wait) // 添加到文件等待队列
│   │   │   │   │           │           └── pwq->wait.func = ep_poll_callback // 🔥🔥🔥 核心回调
│   │   │   │   │           └── 返回当前文件状态 (POLLIN/POLLOUT/...)
│   │   │   │   └── 如果当前就绪，立即添加到就绪链表
│   │   │   ├── ep_rbtree_insert(ep, epi)    // 插入红黑树
│   │   │   └── 反向链接：file->f_ep_links
│   │   └── 如果有就绪事件，唤醒等待的进程
│   ├── case EPOLL_CTL_MOD:                  // 修改监听事件
│   │   ├── epi = ep_find(ep, tf.file, fd)   // 红黑树查找现有项
│   │   ├── 更新 epi->event
│   │   ├── 重新调用 ep_item_poll() 检查状态
│   │   └── 如果状态变化，可能需要唤醒进程
│   └── case EPOLL_CTL_DEL:                  // 删除监听
│       ├── epi = ep_find(ep, tf.file, fd)   // 查找要删除的项
│       ├── ep_remove(ep, epi)               // 从红黑树删除
│       │   ├── rb_erase(&epi->rbn, &ep->rbr) // 从红黑树移除
│       │   ├── 从就绪链表移除 (如果在其中)
│       │   ├── 从文件等待队列移除回调
│       │   └── 清理反向链接
│       └── ep_free(epi)                     // 释放 epitem 内存
├── mutex_unlock(&ep->mtx)                   // 释放 epoll 互斥锁
└── 返回操作结果
```

### 2.3 epoll_wait() - 等待事件就绪

```c
// 用户态调用
int nfds = epoll_wait(epfd, events, maxevents, timeout);

// 内核路径展开  
SYSCALL_DEFINE4(epoll_wait, int, epfd, struct epoll_event __user *, events,
                int, maxevents, int, timeout)
├── f = fdget(epfd)                          // 获取 epoll file
├── ep = f.file->private_data                // 获取 eventpoll
├── ep_poll(ep, events, maxevents, timeout)  // 🔥 核心轮询逻辑
│   ├── ktime_t expires = timespec64_to_ktime(end_time) // 计算超时时间
│   ├── wait_queue_entry_t wait              // 等待队列项
│   ├── init_waitqueue_entry(&wait, current) // 初始化为当前进程
│   ├── spin_lock_irqsave(&ep->wq.lock)      // 获取等待队列锁
│   ├── __add_wait_queue_exclusive(&ep->wq, &wait) // 添加到等待队列 
│   ├── for (;;) {                           // 🔥 主循环
│   │   ├── set_current_state(TASK_INTERRUPTIBLE) // 设置进程为可中断等待
│   │   ├── 🔥 检查就绪链表：
│   │   │   ├── if (!list_empty_careful(&ep->rdllist)) // 快速检查
│   │   │   └── res = ep_send_events(ep, events, maxevents) // 发送事件
│   │   │       ├── ep_scan_ready_list(ep, ep_send_events_proc, &esed, 0, false)
│   │   │       │   ├── list_splice_init(&ep->rdllist, &txlist) // 转移就绪链表
│   │   │       │   ├── ep_send_events_proc()        // 处理每个就绪事件
│   │   │       │   │   ├── ep_item_poll()           // 重新检查事件状态
│   │   │       │   │   ├── 如果是 EPOLLET (边缘触发)，从就绪链表移除
│   │   │       │   │   ├── 如果是电平触发且仍然就绪，保留在链表
│   │   │       │   │   └── copy_to_user() 拷贝事件到用户态
│   │   │       │   └── list_splice(&txlist, &ep->rdllist) // 重新链接剩余事件
│   │   │       └── 返回事件数量
│   │   ├── if (res || (timed_out = ep_timeout_expired(timeout))) break; // 有事件或超时退出
│   │   ├── if (signal_pending(current)) { res = -EINTR; break; } // 信号中断
│   │   ├── spin_unlock_irqsave(&ep->wq.lock)
│   │   ├── 🔥 进程调度等待:
│   │   │   └── if (!schedule_hrtimeout_range(to, slack, HRTIMER_MODE_ABS))
│   │   │       └── timed_out = 1           // 超时标记
│   │   └── spin_lock_irqsave(&ep->wq.lock)
│   ├── __remove_wait_queue(&ep->wq, &wait)  // 从等待队列移除
│   ├── __set_current_state(TASK_RUNNING)    // 恢复运行状态
│   └── spin_unlock_irqsave(&ep->wq.lock)
└── 返回就绪事件数量
```

### 2.4 🔥🔥🔥 核心：事件回调机制

```c
// 当被监听的文件描述符状态变化时 (如网络数据到达)
// 内核会调用之前注册的回调函数

static int ep_poll_callback(wait_queue_entry_t *wait, unsigned mode, int sync, void *key)
{
    struct epitem *epi = ep_item_from_wait(wait); // 获取对应的 epitem
    struct eventpoll *ep = epi->ep;               // 获取 eventpoll 实例
    __poll_t pollflags = key_to_poll(key);        // 获取事件掩码
    unsigned long flags;
    int ewake = 0;

    ├── spin_lock_irqsave(&ep->wq.lock, flags)   // 获取锁
    ├── ep_set_busy_poll_napi_id(epi)            // NAPI 优化
    ├── 🔥 检查事件匹配:
    │   └── if (pollflags && !(pollflags & epi->event.events))
    │       └── goto out_unlock;                 // 事件不匹配，退出
    ├── 🔥 添加到就绪链表:
    │   ├── if (!ep_is_linked(epi))              // 检查是否已在就绪链表
    │   │   └── list_add_tail(&epi->rdllink, &ep->rdllist) // 添加到就绪链表尾部  
    │   └── 处理溢出情况 (ovflist)
    ├── 🔥 唤醒等待的进程:
    │   ├── if (waitqueue_active(&ep->wq))       // 检查是否有等待的进程
    │   │   ├── wake_up_locked(&ep->wq)          // 唤醒 epoll_wait 中的进程
    │   │   └── ewake = 1                        // 标记已唤醒
    │   └── if (waitqueue_active(&ep->poll_wait)) // 如果 epoll fd 也被其他 epoll 监听
    │       └── pwake++                          // 级联唤醒
    ├── spin_unlock_irqrestore(&ep->wq.lock, flags)
    └── if (pwake) ep_poll_safewake(&ep->poll_wait) // 安全唤醒

    return ewake;                                // 返回是否唤醒了进程
}
```

### 2.5 EPOLLET 边缘触发 vs EPOLLIN 水平触发

```c
// 水平触发 (Level Triggered, 默认)
ev.events = EPOLLIN;
// 行为: 只要接收队列有数据，每次 epoll_wait 都返回 EPOLLIN
// 优点: 不会遗漏事件，编程简单
// 缺点: 可能重复通知，增加系统调用

// 边缘触发 (Edge Triggered)
ev.events = EPOLLIN | EPOLLET;
// 行为: 只在状态 无数据→有数据 变化时通知一次
// 要求: 必须配合非阻塞 I/O，一次性读完所有数据
// 优点: 减少 epoll_wait 返回次数
// 缺点: 编程复杂，读不完会丢失后续通知

// 边缘触发正确用法:
set_nonblocking(sockfd);
while (1) {
    ssize_t n = recv(sockfd, buf, sizeof(buf), 0);
    if (n <= 0) {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;  // 读完了
        // 连接关闭或错误
        break;
    }
    process(buf, n);
}
```

**内核实现差异** (ep_send_events_proc):
- 水平触发: 事件处理后若仍就绪，epitem 保留在 rdllist
- 边缘触发: 事件处理后 epitem 从 rdllist 移除，直到下次 ep_poll_callback

### 2.6 关键内核子系统协作

1. **VFS 层**: 提供统一的 poll 接口
2. **网络子系统**: 数据到达/发送空间可用时触发回调
3. **进程调度器**: 管理等待/唤醒
4. **内存管理**: 高效的数据结构 (红黑树, 链表)
5. **中断处理**: 从硬件中断到软中断到回调

---

## 🧱 第三部分：核心数据结构

### 3.1 struct eventpoll - epoll 实例核心

```c
struct eventpoll {
    /* 进程同步 */
    wait_queue_head_t wq;           // 🔥 用户进程等待队列 (epoll_wait 阻塞在此)
    wait_queue_head_t poll_wait;    // 如果 epoll fd 被其他 epoll 监听时使用
    
    /* 就绪事件管理 */  
    struct list_head rdllist;       // 🔥 就绪事件链表 (双向循环链表)
    struct rb_root_cached rbr;      // 🔥 红黑树根 (管理所有 epitem，按 fd 排序)
    
    /* 溢出处理 */
    struct epitem *ovflist;         // 溢出链表 (当回调中再次触发回调时使用)
    
    /* 同步和保护 */
    struct mutex mtx;               // 互斥锁 (保护 epoll 操作)
    
    /* 统计信息 */
    u32 gen;                        // 生成号 (用于检测修改)
    
    /* 用户相关 */
    struct user_struct *user;       // 用户结构 (用于资源限制)
    
    /* 文件相关 */
    struct file *file;              // 对应的 file 结构
    
    /* 性能优化 */  
    int visited;                    // 防止循环依赖标记
    struct list_head visited_list_link; // 访问列表链接

#ifdef CONFIG_NET_RX_BUSY_POLL
    /* NAPI 繁忙轮询优化 */
    unsigned int napi_id;           // NAPI 实例 ID
#endif

#ifdef CONFIG_DEBUG_LOCK_ALLOC
    /* 调试信息 */  
    struct lockdep_map dep_map;     // lockdep 依赖图
#endif
};
```

**作用**: epoll 实例的核心管理结构  
**生命周期**: epoll_create 创建 → close(epfd) 销毁  
**关键字段**:
- `wq`: epoll_wait 中的进程在此等待
- `rdllist`: 存储所有就绪的事件
- `rbr`: 红黑树管理所有监听的 fd，支持 O(log n) 查找

### 3.2 struct epitem - 监听项

```c  
struct epitem {
    union {
        /* RB-tree node links */
        struct rb_node rbn;         // 🔥 红黑树节点 (在 eventpoll.rbr 中)
        /* List header used to link... */
        struct list_head fllink;    // 释放链表 (销毁时使用)
    };

    /* List header used to link to the "struct file" items list */
    struct list_head fllink;        // 链接到 file->f_ep_links (反向链接)

    /* List header used to link to the eventpoll "ready" list */
    struct list_head rdllink;       // 🔥 就绪链表节点 (在 eventpoll.rdllist 中)

    /* The file descriptor information this item refers to */
    struct epoll_filefd ffd;        // 🔥 文件描述符信息
    /*
    struct epoll_filefd {
        struct file *file;          // 指向被监听的 file
        int fd;                     // 文件描述符编号
    };
    */

    /* Number of active wait queue attached to poll operations */  
    int nwait;                      // 活跃等待队列数量

    /* List containing poll wait queues */
    struct list_head pwqlist;       // poll 等待队列链表

    /* The "container" of this item */  
    struct eventpoll *ep;           // 🔥 反向指针，指向所属的 eventpoll

    /* List header used to link this item to the "struct file" items list */
    struct list_head fllink;        // 文件链表 (同一个文件的多个 epoll 监听)

    /* wakeup_source used when EPOLLWAKEUP is set */
    struct wakeup_source __rcu *ws; // 唤醒源 (防止系统休眠)

    /* The structure that describe the interested events and the source fd */
    struct epoll_event event;       // 🔥 用户关注的事件 (EPOLLIN, EPOLLOUT, EPOLLET...)
    /*
    struct epoll_event {
        __poll_t events;            // 事件掩码 (EPOLLIN, EPOLLOUT, EPOLLERR...)
        __u64 data;                 // 用户数据 (通常存储 fd 或指针)
    };
    */
};
```

**作用**: 表示一个被监听的文件描述符  
**生命周期**: epoll_ctl(ADD) 创建 → epoll_ctl(DEL) 销毁  
**关键字段**:
- `rbn`: 在红黑树中的位置，支持快速查找
- `rdllink`: 事件就绪时链接到就绪链表
- `ffd`: 被监听的文件信息
- `event`: 用户关注的事件类型

### 3.3 struct eppoll_entry - 等待队列项

```c
struct eppoll_entry {
    /* List header used to link this structure to the "struct epitem" */
    struct list_head llink;         // 链接到 epitem.pwqlist

    /* The "base" pointer during wakeup */
    struct epitem *base;            // 指向对应的 epitem

    /* 
     * Wait queue item that will be linked to the target file wait
     * queue head.
     */  
    wait_queue_entry_t wait;        // 🔥 等待队列项 (注册到文件的等待队列)
    /*
    wait_queue_entry_t {
        unsigned int flags;         // 等待标志
        void *private;              // 私有数据 (通常指向 task_struct)  
        wait_queue_func_t func;     // 🔥🔥🔥 回调函数指针 (ep_poll_callback)
        struct list_head entry;     // 等待队列链表节点
    };
    */

    /* The wait queue head that linked the "wait" wait queue item */
    wait_queue_head_t *whead;       // 指向文件的等待队列头
};
```

**作用**: 连接 epoll 和文件等待队列的桥梁  
**关键机制**: 当文件状态变化时，`wait.func` 被调用，即 `ep_poll_callback`

---

## ⚙️ 第四部分：最小可运行实验

### 4.1 基础 epoll 服务器

```c
// demo_epoll_server.c  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_EVENTS 1024
#define LISTEN_PORT 8888
#define BUFFER_SIZE 4096

// 设置非阻塞
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");  
        return -1;
    }
    return 0;
}

int main() {
    int listen_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    struct epoll_event ev, events[MAX_EVENTS];
    socklen_t client_len = sizeof(client_addr);
    
    printf("=== epoll 高性能服务器示例 ===\n");

    // 1. 创建监听 socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        exit(1);
    }

    // 设置 SO_REUSEADDR
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(LISTEN_PORT);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(listen_fd);
        exit(1);
    }

    // 开始监听
    if (listen(listen_fd, SOMAXCONN) == -1) {
        perror("listen");
        close(listen_fd);
        exit(1);
    }

    // 设置非阻塞
    set_nonblocking(listen_fd);

    printf("服务器监听在端口 %d\n", LISTEN_PORT);
    printf("进程 PID: %d\n", getpid());

    // 2. 创建 epoll 实例
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(listen_fd);
        exit(1);
    }
    
    printf("epoll fd: %d\n", epoll_fd);

    // 3. 将监听 socket 添加到 epoll
    ev.events = EPOLLIN;  // 监听可读事件 (新连接)
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl: listen_fd");
        close(listen_fd);
        close(epoll_fd);
        exit(1);
    }

    printf("已添加监听 socket 到 epoll，等待连接...\n");
    printf("可使用 'telnet localhost %d' 测试\n\n", LISTEN_PORT);

    // 4. 主事件循环
    int event_count = 0;
    while (1) {
        // 🔥 epoll_wait - 等待事件就绪
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); // 无限等待
        
        if (nfds == -1) {
            if (errno == EINTR) continue;  // 信号中断，继续
            perror("epoll_wait");
            break;
        }

        printf("epoll_wait 返回 %d 个就绪事件\n", nfds);

        // 处理每个就绪事件
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            uint32_t event_mask = events[i].events;
            
            printf("  事件 %d: fd=%d, events=0x%x", i, fd, event_mask);

            if (fd == listen_fd) {
                // 🔥 监听 socket 可读 = 有新连接
                printf(" (新连接)\n");
                
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept");
                    }
                    continue;
                }

                printf("    接受连接: fd=%d, IP=%s:%d\n", 
                       client_fd, 
                       inet_ntoa(client_addr.sin_addr), 
                       ntohs(client_addr.sin_port));

                // 设置客户端 socket 为非阻塞
                set_nonblocking(client_fd);

                // 将客户端 socket 添加到 epoll (边缘触发模式)
                ev.events = EPOLLIN | EPOLLET;  // 可读 + 边缘触发
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    perror("epoll_ctl: client_fd");
                    close(client_fd);
                    continue;
                }

                printf("    已添加客户端 fd=%d 到 epoll\n", client_fd);

            } else {
                // 🔥 客户端 socket 事件
                if (event_mask & EPOLLIN) {
                    printf(" (可读)\n");
                    
                    char buffer[BUFFER_SIZE];
                    ssize_t bytes_read;
                    
                    // 边缘触发模式需要一次性读完所有数据
                    while ((bytes_read = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
                        buffer[bytes_read] = '\0';
                        printf("    收到数据 (%ld bytes): %s", bytes_read, buffer);
                        
                        // 回显数据
                        send(fd, buffer, bytes_read, 0);
                    }
                    
                    if (bytes_read == 0) {
                        // 客户端关闭连接
                        printf("    客户端 fd=%d 关闭连接\n", fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                    } else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("recv");
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                    }
                }
                
                if (event_mask & (EPOLLHUP | EPOLLERR)) {
                    printf(" (连接异常)\n");
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
        }
        
        event_count++;
        printf("完成第 %d 轮事件处理\n\n", event_count);
    }

    close(epoll_fd);
    close(listen_fd);
    return 0;
}
```

### 4.2 epoll 事件类型测试

```c
// demo_epoll_events.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <signal.h>

void test_eventfd_epoll() {
    printf("\n=== 测试 eventfd + epoll ===\n");
    
    // 创建 eventfd
    int efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (efd == -1) {
        perror("eventfd");
        return;
    }
    
    // 创建 epoll
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event ev, events[10];
    
    // 添加 eventfd 到 epoll
    ev.events = EPOLLIN;
    ev.data.fd = efd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, efd, &ev);
    
    printf("eventfd: %d, epoll fd: %d\n", efd, epfd);
    
    // 子进程写入 eventfd
    if (fork() == 0) {
        sleep(1);
        uint64_t value = 42;
        write(efd, &value, sizeof(value));
        printf("子进程写入 eventfd: %lu\n", value);
        exit(0);
    }
    
    // 父进程等待事件
    printf("等待 eventfd 事件...\n");
    int nfds = epoll_wait(epfd, events, 10, 3000); // 3秒超时
    
    if (nfds > 0) {
        printf("epoll_wait 返回 %d 个事件\n", nfds);
        
        uint64_t value;
        read(efd, &value, sizeof(value));
        printf("从 eventfd 读取: %lu\n", value);
    } else {
        printf("epoll_wait 超时或无事件\n");
    }
    
    close(efd);
    close(epfd);
}

void test_timerfd_epoll() {
    printf("\n=== 测试 timerfd + epoll ===\n");
    
    // 创建定时器 fd
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (tfd == -1) {
        perror("timerfd_create");
        return;
    }
    
    // 设置定时器 (2秒后触发，然后每1秒重复)
    struct itimerspec timer_spec;
    timer_spec.it_value.tv_sec = 2;    // 初始延迟
    timer_spec.it_value.tv_nsec = 0;
    timer_spec.it_interval.tv_sec = 1; // 重复间隔
    timer_spec.it_interval.tv_nsec = 0;
    
    timerfd_settime(tfd, 0, &timer_spec, NULL);
    
    // 创建 epoll
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event ev, events[10];
    
    // 添加 timerfd 到 epoll
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev);
    
    printf("timerfd: %d, 等待定时器事件...\n", tfd);
    
    // 等待定时器事件 (最多等 5 次)
    for (int i = 0; i < 5; i++) {
        int nfds = epoll_wait(epfd, events, 10, 5000);
        
        if (nfds > 0) {
            uint64_t timer_count;
            read(tfd, &timer_count, sizeof(timer_count));
            printf("定时器触发第 %d 次，计数: %lu\n", i + 1, timer_count);
        } else {
            printf("等待超时\n");
            break;
        }
    }
    
    close(tfd);
    close(epfd);
}

int main() {
    printf("=== epoll 各种事件类型测试 ===\n");
    
    test_eventfd_epoll();
    test_timerfd_epoll();
    
    return 0;
}
```

### 4.3 编译和运行

```bash
# 编译服务器
gcc -o demo_epoll_server demo_epoll_server.c

# 编译事件测试
gcc -o demo_epoll_events demo_epoll_events.c

# 运行服务器 (在一个终端)
./demo_epoll_server

# 在另一个终端测试连接
telnet localhost 8888
# 或
echo "Hello epoll" | nc localhost 8888

# 运行事件测试
./demo_epoll_events
```

### 4.4 触发的内核行为

这些测试会触发：

1. **sys_epoll_create1()** - 创建 epoll 实例
2. **sys_epoll_ctl()** - 添加/删除监听的 fd
3. **sys_epoll_wait()** - 等待事件就绪  
4. **ep_poll_callback()** - 网络数据到达时的回调
5. **进程调度** - epoll_wait 中的睡眠和唤醒
6. **网络协议栈** - TCP 连接建立和数据处理

---

## 🔍 第五部分：可观测性 & Debug 方法

### 5.1 使用 strace 观察 epoll 系统调用

```bash
# 跟踪 epoll 相关调用
strace -e trace=epoll_create1,epoll_ctl,epoll_wait ./demo_epoll_server

# 详细显示系统调用参数
strace -e trace=epoll_create1,epoll_ctl,epoll_wait -v ./demo_epoll_server

# 跟踪网络相关调用
strace -e trace=network,epoll_create1,epoll_ctl,epoll_wait ./demo_epoll_server
```

**期望输出**：
```
epoll_create1(EPOLL_CLOEXEC) = 4
epoll_ctl(4, EPOLL_CTL_ADD, 3, {EPOLLIN, {u32=3, u64=3}}) = 0
epoll_wait(4, [], 1024, -1) = 1
epoll_ctl(4, EPOLL_CTL_ADD, 5, {EPOLLIN|EPOLLET, {u32=5, u64=5}}) = 0
```

### 5.2 观察 epoll 内核统计

```bash
# 查看 epoll 相关的内存使用
cat /proc/slabinfo | grep -E "(eventpoll|epitem)"

# 查看进程的文件描述符
ls -la /proc/$(pgrep demo_epoll)/fd/

# 查看进程的 epoll 信息 (需要较新内核)
cat /proc/$(pgrep demo_epoll)/fdinfo/4  # 4 是 epoll fd
```

### 5.3 使用 perf 观察性能

```bash
# 记录 epoll 相关事件
sudo perf record -e syscalls:sys_enter_epoll_wait,syscalls:sys_exit_epoll_wait \
    ./demo_epoll_server

# 统计系统调用频率
sudo perf stat -e syscalls:sys_enter_epoll_wait,syscalls:sys_enter_epoll_ctl \
    ./demo_epoll_server

# 查看调用栈
sudo perf script
```

### 5.4 使用 ftrace 跟踪内核函数

```bash
# 跟踪 epoll 核心函数
echo function_graph > /sys/kernel/debug/tracing/current_tracer
echo 'ep_poll ep_poll_callback ep_send_events' > \
    /sys/kernel/debug/tracing/set_ftrace_filter
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 运行测试
./demo_epoll_events

# 查看调用图
cat /sys/kernel/debug/tracing/trace

# 清理
echo 0 > /sys/kernel/debug/tracing/tracing_on
```

### 5.5 网络相关调试

```bash
# 查看网络连接状态
netstat -antlp | grep :8888

# 查看 socket 统计
ss -antlp | grep :8888

# 监控网络中断
cat /proc/interrupts | grep eth

# 查看软中断统计
cat /proc/softirqs
```

### 5.6 使用专门工具观察 epoll

```bash
# 使用 lsof 查看 epoll 监听的文件
lsof -p $(pgrep demo_epoll) | grep -E "(epoll|socket)"

# 使用 gdb 调试
gdb ./demo_epoll_server
(gdb) b epoll_wait
(gdb) run
(gdb) bt  # 查看调用栈当程序停在 epoll_wait
```

---

## ⚡ 第六部分：性能与设计权衡

### 6.1 性能瓶颈分析

1. **系统调用开销**
   - epoll_wait 仍然是系统调用
   - 每次调用约 100-300 个 CPU 周期
   - 批量处理事件可以摊销开销

2. **内存访问模式**
   - 红黑树查找: O(log n)，但缓存友好度一般
   - 链表遍历: O(1)，但可能有缓存缺失
   - 大量活跃连接时内存访问分散

3. **锁竞争**
   - eventpoll.mtx 保护操作
   - 高并发时可能成为瓶颈
   - 读多写少的场景下 RCU 优化

### 6.2 epoll 设计权衡

**epoll vs select/poll**

| 特性 | epoll | select | poll |
|------|-------|--------|------|
| **时间复杂度** | O(1) | O(n) | O(n) |
| **fd 数量限制** | 无硬编码限制 | 1024 (FD_SETSIZE) | 无硬编码限制 |
| **内核数据结构** | 红黑树 + 链表 | fd_set 位图 | pollfd 数组 |
| **事件通知** | 回调驱动 | 轮询检查 | 轮询检查 |
| **内存使用** | 较高 (红黑树节点) | 固定大小 | 与 fd 数成比例 |

**边缘触发 vs 水平触发**

```c
// 水平触发 (Level Triggered) - 默认模式
ev.events = EPOLLIN;

// 边缘触发 (Edge Triggered) - 高性能模式  
ev.events = EPOLLIN | EPOLLET;
```

| 模式 | 触发条件 | 适用场景 | 编程难度 |
|------|----------|----------|----------|
| **水平触发** | 条件满足就触发 | 简单应用，兼容性 | 简单 |
| **边缘触发** | 状态变化时触发 | 高性能服务器 | 复杂 |

### 6.3 优化策略

1. **批量处理**
```c
struct epoll_event events[MAX_BATCH];
int nfds = epoll_wait(epfd, events, MAX_BATCH, timeout);

// 批量处理所有就绪事件
for (int i = 0; i < nfds; i++) {
    process_event(&events[i]);
}
```

2. **使用边缘触发**
```c
// 边缘触发 + 非阻塞 I/O
ev.events = EPOLLIN | EPOLLET;
set_nonblocking(sockfd);

// 必须一次性读完所有数据
while ((n = read(sockfd, buffer, sizeof(buffer))) > 0) {
    process_data(buffer, n);
}
```

3. **EPOLLONESHOT 避免竞争**
```c
// 一次性事件，避免多线程竞争
ev.events = EPOLLIN | EPOLLONESHOT;
```

4. **零拷贝优化**
```c
// 结合 splice/sendfile 实现零拷贝
splice(in_fd, NULL, pipe_fd[1], NULL, len, SPLICE_F_MOVE);
splice(pipe_fd[0], NULL, out_fd, NULL, len, SPLICE_F_MOVE);
```

---

## 🔗 第七部分：横向对比

### 7.1 不同 I/O 多路复用机制

**Linux epoll vs FreeBSD kqueue**

```c
// Linux epoll
int epfd = epoll_create1(0);
epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
epoll_wait(epfd, events, maxevents, timeout);

// FreeBSD kqueue  
int kq = kqueue();
struct kevent kev;
EV_SET(&kev, sockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
kevent(kq, &kev, 1, NULL, 0, NULL);
kevent(kq, NULL, 0, events, maxevents, timeout);
```

**epoll vs io_uring (新一代)**
- **epoll**: 基于回调的事件通知
- **io_uring**: 无锁环形缓冲区，更低开销

### 7.2 用户态 vs 内核态解决方案

**内核态 epoll vs 用户态库 (如 libuv)**

```c
// 直接使用 epoll (内核态)
epoll_wait(epfd, events, maxevents, timeout);

// 使用 libuv (用户态封装)  
uv_run(loop, UV_RUN_DEFAULT);
```

**优势对比**:
- **内核态**: 更底层，性能更高，灵活性更大
- **用户态**: 跨平台，API 更友好，开发效率高

### 7.3 同步 vs 异步 I/O

```c
// 同步 I/O + epoll (反应器模式)
while (1) {
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    for (int i = 0; i < nfds; i++) {
        if (events[i].events & EPOLLIN) {
            read(events[i].data.fd, buffer, sizeof(buffer)); // 同步读取
        }
    }
}

// 异步 I/O + epoll (前摄器模式)  
struct aiocb aio_req;
aio_read(&aio_req);  // 异步读取
// ... epoll 监听 AIO 完成事件
```

---

## 🧠 第八部分：一句话本质总结

> **epoll 的本质是通过红黑树管理监听的文件描述符，利用回调机制将文件状态变化转换为就绪事件，实现 O(1) 复杂度的高效 I/O 多路复用。**

---

## 📌 下一步学习

掌握了 epoll 机制后，建议继续学习：
1. **[TCP 收包流程](05-tcp-recv.md)** - 网络数据如何触发 epoll 事件
2. **[TCP 发包流程](06-tcp-send.md)** - 理解网络 I/O 的完整路径

---

## 🔖 关键要点回顾

- ✅ epoll 通过红黑树 + 就绪链表实现高效管理
- ✅ 回调机制是 epoll 高性能的核心
- ✅ 边缘触发模式可以进一步提升性能
- ✅ epoll 解决了 select/poll 的 O(n) 复杂度问题  
- ✅ 理解了从文件状态变化到用户进程唤醒的完整路径