# Network Device Driver Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel network device driver, NAPI, net_device, and packet processing
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. Network Driver Overview (网络驱动概述)

---

Q: What is the architecture of Linux network device driver?
A: Linux网络设备驱动架构：

```
+------------------------------------------------------------------+
|                    Network Stack Architecture                     |
+------------------------------------------------------------------+
|                                                                  |
|  +------------------------------------------------------------+  |
|  |                   User Space (用户空间)                     |  |
|  |  socket(), send(), recv(), etc.                            |  |
|  +-----------------------------+------------------------------+  |
|                                |                                 |
|                    System Call Interface                         |
|                                |                                 |
|  +-----------------------------v------------------------------+  |
|  |              Protocol Layer (协议层)                        |  |
|  |  TCP, UDP, IP, ICMP, ARP, etc.                             |  |
|  +-----------------------------+------------------------------+  |
|                                |                                 |
|                          sk_buff                                 |
|                                |                                 |
|  +-----------------------------v------------------------------+  |
|  |           Network Device Interface (网络设备接口)           |  |
|  |  net_device, net_device_ops, NAPI                          |  |
|  +-----------------------------+------------------------------+  |
|                                |                                 |
|  +-----------------------------v------------------------------+  |
|  |           Device Driver (设备驱动)                          |  |
|  |  Hardware specific operations                               |  |
|  +-----------------------------+------------------------------+  |
|                                |                                 |
|  +-----------------------------v------------------------------+  |
|  |                Hardware (硬件)                              |  |
|  |  NIC, PHY, DMA Engine                                      |  |
|  +------------------------------------------------------------+  |
|                                                                  |
+------------------------------------------------------------------+
```

驱动主要功能：
| 功能 | 说明 |
|------|------|
| 初始化/销毁 | 探测硬件、分配资源、注册设备 |
| 数据发送 | 从协议层接收sk_buff并发送到硬件 |
| 数据接收 | 从硬件接收数据并传递给协议层 |
| 中断处理 | 响应硬件中断 |
| 设备控制 | 打开/关闭、配置参数 |
| 统计信息 | 收发包计数、错误统计 |
[Basic]

---

Q: What are the key data structures in network driver?
A: 
```c
// 核心数据结构关系
+------------------------------------------------------------------+
|                                                                  |
|  +----------------+         +----------------+                   |
|  |  net_device    |-------->| net_device_ops |                   |
|  |  (网络设备)     |         | (设备操作)      |                   |
|  +-------+--------+         +----------------+                   |
|          |                                                       |
|          +---> priv (私有数据)                                   |
|          |                                                       |
|          +---> napi_struct (NAPI结构)                            |
|          |                                                       |
|          +---> ethtool_ops (ethtool操作)                         |
|                                                                  |
|  +----------------+         +----------------+                   |
|  |   sk_buff      |<------->|   sk_buff      |                   |
|  |  (发送队列)     |         |  (接收队列)     |                   |
|  +----------------+         +----------------+                   |
|                                                                  |
|  +----------------+         +----------------+                   |
|  |  TX Ring       |         |  RX Ring       |                   |
|  | (DMA描述符环)   |         | (DMA描述符环)   |                   |
|  +----------------+         +----------------+                   |
|                                                                  |
+------------------------------------------------------------------+
```

关键结构体：
```c
struct net_device;      // 网络设备
struct net_device_ops;  // 设备操作集
struct sk_buff;         // 网络数据包缓冲区
struct napi_struct;     // NAPI轮询结构
struct ethtool_ops;     // ethtool操作集
struct netdev_queue;    // 发送队列
```
[Basic]

---

## 2. net_device Structure (网络设备结构)

---

Q: What is struct net_device?
A: `net_device`是网络设备的核心结构：

```c
// include/linux/netdevice.h (简化版)
struct net_device {
    /*=== 设备标识 ===*/
    char name[IFNAMSIZ];              // 设备名 (eth0, wlan0)
    int ifindex;                       // 接口索引
    unsigned long state;               // 设备状态
    
    /*=== 硬件信息 ===*/
    unsigned char *dev_addr;           // MAC地址
    unsigned char addr_len;            // 地址长度
    unsigned short type;               // 设备类型 (ARPHRD_ETHER)
    unsigned int mtu;                  // 最大传输单元
    unsigned int min_mtu;
    unsigned int max_mtu;
    unsigned short hard_header_len;    // 硬件头长度
    
    /*=== 特性标志 ===*/
    netdev_features_t features;        // 当前特性
    netdev_features_t hw_features;     // 硬件支持的特性
    netdev_features_t wanted_features;
    netdev_features_t vlan_features;
    
    /*=== 操作函数 ===*/
    const struct net_device_ops *netdev_ops;
    const struct ethtool_ops *ethtool_ops;
    const struct header_ops *header_ops;
    
    /*=== 统计信息 ===*/
    struct net_device_stats stats;     // 基本统计
    atomic_long_t rx_dropped;
    atomic_long_t tx_dropped;
    atomic_long_t rx_nohandler;
    
    /*=== 发送队列 ===*/
    unsigned int num_tx_queues;        // 发送队列数
    unsigned int real_num_tx_queues;
    struct netdev_queue *_tx;          // 发送队列数组
    
    /*=== 接收队列 ===*/
    unsigned int num_rx_queues;
    unsigned int real_num_rx_queues;
    struct netdev_rx_queue *_rx;
    
    /*=== NAPI ===*/
    struct list_head napi_list;        // NAPI实例链表
    
    /*=== 私有数据 ===*/
    void *priv;                        // netdev_priv() 返回
    
    /*=== 其他 ===*/
    struct device dev;                 // 内嵌设备
    struct net *nd_net;                // 网络命名空间
    
    // ... 还有更多字段
};

// 获取私有数据
static inline void *netdev_priv(const struct net_device *dev)
{
    return (char *)dev + ALIGN(sizeof(struct net_device), NETDEV_ALIGN);
}
```

设备特性标志：
```c
// include/linux/netdev_features.h
NETIF_F_SG              // 分散/聚集I/O
NETIF_F_IP_CSUM         // 硬件IP校验和
NETIF_F_HW_CSUM         // 硬件完整校验和
NETIF_F_HIGHDMA         // 高端内存DMA
NETIF_F_GSO             // 通用分段卸载
NETIF_F_TSO             // TCP分段卸载
NETIF_F_GRO             // 通用接收卸载
NETIF_F_RXCSUM          // 接收校验和卸载
NETIF_F_HW_VLAN_*       // 硬件VLAN支持
```
[Intermediate]

---

