# FRR Kernel Interaction Architecture - Part 4: WHERE

## Code Reading Guide

This document provides a structured guide to reading FRR's kernel
interaction source code, focusing on the most important files and functions.

---

## 1. Source File Organization

```
+===========================================================================+
|                    KERNEL INTERACTION SOURCE FILES                        |
+===========================================================================+

frr/zebra/
    |
    +-- kernel_netlink.c      [CORE - Netlink infrastructure]
    |   |
    |   +-- netlink_socket()       - Create netlink socket
    |   +-- netlink_parse_info()   - Parse incoming messages
    |   +-- netlink_talk()         - Send and wait for ACK
    |   +-- netlink_send_msg()     - Low-level send
    |   +-- netlink_recv_msg()     - Low-level receive
    |   +-- nl_attr_put*()         - Attribute building helpers
    |
    +-- rt_netlink.c          [ROUTES - Route message handling]
    |   |
    |   +-- netlink_route_change() - Process route notifications
    |   +-- netlink_route_msg_encode() - Build route messages
    |   +-- kernel_route_update() - Install/delete routes
    |   +-- zebra2proto()         - Protocol ID mapping
    |   +-- proto2zebra()         - Reverse mapping
    |
    +-- if_netlink.c          [INTERFACES - Link message handling]
    |   |
    |   +-- netlink_link_change() - Process link notifications
    |   +-- netlink_interface_addr() - Address change handling
    |   +-- if_netlink_request() - Query interface state
    |
    +-- kernel_socket.c       [BSD - Routing socket (non-Linux)]
    |   |
    |   +-- rtm_read()            - Parse routing messages
    |   +-- rtm_write()           - Write routing messages
    |   +-- kernel_read()         - Event loop handler
    |
    +-- zebra_dplane.c        [DATAPLANE - Async operation layer]
    |   |
    |   +-- dplane_ctx_*()        - Context management
    |   +-- dplane_route_*()      - Route operations
    |   +-- kernel_dplane_read()  - Dataplane event handler
    |
    +-- zebra_rib.c           [RIB - Routing table management]
        |
        +-- rib_add()             - Add route to RIB
        +-- rib_delete()          - Delete route from RIB
        +-- rib_process()         - Best path selection


+===========================================================================+
|                    CODE READING ORDER                                     |
+===========================================================================+

    PHASE 1: Infrastructure
    =======================
    
    Start here to understand the foundation:
    
    +---------------------------------------------------------------------+
    | 1. kernel_netlink.c:netlink_socket()                                |
    |    - How sockets are created                                        |
    |    - Group subscriptions                                            |
    |    - Buffer sizing                                                  |
    +---------------------------------------------------------------------+
              |
              v
    +---------------------------------------------------------------------+
    | 2. kernel_netlink.c:netlink_parse_rtattr()                          |
    |    - TLV attribute parsing                                          |
    |    - Understanding rtattr structure                                 |
    +---------------------------------------------------------------------+
              |
              v
    +---------------------------------------------------------------------+
    | 3. kernel_netlink.c:nl_attr_put*()                                  |
    |    - Building netlink messages                                      |
    |    - Attribute encoding                                             |
    +---------------------------------------------------------------------+


    PHASE 2: Message Flow
    =====================
    
    Understand how messages are sent and received:
    
    +---------------------------------------------------------------------+
    | 4. kernel_netlink.c:netlink_send_msg()                              |
    |    - Low-level send operation                                       |
    |    - Error handling                                                 |
    +---------------------------------------------------------------------+
              |
              v
    +---------------------------------------------------------------------+
    | 5. kernel_netlink.c:netlink_recv_msg()                              |
    |    - Buffer management                                              |
    |    - Dynamic resizing                                               |
    +---------------------------------------------------------------------+
              |
              v
    +---------------------------------------------------------------------+
    | 6. kernel_netlink.c:netlink_parse_info()                            |
    |    - Message loop                                                   |
    |    - Multipart message handling                                     |
    |    - Dispatch to handlers                                           |
    +---------------------------------------------------------------------+


    PHASE 3: Route Operations
    =========================
    
    Core routing functionality:
    
    +---------------------------------------------------------------------+
    | 7. rt_netlink.c:netlink_route_change()                              |
    |    - Parsing RTM_NEWROUTE/DELROUTE                                  |
    |    - Attribute extraction                                           |
    |    - RIB updates                                                    |
    +---------------------------------------------------------------------+
              |
              v
    +---------------------------------------------------------------------+
    | 8. rt_netlink.c:netlink_route_msg_encode()                          |
    |    - Building route install messages                                |
    |    - Multipath encoding                                             |
    |    - MPLS/SRv6 encapsulation                                        |
    +---------------------------------------------------------------------+
              |
              v
    +---------------------------------------------------------------------+
    | 9. rt_netlink.c:kernel_route_update()                               |
    |    - High-level route operation                                     |
    |    - Replace vs add vs delete                                       |
    +---------------------------------------------------------------------+


    PHASE 4: BSD Alternative
    ========================
    
    For completeness, understand the routing socket API:
    
    +---------------------------------------------------------------------+
    | 10. kernel_socket.c (entire file)                                   |
    |     - Non-Linux kernel interaction                                  |
    |     - PF_ROUTE socket usage                                         |
    |     - Message format differences                                    |
    +---------------------------------------------------------------------+


+===========================================================================+
|                    KEY FUNCTIONS DETAILED                                 |
+===========================================================================+

                    netlink_parse_info()
                    ====================

  +------------------------------------------------------------------+
  | Function: Parse netlink response messages                        |
  | File: kernel_netlink.c                                           |
  +------------------------------------------------------------------+
  |                                                                  |
  |  while (1) {                                                     |
  |      +--------------------------------------------------+        |
  |      | netlink_recv_msg()                              |        |
  |      | - Receive raw message                           |        |
  |      +--------------------------------------------------+        |
  |                    |                                             |
  |                    v                                             |
  |      +--------------------------------------------------+        |
  |      | for each NLMSG in buffer:                       |        |
  |      |                                                  |        |
  |      |   if (NLMSG_ERROR)                              |        |
  |      |       netlink_parse_error() ---> handle ACK/err |        |
  |      |                                                  |        |
  |      |   else if (NLMSG_DONE)                          |        |
  |      |       return (end of dump)                      |        |
  |      |                                                  |        |
  |      |   else                                          |        |
  |      |       filter(h, ns_id, startup) --> dispatcher  |        |
  |      +--------------------------------------------------+        |
  |  }                                                               |
  +------------------------------------------------------------------+


                    netlink_route_msg_encode()
                    ==========================

  +------------------------------------------------------------------+
  | Function: Build route install/delete message                     |
  | File: rt_netlink.c                                               |
  +------------------------------------------------------------------+
  |                                                                  |
  |  1. Initialize nlmsghdr + rtmsg                                  |
  |     +------------------------------------------------------+     |
  |     | req.n.nlmsg_type = RTM_NEWROUTE / RTM_DELROUTE       |     |
  |     | req.r.rtm_family = AF_INET / AF_INET6                |     |
  |     | req.r.rtm_protocol = RTPROT_ZEBRA                    |     |
  |     +------------------------------------------------------+     |
  |                                                                  |
  |  2. Add destination                                              |
  |     +------------------------------------------------------+     |
  |     | nl_attr_put(&req.n, RTA_DST, &prefix, bytelen)       |     |
  |     +------------------------------------------------------+     |
  |                                                                  |
  |  3. For single nexthop:                                          |
  |     +------------------------------------------------------+     |
  |     | _netlink_route_build_singlepath()                    |     |
  |     | - RTA_GATEWAY, RTA_OIF, RTA_ENCAP                    |     |
  |     +------------------------------------------------------+     |
  |                                                                  |
  |  4. For multipath (ECMP):                                        |
  |     +------------------------------------------------------+     |
  |     | nl_attr_nest(RTA_MULTIPATH)                          |     |
  |     | for each nexthop:                                    |     |
  |     |   _netlink_route_build_multipath()                   |     |
  |     | nl_attr_nest_end()                                   |     |
  |     +------------------------------------------------------+     |
  +------------------------------------------------------------------+


                    kernel_socket.c overview
                    =========================

  +------------------------------------------------------------------+
  | BSD Routing Socket (Alternative to Netlink)                      |
  +------------------------------------------------------------------+
  |                                                                  |
  |  Socket: PF_ROUTE, SOCK_RAW                                      |
  |                                                                  |
  |  Message Types:                                                  |
  |  +----------------------------------------------------------+   |
  |  | RTM_ADD     - Add route                                  |   |
  |  | RTM_DELETE  - Delete route                               |   |
  |  | RTM_CHANGE  - Modify route                               |   |
  |  | RTM_GET     - Query route                                |   |
  |  | RTM_IFINFO  - Interface status                           |   |
  |  | RTM_NEWADDR - Address added                              |   |
  |  | RTM_DELADDR - Address deleted                            |   |
  |  +----------------------------------------------------------+   |
  |                                                                  |
  |  Key Functions:                                                  |
  |  +----------------------------------------------------------+   |
  |  | rtm_write() - Build and send routing message             |   |
  |  | rtm_read()  - Parse routing message                      |   |
  |  | ifm_read()  - Parse interface message                    |   |
  |  | ifam_read() - Parse interface address message            |   |
  |  +----------------------------------------------------------+   |
  |                                                                  |
  |  Platform Support:                                               |
  |  +----------------------------------------------------------+   |
  |  | FreeBSD, OpenBSD, NetBSD, macOS (Darwin)                 |   |
  |  +----------------------------------------------------------+   |
  +------------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 4.1 文件职责划分

**kernel_netlink.c - 基础设施层：**

```
职责：
1. Netlink 套接字创建和管理
2. 消息发送和接收
3. 属性编码和解码辅助函数
4. 错误处理框架

