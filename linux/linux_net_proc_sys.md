# 从 Device Model 心智模型理解 `/proc/net` 与 `/sys/class/net`
## —— Linux Network Interface 的两种“观察视角”

---

# 1. 先建立整体心智模型

理解 `/proc/net` 和 `/sys/class/net` 最重要的是：

它们并不是两套独立的数据。

它们只是：

> **从不同角度观察同一个内核对象 `struct net_device`。**

---

## 两种视角

```
                    ┌────────────────────────┐
                    │   struct net_device    │
                    │    (网络接口对象)        │
                    └────────────┬───────────┘
                                 │
             ┌───────────────────┴──────────────────┐
             │                                      │
             │                                      │
             ▼                                      ▼

    /proc/net/*                               /sys/class/net/*
  "网络协议栈视角"                           "Device Model视角"

显示运行状态、统计信息                        显示设备关系、属性
与协议栈工作密切相关                          与驱动、总线、设备树相关

谁在发包？                                    这个接口来自哪个设备？
谁有多少流量？                                属于哪个PCI设备？
路由如何？                                    用的什么驱动？
ARP表是什么？                                 MAC地址是多少？

```

---

# 2. Linux 网络接口真正是什么？

很多人误以为：

```
网卡 = network interface
```

实际上：

```
PCI 网卡
    ↓
   驱动
    ↓
struct net_device
    ↓
eth0
```

真正被协议栈使用的是：

```
struct net_device
```

---

## Device Model 中的位置

```
PCI Device
    ↓
Device Driver
    ↓
register_netdev()

        创建

             struct net_device
                     │
         ┌───────────┴────────────┐
         │                        │
         ▼                        ▼

  /sys/class/net/eth0      /proc/net/dev
```

---

# 3. Device Model 中的层次

---

## sysfs 的组织

例如：

```
/sys/class/net/eth0
```

实际上：

```
/sys/class/net/eth0
        ↓

/sys/devices/pci0000:00/.../0000:03:00.0/net/eth0
```

只是一个符号链接：

```
$ ls -l /sys/class/net/eth0

eth0 ->
../../devices/pci0000:00/0000:00:1c.0/0000:03:00.0/net/eth0
```

---

## Device Model 图

```
sysfs Device Model

/sys
│
├─ devices/
│
│    pci0000:00/
│         │
│         └─ 0000:03:00.0
│                │
│                ├─ driver
│                │
│                └─ net/
│                     │
│                     └─ eth0
│
└─ class/
      │
      └─ net/
            │
            └─ eth0
                  ↓
            symbolic link
```

---

## Device Model 的思想

```
devices/
    表示“设备在哪里”

class/
    表示“设备是什么”
```

对于网卡：

```
devices/
    PCI 总线上的真实设备

class/net/
    Network Interface 类别
```

---

# 4. net_device 在内核中的位置

协议栈真正看到的是：

```
struct net_device
```

---

## 内核关系图

```
                    ┌─────────────┐
                    │ PCI Device  │
                    └──────┬──────┘
                           │
                           ▼

                    ┌─────────────┐
                    │ Driver      │
                    │ e1000e      │
                    └──────┬──────┘
                           │

                 alloc_netdev()

                           │
                           ▼

                 ┌─────────────────┐
                 │ struct          │
                 │ net_device      │
                 └────────┬────────┘
                          │
      ┌───────────────────┼──────────────────┐
      │                   │                  │
      ▼                   ▼                  ▼

MAC address        statistics         qdisc queues

      │                   │                  │
      │                   │                  │
      ▼                   ▼                  ▼

/sys/class/net     /proc/net/dev      tc qdisc
```

---

# 5. struct net_device 内部结构详解

---

## 为什么要深入理解 struct net_device？

理解这个结构体是调试网络问题的关键：

```
/proc/net/* 和 /sys/class/net/* 中的每个数值
都直接来源于 struct net_device 的字段
```

---

## 核心结构体定义