Q: What is struct net_device_ops?
A: `net_device_ops`定义设备操作接口：

```c
// include/linux/netdevice.h
struct net_device_ops {
    /*=== 设备生命周期 ===*/
    int (*ndo_init)(struct net_device *dev);
    void (*ndo_uninit)(struct net_device *dev);
    int (*ndo_open)(struct net_device *dev);           // ifconfig up
    int (*ndo_stop)(struct net_device *dev);           // ifconfig down
    
    /*=== 数据传输 ===*/
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                  struct net_device *dev);
    u16 (*ndo_select_queue)(struct net_device *dev, struct sk_buff *skb,
                            struct net_device *sb_dev);
    
    /*=== 设备配置 ===*/
    int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    int (*ndo_set_features)(struct net_device *dev, netdev_features_t features);
    void (*ndo_set_rx_mode)(struct net_device *dev);   // 多播/混杂模式
    
    /*=== 统计 ===*/
    void (*ndo_get_stats64)(struct net_device *dev,
                            struct rtnl_link_stats64 *storage);
    
    /*=== VLAN ===*/
    int (*ndo_vlan_rx_add_vid)(struct net_device *dev,
                               __be16 proto, u16 vid);
    int (*ndo_vlan_rx_kill_vid)(struct net_device *dev,
                                __be16 proto, u16 vid);
    
    /*=== ioctl ===*/
    int (*ndo_do_ioctl)(struct net_device *dev,
                        struct ifreq *ifr, int cmd);
    int (*ndo_eth_ioctl)(struct net_device *dev,
                         struct ifreq *ifr, int cmd);
    
    /*=== 校验和 ===*/
    netdev_features_t (*ndo_fix_features)(struct net_device *dev,
                                          netdev_features_t features);
    
    /*=== TC/XDP ===*/
    int (*ndo_setup_tc)(struct net_device *dev, enum tc_setup_type type,
                        void *type_data);
    int (*ndo_bpf)(struct net_device *dev, struct netdev_bpf *bpf);
    int (*ndo_xdp_xmit)(struct net_device *dev, int n,
                        struct xdp_frame **xdp, u32 flags);
    
    // ... 还有更多回调
};
```

发送返回值：
```c
// include/linux/netdevice.h
enum netdev_tx {
    NETDEV_TX_OK     = 0x00,  // 发送成功（或已排队）
    NETDEV_TX_BUSY   = 0x10,  // 驱动忙，稍后重试
};
```
[Intermediate]

---

## 3. Network Device Registration (网络设备注册)

---

Q: How to register a network device?
A: 网络设备注册流程：

```c
// 1. 分配net_device
struct net_device *dev;
dev = alloc_etherdev(sizeof(struct my_priv));  // 以太网设备
// 或
dev = alloc_netdev(sizeof(struct my_priv), "eth%d", 
                   NET_NAME_UNKNOWN, ether_setup);

// 2. 获取私有数据
struct my_priv *priv = netdev_priv(dev);

// 3. 设置设备属性
dev->netdev_ops = &my_netdev_ops;
dev->ethtool_ops = &my_ethtool_ops;
dev->watchdog_timeo = 5 * HZ;
dev->irq = irq;
dev->base_addr = base;

// 4. 设置MAC地址
memcpy(dev->dev_addr, mac_addr, ETH_ALEN);

// 5. 设置特性
dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM;
dev->hw_features = dev->features;

// 6. 注册设备
ret = register_netdev(dev);
if (ret) {
    free_netdev(dev);
    return ret;
}

// 7. 启用NAPI（如果使用）
netif_napi_add(dev, &priv->napi, my_poll, NAPI_POLL_WEIGHT);
napi_enable(&priv->napi);

// 注销流程
napi_disable(&priv->napi);
netif_napi_del(&priv->napi);
unregister_netdev(dev);
free_netdev(dev);
```

分配函数：
```c
// 以太网设备
struct net_device *alloc_etherdev(int sizeof_priv);
struct net_device *alloc_etherdev_mq(int sizeof_priv, unsigned int queue_count);

// 通用设备
struct net_device *alloc_netdev(int sizeof_priv, const char *name,
                                unsigned char name_assign_type,
                                void (*setup)(struct net_device *));

// 预定义setup函数
ether_setup()   // 以太网
loopback_setup() // 回环
```

注册变体：
```c
register_netdev(dev);       // 持有rtnl_lock
register_netdevice(dev);    // 调用者持有rtnl_lock
```
[Intermediate]

---

Q: How to implement ndo_open and ndo_stop?
A: 设备打开和关闭回调：

```c
// 设备打开 (ifconfig up)
static int my_open(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    int ret;
    
    // 1. 分配接收缓冲区
    ret = alloc_rx_buffers(priv);
    if (ret)
        return ret;
    
    // 2. 分配发送缓冲区
    ret = alloc_tx_buffers(priv);
    if (ret)
        goto err_free_rx;
    
    // 3. 初始化硬件
    ret = init_hardware(priv);
    if (ret)
        goto err_free_tx;
    
    // 4. 请求中断
    ret = request_irq(dev->irq, my_interrupt, IRQF_SHARED,
                      dev->name, dev);
    if (ret)
        goto err_deinit_hw;
    
    // 5. 启用NAPI
    napi_enable(&priv->napi);
    
    // 6. 启用硬件中断
    enable_hw_irq(priv);
    
    // 7. 启动发送队列
    netif_start_queue(dev);
    // 多队列: netif_tx_start_all_queues(dev);
    
    // 8. 启动PHY/连接监控
    phy_start(priv->phydev);
    
    return 0;
    
err_deinit_hw:
    deinit_hardware(priv);
err_free_tx:
    free_tx_buffers(priv);
err_free_rx:
    free_rx_buffers(priv);
    return ret;
}

// 设备关闭 (ifconfig down)
static int my_stop(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    
    // 1. 停止PHY
    phy_stop(priv->phydev);
    
    // 2. 停止发送队列
    netif_stop_queue(dev);
    // 多队列: netif_tx_stop_all_queues(dev);
    
    // 3. 禁用硬件中断
    disable_hw_irq(priv);
    
    // 4. 禁用NAPI
    napi_disable(&priv->napi);
    
    // 5. 释放中断
    free_irq(dev->irq, dev);
    
    // 6. 停止硬件
    stop_hardware(priv);
    
    // 7. 释放缓冲区
    free_tx_buffers(priv);
    free_rx_buffers(priv);
    
    return 0;
}
```
[Intermediate]

---

## 4. Packet Transmission (数据包发送)

---

Q: How does packet transmission work in network driver?
A: 数据包发送流程：

