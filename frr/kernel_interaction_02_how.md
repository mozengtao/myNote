# FRR Kernel Interaction Architecture - Part 2: HOW

## Netlink Strategy

This document explains FRR's design philosophy for interacting with the
Linux kernel via Netlink sockets.

---

## 1. Netlink Communication Architecture

```
+===========================================================================+
|                    FRR NETLINK SOCKET ARCHITECTURE                        |
+===========================================================================+

+---------------------------------------------------------------------------+
|                           ZEBRA DAEMON                                    |
+---------------------------------------------------------------------------+
|                                                                           |
|  +---------------------------+     +---------------------------+          |
|  |    MAIN PTHREAD           |     |    DATAPLANE PTHREAD       |         |
|  |                           |     |                           |          |
|  |  +---------------------+  |     |  +---------------------+  |          |
|  |  |  Event Loop         |  |     |  |  Event Loop         |  |          |
|  |  |  (kernel_read)      |  |     |  |  (dplane_thread)    |  |          |
|  |  +---------------------+  |     |  +---------------------+  |          |
|  |           |               |     |           |               |          |
|  |           v               |     |           v               |          |
|  |  +---------------------+  |     |  +---------------------+  |          |
|  |  |  netlink (listen)   |  |     |  |  netlink_dplane     |  |          |
|  |  |  - Receive events   |  |     |  |  - Send commands    |  |          |
|  |  |  - Route changes    |  |     |  |  - Batch updates    |  |          |
|  |  |  - Nexthop changes  |  |     |  |  - Async I/O        |  |          |
|  |  +---------------------+  |     |  +---------------------+  |          |
|  |           |               |     |           |               |          |
|  +-----------|---------------+     +-----------|---------------+          |
|              |                                 |                          |
|              v                                 v                          |
|  +-----------+---------------------------------+-----------+              |
|  |                NETLINK SOCKET LAYER                     |              |
|  |                                                         |              |
|  |   +-----------------+         +-----------------+       |              |
|  |   | netlink.sock    |         | netlink_cmd.sock|       |              |
|  |   | (Listener)      |         | (Command)       |       |              |
|  |   | Groups:         |         | Sync ops with   |       |              |
|  |   | - RTNLGRP_LINK  |         | ACK required    |       |              |
|  |   | - RTNLGRP_IPV4_*|         +-----------------+       |              |
|  |   | - RTNLGRP_IPV6_*|                                   |              |
|  |   | - RTNLGRP_NEIGH |                                   |              |
|  |   +-----------------+                                   |              |
|  +---------------------------------------------------------+              |
+---------------------------------------------------------------------------+
                                    |
                                    | AF_NETLINK
                                    v
+---------------------------------------------------------------------------+
|                         LINUX KERNEL                                      |
|                                                                           |
|  +-------------------+  +-------------------+  +-------------------+      |
|  |  Route Subsystem  |  |  Link Subsystem   |  |  Neigh Subsystem  |      |
|  |  RTM_*ROUTE       |  |  RTM_*LINK        |  |  RTM_*NEIGH       |      |
|  +-------------------+  +-------------------+  +-------------------+      |
+---------------------------------------------------------------------------+


+===========================================================================+
|                    FULL TABLE SYNC vs INCREMENTAL                         |
+===========================================================================+

                           STARTUP SEQUENCE
                           ================

  +------------------------+
  |   Zebra Starts         |
  +------------------------+
            |
            v
  +------------------------+     RTM_GETLINK (NLM_F_DUMP)
  | Request Interface Dump |----------------------------------+
  +------------------------+                                  |
            |                                                 |
            v                                                 |
  +------------------------+     RTM_GETADDR (NLM_F_DUMP)     |
  | Request Address Dump   |----------------------------------+
  +------------------------+                                  |
            |                                                 |
            v                                                 |
  +------------------------+     RTM_GETROUTE (NLM_F_DUMP)    |
  | Request Route Dump     |----------------------------------+
  +------------------------+                                  |
            |                                                 |
            v                                                 |
  +------------------------+     RTM_GETNEXTHOP (NLM_F_DUMP)  |
  | Request Nexthop Dump   |----------------------------------+
  +------------------------+                                  |
            |                                                 v
            |                              +------------------------+
            |                              |   Kernel responds      |
            |                              |   with full tables     |
            +----------------------------->|   (multipart msgs)     |
                                           +------------------------+
                                                      |
                                                      v
                                           +------------------------+
                                           | Zebra processes each   |
                                           | entry, builds RIB      |
                                           +------------------------+


                         RUNTIME (INCREMENTAL)
                         =====================

  +------------------------+
  | Kernel Event Occurs    |
  | (link up/down, route   |
  |  change, etc.)         |
  +------------------------+
            |
            | Netlink broadcast to subscribed sockets
            v
  +------------------------+
  | Zebra Listener Socket  |
  | receives RTM_* message |
  +------------------------+
            |
            v
  +------------------------+
  | Dispatch to handler:   |
  | - netlink_route_change |
  | - netlink_link_change  |
  | - netlink_neigh_change |
  +------------------------+
            |
            v
  +------------------------+
  | Update RIB/Interface   |
  | state accordingly      |
  +------------------------+


+===========================================================================+
|                    ERROR HANDLING PHILOSOPHY                              |
+===========================================================================+

                    NETLINK ERROR CLASSIFICATION
                    ============================

  +-------------------------+
  | Receive NLMSG_ERROR     |
  +-------------------------+
            |
            v
  +-------------------------+
  | err->error == 0 ?       |----YES----> ACK (Success)
  +-------------------------+
            |
           NO
            v
  +----------------------------+
  |  Classify Error:           |
  |                            |
  |  IGNORABLE:                |
  |  +-----------------------+ |
  |  | ENODEV  (no device)   | |  ---> Log debug, continue
  |  | ESRCH   (not found)   | |
  |  | EEXIST  (exists)      | |
  |  | ENETDOWN (net down)   | |
  |  +-----------------------+ |
  |                            |
  |  EXPECTED:                 |
  |  +-----------------------+ |
  |  | EOPNOTSUPP (tunnel)   | |  ---> Log notice, continue
  |  +-----------------------+ |
  |                            |
  |  SERIOUS:                  |
  |  +-----------------------+ |
  |  | EPERM (permission)    | |  ---> Log error, may need action
  |  | ENOMEM (memory)       | |
  |  | EINVAL (invalid)      | |
  |  +-----------------------+ |
  +----------------------------+


                    BATCHING STRATEGY
                    =================

  +----------------------------------------------------------------+
  |                   NETLINK BATCH BUFFER                         |
  |                                                                |
  |  +--------+  +--------+  +--------+  +--------+  +--------+   |
  |  | MSG 1  |  | MSG 2  |  | MSG 3  |  | MSG 4  |  | MSG 5  |   |
  |  |RTM_NEW |  |RTM_DEL |  |RTM_NEW |  |RTM_NEW |  |RTM_DEL |   |
  |  |ROUTE   |  |ROUTE   |  |NEXTHOP |  |ROUTE   |  |ROUTE   |   |
  |  +--------+  +--------+  +--------+  +--------+  +--------+   |
  |                                                                |
  |  Buffer Size: NL_DEFAULT_BATCH_BUFSIZE (16 * 8192 bytes)      |
  |  Send Threshold: NL_DEFAULT_BATCH_SEND_THRESHOLD              |
  +----------------------------------------------------------------+
            |
            | When threshold reached or flush requested
            v
  +----------------------------------------------------------------+
  |                   SINGLE sendmsg() CALL                        |
  |  - All messages sent atomically                                |
  |  - Kernel processes in order                                   |
  |  - Errors returned per-message                                 |
  +----------------------------------------------------------------+
```

