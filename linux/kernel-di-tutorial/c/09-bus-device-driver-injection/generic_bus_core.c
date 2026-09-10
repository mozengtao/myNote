/*
 * generic_bus_core.c
 *
 * 场景：从零实现一个"总线无关"的驱动核心（driver core），对应真实 Kernel
 * include/linux/device.h 里的 struct bus_type / struct device_driver：
 *
 *   - struct generic_bus_type：只放"契约"——match()/probe()/remove() 函数指针，
 *     不知道任何具体总线（PCI/USB/platform...）的细节。
 *   - generic_device_register()：设备先出现时，反向扫描该 bus 上所有已注册驱动。
 *   - generic_driver_register()：驱动后注册时，反向扫描该 bus 上所有已注册设备。
 *   - 二者共用同一个 try_match_and_probe()，这正是真实 Kernel
 *     drivers/base/bus.c 里 bus_add_device()/bus_add_driver() 都要触发一次
 *     "扫描配对"的原因。
 *
 * 和 04-driver-registration/bus_driver_match_demo.c 的区别：那里直接把
 * "PCI 总线"这一个具体总线的 match 逻辑写死在 demo 里；这里的重点是证明
 * "match/probe 本身也是被注入的契约"——同一份 generic_bus_type 核心代码，
 * 不同的总线只需要提供不同的函数指针实现，核心代码一行都不用改。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o generic_bus_core generic_bus_core.c
 * 运行：./generic_bus_core
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEVICES 16
#define MAX_DRIVERS 16

struct generic_device;
struct generic_driver;

/*
 * struct generic_bus_type —— 对应真实 include/linux/device.h:87 的
 * struct bus_type。只保留 DI 相关的核心字段：match 和名字。
 * 真实结构体还有 probe/remove/uevent/suspend/resume/pm 等字段，
 * 均是同样的"契约指针"模式，这里为了聚焦核心机制做了精简。
 */
struct generic_bus_type {
	const char *name;
	/* match：Core 完全不实现，由具体总线类型注入比较逻辑 */
	int (*match)(struct generic_device *dev, struct generic_driver *drv);

	struct generic_device *devices[MAX_DEVICES];
	int nr_devices;
	struct generic_driver *drivers[MAX_DRIVERS];
	int nr_drivers;
};

/* struct generic_device —— 对应 struct device，内嵌 bus 指针 + 私有数据 */
struct generic_device {
	char name[32];
	char model[32];
	struct generic_bus_type *bus;
	struct generic_driver *bound_driver; /* 对应 device->driver */
	void *driver_data;                   /* dev_set_drvdata() 的位置 */
};

/*
 * struct generic_driver —— 对应 struct device_driver。probe/remove 是
 * "被注入"的具体实现，Core 只负责在匹配成功后盲调。
 */
struct generic_driver {
	char name[32];
	struct generic_bus_type *bus;
	int (*probe)(struct generic_device *dev, struct generic_driver *drv);
	void (*remove)(struct generic_device *dev, struct generic_driver *drv);
};

/* ------------------------------------------------------------------ */
/* Core：总线无关的匹配 + 探测逻辑                                      */
/* ------------------------------------------------------------------ */

/* 对应 drivers/base/dd.c really_probe()：匹配成功后盲调 drv->probe() */
static int core_try_probe(struct generic_device *dev, struct generic_driver *drv)
{
	int ret;

	printf("  [core] trying to bind device '%s' with driver '%s'\n",
	       dev->name, drv->name);
	ret = drv->probe(dev, drv);   /* <-- 盲调，Core 不知道 probe 内部做什么 */
	if (ret == 0) {
		dev->bound_driver = drv;
		printf("  [core] bind succeeded: '%s' <-> '%s'\n", dev->name, drv->name);
	} else {
		printf("  [core] probe declined device '%s' (ret=%d)\n", dev->name, ret);
	}
	return ret;
}

/*
 * 对应 drivers/base/dd.c driver_match_device()：
 *   return drv->bus->match ? drv->bus->match(dev, drv) : 1;
 * bus->match 本身就是"被注入"的比较逻辑，Core 从不关心具体怎么比较。
 */
static int core_match(struct generic_device *dev, struct generic_driver *drv)
{
	if (!dev->bus->match)
		return 1; /* 没有 match 函数，视为总是匹配 */
	return dev->bus->match(dev, drv);
}

