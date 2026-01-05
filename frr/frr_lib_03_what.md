# FRR lib/ Infrastructure: WHAT | Core Components

## Overview Diagram

```
+-----------------------------------------------------------------------------+
|                                                                             |
|                   CORE COMPONENTS IN lib/                                   |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|  +-----------------------------------------------------------------------+  |
|  |                      COMPONENT HIERARCHY                              |  |
|  +-----------------------------------------------------------------------+  |
|  |                                                                       |  |
|  |  GENERIC (Reusable Anywhere)       ROUTING-SPECIFIC                   |  |
|  |  ===========================       ================                   |  |
|  |                                                                       |  |
|  |  +-------------------+             +-------------------+              |  |
|  |  | Event System      |             | VRF Abstraction   |              |  |
|  |  | - frrevent.h      |             | - vrf.h           |              |  |
|  |  | - event.c         |             | - vrf.c           |              |  |
|  |  +-------------------+             +-------------------+              |  |
|  |                                                                       |  |
|  |  +-------------------+             +-------------------+              |  |
|  |  | Memory Management |             | Prefix Utilities  |              |  |
|  |  | - memory.h        |             | - prefix.h        |              |  |
|  |  | - memory.c        |             | - prefix.c        |              |  |
|  |  +-------------------+             +-------------------+              |  |
|  |                                                                       |  |
|  |  +-------------------+             +-------------------+              |  |
|  |  | Logging System    |             | Zebra Client      |              |  |
|  |  | - zlog.h          |             | - zclient.h       |              |  |
|  |  | - log.h           |             | - zclient.c       |              |  |
|  |  +-------------------+             +-------------------+              |  |
|  |                                                                       |  |
|  |  +-------------------+             +-------------------+              |  |
|  |  | Data Structures   |             | Nexthop Groups    |              |  |
|  |  | - typesafe.h      |             | - nexthop_group.h |              |  |
|  |  | - linklist.h      |             | - nexthop.h       |              |  |
|  |  | - hash.h          |             +-------------------+              |  |
|  |  +-------------------+                                                |  |
|  |                                                                       |  |
|  |  +-------------------+             +-------------------+              |  |
|  |  | Buffer/Stream     |             | Route Tables      |              |  |
|  |  | - stream.h        |             | - table.h         |              |  |
|  |  | - buffer.h        |             | - srcdest_table.h |              |  |
|  |  +-------------------+             +-------------------+              |  |
|  |                                                                       |  |
|  +-----------------------------------------------------------------------+  |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  1. EVENT SYSTEM (frrevent.h, event.c)                                      |
|  ======================================                                     |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |            struct event_loop (master)                               |    |
|  |  +---------------------------------------------------------------+  |    |
|  |  |                                                               |  |    |
|  |  |  +-----------+  +-----------+  +-----------+  +------------+  |  |    |
|  |  |  |  Timer    |  |   Read    |  |   Write   |  |   Event    |  |  |    |
|  |  |  |   Heap    |  |   Array   |  |   Array   |  |   List     |  |  |    |
|  |  |  | (min-heap)|  |(fd→event) |  |(fd→event) |  | (immediate)|  |  |    |
|  |  |  +-----------+  +-----------+  +-----------+  +------------+  |  |    |
|  |  |                                                               |  |    |
|  |  |                    poll() system call                         |  |    |
|  |  |                           |                                   |  |    |
|  |  |                           v                                   |  |    |
|  |  |                   +---------------+                           |  |    |
|  |  |                   |  Ready Queue  |                           |  |    |
|  |  |                   +-------+-------+                           |  |    |
|  |  |                           |                                   |  |    |
|  |  |                           v                                   |  |    |
|  |  |                   event_call(event)                           |  |    |
|  |  |                                                               |  |    |
|  |  +---------------------------------------------------------------+  |    |
|  |                                                                     |    |
|  |  API:                                                               |    |
|  |  - event_add_read(master, func, arg, fd, &event)                    |    |
|  |  - event_add_write(master, func, arg, fd, &event)                   |    |
|  |  - event_add_timer(master, func, arg, seconds, &event)              |    |
|  |  - event_add_timer_msec(master, func, arg, msec, &event)            |    |
|  |  - event_add_event(master, func, arg, val, &event)                  |    |
|  |  - event_cancel(&event)                                             |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  2. MEMORY MANAGEMENT (memory.h, memory.c)                                  |
|  =========================================                                  |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Memory Type Hierarchy:                                             |    |
|  |                                                                     |    |
|  |  struct memgroup (LIB, BGP, OSPF, ...)                              |    |
|  |       |                                                             |    |
|  |       +-- struct memtype (MTYPE_TMP, MTYPE_BGP_PEER, ...)           |    |
|  |              |                                                      |    |
|  |              +-- name: "BGP peer"                                   |    |
|  |              +-- n_alloc: 42    (current allocations)               |    |
|  |              +-- n_max: 100     (peak allocations)                  |    |
|  |              +-- size: 256      (bytes per allocation)              |    |
|  |              +-- total: 10752   (total bytes allocated)             |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Usage Pattern:                                                     |    |
|  |                                                                     |    |
|  |  // In header file                                                  |    |
|  |  DECLARE_MGROUP(BGP);                                               |    |
|  |  DECLARE_MTYPE(BGP_PEER);                                           |    |
|  |                                                                     |    |
|  |  // In source file                                                  |    |
|  |  DEFINE_MGROUP(BGP, "BGP daemon memory");                           |    |
|  |  DEFINE_MTYPE(BGP, BGP_PEER, "BGP peer structure");                 |    |
|  |                                                                     |    |
|  |  // Allocation                                                      |    |
|  |  peer = XMALLOC(MTYPE_BGP_PEER, sizeof(*peer));                     |    |
|  |  peer = XCALLOC(MTYPE_BGP_PEER, sizeof(*peer));  // zeroed          |    |
|  |  str = XSTRDUP(MTYPE_BGP_PEER_DESC, "description");                 |    |
|  |                                                                     |    |
|  |  // Deallocation (sets pointer to NULL!)                            |    |
|  |  XFREE(MTYPE_BGP_PEER, peer);  // peer == NULL after                |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  3. LOGGING SYSTEM (zlog.h, log.h)                                          |
|  =================================                                          |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Log Levels (syslog compatible):                                    |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  | LOG_EMERG   | LOG_ALERT   | LOG_CRIT    | LOG_ERR        |      |    |
|  |  | LOG_WARNING | LOG_NOTICE  | LOG_INFO    | LOG_DEBUG      |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  |  API:                                                               |    |
|  |  - zlog_err("Error: %s", msg)                                       |    |
|  |  - zlog_warn("Warning: %s", msg)                                    |    |
|  |  - zlog_info("Info: %s", msg)                                       |    |
|  |  - zlog_notice("Notice: %s", msg)                                   |    |
|  |  - zlog_debug("Debug: %s", msg)                                     |    |
|  |                                                                     |    |
|  |  Error codes:                                                       |    |
|  |  - flog_err(EC_BGP_UPDATE, "BGP update error")                      |    |
|  |  - flog_warn(EC_OSPF_LSA, "OSPF LSA warning")                       |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Log Targets:                                                       |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |  struct zlog_target                                       |      |    |
|  |  |    +-- stdout/stderr                                      |      |    |
|  |  |    +-- syslog                                             |      |    |
|  |  |    +-- file                                               |      |    |
|  |  |    +-- VTY (log monitor)                                  |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  4. DATA STRUCTURES (typesafe.h, linklist.h, hash.h)                        |
|  ===================================================                        |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  TYPESAFE Containers (Modern, Type-safe):                           |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  // Declare a typed list                                  |      |    |
|  |  |  PREDECL_DLIST(peer_list);                                |      |    |
|  |  |  struct peer {                                            |      |    |
|  |  |      struct peer_list_item list_item;  // embed in struct |      |    |
|  |  |      // ... other fields                                  |      |    |
|  |  |  };                                                       |      |    |
|  |  |  DECLARE_DLIST(peer_list, struct peer, list_item);        |      |    |
|  |  |                                                           |      |    |
|  |  |  // Usage                                                 |      |    |
|  |  |  struct peer_list_head head;                              |      |    |
|  |  |  peer_list_init(&head);                                   |      |    |
|  |  |  peer_list_add_tail(&head, peer);                         |      |    |
|  |  |  frr_each(peer_list, &head, p) { ... }                    |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  |  Available Typesafe Containers:                                     |    |
|  |  - DLIST:       Double-linked list                                  |    |
|  |  - LIST:        Single-linked list                                  |    |
|  |  - HEAP:        Min-heap (priority queue)                           |    |
|  |  - HASH:        Hash table                                          |    |
|  |  - SORTLIST:    Sorted list (unique/non-unique)                     |    |
|  |  - SKIPLIST:    Skip list                                           |    |
|  |  - RBTREE:      Red-black tree                                      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Legacy Containers (linklist.h, hash.h):                            |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  | WARNING: Consider using typesafe.h for new code!          |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  // Old-style linked list (void* based)                   |      |    |
|  |  |  struct list *mylist = list_new();                        |      |    |
|  |  |  listnode_add(mylist, data);                              |      |    |
|  |  |  ALL_LIST_ELEMENTS(mylist, node, nextnode, data) { ... }  |      |    |
|  |  |                                                           |      |    |
|  |  |  // Old-style hash table                                  |      |    |
|  |  |  struct hash *myhash = hash_create(keyfn, cmpfn, "name"); |      |    |
|  |  |  hash_get(myhash, key, hash_alloc_intern);                |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  5. VRF ABSTRACTION (vrf.h, vrf.c)                                          |
|  =================================                                          |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  struct vrf                                                         |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  | vrf_id:  uint32_t                  (kernel VRF ID)        |      |    |
|  |  | name:    char[VRF_NAMSIZ+1]        (VRF name)             |      |    |
|  |  | status:  VRF_ACTIVE | VRF_CONFIGURED                      |      |    |
|  |  | ifaces_by_name:   RB tree of interfaces                   |      |    |
|  |  | ifaces_by_index:  RB tree of interfaces                   |      |    |
|  |  | info:    void*                     (daemon-specific data) |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  |  VRF Backend Types:                                                 |    |
|  |  - VRF_BACKEND_VRF_LITE:  Linux VRF device                          |    |
|  |  - VRF_BACKEND_NETNS:     Linux network namespace                   |    |
|  |                                                                     |    |
|  |  API:                                                               |    |
|  |  - vrf_lookup_by_id(vrf_id)                                         |    |
|  |  - vrf_lookup_by_name(name)                                         |    |
|  |  - vrf_socket(domain, type, protocol, vrf_id, name)                 |    |
|  |  - vrf_bind(vrf_id, fd, ifname)                                     |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  6. PREFIX AND ADDRESS UTILITIES (prefix.h, prefix.c)                       |
|  ====================================================                       |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  struct prefix                                                      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  | family:     uint8_t       (AF_INET, AF_INET6, AF_EVPN)    |      |    |
|  |  | prefixlen:  uint16_t      (0-32 for IPv4, 0-128 for IPv6) |      |    |
|  |  | u:          union                                         |      |    |
|  |  |   +-- prefix4:  struct in_addr                            |      |    |
|  |  |   +-- prefix6:  struct in6_addr                           |      |    |
|  |  |   +-- prefix_eth: struct ethaddr                          |      |    |
|  |  |   +-- prefix_evpn: struct evpn_addr                       |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  |  Specialized Types:                                                 |    |
|  |  - struct prefix_ipv4                                               |    |
|  |  - struct prefix_ipv6                                               |    |
|  |  - struct prefix_evpn                                               |    |
|  |  - struct prefix_eth                                                |    |
|  |                                                                     |    |
|  |  API:                                                               |    |
|  |  - str2prefix("10.0.0.0/8", &p)                                     |    |
|  |  - prefix2str(&p, buf, sizeof(buf))                                 |    |
|  |  - prefix_match(&net, &host)                                        |    |
|  |  - prefix_same(&p1, &p2)                                            |    |
|  |  - prefix_cmp(&p1, &p2)                                             |    |
|  |  - apply_mask(&p)                                                   |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  7. BUFFER AND STREAM UTILITIES (stream.h, buffer.h)                        |
|  ===================================================                        |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  struct stream (Network Byte Order Buffer):                         |    |
|  |  +-----------------------------------------------------------+      |    |
|  |  |                                                           |      |    |
|  |  |  |XXXXXXXXXXXXXXXXXXXXXXXXX                         |     |      |    |
|  |  |  0                         ^               ^         ^    |      |    |
|  |  |                           getp            endp      size  |      |    |
|  |  |                                                           |      |    |
|  |  |  getp: next read position                                 |      |    |
|  |  |  endp: end of valid data                                  |      |    |
|  |  |  size: total buffer size                                  |      |    |
|  |  |                                                           |      |    |
|  |  +-----------------------------------------------------------+      |    |
|  |                                                                     |    |
|  |  API for building protocol messages:                                |    |
|  |  - stream_new(size)                                                 |    |
|  |  - stream_put(s, data, len)         // write bytes                  |    |
|  |  - stream_putc(s, byte)             // write uint8                  |    |
|  |  - stream_putw(s, word)             // write uint16 (network order) |    |
|  |  - stream_putl(s, long)             // write uint32 (network order) |    |
|  |  - stream_putq(s, quad)             // write uint64 (network order) |    |
|  |                                                                     |    |
|  |  API for parsing protocol messages:                                 |    |
|  |  - stream_get(s, buf, len)          // read bytes                   |    |
|  |  - stream_getc(s)                   // read uint8                   |    |
|  |  - stream_getw(s)                   // read uint16                  |    |
|  |  - stream_getl(s)                   // read uint32                  |    |
|  |  - stream_getq(s)                   // read uint64                  |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+

+-----------------------------------------------------------------------------+
|                                                                             |
|  8. ZEBRA CLIENT (zclient.h, zclient.c)                                     |
|  ======================================                                     |
|                                                                             |
|  +---------------------------------------------------------------------+    |
|  |                                                                     |    |
|  |  Protocol Daemon <---> Zebra Communication:                         |    |
|  |                                                                     |    |
|  |  +----------+                              +----------+             |    |
|  |  |  bgpd    |  <--- ZAPI Messages --->     |  zebra   |             |    |
|  |  |  ospfd   |                              |          |             |    |
|  |  |  isisd   |                              |          |             |    |
|  |  +----------+                              +----------+             |    |
|  |                                                                     |    |
|  |  Message Types (subset):                                            |    |
|  |  - ZEBRA_ROUTE_ADD / ZEBRA_ROUTE_DELETE                             |    |
|  |  - ZEBRA_INTERFACE_ADD / ZEBRA_INTERFACE_UP                         |    |
|  |  - ZEBRA_REDISTRIBUTE_ADD / ZEBRA_REDISTRIBUTE_DELETE               |    |
|  |  - ZEBRA_NEXTHOP_REGISTER / ZEBRA_NEXTHOP_UPDATE                    |    |
|  |                                                                     |    |
|  |  API:                                                               |    |
|  |  - zclient_new(master, options, handlers, bufsize)                  |    |
|  |  - zclient_start(zc)                                                |    |
|  |  - zclient_send_message(zc)                                         |    |
|  |  - zapi_route_encode(cmd, s, api)                                   |    |
|  |                                                                     |    |
|  +---------------------------------------------------------------------+    |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细解释

### 1. 事件系统 (frrevent.h, event.c)

事件系统是 FRR 所有守护进程的核心运行时。

**主要数据结构：**

```c
struct event_loop {
    struct event **read;       // 文件描述符 -> 读事件映射
    struct event **write;      // 文件描述符 -> 写事件映射
    struct event_timer_list_head timer;  // 定时器最小堆
    struct event_list_head event;        // 立即事件队列
    struct event_list_head ready;        // 就绪队列
};

