# Linux I/O 多路复用：select、poll、epoll 详解

## 概述

**类比**：想象你是一个餐厅经理，需要同时处理多个服务员的需求（读取多个文件描述符）。你有三种管理方式：

- **select**：每隔几分钟巡视一遍所有服务员，看谁需要帮助
- **poll**：改进版的巡视方式，记录更详细的信息
- **epoll**：让服务员有事主动来找你（事件驱动）

## I/O 多路复用的本质

所有 I/O 多路复用机制的本质都是：

```
[User Space] ---> [Kernel: wait for events] ---> [Return ready fds]
```

**区别只在两点**：
1. **如何告诉内核要监听哪些文件描述符**
2. **内核如何返回就绪的文件描述符**

---

## 1. select：最原始的方式

### 数据结构
使用 **bitmap（fd_set）** 来表示文件描述符集合

### 工作流程

```
User Space
───────────────
fd_set (bitmap)
+----------------------+
| 0 1 0 1 0 0 1 ...    |  <- 1表示关注此fd，0表示不关注
+----------------------+
       |
       | copy_to_kernel (每次调用都要拷贝)
       v

Kernel Space
────────────────────────────
       |
       | 遍历所有 fd（O(n)）
       v
+----------------------------+
| fd0 -> ready? ✓            |
| fd1 -> ready? ✗            |
| fd2 -> ready? ✗            |
| fd3 -> ready? ✓            |
| ...                        |
+----------------------------+
       |
       | copy_to_user（整个 bitmap）
       v

User Space
───────────────
再扫描一遍 bitmap（O(n)）
找到置位的 fd
```

### 代码示例

```c
#include <sys/select.h>
#include <sys/time.h>

int main() {
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);          // 清空集合
    FD_SET(0, &readfds);        // 添加标准输入
    FD_SET(sockfd, &readfds);   // 添加socket
    
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
    
    if (ready > 0) {
        if (FD_ISSET(0, &readfds)) {
            // 标准输入有数据
        }
        if (FD_ISSET(sockfd, &readfds)) {
            // socket有数据
        }
    }
    return 0;
}
```

### 核心问题

❌ **每次调用都要做的重复工作**：
- 拷贝整个 fd_set 到内核
- 内核 O(n) 扫描所有 fd
- 用户再 O(n) 扫描 bitmap
- fd 数量受限（通常 1024）

**一句话总结**：每次都"全量扫描 + 全量拷贝"

---

## 2. poll：改进版，但本质未变

### 数据结构
使用 **数组（struct pollfd[]）**，信息更丰富

### 工作流程

```
User Space
───────────────
pollfd array
+------------------------+
| fd=3  events=READ      |
| fd=4  events=WRITE     |
| fd=5  events=READ      |
+------------------------+
       |
       | copy_to_kernel
       v

Kernel Space
────────────────────────────
       |
       | 遍历数组（O(n)）
       v
+----------------------------+
| check fd=3 -> revents=READ |
| check fd=4 -> revents=0    |
| check fd=5 -> revents=READ |
+----------------------------+
       |
       | 写回 revents 字段
       v

User Space
───────────────
遍历数组找 revents!=0 的fd（O(n)）
```

### 代码示例

```c
#include <poll.h>

int main() {
    struct pollfd fds[3];
    
    fds[0].fd = 0;              // 标准输入
    fds[0].events = POLLIN;     // 关注读事件
    
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLOUT;  // 关注读写事件
    
    fds[2].fd = sockfd2;
    fds[2].events = POLLIN;
    
    int ready = poll(fds, 3, 5000);  // 5秒超时
    
    if (ready > 0) {
        for (int i = 0; i < 3; i++) {
            if (fds[i].revents & POLLIN) {
                // fd有数据可读
            }
            if (fds[i].revents & POLLOUT) {
                // fd可写
            }
        }
    }
    return 0;
}
```

### 相比 select 的改进

✅ **改进点**：
- 没有 bitmap 限制（fd 可以很大）
- 表达能力更强（事件类型更细）
- 不会修改原始的 events 字段

❌ **核心问题没变**：还是每次调用 → 全量遍历 + 全量拷贝

---

## 3. epoll：本质优化

### 关键思想
**把"关注的 fd" 和 "就绪事件"分离**

### 内核数据结构

```
+----------------------+
| epoll instance       |
|----------------------|
| 红黑树 (interest set) |  <- 存储关注的fd
| 就绪链表 (ready list) |  <- 存储就绪的fd
+----------------------+
```

### 工作流程

#### Step 1：注册 fd（一次性操作）

```
User Space
───────────────
epoll_ctl(ADD, fd)

       |
       v

Kernel
────────────────────────────
红黑树（interest set）
+----------------------+
| fd=3 -> callback     |
| fd=4 -> callback     |
| fd=5 -> callback     |
+----------------------+
```

**关键点**：fd 只注册一次，不再重复传输

#### Step 2：事件发生（核心优化🔥）

```
[网卡中断 / 数据到达]
         |
         v
Kernel
────────────────────────────
找到对应 fd
       |
       v
触发回调，加入 ready list

+----------------------+
| ready list           |
|----------------------|
| fd=5                 |
| fd=3                 |
+----------------------+
```

**关键点**：不是扫描，而是"事件驱动回调"

#### Step 3：获取结果

