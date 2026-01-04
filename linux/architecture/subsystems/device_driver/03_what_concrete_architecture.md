# WHAT｜具体架构

## 1. 模式：基于注册的框架

```
PATTERN: REGISTRATION-BASED FRAMEWORK
+=============================================================================+
|                                                                              |
|  THE REGISTRATION PATTERN                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Instead of: framework calls specific drivers directly                   │ |
|  │  Use:        drivers register with framework, framework calls back       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  TRADITIONAL (problematic):                                      │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  void init_hardware(void)                                   │ │    │ |
|  │  │  │  {                                                          │ │    │ |
|  │  │  │      if (has_e1000()) e1000_init();                         │ │    │ |
|  │  │  │      if (has_bnx2())  bnx2_init();                          │ │    │ |
|  │  │  │      // Framework must know ALL drivers!                    │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  REGISTRATION-BASED (Linux model):                               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Driver module (e1000):                                  │ │    │ |
|  │  │  │  static int __init e1000_init(void)                         │ │    │ |
|  │  │  │  {                                                          │ │    │ |
|  │  │  │      return pci_register_driver(&e1000_driver);             │ │    │ |
|  │  │  │  }                                                          │ │    │ |
|  │  │  │  module_init(e1000_init);                                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  // Framework knows nothing about e1000 specifically!       │ │    │ |
|  │  │  │  // Only knows: "a driver registered with these IDs"        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  REGISTRATION FLOW                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────┐                                              │    │ |
|  │  │  │ Driver Module  │                                              │    │ |
|  │  │  │                │                                              │    │ |
|  │  │  │ static struct  │                                              │    │ |
|  │  │  │ pci_driver     │                                              │    │ |
|  │  │  │ my_driver = {  │                                              │    │ |
|  │  │  │   .name,       │                                              │    │ |
|  │  │  │   .id_table,   │                                              │    │ |
|  │  │  │   .probe,      │──────────────┐                               │    │ |
|  │  │  │   .remove,     │              │                               │    │ |
|  │  │  │ };             │              │                               │    │ |
|  │  │  └────────────────┘              │                               │    │ |
|  │  │          │                       │                               │    │ |
|  │  │          │ module_init()         │                               │    │ |
|  │  │          ▼                       │                               │    │ |
|  │  │  ┌────────────────────────┐      │                               │    │ |
|  │  │  │ pci_register_driver() │      │                               │    │ |
|  │  │  └───────────┬────────────┘      │                               │    │ |
|  │  │              │                   │                               │    │ |
|  │  │              ▼                   │                               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                     PCI CORE                                │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │              Driver List                              │  │ │    │ |
|  │  │  │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐               │  │ │    │ |
|  │  │  │  │  │ driver1  │ │ driver2  │ │ my_driver│ ◄── added     │  │ │    │ |
|  │  │  │  │  └──────────┘ └──────────┘ └──────────┘               │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  For each registered driver:                                │ │    │ |
|  │  │  │    For each device on bus:                                  │ │    │ |
|  │  │  │      if (driver->id_table matches device)                   │ │    │ |
|  │  │  │        driver->probe(device) ◄─────────────callback─────────┘ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  BENEFITS OF REGISTRATION                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. DECOUPLING                                                           │ |
|  │     Framework code doesn't #include driver headers                       │ |
|  │     Drivers can be built as loadable modules                             │ |
|  │                                                                          │ |
|  │  2. LATE BINDING                                                         │ |
|  │     Driver can register before or after device appears                   │ |
|  │     Matching happens whenever either arrives                             │ |
|  │                                                                          │ |
|  │  3. RUNTIME ADDITION                                                     │ |
|  │     insmod driver.ko → immediately tries to match devices                │ |
|  │     No reboot required                                                   │ |
|  │                                                                          │ |
|  │  4. UNIFORM INTERFACE                                                    │ |
|  │     All drivers implement same struct {probe, remove, ...}               │ |
|  │     Framework can iterate uniformly                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**模式：基于注册的框架**

**传统方式**（有问题）：
- 框架直接调用特定驱动：`if (has_e1000()) e1000_init();`
- 框架必须知道所有驱动！

**基于注册**（Linux 模型）：
- 驱动注册到框架：`pci_register_driver(&e1000_driver);`
- 框架对 e1000 一无所知！只知道"一个驱动注册了这些 ID"

**注册流程**：
1. 驱动模块定义 `struct pci_driver`（name, id_table, probe, remove）
2. `module_init()` 调用 `pci_register_driver()`
3. PCI 核心将驱动添加到列表
4. 对每个设备，如果 ID 匹配，调用 `driver->probe(device)`

**好处**：
1. **解耦**：框架代码不 #include 驱动头文件
2. **延迟绑定**：驱动可在设备出现之前或之后注册
3. **运行时添加**：insmod 立即尝试匹配设备
4. **统一接口**：所有驱动实现相同结构

---

## 2. 核心结构：device 和 driver

```
CORE STRUCTURES: DEVICE AND DRIVER
+=============================================================================+
|                                                                              |
|  STRUCT DEVICE (include/linux/device.h)                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct device {                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* IDENTITY */                                                  │    │ |
|  │  │  struct kobject kobj;        // sysfs representation             │    │ |
|  │  │  const char *init_name;      // Initial name                     │    │ |
|  │  │  struct device_type *type;   // Type information                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* HIERARCHY */                                                 │    │ |
|  │  │  struct device *parent;      // Parent device                    │    │ |
|  │  │  struct bus_type *bus;       // Bus this device is on            │    │ |
|  │  │  struct device_driver *driver; // Currently bound driver         │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* RESOURCES */                                                 │    │ |
|  │  │  void *platform_data;        // Platform-specific data           │    │ |
|  │  │  void *driver_data;          // Driver-private data              │    │ |
|  │  │  struct dev_pm_info power;   // Power management info            │    │ |
|  │  │  struct dev_archdata archdata; // Arch-specific data             │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* DMA */                                                       │    │ |
|  │  │  u64 *dma_mask;              // DMA mask                         │    │ |
|  │  │  u64 coherent_dma_mask;      // Coherent DMA mask                │    │ |
|  │  │  struct device_dma_parameters *dma_parms;                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* DEVICE CLASS */                                              │    │ |
|  │  │  struct class *class;        // Device class (net, block, ...)   │    │ |
|  │  │  dev_t devt;                 // Device number (major:minor)      │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* RELEASE */                                                   │    │ |
|  │  │  void (*release)(struct device *dev);  // Destructor             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  SPECIALIZATIONS:                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct pci_dev {                                                │    │ |
|  │  │      struct device dev;          // Embedded base device         │    │ |
|  │  │      unsigned short vendor;      // PCI vendor ID                │    │ |
|  │  │      unsigned short device;      // PCI device ID                │    │ |
|  │  │      unsigned int irq;           // Interrupt line               │    │ |
|  │  │      struct resource resource[]; // BARs                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct usb_device {                                             │    │ |
|  │  │      struct device dev;                                          │    │ |
|  │  │      __u16 idVendor, idProduct;                                  │    │ |
|  │  │      struct usb_device_descriptor descriptor;                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct platform_device {                                        │    │ |
|  │  │      struct device dev;                                          │    │ |
|  │  │      const char *name;                                           │    │ |
|  │  │      struct resource *resource;                                  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  STRUCT DEVICE_DRIVER (include/linux/device.h)                               |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct device_driver {                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* IDENTITY */                                                  │    │ |
|  │  │  const char *name;           // Driver name                      │    │ |
|  │  │  struct bus_type *bus;       // Bus this driver is for           │    │ |
|  │  │  struct module *owner;       // Module owning this driver        │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* LIFECYCLE CALLBACKS */                                       │    │ |
|  │  │  int (*probe)(struct device *dev);   // Bind to device           │    │ |
|  │  │  int (*remove)(struct device *dev);  // Unbind from device       │    │ |
|  │  │  void (*shutdown)(struct device *dev); // System shutdown        │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* POWER MANAGEMENT */                                          │    │ |
|  │  │  int (*suspend)(struct device *dev, pm_message_t state);         │    │ |
|  │  │  int (*resume)(struct device *dev);                              │    │ |
|  │  │  const struct dev_pm_ops *pm;  // PM operations                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │  };                                                                      │ |
|  │                                                                          │ |
|  │  SPECIALIZATIONS:                                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct pci_driver {                                             │    │ |
|  │  │      struct device_driver driver;     // Embedded base           │    │ |
|  │  │      const struct pci_device_id *id_table;  // Supported IDs     │    │ |
|  │  │      int (*probe)(struct pci_dev *dev, const pci_device_id *id); │    │ |
|  │  │      void (*remove)(struct pci_dev *dev);                        │    │ |
|  │  │      int (*suspend)(struct pci_dev *dev, pm_message_t state);    │    │ |
|  │  │      int (*resume)(struct pci_dev *dev);                         │    │ |
|  │  │      struct pci_error_handlers *err_handler;  // Error recovery  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct usb_driver {                                             │    │ |
|  │  │      struct device_driver driver;                                │    │ |
|  │  │      const struct usb_device_id *id_table;                       │    │ |
|  │  │      int (*probe)(struct usb_interface *intf, ...);              │    │ |
|  │  │      void (*disconnect)(struct usb_interface *intf);             │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct platform_driver {                                        │    │ |
|  │  │      struct device_driver driver;                                │    │ |
|  │  │      int (*probe)(struct platform_device *);                     │    │ |
|  │  │      int (*remove)(struct platform_device *);                    │    │ |
|  │  │      const struct of_device_id *of_match_table;  // Device tree  │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**核心结构**：

**struct device**（include/linux/device.h）

关键字段：
- **身份**：kobject（sysfs 表示）、名称、类型
- **层次**：parent（父设备）、bus（所在总线）、driver（当前绑定驱动）
- **资源**：platform_data、driver_data、power（电源管理）
- **DMA**：dma_mask、coherent_dma_mask
- **设备类**：class（net、block...）、devt（主:次设备号）
- **释放**：release 析构函数

**特化**：`pci_dev`、`usb_device`、`platform_device` 内嵌 `struct device`

**struct device_driver**

关键字段：
- **身份**：name、bus、owner
- **生命周期回调**：probe（绑定）、remove（解绑）、shutdown（关机）
- **电源管理**：suspend、resume、pm

**特化**：`pci_driver`、`usb_driver`、`platform_driver` 内嵌 `struct device_driver`

---

## 3. 控制流：probe 路径

```
CONTROL FLOW: PROBE PATH
+=============================================================================+
|                                                                              |
|  DEVICE DISCOVERY TO DRIVER BINDING                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  [PCI Scan at Boot]                                              │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  pci_scan_bus()                                                  │    │ |
|  │  │        │ Read config space, find devices                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  pci_scan_device()                                               │    │ |
|  │  │        │ For each found device:                                  │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  pci_alloc_dev()                                                 │    │ |
|  │  │        │ Allocate struct pci_dev                                 │    │ |
|  │  │        │ Fill vendor, device, class, etc.                        │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  pci_device_add()                                                │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  device_add(&pci_dev->dev)   ◄── Add to device model             │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ├─────────────────────────────────────────────────────────│    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  bus_add_device(dev)                                             │    │ |
|  │  │        │ Add to bus's device list                                │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  bus_probe_device(dev)       ◄── Try to find driver              │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  device_initial_probe(dev)                                       │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  __device_attach(dev)                                            │    │ |
|  │  │        │ For each driver on bus:                                 │    │ |
|  │  │        │   if (bus->match(dev, drv))                             │    │ |
|  │  │        │     try_bind(dev, drv)                                  │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  driver_probe_device(drv, dev)                                   │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  really_probe(dev, drv)      ◄── Finally call driver             │    │ |
|  │  │        │                                                         │    │ |
|  │  │        │ if (bus->probe)                                         │    │ |
|  │  │        │   ret = bus->probe(dev);                                │    │ |
|  │  │        │ else if (drv->probe)                                    │    │ |
|  │  │        │   ret = drv->probe(dev);                                │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  pci_device_probe()          ◄── PCI bus probe wrapper           │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  local_pci_probe()                                               │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  pci_drv->probe(pci_dev, id) ◄── DRIVER'S PROBE!                 │    │ |
|  │  │        │                                                         │    │ |
|  │  │        │ Driver initializes hardware                             │    │ |
|  │  │        │ Maps resources, requests IRQ                            │    │ |
|  │  │        │ Registers with subsystem                                │    │ |
|  │  │        │                                                         │    │ |
|  │  │        ▼                                                         │    │ |
|  │  │  [Device Ready]                                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  MATCHING LOGIC DETAIL                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  pci_bus_match(dev, drv):                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Device:                           Driver ID table:              │    │ |
|  │  │  ┌──────────────────────┐         ┌──────────────────────┐      │    │ |
|  │  │  │ vendor = 0x8086      │         │ { 0x8086, 0x100e }   │      │    │ |
|  │  │  │ device = 0x100e      │ ────?───│ { 0x8086, 0x100f }   │      │    │ |
|  │  │  │ class  = 0x020000    │         │ { PCI_ANY_ID, ... }  │      │    │ |
|  │  │  └──────────────────────┘         └──────────────────────┘      │    │ |
|  │  │                                                                  │    │ |
|  │  │  for each (id in drv->id_table):                                 │    │ |
|  │  │      if (id.vendor == dev->vendor || id.vendor == PCI_ANY_ID)    │    │ |
|  │  │      if (id.device == dev->device || id.device == PCI_ANY_ID)    │    │ |
|  │  │      if (id.class matches)                                       │    │ |
|  │  │          return MATCH!                                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制流：probe 路径**

从设备发现到驱动绑定：

1. **PCI 扫描**：`pci_scan_bus()` → `pci_scan_device()`
2. **分配设备**：`pci_alloc_dev()` 分配 `struct pci_dev`
3. **添加设备**：`pci_device_add()` → `device_add()` → `bus_add_device()`
4. **探测设备**：`bus_probe_device()` → `__device_attach()`
5. **匹配逻辑**：对总线上的每个驱动，调用 `bus->match(dev, drv)`
6. **绑定**：`driver_probe_device()` → `really_probe()`
7. **驱动 probe**：`pci_device_probe()` → `pci_drv->probe(pci_dev, id)`

**匹配逻辑**：
- 比较 vendor、device、class
- 支持 PCI_ANY_ID 通配符

---

## 4. 扩展点：新总线

```
EXTENSION POINTS: NEW BUSES
+=============================================================================+
|                                                                              |
|  ADDING A NEW BUS TYPE                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Example: Custom SPI-like bus                                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // 1. Define the bus type                                       │    │ |
|  │  │  static int mybus_match(struct device *dev,                      │    │ |
|  │  │                          struct device_driver *drv)              │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct mybus_device *mdev = to_mybus_device(dev);           │    │ |
|  │  │      struct mybus_driver *mdrv = to_mybus_driver(drv);           │    │ |
|  │  │      return strcmp(mdev->name, mdrv->name) == 0;                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  struct bus_type mybus_type = {                                  │    │ |
|  │  │      .name   = "mybus",                                          │    │ |
|  │  │      .match  = mybus_match,                                      │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 2. Register bus at init                                      │    │ |
|  │  │  static int __init mybus_init(void)                              │    │ |
|  │  │  {                                                               │    │ |
|  │  │      return bus_register(&mybus_type);                           │    │ |
|  │  │  }                                                               │    │ |
|  │  │  subsys_initcall(mybus_init);                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Define device and driver structures:                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // 3. Device structure                                          │    │ |
|  │  │  struct mybus_device {                                           │    │ |
|  │  │      struct device dev;       // Embed generic device            │    │ |
|  │  │      const char *name;        // For matching                    │    │ |
|  │  │      void __iomem *regs;      // Hardware registers              │    │ |
|  │  │      int irq;                                                    │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 4. Driver structure                                          │    │ |
|  │  │  struct mybus_driver {                                           │    │ |
|  │  │      struct device_driver driver;  // Embed generic driver       │    │ |
|  │  │      const char *name;             // For matching               │    │ |
|  │  │      int (*probe)(struct mybus_device *);                        │    │ |
|  │  │      void (*remove)(struct mybus_device *);                      │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 5. Helper macros                                             │    │ |
|  │  │  #define to_mybus_device(d) container_of(d, struct mybus_device, dev)│ |
|  │  │  #define to_mybus_driver(d) container_of(d, struct mybus_driver, driver)│
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Registration helpers:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  // 6. Device registration                                       │    │ |
|  │  │  int mybus_device_register(struct mybus_device *mdev)            │    │ |
|  │  │  {                                                               │    │ |
|  │  │      mdev->dev.bus = &mybus_type;                                │    │ |
|  │  │      dev_set_name(&mdev->dev, "%s", mdev->name);                 │    │ |
|  │  │      return device_register(&mdev->dev);                         │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  // 7. Driver registration                                       │    │ |
|  │  │  int mybus_driver_register(struct mybus_driver *mdrv)            │    │ |
|  │  │  {                                                               │    │ |
|  │  │      mdrv->driver.bus = &mybus_type;                             │    │ |
|  │  │      mdrv->driver.name = mdrv->name;                             │    │ |
|  │  │      return driver_register(&mdrv->driver);                      │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  RESULT: FULL INTEGRATION                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  /sys/bus/mybus/                                                         │ |
|  │  ├── devices/                                                            │ |
|  │  │   ├── sensor0 → ../../../devices/.../sensor0                          │ |
|  │  │   └── sensor1 → ../../../devices/.../sensor1                          │ |
|  │  └── drivers/                                                            │ |
|  │      └── my_sensor_driver/                                               │ |
|  │          ├── bind                                                        │ |
|  │          ├── unbind                                                      │ |
|  │          └── sensor0 → ../../../devices/.../sensor0                      │ |
|  │                                                                          │ |
|  │  Features you get for free:                                              │ |
|  │  • Automatic sysfs hierarchy                                             │ |
|  │  • bind/unbind via sysfs                                                 │ |
|  │  • uevent for udev                                                       │ |
|  │  • Power management integration                                          │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**扩展点：新总线**

添加新总线类型的步骤：

1. **定义总线类型**：`struct bus_type` 带 match 函数
2. **注册总线**：`bus_register(&mybus_type)`
3. **设备结构**：内嵌 `struct device`
4. **驱动结构**：内嵌 `struct device_driver`
5. **辅助宏**：`to_mybus_device()`、`to_mybus_driver()`
6. **设备注册**：`mybus_device_register()`
7. **驱动注册**：`mybus_driver_register()`

**结果：完全集成**
- 自动 sysfs 层次结构（/sys/bus/mybus/devices/, drivers/）
- 通过 sysfs 绑定/解绑
- uevent 用于 udev
- 电源管理集成

---

## 5. 代价：间接

```
COSTS: INDIRECTION
+=============================================================================+
|                                                                              |
|  INDIRECTION OVERHEAD                                                        |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. FUNCTION POINTER CALLS                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Direct call (no abstraction):                                   │    │ |
|  │  │    e1000_send_packet(netdev, skb);  // Compiler can inline       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Indirect call (with abstraction):                               │    │ |
|  │  │    netdev->netdev_ops->ndo_start_xmit(skb, netdev);              │    │ |
|  │  │         │              │                                         │    │ |
|  │  │         │              └── Function pointer, cannot inline       │    │ |
|  │  │         └── Pointer chase (cache miss possible)                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  COST: ~5-10 cycles per indirect call                            │    │ |
|  │  │        + potential cache miss for ops table                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. ABSTRACTION LAYERS                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Call stack for PCI driver probe:                                │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  device_add()                        // Generic layer       │ │    │ |
|  │  │  │    └── bus_probe_device()            // Bus layer           │ │    │ |
|  │  │  │          └── device_initial_probe()  // Device model        │ │    │ |
|  │  │  │                └── __device_attach() // Match & bind        │ │    │ |
|  │  │  │                      └── driver_probe_device()              │ │    │ |
|  │  │  │                            └── really_probe()               │ │    │ |
|  │  │  │                                  └── bus->probe() [pci]     │ │    │ |
|  │  │  │                                        └── pci_device_probe()│ │    │ |
|  │  │  │                                              └── local_pci_probe()│ │
|  │  │  │                                                    └── drv->probe()│ │
|  │  │  │                                                          ▲  │ │    │ |
|  │  │  │                                                          │  │ │    │ |
|  │  │  │  9 function calls to reach driver code!                  │  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  COST: Stack frames, function call overhead                      │    │ |
|  │  │        BUT: Only happens at probe, not hot path                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. MEMORY OVERHEAD                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct device:  ~600 bytes                                      │    │ |
|  │  │  struct pci_dev: ~1200 bytes (includes struct device)            │    │ |
|  │  │  struct kobject: ~100 bytes (in device)                          │    │ |
|  │  │                                                                  │    │ |
|  │  │  For system with 100 devices: ~120KB                             │    │ |
|  │  │  Negligible compared to driver benefits                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  WHEN INDIRECTION MATTERS                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  HOT PATH: Packet processing, block I/O                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  At 10 Gbps: ~14 million packets/second                          │    │ |
|  │  │  Indirect call overhead: ~10 cycles × 14M = 140M cycles/sec      │    │ |
|  │  │                                                                  │    │ |
|  │  │  Solution: Keep hot path lean                                    │    │ |
|  │  │  • ndo_start_xmit() is just ONE indirect call                    │    │ |
|  │  │  • Driver does direct operations inside                          │    │ |
|  │  │  • NAPI batching amortizes overhead                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  COLD PATH: Probe, configuration, power management                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Device probe: Once at boot or hotplug                           │    │ |
|  │  │  Configuration: sysfs writes, ioctl                              │    │ |
|  │  │  Suspend/resume: seconds, not microseconds                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  Indirection overhead: DOESN'T MATTER                            │    │ |
|  │  │  Benefits: maintainability, flexibility                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**代价：间接**

**1. 函数指针调用**
- 直接调用：编译器可内联
- 间接调用：函数指针，无法内联，可能缓存未命中
- 代价：每次间接调用 ~5-10 周期

**2. 抽象层**
- PCI 驱动 probe 调用栈：9 层函数调用到达驱动代码
- 代价：栈帧、函数调用开销
- 但：只在 probe 时发生，不在热路径

**3. 内存开销**
- struct device: ~600 字节
- struct pci_dev: ~1200 字节
- 100 个设备：~120KB，相比驱动好处可忽略

**何时间接重要**：
- **热路径**（包处理、块 I/O）：10 Gbps 时 1400 万包/秒，保持热路径精简
- **冷路径**（probe、配置、PM）：间接开销无关紧要，好处是可维护性、灵活性