struct event {
    enum event_types type;     // READ, WRITE, TIMER, EVENT
    void (*func)(struct event *);  // 回调函数
    void *arg;                 // 用户数据
    union {
        int fd;                // 文件描述符（读/写事件）
        struct timeval sands;  // 到期时间（定时器）
        int val;               // 值（立即事件）
    } u;
};
```

**使用模式：**

```c
// 添加读事件
event_add_read(master, socket_readable, data, sockfd, &read_event);

// 添加定时器（10秒后触发）
event_add_timer(master, my_timer, data, 10, &timer_event);

// 添加立即事件（下一个循环执行）
event_add_event(master, process_now, data, 0, &imm_event);

// 取消事件
event_cancel(&timer_event);
```

### 2. 内存管理 (memory.h, memory.c)

FRR 的内存管理系统提供类型化分配和泄漏检测。

**内存类型层次：**

```
memgroup (LIB)
    |
    +-- memtype (MTYPE_TMP)
    +-- memtype (MTYPE_PREFIX)
    
memgroup (BGP)
    |
    +-- memtype (MTYPE_BGP_PEER)
    +-- memtype (MTYPE_BGP_PATH_INFO)
```

**定义内存类型：**

```c
// 在 .h 文件
DECLARE_MGROUP(BGP);
DECLARE_MTYPE(BGP_PEER);

