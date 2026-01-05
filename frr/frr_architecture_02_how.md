# FRR Architecture Guide - Part 2: HOW | Design Philosophy

## ASCII Architecture Overview

```
+==============================================================================+
|                   HOW FRR SOLVES THE PROBLEM - Design Philosophy             |
+==============================================================================+

                        FRR LAYERED ARCHITECTURE
+-----------------------------------------------------------------------------+
|                                                                             |
|  +-----------------------------------------------------------------------+  |
|  |                     MANAGEMENT PLANE                                  |  |
|  |  +----------+  +-----------+  +-----------+  +-------------------+    |  |
|  |  |  vtysh   |  |  YANG/NB  |  |   gRPC    |  |    SNMP Agent     |    |  |
|  |  | (CLI)    |  |  (Model)  |  |   (API)   |  |                   |    |  |
|  |  +----+-----+  +-----+-----+  +-----+-----+  +---------+---------+    |  |
|  |       |              |              |                  |              |  |
|  +-------|--------------|--------------+------------------|--------------+  |
|          |              |              |                  |                 |
|  +-------|--------------|--------------+------------------|--------------+  |
|  |       v              v              v                  v              |  |
|  |                     CONTROL PLANE                                     |  |
|  |                                                                       |  |
|  |  +--------+  +--------+  +--------+  +--------+  +--------+           |  |
|  |  |  bgpd  |  | ospfd  |  | isisd  |  |  ripd  |  |staticd |           |  |
|  |  +---+----+  +---+----+  +---+----+  +---+----+  +---+----+           |  |
|  |      |           |           |           |           |                |  |
|  |      +-----+-----+-----+-----+-----+-----+-----+-----+                |  |
|  |            |                 |                 |                      |  |
|  |            |     ZAPI        |     ZAPI        |                      |  |
|  |            v    (IPC)        v    (IPC)        v                      |  |
|  |       +----+----+       +----+----+       +----+----+                 |  |
|  |       |         |       |         |       |         |                 |  |
|  |       +---------+-------+---------+-------+---------+                 |  |
|  |                         |                                             |  |
|  |                         v                                             |  |
|  |              +--------------------+                                   |  |
|  |              |       zebra        |                                   |  |
|  |              |  (Route Manager)   |                                   |  |
|  |              |                    |                                   |  |
|  |              |  +------+------+   |                                   |  |
|  |              |  | RIB  | FIB  |   |                                   |  |
|  |              |  +------+------+   |                                   |  |
|  |              +---------+----------+                                   |  |
|  |                        |                                              |  |
|  +------------------------|----------------------------------------------+  |
|                           |                                                 |
|  - - - - - - - - - - - - -|- - - - - - - - - - - - - - - - - - - - - - - -  |
|                           | Netlink / Routing Socket                        |
|                           v                                                 |
|  +-----------------------------------------------------------------------+  |
|  |                      DATA PLANE (Kernel)                              |  |
|  |                                                                       |  |
|  |    +------------------+    +-----------------+    +---------------+   |  |
|  |    | Forwarding Table |    |   Interfaces    |    |   Netfilter   |   |  |
|  |    |      (FIB)       |    |   (eth0, etc)   |    |   (iptables)  |   |  |
|  |    +------------------+    +-----------------+    +---------------+   |  |
|  |                                                                       |  |
|  +-----------------------------------------------------------------------+  |
|                                                                             |
+-----------------------------------------------------------------------------+

                        EVENT-DRIVEN EXECUTION MODEL
+-----------------------------------------------------------------------------+
|                                                                             |
|   +-------------------------------------------------------------------+     |
|   |                     EVENT LOOP (event_fetch)                      |     |
|   +-------------------------------------------------------------------+     |
|                                    |                                        |
|          +-------------------------+-------------------------+              |
|          |                         |                         |              |
|          v                         v                         v              |
|   +-------------+           +-------------+           +-------------+       |
|   | I/O Events  |           |   Timers    |           |   Events    |       |
|   | (poll/epoll)|           | (heap-based)|           | (queue)     |       |
|   +------+------+           +------+------+           +------+------+       |
|          |                         |                         |              |
|          |    READ/WRITE           |    Timer expired        |              |
|          |    fd ready             |                         |              |
|          v                         v                         v              |
|   +-------------------------------------------------------------------+     |
|   |                         READY QUEUE                               |     |
|   +-------------------------------------------------------------------+     |
|                                    |                                        |
|                                    v                                        |
|   +-------------------------------------------------------------------+     |
|   |                     CALLBACK EXECUTION                            |     |
|   |                     thread->func(thread)                          |     |
|   +-------------------------------------------------------------------+     |
|                                    |                                        |
|                                    v                                        |
|   +-------------------------------------------------------------------+     |
|   |              RESCHEDULE / STATE UPDATE / CLEANUP                  |     |
|   +-------------------------------------------------------------------+     |
|                                                                             |
+-----------------------------------------------------------------------------+

                      INTER-DAEMON COMMUNICATION (ZAPI)
+-----------------------------------------------------------------------------+
|                                                                             |
|   Protocol Daemon (e.g., bgpd)              Zebra Daemon                    |
|   ============================              ============                    |
|                                                                             |
|   +----------------------+                  +----------------------+        |
|   | Route Learned        |                  | Client Registration  |        |
|   | (e.g., from peer)    |                  | Table                |        |
|   +----------+-----------+                  +----------+-----------+        |
|              |                                         ^                    |
|              | 1. ZAPI_ROUTE_ADD                       |                    |
|              +---------------------------------------->|                    |
|              |                                         |                    |
|              |                              +----------+-----------+        |
|              |                              | RIB Processing       |        |
|              |                              | - Best path select   |        |
|              |                              | - Nexthop resolve    |        |
|              |                              +----------+-----------+        |
|              |                                         |                    |
|              |                                         | 2. Install to FIB  |
|              |                                         v                    |
|              |                              +----------+-----------+        |
|              |                              | Netlink RTM_NEWROUTE |        |
|              |                              +----------+-----------+        |
|              |                                         |                    |
|              |                                         | 3. Kernel ACK      |
|              |                                         v                    |
|              |   4. ZAPI_ROUTE_NOTIFY_OWNER            |                    |
|              |<----------------------------------------+                    |
|              |                                                              |
|   +----------+-----------+                                                  |
|   | Update Protocol      |                                                  |
|   | State (advertise)    |                                                  |
|   +----------------------+                                                  |
|                                                                             |
+-----------------------------------------------------------------------------+

                         CONCURRENCY MODEL
+-----------------------------------------------------------------------------+
|                                                                             |
|   FRR Threading Philosophy:                                                 |
|   ==========================                                                |
|                                                                             |
|   +-------------------+    NOT    +-------------------+                     |
|   |  Heavy            |    ===>   |  Event-Driven     |                     |
|   |  Multi-Threading  |           |  Single Main Loop |                     |
|   +-------------------+           +-------------------+                     |
|           |                               |                                 |
|           v                               v                                 |
|   - Complex locking               - Predictable execution                   |
|   - Race conditions               - Easier debugging                        |
|   - Hard to debug                 - Explicit state machines                 |
|   - Non-deterministic             - Cooperative scheduling                  |
|                                                                             |
|   +-------------------------------------------------------------------+     |
|   |              Main Thread (event_fetch loop)                       |     |
|   |                                                                   |     |
|   |   while (event_fetch(master, &event)) {                           |     |
|   |       event_call(&event);  // Execute callback                    |     |
|   |   }                                                               |     |
|   +-------------------------------------------------------------------+     |
|                              |                                              |
|                              | Auxiliary threads (limited use)              |
|                              v                                              |
|   +-------------------------------------------------------------------+     |
|   |  - I/O intensive operations (e.g., RPKI validation)               |     |
|   |  - Background maintenance tasks                                   |     |
|   |  - Always communicate back via event queue                        |     |
|   +-------------------------------------------------------------------+     |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细说明

### 1. FRR 的根本设计哲学

#### 控制平面与数据平面分离

```
核心原则：
+==================================================================+
|  控制平面 (FRR)           |  数据平面 (内核)                      |
+==================================================================+
|  - 运行路由协议           |  - 执行包转发                         |
|  - 计算最佳路径           |  - 维护 FIB 表                        |
|  - 做策略决策             |  - 处理 ARP/ND                        |
|  - 管理邻居关系           |  - 接口管理                           |
|  - 可以重启而不丢包       |  - 必须持续运行                       |
+==================================================================+
```

**为什么这种分离是关键的：**
1. **故障隔离**：控制平面崩溃时，数据平面继续转发
2. **独立演进**：可以单独升级路由软件
3. **性能优化**：数据平面在内核/硬件中优化
4. **调试便利**：控制平面问题不影响基本转发

#### 长期运行的容错守护进程

从 `event.c` 可以看到 FRR 的守护进程设计：

```c
/* event.c: 531-613 - event_master_create() */
struct event_loop *event_master_create(const char *name)
{
    struct event_loop *rv;
    