```
+------------------------------------------------------------------+
|                    TX Path (发送路径)                             |
+------------------------------------------------------------------+
|                                                                  |
|  协议层                                                          |
|     |                                                            |
|     v                                                            |
|  dev_queue_xmit(skb)                                             |
|     |                                                            |
|     v                                                            |
|  __dev_queue_xmit()                                              |
|     |                                                            |
|     +---> 流量控制(qdisc)排队                                     |
|     |                                                            |
|     v                                                            |
|  dev_hard_start_xmit()                                           |
|     |                                                            |
|     v                                                            |
|  ndo_start_xmit(skb, dev)     <--- 驱动回调                       |
|     |                                                            |
|     +---> 获取TX描述符                                            |
|     +---> 填写描述符 (地址、长度、标志)                            |
|     +---> DMA映射                                                 |
|     +---> 触发硬件发送                                            |
|     |                                                            |
|     v                                                            |
|  硬件发送完成中断                                                 |
|     |                                                            |
|     v                                                            |
|  TX完成处理                                                       |
|     +---> DMA解映射                                               |
|     +---> 释放skb                                                 |
|     +---> 更新统计                                                 |
|                                                                  |
+------------------------------------------------------------------+
```

发送回调实现：
```c
static netdev_tx_t my_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    struct tx_desc *desc;
    dma_addr_t dma_addr;
    int entry;
    
    // 1. 检查队列空间
    if (tx_ring_full(priv)) {
        netif_stop_queue(dev);  // 停止队列
        return NETDEV_TX_BUSY;
    }
    
    // 2. 获取描述符槽位
    entry = priv->tx_head;
    desc = &priv->tx_ring[entry];
    
    // 3. DMA映射
    dma_addr = dma_map_single(&priv->pdev->dev, skb->data,
                              skb->len, DMA_TO_DEVICE);
    if (dma_mapping_error(&priv->pdev->dev, dma_addr)) {
        dev_kfree_skb_any(skb);
        dev->stats.tx_dropped++;
        return NETDEV_TX_OK;
    }
    
    // 4. 保存skb用于完成时释放
    priv->tx_skb[entry] = skb;
    priv->tx_dma[entry] = dma_addr;
    
    // 5. 填写描述符
    desc->buf_addr = cpu_to_le64(dma_addr);
    desc->buf_len = cpu_to_le16(skb->len);
    desc->status = cpu_to_le32(TX_DESC_OWN | TX_DESC_EOP);
    
    // 6. 内存屏障确保描述符写入完成
    wmb();
    
    // 7. 更新head指针
    priv->tx_head = (entry + 1) % TX_RING_SIZE;
    
    // 8. 触发硬件发送
    writel(priv->tx_head, priv->base + TX_TAIL_REG);
    
    // 9. 检查队列是否满
    if (tx_ring_full(priv))
        netif_stop_queue(dev);
    
    return NETDEV_TX_OK;
}
```
[Intermediate]

---

Q: How to handle TX completion?
A: TX完成处理（通常在中断或NAPI中）：

```c
// TX完成处理
static void my_tx_complete(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    unsigned int dirty = priv->tx_dirty;
    unsigned int head = priv->tx_head;
    unsigned int bytes_compl = 0;
    unsigned int pkts_compl = 0;
    
    while (dirty != head) {
        struct tx_desc *desc = &priv->tx_ring[dirty];
        struct sk_buff *skb;
        
        // 检查描述符是否完成
        if (desc->status & TX_DESC_OWN)
            break;  // 硬件还在使用
        
        // 获取skb
        skb = priv->tx_skb[dirty];
        
        // DMA解映射
        dma_unmap_single(&priv->pdev->dev, priv->tx_dma[dirty],
                         skb->len, DMA_TO_DEVICE);
        
        // 检查错误
        if (desc->status & TX_DESC_ERROR) {
            dev->stats.tx_errors++;
        } else {
            bytes_compl += skb->len;
            pkts_compl++;
        }
        
        // 释放skb
        dev_kfree_skb_any(skb);
        priv->tx_skb[dirty] = NULL;
        
        // 清除描述符
        desc->status = 0;
        
        dirty = (dirty + 1) % TX_RING_SIZE;
    }
    
    // 更新dirty指针
    priv->tx_dirty = dirty;
    
    // 更新统计
    dev->stats.tx_packets += pkts_compl;
    dev->stats.tx_bytes += bytes_compl;
    
    // 如果队列之前被停止，现在有空间了，重新启动
    if (netif_queue_stopped(dev) && !tx_ring_full(priv)) {
        netif_wake_queue(dev);
    }
}
```

队列控制函数：
```c
// 单队列
netif_start_queue(dev);     // 启动发送
netif_stop_queue(dev);      // 停止发送
netif_wake_queue(dev);      // 唤醒发送
netif_queue_stopped(dev);   // 检查状态

// 多队列
netif_tx_start_all_queues(dev);
netif_tx_stop_all_queues(dev);
netif_tx_wake_all_queues(dev);

// 特定队列
netif_start_subqueue(dev, queue_index);
netif_stop_subqueue(dev, queue_index);
netif_wake_subqueue(dev, queue_index);
```
[Intermediate]

---

## 5. Packet Reception (数据包接收)

---

Q: How does packet reception work in network driver?
A: 数据包接收流程：

```
+------------------------------------------------------------------+
|                    RX Path (接收路径)                             |
+------------------------------------------------------------------+
|                                                                  |
|  网卡硬件                                                        |
|     |                                                            |
|     +---> DMA写入预分配的缓冲区                                   |
|     +---> 更新RX描述符状态                                        |
|     +---> 触发中断                                                |
|     |                                                            |
|     v                                                            |
|  中断处理程序 (my_interrupt)                                      |
|     |                                                            |
|     +---> 禁用硬件中断                                            |
|     +---> napi_schedule()                                        |
|     |                                                            |
|     v                                                            |
|  NAPI轮询 (my_poll)                                              |
|     |                                                            |
|     +---> 检查RX描述符                                            |
|     +---> DMA解映射                                               |
|     +---> 分配新缓冲区                                            |
|     +---> 设置skb字段                                             |
|     +---> napi_gro_receive(skb) / netif_receive_skb(skb)         |
|     |                                                            |
|     v                                                            |
|  协议栈处理                                                       |
|     +---> GRO合并                                                 |
|     +---> IP/TCP处理                                              |
|     +---> 传递给socket                                            |
|                                                                  |
+------------------------------------------------------------------+
```

