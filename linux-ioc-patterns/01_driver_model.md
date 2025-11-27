# Linux 驱动模型中的依赖注入模式

> 文件路径: `/tmp/linux-ioc-patterns/01_driver_model.md`
> 内核版本: Linux 3.2
> 难度: ⭐⭐⭐

---

## 1. 模式概述

Linux 驱动模型是内核中**最完整、最典型**的依赖注入实现。它实现了设备(device)、驱动(driver)、总线(bus)三者的完全解耦。

### DI/IoC 的具体表现形式

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       驱动模型的三层解耦架构                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   传统方式 (紧耦合):                                                         │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                                                                       │  │
│   │    驱动代码直接寻找和初始化设备:                                      │  │
│   │    pci_find_device(VENDOR, DEVICE, NULL);                            │  │
│   │    init_my_device(dev);                                              │  │
│   │                                                                       │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│   Linux 驱动模型 (依赖注入):                                                 │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │                                                                       │  │
│   │                    ┌─────────────┐                                   │  │
│   │                    │  bus_type   │  ◄── 定义匹配规则                 │  │
│   │                    │  (总线)     │                                   │  │
│   │                    └──────┬──────┘                                   │  │
│   │                           │                                          │  │
│   │              ┌────────────┼────────────┐                             │  │
│   │              │            │            │                             │  │
│   │              ▼            │            ▼                             │  │
│   │    ┌─────────────┐        │   ┌─────────────────┐                   │  │
│   │    │   device    │        │   │ device_driver   │                   │  │
│   │    │   (设备)    │◄───────┘──►│    (驱动)       │                   │  │
│   │    └─────────────┘  自动匹配  └─────────────────┘                   │  │
│   │                                                                       │  │
│   │    驱动只需声明支持哪些设备，框架负责匹配和绑定                        │  │
│   │                                                                       │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
│   控制反转:                                                                  │
│   • 驱动不主动寻找设备 → 框架在设备出现时通知驱动                           │
│   • 驱动不决定初始化时机 → 框架在匹配成功时调用 probe                       │
│   • 驱动不管理设备生命周期 → 框架负责创建、销毁、电源管理                   │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 设计动机

### 要解决的问题

| 问题 | 传统方式的困境 | 驱动模型的解决方案 |
|------|----------------|-------------------|
| **设备发现** | 驱动启动时扫描硬件，可能设备还未就绪 | 设备注册时触发匹配 |
| **热插拔** | 需要驱动自己监听热插拔事件 | 框架统一处理，回调驱动 |
| **资源管理** | 各驱动独立管理，容易泄漏 | 框架统一管理生命周期 |
| **电源管理** | 各驱动独立实现 suspend/resume | 框架统一编排 |
| **用户空间接口** | 各驱动自己创建 /sys 节点 | 框架自动创建 sysfs 层级 |
| **重复代码** | 每个驱动重复相似的初始化逻辑 | 通用逻辑在框架中实现 |

### 设计目标

1. **设备与驱动解耦**: 可以独立开发、独立加载
2. **支持热插拔**: 设备可以在运行时添加/移除
3. **统一的用户空间接口**: /sys/bus/\*, /sys/devices/\*
4. **可扩展的总线架构**: 容易添加新的总线类型

---

## 3. 核心数据结构

### 3.1 bus_type - 总线类型

```c
// include/linux/device.h (第 87-107 行)

struct bus_type {
    const char      *name;              // 总线名称 (如 "pci", "usb", "platform")
    struct bus_attribute    *bus_attrs; // 总线属性 (/sys/bus/xxx/)
    struct device_attribute *dev_attrs; // 设备默认属性
    struct driver_attribute *drv_attrs; // 驱动默认属性

    // ===== 依赖注入点: 总线提供匹配和操作回调 =====
    int (*match)(struct device *dev, struct device_driver *drv);  // 匹配函数
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env); // 热插拔事件
    int (*probe)(struct device *dev);    // 设备探测
    int (*remove)(struct device *dev);   // 设备移除
    void (*shutdown)(struct device *dev); // 关机处理

    int (*suspend)(struct device *dev, pm_message_t state); // 挂起
    int (*resume)(struct device *dev);   // 恢复

    const struct dev_pm_ops *pm;         // 电源管理操作集
    struct iommu_ops *iommu_ops;         // IOMMU 操作集

    struct subsys_private *p;            // 私有数据
};
```

