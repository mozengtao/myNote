# FRR Kernel Interaction Architecture - Part 3: WHAT

## Structures and Messages

This document details the concrete data structures and message formats
used in FRR's kernel interaction layer.

---

## 1. Netlink Message Architecture

```
+===========================================================================+
|                    NETLINK MESSAGE STRUCTURE                              |
+===========================================================================+

                    BASIC MESSAGE FORMAT
                    =====================

  +------------------------------------------------------------------+
  |                    NLMSGHDR (16 bytes)                           |
  +------------------------------------------------------------------+
  | nlmsg_len    | Total message length (including header)          |
  | nlmsg_type   | Message type (RTM_NEWROUTE, RTM_DELLINK, etc.)   |
  | nlmsg_flags  | Flags (NLM_F_REQUEST, NLM_F_ACK, etc.)           |
  | nlmsg_seq    | Sequence number (for matching responses)         |
  | nlmsg_pid    | Sender's port ID                                  |
  +------------------------------------------------------------------+
  |                                                                  |
  |                    PROTOCOL HEADER                               |
  |         (rtmsg, ifinfomsg, ndmsg, etc.)                         |
  |                                                                  |
  +------------------------------------------------------------------+
  |                                                                  |
  |                    ATTRIBUTES (rtattr)                           |
  |         Variable length, TLV encoded                            |
  |                                                                  |
  +------------------------------------------------------------------+


                    ROUTE MESSAGE STRUCTURE
                    =======================

  +------------------------------------------------------------------+
  |                    NLMSGHDR                                      |
  +------------------------------------------------------------------+
  | nlmsg_type = RTM_NEWROUTE | RTM_DELROUTE | RTM_GETROUTE          |
  +------------------------------------------------------------------+
  |                    RTMSG (12 bytes)                              |
  +------------------------------------------------------------------+
  | rtm_family   | Address family (AF_INET, AF_INET6, AF_MPLS)      |
  | rtm_dst_len  | Destination prefix length                        |
  | rtm_src_len  | Source prefix length (policy routing)            |
  | rtm_tos      | Type of Service                                  |
  | rtm_table    | Routing table ID                                  |
  | rtm_protocol | Protocol that installed route (RTPROT_*)         |
  | rtm_scope    | Distance to destination                           |
  | rtm_type     | Route type (RTN_UNICAST, RTN_BLACKHOLE, etc.)    |
  | rtm_flags    | Flags (RTM_F_NOTIFY, etc.)                        |
  +------------------------------------------------------------------+
  |                    ROUTE ATTRIBUTES                              |
  +------------------------------------------------------------------+
  | RTA_DST      | Destination address                               |
  | RTA_GATEWAY  | Gateway/next-hop address                          |
  | RTA_OIF      | Output interface index                            |
  | RTA_PREFSRC  | Preferred source address                          |
  | RTA_TABLE    | Routing table (>255)                              |
  | RTA_PRIORITY | Route priority/metric                             |
  | RTA_MULTIPATH| Multiple next-hops (ECMP)                         |
  | RTA_ENCAP    | Encapsulation info (MPLS, SRv6)                   |
  | RTA_NH_ID    | Nexthop object ID                                 |
  +------------------------------------------------------------------+


                    MULTIPATH STRUCTURE
                    ===================

  +------------------------------------------------------------------+
  |                    RTA_MULTIPATH                                 |
  +------------------------------------------------------------------+
  |  +------------------------------------------------------------+  |
  |  | RTNEXTHOP #1                                               |  |
  |  +------------------------------------------------------------+  |
  |  | rtnh_len      | Length of this nexthop entry               |  |
  |  | rtnh_flags    | Flags (RTNH_F_ONLINK, RTNH_F_LINKDOWN)     |  |
  |  | rtnh_hops     | Weight (for weighted ECMP)                 |  |
  |  | rtnh_ifindex  | Output interface                           |  |
  |  +------------------------------------------------------------+  |
  |  | Nested attributes:                                         |  |
  |  | - RTA_GATEWAY (next-hop address)                           |  |
  |  | - RTA_ENCAP   (MPLS label, etc.)                           |  |
  |  +------------------------------------------------------------+  |
  |                                                                  |
  |  +------------------------------------------------------------+  |
  |  | RTNEXTHOP #2                                               |  |
  |  +------------------------------------------------------------+  |
  |  | ... (same structure)                                       |  |
  |  +------------------------------------------------------------+  |
  +------------------------------------------------------------------+


+===========================================================================+
|                    NEXTHOP OBJECT MESSAGE                                 |
+===========================================================================+

  +------------------------------------------------------------------+
  |                    NLMSGHDR                                      |
  +------------------------------------------------------------------+
  | nlmsg_type = RTM_NEWNEXTHOP | RTM_DELNEXTHOP | RTM_GETNEXTHOP    |
  +------------------------------------------------------------------+
  |                    NHMSG (4 bytes)                               |
  +------------------------------------------------------------------+
  | nh_family    | Address family                                    |
  | nh_scope     | Scope (unused)                                    |
  | nh_protocol  | Protocol that created nexthop                     |
  | nh_flags     | Flags (RTNH_F_ONLINK)                             |
  +------------------------------------------------------------------+
  |                    NEXTHOP ATTRIBUTES                            |
  +------------------------------------------------------------------+
  | NHA_ID       | Unique nexthop ID                                 |
  | NHA_GROUP    | Group of nexthop IDs                              |
  | NHA_OIF      | Output interface                                  |
  | NHA_GATEWAY  | Gateway address                                   |
  | NHA_BLACKHOLE| Blackhole nexthop                                 |
  | NHA_ENCAP    | Encapsulation (MPLS, etc.)                        |
  | NHA_RES_GROUP| Resilient hashing group                           |
  +------------------------------------------------------------------+


+===========================================================================+
|                    LINK MESSAGE STRUCTURE                                 |
+===========================================================================+

  +------------------------------------------------------------------+
  |                    NLMSGHDR                                      |
  +------------------------------------------------------------------+
  | nlmsg_type = RTM_NEWLINK | RTM_DELLINK | RTM_GETLINK             |
  +------------------------------------------------------------------+
  |                    IFINFOMSG                                     |
  +------------------------------------------------------------------+
  | ifi_family   | Address family (usually AF_UNSPEC)                |
  | ifi_type     | Device type (ARPHRD_*)                            |
  | ifi_index    | Interface index                                   |
  | ifi_flags    | IFF_UP, IFF_RUNNING, etc.                        |
  | ifi_change   | Change mask                                       |
  +------------------------------------------------------------------+
  |                    LINK ATTRIBUTES                               |
  +------------------------------------------------------------------+
  | IFLA_IFNAME  | Interface name                                    |
  | IFLA_ADDRESS | Hardware address                                  |
  | IFLA_MTU     | MTU size                                          |
  | IFLA_LINK    | Link layer interface                              |
  | IFLA_MASTER  | Master interface (bridge, bond)                   |
  | IFLA_LINKINFO| Link type specific info                           |
  +------------------------------------------------------------------+


+===========================================================================+
|                    VRF AND NAMESPACE HANDLING                             |
+===========================================================================+

                    VRF ARCHITECTURE
                    ================

  +------------------------------------------------------------------+
  |                    DEFAULT VRF (VRF_DEFAULT)                     |
  |                                                                  |
  |  Netlink Socket -----> Kernel Default Network Namespace          |
  |  Table ID: RT_TABLE_MAIN (254)                                   |
  +------------------------------------------------------------------+
            |
            | ns_socket() with ns_id
            v
  +------------------------------------------------------------------+
  |                    VRF (e.g., "customer-a")                      |
  |                                                                  |
  |  Option A: VRF Lite (same namespace, different table)           |
  |  +---------------------------------------------------------+    |
  |  | - Same netlink socket                                    |    |
  |  | - RTA_TABLE = VRF's table ID                            |    |
  |  | - VRF device links interfaces                           |    |
  |  +---------------------------------------------------------+    |
  |                                                                  |
  |  Option B: VRF Backend Netns (separate namespace)               |
  |  +---------------------------------------------------------+    |
  |  | - Separate netlink socket per namespace                  |    |
  |  | - Full isolation                                         |    |
  |  | - ns_socket(AF_NETLINK, ..., ns_id)                     |    |
  |  +---------------------------------------------------------+    |
  +------------------------------------------------------------------+


                    TABLE ID HANDLING
                    =================

  +------------------------------------------------------------------+
  | Table ID Range        | Usage                                    |
  +------------------------------------------------------------------+
  | 0 (RT_TABLE_UNSPEC)   | Unspecified                              |
  | 253 (RT_TABLE_DEFAULT)| Default routing table                    |
  | 254 (RT_TABLE_MAIN)   | Main routing table                       |
  | 255 (RT_TABLE_LOCAL)  | Local routes (auto-managed)              |
  | 1-252                 | User-defined tables                      |
  | 256+                  | Extended tables (via RTA_TABLE)          |
  +------------------------------------------------------------------+

  When table ID > 255:
  - rtm_table = RT_TABLE_UNSPEC
  - Add RTA_TABLE attribute with actual ID
```

