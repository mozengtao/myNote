# 网络设备 ops 中的依赖注入模式

> 文件路径: `/tmp/linux-ioc-patterns/04_net_device_ops.md`
> 内核版本: Linux 3.2
> 难度: ⭐⭐⭐

---

## 1. 模式概述

网络子系统使用 `net_device_ops` 结构实现协议栈与网卡驱动的完全解耦。TCP/IP 协议栈通过统一的接口发送数据包，而不关心底层是真实网卡、虚拟网卡还是回环设备。

### DI/IoC 的具体表现形式

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     网络子系统的依赖注入架构                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   用户空间                                                                   │
│   ┌────────────────────────────────────────────────────────────────────┐    │
│   │           socket() / sendto() / recvfrom()                         │    │
│   └────────────────────────────────────────────────────────────────────┘    │
│                                    │                                         │
│   ═══════════════════════════════╪═══════════════════════════════════════   │
│                                    │  系统调用                              │
│                                    ▼                                         │
│   ┌────────────────────────────────────────────────────────────────────┐    │
│   │                       协议栈 (TCP/UDP/IP)                          │    │
│   │                                                                     │    │
│   │   tcp_transmit_skb() → ip_queue_xmit() → dev_queue_xmit()         │    │
│   │                                                │                    │    │
│   │                                                │  统一接口          │    │
│   │                                                ▼                    │    │
│   │        dev->netdev_ops->ndo_start_xmit(skb, dev)                   │    │
│   │                                                                     │    │
│   └─────────────────────────────────┬──────────────────────────────────┘    │
│                                     │                                        │
│           不同的 netdev_ops         │                                        │
│           ┌─────────────────────────┼─────────────────────────┐             │
│           │                         │                         │              │
│           ▼                         ▼                         ▼              │
│   ┌───────────────┐         ┌───────────────┐         ┌───────────────┐     │
│   │ e1000_netdev  │         │  loopback_ops │         │  virtio_ops   │     │
│   │     _ops      │         │               │         │               │     │
│   │               │         │               │         │               │     │
│   │.ndo_start_xmit│         │.ndo_start_xmit│         │.ndo_start_xmit│     │
│   │ = e1000_xmit  │         │ = loopback_   │         │ = virtio_xmit │     │
│   │               │         │     xmit      │         │               │     │
│   └───────┬───────┘         └───────┬───────┘         └───────┬───────┘     │
│           │                         │                         │              │
│           ▼                         ▼                         ▼              │
│   ┌───────────────┐         ┌───────────────┐         ┌───────────────┐     │
│   │  Intel 网卡   │         │  直接返回到   │         │  虚拟机通信   │     │
│   │   硬件发送    │         │   接收路径    │         │    通道       │     │
│   └───────────────┘         └───────────────┘         └───────────────┘     │
│                                                                              │
│   控制反转:                                                                  │
│   • 协议栈不知道如何操作具体网卡                                             │
│   • 网卡驱动通过 netdev_ops 注入自己的实现                                   │
│   • 同样是 xmit()，根据设备类型路由到不同实现                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 设计动机

### 要解决的问题

| 问题 | 解决方案 |
|------|----------|
| **网卡硬件多样性** | 统一接口，不同实现 |
| **虚拟网络设备** | 虚拟设备也实现相同接口 |
| **网卡热插拔** | 运行时可以添加/移除网卡 |
| **中间层设备** | bonding、bridge 等复用相同框架 |
| **硬件加速差异** | 通过 features 标志抽象硬件能力 |

### 设计目标

1. **协议栈与驱动解耦**: 协议栈不依赖具体硬件
2. **支持多种设备**: 真实网卡、虚拟网卡、隧道设备
3. **支持设备层叠**: bonding → 真实网卡
4. **硬件特性抽象**: TSO、GSO、校验和卸载等

---

## 3. 核心数据结构

### 3.1 net_device_ops - 网络设备操作接口

