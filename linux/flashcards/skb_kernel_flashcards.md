# Socket Buffer (sk_buff) Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel sk_buff structure, operations, and network buffer management
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. sk_buff Overview (sk_buff概述)

---

Q: What is sk_buff and why is it important?
A: sk_buff是Linux网络栈的核心数据结构：

```
+==================================================================+
||                    SK_BUFF OVERVIEW                            ||
+==================================================================+

设计目标:
+------------------------------------------------------------------+
| 1. 高效的数据包管理                                               |
|    - 避免数据复制                                                 |
|    - 支持协议头的添加/删除                                        |
|                                                                  |
| 2. 协议层之间传递数据                                             |
|    - 从网卡驱动到应用层                                           |
|    - 从应用层到网卡驱动                                           |
|                                                                  |
| 3. 支持多种网络特性                                               |
|    - 分片和重组                                                   |
|    - 校验和卸载                                                   |
|    - 分散/聚集I/O                                                 |
|                                                                  |
| 4. 内存高效                                                       |
|    - 克隆共享数据                                                 |
|    - 引用计数管理                                                 |
+------------------------------------------------------------------+

sk_buff在网络栈中的角色:
+------------------------------------------------------------------+
|                                                                  |
|  Application Layer                                               |
|         |                                                        |
|         v                                                        |
|  +-------------+                                                 |
|  | Socket API  |                                                 |
|  +------+------+                                                 |
|         |                                                        |
|         v                                                        |
|  +------+------+                                                 |
|  |   sk_buff   | <-- 核心数据结构，贯穿整个网络栈                 |
|  +------+------+                                                 |
|         |                                                        |
|    +----+----+----+----+                                         |
|    |         |         |                                         |
|    v         v         v                                         |
|  +---+    +---+    +------+                                      |
|  |TCP|    |UDP|    | ICMP |                                      |
|  +---+    +---+    +------+                                      |
|    |         |         |                                         |
|    +----+----+----+----+                                         |
|         |                                                        |
|         v                                                        |
|  +------+------+                                                 |
|  |     IP      |                                                 |
|  +------+------+                                                 |
|         |                                                        |
|         v                                                        |
|  +------+------+                                                 |
|  | Net Device  |                                                 |
|  +-------------+                                                 |
|                                                                  |
+------------------------------------------------------------------+
```
[Basic]

---

Q: What are the main components of sk_buff?
A: sk_buff的主要组成部分：

```
+==================================================================+
||                  SK_BUFF COMPONENTS                            ||
+==================================================================+

+------------------------------------------------------------------+
|                                                                  |
|  struct sk_buff                                                  |
|  +----------------------------------------------------------+   |
|  |                    控制信息                               |   |
|  |  - 链表指针 (next, prev)                                 |   |
|  |  - 设备信息 (dev, input_dev)                             |   |
|  |  - socket关联 (sk, destructor)                           |   |
|  |  - 时间戳、优先级、标记等                                 |   |
|  +----------------------------------------------------------+   |
|  |                    数据指针                               |   |
|  |  - head: 缓冲区起始                                      |   |
|  |  - data: 数据起始                                        |   |
|  |  - tail: 数据结束                                        |   |
|  |  - end:  缓冲区结束                                      |   |
|  +----------------------------------------------------------+   |
|  |                    长度信息                               |   |
|  |  - len: 数据总长度                                       |   |
|  |  - data_len: 非线性数据长度                              |   |
|  |  - mac_len, hdr_len: 头部长度                            |   |
|  +----------------------------------------------------------+   |
|  |                    协议信息                               |   |
|  |  - protocol: 以太网协议类型                              |   |
|  |  - transport_header: L4头偏移                            |   |
|  |  - network_header: L3头偏移                              |   |
|  |  - mac_header: L2头偏移                                  |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|  +----------------------------------------------------------+   |
|  |              Data Buffer (数据缓冲区)                     |   |
|  |  +------------------------------------------------------+|   |
|  |  | headroom | headers | payload | tailroom              ||   |
|  |  +------------------------------------------------------+|   |
|  |  ^          ^                   ^                      ^ |   |
|  |  head       data                tail                  end|   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|  +----------------------------------------------------------+   |
|  |           skb_shared_info (共享信息)                      |   |
|  |  - 分片数组 (frags[])                                    |   |
|  |  - 分片链表 (frag_list)                                  |   |
|  |  - GSO信息                                               |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```
[Basic]

---

## 2. sk_buff Structure (sk_buff结构详解)

---

Q: What is the detailed structure of sk_buff?
A: sk_buff结构详解：

```c
// include/linux/skbuff.h
struct sk_buff {
    /*=== 链表管理 ===*/
    union {
        struct {
            struct sk_buff *next;      // 队列中的下一个
            struct sk_buff *prev;      // 队列中的上一个
        };
        struct rb_node rbnode;         // 红黑树节点（用于TCP）
        struct list_head list;         // 通用链表
    };
    
    union {
        struct sock     *sk;           // 关联的socket
        int             ip_defrag_offset;
    };
    
    /*=== 时间和设备 ===*/
    ktime_t             tstamp;        // 时间戳
    struct net_device   *dev;          // 接收/发送设备
    
    /*=== 路由/目的地 ===*/
    unsigned long       _skb_refdst;   // 路由缓存
    
    /*=== 析构函数 ===*/
    void (*destructor)(struct sk_buff *skb);
    
    /*=== 连接跟踪 ===*/
#if IS_ENABLED(CONFIG_NF_CONNTRACK)
    unsigned long        _nfct;
#endif
    
    /*=== 长度信息 ===*/
    unsigned int        len;           // 数据总长度(线性+非线性)
    unsigned int        data_len;      // 非线性数据长度
    __u16               mac_len;       // MAC头长度
    __u16               hdr_len;       // 克隆时可写头部长度
    
    /*=== 队列映射 ===*/
    __u16               queue_mapping;
    
    /*=== 校验和 ===*/
    __u8                ip_summed:2;   // 校验和状态
    __u8                ooo_okay:1;    // 允许乱序
    __u8                l4_hash:1;
    __u8                sw_hash:1;
    __u8                wifi_acked_valid:1;
    __u8                wifi_acked:1;
    __u8                no_fcs:1;
    __u8                encapsulation:1;
    __u8                encap_hdr_csum:1;
    __u8                csum_valid:1;
    
    /*=== 协议相关 ===*/
    __be16              protocol;      // 协议类型
    __u16               transport_header;   // L4头偏移
    __u16               network_header;     // L3头偏移
    __u16               mac_header;         // L2头偏移
    
    /*=== 数据指针 ===*/
    sk_buff_data_t      tail;          // 数据尾
    sk_buff_data_t      end;           // 缓冲区尾
    unsigned char       *head;         // 缓冲区头
    unsigned char       *data;         // 数据头
    
    /*=== 缓冲区大小 ===*/
    unsigned int        truesize;      // 总占用内存
    
    /*=== 引用计数 ===*/
    refcount_t          users;         // 引用计数
    
    // ... 更多字段
};
```