### 3.2 device - 设备

```c
// include/linux/device.h (第 604-680 行)

struct device {
    struct device       *parent;         // 父设备

    struct device_private   *p;          // 私有数据

    struct kobject kobj;                 // sysfs 对象
    const char      *init_name;          // 初始名称
    const struct device_type *type;      // 设备类型

    struct mutex        mutex;           // 设备互斥锁

    struct bus_type *bus;                // 所属总线 ◄── 关联总线
    struct device_driver *driver;        // 绑定的驱动 ◄── 绑定关系

    void        *platform_data;          // 平台数据
    struct dev_pm_info  power;           // 电源管理信息
    struct dev_power_domain *pwr_domain; // 电源域

    u64     *dma_mask;                   // DMA 掩码
    u64     coherent_dma_mask;

    struct device_dma_parameters *dma_parms;

    struct list_head    dma_pools;       // DMA 池

    struct dma_coherent_mem *dma_mem;

    struct dev_archdata archdata;        // 架构相关数据

    struct device_node  *of_node;        // Device Tree 节点

    dev_t           devt;                // 设备号

    spinlock_t      devres_lock;
    struct list_head    devres_head;     // 设备资源列表

    struct klist_node   knode_class;
    struct class        *class;          // 设备类
    const struct attribute_group **groups;

    void    (*release)(struct device *dev); // 释放函数
};
```

### 3.3 device_driver - 设备驱动

```c
// include/linux/device.h (第 192-213 行)

struct device_driver {
    const char      *name;               // 驱动名称
    struct bus_type     *bus;            // 所属总线 ◄── 关联总线

    struct module       *owner;          // 所属模块
    const char      *mod_name;

    bool suppress_bind_attrs;            // 禁用 sysfs bind/unbind

    const struct of_device_id   *of_match_table; // Device Tree 匹配表

    // ===== 依赖注入点: 驱动提供生命周期回调 =====
    int (*probe) (struct device *dev);   // 设备初始化
    int (*remove) (struct device *dev);  // 设备移除
    void (*shutdown) (struct device *dev); // 关机处理
    int (*suspend) (struct device *dev, pm_message_t state); // 挂起
    int (*resume) (struct device *dev);  // 恢复

    const struct attribute_group **groups; // 驱动属性组
    const struct dev_pm_ops *pm;         // 电源管理操作集

    struct driver_private *p;            // 私有数据
};
```

### 3.4 结构体关系图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           数据结构关系                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                        subsys_private                                        │
│                        ┌─────────────────────────────────┐                  │
│                        │  klist_devices  ──► 设备链表    │                  │
│                        │  klist_drivers  ──► 驱动链表    │                  │
│                        │  bus            ──► bus_type    │                  │
│                        └─────────────────────────────────┘                  │
│                                     │                                        │
│                                     │                                        │
│    ┌────────────────────────────────┼────────────────────────────────┐      │
│    │                                │                                │      │
│    ▼                                ▼                                ▼      │
│  bus_type                        device                      device_driver  │
│  ┌─────────────┐              ┌─────────────┐              ┌─────────────┐  │
│  │ name="pci"  │              │ kobj        │              │ name        │  │
│  │ match()     │◄─────────────│ bus ────────┼──────────────│ bus ────────┼─►│
│  │ probe()     │              │ driver ─────┼──────────────│ probe()     │  │
│  │ remove()    │              │ platform_   │              │ remove()    │  │
│  │ pm          │              │    data     │              │ pm          │  │
│  │ p ──────────┼──────────────│             │              │ p           │  │
│  └─────────────┘              └─────────────┘              └─────────────┘  │
│                                     ▲                            ▲          │
│                                     │                            │          │
│                                     │      绑定关系              │          │
│                                     └────────────────────────────┘          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. 代码流程分析

### 4.1 注册机制 - 驱动注册

```c
// drivers/base/driver.c (第 155-180 行)

int driver_register(struct device_driver *drv)
{
    int ret;
    struct device_driver *other;

    // 检查是否已注册同名驱动
    other = driver_find(drv->name, drv->bus);
    if (other) {
        printk(KERN_ERR "Error: Driver '%s' is already registered, "
            "aborting...\n", drv->name);
        return -EBUSY;
    }

    // 添加到总线
    ret = bus_add_driver(drv);
    if (ret)
        return ret;

    // 添加属性组
    ret = driver_add_groups(drv, drv->groups);
    if (ret)
        bus_remove_driver(drv);

    return ret;
}
EXPORT_SYMBOL_GPL(driver_register);
```