```c
// include/linux/netdevice.h (第 859-930 行)

struct net_device_ops {
    // ===== 设备生命周期 =====
    int     (*ndo_init)(struct net_device *dev);
    void    (*ndo_uninit)(struct net_device *dev);
    int     (*ndo_open)(struct net_device *dev);      // ifconfig up
    int     (*ndo_stop)(struct net_device *dev);      // ifconfig down

    // ===== 数据发送 (最重要!) =====
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);

    // ===== 多队列支持 =====
    u16     (*ndo_select_queue)(struct net_device *dev, struct sk_buff *skb);

    // ===== 接收模式 =====
    void    (*ndo_change_rx_flags)(struct net_device *dev, int flags);
    void    (*ndo_set_rx_mode)(struct net_device *dev);

    // ===== MAC 地址 =====
    int     (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int     (*ndo_validate_addr)(struct net_device *dev);

    // ===== ioctl =====
    int     (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd);

    // ===== 配置 =====
    int     (*ndo_set_config)(struct net_device *dev, struct ifmap *map);
    int     (*ndo_change_mtu)(struct net_device *dev, int new_mtu);

    // ===== 超时处理 =====
    void    (*ndo_tx_timeout)(struct net_device *dev);

    // ===== 统计信息 =====
    struct rtnl_link_stats64* (*ndo_get_stats64)(struct net_device *dev,
                                    struct rtnl_link_stats64 *storage);
    struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);

    // ===== VLAN 支持 =====
    void    (*ndo_vlan_rx_add_vid)(struct net_device *dev, unsigned short vid);
    void    (*ndo_vlan_rx_kill_vid)(struct net_device *dev, unsigned short vid);

#ifdef CONFIG_NET_POLL_CONTROLLER
    // 网络轮询 (用于 netconsole)
    void    (*ndo_poll_controller)(struct net_device *dev);
#endif

    // ===== SR-IOV 虚拟化 =====
    int     (*ndo_set_vf_mac)(struct net_device *dev, int vf, u8 *mac);
    int     (*ndo_set_vf_vlan)(struct net_device *dev, int vf, u16 vlan, u8 qos);
    int     (*ndo_set_vf_tx_rate)(struct net_device *dev, int vf, int rate);
    int     (*ndo_get_vf_config)(struct net_device *dev, int vf,
                                  struct ifla_vf_info *ivf);

    // ===== 流量控制 =====
    int     (*ndo_setup_tc)(struct net_device *dev, u8 tc);

    // ===== FCoE 支持 =====
    int     (*ndo_fcoe_enable)(struct net_device *dev);
    int     (*ndo_fcoe_disable)(struct net_device *dev);

    // ===== 主从设备 (bonding, bridge) =====
    int     (*ndo_add_slave)(struct net_device *dev,
                              struct net_device *slave_dev);
    int     (*ndo_del_slave)(struct net_device *dev,
                              struct net_device *slave_dev);

    // ===== 硬件特性 =====
    u32     (*ndo_fix_features)(struct net_device *dev, u32 features);
    int     (*ndo_set_features)(struct net_device *dev, u32 features);
};
```

### 3.2 net_device - 网络设备结构

```c
// include/linux/netdevice.h (部分关键字段)

struct net_device {
    char            name[IFNAMSIZ];    // 设备名 (如 "eth0")

    // 硬件信息
    unsigned long   mem_end;
    unsigned long   mem_start;
    unsigned long   base_addr;
    unsigned int    irq;

    // 状态
    unsigned long   state;
    unsigned int    flags;             // IFF_UP, IFF_BROADCAST 等

    // MTU
    unsigned int    mtu;

    // 硬件地址
    unsigned char   dev_addr[MAX_ADDR_LEN];

    // ===== 依赖注入点 =====
    const struct net_device_ops *netdev_ops;     // 设备操作
    const struct ethtool_ops    *ethtool_ops;    // ethtool 操作

    // 硬件特性
    u32             features;          // NETIF_F_* 标志
    u32             hw_features;
    u32             vlan_features;

    // 发送队列
    struct netdev_queue *_tx;
    unsigned int    num_tx_queues;
    unsigned int    real_num_tx_queues;

    // 接收队列
    struct netdev_rx_queue *_rx;
    unsigned int    num_rx_queues;

    // 统计信息
    struct net_device_stats stats;

    // ...
};
```

