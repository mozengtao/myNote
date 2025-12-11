# Device Model and Sysfs Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel device model, sysfs, kobject, and driver framework
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. Device Model Overview (设备模型概述)

---

Q: What is the Linux device model?
A: Linux设备模型是统一的设备管理框架：

```
+------------------------------------------------------------------+
|                    Linux Device Model                             |
+------------------------------------------------------------------+
|                                                                  |
|  目的:                                                           |
|  +------------------------------------------------------------+  |
|  | 1. 统一设备管理 - 所有设备使用相同的数据结构和API            |  |
|  | 2. 电源管理 - 统一的挂起/恢复接口                           |  |
|  | 3. 用户空间接口 - 通过sysfs暴露设备信息                     |  |
|  | 4. 热插拔支持 - uevent通知用户空间                          |  |
|  | 5. 驱动绑定 - 自动匹配设备和驱动                            |  |
|  +------------------------------------------------------------+  |
|                                                                  |
|  核心组件:                                                       |
|  +-------------+  +-------------+  +-------------+               |
|  |   kobject   |  |    kset     |  |   sysfs     |               |
|  | (基础对象)   |  | (对象集合)   |  | (用户接口)   |               |
|  +-------------+  +-------------+  +-------------+               |
|                                                                  |
|  +-------------+  +-------------+  +-------------+               |
|  |    bus      |  |   device    |  |   driver    |               |
|  |   (总线)    |  |   (设备)    |  |   (驱动)    |               |
|  +-------------+  +-------------+  +-------------+               |
|                                                                  |
|  +-------------+  +-------------+                                |
|  |   class     |  |  attribute  |                                |
|  |   (类)      |  |   (属性)    |                                |
|  +-------------+  +-------------+                                |
|                                                                  |
+------------------------------------------------------------------+
```

层次关系：
```
/sys/
├── bus/              # 总线类型
│   ├── pci/
│   │   ├── devices/
│   │   └── drivers/
│   ├── usb/
│   └── platform/
├── class/            # 设备类
│   ├── net/
│   ├── block/
│   └── tty/
├── devices/          # 设备层次结构
│   ├── system/
│   ├── virtual/
│   └── pci0000:00/
├── module/           # 内核模块
├── kernel/           # 内核子系统
├── firmware/         # 固件接口
└── fs/               # 文件系统
```
[Basic]

---

Q: What are the relationships between bus, device, driver, and class?
A: 
```
+------------------------------------------------------------------+
|              Device Model Relationships                           |
+------------------------------------------------------------------+
|                                                                  |
|  Bus (总线) - 连接设备和驱动的桥梁                                |
|  +----------------------------------------------------------+   |
|  |                      PCI Bus                              |   |
|  |                                                          |   |
|  |  Devices:                    Drivers:                    |   |
|  |  +----------+  +----------+  +----------+  +----------+  |   |
|  |  | Device A |  | Device B |  | Driver X |  | Driver Y |  |   |
|  |  | (网卡)   |  | (显卡)   |  | (网卡驱动)|  | (显卡驱动)|  |   |
|  |  +----+-----+  +----+-----+  +-----+----+  +-----+----+  |   |
|  |       |             |              |              |       |   |
|  |       +-------------+--------------+--------------+       |   |
|  |                     |                                    |   |
|  |              match(device, driver)                       |   |
|  |              绑定设备和驱动                               |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  Class (类) - 相同功能设备的集合                                  |
|  +----------------------------------------------------------+   |
|  |                     Net Class                             |   |
|  |  +----------+  +----------+  +----------+                |   |
|  |  |   eth0   |  |   wlan0  |  |   lo     |                |   |
|  |  | (PCI网卡)|  | (USB网卡)|  | (虚拟)   |                |   |
|  |  +----------+  +----------+  +----------+                |   |
|  |  不同总线的设备，相同的功能分类                            |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

关系总结：
| 关系 | 说明 |
|------|------|
| Bus-Device | 设备挂在总线上 |
| Bus-Driver | 驱动注册到总线 |
| Device-Driver | 总线匹配后绑定 |
| Class-Device | 设备按功能分类 |
[Basic]

---

## 2. kobject (内核对象)

---

Q: What is a kobject?
A: kobject是设备模型的基础构建块：

```c
// include/linux/kobject.h
struct kobject {
    const char          *name;         // 对象名称
    struct list_head    entry;         // 链表节点
    struct kobject      *parent;       // 父对象
    struct kset         *kset;         // 所属集合
    struct kobj_type    *ktype;        // 类型描述
    struct kernfs_node  *sd;           // sysfs目录项
    struct kref         kref;          // 引用计数
    
    unsigned int state_initialized:1;  // 已初始化
    unsigned int state_in_sysfs:1;     // 在sysfs中
    unsigned int state_add_uevent_sent:1;
    unsigned int state_remove_uevent_sent:1;
};

// kobject类型描述
struct kobj_type {
    void (*release)(struct kobject *kobj);  // 释放函数
    const struct sysfs_ops *sysfs_ops;      // sysfs操作
    struct attribute **default_attrs;        // 默认属性（已废弃）
    const struct attribute_group **default_groups;  // 默认属性组
    const struct kobj_ns_type_operations *(*child_ns_type)(struct kobject *kobj);
    const void *(*namespace)(struct kobject *kobj);
    void (*get_ownership)(struct kobject *kobj, kuid_t *uid, kgid_t *gid);
};
```

kobject操作：
```c
// 初始化
void kobject_init(struct kobject *kobj, struct kobj_type *ktype);

// 添加到sysfs
int kobject_add(struct kobject *kobj, struct kobject *parent,
                const char *fmt, ...);

// 初始化并添加（组合）
int kobject_init_and_add(struct kobject *kobj, struct kobj_type *ktype,
                         struct kobject *parent, const char *fmt, ...);

// 引用计数
struct kobject *kobject_get(struct kobject *kobj);
void kobject_put(struct kobject *kobj);

// 从sysfs删除
void kobject_del(struct kobject *kobj);

// 创建简单kobject
struct kobject *kobject_create_and_add(const char *name,
                                       struct kobject *parent);
```
[Intermediate]

---

Q: How to create a custom kobject?
A: 
```c
#include <linux/kobject.h>
#include <linux/sysfs.h>

// 1. 定义包含kobject的结构
struct my_device {
    int value;
    struct kobject kobj;
};

// 宏：从kobject获取包含结构
#define to_my_device(obj) container_of(obj, struct my_device, kobj)