// 在 .c 文件
DEFINE_MGROUP(BGP, "BGP daemon memory");
DEFINE_MTYPE(BGP, BGP_PEER, "BGP peer structure");
```

**分配和释放：**

```c
// 分配
peer = XMALLOC(MTYPE_BGP_PEER, sizeof(*peer));
peer = XCALLOC(MTYPE_BGP_PEER, sizeof(*peer));  // 零初始化

// 释放（自动将指针置 NULL）
XFREE(MTYPE_BGP_PEER, peer);  // peer == NULL
```

### 3. 日志系统 (zlog.h, log.h)

日志系统支持多目标输出和结构化错误码。

**日志级别：**

| 级别 | 用途 |
|------|------|
| LOG_ERR | 错误情况，需要关注 |
| LOG_WARNING | 警告，但程序可以继续 |
| LOG_NOTICE | 守护进程启动/关闭 |
| LOG_INFO | 有用信息 |
| LOG_DEBUG | 调试信息 |

**使用方式：**

```c
zlog_err("Connection to %s failed: %s", addr, strerror(errno));
zlog_warn("Neighbor %pI4 not responding", &neighbor_ip);
zlog_info("Route %pFX installed", &prefix);
zlog_debug("Processing update from %s", peer->host);

// 带错误码的日志
flog_err(EC_BGP_UPDATE, "BGP update processing failed");
```

### 4. 数据结构

**类型安全容器 (typesafe.h) - 推荐用于新代码：**

```c
// 声明类型化双向链表
PREDECL_DLIST(peer_list);

