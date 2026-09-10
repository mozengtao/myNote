/*
 * plugin_system.c
 *
 * 场景：一个"框架"（framework）在运行期加载若干"插件”（plugin），
 * 每个插件在加载时把自己的回调函数注册到框架的插件表里，
 * 框架在需要的时候盲调这些回调 —— 框架从不 #include 插件的实现。
 *
 * 这与 Kernel 里 module_init() 时调用 register_xxx(&my_ops) 的模式完全一致：
 *
 *   plugin.c  ---register_plugin()--->  framework 插件表
 *                                            │
 *                                       framework_dispatch()
 *                                            │
 *                                       plugin->on_event(...)
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o plugin_system plugin_system.c
 * 运行：
 *   ./plugin_system
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLUGINS 16
#define MAX_NAME    32

/* ------------------------------------------------------------------ */
/* 1. 契约（Contract）：框架只知道这个函数指针签名，不知道具体实现     */
/* ------------------------------------------------------------------ */

struct plugin;

/* 事件回调：框架发生事件时会调用它，plugin 自己决定怎么处理 */
typedef void (*plugin_on_event_fn)(struct plugin *self, const char *event, void *ctx);

/* 生命周期回调：框架加载/卸载插件时调用 */
typedef int  (*plugin_init_fn)(struct plugin *self);
typedef void (*plugin_exit_fn)(struct plugin *self);

struct plugin {
	char name[MAX_NAME];
	void *private_data;          /* Context Injection 的雏形，见 03-context-injection.md */

	plugin_init_fn     init;
	plugin_on_event_fn on_event;
	plugin_exit_fn     exit;
};

/* ------------------------------------------------------------------ */
/* 2. 框架（Core）：只依赖上面的契约，完全不知道插件的具体实现         */
/* ------------------------------------------------------------------ */

struct framework {
	struct plugin *plugins[MAX_PLUGINS];
	int count;
};

static struct framework g_fw = { .count = 0 };

/* register_xxx() 风格的注册接口，对应 Kernel 的 register_chrdev()/pci_register_driver() 等 */
static int framework_register_plugin(struct plugin *p)
{
	if (g_fw.count >= MAX_PLUGINS) {
		fprintf(stderr, "[framework] plugin table full, reject '%s'\n", p->name);
		return -1;
	}

	/* NULL callback 检查：不允许注册一个连 on_event 都没有的插件 */
	if (!p->on_event) {
		fprintf(stderr, "[framework] plugin '%s' has NULL on_event, reject\n", p->name);
		return -1;
	}

	g_fw.plugins[g_fw.count++] = p;
	printf("[framework] registered plugin '%s' (slot %d)\n", p->name, g_fw.count - 1);

	/* 注册即调用生命周期回调，类似 driver->probe() 在 device_add 匹配后被调用 */
	if (p->init)
		p->init(p);

	return 0;
}

static void framework_unregister_plugin(struct plugin *p)
{
	int i;

	for (i = 0; i < g_fw.count; i++) {
		if (g_fw.plugins[i] == p) {
			if (p->exit)
				p->exit(p);

			/* 从表中移除：把最后一个搬过来填坑，避免留下 dangling 指针 */
			g_fw.plugins[i] = g_fw.plugins[g_fw.count - 1];
			g_fw.plugins[g_fw.count - 1] = NULL;
			g_fw.count--;

			printf("[framework] unregistered plugin '%s'\n", p->name);
			return;
		}
	}

	fprintf(stderr, "[framework] warning: unregister unknown plugin\n");
}

/* framework_dispatch()：框架的"运行期盲调" —— 对应 file->f_op->read() 的调用点 */
static void framework_dispatch(const char *event, void *ctx)
{
	int i;

	printf("[framework] dispatching event '%s' to %d plugin(s)\n", event, g_fw.count);
	for (i = 0; i < g_fw.count; i++) {
		struct plugin *p = g_fw.plugins[i];
		/* 框架完全不知道 p->on_event 背后是谁写的、怎么实现的 */
		p->on_event(p, event, ctx);
	}
}

/* ------------------------------------------------------------------ */
/* 3. 插件实现 A：日志插件                                            */
/* ------------------------------------------------------------------ */

struct logger_state {
	int event_count;
};

static int logger_init(struct plugin *self)
{
	struct logger_state *st = calloc(1, sizeof(*st));
	self->private_data = st;
	printf("  [logger] init, private_data allocated at %p\n", (void *)st);
	return 0;
}

static void logger_on_event(struct plugin *self, const char *event, void *ctx)
{
	struct logger_state *st = self->private_data;
	(void)ctx;
	st->event_count++;
	printf("  [logger] event #%d: '%s'\n", st->event_count, event);
}

static void logger_exit(struct plugin *self)
{
	struct logger_state *st = self->private_data;
	printf("  [logger] exit, saw %d events total, freeing %p\n",
	       st->event_count, (void *)st);
	free(st);
	self->private_data = NULL;
}

static struct plugin logger_plugin = {
	.name      = "logger",
	.init      = logger_init,
	.on_event  = logger_on_event,
	.exit      = logger_exit,
};

/* ------------------------------------------------------------------ */
/* 4. 插件实现 B：告警插件（只关心特定事件，其余不处理）                */
/* ------------------------------------------------------------------ */

static void alert_on_event(struct plugin *self, const char *event, void *ctx)
{
	(void)self;
	(void)ctx;
	if (strcmp(event, "error") == 0)
		printf("  [alert] !!! got an error event, paging on-call !!!\n");
	else
		printf("  [alert] ignoring non-error event '%s'\n", event);
}

static struct plugin alert_plugin = {
	.name     = "alert",
	.on_event = alert_on_event,
	/* 没有 init/exit，说明这些回调是可选的 —— 框架必须对 NULL 做保护 */
};

/* ------------------------------------------------------------------ */
/* 5. 插件实现 C：一个"坏插件”，用来演示 NULL callback 保护            */
/* ------------------------------------------------------------------ */

static struct plugin broken_plugin = {
	.name     = "broken",
	.on_event = NULL,   /* 故意留空，验证 framework_register_plugin 的防御 */
};

int main(void)
{
	printf("=== plugin_system demo ===\n\n");

	printf("-- 注册阶段 --\n");
	framework_register_plugin(&logger_plugin);
	framework_register_plugin(&alert_plugin);
	framework_register_plugin(&broken_plugin); /* 应该被拒绝 */

	printf("\n-- 运行阶段：框架广播事件，不知道插件内部实现 --\n");
	framework_dispatch("startup", NULL);
	framework_dispatch("error", NULL);
	framework_dispatch("shutdown", NULL);

	printf("\n-- 卸载阶段 --\n");
	framework_unregister_plugin(&logger_plugin);
	framework_unregister_plugin(&alert_plugin);

	printf("\n=== done, remaining plugins: %d ===\n", g_fw.count);
	return 0;
}
