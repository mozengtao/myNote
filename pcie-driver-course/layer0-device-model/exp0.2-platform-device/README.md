# Experiment 0.2 — Device / Driver / Bus Matching

## 1. Why

PCI driver 里最核心的疑问是：**为什么我只写了一个 `struct pci_driver` 和一个 `probe()` 函数，
`probe()` 就会自动被调用？** 答案就是 Device Model 的 matching 机制。本实验用最简单的
`platform` bus（而不是 PCI bus）把这个机制剥离出来单独看清楚，因为 platform bus 的匹配逻辑
比 PCI 简单得多（不需要真实硬件、不需要 Config Space），非常适合建立直觉。

**注意**：这里学 platform driver 的目的不是"以后要写 platform driver"，而是单纯为了理解
Device Model 本身。学完这个实验后，我们会在 Layer 3 看到 PCI 的匹配逻辑其实是同一套模式的变体。

## 2. Current Mental Model

来自 Experiment 0.1：模块可以被加载/卸载，`module_init()` 是入口。

## 3. New Concept

本实验唯一新增的核心概念是：**Device 和 Driver 是两个独立注册的对象，Bus 负责在它们之间做 matching，
匹配成功才会调用 `probe()`**。

## 4. Architecture

```
+----------------------------------------------------------+
|                     platform_bus_type                     |
|                                                             |
|   +----------------------+      +----------------------+   |
|   | struct platform_device|      | struct platform_driver|   |
|   | name = "my_pdev"      |      | name = "my_pdev"      |   |
|   +----------------------+      +----------------------+   |
|              |                            |                 |
|              +------------match-----------+                 |
|                   (比较 name 字符串)                         |
|                            |                                |
|                            v                                |
|                       my_probe()                            |
+----------------------------------------------------------+
```

在 PCI bus 上，这个"比较 name 字符串"的步骤，会被替换成"比较 Vendor ID / Device ID"——
但整体结构完全一样。

## 5. Data Flow

本实验没有硬件寄存器，"数据"就是 device 和 driver 各自携带的 `name` 字符串，
bus 用它来做字符串比较完成 matching。

## 6. Control Flow

```
module_init()
     |
     +--> platform_device_register(&my_pdev)   // 先注册一个 device
     |          |
     |          v
     |     bus 把它加入 device 列表，此时还没有匹配的 driver，不会调用 probe()
     |
     +--> platform_driver_register(&my_driver) // 再注册一个 driver
                |
                v
           bus 遍历已有 device 列表，发现 my_pdev.name == my_driver.driver.name
                |
                v
            调用 my_probe(pdev)
```

反过来，如果先注册 driver 再注册 device，流程是对称的：bus 在 device 注册时去遍历已有 driver 列表做匹配。
**matching 永远发生在"后到者注册"的那一刻**，这是理解 probe() 调用时机的关键。

## 7. State

```
      platform_device_register()
NO_DEVICE -------------------------> DEVICE_REGISTERED
                                           |
                                           | platform_driver_register()
                                           v
                                    DRIVER_REGISTERED (name 匹配成功)
                                           |
                                           v
                                        PROBED            (my_probe() 返回 0)
                                           |
                        platform_driver_unregister()
                                           v
                                       REMOVED            (my_remove() 被调用)
```

## 8. Code

### platform_demo.c