struct peer {
    struct peer_list_item item;  // 嵌入链表节点
    char *name;
    // ...
};

DECLARE_DLIST(peer_list, struct peer, item);

// 使用
struct peer_list_head head;
peer_list_init(&head);
peer_list_add_tail(&head, new_peer);

// 类型安全的遍历
frr_each(peer_list, &head, peer) {
    printf("Peer: %s\n", peer->name);
}
```

**可用的类型安全容器：**

| 容器类型 | 说明 |
|----------|------|
| DLIST | 双向链表，O(1) 插入/删除 |
| LIST | 单向链表，更节省内存 |
| HEAP | 最小堆，用于优先队列 |
| HASH | 哈希表，O(1) 查找 |
| SORTLIST | 有序列表 |
| SKIPLIST | 跳表，O(log n) 操作 |
| RBTREE | 红黑树，平衡有序 |

### 5. VRF 抽象 (vrf.h, vrf.c)

VRF (Virtual Routing and Forwarding) 允许在同一设备上运行多个独立的路由表。

**VRF 结构：**

```c
struct vrf {
    vrf_id_t vrf_id;           // 内核 VRF ID
    char name[VRF_NAMSIZ + 1]; // VRF 名称
    uint8_t status;            // VRF_ACTIVE, VRF_CONFIGURED
    struct if_name_head ifaces_by_name;   // 接口（按名称）
    struct if_index_head ifaces_by_index; // 接口（按索引）
    void *info;                // 守护进程特定数据
};
```

**VRF 感知的套接字创建：**

```c
// 在特定 VRF 中创建套接字
int sock = vrf_socket(AF_INET, SOCK_STREAM, 0, vrf_id, vrf_name);

