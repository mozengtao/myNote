# 09. Bus/Device/Driver Object Injection

> 04 篇讲的是 `pci_driver`/`platform_driver` 这些**具体总线类型**如何把 Callback、Ops Table、
> Context Injection 三种手法合体成"驱动注册"。本篇往下钻一层，讲这套注册机制真正的地基：
> **`struct bus_type` + `struct device_driver` + `struct device` 这一套总线无关的通用基础设施**，
> PCI/USB/platform 全部只是这套地基上的具体实例。

---

## 1. Why

04 篇里已经出现过 `struct device_driver driver` 内嵌在 `struct pci_driver` 里，
`bus_add_driver()`、`driver_register()`、`really_probe()` 这些函数名也已经被引用过——
但那时我们把这些函数当作"PCI 专属"的黑盒去理解。真实情况是：**这些函数与 PCI 毫无关系，
它们是 `drivers/base/` 目录下的通用驱动核心（driver core），PCI/USB/I2C/SPI/platform
总线一个字节都没有重新实现它们。**

如果没有这一层通用基础设施，会发生什么？每一种总线类型都要各自实现一套：

- "设备先出现、驱动后注册"和"驱动先注册、设备后出现"两种时序下都要能正确配对的扫描逻辑
- 引用计数、sysfs 节点创建、电源管理钩子转发
- 热插拔事件广播（`uevent`）

这些逻辑对 PCI 和 USB 而言几乎一模一样，唯一真正不同的只有"怎么判断一个设备和一个驱动算是匹配"
（PCI 比较 vendor/device id，USB 比较 idVendor/idProduct，platform 比较设备名字符串）。

设计目标：

> **把"设备与驱动如何被组织、如何被扫描配对、生命周期如何管理"这套逻辑做成总线无关的通用核心，
> 把"两者是否匹配"这一个判断点做成唯一的注入点（`bus_type->match`），
> 交给每一种具体总线类型自己实现。核心代码只写一次，被所有总线类型共享。**

这是一次经典的"提取公共骨架，把差异点做成契约"的重构：04 篇看到的 `pci_driver`/`platform_driver`
是"骨架 + 差异点"合体后的**成品**；本篇要看的是骨架本身长什么样，以及"差异点"具体嵌在骨架的哪个孔位上。

---

## 2. Architecture

```
                    具体总线类型（PCI / USB / platform / I2C / SPI）
                 只提供 match() 的实现，其余全部复用通用核心
                               |
                               | 填入 bus_type.match 字段
                               v
   +--------------------------------------------------------------+
   |                    struct bus_type（通用核心持有）              |
   |                                                                |
   |   match(dev, drv)   <-- 唯一真正的"总线专属"注入点              |
   |   uevent(dev, env)  <-- 总线专属的热插拔环境变量补充（可选）      |
   |   probe/remove      <-- 少数总线选择在这里包一层（可选）          |
   |                                                                |
   |   device 列表（klist_devices）      driver 列表（klist_drivers）|
   +--------------------------------------------------------------+
               |                                    |
        device_register()                   driver_register()
               |                                    |
               v                                    v
   +----------------+                    +----------------------+
   | struct device   |                    | struct device_driver |
   |  .bus  ---------+------------------->|  .bus                |
   |  .driver        |<-------------------+  .probe/.remove      |
   +----------------+   bus_try_bind()    +----------------------+
                          （双向扫描配对，drivers/base/bus.c）
```

真实定义（均为本仓库真实结构体，[include/linux/device.h:87](../../include/linux/device.h)、
[include/linux/device.h:192](../../include/linux/device.h)）：

```c
struct bus_type {
	const char *name;
	int (*match)(struct device *dev, struct device_driver *drv);
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
	void (*shutdown)(struct device *dev);
	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);
	const struct dev_pm_ops *pm;
	struct iommu_ops *iommu_ops;
	struct subsys_private *p;    /* 驱动核心私有数据：device/driver klist 等 */
};

struct device_driver {
	const char *name;
	struct bus_type *bus;
	struct module *owner;
	int (*probe) (struct device *dev);
	int (*remove) (struct device *dev);
	void (*shutdown) (struct device *dev);
	int (*suspend) (struct device *dev, pm_message_t state);
	int (*resume) (struct device *dev);
	const struct dev_pm_ops *pm;
	struct driver_private *p;
};
```

