# 网络子系统 net_device_ops 中的依赖注入 (IoC) 模式

## 概述

Linux 网络子系统使用 `net_device_ops` 结构实现网络设备驱动与协议栈的解耦。协议栈通过统一的接口调用设备驱动，而不关心具体的硬件实现。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      网络子系统 net_device_ops IoC 架构                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   用户空间                                                                   │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │  应用程序: socket(), sendto(), recvfrom(), ioctl()                │     │
│   └───────────────────────────────────────────────────────────────────┘     │
│                                    │                                         │
│  ══════════════════════════════════╪════════════════════════════════════    │
│                                    │  系统调用                              │
│                                    ▼                                         │
│   ┌───────────────────────────────────────────────────────────────────┐     │
│   │                        协议栈 (TCP/IP/UDP)                        │     │
│   │                                                                    │     │
│   │   发送数据包:                                                      │     │
│   │   dev_queue_xmit(skb)                                             │     │
│   │       └──► netdev_start_xmit(skb, dev)                            │     │
│   │               └──► dev->netdev_ops->ndo_start_xmit(skb, dev)      │     │
│   │                                         ▲                          │     │
│   │                                         │ 调用注入的发送函数       │     │
│   └─────────────────────────────────────────┼─────────────────────────┘     │
│                                             │                                │
│           ┌─────────────────────────────────┼─────────────────────────┐     │
│           │                                 │                         │      │
│           ▼                                 ▼                         ▼      │
│   ┌───────────────┐               ┌───────────────┐           ┌───────────┐ │
│   │ Intel e1000   │               │  Realtek r8169│           │  virtio   │ │
│   │               │               │               │           │  _net     │ │
│   │.ndo_start_xmit│               │.ndo_start_xmit│           │.ndo_start │ │
│   │  = e1000_xmit │               │  = rtl_xmit   │           │ = virtio_ │ │
│   │               │               │               │           │   xmit    │ │
│   │.ndo_open      │               │.ndo_open      │           │.ndo_open  │ │
│   │  = e1000_open │               │  = rtl_open   │           │           │ │
│   └───────────────┘               └───────────────┘           └───────────┘ │
│          │                               │                         │         │
│          ▼                               ▼                         ▼         │
│   ┌───────────────┐               ┌───────────────┐           ┌───────────┐ │
│   │  Intel 网卡   │               │  Realtek 网卡 │           │ 虚拟网卡  │ │
│   │   硬件        │               │     硬件      │           │ (QEMU)    │ │
│   └───────────────┘               └───────────────┘           └───────────┘ │
│                                                                              │
│   控制反转体现:                                                              │
│   - 协议栈不知道如何操作具体硬件                                             │
│   - 网卡驱动通过 netdev_ops 注入自己的实现                                   │
│   - 协议栈通过统一接口调用，实现多态                                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 核心代码片段

### 1. net_device_ops 结构 - 网络设备操作的"接口契约"