// 2. 定义属性
static ssize_t value_show(struct kobject *kobj, struct kobj_attribute *attr,
                          char *buf)
{
    struct my_device *dev = to_my_device(kobj);
    return sprintf(buf, "%d\n", dev->value);
}

static ssize_t value_store(struct kobject *kobj, struct kobj_attribute *attr,
                           const char *buf, size_t count)
{
    struct my_device *dev = to_my_device(kobj);
    sscanf(buf, "%d", &dev->value);
    return count;
}

static struct kobj_attribute value_attr = __ATTR(value, 0664, value_show, value_store);

// 3. 定义属性数组
static struct attribute *my_attrs[] = {
    &value_attr.attr,
    NULL,
};

static struct attribute_group my_group = {
    .attrs = my_attrs,
};

// 4. 定义释放函数
static void my_release(struct kobject *kobj)
{
    struct my_device *dev = to_my_device(kobj);
    kfree(dev);
}

// 5. 定义kobj_type
static struct kobj_type my_ktype = {
    .release = my_release,
    .sysfs_ops = &kobj_sysfs_ops,
    .default_groups = (const struct attribute_group *[]){ &my_group, NULL },
};

// 6. 创建和注册
static struct my_device *create_my_device(const char *name)
{
    struct my_device *dev;
    int ret;
    
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return NULL;
    
    // 初始化并添加到sysfs
    ret = kobject_init_and_add(&dev->kobj, &my_ktype,
                               kernel_kobj, "%s", name);
    if (ret) {
        kobject_put(&dev->kobj);
        return NULL;
    }
    
    // 发送uevent
    kobject_uevent(&dev->kobj, KOBJ_ADD);
    
    return dev;
}

// 7. 销毁
static void destroy_my_device(struct my_device *dev)
{
    kobject_uevent(&dev->kobj, KOBJ_REMOVE);
    kobject_del(&dev->kobj);
    kobject_put(&dev->kobj);  // 触发release
}
```
[Advanced]

---

Q: What is a kset?
A: kset是kobject的集合：

```c
// include/linux/kobject.h
struct kset {
    struct list_head list;         // kobject链表
    spinlock_t list_lock;          // 链表锁
    struct kobject kobj;           // 内嵌kobject
    const struct kset_uevent_ops *uevent_ops;  // uevent操作
};

// uevent操作
struct kset_uevent_ops {
    // 过滤uevent
    int (*filter)(struct kset *kset, struct kobject *kobj);
    // 返回uevent的子系统名
    const char *(*name)(struct kset *kset, struct kobject *kobj);
    // 添加环境变量
    int (*uevent)(struct kset *kset, struct kobject *kobj,
                  struct kobj_uevent_env *env);
};
```

kset操作：
```c
// 创建并注册kset
struct kset *kset_create_and_add(const char *name,
                                 const struct kset_uevent_ops *uevent_ops,
                                 struct kobject *parent);

// 注册已初始化的kset
int kset_register(struct kset *kset);

// 注销
void kset_unregister(struct kset *kset);

// 遍历kset中的kobject
// 需要手动遍历list
```

示例：
```c
// 创建kset
static struct kset *my_kset;

static int __init my_init(void)
{
    // 在/sys/kernel/下创建my_kset目录
    my_kset = kset_create_and_add("my_kset", NULL, kernel_kobj);
    if (!my_kset)
        return -ENOMEM;
    
    return 0;
}

// kobject加入kset
struct kobject *kobj;
kobj->kset = my_kset;  // 在kobject_init_and_add之前设置
```

kset与kobject关系：
```
/sys/kernel/
    └── my_kset/              <- kset的kobject
        ├── device1/          <- kset中的kobject
        ├── device2/
        └── device3/
```
[Intermediate]

---

## 3. Sysfs (系统文件系统)

---

Q: What is sysfs and how does it work?
A: sysfs是基于内存的虚拟文件系统，暴露内核数据结构：

```
+------------------------------------------------------------------+
|                        Sysfs Architecture                         |
+------------------------------------------------------------------+
|                                                                  |
|  User Space                                                      |
|  +----------------------------------------------------------+   |
|  | cat /sys/class/net/eth0/address                          |   |
|  | echo 1 > /sys/devices/system/cpu/cpu0/online             |   |
|  +---------------------------+------------------------------+   |
|                              |                                   |
|                              | VFS Interface                     |
|                              v                                   |
|  Kernel Space                                                    |
|  +----------------------------------------------------------+   |
|  |                    Sysfs (kernfs)                         |   |
|  |  +----------------+  +----------------+  +----------------+  |
|  |  | kernfs_node    |  | kernfs_node    |  | kernfs_node    |  |
|  |  | (目录)         |  | (文件-属性)     |  | (符号链接)     |  |
|  |  +-------+--------+  +-------+--------+  +----------------+  |
|  +----------|------------------|-----------------------------+   |
|             |                  |                                 |
|             v                  v                                 |
|  +----------------------------------------------------------+   |
|  |                    Device Model                           |   |
|  |  +----------------+  +----------------+                   |   |
|  |  |   kobject      |  |   attribute    |                   |   |
|  |  +----------------+  +----------------+                   |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

sysfs文件类型：
```c
// 1. 普通属性文件 - 读/写小量数据
-rw-r--r-- 1 root root 4096 ... /sys/class/net/eth0/mtu

// 2. 二进制属性文件 - 读/写二进制数据
-r-------- 1 root root 4096 ... /sys/firmware/acpi/tables/DSDT

// 3. 目录 - kobject
drwxr-xr-x 4 root root 0 ... /sys/class/net/eth0/

// 4. 符号链接 - 关联关系
lrwxrwxrwx 1 root root 0 ... /sys/class/net/eth0/device -> ../../../0000:02:00.0
```
[Basic]

---

Q: How to create sysfs attributes?
A: 创建sysfs属性的多种方式：