对比 04 篇的 `struct pci_driver`：它内嵌了一份 `struct device_driver driver`——这正是
03 篇 Context Injection 的手法。**04 篇是"实例"，本篇是"元框架"**：`pci_bus_type`、
`platform_bus_type`、`usb_bus_type` 都是 `struct bus_type` 的**具体实例**（各自填充不同的
`match`/`uevent` 实现），`pci_register_driver()`/`platform_driver_register()` 内部最终都
落到同一份 `driver_register()` -> `bus_add_driver()` -> `driver_attach()` 代码上。

---

## 3. Lifecycle

```
   allocate
   (定义 static struct bus_type my_bus = { .name = "my-bus", .match = my_match };
    以及若干 struct device / struct device_driver 实例)
        |
        v
   initialize
   (bus_register(&my_bus) —— 建立 sysfs 下的 /sys/bus/my-bus/ 骨架，
    初始化空的 device/driver klist)
        |
        v
   inject（设备/驱动的注册，顺序任意）
   +- device_register(&dev)  —— dev->bus = &my_bus，加入 device 列表
   |      |
   |      +- bus_probe_device() -> device_attach() -> 反向扫描 driver 列表
   |
   +- driver_register(&drv) —— drv->bus = &my_bus，加入 driver 列表
          |
          +- bus_add_driver() -> driver_attach() -> 正向扫描 device 列表
        |
        v
   framework owns dispatch（匹配）
   (driver_match_device() 调用 bus->match(dev, drv) —— 唯一的总线专属判断)
        |
        v
   callback（probe）
   (匹配成功 -> really_probe() -> drv->probe(dev) 被调用)
        |
        v
   [设备使用中：suspend/resume/shutdown 等 ops 陆续被调用]
        |
        v
   destroy
   (driver_unregister(&drv) -> driver_detach() -> 对所有已绑定设备调用 drv->remove()；
    device_unregister(&dev) -> device_del() -> 若仍绑定着驱动，先解绑再移除；
    bus_unregister(&my_bus) —— 整条总线本身被卸载，罕见但存在，如虚拟总线卸载)
```

---

## 4. Relationship

```
                        struct bus_type（Core，全局唯一实例）
                               |
              持有两条独立的 klist（双向、可并发遍历）
                               |
             +-----------------+------------------+
             |                                    |
     klist_devices                         klist_drivers
             |                                    |
             v                                    v
      struct device <---.driver------ struct device_driver
             |                                    |
             | 内嵌于（container_of 反推，03 篇手法）| 内嵌于
             v                                    v
     struct pci_dev /                    struct pci_driver /
     struct platform_device              struct platform_driver
     （04 篇讨论的"实例层"）                （04 篇讨论的"实例层"）
```

一句话：`bus_type` 是唯一的横向纽带——它不知道 `device`/`device_driver` 背后到底内嵌在
`pci_dev`/`pci_driver` 里还是别的什么结构体里，只通过 `.bus`/`.driver` 两个指针互相找到对方。

---

## 5. Data Flow

以 `generic_bus_core.c` 的"设备先出现，驱动后注册"路径为例：

```
main()
  |
  +- generic_device_register(&pci_bus, &eth0)   // 设备先出现，此时 bus 上没有驱动
  |     |
  |     v
  |  遍历 0 个驱动 -> "no driver matched device 'eth0' yet"
  |
  +- generic_driver_register(&pci_bus, &nic_driver)  // 驱动后注册
        |
        v
     反向扫描 bus->devices[]，对每个未绑定设备调用 core_match()
        |
        v
     core_match(eth0, nic_driver)  ->  bus->match(dev, drv)
        |  匹配成功
        v
     core_try_probe(eth0, nic_driver) -> nic_driver.probe(eth0, nic_driver)
        |
        v
     eth0.bound_driver = &nic_driver; eth0.driver_data = 私有 ring buffer
```

对照真实 Kernel（本仓库真实代码，路径与 04 篇引用的一致，但这里关注"谁在调用谁"这条总线无关的主线）：

