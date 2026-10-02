# Experiment 1.2 — Deep Inspection of PCI Config Space

## 1. Why

光知道设备的 Vendor/Device ID 还远远不够。
一个 PCI 设备究竟需要多少片内存空间（BAR）？支持哪些特性（MSI？PCI Express？电源管理？）？
这些关键信息全藏在 PCI 配置空间（Configuration Space）中。
深入理解配置空间的每一个字段，是掌握后续 BAR 映射与中断机制的先决条件。

## 2. Current Mental Model

在 Exp 1.1 中，我们知道通过 BDF 可以定位一个设备，且设备有 Vendor ID 和 Device ID。
但我们还不清楚配置空间里除 ID 之外的其余 60 多个字节（标准头部）和扩展 Capabilities 是什么。

## 3. New Concept

本实验唯一新增的核心概念是：**PCI Type 0 配置空间标准头部（Header Type 0）的内存布局，包括 Command/Status 寄存器、Base Address Registers (BARs) 以及 Capability 链表指针**。

## 4. Architecture

标准 PCI Type 0 配置空间（前 64 字节）结构图：

```
+-----------------------------------------------------------+
| 00: Vendor ID (16-bit)      | 02: Device ID (16-bit)      |
+-----------------------------+-----------------------------+
| 04: Command Register (16)   | 06: Status Register (16)    |
+-----------------------------+-----------------------------+
| 08: Revision ID (8) | Class Code (24-bit)                 |
+-----------------------------------------------------------+
| 0C: CacheLine | Latency     | Header Type | BIST          |
+-----------------------------------------------------------+
| 10: Base Address Register 0 (BAR0)                        |
+-----------------------------------------------------------+
| 14: Base Address Register 1 (BAR1)                        |
+-----------------------------------------------------------+
| 18: Base Address Register 2 (BAR2)                        |
+-----------------------------------------------------------+
| 1C: Base Address Register 3 (BAR3)                        |
+-----------------------------------------------------------+
| 20: Base Address Register 4 (BAR4)                        |
+-----------------------------------------------------------+
| 24: Base Address Register 5 (BAR5)                        |
+-----------------------------------------------------------+
| 28: CardBus CIS Pointer                                   |
+-----------------------------------------------------------+
| 2C: Subsystem Vendor ID     | 2E: Subsystem Device ID     |
+-----------------------------------------------------------+
| 30: Expansion ROM Base Address                            |
+-----------------------------------------------------------+
| 34: Capabilities Pointer (8)| Reserved                    |
+-----------------------------------------------------------+
| 38: Reserved                                              |
+-----------------------------------------------------------+
| 3C: Interrupt Line (8)      | 3D: Interrupt Pin (8)       |
| 3E: Min_Gnt (8)             | 3F: Max_Lat (8)             |
+-----------------------------------------------------------+
```

## 5. Data Flow

```
Hardware Config Space (QEMU Edu Device)
                   |
                   | lspci -xxx 读取原始 64/256 字节
                   v
Raw Hex Dump (字节 0x00 ~ 0xFF)
                   |
                   | lspci -vv 解码字段含义
                   v
Human-readable Details (BAR0 地址/大小、MSI Capability 等)
```

## 6. Control Flow

```
User runs "lspci -vv -xxx -d 1234:11e8"
       |
       v
通过 sysfs 读取 config 文件
       |
       v
lspci 解析 0x00-0x03: Vendor/Device ID
       |
       v
lspci 解析 0x04-0x05: Command (IO/Memory/BusMaster使能位)
       |
       v
lspci 解析 0x10-0x27: BAR0-BAR5 (基地址与Memory/IO标志)
       |
       v
lspci 检查 Status 的 bit 4 (Capabilities List flag)
       |
       v
沿着 0x34 处的 Cap Pointer 遍历链表 (读取 MSI Capability)
       |
       v
打印出详细的解码报告与原始 Hex 转储
```

## 7. State

```
CONFIG_SPACE_RAW
       |
       | 读取基础 Header
       v
HEADER_PARSED (识别出 BAR 数量、中断引脚、Header 类型)
       |
       | 遍历 Capability 指针 (Offset 0x34)
       v
CAPABILITIES_RESOLVED (找到 MSI Cap 结构体)
```

