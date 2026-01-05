# FRR Kernel Interaction Architecture - Part 5: API

## API Boundaries

This document defines what kernel interfaces are abstracted by FRR,
what should never leak upward to protocol daemons, and the proper
integration points for external systems.

---

## 1. Abstraction Layers

```
+===========================================================================+
|                    FRR KERNEL ABSTRACTION LAYERS                          |
+===========================================================================+

+-------------------------------------------------------------------------+
|                     PROTOCOL DAEMONS                                     |
|                                                                          |
|  +-------------+  +-------------+  +-------------+  +-------------+     |
|  |   bgpd      |  |   ospfd     |  |   ripd      |  |   isisd     |     |
|  +-------------+  +-------------+  +-------------+  +-------------+     |
|         |               |               |               |                |
|         +---------------+---------------+---------------+                |
|                                 |                                        |
|                                 | ZAPI (Zebra API)                       |
|                                 | - Route redistribution                 |
|                                 | - Nexthop registration                 |
|                                 | - Interface state                      |
|                                 |                                        |
+-------------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------------+
|                         ZEBRA DAEMON                                     |
|                                                                          |
|  +------------------------------------------------------------------+   |
|  |                      RIB LAYER                                    |   |
|  |                                                                   |   |
|  |  - Route selection logic                                          |   |
|  |  - Nexthop resolution                                             |   |
|  |  - Policy application                                             |   |
|  |  - VRF management                                                 |   |
|  +------------------------------------------------------------------+   |
|                                  |                                       |
|                                  v                                       |
|  +------------------------------------------------------------------+   |
|  |                   DATAPLANE LAYER                                 |   |
|  |                                                                   |   |
|  |  - Async operation queue                                          |   |
|  |  - Context management                                             |   |
|  |  - Provider abstraction                                           |   |
|  +------------------------------------------------------------------+   |
|                                  |                                       |
|                                  v                                       |
|  +------------------------------------------------------------------+   |
|  |                   KERNEL LAYER                                    |   |
|  |                                                                   |   |
|  |  +-----------------------+  +-----------------------+             |   |
|  |  |    rt_netlink.c       |  |   kernel_socket.c     |             |   |
|  |  |    (Linux)            |  |   (BSD)               |             |   |
|  |  +-----------------------+  +-----------------------+             |   |
|  +------------------------------------------------------------------+   |
+-------------------------------------------------------------------------+
                                  |
                                  | KERNEL INTERFACE
                                  | (Netlink / Routing Socket)
                                  v
+-------------------------------------------------------------------------+
|                         OPERATING SYSTEM KERNEL                          |
+-------------------------------------------------------------------------+


+===========================================================================+
|                    WHAT MUST STAY HIDDEN                                  |
+===========================================================================+

                    LEAKED ABSTRACTIONS (BAD)
                    =========================

  +---------------------------------------------------------------+
  |  PROTOCOL DAEMON                                              |
  |                                                               |
  |  // BAD: Direct kernel interaction                            |
  |  #include <linux/rtnetlink.h>                                 |
  |  sendmsg(netlink_sock, &rtm_msg, 0);  // WRONG!              |
  |                                                               |
  |  // BAD: Kernel-specific types in protocol code               |
  |  struct rtmsg rtm;                    // WRONG!              |
  |  rtm.rtm_protocol = RTPROT_BGP;       // WRONG!              |
  +---------------------------------------------------------------+


                    PROPER ABSTRACTION (GOOD)
                    =========================

  +---------------------------------------------------------------+
  |  PROTOCOL DAEMON                                              |
  |                                                               |
  |  // GOOD: Use ZAPI abstractions                               |
  |  #include "lib/zclient.h"                                     |
  |                                                               |
  |  struct zapi_route api;                                       |
  |  api.type = ZEBRA_ROUTE_BGP;                                  |
  |  api.prefix = route_prefix;                                   |
  |                                                               |
  |  zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api);         |
  +---------------------------------------------------------------+


+===========================================================================+
|                    ABSTRACTED INTERFACES                                  |
+===========================================================================+

  +-----------------------------------------------------------------+
  |  KERNEL CONCEPT           |  FRR ABSTRACTION                    |
  +-----------------------------------------------------------------+
  |  AF_NETLINK socket        |  struct nlsock (internal only)      |
  |  RTM_NEWROUTE msg         |  zapi_route + ZEBRA_ROUTE_ADD      |
  |  RTPROT_* constants       |  ZEBRA_ROUTE_* enum                 |
  |  struct rtmsg             |  struct route_entry (internal)      |
  |  ifindex                  |  struct interface *                 |
  |  Netlink groups           |  ZAPI redistribute subscribe        |
  |  RTM_GETLINK dump         |  zclient_interface_add callback     |
  |  RTA_* attributes         |  zapi_* structure fields            |
  +-----------------------------------------------------------------+


+===========================================================================+
|                    ZAPI MESSAGE FLOW                                      |
+===========================================================================+

  ROUTE INSTALLATION:
  ===================

  Protocol Daemon                   Zebra                     Kernel
       |                              |                          |
       |  ZEBRA_ROUTE_ADD             |                          |
       |  (zapi_route)                |                          |
       |----------------------------->|                          |
       |                              |                          |
       |                              |  [RIB processing]        |
       |                              |  [Best path selection]   |
       |                              |  [Nexthop resolution]    |
       |                              |                          |
       |                              |  RTM_NEWROUTE            |
       |                              |  (netlink msg)           |
       |                              |------------------------->|
       |                              |                          |
       |                              |  ACK or ERROR            |
       |                              |<-------------------------|
       |                              |                          |
       |                              |  [Update RIB status]     |
       |                              |                          |


  INTERFACE NOTIFICATION:
  =======================

  Kernel                     Zebra                     Protocol Daemon
     |                          |                              |
     |  RTM_NEWLINK             |                              |
     |------------------------->|                              |
     |                          |                              |
     |                          |  [Parse netlink msg]         |
     |                          |  [Update interface DB]       |
     |                          |                              |
     |                          |  ZEBRA_INTERFACE_ADD         |
     |                          |  (zapi_interface)            |
     |                          |----------------------------->|
     |                          |                              |
     |                          |                              |  [Update
     |                          |                              |   neighbors]
```

