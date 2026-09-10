# 04. Driver Registration（pci_driver / platform_driver / usb_driver / i2c_driver / spi_driver）

> 前三篇分别讲了 Callback（01）、Ops Table（02）、Context Injection（03）。
> 本篇把三者合体，加上"匹配（match）"逻辑，就是 Kernel 设备驱动模型的完整范式。

---

## 1. Why

一个物理总线（PCI/USB/I2C/SPI/platform）上可能插着任意组合、任意数量的设备，而驱动是**独立编译、
独立加载**的模块。Core（总线子系统）在编译时完全不知道：

- 会插上哪些设备（型号、vendor id 事先不知道）
- 会加载哪些驱动（`insmod` 的时机、顺序不确定——设备可能先于驱动出现，也可能驱动先加载）

如果不用 Driver Registration 这套机制，唯一的替代方案是**总线代码里写死每一种设备/驱动的组合**——
这不仅违反开闭原则，在物理上也不可能：Kernel 编译时，某个 USB 网卡驱动可能还没被写出来。

设计目标：

> **把"设备"和"驱动"都做成运行期可以任意顺序出现/消失的独立实体，Core 只负责在两者出现时"撮合"（match），
> 撮合成功后调用驱动的 `probe()`，把 Ops Table + Context 一次性注入进设备。**

这是三篇内容的合体：

```
01 Callback         -> probe()/remove() 本身就是一对回调
02 Ops Table         -> struct pci_driver 里的 probe/remove/suspend/resume 是一整套 ops
03 Context Injection -> probe() 成功后，驱动通过 dev_set_drvdata() 把私有状态注入到 device 上
04（本篇）新增的部分   -> "匹配"这一步：谁负责决定"这个驱动能不能处理这个设备"
```

---

## 2. Architecture

```
                     device                 driver
                 (硬件插上/枚举到)      (insmod/模块加载注册)
                        |                      |
                        |  register_device()   |  register_driver()
                        v                      v
                  +---------------------------------+
                  |              bus                |   <-- Core，match 逻辑所在
                  |  device 列表           driver 列表 |
                  +---------------------------------+
                        |
                        |  bus->match(device, driver) 逐一尝试
                        v
                  匹配成功 -> driver->probe(device)
                        |
                        v
                  device 与 driver 绑定
                  (device->driver = drv; drv 可以 dev_set_drvdata 注入私有数据)
```

对应真实 Kernel 结构体（均为本仓库真实定义）：

```
struct pci_driver {                              struct platform_driver {
    struct list_head node;                           int (*probe)(struct platform_device *);
    const char *name;                                int (*remove)(struct platform_device *);
    const struct pci_device_id *id_table;            int (*suspend)(...);
    int (*probe)(struct pci_dev *, ...);             int (*resume)(...);
    void (*remove)(struct pci_dev *);                struct device_driver driver;
    ...                                              const struct platform_device_id *id_table;
    struct device_driver driver;                 };
    struct pci_dynids dynids;
};
```
（见 [include/linux/pci.h:551](../../include/linux/pci.h)、
[include/linux/platform_device.h:164](../../include/linux/platform_device.h)）

注意两者都内嵌了 `struct device_driver driver`——这正是 03 篇 Context Injection 的手法：
`pci_driver`/`platform_driver` 是对通用 `device_driver` 的"业务特化"，`to_pci_driver()` 用
`container_of` 反推：`#define to_pci_driver(drv) container_of(drv, struct pci_driver, driver)`。

---

## 3. Lifecycle

```
   allocate
   (定义 static struct pci_driver my_driver = { .probe = ..., .id_table = ... })
        |
        v
   initialize
   (id_table 填好能处理的 vendor/device id 列表)
        |
        v
   inject（注册）
   (pci_register_driver(&my_driver) —— 等价于本文 core_register_driver())
        |
        v
   framework owns dispatch（匹配）
   (bus 扫描已存在的设备，或者等待新设备 hot-plug 出现时反向扫描已注册驱动)
        |
        v
   callback（probe）
   (匹配成功 -> my_driver.probe(pci_dev, id) 被调用，驱动在这里做初始化、
    注入私有数据 pci_set_drvdata(pdev, priv))
        |
        v
   [设备在用中：driver 的其它 ops（如 suspend/resume/ioctl）会被陆续调用]
        |
        v
   destroy（remove / unregister）
   (设备被拔出 -> my_driver.remove(pci_dev) 被调用；
    或驱动被卸载 -> pci_unregister_driver()，Core 对所有已绑定设备逐一调用 remove())
```