```c
struct net_device {
    /* Device identity */
    char                name[IFNAMSIZ];     // "eth0"
    int                 ifindex;            // 接口索引

    /* Hardware info */
    unsigned char       dev_addr[MAX_ADDR_LEN];  // MAC 地址
    unsigned short      type;               // 设备类型 (ARPHRD_ETHER)
    unsigned int        mtu;                // MTU

    /* State */
    unsigned long       state;              // __LINK_STATE_* flags
    unsigned int        flags;              // IFF_* flags
    unsigned char       operstate;          // RFC2863 operational state

    /* Statistics */
    struct net_device_stats stats;         // 统计信息

    /* Device operations */
    const struct net_device_ops *netdev_ops;

    /* Queue management */
    struct netdev_queue *_tx;               // TX queues
    unsigned int        num_tx_queues;
    unsigned int        real_num_tx_queues;

    /* Device model integration */
    struct device       dev;                // 嵌入的 device 结构

    /* Network namespace */
    struct net          *nd_net;            // 所属网络命名空间

    /* Driver private data */
    void               *priv;               // 驱动私有数据
};
```

---

## 字段与文件系统映射关系

### `/sys/class/net/eth0/*` 映射

```c
// /sys/class/net/eth0/address
net_device->dev_addr[]

// /sys/class/net/eth0/mtu
net_device->mtu

// /sys/class/net/eth0/ifindex
net_device->ifindex

// /sys/class/net/eth0/operstate
net_device->operstate

// /sys/class/net/eth0/flags
net_device->flags

// /sys/class/net/eth0/type
net_device->type

// /sys/class/net/eth0/carrier
net_device->state & __LINK_STATE_NOCARRIER
```

### `/proc/net/dev` 映射

```c
// Interface: 列
net_device->name

// RX bytes:
net_device->stats.rx_bytes

// RX packets:
net_device->stats.rx_packets

// RX errors:
net_device->stats.rx_errors + rx_dropped + rx_crc_errors + ...

// TX bytes:
net_device->stats.tx_bytes

// TX packets:
net_device->stats.tx_packets

// TX errors:
net_device->stats.tx_errors + tx_dropped + tx_fifo_errors + ...
```

---

## Device Model 集成机制

### 嵌入式设计

```c
struct net_device {
    // ...
    struct device dev;    // 嵌入的 device 结构
    // ...
};
```

这种设计让 `net_device` 自动成为 Linux Device Model 的一部分：

```
PCI Device
    ↓
device_register()
    ↓
struct device (in struct net_device)
    ↓
/sys/devices/pci.../net/eth0
    ↓
/sys/class/net/eth0 (symlink)
```

### 注册过程

```c
// 1. 分配 net_device
struct net_device *dev = alloc_netdev(sizeof(struct private_data),
                                      "eth%d", NET_NAME_UNKNOWN,
                                      ether_setup);

// 2. 设置设备信息
dev->netdev_ops = &my_netdev_ops;
memcpy(dev->dev_addr, mac_addr, ETH_ALEN);
dev->mtu = 1500;

// 3. 注册到内核
register_netdev(dev);
    ↓
// 内部会调用
device_add(&dev->dev);  // 创建 /sys 条目
```

---

## 生命周期与调试

### 设备状态机

```
UNINITIALIZED
    ↓ alloc_netdev()
ALLOCATED
    ↓ register_netdev()
REGISTERED
    ↓ dev_open()
UP
    ↓ dev_close()
DOWN
    ↓ unregister_netdev()
UNREGISTERED
```

### 关键状态标志

```c
// net_device->state 位标志
__LINK_STATE_START      // 设备已启动
__LINK_STATE_PRESENT    // 设备存在
__LINK_STATE_NOCARRIER  // 无载波信号
__LINK_STATE_LINKWATCH_PENDING // 链路监控待处理
__LINK_STATE_DORMANT    // 设备休眠

// net_device->flags 标志 (对应 ifconfig flags)
IFF_UP          // 接口已启用
IFF_BROADCAST   // 支持广播
IFF_MULTICAST   // 支持多播
IFF_RUNNING     // 资源已分配
IFF_PROMISC     // 混杂模式
```