---

## 中文解释 (Chinese Explanation)

### 3.1 Netlink 消息生命周期

**消息发送流程：**

```
步骤1：构造消息
+------------------------------------------+
| 分配缓冲区                                |
| 填充 nlmsghdr                            |
|   - 设置 nlmsg_type (RTM_NEWROUTE)       |
|   - 设置 nlmsg_flags (NLM_F_REQUEST|ACK) |
|   - 设置 nlmsg_seq (递增序列号)          |
+------------------------------------------+
            |
            v
+------------------------------------------+
| 填充协议头 (rtmsg)                       |
|   - 设置 rtm_family (AF_INET/AF_INET6)   |
|   - 设置 rtm_dst_len (前缀长度)          |
|   - 设置 rtm_protocol (RTPROT_ZEBRA)     |
+------------------------------------------+
            |
            v
+------------------------------------------+
| 添加属性 (rtattr)                        |
|   - RTA_DST: 目的地址                    |
|   - RTA_GATEWAY: 下一跳                  |
|   - RTA_OIF: 出接口                      |
|   - 更新 nlmsg_len                       |
+------------------------------------------+
            |
            v
步骤2：发送消息
+------------------------------------------+
| sendmsg(nl->sock, &msg, 0)               |
| 消息发送到内核                           |
+------------------------------------------+
            |
            v
步骤3：等待响应
+------------------------------------------+
| recvmsg(nl->sock, &msg, 0)               |
| 接收 NLMSG_ERROR 消息                    |
|   - err->error == 0: 成功 (ACK)          |
|   - err->error < 0: 失败 (errno)         |
+------------------------------------------+
```

