/*
 * uevent_hotplug_notify.c
 *
 * 场景：模拟真实 Kernel drivers/base/core.c 里 device_add() 成功后
 * kobject_uevent(&dev->kobj, KOBJ_ADD) 这一步——bus/driver 核心在
 * "匹配成功"或"匹配失败"之后，会触发一次"环境变量事件"广播给用户空间
 * （真实 Kernel 由 udev/systemd-udevd 监听 netlink socket 接收）。
 *
 * 本示例是一个全新的、独立的实现，不复用 05-notifier-chain 的任何代码，
 * 但故意在结构上与 notifier chain 呼应，以便在文档第 10 节做对比：
 *   - 这里的"广播列表"是固定的两类监听者（udev 模拟器 + 审计日志），
 *     用简单数组而非链表，强调"总线核心不知道监听者是谁，只管调用契约"。
 *   - 事件本身携带一组 key=value 环境变量（对应真实 struct kobj_uevent_env），
 *     由触发事件的一方（bus core）负责组装，监听者只读不改。
 *
 * 覆盖三种场景：
 *   1. 设备注册 + match 成功 -> probe 成功 -> 触发 "bind" 事件
 *   2. 设备注册，但没有任何驱动匹配 -> 触发 "no_driver" 事件
 *   3. 驱动被移除 -> 触发 "unbind" 事件
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o uevent_hotplug_notify uevent_hotplug_notify.c
 * 运行：./uevent_hotplug_notify
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENV 8
#define MAX_LISTENERS 4

/* 对应 struct kobj_uevent_env：一组 KEY=VALUE 环境变量 */
struct uevent_env {
	char vars[MAX_ENV][64];
	int  count;
};

static void env_add(struct uevent_env *env, const char *fmt_key, const char *val)
{
	snprintf(env->vars[env->count], sizeof(env->vars[0]), "%s=%s", fmt_key, val);
	env->count++;
}

/*
 * 监听者契约：Core 只认识这一个函数指针签名，不知道具体是 udev 模拟器还是
 * 审计日志。对应真实场景里 udev 通过 netlink 收到 uevent 后自行解析。
 */
typedef void (*uevent_listener_fn)(const char *action, const struct uevent_env *env, void *ctx);

struct uevent_listener {
	char name[32];
	uevent_listener_fn fn;
	void *ctx;
};

/* Core：总线核心持有的监听者表（模拟内核态 kobject_uevent 底层的 netlink broadcast） */
struct hotplug_bus {
	char name[32];
	struct uevent_listener listeners[MAX_LISTENERS];
	int nr_listeners;
};

static void hotplug_bus_add_listener(struct hotplug_bus *bus, const char *name,
				     uevent_listener_fn fn, void *ctx)
{
	struct uevent_listener *l = &bus->listeners[bus->nr_listeners++];
	snprintf(l->name, sizeof(l->name), "%s", name);
	l->fn = fn;
	l->ctx = ctx;
	printf("[hotplug:%s] listener '%s' attached\n", bus->name, name);
}

/* Core 广播：对应 kobject_uevent()，逐一盲调每个监听者 */
static void hotplug_bus_broadcast(struct hotplug_bus *bus, const char *action,
				  const struct uevent_env *env)
{
	int i;
	printf("[hotplug:%s] broadcasting action='%s' (%d env var(s)) to %d listener(s)\n",
	       bus->name, action, env->count, bus->nr_listeners);
	for (i = 0; i < bus->nr_listeners; i++)
		bus->listeners[i].fn(action, env, bus->listeners[i].ctx);
}

/* ------------------------------------------------------------------ */
/* 业务侧：设备 + 驱动 + match/probe，触发时机与 04/09 篇一致           */
/* ------------------------------------------------------------------ */

struct hp_device {
	char name[32];
	char model[32];
	int  bound;
};

struct hp_driver {
	char name[32];
	const char *supported_model;
};

static int hp_match(struct hp_device *dev, struct hp_driver *drv)
{
	return strcmp(dev->model, drv->supported_model) == 0;
}

static void hp_register_device(struct hotplug_bus *bus, struct hp_device *dev,
				struct hp_driver *drivers[], int nr_drivers)
{
	int i;
	struct uevent_env env = {0};

	env_add(&env, "DEVNAME", dev->name);
	env_add(&env, "MODEL", dev->model);