### 3.3 数据结构关系

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        网络设备数据结构关系                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        struct net_device                             │   │
│   │                                                                      │   │
│   │   name = "eth0"                                                      │   │
│   │   flags = IFF_UP | IFF_BROADCAST | IFF_MULTICAST                    │   │
│   │   mtu = 1500                                                         │   │
│   │   dev_addr = { 00:11:22:33:44:55 }                                  │   │
│   │                                                                      │   │
│   │   netdev_ops ─────────────────────────────────────────────┐         │   │
│   │   ethtool_ops ────────────────────────────────────────┐   │         │   │
│   │                                                        │   │         │   │
│   │   features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO│   │         │   │
│   │                                                        │   │         │   │
│   │   _tx[0..num_tx_queues] ───► struct netdev_queue      │   │         │   │
│   │   _rx[0..num_rx_queues] ───► struct netdev_rx_queue   │   │         │   │
│   │                                                        │   │         │   │
│   └────────────────────────────────────────────────────────┼───┼─────────┘   │
│                                                            │   │             │
│                                                            ▼   ▼             │
│   ┌────────────────────────────────┐    ┌────────────────────────────────┐  │
│   │   struct net_device_ops        │    │   struct ethtool_ops           │  │
│   │                                │    │                                │  │
│   │   .ndo_open = e1000_open      │    │   .get_settings = e1000_get_  │  │
│   │   .ndo_stop = e1000_close     │    │                     settings  │  │
│   │   .ndo_start_xmit = e1000_xmit│    │   .set_settings = e1000_set_  │  │
│   │   .ndo_get_stats = e1000_stats│    │                     settings  │  │
│   │   .ndo_set_mac = e1000_set_mac│    │   .get_drvinfo = e1000_get_   │  │
│   │   ...                          │    │                     drvinfo   │  │
│   └────────────────────────────────┘    └────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. 代码流程分析

### 4.1 数据发送路径

```c
// net/core/dev.c (第 2400-2500 行)

int dev_queue_xmit(struct sk_buff *skb)
{
    struct net_device *dev = skb->dev;
    struct netdev_queue *txq;
    struct Qdisc *q;
    int rc = -ENOMEM;

    // 选择发送队列
    txq = netdev_pick_tx(dev, skb);
    q = rcu_dereference_bh(txq->qdisc);

    if (q->enqueue) {
        // 有队列规则 (如 TC)
        rc = __dev_xmit_skb(skb, q, dev, txq);
        goto out;
    }

    // 直接发送 (无队列)
    if (dev->flags & IFF_UP) {
        int cpu = smp_processor_id();

        if (txq->xmit_lock_owner != cpu) {
            HARD_TX_LOCK(dev, txq, cpu);

            if (!netif_tx_queue_stopped(txq)) {
                // 关键: 调用注入的发送函数
                rc = dev_hard_start_xmit(skb, dev, txq);
            }

            HARD_TX_UNLOCK(dev, txq);
        }
    }

out:
    return rc;
}

// 实际调用驱动
int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev,
                        struct netdev_queue *txq)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    int rc;

    // 控制反转: 调用驱动注入的 xmit 函数
    rc = ops->ndo_start_xmit(skb, dev);

    if (rc == NETDEV_TX_OK)
        txq_trans_update(txq);

    return rc;
}
```

### 4.2 设备打开/关闭

```c
// net/core/dev.c

int dev_open(struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    int ret;

    if (dev->flags & IFF_UP)
        return 0;

    // 调用驱动注入的 open 函数
    if (ops->ndo_open)
        ret = ops->ndo_open(dev);
    else
        ret = 0;

    if (ret)
        return ret;

    // 设置设备状态
    dev->flags |= IFF_UP;
    dev_set_rx_mode(dev);
    dev_activate(dev);

    // 发送 netlink 通知
    call_netdevice_notifiers(NETDEV_UP, dev);

    return 0;
}

int dev_close(struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;

    if (!(dev->flags & IFF_UP))
        return 0;

    // 停止发送队列
    dev_deactivate(dev);

    // 清除状态
    dev->flags &= ~IFF_UP;

    // 调用驱动注入的 stop 函数
    if (ops->ndo_stop)
        ops->ndo_stop(dev);

    // 发送 netlink 通知
    call_netdevice_notifiers(NETDEV_DOWN, dev);

    return 0;
}
```

### 4.3 完整调用流程

