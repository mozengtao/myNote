# Netfilter Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel Netfilter framework, hooks, connection tracking, and NAT
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. Netfilter Overview (Netfilter概述)

---

Q: What is Netfilter and its architecture?
A: Netfilter是Linux内核中的网络包过滤框架：

```
+==================================================================+
||                  NETFILTER ARCHITECTURE                        ||
+==================================================================+

用户空间工具:
+------------------------------------------------------------------+
|  iptables  |  ip6tables  |  nftables  |  conntrack  |  ipset    |
+------------------------------------------------------------------+
        |            |            |            |            |
        v            v            v            v            v
+------------------------------------------------------------------+
|                    Netlink / Xtables Interface                    |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                                                                  |
|                    NETFILTER FRAMEWORK                           |
|                                                                  |
|  +----------------------------------------------------------+   |
|  |                    Hook Points                            |   |
|  |  PREROUTING -> INPUT -> FORWARD -> OUTPUT -> POSTROUTING  |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  +----------------------------------------------------------+   |
|  |                    Core Components                        |   |
|  |  - Hook Functions                                        |   |
|  |  - Connection Tracking (conntrack)                       |   |
|  |  - NAT                                                    |   |
|  |  - Packet Mangling                                       |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
                              |
                              v
+------------------------------------------------------------------+
|                    Network Stack                                  |
|                 (IP, TCP, UDP, ICMP, ...)                        |
+------------------------------------------------------------------+
```

Netfilter特点：
| 特点 | 说明 |
|------|------|
| 模块化 | 钩子系统允许动态注册处理函数 |
| 灵活性 | 支持各种协议和规则 |
| 连接跟踪 | 有状态包过滤 |
| NAT支持 | 源NAT、目标NAT、伪装 |
| 可扩展 | 支持自定义匹配和目标 |
[Basic]

---

Q: What are the main Netfilter components?
A: Netfilter核心组件：

```c
// Netfilter主要组件
+------------------------------------------------------------------+
|                                                                  |
|  1. Hook System (钩子系统)                                        |
|     - 5个IPv4钩子点                                               |
|     - 允许在数据包路径中插入处理函数                               |
|                                                                  |
|  2. Connection Tracking (连接跟踪)                                |
|     - 跟踪连接状态                                                |
|     - 支持有状态防火墙                                            |
|     - 为NAT提供基础                                               |
|                                                                  |
|  3. NAT (网络地址转换)                                            |
|     - SNAT (源地址转换)                                           |
|     - DNAT (目标地址转换)                                         |
|     - MASQUERADE (伪装)                                           |
|                                                                  |
|  4. Packet Filtering (包过滤)                                     |
|     - iptables/nftables规则                                      |
|     - 匹配和目标模块                                              |
|                                                                  |
|  5. Packet Mangling (包修改)                                      |
|     - 修改包头字段                                                |
|     - TOS/DSCP标记                                                |
|                                                                  |
+------------------------------------------------------------------+
```

内核配置选项：
```bash
CONFIG_NETFILTER=y              # Netfilter核心
CONFIG_NETFILTER_ADVANCED=y     # 高级功能
CONFIG_NF_CONNTRACK=m           # 连接跟踪
CONFIG_NF_NAT=m                 # NAT
CONFIG_NETFILTER_XTABLES=m      # xtables框架
CONFIG_NF_TABLES=m              # nftables
CONFIG_IP_NF_IPTABLES=m         # iptables
CONFIG_IP_NF_FILTER=m           # filter表
CONFIG_IP_NF_NAT=m              # nat表
CONFIG_IP_NF_MANGLE=m           # mangle表
```
[Basic]

---

## 2. Netfilter Hooks (Netfilter钩子)

---

Q: What are the Netfilter hook points?
A: Netfilter定义了5个IPv4钩子点：

```
+==================================================================+
||                  NETFILTER HOOK POINTS                         ||
+==================================================================+

                    +----------------+
                    |  Network       |
                    |  Interface     |
                    +-------+--------+
                            |
                            v
              +-------------+-------------+
              |      NF_INET_PRE_ROUTING  |  <-- 钩子点1
              +-------------+-------------+
                            |
              +-------------+-------------+
              |   Routing Decision        |
              +------+------------+-------+
                     |            |
          本地目的   |            |  转发
                     v            v
      +-------------+--+      +--+--------------+
      | NF_INET_LOCAL  |      | NF_INET_FORWARD |  <-- 钩子点2,3
      |    _INPUT      |      +--------+--------+
      +-------+--------+               |
              |                        |
              v                        |
      +-------+--------+               |
      | Local Process  |               |
      +-------+--------+               |
              |                        |
              | 发送数据               |
              v                        |
      +-------+--------+               |
      | NF_INET_LOCAL  |               |
      |   _OUTPUT      |  <-- 钩子点4  |
      +-------+--------+               |
              |                        |
              +----------+-------------+
                         |
              +----------v-------------+
              |      Routing           |
              +----------+-------------+
                         |
              +----------v--------------+
              |  NF_INET_POST_ROUTING   |  <-- 钩子点5
              +----------+--------------+
                         |
                         v
              +----------+--------------+
              |    Network Interface    |
              +-------------------------+
```

钩子点定义：
```c
// include/uapi/linux/netfilter.h
enum nf_inet_hooks {
    NF_INET_PRE_ROUTING,    // 0: 路由前，刚进入协议栈
    NF_INET_LOCAL_IN,       // 1: 路由后，目的是本机
    NF_INET_FORWARD,        // 2: 路由后，转发到其他主机
    NF_INET_LOCAL_OUT,      // 3: 本机产生的包，路由前
    NF_INET_POST_ROUTING,   // 4: 路由后，即将离开协议栈
    NF_INET_NUMHOOKS
};
```

