# FRR Architecture Guide - Part 5: Using FRR Libraries in Real Projects

## ASCII Architecture Overview

```
+==============================================================================+
|              USING FRR LIBRARIES IN REAL PROJECTS                            |
+==============================================================================+

                    WHEN TO USE FRR AS A LIBRARY
+-----------------------------------------------------------------------------+
|                                                                             |
|   Use Case 1: EMBEDDING ROUTING LOGIC                                       |
|   ======================================                                    |
|                                                                             |
|   +-------------------------+                                               |
|   |   Your Application      |                                               |
|   |   +-----------------+   |                                               |
|   |   |   libfrr.so     |   |  <- Link against FRR library                 |
|   |   | - event loop    |   |                                               |
|   |   | - prefix utils  |   |                                               |
|   |   | - memory mgmt   |   |                                               |
|   |   +-----------------+   |                                               |
|   +-------------------------+                                               |
|                                                                             |
+-----------------------------------------------------------------------------+

   Use Case 2: CUSTOM ROUTING APPLIANCE
   =====================================

+-----------------------------------------------------------------------------+
|                                                                             |
|   +-----------------------------------------------------------------------+ |
|   |                    Custom Routing Platform                            | |
|   +-----------------------------------------------------------------------+ |
|   |                                                                       | |
|   |   +-------------+  +-------------+  +-------------+                   | |
|   |   | Your Custom |  | Standard    |  | Standard    |                   | |
|   |   | Protocol    |  | bgpd        |  | ospfd       |                   | |
|   |   +------+------+  +------+------+  +------+------+                   | |
|   |          |                |                |                          | |
|   |          +----------------+----------------+                          | |
|   |                           |                                           | |
|   |                           v                                           | |
|   |                    +-------------+                                    | |
|   |                    |    zebra    |                                    | |
|   |                    +-------------+                                    | |
|   |                                                                       | |
|   +-----------------------------------------------------------------------+ |
|                                                                             |
+-----------------------------------------------------------------------------+

   Use Case 3: PROTOCOL EXTENSION/PROTOTYPING
   ===========================================

+-----------------------------------------------------------------------------+
|                                                                             |
|   +-----------------------------------------------------------------------+ |
|   |                    Research/Extension Project                         | |
|   +-----------------------------------------------------------------------+ |
|   |                                                                       | |
|   |   Fork bgpd/ and extend:                                              | |
|   |                                                                       | |
|   |   +-------------------+                                               | |
|   |   | bgpd_extended/    |                                               | |
|   |   |                   |                                               | |
|   |   | + new_feature.c   |  <- Your new BGP extension                    | |
|   |   | + custom_attr.c   |  <- Custom path attributes                    | |
|   |   |                   |                                               | |
|   |   +-------------------+                                               | |
|   |           |                                                           | |
|   |           | Uses existing                                             | |
|   |           v                                                           | |
|   |   +-------------------+                                               | |
|   |   |      lib/         |                                               | |
|   |   | (unchanged)       |                                               | |
|   |   +-------------------+                                               | |
|   |                                                                       | |
|   +-----------------------------------------------------------------------+ |
|                                                                             |
+-----------------------------------------------------------------------------+

                    HOW TO SAFELY DEPEND ON FRR
+-----------------------------------------------------------------------------+
|                                                                             |
|   LINKING ARCHITECTURE                                                      |
|   ====================                                                      |
|                                                                             |
|   Your Application                                                          |
|   +----------------------------------+                                      |
|   |  main.c                          |                                      |
|   |  your_code.c                     |                                      |
|   +----------------------------------+                                      |
|              |                                                              |
|              | Link against                                                 |
|              v                                                              |
|   +----------------------------------+                                      |
|   |  libfrr.so                       |  <- Shared library                  |
|   |  (installed by FRR)              |                                      |
|   +----------------------------------+                                      |
|              |                                                              |
|              | Includes                                                     |
|              v                                                              |
|   +----------------------------------+                                      |
|   |  /usr/include/frr/               |  <- Headers                         |
|   |  - frrevent.h                    |                                      |
|   |  - prefix.h                      |                                      |
|   |  - memory.h                      |                                      |
|   |  - zclient.h                     |                                      |
|   +----------------------------------+                                      |
|                                                                             |
|   STABLE APIs                         UNSTABLE (Avoid)                      |
|   ===========                         =================                     |
|   - lib/frrevent.h                    - *_internal.h files                 |
|   - lib/prefix.h                      - Static functions                   |
|   - lib/memory.h                      - Struct internal fields             |
|   - lib/stream.h                      - Daemon-specific code               |
|   - lib/zclient.h (basic)             - Undocumented functions             |
|                                                                             |
+-----------------------------------------------------------------------------+

                    INTEGRATION PATTERNS
+-----------------------------------------------------------------------------+
|                                                                             |
|   Pattern 1: EXTERNAL CONTROL APPLICATION                                   |
|   =========================================                                 |
|                                                                             |
|   +-------------------+                 +-------------------+               |
|   | Control App       |    gRPC/YANG    |  FRR Daemons      |               |
|   | (Python/Go/etc)   |---------------->|  (bgpd, etc)      |               |
|   +-------------------+                 +-------------------+               |
|                                                                             |
|   - Uses FRR's northbound API                                               |
|   - No C code required                                                      |
|   - Best for operational tools                                              |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|   Pattern 2: CUSTOM DAEMON USING FRR LIB                                    |
|   ========================================                                  |
|                                                                             |
|   +-------------------+                                                     |
|   | Your Daemon       |                                                     |
|   | +--------------+  |                                                     |
|   | | Uses:        |  |                                                     |
|   | | - event loop |  |                                                     |
|   | | - logging    |  |                                                     |
|   | | - memory     |  |                                                     |
|   | | - zclient    |  |                                                     |
|   | +--------------+  |                                                     |
|   +--------+----------+                                                     |
|            |                                                                |
|            | ZAPI                                                           |
|            v                                                                |
|   +-------------------+                                                     |
|   |      zebra        |                                                     |
|   +-------------------+                                                     |
|                                                                             |
|   - Links against libfrr.so                                                 |
|   - Registers as Zebra client                                               |
|   - Can inject/receive routes                                               |
|                                                                             |
+-----------------------------------------------------------------------------+
|                                                                             |
|   Pattern 3: POLICY ENGINE ABOVE FRR                                        |
|   ===================================                                       |
|                                                                             |
|   +-------------------+                                                     |
|   | SDN Controller    |                                                     |
|   +--------+----------+                                                     |
|            |                                                                |
|            | Policy decisions                                               |
|            v                                                                |
|   +-------------------+                                                     |
|   | Policy Translator |  <- Your code                                      |
|   +--------+----------+                                                     |
|            |                                                                |
|            | route-maps, prefix-lists                                       |
|            v                                                                |
|   +-------------------+                                                     |
|   |  FRR (bgpd, etc)  |                                                     |
|   +-------------------+                                                     |
|                                                                             |
|   - Configures FRR via CLI/YANG                                             |
|   - FRR handles protocol details                                            |
|   - Clean separation of concerns                                            |
|                                                                             |
+-----------------------------------------------------------------------------+
```