RX缓冲区预分配：
```c
static int alloc_rx_buffers(struct my_priv *priv)
{
    int i;
    
    for (i = 0; i < RX_RING_SIZE; i++) {
        struct sk_buff *skb;
        dma_addr_t dma_addr;
        struct rx_desc *desc = &priv->rx_ring[i];
        
        // 1. 分配skb
        skb = netdev_alloc_skb(priv->netdev, RX_BUF_SIZE);
        if (!skb)
            return -ENOMEM;
        
        // 2. DMA映射
        dma_addr = dma_map_single(&priv->pdev->dev, skb->data,
                                  RX_BUF_SIZE, DMA_FROM_DEVICE);
        if (dma_mapping_error(&priv->pdev->dev, dma_addr)) {
            dev_kfree_skb(skb);
            return -ENOMEM;
        }
        
        // 3. 保存信息
        priv->rx_skb[i] = skb;
        priv->rx_dma[i] = dma_addr;
        
        // 4. 填写描述符
        desc->buf_addr = cpu_to_le64(dma_addr);
        desc->status = cpu_to_le32(RX_DESC_OWN);  // 交给硬件
    }
    
    return 0;
}
```
[Intermediate]

---

Q: How to implement NAPI poll function?
A: NAPI轮询函数实现：

```c
static int my_poll(struct napi_struct *napi, int budget)
{
    struct my_priv *priv = container_of(napi, struct my_priv, napi);
    struct net_device *dev = priv->netdev;
    int work_done = 0;
    
    // 处理TX完成
    my_tx_complete(dev);
    
    // 处理RX
    while (work_done < budget) {
        struct rx_desc *desc;
        struct sk_buff *skb;
        unsigned int entry;
        int len;
        
        entry = priv->rx_tail;
        desc = &priv->rx_ring[entry];
        
        // 检查描述符是否完成
        if (desc->status & RX_DESC_OWN)
            break;  // 没有更多数据
        
        // 读内存屏障
        rmb();
        
        // 检查错误
        if (desc->status & RX_DESC_ERROR) {
            dev->stats.rx_errors++;
            goto next;
        }
        
        // 获取长度
        len = le16_to_cpu(desc->buf_len);
        
        // DMA解映射
        dma_unmap_single(&priv->pdev->dev, priv->rx_dma[entry],
                         RX_BUF_SIZE, DMA_FROM_DEVICE);
        
        // 获取skb
        skb = priv->rx_skb[entry];
        
        // 设置skb长度
        skb_put(skb, len);
        
        // 设置协议
        skb->protocol = eth_type_trans(skb, dev);
        
        // 硬件校验和
        if (dev->features & NETIF_F_RXCSUM) {
            if (desc->status & RX_DESC_CSUM_OK)
                skb->ip_summed = CHECKSUM_UNNECESSARY;
        }
        
        // 传递给协议栈
        napi_gro_receive(napi, skb);
        
        // 更新统计
        dev->stats.rx_packets++;
        dev->stats.rx_bytes += len;
        
next:
        // 分配新缓冲区
        skb = netdev_alloc_skb(dev, RX_BUF_SIZE);
        if (skb) {
            dma_addr_t dma = dma_map_single(&priv->pdev->dev,
                                            skb->data, RX_BUF_SIZE,
                                            DMA_FROM_DEVICE);
            priv->rx_skb[entry] = skb;
            priv->rx_dma[entry] = dma;
            desc->buf_addr = cpu_to_le64(dma);
        }
        
        // 归还描述符给硬件
        wmb();
        desc->status = cpu_to_le32(RX_DESC_OWN);
        
        priv->rx_tail = (entry + 1) % RX_RING_SIZE;
        work_done++;
    }
    
    // 如果处理完所有数据，退出轮询模式
    if (work_done < budget) {
        napi_complete_done(napi, work_done);
        // 重新启用中断
        enable_hw_irq(priv);
    }
    
    return work_done;
}
```
[Advanced]

---

## 6. NAPI (New API)

---

Q: What is NAPI and why is it important?
A: NAPI是高效的中断+轮询混合机制：

```
+------------------------------------------------------------------+
|                    NAPI vs Traditional                            |
+------------------------------------------------------------------+
|                                                                  |
|  传统中断模式:                                                    |
|  +----+    +----+    +----+    +----+    +----+                  |
|  |IRQ |    |IRQ |    |IRQ |    |IRQ |    |IRQ |  高中断负载       |
|  +----+    +----+    +----+    +----+    +----+                  |
|    |        |         |         |         |                       |
|    v        v         v         v         v                       |
|  [处理]   [处理]    [处理]    [处理]    [处理]                     |
|                                                                  |
|  NAPI模式:                                                        |
|  +----+              +----+                                      |
|  |IRQ |              |IRQ |                                      |
|  +----+              +----+                 低中断负载            |
|    |                   |                                          |
|    v                   v                                          |
|  禁用IRQ            禁用IRQ                                       |
|    |                   |                                          |
|    v                   v                                          |
|  [轮询处理]         [轮询处理]                                    |
|  [多个包]           [多个包]                                      |
|    |                   |                                          |
|    v                   v                                          |
|  启用IRQ            启用IRQ                                       |
|                                                                  |
+------------------------------------------------------------------+
```

NAPI数据结构：
```c
// include/linux/netdevice.h
struct napi_struct {
    struct list_head poll_list;      // 调度链表
    unsigned long state;             // NAPI状态
    int weight;                      // 每次轮询的最大包数
    int defer_hard_irqs_count;
    unsigned long gro_bitmask;
    int (*poll)(struct napi_struct *, int);  // 轮询函数
    
    struct net_device *dev;          // 所属设备
    struct gro_list gro_hash[GRO_HASH_BUCKETS];
    struct sk_buff *skb;             // GRO skb
    struct list_head rx_list;        // RX skb链表
    int rx_count;
    // ...
};
```

NAPI操作：
```c
// 初始化NAPI
void netif_napi_add(struct net_device *dev, struct napi_struct *napi,
                    int (*poll)(struct napi_struct *, int), int weight);

// 删除NAPI
void netif_napi_del(struct napi_struct *napi);

// 启用/禁用
void napi_enable(struct napi_struct *napi);
void napi_disable(struct napi_struct *napi);

// 调度NAPI（通常在中断中调用）
void napi_schedule(struct napi_struct *napi);
bool napi_schedule_prep(struct napi_struct *napi);
void __napi_schedule(struct napi_struct *napi);

// 完成NAPI轮询
bool napi_complete(struct napi_struct *napi);
bool napi_complete_done(struct napi_struct *napi, int work_done);

// 传递skb给协议栈
void napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb);
gro_result_t napi_gro_frags(struct napi_struct *napi);
```
[Intermediate]

