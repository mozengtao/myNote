# Zebra Architecture Guide - Part 5: API

## Zebra Client Interface (ZAPI)

```
+============================================================================+
|                    ZAPI ARCHITECTURE                                       |
+============================================================================+

Protocol Daemon                              Zebra Daemon
+------------------+                         +------------------+
|                  |                         |                  |
|  zclient library |                         |  zserv module    |
|  (lib/zclient.c) |                         |  (zebra/zserv.c) |
|                  |                         |                  |
|  +------------+  |    Unix Domain Socket   |  +------------+  |
|  | zclient    |  |     /var/run/frr/       |  | zserv      |  |
|  | struct     |<-|------- zserv.api -------|->| struct     |  |
|  +------------+  |                         |  +------------+  |
|                  |                         |                  |
|  +------------+  |                         |  +------------+  |
|  | stream     |  |   ZAPI Messages         |  | stream     |  |
|  | buffer     |<-|-------------------------|->| buffer     |  |
|  +------------+  |                         |  +------------+  |
|                  |                         |                  |
+------------------+                         +------------------+

ZAPI Message Format:
+------------------------------------------------------------------+
|  0                   1                   2                   3   |
|  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 |
| +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |
| |           Length              |    Marker     |   Version     | |
| +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |
| |                          VRF ID                               | |
| +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |
| |           Command             |             Payload ...       | |
| +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ |
+------------------------------------------------------------------+
```

**中文说明：**

ZAPI 架构图展示了协议进程和 Zebra 守护进程之间的通信机制。协议进程使用 zclient 库（lib/zclient.c）通过 Unix 域套接字（/var/run/frr/zserv.api）与 Zebra 的 zserv 模块（zebra/zserv.c）通信。双方使用 stream buffer 进行消息序列化。ZAPI 消息格式包含 Length（长度）、Marker（标记）、Version（版本）、VRF ID 和 Command（命令）字段，后面跟着 Payload（负载）。

---

## 1. How Protocol Processes Communicate with Zebra

```
+===========================================================================+
|                     ZAPI Client Initialization                            |
+===========================================================================+

BGP Process Startup Example:

+------------------------------------------------------------------+
|                                                                  |
|  bgp_main.c:                                                     |
|                                                                  |
|  int main() {                                                    |
|      ...                                                         |
|      // 1. Create zclient struct                                 |
|      zclient = zclient_new(master, &zclient_options_default,     |
|                            bgp_handlers, array_size(bgp_handlers));|
|                                                                  |
|      // 2. Set callback functions                                |
|      zclient->router_id_update = bgp_router_id_update;           |
|      zclient->interface_add = bgp_interface_add;                 |
|      zclient->nexthop_update = bgp_nexthop_update;               |
|                                                                  |
|      // 3. Start connection                                      |
|      zclient_start(zclient);                                     |
|      ...                                                         |
|  }                                                               |
|                                                                  |
+------------------------------------------------------------------+

Connection Establishment Flow:
+------------------------------------------------------------------+
|                                                                  |
|  zclient_start()                                                 |
|      |                                                           |
|      +---> socket(AF_UNIX, SOCK_STREAM, 0)                       |
|      |                                                           |
|      +---> connect("/var/run/frr/zserv.api")                     |
|      |                                                           |
|      +---> Send ZEBRA_HELLO message                              |
|            +------------------------------------------+          |
|            | proto       = ZEBRA_ROUTE_BGP           |          |
|            | instance    = 0                         |          |
|            | session_id  = random                    |          |
|            | capabilities = ...                      |          |
|            +------------------------------------------+          |
|      |                                                           |
|      +---> Request initial data:                                 |
|            - ZEBRA_INTERFACE_ADD                                 |
|            - ZEBRA_REDISTRIBUTE_ADD (if needed)                  |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

ZAPI 客户端初始化示例展示了 BGP 进程启动过程。在 bgp_main.c 中：(1) 创建 zclient 结构；(2) 设置回调函数（router_id_update、interface_add、nexthop_update）；(3) 启动连接 zclient_start()。连接建立流程：zclient_start() → socket(AF_UNIX, SOCK_STREAM, 0) → connect("/var/run/frr/zserv.api") → 发送 ZEBRA_HELLO 消息（包含 proto=ZEBRA_ROUTE_BGP、instance=0、session_id=随机生成、capabilities）→ 请求初始数据（ZEBRA_INTERFACE_ADD、ZEBRA_REDISTRIBUTE_ADD 如果需要）。

---

## 2. Message Flow Lifecycle

```
+===========================================================================+
|                     Route Add Message Flow                                |
+===========================================================================+