### 4.2 注册机制 - 设备注册

```c
// drivers/base/core.c (第 925-1020 行)

int device_add(struct device *dev)
{
    struct device *parent = NULL;
    int error = -EINVAL;

    // 获取设备引用
    dev = get_device(dev);
    if (!dev)
        goto done;

    // 设置设备名称
    if (!dev->p) {
        error = device_private_init(dev);
        if (error)
            goto done;
    }

    // 添加到 sysfs
    error = kobject_add(&dev->kobj, dev->kobj.parent, NULL);
    if (error)
        goto Error;

    // 创建 sysfs 属性
    error = device_create_file(dev, &uevent_attr);
    if (error)
        goto attrError;

    // 添加到父设备
    if (parent)
        klist_add_tail(&dev->p->knode_parent, &parent->p->klist_children);

    // 关键: 添加到总线，触发匹配
    if (dev->bus)
        bus_probe_device(dev);  // ◄── 触发与驱动的匹配

    // 发送 uevent
    kobject_uevent(&dev->kobj, KOBJ_ADD);

    return 0;
    // ...
}
```

### 4.3 调用路径 - 匹配与绑定

```c
// drivers/base/bus.c (第 455-490 行)

void bus_probe_device(struct device *dev)
{
    struct bus_type *bus = dev->bus;
    int ret;

    if (!bus)
        return;

    if (bus->p->drivers_autoprobe) {
        // 触发自动匹配
        ret = device_attach(dev);
        WARN_ON(ret < 0);
    }
}

// drivers/base/dd.c (第 226-260 行)

int device_attach(struct device *dev)
{
    int ret = 0;

    device_lock(dev);
    if (dev->driver) {
        // 已有驱动，尝试绑定
        ret = device_bind_driver(dev);
        if (ret == 0)
            ret = 1;
    } else {
        // 遍历总线上所有驱动，尝试匹配
        ret = bus_for_each_drv(dev->bus, NULL, dev, __device_attach);
    }
    device_unlock(dev);
    return ret;
}

static int __device_attach(struct device_driver *drv, void *data)
{
    struct device *dev = data;

    // 控制反转: 调用总线的 match 函数
    if (!driver_match_device(drv, dev))
        return 0;

    // 匹配成功，尝试绑定
    return driver_probe_device(drv, dev);
}
```

### 4.4 实际探测

```c
// drivers/base/dd.c (第 108-150 行)

static int really_probe(struct device *dev, struct device_driver *drv)
{
    int ret = 0;

    atomic_inc(&probe_count);
    pr_debug("bus: '%s': %s: probing driver %s with device %s\n",
         drv->bus->name, __func__, drv->name, dev_name(dev));

    // 设置 driver 指针
    dev->driver = drv;

    // 创建 sysfs 链接
    if (driver_sysfs_add(dev)) {
        goto probe_failed;
    }

    // 控制反转: 调用 probe (优先总线的，其次驱动的)
    if (dev->bus->probe) {
        ret = dev->bus->probe(dev);         // 总线的 probe
        if (ret)
            goto probe_failed;
    } else if (drv->probe) {
        ret = drv->probe(dev);              // 驱动的 probe
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

### 4.5 完整调用流程图

```
驱动加载:                                设备热插拔:
module_init(my_driver_init)             设备插入
        │                                    │
        ▼                                    ▼
driver_register(&my_driver)             device_add(&my_device)
        │                                    │
        ▼                                    ▼
bus_add_driver(drv)                     bus_probe_device(dev)
        │                                    │
        ├─► 添加到 bus->p->klist_drivers    │
        │                                    │
        ▼                                    ▼
driver_attach(drv)                      device_attach(dev)
        │                                    │
        ▼                                    ▼
bus_for_each_dev()                      bus_for_each_drv()
遍历所有设备                             遍历所有驱动
        │                                    │
        ▼                                    ▼
__driver_attach(dev, drv)               __device_attach(drv, dev)
        │                                    │
        │                                    │
        └────────────────┬───────────────────┘
                         │
                         ▼
              driver_match_device(drv, dev)
                         │
                         ▼
              drv->bus->match(dev, drv)  ◄── 依赖注入: 调用总线的匹配函数
                         │
                    匹配成功?
                    /        \
                  是          否
                  │            │
                  ▼            └── 继续下一个
         driver_probe_device()
                  │
                  ▼
            really_probe()
                  │
                  ▼
            drv->probe(dev)  ◄── 依赖注入: 调用驱动的初始化函数