---

## 统计信息详解

### net_device_stats 结构

```c
struct net_device_stats {
    unsigned long rx_packets;        // 接收包数
    unsigned long tx_packets;        // 发送包数
    unsigned long rx_bytes;          // 接收字节数
    unsigned long tx_bytes;          // 发送字节数
    unsigned long rx_errors;         // 接收错误数
    unsigned long tx_errors;         // 发送错误数
    unsigned long rx_dropped;        // 接收丢包数
    unsigned long tx_dropped;        // 发送丢包数

    // 更细粒度的错误统计
    unsigned long rx_length_errors;   // 长度错误
    unsigned long rx_over_errors;     // 接收溢出
    unsigned long rx_crc_errors;      // CRC错误
    unsigned long rx_frame_errors;    // 帧错误
    unsigned long rx_fifo_errors;     // FIFO错误
    unsigned long rx_missed_errors;   // 遗漏错误

    unsigned long tx_aborted_errors;  // 发送中止
    unsigned long tx_carrier_errors;  // 载波错误
    unsigned long tx_fifo_errors;     // 发送FIFO错误
    unsigned long tx_heartbeat_errors;// 心跳错误
    unsigned long tx_window_errors;   // 窗口错误

    unsigned long rx_compressed;      // 压缩包统计
    unsigned long tx_compressed;
    unsigned long multicast;          // 多播包数
    unsigned long collisions;         // 冲突次数
};
```

---

## 调试实用技巧

### 1. 快速检查设备状态

```bash
# 检查设备是否在内核中注册
ls /sys/class/net/

# 检查设备基本信息
cat /sys/class/net/eth0/ifindex
cat /sys/class/net/eth0/operstate
cat /sys/class/net/eth0/carrier

# 检查统计信息
cat /proc/net/dev
```

### 2. 追踪数据路径

```bash
# 检查包计数器变化
watch -d 'cat /proc/net/dev'

# 检查队列状态
ls /sys/class/net/eth0/queues/
cat /sys/class/net/eth0/queues/tx-0/tx_timeout
```

### 3. 设备模型关系

```bash
# 找到真实的PCI设备
readlink /sys/class/net/eth0/device

# 查看驱动信息
readlink /sys/class/net/eth0/device/driver
cat /sys/class/net/eth0/device/driver/module/version

# 查看PCI信息
lspci -vvv -s $(basename $(readlink /sys/class/net/eth0/device))
```

### 4. 网络命名空间隔离

```bash
# 在特定命名空间中查看设备
ip netns exec myns cat /proc/net/dev

# 设备在哪个命名空间
readlink /sys/class/net/eth0/net_ns
```

---

## 内存布局示意

```
struct net_device 内存布局:

+------------------------+  <- alloc_netdev() 返回地址
|     net_device         |
|   +--------------+     |
|   | char name[]  |     | -> /sys/class/net/eth0/
|   | int ifindex  |     |
|   | u8 dev_addr[]|     | -> /sys/class/net/eth0/address
|   | uint mtu     |     | -> /sys/class/net/eth0/mtu
|   | ulong state  |     | -> /sys/class/net/eth0/operstate
|   | uint flags   |     | -> /sys/class/net/eth0/flags
|   +--------------+     |
|   | device dev   |     | -> Device Model 集成点
|   +--------------+     |
|   |stats (struct |     | -> /proc/net/dev 数据源
|   | net_device_  |     |
|   | stats)       |     |
|   +--------------+     |
+------------------------+
|   Driver Private Data  |  <- netdev_priv() 返回
|   (e.g. e1000_adapter) |
+------------------------+
```

---

# 6. `/sys/class/net`

---

它回答的是：

> **这个 network interface 是谁？**

---

例如：

```
/sys/class/net/eth0
```

包含：

