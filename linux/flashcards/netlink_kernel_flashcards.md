# Netlink Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] 什么是Netlink？它解决了什么问题？
A: **Netlink**是Linux内核与用户空间进程之间的双向通信机制。

解决的问题：
- **替代ioctl**：提供更灵活、异步的内核-用户空间通信
- **异步通知**：内核可主动通知用户空间事件
- **组播支持**：一对多消息分发
- **可靠传输**：基于socket，支持确认和重传

```
用户空间                         内核空间
+-------------+                 +-------------+
|   iproute2  | <-- Netlink --> |  rtnetlink  |
+-------------+                 +-------------+
|   iptables  | <-- Netlink --> |  nfnetlink  |
+-------------+                 +-------------+
|    udevd    | <-- Netlink --> | uevent/kobject|
+-------------+                 +-------------+
```

Q: [Basic] Netlink与其他内核-用户空间通信机制的比较？
A: 各种通信机制的特点对比：
```
+------------+----------+----------+----------+----------+
|   机制     | 双向通信 | 异步通知 | 组播支持 | 大数据量 |
+------------+----------+----------+----------+----------+
| ioctl      |    是    |    否    |    否    |    差    |
| procfs     |    是    |    否    |    否    |    差    |
| sysfs      |    是    |    否    |    否    |    差    |
| Netlink    |    是    |    是    |    是    |    好    |
| 字符设备   |    是    |    是    |    否    |    好    |
+------------+----------+----------+----------+----------+
```

Netlink优势：
- 支持多播（一个消息发给多个接收者）
- 内核可主动发消息
- 面向消息（有边界）
- 支持dump（批量获取）

Q: [Basic] Linux内核定义了哪些Netlink协议家族？
A: 内核预定义的Netlink协议类型：
```c
/* include/linux/netlink.h */
#define NETLINK_ROUTE        0   // 路由/设备/地址管理
#define NETLINK_USERSOCK     2   // 用户自定义
#define NETLINK_FIREWALL     3   // iptables (已废弃)
#define NETLINK_INET_DIAG    4   // socket诊断
#define NETLINK_NFLOG        5   // Netfilter日志
#define NETLINK_XFRM         6   // IPsec策略
#define NETLINK_SELINUX      7   // SELinux事件
#define NETLINK_ISCSI        8   // iSCSI管理
#define NETLINK_AUDIT        9   // 审计子系统
#define NETLINK_FIB_LOOKUP  10   // 路由查找
#define NETLINK_CONNECTOR   11   // 内核连接器
#define NETLINK_NETFILTER   12   // Netfilter子系统
#define NETLINK_KOBJECT_UEVENT 15  // 设备热插拔(udev)
#define NETLINK_GENERIC     16   // Generic Netlink
#define NETLINK_CRYPTO      21   // 加密子系统

#define MAX_LINKS           32   // 最大协议数
```

Q: [Intermediate] Netlink Socket地址结构是什么？
A: Netlink使用sockaddr_nl结构标识端点：
```c
/* include/linux/netlink.h */
struct sockaddr_nl {
    __kernel_sa_family_t nl_family;  // AF_NETLINK
    unsigned short nl_pad;           // 填充，必须为0
    __u32 nl_pid;                    // 端口ID(port ID)
    __u32 nl_groups;                 // 组播组掩码
};

/* nl_pid说明 */
// 内核端：nl_pid = 0
// 用户端：通常为进程PID，但可以是任何唯一值
// 多个socket：可用 pid | (socket_id << 16)

/* nl_groups说明 */
// 位掩码，表示订阅的组播组
// 组1 = 0x01, 组2 = 0x02, 组3 = 0x04 ...
```

---

## 消息格式 (Message Format)

Q: [Basic] Netlink消息的基本格式是什么？
A: Netlink消息由消息头和负载组成：
```
+------------------+------------------+------------------+
|   nlmsghdr       |   Payload        |   Padding        |
| (16 bytes)       |   (variable)     |   (0-3 bytes)    |
+------------------+------------------+------------------+
|<------------ NLMSG_ALIGN ---------->|

/* struct nlmsghdr */
struct nlmsghdr {
    __u32 nlmsg_len;    // 包含头部的总长度
    __u16 nlmsg_type;   // 消息类型
    __u16 nlmsg_flags;  // 标志位
    __u32 nlmsg_seq;    // 序列号
    __u32 nlmsg_pid;    // 发送者的端口ID
};

/* 常用宏 */
NLMSG_ALIGN(len)     // 对齐到4字节边界
NLMSG_HDRLEN         // 头部长度(16)
NLMSG_LENGTH(len)    // 头部 + 负载长度
NLMSG_SPACE(len)     // 对齐后的总空间
NLMSG_DATA(nlh)      // 获取负载指针
NLMSG_NEXT(nlh,len)  // 获取下一条消息
NLMSG_OK(nlh,len)    // 检查消息有效性
```

Q: [Basic] nlmsghdr中的nlmsg_type和nlmsg_flags常用值？
A: 消息类型和标志的定义：
```c
/* 通用消息类型 (< NLMSG_MIN_TYPE = 0x10) */
#define NLMSG_NOOP      0x1  // 空操作
#define NLMSG_ERROR     0x2  // 错误响应
#define NLMSG_DONE      0x3  // dump结束标记
#define NLMSG_OVERRUN   0x4  // 数据丢失

/* 请求标志 */
#define NLM_F_REQUEST   0x01  // 请求消息
#define NLM_F_MULTI     0x02  // 多部分消息
#define NLM_F_ACK       0x04  // 请求确认
#define NLM_F_ECHO      0x08  // 回显请求

/* GET请求修饰符 */
#define NLM_F_ROOT      0x100  // 返回完整表
#define NLM_F_MATCH     0x200  // 返回匹配项
#define NLM_F_DUMP      (NLM_F_ROOT|NLM_F_MATCH)

/* NEW请求修饰符 */
#define NLM_F_REPLACE   0x100  // 替换已存在的
#define NLM_F_EXCL      0x200  // 不存在才创建
#define NLM_F_CREATE    0x400  // 不存在则创建
#define NLM_F_APPEND    0x800  // 追加到列表末尾
```