```

---

## 5. 实际案例

### 案例1: PCI 总线和网卡驱动

```c
// drivers/pci/pci-driver.c

// PCI 总线定义
struct bus_type pci_bus_type = {
    .name       = "pci",
    .match      = pci_bus_match,        // 注入: 根据 vendor/device ID 匹配
    .uevent     = pci_uevent,
    .probe      = pci_device_probe,     // 注入: 调用驱动的 probe
    .remove     = pci_device_remove,
    .shutdown   = pci_device_shutdown,
    .pm         = PCI_PM_OPS_PTR,
};

// PCI 匹配函数
static int pci_bus_match(struct device *dev, struct device_driver *drv)
{
    struct pci_dev *pci_dev = to_pci_dev(dev);
    struct pci_driver *pci_drv = to_pci_driver(drv);
    const struct pci_device_id *found_id;

    // 查找匹配的设备 ID
    found_id = pci_match_device(pci_drv, pci_dev);
    if (found_id)
        return 1;

    return 0;
}

// ============ e1000 网卡驱动示例 ============
// drivers/net/ethernet/intel/e1000/e1000_main.c

// 支持的设备 ID 列表
static DEFINE_PCI_DEVICE_TABLE(e1000_pci_tbl) = {
    { PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x1000) },
    { PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x1001) },
    { PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x1004) },
    // ... 更多设备 ID
    { 0, }
};

// probe 函数 - 框架匹配成功后调用
static int __devinit e1000_probe(struct pci_dev *pdev,
                                 const struct pci_device_id *ent)
{
    struct net_device *netdev;
    struct e1000_adapter *adapter;
    int err;

    // 启用 PCI 设备
    err = pci_enable_device(pdev);
    if (err)
        return err;

    // 申请 I/O 区域
    err = pci_request_regions(pdev, e1000_driver_name);
    if (err)
        goto err_pci_reg;

    // 设置 DMA
    pci_set_master(pdev);
    err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64));
    if (err) {
        err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
        if (err)
            goto err_dma;
    }

    // 分配网络设备
    netdev = alloc_etherdev(sizeof(struct e1000_adapter));
    if (!netdev)
        goto err_alloc;

    // 映射硬件寄存器
    adapter = netdev_priv(netdev);
    adapter->hw.hw_addr = pci_iomap(pdev, 0, 0);

    // 初始化硬件
    e1000_reset_hw(&adapter->hw);

    // 注册网络设备
    err = register_netdev(netdev);
    if (err)
        goto err_register;

    return 0;

err_register:
    // 错误处理...
}

// PCI 驱动结构
static struct pci_driver e1000_driver = {
    .name       = "e1000",
    .id_table   = e1000_pci_tbl,    // 支持的设备列表
    .probe      = e1000_probe,       // 注入: 初始化函数
    .remove     = __devexit_p(e1000_remove),
    .shutdown   = e1000_shutdown,
    .driver.pm  = E1000_PM_OPS,
};

// 驱动注册
static int __init e1000_init_module(void)
{
    return pci_register_driver(&e1000_driver);
}
module_init(e1000_init_module);
```

### 案例2: Platform 总线和嵌入式设备

```c
// drivers/base/platform.c

// Platform 总线定义
struct bus_type platform_bus_type = {
    .name       = "platform",
    .dev_attrs  = platform_dev_attrs,
    .match      = platform_match,       // 注入: 按名称或 ID 表匹配
    .uevent     = platform_uevent,
    .pm         = &platform_dev_pm_ops,
};

// Platform 匹配函数
static int platform_match(struct device *dev, struct device_driver *drv)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct platform_driver *pdrv = to_platform_driver(drv);

    // 1. 尝试 OF (Device Tree) 匹配
    if (of_driver_match_device(dev, drv))
        return 1;

    // 2. 尝试 ID 表匹配
    if (pdrv->id_table)
        return platform_match_id(pdrv->id_table, pdev) != NULL;

    // 3. 按名称匹配
    return (strcmp(pdev->name, drv->name) == 0);
}

// ============ LED 驱动示例 ============
// drivers/leds/leds-gpio.c

