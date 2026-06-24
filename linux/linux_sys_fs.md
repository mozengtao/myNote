# Linux `/sys` 文件系统与 Device Model 心智模型

## 目录

- [1. 引言：为什么要理解 Device Model？](#1-引言为什么要理解-device-model)
- [2. Linux Device Model 要解决的问题](#2-linux-device-model-要解决的问题)
- [3. Device Model 核心概念](#3-device-model-核心概念)
- [4. sysfs 文件系统结构](#4-sysfs-文件系统结构)
- [5. 实例分析：网卡设备](#5-实例分析网卡设备)
- [6. 调试与实际应用](#6-调试与实际应用)
- [7. 总结与心智模型](#7-总结与心智模型)

---

## 1. 引言：为什么要理解 Device Model？

### 1.1 传统的误区

很多人学习 `/sys` 时，从目录结构开始记忆：

```
/sys/
├── block/
├── bus/
├── class/
├── devices/
├── module/
└── ...
```

**结果导致的困惑：**
- 今天看到 `/sys/class/net/eth0`
- 明天看到 `/sys/bus/pci/devices/0000:03:00.0`
- 后天看到 `/sys/devices/pci0000:00/...`

完全不知道这些目录之间的关系。

### 1.2 正确的理解方式

**核心概念：**

> `/sys` 不是简单的目录树，而是 Linux Device Model 在用户空间的投影。

**层次关系：**

```
Linux Device Model (内核空间)
        |
        v
     kobject 框架
        |
        v
      sysfs 虚拟文件系统
        |
        v
      /sys 目录 (用户空间)
```

---

## 2. Linux Device Model 要解决的问题

### 2.1 早期 Linux 的问题

**简单但混乱的驱动模型：**

```
Driver <-----> Device
```

**存在的问题：**
- ❌ 缺少统一管理框架
- ❌ 热插拔支持困难
- ❌ 电源管理复杂
- ❌ 用户空间无法获取统一设备信息
- ❌ 设备与驱动关系难以追踪
- ❌ 缺少层次化的设备组织

### 2.2 Linux 2.6 Device Model 解决方案

**统一的设备管理框架：**

```
            +----------+
            |   Bus    |  <-- 设备发现机制
            +----------+
                 |
                 | discovers
                 v
            +----------+
            |  Device  |  <-- 实际硬件设备
            +----------+
                 |
                 | bound to
                 v
            +----------+
            |  Driver  |  <-- 设备驱动程序
            +----------+
                 |
                 | exposed as
                 v
            +----------+
            |  Class   |  <-- 用户空间功能接口
            +----------+
```

**要统一回答的核心问题：**
- 设备是什么？（Device）
- 挂在哪个总线上？（Bus）
- 由哪个驱动管理？（Driver）
- 向用户提供什么功能？（Class）

---

## 3. Device Model 核心概念

### 3.1 四个核心对象及其关系

**核心对象：**
- **Bus**：设备发现机制
- **Device**：实际的硬件或虚拟设备
- **Driver**：操作设备的软件逻辑
- **Class**：面向用户空间的功能分类

**对象关系图：**

```
                    Bus
                     |
                     | discovers
                     v
                  Device
                   /   \
                  /     \
                 v       v
             Driver     Class
```

### 3.2 Bus（总线）详解

**定义：** 设备发现和管理机制

**常见总线类型：**
- `PCI` - 外设互连总线
- `USB` - 通用串行总线
- `I2C` - 内部集成电路总线
- `SPI` - 串行外设接口
- `platform` - 平台设备总线
- `virtio` - 虚拟化设备总线

**Bus 职责：**
- 发现连接的设备
- 匹配合适的驱动
- 维护设备列表
- 管理设备生命周期

**示例结构：**

```
PCI Bus
  |
  +-- Device A (0000:01:00.0)
  |
  +-- Device B (0000:02:00.0)
  |
  +-- Device C (0000:03:00.0)
```

**sysfs 映射：**
```
/sys/bus/pci/        # PCI总线
/sys/bus/usb/        # USB总线
/sys/bus/platform/   # 平台总线
```

### 3.3 Device（设备）详解

**定义：** 实际存在的硬件或虚拟设备

**设备标识示例：**
- `0000:03:00.0` - PCI设备
- `usb1` - USB控制器
- `virtio0` - 虚拟设备
- `sda` - SCSI磁盘

**设备层次结构：**

```
CPU
 |
 +-- PCI Root Complex
     |
     +-- 0000:00:01.0 (PCI Bridge)
     |
     +-- 0000:00:02.0 (Graphics)
     |
     +-- 0000:03:00.0 (Network)
```

**sysfs 核心位置：**
```
/sys/devices/   # 设备树的根目录，所有真实设备对象
```

> **重要：** `/sys/devices` 是整个 Device Model 的核心，存储所有真实的设备对象。

### 3.4 Driver（驱动）详解

**定义：** 操作和管理设备的软件逻辑

**常见驱动示例：**
- `ixgbe` - Intel 10GbE 网卡驱动
- `e1000e` - Intel 千兆网卡驱动
- `virtio_net` - 虚拟网卡驱动
- `xhci_hcd` - USB 3.0 主机控制器驱动
- `nvme` - NVMe SSD 驱动

**Driver 职责：**
- 设备初始化和配置
- 命令发送和处理
- 中断处理
- 数据收发管理
- 电源管理

**设备-驱动绑定：**

```
Device (0000:03:00.0)
        |
        | bind/unbind
        v
Driver (ixgbe)
```

**sysfs 位置：**
```
/sys/bus/pci/drivers/ixgbe/   # PCI驱动
/sys/bus/usb/drivers/usbhid/  # USB驱动
```

### 3.5 Class（设备类）详解

**定义：** 面向用户空间的功能分类，而非硬件分类

**主要设备类：**
- `net` - 网络接口（eth0、wlan0等）
- `block` - 块设备（sda、nvme0n1等）
- `tty` - 终端设备
- `input` - 输入设备（键盘、鼠标）
- `sound` - 声音设备
- `graphics` - 显卡设备

**用户视角 vs 硬件视角：**

```
用户需要知道：     "这是网卡"    (net class)
而不是：          "这是PCI设备"  (bus type)
```

**功能抽象示例：**

```
用户空间应用
        |
        v
    eth0 (网络接口)
        |
        v
   net class (功能抽象)
```

**sysfs 位置：**
```
/sys/class/net/      # 网络设备
/sys/class/block/    # 块设备
/sys/class/tty/      # 终端设备
```

---

## 4. sysfs 文件系统结构

### 4.1 sysfs 顶级目录组织

```
/sys/
├── devices/     # 真实的设备对象树（核心）
├── bus/         # 按总线类型组织的设备视图
├── class/       # 按功能分类组织的设备视图
├── module/      # 内核模块视图
├── firmware/    # 固件相关
├── fs/          # 文件系统相关
├── kernel/      # 内核参数和状态
└── power/       # 电源管理
```

### 4.2 核心原则

**重要概念：**

> `/sys/devices` 存储真实的设备对象，其他目录只是提供不同的观察视角。

**"一个事实，多个视角"：**

```
/sys/devices/  <-- 唯一的事实来源（真实设备对象）
      ^
      |
      +-- /sys/bus/     (总线视角)
      |
      +-- /sys/class/   (功能视角)
      |
      +-- /sys/module/  (模块视角)
```

### 4.3 完整的 sysfs 心智模型

**多层视角关系图：**

```
                 USER SPACE
                      |
                      v
                /sys/class/
                (功能视角)
                      |
                      | symlink
                      v
              +---------------+
              |    Device     |  <-- 真实对象
              |   Object      |
              +---------------+
                      ^
                      |
                      | symlink
              +---------------+
              |    Driver     |
              +---------------+
                      ^
                      |
                      | managed by
              +---------------+
              |      Bus      |
              +---------------+
                      |
                      v
                /sys/devices/
                (物理视角)
```

### 4.4 符号链接的意义

**为什么大量使用符号链接？**

同一个设备需要从多个角度观察：

```
硬件工程师视角：  "这是一个PCI设备"
驱动开发者视角：  "这是ixgbe管理的设备"
用户应用视角：    "这是eth0网卡"
```

**避免重复的设计：**

如果每个视角都复制完整的设备信息 → 大量重复数据

通过符号链接 → 一份真实数据，多个引用视角

**实现方式：**

```
真实对象位置：    /sys/devices/pci0000:00/0000:03:00.0/
总线视角链接：    /sys/bus/pci/devices/0000:03:00.0 -> ../../devices/...
功能视角链接：    /sys/class/net/eth0 -> ../../devices/.../net/eth0
```

---

## 5. 实例分析：网卡设备

以 `eth0` 网卡设备为例，深入理解 Device Model 的实际应用。

### 5.1 基础信息查看

**查看设备类型：**

```bash
ls -l /sys/class/net/eth0
```

**典型输出：**
```
eth0 -> ../../devices/pci0000:00/0000:03:00.0/net/eth0
```

**关键洞察：**
> `eth0` 并不真正存在于 `/sys/class/net/` 中，它只是一个符号链接，真正的设备对象位于 `/sys/devices/` 下。

### 5.2 网卡的完整 Device Model 关系

**四个对象的关系图：**

```
                    PCI Bus
                       |
                       | discovers
                       v
                0000:03:00.0 (PCI Device)
                       |
              +--------+--------+
              |                 |
              | bound to        | classified as
              v                 v
        ixgbe Driver        net Class
              |                 |
              |                 |
              +----> eth0 <-----+
                   (网络接口)
```

### 5.3 对应的 sysfs 目录结构

```
/sys/
├── devices/                    # 真实设备树
│   └── pci0000:00/
│       └── 0000:03:00.0/      # PCI设备真实位置
│           ├── driver -> ../../../bus/pci/drivers/ixgbe
│           ├── subsystem -> ../../../bus/pci
│           └── net/
│               └── eth0/      # 网络接口真实位置
│
├── bus/                       # 总线视角
│   └── pci/
│       ├── devices/
│       │   └── 0000:03:00.0 -> ../../../devices/pci0000:00/0000:03:00.0
│       └── drivers/
│           └── ixgbe/
│               └── 0000:03:00.0 -> ../../../../devices/pci0000:00/0000:03:00.0
│
└── class/                     # 功能视角
    └── net/
        └── eth0 -> ../../devices/pci0000:00/0000:03:00.0/net/eth0
```

### 5.4 设备追踪实战

#### 5.4.1 从功能视角找到真实设备

```bash
# 从网卡名称找到真实设备位置
readlink -f /sys/class/net/eth0
```

**输出：**
```
/sys/devices/pci0000:00/0000:03:00.0/net/eth0
```

**追踪路径：**
```
Class View (功能视角)
      |
      v
   eth0 (符号链接)
      |
      v
Device View (物理视角)
      |
      v
0000:03:00.0 (PCI设备)
```

#### 5.4.2 从设备找到绑定的驱动

```bash
# 查看设备绑定的驱动
readlink -f /sys/class/net/eth0/device/driver
```

**输出：**
```
/sys/bus/pci/drivers/ixgbe
```

**追踪路径：**
```
eth0 (网络接口)
  |
  v
device (指向PCI设备)
  |
  v
driver (指向驱动)
  |
  v
ixgbe (PCI驱动)
```

#### 5.4.3 从设备找到所属总线

```bash
# 查看设备所属的总线类型
readlink -f /sys/class/net/eth0/device/subsystem
```

**输出：**
```
/sys/bus/pci
```

**追踪路径：**
```
eth0 (网络接口)
  |
  v
device (PCI设备)
  |
  v
subsystem (总线类型)
  |
  v
PCI Bus (PCI总线)
```

### 5.5 完整的关系追踪

**正向追踪（从硬件到用户）：**

```
PCI Bus (硬件总线)
    |
    | discovers
    v
0000:03:00.0 (PCI设备)
    |
    | managed by
    v
ixgbe Driver (设备驱动)
    |
    | exposes as
    v
net Class (功能分类)
    |
    | provides
    v
eth0 (用户接口)
```

**反向追踪（从用户到硬件）：**

```
eth0 (用户看到的网卡)
  |
  | belongs to
  v
net Class (网络设备类)
  |
  | implemented by
  v
0000:03:00.0 (实际PCI设备)
  |
  | controlled by
  v
ixgbe Driver (设备驱动)
  |
  | discovered on
  v
PCI Bus (物理总线)
```

---

## 6. 调试与实际应用

### 6.1 符号链接设计的深层原理

**为什么 sysfs 大量使用符号链接？**

**核心原因：** 同一个设备对象需要从多个角度观察和管理。

**多视角需求：**

| 视角 | 关心的问题 | 典型用户 |
|------|-----------|----------|
| 硬件视角 | "这是什么类型的PCI设备？" | 硬件工程师 |
| 驱动视角 | "ixgbe驱动管理哪些设备？" | 驱动开发者 |
| 功能视角 | "eth0提供什么网络功能？" | 系统管理员 |
| 应用视角 | "如何通过eth0发送数据？" | 应用开发者 |

**设计选择对比：**

```
方案A：复制目录结构
/sys/bus/pci/devices/0000:03:00.0/     (完整设备信息)
/sys/class/net/eth0/                   (完整设备信息副本)
结果：大量重复数据，同步困难

方案B：符号链接 (Linux采用)
/sys/devices/pci0000:00/0000:03:00.0/  (唯一真实对象)
/sys/bus/pci/devices/0000:03:00.0      -> (符号链接)
/sys/class/net/eth0                    -> (符号链接)
结果：一份数据，多个视角，自动同步
```

### 6.2 设备调试完整工具链

**场景：** 网卡性能问题排查

#### 6.2.1 基本设备信息收集

```bash
# 1. 查看网卡基本状态
ethtool eth0

# 2. 找到对应的PCI设备
PCI_DEVICE=$(readlink -f /sys/class/net/eth0/device)
echo "PCI设备位置: $PCI_DEVICE"

# 3. 查看PCI设备详细信息
lspci -s $(basename $PCI_DEVICE) -v
```

#### 6.2.2 驱动和模块信息

```bash
# 4. 找到绑定的驱动
DRIVER_PATH=$(readlink -f /sys/class/net/eth0/device/driver)
DRIVER_NAME=$(basename $DRIVER_PATH)
echo "驱动名称: $DRIVER_NAME"

# 5. 查看驱动模块信息
MODULE_NAME=$(basename $(readlink -f $DRIVER_PATH/module))
echo "内核模块: $MODULE_NAME"

# 6. 查看模块详细信息
modinfo $MODULE_NAME

# 7. 查看驱动版本和参数
cat /sys/module/$MODULE_NAME/version 2>/dev/null || echo "无版本信息"
ls /sys/module/$MODULE_NAME/parameters/ 2>/dev/null || echo "无参数"
```

#### 6.2.3 总线和电源信息

```bash
# 8. 确认总线类型
SUBSYSTEM=$(readlink -f /sys/class/net/eth0/device/subsystem)
echo "总线类型: $(basename $SUBSYSTEM)"

# 9. 查看设备电源状态
cat /sys/class/net/eth0/device/power/runtime_status 2>/dev/null || echo "无电源信息"

# 10. 查看PCI配置空间
cat /sys/class/net/eth0/device/config | hexdump -C | head -10
```

#### 6.2.4 设备层次结构分析

```bash
# 11. 显示完整的设备路径层次
echo "设备层次结构:"
echo "用户接口: /sys/class/net/eth0"
echo "真实设备: $(readlink -f /sys/class/net/eth0)"
echo "父设备链:"
CURRENT=$(dirname $(readlink -f /sys/class/net/eth0/device))
while [ "$CURRENT" != "/sys/devices" ]; do
    echo "  $(basename $CURRENT)"
    CURRENT=$(dirname $CURRENT)
done
```

### 6.3 完整的问题排查路径

**从用户问题到硬件根因：**

```
用户报告: "eth0网卡很慢"
     |
     v
eth0 (用户接口层)
     |
     | /sys/class/net/eth0 ->
     v
net/eth0 (功能抽象层)
     |
     | ../device ->
     v
0000:03:00.0 (PCI设备层)
     |
     | driver ->
     v
ixgbe (驱动层)
     |
     | module ->
     v
ixgbe.ko (内核模块层)
     |
     | 检查参数、版本、配置
     v
硬件根因分析
```

### 6.4 自动化脚本示例

**网卡信息一键收集脚本：**

```bash
#!/bin/bash
# 网卡设备信息完整收集脚本

INTERFACE=$1
if [ -z "$INTERFACE" ]; then
    echo "用法: $0 <网卡名称>"
    echo "示例: $0 eth0"
    exit 1
fi

echo "=== 网卡 $INTERFACE 完整信息 ==="
echo

# 基本检查
if [ ! -e "/sys/class/net/$INTERFACE" ]; then
    echo "错误: 网卡 $INTERFACE 不存在"
    exit 1
fi

# 设备路径追踪
DEVICE_PATH=$(readlink -f /sys/class/net/$INTERFACE/device)
DRIVER_PATH=$(readlink -f $DEVICE_PATH/driver)
DRIVER_NAME=$(basename $DRIVER_PATH)
MODULE_PATH=$(readlink -f $DRIVER_PATH/module 2>/dev/null)
MODULE_NAME=$(basename $MODULE_PATH 2>/dev/null)

echo "1. 设备路径追踪:"
echo "   用户接口: /sys/class/net/$INTERFACE"
echo "   真实设备: $DEVICE_PATH"
echo "   驱动路径: $DRIVER_PATH"
echo "   驱动名称: $DRIVER_NAME"
echo "   模块名称: ${MODULE_NAME:-未知}"
echo

echo "2. PCI设备信息:"
PCI_ID=$(basename $DEVICE_PATH)
lspci -s $PCI_ID -v | head -20
echo

echo "3. 驱动模块信息:"
if [ -n "$MODULE_NAME" ]; then
    modinfo $MODULE_NAME | head -10
else
    echo "   内置驱动，无模块信息"
fi
echo

echo "4. 网卡状态:"
ethtool $INTERFACE 2>/dev/null | grep -E "(Speed|Duplex|Link detected)" || echo "   无法获取状态信息"
```

---

## 7. 总结与心智模型

### 7.1 Linux Device Model 完整架构

**统一设备管理架构：**

```
                Linux Device Model
                       |
           +-----------+-----------+
           |                       |
           v                       v
      内核空间                  用户空间
           |                       |
    +------+------+         +------+------+
    |             |         |             |
    v             v         v             v
kobject框架   设备驱动    sysfs VFS    用户工具
    |             |         |             |
    +------+------+         +------+------+
           |                       |
           +-----------+-----------+
                       |
                       v
               统一的设备视图
```

**四大核心对象关系：**

```
            +----------+
            |   Bus    |  <-- 设备发现和管理
            +----------+
                 |
                 | discovers
                 v
            +----------+
            |  Device  |  <-- 真实的设备对象
            +----------+
             |        |
             |        |
             v        v
      +----------+  +----------+
      |  Driver  |  |  Class   |  <-- 驱动控制 + 功能分类
      +----------+  +----------+
             |        |
             |        |
             +----+---+
                  |
                  v
                User Application
```

### 7.2 sysfs 视角映射

**多视角统一模型：**

| sysfs路径 | 视角类型 | 主要用途 | 典型用户 |
|-----------|----------|----------|----------|
| `/sys/devices/` | **物理视角** | 真实设备对象存储 | 内核开发者 |
| `/sys/bus/` | **总线视角** | 设备发现和分类管理 | 驱动开发者 |
| `/sys/class/` | **功能视角** | 用户空间功能接口 | 系统管理员 |
| `/sys/module/` | **模块视角** | 驱动模块管理 | 驱动调试者 |

**视角关系图：**

```
     /sys/devices/ (唯一事实来源)
           ^
           |
    +------+------+------+
    |      |      |      |
    v      v      v      v
  /sys/  /sys/  /sys/  /sys/
  bus/   class/ module/ firmware/
 (总线)  (功能) (模块)  (固件)
```

### 7.3 核心设计原则

#### 7.3.1 单一事实来源 (Single Source of Truth)

```
原则：所有设备信息只在 /sys/devices/ 中存储一份
实现：其他目录通过符号链接引用
好处：避免数据重复，确保一致性
```

#### 7.3.2 多视角访问 (Multiple Views)

```
需求：不同角色需要不同的设备组织方式
解决：提供bus、class、module等不同视角
机制：符号链接实现视角切换
```

#### 7.3.3 层次化组织 (Hierarchical Organization)

```
物理层次：CPU -> PCI总线 -> PCI设备 -> 功能单元
逻辑层次：总线 -> 设备 -> 驱动 -> 设备类
映射关系：/sys/devices/ 反映真实的物理层次
```

### 7.4 关键洞察总结

**理解 `/sys` 的核心洞察：**

> **核心原则：** `/sys/devices` 是事实，其他都是视角

**具体含义：**

1. **`/sys/devices`** = 设备**是什么**（真实的硬件对象）
2. **`/sys/bus`** = 设备**如何被发现**（通过什么总线）
3. **`/sys/class`** = 用户**如何使用设备**（提供什么功能）
4. **`/sys/module`** = 驱动**如何实现设备**（哪个模块控制）

**符号链接的作用：** 把这些不同视角有机地连接起来，共同构成完整的 Linux Device Model。

### 7.5 实践记忆法

**"四个问题"记忆法：**

当你看到任何 `/sys` 路径时，问自己：

1. 这个设备**是什么**？ → 去 `/sys/devices` 找真实对象
2. 它**怎么被发现的**？ → 去 `/sys/bus` 看总线类型
3. 用户**怎么使用它**？ → 去 `/sys/class` 看功能分类
4. 驱动**怎么控制它**？ → 去 `/sys/module` 看模块信息

**"追踪三步法"：**

对于任何设备问题：

1. **定位真实设备：** `readlink -f /sys/class/*/设备名/device`
2. **找到驱动：** `readlink -f 设备路径/driver`
3. **确认总线：** `readlink -f 设备路径/subsystem`

### 7.6 扩展学习方向

**进阶话题：**

- **udev规则** - 如何基于设备属性自动化设备管理
- **设备树 (Device Tree)** - 嵌入式系统中的设备描述
- **热插拔机制** - 设备动态加载和卸载
- **电源管理** - 设备的电源状态管理
- **IOMMU** - 设备内存管理单元
- **容器化设备** - 容器环境中的设备访问

**相关工具：**
- `udevadm` - udev设备管理工具
- `lsof` - 查看设备文件使用情况
- `fuser` - 查找使用设备的进程
- `systemd-analyze` - 系统启动和设备初始化分析

---

## 参考资料

- [Linux Kernel Documentation - Driver Model](https://www.kernel.org/doc/html/latest/driver-api/driver-model/)
- [sysfs - Linux man page](https://man7.org/linux/man-pages/man5/sysfs.5.html)
- [Understanding the Linux Kernel Device Model](https://lwn.net/Articles/645810/)
- [Linux Device Drivers, 3rd Edition](https://lwn.net/Kernel/LDD3/)

---

*最后更新时间：2026年6月*

---