关键字段说明：
| 字段 | 说明 |
|------|------|
| len | 整个数据包长度（线性+非线性） |
| data_len | 非线性部分长度（分片） |
| head/data/tail/end | 缓冲区和数据边界 |
| protocol | 以太网帧类型（如ETH_P_IP） |
| ip_summed | 校验和卸载状态 |
| truesize | skb占用的总内存 |
[Intermediate]

---

Q: How do the data pointers (head, data, tail, end) work?
A: 数据指针的工作原理：

```
+==================================================================+
||                  SK_BUFF DATA POINTERS                         ||
+==================================================================+

初始状态（分配后）:
+------------------------------------------------------------------+
|                                                                  |
|  head                                                     end    |
|    |                                                       |     |
|    v                                                       v     |
|    +-------------------------------------------------------+     |
|    |                   空闲空间                             |     |
|    +-------------------------------------------------------+     |
|    ^                                                       ^     |
|    |                                                       |     |
|  data                                                    tail    |
|                                                                  |
|  len = 0, data_len = 0                                          |
|                                                                  |
+------------------------------------------------------------------+

调用 skb_reserve(skb, headroom) 后:
+------------------------------------------------------------------+
|                                                                  |
|  head                                                     end    |
|    |                                                       |     |
|    v                                                       v     |
|    +----------+--------------------------------------------+     |
|    | headroom |              空闲空间                       |     |
|    +----------+--------------------------------------------+     |
|               ^                                            ^     |
|               |                                            |     |
|             data                                         tail    |
|                                                                  |
|  预留空间用于之后添加协议头                                       |
|                                                                  |
+------------------------------------------------------------------+

调用 skb_put(skb, len) 添加数据后:
+------------------------------------------------------------------+
|                                                                  |
|  head                                                     end    |
|    |                                                       |     |
|    v                                                       v     |
|    +----------+----------------+---------------------------+     |
|    | headroom |    payload     |        tailroom           |     |
|    +----------+----------------+---------------------------+     |
|               ^                ^                                 |
|               |                |                                 |
|             data             tail                                |
|                                                                  |
|  len = payload_size                                             |
|                                                                  |
+------------------------------------------------------------------+

调用 skb_push(skb, hdr_len) 添加头部后:
+------------------------------------------------------------------+
|                                                                  |
|  head                                                     end    |
|    |                                                       |     |
|    v                                                       v     |
|    +----+------+----------------+---------------------------+    |
|    |free| hdr  |    payload     |        tailroom           |    |
|    +----+------+----------------+---------------------------+    |
|         ^                       ^                                |
|         |                       |                                |
|       data                    tail                               |
|                                                                  |
|  len = hdr_len + payload_size                                   |
|  data指针向前移动                                                 |
|                                                                  |
+------------------------------------------------------------------+

调用 skb_pull(skb, hdr_len) 移除头部后:
+------------------------------------------------------------------+
|                                                                  |
|  head                                                     end    |
|    |                                                       |     |
|    v                                                       v     |
|    +----+------+----------------+---------------------------+    |
|    |    | hdr  |    payload     |        tailroom           |    |
|    +----+------+----------------+---------------------------+    |
|                ^                ^                                |
|                |                |                                |
|              data             tail                               |
|                                                                  |
|  len = payload_size                                             |
|  data指针向后移动，头部被"跳过"                                   |
|                                                                  |
+------------------------------------------------------------------+
```

数据指针操作总结：
| 操作 | data指针 | tail指针 | len变化 |
|------|----------|----------|---------|
| skb_reserve | 后移 | 后移 | 不变 |
| skb_put | 不变 | 后移 | 增加 |
| skb_push | 前移 | 不变 | 增加 |
| skb_pull | 后移 | 不变 | 减少 |
[Intermediate]

---

## 3. sk_buff Allocation (sk_buff分配)

---

Q: How to allocate sk_buff?
A: sk_buff分配方法：

```c
// 主要分配函数

// 1. 基本分配（通用）
struct sk_buff *alloc_skb(unsigned int size, gfp_t priority);

// 2. 网络设备分配（推荐用于接收）
struct sk_buff *netdev_alloc_skb(struct net_device *dev,
                                  unsigned int length);

// 3. 带IP对齐的分配
struct sk_buff *netdev_alloc_skb_ip_align(struct net_device *dev,
                                           unsigned int length);

// 4. NAPI分配（在NAPI上下文中更高效）
struct sk_buff *napi_alloc_skb(struct napi_struct *napi,
                                unsigned int length);

// 5. 构建skb（使用已有缓冲区）
struct sk_buff *build_skb(void *data, unsigned int frag_size);

// 6. 从page构建
struct sk_buff *__build_skb_around(struct sk_buff *skb,
                                    void *data, unsigned int frag_size);
```

分配示例：
```c
// 发送路径 - 分配skb
struct sk_buff *skb;
int len = ETH_HLEN + sizeof(struct iphdr) + sizeof(struct tcphdr) + payload_len;

skb = alloc_skb(len + NET_SKB_PAD, GFP_KERNEL);
if (!skb)
    return -ENOMEM;

// 预留headroom用于可能的封装
skb_reserve(skb, NET_SKB_PAD);

// 预留以太网头空间
skb_reserve(skb, ETH_HLEN);

// 现在可以构建IP头
struct iphdr *iph = skb_push(skb, sizeof(struct iphdr));
// 填充IP头...


// 接收路径 - 驱动中分配
static int my_rx(struct net_device *dev)
{
    struct sk_buff *skb;
    int len = received_len;
    
    // 使用netdev_alloc_skb_ip_align确保IP头对齐
    skb = netdev_alloc_skb_ip_align(dev, len);
    if (!skb) {
        dev->stats.rx_dropped++;
        return -ENOMEM;
    }
    
    // DMA数据到skb
    memcpy(skb_put(skb, len), rx_buffer, len);
    
    // 设置协议
    skb->protocol = eth_type_trans(skb, dev);
    
    // 传递给协议栈
    netif_receive_skb(skb);
    
    return 0;
}
```

内存布局：
```c
// alloc_skb分配的内存布局
+------------------------------------------------------------------+
|  struct sk_buff  |  data buffer  |  skb_shared_info             |
+------------------------------------------------------------------+
       ~240B            size             ~320B

// skb_shared_info位于end指针处
struct skb_shared_info *shinfo = skb_shinfo(skb);
```
[Intermediate]

---

Q: How to free sk_buff?
A: sk_buff释放方法：

```c
// 释放函数

// 1. 减少引用计数，可能释放
void kfree_skb(struct sk_buff *skb);

// 2. 用于发送完成（正常路径）
void dev_kfree_skb(struct sk_buff *skb);

// 3. 可在任意上下文调用（中断安全）
void dev_kfree_skb_any(struct sk_buff *skb);

// 4. 用于IRQ上下文
void dev_kfree_skb_irq(struct sk_buff *skb);

// 5. 消费skb（减少引用计数，总是释放）
void consume_skb(struct sk_buff *skb);

// 6. 释放skb但保留数据
void __kfree_skb(struct sk_buff *skb);
```