	printf("\n-- 注册设备 '%s' (model=%s) --\n", dev->name, dev->model);

	for (i = 0; i < nr_drivers; i++) {
		if (hp_match(dev, drivers[i])) {
			dev->bound = 1;
			env_add(&env, "DRIVER", drivers[i]->name);
			hotplug_bus_broadcast(bus, "bind", &env);
			return;
		}
	}
	/* 没有任何驱动匹配：广播一个"无人处理"事件，运维/udev 可以借此告警 */
	hotplug_bus_broadcast(bus, "no_driver", &env);
}

static void hp_unbind_device(struct hotplug_bus *bus, struct hp_device *dev,
			     struct hp_driver *drv)
{
	struct uevent_env env = {0};
	env_add(&env, "DEVNAME", dev->name);
	env_add(&env, "DRIVER", drv->name);
	dev->bound = 0;
	printf("\n-- 驱动 '%s' 移除，解绑设备 '%s' --\n", drv->name, dev->name);
	hotplug_bus_broadcast(bus, "unbind", &env);
}

/* ------------------------------------------------------------------ */
/* 两类监听者：模拟 udev（响应 bind 生成设备节点）+ 审计日志            */
/* ------------------------------------------------------------------ */

struct udev_sim_ctx {
	int nodes_created;
};

static void udev_sim_listener(const char *action, const struct uevent_env *env, void *ctx)
{
	struct udev_sim_ctx *c = ctx;
	int i;
	char devname[64] = "?";

	for (i = 0; i < env->count; i++) {
		if (strncmp(env->vars[i], "DEVNAME=", 8) == 0)
			snprintf(devname, sizeof(devname), "%s", env->vars[i] + 8);
	}

	if (strcmp(action, "bind") == 0) {
		c->nodes_created++;
		printf("    [udev-sim] creating /dev/%s node (bind event)\n", devname);
	} else if (strcmp(action, "unbind") == 0) {
		c->nodes_created--;
		printf("    [udev-sim] removing /dev/%s node (unbind event)\n", devname);
	} else if (strcmp(action, "no_driver") == 0) {
		printf("    [udev-sim] device '%s' has no driver, skipping node creation\n", devname);
	}
}

struct audit_ctx {
	int events_seen;
};

static void audit_listener(const char *action, const struct uevent_env *env, void *ctx)
{
	struct audit_ctx *c = ctx;
	int i;

	c->events_seen++;
	printf("    [audit-log] action=%s env={ ", action);
	for (i = 0; i < env->count; i++)
		printf("%s ", env->vars[i]);
	printf("}\n");
}

int main(void)
{
	struct hotplug_bus bus = { .name = "vbus0" };
	struct udev_sim_ctx udev_ctx = {0};
	struct audit_ctx audit_ctx = {0};

	struct hp_driver nvme_drv  = { .name = "nvme",  .supported_model = "STORAGE-NVME" };
	struct hp_driver sata_drv  = { .name = "ahci",  .supported_model = "STORAGE-SATA" };
	struct hp_driver *drivers[] = { &nvme_drv, &sata_drv };

	struct hp_device disk0 = { .name = "disk0", .model = "STORAGE-NVME" };
	struct hp_device disk1 = { .name = "disk1", .model = "STORAGE-SCSI" }; /* 无匹配驱动 */

	printf("=== uevent_hotplug_notify demo ===\n\n");

	hotplug_bus_add_listener(&bus, "udev-sim", udev_sim_listener, &udev_ctx);
	hotplug_bus_add_listener(&bus, "audit-log", audit_listener, &audit_ctx);

	printf("\n-- 场景 1：设备注册，match 成功，触发 bind 事件 --\n");
	hp_register_device(&bus, &disk0, drivers, 2);

	printf("\n-- 场景 2：设备注册，没有驱动匹配，触发 no_driver 事件 --\n");
	hp_register_device(&bus, &disk1, drivers, 2);

	printf("\n-- 场景 3：驱动移除，触发 unbind 事件 --\n");
	hp_unbind_device(&bus, &disk0, &nvme_drv);

	printf("\n=== done ===\n");
	printf("udev_ctx.nodes_created (预期 0，创建后又被 unbind 移除) = %d\n", udev_ctx.nodes_created);
	printf("audit_ctx.events_seen  (预期 3，三次广播全部被审计记录) = %d\n", audit_ctx.events_seen);
	return 0;
}