static int __devinit gpio_led_probe(struct platform_device *pdev)
{
    struct gpio_led_platform_data *pdata = pdev->dev.platform_data;
    struct gpio_leds_priv *priv;
    int i, ret = 0;

    // 从平台数据获取 LED 配置
    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    // 初始化每个 LED
    for (i = 0; i < pdata->num_leds; i++) {
        ret = create_gpio_led(&pdata->leds[i], &priv->leds[i], &pdev->dev);
        if (ret < 0) {
            goto err;
        }
    }

    platform_set_drvdata(pdev, priv);
    return 0;

err:
    // 错误处理...
}

static struct platform_driver gpio_led_driver = {
    .probe      = gpio_led_probe,
    .remove     = __devexit_p(gpio_led_remove),
    .driver = {
        .name   = "leds-gpio",
        .owner  = THIS_MODULE,
    },
};

module_platform_driver(gpio_led_driver);

// 设备在板级文件中定义
// arch/arm/mach-xxx/board-xxx.c

static struct gpio_led my_leds[] = {
    { .name = "led1", .gpio = 10, },
    { .name = "led2", .gpio = 11, },
};

static struct gpio_led_platform_data my_led_data = {
    .leds       = my_leds,
    .num_leds   = ARRAY_SIZE(my_leds),
};

static struct platform_device my_led_device = {
    .name   = "leds-gpio",           // 与驱动名称匹配
    .id     = -1,
    .dev    = {
        .platform_data = &my_led_data,
    },
};

// 板级初始化时注册设备
static void __init my_board_init(void)
{
    platform_device_register(&my_led_device);
}
```

### 案例3: USB 总线

```c
// drivers/usb/core/driver.c

struct bus_type usb_bus_type = {
    .name       = "usb",
    .match      = usb_device_match,     // 注入: 复杂的 USB 匹配逻辑
    .uevent     = usb_uevent,
};

// USB 键盘驱动示例
// drivers/hid/usbhid/usbkbd.c

static struct usb_device_id usb_kbd_id_table[] = {
    { USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID,
                         USB_INTERFACE_SUBCLASS_BOOT,
                         USB_INTERFACE_PROTOCOL_KEYBOARD) },
    { }
};

static int usb_kbd_probe(struct usb_interface *iface,
                         const struct usb_device_id *id)
{
    struct usb_device *dev = interface_to_usbdev(iface);
    struct usb_kbd *kbd;
    struct input_dev *input_dev;

    // 分配内存
    kbd = kzalloc(sizeof(*kbd), GFP_KERNEL);
    input_dev = input_allocate_device();

    // 设置输入设备
    input_dev->name = "USB Keyboard";
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_LED);

    // 注册输入设备
    input_register_device(input_dev);

    // 设置 USB 传输
    usb_fill_int_urb(kbd->irq, dev, usb_rcvintpipe(dev, endpoint->bEndpointAddress),
                     kbd->new, 8, usb_kbd_irq, kbd, endpoint->bInterval);

    // 提交 URB
    usb_submit_urb(kbd->irq, GFP_KERNEL);

    return 0;
}

static struct usb_driver usb_kbd_driver = {
    .name       = "usbkbd",
    .probe      = usb_kbd_probe,
    .disconnect = usb_kbd_disconnect,
    .id_table   = usb_kbd_id_table,
};

module_usb_driver(usb_kbd_driver);
```

---

## 6. 优势分析

### 6.1 热插拔支持

```
设备插入事件流:

     硬件插入
         │
         ▼
    PCI/USB 控制器检测
         │
         ▼
    device_add() 注册新设备
         │
         ▼
    bus_probe_device() 触发匹配
         │
         ▼
    找到匹配的驱动，调用 probe()
         │
         ▼
    设备可用
```

### 6.2 代码复用

| 功能 | 框架实现 | 驱动实现 |
|------|----------|----------|
| sysfs 节点创建 | ✅ | |
| 电源管理调度 | ✅ | 只实现 suspend/resume |
| 热插拔通知 | ✅ | |
| 驱动绑定/解绑 | ✅ | |
| 设备枚举 | ✅ | |
| 硬件初始化 | | ✅ |
| 设备特定操作 | | ✅ |

### 6.3 灵活性

```c
// 驱动可以在不修改框架代码的情况下:
// 1. 支持新设备 - 只需添加 ID 到 id_table
static struct pci_device_id my_ids[] = {
    { PCI_DEVICE(0x1234, 0x5678) },  // 添加新设备 ID
    { },
};