引用计数：
```c
// 引用计数操作
void skb_get(struct sk_buff *skb);     // 增加引用
bool skb_unref(struct sk_buff *skb);   // 减少引用，返回是否为0

// 检查引用计数
bool skb_shared(const struct sk_buff *skb);   // 是否被共享

// 确保唯一引用（如果共享则复制）
struct sk_buff *skb_share_check(struct sk_buff *skb, gfp_t pri);
```

释放流程：
```c
// kfree_skb实现概要
void kfree_skb(struct sk_buff *skb)
{
    if (!skb)
        return;
    
    // 减少引用计数
    if (likely(refcount_dec_and_test(&skb->users))) {
        // 调用析构函数
        if (skb->destructor)
            skb->destructor(skb);
        
        // 释放分片
        skb_release_data(skb);
        
        // 释放skb本身
        kfree_skbmem(skb);
    }
}

// 释放分片数据
static void skb_release_data(struct sk_buff *skb)
{
    struct skb_shared_info *shinfo = skb_shinfo(skb);
    int i;
    
    // 释放分片页面
    for (i = 0; i < shinfo->nr_frags; i++)
        __skb_frag_unref(&shinfo->frags[i]);
    
    // 释放frag_list
    if (shinfo->frag_list)
        kfree_skb_list(shinfo->frag_list);
    
    // 释放头部数据
    skb_free_head(skb);
}
```

最佳实践：
```c
// 发送完成时
static void my_tx_complete(struct net_device *dev, struct sk_buff *skb)
{
    // DMA解映射
    dma_unmap_single(&dev->dev, tx_dma, skb->len, DMA_TO_DEVICE);
    
    // 释放skb
    dev_kfree_skb_any(skb);  // 可能在中断或进程上下文
}

// 接收错误时
static void my_rx_error(struct sk_buff *skb)
{
    kfree_skb(skb);  // 记录为丢弃
}

// 正常消费时
static void my_consume(struct sk_buff *skb)
{
    // 处理数据...
    consume_skb(skb);  // 不记录为丢弃
}
```
[Intermediate]

---

## 4. Data Manipulation Operations (数据操作)

---

Q: How do skb_put, skb_push, skb_pull, and skb_reserve work?
A: 四个核心数据操作函数：

```c
/*=== skb_reserve - 预留头部空间 ===*/
// 在数据区前预留空间，用于之后添加协议头
static inline void skb_reserve(struct sk_buff *skb, int len)
{
    skb->data += len;
    skb->tail += len;
}

// 使用示例：
skb = alloc_skb(size, GFP_KERNEL);
skb_reserve(skb, NET_IP_ALIGN);        // IP头对齐
skb_reserve(skb, LL_RESERVED_SPACE(dev)); // 链路层空间


/*=== skb_put - 在尾部添加数据 ===*/
// 扩展数据区，返回旧tail位置（可以写入数据）
void *skb_put(struct sk_buff *skb, unsigned int len)
{
    void *tmp = skb_tail_pointer(skb);
    skb->tail += len;
    skb->len += len;
    return tmp;
}

// 使用示例：
unsigned char *data = skb_put(skb, payload_len);
memcpy(data, payload, payload_len);

// 带零初始化
void *skb_put_zero(struct sk_buff *skb, unsigned int len);

// 复制数据
void *skb_put_data(struct sk_buff *skb, const void *data, unsigned int len);


/*=== skb_push - 在头部添加协议头 ===*/
// 扩展数据区向前，返回新的data位置
void *skb_push(struct sk_buff *skb, unsigned int len)
{
    skb->data -= len;
    skb->len += len;
    return skb->data;
}

// 使用示例：
struct iphdr *iph = skb_push(skb, sizeof(struct iphdr));
iph->version = 4;
iph->ihl = 5;
// ...


/*=== skb_pull - 移除头部（向上层传递时）===*/
// 缩短数据区，跳过协议头
void *skb_pull(struct sk_buff *skb, unsigned int len)
{
    skb->len -= len;
    if (skb->len < skb->data_len)
        BUG();
    return skb->data += len;
}

// 使用示例：
// 在IP层接收时，跳过IP头
skb_pull(skb, ip_hdrlen(skb));

// 安全版本（检查长度）
void *skb_pull_inline(struct sk_buff *skb, unsigned int len);
```

协议栈中的典型使用：
```
发送方向 (从上到下):

应用层数据:
    +------------------+
    |     payload      |
    +------------------+
          
TCP处理 (skb_push):
    +------+------------------+
    | TCP  |     payload      |
    +------+------------------+
          
IP处理 (skb_push):
    +----+------+------------------+
    | IP | TCP  |     payload      |
    +----+------+------------------+
          
以太网处理 (skb_push):
    +-----+----+------+------------------+
    | ETH | IP | TCP  |     payload      |
    +-----+----+------+------------------+


接收方向 (从下到上):

以太网帧:
    +-----+----+------+------------------+
    | ETH | IP | TCP  |     payload      |
    +-----+----+------+------------------+
          
以太网处理 (skb_pull ETH):
    +----+------+------------------+
    | IP | TCP  |     payload      |
    +----+------+------------------+
          
IP处理 (skb_pull IP):
    +------+------------------+
    | TCP  |     payload      |
    +------+------------------+
          
TCP处理 (skb_pull TCP):
    +------------------+
    |     payload      |
    +------------------+
```
[Intermediate]

---

Q: What are the safe versions of skb operations?
A: 安全版本的skb操作：

```c
// 检查空间是否足够
static inline int skb_headroom(const struct sk_buff *skb)
{
    return skb->data - skb->head;
}

static inline int skb_tailroom(const struct sk_buff *skb)
{
    return skb_is_nonlinear(skb) ? 0 : skb->end - skb->tail;
}

// 安全的put操作
static inline void *__skb_put(struct sk_buff *skb, unsigned int len)
{
    void *tmp = skb_tail_pointer(skb);
    SKB_LINEAR_ASSERT(skb);
    skb->tail += len;
    skb->len += len;
    return tmp;
}

// 带检查的版本
void *skb_put(struct sk_buff *skb, unsigned int len)
{
    void *tmp = skb_tail_pointer(skb);
    SKB_LINEAR_ASSERT(skb);
    if (skb_tailroom(skb) < len)
        skb_over_panic(skb, len, __func__);
    skb->tail += len;
    skb->len += len;
    return tmp;
}

// 扩展headroom（如果不够会重新分配）
int pskb_expand_head(struct sk_buff *skb, int nhead, int ntail, gfp_t gfp_mask);

// 确保头部空间足够
static inline int skb_cow_head(struct sk_buff *skb, unsigned int headroom)
{
    int delta = 0;
    
    if (headroom > skb_headroom(skb))
        delta = headroom - skb_headroom(skb);
    
    if (delta || skb_cloned(skb))
        return pskb_expand_head(skb, ALIGN(delta, NET_SKB_PAD), 0, GFP_ATOMIC);
    
    return 0;
}

// 使用示例
static int my_xmit(struct sk_buff *skb, struct net_device *dev)
{
    // 确保有足够的头部空间
    if (skb_cow_head(skb, LL_RESERVED_SPACE(dev))) {
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }
    
    // 现在可以安全地添加头部
    struct ethhdr *eth = skb_push(skb, ETH_HLEN);
    // ...
}
```

