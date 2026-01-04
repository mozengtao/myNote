# HOW｜架构策略

## 1. 设备/驱动分离

```
DEVICE / DRIVER SEPARATION
+=============================================================================+
|                                                                              |
|  THE FUNDAMENTAL INSIGHT                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  SEPARATE WHAT EXISTS from HOW TO USE IT                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────┐    ┌────────────────────────┐       │    │ |
|  │  │  │        DEVICE          │    │        DRIVER          │       │    │ |
|  │  │  │                        │    │                        │       │    │ |
|  │  │  │  "I am a thing that   │    │  "I know how to       │       │    │ |
|  │  │  │   exists at this      │    │   operate things      │       │    │ |
|  │  │  │   address with these  │    │   with this vendor/   │       │    │ |
|  │  │  │   resources"          │    │   product ID"         │       │    │ |
|  │  │  │                        │    │                        │       │    │ |
|  │  │  │  • Vendor ID: 0x8086   │    │  • ID table            │       │    │ |
|  │  │  │  • Product ID: 0x1234  │    │  • probe()             │       │    │ |
|  │  │  │  • IRQ: 11             │    │  • remove()            │       │    │ |
|  │  │  │  • MMIO: 0xFE000000    │    │  • PM callbacks        │       │    │ |
|  │  │  │  • Parent: PCI bus 0   │    │  • Operations          │       │    │ |
|  │  │  │                        │    │                        │       │    │ |
|  │  │  └───────────┬────────────┘    └───────────┬────────────┘       │    │ |
|  │  │              │                              │                    │    │ |
|  │  │              │                              │                    │    │ |
|  │  │              └───────────────┬──────────────┘                    │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │                  ┌───────────────────────┐                       │    │ |
|  │  │                  │      BUS CORE         │                       │    │ |
|  │  │                  │                       │                       │    │ |
|  │  │                  │  Match device to      │                       │    │ |
|  │  │                  │  driver based on IDs  │                       │    │ |
|  │  │                  │                       │                       │    │ |
|  │  │                  │  If match:            │                       │    │ |
|  │  │                  │    driver->probe(dev) │                       │    │ |
|  │  │                  │                       │                       │    │ |
|  │  │                  └───────────────────────┘                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  WHY SEPARATE?                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. DISCOVERY ORDER INDEPENDENCE                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Scenario: Module loads before hardware enumeration              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Boot sequence:                                                  │    │ |
|  │  │  T=0ms  modprobe e1000   → driver registered, no devices yet    │    │ |
|  │  │  T=50ms PCI scan starts  → device discovered                    │    │ |
|  │  │  T=51ms Bus matches      → probe() called automatically!        │    │ |
|  │  │                                                                  │    │ |
|  │  │  OR:                                                             │    │ |
|  │  │  T=0ms  PCI scan         → device discovered, no driver yet     │    │ |
|  │  │  T=50ms modprobe e1000   → driver registered                    │    │ |
|  │  │  T=51ms Bus matches      → probe() called automatically!        │    │ |
|  │  │                                                                  │    │ |
|  │  │  SAME RESULT either way!                                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. MULTIPLE DEVICES PER DRIVER                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────┐                                                 │    │ |
|  │  │  │ e1000 driver│ ◄── ONE driver module                          │    │ |
|  │  │  │             │                                                 │    │ |
|  │  │  │ ID table:   │                                                 │    │ |
|  │  │  │  8086:1234  │                                                 │    │ |
|  │  │  │  8086:1235  │                                                 │    │ |
|  │  │  │  8086:1236  │                                                 │    │ |
|  │  │  └──────┬──────┘                                                 │    │ |
|  │  │         │                                                        │    │ |
|  │  │         │ binds to all matching devices                          │    │ |
|  │  │         │                                                        │    │ |
|  │  │    ┌────┴────┬────────────┬────────────┐                         │    │ |
|  │  │    ▼         ▼            ▼            ▼                         │    │ |
|  │  │  ┌────┐   ┌────┐      ┌────┐      ┌────┐                         │    │ |
|  │  │  │eth0│   │eth1│      │eth2│      │eth3│                         │    │ |
|  │  │  │8086│   │8086│      │8086│      │8086│                         │    │ |
|  │  │  │1234│   │1234│      │1235│      │1236│                         │    │ |
|  │  │  └────┘   └────┘      └────┘      └────┘                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Each device has its own probe() call, own private data          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. HOT-PLUG FRIENDLY                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Driver already loaded:                                          │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ usb_storage driver (registered, waiting)                  │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  User plugs USB drive:                                           │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ New device appears → bus matches → probe() called         │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  │  User unplugs:                                                   │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │ Device removed → remove() called → driver stays loaded    │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**设备/驱动分离**：

**基本洞见**：分离"存在什么"和"如何使用它"

- **设备**："我是一个存在于这个地址、拥有这些资源的东西"
  - Vendor ID、Product ID、IRQ、MMIO 地址、父设备

- **驱动**："我知道如何操作这个 vendor/product ID 的设备"
  - ID 表、probe()、remove()、PM 回调、操作函数

**为什么分离**：
1. **发现顺序无关**：驱动先加载或设备先发现，结果相同
2. **一个驱动多个设备**：e1000 驱动可以绑定所有匹配设备（eth0, eth1...）
3. **热插拔友好**：驱动保持加载，设备来来去去

---

## 2. 总线抽象

```
BUS ABSTRACTION
+=============================================================================+
|                                                                              |
|  BUS AS THE MATCHMAKER                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                        struct bus_type                           │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  const char *name;           // "pci", "usb", "platform"  │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  int (*match)(dev, drv);     // Does driver support dev?  │   │    │ |
|  │  │  │  int (*probe)(dev);          // Initialize binding        │   │    │ |
|  │  │  │  int (*remove)(dev);         // Cleanup binding           │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  int (*suspend)(dev, state); // Power management          │   │    │ |
|  │  │  │  int (*resume)(dev);                                      │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  struct klist devices;       // All devices on this bus   │   │    │ |
|  │  │  │  struct klist drivers;       // All drivers for this bus  │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CONCRETE BUS EXAMPLES                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PCI BUS:                                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct bus_type pci_bus_type = {                                │    │ |
|  │  │      .name    = "pci",                                           │    │ |
|  │  │      .match   = pci_bus_match,      // Compare vendor:device     │    │ |
|  │  │      .probe   = pci_device_probe,                                │    │ |
|  │  │      .remove  = pci_device_remove,                               │    │ |
|  │  │      .suspend = pci_pm_suspend,                                  │    │ |
|  │  │      .resume  = pci_pm_resume,                                   │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Match logic:                                                    │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ Device: vendor=0x8086, device=0x100e, class=0x020000       │ │    │ |
|  │  │  │ Driver: { PCI_DEVICE(0x8086, 0x100e) }                     │ │    │ |
|  │  │  │ Match! → call driver->probe()                              │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  USB BUS:                                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct bus_type usb_bus_type = {                                │    │ |
|  │  │      .name    = "usb",                                           │    │ |
|  │  │      .match   = usb_device_match,   // Complex: class, vendor,   │    │ |
|  │  │                                      // product, interface...    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Match logic (more complex):                                     │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ Device: class=0x08 (storage), subclass=0x06 (SCSI)         │ │    │ |
|  │  │  │ Driver: { USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, ...) } │ │    │ |
|  │  │  │ Match by class! → call driver->probe()                     │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PLATFORM BUS (for SoC/ACPI devices):                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct bus_type platform_bus_type = {                           │    │ |
|  │  │      .name    = "platform",                                      │    │ |
|  │  │      .match   = platform_match,     // By name or compatible     │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Match logic (device tree or ACPI):                              │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ Device: compatible = "arm,pl011"                           │ │    │ |
|  │  │  │ Driver: .of_match = { { .compatible = "arm,pl011" } }      │ │    │ |
|  │  │  │ Match by compatible string! → call driver->probe()         │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  BUS HIERARCHY                                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Devices form a tree through buses:                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                    /sys/devices/                                 │    │ |
|  │  │                          │                                       │    │ |
|  │  │           ┌───────────────┼───────────────┐                      │    │ |
|  │  │           │               │               │                      │    │ |
|  │  │           ▼               ▼               ▼                      │    │ |
|  │  │      pci0000:00      platform      virtual                       │    │ |
|  │  │           │               │               │                      │    │ |
|  │  │    ┌──────┴──────┐        │               └── net/lo             │    │ |
|  │  │    │             │        │                                      │    │ |
|  │  │    ▼             ▼        ▼                                      │    │ |
|  │  │  0000:00:1f.2  0000:00:14.0  serial8250                          │    │ |
|  │  │  (AHCI)       (xHCI)     (UART)                                  │    │ |
|  │  │    │             │                                               │    │ |
|  │  │    ▼             ▼                                               │    │ |
|  │  │  host0         usb1                                              │    │ |
|  │  │    │             │                                               │    │ |
|  │  │    ▼             ▼                                               │    │ |
|  │  │ target0:0:0   1-1 (hub)                                          │    │ |
|  │  │    │             │                                               │    │ |
|  │  │    ▼             ▼                                               │    │ |
|  │  │  0:0:0:0      1-1.2 (keyboard)                                   │    │ |
|  │  │  (sda)                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Every device knows its parent → enables ordered PM, ordered removal     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**总线抽象**：

**总线作为媒人**：
- `struct bus_type` 包含：名称、match()、probe()、remove()、suspend()、resume()
- 管理设备列表和驱动列表

**具体总线示例**：
- **PCI 总线**：通过 vendor:device 匹配
- **USB 总线**：复杂匹配（类、厂商、产品、接口）
- **Platform 总线**：通过名称或 compatible 字符串匹配（设备树/ACPI）

**总线层次结构**：
- 设备通过总线形成树：pci0000:00 → AHCI → host0 → sda
- 每个设备知道其父设备 → 实现有序的 PM 和有序的移除

---

## 3. 生命周期：probe/remove

```
LIFECYCLE: PROBE / REMOVE
+=============================================================================+
|                                                                              |
|  PROBE: DRIVER TAKES OWNERSHIP                                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static int my_driver_probe(struct pci_dev *pdev,                │    │ |
|  │  │                              const struct pci_device_id *id)     │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct my_device *priv;                                     │    │ |
|  │  │      int ret;                                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      // PHASE 1: ALLOCATE PRIVATE DATA                          │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL); │    │ |
|  │  │      if (!priv)                                                  │    │ |
|  │  │          return -ENOMEM;                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │      pci_set_drvdata(pdev, priv);  // Store for later           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      // PHASE 2: ENABLE DEVICE                                   │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      ret = pci_enable_device(pdev);                              │    │ |
|  │  │      if (ret)                                                    │    │ |
|  │  │          return ret;  // devm_kzalloc auto-freed on error       │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      // PHASE 3: MAP RESOURCES                                   │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      priv->regs = pci_iomap(pdev, 0, 0);  // BAR0               │    │ |
|  │  │      if (!priv->regs) {                                          │    │ |
|  │  │          ret = -ENOMEM;                                          │    │ |
|  │  │          goto err_disable;                                       │    │ |
|  │  │      }                                                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      // PHASE 4: REQUEST IRQ                                     │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      ret = request_irq(pdev->irq, my_irq_handler, IRQF_SHARED,   │    │ |
|  │  │                        "my_driver", priv);                       │    │ |
|  │  │      if (ret)                                                    │    │ |
|  │  │          goto err_unmap;                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      // PHASE 5: INITIALIZE HARDWARE                             │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      writel(CTRL_RESET, priv->regs + REG_CTRL);                  │    │ |
|  │  │      // ... hardware-specific init ...                           │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      // PHASE 6: REGISTER WITH SUBSYSTEM                         │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      priv->netdev = alloc_etherdev(0);                           │    │ |
|  │  │      ret = register_netdev(priv->netdev);                        │    │ |
|  │  │      if (ret)                                                    │    │ |
|  │  │          goto err_free_irq;                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │      return 0;  // SUCCESS                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  err_free_irq:                                                   │    │ |
|  │  │      free_irq(pdev->irq, priv);                                  │    │ |
|  │  │  err_unmap:                                                      │    │ |
|  │  │      pci_iounmap(pdev, priv->regs);                              │    │ |
|  │  │  err_disable:                                                    │    │ |
|  │  │      pci_disable_device(pdev);                                   │    │ |
|  │  │      return ret;                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  REMOVE: DRIVER RELEASES OWNERSHIP                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  static void my_driver_remove(struct pci_dev *pdev)              │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct my_device *priv = pci_get_drvdata(pdev);             │    │ |
|  │  │                                                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │      // REVERSE ORDER OF PROBE!                                  │    │ |
|  │  │      // ═══════════════════════════════════════════════════════ │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 6. Unregister from subsystem                             │    │ |
|  │  │      unregister_netdev(priv->netdev);                            │    │ |
|  │  │      free_netdev(priv->netdev);                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 5. Stop hardware                                         │    │ |
|  │  │      writel(CTRL_DISABLE, priv->regs + REG_CTRL);                │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 4. Free IRQ                                              │    │ |
|  │  │      free_irq(pdev->irq, priv);                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 3. Unmap resources                                       │    │ |
|  │  │      pci_iounmap(pdev, priv->regs);                              │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 2. Disable device                                        │    │ |
|  │  │      pci_disable_device(pdev);                                   │    │ |
|  │  │                                                                  │    │ |
|  │  │      // 1. Private data freed by devm_* automatically            │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  KEY RULE: remove() is REVERSE of probe()                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**生命周期：probe/remove**

**probe()：驱动接管所有权**

阶段：
1. 分配私有数据（`devm_kzalloc`）
2. 启用设备（`pci_enable_device`）
3. 映射资源（`pci_iomap`）
4. 请求 IRQ（`request_irq`）
5. 初始化硬件
6. 向子系统注册（`register_netdev`）

错误处理：使用 goto 清理，以相反顺序释放资源

**remove()：驱动释放所有权**

**关键规则**：`remove()` 是 `probe()` 的逆序！
1. 从子系统注销
2. 停止硬件
3. 释放 IRQ
4. 取消映射资源
5. 禁用设备
6. 私有数据由 `devm_*` 自动释放

---

## 4. 所有权规则

```
OWNERSHIP RULES
+=============================================================================+
|                                                                              |
|  CLEAR OWNERSHIP AT EVERY LEVEL                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  RULE 1: BUS OWNS THE DEVICE                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct device is allocated by bus infrastructure:               │    │ |
|  │  │                                                                  │    │ |
|  │  │  PCI:      pci_scan_device()     → allocates struct pci_dev      │    │ |
|  │  │  USB:      usb_new_device()      → allocates struct usb_device   │    │ |
|  │  │  Platform: platform_device_add() → allocates struct platform_dev │    │ |
|  │  │                                                                  │    │ |
|  │  │  Driver MUST NOT free the device struct!                         │    │ |
|  │  │  Bus will free it when device is removed.                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  RULE 2: DRIVER OWNS PRIVATE DATA                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Driver allocates and frees its own data:                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  probe() {                                                       │    │ |
|  │  │      priv = kzalloc(sizeof(*priv), GFP_KERNEL);                  │    │ |
|  │  │      dev_set_drvdata(dev, priv);  // Associate with device       │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  remove() {                                                      │    │ |
|  │  │      priv = dev_get_drvdata(dev);                                │    │ |
|  │  │      kfree(priv);                 // Driver frees it             │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  OR with devm_* (managed resources):                             │    │ |
|  │  │  probe() {                                                       │    │ |
|  │  │      priv = devm_kzalloc(dev, ...);  // Auto-freed on unbind     │    │ |
|  │  │  }                                                               │    │ |
|  │  │  remove() {                                                      │    │ |
|  │  │      // Nothing to do! devm handles it                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  RULE 3: SUBSYSTEM OWNS REGISTERED OBJECTS                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  When driver registers with a subsystem:                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Net:                                                            │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ netdev = alloc_etherdev(priv_size);  // Driver allocates   │ │    │ |
|  │  │  │ register_netdev(netdev);              // Subsystem takes ref│ │    │ |
|  │  │  │ ...                                                         │ │    │ |
|  │  │  │ unregister_netdev(netdev);            // Subsystem releases │ │    │ |
|  │  │  │ free_netdev(netdev);                  // Driver frees       │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Block:                                                          │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ disk = alloc_disk(minors);                                  │ │    │ |
|  │  │  │ add_disk(disk);                       // Subsystem takes ref│ │    │ |
|  │  │  │ ...                                                         │ │    │ |
|  │  │  │ del_gendisk(disk);                    // Subsystem releases │ │    │ |
|  │  │  │ put_disk(disk);                       // Driver releases    │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  RULE 4: REFERENCE COUNTING FOR SHARED RESOURCES                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct device uses kobject with refcount:                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  get_device(dev);   // Increment refcount                        │    │ |
|  │  │  put_device(dev);   // Decrement, may free                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Example: Passing device to worker thread                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ void start_work(struct device *dev) {                       │ │    │ |
|  │  │  │     get_device(dev);  // Hold reference                     │ │    │ |
|  │  │  │     schedule_work(&my_work);                                │ │    │ |
|  │  │  │ }                                                           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ void work_func(struct work_struct *work) {                  │ │    │ |
|  │  │  │     // ... use device ...                                   │ │    │ |
|  │  │  │     put_device(dev);  // Release reference                  │ │    │ |
|  │  │  │ }                                                           │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**所有权规则**：

**规则 1：总线拥有设备**
- `struct device` 由总线基础设施分配
- 驱动不能释放设备结构！总线在设备移除时释放

**规则 2：驱动拥有私有数据**
- 驱动分配和释放自己的数据
- 使用 `dev_set_drvdata()` / `dev_get_drvdata()` 关联
- 或使用 `devm_*` 自动管理

**规则 3：子系统拥有注册对象**
- 网络：`alloc_etherdev()` → `register_netdev()` → `unregister_netdev()` → `free_netdev()`
- 块设备：`alloc_disk()` → `add_disk()` → `del_gendisk()` → `put_disk()`

**规则 4：共享资源使用引用计数**
- `get_device()` 增加引用计数
- `put_device()` 减少，可能释放
- 传递设备到工作线程时使用
