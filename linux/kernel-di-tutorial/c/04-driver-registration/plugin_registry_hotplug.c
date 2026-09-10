/*
 * plugin_registry_hotplug.c
 *
 * 场景：一个"支持热插拔"的驱动注册表 —— 驱动可以在程序运行期间
 * 任意时刻注册/注销，framework 始终维护一份一致的可用驱动列表，
 * 并在注销时保证"正在使用中的驱动不能被强行卸掉"（简化版引用计数）。
 *
 * 对应 Kernel 真实场景：insmod/rmmod 一个内核模块（driver）时，
 * module 的引用计数（try_module_get/module_put）决定 rmmod 是否成功。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o plugin_registry_hotplug plugin_registry_hotplug.c
 * 运行：
 *   ./plugin_registry_hotplug
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DRIVERS 16

struct hotplug_driver {
	char name[32];
	int  version;
	int  refcount;          /* 正在使用中的句柄数，rmmod 前必须为 0 */
	int  active;             /* 0 = 未注册/已注销，1 = 已注册可用 */

	void *(*open)(struct hotplug_driver *self);
	void  (*use)(struct hotplug_driver *self, void *handle);
	void  (*close)(struct hotplug_driver *self, void *handle);
};

struct registry {
	struct hotplug_driver *slots[MAX_DRIVERS];
	int count;
};

static struct registry g_registry = { .count = 0 };

/* ---- 注册 / 注销：等价于 insmod / rmmod ---- */

static int registry_register(struct hotplug_driver *d)
{
	int i;
	for (i = 0; i < g_registry.count; i++) {
		if (g_registry.slots[i] == d) {
			fprintf(stderr, "[registry] driver '%s' already registered\n", d->name);
			return -1;
		}
	}
	if (g_registry.count >= MAX_DRIVERS)
		return -1;

	d->active = 1;
	d->refcount = 0;
	g_registry.slots[g_registry.count++] = d;
	printf("[registry] driver '%s' v%d registered (hot-plugged in)\n", d->name, d->version);
	return 0;
}

/* try_module_get() 风格接口：使用前必须先"借用"一次，增加引用计数 */
static struct hotplug_driver *registry_get(const char *name)
{
	int i;
	for (i = 0; i < g_registry.count; i++) {
		struct hotplug_driver *d = g_registry.slots[i];
		if (d->active && strcmp(d->name, name) == 0) {
			d->refcount++;
			printf("[registry] '%s' refcount -> %d (get)\n", d->name, d->refcount);
			return d;
		}
	}
	fprintf(stderr, "[registry] driver '%s' not found or inactive\n", name);
	return NULL;
}

static void registry_put(struct hotplug_driver *d)
{
	if (d->refcount > 0)
		d->refcount--;
	printf("[registry] '%s' refcount -> %d (put)\n", d->name, d->refcount);
}

/* rmmod 风格接口：refcount 非 0 时拒绝注销，这是热插拔安全的核心保障 */
static int registry_unregister(const char *name)
{
	int i;
	for (i = 0; i < g_registry.count; i++) {
		struct hotplug_driver *d = g_registry.slots[i];
		if (strcmp(d->name, name) == 0) {
			if (d->refcount > 0) {
				fprintf(stderr,
					"[registry] refuse to unregister '%s', refcount=%d (still in use)\n",
					d->name, d->refcount);
				return -1;
			}
			d->active = 0;
			g_registry.slots[i] = g_registry.slots[--g_registry.count];
			printf("[registry] driver '%s' unregistered (hot-plugged out)\n", d->name);
			return 0;
		}
	}
	return -1;
}

/* ---- 驱动实现 A：内存分配器风格插件 ---- */

struct mem_handle { size_t size; char tag[16]; };

static void *mem_open(struct hotplug_driver *self)
{
	struct mem_handle *h = malloc(sizeof(*h));
	h->size = 4096;
	strcpy(h->tag, "mem-handle");
	printf("    [%s] opened handle, size=%zu\n", self->name, h->size);
	return h;
}

static void mem_use(struct hotplug_driver *self, void *handle)
{
	struct mem_handle *h = handle;
	printf("    [%s] using handle tag='%s' size=%zu\n", self->name, h->tag, h->size);
}

static void mem_close(struct hotplug_driver *self, void *handle)
{
	printf("    [%s] closing handle\n", self->name);
	free(handle);
}

static struct hotplug_driver mem_allocator_driver = {
	.name = "mem-allocator",
	.version = 1,
	.open = mem_open,
	.use = mem_use,
	.close = mem_close,
};

/* ---- 驱动实现 B：日志插件（v2，演示版本升级 = 先卸后装）---- */

static void *log_open(struct hotplug_driver *self)
{
	printf("    [%s] opened log session\n", self->name);
	return (void *)1;   /* 简单句柄，非堆分配 */
}

static void log_use(struct hotplug_driver *self, void *handle)
{
	(void)handle;
	printf("    [%s] writing log entry\n", self->name);
}

static void log_close(struct hotplug_driver *self, void *handle)
{
	(void)handle;
	printf("    [%s] closed log session\n", self->name);
}

static struct hotplug_driver logger_driver_v1 = {
	.name = "logger", .version = 1, .open = log_open, .use = log_use, .close = log_close,
};
static struct hotplug_driver logger_driver_v2 = {
	.name = "logger", .version = 2, .open = log_open, .use = log_use, .close = log_close,
};

int main(void)
{
	void *h1, *h2;

	printf("=== plugin_registry_hotplug demo ===\n\n");

	registry_register(&mem_allocator_driver);
	registry_register(&logger_driver_v1);

	printf("\n-- 使用 mem-allocator，保持句柄不释放 --\n");
	{
		struct hotplug_driver *d = registry_get("mem-allocator");
		h1 = d->open(d);
		d->use(d, h1);
		/* 故意不 close，模拟"仍在使用中" */
	}

	printf("\n-- 尝试卸载 mem-allocator（应该失败，因为 refcount > 0）--\n");
	registry_unregister("mem-allocator");

	printf("\n-- 归还引用后再卸载（应该成功）--\n");
	{
		struct hotplug_driver *d = &mem_allocator_driver;
		d->close(d, h1);
		registry_put(d);
	}
	registry_unregister("mem-allocator");

	printf("\n-- logger 热升级：v1 使用完毕后卸载，再装 v2 --\n");
	{
		struct hotplug_driver *d = registry_get("logger");
		h2 = d->open(d);
		d->use(d, h2);
		d->close(d, h2);
		registry_put(d);
	}
	registry_unregister("logger");
	registry_register(&logger_driver_v2);

	{
		struct hotplug_driver *d = registry_get("logger");
		printf("    now running logger v%d\n", d->version);
		registry_put(d);
	}

	printf("\n=== done, %d driver(s) still registered ===\n", g_registry.count);
	return 0;
}