```
pci_register_driver(&my_nic_driver)          usb_register(&my_usb_driver)
        |                                            |
        v                                            v
driver_register(&drv->driver)  <-------共用同一份---->  driver_register(&drv->driver)
        |
        v
bus_add_driver(drv)                    // drivers/base/bus.c:625，总线无关
        |
        v
driver_attach(drv)                     // drivers/base/dd.c，反向扫描 bus 上所有 device
        |
        v
bus_for_each_dev(drv->bus, ..., __driver_attach)
        |
        v
__driver_attach(dev, drv)
        |
        v
driver_match_device(drv, dev)          // drivers/base/base.h:109
        |   return drv->bus->match ? drv->bus->match(dev, drv) : 1;
        |   <-- 唯一分叉点：PCI 走 pci_bus_type.match，USB 走 usb_bus_type.match
        v
driver_probe_device(drv, dev) -> really_probe(dev, drv) -> drv->probe(dev)
```

PCI 和 USB 两条调用链在 `bus_add_driver`/`driver_attach`/`really_probe` 这几步上**完全共用同一份
二进制代码**，唯一走向不同分支的地方就是 `driver_match_device()` 里的那一次 `bus->match()` 盲调。

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `bus_type.match`/`uevent`/`probe` 等函数指针 | **`bus_register()` 之前固定，此后不变** | 总线类型通常是编译期 `static struct bus_type`，如 `pci_bus_type` |
| `bus->p`（`subsys_private`，含 device/driver klist） | **运行期高度动态** | `bus_register()` 时才分配，`device_register()`/`driver_register()` 随时增删节点 |
| `device->driver`（绑定关系） | **运行期变化**，由 probe 成功/remove 触发 | 与 04 篇的 `bound_driver` 是同一个状态维度，只是这里是通用字段 |
| `device->p`/`driver->p`（driver core 私有数据，sysfs kobject 等） | 注册时分配，注销时释放 | 业务代码（PCI/USB 层）完全不感知，是"元框架自己的账本" |
| `drivers_autoprobe`（`bus->p->drivers_autoprobe`） | **运行期可通过 sysfs 开关** | 见 [drivers/base/bus.c](../../drivers/base/bus.c) `store_drivers_autoprobe()`，关闭后新驱动注册不会自动扫描设备，必须手动 `echo dev > .../bind` |

---

## 7. Failure

1. **把"总线专属逻辑"误塞进通用核心**：如果在 `drivers/base/bus.c` 里写
   `if (bus == &pci_bus_type) ...` 这种特判，就彻底破坏了本篇的设计意图——
   `bus_add_driver()`/`driver_attach()` 任何一行出现"认出具体总线类型"的代码都是设计上的坏味道，
   真实 Kernel 里这些函数从未 `#include <linux/pci.h>`。
2. **只在一侧触发扫描**：与 04 篇的坑相同，但根源在这一层——`device_add()`
   （[drivers/base/core.c:891](../../drivers/base/core.c)）末尾调用 `bus_probe_device(dev)`，
   `bus_add_driver()`（[drivers/base/bus.c:625](../../drivers/base/bus.c)）里调用 `driver_attach(drv)`，
   两处都要触发，任何一处被后来的重构误删都会导致"先加载驱动再插设备"或反过来的场景失效。
3. **`drv->bus` 与 `bus->match` 不一致**：如果驱动注册时 `drv->bus` 指向了错误的 `bus_type`
   实例（比如手写代码时把 `usb_bus_type` 误写成 `pci_bus_type`），`driver_match_device()`
   会调用错误总线的 `match` 逻辑，可能出现"看起来匹配成功但字段语义完全对不上"的诡异 bug——
   这也是 09.2 示例特意让两条总线各自独立持有 `nr_drivers`/`nr_devices` 的原因，用来验证隔离性。
4. **`uevent` 回调里做了重量级操作**：`bus->uevent(dev, env)`
   （[drivers/base/core.c:191](../../drivers/base/core.c) `dev_uevent()`）运行在
   `device_add()`/`device_del()` 的同步路径上，如果总线类型的 `uevent` 实现做了阻塞式 I/O，
   会拖慢所有设备的注册/注销速度——这是"契约"里"允许你做什么、不允许你做什么"这条隐性约定
   被违反的典型例子。
5. **忘记检查 `drivers_autoprobe`**：某些总线场景下运维会临时关闭自动探测（防止某个驱动在
   系统未完全初始化时过早接管设备），如果代码里假设"注册后一定会自动 probe"而没有检查这个开关，
   会看到"注册成功但设备一直未绑定"的现象，误以为是 `match()` 出了 bug。