---

## 4. Relationship

```
   device                              driver
     |                                    |
     | 绑定后互相持有对方引用                |
     v                                    v
   device->driver ----------------> struct device_driver
     ^                                    |
     |                                    | 内嵌于（container_of 反推）
     |                                    v
     |                             struct pci_driver / platform_driver
     |                                    |
     |                                    | probe() 时调用
     +-------- dev_set_drvdata() <--------+
       (驱动的私有数据被注入回 device 对象，
        后续 dev_get_drvdata() 取回)
```

---

## 5. Data Flow

以 `bus_driver_match_demo.c` 的"设备先出现，驱动后注册"路径为例：

```
main()
  |
  +- bus_add_device(&pci_bus, &dev_nic1)     // 设备先出现，此时 bus 没有驱动，匹配失败
  |     |
  |     v
  |  bus_try_bind() -> 遍历 0 个驱动 -> "no driver matched"
  |
  +- bus_add_driver(&pci_bus, &nic_driver)    // 驱动后注册
        |
        v
     反向扫描 bus->devices[]，对每个未绑定设备调用 bus_try_bind()
        |
        v
     bus->match(dev_nic1, nic_driver)  ->  比较 dev->model 与 id_table
        |  匹配成功
        v
     nic_driver.probe(nic_driver, dev_nic1)   // <-- DI 真正发生的地方
        |
        v
     dev_nic1.bound_driver = &nic_driver
```

对照真实 Kernel（`drivers/base/bus.c` / `drivers/pci/pci-driver.c`，本仓库版本简化示意）：

```
pci_register_driver(&my_driver)
  |
  v
driver_register(&my_driver.driver)
  |
  v
bus_add_driver(bus, drv)
  |
  v
driver_attach(drv)  ->  对 bus 上每个 device 调用 __driver_attach()
  |
  v
driver_match_device(drv, dev)   // 内部调用 bus->match，PCI 是按 vendor/device id 比较 id_table
  |  匹配成功
  v
really_probe(dev, drv)  ->  drv->probe(dev)   // pci_driver->probe()
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `id_table`（驱动能处理的型号列表） | **注册前固定，注册后基本不变** | 但 Kernel 也支持"动态 id"（`pci_dynids`），可以运行期追加 |
| `device->bound_driver`（绑定关系） | **运行期变化**，由 probe 成功/remove 触发 | 一个设备任意时刻最多绑定一个驱动 |
| `driver_data` / `private_data`（驱动私有状态） | **在 probe 时分配，remove 时释放** | 归属驱动，Core 不感知内容 |
| 驱动注册表本身（`bus->drivers[]`） | **运行期动态增减** | 支持热插拔，新驱动随时可以加入并反向匹配已有设备 |
| 引用计数（`refcount`，见 9.2 示例） | **运行期动态变化** | 决定"能否安全卸载"，是热插拔安全性的关键状态 |

---

## 7. Failure

1. **设备/驱动注册顺序竟然影响行为**：`bus_driver_match_demo.c` 显式验证了两种顺序（设备先/驱动先）都能
   正确匹配——这是因为 Core 同时在 `bus_add_device()` 和 `bus_add_driver()` 里都触发了 `bus_try_bind()`。
   **常见错误**：只在一侧触发扫描（比如只在加设备时扫描驱动，忘了加驱动时反向扫描设备），
   会导致"先加载驱动再插设备"的场景下设备永远绑定不上。
2. **多个驱动都能匹配同一设备，顺序敏感**：`multi_driver_probe.c` 演示了`vendor_driver`（priority 更高）
   总是先于 `generic_driver` 被尝试。如果注册顺序/优先级设计不当，可能出现"通用驱动先抢到了本该由专用驱动
   处理的设备"，专用驱动的优化/修复永远用不上。
3. **probe 失败后没有正确回滚**：如果 `probe()` 中途分配了资源（内存、IRQ、DMA 缓冲）但后续步骤失败返回
   非 0，必须完整释放已分配的资源，否则每次"尝试-失败"都会泄漏——这是 Kernel `probe()` 实现里最容易出错、
   review 最关注的地方。
4. **refcount 保护缺失导致"正在使用中被卸载"**：`plugin_registry_hotplug.c` 显式演示了没有 refcount
   保护会怎样：`registry_unregister()` 如果不检查 `refcount`，会在设备正在被使用时把驱动摘除，
   后续对 `driver_data` 的访问就是悬挂指针——这正是 Kernel `try_module_get()`/`module_put()` 存在的原因。
5. **remove() 未清理绑定状态**：`bus_remove_driver()` 忘记把 `dev->bound_driver` 置回 `NULL`，
   会导致设备"看起来还绑定着一个已经不存在的驱动"，后续任何基于 `bound_driver` 的判断都会出错。
6. **调试方法**：
   - 用户态：给注册表/绑定关系加日志（如三个示例中大量 `printf`），配合 `gdb` 断点单步观察 `bus_try_bind`
     的匹配顺序。
   - Kernel：`/sys/bus/<bus>/devices/*/driver` 和 `/sys/bus/<bus>/drivers/*/` 可以直接查看当前绑定关系；
     `driver_probe_device()` 路径上的 `dev_dbg()`/`dynamic debug`（`echo 'file drivers/base/dd.c +p' >
     /sys/kernel/debug/dynamic_debug/control`）可以打开详细匹配日志；`lsmod`/`modinfo` 查看模块引用计数
     （对应本文 `refcount`）。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `pci_register_driver()` -> `driver_register()` -> `bus_add_driver()`

真实定义：[include/linux/pci.h:551](../../include/linux/pci.h)

```c
struct pci_driver {
	struct list_head node;
	const char *name;
	const struct pci_device_id *id_table;
	int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);
	void (*remove)(struct pci_dev *dev);
	struct device_driver driver;
};