### 3.2 FRR 中的关键结构

**struct nlsock - Netlink 套接字封装：**

```c
struct nlsock {
    int sock;              // 套接字文件描述符
    struct sockaddr_nl snl; // Netlink 地址
    char *name;            // 套接字名称（用于日志）
    char *buf;             // 接收缓冲区
    size_t buflen;         // 缓冲区长度
};
```

**struct nl_batch - 批量处理上下文：**

```c
struct nl_batch {
    void *buf;             // 消息缓冲区
    size_t bufsiz;         // 缓冲区大小
    size_t limit;          // 发送阈值
    void *buf_head;        // 当前写入位置
    size_t curlen;         // 当前已使用长度
    size_t msgcnt;         // 消息计数
    struct dplane_ctx_list_head ctx_list;  // 关联的上下文
};
```

### 3.3 路由、链路、地址对象

**路由对象 (Route)：**

```
组成部分：
1. 目的地 (Destination)
   - 网络前缀: 10.0.0.0/8
   - 地址族: IPv4 或 IPv6

2. 下一跳 (Nexthop)
   - 单个下一跳: 网关地址 + 出接口
   - 多路径 (ECMP): 多个下一跳，带权重

3. 元数据
   - 协议 (protocol): 谁安装的路由
   - 优先级 (priority): 路由选择顺序
   - 表 ID (table): 属于哪个路由表

FRR 使用 RTPROT_ZEBRA 标识自己安装的路由，
可以在 "ip route" 输出中看到 "proto zebra"。
```

**链路对象 (Link/Interface)：**

```
关键属性：
1. 索引 (ifindex): 内核分配的唯一标识
2. 名称 (ifname): 如 eth0, vlan100
3. 状态标志:
   - IFF_UP: 管理状态开启
   - IFF_RUNNING: 链路状态正常
   - IFF_LOWER_UP: 物理层正常
4. 类型 (type): 以太网、VLAN、VRF等
5. MTU: 最大传输单元
```

**地址对象 (Address)：**

```
关联到接口：
- 一个接口可以有多个地址
- 地址变化会触发路由重算

属性：
- 地址族 (family): IPv4/IPv6
- 前缀长度 (prefixlen): 子网掩码
- 范围 (scope): global, link, host
```