6. **调试方法**：
   - 用户态：像 `generic_bus_core.c`/`multi_bus_types_demo.c` 那样打印每一步 `match`/`probe`
     的返回值，配合两条独立总线的计数器验证隔离性。
   - Kernel：`/sys/bus/<bus>/devices/`、`/sys/bus/<bus>/drivers/` 直接反映
     `bus->p->klist_devices`/`klist_drivers` 的当前内容；
     `/sys/bus/<bus>/drivers_autoprobe`（可读写）对应 `store_drivers_autoprobe()`；
     `echo <dev_name> > /sys/bus/<bus>/drivers/<drv>/bind` 手动触发一次
     `driver_bind()`（[drivers/base/bus.c](../../drivers/base/bus.c) 附近），可以在自动探测关闭时
     手动复现匹配流程；`udevadm monitor`/`udevadm info` 可以观察真实的 uevent 广播内容。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `bus_register()`：总线本身的注册，与任何具体设备/驱动无关

真实定义：[drivers/base/bus.c:857](../../drivers/base/bus.c)

```c
int bus_register(struct bus_type *bus)
{
	struct subsys_private *priv = kzalloc(sizeof(*priv), GFP_KERNEL);

	priv->bus = bus;
	bus->p = priv;
	BLOCKING_INIT_NOTIFIER_HEAD(&priv->bus_notifier);   /* 05 篇的机制在此复用 */
	kobject_set_name(&priv->subsys.kobj, "%s", bus->name);
	priv->drivers_autoprobe = 1;
	kset_register(&priv->subsys);
	klist_init(&priv->klist_devices, klist_devices_get, klist_devices_put);
	klist_init(&priv->klist_drivers, NULL, NULL);
	return 0;
}
```
为什么是 DI：这个函数完全不知道会有 PCI、USB 还是任何其它总线来调用它——`bus->name`、
`bus->match` 全部是调用者已经填好的字段，`bus_register()` 只负责搭好"骨架"（两条空 klist）。

### Example 2 — `bus_add_device()`：设备侧触发的扫描

真实定义：[drivers/base/bus.c:450](../../drivers/base/bus.c)

```c
int bus_add_device(struct device *dev)
{
	struct bus_type *bus = bus_get(dev->bus);

	if (bus) {
		sysfs_create_link(&bus->p->devices_kset->kobj, &dev->kobj, dev_name(dev));
		klist_add_tail(&dev->p->knode_bus, &bus->p->klist_devices);
	}
	return 0;
}
```
真正触发"扫描配对"的调用点在上一层 [drivers/base/core.c:978](../../drivers/base/core.c)
`device_add()` 里：`bus_probe_device(dev)` 紧跟在 `kobject_uevent(&dev->kobj, KOBJ_ADD)` 之后
被调用——这正是 09.3 示例 `uevent_hotplug_notify.c` 建模的"广播 + 扫描"两步走时序。

### Example 3 — `driver_match_device()`：唯一的总线专属注入点

真实定义：[drivers/base/base.h:109](../../drivers/base/base.h)

```c
static inline int driver_match_device(struct device_driver *drv, struct device *dev)
{
	return drv->bus->match ? drv->bus->match(dev, drv) : 1;
}
```
这是整篇文档的核心一行：Core（`__driver_attach`/`__device_attach`）永远调用这同一个
`driver_match_device()`，函数体内部再转发到 `drv->bus->match`——PCI 总线的 `match` 比较
`pci_device_id` 表，USB 总线的 `match` 比较 `usb_device_id` 表，platform 总线的 `match`
比较设备名字符串，三者的差异被**严格限制在这一次函数指针跳转的背后**，`driver_match_device()`
自身的代码永远不用为新增一种总线类型而修改。

### Example 4 — `really_probe()`：绑定关系建立 + 失败回滚

真实定义：[drivers/base/dd.c:108](../../drivers/base/dd.c)

```c
static int really_probe(struct device *dev, struct device_driver *drv)
{
	int ret;

	dev->driver = drv;                 /* 先建立绑定关系 */
	if (driver_sysfs_add(dev))
		goto probe_failed;

	if (dev->bus->probe)
		ret = dev->bus->probe(dev);      /* 少数总线选择自己包一层 */
	else if (drv->probe)
		ret = drv->probe(dev);           /* 多数情况：直达驱动的 probe() */
	if (ret)
		goto probe_failed;

	driver_bound(dev);                 /* 绑定成功，广播 BUS_NOTIFY_BOUND_DRIVER */
	return 1;

probe_failed:
	devres_release_all(dev);
	dev->driver = NULL;                /* 失败必须回滚绑定关系 */
	return 0;
}
```
注意 `if (dev->bus->probe) ... else if (drv->probe) ...` 这一段——`bus_type` 自己也可以有
一个 `probe`，这是给"总线需要在驱动 probe 之前/之后插入自己的统一逻辑"留的第二个注入点
（比如某些总线要先做电源上电才能调用驱动的 probe）。