```c
/*=== 方式1：使用DEVICE_ATTR宏 ===*/
static ssize_t foo_show(struct device *dev, struct device_attribute *attr,
                        char *buf)
{
    return sprintf(buf, "%d\n", some_value);
}

static ssize_t foo_store(struct device *dev, struct device_attribute *attr,
                         const char *buf, size_t count)
{
    sscanf(buf, "%d", &some_value);
    return count;
}

// 创建属性（名称，权限，读函数，写函数）
static DEVICE_ATTR_RW(foo);           // 读写属性
static DEVICE_ATTR_RO(bar);           // 只读属性
static DEVICE_ATTR_WO(baz);           // 只写属性

// 展开为：
// static struct device_attribute dev_attr_foo = __ATTR(foo, 0644, foo_show, foo_store);


/*=== 方式2：使用属性组 ===*/
static struct attribute *my_attrs[] = {
    &dev_attr_foo.attr,
    &dev_attr_bar.attr,
    NULL,
};

// 属性组，可选name（子目录）
static struct attribute_group my_attr_group = {
    .name = "my_group",     // 可选：创建子目录
    .attrs = my_attrs,
};

// 多个属性组
static const struct attribute_group *my_attr_groups[] = {
    &my_attr_group,
    NULL,
};


/*=== 方式3：手动创建/删除 ===*/
// 创建单个文件
device_create_file(dev, &dev_attr_foo);

// 删除单个文件
device_remove_file(dev, &dev_attr_foo);

// 创建属性组
sysfs_create_group(&dev->kobj, &my_attr_group);

// 删除属性组
sysfs_remove_group(&dev->kobj, &my_attr_group);


/*=== 方式4：使用default_groups（推荐）===*/
// 在device_type或class中设置
static struct device_type my_device_type = {
    .name = "my_device",
    .groups = my_attr_groups,  // 自动创建
};

// 或在驱动中
static struct device_driver my_driver = {
    .name = "my_driver",
    .groups = my_attr_groups,
};
```
[Intermediate]

---

Q: How to create binary attributes in sysfs?
A: 二进制属性用于大块数据：

```c
// 二进制属性结构
struct bin_attribute {
    struct attribute    attr;
    size_t              size;      // 数据大小（0表示可变）
    void                *private;
    ssize_t (*read)(struct file *, struct kobject *, struct bin_attribute *,
                    char *, loff_t, size_t);
    ssize_t (*write)(struct file *, struct kobject *, struct bin_attribute *,
                     char *, loff_t, size_t);
    int (*mmap)(struct file *, struct kobject *, struct bin_attribute *,
                struct vm_area_struct *);
};

// 定义二进制属性
static ssize_t my_bin_read(struct file *filp, struct kobject *kobj,
                           struct bin_attribute *attr,
                           char *buf, loff_t off, size_t count)
{
    // 从off位置读取count字节到buf
    memcpy(buf, my_data + off, count);
    return count;
}

static ssize_t my_bin_write(struct file *filp, struct kobject *kobj,
                            struct bin_attribute *attr,
                            char *buf, loff_t off, size_t count)
{
    // 从buf写入count字节到off位置
    memcpy(my_data + off, buf, count);
    return count;
}

// 使用宏定义
static BIN_ATTR(firmware, 0644, my_bin_read, my_bin_write, 4096);
// 或
static BIN_ATTR_RO(config, 256);
static BIN_ATTR_WO(command, 64);

// 创建/删除
sysfs_create_bin_file(&kobj, &bin_attr_firmware);
sysfs_remove_bin_file(&kobj, &bin_attr_firmware);

// 在属性组中
static struct bin_attribute *my_bin_attrs[] = {
    &bin_attr_firmware,
    NULL,
};

static struct attribute_group my_group = {
    .attrs = my_attrs,
    .bin_attrs = my_bin_attrs,
};
```

常见用途：
- ACPI表 (`/sys/firmware/acpi/tables/`)
- PCI配置空间 (`/sys/bus/pci/devices/*/config`)
- 固件加载 (`/sys/class/firmware/`)
[Intermediate]

---

## 4. Device and Driver (设备和驱动)

---

Q: What is struct device?
A: `struct device`表示系统中的一个设备：

```c
// include/linux/device.h
struct device {
    struct kobject kobj;              // 内嵌kobject
    struct device *parent;            // 父设备
    
    struct device_private *p;         // 私有数据
    
    const char *init_name;            // 初始名称
    const struct device_type *type;   // 设备类型
    
    struct bus_type *bus;             // 所在总线
    struct device_driver *driver;     // 绑定的驱动
    void *platform_data;              // 平台数据
    void *driver_data;                // 驱动私有数据
    
    struct dev_pm_info power;         // 电源管理
    struct dev_pm_domain *pm_domain;
    
    u64 *dma_mask;                    // DMA掩码
    u64 coherent_dma_mask;
    u64 bus_dma_limit;
    
    const struct dma_map_ops *dma_ops;
    struct device_dma_parameters *dma_parms;
    
    struct list_head dma_pools;       // DMA池
    
    struct dev_archdata archdata;
    
    struct device_node *of_node;      // 设备树节点
    struct fwnode_handle *fwnode;     // 固件节点
    
    dev_t devt;                       // 设备号
    u32 id;
    
    spinlock_t devres_lock;
    struct list_head devres_head;     // 设备资源
    
    struct class *class;              // 所属类
    const struct attribute_group **groups;  // 属性组
    
    void (*release)(struct device *dev);  // 释放函数
    
    struct iommu_group *iommu_group;
    struct dev_iommu *iommu;
    
    // ...
};
```

设备操作：
```c
// 初始化
void device_initialize(struct device *dev);

// 添加设备
int device_add(struct device *dev);

// 初始化并添加
int device_register(struct device *dev);

// 删除设备
void device_del(struct device *dev);

// 注销（del + put）
void device_unregister(struct device *dev);

// 引用计数
struct device *get_device(struct device *dev);
void put_device(struct device *dev);

// 获取/设置驱动数据
void *dev_get_drvdata(const struct device *dev);
void dev_set_drvdata(struct device *dev, void *data);

// 打印
dev_info(dev, "Device initialized\n");
dev_err(dev, "Error: %d\n", err);
dev_dbg(dev, "Debug message\n");
dev_warn(dev, "Warning\n");
```
[Intermediate]

---

Q: What is struct device_driver?
A: `struct device_driver`表示设备驱动：

```c
// include/linux/device/driver.h
struct device_driver {
    const char          *name;        // 驱动名称
    struct bus_type     *bus;         // 所属总线
    
    struct module       *owner;       // 所属模块
    const char          *mod_name;    // 模块名
    
    bool suppress_bind_attrs;         // 禁止sysfs绑定
    enum probe_type probe_type;       // probe类型
    
    const struct of_device_id *of_match_table;     // 设备树匹配表
    const struct acpi_device_id *acpi_match_table; // ACPI匹配表
    
    // 回调函数
    int (*probe)(struct device *dev);     // 探测设备
    void (*sync_state)(struct device *dev);
    int (*remove)(struct device *dev);    // 移除设备
    void (*shutdown)(struct device *dev); // 关机
    int (*suspend)(struct device *dev, pm_message_t state);  // 挂起
    int (*resume)(struct device *dev);    // 恢复
    
    const struct attribute_group **groups;  // 属性组
    const struct attribute_group **dev_groups;  // 设备属性组
    
    const struct dev_pm_ops *pm;      // 电源管理操作
    void (*coredump)(struct device *dev);
    
    struct driver_private *p;
};
```