---

Q: How does interrupt handling work with NAPI?
A: 中断处理与NAPI配合：

```c
// 中断处理程序
static irqreturn_t my_interrupt(int irq, void *dev_id)
{
    struct net_device *dev = dev_id;
    struct my_priv *priv = netdev_priv(dev);
    u32 status;
    
    // 读取中断状态
    status = readl(priv->base + INT_STATUS_REG);
    if (!status)
        return IRQ_NONE;  // 不是我们的中断
    
    // 确认中断
    writel(status, priv->base + INT_STATUS_REG);
    
    // RX/TX中断：调度NAPI
    if (status & (INT_RX | INT_TX)) {
        // 禁用RX/TX中断
        writel(0, priv->base + INT_ENABLE_REG);
        
        // 调度NAPI
        if (napi_schedule_prep(&priv->napi))
            __napi_schedule(&priv->napi);
    }
    
    // 链路状态变化
    if (status & INT_LINK) {
        handle_link_change(priv);
    }
    
    // 错误中断
    if (status & INT_ERROR) {
        handle_error(priv);
    }
    
    return IRQ_HANDLED;
}

// 典型的NAPI流程
// 1. 硬件产生中断
//        |
//        v
// 2. 中断处理程序禁用中断，调度NAPI
//        |
//        v
// 3. softirq调用poll函数
//        |
//        v
// 4. poll处理数据包（最多budget个）
//        |
//    +---+---+
//    |       |
//    v       v
// 处理完    未处理完
//    |       |
//    v       v
// napi_complete  返回budget
// 启用中断      继续等待softirq
```

中断合并（Interrupt Coalescing）：
```c
// 硬件中断合并设置
// 减少中断频率，提高吞吐量
static void set_interrupt_coalesce(struct my_priv *priv)
{
    // 设置接收中断延迟（微秒）
    writel(100, priv->base + RX_INT_DELAY);
    
    // 设置接收中断阈值（包数）
    writel(64, priv->base + RX_INT_THRESHOLD);
    
    // 类似设置发送
    writel(100, priv->base + TX_INT_DELAY);
    writel(64, priv->base + TX_INT_THRESHOLD);
}
```
[Intermediate]

---

## 7. DMA and Ring Buffers (DMA和环形缓冲区)

---

Q: How does DMA ring buffer work?
A: DMA描述符环形缓冲区：

```
+------------------------------------------------------------------+
|                    DMA Ring Buffer                                |
+------------------------------------------------------------------+
|                                                                  |
|  描述符环（在DMA一致性内存中）:                                    |
|                                                                  |
|     head (软件写入点)                                             |
|       v                                                          |
|  +---+---+---+---+---+---+---+---+                               |
|  | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |                               |
|  +---+---+---+---+---+---+---+---+                               |
|            ^                                                     |
|           tail (硬件处理点 / 软件清理点)                          |
|                                                                  |
|  每个描述符包含:                                                  |
|  +----------------------------------------------------------+   |
|  | buf_addr (64bit) | buf_len (16bit) | status (32bit) | ... |   |
|  +----------------------------------------------------------+   |
|       |                                                         |
|       v                                                         |
|  数据缓冲区（在流式DMA内存中）                                    |
|  +----------------------------------------------------------+   |
|  |              Packet Data                                 |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+

TX环:
- 软件将skb映射到描述符，设置OWN位，更新head
- 硬件从tail处理，清除OWN位
- 软件扫描tail到head，释放已完成的skb

RX环:
- 软件预分配缓冲区，设置OWN位
- 硬件写入数据，清除OWN位，更新状态
- 软件读取数据，重新分配缓冲区
```

描述符结构示例：
```c
// 发送描述符
struct tx_desc {
    __le64 buf_addr;     // 缓冲区DMA地址
    __le16 buf_len;      // 缓冲区长度
    __le16 vlan_tag;     // VLAN标签
    __le32 status;       // 状态/命令
    // TX_DESC_OWN      - 硬件拥有此描述符
    // TX_DESC_EOP      - 包结束
    // TX_DESC_SOP      - 包开始
    // TX_DESC_CSUM     - 计算校验和
};

// 接收描述符
struct rx_desc {
    __le64 buf_addr;     // 缓冲区DMA地址
    __le16 buf_len;      // 接收的长度
    __le16 vlan_tag;     // VLAN标签
    __le32 status;       // 状态
    // RX_DESC_OWN      - 硬件拥有此描述符
    // RX_DESC_EOP      - 包结束
    // RX_DESC_ERROR    - 错误
    // RX_DESC_CSUM_OK  - 校验和正确
};
```

DMA内存分配：
```c
// 分配描述符环（一致性DMA内存）
priv->tx_ring = dma_alloc_coherent(&pdev->dev,
                                   TX_RING_SIZE * sizeof(struct tx_desc),
                                   &priv->tx_ring_dma, GFP_KERNEL);

// 分配数据缓冲区（流式DMA映射）
dma_addr = dma_map_single(&pdev->dev, skb->data, len, DMA_TO_DEVICE);

// 释放
dma_free_coherent(&pdev->dev, size, priv->tx_ring, priv->tx_ring_dma);
dma_unmap_single(&pdev->dev, dma_addr, len, DMA_TO_DEVICE);
```
[Advanced]

---

## 8. ethtool Interface (ethtool接口)

---

Q: How to implement ethtool operations?
A: ethtool提供配置和诊断接口：