```c
// include/linux/netdevice.h

struct net_device_ops {
    // 设备初始化/清理
    int         (*ndo_init)(struct net_device *dev);
    void        (*ndo_uninit)(struct net_device *dev);
    
    // 设备打开/关闭 (ifconfig up/down)
    int         (*ndo_open)(struct net_device *dev);
    int         (*ndo_stop)(struct net_device *dev);
    
    // 发送数据包 (最重要的函数!)
    netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb,
                                   struct net_device *dev);
    
    // 多队列选择
    u16         (*ndo_select_queue)(struct net_device *dev,
                                    struct sk_buff *skb);
    
    // 接收模式设置 (混杂模式、多播等)
    void        (*ndo_change_rx_flags)(struct net_device *dev, int flags);
    void        (*ndo_set_rx_mode)(struct net_device *dev);
    
    // MAC 地址操作
    int         (*ndo_set_mac_address)(struct net_device *dev, void *addr);
    int         (*ndo_validate_addr)(struct net_device *dev);
    
    // ioctl 处理
    int         (*ndo_do_ioctl)(struct net_device *dev,
                                struct ifreq *ifr, int cmd);
    
    // 配置接口
    int         (*ndo_set_config)(struct net_device *dev, struct ifmap *map);
    
    // MTU 修改
    int         (*ndo_change_mtu)(struct net_device *dev, int new_mtu);
    
    // 发送超时处理
    void        (*ndo_tx_timeout)(struct net_device *dev);
    
    // 统计信息
    struct rtnl_link_stats64* (*ndo_get_stats64)(struct net_device *dev,
                                struct rtnl_link_stats64 *storage);
    struct net_device_stats* (*ndo_get_stats)(struct net_device *dev);
    
    // VLAN 支持
    void        (*ndo_vlan_rx_add_vid)(struct net_device *dev,
                                       unsigned short vid);
    void        (*ndo_vlan_rx_kill_vid)(struct net_device *dev,
                                        unsigned short vid);
    
#ifdef CONFIG_NET_POLL_CONTROLLER
    // 网络轮询 (用于 netconsole)
    void        (*ndo_poll_controller)(struct net_device *dev);
#endif
    
    // SR-IOV 虚拟化支持
    int         (*ndo_set_vf_mac)(struct net_device *dev, int vf, u8 *mac);
    int         (*ndo_set_vf_vlan)(struct net_device *dev, int vf, 
                                   u16 vlan, u8 qos);
    int         (*ndo_set_vf_tx_rate)(struct net_device *dev, int vf, int rate);
    int         (*ndo_get_vf_config)(struct net_device *dev, int vf,
                                     struct ifla_vf_info *ivf);
    
    // Traffic Control (流量控制)
    int         (*ndo_setup_tc)(struct net_device *dev, u8 tc);
    
    // FCoE 支持
    int         (*ndo_fcoe_enable)(struct net_device *dev);
    int         (*ndo_fcoe_disable)(struct net_device *dev);
    
    // 主从设备 (bonding, bridge)
    int         (*ndo_add_slave)(struct net_device *dev,
                                 struct net_device *slave_dev);
    int         (*ndo_del_slave)(struct net_device *dev,
                                 struct net_device *slave_dev);
    
    // 硬件特性调整
    u32         (*ndo_fix_features)(struct net_device *dev, u32 features);
    int         (*ndo_set_features)(struct net_device *dev, u32 features);
};
```

---

### 2. 协议栈如何调用注入的 ops

```c
// net/core/dev.c

// 发送数据包入口
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
        // 有队列规则，入队列
        rc = __dev_xmit_skb(skb, q, dev, txq);
    } else {
        // 无队列，直接发送
        if (dev->flags & IFF_UP) {
            // 控制反转: 调用驱动注入的发送函数
            rc = dev_hard_start_xmit(skb, dev, txq);
        }
    }
    
    return rc;
}

// 实际调用驱动发送函数
static inline netdev_tx_t netdev_start_xmit(struct sk_buff *skb, 
                                            struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    
    // 调用驱动注入的 ndo_start_xmit
    return ops->ndo_start_xmit(skb, dev);
}
```

---

### 3. 设备打开/关闭

```c
// net/core/dev.c

int dev_open(struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;
    int ret;

    // 设备已经打开
    if (dev->flags & IFF_UP)
        return 0;

    // 调用驱动注入的 ndo_open
    if (ops->ndo_open)
        ret = ops->ndo_open(dev);
    else
        ret = 0;

    if (ret == 0) {
        dev->flags |= IFF_UP;
        // 初始化接收队列等
        dev_set_rx_mode(dev);
        dev_activate(dev);
    }

    return ret;
}

int dev_close(struct net_device *dev)
{
    const struct net_device_ops *ops = dev->netdev_ops;

    // 清理发送队列
    dev_deactivate(dev);

    dev->flags &= ~IFF_UP;

    // 调用驱动注入的 ndo_stop
    if (ops->ndo_stop)
        ops->ndo_stop(dev);

    return 0;
}
```

---

### 4. e1000 网卡驱动 - 注入实现示例