Q: [Intermediate] Netlink属性(nlattr)的格式是什么？
A: Netlink属性使用TLV(Type-Length-Value)格式：
```c
/*
 *  <-- NLA_HDRLEN --> <-- NLA_ALIGN(payload) -->
 * +-----------------+------------------------+-----+
 * |    nlattr头     |        负载            | Pad |
 * +-----------------+------------------------+-----+
 *  <------------ nla_len ------------------>
 */

struct nlattr {
    __u16 nla_len;   // 包含头部的总长度
    __u16 nla_type;  // 属性类型
    /* 紧跟着是属性值 */
};

/* nla_type的高2位有特殊含义 */
#define NLA_F_NESTED       (1 << 15)  // 包含嵌套属性
#define NLA_F_NET_BYTEORDER (1 << 14) // 网络字节序
#define NLA_TYPE_MASK      ~(NLA_F_NESTED | NLA_F_NET_BYTEORDER)

/* 常用宏 */
#define NLA_ALIGNTO         4
#define NLA_ALIGN(len)      (((len) + NLA_ALIGNTO - 1) & ~(NLA_ALIGNTO - 1))
#define NLA_HDRLEN          ((int) NLA_ALIGN(sizeof(struct nlattr)))
```

Q: [Intermediate] 如何定义属性验证策略(nla_policy)？
A: nla_policy用于自动验证属性：
```c
/* include/net/netlink.h */
struct nla_policy {
    u16 type;  // 属性类型
    u16 len;   // 长度限制
};

/* 支持的属性类型 */
enum {
    NLA_UNSPEC,     // 未指定类型
    NLA_U8,         // 8位无符号
    NLA_U16,        // 16位无符号
    NLA_U32,        // 32位无符号
    NLA_U64,        // 64位无符号
    NLA_STRING,     // 字符串(可不含NUL)
    NLA_NUL_STRING, // NUL结尾字符串
    NLA_FLAG,       // 布尔标志(无值)
    NLA_MSECS,      // 毫秒时间
    NLA_NESTED,     // 嵌套属性
    NLA_BINARY,     // 二进制数据
};

/* 定义策略示例 */
static const struct nla_policy my_policy[MY_ATTR_MAX + 1] = {
    [MY_ATTR_NAME]   = { .type = NLA_NUL_STRING, .len = 32 },
    [MY_ATTR_VALUE]  = { .type = NLA_U32 },
    [MY_ATTR_FLAGS]  = { .type = NLA_FLAG },
    [MY_ATTR_DATA]   = { .type = NLA_BINARY, .len = 1024 },
    [MY_ATTR_NESTED] = { .type = NLA_NESTED },
};
```

Q: [Intermediate] Netlink错误消息的格式是什么？
A: 错误响应使用nlmsgerr结构：
```c
/* include/linux/netlink.h */
struct nlmsgerr {
    int error;           // 负值错误码或0表示ACK
    struct nlmsghdr msg; // 导致错误的原始消息头
};

/* 错误消息格式 */
+----------------+----------------+------------------+
|   nlmsghdr     |   nlmsgerr     |  (可选扩展属性)  |
| type=NLMSG_ERROR|  error + msg   |                  |
+----------------+----------------+------------------+

/* 内核发送错误响应 */
void netlink_ack(struct sk_buff *in_skb, 
                 struct nlmsghdr *nlh, 
                 int err);

/* 用户空间检查错误 */
if (nlh->nlmsg_type == NLMSG_ERROR) {
    struct nlmsgerr *err = NLMSG_DATA(nlh);
    if (err->error < 0) {
        fprintf(stderr, "Error: %s\n", 
                strerror(-err->error));
    }
}
```

---

## 内核API (Kernel API)

Q: [Intermediate] 如何在内核中创建Netlink socket？
A: 使用netlink_kernel_create()创建：
```c
/* include/linux/netlink.h */
struct sock *netlink_kernel_create(
    struct net *net,           // 网络命名空间
    int unit,                  // 协议类型(NETLINK_*)
    unsigned int groups,       // 组播组数量
    void (*input)(struct sk_buff *skb),  // 接收回调
    struct mutex *cb_mutex,    // 回调互斥锁(可为NULL)
    struct module *module      // 所属模块
);

/* 示例：创建自定义Netlink socket */
static struct sock *nl_sock;

static void nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh = nlmsg_hdr(skb);
    /* 处理消息 */
}

static int __init my_init(void)
{
    nl_sock = netlink_kernel_create(&init_net,
                                    NETLINK_USERSOCK,
                                    0,          // groups
                                    nl_recv_msg,
                                    NULL,       // mutex
                                    THIS_MODULE);
    if (!nl_sock)
        return -ENOMEM;
    return 0;
}

static void __exit my_exit(void)
{
    netlink_kernel_release(nl_sock);
}
```

Q: [Intermediate] 内核如何发送Netlink消息到用户空间？
A: 使用netlink_unicast或netlink_broadcast：
```c
/* 单播 - 发送给指定进程 */
int netlink_unicast(struct sock *ssk,      // 内核socket
                    struct sk_buff *skb,    // 消息
                    __u32 pid,              // 目标端口ID
                    int nonblock);          // 非阻塞标志

/* 组播 - 发送给组内所有进程 */
int netlink_broadcast(struct sock *ssk,
                      struct sk_buff *skb,
                      __u32 pid,            // 排除的PID(0=不排除)
                      __u32 group,          // 目标组播组
                      gfp_t allocation);

/* 完整示例 */
static int send_to_user(int pid, void *data, int len)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    int ret;

    /* 1. 分配skb */
    skb = nlmsg_new(len, GFP_KERNEL);
    if (!skb)
        return -ENOMEM;

    /* 2. 填充消息头 */
    nlh = nlmsg_put(skb, 0, 0, MY_MSG_TYPE, len, 0);
    if (!nlh) {
        kfree_skb(skb);
        return -EMSGSIZE;
    }

    /* 3. 复制数据 */
    memcpy(nlmsg_data(nlh), data, len);

    /* 4. 发送 */
    ret = netlink_unicast(nl_sock, skb, pid, MSG_DONTWAIT);
    return ret < 0 ? ret : 0;
}
```