各钩子点的典型用途：
| 钩子点 | 用途 |
|--------|------|
| PRE_ROUTING | DNAT、连接跟踪入口 |
| LOCAL_IN | 入站过滤 |
| FORWARD | 转发过滤 |
| LOCAL_OUT | 出站过滤 |
| POST_ROUTING | SNAT、连接跟踪出口 |
[Intermediate]

---

Q: What is the nf_hook_ops structure?
A: `nf_hook_ops`用于注册钩子函数：

```c
// include/linux/netfilter.h
struct nf_hook_ops {
    // 钩子函数
    nf_hookfn       *hook;
    // 网络设备（可选）
    struct net_device *dev;
    // 钩子所有者模块
    struct module   *owner;
    // 协议族（AF_INET, AF_INET6等）
    u_int8_t        pf;
    // 钩子点
    unsigned int    hooknum;
    // 优先级（数值小的先执行）
    int             priority;
};

// 钩子函数原型
typedef unsigned int nf_hookfn(void *priv,
                               struct sk_buff *skb,
                               const struct nf_hook_state *state);

// 钩子状态
struct nf_hook_state {
    unsigned int hook;           // 钩子点
    u_int8_t pf;                 // 协议族
    struct net_device *in;       // 入接口
    struct net_device *out;      // 出接口
    struct sock *sk;             // socket（如果有）
    struct net *net;             // 网络命名空间
    int (*okfn)(struct net *, struct sock *, struct sk_buff *);
};
```

钩子函数返回值：
```c
// include/uapi/linux/netfilter.h
#define NF_DROP   0    // 丢弃包
#define NF_ACCEPT 1    // 接受包，继续处理
#define NF_STOLEN 2    // 包被钩子接管，不再处理
#define NF_QUEUE  3    // 包排队到用户空间
#define NF_REPEAT 4    // 再次调用此钩子
#define NF_STOP   5    // 停止，相当于ACCEPT但不调用后续钩子
```
[Intermediate]

---

Q: How to write a simple Netfilter hook module?
A: 简单的Netfilter钩子模块：

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>

static struct nf_hook_ops nfho;

// 钩子函数
static unsigned int my_hook_func(void *priv,
                                  struct sk_buff *skb,
                                  const struct nf_hook_state *state)
{
    struct iphdr *iph;
    struct tcphdr *tcph;
    
    if (!skb)
        return NF_ACCEPT;
    
    // 获取IP头
    iph = ip_hdr(skb);
    if (!iph)
        return NF_ACCEPT;
    
    // 只处理TCP
    if (iph->protocol != IPPROTO_TCP)
        return NF_ACCEPT;
    
    // 获取TCP头
    tcph = tcp_hdr(skb);
    if (!tcph)
        return NF_ACCEPT;
    
    // 打印信息
    pr_info("Packet: %pI4:%d -> %pI4:%d\n",
            &iph->saddr, ntohs(tcph->source),
            &iph->daddr, ntohs(tcph->dest));
    
    // 示例：阻止特定端口
    if (ntohs(tcph->dest) == 8080) {
        pr_info("Dropping packet to port 8080\n");
        return NF_DROP;
    }
    
    return NF_ACCEPT;
}

static int __init my_hook_init(void)
{
    // 设置钩子选项
    nfho.hook = my_hook_func;
    nfho.hooknum = NF_INET_PRE_ROUTING;  // 钩子点
    nfho.pf = PF_INET;                   // IPv4
    nfho.priority = NF_IP_PRI_FIRST;     // 最高优先级
    
    // 注册钩子
    nf_register_net_hook(&init_net, &nfho);
    
    pr_info("Netfilter hook registered\n");
    return 0;
}

static void __exit my_hook_exit(void)
{
    // 注销钩子
    nf_unregister_net_hook(&init_net, &nfho);
    pr_info("Netfilter hook unregistered\n");
}

module_init(my_hook_init);
module_exit(my_hook_exit);
MODULE_LICENSE("GPL");
```

编译和测试：
```bash
# 编译
make

# 加载模块
insmod my_hook.ko

# 测试
curl http://localhost:8080  # 应该被阻止

# 查看日志
dmesg | tail

# 卸载模块
rmmod my_hook
```
[Intermediate]

---

## 3. Hook Priorities (钩子优先级)

---

Q: What are the Netfilter hook priorities?
A: 钩子优先级决定执行顺序：

```c
// include/uapi/linux/netfilter_ipv4.h
enum nf_ip_hook_priorities {
    NF_IP_PRI_FIRST = INT_MIN,
    NF_IP_PRI_RAW_BEFORE_DEFRAG = -450,
    NF_IP_PRI_CONNTRACK_DEFRAG = -400,  // 连接跟踪碎片重组
    NF_IP_PRI_RAW = -300,               // raw表
    NF_IP_PRI_SELINUX_FIRST = -225,
    NF_IP_PRI_CONNTRACK = -200,         // 连接跟踪
    NF_IP_PRI_MANGLE = -150,            // mangle表
    NF_IP_PRI_NAT_DST = -100,           // DNAT
    NF_IP_PRI_FILTER = 0,               // filter表
    NF_IP_PRI_SECURITY = 50,            // security表
    NF_IP_PRI_NAT_SRC = 100,            // SNAT
    NF_IP_PRI_SELINUX_LAST = 225,
    NF_IP_PRI_CONNTRACK_HELPER = 300,   // 连接跟踪helper
    NF_IP_PRI_CONNTRACK_CONFIRM = INT_MAX,  // 连接确认
    NF_IP_PRI_LAST = INT_MAX,
};
```

优先级执行顺序（PRE_ROUTING为例）：
```
数据包到达
    |
    v
+---+-------------------------------------------+
| -450 | RAW_BEFORE_DEFRAG                      |
+---+-------------------------------------------+
    |
    v
