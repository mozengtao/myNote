# Mental Model Maps (Layer 0 ~ Layer 8 & Capstone)

## Layer 0: Linux Device Model
```
                  Linux Device Model
                         Bus
                          |
             +------------+------------+
             v                         v
           Device                    Driver
             |                         |
             +-------- match ----------+
                          |
                          v
                        probe()
```

## Layer 1: PCIe 物理拓扑与配置空间
```
    CPU
     |
Root Complex
     |
  Root Port (0000:00:xx.x)
     |
  Endpoint (Vendor ID: Device ID, BAR0..5, Capabilities)
```

## Layer 2: PCI 枚举与对象实例化
```
Firmware / Root Port
         |
         v
pci_scan_child_bus()
         |
         v
pci_scan_single_device()
         |
         v
pci_setup_device() [读 BAR 大小，生成 struct resource]
         |
         v
alloc struct pci_dev -> 挂载至 /sys/bus/pci/devices/
```

## Layer 3: PCI Core 与驱动匹配
```
struct pci_dev (0x1234:0x11e8)
       |
       | 比对
       v
struct pci_driver.id_table[]
       |
       +--> 吻合 -> 调用 probe(struct pci_dev *pdev, ...)
```

## Layer 4: BAR 与 MMIO 虚拟地址映射
```
Config Space BAR0 (物理基地址)
       |
       | pci_request_regions() [声明独占]
       v
System iomem Resource Tree (/proc/iomem)
       |
       | pci_iomap() [建立页表]
       v
Kernel Virtual Pointer (void __iomem *)
       |
       | readl() / writel() [内联总线屏障]
       v
硬件物理寄存器读写
```

## Layer 5: DMA 内存一致性与 Buffer 所有权
```
CPU 拥有 (IDLE)
       | CPU 填充待传数据
       v
CPU_PREPARED
       | 写入设备 DMA 寄存器并置位 RUN
       v
DEVICE_OWNED (CPU 严禁触碰内存)
       | 设备发起 PCIe Bus Master 读写主机物理内存
       v
DMA_DONE (硬件搬运完毕)
       | 状态归还
       v
CPU_OWNED (CPU 安全校验结果)
```

## Layer 6: 中断机制 (MSI / MSI-X)
```
硬件传输完毕
       |
       v
向 APIC 写入 Memory Write TLP (MSI 报文)
       |
       v
CPU 产生硬中断 -> 跳入驱动 ISR
       |
       +--> 1. readl(REG_IRQ_STATUS)
       +--> 2. writel(REG_IRQ_ACK) 确认清除硬件中断
       +--> 3. complete() 唤醒等待进程
```

## Layer 7: PCIe 高级特性 (TLP / AER / PM / Hotplug)
```
      PCIe Link
          |
  +-------+-------+
  |       |       |
 TLP     AER   Hotplug
(报文) (自愈)  (热插拔)
```

## Layer 8 & Capstone: 终极 DMA Ring 驱动闭环
```
User App (ioctl)
       |
       v
Driver (fill desc, update producer, write doorbell)
       |
       v
Hardware (DMA fetch desc, execute payload DMA, write DONE, update consumer)
       |
       v
MSI-X Interrupt
       |
       v
Driver ISR (ACK, reap completed descs, complete user request)
```
