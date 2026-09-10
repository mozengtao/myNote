/*
 * multi_bus_types_demo.c
 *
 * 场景：证明"总线无关核心"（generic_bus_core.c 的思路）可以同时服务多种
 * 完全不同的总线类型，且互不干扰——这正是真实 Kernel 里 pci_bus_type、
 * usb_bus_type、platform_bus_type 共用同一份 drivers/base/bus.c 逻辑，
 * 只是各自的 bus->match() 实现不同的原因。
 *
 * 本示例同时跑两条独立的总线：
 *   - "virtual-pci" 总线：按 (vendor_id, device_id) 数值精确匹配
 *     （模拟真实 struct pci_device_id 的比较方式）
 *   - "virtual-usb" 总线：按 class 字符串 + 一个 quirk 位图匹配
 *     （模拟真实 usb_driver->id_table 的 bInterfaceClass 比较方式）
 *
 * 两条总线各自维护独立的 device[]/driver[] 数组，验证：
 *   1. 同一个 core_register_device()/core_register_driver() 函数被两种
 *      总线复用，核心代码只有一份。
 *   2. virtual-pci 上注册的驱动，绝不会被拿去匹配 virtual-usb 上的设备
 *      （因为它们是两个独立的 bus 实例，核心从不跨 bus 扫描）。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o multi_bus_types_demo multi_bus_types_demo.c
 * 运行：./multi_bus_types_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENTITIES 8

struct bus;
struct dev_entity;
struct drv_entity;

/* Core 契约：每种总线类型注入自己的 match 实现 */
typedef int (*match_fn_t)(struct dev_entity *dev, struct drv_entity *drv);
typedef int (*probe_fn_t)(struct dev_entity *dev, struct drv_entity *drv);

struct dev_entity {
	char label[32];
	int  id_a, id_b;         /* 通用型号字段，不同总线赋予不同语义 */
	struct drv_entity *bound;
};

struct drv_entity {
	char label[32];
	int  match_a, match_b;   /* 通用匹配字段 */
	probe_fn_t probe;
};

/* struct bus —— 完全总线无关的 Core 数据结构，对应 struct bus_type */
struct bus {
	char name[32];
	match_fn_t match;
	struct dev_entity *devices[MAX_ENTITIES];
	int nr_devices;
	struct drv_entity *drivers[MAX_ENTITIES];
	int nr_drivers;
};

/* ------------------------------------------------------------------ */
/* Core：与总线类型完全无关，两条总线共用这两个函数                     */
/* ------------------------------------------------------------------ */

static void core_scan_and_bind(struct bus *bus, struct dev_entity *dev)
{
	int i;
	for (i = 0; i < bus->nr_drivers; i++) {
		struct drv_entity *drv = bus->drivers[i];
		if (dev->bound)
			return;
		if (bus->match(dev, drv)) {
			if (drv->probe(dev, drv) == 0) {
				dev->bound = drv;
				printf("  [core:%s] '%s' bound to driver '%s'\n",
				       bus->name, dev->label, drv->label);
			}
		}
	}
	if (!dev->bound)
		printf("  [core:%s] '%s' has no matching driver (yet)\n", bus->name, dev->label);
}

static void core_register_device(struct bus *bus, struct dev_entity *dev)
{
	bus->devices[bus->nr_devices++] = dev;
	printf("[bus:%s] device '%s' registered (id_a=%d id_b=%d)\n",
	       bus->name, dev->label, dev->id_a, dev->id_b);
	core_scan_and_bind(bus, dev);
}

static void core_register_driver(struct bus *bus, struct drv_entity *drv)
{
	int i;
	bus->drivers[bus->nr_drivers++] = drv;
	printf("[bus:%s] driver '%s' registered (match_a=%d match_b=%d)\n",
	       bus->name, drv->label, drv->match_a, drv->match_b);
	for (i = 0; i < bus->nr_devices; i++)
		core_scan_and_bind(bus, bus->devices[i]);
}

/* ------------------------------------------------------------------ */
/* 总线类型 1：virtual-pci —— 按 (vendor_id, device_id) 精确匹配         */
/* ------------------------------------------------------------------ */

static int pci_match(struct dev_entity *dev, struct drv_entity *drv)
{
	return dev->id_a == drv->match_a && dev->id_b == drv->match_b;
}

static int pci_probe(struct dev_entity *dev, struct drv_entity *drv)
{
	(void)drv;
	printf("      [pci-probe] init BAR mappings for '%s'\n", dev->label);
	return 0;
}

/* ------------------------------------------------------------------ */
/* 总线类型 2：virtual-usb —— 按 class 数值匹配（id_a 复用为 class 编号） */
/* ------------------------------------------------------------------ */

static int usb_match(struct dev_entity *dev, struct drv_entity *drv)
{
	/* 只比较 class（id_a），quirk（id_b）无需相等，模拟"驱动兼容一大类设备" */
	return dev->id_a == drv->match_a;
}

static int usb_probe(struct dev_entity *dev, struct drv_entity *drv)
{
	(void)drv;
	printf("      [usb-probe] claiming interface for '%s'\n", dev->label);
	return 0;
}

int main(void)
{
	struct bus vpci = { .name = "virtual-pci", .match = pci_match };
	struct bus vusb = { .name = "virtual-usb", .match = usb_match };

	struct dev_entity pci_nic  = { .label = "pci-nic0",  .id_a = 0x8086, .id_b = 0x1000 };
	struct dev_entity pci_disk = { .label = "pci-disk0", .id_a = 0x1234, .id_b = 0x5678 };
	struct drv_entity pci_drv  = { .label = "e1000e", .match_a = 0x8086, .match_b = 0x1000, .probe = pci_probe };

	struct dev_entity usb_cam  = { .label = "usb-cam0", .id_a = 14 /* video class */, .id_b = 1 };
	struct dev_entity usb_mouse = { .label = "usb-mouse0", .id_a = 3 /* HID class */, .id_b = 0 };
	struct drv_entity usb_drv  = { .label = "uvcvideo", .match_a = 14, .match_b = 0, .probe = usb_probe };

	printf("=== multi_bus_types_demo ===\n\n");

	printf("-- 在 virtual-pci 总线上注册设备 + 驱动 --\n");
	core_register_device(&vpci, &pci_nic);
	core_register_device(&vpci, &pci_disk);
	core_register_driver(&vpci, &pci_drv);

	printf("\n-- 在 virtual-usb 总线上注册设备 + 驱动（完全独立的核心实例）--\n");
	core_register_device(&vusb, &usb_cam);
	core_register_device(&vusb, &usb_mouse);
	core_register_driver(&vusb, &usb_drv);

	printf("\n-- 验证：同一份 core_register_driver() 代码服务了两种完全不同的总线 --\n");
	printf("  vpci.nr_drivers=%d, vusb.nr_drivers=%d (两条总线各自独立计数)\n",
	       vpci.nr_drivers, vusb.nr_drivers);

	printf("\n=== 最终绑定结果 ===\n");
	printf("pci_nic    bound to: %s\n", pci_nic.bound ? pci_nic.bound->label : "(none)");
	printf("pci_disk   bound to: %s (预期未匹配到任何驱动)\n", pci_disk.bound ? pci_disk.bound->label : "(none)");
	printf("usb_cam    bound to: %s\n", usb_cam.bound ? usb_cam.bound->label : "(none)");
	printf("usb_mouse  bound to: %s (预期未匹配到任何驱动)\n", usb_mouse.bound ? usb_mouse.bound->label : "(none)");

	printf("\n=== done ===\n");
	return 0;
}