## 8. Code

### test.sh

```bash
#!/bin/sh
# test.sh - 深入查看 QEMU edu 设备的 PCI 配置空间

echo "=================================================="
echo " Experiment 1.2: PCI 配置空间深度观察"
echo "=================================================="

DEV="1234:11e8"

echo "[1] 查看设备的详细解码信息 (lspci -vv):"
lspci -d "$DEV" -vv

echo ""
echo "[2] 查看配置空间的原始十六进制字节转储 (lspci -xxx):"
lspci -d "$DEV" -xxx

echo ""
echo "[3] 验证 sysfs 中直接读取的 64 字节 Header:"
BDF=$(lspci -d "$DEV" | awk '{print $1}')
hexdump -C "/sys/bus/pci/devices/0000:${BDF}/config" | head -n 4

echo ""
echo "=== 观察完成，5秒后自动关机 ==="
poweroff -f
```

## 9. Build

```bash
mkdir -p build
cp test.sh build/
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
```

## 10. Run

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

对比 `lspci -vv` 与 `lspci -xxx` 的输出：
1. **0x00 - 0x03**: `34 12 e8 11` -> 小端序，Vendor ID = 0x1234, Device ID = 0x11e8。
2. **0x10 - 0x13 (BAR0)**: 观察其基地址，最低 4 位若末位为 0 表示 Memory BAR（64位或32位，可预取或不可预取）。
3. **0x34 (Capabilities Pointer)**: 观察该字节的值（通常指向 0x40 处），并在 `lspci -vv` 中看是否列出了 `Capabilities: [40] MSI: ...`。

## 12. Expected Result

```
00:04.0 Unclassified device [00ff]: Device 1234:11e8 (rev 10)
	Subsystem: Red Hat, Inc. Device 1100
	Control: I/O- Mem- BusMaster- SpecCycle- MemWINV- VGASnoop- ParErr- Stepping- SERR- FastB2B- DisINTx-
	Status: Cap+ 66MHz- UDF- FastB2B- ParErr- DEVSEL=fast >TAbort- <TAbort- <MAbort- >SERR- <PERR- INTx-
	Interrupt: pin A routed to IRQ 11
	Region 0: Memory at feb90000 (32-bit, non-prefetchable) [size=1M]
	Capabilities: [40] MSI: Enable- Count=1/1 Maskable- 64bit+
		Address: 0000000000000000  Data: 0000
```

- `Region 0: Memory at ... [size=1M]` 说明 BAR0 请求了 1MB 的 MMIO 区域。
- `Capabilities: [40] MSI` 说明该设备支持 MSI 中断。

## 13. Source Code

- 内核结构体定义：`include/uapi/linux/pci_regs.h`
  - `PCI_VENDOR_ID` (0x00)
  - `PCI_COMMAND` (0x04)
  - `PCI_BASE_ADDRESS_0` (0x10)
  - `PCI_CAPABILITY_LIST` (0x34)
- 内核能力链表遍历：`drivers/pci/pci.c` 中的 `pci_find_capability()`

## 14. Failure Injection

### 假设 Command 寄存器的 Memory 空间访问位被清零

观察 `Control: I/O- Mem- BusMaster-`。
在驱动调用 `pci_enable_device()` 之前，Command 寄存器的 `PCI_COMMAND_MEMORY` 位默认可能处于关闭状态。
如果软件强行去读写 BAR 空间，由于硬件没有使能地址译码，总线会发生致命错误或返回全 1 (`0xFFFFFFFF`)。这正是为什么 Layer 4 在访问 MMIO 前必须执行 `pci_enable_device()` 的物理本质！

## 15. Mental Model Summary

```
Before: 只知道设备是 1234:11e8
   |
New Concept: 配置空间不仅告知身份，还声明了资源需求(1MB BAR0)
             和高级通信特性(Offset 0x40 处的 MSI Capability)
   |
After: 理解了操作系统是如何知道该为设备分配多大的物理内存窗口，
       以及设备具备哪些硬件中断能力
```
