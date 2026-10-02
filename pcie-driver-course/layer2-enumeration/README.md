# Layer 2 — PCI Enumeration（PCI 设备枚举）

## 1. Mental Model

操作系统是如何知道系统中插入了哪些 PCI 设备，并且如何为它们分配总线编号和资源地址的？
这就是 **PCI Enumeration（PCI 设备枚举）** 的职责。

```
              Firmware (BIOS / UEFI)
                        |
       (分配基本总线号与初始物理地址窗口)
                        v
              Linux Kernel Boot
                        |
            pci_scan_root_bus_bridge()
                        |
              pci_scan_child_bus()
                        |
         [递归遍历: Bus 0..255, Dev 0..31, Func 0..7]
                        |
            pci_scan_single_device()
                        |
              读取 0x00 (Vendor ID)
             /                     \
       != 0xFFFF                 == 0xFFFF
           |                         |
      发现物理设备               插槽无设备，跳过
           |
     pci_setup_device()
           |
   (解析 BAR 大小、Class)
           |
           v
    alloc struct pci_dev
           |
           v
     pci_device_add()
           |
     [注册到 pci_bus_type]
           |
           v
/sys/bus/pci/devices/0000:xx:xx.x
```

## 2. Why

PCI 是一种动态即插即用总线，硬件上不预留固定的内存地址或中断线。
如果内核不进行统一的枚举和资源规划：
1. 驱动程序根本不知道设备的物理 MMIO 寄存器映射在 CPU 物理地址的哪个角落；
2. 两个不同设备可能会申请相互冲突的物理地址范围；
3. 内核无法在抽象层面构造出 `struct pci_dev`。

枚举的本质就是：**探测物理存在、分配不冲突的系统资源、构建内核内部的对象模型（struct pci_dev）**。

## 3. 核心机制：How Scanning Works

### 3.1 怎么发现设备是否存在？
CPU 通过 Host Bridge 尝试读取某个 BDF 的 0x00 偏移（Vendor ID）。
- 如果读取结果是 `0xFFFF`，说明总线上没有设备响应终止（Master Abort），该插槽无设备。
- 如果读取结果不是 `0xFFFF`（例如 `0x1234`），说明物理设备存在！

### 3.2 怎么知道 BAR 需要多大物理内存？（著名的全 1 写入法）
软件先向某个 BAR 写入全 1（`0xFFFFFFFF`），然后再读取回来。
由于硬件的 BAR 低位会被芯片设计逻辑硬连线（Hardwired）为 0，用来指示对齐与大小：
例如，读回来的值是 `0xFFF00000`（低 20 位全为 0）：
取反再加 1：`~0xFFF00000 + 1 = 0x00100000`（即 1MB）。
内核由此瞬间获知：**该设备需要 1MB 且自然对齐的物理内存窗口！**
随后内核在系统空闲的物理 MMIO 窗口中圈出 1MB 地址，写回到该 BAR 中。设备自此开始监听这个物理地址。

## Layer 2 实验列表

| Experiment | 目标 | 目录 |
|---|---|---|
| 2.1 | 源码追踪：Linux 6.12 中 `pci_scan_bus` 与 `pci_scan_child_bus` 的实现路径 | [exp2.1-scan-trace/](exp2.1-scan-trace/) |
| 2.2 | 观测 `/sys/bus/pci/devices/` 与 `/proc/iomem`，建立从硬件到系统资源的映射认知 | [exp2.2-sysfs-iomem/](exp2.2-sysfs-iomem/) |

## Checkpoint

- 枚举阶段中，内核如何判断一个 BDF 上是否存在设备？
- 什么是 BAR 探测中的“全 1 写入法”？硬件是如何告诉内核它需要多大内存空间的？
- 在 Linux 6.12 内核中，`struct pci_dev` 是在哪个函数中被分配和填充的？
- `/proc/iomem` 中的资源树与 PCI 设备的 BAR 有什么对应关系？
