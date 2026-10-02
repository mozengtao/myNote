# Experiment 2.1 手动实践与调试指南 (Manual Guide)

## 1. 实验目标与硬件环境

* **核心目标**：在 QEMU 启动阶段深入分析 Linux 6.12 内核中 PCI 总线扫描机制（`pci_scan_child_bus` -> `pci_scan_slot` -> `pci_scan_single_device`），通过 `dmesg` 完整追踪 QEMU `edu` 设备从物理探测到 `struct pci_dev` 实例化全过程。
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
| - dmesg | grep -iE "pci 0000:|pci_bus"                      |
| - dmesg | grep -i "1234:11e8"                               |
| - dmesg | grep -i "BAR 0"                                   |
| - poweroff -f                                               |
+-------------------------------------------------------------+
```

---

## 2. 宿主机准备与打包 (Host)

本实验纯观察内核启动阶段的枚举日志，无需编译模块：

```bash
cd /home/morrism/repos/pcie-driver-course/layer2-enumeration/exp2.1-scan-trace

# 1. 准备空打包目录 (纯手动交互)
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

在 QEMU 控制台内依次手动执行以下追踪命令：

```sh
# 1. 检索内核 PCI Root Bus 初始化与总线扫描日志
dmesg | grep -iE "pci 0000:|pci_bus" | head -n 25

# 2. 精确过滤 QEMU edu 设备的枚举发现日志
dmesg | grep -i "1234:11e8"

# 3. 查看内核对该设备 BAR 资源的探测与大小分配计算
dmesg | grep -i "00:04.0: BAR 0"

# 4. 查看内核分配后的物理基地址确认日志
dmesg | grep -i "00:04.0: assigned"

# 5. 退出虚拟机
poweroff -f
```

---

## 5. 预期输出与观测指标 (Checklist)

| 检验步骤 | 预期控制台输出关键特征 | 原理解析 |
|---|---|---|
| `dmesg \| grep "1234:11e8"` | `pci 0000:00:04.0: [1234:11e8] type 00 class 0x00ff00` | 内核在 Slot 4 探测到合法 Vendor/Device ID，实例化 `struct pci_dev` |
| `dmesg \| grep "BAR 0"` | `pci 0000:00:04.0: BAR 0 [mem 0x...-0x...]: assigned` | `pci_setup_device` 通过全 1 写入法测量出 1MB 空间，并由总线分配物理地址 |
| `dmesg \| grep "pci_bus"` | `pci_bus 0000:00: scan_bus` | 主总线执行深度遍历（Root Bus Scan） |

---

## 6. 进阶内核源码对应 (Host 端研读)

如需对齐 C 代码调用链，请在宿主机查看以下内核源文件：
* `kernel/drivers/pci/probe.c`:
  - `pci_scan_child_bus_extend()`: 遍历总线槽位
  - `pci_scan_single_device()`: 读取 Vendor ID
  - `pci_setup_device()`: 解析配置空间
  - `pci_read_bases()`: 确定 BAR 大小

---

## 7. 实验完成后的环境清理 (Host Clean Up)

在虚拟机执行 `poweroff -f` 关机并退回宿主机终端后，执行以下命令清理临时打包目录与内存盘镜像（本实验无需编译内核驱动）：

```bash
# 在宿主机当前实验目录下执行：
rm -rf build initramfs.cpio.gz
```

