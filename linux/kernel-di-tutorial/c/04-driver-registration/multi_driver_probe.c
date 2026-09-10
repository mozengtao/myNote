/*
 * multi_driver_probe.c
 *
 * 场景：多个驱动"竞争" probe 同一个设备 —— 对应 Kernel 里
 * 一个设备可能被多种驱动的 id_table 同时匹配到（比如通用驱动 vs
 * 厂商专用驱动都能处理同一个 PCI vendor/device id），Core 按注册顺序
 * 或优先级依次尝试 probe()，第一个 probe 成功的独占该设备。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o multi_driver_probe multi_driver_probe.c
 * 运行：
 *   ./multi_driver_probe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DRIVERS 8

struct probe_device {
	char name[32];
	char class_name[32];    /* 设备类别，比如 "usb-storage" */
	int  quirk_needs_vendor_driver;   /* 模拟"这个设备有点特殊，通用驱动会失败" */
	void *bound_data;
	char bound_driver_name[32];
};

struct probe_driver {
	char name[32];
	int  priority;           /* 数字越小优先级越高，越先尝试 */
	int  is_generic;         /* 通用驱动 vs 专用驱动 */

	/* probe 返回 0 表示接管成功，非 0 表示"这个设备我处理不了，让下一个试" */
	int (*probe)(struct probe_driver *self, struct probe_device *dev);
	void (*remove)(struct probe_driver *self, struct probe_device *dev);
};

struct probe_core {
	struct probe_driver *drivers[MAX_DRIVERS];
	int count;
};

static struct probe_core g_core = { .count = 0 };

static int core_register_driver(struct probe_driver *d)
{
	int i, pos;

	if (g_core.count >= MAX_DRIVERS)
		return -1;

	/* 按 priority 插入排序，保证 probe 尝试顺序稳定可预测 —— 这正是
	 * Kernel 驱动模型里 "驱动匹配顺序依赖注册顺序/优先级" 这个常见坑的来源 */
	pos = g_core.count;
	for (i = 0; i < g_core.count; i++) {
		if (d->priority < g_core.drivers[i]->priority) {
			pos = i;
			break;
		}
	}
	memmove(&g_core.drivers[pos + 1], &g_core.drivers[pos],
		(size_t)(g_core.count - pos) * sizeof(struct probe_driver *));
	g_core.drivers[pos] = d;
	g_core.count++;

	printf("[core] registered driver '%s' (priority=%d, generic=%d)\n",
	       d->name, d->priority, d->is_generic);
	return 0;
}

/* 核心竞争逻辑：按优先级顺序依次尝试，第一个成功 probe 的独占设备 */
static void core_probe_device(struct probe_device *dev)
{
	int i;

	printf("[core] probing device '%s' (class=%s) against %d driver(s)\n",
	       dev->name, dev->class_name, g_core.count);

	for (i = 0; i < g_core.count; i++) {
		struct probe_driver *d = g_core.drivers[i];
		int ret;

		printf("  [core] trying driver '%s' (priority=%d)...\n", d->name, d->priority);
		ret = d->probe(d, dev);
		if (ret == 0) {
			snprintf(dev->bound_driver_name, sizeof(dev->bound_driver_name), "%s", d->name);
			printf("  [core] '%s' claimed the device\n", d->name);
			return;
		}
		printf("  [core] '%s' declined (ret=%d), trying next\n", d->name, ret);
	}
	printf("  [core] no driver could handle '%s'\n", dev->name);
}

/* ------------------------------------------------------------------ */
/* 驱动 A：厂商专用驱动，priority 更高（数值更小，更优先尝试）           */
/* ------------------------------------------------------------------ */

static int vendor_probe(struct probe_driver *self, struct probe_device *dev)
{
	(void)self;
	if (strcmp(dev->class_name, "usb-storage") != 0)
		return -1;   /* 只认自己的类别 */

	printf("    [vendor_driver] doing vendor-specific init for '%s'\n", dev->name);
	dev->bound_data = malloc(64);
	return 0;   /* 总是能处理 usb-storage 类别 */
}