驱动操作：
```c
// 注册驱动
int driver_register(struct device_driver *drv);

// 注销驱动
void driver_unregister(struct device_driver *drv);

// 查找驱动
struct device_driver *driver_find(const char *name, struct bus_type *bus);

// 遍历驱动的设备
int driver_for_each_device(struct device_driver *drv, struct device *start,
                           void *data, int (*fn)(struct device *, void *));
```

probe函数模式：
```c
static int my_probe(struct device *dev)
{
    struct my_device *mydev;
    int ret;
    
    // 1. 分配私有数据
    mydev = devm_kzalloc(dev, sizeof(*mydev), GFP_KERNEL);
    if (!mydev)
        return -ENOMEM;
    
    // 2. 保存私有数据
    dev_set_drvdata(dev, mydev);
    
    // 3. 初始化硬件
    ret = init_hardware(mydev);
    if (ret)
        return ret;  // devm自动清理
    
    // 4. 注册子系统
    ret = register_subsystem(mydev);
    if (ret)
        return ret;
    
    dev_info(dev, "Device probed successfully\n");
    return 0;
}

static int my_remove(struct device *dev)
{
    struct my_device *mydev = dev_get_drvdata(dev);
    
    // 反初始化
    unregister_subsystem(mydev);
    // devm资源自动释放
    
    return 0;
}
```
[Intermediate]

---

## 5. Bus (总线)

---

Q: What is struct bus_type?
A: `struct bus_type`表示总线类型：

```c
// include/linux/device/bus.h
struct bus_type {
    const char          *name;        // 总线名称
    const char          *dev_name;    // 设备名称格式
    struct device       *dev_root;    // 总线根设备
    
    const struct attribute_group **bus_groups;    // 总线属性组
    const struct attribute_group **dev_groups;    // 设备属性组
    const struct attribute_group **drv_groups;    // 驱动属性组
    
    // 匹配设备和驱动
    int (*match)(struct device *dev, struct device_driver *drv);
    // 添加设备时的uevent
    int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
    // 设备探测
    int (*probe)(struct device *dev);
    void (*sync_state)(struct device *dev);
    int (*remove)(struct device *dev);
    void (*shutdown)(struct device *dev);
    
    int (*online)(struct device *dev);
    int (*offline)(struct device *dev);
    
    int (*suspend)(struct device *dev, pm_message_t state);
    int (*resume)(struct device *dev);
    
    int (*num_vf)(struct device *dev);
    int (*dma_configure)(struct device *dev);
    
    const struct dev_pm_ops *pm;      // 电源管理
    
    const struct iommu_ops *iommu_ops;
    
    struct subsys_private *p;
    struct lock_class_key lock_key;
    
    bool need_parent_lock;
};
```

总线操作：
```c
// 注册总线
int bus_register(struct bus_type *bus);

// 注销总线
void bus_unregister(struct bus_type *bus);

// 遍历总线上的设备
int bus_for_each_dev(struct bus_type *bus, struct device *start,
                     void *data, int (*fn)(struct device *, void *));

// 遍历总线上的驱动
int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,
                     void *data, int (*fn)(struct device_driver *, void *));

// 重新扫描总线
int bus_rescan_devices(struct bus_type *bus);
```

设备-驱动匹配流程：
```
device_add() 或 driver_register()
        |
        v
bus_probe_device() 或 driver_attach()
        |
        v
    __driver_attach() / __device_attach()
        |
        v
    driver_match_device()
        |
        +---> bus->match(dev, drv)  // 总线匹配
        |         |
        |         v
        |     匹配成功？
        |         |
        |    +----+----+
        |    |         |
        |    v         v
        |   是        否
        |    |
        v    v
    driver_probe_device()
        |
        v
    really_probe()
        |
        +---> bus->probe() 或 drv->probe()
```
[Intermediate]

---

Q: How to implement a custom bus?
A: 
```c
// 1. 定义总线类型
static int my_bus_match(struct device *dev, struct device_driver *drv)
{
    // 简单的名称匹配
    return (strcmp(dev_name(dev), drv->name) == 0);
}

static int my_bus_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    // 添加环境变量到uevent
    add_uevent_var(env, "MY_BUS_DEVICE=%s", dev_name(dev));
    return 0;
}

struct bus_type my_bus_type = {
    .name   = "my_bus",
    .match  = my_bus_match,
    .uevent = my_bus_uevent,
};
EXPORT_SYMBOL_GPL(my_bus_type);

// 2. 定义设备结构
struct my_device {
    const char *name;
    struct device dev;
};

#define to_my_device(d) container_of(d, struct my_device, dev)

// 3. 设备注册函数
int my_device_register(struct my_device *mydev)
{
    mydev->dev.bus = &my_bus_type;
    mydev->dev.release = my_device_release;
    dev_set_name(&mydev->dev, "%s", mydev->name);
    
    return device_register(&mydev->dev);
}

// 4. 定义驱动结构
struct my_driver {
    struct device_driver driver;
    int (*probe)(struct my_device *);
    int (*remove)(struct my_device *);
};

#define to_my_driver(d) container_of(d, struct my_driver, driver)

// 5. 驱动probe包装
static int my_driver_probe(struct device *dev)
{
    struct my_driver *drv = to_my_driver(dev->driver);
    struct my_device *mydev = to_my_device(dev);
    
    if (drv->probe)
        return drv->probe(mydev);
    return 0;
}

// 6. 驱动注册函数
int my_driver_register(struct my_driver *drv)
{
    drv->driver.bus = &my_bus_type;
    drv->driver.probe = my_driver_probe;
    drv->driver.remove = my_driver_remove;
    
    return driver_register(&drv->driver);
}

// 7. 模块初始化
static int __init my_bus_init(void)
{
    return bus_register(&my_bus_type);
}

static void __exit my_bus_exit(void)
{
    bus_unregister(&my_bus_type);
}
```

sysfs布局：
```
/sys/bus/my_bus/
├── devices/
│   ├── device1 -> ../../../devices/.../device1
│   └── device2 -> ../../../devices/.../device2
├── drivers/
│   └── my_driver/
│       ├── bind
│       ├── unbind
│       └── device1 -> ../../../../devices/.../device1
├── drivers_autoprobe
├── drivers_probe
└── uevent
```
[Advanced]

---

## 6. Class (类)

---

Q: What is struct class?
A: `struct class`用于按功能对设备分类：