    pthread_once(&init_once, &initializer);  // 线程安全初始化
    
    rv = XCALLOC(MTYPE_EVENT_MASTER, sizeof(struct event_loop));
    
    /* 初始化互斥锁和条件变量 */
    pthread_mutex_init(&rv->mtx, NULL);
    pthread_cond_init(&rv->cancel_cond, NULL);
    
    /* 设置事件循环名称 */
    rv->name = XSTRDUP(MTYPE_EVENT_MASTER, name);
    
    /* 初始化各种事件队列 */
    event_list_init(&rv->event);
    event_list_init(&rv->ready);
    event_list_init(&rv->unuse);
    event_timer_list_init(&rv->timer);
    
    /* 初始化管道用于跨线程唤醒 */
    pipe(rv->io_pipe);
    
    return rv;
}
```

**这体现的设计理念：**
- 每个守护进程有独立的事件循环
- 资源清晰管理（分配、跟踪、释放）
- 支持优雅取消和清理

---

### 2. 高层架构详解

#### 各层职责边界

| 层次 | 拥有的职责 | 不拥有的职责 |
|------|------------|--------------|
| **协议守护进程** | 协议状态机、邻居管理、路由计算 | 内核路由安装、接口管理 |
| **Zebra** | RIB 管理、最佳路径选择、内核同步 | 协议细节、邻居协商 |
| **lib/** | 事件循环、内存管理、日志 | 业务逻辑 |
| **管理平面** | 配置解析、CLI 命令、YANG 模型 | 路由计算 |
| **内核** | 包转发、FIB、接口状态 | 路由协议逻辑 |

---

### 3. 控制平面与数据平面交互

#### 为什么协议不直接操作内核？

```
错误方式 (直接内核访问)：
+------------------------------------------------------------------+
|  bgpd                                    Kernel                  |
|    |                                       |                     |
|    | Route learned                         |                     |
|    |-------- RTM_NEWROUTE ---------------->|  问题：             |
|    |                                       |  - 无统一RIB        |
|    | ospfd                                 |  - 路由冲突         |
|    |-------- RTM_NEWROUTE ---------------->|  - 无最佳路径选择   |
|    |                                       |  - 状态不一致       |
+------------------------------------------------------------------+