边界检查宏：
```c
// 检查是否是线性skb
#define SKB_LINEAR_ASSERT(skb)  BUG_ON(skb_is_nonlinear(skb))

// 空间不足时的panic
void skb_over_panic(struct sk_buff *skb, unsigned int len, void *here);
void skb_under_panic(struct sk_buff *skb, unsigned int len, void *here);
```
[Intermediate]

---

## 5. Protocol Header Access (协议头访问)

---

Q: How to access protocol headers in sk_buff?
A: 协议头访问方法：

```c
// 头部偏移存储
struct sk_buff {
    __u16 transport_header;   // L4头偏移
    __u16 network_header;     // L3头偏移
    __u16 mac_header;         // L2头偏移
    // ...
};

// 设置头部偏移
static inline void skb_reset_mac_header(struct sk_buff *skb)
{
    skb->mac_header = skb->data - skb->head;
}

static inline void skb_reset_network_header(struct sk_buff *skb)
{
    skb->network_header = skb->data - skb->head;
}

static inline void skb_reset_transport_header(struct sk_buff *skb)
{
    skb->transport_header = skb->data - skb->head;
}

// 设置相对偏移
static inline void skb_set_network_header(struct sk_buff *skb, const int offset)
{
    skb->network_header = skb->data - skb->head + offset;
}

// 获取头部指针
static inline unsigned char *skb_mac_header(const struct sk_buff *skb)
{
    return skb->head + skb->mac_header;
}

static inline unsigned char *skb_network_header(const struct sk_buff *skb)
{
    return skb->head + skb->network_header;
}

static inline unsigned char *skb_transport_header(const struct sk_buff *skb)
{
    return skb->head + skb->transport_header;
}

// 类型转换的便捷宏
#define ip_hdr(skb)     ((struct iphdr *)skb_network_header(skb))
#define ipv6_hdr(skb)   ((struct ipv6hdr *)skb_network_header(skb))
#define tcp_hdr(skb)    ((struct tcphdr *)skb_transport_header(skb))
#define udp_hdr(skb)    ((struct udphdr *)skb_transport_header(skb))
#define icmp_hdr(skb)   ((struct icmphdr *)skb_transport_header(skb))
#define eth_hdr(skb)    ((struct ethhdr *)skb_mac_header(skb))
```

使用示例：
```c
// 接收处理
static int my_receive(struct sk_buff *skb)
{
    struct ethhdr *eth;
    struct iphdr *iph;
    struct tcphdr *tcph;
    
    // 设置MAC头位置
    skb_reset_mac_header(skb);
    
    // 跳过MAC头
    skb_pull(skb, ETH_HLEN);
    
    // 设置网络头位置
    skb_reset_network_header(skb);
    
    // 获取IP头
    iph = ip_hdr(skb);
    pr_info("Protocol: %u, src: %pI4, dst: %pI4\n",
            iph->protocol, &iph->saddr, &iph->daddr);
    
    // 设置传输层头
    skb_set_transport_header(skb, iph->ihl * 4);
    
    // 获取TCP头
    if (iph->protocol == IPPROTO_TCP) {
        tcph = tcp_hdr(skb);
        pr_info("TCP: src_port=%u, dst_port=%u\n",
                ntohs(tcph->source), ntohs(tcph->dest));
    }
    
    return 0;
}

// 发送处理
static void my_build_headers(struct sk_buff *skb)
{
    struct iphdr *iph;
    struct tcphdr *tcph;
    
    // 添加TCP头
    tcph = skb_push(skb, sizeof(struct tcphdr));
    skb_reset_transport_header(skb);
    // 填充TCP头...
    
    // 添加IP头
    iph = skb_push(skb, sizeof(struct iphdr));
    skb_reset_network_header(skb);
    // 填充IP头...
}
```
[Intermediate]

---

## 6. sk_buff Clone and Copy (克隆和复制)

---

Q: What is the difference between skb_clone and skb_copy?
A: 克隆和复制的区别：

```
+==================================================================+
||                  SKB CLONE VS COPY                             ||
+==================================================================+

skb_clone (克隆):
+------------------------------------------------------------------+
|  - 创建新的sk_buff结构                                            |
|  - 共享数据缓冲区（引用计数增加）                                  |
|  - 头部不可写（需要cow_head）                                     |
|  - 快速，不复制数据                                               |
|  - 用于需要多个引用同一数据的场景                                  |
+------------------------------------------------------------------+

  Original skb        Cloned skb
  +-----------+       +-----------+
  | sk_buff   |       | sk_buff   |
  |           |       |           |
  | head -----+------>| head -----+--+
  | data -----+--+    | data -----+--+---+
  | tail      |  |    | tail      |      |
  | end       |  |    | end       |      |
  +-----------+  |    +-----------+      |
                 |                       |
                 v                       v
              +-----------------------------+
              |     Shared Data Buffer      |
              |  (dataref = 2)              |
              +-----------------------------+


skb_copy (复制):
+------------------------------------------------------------------+
|  - 创建新的sk_buff结构                                            |
|  - 创建新的数据缓冲区                                             |
|  - 复制所有数据                                                   |
|  - 完全独立，可修改                                               |
|  - 较慢，需要复制数据                                             |
+------------------------------------------------------------------+

  Original skb        Copied skb
  +-----------+       +-----------+
  | sk_buff   |       | sk_buff   |
  |           |       |           |
  | head -----+--+    | head -----+--+
  | data      |  |    | data      |  |
  | tail      |  |    | tail      |  |
  | end       |  |    | end       |  |
  +-----------+  |    +-----------+  |
                 |                   |
                 v                   v
              +--------+         +--------+
              | Data 1 |         | Data 2 |
              | Buffer |         | Buffer |
              +--------+         +--------+
              (独立)             (独立副本)
```

API函数：
```c
// 克隆
struct sk_buff *skb_clone(struct sk_buff *skb, gfp_t priority);

// 完整复制
struct sk_buff *skb_copy(const struct sk_buff *skb, gfp_t priority);

// 复制并扩展头部空间
struct sk_buff *skb_copy_expand(const struct sk_buff *skb,
                                 int newheadroom, int newtailroom,
                                 gfp_t priority);

// 只复制头部（线性数据），共享分片
struct sk_buff *pskb_copy(struct sk_buff *skb, gfp_t gfp_mask);

// 检查是否被克隆
static inline int skb_cloned(const struct sk_buff *skb)
{
    return skb->cloned && refcount_read(&skb_shinfo(skb)->dataref) != 1;
}

// 确保头部可写
int skb_cow_head(struct sk_buff *skb, unsigned int headroom);
```