```c
// include/linux/ethtool.h
struct ethtool_ops {
    // 驱动信息
    void (*get_drvinfo)(struct net_device *, struct ethtool_drvinfo *);
    
    // 链路设置
    int (*get_link_ksettings)(struct net_device *,
                              struct ethtool_link_ksettings *);
    int (*set_link_ksettings)(struct net_device *,
                              const struct ethtool_link_ksettings *);
    
    // 链路状态
    u32 (*get_link)(struct net_device *);
    
    // 消息级别
    u32 (*get_msglevel)(struct net_device *);
    void (*set_msglevel)(struct net_device *, u32);
    
    // 寄存器dump
    int (*get_regs_len)(struct net_device *);
    void (*get_regs)(struct net_device *, struct ethtool_regs *, void *);
    
    // WOL (Wake-on-LAN)
    void (*get_wol)(struct net_device *, struct ethtool_wolinfo *);
    int (*set_wol)(struct net_device *, struct ethtool_wolinfo *);
    
    // 中断合并
    int (*get_coalesce)(struct net_device *, struct ethtool_coalesce *,
                        struct kernel_ethtool_coalesce *,
                        struct netlink_ext_ack *);
    int (*set_coalesce)(struct net_device *, struct ethtool_coalesce *,
                        struct kernel_ethtool_coalesce *,
                        struct netlink_ext_ack *);
    
    // 环形缓冲区大小
    void (*get_ringparam)(struct net_device *, struct ethtool_ringparam *,
                          struct kernel_ethtool_ringparam *,
                          struct netlink_ext_ack *);
    int (*set_ringparam)(struct net_device *, struct ethtool_ringparam *,
                         struct kernel_ethtool_ringparam *,
                         struct netlink_ext_ack *);
    
    // 统计信息
    void (*get_strings)(struct net_device *, u32, u8 *);
    void (*get_ethtool_stats)(struct net_device *, struct ethtool_stats *, u64 *);
    int (*get_sset_count)(struct net_device *, int);
    
    // 自检
    void (*self_test)(struct net_device *, struct ethtool_test *, u64 *);
    
    // 校验和卸载
    int (*get_rxfh_indir_size)(struct net_device *);
    int (*get_rxfh)(struct net_device *, u32 *indir, u8 *key, u8 *hfunc);
    int (*set_rxfh)(struct net_device *, const u32 *indir, const u8 *key,
                    const u8 hfunc);
    
    // ... 更多操作
};
```

实现示例：
```c
static void my_get_drvinfo(struct net_device *dev,
                           struct ethtool_drvinfo *info)
{
    struct my_priv *priv = netdev_priv(dev);
    
    strlcpy(info->driver, "my_driver", sizeof(info->driver));
    strlcpy(info->version, DRV_VERSION, sizeof(info->version));
    strlcpy(info->bus_info, pci_name(priv->pdev), sizeof(info->bus_info));
}

static int my_get_link_ksettings(struct net_device *dev,
                                  struct ethtool_link_ksettings *cmd)
{
    struct my_priv *priv = netdev_priv(dev);
    
    // 如果使用phylib
    return phylink_ethtool_ksettings_get(priv->phylink, cmd);
    
    // 或手动填写
    cmd->base.speed = priv->speed;
    cmd->base.duplex = priv->duplex;
    cmd->base.autoneg = priv->autoneg;
    
    return 0;
}

static u32 my_get_link(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    return priv->link_up;
}

static const struct ethtool_ops my_ethtool_ops = {
    .get_drvinfo        = my_get_drvinfo,
    .get_link_ksettings = my_get_link_ksettings,
    .set_link_ksettings = my_set_link_ksettings,
    .get_link           = my_get_link,
    .get_msglevel       = my_get_msglevel,
    .set_msglevel       = my_set_msglevel,
    .get_regs_len       = my_get_regs_len,
    .get_regs           = my_get_regs,
    .get_coalesce       = my_get_coalesce,
    .set_coalesce       = my_set_coalesce,
};

// 设置ethtool_ops
dev->ethtool_ops = &my_ethtool_ops;
```

用户空间使用：
```bash
ethtool eth0                    # 显示链路设置
ethtool -i eth0                 # 驱动信息
ethtool -S eth0                 # 统计信息
ethtool -s eth0 speed 1000      # 设置速度
ethtool -C eth0 rx-usecs 100    # 中断合并
ethtool -G eth0 rx 512          # 环形缓冲区大小
ethtool -k eth0                 # 查看卸载功能
ethtool -K eth0 rx on           # 启用RX校验和卸载
```
[Intermediate]

---

## 9. PHY and Link Management (PHY和链路管理)

---

Q: How to integrate with PHY using phylib?
A: phylib提供PHY层抽象：

```c
#include <linux/phy.h>

// 在probe中连接PHY
static int my_probe(struct platform_device *pdev)
{
    struct net_device *dev;
    struct my_priv *priv;
    struct phy_device *phydev;
    
    // ... 分配设备 ...
    
    // 连接PHY
    phydev = of_phy_connect(dev, priv->phy_node,
                            my_adjust_link, 0, priv->phy_interface);
    // 或
    phydev = phy_connect(dev, bus_id, my_adjust_link, priv->phy_interface);
    
    if (!phydev) {
        dev_err(&pdev->dev, "Could not connect PHY\n");
        return -ENODEV;
    }
    
    // 配置PHY支持的特性
    phy_set_max_speed(phydev, SPEED_1000);
    phy_support_asym_pause(phydev);
    
    priv->phydev = phydev;
    
    return 0;
}

// 链路变化回调
static void my_adjust_link(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    struct phy_device *phydev = priv->phydev;
    bool link_changed = false;
    
    if (phydev->link) {
        // 链路建立
        if (priv->speed != phydev->speed) {
            priv->speed = phydev->speed;
            link_changed = true;
            // 配置MAC速度
            set_mac_speed(priv, phydev->speed);
        }
        
        if (priv->duplex != phydev->duplex) {
            priv->duplex = phydev->duplex;
            link_changed = true;
            // 配置MAC双工
            set_mac_duplex(priv, phydev->duplex);
        }
        
        if (!priv->link_up) {
            priv->link_up = true;
            link_changed = true;
            netif_carrier_on(dev);
        }
    } else {
        // 链路断开
        if (priv->link_up) {
            priv->link_up = false;
            link_changed = true;
            netif_carrier_off(dev);
        }
    }
    
    if (link_changed)
        phy_print_status(phydev);
}

// 在open中启动PHY
static int my_open(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    
    // ... 其他初始化 ...
    
    phy_start(priv->phydev);
    
    return 0;
}

// 在stop中停止PHY
static int my_stop(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    
    phy_stop(priv->phydev);
    
    // ... 其他清理 ...
    
    return 0;
}

// 在remove中断开PHY
static void my_remove(struct platform_device *pdev)
{
    struct net_device *dev = platform_get_drvdata(pdev);
    struct my_priv *priv = netdev_priv(dev);
    
    phy_disconnect(priv->phydev);
    
    // ... 其他清理 ...
}
```

carrier状态：
```c
// 设置载波状态（链路状态）
netif_carrier_on(dev);    // 链路建立
netif_carrier_off(dev);   // 链路断开
netif_carrier_ok(dev);    // 检查状态
```
[Intermediate]

---

## 10. Multi-Queue Support (多队列支持)

---

Q: How to implement multi-queue network driver?
A: 多队列提高并行处理能力：

