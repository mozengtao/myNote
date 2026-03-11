# Linux Driver Model 中的依赖注入 (IoC) 模式

## 概述

Linux 驱动模型是内核中最典型的 IoC 实现。设备(device)、驱动(driver)、总线(bus)三者之间的关系完全由框架层控制，驱动开发者只需"注入"自己的回调函数。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Linux Driver Model IoC 架构                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         驱动核心框架 (控制方)                         │   │
│  │                        drivers/base/                                  │   │
│  │                                                                       │   │
│  │   bus_register()  driver_register()  device_register()               │   │
│  │         │               │                  │                          │   │
│  │         ▼               ▼                  ▼                          │   │
│  │   ┌──────────────────────────────────────────────────────────────┐   │   │
│  │   │                 自动匹配与绑定机制                            │   │   │
│  │   │                                                               │   │   │
│  │   │   for each (driver, device) pair:                            │   │   │
│  │   │       if bus->match(device, driver):                         │   │   │
│  │   │           bus->probe(device) 或 driver->probe(device)        │   │   │
│  │   │                                                               │   │   │
│  │   └──────────────────────────────────────────────────────────────┘   │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│                          ▲  注入回调函数  ▲                                  │
│                          │               │                                   │
│           ┌──────────────┴───────────────┴──────────────┐                   │
│           │                                              │                   │
│  ┌────────────────────┐                      ┌────────────────────┐         │
│  │   总线类型定义      │                      │   设备驱动定义      │         │
│  │                     │                      │                     │         │
│  │  struct bus_type {  │                      │  struct device_    │         │
│  │    .match = xxx,    │                      │         driver {   │         │
│  │    .probe = xxx,    │                      │    .probe = xxx,   │         │
│  │    .remove = xxx,   │                      │    .remove = xxx,  │         │
│  │  }                  │                      │  }                  │         │
│  └────────────────────┘                      └────────────────────┘         │
│                                                                              │
│  控制反转体现:                                                               │
│  - 驱动不主动寻找设备，由框架自动匹配                                        │
│  - 驱动不控制初始化时机，由框架在匹配成功时调用 probe                        │
│  - 驱动不管理设备生命周期，由框架负责创建、销毁                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 核心代码片段

### 1. bus_type 结构 - 定义总线的"接口契约"

```c
// include/linux/device.h

struct bus_type {
    const char      *name;
    struct bus_attribute    *bus_attrs;
    struct device_attribute *dev_attrs;
    struct driver_attribute *drv_attrs;

    // 匹配函数 - 判断设备和驱动是否匹配
    int (*match)(struct device *dev, struct device_driver *drv);
    
    // 热插拔事件处理
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    
    // 探测/初始化设备
    int (*probe)(struct device *dev);
    
    // 移除设备
    int (*remove)(struct device *dev);
    
    // 关机处理
    void (*shutdown)(struct device *dev);

    // 电源管理回调
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);

    const struct dev_pm_ops *pm;
    struct iommu_ops *iommu_ops;
    struct subsys_private *p;
};
```

**说明**: `bus_type` 定义了一套"回调接口"，具体的总线(如 PCI、USB、platform)通过实现这些回调来"注入"自己的行为。

---

### 2. device_driver 结构 - 驱动的依赖注入点

```c
// include/linux/device.h

struct device_driver {
    const char      *name;
    struct bus_type     *bus;       // 关联的总线类型

    struct module       *owner;
    const char      *mod_name;

    bool suppress_bind_attrs;

    // Open Firmware 匹配表
    const struct of_device_id   *of_match_table;

    // 驱动注入的核心回调
    int (*probe) (struct device *dev);      // 设备初始化
    int (*remove) (struct device *dev);     // 设备移除
    void (*shutdown) (struct device *dev);  // 关机处理
    int (*suspend) (struct device *dev, pm_message_t state);
    int (*resume) (struct device *dev);
    
    const struct attribute_group **groups;
    const struct dev_pm_ops *pm;
    struct driver_private *p;
};
```

**说明**: 驱动开发者只需填充 `probe`、`remove` 等函数指针，框架负责在适当时机调用。

---

### 3. 框架如何执行"控制反转"

```c
// drivers/base/dd.c

static int really_probe(struct device *dev, struct device_driver *drv)
{
    int ret = 0;

    atomic_inc(&probe_count);
    pr_debug("bus: '%s': %s: probing driver %s with device %s\n",
         drv->bus->name, __func__, drv->name, dev_name(dev));

    dev->driver = drv;
    
    if (driver_sysfs_add(dev)) {
        printk(KERN_ERR "%s: driver_sysfs_add(%s) failed\n",
            __func__, dev_name(dev));
        goto probe_failed;
    }

    // 控制反转的核心: 框架决定调用哪个 probe
    if (dev->bus->probe) {
        // 优先使用总线的 probe
        ret = dev->bus->probe(dev);
        if (ret)
            goto probe_failed;
    } else if (drv->probe) {
        // 否则使用驱动的 probe
        ret = drv->probe(dev);
        if (ret)
            goto probe_failed;
    }

    driver_bound(dev);
    ret = 1;
    pr_debug("bus: '%s': %s: bound device %s to driver %s\n",
         drv->bus->name, __func__, dev_name(dev), drv->name);
    goto done;

probe_failed:
    devres_release_all(dev);
    driver_sysfs_remove(dev);
    dev->driver = NULL;
    // ...
}
```