使用场景：
```c
// 场景1：多播发送（需要向多个接口发送同一数据）
void multicast_send(struct sk_buff *skb, struct list_head *dests)
{
    struct sk_buff *clone;
    struct dest *d;
    
    list_for_each_entry(d, dests, list) {
        if (list_is_last(&d->list, dests)) {
            // 最后一个目的地使用原skb
            dev_queue_xmit(skb);
        } else {
            // 其他目的地使用克隆
            clone = skb_clone(skb, GFP_ATOMIC);
            if (clone)
                dev_queue_xmit(clone);
        }
    }
}

// 场景2：需要修改数据
void modify_packet(struct sk_buff *skb)
{
    // 如果被共享，需要复制
    if (skb_shared(skb)) {
        struct sk_buff *nskb = skb_copy(skb, GFP_ATOMIC);
        if (!nskb)
            return;
        kfree_skb(skb);
        skb = nskb;
    }
    
    // 现在可以安全修改
    // ...
}

// 场景3：修改头部
int add_header(struct sk_buff *skb)
{
    // 确保头部可写
    if (skb_cow_head(skb, MY_HDR_LEN))
        return -ENOMEM;
    
    // 添加头部
    my_hdr = skb_push(skb, MY_HDR_LEN);
    // ...
}
```
[Intermediate]

---

## 7. Shared Info and Fragments (共享信息和分片)

---

Q: What is skb_shared_info?
A: `skb_shared_info`存储共享信息和分片数据：

```c
// include/linux/skbuff.h
struct skb_shared_info {
    __u8        __unused;
    __u8        meta_len;
    __u8        nr_frags;           // 分片数量
    __u8        tx_flags;           // 发送标志
    unsigned short  gso_size;       // GSO段大小
    unsigned short  gso_segs;       // GSO段数
    struct sk_buff  *frag_list;     // 分片链表
    
    struct skb_shared_hwtstamps hwtstamps;  // 硬件时间戳
    
    unsigned int    gso_type;       // GSO类型
    u32             tskey;          // 时间戳key
    
    atomic_t        dataref;        // 数据引用计数
    
    // 分片数组
    skb_frag_t      frags[MAX_SKB_FRAGS];  // 最多17个分片
};

// 分片结构
typedef struct skb_frag_struct skb_frag_t;
struct skb_frag_struct {
    struct {
        struct page *p;
    } bvec;
    unsigned int page_offset;       // 页内偏移
    unsigned int size;              // 分片大小
};

// 获取shared_info
#define skb_shinfo(SKB) ((struct skb_shared_info *)(skb_end_pointer(SKB)))
```

分片操作：
```c
// 添加分片
void skb_fill_page_desc(struct sk_buff *skb, int i,
                        struct page *page, int off, int size)
{
    skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
    
    __skb_frag_set_page(frag, page);
    skb_frag_off_set(frag, off);
    skb_frag_size_set(frag, size);
    
    skb_shinfo(skb)->nr_frags = i + 1;
}

// 获取分片页面
static inline struct page *skb_frag_page(const skb_frag_t *frag)
{
    return frag->bvec.p;
}

// 获取分片大小
static inline unsigned int skb_frag_size(const skb_frag_t *frag)
{
    return frag->size;
}

// 遍历分片
int i;
for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
    skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
    struct page *page = skb_frag_page(frag);
    unsigned int offset = skb_frag_off(frag);
    unsigned int size = skb_frag_size(frag);
    
    // 处理分片数据...
}
```

线性vs非线性数据：
```
+==================================================================+
||                LINEAR VS NON-LINEAR DATA                       ||
+==================================================================+

线性数据:
+------------------------------------------------------------------+
|  sk_buff                                                         |
|  +------+                                                        |
|  | head |---> +----------------------------------------+         |
|  | data |---> |           线性数据                      |         |
|  | tail |---> +----------------------------------------+         |
|  | end  |---> | skb_shared_info                        |         |
|  +------+     | nr_frags = 0                           |         |
|               +----------------------------------------+         |
|                                                                  |
|  len = 数据大小                                                  |
|  data_len = 0                                                    |
+------------------------------------------------------------------+

非线性数据 (有分片):
+------------------------------------------------------------------+
|  sk_buff                                                         |
|  +------+                                                        |
|  | head |---> +----------------+                                 |
|  | data |---> |  线性头部      |                                 |
|  | tail |---> +----------------+                                 |
|  | end  |---> | skb_shared_info|                                 |
|  +------+     | nr_frags = 3   |                                 |
|               | frags[0] ------+--> [Page 0][off][size]          |
|               | frags[1] ------+--> [Page 1][off][size]          |
|               | frags[2] ------+--> [Page 2][off][size]          |
|               +----------------+                                 |
|                                                                  |
|  len = 线性长度 + 所有分片长度                                    |
|  data_len = 所有分片长度                                         |
|                                                                  |
+------------------------------------------------------------------+
```

线性化操作：
```c
// 检查是否非线性
static inline bool skb_is_nonlinear(const struct sk_buff *skb)
{
    return skb->data_len;
}

// 线性化（将分片数据复制到线性缓冲区）
int skb_linearize(struct sk_buff *skb);

// 部分线性化
int __pskb_pull_tail(struct sk_buff *skb, int delta);

// 使用示例
if (skb_is_nonlinear(skb)) {
    if (skb_linearize(skb))
        return -ENOMEM;
}
// 现在可以安全地作为连续内存访问
```
[Advanced]

---

## 8. Checksum Handling (校验和处理)

---

Q: How does sk_buff handle checksums?
A: sk_buff的校验和处理：

```c
// 校验和状态
enum {
    CHECKSUM_NONE,          // 无校验和信息
    CHECKSUM_UNNECESSARY,   // 硬件已验证
    CHECKSUM_COMPLETE,      // 硬件计算了伪头以外的校验和
    CHECKSUM_PARTIAL,       // 需要软件完成校验和
};

// sk_buff中的字段
struct sk_buff {
    __u8  ip_summed:2;      // 校验和状态
    __u8  csum_valid:1;     // 校验和有效
    __u8  csum_complete_sw:1;
    __u8  csum_level:2;     // 封装层级
    // ...
    __wsum csum;            // 校验和值
    // ...
};
```