/* driver 侧：注入 */
static struct pci_driver my_nic_driver = {
	.name = "my_nic", .id_table = my_nic_ids, .probe = my_nic_probe, .remove = my_nic_remove,
};
module_init(my_nic_driver_init) { return pci_register_driver(&my_nic_driver); }

/* core 侧：drivers/pci/pci-driver.c 简化 */
int pci_register_driver(struct pci_driver *drv)
{
	drv->driver.bus = &pci_bus_type;
	return driver_register(&drv->driver);   /* 委托给通用 device_driver 注册逻辑 */
}
```

### Example 2 — `driver_register()` -> `driver_attach()` 反向扫描设备

真实位置：[drivers/base/driver.c:222](../../drivers/base/driver.c)

```c
/* core: drivers/base/driver.c 简化 */
int driver_register(struct device_driver *drv)
{
	BUG_ON(!drv->bus->p);
	/* ... 冲突检查 ... */
	return bus_add_driver(drv);
}

/* core: drivers/base/bus.c 简化 */
int bus_add_driver(struct device_driver *drv)
{
	priv->driver = drv;
	list_add_tail(&priv->klist_drivers.n_node, &bus->p->klist_drivers.k_list);
	driver_attach(drv);   /* <-- 反向扫描 bus 上所有 device，尝试匹配 */
	return 0;
}
```

### Example 3 — `driver_match_device()`（match 策略本身也是可注入的）

```c
/* core: drivers/base/bus.c 简化 */
static int __driver_attach(struct device *dev, void *data)
{
	struct device_driver *drv = data;

	if (!driver_match_device(drv, dev))   /* <-- bus->match(dev, drv)，PCI/USB/I2C 各自实现 */
		return 0;

	return driver_probe_device(drv, dev);   /* 匹配成功才 probe */
}
```
为什么是 DI：`match` 函数本身也是"总线类型"的注入点——PCI 按 vendor/device id 比较，
USB 按 `idVendor`/`idProduct`，platform 按设备名字符串，I2C 按 `i2c_device_id`——Core（`__driver_attach`）
完全不关心具体比较逻辑，只调用 `bus->match()`。

### Example 4 — `really_probe()` -> `drv->probe(dev)` + 私有数据注入

```c
/* core: drivers/base/dd.c 简化 */
static int really_probe(struct device *dev, struct device_driver *drv)
{
	dev->driver = drv;   /* 先建立绑定关系 */
	if (dev->bus->probe)
		ret = dev->bus->probe(dev);        /* 有些总线自己包一层 */
	else if (drv->probe)
		ret = drv->probe(dev);             /* <-- 最终盲调到驱动的 probe() */
	if (ret)
		dev->driver = NULL;                /* 失败要回滚绑定关系 */
	return ret;
}