```
address
carrier
dev_id
dev_port
duplex
flags
ifalias
ifindex
iflink
mtu
operstate
queues
speed
statistics
type
```

---

## ASCII Diagram

```
/sys/class/net/eth0
│
├─ address
│      MAC 地址
│
├─ mtu
│      MTU
│
├─ operstate
│      up/down
│
├─ carrier
│      是否检测到链路
│
├─ speed
│      链路速率
│
├─ statistics/
│      RX/TX counters
│
├─ queues/
│      TX/RX queue
│
└─ device
       ↓
    PCI Device
```

---

## 查看驱动

```
eth0
 │
 ▼

device
 │
 ▼

driver
```

例如：

```
readlink /sys/class/net/eth0/device/driver
```

输出：

```
.../drivers/ixgbe
```

---

## 查看对应 PCI 设备

```
readlink /sys/class/net/eth0/device
```

例如：

```
../../../0000:03:00.0
```

---

# 6. `/proc/net`

---

它回答的是：

> **这个 network interface 正在做什么？**

---

---

# /proc/net/dev

```
Inter-| Receive | Transmit
 face  bytes packets ...
 eth0 ...
 lo ...
```

来源：

```
net/core/dev.c
dev_seq_show()
```

遍历：

```
for_each_netdev()
```

输出：

```
net_device stats
```

---

ASCII：

```
net namespace
     │
     ▼

net_device list

     │
     ▼

eth0
eth1
lo

     │
     ▼

rx packets
tx packets
rx errors
tx errors

     │
     ▼

/proc/net/dev
```

---

# /proc/net/route

回答：

```
流量应该走哪个 interface？
```

例如：

```
Iface Destination Gateway
eth0 ...
```

关系：

```
routing table

        │
        ▼

fib_info

        │
        ▼

net_device
```

---

ASCII：

```
Destination IP

      │
      ▼

Routing Table

      │
      ▼

Next Hop

      │
      ▼

net_device

      │
      ▼

eth0
```

---

# /proc/net/arp

回答：

```
IP 地址对应哪个 MAC？
```

例如：

```
IP address       HW address
192.168.1.1      xx:xx:xx
```

关系：

```
neighbor table

       │
       ▼

struct neighbour

       │
       ▼

net_device
```

---

ASCII：

```
IP Address

     │
     ▼

ARP Cache

     │
     ▼

neighbour

     │
     ▼

net_device

     │
     ▼

eth0
```

---

# 7. 两者真正区别

---

## sysfs

关注：

```
Who am I ?
```

```
我是哪个设备？

我的驱动是什么？

我属于哪个PCI设备？

我的属性是什么？
```

---

## procfs

关注：

```
What am I doing ?
```

```
发了多少包？

收了多少包？

路由如何？

ARP表如何？

Socket有哪些？
```

---

# 8. 完整心智模型

```
                     HARDWARE

                  ┌──────────┐
                  │ PCI NIC  │
                  └────┬─────┘
                       │
                       ▼

                 Device Driver
                       │
                       ▼

               struct net_device
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        │              │              │
        ▼              ▼              ▼

  Device Model     Protocol Stack   Traffic Control

        │              │              │
        │              │              │
        ▼              ▼              ▼

/sys/class/net   /proc/net/*        tc

Who am I ?       What am I doing?   How packets are queued?

```

---

# 9. 具体示例：eth0

查看：

```
ip link show eth0
```

得到：

```
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP>
```

---

查看 Device Model：

```
readlink /sys/class/net/eth0/device
```

得到：

```
../../../0000:03:00.0
```

说明：

```
eth0
 ↓
PCI Device 0000:03:00.0
```

---

查看驱动：

```
readlink /sys/class/net/eth0/device/driver
```

得到：

```
ixgbe
```

说明：

```
eth0
 ↓
ixgbe driver
```

---

查看协议栈统计：

```
cat /proc/net/dev
```

得到：

```
eth0:
    RX bytes
    RX packets
    TX bytes
    TX packets
```

说明：