// 绑定到 VRF
vrf_bind(vrf_id, sock, ifname);
```

### 6. 前缀和地址工具 (prefix.h, prefix.c)

用于处理 IP 前缀的核心数据结构。

**前缀结构：**

```c
struct prefix {
    uint8_t family;     // AF_INET, AF_INET6, AF_EVPN
    uint16_t prefixlen; // 前缀长度
    union {
        struct in_addr prefix4;    // IPv4 地址
        struct in6_addr prefix6;   // IPv6 地址
        struct ethaddr prefix_eth; // MAC 地址
        struct evpn_addr prefix_evpn; // EVPN 地址
    } u;
};
```

**常用 API：**

```c
struct prefix p;

// 解析字符串
str2prefix("10.0.0.0/8", &p);
str2prefix_ipv4("192.168.1.0/24", (struct prefix_ipv4 *)&p);

// 转换为字符串
char buf[PREFIX_STRLEN];
prefix2str(&p, buf, sizeof(buf));

// 比较和匹配
if (prefix_match(&network, &host)) { ... }
if (prefix_same(&p1, &p2)) { ... }
int cmp = prefix_cmp(&p1, &p2);

// 应用掩码
apply_mask(&p);  // 将主机位清零
```

### 7. 缓冲区和流工具 (stream.h, buffer.h)

用于构建和解析网络协议消息。

**Stream 用于网络字节序操作：**

```c
struct stream *s = stream_new(1024);