```c
// platform_demo.c
// 用 platform bus 演示 Device Model 的 device/driver matching 机制
// 这不是一个真实硬件驱动——device 和 driver 都在同一个模块里手工注册，
// 只是为了单独观察 "matching 触发 probe()" 这件事本身。

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#define DEV_NAME "my_pdev"

static struct platform_device *my_pdev;

/*
 * my_probe - bus 完成 matching 后调用的函数
 *
 * 对应 Mental Model 里的: Device + Driver --match--> probe()
 * 这里只打印一条日志，证明 "matching -> probe()" 这条链路真的发生了。
 */
static int my_probe(struct platform_device *pdev)
{
	pr_info("platform_demo: my_probe() called for device '%s' -> MATCHED!\n",
		pdev->name);
	return 0;
}

/*
 * my_remove - driver 被卸载 (或 device 被移除) 时调用
 */
static void my_remove(struct platform_device *pdev)
{
	pr_info("platform_demo: my_remove() called for device '%s'\n",
		pdev->name);
}

static struct platform_driver my_driver = {
	.driver = {
		.name = DEV_NAME,   /* <-- matching 时比较的就是这个字符串 */
		.owner = THIS_MODULE,
	},
	.probe  = my_probe,
	.remove = my_remove,
};

static int __init platform_demo_init(void)
{
	int ret;

	pr_info("platform_demo: registering platform_device '%s'\n", DEV_NAME);

	/* 第一步：注册一个 device。此刻还没有对应的 driver，bus 上没有 matching 发生 */
	my_pdev = platform_device_register_simple(DEV_NAME, -1, NULL, 0);
	if (IS_ERR(my_pdev))
		return PTR_ERR(my_pdev);

	pr_info("platform_demo: registering platform_driver '%s'\n", DEV_NAME);

	/* 第二步：注册 driver。此刻 bus 发现名字匹配的 device 已经存在 -> 触发 probe() */
	ret = platform_driver_register(&my_driver);
	if (ret) {
		platform_device_unregister(my_pdev);
		return ret;
	}

	return 0;
}

static void __exit platform_demo_exit(void)
{
	platform_driver_unregister(&my_driver);   /* 触发 my_remove() */
	platform_device_unregister(my_pdev);
}

module_init(platform_demo_init);
module_exit(platform_demo_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Experiment 0.2: device/driver matching on platform bus");
```

### Makefile

```makefile
obj-m += platform_demo.o

KERNELDIR ?= ../../kernel
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
```

## 9. Build

```bash
cd layer0-device-model/exp0.2-platform-device
make
```

## 10. Run

```bash
mkdir -p build
cp platform_demo.ko test.sh build/
../../common/build-initramfs.sh build initramfs.cpio.gz build/test.sh
../../common/run-qemu.sh initramfs.cpio.gz
```

## 11. Observe

```
dmesg
cat /sys/bus/platform/devices/my_pdev/modalias   # 可以看到 name 字符串
ls   /sys/bus/platform/drivers/my_pdev/          # driver 的 sysfs 目录
ls   /sys/bus/platform/devices/my_pdev/driver     # 一个指向已绑定 driver 的符号链接
                                                   # —— 这就是 "matching 成功" 的直接证据
```

## 12. Expected Result

```
[   1.111] platform_demo: registering platform_device 'my_pdev'
[   1.112] platform_demo: registering platform_driver 'my_pdev'
[   1.113] platform_demo: my_probe() called for device 'my_pdev' -> MATCHED!
```

卸载时：

```
[   2.001] platform_demo: my_remove() called for device 'my_pdev'
```

## 13. Source Code

- `drivers/base/platform.c` — `platform_driver_register()`、`platform_device_register()`，
  以及真正做字符串匹配的 `platform_match()`
- `drivers/base/bus.c` — `bus_add_driver()` / `device_attach()`，通用的 "遍历并尝试匹配" 逻辑，
  **所有 bus（包括 PCI bus）都复用这套通用代码**，只是每种 bus 自己提供一个 `match()` 回调
- `include/linux/device/bus.h` — `struct bus_type` 定义，其中 `.match` 字段就是各 bus 自定义的匹配函数指针

## 14. Failure Injection

把 driver 的 name 改成 `"my_pdev_typo"`（与 device 的名字不一致）：

```c
.driver = {
	.name = "my_pdev_typo",
	...
},
```

**现象**：`dmesg` 里只会看到两条 "registering" 日志，**永远不会**出现 `my_probe() called`。
`ls /sys/bus/platform/devices/my_pdev/driver` 会报 "No such file or directory"
（因为没有绑定的 driver，这个符号链接根本不存在）。

**结论**：`module_init()` 执行成功 ≠ `probe()` 一定会被调用。两者是完全独立的两件事，
中间隔着一层"matching"，这一层出错时，**现象是"probe 没有被调用"，但是 insmod 本身会显示成功**——
这正是调试 PCI driver 时最常见的误区之一（第 22 节排查地图里 "Device 存在但 probe 不执行" 对应的根因）。

## 15. Mental Model Summary

```
Before: 以为 module_init() 里写了逻辑就会执行
   |
New concept: Device 和 Driver 是分别注册的对象，Bus 做 matching，
             只有 matching 成功才会调用 probe()
   |
After: 理解了 probe() 调用的真正前提条件，
       这正是 Layer 3 要看的 struct pci_driver.id_table 匹配逻辑的通用版本
```