---

## 中文详细说明

### 1. 何时应该使用 FRR 作为库？

#### 适合的场景

```
适合使用 FRR 库的场景：
+==================================================================+
|  场景                      |  原因                                |
+==================================================================+
|  嵌入路由逻辑到自定义系统  |  复用成熟的事件循环和工具            |
|  构建定制路由设备          |  不需要重新实现协议栈                |
|  原型化新协议或扩展        |  利用现有基础设施                    |
|  网络设备模拟器            |  获得真实的协议行为                  |
|  SDN 控制器的本地代理      |  与内核路由交互                      |
+==================================================================+
```

#### 不适合的场景

```
不适合使用 FRR 的场景：
+==================================================================+
|  场景                      |  更好的选择                          |
+==================================================================+
|  简单的路由查询            |  直接使用 Netlink                    |
|  纯用户空间网络处理        |  考虑 DPDK 或 XDP                    |
|  极度资源受限的嵌入式      |  考虑更轻量的实现                    |
|  非 IP 路由域              |  FRR 专注于 IP 路由                  |
+==================================================================+
```

---

### 2. 如何安全地依赖 FRR

#### 链接到 libfrr

```makefile
# Makefile 示例
CFLAGS += $(shell pkg-config --cflags frr)
LDFLAGS += $(shell pkg-config --libs frr)

my_daemon: my_daemon.o
    $(CC) -o $@ $^ $(LDFLAGS)
```

#### 使用稳定 API