---

## 中文解释 (Chinese Explanation)

### 2.1 双套接字架构

**为什么需要两个 Netlink 套接字？**

FRR 使用两个独立的 Netlink 套接字，各有不同用途：

1. **监听套接字 (Listener Socket)**
   ```
   用途：接收内核广播的事件通知
   - 订阅 RTNLGRP_LINK：接口状态变化
   - 订阅 RTNLGRP_IPV4_ROUTE：IPv4 路由变化
   - 订阅 RTNLGRP_IPV6_ROUTE：IPv6 路由变化
   - 订阅 RTNLGRP_NEIGH：邻居表变化
   
   特点：
   - 异步接收，事件驱动
   - 不发送命令，只接收通知
   - 在主 pthread 中处理
   ```

2. **命令套接字 (Command Socket)**
   ```
   用途：向内核发送配置命令
   - 安装/删除路由
   - 创建/删除 nexthop 对象
   - 配置接口参数
   
   特点：
   - 同步操作，等待 ACK
   - 可以批量发送
   - 在 dataplane pthread 中处理
   ```

### 2.2 全量同步 vs 增量更新

**启动时全量同步：**

```
启动序列：
1. 请求接口列表 (RTM_GETLINK + NLM_F_DUMP)
   - 获取所有网络接口
   - 建立接口索引映射

2. 请求地址列表 (RTM_GETADDR + NLM_F_DUMP)
   - 获取所有 IP 地址
   - 关联到接口

3. 请求路由列表 (RTM_GETROUTE + NLM_F_DUMP)
   - 获取所有路由
   - 识别 FRR 自己安装的路由（通过 RTPROT）

4. 请求 Nexthop 列表 (RTM_GETNEXTHOP + NLM_F_DUMP)
   - 如果内核支持 nexthop 对象
   - 获取已安装的 nexthop 组
```

**运行时增量更新：**