```c
// drivers/net/ethernet/intel/e1000/e1000_main.c

// 发送数据包实现
static netdev_tx_t e1000_xmit_frame(struct sk_buff *skb,
                                    struct net_device *netdev)
{
    struct e1000_adapter *adapter = netdev_priv(netdev);
    struct e1000_tx_ring *tx_ring = adapter->tx_ring;
    unsigned int max_txd_pwr = 12;
    unsigned int tx_flags = 0;
    unsigned int len;
    
    // 检查队列空间
    if (unlikely(e1000_maybe_stop_tx(netdev, tx_ring, count + 2))) {
        return NETDEV_TX_BUSY;
    }
    
    // 设置描述符
    len = skb->len;
    tx_flags |= E1000_TX_FLAGS_TSO;
    
    // 将 skb 数据映射到 DMA
    e1000_tx_map(adapter, tx_ring, skb, first, max_per_txd,
                 nr_frags, mss);
    
    // 触发硬件发送
    e1000_tx_queue(adapter, tx_ring, tx_flags, count);
    
    return NETDEV_TX_OK;
}

// 打开网卡
static int e1000_open(struct net_device *netdev)
{
    struct e1000_adapter *adapter = netdev_priv(netdev);
    int err;
    
    // 分配 DMA 缓冲区
    err = e1000_setup_all_tx_resources(adapter);
    if (err) goto err_setup_tx;
    
    err = e1000_setup_all_rx_resources(adapter);
    if (err) goto err_setup_rx;
    
    // 启动硬件
    e1000_power_up_phy(adapter);
    e1000_configure(adapter);
    
    // 注册中断
    err = e1000_request_irq(adapter);
    if (err) goto err_req_irq;
    
    // 启动接收
    e1000_irq_enable(adapter);
    netif_start_queue(netdev);
    
    return 0;
}

// 关闭网卡
static int e1000_close(struct net_device *netdev)
{
    struct e1000_adapter *adapter = netdev_priv(netdev);

    netif_stop_queue(netdev);
    e1000_irq_disable(adapter);
    
    // 释放中断
    e1000_free_irq(adapter);
    
    // 释放 DMA 缓冲区
    e1000_free_all_tx_resources(adapter);
    e1000_free_all_rx_resources(adapter);
    
    // 关闭硬件
    e1000_power_down_phy(adapter);
    
    return 0;
}

// 注入 net_device_ops
static const struct net_device_ops e1000_netdev_ops = {
    .ndo_open               = e1000_open,           // 注入: 打开
    .ndo_stop               = e1000_close,          // 注入: 关闭
    .ndo_start_xmit         = e1000_xmit_frame,     // 注入: 发送
    .ndo_get_stats          = e1000_get_stats,      // 注入: 统计
    .ndo_set_rx_mode        = e1000_set_rx_mode,    // 注入: 接收模式
    .ndo_set_mac_address    = e1000_set_mac,        // 注入: MAC 地址
    .ndo_tx_timeout         = e1000_tx_timeout,     // 注入: 超时处理
    .ndo_change_mtu         = e1000_change_mtu,     // 注入: MTU 修改
    .ndo_do_ioctl           = e1000_ioctl,          // 注入: ioctl
    .ndo_validate_addr      = eth_validate_addr,    // 复用通用实现
    .ndo_vlan_rx_add_vid    = e1000_vlan_rx_add_vid,
    .ndo_vlan_rx_kill_vid   = e1000_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
    .ndo_poll_controller    = e1000_netpoll,
#endif
    .ndo_fix_features       = e1000_fix_features,
    .ndo_set_features       = e1000_set_features,
};

// 驱动初始化时绑定 ops
static int e1000_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct net_device *netdev;
    
    // 分配 net_device
    netdev = alloc_etherdev(sizeof(struct e1000_adapter));
    
    // 绑定 ops (依赖注入)
    netdev->netdev_ops = &e1000_netdev_ops;
    
    // 注册网络设备
    register_netdev(netdev);
    
    return 0;
}
```

---

### 5. 虚拟网卡 (loopback) - 简单示例

```c
// drivers/net/loopback.c

// 回环发送: 直接将数据包返回给接收路径
static netdev_tx_t loopback_xmit(struct sk_buff *skb,
                                 struct net_device *dev)
{
    // 更新统计
    u64_stats_update_begin(&lb_stats->syncp);
    lb_stats->packets++;
    lb_stats->bytes += len;
    u64_stats_update_end(&lb_stats->syncp);

    // 调整 skb，准备接收
    skb_orphan(skb);
    skb->protocol = eth_type_trans(skb, dev);

    // 直接送入接收路径
    netif_rx(skb);

    return NETDEV_TX_OK;
}

// 回环设备不需要真正的 open/stop
static int loopback_open(struct net_device *dev)
{
    return 0;
}

// 注入 net_device_ops
static const struct net_device_ops loopback_ops = {
    .ndo_init        = loopback_dev_init,
    .ndo_open        = loopback_open,
    .ndo_start_xmit  = loopback_xmit,       // 回环发送
    .ndo_get_stats64 = loopback_get_stats64,
};

// 创建回环设备
static void loopback_setup(struct net_device *dev)
{
    dev->netdev_ops = &loopback_ops;
    // ...
}
```

---

## 这样做的好处