Q: [Intermediate] netlink_rcv_skb辅助函数如何使用？
A: netlink_rcv_skb简化消息接收处理：
```c
/* net/netlink/af_netlink.c */
int netlink_rcv_skb(struct sk_buff *skb,
                    int (*cb)(struct sk_buff *, struct nlmsghdr *));

/* 它自动处理：
 * 1. 遍历skb中的所有消息
 * 2. 检查消息有效性
 * 3. 过滤非请求消息
 * 4. 调用回调处理每条消息
 * 5. 自动发送ACK(如果请求了)
 */

/* 使用示例 */
static int my_msg_handler(struct sk_buff *skb, struct nlmsghdr *nlh)
{
    /* nlh已验证，可以安全使用 */
    switch (nlh->nlmsg_type) {
    case MY_MSG_GET:
        return handle_get(skb, nlh);
    case MY_MSG_SET:
        return handle_set(skb, nlh);
    default:
        return -EOPNOTSUPP;
    }
}

static void nl_input(struct sk_buff *skb)
{
    netlink_rcv_skb(skb, my_msg_handler);
}
```

Q: [Intermediate] 如何实现Netlink dump操作？
A: dump用于批量获取数据，使用回调机制：
```c
/* 启动dump */
int netlink_dump_start(struct sock *ssk,
                       struct sk_buff *skb,
                       const struct nlmsghdr *nlh,
                       int (*dump)(struct sk_buff *skb,
                                   struct netlink_callback *cb),
                       int (*done)(struct netlink_callback *cb),
                       u16 min_dump_alloc);

/* netlink_callback结构 */
struct netlink_callback {
    struct sk_buff *skb;
    const struct nlmsghdr *nlh;
    int (*dump)(struct sk_buff *skb, struct netlink_callback *cb);
    int (*done)(struct netlink_callback *cb);
    long args[6];          // 保存状态
};

/* dump回调示例 */
static int my_dump(struct sk_buff *skb, struct netlink_callback *cb)
{
    int idx = cb->args[0];  // 上次位置
    struct my_entry *entry;
    
    list_for_each_entry(entry, &my_list, list) {
        if (idx-- > 0)
            continue;  // 跳过已dump的
        
        /* 添加一条记录 */
        if (nla_put_xxx(skb, ...) < 0)
            break;  // skb满了
        
        cb->args[0]++;  // 更新位置
    }
    return skb->len;  // 返回数据长度
}

static int my_done(struct netlink_callback *cb)
{
    /* 清理资源 */
    return 0;
}
```

---

## 消息构建辅助函数 (Message Building Helpers)

Q: [Intermediate] nlmsg_*系列函数如何使用？
A: nlmsg_*函数用于构建和解析消息：
```c
/* 创建新skb */
struct sk_buff *nlmsg_new(size_t payload, gfp_t flags);

/* 添加消息头 */
struct nlmsghdr *nlmsg_put(struct sk_buff *skb,
                           u32 pid, u32 seq, int type,
                           int payload, int flags);

/* 结束消息 */
int nlmsg_end(struct sk_buff *skb, struct nlmsghdr *nlh);

/* 取消消息 */
void nlmsg_cancel(struct sk_buff *skb, struct nlmsghdr *nlh);

/* 释放skb */
void nlmsg_free(struct sk_buff *skb);

/* 获取负载指针 */
void *nlmsg_data(const struct nlmsghdr *nlh);

/* 获取负载长度 */
int nlmsg_len(const struct nlmsghdr *nlh);

/* 遍历消息 */
nlmsg_for_each_msg(nlh, head, len, rem) {
    /* 处理每条消息 */
}

/* 构建消息示例 */
struct sk_buff *build_reply(u32 pid, u32 seq, void *data, int len)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;

    skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!skb)
        return NULL;

    nlh = nlmsg_put(skb, pid, seq, MY_MSG_TYPE, len, 0);
    if (!nlh) {
        nlmsg_free(skb);
        return NULL;
    }

    memcpy(nlmsg_data(nlh), data, len);
    nlmsg_end(skb, nlh);

    return skb;
}
```

Q: [Intermediate] nla_*系列函数如何使用？
A: nla_*函数用于处理属性：
```c
/* 添加属性 */
int nla_put(struct sk_buff *skb, int attrtype, int len, const void *data);
int nla_put_u8(struct sk_buff *skb, int attrtype, u8 value);
int nla_put_u16(struct sk_buff *skb, int attrtype, u16 value);
int nla_put_u32(struct sk_buff *skb, int attrtype, u32 value);
int nla_put_u64(struct sk_buff *skb, int attrtype, u64 value);
int nla_put_string(struct sk_buff *skb, int attrtype, const char *str);
int nla_put_flag(struct sk_buff *skb, int attrtype);

/* 解析属性 */
int nla_parse(struct nlattr **tb, int maxtype,
              const struct nlattr *head, int len,
              const struct nla_policy *policy);

/* 获取属性值 */
void *nla_data(const struct nlattr *nla);
int nla_len(const struct nlattr *nla);
int nla_type(const struct nlattr *nla);
u8 nla_get_u8(const struct nlattr *nla);
u16 nla_get_u16(const struct nlattr *nla);
u32 nla_get_u32(const struct nlattr *nla);
u64 nla_get_u64(const struct nlattr *nla);

/* 遍历属性 */
nla_for_each_attr(nla, head, len, rem) {
    /* 处理每个属性 */
}

/* 嵌套属性 */
struct nlattr *nla_nest_start(struct sk_buff *skb, int attrtype);
int nla_nest_end(struct sk_buff *skb, struct nlattr *start);
void nla_nest_cancel(struct sk_buff *skb, struct nlattr *start);
int nla_parse_nested(struct nlattr *tb[], int maxtype,
                     const struct nlattr *nla,
                     const struct nla_policy *policy);
```