```c
/* 示例：使用 FRR 事件循环 */
#include <frrevent.h>
#include <memory.h>
#include <log.h>

struct event_loop *master;

/* 定义内存类型 */
DEFINE_MTYPE_STATIC(MY_DAEMON, MY_DATA, "My data structure");

void my_callback(struct event *event)
{
    struct my_data *data = EVENT_ARG(event);
    /* 处理事件 */
}

int main(int argc, char **argv)
{
    /* 初始化 FRR 库 */
    frr_preinit(&my_di, argc, argv);
    
    /* 创建事件循环 */
    master = event_master_create("my_daemon");
    
    /* 注册事件 */
    struct my_data *data = XMALLOC(MTYPE_MY_DATA, sizeof(*data));
    event_add_timer(master, my_callback, data, 10, &data->timer);
    
    /* 运行事件循环 */
    struct event event;
    while (event_fetch(master, &event))
        event_call(&event);
    
    return 0;
}
```

#### 避免内部符号

```c
/* 好的做法 - 使用公共 API */
#include <prefix.h>

struct prefix p;
str2prefix("10.0.0.0/24", &p);

/* 不好的做法 - 使用内部函数 */
// 不要使用带 _internal 后缀的函数
// 不要直接访问结构体的私有字段
```

---

### 3. 建议的集成模式

#### 模式 1：外部控制应用

```python
# Python 示例 - 使用 FRR 的 gRPC 接口
import grpc
from frr_northbound_pb2 import *
from frr_northbound_pb2_grpc import *

def configure_bgp_neighbor(stub, neighbor_ip, remote_as):
    """通过 gRPC 配置 BGP 邻居"""
    
    path = f"/frr-routing:routing/control-plane-protocols/" \
           f"control-plane-protocol[type='frr-bgp:bgp'][name='default']/" \
           f"frr-bgp:bgp/neighbors/neighbor[remote-address='{neighbor_ip}']"
    
    request = EditConfigRequest(
        operation=EDIT_CONFIG_OPERATION_CREATE,
        path=path,
        data=f"<remote-as>{remote_as}</remote-as>"
    )
    
    response = stub.EditConfig(request)
    return response
```

**优点：**
- 不需要 C 代码
- 与 FRR 版本解耦
- 适合运维工具和 SDN 控制器

#### 模式 2：自定义守护进程使用 FRR 库

```c
/* 自定义路由守护进程示例 */

#include <zebra.h>
#include <frrevent.h>
#include <zclient.h>
#include <prefix.h>

struct zclient *zclient;

/* Zebra 连接回调 */
void zebra_connected(struct zclient *zclient)
{
    zlog_info("Connected to Zebra");
    
    /* 注册感兴趣的路由类型 */
    zclient_send_reg_requests(zclient, VRF_DEFAULT);
}

/* 路由更新回调 */
int route_notify(ZAPI_CALLBACK_ARGS)
{
    struct zapi_route api;
    char buf[PREFIX_STRLEN];
    
    if (zapi_route_decode(zclient->ibuf, &api) < 0)
        return -1;
    
    prefix2str(&api.prefix, buf, sizeof(buf));
    zlog_info("Route update: %s", buf);
    
    return 0;
}

int main(int argc, char **argv)
{
    struct event_loop *master;
    
    /* 初始化 */
    frr_preinit(&my_di, argc, argv);
    master = frr_init();
    
    /* 创建 Zebra 客户端 */
    zclient = zclient_new(master, &zclient_options_default,
                          my_handlers, sizeof(my_handlers));
    zclient->zebra_connected = zebra_connected;
    zclient->redistribute_route_add = route_notify;
    
    /* 连接到 Zebra */
    zclient_init(zclient, ZEBRA_ROUTE_MY_PROTOCOL, 0, 
                 &my_privs);
    
    /* 运行 */
    frr_run(master);
    
    return 0;
}
```

**使用场景：**
- 自定义路由协议
- 路由监控代理
- 策略执行点

#### 模式 3：策略引擎叠加在 FRR 之上