```
用户空间:
    sendto(sockfd, buf, len, 0, &addr, sizeof(addr));

                    │
                    ▼
════════════════════════════════════════════════════════════════════
                    │  系统调用
                    ▼
┌────────────────────────────────────────────────────────────────────┐
│  sys_sendto()                                                      │
│      │                                                             │
│      ▼                                                             │
│  sock_sendmsg()                                                    │
│      │                                                             │
│      ▼                                                             │
│  inet_sendmsg()         (协议族层)                                 │
│      │                                                             │
│      ▼                                                             │
│  tcp_sendmsg() / udp_sendmsg()   (传输层)                         │
│      │                                                             │
│      ▼                                                             │
│  ip_queue_xmit()        (网络层)                                   │
│      │                                                             │
│      │  添加 IP 头，查找路由                                       │
│      │                                                             │
│      ▼                                                             │
│  ip_local_out()                                                    │
│      │                                                             │
│      ▼                                                             │
│  ip_output() → ip_finish_output()                                  │
│      │                                                             │
│      │  分片 (如需要)                                              │
│      │                                                             │
│      ▼                                                             │
│  dev_queue_xmit(skb)    (链路层入口)                              │
│      │                                                             │
│      │  // 选择发送队列                                            │
│      │  txq = netdev_pick_tx(dev, skb);                           │
│      │                                                             │
│      │  // 检查队列规则                                            │
│      │                                                             │
│      ▼                                                             │
│  dev_hard_start_xmit(skb, dev, txq)                               │
│      │                                                             │
│      │  // 控制反转: 调用驱动注入的函数                            │
│      │                                                             │
│      ▼                                                             │
│  dev->netdev_ops->ndo_start_xmit(skb, dev)                        │
│      │                                                             │
│      │  // 如果是 e1000 网卡                                       │
│      │                                                             │
│      ▼                                                             │
│  e1000_xmit_frame(skb, dev)                                       │
│      │                                                             │
│      │  1. 获取 TX 描述符                                         │
│      │  2. 设置 DMA 地址                                          │
│      │  3. 更新 TX tail 寄存器                                    │
│      │  4. 触发硬件发送                                            │
│      │                                                             │
└──────┼─────────────────────────────────────────────────────────────┘
       │
       ▼
  [ 硬件网卡发送数据包 ]
```

---

## 5. 实际案例

### 案例1: Intel e1000 网卡驱动