校验和状态说明：
```
+==================================================================+
||                  CHECKSUM STATES                               ||
+==================================================================+

接收方向:
+------------------------------------------------------------------+
|                                                                  |
|  CHECKSUM_NONE:                                                  |
|    - 硬件未提供校验和                                            |
|    - 软件需要计算和验证                                          |
|                                                                  |
|  CHECKSUM_UNNECESSARY:                                           |
|    - 硬件已验证校验和正确                                        |
|    - 软件无需再验证                                              |
|                                                                  |
|  CHECKSUM_COMPLETE:                                              |
|    - 硬件计算了L4校验和（不含伪头）                               |
|    - skb->csum 包含硬件计算值                                    |
|    - 软件需要加上伪头验证                                        |
|                                                                  |
+------------------------------------------------------------------+

发送方向:
+------------------------------------------------------------------+
|                                                                  |
|  CHECKSUM_NONE:                                                  |
|    - 校验和已计算完成                                            |
|    - 或不需要校验和                                              |
|                                                                  |
|  CHECKSUM_PARTIAL:                                               |
|    - 需要硬件计算校验和                                          |
|    - skb->csum_start: 校验和起始位置                             |
|    - skb->csum_offset: 校验和字段偏移                            |
|                                                                  |
+------------------------------------------------------------------+
```

校验和操作API：
```c
// 设置校验和卸载（发送）
static inline void skb_set_transport_header(struct sk_buff *skb,
                                             const int offset);

// 请求硬件计算校验和
void skb_partial_csum_set(struct sk_buff *skb, u16 start, u16 off);

// 软件计算校验和
__wsum skb_checksum(const struct sk_buff *skb, int offset,
                    int len, __wsum csum);

// 验证校验和
__sum16 __skb_checksum_complete(struct sk_buff *skb);

// 复制并计算校验和
__wsum csum_partial_copy(const void *src, void *dst, int len, __wsum sum);
```

使用示例：
```c
// 发送：请求硬件计算校验和
static void setup_checksum_offload(struct sk_buff *skb, struct iphdr *iph)
{
    struct tcphdr *tcph = tcp_hdr(skb);
    
    // 设置校验和卸载
    skb->ip_summed = CHECKSUM_PARTIAL;
    skb->csum_start = skb_transport_header(skb) - skb->head;
    skb->csum_offset = offsetof(struct tcphdr, check);
    
    // 预填伪头校验和
    tcph->check = ~tcp_v4_check(skb->len - ip_hdrlen(skb),
                                 iph->saddr, iph->daddr, 0);
}

// 接收：验证校验和
static int verify_checksum(struct sk_buff *skb)
{
    switch (skb->ip_summed) {
    case CHECKSUM_COMPLETE:
        // 硬件已计算，只需验证
        if (!csum_tcpudp_magic(...))
            return 0;  // OK
        break;
        
    case CHECKSUM_UNNECESSARY:
        // 硬件已验证
        return 0;
        
    case CHECKSUM_NONE:
    default:
        // 软件计算
        if (__skb_checksum_complete(skb))
            return -1;  // 校验失败
    }
    return 0;
}

// 检查网卡校验和卸载能力
if (dev->features & NETIF_F_HW_CSUM) {
    // 支持任意协议的校验和卸载
}
if (dev->features & NETIF_F_IP_CSUM) {
    // 支持IPv4 TCP/UDP校验和卸载
}
if (dev->features & NETIF_F_RXCSUM) {
    // 支持接收校验和卸载
}
```
[Intermediate]

---

## 9. sk_buff Queue Operations (队列操作)

---

Q: How do sk_buff queue operations work?
A: sk_buff队列操作：

```c
// 队列结构
struct sk_buff_head {
    struct sk_buff *next;     // 队列头
    struct sk_buff *prev;     // 队列尾
    __u32           qlen;     // 队列长度
    spinlock_t      lock;     // 队列锁
};

// 初始化
void skb_queue_head_init(struct sk_buff_head *list);

// 入队操作
void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk);
void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk);

// 出队操作
struct sk_buff *skb_dequeue(struct sk_buff_head *list);
struct sk_buff *skb_dequeue_tail(struct sk_buff_head *list);

// 队列信息
static inline __u32 skb_queue_len(const struct sk_buff_head *list);
static inline int skb_queue_empty(const struct sk_buff_head *list);

// 查看但不出队
struct sk_buff *skb_peek(const struct sk_buff_head *list);
struct sk_buff *skb_peek_tail(const struct sk_buff_head *list);

// 从队列中移除指定skb
void skb_unlink(struct sk_buff *skb, struct sk_buff_head *list);

// 清空队列
void skb_queue_purge(struct sk_buff_head *list);
```

队列结构图：
```
struct sk_buff_head
+---------------+
|    next   ----+---> sk_buff 1 ---> sk_buff 2 ---> sk_buff 3 --+
|    prev   ----+-----------------------------------------------|
|    qlen = 3   |                                               |
|    lock       |                                               |
+---------------+<----------------------------------------------+

双向循环链表:
  head.next -> skb1 -> skb2 -> skb3 -> head
  head.prev <- skb1 <- skb2 <- skb3 <- head
```

带锁的操作：
```c
// 自动加锁版本
void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
    unsigned long flags;
    spin_lock_irqsave(&list->lock, flags);
    __skb_queue_head(list, newsk);
    spin_unlock_irqrestore(&list->lock, flags);
}

// 不加锁版本（需要调用者持有锁）
void __skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk);
void __skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk);
struct sk_buff *__skb_dequeue(struct sk_buff_head *list);
```

使用示例：
```c
// 定义和初始化队列
struct sk_buff_head tx_queue;
skb_queue_head_init(&tx_queue);

// 入队
void queue_tx_packet(struct sk_buff *skb)
{
    skb_queue_tail(&tx_queue, skb);
}

// 处理队列
void process_tx_queue(void)
{
    struct sk_buff *skb;
    
    while ((skb = skb_dequeue(&tx_queue)) != NULL) {
        // 处理skb
        send_packet(skb);
    }
}

// 遍历队列（不移除）
void dump_queue(struct sk_buff_head *list)
{
    struct sk_buff *skb;
    
    spin_lock(&list->lock);
    skb_queue_walk(list, skb) {
        pr_info("skb len=%u\n", skb->len);
    }
    spin_unlock(&list->lock);
}

// 遍历宏
#define skb_queue_walk(queue, skb) \
    for (skb = (queue)->next; skb != (struct sk_buff *)(queue); \
         skb = skb->next)

#define skb_queue_walk_safe(queue, skb, tmp) \
    for (skb = (queue)->next, tmp = skb->next; \
         skb != (struct sk_buff *)(queue); \
         skb = tmp, tmp = skb->next)
```
[Intermediate]

---

## 10. GSO/GRO (Generic Segmentation/Receive Offload)

---

Q: What are GSO and GRO?
A: GSO和GRO是网络卸载技术：