```
协议栈正在通过 eth0 收发流量
```

---

查看路由：

```
cat /proc/net/route
```

得到：

```
default
    ↓
eth0
```

说明：

```
默认路由通过 eth0 发出
```

---

# 10. 最终总结

```
理解 Linux Network Interface 的关键：

不要把 eth0 看成“网卡”。

真正存在的是：

PCI Device
    ↓
Driver
    ↓
struct net_device
    ↓
eth0

然后：

/sys/class/net
    =
Device Model 对 net_device 的投影

/proc/net
    =
Network Stack 对 net_device 的投影
```

最终记忆口诀：

```
/sys/class/net
    看“身份”

/proc/net
    看“行为”

struct net_device
    才是真正的主角
```

```
PCI Device
      ↓
Driver
      ↓
struct net_device
      ↓
 ┌───────────────┬────────────────┐
 │               │                │
 ▼               ▼                ▼

/sys         /proc/net          tc

身份            行为           队列控制
Who am I?   What am I doing?  How packets flow?
```

---

# 11. 实战调试指南

基于 `struct net_device` 的理解，这里提供常见网络问题的调试思路。

---

## 问题1：接口无法收发包

### 调试路径

```bash
# 1. 确认 net_device 是否正确注册
ls /sys/class/net/eth0  # 应该存在

# 2. 检查 Device Model 层
readlink /sys/class/net/eth0/device  # 确认对应 PCI 设备
cat /sys/class/net/eth0/operstate    # 应该是 'up'
cat /sys/class/net/eth0/carrier      # 应该是 1

# 3. 检查驱动层
readlink /sys/class/net/eth0/device/driver  # 确认驱动加载
dmesg | grep -i eth0  # 查看驱动日志

# 4. 检查协议栈层
cat /proc/net/dev  # 查看是否有包计数变化
ip link show eth0  # 查看 IFF_UP, IFF_RUNNING 标志
```

### struct net_device 字段诊断

```c
// 对应上述检查的内核字段:
net_device->name           // eth0 存在
net_device->dev            // PCI 设备关联
net_device->operstate      // RFC2863 状态
net_device->state & __LINK_STATE_NOCARRIER  // 载波检测
net_device->netdev_ops     // 驱动操作函数
net_device->flags & IFF_UP // 接口启用状态
net_device->stats.*        // 包统计
```

---

## 问题2：丢包问题分析

### 分层诊断

```bash
# 1. 硬件层丢包 (net_device->stats 层面)
cat /sys/class/net/eth0/statistics/rx_dropped  # 驱动层丢包
cat /sys/class/net/eth0/statistics/rx_errors   # 硬件错误
cat /sys/class/net/eth0/statistics/rx_fifo_errors  # FIFO 溢出

# 2. 协议栈丢包
cat /proc/net/snmp | grep -i ip  # IP 层统计
cat /proc/net/netstat             # 详细网络统计

# 3. 应用层丢包
ss -s  # socket 统计
```

### 对应 struct net_device 字段

```c
// RX 路径丢包分析:
net_device->stats.rx_dropped      // 驱动决定丢弃
net_device->stats.rx_errors       // 硬件报告错误
net_device->stats.rx_fifo_errors  // 接收 FIFO 满
net_device->stats.rx_missed_errors // 硬件遗漏

// TX 路径丢包分析:
net_device->stats.tx_dropped      // 传输队列满等
net_device->stats.tx_errors       // 传输错误
net_device->stats.tx_carrier_errors // 载波丢失
```

---

## 问题3：性能问题诊断

### 多队列检查

```bash
# 1. 检查队列数量
cat /sys/class/net/eth0/queues/rx-*/rps_cpus
ls /sys/class/net/eth0/queues/tx-*

# 2. 检查队列统计
cat /proc/interrupts | grep eth0
cat /sys/class/net/eth0/queues/tx-0/tx_timeout

# 3. 检查 CPU 亲和性
cat /proc/irq/*/smp_affinity_list
```