+---+-------------------------------------------+
| -400 | CONNTRACK_DEFRAG (碎片重组)            |
+---+-------------------------------------------+
    |
    v
+---+-------------------------------------------+
| -300 | RAW表                                  |
+---+-------------------------------------------+
    |
    v
+---+-------------------------------------------+
| -200 | CONNTRACK (连接跟踪)                   |
+---+-------------------------------------------+
    |
    v
+---+-------------------------------------------+
| -150 | MANGLE表                               |
+---+-------------------------------------------+
    |
    v
+---+-------------------------------------------+
| -100 | NAT_DST (DNAT)                         |
+---+-------------------------------------------+
    |
    v
+---+-------------------------------------------+
|   0  | FILTER表                               |
+---+-------------------------------------------+
    |
    v
路由决策
```
[Intermediate]

---

## 4. Connection Tracking (连接跟踪)

---

Q: What is connection tracking and how does it work?
A: 连接跟踪是Netfilter的核心组件：

```
+==================================================================+
||                  CONNECTION TRACKING                           ||
+==================================================================+

连接跟踪的作用:
+------------------------------------------------------------------+
| 1. 跟踪每个连接的状态                                             |
| 2. 为有状态防火墙提供基础                                         |
| 3. 为NAT提供地址转换映射                                          |
| 4. 支持应用层协议（FTP、SIP等）                                   |
+------------------------------------------------------------------+

连接状态:
+------------------------------------------------------------------+
|                                                                  |
|  NEW        - 新连接的第一个包                                    |
|  ESTABLISHED - 已看到双向流量                                     |
|  RELATED    - 与已有连接相关的新连接（如FTP数据连接）             |
|  INVALID    - 无法识别或不正确的包                                |
|  UNTRACKED  - 被raw表NOTRACK标记的包                              |
|                                                                  |
+------------------------------------------------------------------+

连接跟踪流程:
+------------------------------------------------------------------+
|                                                                  |
|  PRE_ROUTING                                                     |
|      |                                                           |
|      v                                                           |
|  +-------+                                                       |
|  |conntrack| -> 查找/创建连接条目                                |
|  +-------+      |                                                |
|      |          |                                                |
|      |    +-----+-----+                                          |
|      |    |           |                                          |
|      |  找到       未找到                                         |
|      |    |           |                                          |
|      |    v           v                                          |
|      | 更新状态    创建新条目                                     |
|      |    |           |                                          |
|      +----+-----------+                                          |
|      |                                                           |
|      v                                                           |
|  数据包处理...                                                   |
|      |                                                           |
|      v                                                           |
|  POST_ROUTING                                                    |
|      |                                                           |
|      v                                                           |
|  +----------+                                                    |
|  |confirm() | -> 确认连接（加入全局表）                          |
|  +----------+                                                    |
|                                                                  |
+------------------------------------------------------------------+
```

连接跟踪数据结构：
```c
// include/net/netfilter/nf_conntrack.h
struct nf_conn {
    // 连接元组（哈希键）
    struct nf_conntrack_tuple_hash tuplehash[IP_CT_DIR_MAX];
    
    // 连接状态
    unsigned long status;
    
    // 超时时间
    u32 timeout;
    
    // 使用计数
    possible_net_t ct_net;
    struct hlist_node nat_bysource;
    
    // 扩展数据（NAT、helper等）
    struct nf_ct_ext *ext;
    
    // 标记
    u32 mark;
    u32 secmark;
    
    // 协议特定数据
    union nf_conntrack_proto proto;
};

// 连接元组
struct nf_conntrack_tuple {
    struct nf_conntrack_man src;    // 源地址/端口
    struct {
        union nf_inet_addr u3;       // 目标地址
        union {
            __be16 all;
            struct { __be16 port; } tcp;
            struct { __be16 port; } udp;
            struct { u_int8_t type, code; } icmp;
        } u;
        u_int8_t protonum;           // 协议号
        u_int8_t dir;                // 方向
    } dst;
};
```
[Intermediate]

---

Q: How to use connection tracking in iptables?
A: 使用连接跟踪进行有状态过滤：

```bash
# 基于连接状态的规则
# 允许已建立和相关的连接
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# 允许新的SSH连接
iptables -A INPUT -p tcp --dport 22 -m conntrack --ctstate NEW -j ACCEPT

# 丢弃无效包
iptables -A INPUT -m conntrack --ctstate INVALID -j DROP

# 查看连接跟踪表
cat /proc/net/nf_conntrack
conntrack -L

# 连接跟踪统计
conntrack -S

# 删除特定连接
conntrack -D -s 192.168.1.100

# 清空连接跟踪表
conntrack -F
```

连接跟踪参数调整：
```bash
# 最大连接数
echo 65536 > /proc/sys/net/netfilter/nf_conntrack_max

# 哈希表大小
echo 16384 > /proc/sys/net/netfilter/nf_conntrack_buckets

# TCP超时时间
echo 3600 > /proc/sys/net/netfilter/nf_conntrack_tcp_timeout_established

# 查看当前连接数
cat /proc/sys/net/netfilter/nf_conntrack_count
```

内核中使用连接跟踪：
```c
// 获取连接跟踪信息
struct nf_conn *ct;
enum ip_conntrack_info ctinfo;

ct = nf_ct_get(skb, &ctinfo);
if (ct) {
    // 检查连接状态
    if (ctinfo == IP_CT_ESTABLISHED)
        pr_info("Established connection\n");
    
    // 检查方向
    if (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL)
        pr_info("Original direction\n");
}
```
[Intermediate]

---

## 5. NAT Implementation (NAT实现)

---

Q: How does NAT work in Netfilter?
A: NAT（网络地址转换）实现：

```
+==================================================================+
||                  NAT ARCHITECTURE                              ||
+==================================================================+