Scenario: BGP learns a new route, needs to install to RIB

Step 1: BGP constructs and sends message
+------------------------------------------------------------------+
|  bgp_zebra.c:                                                    |
|                                                                  |
|  bgp_zebra_announce()                                            |
|      |                                                           |
|      v                                                           |
|  zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api)              |
|      |                                                           |
|      v                                                           |
|  +------------------------------------------------------------+  |
|  | ZAPI Message:                                              |  |
|  | - type: ZEBRA_ROUTE_BGP                                    |  |
|  | - prefix: 10.0.0.0/24                                      |  |
|  | - nexthops: [192.168.1.1, 192.168.1.2]                     |  |
|  | - distance: 20 (eBGP)                                      |  |
|  | - metric: 100                                              |  |
|  | - flags: ZEBRA_FLAG_ALLOW_RECURSION                        |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+

Step 2: Zebra receives and processes
+------------------------------------------------------------------+
|  zserv.c -> zapi_msg.c:                                          |
|                                                                  |
|  zread_route_add()                                               |
|      |                                                           |
|      v                                                           |
|  zapi_route_decode()  // Parse message                           |
|      |                                                           |
|      v                                                           |
|  rib_add_multipath()  // Add to RIB                              |
|      |                                                           |
|      v                                                           |
|  rib_queue_add()      // Queue for processing                    |
|                                                                  |
+------------------------------------------------------------------+

Step 3: RIB processing and FIB installation
+------------------------------------------------------------------+
|  zebra_rib.c:                                                    |
|                                                                  |
|  rib_process()                                                   |
|      |                                                           |
|      +---> Best path selection (if multi-proto same prefix)      |
|      |                                                           |
|      +---> Nexthop resolution                                    |
|      |                                                           |
|      v                                                           |
|  dplane_route_add()   // Push to Dataplane                       |
|      |                                                           |
|      v                                                           |
|  [Kernel installation]                                           |
|      |                                                           |
|      v                                                           |
|  rib_process_result() // Update status                           |
|                                                                  |
+------------------------------------------------------------------+

Step 4: Notify client (if requested)
+------------------------------------------------------------------+
|  If BGP set ZEBRA_FLAG_FPM_NOTIFY:                               |
|                                                                  |
|  zsend_route_notify_owner()                                      |
|      |                                                           |
|      v                                                           |
|  +------------------------------------------------------------+  |
|  | ZEBRA_ROUTE_NOTIFY_OWNER                                   |  |
|  | - status: ZEBRA_ROUTE_INSTALLED                            |  |
|  | - prefix: 10.0.0.0/24                                      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

路由添加消息流展示了 BGP 学到一条新路由需要安装到 RIB 的完整过程。步骤 1：BGP 构造并发送消息 - bgp_zebra_announce() 调用 zclient_route_send() 发送 ZAPI 消息（包含类型、前缀、下一跳、距离、度量值、标志）。步骤 2：Zebra 接收并处理 - zread_route_add() → zapi_route_decode() 解析消息 → rib_add_multipath() 添加到 RIB → rib_queue_add() 入队等待处理。步骤 3：RIB 处理和 FIB 安装 - rib_process() → 最佳路径选择 → 下一跳解析 → dplane_route_add() 推送到 Dataplane → 内核安装 → rib_process_result() 更新状态。步骤 4：通知客户端（如果请求了）- 如果 BGP 设置了 ZEBRA_FLAG_FPM_NOTIFY，则 zsend_route_notify_owner() 发送 ZEBRA_ROUTE_NOTIFY_OWNER 消息。

---

## 3. Core ZAPI Message Types

