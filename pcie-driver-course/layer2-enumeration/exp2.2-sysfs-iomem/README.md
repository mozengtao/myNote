# Experiment 2.2 — sysfs & /proc/iomem Observation

## 1. Why

在软件调试中，“我看不见就等于它不存在”。
PCI 核心扫描完成后，会将所有硬件信息向用户空间公开。
`/sys/bus/pci/devices/` 与 `/proc/iomem` 是我们验证内核资源分配是否符合预期的第一视窗。

## 2. Current Mental Model

我们已经知道内核生成了 `struct pci_dev`，并且计算出了 BAR 的物理地址范围。现在需要看清这些内核对象在 Linux 用户可见空间中的投射。

## 3. New Concept

本实验唯一新增的核心概念是：**内核通过 sysfs 将 `struct pci_dev` 的内部属性（vendor、device、resource 等）完整导出为文件树，并将已分配的物理 MMIO 地址注册进全局系统内存资源树（`/proc/iomem`）**。

## 4. Architecture

```
+-------------------------------------------------------------+
|                      struct pci_dev                         |
|   - dev->resource[0].start = 0xfeb90000                     |
|   - dev->resource[0].end   = 0xfec8ffff                     |
+-------------------------------------------------------------+
        |                                             |
        | 注册进 resource 根树                         | 导出为 sysfs 节点
        v                                             v
+-----------------------------+     +-------------------------------+
|        /proc/iomem          |     |   /sys/bus/pci/devices/.../   |
|                             |     |                               |
| feb90000-fec8ffff : PCI Bus |     |   vendor, device, config      |
|   feb90000-fec8ffff : 00:04 |     |   resource, resource0         |
+-----------------------------+     +-------------------------------+
```

## 5. Data Flow

```
struct pci_dev 字段
       |
       +--> drivers/pci/pci-sysfs.c (sysfs attributes)
       |          |
       |          v
       |    /sys/bus/pci/devices/0000:00:04.0/resource
       |
       +--> kernel/resource.c (request_resource / insert_resource)
                  |
                  v
            /proc/iomem
```

## 6. Control Flow

```
User inspects sysfs & proc
            |
            v
cat /sys/bus/pci/devices/0000:00:04.0/resource
            |
            v
内核遍历 dev->resource[0..PCI_NUM_RESOURCES-1]
            |
            v
格式化输出每一行的: 起始物理地址、终止物理地址、Flags
            |
            v
cat /proc/iomem
            |
            v
内核遍历 iomem_resource 全局树，展示总线分配的物理地址区间
```

## 7. State

```
DEVICE_SCAN_COMPLETE
         |
         | insert_resource()
         v
IOMEM_REGISTERED (在全局物理地址映射表中圈地)
         |
         | sysfs_create_group()
         v
SYSFS_EXPOSED (向用户态呈现属性文件)
```

## 8. Code

### test.sh

```bash
#!/bin/sh
# test.sh - 观察 sysfs 与 /proc/iomem 中的 PCI 资源分布

echo "=================================================="
echo " Experiment 2.2: sysfs 与 /proc/iomem 资源观察"
echo "=================================================="

BDF="0000:00:04.0"
SYS_DEV="/sys/bus/pci/devices/$BDF"

echo "[1] 查看设备 sysfs 目录中的关键属性文件:"
ls -F "$SYS_DEV"

echo ""
echo "[2] 查看设备 Vendor ID 与 Device ID 文件内容:"
echo -n "Vendor: "; cat "$SYS_DEV/vendor"
echo -n "Device: "; cat "$SYS_DEV/device"

echo ""
echo "[3] 查看设备导出的 resource 列表 (起始 结束 标志位):"
cat "$SYS_DEV/resource" | head -n 3

echo ""
echo "[4] 查看 /proc/iomem 中该物理地址区间的归属记录:"
cat /proc/iomem | grep -B 1 -A 2 -i "00:04"

echo ""
echo "=== 观察完成，关机 ==="
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

- 检查 `$SYS_DEV/resource` 的第一行（代表 BAR0）。
  格式为：`0x00000000feb90000 0x00000000fec8ffff 0x0000000000040200`。
  终止地址减起始地址正好是 0xFFFFF（即 1MB）。
- 在 `/proc/iomem` 中可以清晰看到这 1MB 物理地址归属于 `0000:00:04.0`。

## 12. Expected Result

```
Vendor: 0x1234
Device: 0x11e8
0x00000000feb90000 0x00000000fec8ffff 0x0000000000040200
feb90000-fec8ffff : 0000:00:04.0
```

## 13. Source Code

- `drivers/pci/pci-sysfs.c`: `pci_show_resource()`，负责格式化输出 `/sys/.../resource`。
- `kernel/resource.c`: `r_show()`，负责遍历并格式化 `/proc/iomem`。

## 14. Failure Injection

### 若 BIOS 或内核分配了冲突的 MMIO 地址
假设两个设备被强行写入了同一段 BAR 地址：
- **Symptom**：内核启动日志中出现 `BAR x: failed to assign [mem ...]` 或 `can't claim BAR ...: address conflict`。
- **Root Cause**：`request_resource()` 发现该地址区间在 `iomem_resource` 树中已被其他设备独占占用，拒绝插入，设备处于未分配资源状态，后续驱动 probe 必然失败。

## 15. Mental Model Summary

```
Before: 以为 BAR 只是硬件配置空间里的几个数字
   |
New Concept: BAR 在内核中被统一注册为 struct resource 节点，
             既在 /proc/iomem 形成全局独占树，又在 sysfs 提供用户态可读文件
   |
After: 当怀疑硬件未被分配 MMIO 地址或存在地址冲突时，
       知道通过 /proc/iomem 和 /sys/bus/pci/devices/ 迅速定位根因
```