正确方式 (通过 Zebra)：
+------------------------------------------------------------------+
|  bgpd         Zebra                        Kernel                |
|    |            |                            |                   |
|    | ZAPI_ROUTE |                            |                   |
|    |----------->| RIB Insert                 |                   |
|    |            | Best Path Select           |                   |
|    | ospfd      | (admin distance, metric)   |                   |
|    |----------->|                            |                   |
|    |            | Winner determined          |                   |
|    |            |-------- RTM_NEWROUTE ----->|                   |
|    |            |                            |                   |
|    |            |<------- ACK ---------------|                   |
|    |<-- NOTIFY--|                            |                   |
+------------------------------------------------------------------+
```

#### Zebra 的角色

**Zebra 是 FRR 的核心枢纽：**
1. **唯一的内核接口**：所有路由安装都通过 Zebra
2. **RIB 仲裁者**：多协议路由在此选择最佳
3. **状态同步**：确保控制平面与内核一致
4. **客户端管理**：跟踪连接的协议守护进程

---

### 4. 进程间通信模型

#### 为什么使用 IPC 而非共享内存？

| 方面 | IPC (ZAPI) | 共享内存 |
|------|------------|----------|
| **隔离性** | 进程崩溃不影响其他 | 可能导致数据损坏 |
| **调试** | 可以捕获消息流 | 难以跟踪访问 |
| **版本兼容** | 消息格式可版本化 | 结构变化困难 |
| **分布式扩展** | 可扩展到网络 | 仅限本机 |

#### 客户端-服务器模型

```c
/* ZAPI 消息流程 */

/* 1. 协议守护进程连接到 Zebra */
zclient = zclient_new(master);
zclient->sock = -1;
zclient_connect(zclient);  // 连接到 /var/run/frr/zserv.api

/* 2. 发送路由更新 */
zapi_route_encode(ZAPI_ROUTE_ADD, &api, buffer);
zclient_send_message(zclient, buffer);