### struct net_device 队列字段

```c
// 多队列相关字段:
net_device->num_tx_queues         // TX 队列总数
net_device->real_num_tx_queues    // 实际使用的 TX 队列数
net_device->_tx                   // TX 队列数组
net_device->rx_queue              // RX 队列 (如果支持)
```

---

## 问题4：链路状态异常

### 状态机调试

```bash
# 1. 检查物理链路
cat /sys/class/net/eth0/carrier    # 载波检测
cat /sys/class/net/eth0/duplex     # 双工模式
cat /sys/class/net/eth0/speed      # 链路速度

# 2. 检查设备状态
ip link show eth0                  # 查看 state 字段
cat /sys/class/net/eth0/operstate  # RFC2863 状态

# 3. 查看状态变化历史
dmesg | grep -i "eth0.*link"      # 链路状态变化日志
```

### 状态字段解读

```c
// 链路状态相关字段:
net_device->state & __LINK_STATE_NOCARRIER  // 无载波
net_device->state & __LINK_STATE_DORMANT    // 休眠状态
net_device->operstate                       // 操作状态
net_device->flags & IFF_RUNNING             // 运行状态
net_device->flags & IFF_LOWER_UP            // 底层链路正常
```

---

## 问题5：驱动问题排查

### Device Model 路径

```bash
# 1. 确认设备树
readlink -f /sys/class/net/eth0/device  # 真实设备路径
ls -la /sys/class/net/eth0/device/      # 设备属性

# 2. 检查驱动绑定
cat /sys/class/net/eth0/device/driver/module/version
cat /sys/class/net/eth0/device/uevent   # 设备事件

# 3. 检查资源分配
cat /sys/class/net/eth0/device/resource  # PCI 资源
lspci -vvv -s $(basename $(readlink /sys/class/net/eth0/device))
```

### 设备模型集成点

```c
// Device Model 集成:
net_device->dev.parent             // 父设备 (PCI device)
net_device->dev.driver             // 绑定的驱动
net_device->dev.kobj               // sysfs 对象
net_device->dev.release            // 释放函数
```

---

## 调试脚本示例

```bash
#!/bin/bash
# 网络接口完整健康检查脚本

IFACE=${1:-eth0}

echo "=== $IFACE 完整诊断 ==="

echo "1. Device Model 层:"
echo "  设备路径: $(readlink -f /sys/class/net/$IFACE/device 2>/dev/null || echo 'N/A')"
echo "  驱动: $(basename $(readlink /sys/class/net/$IFACE/device/driver 2>/dev/null) 2>/dev/null || echo 'N/A')"
echo "  PCI ID: $(basename $(readlink /sys/class/net/$IFACE/device 2>/dev/null) 2>/dev/null || echo 'N/A')"

echo "2. net_device 状态:"
echo "  ifindex: $(cat /sys/class/net/$IFACE/ifindex 2>/dev/null || echo 'N/A')"
echo "  operstate: $(cat /sys/class/net/$IFACE/operstate 2>/dev/null || echo 'N/A')"
echo "  carrier: $(cat /sys/class/net/$IFACE/carrier 2>/dev/null || echo 'N/A')"
echo "  mtu: $(cat /sys/class/net/$IFACE/mtu 2>/dev/null || echo 'N/A')"

echo "3. 统计信息:"
grep "$IFACE:" /proc/net/dev 2>/dev/null || echo "  接口未找到"

echo "4. 队列信息:"
echo "  TX queues: $(ls /sys/class/net/$IFACE/queues/tx-* 2>/dev/null | wc -l)"
echo "  RX queues: $(ls /sys/class/net/$IFACE/queues/rx-* 2>/dev/null | wc -l)"

echo "5. 链路状态:"
ip link show $IFACE 2>/dev/null | grep -o '<.*>' || echo "  接口不存在"
```

这个调试指南将理论知识与实际问题排查结合，基于对 `struct net_device` 的深入理解，提供了系统性的网络接口调试方法。