NAT类型:
+------------------------------------------------------------------+
|                                                                  |
|  SNAT (Source NAT) - 修改源地址                                   |
|  +----------------------------------------------------------+   |
|  | 内网主机 -> 外网                                          |   |
|  | 192.168.1.100:12345 -> 203.0.113.1:80                    |   |
|  | 转换为: 203.0.113.10:54321 -> 203.0.113.1:80             |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  DNAT (Destination NAT) - 修改目标地址                           |
|  +----------------------------------------------------------+   |
|  | 外网 -> 内网服务器                                        |   |
|  | 1.2.3.4:54321 -> 203.0.113.10:80                         |   |
|  | 转换为: 1.2.3.4:54321 -> 192.168.1.100:8080              |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  MASQUERADE - 动态SNAT                                           |
|  +----------------------------------------------------------+   |
|  | 用于动态IP（如PPPoE）                                     |   |
|  | 自动使用出接口的IP地址                                    |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

NAT处理流程:
+------------------------------------------------------------------+
|                                                                  |
|  DNAT在PRE_ROUTING处理（路由决策前）                              |
|                                                                  |
|      入站包                                                      |
|         |                                                        |
|         v                                                        |
|    PRE_ROUTING                                                   |
|         |                                                        |
|         v                                                        |
|    +----+----+                                                   |
|    | DNAT    | <- 修改目标地址                                   |
|    +---------+                                                   |
|         |                                                        |
|         v                                                        |
|    路由决策（使用新的目标地址）                                   |
|                                                                  |
|                                                                  |
|  SNAT在POST_ROUTING处理（路由决策后）                             |
|                                                                  |
|    路由决策                                                      |
|         |                                                        |
|         v                                                        |
|    POST_ROUTING                                                  |
|         |                                                        |
|         v                                                        |
|    +----+----+                                                   |
|    | SNAT    | <- 修改源地址                                     |
|    +---------+                                                   |
|         |                                                        |
|         v                                                        |
|      出站包                                                      |
|                                                                  |
+------------------------------------------------------------------+
```

NAT规则示例：
```bash
# SNAT - 固定源IP
iptables -t nat -A POSTROUTING -o eth0 -j SNAT --to-source 203.0.113.10

# MASQUERADE - 动态源IP
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE

# DNAT - 端口转发
iptables -t nat -A PREROUTING -p tcp --dport 80 -j DNAT --to-destination 192.168.1.100:8080

# REDIRECT - 本地端口重定向
iptables -t nat -A PREROUTING -p tcp --dport 80 -j REDIRECT --to-ports 8080

# 启用IP转发
echo 1 > /proc/sys/net/ipv4/ip_forward
```
[Intermediate]

---

Q: How is NAT implemented in the kernel?
A: 内核中的NAT实现：

```c
// net/netfilter/nf_nat_core.c

// NAT扩展结构
struct nf_nat_hook {
    // 执行NAT转换
    unsigned int (*manip_pkt)(struct sk_buff *skb, 
                              struct nf_conn *ct,
                              enum nf_nat_manip_type mtype,
                              enum ip_conntrack_dir dir);
};

// NAT转换类型
enum nf_nat_manip_type {
    NF_NAT_MANIP_SRC,    // 源地址转换
    NF_NAT_MANIP_DST     // 目标地址转换
};

// NAT范围
struct nf_nat_range2 {
    unsigned int flags;
    union nf_inet_addr min_addr;
    union nf_inet_addr max_addr;
    union nf_conntrack_man_proto min_proto;
    union nf_conntrack_man_proto max_proto;
    union nf_conntrack_man_proto base_proto;
};

// 执行NAT
unsigned int nf_nat_packet(struct nf_conn *ct,
                           enum ip_conntrack_info ctinfo,
                           unsigned int hooknum,
                           struct sk_buff *skb)
{
    enum nf_nat_manip_type mtype = HOOK2MANIP(hooknum);
    enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
    unsigned int verdict = NF_ACCEPT;

    // 检查是否需要NAT
    if (test_bit(statusbit, &ct->status)) {
        // 执行NAT转换
        verdict = nf_nat_manip_pkt(skb, ct, mtype, dir);
    }

    return verdict;
}

// 修改包头
static unsigned int nf_nat_manip_pkt(struct sk_buff *skb,
                                      struct nf_conn *ct,
                                      enum nf_nat_manip_type mtype,
                                      enum ip_conntrack_dir dir)
{
    struct iphdr *iph = ip_hdr(skb);
    struct nf_conntrack_tuple target;

    // 获取目标元组
    nf_ct_invert_tuple(&target, &ct->tuplehash[!dir].tuple);

    // 修改IP头
    if (mtype == NF_NAT_MANIP_SRC) {
        iph->saddr = target.src.u3.ip;
    } else {
        iph->daddr = target.dst.u3.ip;
    }

    // 重新计算校验和
    csum_replace4(&iph->check, ...);

    // 修改L4头（TCP/UDP端口）
    l4proto->manip_pkt(skb, ct, mtype, ...);

    return NF_ACCEPT;
}
```

NAT数据结构：
```c
// NAT映射条目
struct nf_conn_nat {
    struct rhash_head bysource;    // 按源查找
    struct nf_nat_range2 range;    // NAT范围
};

// 获取NAT扩展
struct nf_conn_nat *nat = nfct_nat(ct);
```
[Advanced]

---

## 6. iptables Tables and Chains (iptables表和链)

---

Q: What are the iptables tables and chains?
A: iptables的表和链结构：

```
+==================================================================+
||                  IPTABLES TABLES & CHAINS                      ||
+==================================================================+