Q: [Intermediate] 如何构建带属性的Netlink消息？
A: 完整的消息构建示例：
```c
/* 消息格式：
 * +----------+----------+--------+--------+--------+
 * | nlmsghdr | 协议头   | attr1  | attr2  | ...    |
 * +----------+----------+--------+--------+--------+
 */

static int build_info_msg(struct sk_buff *skb, u32 pid, u32 seq,
                          struct my_info *info)
{
    struct nlmsghdr *nlh;
    struct my_header *hdr;
    struct nlattr *nest;

    /* 1. 添加netlink消息头 */
    nlh = nlmsg_put(skb, pid, seq, MY_MSG_INFO, 
                    sizeof(*hdr), NLM_F_MULTI);
    if (!nlh)
        return -EMSGSIZE;

    /* 2. 添加协议头 */
    hdr = nlmsg_data(nlh);
    hdr->version = 1;
    hdr->type = info->type;

    /* 3. 添加属性 */
    if (nla_put_string(skb, MY_ATTR_NAME, info->name))
        goto nla_put_failure;
    if (nla_put_u32(skb, MY_ATTR_ID, info->id))
        goto nla_put_failure;
    if (nla_put_u64(skb, MY_ATTR_SIZE, info->size))
        goto nla_put_failure;

    /* 4. 添加嵌套属性 */
    nest = nla_nest_start(skb, MY_ATTR_STATS);
    if (!nest)
        goto nla_put_failure;
    if (nla_put_u64(skb, MY_STAT_RX, info->rx_bytes))
        goto nla_put_failure;
    if (nla_put_u64(skb, MY_STAT_TX, info->tx_bytes))
        goto nla_put_failure;
    nla_nest_end(skb, nest);

    /* 5. 结束消息 */
    return nlmsg_end(skb, nlh);

nla_put_failure:
    nlmsg_cancel(skb, nlh);
    return -EMSGSIZE;
}
```

---

## Generic Netlink (GENL)

Q: [Basic] 什么是Generic Netlink？为什么需要它？
A: Generic Netlink是Netlink的扩展层：
```
传统Netlink问题：
- 协议号有限(MAX_LINKS=32)
- 新子系统需要分配新协议号
- 需要修改内核头文件

Generic Netlink解决方案：
- 所有子系统共享NETLINK_GENERIC
- 动态分配Family ID
- 通过字符串名称注册

+------------------+
|   用户空间应用   |
+--------+---------+
         |
    Generic Netlink (NETLINK_GENERIC=16)
         |
+--------+---------+
| nl80211 | taskstats | ... |  (Families)
+---------+-----------+-----+
```

Q: [Intermediate] struct genl_family如何定义？
A: genl_family定义一个Generic Netlink家族：
```c
/* include/net/genetlink.h */
struct genl_family {
    unsigned int    id;         // 动态分配的ID
    unsigned int    hdrsize;    // 用户头大小
    char            name[GENL_NAMSIZ]; // 家族名称
    unsigned int    version;    // 协议版本
    unsigned int    maxattr;    // 最大属性ID
    bool            netnsok;    // 是否支持网络命名空间
    
    /* 可选的前后置回调 */
    int (*pre_doit)(struct genl_ops *ops,
                    struct sk_buff *skb,
                    struct genl_info *info);
    void (*post_doit)(struct genl_ops *ops,
                      struct sk_buff *skb,
                      struct genl_info *info);
    
    /* 内部使用 */
    struct nlattr **attrbuf;
    struct list_head ops_list;
    struct list_head family_list;
    struct list_head mcast_groups;
};

/* 定义示例 */
static struct genl_family my_family = {
    .id = GENL_ID_GENERATE,  // 自动分配ID
    .hdrsize = 0,
    .name = "MY_FAMILY",
    .version = 1,
    .maxattr = MY_ATTR_MAX,
    .netnsok = true,
};
```

Q: [Intermediate] struct genl_ops如何定义？
A: genl_ops定义家族支持的操作：
```c
/* include/net/genetlink.h */
struct genl_ops {
    u8                cmd;          // 命令ID
    u8                internal_flags;
    unsigned int      flags;        // 权限标志
    const struct nla_policy *policy; // 属性策略
    
    int (*doit)(struct sk_buff *skb, struct genl_info *info);
    int (*dumpit)(struct sk_buff *skb, struct netlink_callback *cb);
    int (*done)(struct netlink_callback *cb);
    
    struct list_head ops_list;
};

/* flags常用值 */
#define GENL_ADMIN_PERM 0x01  // 需要CAP_NET_ADMIN

/* 定义操作示例 */
static const struct nla_policy my_policy[MY_ATTR_MAX + 1] = {
    [MY_ATTR_NAME] = { .type = NLA_NUL_STRING, .len = 64 },
    [MY_ATTR_VALUE] = { .type = NLA_U32 },
};

static struct genl_ops my_ops[] = {
    {
        .cmd = MY_CMD_GET,
        .doit = my_get,
        .dumpit = my_dump,
        .policy = my_policy,
    },
    {
        .cmd = MY_CMD_SET,
        .doit = my_set,
        .flags = GENL_ADMIN_PERM,
        .policy = my_policy,
    },
};
```