### Example 5 — `driver_register()` 与冲突检查

真实定义：[drivers/base/driver.c:222](../../drivers/base/driver.c)

```c
int driver_register(struct device_driver *drv)
{
	struct device_driver *other;

	BUG_ON(!drv->bus->p);   /* 必须先 bus_register()，否则 bus->p 为空 */

	if ((drv->bus->probe && drv->probe) || ...)
		printk(KERN_WARNING "Driver '%s' needs updating...\n", drv->name);

	other = driver_find(drv->name, drv->bus);
	if (other) {
		put_driver(other);
		return -EBUSY;          /* 同一总线下不允许重名驱动 */
	}
	return bus_add_driver(drv);
}
```
为什么是 DI：`driver_register()` 本身不知道 `drv` 到底是 `to_pci_driver(drv)` 还是
`to_platform_driver(drv)` 反推出来的——它只操作 `struct device_driver` 这一份契约字段
（`name`/`bus`/`probe`），`BUG_ON(!drv->bus->p)` 这一行也说明了"总线必须先注册"这个
硬性生命周期约束：`bus_type` 在文档结构里天然是"更早创建、更晚销毁"的那一层。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/09-bus-device-driver-injection/`](c/09-bus-device-driver-injection/)

### 9.1 `generic_bus_core.c` — 从零实现总线无关的匹配核心

- 场景：完整复刻 `struct bus_type`（只放 `match` 函数指针）+ `generic_device_register()`/
  `generic_driver_register()` 双向扫描配对，验证"设备先/驱动先"两种顺序都能正确绑定，
  且没有任何驱动能匹配的设备（`sda`）会保持未绑定状态。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o generic_bus_core generic_bus_core.c`
- 实测输出：
  ```
  === generic_bus_core demo ===

  -- 阶段 1：设备先出现，此时总线上还没有驱动 --
  [bus:pci] device 'eth0' (model=NIC-8086) registered
  [bus:pci] no driver matched device 'eth0' yet
  [bus:pci] device 'sda' (model=DISK-1000) registered
  [bus:pci] no driver matched device 'sda' yet

  -- 阶段 2：注册 nic 驱动，反向扫描已存在的设备 --
  [bus:pci] driver 'nic-NIC-driver' registered
    [core] trying to bind device 'eth0' with driver 'nic-NIC-driver'
        [driver:nic-NIC-driver] allocated ring buffer (size=256) for 'eth0'
    [core] bind succeeded: 'eth0' <-> 'nic-NIC-driver'

  -- 阶段 3：sda 的型号没有驱动能匹配，保持未绑定状态 --
    sda.bound_driver = (null)

  -- 阶段 4：卸载 nic 驱动，eth0 被 remove --
    [core] removing device 'eth0' from driver 'nic-NIC-driver'
        [driver:nic-NIC-driver] freeing ring buffer for 'eth0'
  [bus:pci] driver 'nic-NIC-driver' unregistered

  === done ===
  eth0.bound_driver (should be null after unregister) = (null)
  ```
  （`sda` 的型号 `DISK-1000` 与驱动名 `nic-NIC-driver` 中的前缀 `NIC` 不匹配，输出确认
  `sda.bound_driver` 始终为 `(null)`；卸载驱动后 `eth0.bound_driver` 也被正确清空。）
- 为什么是 DI：`core_try_probe()`/`core_match()`（Core）从不知道"型号前缀比较"这个具体
  匹配算法，只调用 `bus->match()`；换一种总线类型只需要换一个 `match` 函数指针，
  `generic_device_register()`/`generic_driver_register()` 一行都不用改。

### 9.2 `multi_bus_types_demo.c` — 同一套核心服务两种完全不同的总线