```
User Space
───────────────
epoll_wait()
       |
       v

Kernel
────────────────────────────
直接返回 ready list
       |
       v

User Space
───────────────
直接拿到 ready fd（O(ready)）
```

### 代码示例

```c
#include <sys/epoll.h>

int main() {
    // 创建epoll实例
    int epfd = epoll_create1(0);
    
    struct epoll_event ev, events[MAX_EVENTS];
    
    // 添加文件描述符
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
    
    // 等待事件
    while (1) {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].events & EPOLLIN) {
                int fd = events[i].data.fd;
                // 处理可读事件
            }
        }
    }
    
    close(epfd);
    return 0;
}
```

### epoll 的两种触发模式

#### 水平触发（Level Triggered, LT）- 默认模式
```c
// 只要fd可读，就会持续通知
epoll_wait() 返回 -> fd可读
如果不读完数据 -> 下次 epoll_wait() 还会返回该fd
```

#### 边缘触发（Edge Triggered, ET）- 高效模式
```c
// 只在fd状态改变时通知一次
ev.events = EPOLLIN | EPOLLET;  // 设置ET模式
// fd可读时只通知一次，必须一次性读完所有数据
```

### 关键优化总结

✅ **三大优化**：

1. **不再全量扫描**
   - select/poll：O(n) - 每次检查所有fd
   - epoll：O(ready) - 只处理就绪的fd

2. **不再重复拷贝 fd**
   - fd 只需注册一次（epoll_ctl）
   - 不用每次传递fd列表

3. **事件驱动（callback）**
   - 传统：你问内核"有事件吗？"（轮询）
   - epoll：内核告诉你"有事件了！"（通知）

---

## 综合对比

| 特性 | select | poll | epoll |
|------|--------|------|-------|
| **数据结构** | bitmap (fd_set) | array (pollfd[]) | 红黑树 + 链表 |
| **fd数量限制** | 1024（可调） | 无限制 | 无限制 |
| **fd管理方式** | 每次传入 | 每次传入 | 一次注册 |
| **内核行为** | 遍历所有fd | 遍历所有fd | 事件触发回调 |
| **时间复杂度** | O(n) | O(n) | O(ready) |
| **拷贝开销** | 每次拷贝bitmap | 每次拷贝数组 | 几乎没有 |
| **触发模式** | 轮询 | 轮询 | 事件驱动 |
| **跨平台** | ✅ | ✅ | ❌ (Linux专有) |

## 性能对比

```
连接数量        select/poll    epoll
─────────────────────────────────
100个           相当           相当
1,000个         慢             快
10,000个        很慢           快
100,000个       不可用         很快
```

## 最佳实践

### 什么时候用 select？
- **连接数少**（< 1000）且需要**跨平台**
- 简单的应用场景
- 需要精确的超时控制
- 兼容性要求高的系统

### 什么时候用 poll？
- 需要**跨平台**但 select 的限制太严格
- fd 数量可能超过 1024
- 需要更精细的事件控制
- 不想受 fd_set 大小限制

### 什么时候用 epoll？
- **高并发**场景（> 1000 连接）
- **Linux 专有**应用
- 长连接场景（如聊天服务器）
- 服务器应用
- 追求极致性能

## 常见错误和注意事项

### select 常见问题
```c
// ❌ 错误：忘记重新设置fd_set
fd_set readfds;
while (1) {
    // select会修改readfds，需要每次重新设置
    select(maxfd + 1, &readfds, NULL, NULL, NULL);
}

// ✅ 正确：使用备份
fd_set readfds, backup;
FD_SET(sockfd, &backup);
while (1) {
    readfds = backup;  // 恢复原始状态
    select(maxfd + 1, &readfds, NULL, NULL, NULL);
}
```

### epoll 常见问题
```c
// ❌ 错误：ET模式下没有读完所有数据
if (events[i].events & EPOLLIN) {
    read(fd, buf, sizeof(buf));  // 只读一次，可能有数据残留
}

// ✅ 正确：ET模式下循环读取直到EAGAIN
while (1) {
    int n = read(fd, buf, sizeof(buf));
    if (n == -1 && errno == EAGAIN) break;  // 没有更多数据
    if (n <= 0) break;  // 连接关闭或出错
    // 处理数据...
}
```

## 类比总结

**运维监控 10 万台服务器**：

### select/poll 方式
```
每秒你要做的事：
1. 逐台 ping 10 万机器  <- O(n) 扫描
2. 记录哪台机器有问题   <- 全量检查
3. 处理有问题的机器     <- 找出就绪的

👉 累死 CPU，效率低下
```

### epoll 方式
```
一次性设置：
1. 每台机器配置监控代理  <- epoll_ctl 注册

运行时：
2. 机器出问题主动报警    <- 事件驱动回调
3. 你只处理报警的机器    <- 只处理就绪的

👉 这就是 epoll 的本质：变轮询为通知
```

## 一句话总结

**select/poll**：你不停问内核"有事件吗？"

**epoll**：内核在事件发生时主动通知你

这就是从**轮询模式**到**事件驱动模式**的本质转变！

---

## 扩展：其他平台的类似机制

- **FreeBSD/MacOS**: kqueue
- **Windows**: IOCP (I/O Completion Port)
- **Solaris**: /dev/poll, event ports

虽然 API 不同，但核心思想都是**事件驱动 + 避免轮询扫描**。