```c
// 分配多队列设备
#define NUM_TX_QUEUES 4
#define NUM_RX_QUEUES 4

struct net_device *dev;
dev = alloc_etherdev_mqs(sizeof(struct my_priv),
                         NUM_TX_QUEUES, NUM_RX_QUEUES);

// 设置实际队列数
dev->real_num_tx_queues = NUM_TX_QUEUES;
dev->real_num_rx_queues = NUM_RX_QUEUES;

// 队列选择函数
static u16 my_select_queue(struct net_device *dev, struct sk_buff *skb,
                           struct net_device *sb_dev)
{
    // 默认：基于流哈希
    return netdev_pick_tx(dev, skb, sb_dev);
    
    // 或自定义逻辑
    // 例如：基于源端口
    if (skb->protocol == htons(ETH_P_IP)) {
        struct iphdr *iph = ip_hdr(skb);
        return (ntohs(iph->id) % dev->real_num_tx_queues);
    }
    return 0;
}

// 访问特定队列
struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);

// 多NAPI实例（每个RX队列一个）
struct my_priv {
    struct napi_struct napi[NUM_RX_QUEUES];
    // ...
};

// 注册多个NAPI
for (i = 0; i < NUM_RX_QUEUES; i++) {
    netif_napi_add(dev, &priv->napi[i], my_poll, NAPI_POLL_WEIGHT);
}

// 多队列中断处理
static irqreturn_t my_msix_handler(int irq, void *data)
{
    struct my_queue *queue = data;
    struct my_priv *priv = queue->priv;
    
    // 调度对应的NAPI
    napi_schedule(&priv->napi[queue->index]);
    
    return IRQ_HANDLED;
}
```

RSS (Receive Side Scaling)：
```c
// 设置RSS哈希键和间接表
static int my_set_rxfh(struct net_device *dev, const u32 *indir,
                       const u8 *key, const u8 hfunc)
{
    struct my_priv *priv = netdev_priv(dev);
    
    // 设置间接表
    if (indir) {
        for (i = 0; i < INDIR_TABLE_SIZE; i++)
            priv->rss_indir[i] = indir[i];
        write_rss_indir_table(priv);
    }
    
    // 设置哈希键
    if (key) {
        memcpy(priv->rss_key, key, RSS_KEY_SIZE);
        write_rss_key(priv);
    }
    
    return 0;
}
```
[Advanced]

---

## 11. Virtual Network Devices (虚拟网络设备)

---

Q: What are common virtual network devices?
A: 常见虚拟网络设备：

```
+------------------------------------------------------------------+
|                  Virtual Network Devices                          |
+------------------------------------------------------------------+
|                                                                  |
|  +----------------+  +----------------+  +----------------+      |
|  |    loopback    |  |     veth       |  |    bridge      |      |
|  |  (回环设备)     |  |  (虚拟以太网)   |  |    (桥接)      |      |
|  +----------------+  +----------------+  +----------------+      |
|                                                                  |
|  +----------------+  +----------------+  +----------------+      |
|  |      tun       |  |      tap       |  |    macvlan     |      |
|  | (三层隧道)      |  |  (二层隧道)     |  |  (MAC虚拟)     |      |
|  +----------------+  +----------------+  +----------------+      |
|                                                                  |
|  +----------------+  +----------------+  +----------------+      |
|  |     bond       |  |     vlan       |  |    vxlan       |      |
|  |   (链路聚合)    |  | (VLAN子接口)   |  |  (VXLAN隧道)   |      |
|  +----------------+  +----------------+  +----------------+      |
|                                                                  |
+------------------------------------------------------------------+
```

创建简单虚拟设备：
```c
#include <linux/netdevice.h>

// 虚拟设备发送（直接回环）
static netdev_tx_t virt_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct net_device *peer = netdev_priv(dev);
    
    // 更新统计
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += skb->len;
    
    // 修改skb，传递给对端
    skb->dev = peer;
    skb->protocol = eth_type_trans(skb, peer);
    
    peer->stats.rx_packets++;
    peer->stats.rx_bytes += skb->len;
    
    // 传递给协议栈
    netif_rx(skb);
    
    return NETDEV_TX_OK;
}

static const struct net_device_ops virt_netdev_ops = {
    .ndo_start_xmit = virt_xmit,
    .ndo_open       = virt_open,
    .ndo_stop       = virt_stop,
};

// 设置函数
static void virt_setup(struct net_device *dev)
{
    ether_setup(dev);
    
    dev->netdev_ops = &virt_netdev_ops;
    dev->needs_free_netdev = true;
    
    // 随机MAC地址
    eth_hw_addr_random(dev);
}

// 创建设备
struct net_device *dev;
dev = alloc_netdev(sizeof(void *), "virt%d", NET_NAME_UNKNOWN, virt_setup);
register_netdev(dev);
```

rtnl_link_ops（用于ip link命令）：
```c
static struct rtnl_link_ops virt_link_ops = {
    .kind       = "virt",
    .priv_size  = sizeof(struct virt_priv),
    .setup      = virt_setup,
    .validate   = virt_validate,
    .newlink    = virt_newlink,
    .dellink    = virt_dellink,
};

// 注册
rtnl_link_register(&virt_link_ops);

// 用户空间创建
// ip link add virt0 type virt
```
[Intermediate]

---

## 12. XDP (eXpress Data Path)

---

Q: What is XDP and how to support it?
A: XDP在驱动层提供高性能包处理：

```
+------------------------------------------------------------------+
|                    XDP Architecture                               |
+------------------------------------------------------------------+
|                                                                  |
|  网卡硬件                                                        |
|     |                                                            |
|     v                                                            |
|  DMA接收完成                                                      |
|     |                                                            |
|     v                                                            |
|  +----------------------------------------------------------+   |
|  |                   XDP Program (eBPF)                      |   |
|  |  在NAPI之前运行，极低延迟                                  |   |
|  +--------+-------------+-------------+-------------+-------+   |
|           |             |             |             |            |
|           v             v             v             v            |
|       XDP_DROP      XDP_TX      XDP_REDIRECT   XDP_PASS         |
|       (丢弃)        (发回)      (重定向)       (继续处理)        |
|           |             |             |             |            |
|           v             v             v             v            |
|         drop         发送         其他CPU/     正常协议栈        |
|                                   其他设备                       |
|                                                                  |
+------------------------------------------------------------------+
```