```
监听事件：
- RTM_NEWROUTE / RTM_DELROUTE：路由变化
- RTM_NEWLINK / RTM_DELLINK：接口变化
- RTM_NEWADDR / RTM_DELADDR：地址变化
- RTM_NEWNEIGH / RTM_DELNEIGH：邻居变化

处理流程：
1. 接收 Netlink 消息
2. 解析消息类型和内容
3. 调用相应处理函数
4. 更新内部状态
5. 通知相关订阅者
```

### 2.3 错误处理哲学

**可忽略的错误：**

| 错误码 | 含义 | 处理方式 |
|--------|------|----------|
| ENODEV | 设备不存在 | 设备可能正在被删除，忽略 |
| ESRCH | 找不到条目 | 条目可能已被删除，忽略 |
| EEXIST | 条目已存在 | 可能是重复安装，忽略 |
| ENETDOWN | 网络不可达 | 临时状态，忽略 |

**为什么这样设计？**

1. **竞态条件是正常的**
   - FRR 删除路由时，内核可能已经因为接口 down 删除了
   - FRR 安装路由时，另一个进程可能刚安装了相同路由

2. **幂等性设计**
   - 安装已存在的路由 = 成功
   - 删除不存在的路由 = 成功
   - 这简化了错误处理逻辑

3. **容错优于正确**
   - 宁可多安装一次，不可漏装
   - 调和机制会清理多余条目

### 2.4 批量处理策略

**为什么要批量发送？**

```
场景：BGP 收到 10000 条路由更新

方案A - 逐条发送：
for each route:
    sendmsg(route)    # 10000 次系统调用
    recvmsg(ack)      # 10000 次系统调用
总计：20000 次系统调用

方案B - 批量发送：
batch = []
for each route:
    batch.append(route)
    if batch.size > threshold:
        sendmsg(batch)  # ~80 次系统调用
        batch = []
总计：~160 次系统调用
```

**FRR 的批量实现：**

```c
// 缓冲区配置
#define NL_DEFAULT_BATCH_BUFSIZE       (16 * 8192)  // 128KB
#define NL_DEFAULT_BATCH_SEND_THRESHOLD (15 * 8192) // 120KB

// 批量处理流程：
1. 将 Netlink 消息追加到缓冲区
2. 当达到阈值或需要刷新时，一次性发送
3. 处理每条消息的响应
4. 清空缓冲区，继续下一批
```

### 2.5 能力检测

**运行时能力检测：**

```c
// 检测内核是否支持 nexthop 对象
static bool supports_nh;

// 在启动时通过尝试操作来检测
if (RTM_GETNEXTHOP 操作成功) {
    supports_nh = true;
} else if (errno == EOPNOTSUPP) {
    supports_nh = false;
    // 回退到传统路由安装方式
}
```

**适配不同内核版本：**

| 功能 | 最低内核版本 | 回退策略 |
|------|--------------|----------|
| Nexthop 对象 | 5.3+ | 使用传统多路径路由 |
| MPLS | 4.1+ | 禁用 MPLS 功能 |
| VRF | 4.3+ | 使用 namespace 隔离 |
| 扩展 ACK | 4.12+ | 使用基本错误信息 |

### 2.6 消息过滤

**BPF 过滤器：**

FRR 在监听套接字上安装 BPF 过滤器：

```c
// 过滤逻辑：
if (nlmsg_pid == frr_pid || nlmsg_pid == dplane_pid) {
    // 这是我们自己发送的消息的回显
    if (type == RTM_NEWADDR || type == RTM_DELADDR ||
        type == RTM_NEWNETCONF || type == RTM_DELNETCONF) {
        // 保留这些消息（我们需要处理地址变化）
        return KEEP;
    }
    return DROP;  // 其他消息丢弃，避免重复处理
} else {
    return KEEP;  // 其他来源的消息，保留
}
```

这避免了处理自己发送命令导致的内核广播通知。

---

## 2. Key Design Decisions

### 2.1 Separation of Concerns

| Thread | Socket | Purpose |
|--------|--------|---------|
| Main | netlink (listener) | Event reception |
| Dataplane | netlink_cmd | Command execution |
| Dataplane | netlink_dplane | Async operations |

### 2.2 Message Acknowledgment

FRR requests ACKs for all command messages:

```c
req.n.nlmsg_flags = NLM_F_CREATE | NLM_F_REQUEST | NLM_F_ACK;
```

This ensures:
- Confirmation of successful installation
- Immediate error detection
- Proper sequencing of operations

### 2.3 Extended ACK Support

Modern kernels provide detailed error information:

```c
if (h->nlmsg_flags & NLM_F_ACK_TLVS)
    netlink_parse_extended_ack(h);
```

This includes:
- Human-readable error messages
- Offset to the offending attribute
- Additional context for debugging

---

## Next: Part 3 - WHAT (Structures and Messages)