Q: [Intermediate] struct genl_info包含什么信息？
A: genl_info在doit回调中提供请求信息：
```c
/* include/net/genetlink.h */
struct genl_info {
    u32              snd_seq;    // 发送序列号
    u32              snd_pid;    // 发送者端口ID
    struct nlmsghdr *nlhdr;      // netlink消息头
    struct genlmsghdr *genlhdr;  // genl消息头
    void            *userhdr;    // 用户定义头
    struct nlattr  **attrs;      // 解析后的属性数组
    struct net      *_net;       // 网络命名空间
    void            *user_ptr[2]; // pre_doit可用
};

/* 在doit回调中使用 */
static int my_set(struct sk_buff *skb, struct genl_info *info)
{
    /* 检查必需属性 */
    if (!info->attrs[MY_ATTR_NAME])
        return -EINVAL;

    /* 获取属性值 */
    char *name = nla_data(info->attrs[MY_ATTR_NAME]);
    u32 value = 0;
    if (info->attrs[MY_ATTR_VALUE])
        value = nla_get_u32(info->attrs[MY_ATTR_VALUE]);

    /* 处理请求 */
    return do_set(name, value);
}
```

Q: [Intermediate] 如何注册Generic Netlink家族和操作？
A: 注册流程示例：
```c
/* 定义家族 */
static struct genl_family my_family = {
    .id = GENL_ID_GENERATE,
    .hdrsize = 0,
    .name = "MY_NETLINK",
    .version = 1,
    .maxattr = MY_ATTR_MAX,
};

/* 定义操作 */
static struct genl_ops my_ops[] = {
    { .cmd = MY_CMD_GET, .doit = my_get, ... },
    { .cmd = MY_CMD_SET, .doit = my_set, ... },
};

/* 方法1：分别注册 */
static int __init my_init(void)
{
    int ret;

    ret = genl_register_family(&my_family);
    if (ret)
        return ret;

    ret = genl_register_ops(&my_family, &my_ops[0]);
    if (ret)
        goto err_ops0;
    
    ret = genl_register_ops(&my_family, &my_ops[1]);
    if (ret)
        goto err_ops1;

    return 0;

err_ops1:
    genl_unregister_ops(&my_family, &my_ops[0]);
err_ops0:
    genl_unregister_family(&my_family);
    return ret;
}

/* 方法2：一次性注册(推荐) */
static int __init my_init(void)
{
    return genl_register_family_with_ops(&my_family,
                                         my_ops,
                                         ARRAY_SIZE(my_ops));
}

static void __exit my_exit(void)
{
    genl_unregister_family(&my_family);
}
```

Q: [Intermediate] Generic Netlink如何发送响应和组播？
A: GENL响应和组播的方法：
```c
/* 发送响应 */
static int my_get(struct sk_buff *skb, struct genl_info *info)
{
    struct sk_buff *msg;
    void *hdr;
    int ret;

    /* 1. 分配响应skb */
    msg = genlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!msg)
        return -ENOMEM;

    /* 2. 添加GENL头 */
    hdr = genlmsg_put(msg, info->snd_pid, info->snd_seq,
                      &my_family, 0, MY_CMD_GET);
    if (!hdr) {
        nlmsg_free(msg);
        return -EMSGSIZE;
    }

    /* 3. 添加属性 */
    if (nla_put_u32(msg, MY_ATTR_VALUE, my_value))
        goto nla_put_failure;

    /* 4. 结束并发送 */
    genlmsg_end(msg, hdr);
    return genlmsg_reply(msg, info);

nla_put_failure:
    genlmsg_cancel(msg, hdr);
    nlmsg_free(msg);
    return -EMSGSIZE;
}

/* 组播 */
static struct genl_multicast_group my_mcgrp = {
    .name = "my_events",
};

/* 注册组播组 */
genl_register_mc_group(&my_family, &my_mcgrp);

/* 发送组播 */
static void send_event(int event_type)
{
    struct sk_buff *msg;
    void *hdr;

    msg = genlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    hdr = genlmsg_put(msg, 0, 0, &my_family, 0, MY_CMD_EVENT);
    nla_put_u32(msg, MY_ATTR_EVENT, event_type);
    genlmsg_end(msg, hdr);
    
    genlmsg_multicast(msg, 0, my_mcgrp.id, GFP_KERNEL);
}
```

---

## RTNetlink

Q: [Basic] RTNetlink是什么？用于什么场景？
A: RTNetlink(Routing Netlink)管理网络配置：
```
RTNetlink用途：
+------------------+
| 接口管理         | ip link ...
| 地址管理         | ip addr ...
| 路由管理         | ip route ...
| 邻居表管理       | ip neigh ...
| 流量控制         | tc ...
| 策略路由         | ip rule ...
+------------------+

消息类型分类：
RTM_NEWLINK / RTM_DELLINK / RTM_GETLINK  // 接口
RTM_NEWADDR / RTM_DELADDR / RTM_GETADDR  // 地址
RTM_NEWROUTE / RTM_DELROUTE / RTM_GETROUTE // 路由
RTM_NEWNEIGH / RTM_DELNEIGH / RTM_GETNEIGH // 邻居
RTM_NEWQDISC / RTM_DELQDISC / RTM_GETQDISC // 队列规则
```

Q: [Intermediate] 如何向RTNetlink注册消息处理程序？
A: 使用rtnl_register注册处理函数：
```c
/* include/net/rtnetlink.h */
typedef int (*rtnl_doit_func)(struct sk_buff *, struct nlmsghdr *, void *);
typedef int (*rtnl_dumpit_func)(struct sk_buff *, struct netlink_callback *);

void rtnl_register(int protocol, int msgtype,
                   rtnl_doit_func doit,
                   rtnl_dumpit_func dumpit,
                   rtnl_calcit_func calcit);

/* 注册示例 - 路由子系统 */
void __init ip_fib_init(void)
{
    rtnl_register(PF_INET, RTM_NEWROUTE, inet_rtm_newroute, NULL, NULL);
    rtnl_register(PF_INET, RTM_DELROUTE, inet_rtm_delroute, NULL, NULL);
    rtnl_register(PF_INET, RTM_GETROUTE, NULL, inet_dump_fib, NULL);
}

/* 处理函数示例 */
static int inet_rtm_newroute(struct sk_buff *skb, struct nlmsghdr *nlh,
                             void *arg)
{
    struct rtmsg *rtm = nlmsg_data(nlh);
    struct nlattr *tb[RTA_MAX + 1];
    int err;

    err = nlmsg_parse(nlh, sizeof(*rtm), tb, RTA_MAX, rtm_ipv4_policy);
    if (err < 0)
        return err;

    /* 处理路由添加 */
    return fib_table_insert(net, tb, rtm, nlh, ...);
}
```