- 场景：`virtual-pci`（按数值 id 精确匹配）和 `virtual-usb`（按 class 编号匹配）两条总线
  共用同一份 `core_register_device()`/`core_register_driver()`/`core_scan_and_bind()`，
  验证互不干扰。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o multi_bus_types_demo multi_bus_types_demo.c`
- 实测输出：
  ```
  === multi_bus_types_demo ===

  -- 在 virtual-pci 总线上注册设备 + 驱动 --
  [bus:virtual-pci] device 'pci-nic0' registered (id_a=32902 id_b=4096)
    [core:virtual-pci] 'pci-nic0' has no matching driver (yet)
  [bus:virtual-pci] device 'pci-disk0' registered (id_a=4660 id_b=22136)
    [core:virtual-pci] 'pci-disk0' has no matching driver (yet)
  [bus:virtual-pci] driver 'e1000e' registered (match_a=32902 match_b=4096)
        [pci-probe] init BAR mappings for 'pci-nic0'
    [core:virtual-pci] 'pci-nic0' bound to driver 'e1000e'
    [core:virtual-pci] 'pci-disk0' has no matching driver (yet)

  -- 在 virtual-usb 总线上注册设备 + 驱动（完全独立的核心实例）--
  [bus:virtual-usb] device 'usb-cam0' registered (id_a=14 id_b=1)
    [core:virtual-usb] 'usb-cam0' has no matching driver (yet)
  [bus:virtual-usb] device 'usb-mouse0' registered (id_a=3 id_b=0)
    [core:virtual-usb] 'usb-mouse0' has no matching driver (yet)
  [bus:virtual-usb] driver 'uvcvideo' registered (match_a=14 match_b=0)
        [usb-probe] claiming interface for 'usb-cam0'
    [core:virtual-usb] 'usb-cam0' bound to driver 'uvcvideo'
    [core:virtual-usb] 'usb-mouse0' has no matching driver (yet)

  -- 验证：同一份 core_register_driver() 代码服务了两种完全不同的总线 --
    vpci.nr_drivers=1, vusb.nr_drivers=1 (两条总线各自独立计数)

  === 最终绑定结果 ===
  pci_nic    bound to: e1000e
  pci_disk   bound to: (none) (预期未匹配到任何驱动)
  usb_cam    bound to: uvcvideo
  usb_mouse  bound to: (none) (预期未匹配到任何驱动)

  === done ===
  ```
  （`id_a=32902 id_b=4096` 即 `0x8086`/`0x1000` 的十进制显示；`pci_disk0`/`usb_mouse0`
  确认始终未绑定，证明两条总线各自独立比较，不会互相"借用"对方的驱动。）
- 为什么是 DI：`core_scan_and_bind()`（Core）里的 `bus->match` 调用同一行代码在两条总线上
  执行了两种完全不同的比较逻辑，且两条总线的 `devices[]`/`drivers[]` 数组物理隔离——
  这正是真实 Kernel `pci_bus_type`/`usb_bus_type`/`platform_bus_type` 同时存在、互不干扰的建模。

### 9.3 `uevent_hotplug_notify.c` — bus core 触发的热插拔事件广播

- 场景：模拟 `device_add()`/`device_del()` 内部的 `kobject_uevent()` 广播——设备注册后
  match 成功/失败都会触发一次带环境变量的事件，两个独立的监听者（udev 模拟器 + 审计日志）
  各自响应。与 05 篇 notifier chain 概念呼应但是全新独立实现（数组式监听表，而非链表 + 优先级）。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o uevent_hotplug_notify uevent_hotplug_notify.c`
- 实测输出：
  ```
  === uevent_hotplug_notify demo ===

  [hotplug:vbus0] listener 'udev-sim' attached
  [hotplug:vbus0] listener 'audit-log' attached

  -- 场景 1：设备注册，match 成功，触发 bind 事件 --

  -- 注册设备 'disk0' (model=STORAGE-NVME) --
  [hotplug:vbus0] broadcasting action='bind' (3 env var(s)) to 2 listener(s)
      [udev-sim] creating /dev/disk0 node (bind event)
      [audit-log] action=bind env={ DEVNAME=disk0 MODEL=STORAGE-NVME DRIVER=nvme }

  -- 场景 2：设备注册，没有驱动匹配，触发 no_driver 事件 --

  -- 注册设备 'disk1' (model=STORAGE-SCSI) --
  [hotplug:vbus0] broadcasting action='no_driver' (2 env var(s)) to 2 listener(s)
      [udev-sim] device 'disk1' has no driver, skipping node creation
      [audit-log] action=no_driver env={ DEVNAME=disk1 MODEL=STORAGE-SCSI }

  -- 场景 3：驱动移除，触发 unbind 事件 --

  -- 驱动 'nvme' 移除，解绑设备 'disk0' --
  [hotplug:vbus0] broadcasting action='unbind' (2 env var(s)) to 2 listener(s)
      [udev-sim] removing /dev/disk0 node (unbind event)
      [audit-log] action=unbind env={ DEVNAME=disk0 DRIVER=nvme }

  === done ===
  udev_ctx.nodes_created (预期 0，创建后又被 unbind 移除) = 0
  audit_ctx.events_seen  (预期 3，三次广播全部被审计记录) = 3
  ```
  （`disk1` 的型号 `STORAGE-SCSI` 没有对应驱动，输出确认走的是 `no_driver` 分支，
  udev-sim 没有为它创建节点；三次广播 `audit_ctx.events_seen` 精确计数为 3。）
