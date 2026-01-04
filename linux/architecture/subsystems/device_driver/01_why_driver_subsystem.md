# WHY｜为什么需要驱动子系统

## 1. 硬件多样性问题

```
PROBLEMS OF HARDWARE DIVERSITY
+=============================================================================+
|                                                                              |
|  THE CHALLENGE: ONE KERNEL, INFINITE HARDWARE                                |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Linux kernel must support:                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  STORAGE                    NETWORK                              │    │ |
|  │  │  ├── IDE drives             ├── Intel NICs (e1000, igb, ixgbe)   │    │ |
|  │  │  ├── SATA drives            ├── Mellanox NICs                    │    │ |
|  │  │  ├── SCSI                   ├── Broadcom NICs                    │    │ |
|  │  │  ├── NVMe                   ├── Realtek NICs                     │    │ |
|  │  │  ├── USB mass storage       ├── WiFi (Intel, Qualcomm, ...)      │    │ |
|  │  │  └── SD/MMC cards           └── Bluetooth adapters               │    │ |
|  │  │                                                                  │    │ |
|  │  │  GRAPHICS                   INPUT                                │    │ |
|  │  │  ├── Intel integrated       ├── USB keyboards                    │    │ |
|  │  │  ├── NVIDIA discrete        ├── USB mice                         │    │ |
|  │  │  ├── AMD/ATI                ├── Touchscreens                     │    │ |
|  │  │  ├── ARM Mali               ├── Game controllers                 │    │ |
|  │  │  └── Virtual (QEMU, VMware) └── Touchpads                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  BUSES                      OTHER                                │    │ |
|  │  │  ├── PCI/PCIe               ├── Audio codecs                     │    │ |
|  │  │  ├── USB (1.1, 2.0, 3.x)    ├── I2C sensors                      │    │ |
|  │  │  ├── I2C                    ├── SPI flash                        │    │ |
|  │  │  ├── SPI                    ├── GPIO controllers                 │    │ |
|  │  │  ├── Platform (SoC)         ├── PWM controllers                  │    │ |
|  │  │  └── Thunderbolt            └── DMA controllers                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  TOTAL: 10,000+ drivers in Linux kernel!                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  THE DIVERSITY DIMENSIONS                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. REGISTER INTERFACE                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Device A:                     Device B:                         │    │ |
|  │  │  ┌──────────────────────┐     ┌──────────────────────┐          │    │ |
|  │  │  │ REG 0x00: Control    │     │ REG 0x00: Status     │          │    │ |
|  │  │  │ REG 0x04: Status     │     │ REG 0x08: Control    │          │    │ |
|  │  │  │ REG 0x08: Data       │     │ REG 0x10: TX_Desc    │          │    │ |
|  │  │  │ REG 0x0C: IRQ_Mask   │     │ REG 0x18: RX_Desc    │          │    │ |
|  │  │  └──────────────────────┘     └──────────────────────┘          │    │ |
|  │  │                                                                  │    │ |
|  │  │  Every vendor has different register layouts!                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. INTERRUPT BEHAVIOR                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Some devices use level-triggered interrupts                   │    │ |
|  │  │  • Some use edge-triggered                                       │    │ |
|  │  │  • Some support MSI/MSI-X (message-signaled)                     │    │ |
|  │  │  • Some share interrupt lines                                    │    │ |
|  │  │  • Different ways to acknowledge/clear interrupts                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. DMA CAPABILITIES                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • 32-bit vs 64-bit addressing                                   │    │ |
|  │  │  • Scatter-gather support (or not)                               │    │ |
|  │  │  • Coherent vs non-coherent                                      │    │ |
|  │  │  • IOMMU requirements                                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  4. POWER MANAGEMENT                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Different sleep states (D0, D1, D2, D3)                       │    │ |
|  │  │  • Wake-up capabilities                                          │    │ |
|  │  │  • Clock gating requirements                                     │    │ |
|  │  │  • Voltage domain dependencies                                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**硬件多样性问题**：

Linux 内核必须支持的设备：
- **存储**：IDE、SATA、SCSI、NVMe、USB 大容量存储、SD/MMC
- **网络**：Intel、Mellanox、Broadcom、Realtek、WiFi、蓝牙
- **图形**：Intel 集成、NVIDIA 独立、AMD、ARM Mali、虚拟设备
- **总线**：PCI/PCIe、USB、I2C、SPI、平台设备、Thunderbolt
- **其他**：音频、传感器、GPIO、PWM、DMA 控制器

**总计**：Linux 内核中超过 10,000 个驱动！

**多样性维度**：
1. **寄存器接口**：每个厂商有不同的寄存器布局
2. **中断行为**：电平触发/边沿触发、MSI/MSI-X、共享中断线
3. **DMA 能力**：32 位/64 位寻址、Scatter-gather、一致性
4. **电源管理**：不同睡眠状态、唤醒能力、时钟门控

---

## 2. 没有驱动模型会失败什么

```
WHAT FAILS WITHOUT A DRIVER MODEL
+=============================================================================+
|                                                                              |
|  WITHOUT ABSTRACTION: CHAOS                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Imaginary kernel without driver model:                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  void init_all_hardware(void)                                    │    │ |
|  │  │  {                                                               │    │ |
|  │  │      // Every device hardcoded!                                  │    │ |
|  │  │      if (has_intel_e1000()) init_intel_e1000();                  │    │ |
|  │  │      if (has_realtek_8139()) init_realtek_8139();                │    │ |
|  │  │      if (has_broadcom_bnx2()) init_broadcom_bnx2();              │    │ |
|  │  │      if (has_mellanox_mlx4()) init_mellanox_mlx4();              │    │ |
|  │  │      // ... 10,000 more ...                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │      if (has_intel_ahci()) init_intel_ahci();                    │    │ |
|  │  │      if (has_marvell_sata()) init_marvell_sata();                │    │ |
|  │  │      // ... 5,000 more ...                                       │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  PROBLEMS:                                                       │    │ |
|  │  │  • Kernel must include ALL drivers                               │    │ |
|  │  │  • Boot time probes everything                                   │    │ |
|  │  │  • No hot-plug possible                                          │    │ |
|  │  │  • Adding new driver requires kernel modification                │    │ |
|  │  │  • Dependencies between devices impossible to manage             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  FAILURE 1: NO MODULARITY                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Without driver model:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                     MONOLITHIC KERNEL                     │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │   │    │ |
|  │  │  │  │ e1000│ │r8139 │ │bnx2 │ │ ahci │ │ nvme │ │ xhci │   │   │    │ |
|  │  │  │  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘   │   │    │ |
|  │  │  │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │   │    │ |
|  │  │  │  │i915  │ │radeon│ │nouveau│ │virtio│ │ usb │ │ spi │   │   │    │ |
|  │  │  │  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘ └──────┘   │   │    │ |
|  │  │  │         ... 10,000 more drivers compiled in ...          │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  Kernel size: 500MB+                                      │   │    │ |
|  │  │  │  Boot time: minutes (probing everything)                  │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  With driver model:                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │            CORE KERNEL (small, ~5MB)                      │   │    │ |
|  │  │  │  ┌────────────────────────────────────────────────────┐   │   │    │ |
|  │  │  │  │           Driver Model Framework                    │   │   │    │ |
|  │  │  │  │  • Bus registration                                 │   │   │    │ |
|  │  │  │  │  • Device/driver matching                           │   │   │    │ |
|  │  │  │  │  • Lifecycle management                             │   │   │    │ |
|  │  │  │  └────────────────────────────────────────────────────┘   │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                              │                                   │    │ |
|  │  │                    load on demand                                │    │ |
|  │  │                              │                                   │    │ |
|  │  │         ┌──────────────┬─────┴─────┬──────────────┐              │    │ |
|  │  │         ▼              ▼           ▼              ▼              │    │ |
|  │  │    ┌─────────┐   ┌─────────┐  ┌─────────┐   ┌─────────┐         │    │ |
|  │  │    │ e1000.ko│   │nvme.ko  │  │i915.ko  │   │ xhci.ko │         │    │ |
|  │  │    │(loaded) │   │(loaded) │  │(loaded) │   │(loaded) │         │    │ |
|  │  │    └─────────┘   └─────────┘  └─────────┘   └─────────┘         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Only needed drivers loaded!                                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  FAILURE 2: NO HOT-PLUG                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Without driver model: Device added at boot, stays forever              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User plugs in USB drive:                                        │    │ |
|  │  │  1. Hardware signals interrupt                                   │    │ |
|  │  │  2. ??? Who handles this? No framework to discover device        │    │ |
|  │  │  3. ??? Which driver should handle it?                           │    │ |
|  │  │  4. ??? How to notify userspace?                                 │    │ |
|  │  │  5. CRASH or IGNORED                                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  With driver model: Elegant hot-plug                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User plugs in USB drive:                                        │    │ |
|  │  │  1. USB hub detects new device                                   │    │ |
|  │  │  2. USB core creates struct device                               │    │ |
|  │  │  3. Bus matching finds usb-storage driver                        │    │ |
|  │  │  4. Driver's probe() called                                      │    │ |
|  │  │  5. udev notifies userspace                                      │    │ |
|  │  │  6. Device ready to use!                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  User unplugs:                                                   │    │ |
|  │  │  1. USB hub detects removal                                      │    │ |
|  │  │  2. Driver's remove() called                                     │    │ |
|  │  │  3. Resources cleaned up                                         │    │ |
|  │  │  4. struct device freed                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  FAILURE 3: NO POWER MANAGEMENT                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Without driver model: Every driver does its own thing                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Suspend sequence:                                               │    │ |
|  │  │  1. Tell e1000 to suspend... but it depends on PCI              │    │ |
|  │  │  2. PCI suspended first? e1000 suspend fails!                    │    │ |
|  │  │  3. Order of operations is random                                │    │ |
|  │  │  4. System hangs or corrupts state                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  With driver model: Ordered by device tree                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Device hierarchy:                                               │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────┐    │    │ |
|  │  │  │              PCI bus (parent)                            │    │    │ |
|  │  │  │                    │                                     │    │    │ |
|  │  │  │        ┌───────────┴───────────┐                         │    │    │ |
|  │  │  │        ▼                       ▼                         │    │    │ |
|  │  │  │   e1000 NIC (child)      AHCI (child)                    │    │    │ |
|  │  │  │        │                       │                         │    │    │ |
|  │  │  │        ▼                       ▼                         │    │    │ |
|  │  │  │   (no children)           SATA disk                      │    │    │ |
|  │  │  └─────────────────────────────────────────────────────────┘    │    │ |
|  │  │                                                                  │    │ |
|  │  │  Suspend: children first (SATA disk, e1000) → then parent (PCI)  │    │ |
|  │  │  Resume: parent first (PCI) → then children                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**没有驱动模型会失败什么**：

**失败 1：无模块化**
- 没有模型：内核必须包含所有驱动，大小 500MB+，启动需要几分钟
- 有模型：核心内核小（~5MB），驱动按需加载

**失败 2：无热插拔**
- 没有模型：用户插入 USB，没有框架发现设备，崩溃或忽略
- 有模型：USB 核心创建 struct device → 总线匹配 → probe() 调用 → udev 通知 → 设备就绪

**失败 3：无电源管理**
- 没有模型：挂起顺序随机，e1000 依赖 PCI 但 PCI 先挂起，系统挂起
- 有模型：设备层次结构，挂起时子设备先，恢复时父设备先

---

## 3. 复杂度：可维护性

```
COMPLEXITY DRIVER: MAINTAINABILITY
+=============================================================================+
|                                                                              |
|  THE MAINTAINABILITY CHALLENGE                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Scale of Linux driver ecosystem:                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • 10,000+ device drivers                                        │    │ |
|  │  │  • 1,000+ contributors per year                                  │    │ |
|  │  │  • Most contributed code is in drivers/                          │    │ |
|  │  │  • drivers/ is ~60% of kernel source                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  Without standards:                                              │    │ |
|  │  │  ├── 10,000 different initialization patterns                   │    │ |
|  │  │  ├── 10,000 different error handling approaches                 │    │ |
|  │  │  ├── 10,000 different resource cleanup patterns                 │    │ |
|  │  │  └── 10,000 ways to get it wrong                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DRIVER MODEL ENFORCES PATTERNS                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Standard driver skeleton:                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static int my_probe(struct xxx_device *dev)                     │    │ |
|  │  │  {                                                               │    │ |
|  │  │      // 1. Allocate driver-private data                          │    │ |
|  │  │      struct my_priv *priv = devm_kzalloc(&dev->dev, ...);        │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 2. Map hardware resources                                │    │ |
|  │  │      priv->regs = devm_ioremap_resource(&dev->dev, ...);         │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 3. Request interrupt                                     │    │ |
|  │  │      devm_request_irq(&dev->dev, irq, handler, ...);             │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 4. Register with subsystem                               │    │ |
|  │  │      return net_device_register(priv->netdev);                   │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static void my_remove(struct xxx_device *dev)                   │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct my_priv *priv = dev_get_drvdata(&dev->dev);          │    │ |
|  │  │      // Subsystem unregister (if not using devm)                 │    │ |
|  │  │      net_device_unregister(priv->netdev);                        │    │ |
|  │  │      // devm_* resources freed automatically!                    │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  PATTERN ENFORCED:                                               │    │ |
|  │  │  • probe() for initialization                                    │    │ |
|  │  │  • remove() for cleanup                                          │    │ |
|  │  │  • devm_* for automatic resource management                      │    │ |
|  │  │  • Standard registration/unregistration                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  BENEFITS:                                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. REVIEWABILITY                                                │    │ |
|  │  │     • Maintainers know what to look for                          │    │ |
|  │  │     • Common bugs have common patterns                           │    │ |
|  │  │     • Checklists work across drivers                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. TOOLING                                                      │    │ |
|  │  │     • Static analyzers know the patterns                         │    │ |
|  │  │     • Coccinelle scripts for bulk fixes                          │    │ |
|  │  │     • Kernel docs can be generated                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. LEARNING                                                     │    │ |
|  │  │     • New developers learn ONE pattern                           │    │ |
|  │  │     • Copy from existing drivers                                 │    │ |
|  │  │     • Subsystem-specific guides                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**可维护性复杂度**：

Linux 驱动生态规模：
- 10,000+ 设备驱动
- 每年 1,000+ 贡献者
- drivers/ 占内核源码 ~60%

没有标准：10,000 种不同的初始化模式、错误处理、资源清理模式

**驱动模型强制模式**：
- `probe()` 用于初始化
- `remove()` 用于清理
- `devm_*` 用于自动资源管理

**好处**：
1. **可审查性**：维护者知道要查找什么
2. **工具化**：静态分析器知道模式，Coccinelle 脚本批量修复
3. **学习**：新开发者学习一种模式，从现有驱动复制

---

## 4. 热插拔需求

```
HOTPLUG REQUIREMENTS
+=============================================================================+
|                                                                              |
|  MODERN SYSTEMS REQUIRE DYNAMIC HARDWARE                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Hotplug scenarios:                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  USB DEVICES                                                     │    │ |
|  │  │  • Keyboards, mice, storage, cameras...                          │    │ |
|  │  │  • Plug in anytime, unplug anytime                               │    │ |
|  │  │  • System must continue working                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  THUNDERBOLT/USB-C                                               │    │ |
|  │  │  • External GPUs                                                 │    │ |
|  │  │  • Docking stations with multiple devices                        │    │ |
|  │  │  • Entire PCI hierarchies appearing/disappearing                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  VIRTUALIZATION                                                  │    │ |
|  │  │  • Virtual devices added/removed at runtime                      │    │ |
|  │  │  • virtio devices                                                │    │ |
|  │  │  • SR-IOV VFs                                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  SERVERS                                                         │    │ |
|  │  │  • Hot-swap drives (SATA, SAS, NVMe)                             │    │ |
|  │  │  • Hot-add memory (ACPI)                                         │    │ |
|  │  │  • Hot-add CPUs                                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DRIVER MODEL HOTPLUG FLOW                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  DEVICE INSERTION:                                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ Hardware   │ ──► 1. Physical connection                       │    │ |
|  │  │  │ Event      │                                                  │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ Bus Driver │ ──► 2. Detect new device (USB hub, PCI bridge)   │    │ |
|  │  │  │ (e.g. USB  │     Enumerate, read vendor/product IDs           │    │ |
|  │  │  │  hub)      │                                                  │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ device_add │ ──► 3. Create struct device                      │    │ |
|  │  │  │            │     Add to device hierarchy                      │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ Bus Match  │ ──► 4. Find matching driver                      │    │ |
|  │  │  │            │     Compare device IDs with driver's ID table    │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ driver_    │ ──► 5. Call driver's probe()                     │    │ |
|  │  │  │ probe()    │     Initialize hardware, register functionality  │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ uevent     │ ──► 6. Notify userspace (udev)                   │    │ |
|  │  │  │            │     Load firmware, create /dev nodes             │    │ |
|  │  │  └────────────┘                                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  DEVICE REMOVAL:                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ Hardware   │ ──► 1. Physical disconnection                    │    │ |
|  │  │  │ Event      │                                                  │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ Bus Driver │ ──► 2. Detect removal                            │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ driver_    │ ──► 3. Call driver's remove()                    │    │ |
|  │  │  │ remove()   │     Stop operations, release resources           │    │ |
|  │  │  └─────┬──────┘                                                  │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  ┌────────────┐                                                  │    │ |
|  │  │  │ device_del │ ──► 4. Remove from hierarchy                     │    │ |
|  │  │  │ uevent     │     Notify userspace, cleanup /dev               │    │ |
|  │  │  └────────────┘                                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**热插拔需求**：

现代系统需要动态硬件：
- **USB 设备**：键盘、鼠标、存储、摄像头，随时插拔
- **Thunderbolt/USB-C**：外接 GPU、坞站、整个 PCI 层次出现/消失
- **虚拟化**：运行时添加/移除虚拟设备，virtio、SR-IOV VF
- **服务器**：热插拔驱动器、热添加内存、热添加 CPU

**驱动模型热插拔流程**：

**设备插入**：
1. 物理连接
2. 总线驱动检测新设备（USB hub、PCI 桥）
3. `device_add()` 创建 struct device
4. 总线匹配找到匹配驱动
5. 调用驱动的 `probe()`
6. uevent 通知用户空间（udev）

**设备移除**：
1. 物理断开
2. 总线驱动检测移除
3. 调用驱动的 `remove()`
4. `device_del()` 从层次结构移除，通知用户空间