表（Tables）- 按功能分类:
+------------------------------------------------------------------+
|                                                                  |
|  raw表      - 连接跟踪豁免（NOTRACK）                             |
|  mangle表   - 包修改（TOS、TTL、MARK）                            |
|  nat表      - 地址转换（SNAT、DNAT）                              |
|  filter表   - 包过滤（ACCEPT、DROP）                              |
|  security表 - SELinux标记                                        |
|                                                                  |
+------------------------------------------------------------------+

链（Chains）- 按钩子点分类:
+------------------------------------------------------------------+
|                                                                  |
|  PREROUTING   - PRE_ROUTING钩子点                                |
|  INPUT        - LOCAL_IN钩子点                                   |
|  FORWARD      - FORWARD钩子点                                    |
|  OUTPUT       - LOCAL_OUT钩子点                                  |
|  POSTROUTING  - POST_ROUTING钩子点                               |
|                                                                  |
+------------------------------------------------------------------+

表-链对应关系:
+------------+-----------+-------+---------+--------+-------------+
|    表      | PREROUTING| INPUT | FORWARD | OUTPUT | POSTROUTING |
+------------+-----------+-------+---------+--------+-------------+
| raw        |     √     |       |         |   √    |             |
+------------+-----------+-------+---------+--------+-------------+
| mangle     |     √     |   √   |    √    |   √    |      √      |
+------------+-----------+-------+---------+--------+-------------+
| nat        |     √     |   √   |         |   √    |      √      |
+------------+-----------+-------+---------+--------+-------------+
| filter     |           |   √   |    √    |   √    |             |
+------------+-----------+-------+---------+--------+-------------+
| security   |           |   √   |    √    |   √    |             |
+------------+-----------+-------+---------+--------+-------------+
```

数据包流程（完整）：
```
入站（本机目的）:
  -> raw:PREROUTING 
  -> mangle:PREROUTING 
  -> nat:PREROUTING (DNAT)
  -> [路由决策]
  -> mangle:INPUT
  -> filter:INPUT
  -> security:INPUT
  -> [本机进程]

转发:
  -> raw:PREROUTING
  -> mangle:PREROUTING
  -> nat:PREROUTING (DNAT)
  -> [路由决策]
  -> mangle:FORWARD
  -> filter:FORWARD
  -> security:FORWARD
  -> mangle:POSTROUTING
  -> nat:POSTROUTING (SNAT)
  -> [发送]

出站（本机产生）:
  -> raw:OUTPUT
  -> mangle:OUTPUT
  -> nat:OUTPUT (DNAT)
  -> [路由决策]
  -> filter:OUTPUT
  -> security:OUTPUT
  -> mangle:POSTROUTING
  -> nat:POSTROUTING (SNAT)
  -> [发送]
```

常用命令：
```bash
# 列出规则
iptables -L -n -v                    # filter表
iptables -t nat -L -n -v             # nat表
iptables -t mangle -L -n -v          # mangle表
iptables -t raw -L -n -v             # raw表

# 添加规则
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -I INPUT 1 -p icmp -j ACCEPT  # 插入到第一条

# 删除规则
iptables -D INPUT -p tcp --dport 22 -j ACCEPT
iptables -D INPUT 3                     # 删除第3条

# 创建自定义链
iptables -N MYCHAIN
iptables -A INPUT -j MYCHAIN

# 设置默认策略
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# 保存/恢复规则
iptables-save > /etc/iptables/rules.v4
iptables-restore < /etc/iptables/rules.v4
```
[Intermediate]

---

## 7. nftables (新一代防火墙)

---

Q: What is nftables and how does it compare to iptables?
A: nftables是iptables的替代者：

```
+==================================================================+
||                  NFTABLES ARCHITECTURE                         ||
+==================================================================+

iptables vs nftables:
+------------------+------------------+
|    iptables      |    nftables      |
+------------------+------------------+
| 多个工具          | 单一工具(nft)    |
| (iptables,       |                  |
|  ip6tables,      |                  |
|  ebtables,       |                  |
|  arptables)      |                  |
+------------------+------------------+
| 固定表和链        | 可自定义         |
+------------------+------------------+
| 线性规则匹配      | 优化的规则集     |
| (逐条匹配)        | (集合、映射)     |
+------------------+------------------+
| xtables框架      | nf_tables框架    |
+------------------+------------------+

nftables架构:
+------------------------------------------------------------------+
|                                                                  |
|  用户空间: nft命令                                                |
|                 |                                                |
|                 v                                                |
|  +----------------------------------------------------------+   |
|  |                    Netlink Interface                      |   |
|  +----------------------------------------------------------+   |
|                 |                                                |
|                 v                                                |
|  +----------------------------------------------------------+   |
|  |                    nf_tables Core                         |   |
|  |  +----------------+  +----------------+  +----------------+  |
|  |  |    Tables      |  |    Chains      |  |    Rules       |  |
|  |  +----------------+  +----------------+  +----------------+  |
|  |  +----------------+  +----------------+                     |  |
|  |  |     Sets       |  |     Maps       |                     |  |
|  |  +----------------+  +----------------+                     |  |
|  +----------------------------------------------------------+   |
|                 |                                                |
|                 v                                                |
|  +----------------------------------------------------------+   |
|  |              Netfilter Hooks                              |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

nftables基本语法：
```bash
# 创建表
nft add table inet filter

# 创建链
nft add chain inet filter input { type filter hook input priority 0 \; policy accept \; }

# 添加规则
nft add rule inet filter input tcp dport 22 accept
nft add rule inet filter input ct state established,related accept
nft add rule inet filter input drop

# 使用集合
nft add set inet filter allowed_ports { type inet_service \; }
nft add element inet filter allowed_ports { 22, 80, 443 }
nft add rule inet filter input tcp dport @allowed_ports accept

# 使用映射
nft add map inet filter portmap { type inet_service : verdict \; }
nft add element inet filter portmap { 22 : accept, 80 : accept }
nft add rule inet filter input tcp dport vmap @portmap

# 列出规则
nft list ruleset
nft list table inet filter

# 导出/导入
nft list ruleset > /etc/nftables.conf
nft -f /etc/nftables.conf
```