```
+===========================================================================+
|                     ZAPI Message Types Details                            |
+===========================================================================+

Route Related:
+------------------------------------------------------------------+
| Message                 | Direction  | Purpose                   |
+------------------------------------------------------------------+
| ZEBRA_ROUTE_ADD         | C -> S     | Add route                 |
| ZEBRA_ROUTE_DELETE      | C -> S     | Delete route              |
| ZEBRA_REDISTRIBUTE_ADD  | C -> S     | Request route redistrib   |
| ZEBRA_REDISTRIBUTE_DEL  | C -> S     | Stop route redistrib      |
| ZEBRA_ROUTE_NOTIFY_OWNER| S -> C     | Route install status      |
+------------------------------------------------------------------+

Interface Related:
+------------------------------------------------------------------+
| ZEBRA_INTERFACE_ADD     | S -> C     | Interface add notify      |
| ZEBRA_INTERFACE_DELETE  | S -> C     | Interface delete notify   |
| ZEBRA_INTERFACE_UP      | S -> C     | Interface UP notify       |
| ZEBRA_INTERFACE_DOWN    | S -> C     | Interface DOWN notify     |
| ZEBRA_INTERFACE_ADDRESS_ADD | S -> C | Interface address add     |
| ZEBRA_INTERFACE_ADDRESS_DELETE| S->C | Interface address delete  |
+------------------------------------------------------------------+

Nexthop Tracking (NHT):
+------------------------------------------------------------------+
| ZEBRA_NEXTHOP_REGISTER  | C -> S     | Register nexthop track    |
| ZEBRA_NEXTHOP_UNREGISTER| C -> S     | Unregister nexthop track  |
| ZEBRA_NEXTHOP_UPDATE    | S -> C     | Nexthop state change      |
+------------------------------------------------------------------+

VRF Related:
+------------------------------------------------------------------+
| ZEBRA_VRF_ADD           | S -> C     | VRF add notify            |
| ZEBRA_VRF_DELETE        | S -> C     | VRF delete notify         |
+------------------------------------------------------------------+

Label Related:
+------------------------------------------------------------------+
| ZEBRA_MPLS_LABELS_ADD   | C -> S     | Add MPLS label binding    |
| ZEBRA_MPLS_LABELS_DELETE| C -> S     | Delete MPLS label binding |
+------------------------------------------------------------------+
```

**中文说明：**

ZAPI 消息类型详解。路由相关：ZEBRA_ROUTE_ADD（C→S，添加路由）、ZEBRA_ROUTE_DELETE（C→S，删除路由）、ZEBRA_REDISTRIBUTE_ADD（C→S，请求路由重分发）、ZEBRA_REDISTRIBUTE_DEL（C→S，停止路由重分发）、ZEBRA_ROUTE_NOTIFY_OWNER（S→C，路由安装状态通知）。接口相关：ZEBRA_INTERFACE_ADD/DELETE/UP/DOWN/ADDRESS_ADD/ADDRESS_DELETE（全部 S→C）。下一跳跟踪：ZEBRA_NEXTHOP_REGISTER/UNREGISTER（C→S）、ZEBRA_NEXTHOP_UPDATE（S→C）。VRF 相关：ZEBRA_VRF_ADD/DELETE（S→C）。标签相关：ZEBRA_MPLS_LABELS_ADD/DELETE（C→S）。

---

## 4. zclient Callback Mechanism

```
+===========================================================================+
|                     Callback Function Registration                        |
+===========================================================================+

zclient struct callback function pointers:

struct zclient {
    ...
    // Router ID change
    int (*router_id_update)(ZAPI_CALLBACK_ARGS);

    // Interface events
    int (*interface_add)(ZAPI_CALLBACK_ARGS);
    int (*interface_delete)(ZAPI_CALLBACK_ARGS);
    int (*interface_up)(ZAPI_CALLBACK_ARGS);
    int (*interface_down)(ZAPI_CALLBACK_ARGS);
    int (*interface_address_add)(ZAPI_CALLBACK_ARGS);
    int (*interface_address_delete)(ZAPI_CALLBACK_ARGS);

    // Route redistribution
    int (*redistribute_route_add)(ZAPI_CALLBACK_ARGS);
    int (*redistribute_route_del)(ZAPI_CALLBACK_ARGS);

    // Nexthop update
    int (*nexthop_update)(ZAPI_CALLBACK_ARGS);

    // VRF events
    int (*vrf_add)(ZAPI_CALLBACK_ARGS);
    int (*vrf_delete)(ZAPI_CALLBACK_ARGS);
    ...
};


Callback Trigger Flow:
+------------------------------------------------------------------+
|                                                                  |
|  Zebra sends ZEBRA_NEXTHOP_UPDATE                                |
|      |                                                           |
|      v                                                           |
|  zclient_read() [lib/zclient.c]                                  |
|      |                                                           |
|      +---> Parse message header                                  |
|      |                                                           |
|      +---> Dispatch by command:                                  |
|            switch(command) {                                     |
|                case ZEBRA_NEXTHOP_UPDATE:                        |
|                    if (zclient->nexthop_update)                  |
|                        (*zclient->nexthop_update)(cmd, zclient,  |
|                                                    length, vrf_id);|
|                    break;                                        |
|                ...                                               |
|            }                                                     |
|                                                                  |
+------------------------------------------------------------------+
```