- 为什么是 DI：`hotplug_bus_broadcast()`（Core）不知道 `udev_sim_listener`/`audit_listener`
  具体做什么，只按注册顺序盲调 `uevent_listener_fn`；新增一种监听者（比如"电源管理策略"）
  只需要 `hotplug_bus_add_listener()`，不需要改总线核心一行代码——这也说明"总线核心"内部
  其实同时用了两种 DI 手法：`match` 是"路由"（04/09 篇），`uevent` 广播是"通知"（05 篇），
  两者服务的目标不同：前者决定"谁来处理"，后者决定"谁需要被告知"。

一次性编译并运行全部三个示例：`cd c/09-bus-device-driver-injection && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | 09 Bus/Device/Driver（元框架） | 04 Driver Registration（实例） | Ops Table | Notifier Chain |
|---|---|---|---|---|
| 抽象层级 | **总线无关的通用核心**（`bus_type`/`device_driver`/`device`） | **具体总线类型**的特化（`pci_driver`/`platform_driver`），内嵌 04 的字段 | 单个对象的一整套操作 | 单个事件的一对多广播 |
| 谁定义"匹配"逻辑 | 只定义"匹配"这个**孔位**（`bus->match` 函数指针类型） | 填入孔位的**具体实现**（比较 `pci_device_id`/`usb_device_id`） | 不涉及 | 不涉及匹配，无条件广播 |
| 新增一种总线类型的成本 | **零**——`drivers/base/*.c` 一行不改 | 需要新写一个 `xxx_bus_type` 实例 + 对应的 `xxx_register_driver()` 包装函数 | — | — |
| 新增一种设备/驱动组合的成本 | 不适用（这一层不关心"多少个"） | **零**——只需 `xxx_register_driver()`，`bus_add_driver()` 逻辑复用 | — | — |
| 典型 Kernel 代码位置 | `drivers/base/bus.c`、`drivers/base/dd.c`、`drivers/base/driver.c`、`drivers/base/core.c` | `drivers/pci/pci-driver.c`、`drivers/base/platform.c`、`drivers/usb/core/driver.c` | 各子系统的 `xxx_operations` | `kernel/notifier.c` |
| 与 04 篇的关系 | **04 是本篇的"客户"**：`pci_register_driver()` 最终调用本篇的 `driver_register()` | 04 依赖 09 提供的通用注册/匹配基础设施 | 独立机制 | 独立机制，但 `bus_type` 内部也用 `blocking_notifier_head` 广播 `BUS_NOTIFY_*` 事件（见 Example 1） |

一句话：**04 篇回答"一个具体总线类型的驱动模型长什么样"，09 篇回答"为什么所有总线类型的驱动模型
长得都很像"**——答案就是它们共享同一套 `bus_type`/`device_driver`/`device` 元框架，
真正因总线而异的代码量被压缩到一个函数指针（`match`）的实现体里。理解了 09 篇，
就能看懂为什么 Kernel 新增一种总线类型（哪怕是一种全新发明的、之前从未存在过的总线协议）
只需要写 `match`/`uevent`/`probe` 这几个回调，而不需要重新发明一套"设备如何被发现、
驱动如何被卸载、sysfs 节点如何维护"的完整机制。

下一篇 [10-scheduler-class.md](10-scheduler-class.md) 转向另一个完全不同的场景：
**当"多个实现"不是被动等待被匹配，而是要按固定优先级链，主动依次尝试"接不接这个任务"，
直到有人接受为止时，用什么手法？**