```c
// drivers/net/ethernet/intel/e1000/e1000_main.c

// 发送数据包
static netdev_tx_t e1000_xmit_frame(struct sk_buff *skb,
                                    struct net_device *netdev)
{
    struct e1000_adapter *adapter = netdev_priv(netdev);
    struct e1000_hw *hw = &adapter->hw;
    struct e1000_tx_ring *tx_ring = adapter->tx_ring;
    unsigned int first, tx_flags = 0;
    unsigned int len = skb->len;
    int count = 0;

    // 检查队列是否有空间
    if (unlikely(e1000_maybe_stop_tx(netdev, tx_ring,
                                      skb_shinfo(skb)->nr_frags + 2))) {
        return NETDEV_TX_BUSY;
    }

    // 处理 TSO (TCP Segmentation Offload)
    if (skb_is_gso(skb)) {
        if (e1000_tso(adapter, tx_ring, skb))
            tx_flags |= E1000_TX_FLAGS_TSO;
    }

    // 处理校验和卸载
    if (skb->ip_summed == CHECKSUM_PARTIAL) {
        if (e1000_tx_csum(adapter, tx_ring, skb))
            tx_flags |= E1000_TX_FLAGS_CSUM;
    }

    // 获取第一个描述符
    first = tx_ring->next_to_use;

    // 映射 skb 到 DMA
    count = e1000_tx_map(adapter, tx_ring, skb, first,
                         skb->len, skb_shinfo(skb)->nr_frags);

    if (count) {
        // 设置发送描述符
        e1000_tx_queue(adapter, tx_ring, tx_flags, count);

        // 更新尾指针，触发硬件发送
        writel(tx_ring->next_to_use, hw->hw_addr + tx_ring->tdt);

        // 更新时间戳
        netdev->trans_start = jiffies;
    }

    return NETDEV_TX_OK;
}

// 打开网卡
static int e1000_open(struct net_device *netdev)
{
    struct e1000_adapter *adapter = netdev_priv(netdev);
    struct e1000_hw *hw = &adapter->hw;
    int err;

    // 阻止电源状态变化
    netif_carrier_off(netdev);

    // 分配发送/接收环
    err = e1000_setup_all_tx_resources(adapter);
    if (err)
        goto err_setup_tx;

    err = e1000_setup_all_rx_resources(adapter);
    if (err)
        goto err_setup_rx;

    // 启动硬件
    e1000_power_up_phy(adapter);
    e1000_configure(adapter);

    // 注册中断
    err = e1000_request_irq(adapter);
    if (err)
        goto err_req_irq;

    // 启用中断
    e1000_irq_enable(adapter);

    // 启动发送队列
    netif_start_queue(netdev);

    return 0;

err_req_irq:
    e1000_free_all_rx_resources(adapter);
err_setup_rx:
    e1000_free_all_tx_resources(adapter);
err_setup_tx:
    e1000_reset(adapter);

    return err;
}

// 关闭网卡
static int e1000_close(struct net_device *netdev)
{
    struct e1000_adapter *adapter = netdev_priv(netdev);

    // 停止发送队列
    netif_stop_queue(netdev);

    // 禁用中断
    e1000_irq_disable(adapter);

    // 释放中断
    e1000_free_irq(adapter);

    // 释放发送/接收资源
    e1000_free_all_tx_resources(adapter);
    e1000_free_all_rx_resources(adapter);

    // 关闭硬件
    e1000_power_down_phy(adapter);

    return 0;
}

// 网卡操作集 - 依赖注入
static const struct net_device_ops e1000_netdev_ops = {
    .ndo_open               = e1000_open,           // 打开
    .ndo_stop               = e1000_close,          // 关闭
    .ndo_start_xmit         = e1000_xmit_frame,     // 发送
    .ndo_get_stats          = e1000_get_stats,      // 统计
    .ndo_set_rx_mode        = e1000_set_rx_mode,    // 接收模式
    .ndo_set_mac_address    = e1000_set_mac,        // MAC 地址
    .ndo_tx_timeout         = e1000_tx_timeout,     // 超时
    .ndo_change_mtu         = e1000_change_mtu,     // MTU
    .ndo_do_ioctl           = e1000_ioctl,          // ioctl
    .ndo_validate_addr      = eth_validate_addr,    // 通用实现
    .ndo_vlan_rx_add_vid    = e1000_vlan_rx_add_vid,
    .ndo_vlan_rx_kill_vid   = e1000_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
    .ndo_poll_controller    = e1000_netpoll,
#endif
    .ndo_fix_features       = e1000_fix_features,
    .ndo_set_features       = e1000_set_features,
};

// PCI probe 时绑定 ops
static int e1000_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct net_device *netdev;
    struct e1000_adapter *adapter;

    // 分配网络设备
    netdev = alloc_etherdev(sizeof(struct e1000_adapter));
    if (!netdev)
        return -ENOMEM;

    // 依赖注入: 绑定操作集
    netdev->netdev_ops = &e1000_netdev_ops;
    netdev->ethtool_ops = &e1000_ethtool_ops;

    // 设置硬件特性
    netdev->features = NETIF_F_SG | NETIF_F_HW_CSUM |
                       NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;

    // 注册网络设备
    err = register_netdev(netdev);
    if (err)
        goto err_register;

    return 0;
}
```

### 案例2: 回环设备 (loopback)