**中文说明：**

回调函数注册机制。zclient 结构包含多个回调函数指针：router_id_update（路由器 ID 变化）、interface_add/delete/up/down（接口事件）、interface_address_add/delete（接口地址事件）、redistribute_route_add/del（路由重分发）、nexthop_update（下一跳更新）、vrf_add/delete（VRF 事件）。回调触发流程：Zebra 发送 ZEBRA_NEXTHOP_UPDATE → zclient_read()（lib/zclient.c）→ 解析消息头 → 根据 command 调度，调用对应的回调函数（如 zclient->nexthop_update）。

---

## 5. APIs External Projects Should NOT Call

```
+===========================================================================+
|                     API Boundary Warning                                  |
+===========================================================================+

DO NOT directly call internal functions:
+------------------------------------------------------------------+
| Function                      | Reason                           |
+------------------------------------------------------------------+
| rib_process()                 | Internal RIB proc, work queue    |
| rib_install_kernel()          | Internal FIB install via dplane  |
| dplane_ctx_*()                | Dataplane context is internal    |
| zserv_send_message()          | Server-side internal function    |
| netlink_route_*()             | Platform-specific internal impl  |
+------------------------------------------------------------------+

DO NOT directly manipulate data structures:
+------------------------------------------------------------------+
| Structure                     | Reason                           |
+------------------------------------------------------------------+
| struct route_entry            | Managed by RIB, don't create     |
| struct rib_dest_t             | Internal destination struct      |
| struct dplane_ctx             | Dataplane internal context       |
| struct zserv                  | Server-side session struct       |
+------------------------------------------------------------------+


RECOMMENDED public APIs:
+------------------------------------------------------------------+
| API                           | Purpose                          |
+------------------------------------------------------------------+
| zclient_new()                 | Create ZAPI client               |
| zclient_start()               | Start connection                 |
| zclient_route_send()          | Send route operation             |
| zclient_send_interface_*()    | Interface-related operations     |
| zclient_send_rnh_*()          | Nexthop tracking operations      |
| zclient_redistribute()        | Route redistribution register    |
+------------------------------------------------------------------+
```

**中文说明：**

API 边界警告。不要直接调用的内部函数：rib_process()（内部 RIB 处理，由工作队列调度）、rib_install_kernel()（内部 FIB 安装，通过 Dataplane）、dplane_ctx_*()（Dataplane 上下文是内部结构）、zserv_send_message()（服务端内部函数）、netlink_route_*()（平台特定内部实现）。不要直接操作的数据结构：route_entry（由 RIB 管理，不要直接创建）、rib_dest_t（内部目的地结构）、dplane_ctx（Dataplane 内部上下文）、zserv（服务端会话结构）。推荐使用的公开 API：zclient_new()（创建 ZAPI 客户端）、zclient_start()（启动连接）、zclient_route_send()（发送路由操作）、zclient_send_interface_*()（接口相关操作）、zclient_send_rnh_*()（下一跳跟踪操作）、zclient_redistribute()（路由重分发注册）。

---

## 6. Custom Protocol Process Implementation Example