---

## 中文解释 (Chinese Explanation)

### 5.1 抽象层边界

**为什么需要抽象？**

```
问题：
1. 协议守护进程不应该知道底层是 Linux 还是 BSD
2. 内核接口变化不应该影响协议实现
3. 测试和模拟需要能替换内核层

解决方案 - 三层抽象：
+--------------------------------------------+
| 层次         | 职责                        |
+--------------------------------------------+
| ZAPI 层      | 协议到 Zebra 的通信         |
| RIB/Dataplane| 路由决策和操作队列          |
| Kernel 层    | 实际内核交互                |
+--------------------------------------------+
```

### 5.2 什么必须保持隐藏

**绝不应该泄漏到上层的内容：**

```c
// 错误示例 - 在协议守护进程中
#include <linux/rtnetlink.h>  // 不要这样做！

void install_bgp_route() {
    struct nlmsghdr nlh;           // 不要直接使用！
    struct rtmsg rtm;              // 这是内核层细节！
    rtm.rtm_protocol = RTPROT_BGP; // 不要直接使用！
    sendmsg(sock, ...);            // 绝对不行！
}

// 正确示例 - 在协议守护进程中
#include "lib/zclient.h"

void install_bgp_route() {
    struct zapi_route api;
    
    // 使用抽象的 FRR 类型
    api.type = ZEBRA_ROUTE_BGP;
    api.prefix = prefix;
    api.nexthop_num = 1;
    api.nexthops[0].gate = gateway;
    
    // 通过 ZAPI 发送
    zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api);
}
```

**为什么这很重要？**

1. **可移植性**
   - 相同的协议代码在 Linux 和 BSD 上工作
   - 不需要为不同平台维护不同版本

2. **可测试性**
   - 可以 mock Zebra 来测试协议逻辑
   - 不需要真实的内核进行单元测试

3. **可维护性**
   - 内核 API 变化只影响 Zebra
   - 协议守护进程无需修改

### 5.3 ZAPI (Zebra API) 接口

**主要消息类型：**

```c
// 路由操作
ZEBRA_ROUTE_ADD        // 添加路由
ZEBRA_ROUTE_DELETE     // 删除路由
ZEBRA_REDISTRIBUTE_ADD // 订阅路由类型

// 接口操作
ZEBRA_INTERFACE_ADD    // 接口添加通知
ZEBRA_INTERFACE_DELETE // 接口删除通知
ZEBRA_INTERFACE_UP     // 接口 up 通知
ZEBRA_INTERFACE_DOWN   // 接口 down 通知

// 地址操作
ZEBRA_INTERFACE_ADDRESS_ADD    // 地址添加
ZEBRA_INTERFACE_ADDRESS_DELETE // 地址删除

// 邻居操作
ZEBRA_IMPORT_CHECK_UPDATE // 导入检查结果
```