```c
// drivers/net/loopback.c

// 回环发送: 直接转到接收路径
static netdev_tx_t loopback_xmit(struct sk_buff *skb,
                                 struct net_device *dev)
{
    struct pcpu_lstats *lb_stats;
    int len;

    // 准备接收
    skb_orphan(skb);

    // 获取长度 (用于统计)
    len = skb->len;

    // 更新统计
    lb_stats = this_cpu_ptr(dev->lstats);
    u64_stats_update_begin(&lb_stats->syncp);
    lb_stats->bytes += len;
    lb_stats->packets++;
    u64_stats_update_end(&lb_stats->syncp);

    // 设置协议
    skb->protocol = eth_type_trans(skb, dev);

    // 直接送入接收路径 (控制反转: 发送变成接收)
    if (likely(netif_rx(skb) == NET_RX_SUCCESS)) {
        return NETDEV_TX_OK;
    }

    return NETDEV_TX_OK;
}

// 回环设备的 ops (非常简单)
static const struct net_device_ops loopback_ops = {
    .ndo_init        = loopback_dev_init,
    .ndo_start_xmit  = loopback_xmit,        // 关键: 回环发送
    .ndo_get_stats64 = loopback_get_stats64,
};

// 设置回环设备
static void loopback_setup(struct net_device *dev)
{
    // 设置名称
    dev->name[0] = '\0';

    // 依赖注入: 绑定操作集
    dev->netdev_ops = &loopback_ops;

    // 设置 MTU (回环没有物理限制)
    dev->mtu = 64 * 1024;

    // 设置硬件特性
    dev->features = NETIF_F_SG | NETIF_F_FRAGLIST |
                    NETIF_F_ALL_TSO | NETIF_F_ALL_CSUM |
                    NETIF_F_HIGHDMA | NETIF_F_LLTX |
                    NETIF_F_NETNS_LOCAL | NETIF_F_VLAN_CHALLENGED;

    dev->flags = IFF_LOOPBACK;
}
```

### 案例3: bonding (链路聚合)

```c
// drivers/net/bonding/bond_main.c

// bonding 发送: 选择一个 slave 发送
static netdev_tx_t bond_start_xmit(struct sk_buff *skb,
                                   struct net_device *dev)
{
    struct bonding *bond = netdev_priv(dev);
    struct slave *slave;

    // 根据模式选择 slave
    switch (bond->params.mode) {
    case BOND_MODE_ROUNDROBIN:
        slave = bond_xmit_roundrobin(bond, skb);
        break;
    case BOND_MODE_ACTIVEBACKUP:
        slave = bond_xmit_activebackup(bond, skb);
        break;
    case BOND_MODE_XOR:
        slave = bond_xmit_xor(bond, skb);
        break;
    case BOND_MODE_8023AD:
        slave = bond_3ad_xmit_xor(bond, skb);
        break;
    // ...
    }

    if (slave) {
        // 设置 skb 的设备为 slave 设备
        skb->dev = slave->dev;
        skb->priority = 1;

        // 调用 slave 设备的发送函数 (再次控制反转)
        return slave->dev->netdev_ops->ndo_start_xmit(skb, slave->dev);
    }

    // 无可用 slave，丢弃
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

// bonding 的 ops
static const struct net_device_ops bond_netdev_ops = {
    .ndo_init           = bond_init,
    .ndo_uninit         = bond_uninit,
    .ndo_open           = bond_open,
    .ndo_stop           = bond_close,
    .ndo_start_xmit     = bond_start_xmit,      // 链路聚合发送
    .ndo_select_queue   = bond_select_queue,
    .ndo_get_stats64    = bond_get_stats,
    .ndo_do_ioctl       = bond_do_ioctl,
    .ndo_change_mtu     = bond_change_mtu,
    .ndo_set_mac_address = bond_set_mac_address,
    .ndo_add_slave      = bond_enslave,         // 添加 slave
    .ndo_del_slave      = bond_release,         // 移除 slave
    .ndo_fix_features   = bond_fix_features,
};
```

---

## 6. 优势分析

### 6.1 协议栈与硬件解耦

```c
// 协议栈不关心底层设备类型
dev_queue_xmit(skb);

// 底层可以是:
// - 真实网卡 (e1000, rtl8169, ...)
// - 虚拟网卡 (virtio, veth, tun/tap)
// - 无线网卡 (iwl, ath9k)
// - 回环设备 (lo)
// - 桥接设备 (bridge)
// - 聚合设备 (bonding)
```