nftables示例配置：
```bash
#!/usr/sbin/nft -f

flush ruleset

table inet filter {
    chain input {
        type filter hook input priority 0; policy drop;
        
        # 允许loopback
        iif lo accept
        
        # 允许已建立连接
        ct state established,related accept
        
        # 允许ICMP
        ip protocol icmp accept
        ip6 nexthdr icmpv6 accept
        
        # 允许SSH
        tcp dport 22 accept
        
        # 记录并丢弃其他
        log prefix "INPUT DROP: " drop
    }
    
    chain forward {
        type filter hook forward priority 0; policy drop;
    }
    
    chain output {
        type filter hook output priority 0; policy accept;
    }
}

table inet nat {
    chain prerouting {
        type nat hook prerouting priority -100;
    }
    
    chain postrouting {
        type nat hook postrouting priority 100;
        oif eth0 masquerade
    }
}
```
[Intermediate]

---

## 8. Netfilter Extensions (Netfilter扩展)

---

Q: How to write a custom Netfilter match/target?
A: 自定义匹配和目标模块：

```c
// 自定义匹配模块
#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include <linux/ip.h>

// 匹配数据结构（用户空间传入）
struct xt_myport_info {
    __u16 port;
    __u8 invert;
};

// 匹配函数
static bool myport_mt(const struct sk_buff *skb,
                      struct xt_action_param *par)
{
    const struct xt_myport_info *info = par->matchinfo;
    const struct iphdr *iph = ip_hdr(skb);
    const struct tcphdr *tcph;
    struct tcphdr _tcph;
    bool match;
    
    if (iph->protocol != IPPROTO_TCP)
        return false;
    
    tcph = skb_header_pointer(skb, ip_hdrlen(skb),
                              sizeof(_tcph), &_tcph);
    if (!tcph)
        return false;
    
    match = (ntohs(tcph->dest) == info->port);
    
    return match ^ info->invert;
}

// 检查函数（验证参数）
static int myport_mt_check(const struct xt_mtchk_param *par)
{
    const struct xt_myport_info *info = par->matchinfo;
    
    if (info->port == 0) {
        pr_err("myport: invalid port 0\n");
        return -EINVAL;
    }
    
    return 0;
}

// 注册结构
static struct xt_match myport_mt_reg __read_mostly = {
    .name       = "myport",
    .revision   = 0,
    .family     = NFPROTO_IPV4,
    .match      = myport_mt,
    .checkentry = myport_mt_check,
    .matchsize  = sizeof(struct xt_myport_info),
    .me         = THIS_MODULE,
};

static int __init myport_mt_init(void)
{
    return xt_register_match(&myport_mt_reg);
}

static void __exit myport_mt_exit(void)
{
    xt_unregister_match(&myport_mt_reg);
}

module_init(myport_mt_init);
module_exit(myport_mt_exit);
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_myport");
```

自定义目标模块：
```c
// 目标函数
static unsigned int mytarget_tg(struct sk_buff *skb,
                                 const struct xt_action_param *par)
{
    const struct xt_mytarget_info *info = par->targinfo;
    
    // 执行自定义操作
    pr_info("mytarget: packet matched\n");
    
    // 返回动作
    return info->action;  // NF_DROP, NF_ACCEPT, etc.
}

// 注册结构
static struct xt_target mytarget_tg_reg __read_mostly = {
    .name       = "MYTARGET",
    .revision   = 0,
    .family     = NFPROTO_IPV4,
    .target     = mytarget_tg,
    .checkentry = mytarget_tg_check,
    .targetsize = sizeof(struct xt_mytarget_info),
    .me         = THIS_MODULE,
};
```

用户空间库（libxt_myport.c）：
```c
#include <xtables.h>
#include "xt_myport.h"

static void myport_help(void)
{
    printf("myport match options:\n"
           "  --port <num>  Match destination port\n");
}

static const struct option myport_opts[] = {
    { "port", 1, NULL, 'p' },
    { NULL }
};

static struct xtables_match myport_mt_reg = {
    .version       = XTABLES_VERSION,
    .name          = "myport",
    .family        = NFPROTO_IPV4,
    .size          = XT_ALIGN(sizeof(struct xt_myport_info)),
    .userspacesize = XT_ALIGN(sizeof(struct xt_myport_info)),
    .help          = myport_help,
    .parse         = myport_parse,
    .print         = myport_print,
    .save          = myport_save,
    .extra_opts    = myport_opts,
};

void _init(void)
{
    xtables_register_match(&myport_mt_reg);
}
```
[Advanced]

---

## 9. Connection Tracking Helpers (连接跟踪助手)

---

Q: What are conntrack helpers?
A: 连接跟踪助手处理复杂协议：

```
+==================================================================+
||                  CONNTRACK HELPERS                             ||
+==================================================================+

为什么需要Helper:
+------------------------------------------------------------------+
|                                                                  |
|  问题：某些协议在控制连接中协商数据连接（如FTP）                    |
|                                                                  |
|  FTP主动模式:                                                    |
|  Client:12345 --> Server:21   (控制连接)                         |
|  Client:12345 <-- Server:20   (数据连接) <- NAT无法追踪！         |
|                                                                  |
|  解决方案：Helper解析控制协议，预期数据连接                        |
|                                                                  |
+------------------------------------------------------------------+

支持的Helper:
+------------------------------------------------------------------+
|  ftp     - FTP协议 (PORT/PASV命令)                               |
|  tftp    - TFTP协议                                              |
|  irc     - IRC DCC                                               |
|  sip     - SIP协议 (VoIP)                                        |
|  h323    - H.323协议 (VoIP)                                      |
|  pptp    - PPTP VPN                                              |
|  amanda  - Amanda备份                                            |
+------------------------------------------------------------------+
```

