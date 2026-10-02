# Experiment 1.1 — Discovering PCI Devices with lspci

## 1. Why

在编写任何 PCI 驱动之前，必须先回答一个物理世界的问题：
**系统里到底插了哪些 PCI 设备？**
`lspci` 是 Linux 下观察 PCI 世界最基础的工具。通过它，我们可以建立从物理硬件插槽到操作系统的第一座认知桥梁。

## 2. Current Mental Model

在 Layer 0 中，我们理解了 Linux Device Model 的三元组：
`Bus`、`Device`、`Driver`。
我们知道系统里需要先有 `Device`，`Driver` 才能通过 matching 绑定它。
现在的问题是：这些 `Device` 是怎么从硬件物理世界映射到软件视图里的？

## 3. New Concept

本实验唯一新增的核心概念是：**PCI 总线上的每一个功能单元都由一个唯一的 BDF（Bus:Device.Function）三元组标识，并且通过 Vendor ID / Device ID 声明自身身份**。

## 4. Architecture

```
+-----------------------------------------------------------+
|                       CPU & Memory                         |
+-----------------------------------------------------------+
                              |
                              v
+-----------------------------------------------------------+
|                   PCI Host Bridge / Root Complex          |
|                   (Domain 0000, Bus 00)                   |
+-----------------------------------------------------------+
         |                      |                     |
         v                      v                     v
+-----------------+    +-----------------+    +-----------------+
| BDF 00:01.0     |    | BDF 00:02.0     |    | BDF 00:04.0     |
| ISA Bridge      |    | VGA Controller  |    | QEMU edu device |
| (8086:7000)     |    | (1234:1111)     |    | (1234:11e8)     |
+-----------------+    +-----------------+    +-----------------+
```

## 5. Data Flow

```
Hardware Registers (Config Space)
              |
              | CPU executes IN/OUT (Port 0xCF8/0xCFC) or ECAM (MMCONFIG)
              v
Kernel PCI Subsystem (/sys/bus/pci/devices/...)
              |
              | sysfs read
              v
User Space (lspci tool)
```

## 6. Control Flow

```
User runs "lspci"
      |
      v
lspci opens /sys/bus/pci/devices/
      |
      v
内核返回系统枚举到的所有 PCI 设备节点
      |
      v
lspci 读取每个设备的 config 二进制属性 (Config Space 前 64 字节)
      |
      v
解析 Vendor ID, Device ID, Class Code
      |
      v
格式化打印输出到终端
```

## 7. State

```
HARDWARE_POWER_ON
       |
       v
FIRMWARE_OR_KERNEL_SCAN (扫描所有 Bus/Device/Function)
       |
       v
DEVICE_NODE_CREATED (/sys/bus/pci/devices/0000:00:xx.x 生成)
       |
       v
OBSERVABLE_BY_USERSPACE (可被 lspci 读取并解析)
```

## 8. Code

本实验属于观察性实验，重点在于启动 QEMU 并在 guest 中运行 `lspci`。
我们提供自动化验证脚本 `test.sh`：

### test.sh

```bash
#!/bin/sh
# test.sh - 自动执行 lspci 并过滤出教育设备

echo "=================================================="
echo " Experiment 1.1: lspci 基础观察"
echo "=================================================="

echo "[1] 列出虚拟机中识别到的所有 PCI 设备:"
lspci

echo ""
echo "[2] 专门检索 QEMU 教育设备 (Vendor 1234: Device 11e8):"
lspci -d 1234:11e8

echo ""
echo "[3] 检查 /sys/bus/pci/devices/ 下对应的目录:"
ls -l /sys/bus/pci/devices/

echo ""
echo "=== 观察完成，5秒后自动关机 ==="
poweroff -f
```

## 9. Build

无需编译内核模块。利用公共工具打包 initramfs：

```bash
mkdir -p build
cp test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
```

## 10. Run

启动 QEMU，并挂载 QEMU 自带的 `edu` PCI 设备：

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

观察 QEMU 控制台输出：
- `lspci` 输出中是否包含 `1234:11e8`。
- 记下该设备分配到的 BDF 地址（通常在 QEMU 默认拓扑下为 `00:04.0` 或 `00:05.0`）。
- 检查 `/sys/bus/pci/devices/0000:00:xx.x` 目录结构。

## 12. Expected Result

```
00:00.0 Host bridge: Intel Corporation 440FX - 82441FX PMC [Natoma] (rev 02)
00:01.0 ISA bridge: Intel Corporation 82371SB PIIX3 ISA [Natoma/Triton II]
00:01.1 IDE interface: Intel Corporation 82371SB PIIX3 IDE [Natoma/Triton II]
00:02.0 VGA compatible controller: Device 1234:1111 (rev 02)
00:04.0 Unclassified device [00ff]: Device 1234:11e8 (rev 10)
```

其中 `00:04.0 Unclassified device [00ff]: Device 1234:11e8` 即为 QEMU `edu` 设备。

## 13. Source Code

Linux Kernel 中提供 sysfs 访问 PCI 配置空间的实现位于：
- `drivers/pci/pci-sysfs.c`：`pci_read_config()` 实现了用户态读取 `/sys/bus/pci/devices/.../config` 时的回调。

## 14. Failure Injection

### 故意不挂载设备

启动时不加 `-device edu` 参数：

```bash
../../common/run-qemu.sh initramfs.cpio.gz
```

**Symptom**：
`lspci -d 1234:11e8` 输出为空。

**Root Cause**：
PCI Bus 是即插即用/物理探测总线。硬件上不存在该设备时，Host Bridge 扫描时读取到的 Vendor ID 会是全 1（`0xFFFF`），内核认定该 BDF 槽位无设备，不会在 sysfs 中创建任何节点。

## 15. Mental Model Summary

```
Before: 认为设备驱动需要自己去"盲猜"硬件在哪里
   |
New Concept: PCI 是可自枚举(Self-Enumerating)的总线，
             硬件通过固定格式的 BDF 和身份 ID 被内核扫描并导出
   |
After: 任何 PCIe 设备在写驱动前，都可以先通过 lspci 确认其物理存在与 BDF 地址
```