**说明**: 驱动的 `probe` 函数何时被调用、如何被调用，完全由框架控制。驱动开发者只需"注入"probe 实现。

---

### 4. 设备-驱动匹配过程

```c
// drivers/base/bus.c

static int __driver_attach(struct device *dev, void *data)
{
    struct device_driver *drv = data;

    // 框架调用总线的 match 函数判断是否匹配
    if (!driver_match_device(drv, dev))
        return 0;

    if (dev->parent)
        device_lock(dev->parent);
    device_lock(dev);
    
    if (!dev->driver)
        driver_probe_device(drv, dev);  // 匹配成功，执行探测
    
    device_unlock(dev);
    if (dev->parent)
        device_unlock(dev->parent);

    return 0;
}

int driver_attach(struct device_driver *drv)
{
    // 遍历总线上所有设备，尝试匹配
    return bus_for_each_dev(drv->bus, NULL, drv, __driver_attach);
}
```

---

### 5. 实际驱动示例 - PCI 总线

```c
// drivers/pci/pci-driver.c

struct bus_type pci_bus_type = {
    .name       = "pci",
    .match      = pci_bus_match,        // 注入: 根据 vendor/device ID 匹配
    .uevent     = pci_uevent,           // 注入: 热插拔事件
    .probe      = pci_device_probe,     // 注入: 设备探测
    .remove     = pci_device_remove,    // 注入: 设备移除
    .shutdown   = pci_device_shutdown,
    .dev_attrs  = pci_dev_attrs,
    .bus_attrs  = pci_bus_attrs,
    .drv_attrs  = pci_drv_attrs,
    .pm         = PCI_PM_OPS_PTR,
};

// PCI 驱动注册时，只需填充 pci_driver 结构
static struct pci_driver my_pci_driver = {
    .name       = "my_pci_device",
    .id_table   = my_pci_ids,           // 支持的设备 ID 列表
    .probe      = my_probe,             // 我的初始化函数
    .remove     = my_remove,            // 我的移除函数
};

// 一行代码完成注册，其余由框架处理
module_pci_driver(my_pci_driver);
```

---

## 这样做的好处

### 1. 解耦设备与驱动

```
传统方式 (硬编码):
┌─────────────┐              ┌─────────────┐
│   驱动 A    │──直接依赖───►│   设备 A    │
└─────────────┘              └─────────────┘

IoC 方式 (依赖注入):
┌─────────────┐              ┌─────────────┐
│   驱动 A    │              │   设备 A    │
└──────┬──────┘              └──────┬──────┘
       │                            │
       │   ┌─────────────────┐      │
       └──►│    驱动核心      │◄─────┘
           │  (控制反转中心)  │
           └─────────────────┘
```

### 2. 支持热插拔

- 设备可以在运行时动态添加/移除
- 框架自动触发匹配和 probe/remove
- 驱动无需关心设备何时出现

### 3. 统一的生命周期管理

| 阶段 | 框架职责 | 驱动职责 |
|------|----------|----------|
| 注册 | 添加到总线，触发匹配 | 提供 probe 回调 |
| 匹配 | 调用 bus->match | 无 |
| 探测 | 调用 probe，建立 sysfs | 初始化硬件 |
| 运行 | 电源管理，事件通知 | 处理 I/O |
| 移除 | 调用 remove，清理资源 | 释放硬件资源 |

### 4. 代码复用

- 通用逻辑在框架中实现一次
- 不同驱动共享相同的绑定/解绑流程
- 减少重复代码，降低 bug 风险

### 5. 可测试性

- 可以模拟设备进行单元测试
- 回调函数可以独立测试
- 框架行为可预测

---

## 核心源码文件

| 文件 | 功能 |
|------|------|
| `include/linux/device.h` | bus_type, device_driver, device 定义 |
| `drivers/base/bus.c` | 总线注册、设备/驱动遍历 |
| `drivers/base/dd.c` | 设备-驱动绑定核心逻辑 |
| `drivers/base/core.c` | device 注册、sysfs 集成 |
| `drivers/base/driver.c` | driver 注册 |

---

## 总结

Linux 驱动模型的 IoC 模式:

1. **控制反转**: 框架控制设备发现、匹配、初始化时机
2. **依赖注入**: 驱动通过结构体函数指针"注入"行为
3. **好莱坞原则**: "Don't call us, we'll call you" —— 驱动不主动寻找设备，等待框架回调