static void vendor_remove(struct probe_driver *self, struct probe_device *dev)
{
	(void)self;
	free(dev->bound_data);
	dev->bound_data = NULL;
}

static struct probe_driver vendor_driver = {
	.name = "vendor-usb-storage", .priority = 1, .is_generic = 0,
	.probe = vendor_probe, .remove = vendor_remove,
};

/* ------------------------------------------------------------------ */
/* 驱动 B：通用驱动，priority 更低（数值更大，靠后尝试），但覆盖面更广   */
/* ------------------------------------------------------------------ */

static int generic_probe(struct probe_driver *self, struct probe_device *dev)
{
	(void)self;
	if (dev->quirk_needs_vendor_driver) {
		/* 模拟：某些"怪异"设备通用驱动搞不定，主动放弃 */
		printf("    [generic_driver] device has quirk, refusing to claim\n");
		return -1;
	}
	printf("    [generic_driver] doing generic init for '%s'\n", dev->name);
	return 0;
}

static void generic_remove(struct probe_driver *self, struct probe_device *dev)
{
	(void)self; (void)dev;
	printf("    [generic_driver] generic cleanup\n");
}

static struct probe_driver generic_driver = {
	.name = "generic-usb-storage", .priority = 10, .is_generic = 1,
	.probe = generic_probe, .remove = generic_remove,
};

/* ------------------------------------------------------------------ */
/* 驱动 C：另一个只关心不同类别的驱动，用来验证"类别不匹配就该跳过"       */
/* ------------------------------------------------------------------ */

static int network_probe(struct probe_driver *self, struct probe_device *dev)
{
	(void)self;
	if (strcmp(dev->class_name, "network") != 0)
		return -1;
	printf("    [network_driver] claiming network device '%s'\n", dev->name);
	return 0;
}

static void network_remove(struct probe_driver *self, struct probe_device *dev)
{
	(void)self; (void)dev;
	printf("    [network_driver] cleanup\n");
}

static struct probe_driver network_driver = {
	.name = "network-driver", .priority = 5, .is_generic = 0,
	.probe = network_probe, .remove = network_remove,
};

int main(void)
{
	struct probe_device dev_normal_usb = { .name = "usb-disk-A", .class_name = "usb-storage", .quirk_needs_vendor_driver = 0 };
	struct probe_device dev_quirky_usb = { .name = "usb-disk-B", .class_name = "usb-storage", .quirk_needs_vendor_driver = 1 };
	struct probe_device dev_nic        = { .name = "eth0",       .class_name = "network",     .quirk_needs_vendor_driver = 0 };

	printf("=== multi_driver_probe demo ===\n\n");

	/* 故意乱序注册，验证按 priority 排序后 probe 顺序仍然正确 */
	core_register_driver(&generic_driver);
	core_register_driver(&network_driver);
	core_register_driver(&vendor_driver);

	printf("\n-- probe 普通 USB 存储设备：vendor_driver（priority=1）先尝试并成功接管 --\n");
	core_probe_device(&dev_normal_usb);

	printf("\n-- probe 有 quirk 的 USB 设备：vendor_driver 优先，也接管成功（vendor 驱动无视 quirk）--\n");
	core_probe_device(&dev_quirky_usb);

	printf("\n-- probe 网卡设备：vendor/network 都会被尝试，vendor 因 class 不符先失败，network 接管 --\n");
	core_probe_device(&dev_nic);

	printf("\n-- 移除设备 A 的绑定 --\n");
	vendor_remove(&vendor_driver, &dev_normal_usb);

	printf("\n=== done ===\n");
	printf("dev_normal_usb bound to: %s\n", dev_normal_usb.bound_driver_name);
	printf("dev_quirky_usb bound to: %s\n", dev_quirky_usb.bound_driver_name);
	printf("dev_nic bound to:        %s\n", dev_nic.bound_driver_name);
	return 0;
}