/* 驱动侧 probe() 内部典型写法 */
static int my_nic_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct my_nic_priv *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	pci_set_drvdata(pdev, priv);   /* Context Injection：把私有数据挂回 device */
	return 0;
}
```

### Example 5 — `driver_detach()` / `pci_unregister_driver()`：卸载时的清理

```c
/* core: drivers/base/dd.c 简化 */
static void __device_release_driver(struct device *dev)
{
	struct device_driver *drv = dev->driver;
	if (drv->remove)
		drv->remove(dev);          /* <-- 盲调 remove，驱动自己释放资源 */
	dev->driver = NULL;              /* 清理绑定关系，防止悬挂引用 */
}

void pci_unregister_driver(struct pci_driver *drv)
{
	driver_unregister(&drv->driver);   /* 内部对所有已绑定设备调用 __device_release_driver */
}
```

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/04-driver-registration/`](c/04-driver-registration/)

### 9.1 `bus_driver_match_demo.c` — bus/device/driver 匹配注册

- 场景：模拟 `struct bus_type` 的 match 撮合逻辑，验证"设备先/驱动先"两种注册顺序都能正确匹配。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o bus_driver_match_demo bus_driver_match_demo.c`
- 实测输出（节选，完整见运行记录）：
  ```
  === bus_driver_match_demo ===

  -- 阶段 1：先注册设备，此时还没有驱动，无法匹配 --
  [bus:pci] device 'eth0' registered
  [bus:pci] no driver matched device 'eth0' (model=NIC-8086)
  ...
  -- 阶段 2：注册网卡驱动，反向扫描已存在的设备 --
  [bus:pci] driver 'e1000e' registered (id_table has 2 entries)
  [bus:pci] matched device 'eth0' (model=NIC-8086) with driver 'e1000e'
      [nic_driver:e1000e] probing 'eth0', allocating ring buffers
    -> probe() succeeded, device bound
  ...
  -- 阶段 5：卸载网卡驱动，观察绑定设备被 remove --
      [nic_driver:e1000e] removing 'eth0', freeing ring buffers
  [bus:pci] device 'eth0' unbound from removed driver 'e1000e'
      [nic_driver:e1000e] removing 'eth1', freeing ring buffers
  [bus:pci] device 'eth1' unbound from removed driver 'e1000e'
  [bus:pci] driver 'e1000e' unregistered

  === done ===
  ```
- 为什么是 DI：`bus_try_bind()`（Core）从不知道具体是网卡还是存储控制器，只调用 `bus->match()` 和
  `drv->probe()`；新增一种设备类型/驱动完全不需要修改 `bus_add_device`/`bus_add_driver` 代码。

### 9.2 `plugin_registry_hotplug.c` — 支持热插拔的驱动注册表

- 场景：模拟 `insmod`/`rmmod` 语义，用 `refcount` 保护"正在使用中的驱动不能被卸载"。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o plugin_registry_hotplug plugin_registry_hotplug.c`
- 实测输出：
  ```
  [registry] refuse to unregister 'mem-allocator', refcount=1 (still in use)
  === plugin_registry_hotplug demo ===

  [registry] driver 'mem-allocator' v1 registered (hot-plugged in)
  [registry] driver 'logger' v1 registered (hot-plugged in)

  -- 使用 mem-allocator，保持句柄不释放 --
  [registry] 'mem-allocator' refcount -> 1 (get)
      [mem-allocator] opened handle, size=4096
      [mem-allocator] using handle tag='mem-handle' size=4096

  -- 尝试卸载 mem-allocator（应该失败，因为 refcount > 0）--

  -- 归还引用后再卸载（应该成功）--
      [mem-allocator] closing handle
  [registry] 'mem-allocator' refcount -> 0 (put)
  [registry] driver 'mem-allocator' unregistered (hot-plugged out)

  -- logger 热升级：v1 使用完毕后卸载，再装 v2 --
  [registry] 'logger' refcount -> 1 (get)
      [logger] opened log session
      [logger] writing log entry
      [logger] closed log session
  [registry] 'logger' refcount -> 0 (put)
  [registry] driver 'logger' unregistered (hot-plugged out)
  [registry] driver 'logger' v2 registered (hot-plugged in)
  [registry] 'logger' refcount -> 1 (get)
      now running logger v2
  [registry] 'logger' refcount -> 0 (put)

  === done, 1 driver(s) still registered ===
  ```
  （首行提前刷出的 stderr 告警正是"拒绝卸载正在使用中的驱动"这一预期防御逻辑生效的证明。）
- 为什么是 DI：`registry_get`/`registry_put`/`registry_unregister`（Core）不知道 `mem-allocator`/`logger`
  内部做什么，只维护"名字 -> 驱动对象 + refcount"的映射；驱动的热升级（v1 -> v2）对 Core 逻辑零影响。

