# Experiment 1.2 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：深入转储并解析 QEMU `edu` 设备的 256 字节 PCI 配置空间，手动对照 PCI Header Type 0 规范验证 Vendor/Device ID、Command/Status 寄存器、BAR0 物理基址以及 Capabilities 链表（MSI 扩展能力）。
* **硬件依赖**：**必须**在 QEMU 启动参数中携带 `-device edu`。

```
+-------------------------------------------------------------+
| Host OS (Linux / WSL2)                                      |
| 1. build-initramfs.sh -> 制作基础 rootfs                    |
| 2. run-qemu.sh -> 携带 -device edu 启动 QEMU 虚拟机         |
+-------------------------------------------------------------+
                              |
                              v (串口终端直连)
+-------------------------------------------------------------+
| Guest Shell (/bin/sh)                                       |
| - lspci -d 1234:11e8 -vv                                    |
| - lspci -d 1234:11e8 -xxx                                   |
| - hexdump -C /sys/bus/pci/devices/0000:00:04.0/config       |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与打包 (Host)

```bash
cd /home/morrism/repos/pcie-driver-course/layer1-pci-basics/exp1.2-lspci-deep

# 1. 创建空打包目录 (纯手动交互，不放入自动脚本)
rm -rf build && mkdir -p build

# 2. 生成 initramfs
../../common/build-initramfs.sh build initramfs.cpio.gz
```

---

## 3. 启动 QEMU 虚拟机 (Host)

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

---

## 4. 虚拟机内手动执行步骤 (Guest)

在 QEMU 控制台内依次手动执行以下分析命令：

```sh
DEV="1234:11e8"

# 1. 详细解码 PCI 配置空间所有已知标准字段
lspci -d "$DEV" -vv

# 2. 转储原始 256 字节配置空间十六进制矩阵
lspci -d "$DEV" -xxx

# 3. 通过 sysfs 直接使用 hexdump 查看原始二进制 config 文件前 64 字节
BDF=$(lspci -d "$DEV" | awk '{print $1}')
hexdump -C "/sys/bus/pci/devices/0000:${BDF}/config" | head -n 4

# 4. 手工字段比对验证：
# - 偏移 0x00 ~ 0x03: 34 12 e8 11 -> 小端解析: Vendor=0x1234, Device=0x11e8
# - 偏移 0x10 ~ 0x13: BAR0 物理基址寄存器
# - 偏移 0x34: Capabilities Pointer 指针值（通常为 0x40）
# - 偏移 0x40: 第一个 Capability Header (05 00 ... 即 MSI Capability ID 0x05)

# 5. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `lspci -vv` | `Region 0: Memory at ... [size=1M]` | BAR0 声明了 1MB 大小的 32-bit 非预取 MMIO 空间 |
| `lspci -vv` | `Capabilities: [40] MSI: Enable- Count=1/1` | 硬件支持 MSI 消息中断，Capability 结构位于偏移 `0x40` |
| `hexdump .../config` 第一行 | `34 12 e8 11 00 00 10 00 10 00 00 ff ...` | x86 为小端字节序，前 4 字节精确对应 Vendor `0x1234` 和 Device `0x11e8` |
| 偏移 0x34 字节 | `40` | PCI Capabilities Pointer 指向首个扩展能力结构起始偏移 `0x40` |

---

## 6. 进阶探索

在虚拟机内尝试查看其他设备（如 Host Bridge `00:00.0` 或 ISA Bridge `00:01.0`）的配置空间：
```sh
lspci -s 00:00.0 -xxx
```
对比可发现：不同厂商设备有不同的 Header Type、BAR 分配和能力链表，但前 16 字节通用字段规范完全统一。

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理临时打包目录与内存盘镜像（本实验无需编译内核驱动）：

```bash
# 在宿主机当前实验目录下执行：
rm -rf build initramfs.cpio.gz
```

