/*
 * bus_driver_match_demo.c
 *
 * 场景：模拟 Kernel 的 bus/device/driver 三元关系与匹配机制
 * （drivers/base/bus.c bus_add_driver() / driver_register() 的简化版）：
 *
 *   device 出现  ---注册到 bus--->  bus 维护 device 列表
 *   driver 出现  ---注册到 bus--->  bus 维护 driver 列表
 *                     │
 *                     ▼
 *              bus->match(device, driver)   逐一尝试匹配
 *                     │ 匹配成功
 *                     ▼
 *              driver->probe(device)         <-- 真正的 DI 发生点
 *
 * Core（bus）完全不知道具体是什么设备、什么驱动，只负责"撮合"。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o bus_driver_match_demo bus_driver_match_demo.c
 * 运行：
 *   ./bus_driver_match_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 16

struct mini_device;
struct mini_driver;

/* ------------------------------------------------------------------ */
/* 1. 契约：driver 必须实现的操作，对应 struct device_driver + probe   */
/* ------------------------------------------------------------------ */

struct mini_driver {
	char name[32];
	char id_table[4][32];   /* 该驱动能处理的 device 型号列表（简化版 id_table） */
	int  id_count;

	int  (*probe)(struct mini_driver *self, struct mini_device *dev);
	void (*remove)(struct mini_driver *self, struct mini_device *dev);
};

struct mini_device {
	char name[32];
	char model[32];         /* 用于匹配的型号字符串，对应 pci_device_id/of_device_id */
	struct mini_driver *bound_driver;   /* 匹配成功后回填 */
	void *driver_data;                   /* probe() 期间驱动可以存自己的私有状态 */
};

/* ------------------------------------------------------------------ */
/* 2. Core：bus，负责注册表管理 + match 撮合逻辑                        */
/* ------------------------------------------------------------------ */

struct mini_bus {
	char name[32];
	struct mini_device *devices[MAX_ITEMS];
	int nr_devices;
	struct mini_driver *drivers[MAX_ITEMS];
	int nr_drivers;

	/* bus->match：Core 的核心策略函数，本身也是一个可注入点，
	 * 但这里简化为固定策略（按 model 字符串匹配 id_table） */
	int (*match)(struct mini_device *dev, struct mini_driver *drv);
};

static int default_match(struct mini_device *dev, struct mini_driver *drv)
{
	int i;
	for (i = 0; i < drv->id_count; i++) {
		if (strcmp(dev->model, drv->id_table[i]) == 0)
			return 1;
	}
	return 0;
}

static void bus_try_bind(struct mini_bus *bus, struct mini_device *dev)
{
	int i;
	for (i = 0; i < bus->nr_drivers; i++) {
		struct mini_driver *drv = bus->drivers[i];
		if (bus->match(dev, drv)) {
			printf("[bus:%s] matched device '%s' (model=%s) with driver '%s'\n",
			       bus->name, dev->name, dev->model, drv->name);
			if (drv->probe(drv, dev) == 0) {
				dev->bound_driver = drv;
				printf("  -> probe() succeeded, device bound\n");
			} else {
				printf("  -> probe() failed, trying next driver\n");
				continue;
			}
			return;
		}
	}
	printf("[bus:%s] no driver matched device '%s' (model=%s)\n",
	       bus->name, dev->name, dev->model);
}

/* device_register()：设备出现时，尝试跟已注册的所有驱动匹配 */
static void bus_add_device(struct mini_bus *bus, struct mini_device *dev)
{
	bus->devices[bus->nr_devices++] = dev;
	printf("[bus:%s] device '%s' registered\n", bus->name, dev->name);
	bus_try_bind(bus, dev);
}

/* driver_register()：驱动出现时，反过来扫描所有未绑定的设备 */
static void bus_add_driver(struct mini_bus *bus, struct mini_driver *drv)
{
	int i;
	bus->drivers[bus->nr_drivers++] = drv;
	printf("[bus:%s] driver '%s' registered (id_table has %d entries)\n",
	       bus->name, drv->name, drv->id_count);

	for (i = 0; i < bus->nr_devices; i++) {
		if (!bus->devices[i]->bound_driver)
			bus_try_bind(bus, bus->devices[i]);
	}
}