关键函数：
+------------------------------------------+
| netlink_socket()                         |
| - 创建 AF_NETLINK 套接字                 |
| - 设置组订阅                             |
| - 绑定地址                               |
+------------------------------------------+
| netlink_parse_info()                     |
| - 消息解析主循环                         |
| - 处理多部分消息                         |
| - 错误处理                               |
+------------------------------------------+
| nl_attr_put*()                           |
| - 添加各种类型的属性                     |
| - 自动处理对齐                           |
| - 长度计算                               |
+------------------------------------------+
```

**rt_netlink.c - 路由操作层：**

```
职责：
1. 路由消息构造
2. 路由变更处理
3. 多路径/ECMP 支持
4. 协议类型映射

关键函数：
+------------------------------------------+
| netlink_route_change()                   |
| - 解析 RTM_NEWROUTE/DELROUTE            |
| - 提取路由属性                           |
| - 调用 RIB 更新                          |
+------------------------------------------+
| netlink_route_msg_encode()               |
| - 构造路由安装消息                       |
| - 处理单路径和多路径                     |
| - 支持 MPLS/SRv6 封装                    |
+------------------------------------------+
| kernel_route_update()                    |
| - 高层路由操作入口                       |
| - 决定是添加还是替换                     |
+------------------------------------------+
```

**if_netlink.c - 接口操作层：**

```
职责：
1. 接口变更处理
2. 地址变更处理
3. 接口属性查询