Q: [Intermediate] RTNetlink如何发送通知？
A: 内核状态变化时发送通知：
```c
/* 路由变化通知 */
void rtmsg_fib(int event, struct fib_info *fi, ...)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    
    skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    /* 填充消息 */
    
    rtnl_notify(skb, net, 0, RTNLGRP_IPV4_ROUTE, NULL, GFP_KERNEL);
}

/* 接口状态通知 */
void rtmsg_ifinfo(int type, struct net_device *dev, unsigned int change)
{
    struct sk_buff *skb;
    
    skb = rtmsg_ifinfo_build_skb(type, dev, change, GFP_KERNEL);
    if (skb)
        rtnl_notify(skb, dev_net(dev), 0, RTNLGRP_LINK, NULL, GFP_KERNEL);
}

/* 组播组定义 */
enum rtnetlink_groups {
    RTNLGRP_NONE,
    RTNLGRP_LINK,        // 接口状态
    RTNLGRP_NOTIFY,      // 通用通知
    RTNLGRP_NEIGH,       // 邻居表
    RTNLGRP_TC,          // 流量控制
    RTNLGRP_IPV4_IFADDR, // IPv4地址
    RTNLGRP_IPV4_ROUTE,  // IPv4路由
    RTNLGRP_IPV6_IFADDR, // IPv6地址
    RTNLGRP_IPV6_ROUTE,  // IPv6路由
    /* ... */
};
```

---

## 用户空间编程 (Userspace Programming)

Q: [Basic] 用户空间如何创建Netlink socket？
A: 使用标准socket API：
```c
#include <sys/socket.h>
#include <linux/netlink.h>

/* 创建socket */
int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
if (sock < 0) {
    perror("socket");
    exit(1);
}

/* 绑定地址 */
struct sockaddr_nl addr = {
    .nl_family = AF_NETLINK,
    .nl_pid = getpid(),  // 或0让内核分配
    .nl_groups = 0,      // 不订阅组播
};
if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
}

/* 订阅组播（可选） */
int group = RTMGRP_LINK;
setsockopt(sock, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
           &group, sizeof(group));

/* 或者在bind时指定 */
addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
```

Q: [Intermediate] 用户空间如何发送和接收Netlink消息？
A: 发送和接收的完整示例：
```c
/* 发送请求 */
int send_request(int sock, int type, int flags)
{
    struct {
        struct nlmsghdr nlh;
        struct rtgenmsg rtgen;
    } req = {
        .nlh = {
            .nlmsg_len = sizeof(req),
            .nlmsg_type = type,
            .nlmsg_flags = NLM_F_REQUEST | flags,
            .nlmsg_seq = ++seq,
            .nlmsg_pid = getpid(),
        },
        .rtgen = {
            .rtgen_family = AF_UNSPEC,
        },
    };

    struct sockaddr_nl dest = {
        .nl_family = AF_NETLINK,
        .nl_pid = 0,      // 发给内核
        .nl_groups = 0,
    };

    return sendto(sock, &req, sizeof(req), 0,
                  (struct sockaddr *)&dest, sizeof(dest));
}

/* 接收响应 */
void receive_response(int sock)
{
    char buf[8192];
    struct sockaddr_nl addr;
    socklen_t addrlen = sizeof(addr);
    int len;

    while ((len = recvfrom(sock, buf, sizeof(buf), 0,
                           (struct sockaddr *)&addr, &addrlen)) > 0) {
        struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
        
        for (; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
            if (nlh->nlmsg_type == NLMSG_DONE)
                return;
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *err = NLMSG_DATA(nlh);
                fprintf(stderr, "Error: %s\n", strerror(-err->error));
                return;
            }
            
            /* 处理消息 */
            process_message(nlh);
        }
    }
}
```

Q: [Intermediate] libnl库如何简化Netlink编程？
A: libnl提供高层API：
```c
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

/* 创建socket */
struct nl_sock *sock = nl_socket_alloc();
genl_connect(sock);  // Generic Netlink
// 或 nl_connect(sock, NETLINK_ROUTE);  // RTNetlink

/* 获取family ID */
int family_id = genl_ctrl_resolve(sock, "MY_FAMILY");

/* 构建消息 */
struct nl_msg *msg = nlmsg_alloc();
genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, family_id,
            0, 0, MY_CMD_GET, 1);
nla_put_string(msg, MY_ATTR_NAME, "test");

/* 发送并接收 */
nl_send_auto(sock, msg);
nl_recvmsgs_default(sock);

/* 设置回调 */
nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM,
                    my_callback, user_data);

/* 清理 */
nlmsg_free(msg);
nl_socket_free(sock);
```

---

## 组播和通知 (Multicast and Notifications)

Q: [Intermediate] Netlink组播是如何工作的？
A: Netlink组播机制：
```
+------------------+
|  内核 Netlink    |
|    Socket        |
+--------+---------+
         | netlink_broadcast()
         v
+--------+---------+
| 组播组1 | 组播组2 |  (nl_table[].listeners)
+----+----+----+----+
     |         |
     v         v
+----+----+  +-+-------+
| 用户A   |  | 用户B   |
| 订阅组1 |  | 订阅组2 |
+---------+  +---------+

/* 内核发送组播 */
int netlink_broadcast(struct sock *ssk,
                      struct sk_buff *skb,
                      __u32 exclude_pid,  // 排除的PID
                      __u32 group,        // 组播组号
                      gfp_t allocation);

/* 用户空间订阅组播 */
// 方法1：bind时指定
addr.nl_groups = (1 << (group - 1));

// 方法2：setsockopt
int grp = group;
setsockopt(sock, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
           &grp, sizeof(grp));

// 方法3：libnl
nl_socket_add_membership(sock, group);
```