static void bus_remove_driver(struct mini_bus *bus, struct mini_driver *drv)
{
	int i;
	for (i = 0; i < bus->nr_devices; i++) {
		struct mini_device *dev = bus->devices[i];
		if (dev->bound_driver == drv) {
			drv->remove(drv, dev);
			dev->bound_driver = NULL;
			printf("[bus:%s] device '%s' unbound from removed driver '%s'\n",
			       bus->name, dev->name, drv->name);
		}
	}
	for (i = 0; i < bus->nr_drivers; i++) {
		if (bus->drivers[i] == drv) {
			bus->drivers[i] = bus->drivers[--bus->nr_drivers];
			break;
		}
	}
	printf("[bus:%s] driver '%s' unregistered\n", bus->name, drv->name);
}

/* ------------------------------------------------------------------ */
/* 3. 驱动实现 A：网卡驱动（能处理两种型号）                            */
/* ------------------------------------------------------------------ */

static int nic_probe(struct mini_driver *self, struct mini_device *dev)
{
	int *counter = malloc(sizeof(int));
	*counter = 0;
	dev->driver_data = counter;
	printf("    [nic_driver:%s] probing '%s', allocating ring buffers\n", self->name, dev->name);
	return 0;
}

static void nic_remove(struct mini_driver *self, struct mini_device *dev)
{
	printf("    [nic_driver:%s] removing '%s', freeing ring buffers\n", self->name, dev->name);
	free(dev->driver_data);
	dev->driver_data = NULL;
}

static struct mini_driver nic_driver = {
	.name = "e1000e",
	.id_table = { "NIC-8086", "NIC-8087" },
	.id_count = 2,
	.probe = nic_probe,
	.remove = nic_remove,
};

/* ------------------------------------------------------------------ */
/* 4. 驱动实现 B：存储控制器驱动                                       */
/* ------------------------------------------------------------------ */

static int storage_probe(struct mini_driver *self, struct mini_device *dev)
{
	printf("    [storage_driver:%s] probing '%s', initializing DMA descriptors\n",
	       self->name, dev->name);
	return 0;
}

static void storage_remove(struct mini_driver *self, struct mini_device *dev)
{
	printf("    [storage_driver:%s] removing '%s'\n", self->name, dev->name);
}

static struct mini_driver storage_driver = {
	.name = "ahci",
	.id_table = { "SATA-CTRL-1B4B" },
	.id_count = 1,
	.probe = storage_probe,
	.remove = storage_remove,
};

int main(void)
{
	struct mini_bus pci_bus = { .name = "pci", .match = default_match };

	struct mini_device dev_nic1  = { .name = "eth0", .model = "NIC-8086" };
	struct mini_device dev_nic2  = { .name = "eth1", .model = "NIC-8087" };
	struct mini_device dev_disk  = { .name = "sda",  .model = "SATA-CTRL-1B4B" };
	struct mini_device dev_weird = { .name = "gpu0", .model = "GPU-10DE-UNKNOWN" };

	printf("=== bus_driver_match_demo ===\n\n");

	printf("-- 阶段 1：先注册设备，此时还没有驱动，无法匹配 --\n");
	bus_add_device(&pci_bus, &dev_nic1);
	bus_add_device(&pci_bus, &dev_weird);

	printf("\n-- 阶段 2：注册网卡驱动，反向扫描已存在的设备 --\n");
	bus_add_driver(&pci_bus, &nic_driver);

	printf("\n-- 阶段 3：注册第二块网卡设备，此时驱动已存在，正向匹配 --\n");
	bus_add_device(&pci_bus, &dev_nic2);

	printf("\n-- 阶段 4：注册存储设备和存储驱动 --\n");
	bus_add_device(&pci_bus, &dev_disk);
	bus_add_driver(&pci_bus, &storage_driver);

	printf("\n-- 阶段 5：卸载网卡驱动，观察绑定设备被 remove --\n");
	bus_remove_driver(&pci_bus, &nic_driver);

	printf("\n=== done ===\n");
	return 0;
}