驱动XDP支持：
```c
// ndo_bpf回调
static int my_xdp(struct net_device *dev, struct netdev_bpf *xdp)
{
    struct my_priv *priv = netdev_priv(dev);
    
    switch (xdp->command) {
    case XDP_SETUP_PROG:
        return my_xdp_setup(dev, xdp->prog, xdp->extack);
    case XDP_QUERY_PROG:
        xdp->prog_id = priv->xdp_prog ? 
                       bpf_prog_get_id(priv->xdp_prog) : 0;
        return 0;
    default:
        return -EINVAL;
    }
}

static int my_xdp_setup(struct net_device *dev, struct bpf_prog *prog,
                        struct netlink_ext_ack *extack)
{
    struct my_priv *priv = netdev_priv(dev);
    struct bpf_prog *old_prog;
    
    // 保存旧程序
    old_prog = xchg(&priv->xdp_prog, prog);
    
    // 释放旧程序
    if (old_prog)
        bpf_prog_put(old_prog);
    
    return 0;
}

// 在NAPI poll中运行XDP
static int my_poll(struct napi_struct *napi, int budget)
{
    struct my_priv *priv = container_of(napi, struct my_priv, napi);
    struct bpf_prog *xdp_prog = READ_ONCE(priv->xdp_prog);
    
    while (work_done < budget) {
        // ... 获取数据包 ...
        
        if (xdp_prog) {
            struct xdp_buff xdp;
            u32 act;
            
            // 设置xdp_buff
            xdp.data = data;
            xdp.data_end = data + len;
            xdp.data_hard_start = data - headroom;
            xdp.rxq = &priv->xdp_rxq;
            
            // 运行XDP程序
            act = bpf_prog_run_xdp(xdp_prog, &xdp);
            
            switch (act) {
            case XDP_PASS:
                // 继续正常处理
                break;
            case XDP_TX:
                // 从同一设备发回
                my_xdp_xmit(dev, &xdp);
                continue;
            case XDP_REDIRECT:
                // 重定向到其他设备
                xdp_do_redirect(dev, &xdp, xdp_prog);
                continue;
            default:
            case XDP_DROP:
                // 丢弃
                continue;
            }
        }
        
        // 正常处理路径
        // ...
    }
}
```

用户空间加载XDP：
```bash
# 使用ip命令
ip link set dev eth0 xdp obj xdp_prog.o sec xdp

# 或使用bpftool
bpftool prog load xdp_prog.o /sys/fs/bpf/xdp_prog
bpftool net attach xdp id <prog_id> dev eth0
```
[Advanced]

---

## 13. Network Debugging (网络调试)

---

Q: How to debug network drivers?
A: 网络驱动调试方法：

```bash
# 查看网络设备
ip link show
ifconfig -a
ethtool eth0

# 查看统计信息
ip -s link show eth0
ethtool -S eth0
cat /proc/net/dev

# 查看驱动信息
ethtool -i eth0
modinfo <driver_name>

# 查看队列状态
cat /sys/class/net/eth0/queues/tx-*/xps_cpus
cat /sys/class/net/eth0/queues/rx-*/rps_cpus

# 查看NAPI状态
cat /proc/softirqs | grep NET

# 网络跟踪
tcpdump -i eth0 -n
wireshark &

# 内核跟踪
echo 1 > /sys/kernel/debug/tracing/events/net/enable
cat /sys/kernel/debug/tracing/trace_pipe

# perf分析
perf top -e net:*
perf record -e net:netif_receive_skb -a
```

驱动调试打印：
```c
// 使用netdev_*系列
netdev_info(dev, "Device initialized\n");
netdev_err(dev, "Error: %d\n", err);
netdev_dbg(dev, "Debug message\n");
netdev_warn(dev, "Warning\n");

// 动态调试
echo 'module my_driver +p' > /sys/kernel/debug/dynamic_debug/control

// 消息级别控制（通过ethtool）
static u32 my_get_msglevel(struct net_device *dev)
{
    struct my_priv *priv = netdev_priv(dev);
    return priv->msg_enable;
}

// 使用条件打印
if (netif_msg_rx_status(priv))
    netdev_dbg(dev, "RX status: %x\n", status);

// ethtool设置
ethtool -s eth0 msglvl 0xff
```

常见问题排查：
```c
// 1. 没有收到数据包
// - 检查中断是否触发
// - 检查NAPI是否调度
// - 检查描述符状态
// - 检查netif_carrier状态

// 2. 发送失败
// - 检查队列是否停止
// - 检查描述符空间
// - 检查DMA映射

// 3. 性能问题
// - 检查中断合并设置
// - 检查多队列配置
// - 检查GRO/GSO状态
// - 使用perf分析热点
```
[Basic]

---

## 14. Summary (总结)

---

Q: Summarize Linux network device driver architecture.
A: 
```
+==================================================================+
||           LINUX NETWORK DEVICE DRIVER ARCHITECTURE             ||
+==================================================================+

核心数据结构:
+------------------------------------------------------------------+
|  net_device           网络设备主结构                              |
|  net_device_ops       设备操作回调                                |
|  napi_struct          NAPI轮询结构                                |
|  sk_buff              网络数据包缓冲区                            |
|  ethtool_ops          ethtool配置接口                             |
+------------------------------------------------------------------+


数据流:
+------------------------------------------------------------------+
|  TX Path:                                                        |
|  协议层 -> dev_queue_xmit -> ndo_start_xmit -> DMA -> 硬件       |
|                                                                  |
|  RX Path:                                                        |
|  硬件 -> DMA -> 中断 -> NAPI poll -> napi_gro_receive -> 协议层  |
+------------------------------------------------------------------+


关键机制:
+----------------+--------------------------------------------------+
| NAPI           | 中断+轮询混合，减少中断开销                        |
| DMA Ring       | 环形描述符，高效硬件交互                           |
| 多队列         | 并行处理，提高吞吐量                               |
| GRO/GSO        | 合并/分段卸载，减少CPU负载                         |
| XDP            | 驱动层包处理，极低延迟                             |
+----------------+--------------------------------------------------+


驱动生命周期:
    probe()
        |
        +---> 分配net_device
        +---> 设置ops
        +---> 注册NAPI
        +---> register_netdev()
        |
    open() (ifconfig up)
        |
        +---> 分配缓冲区
        +---> 请求中断
        +---> napi_enable()
        +---> netif_start_queue()
        +---> phy_start()
        |
    运行中
        |
        +---> ndo_start_xmit() (发送)
        +---> 中断 -> napi_schedule() -> poll() (接收)
        |
    stop() (ifconfig down)
        |
        +---> phy_stop()
        +---> netif_stop_queue()
        +---> napi_disable()
        +---> 释放资源
        |
    remove()
        |
        +---> unregister_netdev()
        +---> netif_napi_del()
        +---> free_netdev()


性能优化:
    - 中断合并 (ethtool -C)
    - 多队列/RSS
    - GRO/GSO
    - XDP
    - 零拷贝
```
[Basic]

---

*Total: 100+ cards covering Linux kernel network device driver implementation*