### 9.3 `multi_driver_probe.c` — 多驱动竞争 probe 同一设备

- 场景：`vendor_driver`（priority=1）、`network_driver`（priority=5）、`generic_driver`（priority=10）
  按优先级顺序竞争 probe，验证类别不匹配自动跳过、优先级高的驱动优先接管。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o multi_driver_probe multi_driver_probe.c`
- 实测输出：
  ```
  === multi_driver_probe demo ===

  [core] registered driver 'generic-usb-storage' (priority=10, generic=1)
  [core] registered driver 'network-driver' (priority=5, generic=0)
  [core] registered driver 'vendor-usb-storage' (priority=1, generic=0)

  -- probe 普通 USB 存储设备：vendor_driver（priority=1）先尝试并成功接管 --
  [core] probing device 'usb-disk-A' (class=usb-storage) against 3 driver(s)
    [core] trying driver 'vendor-usb-storage' (priority=1)...
      [vendor_driver] doing vendor-specific init for 'usb-disk-A'
    [core] 'vendor-usb-storage' claimed the device

  -- probe 有 quirk 的 USB 设备：vendor_driver 优先，也接管成功（vendor 驱动无视 quirk）--
  [core] probing device 'usb-disk-B' (class=usb-storage) against 3 driver(s)
    [core] trying driver 'vendor-usb-storage' (priority=1)...
      [vendor_driver] doing vendor-specific init for 'usb-disk-B'
    [core] 'vendor-usb-storage' claimed the device

  -- probe 网卡设备：vendor/network 都会被尝试，vendor 因 class 不符先失败，network 接管 --
  [core] probing device 'eth0' (class=network) against 3 driver(s)
    [core] trying driver 'vendor-usb-storage' (priority=1)...
    [core] 'vendor-usb-storage' declined (ret=-1), trying next
    [core] trying driver 'network-driver' (priority=5)...
      [network_driver] claiming network device 'eth0'
    [core] 'network-driver' claimed the device

  -- 移除设备 A 的绑定 --

  === done ===
  dev_normal_usb bound to: vendor-usb-storage
  dev_quirky_usb bound to: vendor-usb-storage
  dev_nic bound to:        network-driver
  ```
  （驱动注册时故意打乱顺序注册，`core_register_driver()` 内部按 `priority` 插入排序，
  验证了"注册顺序不等于 probe 尝试顺序，真正决定顺序的是显式的优先级"。）
- 为什么是 DI：`core_probe_device()`（Core）不知道也不关心到底是几个驱动、谁的优先级更高，
  只是机械地按已排好序的列表依次调用 `d->probe(d, dev)`，第一个返回 0 的就"胜出"——这正是
  Kernel 里"一个设备可能被多种驱动的 id_table 同时命中"场景的真实建模。

一次性编译并运行全部三个示例：`cd c/04-driver-registration && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Driver Registration | Ops Table | Context Injection | Notifier Chain |
|---|---|---|---|---|
| 注入对象 | 整个"设备驱动实体"（ops + 元数据 + 匹配规则） | 一组相关函数指针 | 单个私有数据/嵌入字段 | 单个回调 + 优先级 |
| 是否需要"匹配"逻辑 | **是**，独有特征——需要 `match()` 决定谁负责谁 | 否，创建对象时直接指定 | 否 | 否，广播给所有订阅者 |
| 典型触发时机 | 设备热插拔、模块加载/卸载 | 对象创建 | 对象创建/回调注册 | 事件发生 |
| 支持"多个实现竞争同一目标" | **是**，核心特性（见 9.3 示例） | 否，一对一 | 不适用 | 否，是"广播"而非"竞争" |
| 何时用 Driver Registration | 需要支持"运行期任意组合的多对多匹配"（N 种设备 × M 种驱动），且要有生命周期管理（probe/remove）| 只需要固定绑定一份实现 | — | — |

一句话：**Driver Registration 是 DI 复杂度的"天花板"**——它把 Callback（probe/remove）、
Ops Table（一整套驱动操作）、Context Injection（driver_data）三种手法组合起来，再加上"匹配"和
"引用计数"两个新维度，用来解决"运行期任意多个实现 × 任意多个目标"的动态绑定问题。
下一篇 [05-notifier-chain.md](05-notifier-chain.md) 转向另一个维度：**当一个事件需要广播给不确定数量的
"订阅者"而不是"精确匹配一个实现"时，用什么手法？**