FTP Helper工作流程：
```
1. 客户端连接FTP服务器（控制连接）
   Client:12345 -> Server:21

2. 客户端发送PASV命令
   -> "PASV"

3. 服务器响应被动端口
   <- "227 Entering Passive Mode (192,168,1,100,196,87)"
   
4. FTP Helper解析响应
   - 解析出IP和端口：192.168.1.100:50263
   - 创建"期望"连接条目

5. 客户端连接数据端口
   Client:54321 -> Server:50263
   - 此连接状态为RELATED

6. 如果有NAT，Helper修改响应中的IP地址
```

配置Helper：
```bash
# 加载helper模块
modprobe nf_conntrack_ftp
modprobe nf_nat_ftp

# 分配helper到连接（nftables）
nft add ct helper inet filter ftp-standard { type "ftp" protocol tcp \; }
nft add rule inet filter input tcp dport 21 ct helper set "ftp-standard"

# iptables方式（旧）
iptables -A PREROUTING -t raw -p tcp --dport 21 -j CT --helper ftp

# 修改跟踪端口（非标准端口FTP）
echo 2121 > /sys/module/nf_conntrack_ftp/parameters/ports

# 查看期望连接
cat /proc/net/nf_conntrack_expect
conntrack -E expect
```

内核Helper实现：
```c
// net/netfilter/nf_conntrack_ftp.c
static struct nf_conntrack_helper ftp[MAX_PORTS] __read_mostly;

// 数据回调
static int help(struct sk_buff *skb, unsigned int protoff,
                struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
    // 解析FTP命令
    // 查找PORT或PASV命令
    // 创建期望连接
    struct nf_conntrack_expect *exp;
    
    exp = nf_ct_expect_alloc(ct);
    if (!exp)
        return NF_DROP;
    
    // 设置期望的数据连接
    nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT,
                      ct->tuplehash[!dir].tuple.src.l3num,
                      &ct->tuplehash[!dir].tuple.src.u3,
                      &ct->tuplehash[!dir].tuple.dst.u3,
                      IPPROTO_TCP, NULL, &port);
    
    // 注册期望
    nf_ct_expect_related(exp);
    nf_ct_expect_put(exp);
    
    return NF_ACCEPT;
}
```
[Advanced]

---

## 10. Netfilter Queuing (Netfilter队列)

---

Q: How does NFQUEUE work?
A: NFQUEUE将包发送到用户空间处理：

```
+==================================================================+
||                  NFQUEUE MECHANISM                             ||
+==================================================================+

工作流程:
+------------------------------------------------------------------+
|                                                                  |
|  内核                                                            |
|  +----------------------------------------------------------+   |
|  |  Netfilter Hook                                          |   |
|  |       |                                                  |   |
|  |       v                                                  |   |
|  |  iptables -j NFQUEUE --queue-num 0                       |   |
|  |       |                                                  |   |
|  |       v                                                  |   |
|  |  +------------+                                          |   |
|  |  | nf_queue() | --> 放入队列                              |   |
|  |  +------------+                                          |   |
|  +-----|----------------------------------------------------+   |
|        |                                                        |
|        | Netlink                                                |
|        v                                                        |
|  用户空间                                                        |
|  +----------------------------------------------------------+   |
|  |  libnetfilter_queue                                      |   |
|  |       |                                                  |   |
|  |       v                                                  |   |
|  |  用户程序处理                                             |   |
|  |  - 检查包内容                                            |   |
|  |  - 修改包（可选）                                         |   |
|  |  - 返回裁决（ACCEPT/DROP/REPEAT）                        |   |
|  +-----|----------------------------------------------------+   |
|        |                                                        |
|        | Netlink verdict                                        |
|        v                                                        |
|  内核继续处理或丢弃包                                            |
|                                                                  |
+------------------------------------------------------------------+
```

用户空间程序示例：
```c
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

// 回调函数
static int callback(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
                    struct nfq_data *nfa, void *data)
{
    struct nfqnl_msg_packet_hdr *ph;
    unsigned char *payload;
    int id, len;
    
    ph = nfq_get_msg_packet_hdr(nfa);
    id = ntohl(ph->packet_id);
    
    len = nfq_get_payload(nfa, &payload);
    if (len >= 0) {
        printf("Packet received, len=%d\n", len);
        // 检查包内容...
    }
    
    // 返回裁决
    return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
    // 或修改包后返回
    // return nfq_set_verdict(qh, id, NF_ACCEPT, len, modified_payload);
}

int main()
{
    struct nfq_handle *h;
    struct nfq_q_handle *qh;
    int fd, rv;
    char buf[4096];
    
    // 打开库句柄
    h = nfq_open();
    
    // 绑定到AF_INET
    nfq_bind_pf(h, AF_INET);
    
    // 创建队列
    qh = nfq_create_queue(h, 0, &callback, NULL);
    
    // 设置复制模式
    nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff);
    
    // 获取文件描述符
    fd = nfq_fd(h);
    
    // 主循环
    while ((rv = recv(fd, buf, sizeof(buf), 0)) >= 0) {
        nfq_handle_packet(h, buf, rv);
    }
    
    nfq_destroy_queue(qh);
    nfq_close(h);
    return 0;
}
```

iptables配置：
```bash
# 将包发送到队列0
iptables -A INPUT -p tcp --dport 80 -j NFQUEUE --queue-num 0

# 多队列负载均衡
iptables -A INPUT -j NFQUEUE --queue-balance 0:3

# 队列满时的行为
iptables -A INPUT -j NFQUEUE --queue-num 0 --queue-bypass  # 跳过
# 默认是丢弃
```
[Intermediate]

---

## 11. Netfilter Debugging (Netfilter调试)