// 2. 实现新功能 - 只需实现对应回调
static struct pci_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .suspend = my_suspend,  // 新增: 支持挂起
    .resume  = my_resume,   // 新增: 支持恢复
};
```

---

## 7. 对比思考

### 如果不使用驱动模型

```c
// 传统方式: 驱动直接管理设备

// 驱动初始化
int my_driver_init(void)
{
    struct pci_dev *dev = NULL;

    // 1. 手动扫描 PCI 总线
    while ((dev = pci_get_device(VENDOR_ID, DEVICE_ID, dev)) != NULL) {
        // 2. 手动初始化设备
        if (pci_enable_device(dev) < 0)
            continue;

        // 3. 手动创建 sysfs 节点
        sysfs_create_file(...);

        // 4. 手动注册中断
        request_irq(...);

        // 5. 保存设备引用
        add_to_my_device_list(dev);
    }
    return 0;
}

// 热插拔处理
void my_hotplug_handler(struct pci_dev *dev)
{
    // 需要自己实现热插拔检测
    // 需要自己管理设备列表
    // 需要自己处理并发
}

// 问题:
// 1. 代码重复 - 每个驱动都写相似逻辑
// 2. 错误容易 - sysfs、引用计数等容易出错
// 3. 热插拔困难 - 需要自己监听和处理
// 4. 电源管理复杂 - 没有统一的调度
```

---

## 8. 相关 API

### 总线注册

```c
// 注册总线类型
int bus_register(struct bus_type *bus);

// 注销总线类型
void bus_unregister(struct bus_type *bus);

// 遍历总线上的设备
int bus_for_each_dev(struct bus_type *bus, struct device *start,
                     void *data, int (*fn)(struct device *, void *));

// 遍历总线上的驱动
int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,
                     void *data, int (*fn)(struct device_driver *, void *));
```

### 设备注册

```c
// 初始化设备结构
void device_initialize(struct device *dev);

// 添加设备到系统
int device_add(struct device *dev);

// device_initialize + device_add
int device_register(struct device *dev);

// 移除设备
void device_del(struct device *dev);

// 释放设备引用
void put_device(struct device *dev);

// 获取设备引用
struct device *get_device(struct device *dev);
```

### 驱动注册

```c
// 注册驱动
int driver_register(struct device_driver *drv);

// 注销驱动
void driver_unregister(struct device_driver *drv);

// 查找驱动
struct device_driver *driver_find(const char *name, struct bus_type *bus);

// 手动触发设备匹配
int driver_attach(struct device_driver *drv);

// 手动触发驱动匹配
int device_attach(struct device *dev);
```

### 便捷宏

```c
// PCI 驱动注册
#define pci_register_driver(driver) \
    __pci_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)

// Platform 驱动注册
#define platform_driver_register(drv) \
    __platform_driver_register(drv, THIS_MODULE)

// 简化的模块初始化
#define module_pci_driver(__pci_driver) \
    module_driver(__pci_driver, pci_register_driver, pci_unregister_driver)

#define module_platform_driver(__platform_driver) \
    module_driver(__platform_driver, platform_driver_register, \
                  platform_driver_unregister)
```

---

## 🤔 思考题

1. **为什么需要 bus_type 这一层抽象？**
   - 提示: 考虑 PCI 和 USB 的匹配规则有何不同

2. **如果一个设备可以匹配多个驱动，会发生什么？**
   - 提示: 阅读 `driver_match_device` 和 `device_attach`

3. **驱动的 probe 函数返回 -EPROBE_DEFER 有什么作用？**
   - 提示: 这是处理驱动依赖的机制

4. **sysfs 中的 bind/unbind 文件是如何实现的？**
   - 提示: 阅读 `drivers/base/bus.c` 中的 `driver_bind`

---

## 📚 相关源码文件

| 文件 | 行数 | 内容 |
|------|------|------|
| `include/linux/device.h` | 1-928 | 核心数据结构定义 |
| `drivers/base/bus.c` | 1-1038 | 总线管理 |
| `drivers/base/dd.c` | 1-432 | 设备-驱动绑定 |
| `drivers/base/driver.c` | 1-290 | 驱动管理 |
| `drivers/base/core.c` | 1-1800 | 设备核心 |
| `drivers/base/platform.c` | 1-1200 | Platform 总线 |
| `drivers/pci/pci-driver.c` | 1-1400 | PCI 总线 |