关键函数：
+------------------------------------------+
| netlink_link_change()                    |
| - 处理 RTM_NEWLINK/DELLINK              |
| - 更新接口状态                           |
+------------------------------------------+
| netlink_interface_addr()                 |
| - 处理 RTM_NEWADDR/DELADDR              |
| - 触发路由重算                           |
+------------------------------------------+
```

**kernel_socket.c - BSD 兼容层：**

```
职责：
1. 非 Linux 系统的内核交互
2. 使用 PF_ROUTE 套接字
3. 提供与 Netlink 相似的抽象

特点：
- 条件编译 (#ifndef HAVE_NETLINK)
- 消息格式不同但概念相似
- 功能相对有限
```

### 4.2 代码阅读路径

**推荐阅读顺序：**

```
第一阶段：理解基础
==================
1. kernel_netlink.c 头部
   - 了解宏定义和结构
   - 理解 nlsock 结构

2. netlink_socket() 函数
   - 套接字创建流程
   - 组订阅机制

3. nl_attr_put() 系列函数
   - TLV 编码方式
   - 对齐规则

第二阶段：理解消息流
==================
4. netlink_send_msg()
   - 消息发送过程
   - 权限处理

5. netlink_recv_msg()
   - 消息接收
   - 缓冲区动态调整

6. netlink_parse_info()
   - 消息解析循环
   - 错误处理策略

第三阶段：理解路由
==================
7. netlink_route_change()
   - 路由通知处理
   - 属性解析

8. netlink_route_msg_encode()
   - 路由消息构造
   - ECMP 处理

9. kernel_route_update()
   - 上层调用接口
   - 与 dataplane 的交互
```

### 4.3 关键数据结构定位

**在代码中查找：**

```c
// Netlink 套接字结构 - kernel_netlink.c
struct nlsock {
    int sock;
    struct sockaddr_nl snl;
    char *name;
    char *buf;
    size_t buflen;
};

// 批量处理结构 - kernel_netlink.c
struct nl_batch {
    void *buf;
    size_t bufsiz;
    size_t limit;
    ...
};

// 消息类型映射 - kernel_netlink.c
static const struct message nlmsg_str[] = {
    { RTM_NEWROUTE, "RTM_NEWROUTE" },
    { RTM_DELROUTE, "RTM_DELROUTE" },
    ...
};

// 协议映射 - rt_netlink.c
int zebra2proto(int proto) { ... }
int proto2zebra(int proto, int family, bool is_nexthop) { ... }
```

### 4.4 调试技巧

**启用调试输出：**

```
在 vtysh 中：
debug zebra kernel
debug zebra kernel msgdump recv
debug zebra kernel msgdump send

这会显示：
- 所有 Netlink 消息的发送和接收
- 消息的十六进制转储
- 错误详情
```

**关键调试宏：**

```c
IS_ZEBRA_DEBUG_KERNEL
  - 一般内核调试信息

IS_ZEBRA_DEBUG_KERNEL_MSGDUMP_SEND
  - 发送消息转储

IS_ZEBRA_DEBUG_KERNEL_MSGDUMP_RECV
  - 接收消息转储
```

### 4.5 平台差异

**Linux vs BSD：**

```
+----------------------------------------+
| 特性           | Linux    | BSD       |
+----------------------------------------+
| 套接字类型     | AF_NETLINK| PF_ROUTE |
| 消息格式       | TLV      | 固定结构  |
| ECMP 支持      | 完整     | 有限      |
| Nexthop 对象   | 支持     | 不支持    |
| VRF 支持       | 完整     | 有限      |
| MPLS 支持      | 完整     | OpenBSD   |
+----------------------------------------+
```

**条件编译：**

```c
#ifdef HAVE_NETLINK
// Linux Netlink 代码
#else
// BSD 路由套接字代码
#endif
```

---

## 2. Important Code Patterns

### 2.1 Message Building Pattern

```c
// Standard pattern for building netlink messages
struct {
    struct nlmsghdr n;
    struct rtmsg r;
    char buf[NL_PKT_BUF_SIZE];
} req;

memset(&req, 0, sizeof(req));
req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
req.n.nlmsg_type = RTM_NEWROUTE;
req.n.nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST;

// Add attributes
nl_attr_put(&req.n, sizeof(req), RTA_DST, &dest, addrlen);
nl_attr_put32(&req.n, sizeof(req), RTA_OIF, ifindex);
```

### 2.2 Message Parsing Pattern

```c
// Standard pattern for parsing netlink messages
struct rtattr *tb[RTA_MAX + 1];
netlink_parse_rtattr(tb, RTA_MAX, RTM_RTA(rtm),
                     len - sizeof(struct rtmsg));

if (tb[RTA_DST])
    dest = RTA_DATA(tb[RTA_DST]);
if (tb[RTA_GATEWAY])
    gateway = RTA_DATA(tb[RTA_GATEWAY]);
```

---

## Next: Part 5 - API (Boundaries)
