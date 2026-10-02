# Experiment 2.1 — Tracing Linux PCI Enumeration in Kernel 6.12

## 1. Why

许多教程照搬老旧内核的 `pci_scan_bus()`，但在现代化内核（如 Linux 6.12）中，PCI 枚举链路已经经过了深度的重构与模块化。
只有亲手在真实的内核源码树中追溯一次执行流，才能把理论概念与实际内核代码严密锚定。

## 2. Current Mental Model

我们知道硬件里有配置空间，知道内核启动时会做扫描，但不知道具体是哪一段内核 C 代码在执行这一过程，也不知道数据结构 `struct pci_dev` 是在何时何地被实例化的。

## 3. New Concept

本实验唯一新增的核心概念是：**Linux 6.12 内核中 PCI 枚举的核心调用链：`pci_scan_child_bus()` -> `pci_scan_slot()` -> `pci_scan_single_device()` -> `pci_setup_device()` -> `pci_device_add()`**。

## 4. Architecture

```
Linux 6.12 PCI Scanning Call Graph:

pci_scan_root_bus_bridge() [drivers/pci/probe.c]
           |
           v
  pci_scan_child_bus()
           |
           v
    pci_scan_slot()
           |
           v
      pci_scan_single_device()
           |
           +--> pci_bus_read_dev_vendor_id() [读 Vendor ID，判断 0xFFFF]
           |
           +--> pci_alloc_dev()              [kzalloc 分配 struct pci_dev]
           |
           +--> pci_setup_device()           [读取 BAR 大小、Class、IRQ Pin 等]
           |          |
           |          +--> pci_read_bases()  [全 1 写入法计算每个 BAR 的 size]
           |
           v
    pci_device_add()                         [注册入 Device Model: device_add()]
```

## 5. Data Flow

```
Hardware Config Space
         |
         | pci_bus_read_config_dword()
         v
struct pci_dev 临时对象 (包含 dev->vendor, dev->device, dev->resource[BAR0..5])
         |
         | pci_device_add()
         v
pci_bus_type 维护的全局设备链表
         |
         v
导出至 sysfs /sys/bus/pci/devices/
```

## 6. Control Flow

在当前克隆的 `kernel/drivers/pci/probe.c` 中：
1. `pci_scan_child_bus_extend()`：外层循环遍历总线上的 32 个 device 槽位。
2. `pci_scan_slot()`：处理多功能设备（Function 0..7）。
3. `pci_scan_single_device()`：调用 `pci_bus_read_dev_vendor_id()` 读取 `PCI_VENDOR_ID`。
4. 若存在，调用 `pci_alloc_dev()` 分配内存。
5. 调用 `pci_setup_device()` 解析设备的全部静态属性。
6. 调用 `pci_device_add()` 将该设备正式公之于世（进入 Linux Device Model，触发可能的驱动匹配）。

## 7. State

```
SLOT_UNCHECKED
      |
      | 读 Vendor ID
      v
DEVICE_FOUND (!= 0xFFFF)
      |
      | pci_alloc_dev()
      v
PCI_DEV_ALLOCATED
      |
      | pci_setup_device() 读取 BAR
      v
RESOURCES_RECORDED
      |
      | pci_device_add()
      v
DEVICE_ONLINE (挂在 pci_bus_type 下，等待驱动 match)
```

## 8. Code & Trace Guide

在虚拟机或本地阅读 Linux 6.12 核心代码切片：

### `drivers/pci/probe.c`: `pci_scan_single_device()`
```c
// 伪代码与真实代码映射切片 (摘自 6.12 kernel/drivers/pci/probe.c)
struct pci_dev *pci_scan_single_device(struct pci_bus *bus, int devfn)
{
    struct pci_dev *dev;

    dev = pci_scan_device(bus, devfn);
    if (!dev)
        return NULL;

    pci_device_add(dev, bus);
    return dev;
}
```

### `drivers/pci/probe.c`: `pci_scan_device()`
```c
static struct pci_dev *pci_scan_device(struct pci_bus *bus, int devfn)
{
    struct pci_dev *dev;
    u32 l;

    // 关键点：探测 Vendor ID 是否有效！
    if (!pci_bus_read_dev_vendor_id(bus, devfn, &l, 60*1000))
        return NULL;

    dev = pci_alloc_dev(bus);
    if (!dev)
        return NULL;

    dev->devfn = devfn;
    pci_setup_device(dev);
    return dev;
}
```

### 追踪验证脚本 `trace_scan.sh`
```bash
#!/bin/sh
# trace_scan.sh - 在 QEMU 启动日志中寻找内核枚举 PCI 设备的证据

echo "=================================================="
echo " Experiment 2.1: 内核 PCI 枚举日志检查"
echo "=================================================="

echo "[1] 提取内核启动阶段关于 PCI bus scan 的 dmesg 日志:"
dmesg | grep -iE "pci 0000:|pci_bus" | head -n 20

echo ""
echo "[2] 查看 QEMU edu 设备的枚举记录:"
dmesg | grep -i "1234:11e8"

echo ""
echo "=== 检查完成，关机 ==="
poweroff -f
```

## 9. Build

```bash
mkdir -p build
cp trace_scan.sh build/test.sh
chmod +x build/test.sh
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
```

## 10. Run

```bash
../../common/run-qemu.sh initramfs.cpio.gz -device edu
```

## 11. Observe

在控制台输出中寻找如下特征行：
- `pci 0000:00:04.0: [1234:11e8] type 00 class 0x00ff00`
- `pci 0000:00:04.0: reg 0x10: [mem 0xfeb90000-0xfec8ffff]`
这证实了 `pci_setup_device()` 和 `pci_read_bases()` 已经在内核引导期间成功执行！

## 12. Expected Result

```
pci 0000:00:04.0: [1234:11e8] type 00 class 0x00ff00
pci 0000:00:04.0: reg 0x10: [mem 0x00000000-0x000fffff]
pci 0000:00:04.0: BAR 0: assigned [mem 0xfeb90000-0xfec8ffff]
```

## 13. Source Code

- `drivers/pci/probe.c`: PCI 枚举的核心主逻辑。
  - `pci_scan_child_bus()`
  - `pci_scan_slot()`
  - `pci_scan_device()`
  - `pci_setup_device()`
  - `pci_read_bases()`
- `include/linux/pci.h`: `struct pci_dev` 的完整定义。

## 14. Failure Injection

### 假定硬件损坏或时序异常，导致 Vendor ID 持续返回 0xFFFF
如果设备在总线上出现接触不良或未上电，`pci_bus_read_dev_vendor_id()` 读出全 1：
- **Symptom**：系统启动日志中完全看不到该设备的任何信息；
- **Root Cause**：`pci_scan_device()` 遇到无效 Vendor ID 会直接返回 `NULL`，连 `struct pci_dev` 都不会被分配，根本进入不了后面的设备驱动匹配环节。

## 15. Mental Model Summary

```
Before: 以为 struct pci_dev 是驱动自己创建的
   |
New Concept: struct pci_dev 是内核 PCI Core 在系统启动阶段(或热插拔时)
             通过配置空间扫描自动实例化并维护的
   |
After: 明确了硬件到软件对象的一对一绑定时刻，
       驱动的 probe() 只是接收已经由内核扫描创建好的 struct pci_dev 指针
```
