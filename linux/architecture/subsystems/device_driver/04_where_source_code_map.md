# WHERE｜源代码地图

## 1. drivers/ 目录结构

```
DRIVERS/ DIRECTORY STRUCTURE
+=============================================================================+
|                                                                              |
|  drivers/                                                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  CORE INFRASTRUCTURE (核心基础设施)                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/base/              ◄── DRIVER MODEL CORE               │    │ |
|  │  │  ├── core.c                 ◄── device_register, device_add     │    │ |
|  │  │  ├── bus.c                  ◄── bus_register, bus_for_each_drv  │    │ |
|  │  │  ├── driver.c               ◄── driver_register, driver_probe   │    │ |
|  │  │  ├── dd.c                   ◄── Device/driver binding logic     │    │ |
|  │  │  ├── platform.c             ◄── Platform bus implementation     │    │ |
|  │  │  ├── class.c                ◄── Device class management         │    │ |
|  │  │  ├── devres.c               ◄── Managed device resources (devm_*)│    │ |
|  │  │  ├── firmware.c             ◄── Firmware loading                │    │ |
|  │  │  └── power/                 ◄── Power management integration    │    │ |
|  │  │      ├── main.c                                                  │    │ |
|  │  │      └── runtime.c          ◄── Runtime PM                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  BUS IMPLEMENTATIONS (总线实现)                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/pci/               ◄── PCI bus                          │    │ |
|  │  │  ├── pci-driver.c           ◄── pci_register_driver, pci_match  │    │ |
|  │  │  ├── probe.c                ◄── PCI device discovery            │    │ |
|  │  │  ├── setup-bus.c            ◄── PCI bus setup                   │    │ |
|  │  │  └── pci.c                  ◄── Core PCI functions              │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/usb/               ◄── USB bus                          │    │ |
|  │  │  ├── core/                  ◄── USB core                         │    │ |
|  │  │  │   ├── driver.c           ◄── usb_register, usb_match         │    │ |
|  │  │  │   ├── hub.c              ◄── Hub driver (device discovery)   │    │ |
|  │  │  │   └── usb.c              ◄── USB device management           │    │ |
|  │  │  ├── host/                  ◄── USB host controllers            │    │ |
|  │  │  │   ├── xhci.c             ◄── USB 3.x                         │    │ |
|  │  │  │   ├── ehci.c             ◄── USB 2.0                         │    │ |
|  │  │  │   └── uhci.c             ◄── USB 1.x                         │    │ |
|  │  │  └── storage/               ◄── USB mass storage                │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/i2c/               ◄── I2C bus                          │    │ |
|  │  │  ├── i2c-core-base.c        ◄── i2c_register_driver             │    │ |
|  │  │  └── busses/                ◄── I2C controller drivers          │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/spi/               ◄── SPI bus                          │    │ |
|  │  │  ├── spi.c                  ◄── spi_register_driver             │    │ |
|  │  │  └── spi-*.c                ◄── SPI controller drivers          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  DEVICE DRIVERS BY SUBSYSTEM (按子系统的设备驱动)                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/net/               ◄── Network drivers                  │    │ |
|  │  │  ├── ethernet/              ◄── Ethernet NICs                    │    │ |
|  │  │  │   ├── intel/             ◄── Intel NICs                       │    │ |
|  │  │  │   │   ├── e1000/                                              │    │ |
|  │  │  │   │   ├── igb/                                                │    │ |
|  │  │  │   │   └── ixgbe/                                              │    │ |
|  │  │  │   ├── mellanox/          ◄── Mellanox NICs                    │    │ |
|  │  │  │   └── broadcom/          ◄── Broadcom NICs                    │    │ |
|  │  │  ├── wireless/              ◄── WiFi drivers                     │    │ |
|  │  │  └── virtio_net.c           ◄── Virtio network                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/block/             ◄── Block device drivers             │    │ |
|  │  │  ├── loop.c                 ◄── Loop device                      │    │ |
|  │  │  ├── nbd.c                  ◄── Network block device             │    │ |
|  │  │  └── virtio_blk.c           ◄── Virtio block                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/ata/               ◄── SATA/PATA drivers                │    │ |
|  │  │  ├── libata-core.c          ◄── Libata framework                 │    │ |
|  │  │  └── ahci.c                 ◄── AHCI controller                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/nvme/              ◄── NVMe drivers                     │    │ |
|  │  │  ├── host/                  ◄── NVMe host side                   │    │ |
|  │  │  │   └── pci.c              ◄── NVMe over PCIe                   │    │ |
|  │  │  └── target/                ◄── NVMe target (for SSDs)           │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/gpu/               ◄── Graphics drivers                 │    │ |
|  │  │  └── drm/                   ◄── Direct Rendering Manager         │    │ |
|  │  │      ├── i915/              ◄── Intel graphics                   │    │ |
|  │  │      ├── amdgpu/            ◄── AMD graphics                     │    │ |
|  │  │      └── nouveau/           ◄── NVIDIA (open source)             │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/input/             ◄── Input device drivers             │    │ |
|  │  │  ├── keyboard/                                                   │    │ |
|  │  │  ├── mouse/                                                      │    │ |
|  │  │  └── touchscreen/                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  drivers/char/              ◄── Character device drivers         │    │ |
|  │  │  drivers/tty/               ◄── TTY/serial drivers               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  INCLUDE FILES (头文件)                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  include/linux/                                                          │ |
|  │  ├── device.h              ◄── struct device, device_driver             │ |
|  │  ├── platform_device.h     ◄── Platform device/driver                   │ |
|  │  ├── pci.h                 ◄── PCI definitions                          │ |
|  │  ├── usb.h                 ◄── USB definitions                          │ |
|  │  ├── i2c.h                 ◄── I2C definitions                          │ |
|  │  ├── spi/spi.h             ◄── SPI definitions                          │ |
|  │  ├── mod_devicetable.h     ◄── Device ID table definitions              │ |
|  │  └── pm.h                  ◄── Power management                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**drivers/ 目录结构**：

**核心基础设施**（drivers/base/）：
- `core.c`：device_register, device_add
- `bus.c`：bus_register
- `driver.c`：driver_register, driver_probe
- `dd.c`：设备/驱动绑定逻辑
- `platform.c`：Platform 总线实现
- `devres.c`：管理的设备资源（devm_*）

**总线实现**：
- `drivers/pci/`：PCI 总线
- `drivers/usb/`：USB 总线（core、host、storage）
- `drivers/i2c/`：I2C 总线
- `drivers/spi/`：SPI 总线

**按子系统的设备驱动**：
- `drivers/net/`：网络驱动
- `drivers/block/`：块设备驱动
- `drivers/ata/`：SATA/PATA
- `drivers/nvme/`：NVMe
- `drivers/gpu/`：图形驱动
- `drivers/input/`：输入设备

---

## 2. 架构锚点：struct device

```
ARCHITECTURAL ANCHOR: STRUCT DEVICE
+=============================================================================+
|                                                                              |
|  WHERE TO FIND STRUCT DEVICE                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Definition: include/linux/device.h                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct device {                                                 │    │ |
|  │  │      struct kobject kobj;                                        │    │ |
|  │  │      struct device *parent;                                      │    │ |
|  │  │      struct device_private *p;                                   │    │ |
|  │  │      const char *init_name;                                      │    │ |
|  │  │      const struct device_type *type;                             │    │ |
|  │  │      struct bus_type *bus;                                       │    │ |
|  │  │      struct device_driver *driver;                               │    │ |
|  │  │      void *platform_data;                                        │    │ |
|  │  │      void *driver_data;                                          │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Implementation: drivers/base/core.c                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Key functions:                                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  device_register()     - Add device to model (init + add)        │    │ |
|  │  │  device_initialize()   - Initialize device struct                │    │ |
|  │  │  device_add()          - Add initialized device                  │    │ |
|  │  │  device_del()          - Remove device                           │    │ |
|  │  │  device_unregister()   - Del + put                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  get_device()          - Increment refcount                      │    │ |
|  │  │  put_device()          - Decrement refcount                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  dev_set_drvdata()     - Set driver private data                 │    │ |
|  │  │  dev_get_drvdata()     - Get driver private data                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  dev_set_name()        - Set device name                         │    │ |
|  │  │  dev_name()            - Get device name                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DEVICE LIFECYCLE                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ALLOCATION                                                      │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  // For PCI:                                              │   │    │ |
|  │  │  │  pdev = pci_alloc_dev(bus);                               │   │    │ |
|  │  │  │    └── device_initialize(&pdev->dev);                     │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  // For platform:                                         │   │    │ |
|  │  │  │  pdev = platform_device_alloc(name, id);                  │   │    │ |
|  │  │  │    └── device_initialize(&pdev->dev);                     │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  REGISTRATION                                                    │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  device_add(&pdev->dev);                                  │   │    │ |
|  │  │  │    ├── kobject_add()          // sysfs entry              │   │    │ |
|  │  │  │    ├── bus_add_device()       // add to bus               │   │    │ |
|  │  │  │    ├── bus_probe_device()     // try to bind driver       │   │    │ |
|  │  │  │    └── kobject_uevent(&dev->kobj, KOBJ_ADD)  // udev      │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  ACTIVE (driver bound)                                           │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  dev->driver = drv;  // Driver bound                      │   │    │ |
|  │  │  │  Driver uses device normally                              │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  REMOVAL                                                         │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  device_del(&pdev->dev);                                  │   │    │ |
|  │  │  │    ├── device_release_driver()  // call remove()          │   │    │ |
|  │  │  │    ├── bus_remove_device()      // remove from bus        │   │    │ |
|  │  │  │    ├── kobject_uevent(KOBJ_REMOVE)                        │   │    │ |
|  │  │  │    └── kobject_del()            // remove sysfs           │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  DESTRUCTION                                                     │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  put_device(&pdev->dev);                                  │   │    │ |
|  │  │  │    └── When refcount == 0:                                │   │    │ |
|  │  │  │          dev->release(dev);  // or type->release()        │   │    │ |
|  │  │  │          // Frees device struct                           │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**架构锚点：struct device**

**位置**：
- 定义：`include/linux/device.h`
- 实现：`drivers/base/core.c`

**关键函数**：
- `device_register()`：添加设备到模型
- `device_add()`：添加已初始化的设备
- `device_del()`：移除设备
- `get_device()/put_device()`：引用计数
- `dev_set_drvdata()/dev_get_drvdata()`：驱动私有数据

**设备生命周期**：
1. **分配**：`pci_alloc_dev()` 或 `platform_device_alloc()`
2. **注册**：`device_add()` → kobject_add、bus_add_device、bus_probe_device、uevent
3. **活动**：驱动绑定，正常使用
4. **移除**：`device_del()` → release_driver、bus_remove_device、uevent、kobject_del
5. **销毁**：`put_device()` → 引用计数为 0 时调用 release

---

## 3. 控制中心：driver_register()

```
CONTROL HUB: DRIVER_REGISTER()
+=============================================================================+
|                                                                              |
|  LOCATION: drivers/base/driver.c                                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  int driver_register(struct device_driver *drv)                          │ |
|  │  {                                                                       │ |
|  │      int ret;                                                            │ |
|  │      struct device_driver *other;                                        │ |
|  │                                                                          │ |
|  │      // Check for duplicate driver                                       │ |
|  │      other = driver_find(drv->name, drv->bus);                           │ |
|  │      if (other) {                                                        │ |
|  │          // Already registered!                                          │ |
|  │          return -EBUSY;                                                  │ |
|  │      }                                                                   │ |
|  │                                                                          │ |
|  │      // Add to bus's driver list                                         │ |
|  │      ret = bus_add_driver(drv);                                          │ |
|  │      if (ret)                                                            │ |
|  │          return ret;                                                     │ |
|  │                                                                          │ |
|  │      // Create sysfs entries                                             │ |
|  │      ret = driver_add_groups(drv, drv->groups);                          │ |
|  │      if (ret)                                                            │ |
|  │          bus_remove_driver(drv);                                         │ |
|  │                                                                          │ |
|  │      return ret;                                                         │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  BUS_ADD_DRIVER (drivers/base/bus.c)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  int bus_add_driver(struct device_driver *drv)                           │ |
|  │  {                                                                       │ |
|  │      struct bus_type *bus = drv->bus;                                    │ |
|  │                                                                          │ |
|  │      // Add driver to bus's driver list                                  │ |
|  │      klist_add_tail(&priv->knode_bus, &bus->p->klist_drivers);           │ |
|  │                                                                          │ |
|  │      // Create sysfs directory                                           │ |
|  │      error = driver_create_dir(drv);                                     │ |
|  │                                                                          │ |
|  │      // KEY: Try to bind to existing devices!                            │ |
|  │      if (drv->bus->p->drivers_autoprobe)                                 │ |
|  │          error = driver_attach(drv);                                     │ |
|  │                                                                          │ |
|  │      return error;                                                       │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DRIVER_ATTACH (drivers/base/dd.c)                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  int driver_attach(struct device_driver *drv)                            │ |
|  │  {                                                                       │ |
|  │      // For each device on bus, try to bind this driver                  │ |
|  │      return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);      │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  │  static int __driver_attach(struct device *dev, void *data)              │ |
|  │  {                                                                       │ |
|  │      struct device_driver *drv = data;                                   │ |
|  │                                                                          │ |
|  │      // Does driver support this device?                                 │ |
|  │      if (!driver_match_device(drv, dev))                                 │ |
|  │          return 0;  // No match, try next device                         │ |
|  │                                                                          │ |
|  │      // Match! Try to probe                                              │ |
|  │      return driver_probe_device(drv, dev);                               │ |
|  │  }                                                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OTHER CONTROL HUBS                                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  drivers/base/dd.c:                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  driver_probe_device()  - Try to bind driver to device          │    │ |
|  │  │  really_probe()         - Actually call driver's probe          │    │ |
|  │  │  device_release_driver() - Unbind driver from device            │    │ |
|  │  │  device_driver_attach()  - Attach specific driver to device     │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  drivers/base/core.c:                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  device_add()          - Add device to model                    │    │ |
|  │  │  device_del()          - Remove device from model               │    │ |
|  │  │  device_create()       - Create device and add to class         │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  drivers/base/bus.c:                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  bus_register()        - Register new bus type                  │    │ |
|  │  │  bus_add_device()      - Add device to bus                      │    │ |
|  │  │  bus_probe_device()    - Try to find driver for device          │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  Bus-specific registration:                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │  pci_register_driver()      - drivers/pci/pci-driver.c          │    │ |
|  │  │  usb_register()             - drivers/usb/core/driver.c         │    │ |
|  │  │  platform_driver_register() - drivers/base/platform.c           │    │ |
|  │  │  i2c_add_driver()           - drivers/i2c/i2c-core-base.c       │    │ |
|  │  │  spi_register_driver()      - drivers/spi/spi.c                 │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**控制中心：driver_register()**（drivers/base/driver.c）

流程：
1. 检查重复驱动
2. `bus_add_driver()`：添加到总线驱动列表
3. 创建 sysfs 条目
4. **关键**：`driver_attach()` 尝试绑定到现有设备

**driver_attach()**（drivers/base/dd.c）：
- 对总线上的每个设备，调用 `__driver_attach()`
- 如果 `driver_match_device()` 匹配，调用 `driver_probe_device()`

**其他控制中心**：
- `drivers/base/dd.c`：driver_probe_device、really_probe、device_release_driver
- `drivers/base/core.c`：device_add、device_del、device_create
- `drivers/base/bus.c`：bus_register、bus_add_device、bus_probe_device

---

## 4. 验证方法

```
VALIDATION APPROACH
+=============================================================================+
|                                                                              |
|  METHOD 1: SYSFS INSPECTION                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # View device hierarchy                                                 │ |
|  │  ls /sys/devices/                                                        │ |
|  │                                                                          │ |
|  │  # View specific bus                                                     │ |
|  │  ls /sys/bus/pci/devices/                                                │ |
|  │  ls /sys/bus/pci/drivers/                                                │ |
|  │                                                                          │ |
|  │  # Check device-driver binding                                           │ |
|  │  cat /sys/bus/pci/devices/0000:00:1f.2/driver                            │ |
|  │  # Returns symlink to driver directory                                   │ |
|  │                                                                          │ |
|  │  # View driver's bound devices                                           │ |
|  │  ls /sys/bus/pci/drivers/ahci/                                           │ |
|  │  # Shows symlinks to bound devices                                       │ |
|  │                                                                          │ |
|  │  # Force unbind/bind                                                     │ |
|  │  echo "0000:00:1f.2" > /sys/bus/pci/drivers/ahci/unbind                  │ |
|  │  echo "0000:00:1f.2" > /sys/bus/pci/drivers/ahci/bind                    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 2: DMESG AND KERNEL LOG                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Watch driver messages                                                 │ |
|  │  dmesg | grep -i "probe\|driver\|bound"                                  │ |
|  │                                                                          │ |
|  │  # Watch for new devices                                                 │ |
|  │  dmesg -w                                                                │ |
|  │                                                                          │ |
|  │  # Example output:                                                       │ |
|  │  ahci 0000:00:1f.2: AHCI 0001.0000 32 slots 6 ports 6 Gbps               │ |
|  │  scsi host0: ahci                                                        │ |
|  │  ata1: SATA max UDMA/133 abar m2048@0xdf22e000 port...                   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 3: LSPCI/LSUSB                                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Show PCI devices with drivers                                         │ |
|  │  lspci -k                                                                │ |
|  │  # 00:1f.2 SATA controller: Intel Corporation...                         │ |
|  │  #     Kernel driver in use: ahci                                        │ |
|  │                                                                          │ |
|  │  # Show USB devices with drivers                                         │ |
|  │  lsusb -t                                                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 4: FTRACE                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Trace driver probe                                                    │ |
|  │  echo 'driver_probe_device' > /sys/kernel/debug/tracing/set_ftrace_filter│ |
|  │  echo function > /sys/kernel/debug/tracing/current_tracer                │ |
|  │  echo 1 > /sys/kernel/debug/tracing/tracing_on                           │ |
|  │                                                                          │ |
|  │  # Trigger hotplug (plug in USB device)                                  │ |
|  │                                                                          │ |
|  │  cat /sys/kernel/debug/tracing/trace                                     │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  METHOD 5: UDEVADM                                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  # Monitor uevents                                                       │ |
|  │  udevadm monitor                                                         │ |
|  │                                                                          │ |
|  │  # Show device info                                                      │ |
|  │  udevadm info /dev/sda                                                   │ |
|  │                                                                          │ |
|  │  # Trigger re-probe                                                      │ |
|  │  udevadm trigger                                                         │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**验证方法**：

**方法 1：sysfs 检查**
- `/sys/bus/pci/devices/`：查看设备
- `/sys/bus/pci/drivers/`：查看驱动
- 检查设备-驱动绑定
- 强制解绑/绑定

**方法 2：dmesg 和内核日志**
- `dmesg | grep -i "probe\|driver\|bound"`

**方法 3：lspci/lsusb**
- `lspci -k`：显示 PCI 设备及其驱动
- `lsusb -t`：USB 树

**方法 4：ftrace**
- 跟踪 `driver_probe_device`

**方法 5：udevadm**
- `udevadm monitor`：监控 uevent
- `udevadm trigger`：触发重新探测

---

## 5. 阅读顺序

```
RECOMMENDED READING ORDER
+=============================================================================+
|                                                                              |
|  LEVEL 1: CORE ABSTRACTIONS (核心抽象)                                       |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. include/linux/device.h                                               │ |
|  │     • struct device - the core device                                    │ |
|  │     • struct device_driver - the core driver                             │ |
|  │     • struct bus_type - the bus abstraction                              │ |
|  │                                                                          │ |
|  │  2. drivers/base/core.c                                                  │ |
|  │     • device_register(), device_add()                                    │ |
|  │     • Understand device lifecycle                                        │ |
|  │                                                                          │ |
|  │  3. drivers/base/bus.c                                                   │ |
|  │     • bus_register()                                                     │ |
|  │     • bus_add_device(), bus_add_driver()                                 │ |
|  │     • Understand bus role in matching                                    │ |
|  │                                                                          │ |
|  │  4. drivers/base/driver.c                                                │ |
|  │     • driver_register()                                                  │ |
|  │     • driver_attach()                                                    │ |
|  │                                                                          │ |
|  │  5. drivers/base/dd.c                                                    │ |
|  │     • driver_probe_device()                                              │ |
|  │     • really_probe() - where it all comes together                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 2: A CONCRETE BUS (具体总线)                                          |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  6. include/linux/pci.h                                                  │ |
|  │     • struct pci_dev                                                     │ |
|  │     • struct pci_driver                                                  │ |
|  │     • PCI_DEVICE() macro                                                 │ |
|  │                                                                          │ |
|  │  7. drivers/pci/pci-driver.c                                             │ |
|  │     • pci_register_driver()                                              │ |
|  │     • pci_bus_match() - how PCI matching works                           │ |
|  │     • pci_device_probe() - PCI probe wrapper                             │ |
|  │                                                                          │ |
|  │  8. drivers/pci/probe.c                                                  │ |
|  │     • pci_scan_device()                                                  │ |
|  │     • How devices are discovered at boot                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 3: A SIMPLE DRIVER (简单驱动)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  9. drivers/net/ethernet/intel/e1000/ (older, simpler)                   │ |
|  │     OR drivers/virtio/virtio_net.c (simpler, modern)                     │ |
|  │                                                                          │ |
|  │     • Look at module_init/exit                                           │ |
|  │     • Study probe() function                                             │ |
|  │     • Study remove() function                                            │ |
|  │     • Understand ID table                                                │ |
|  │                                                                          │ |
|  │  10. drivers/base/platform.c                                             │ |
|  │      • Platform bus for SoC devices                                      │ |
|  │      • Simpler than PCI                                                  │ |
|  │      • Good for embedded developers                                      │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LEVEL 4: ADVANCED TOPICS (高级主题)                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  11. drivers/base/devres.c                                               │ |
|  │      • devm_* managed resources                                          │ |
|  │      • Automatic cleanup on unbind                                       │ |
|  │                                                                          │ |
|  │  12. drivers/base/power/                                                 │ |
|  │      • Power management integration                                      │ |
|  │      • Suspend/resume ordering                                           │ |
|  │                                                                          │ |
|  │  13. drivers/base/class.c                                                │ |
|  │      • Device classes (net, block, tty)                                  │ |
|  │      • /dev node creation                                                │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  READING STRATEGY                                                            |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1. START WITH SYSFS                                                     │ |
|  │     Explore /sys/bus, /sys/devices, /sys/class                           │ |
|  │     Understand the structure before code                                 │ |
|  │                                                                          │ |
|  │  2. TRACE A HOTPLUG                                                      │ |
|  │     Plug in USB device                                                   │ |
|  │     Watch dmesg output                                                   │ |
|  │     Follow code path from hub.c                                          │ |
|  │                                                                          │ |
|  │  3. WRITE A MINIMAL DRIVER                                               │ |
|  │     Use platform_driver template                                         │ |
|  │     Implement probe/remove                                               │ |
|  │     Watch it bind to a device                                            │ |
|  │                                                                          │ |
|  │  4. READ DRIVER REVIEWS                                                  │ |
|  │     lore.kernel.org driver patches                                       │ |
|  │     Learn from maintainer feedback                                       │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**第 1 层：核心抽象**
1. `include/linux/device.h`：struct device、device_driver、bus_type
2. `drivers/base/core.c`：device_register、device_add
3. `drivers/base/bus.c`：bus_register、bus_add_device
4. `drivers/base/driver.c`：driver_register、driver_attach
5. `drivers/base/dd.c`：really_probe（核心绑定逻辑）

**第 2 层：具体总线**
6. `include/linux/pci.h`：pci_dev、pci_driver
7. `drivers/pci/pci-driver.c`：pci_register_driver、pci_bus_match
8. `drivers/pci/probe.c`：设备发现

**第 3 层：简单驱动**
9. `drivers/net/ethernet/intel/e1000/` 或 `virtio_net.c`
10. `drivers/base/platform.c`：Platform 总线

**第 4 层：高级主题**
11. `drivers/base/devres.c`：devm_* 管理资源
12. `drivers/base/power/`：电源管理
13. `drivers/base/class.c`：设备类

**阅读策略**：
1. 从 sysfs 开始
2. 跟踪热插拔
3. 写一个最小驱动
4. 阅读驱动审查