```
架构：
+==================================================================+
|                                                                  |
|  +------------------+                                            |
|  | SDN Controller   |  (高层策略决策)                            |
|  +--------+---------+                                            |
|           |                                                      |
|           | Intent/Policy                                        |
|           v                                                      |
|  +------------------+                                            |
|  | Policy Engine    |  (策略翻译层)                              |
|  | - 解析意图       |                                            |
|  | - 生成配置       |                                            |
|  +--------+---------+                                            |
|           |                                                      |
|           | CLI/YANG/gRPC                                        |
|           v                                                      |
|  +------------------+                                            |
|  |       FRR        |  (协议实现)                                |
|  | - BGP/OSPF/IS-IS |                                            |
|  | - 路由计算       |                                            |
|  +------------------+                                            |
|                                                                  |
+==================================================================+
```

---

### 4. 常见陷阱

```
必须避免的陷阱：
+==================================================================+

陷阱 1：将 FRR 视为简单的路由库
---------------------------------
错误认知：FRR 只是一个可以调用的函数库
正确理解：FRR 是一套完整的守护进程系统，有自己的生命周期

陷阱 2：不理解 IPC 就修改守护进程内部
--------------------------------------
错误做法：直接修改 bgpd 内部数据结构期望影响其他进程
正确做法：通过 ZAPI 或配置接口进行通信

陷阱 3：忽略 VRF 和命名空间
----------------------------
错误假设：所有路由都在默认 VRF
正确做法：始终考虑 VRF ID 和网络命名空间

陷阱 4：假设内核状态等于 FRR 状态
---------------------------------
错误假设：读取 /proc/net/route 就能获得完整路由状态
正确做法：FRR 的 RIB 可能包含尚未安装的路由

+==================================================================+
```

---

### 5. 反模式

```
必须避免的反模式：
+==================================================================+

反模式 1：混合控制平面逻辑到数据平面代码
------------------------------------------
错误：在 XDP/DPDK 数据路径中调用 FRR 函数
正确：控制平面和数据平面应该通过明确接口通信

反模式 2：绕过 Zebra
---------------------
错误：直接从 bgpd 调用 Netlink 安装路由
正确：所有路由安装必须通过 Zebra

反模式 3：硬编码协议行为
------------------------
错误：在代码中硬编码 BGP 策略
正确：使用 route-map 和配置实现策略

反模式 4：将路由视为同步操作
-----------------------------
错误：期望 zclient_route_send() 返回时路由已安装
正确：路由安装是异步的，需要等待通知

+==================================================================+
```

---

### 6. 示例：最小 FRR 客户端

```c
/* minimal_frr_client.c - 最小 FRR 客户端示例 */

#include <zebra.h>
#include <frrevent.h>
#include <log.h>
#include <prefix.h>
#include <zclient.h>
#include <privs.h>

/* 守护进程信息 */
static struct frr_daemon_info my_di = {
    .name = "my_client",
    .vty_port = 0,
    .proghelp = "Minimal FRR Client Example",
};

/* 权限（通常需要 root） */
static struct zebra_privs_t my_privs = {
    .user = NULL,
    .group = NULL,
    .vty_group = NULL,
};

/* Zebra 客户端 */
static struct zclient *zclient;

/* 连接回调 */
static void zebra_connected(struct zclient *zc)
{
    zlog_info("Connected to Zebra!");
    
    /* 可以在这里请求路由重分发等 */
}

/* 接口更新回调 */
static int interface_add(ZAPI_CALLBACK_ARGS)
{
    struct interface *ifp;
    
    ifp = zebra_interface_add_read(zclient->ibuf, vrf_id);
    if (ifp)
        zlog_info("Interface added: %s", ifp->name);
    
    return 0;
}

/* 回调处理表 */
static zclient_handler *const my_handlers[] = {
    [ZEBRA_INTERFACE_ADD] = interface_add,
};

int main(int argc, char **argv)
{
    struct event_loop *master;
    
    /* 初始化 */
    frr_preinit(&my_di, argc, argv);
    master = frr_init();
    
    /* 创建 Zebra 客户端 */
    zclient = zclient_new(master, &zclient_options_default,
                          my_handlers, array_size(my_handlers));
    zclient->zebra_connected = zebra_connected;
    
    /* 连接到 Zebra */
    zclient_init(zclient, ZEBRA_ROUTE_SYSTEM, 0, &my_privs);
    
    /* 运行事件循环 */
    frr_run(master);
    
    return 0;
}
```

**编译：**
```bash
gcc -o my_client minimal_frr_client.c \
    $(pkg-config --cflags --libs frr)
```

**运行：**
```bash
# 确保 zebra 正在运行
sudo ./my_client
```