```c
// include/linux/device/class.h
struct class {
    const char          *name;        // 类名
    struct module       *owner;       // 所属模块
    
    const struct attribute_group **class_groups;  // 类属性组
    const struct attribute_group **dev_groups;    // 设备属性组
    
    // 设备节点权限和名称
    int (*dev_uevent)(struct device *dev, struct kobj_uevent_env *env);
    char *(*devnode)(struct device *dev, umode_t *mode);
    
    // 设备生命周期
    void (*class_release)(struct class *class);
    void (*dev_release)(struct device *dev);
    
    // 电源管理
    int (*shutdown_pre)(struct device *dev);
    
    const struct kobj_ns_type_operations *ns_type;
    const void *(*namespace)(struct device *dev);
    
    void (*get_ownership)(struct device *dev, kuid_t *uid, kgid_t *gid);
    
    const struct dev_pm_ops *pm;
    
    struct subsys_private *p;
};
```

类操作：
```c
// 注册/注销类
int class_register(struct class *cls);
void class_unregister(struct class *cls);

// 创建/销毁类（简化版）
struct class *class_create(struct module *owner, const char *name);
void class_destroy(struct class *cls);

// 遍历类中的设备
int class_for_each_device(struct class *cls, struct device *start,
                          void *data, int (*fn)(struct device *, void *));

// 在类中创建设备
struct device *device_create(struct class *cls, struct device *parent,
                             dev_t devt, void *drvdata, const char *fmt, ...);
void device_destroy(struct class *cls, dev_t devt);
```

示例：字符设备使用class
```c
static struct class *my_class;
static dev_t my_devt;

static int __init my_init(void)
{
    int ret;
    
    // 1. 分配设备号
    ret = alloc_chrdev_region(&my_devt, 0, 1, "my_device");
    if (ret)
        return ret;
    
    // 2. 创建类
    my_class = class_create(THIS_MODULE, "my_class");
    if (IS_ERR(my_class)) {
        unregister_chrdev_region(my_devt, 1);
        return PTR_ERR(my_class);
    }
    
    // 3. 创建设备（在/dev下自动创建节点）
    device_create(my_class, NULL, my_devt, NULL, "my_device");
    
    return 0;
}

static void __exit my_exit(void)
{
    device_destroy(my_class, my_devt);
    class_destroy(my_class);
    unregister_chrdev_region(my_devt, 1);
}
```

sysfs布局：
```
/sys/class/my_class/
└── my_device/
    ├── dev             # 设备号
    ├── uevent
    ├── subsystem -> ../../class/my_class
    └── ... (其他属性)

/dev/my_device          # udev自动创建
```
[Intermediate]

---

## 7. Platform Device (平台设备)

---

Q: What are platform devices?
A: 平台设备是不通过标准总线枚举的设备：

```c
// include/linux/platform_device.h
struct platform_device {
    const char      *name;            // 设备名
    int             id;               // 设备ID（-1表示无ID）
    bool            id_auto;          // 自动分配ID
    struct device   dev;              // 内嵌device
    u64             platform_dma_mask;
    struct device_dma_parameters dma_parms;
    u32             num_resources;    // 资源数量
    struct resource *resource;        // 资源数组
    
    const struct platform_device_id *id_entry;
    char *driver_override;            // 强制驱动
    
    struct mfd_cell *mfd_cell;        // MFD单元
    struct pdev_archdata archdata;
};

// 平台驱动
struct platform_driver {
    int (*probe)(struct platform_device *);
    int (*remove)(struct platform_device *);
    void (*shutdown)(struct platform_device *);
    int (*suspend)(struct platform_device *, pm_message_t state);
    int (*resume)(struct platform_device *);
    struct device_driver driver;
    const struct platform_device_id *id_table;  // ID匹配表
    bool prevent_deferred_probe;
};
```

资源类型：
```c
// include/linux/ioport.h
struct resource {
    resource_size_t start;    // 起始地址
    resource_size_t end;      // 结束地址
    const char *name;         // 资源名
    unsigned long flags;      // 资源类型
    // ...
};

// 资源类型标志
IORESOURCE_IO       // I/O端口
IORESOURCE_MEM      // 内存映射
IORESOURCE_IRQ      // 中断
IORESOURCE_DMA      // DMA通道
IORESOURCE_BUS      // 总线
```

平台设备操作：
```c
// 注册/注销平台设备
int platform_device_register(struct platform_device *pdev);
void platform_device_unregister(struct platform_device *pdev);

// 分配并注册
struct platform_device *platform_device_register_simple(const char *name,
                                                        int id,
                                                        const struct resource *res,
                                                        unsigned int num);

// 获取资源
struct resource *platform_get_resource(struct platform_device *pdev,
                                       unsigned int type, unsigned int num);
struct resource *platform_get_resource_byname(struct platform_device *pdev,
                                              unsigned int type, const char *name);

// 获取IRQ
int platform_get_irq(struct platform_device *pdev, unsigned int num);
int platform_get_irq_byname(struct platform_device *pdev, const char *name);

// 注册/注销驱动
int platform_driver_register(struct platform_driver *drv);
void platform_driver_unregister(struct platform_driver *drv);

// 快捷宏
module_platform_driver(my_platform_driver);
```
[Intermediate]

---

Q: How to write a platform driver?
A: 
```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>

struct my_device {
    void __iomem *base;
    int irq;
    struct clk *clk;
};

static int my_probe(struct platform_device *pdev)
{
    struct my_device *mydev;
    struct resource *res;
    int ret;
    
    // 1. 分配私有数据
    mydev = devm_kzalloc(&pdev->dev, sizeof(*mydev), GFP_KERNEL);
    if (!mydev)
        return -ENOMEM;
    
    // 2. 获取并映射内存资源
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    mydev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(mydev->base))
        return PTR_ERR(mydev->base);
    
    // 3. 获取IRQ
    mydev->irq = platform_get_irq(pdev, 0);
    if (mydev->irq < 0)
        return mydev->irq;
    
    // 4. 获取时钟
    mydev->clk = devm_clk_get(&pdev->dev, NULL);
    if (IS_ERR(mydev->clk))
        return PTR_ERR(mydev->clk);
    
    // 5. 请求IRQ
    ret = devm_request_irq(&pdev->dev, mydev->irq, my_irq_handler,
                           0, "my_device", mydev);
    if (ret)
        return ret;
    
    // 6. 使能时钟
    clk_prepare_enable(mydev->clk);
    
    // 7. 保存私有数据
    platform_set_drvdata(pdev, mydev);
    
    dev_info(&pdev->dev, "Device probed\n");
    return 0;
}

static int my_remove(struct platform_device *pdev)
{
    struct my_device *mydev = platform_get_drvdata(pdev);
    
    clk_disable_unprepare(mydev->clk);
    // devm资源自动释放
    
    return 0;
}

// 设备树匹配表
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-device" },
    { },
};
MODULE_DEVICE_TABLE(of, my_of_match);

// ACPI匹配表（可选）
static const struct acpi_device_id my_acpi_match[] = {
    { "MYDEV001", 0 },
    { },
};
MODULE_DEVICE_TABLE(acpi, my_acpi_match);

// 平台设备ID表（传统方式）
static const struct platform_device_id my_id_table[] = {
    { "my-device", 0 },
    { },
};
MODULE_DEVICE_TABLE(platform, my_id_table);

static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my-device",
        .of_match_table = my_of_match,
        .acpi_match_table = my_acpi_match,
    },
    .id_table = my_id_table,
};

module_platform_driver(my_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("My Platform Driver");
```
[Advanced]