### 1. 协议栈与硬件解耦

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  协议栈完全不知道底层硬件                                                    │
│                                                                              │
│  TCP/IP ──► dev->netdev_ops->ndo_start_xmit() ──► ???                       │
│                                                                              │
│  ???可以是:                                                                  │
│  - 真实网卡 (e1000, rtl8169, ...)                                           │
│  - 虚拟网卡 (virtio, veth, tun/tap)                                         │
│  - 无线网卡 (iwl, ath9k)                                                    │
│  - 回环设备 (lo)                                                             │
│  - 桥接设备 (bridge)                                                         │
│  - 容器虚拟网络 (veth)                                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2. 热插拔支持

- 网卡可以在运行时添加/移除
- 驱动可以动态加载/卸载
- 虚拟网卡可以动态创建/销毁

### 3. 统一的管理接口

```bash
# 所有网卡都使用相同的命令管理
ip link show
ip link set eth0 up
ip addr add 192.168.1.1/24 dev eth0
ethtool eth0

# 底层调用的都是 netdev_ops 中的函数
```

### 4. 硬件特性抽象

```c
// 协议栈根据硬件能力调整行为
if (dev->features & NETIF_F_SG)
    // 支持 scatter-gather，可以零拷贝发送
    
if (dev->features & NETIF_F_IP_CSUM)
    // 硬件支持校验和卸载
    
if (dev->features & NETIF_F_TSO)
    // 硬件支持 TCP 分段卸载
```

### 5. 中间层设备

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  bonding (链路聚合) 示例                                                     │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────┐     │
│  │                         bond0                                       │     │
│  │                    netdev_ops = &bond_ops                          │     │
│  │                                                                     │     │
│  │   bond_xmit() {                                                    │     │
│  │       // 选择一个 slave 设备                                        │     │
│  │       slave = bond_select_slave(bond);                             │     │
│  │       // 调用 slave 的 ndo_start_xmit                              │     │
│  │       slave->dev->netdev_ops->ndo_start_xmit(skb, slave->dev);    │     │
│  │   }                                                                 │     │
│  │                                                                     │     │
│  └───────────────────────┬────────────────────┬───────────────────────┘     │
│                          │                    │                              │
│                          ▼                    ▼                              │
│                    ┌──────────┐          ┌──────────┐                       │
│                    │   eth0   │          │   eth1   │                       │
│                    │ (slave)  │          │ (slave)  │                       │
│                    └──────────┘          └──────────┘                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## ethtool_ops - 另一个 ops 注入点

```c
// include/linux/ethtool.h

struct ethtool_ops {
    // 获取驱动信息
    void (*get_drvinfo)(struct net_device *, struct ethtool_drvinfo *);
    
    // 获取/设置链路速度
    int (*get_settings)(struct net_device *, struct ethtool_cmd *);
    int (*set_settings)(struct net_device *, struct ethtool_cmd *);
    
    // 重启自协商
    int (*nway_reset)(struct net_device *);
    
    // 获取链路状态
    u32 (*get_link)(struct net_device *);
    
    // 获取/设置寄存器
    int (*get_regs_len)(struct net_device *);
    void (*get_regs)(struct net_device *, struct ethtool_regs *, void *);
    
    // 唤醒 (Wake-on-LAN)
    void (*get_wol)(struct net_device *, struct ethtool_wolinfo *);
    int (*set_wol)(struct net_device *, struct ethtool_wolinfo *);
    
    // ... 更多操作
};
```

---

## 核心源码文件

| 文件 | 功能 |
|------|------|
| `include/linux/netdevice.h` | net_device_ops 定义 |
| `net/core/dev.c` | 网络设备核心，发送/接收路径 |
| `drivers/net/ethernet/intel/e1000/` | Intel e1000 驱动 |
| `drivers/net/loopback.c` | 回环设备实现 |
| `drivers/net/bonding/` | 链路聚合驱动 |
| `drivers/net/tun.c` | TUN/TAP 虚拟网卡 |

---

## 总结

网络子系统 net_device_ops 的 IoC 模式:

1. **接口契约**: `net_device_ops` 定义所有网络设备必须/可选实现的操作
2. **依赖注入**: 网卡驱动在 probe 时设置 `dev->netdev_ops`
3. **多态调用**: 协议栈通过 `ops->ndo_xxx()` 调用，不关心具体硬件
4. **分层抽象**: 支持虚拟设备、中间层设备 (bonding, bridge, vlan)
5. **运行时绑定**: 设备可以动态创建/销毁，ops 可以替换