**zapi_route 结构（简化版）：**

```c
struct zapi_route {
    uint8_t type;           // ZEBRA_ROUTE_BGP 等
    uint16_t instance;      // 协议实例
    uint32_t flags;         // 路由标志
    struct prefix prefix;   // 目的前缀
    
    uint16_t nexthop_num;   // 下一跳数量
    struct zapi_nexthop nexthops[MULTIPATH_NUM];
    
    uint32_t metric;        // 路由度量
    route_tag_t tag;        // 路由标签
    uint32_t mtu;           // MTU
    vrf_id_t vrf_id;        // VRF ID
};
```

### 5.4 Dataplane Provider 接口

**为什么有 Dataplane 层？**

```
传统模式：
协议 -> Zebra RIB -> 直接内核调用

问题：
- 内核调用可能阻塞
- 批量操作效率低
- 难以添加其他数据面

Dataplane 模式：
协议 -> Zebra RIB -> Dataplane Queue -> Provider -> 内核

优点：
- 异步操作，不阻塞 RIB 线程
- 可以批量处理
- 可以添加 DPDK/eBPF 等 provider
```

**Provider 接口：**

```c
// 注册 dataplane provider
struct zebra_dplane_provider {
    const char *name;
    int priority;
    
    // 处理函数
    int (*process_func)(struct zebra_dplane_provider *prov);
    
    // 启动/停止
    int (*start_func)(struct zebra_dplane_provider *prov);
    int (*finish_func)(struct zebra_dplane_provider *prov);
};

// 内置 provider
// 1. Kernel provider (Netlink/routing socket)
// 2. FPM provider (Forwarding Plane Manager)
// 3. gRPC provider (可选)
```

### 5.5 外部系统集成点

**合法的集成方式：**

```
方式1：使用 FPM (Forwarding Plane Manager)
+------------------------------------------------+
| FRR Zebra                                      |
|   |                                            |
|   +-- FPM Provider                             |
|       |                                        |
|       +-- TCP connection to external system    |
|           |                                    |
|           v                                    |
|       [Protobuf/Netlink format routes]         |
+------------------------------------------------+

方式2：使用 gRPC 接口
+------------------------------------------------+
| FRR daemons                                    |
|   |                                            |
|   +-- gRPC server                              |
|       |                                        |
|       +-- External client connects             |
|           |                                    |
|           v                                    |
|       [Read/write routes via gRPC]             |
+------------------------------------------------+

方式3：使用 vtysh 脚本接口
+------------------------------------------------+
| 外部脚本                                        |
|   |                                            |
|   +-- vtysh -c "show ip route json"            |
|       |                                        |
|       v                                        |
|   [JSON 格式输出]                              |
+------------------------------------------------+
```

### 5.6 不应该做的事情

**反模式列表：**

```
1. 直接读写 /proc/net/route
   - 这绕过了 FRR，会导致状态不一致

2. 在协议守护进程中使用 Netlink
   - 违反了抽象边界
   - 难以测试和移植

3. 共享内存访问 RIB
   - RIB 结构是内部实现细节
   - 可能随时变化

4. 依赖内核路由表作为真相来源
   - 应该使用 FRR 的 RIB
   - 内核状态可能被外部修改
```

---

## 2. External Integration Points

### 2.1 Legitimate Integration Methods

| Method | Use Case | Interface |
|--------|----------|-----------|
| ZAPI | Protocol daemons | Unix socket to Zebra |
| FPM | External forwarding planes | TCP/Protobuf |
| gRPC | Automation/orchestration | Standard gRPC |
| CLI/vtysh | Human operators, scripts | Text commands |
| SNMP | Network management | MIB queries |

### 2.2 What Should NOT Be Done

- **Direct Netlink from protocol daemons**: Breaks abstraction
- **Reading /proc/net/route**: Bypasses FRR entirely
- **Shared memory with RIB**: Internal structure changes
- **Kernel as source of truth**: External modifications possible

---

## 3. API Stability Guidelines

### 3.1 Stable Interfaces

- ZAPI message types and structures
- CLI command syntax
- FPM message format
- gRPC protobuf definitions

### 3.2 Unstable Interfaces

- Internal RIB structures
- Netlink message building helpers
- Dataplane context internals
- Debug/tracing hooks

---

## Next: Part 6 - REUSE (Lessons)