---

## 8. Device Tree (设备树)

---

Q: What is a device tree and how does it work?
A: 设备树描述硬件配置：

```
+------------------------------------------------------------------+
|                    Device Tree Flow                               |
+------------------------------------------------------------------+
|                                                                  |
|  .dts/.dtsi 源文件                                                |
|       |                                                          |
|       v  dtc编译                                                  |
|  .dtb 二进制文件                                                  |
|       |                                                          |
|       v  bootloader加载                                          |
|  内存中的DTB                                                      |
|       |                                                          |
|       v  内核解析                                                 |
|  device_node树                                                    |
|       |                                                          |
|       v  platform_device创建                                     |
|  设备注册到系统                                                   |
|       |                                                          |
|       v  驱动匹配                                                 |
|  probe()调用                                                      |
|                                                                  |
+------------------------------------------------------------------+
```

设备树语法：
```dts
// my_board.dts
/dts-v1/;
#include "soc.dtsi"

/ {
    model = "My Board";
    compatible = "vendor,my-board";
    
    chosen {
        bootargs = "console=ttyS0,115200";
    };
    
    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x40000000>;  // 1GB @ 0x80000000
    };
    
    my_device@10000000 {
        compatible = "vendor,my-device";
        reg = <0x10000000 0x1000>;      // 4KB @ 0x10000000
        interrupts = <GIC_SPI 42 IRQ_TYPE_LEVEL_HIGH>;
        clocks = <&clk_controller 5>;
        clock-names = "main";
        status = "okay";
        
        // 自定义属性
        my-property = <0x1234>;
        my-string = "hello";
    };
};
```

内核设备树API：
```c
// 获取属性
int of_property_read_u32(const struct device_node *np,
                         const char *propname, u32 *out_value);
int of_property_read_string(const struct device_node *np,
                            const char *propname, const char **out_string);
bool of_property_read_bool(const struct device_node *np, const char *propname);

// 遍历子节点
for_each_child_of_node(parent, child) {
    // 处理child
}

// 匹配
const struct of_device_id *of_match_device(const struct of_device_id *matches,
                                           const struct device *dev);

// 在probe中使用
static int my_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    u32 value;
    
    if (of_property_read_u32(np, "my-property", &value))
        value = 0;  // 默认值
    
    // ...
}
```
[Intermediate]

---

## 9. Uevent (热插拔事件)

---

Q: What is uevent and how does it work?
A: uevent用于设备热插拔通知：

```
+------------------------------------------------------------------+
|                    Uevent Flow                                    |
+------------------------------------------------------------------+
|                                                                  |
|  内核事件发生                                                     |
|  (device_add, device_remove等)                                   |
|       |                                                          |
|       v                                                          |
|  kobject_uevent()                                                |
|       |                                                          |
|       +---> kset->uevent_ops->filter()  过滤事件                  |
|       |                                                          |
|       +---> kset->uevent_ops->name()    获取子系统名              |
|       |                                                          |
|       +---> kset->uevent_ops->uevent()  添加环境变量              |
|       |                                                          |
|       v                                                          |
|  发送netlink消息到用户空间                                        |
|       |                                                          |
|       v                                                          |
|  udevd/systemd-udevd                                             |
|       |                                                          |
|       +---> 解析环境变量                                          |
|       +---> 匹配规则 (/etc/udev/rules.d/)                         |
|       +---> 执行动作 (创建节点, 设置权限, 运行脚本等)              |
|                                                                  |
+------------------------------------------------------------------+
```

uevent类型：
```c
// include/linux/kobject.h
enum kobject_action {
    KOBJ_ADD,       // 设备添加
    KOBJ_REMOVE,    // 设备移除
    KOBJ_CHANGE,    // 设备改变
    KOBJ_MOVE,      // 设备移动
    KOBJ_ONLINE,    // 设备上线
    KOBJ_OFFLINE,   // 设备下线
    KOBJ_BIND,      // 驱动绑定
    KOBJ_UNBIND,    // 驱动解绑
};

// 发送uevent
int kobject_uevent(struct kobject *kobj, enum kobject_action action);

// 带环境变量的uevent
int kobject_uevent_env(struct kobject *kobj, enum kobject_action action,
                       char *envp[]);
```

uevent环境变量：
```bash
# 监视uevent
udevadm monitor --property

# 典型uevent内容
ACTION=add
DEVPATH=/devices/pci0000:00/0000:00:1f.2/ata1/host0/target0:0:0/0:0:0:0/block/sda
SUBSYSTEM=block
DEVNAME=/dev/sda
DEVTYPE=disk
MAJOR=8
MINOR=0
SEQNUM=12345
```

udev规则示例：
```bash
# /etc/udev/rules.d/99-my-device.rules

# 匹配设备并设置权限
SUBSYSTEM=="my_class", MODE="0666"

# 匹配设备并创建符号链接
SUBSYSTEM=="block", KERNEL=="sd[a-z]", SYMLINK+="my_disk"

# 匹配设备并运行脚本
SUBSYSTEM=="usb", ACTION=="add", RUN+="/usr/local/bin/my_script.sh"
```
[Intermediate]

---

## 10. Device Resource Management (设备资源管理)

---

Q: What is devres (managed device resources)?
A: devres自动管理设备资源生命周期：