```
+===========================================================================+
|                     Minimal ZAPI Client Example                           |
+===========================================================================+

/* my_routing_daemon.c */

#include "zclient.h"

static struct zclient *zclient;

/* Nexthop update callback */
static int my_nexthop_update(ZAPI_CALLBACK_ARGS)
{
    struct zapi_route nhr;

    zapi_nexthop_update_decode(zclient->ibuf, &nhr);

    /* Handle nexthop state change */
    if (nhr.nexthop_num > 0) {
        /* Nexthop reachable */
        my_route_enable(nhr.prefix);
    } else {
        /* Nexthop unreachable */
        my_route_disable(nhr.prefix);
    }

    return 0;
}

/* Interface add callback */
static int my_interface_add(ZAPI_CALLBACK_ARGS)
{
    struct interface *ifp;

    ifp = zebra_interface_add_read(zclient->ibuf, vrf_id);
    /* Handle interface add */

    return 0;
}

/* Initialization */
void my_zebra_init(struct event_loop *master)
{
    /* Create zclient */
    zclient = zclient_new(master, &zclient_options_default,
                          NULL, 0);

    /* Set protocol type */
    zclient_init(zclient, ZEBRA_ROUTE_STATIC, 0,
                 &my_daemon_privs);

    /* Register callbacks */
    zclient->nexthop_update = my_nexthop_update;
    zclient->interface_add = my_interface_add;

    /* Start connection */
    zclient_start(zclient);
}

/* Add route */
void my_add_route(struct prefix *p, struct in_addr *nexthop)
{
    struct zapi_route api;

    memset(&api, 0, sizeof(api));
    api.type = ZEBRA_ROUTE_STATIC;  /* Or custom type */
    api.flags = 0;
    api.message = 0;
    api.prefix = *p;

    SET_FLAG(api.message, ZAPI_MESSAGE_NEXTHOP);
    api.nexthop_num = 1;
    api.nexthops[0].type = NEXTHOP_TYPE_IPV4;
    api.nexthops[0].gate.ipv4 = *nexthop;

    zclient_route_send(ZEBRA_ROUTE_ADD, zclient, &api);
}

/* Delete route */
void my_delete_route(struct prefix *p)
{
    struct zapi_route api;

    memset(&api, 0, sizeof(api));
    api.type = ZEBRA_ROUTE_STATIC;
    api.prefix = *p;

    zclient_route_send(ZEBRA_ROUTE_DELETE, zclient, &api);
}

/* Register nexthop tracking */
void my_track_nexthop(struct prefix *nh)
{
    zclient_send_rnh(zclient, ZEBRA_NEXTHOP_REGISTER, nh,
                     SAFI_UNICAST, false, false, VRF_DEFAULT);
}
```

**中文说明：**

最小 ZAPI 客户端示例展示了如何实现自定义路由进程。包括：(1) 下一跳更新回调 my_nexthop_update() - 解码下一跳更新，根据下一跳是否可达启用或禁用路由；(2) 接口添加回调 my_interface_add() - 处理接口添加事件；(3) 初始化函数 my_zebra_init() - 创建 zclient、设置协议类型、注册回调、启动连接；(4) 添加路由函数 my_add_route() - 构造 zapi_route 结构并调用 zclient_route_send()；(5) 删除路由函数 my_delete_route()；(6) 注册下一跳跟踪函数 my_track_nexthop()。

---

## 7. ZAPI Version Compatibility

```
+===========================================================================+
|                     Version Notes                                         |
+===========================================================================+

ZAPI Protocol Versions:
+------------------------------------------------------------------+
| Version | FRR Version    | Major Changes                         |
+------------------------------------------------------------------+
| 6       | FRR 7.x+       | Current stable version                |
| 5       | FRR 5.x-6.x    | Added VRF support                     |
| 4       | FRR 3.x-4.x    | MPLS enhancements                     |
+------------------------------------------------------------------+

Version Negotiation:
- ZEBRA_HELLO message contains version info
- Zebra can reject incompatible clients
- Always use latest ZAPI version recommended

Compatibility Recommendations:
1. Use wrapper functions from lib/zclient.c
2. Don't construct ZAPI messages directly
3. Check zclient->sock to confirm connection status
4. Handle ZEBRA_ERROR messages
```

**中文说明：**

ZAPI 版本兼容性说明。ZAPI 协议版本：版本 6（FRR 7.x+，当前稳定版本）、版本 5（FRR 5.x-6.x，添加了 VRF 支持）、版本 4（FRR 3.x-4.x，MPLS 增强）。版本协商：ZEBRA_HELLO 消息中包含版本信息，Zebra 可以拒绝不兼容的客户端，建议始终使用最新 ZAPI 版本。兼容性建议：(1) 使用 lib/zclient.c 提供的封装函数；(2) 不要直接构造 ZAPI 消息；(3) 检查 zclient->sock 确认连接状态；(4) 处理 ZEBRA_ERROR 消息。