/* generic_device_register()：设备出现时，扫描该 bus 上所有已注册驱动 */
static int generic_device_register(struct generic_bus_type *bus, struct generic_device *dev)
{
	int i;

	dev->bus = bus;
	dev->bound_driver = NULL;
	bus->devices[bus->nr_devices++] = dev;
	printf("[bus:%s] device '%s' (model=%s) registered\n", bus->name, dev->name, dev->model);

	for (i = 0; i < bus->nr_drivers; i++) {
		struct generic_driver *drv = bus->drivers[i];
		if (core_match(dev, drv)) {
			if (core_try_probe(dev, drv) == 0)
				return 0;
		}
	}
	printf("[bus:%s] no driver matched device '%s' yet\n", bus->name, dev->name);
	return -1;
}

/* generic_driver_register()：驱动出现时，反向扫描该 bus 上所有未绑定设备 */
static int generic_driver_register(struct generic_bus_type *bus, struct generic_driver *drv)
{
	int i;

	drv->bus = bus;
	bus->drivers[bus->nr_drivers++] = drv;
	printf("[bus:%s] driver '%s' registered\n", bus->name, drv->name);

	for (i = 0; i < bus->nr_devices; i++) {
		struct generic_device *dev = bus->devices[i];
		if (dev->bound_driver)
			continue;
		if (core_match(dev, drv))
			core_try_probe(dev, drv);
	}
	return 0;
}

/* generic_driver_unregister()：卸载驱动，对所有已绑定设备调用 remove() */
static void generic_driver_unregister(struct generic_bus_type *bus, struct generic_driver *drv)
{
	int i;

	for (i = 0; i < bus->nr_devices; i++) {
		struct generic_device *dev = bus->devices[i];
		if (dev->bound_driver == drv) {
			printf("  [core] removing device '%s' from driver '%s'\n",
			       dev->name, drv->name);
			drv->remove(dev, drv);   /* <-- 盲调 remove */
			dev->bound_driver = NULL;
		}
	}
	printf("[bus:%s] driver '%s' unregistered\n", bus->name, drv->name);
}

/* ------------------------------------------------------------------ */
/* 具体总线类型的实现：模拟一种"型号字符串前缀匹配"的 PCI 风格总线       */
/* ------------------------------------------------------------------ */

static int pci_style_match(struct generic_device *dev, struct generic_driver *drv)
{
	/* 简化版：驱动名字里包含设备型号前缀就算匹配（真实 PCI 是比较 vendor/device id） */
	char prefix[16];
	size_t n = strcspn(dev->model, "-");

	if (n >= sizeof(prefix))
		n = sizeof(prefix) - 1;
	memcpy(prefix, dev->model, n);
	prefix[n] = '\0';

	return strstr(drv->name, prefix) != NULL;
}

struct nic_priv {
	int ring_size;
};

static int nic_probe(struct generic_device *dev, struct generic_driver *drv)
{
	struct nic_priv *priv = calloc(1, sizeof(*priv));
	priv->ring_size = 256;
	dev->driver_data = priv;   /* Context Injection：私有数据挂回 device */
	printf("      [driver:%s] allocated ring buffer (size=%d) for '%s'\n",
	       drv->name, priv->ring_size, dev->name);
	return 0;
}

static void nic_remove(struct generic_device *dev, struct generic_driver *drv)
{
	struct nic_priv *priv = dev->driver_data;
	printf("      [driver:%s] freeing ring buffer for '%s'\n", drv->name, dev->name);
	free(priv);
	dev->driver_data = NULL;
}

int main(void)
{
	struct generic_bus_type pci_bus = { .name = "pci", .match = pci_style_match };
	struct generic_device eth0 = { .name = "eth0", .model = "NIC-8086" };
	struct generic_device sda  = { .name = "sda",  .model = "DISK-1000" };
	struct generic_driver nic_driver = {
		.name = "nic-NIC-driver", .probe = nic_probe, .remove = nic_remove,
	};

	printf("=== generic_bus_core demo ===\n\n");

	printf("-- 阶段 1：设备先出现，此时总线上还没有驱动 --\n");
	generic_device_register(&pci_bus, &eth0);
	generic_device_register(&pci_bus, &sda);

	printf("\n-- 阶段 2：注册 nic 驱动，反向扫描已存在的设备 --\n");
	generic_driver_register(&pci_bus, &nic_driver);

	printf("\n-- 阶段 3：sda 的型号没有驱动能匹配，保持未绑定状态 --\n");
	printf("  sda.bound_driver = %s\n", sda.bound_driver ? sda.bound_driver->name : "(null)");

	printf("\n-- 阶段 4：卸载 nic 驱动，eth0 被 remove --\n");
	generic_driver_unregister(&pci_bus, &nic_driver);

	printf("\n=== done ===\n");
	printf("eth0.bound_driver (should be null after unregister) = %s\n",
	       eth0.bound_driver ? eth0.bound_driver->name : "(null)");
	return 0;
}