```
+------------------------------------------------------------------+
|                    Devres Architecture                            |
+------------------------------------------------------------------+
|                                                                  |
|  传统方式:                                                        |
|  +----------------------------------------------------------+   |
|  | probe() {                                                |   |
|  |     res1 = request_resource1();                          |   |
|  |     if (error) return err;                               |   |
|  |     res2 = request_resource2();                          |   |
|  |     if (error) goto free_res1;                           |   |
|  |     res3 = request_resource3();                          |   |
|  |     if (error) goto free_res2;                           |   |
|  |     return 0;                                            |   |
|  | free_res2:                                               |   |
|  |     release_resource2(res2);                             |   |
|  | free_res1:                                               |   |
|  |     release_resource1(res1);                             |   |
|  |     return err;                                          |   |
|  | }                                                        |   |
|  +----------------------------------------------------------+   |
|                                                                  |
|  devres方式:                                                      |
|  +----------------------------------------------------------+   |
|  | probe() {                                                |   |
|  |     res1 = devm_request_resource1();                     |   |
|  |     if (error) return err;  // 自动清理                   |   |
|  |     res2 = devm_request_resource2();                     |   |
|  |     if (error) return err;  // 自动清理res1              |   |
|  |     res3 = devm_request_resource3();                     |   |
|  |     if (error) return err;  // 自动清理res1,res2         |   |
|  |     return 0;                                            |   |
|  | }                                                        |   |
|  | // remove()中无需手动释放                                  |   |
|  +----------------------------------------------------------+   |
|                                                                  |
+------------------------------------------------------------------+
```

常用devm函数：
```c
/*=== 内存分配 ===*/
void *devm_kmalloc(struct device *dev, size_t size, gfp_t gfp);
void *devm_kzalloc(struct device *dev, size_t size, gfp_t gfp);
void *devm_kcalloc(struct device *dev, size_t n, size_t size, gfp_t gfp);
char *devm_kstrdup(struct device *dev, const char *s, gfp_t gfp);

/*=== I/O资源 ===*/
void __iomem *devm_ioremap(struct device *dev, resource_size_t offset, 
                           resource_size_t size);
void __iomem *devm_ioremap_resource(struct device *dev, 
                                    const struct resource *res);
void __iomem *devm_platform_ioremap_resource(struct platform_device *pdev,
                                             unsigned int index);

/*=== 中断 ===*/
int devm_request_irq(struct device *dev, unsigned int irq, irq_handler_t handler,
                     unsigned long flags, const char *name, void *data);
int devm_request_threaded_irq(struct device *dev, unsigned int irq,
                              irq_handler_t handler, irq_handler_t thread_fn,
                              unsigned long flags, const char *name, void *data);

/*=== 时钟 ===*/
struct clk *devm_clk_get(struct device *dev, const char *id);
int devm_clk_prepare_enable(struct device *dev, struct clk *clk);

/*=== GPIO ===*/
int devm_gpio_request(struct device *dev, unsigned gpio, const char *label);
struct gpio_desc *devm_gpiod_get(struct device *dev, const char *con_id,
                                 enum gpiod_flags flags);

/*=== 电源 ===*/
struct regulator *devm_regulator_get(struct device *dev, const char *id);

/*=== PWM ===*/
struct pwm_device *devm_pwm_get(struct device *dev, const char *con_id);

/*=== 复位 ===*/
struct reset_control *devm_reset_control_get(struct device *dev, const char *id);
```

devres原理：
```c
// 资源链表存储在device结构中
struct device {
    // ...
    spinlock_t devres_lock;
    struct list_head devres_head;  // 资源链表
};

// 资源节点
struct devres {
    struct devres_node node;
    // 资源数据跟在后面
};

struct devres_node {
    struct list_head entry;
    dr_release_t release;  // 释放函数
    // ...
};

// 释放时机
// 1. probe()失败返回时
// 2. remove()完成后
// 3. device_del()时
```
[Intermediate]

---

Q: How to create custom devres?
A: 
```c
// 定义释放函数
static void my_resource_release(struct device *dev, void *res)
{
    struct my_resource *r = res;
    
    // 释放资源
    free_my_resource(r->handle);
}

// 分配managed资源
struct my_resource *devm_my_resource_get(struct device *dev, int param)
{
    struct my_resource *r;
    
    // 分配devres（包含自定义数据）
    r = devres_alloc(my_resource_release, sizeof(*r), GFP_KERNEL);
    if (!r)
        return NULL;
    
    // 获取实际资源
    r->handle = allocate_my_resource(param);
    if (!r->handle) {
        devres_free(r);
        return NULL;
    }
    
    // 添加到设备资源链表
    devres_add(dev, r);
    
    return r;
}

// 手动释放（可选）
void devm_my_resource_put(struct device *dev, struct my_resource *r)
{
    devres_release(dev, my_resource_release, 
                   devm_my_resource_match, r);
}

// 使用devm_add_action（简化版）
static void cleanup_action(void *data)
{
    struct my_data *d = data;
    // 清理
}

static int my_probe(struct platform_device *pdev)
{
    struct my_data *data;
    
    data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
    
    // 注册清理动作
    devm_add_action_or_reset(&pdev->dev, cleanup_action, data);
    
    return 0;
}
```
[Advanced]

---

## 11. Power Management (电源管理)

---

Q: How does device power management work?
A: 设备电源管理通过dev_pm_ops实现：

```c
// include/linux/pm.h
struct dev_pm_ops {
    // 系统睡眠
    int (*prepare)(struct device *dev);
    void (*complete)(struct device *dev);
    int (*suspend)(struct device *dev);
    int (*resume)(struct device *dev);
    int (*freeze)(struct device *dev);    // 休眠准备
    int (*thaw)(struct device *dev);      // 休眠恢复
    int (*poweroff)(struct device *dev);  // 休眠关机
    int (*restore)(struct device *dev);   // 休眠恢复
    
    // 运行时电源管理
    int (*runtime_suspend)(struct device *dev);
    int (*runtime_resume)(struct device *dev);
    int (*runtime_idle)(struct device *dev);
};

// 简化宏
#define SET_SYSTEM_SLEEP_PM_OPS(suspend, resume) \
    .suspend = suspend, \
    .resume = resume, \
    .freeze = suspend, \
    .thaw = resume, \
    .poweroff = suspend, \
    .restore = resume,

#define SET_RUNTIME_PM_OPS(suspend, resume, idle) \
    .runtime_suspend = suspend, \
    .runtime_resume = resume, \
    .runtime_idle = idle,

// 组合宏
#define DEFINE_SIMPLE_DEV_PM_OPS(name, suspend, resume) \
    static const struct dev_pm_ops name = { \
        SET_SYSTEM_SLEEP_PM_OPS(suspend, resume) \
    }
```