// 写入数据（自动转换为网络字节序）
stream_putc(s, 0x01);           // 1 字节
stream_putw(s, 0x1234);         // 2 字节，网络字节序
stream_putl(s, 0x12345678);     // 4 字节，网络字节序
stream_put(s, data, len);       // 原始字节

// 读取数据
uint8_t b = stream_getc(s);
uint16_t w = stream_getw(s);
uint32_t l = stream_getl(s);
stream_get(s, buf, len);
```

### 8. Zebra 客户端 (zclient.h, zclient.c)

协议守护进程与 Zebra 通信的 API。

**初始化和使用：**

```c
// 创建 zclient
struct zclient *zclient = zclient_new(master, &zclient_options_default,
                                      my_handlers, 128);

// 启动连接
zclient_start(zclient);

// 发送路由
struct zapi_route api = {};
api.type = ZEBRA_ROUTE_BGP;
api.prefix = route_prefix;
api.nexthop_num = 1;
// ... 填充其他字段
zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api);
```

### 通用 vs 路由专用组件

| 组件 | 类型 | 可在外部项目使用？ |
|------|------|-------------------|
| event.c | 通用 | ✓ 需要 libfrr |
| memory.c | 通用 | ✓ 需要 libfrr |
| zlog.h | 通用 | ✓ 需要 libfrr |
| typesafe.h | 通用 | ✓ 头文件即可 |
| stream.h | 通用 | ✓ 需要 libfrr |
| vrf.h | 路由专用 | ⚠ 需要 Zebra |
| prefix.h | 路由专用 | ✓ 需要 libfrr |
| zclient.h | 路由专用 | ⚠ 需要 Zebra |