### 6.2 设备层叠

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  应用程序                                                                   │
│      │                                                                       │
│      │  send() 到 bond0                                                     │
│      ▼                                                                       │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │  bond0 (bonding 设备)                                                │    │
│  │  netdev_ops = &bond_netdev_ops                                      │    │
│  │                                                                      │    │
│  │  bond_start_xmit() {                                                │    │
│  │      slave = 选择一个 slave;                                        │    │
│  │      slave->dev->netdev_ops->ndo_start_xmit(skb, slave->dev);      │    │
│  │  }                                                                   │    │
│  └──────────────────────────┬────────────────────────┬─────────────────┘    │
│                             │                        │                       │
│                             ▼                        ▼                       │
│  ┌───────────────────────────────┐  ┌───────────────────────────────┐       │
│  │  eth0 (真实网卡)              │  │  eth1 (真实网卡)              │       │
│  │  netdev_ops = &e1000_ops      │  │  netdev_ops = &e1000_ops      │       │
│  │                               │  │                               │       │
│  │  e1000_xmit_frame() {         │  │  e1000_xmit_frame() {         │       │
│  │      // 写入 DMA 描述符      │  │      // 写入 DMA 描述符      │       │
│  │      // 触发硬件发送          │  │      // 触发硬件发送          │       │
│  │  }                            │  │  }                            │       │
│  └───────────────────────────────┘  └───────────────────────────────┘       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.3 硬件特性抽象

```c
// 协议栈根据硬件能力调整行为
if (dev->features & NETIF_F_SG)
    // 使用 scatter-gather，无需复制数据

if (dev->features & NETIF_F_IP_CSUM)
    // 硬件计算校验和，无需软件计算

if (dev->features & NETIF_F_TSO)
    // 硬件做 TCP 分段，可以发送大于 MTU 的包
```

---

## 7. 对比思考

### 如果不使用 net_device_ops

```c
// 假设协议栈直接调用硬件函数

void ip_finish_output(struct sk_buff *skb)
{
    // 问题1: 需要知道设备类型
    if (is_e1000(skb->dev)) {
        e1000_xmit(skb);
    } else if (is_rtl8169(skb->dev)) {
        rtl8169_xmit(skb);
    } else if (is_virtio(skb->dev)) {
        virtio_xmit(skb);
    }
    // 问题2: 每添加新网卡都要修改协议栈代码
    // 问题3: 无法支持虚拟设备层叠
}
```

---

## 8. 相关 API

### 网络设备注册

```c
// 分配网络设备
struct net_device *alloc_netdev(int sizeof_priv, const char *name,
                                void (*setup)(struct net_device *));

// 以太网设备快捷分配
#define alloc_etherdev(sizeof_priv) \
    alloc_netdev(sizeof_priv, "eth%d", ether_setup)

// 注册网络设备
int register_netdev(struct net_device *dev);

// 注销网络设备
void unregister_netdev(struct net_device *dev);

// 释放网络设备
void free_netdev(struct net_device *dev);
```

### 发送控制

```c
// 停止发送队列
void netif_stop_queue(struct net_device *dev);

// 启动发送队列
void netif_start_queue(struct net_device *dev);

// 唤醒发送队列
void netif_wake_queue(struct net_device *dev);

// 检查发送队列是否停止
int netif_queue_stopped(const struct net_device *dev);
```

### 接收路径

```c
// 普通接收
int netif_rx(struct sk_buff *skb);

// NAPI 接收
int napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb);

// GRO (Generic Receive Offload)
gro_result_t napi_gro_receive(struct napi_struct *napi, struct sk_buff *skb);
```

---

## 🤔 思考题

1. **bonding 设备如何保证数据包不乱序？**
   - 提示: 查看 bonding 的不同模式 (ROUNDROBIN vs XOR)

2. **如果网卡不支持硬件校验和，协议栈如何处理？**
   - 提示: 查看 `features` 标志和软件校验和计算

3. **虚拟网卡 (如 veth) 的 xmit 如何实现？数据包去哪了？**
   - 提示: 查看 `drivers/net/veth.c`

4. **NAPI 机制如何与 netdev_ops 配合？**
   - 提示: 查看 `ndo_poll` 和中断处理

---

## 📚 相关源码文件

| 文件 | 行数 | 内容 |
|------|------|------|
| `include/linux/netdevice.h` | 1-2700 | net_device, net_device_ops 定义 |
| `net/core/dev.c` | 1-6500 | 网络设备核心 |
| `drivers/net/ethernet/intel/e1000/` | - | Intel e1000 驱动 |
| `drivers/net/loopback.c` | 1-200 | 回环设备 |
| `drivers/net/bonding/` | - | 链路聚合 |
| `drivers/net/tun.c` | - | TUN/TAP 虚拟网卡 |