电源管理示例：
```c
static int my_suspend(struct device *dev)
{
    struct my_device *mydev = dev_get_drvdata(dev);
    
    // 保存状态
    mydev->saved_reg = readl(mydev->base + REG_OFFSET);
    
    // 禁用设备
    clk_disable_unprepare(mydev->clk);
    
    return 0;
}

static int my_resume(struct device *dev)
{
    struct my_device *mydev = dev_get_drvdata(dev);
    
    // 启用时钟
    clk_prepare_enable(mydev->clk);
    
    // 恢复状态
    writel(mydev->saved_reg, mydev->base + REG_OFFSET);
    
    return 0;
}

static int my_runtime_suspend(struct device *dev)
{
    struct my_device *mydev = dev_get_drvdata(dev);
    
    clk_disable_unprepare(mydev->clk);
    return 0;
}

static int my_runtime_resume(struct device *dev)
{
    struct my_device *mydev = dev_get_drvdata(dev);
    
    return clk_prepare_enable(mydev->clk);
}

static const struct dev_pm_ops my_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(my_suspend, my_resume)
    SET_RUNTIME_PM_OPS(my_runtime_suspend, my_runtime_resume, NULL)
};

static struct platform_driver my_driver = {
    .driver = {
        .name = "my-device",
        .pm = &my_pm_ops,
    },
};
```

运行时PM API：
```c
// 在probe中启用
pm_runtime_enable(&pdev->dev);
pm_runtime_set_active(&pdev->dev);

// 使用前获取
pm_runtime_get_sync(&pdev->dev);

// 使用完释放
pm_runtime_put(&pdev->dev);
pm_runtime_put_sync(&pdev->dev);
pm_runtime_put_autosuspend(&pdev->dev);

// 在remove中禁用
pm_runtime_disable(&pdev->dev);
```
[Intermediate]

---

## 12. Debugging Device Model (调试设备模型)

---

Q: How to debug the device model?
A: 
```bash
# 查看设备树结构
ls -la /sys/devices/
tree /sys/devices/platform/

# 查看总线
ls /sys/bus/
cat /sys/bus/platform/drivers_autoprobe

# 查看类
ls /sys/class/
ls /sys/class/net/

# 查看设备属性
cat /sys/class/net/eth0/address
cat /sys/class/net/eth0/mtu

# 手动绑定/解绑驱动
echo "device_name" > /sys/bus/platform/drivers/driver_name/unbind
echo "device_name" > /sys/bus/platform/drivers/driver_name/bind

# 查看驱动信息
modinfo driver_name
cat /sys/module/driver_name/parameters/*

# 监视uevent
udevadm monitor --environment

# 查看设备树（如果编译到内核）
ls /sys/firmware/devicetree/base/
cat /sys/firmware/devicetree/base/compatible

# debugfs信息
mount -t debugfs none /sys/kernel/debug
cat /sys/kernel/debug/devices_deferred  # 延迟探测的设备
cat /sys/kernel/debug/gpio

# dmesg相关
dmesg | grep "my_device"
echo 8 > /proc/sys/kernel/printk  # 提高打印级别
```

内核调试选项：
```bash
# 启用调试选项
CONFIG_DEBUG_DRIVER=y
CONFIG_DEBUG_DEVRES=y

# 动态调试
echo 'file drivers/base/* +p' > /sys/kernel/debug/dynamic_debug/control
echo 'module my_driver +p' > /sys/kernel/debug/dynamic_debug/control
```

常见问题排查：
```c
// 1. probe未调用
// 检查匹配：compatible, id_table, driver.name
// 检查依赖：deferred probe

// 2. 资源获取失败
// 检查设备树节点
// 检查资源索引

// 3. sysfs属性不出现
// 检查groups设置
// 检查show/store函数

// 4. uevent未发送
// 检查kobject_uevent调用
// 检查kset->uevent_ops->filter
```
[Basic]

---

## 13. Summary (总结)

---

Q: Summarize the Linux device model architecture.
A: 
```
+==================================================================+
||                 LINUX DEVICE MODEL ARCHITECTURE                ||
+==================================================================+

层次结构:
+------------------------------------------------------------------+
|                         sysfs (/sys/)                            |
|  用户空间接口，暴露内核对象和属性                                   |
+------------------------------+-----------------------------------+
                               |
+------------------------------v-----------------------------------+
|                    kobject / kset                                |
|  基础对象，引用计数，层次结构                                       |
+------------------------------+-----------------------------------+
                               |
        +----------------------+----------------------+
        |                      |                      |
+-------v-------+    +---------v--------+   +--------v--------+
|     bus       |    |     device       |   |     driver      |
|  总线类型      |    |   设备实例       |   |   驱动程序       |
+-------+-------+    +---------+--------+   +--------+--------+
        |                      |                      |
        +----------------------+----------------------+
                               |
                        match & probe
                               |
+------------------------------v-----------------------------------+
|                         class                                    |
|  设备功能分类 (net, block, tty, ...)                             |
+------------------------------------------------------------------+


关键数据结构:
+-------------+--------------------------------------------------+
| kobject     | 基础对象，提供引用计数和sysfs表示                  |
| kset        | kobject集合，提供uevent操作                        |
| bus_type    | 总线类型，管理设备-驱动匹配                         |
| device      | 设备实例，挂在总线上                               |
| driver      | 驱动程序，注册到总线                               |
| class       | 设备类，按功能分组                                 |
| attribute   | sysfs属性文件                                      |
+-------------+--------------------------------------------------+


设备注册流程:
    device_register()
         |
         v
    device_add()
         |
         +---> kobject_add()           // 添加到sysfs
         |
         +---> bus_add_device()        // 添加到总线
         |
         +---> bus_probe_device()      // 尝试匹配驱动
         |         |
         |         v
         |    driver_match_device()    // 匹配
         |         |
         |         v
         |    driver_probe_device()    // 探测
         |
         +---> kobject_uevent(ADD)     // 发送uevent


驱动注册流程:
    driver_register()
         |
         v
    bus_add_driver()
         |
         +---> kobject_init_and_add()  // 添加到sysfs
         |
         +---> driver_attach()         // 尝试绑定设备
                   |
                   v
              bus_for_each_dev()       // 遍历设备
                   |
                   v
              __driver_attach()        // 匹配并探测


资源管理:
    devres (devm_xxx API)
         |
         +---> 自动管理资源生命周期
         |
         +---> probe失败自动清理
         |
         +---> remove后自动释放


电源管理:
    dev_pm_ops
         |
         +---> 系统睡眠 (suspend/resume)
         |
         +---> 运行时PM (runtime_suspend/resume)
```
[Basic]

---

*Total: 100+ cards covering Linux kernel device model and sysfs*