Q: [Intermediate] 如何实现内核到用户空间的异步通知？
A: 异步通知的完整示例：
```c
/* 内核端 - 定义通知函数 */
static void notify_event(struct net *net, int event, void *data)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;

    /* 检查是否有监听者 */
    if (!netlink_has_listeners(my_sock, MY_GROUP))
        return;

    /* 分配消息 */
    skb = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_ATOMIC);
    if (!skb)
        return;

    /* 构建消息 */
    nlh = nlmsg_put(skb, 0, 0, MY_EVENT, sizeof(struct my_event), 0);
    if (!nlh) {
        kfree_skb(skb);
        return;
    }

    /* 填充事件数据 */
    struct my_event *evt = nlmsg_data(nlh);
    evt->type = event;
    memcpy(evt->data, data, sizeof(evt->data));

    nlmsg_end(skb, nlh);

    /* 组播发送 */
    netlink_broadcast(my_sock, skb, 0, MY_GROUP, GFP_ATOMIC);
}

/* 用户端 - 监听通知 */
void listen_events(int sock)
{
    fd_set rfds;
    char buf[4096];

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);

        if (select(sock + 1, &rfds, NULL, NULL, NULL) < 0)
            break;

        int len = recv(sock, buf, sizeof(buf), 0);
        if (len < 0)
            break;

        struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
        if (nlh->nlmsg_type == MY_EVENT) {
            struct my_event *evt = NLMSG_DATA(nlh);
            printf("Event: type=%d\n", evt->type);
        }
    }
}
```

---

## 最佳实践 (Best Practices)

Q: [Intermediate] Netlink编程的常见错误有哪些？
A: 常见错误及解决方法：
```c
/* 错误1：忘记检查nla_put返回值 */
// 错误
nla_put_u32(skb, MY_ATTR, value);

// 正确
if (nla_put_u32(skb, MY_ATTR, value))
    goto nla_put_failure;

/* 错误2：属性策略数组大小错误 */
// 错误 - 数组太小
static const struct nla_policy my_policy[MY_ATTR_MAX] = {...};

// 正确 - 需要+1
static const struct nla_policy my_policy[MY_ATTR_MAX + 1] = {...};

/* 错误3：未初始化属性数组 */
// 错误
struct nlattr *tb[MY_ATTR_MAX + 1];
nla_parse(tb, ...);
if (tb[MY_ATTR_OPT])  // 可能访问未初始化内存

// 正确
struct nlattr *tb[MY_ATTR_MAX + 1] = {};
// 或使用memset

/* 错误4：组播时skb被消耗 */
// 错误
netlink_broadcast(sock, skb, 0, group, GFP_KERNEL);
kfree_skb(skb);  // 双重释放！

// 正确 - broadcast会消耗skb，失败时也会释放
netlink_broadcast(sock, skb, 0, group, GFP_KERNEL);
// 不需要再释放

/* 错误5：在中断上下文分配 */
// 错误
skb = nlmsg_new(size, GFP_KERNEL);  // 可能睡眠

// 正确
skb = nlmsg_new(size, in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
```

Q: [Advanced] 如何设计高效的Netlink协议？
A: 协议设计建议：
```
1. 消息设计
+------------------+
| 使用TLV属性      | 便于扩展和向后兼容
| 定义版本号       | 协议演进
| 使用嵌套属性     | 结构化数据
+------------------+

2. 命名规范
enum my_commands {
    MY_CMD_UNSPEC,      // 0保留
    MY_CMD_GET,         // GET操作
    MY_CMD_SET,         // SET操作
    MY_CMD_NEW,         // 创建
    MY_CMD_DEL,         // 删除
    __MY_CMD_MAX,
};
#define MY_CMD_MAX (__MY_CMD_MAX - 1)

3. 属性设计
enum my_attrs {
    MY_ATTR_UNSPEC,     // 0保留
    MY_ATTR_ID,
    MY_ATTR_NAME,
    MY_ATTR_FLAGS,
    MY_ATTR_STATS,      // 嵌套属性
    __MY_ATTR_MAX,
};
#define MY_ATTR_MAX (__MY_ATTR_MAX - 1)

4. 错误处理
- 使用标准errno
- 提供详细的NLMSG_ERROR响应
- 考虑部分成功的情况

5. 性能优化
- 使用NLM_F_DUMP批量获取
- 合理设置缓冲区大小
- 考虑使用NETLINK_NO_ENOBUFS
```

Q: [Advanced] Netlink与网络命名空间如何交互？
A: Netlink支持网络命名空间隔离：
```c
/* 内核端 */
/* 创建时指定网络命名空间 */
nl_sock = netlink_kernel_create(net,  // struct net *
                                NETLINK_MYPROTO,
                                0, input, NULL, THIS_MODULE);

/* Generic Netlink默认支持 */
static struct genl_family my_family = {
    .netnsok = true,  // 支持命名空间
    /* ... */
};

/* 在处理函数中获取命名空间 */
static int my_handler(struct sk_buff *skb, struct nlmsghdr *nlh)
{
    struct net *net = sock_net(skb->sk);
    /* 在正确的命名空间中操作 */
}

/* 发送时指定命名空间 */
static void send_to_ns(struct net *net, struct sk_buff *skb)
{
    netlink_broadcast(net->my_nl_sock, skb, 0, group, GFP_KERNEL);
}

/* 用户端 */
/* 每个命名空间看到自己的Netlink socket */
/* unshare/setns改变命名空间后创建的socket在新命名空间中 */
```

Q: [Intermediate] 如何调试Netlink通信问题？
A: Netlink调试技术：
```bash
# 1. 使用strace跟踪系统调用
$ strace -e sendto,recvfrom ip link show

# 2. 使用nlmon虚拟接口捕获
$ modprobe nlmon
$ ip link add nlmon0 type nlmon
$ ip link set nlmon0 up
$ tcpdump -i nlmon0 -w netlink.pcap
# 用Wireshark分析

# 3. 内核调试输出
$ echo 'file net/netlink/*.c +p' > /sys/kernel/debug/dynamic_debug/control

# 4. 使用libnl调试
$ NL_DEBUG=1 ./my_program

# 5. 常见问题检查
- 检查返回的errno
- 确认消息格式正确（nlmsg_len等）
- 检查权限（CAP_NET_ADMIN）
- 验证属性解析结果
```