### 3.4 VRF 和命名空间处理

**VRF 实现方式：**

```
方式1：VRF Lite（推荐）
+--------------------------------------------+
| 优点：                                      |
| - 单个 netlink 套接字                       |
| - 简单的表 ID 隔离                          |
| - 低开销                                    |
|                                             |
| 工作原理：                                   |
| - 创建 VRF 设备（类型 = vrf）               |
| - 将接口绑定到 VRF 设备                      |
| - 路由通过 RTA_TABLE 属性指定表 ID           |
+--------------------------------------------+

方式2：VRF Backend Netns
+--------------------------------------------+
| 优点：                                      |
| - 完全隔离                                  |
| - 独立的网络栈                              |
|                                             |
| 工作原理：                                   |
| - 每个 VRF 对应一个网络命名空间              |
| - 每个命名空间有独立的 netlink 套接字        |
| - 通过 ns_socket() 在特定命名空间创建套接字  |
+--------------------------------------------+
```

**表 ID 处理：**

```c
// 表 ID 小于 256 时
rtm->rtm_table = table_id;

// 表 ID 大于等于 256 时
rtm->rtm_table = RT_TABLE_UNSPEC;
nl_attr_put32(&req->n, sizeof(req), RTA_TABLE, table_id);
```

### 3.5 属性编码辅助函数

**FRR 提供的属性操作函数：**

```c
// 添加属性
bool nl_attr_put(struct nlmsghdr *n, unsigned int maxlen,
                 int type, const void *data, unsigned int alen);

// 添加固定大小属性
bool nl_attr_put32(struct nlmsghdr *n, unsigned int maxlen,
                   int type, uint32_t data);

// 开始嵌套属性
struct rtattr *nl_attr_nest(struct nlmsghdr *n,
                            unsigned int maxlen, int type);

// 结束嵌套属性
int nl_attr_nest_end(struct nlmsghdr *n, struct rtattr *nest);

// 解析属性
void netlink_parse_rtattr(struct rtattr **tb, int max,
                          struct rtattr *rta, int len);
```

### 3.6 协议标识符映射

**RTPROT 和 ZEBRA_ROUTE 的映射：**

```c
// zebra2proto(): FRR 类型 -> 内核类型
ZEBRA_ROUTE_BGP    -> RTPROT_BGP
ZEBRA_ROUTE_OSPF   -> RTPROT_OSPF
ZEBRA_ROUTE_STATIC -> RTPROT_ZSTATIC
ZEBRA_ROUTE_KERNEL -> RTPROT_KERNEL

// proto2zebra(): 内核类型 -> FRR 类型
RTPROT_BGP         -> ZEBRA_ROUTE_BGP
RTPROT_OSPF        -> ZEBRA_ROUTE_OSPF
RTPROT_KERNEL      -> ZEBRA_ROUTE_KERNEL
```

这个映射允许：
1. FRR 识别自己安装的路由
2. 内核路由（如 connected）被正确分类
3. 调试时可以追踪路由来源

---

## 2. Message Type Summary

### 2.1 Route Messages

| Type | Direction | Purpose |
|------|-----------|---------|
| RTM_NEWROUTE | To kernel | Install route |
| RTM_DELROUTE | To kernel | Delete route |
| RTM_GETROUTE | To kernel | Query route (with NLM_F_DUMP for all) |
| RTM_NEWROUTE | From kernel | Route added notification |
| RTM_DELROUTE | From kernel | Route deleted notification |

### 2.2 Nexthop Messages

| Type | Direction | Purpose |
|------|-----------|---------|
| RTM_NEWNEXTHOP | To kernel | Create nexthop object |
| RTM_DELNEXTHOP | To kernel | Delete nexthop object |
| RTM_GETNEXTHOP | To kernel | Query nexthops |

### 2.3 Link Messages

| Type | Direction | Purpose |
|------|-----------|---------|
| RTM_NEWLINK | To kernel | Create/modify interface |
| RTM_DELLINK | To kernel | Delete interface |
| RTM_GETLINK | To kernel | Query interfaces |
| RTM_NEWLINK | From kernel | Interface added/changed |
| RTM_DELLINK | From kernel | Interface removed |

---

## Next: Part 4 - WHERE (Code Reading Guide)