/* 3. 接收响应 */
// Zebra 处理后通过回调通知结果
```

---

### 5. 并发与执行模型

#### 事件驱动设计

从 `event.c` 的 `event_fetch` 函数可以看到核心循环：

```c
/* event.c: 1754-1871 - 事件获取主循环 */
struct event *event_fetch(struct event_loop *m, struct event *fetch)
{
    do {
        /* 处理信号 */
        if (m->handle_signals)
            frr_sigevent_process();

        pthread_mutex_lock(&m->mtx);

        /* 处理取消请求 */
        do_event_cancel(m);

        /* 尝试从就绪队列获取 */
        if ((thread = event_list_pop(&m->ready))) {
            fetch = thread_run(m, thread, fetch);
            pthread_mutex_unlock(&m->mtx);
            break;
        }

        /* 处理待处理事件 */
        thread_process(&m->event);

        /* 计算等待时间（基于定时器） */
        tw = thread_timer_wait(&m->timer, &tv);

        /* 拷贝 pollfd 数组 */
        m->handler.copycount = m->handler.pfdcount;
        memcpy(m->handler.copy, m->handler.pfds, ...);

        pthread_mutex_unlock(&m->mtx);

        /* 执行 poll/ppoll */
        num = fd_poll(m, tw, &eintr_p);

        pthread_mutex_lock(&m->mtx);

        /* 处理定时器 */
        monotime(&now);
        thread_process_timers(m, &now);

        /* 处理 I/O 事件 */
        if (num > 0)
            thread_process_io(m, num);

        pthread_mutex_unlock(&m->mtx);

    } while (!thread && m->spin);

    return fetch;
}
```

#### 为什么避免重度多线程？

```
FRR 的线程策略：
+==================================================================+
|  主线程：事件驱动循环                                             |
|  ============================                                    |
|  - 所有协议状态机                                                |
|  - 所有路由计算                                                  |
|  - 所有配置处理                                                  |
|                                                                  |
|  辅助线程（有限使用）：                                          |
|  ====================                                            |
|  - I/O 密集操作（如 RPKI 验证）                                  |
|  - 后台维护任务                                                  |
|  - 总是通过事件队列与主线程通信                                  |
+==================================================================+
```

**优势：**
1. **可预测性**：没有隐藏的并发问题
2. **可调试性**：单一执行流易于跟踪
3. **正确性**：避免复杂的锁策略
4. **协作调度**：回调负责让出控制权

---

### 6. 定时器管理

从 `event.c` 可以看到定时器使用堆结构：

```c
/* event.c: 45-58 - 定时器比较函数 */
static int event_timer_cmp(const struct event *a, const struct event *b)
{
    if (a->u.sands.tv_sec < b->u.sands.tv_sec)
        return -1;
    if (a->u.sands.tv_sec > b->u.sands.tv_sec)
        return 1;
    if (a->u.sands.tv_usec < b->u.sands.tv_usec)
        return -1;
    if (a->u.sands.tv_usec > b->u.sands.tv_usec)
        return 1;
    return 0;
}

DECLARE_HEAP(event_timer_list, struct event, timeritem, event_timer_cmp);
```

**定时器在路由协议中的应用：**
- BGP Hold Timer (180s 默认)
- OSPF Hello Interval (10s 默认)
- 重传定时器
- 收敛延迟定时器

---

### 7. CPU 时间跟踪

FRR 内置了性能监控：

```c
/* event.c: 1971-2069 - event_call() 性能跟踪 */
void event_call(struct event *thread)
{
    RUSAGE_T before, after;
    
    GETRUSAGE(&before);
    
    /* 执行回调 */
    (*thread->func)(thread);
    
    GETRUSAGE(&after);
    
    /* 计算耗时 */
    walltime = event_consumed_time(&after, &before, &cputime);
    
    /* 记录统计 */
    atomic_fetch_add_explicit(&thread->hist->real.total, walltime, ...);
    atomic_fetch_add_explicit(&thread->hist->total_calls, 1, ...);
    
    /* 检测 CPU HOG */
    if (cputime > cputime_threshold) {
        flog_warn(EC_LIB_SLOW_THREAD_CPU,
                  "CPU HOG: task %s ran for %lums",
                  thread->xref->funcname, walltime / 1000);
    }
}
```

这允许运维人员识别性能问题：
```
Router# show event cpu
Active   Runtime(ms)   Invoked  Type  Event
  1234      5678.123     9999    T     bgp_keepalive_timer
```