---

## 完整示例 (Complete Examples)

Q: [Advanced] 如何实现一个完整的内核Netlink模块？
A: 完整的Generic Netlink模块示例：
```c
/* my_netlink.c */
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/genetlink.h>

/* 属性定义 */
enum {
    MY_ATTR_UNSPEC,
    MY_ATTR_MSG,
    MY_ATTR_VALUE,
    __MY_ATTR_MAX,
};
#define MY_ATTR_MAX (__MY_ATTR_MAX - 1)

/* 命令定义 */
enum {
    MY_CMD_UNSPEC,
    MY_CMD_ECHO,
    __MY_CMD_MAX,
};
#define MY_CMD_MAX (__MY_CMD_MAX - 1)

/* 属性策略 */
static struct nla_policy my_policy[MY_ATTR_MAX + 1] = {
    [MY_ATTR_MSG] = { .type = NLA_NUL_STRING, .len = 256 },
    [MY_ATTR_VALUE] = { .type = NLA_U32 },
};

/* 家族定义 */
static struct genl_family my_family = {
    .id = GENL_ID_GENERATE,
    .hdrsize = 0,
    .name = "MY_GENL",
    .version = 1,
    .maxattr = MY_ATTR_MAX,
};

/* 处理函数 */
static int my_echo(struct sk_buff *skb, struct genl_info *info)
{
    struct sk_buff *msg;
    void *hdr;
    char *recv_msg = "no message";
    u32 recv_val = 0;

    if (info->attrs[MY_ATTR_MSG])
        recv_msg = nla_data(info->attrs[MY_ATTR_MSG]);
    if (info->attrs[MY_ATTR_VALUE])
        recv_val = nla_get_u32(info->attrs[MY_ATTR_VALUE]);

    pr_info("Received: msg=%s, value=%u\n", recv_msg, recv_val);

    /* 构建响应 */
    msg = genlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
    if (!msg)
        return -ENOMEM;

    hdr = genlmsg_put(msg, info->snd_pid, info->snd_seq,
                      &my_family, 0, MY_CMD_ECHO);
    if (!hdr) {
        nlmsg_free(msg);
        return -EMSGSIZE;
    }

    nla_put_string(msg, MY_ATTR_MSG, "Echo from kernel");
    nla_put_u32(msg, MY_ATTR_VALUE, recv_val + 1);

    genlmsg_end(msg, hdr);
    return genlmsg_reply(msg, info);
}

/* 操作定义 */
static struct genl_ops my_ops[] = {
    {
        .cmd = MY_CMD_ECHO,
        .doit = my_echo,
        .policy = my_policy,
    },
};

/* 模块初始化 */
static int __init my_init(void)
{
    return genl_register_family_with_ops(&my_family,
                                         my_ops,
                                         ARRAY_SIZE(my_ops));
}

static void __exit my_exit(void)
{
    genl_unregister_family(&my_family);
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
```

Q: [Advanced] 对应的用户空间程序示例？
A: 使用libnl的用户空间程序：
```c
/* my_client.c */
#include <stdio.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

/* 与内核保持一致的定义 */
enum {
    MY_ATTR_UNSPEC,
    MY_ATTR_MSG,
    MY_ATTR_VALUE,
    __MY_ATTR_MAX,
};
#define MY_ATTR_MAX (__MY_ATTR_MAX - 1)

enum {
    MY_CMD_UNSPEC,
    MY_CMD_ECHO,
    __MY_CMD_MAX,
};

static int family_id;

/* 响应回调 */
static int response_handler(struct nl_msg *msg, void *arg)
{
    struct nlmsghdr *nlh = nlmsg_hdr(msg);
    struct nlattr *attrs[MY_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = nlmsg_data(nlh);

    nla_parse(attrs, MY_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
              genlmsg_attrlen(gnlh, 0), NULL);

    if (attrs[MY_ATTR_MSG])
        printf("Response msg: %s\n", nla_get_string(attrs[MY_ATTR_MSG]));
    if (attrs[MY_ATTR_VALUE])
        printf("Response value: %u\n", nla_get_u32(attrs[MY_ATTR_VALUE]));

    return NL_OK;
}

int main(int argc, char *argv[])
{
    struct nl_sock *sock;
    struct nl_msg *msg;

    /* 创建socket */
    sock = nl_socket_alloc();
    if (!sock) {
        fprintf(stderr, "Failed to allocate socket\n");
        return 1;
    }

    /* 连接Generic Netlink */
    if (genl_connect(sock)) {
        fprintf(stderr, "Failed to connect\n");
        nl_socket_free(sock);
        return 1;
    }

    /* 获取family ID */
    family_id = genl_ctrl_resolve(sock, "MY_GENL");
    if (family_id < 0) {
        fprintf(stderr, "Family not found\n");
        nl_socket_free(sock);
        return 1;
    }

    /* 设置回调 */
    nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM,
                        response_handler, NULL);

    /* 构建消息 */
    msg = nlmsg_alloc();
    genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, family_id,
                0, 0, MY_CMD_ECHO, 1);
    nla_put_string(msg, MY_ATTR_MSG, "Hello from userspace");
    nla_put_u32(msg, MY_ATTR_VALUE, 42);

    /* 发送并接收 */
    nl_send_auto(sock, msg);
    nl_recvmsgs_default(sock);

    /* 清理 */
    nlmsg_free(msg);
    nl_socket_free(sock);

    return 0;
}

/* 编译：gcc -o my_client my_client.c $(pkg-config --cflags --libs libnl-3.0 libnl-genl-3.0) */
```