---

Q: How to debug Netfilter issues?
A: Netfilter调试方法：

```bash
# 查看规则
iptables -L -n -v --line-numbers
iptables -t nat -L -n -v
nft list ruleset

# 查看计数器
iptables -L -n -v -x    # 精确计数
watch -n 1 "iptables -L -n -v"   # 实时监控

# 日志规则（调试用）
iptables -A INPUT -j LOG --log-prefix "INPUT: " --log-level 4
iptables -A FORWARD -j LOG --log-prefix "FORWARD: "

# nftables日志
nft add rule inet filter input log prefix \"INPUT: \"

# 查看日志
dmesg | grep -E "INPUT:|FORWARD:"
journalctl -f -k | grep -E "INPUT:|FORWARD:"

# 连接跟踪调试
cat /proc/net/nf_conntrack
conntrack -L
conntrack -E    # 实时事件

# 连接跟踪统计
conntrack -S
cat /proc/sys/net/netfilter/nf_conntrack_count

# 追踪特定包
iptables -t raw -A PREROUTING -p tcp --dport 80 -j TRACE
iptables -t raw -A OUTPUT -p tcp --sport 80 -j TRACE
# 查看/var/log/kern.log 或 dmesg

# 内核追踪
echo 1 > /sys/kernel/debug/tracing/events/nf_tables/enable
cat /sys/kernel/debug/tracing/trace_pipe

# tcpdump配合
tcpdump -i any -n port 80

# nftables调试
nft monitor trace
```

常见问题排查：
```bash
# 1. 规则不生效
# 检查链的默认策略
iptables -L INPUT | head -1

# 检查规则顺序
iptables -L INPUT -n --line-numbers

# 2. NAT不工作
# 检查IP转发
cat /proc/sys/net/ipv4/ip_forward

# 检查NAT表
iptables -t nat -L -n -v

# 3. 连接跟踪表满
cat /proc/sys/net/netfilter/nf_conntrack_max
cat /proc/sys/net/netfilter/nf_conntrack_count

# 4. 性能问题
# 检查规则数量
iptables-save | wc -l

# 使用ipset优化大量IP匹配
ipset create myset hash:ip
ipset add myset 192.168.1.100
iptables -A INPUT -m set --match-set myset src -j DROP
```

调试模块加载：
```bash
# 查看已加载模块
lsmod | grep nf_
lsmod | grep xt_

# 手动加载模块
modprobe nf_conntrack
modprobe iptable_filter
modprobe nf_log_ipv4

# 查看模块参数
modinfo nf_conntrack
cat /sys/module/nf_conntrack/parameters/hashsize
```
[Basic]

---

## 12. Summary (总结)

---

Q: Summarize the Netfilter architecture.
A: 
```
+==================================================================+
||                  NETFILTER ARCHITECTURE                        ||
+==================================================================+

钩子点:
+------------------------------------------------------------------+
|                                                                  |
|  PRE_ROUTING --> INPUT    --> Local Process                      |
|       |                                                          |
|       +--> FORWARD --------+                                     |
|                            |                                     |
|                            v                                     |
|  Local Process --> OUTPUT --> POST_ROUTING --> 出口              |
|                                                                  |
+------------------------------------------------------------------+


表和链:
+------------------------------------------------------------------+
|  raw表:       PREROUTING, OUTPUT                                 |
|  mangle表:    PREROUTING, INPUT, FORWARD, OUTPUT, POSTROUTING    |
|  nat表:       PREROUTING, INPUT, OUTPUT, POSTROUTING             |
|  filter表:    INPUT, FORWARD, OUTPUT                             |
|  security表:  INPUT, FORWARD, OUTPUT                             |
+------------------------------------------------------------------+


优先级顺序 (同一钩子点):
+------------------------------------------------------------------+
|  conntrack_defrag (-400) ->                                      |
|  raw (-300) ->                                                   |
|  conntrack (-200) ->                                             |
|  mangle (-150) ->                                                |
|  nat_dst (-100) ->                                               |
|  filter (0) ->                                                   |
|  security (50) ->                                                |
|  nat_src (100) ->                                                |
|  conntrack_confirm (MAX)                                         |
+------------------------------------------------------------------+


核心组件:
+------------------------------------------------------------------+
|  1. Hook System       - 数据包路径中的拦截点                      |
|  2. Connection Track  - 跟踪连接状态                             |
|  3. NAT               - 地址/端口转换                            |
|  4. Match/Target      - 规则匹配和动作                           |
|  5. NFQUEUE           - 用户空间裁决                             |
+------------------------------------------------------------------+


数据结构:
+------------------------------------------------------------------+
|  nf_hook_ops         - 钩子注册                                  |
|  nf_conn             - 连接跟踪条目                              |
|  nf_conntrack_tuple  - 连接标识(五元组)                          |
|  xt_match/xt_target  - 匹配/目标模块                             |
+------------------------------------------------------------------+


工具对比:
+------------------------------------------------------------------+
|             iptables              |           nftables           |
|-----------------------------------|------------------------------|
|  iptables, ip6tables, arptables   |  nft (统一工具)              |
|  固定表/链                        |  可自定义                    |
|  xtables框架                      |  nf_tables框架              |
|  线性规则匹配                     |  集合、映射优化              |
+------------------------------------------------------------------+


关键API:
+------------------------------------------------------------------+
|  nf_register_net_hook()    - 注册钩子                            |
|  nf_unregister_net_hook()  - 注销钩子                            |
|  nf_ct_get()               - 获取连接跟踪                        |
|  nf_nat_packet()           - 执行NAT                             |
|  nf_ct_expect_related()    - 创建期望连接                        |
+------------------------------------------------------------------+
```
[Basic]

---

*Total: 100+ cards covering Linux kernel Netfilter implementation*