```
+==================================================================+
||                  GSO (Generic Segmentation Offload)            ||
+==================================================================+

目的: 延迟分段，减少协议栈处理开销

传统方式:
+------------------------------------------------------------------+
|  应用层发送64KB数据                                               |
|         |                                                        |
|         v                                                        |
|  TCP层分成44个1500字节的段                                        |
|         |                                                        |
|         v                                                        |
|  IP层处理44次                                                    |
|         |                                                        |
|         v                                                        |
|  发送44个包                                                      |
+------------------------------------------------------------------+

GSO方式:
+------------------------------------------------------------------+
|  应用层发送64KB数据                                               |
|         |                                                        |
|         v                                                        |
|  TCP层创建一个大skb (64KB)                                       |
|  skb_shinfo->gso_size = 1460                                    |
|  skb_shinfo->gso_segs = 44                                      |
|         |                                                        |
|         v                                                        |
|  IP层处理1次                                                     |
|         |                                                        |
|         v                                                        |
|  在发送前（或网卡）分段                                           |
+------------------------------------------------------------------+


+==================================================================+
||                  GRO (Generic Receive Offload)                 ||
+==================================================================+

目的: 合并接收的小包，减少协议栈处理次数

传统方式:
+------------------------------------------------------------------+
|  网卡接收44个包                                                  |
|         |                                                        |
|         v                                                        |
|  协议栈处理44次                                                  |
|         |                                                        |
|         v                                                        |
|  应用层44次recv                                                  |
+------------------------------------------------------------------+

GRO方式:
+------------------------------------------------------------------+
|  网卡接收44个包                                                  |
|         |                                                        |
|         v                                                        |
|  NAPI poll中合并成大skb                                          |
|         |                                                        |
|         v                                                        |
|  协议栈处理1次                                                   |
|         |                                                        |
|         v                                                        |
|  应用层可一次recv全部数据                                        |
+------------------------------------------------------------------+
```

GSO相关字段：
```c
struct skb_shared_info {
    unsigned short  gso_size;       // 每个段的大小
    unsigned short  gso_segs;       // 段数量
    unsigned int    gso_type;       // GSO类型
    // ...
};

// GSO类型
enum {
    SKB_GSO_TCPV4       = 1 << 0,   // IPv4 TCP
    SKB_GSO_TCPV6       = 1 << 4,   // IPv6 TCP
    SKB_GSO_UDP         = 1 << 1,   // UDP
    SKB_GSO_UDP_L4      = 1 << 17,  // UDP L4分段
    // ...
};

// 检查是否是GSO包
static inline bool skb_is_gso(const struct sk_buff *skb)
{
    return skb_shinfo(skb)->gso_size;
}

// 软件GSO分段
struct sk_buff *skb_gso_segment(struct sk_buff *skb, netdev_features_t features);
```

GRO操作：
```c
// 在NAPI poll中使用
static int my_poll(struct napi_struct *napi, int budget)
{
    while (work_done < budget) {
        struct sk_buff *skb = receive_packet();
        
        // 使用GRO传递
        napi_gro_receive(napi, skb);
        // 或者禁用GRO
        // netif_receive_skb(skb);
        
        work_done++;
    }
    return work_done;
}

// GRO刷新（强制传递合并的包）
void napi_gro_flush(struct napi_struct *napi, bool flush_old);
```

检查和配置：
```bash
# 查看GSO/GRO状态
ethtool -k eth0 | grep -E "gso|gro"

# 启用/禁用
ethtool -K eth0 gso on
ethtool -K eth0 gro on
ethtool -K eth0 tso on    # TCP Segmentation Offload
```
[Advanced]

---

## 11. Scatter-Gather I/O (分散聚集I/O)

---

Q: How does scatter-gather I/O work with sk_buff?
A: 分散聚集I/O允许从多个内存位置构建数据包：

```
+==================================================================+
||              SCATTER-GATHER I/O                                ||
+==================================================================+

传统I/O (需要复制):
+------------------------------------------------------------------+
|  Page 1     Page 2     Page 3                                    |
|  +----+     +----+     +----+                                    |
|  |data|     |data|     |data|                                    |
|  +----+     +----+     +----+                                    |
|     |          |          |                                       |
|     +----------+----------+                                       |
|           复制到                                                  |
|              |                                                    |
|              v                                                    |
|       +----------------+                                         |
|       | 连续缓冲区     |                                         |
|       +----------------+                                         |
|              |                                                    |
|              v                                                    |
|           DMA发送                                                 |
+------------------------------------------------------------------+

Scatter-Gather I/O (零拷贝):
+------------------------------------------------------------------+
|  Page 1     Page 2     Page 3                                    |
|  +----+     +----+     +----+                                    |
|  |data|     |data|     |data|                                    |
|  +----+     +----+     +----+                                    |
|     |          |          |                                       |
|     v          v          v                                       |
|  +------+  +------+  +------+                                    |
|  |entry1|  |entry2|  |entry3|  <-- 描述符表                       |
|  +------+  +------+  +------+                                    |
|              |                                                    |
|              v                                                    |
|      网卡DMA引擎依次读取                                          |
|      各描述符指向的数据                                           |
+------------------------------------------------------------------+
```

sk_buff分片支持：
```c
// 构建分散聚集skb
struct sk_buff *build_sg_skb(void)
{
    struct sk_buff *skb;
    struct page *page;
    int i;
    
    // 分配skb（只包含头部）
    skb = alloc_skb(HEADER_SIZE, GFP_KERNEL);
    if (!skb)
        return NULL;
    
    // 添加头部
    skb_reserve(skb, HEADER_SIZE);
    build_headers(skb);
    
    // 添加分片（页面数据）
    for (i = 0; i < nr_pages; i++) {
        page = alloc_page(GFP_KERNEL);
        if (!page)
            goto err;
        
        // 填充页面数据
        fill_page_data(page);
        
        // 添加到分片数组
        skb_fill_page_desc(skb, i, page, 0, PAGE_SIZE);
        skb->len += PAGE_SIZE;
        skb->data_len += PAGE_SIZE;
    }
    
    return skb;
    
err:
    kfree_skb(skb);
    return NULL;
}

// 发送分散聚集数据
static int my_xmit_sg(struct sk_buff *skb, struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    dma_addr_t dma;
    int i, nr_desc = 0;
    
    // 映射线性部分
    if (skb_headlen(skb)) {
        dma = dma_map_single(&priv->pdev->dev, skb->data,
                             skb_headlen(skb), DMA_TO_DEVICE);
        setup_tx_desc(priv, nr_desc++, dma, skb_headlen(skb), false);
    }
    
    // 映射分片
    for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
        skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
        
        dma = skb_frag_dma_map(&priv->pdev->dev, frag, 0,
                               skb_frag_size(frag), DMA_TO_DEVICE);
        
        bool is_last = (i == skb_shinfo(skb)->nr_frags - 1);
        setup_tx_desc(priv, nr_desc++, dma, skb_frag_size(frag), is_last);
    }
    
    // 触发发送
    trigger_tx(priv);
    
    return NETDEV_TX_OK;
}
```

检查设备支持：
```c
// 检查设备是否支持分散聚集
if (dev->features & NETIF_F_SG) {
    // 支持分散聚集
}

// 设置支持的最大分片数
dev->gso_max_segs = MAX_SKB_FRAGS;
```
[Advanced]

---

## 12. sk_buff Debugging (调试)

---

Q: How to debug sk_buff issues?
A: sk_buff调试方法：

```c
// 打印skb信息
void skb_dump(const char *level, const struct sk_buff *skb, bool full_pkt);

// 使用示例
skb_dump(KERN_DEBUG, skb, true);

// 手动打印关键信息
void print_skb_info(struct sk_buff *skb)
{
    pr_info("skb: %p\n", skb);
    pr_info("  len=%u, data_len=%u\n", skb->len, skb->data_len);
    pr_info("  headroom=%d, tailroom=%d\n",
            skb_headroom(skb), skb_tailroom(skb));
    pr_info("  head=%p, data=%p, tail=%p, end=%p\n",
            skb->head, skb->data, 
            skb_tail_pointer(skb), skb_end_pointer(skb));
    pr_info("  protocol=%04x\n", ntohs(skb->protocol));
    pr_info("  ip_summed=%u\n", skb->ip_summed);
    pr_info("  nr_frags=%u\n", skb_shinfo(skb)->nr_frags);
    pr_info("  gso_size=%u, gso_segs=%u\n",
            skb_shinfo(skb)->gso_size, skb_shinfo(skb)->gso_segs);
}

// 打印数据内容
void print_skb_data(struct sk_buff *skb, int max_len)
{
    int i, len = min(skb->len, max_len);
    
    for (i = 0; i < len; i++) {
        if (i % 16 == 0)
            pr_cont("\n%04x: ", i);
        pr_cont("%02x ", skb->data[i]);
    }
    pr_cont("\n");
}
```

常见问题和检查：
```c
// 1. 检查skb是否有效
void validate_skb(struct sk_buff *skb)
{
    // 检查长度一致性
    BUG_ON(skb->len < skb->data_len);
    
    // 检查数据指针
    BUG_ON(skb->data < skb->head);
    BUG_ON(skb->tail > skb->end);
    BUG_ON(skb->data > skb_tail_pointer(skb));
    
    // 检查头部空间
    BUG_ON(skb_headroom(skb) < 0);
    BUG_ON(skb_tailroom(skb) < 0);
}

// 2. 使用CONFIG_DEBUG_NET
// 启用额外的网络调试检查
// CONFIG_DEBUG_NET=y

// 3. 使用kmemleak检查内存泄漏
// 确保所有分配的skb都被释放

// 4. 使用ftrace追踪
echo 'kfree_skb' >> /sys/kernel/debug/tracing/set_ftrace_filter
echo function > /sys/kernel/debug/tracing/current_tracer
cat /sys/kernel/debug/tracing/trace

// 5. 使用dropwatch查找丢包
dropwatch -l kas
```

调试工具命令：
```bash
# 使用bpftrace追踪skb
bpftrace -e 'kprobe:kfree_skb { @[kstack] = count(); }'

# 查看网络统计
cat /proc/net/softnet_stat

# 查看丢包
netstat -s | grep -i drop
cat /sys/class/net/eth0/statistics/rx_dropped
cat /sys/class/net/eth0/statistics/tx_dropped

# 使用perf追踪
perf probe --add 'kfree_skb skb->len'
perf record -e probe:kfree_skb -a sleep 10
perf report
```
[Basic]

---

## 13. Summary (总结)

---

Q: Summarize sk_buff structure and operations.
A: 
```
+==================================================================+
||                  SK_BUFF SUMMARY                               ||
+==================================================================+

sk_buff结构:
+------------------------------------------------------------------+
|  +----------------------------------------------------------+   |
|  |                    struct sk_buff                         |   |
|  +----------------------------------------------------------+   |
|  | 链表指针: next, prev                                      |   |
|  | 设备: dev, sk                                             |   |
|  | 长度: len, data_len                                       |   |
|  | 协议头: mac_header, network_header, transport_header      |   |
|  | 数据指针: head, data, tail, end                          |   |
|  | 校验和: ip_summed, csum                                   |   |
|  | 引用计数: users                                           |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|  +----------------------------------------------------------+   |
|  |                   Data Buffer                             |   |
|  |  [headroom][headers][payload][tailroom]                  |   |
|  +----------------------------------------------------------+   |
|                         |                                        |
|                         v                                        |
|  +----------------------------------------------------------+   |
|  |                skb_shared_info                            |   |
|  |  frags[], frag_list, gso_size, gso_segs                  |   |
|  +----------------------------------------------------------+   |
+------------------------------------------------------------------+


数据操作:
+------------------------------------------------------------------+
|  skb_reserve(skb, len)  - 预留头部空间，data/tail后移            |
|  skb_put(skb, len)      - 在尾部添加数据，tail后移，len增加       |
|  skb_push(skb, len)     - 在头部添加数据，data前移，len增加       |
|  skb_pull(skb, len)     - 移除头部数据，data后移，len减少         |
+------------------------------------------------------------------+


分配和释放:
+------------------------------------------------------------------+
|  alloc_skb()              - 通用分配                             |
|  netdev_alloc_skb()       - 设备驱动分配                         |
|  napi_alloc_skb()         - NAPI上下文分配                       |
|  kfree_skb()              - 释放（记录丢弃）                      |
|  consume_skb()            - 正常消费释放                         |
|  dev_kfree_skb_any()      - 发送完成释放                         |
+------------------------------------------------------------------+


克隆和复制:
+------------------------------------------------------------------+
|  skb_clone()    - 共享数据，新sk_buff                            |
|  skb_copy()     - 完全复制                                       |
|  pskb_copy()    - 复制线性，共享分片                             |
+------------------------------------------------------------------+


队列操作:
+------------------------------------------------------------------+
|  skb_queue_head/tail()    - 入队                                 |
|  skb_dequeue()            - 出队                                 |
|  skb_queue_purge()        - 清空队列                             |
+------------------------------------------------------------------+


关键特性:
+------------------------------------------------------------------+
|  校验和卸载    - CHECKSUM_PARTIAL, CHECKSUM_UNNECESSARY          |
|  GSO/GRO       - 大包分段/合并                                   |
|  分散聚集      - 多页面零拷贝                                    |
|  分片          - nr_frags, frags[], frag_list                    |
+------------------------------------------------------------------+


协议栈流程:
+------------------------------------------------------------------+
|                                                                  |
|  发送: 应用 -> TCP -> IP -> 设备                                 |
|        skb_put(payload) -> skb_push(tcp) -> skb_push(ip)        |
|                                                                  |
|  接收: 设备 -> IP -> TCP -> 应用                                 |
|        skb_pull(eth) -> skb_pull(ip) -> skb_pull(tcp)           |
|                                                                  |
+------------------------------------------------------------------+
```
[Basic]

---

*Total: 100+ cards covering Linux kernel sk_buff implementation*

