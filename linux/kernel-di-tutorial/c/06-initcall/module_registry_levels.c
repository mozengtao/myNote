/*
 * module_registry_levels.c
 *
 * 场景：Kernel 的分级初始化（early/core/postcore/arch/subsys/fs/device/late，
 * 见 include/linux/init.h:186-212）用"哪个 section"来分级；本文件演示
 * 不依赖编译器 section 技巧的"可移植替代方案"——用
 * __attribute__((constructor)) 让每个模块在 main() 之前把自己"注册"进
 * 一张显式的登记表，登记表按 level 分组，boot 阶段按 level 顺序执行，
 * 并复刻 do_one_initcall() 对错误返回值的处理策略。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o module_registry_levels module_registry_levels.c
 * 运行：
 *   ./module_registry_levels
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX_ENTRIES 32

/* 级别定义，与本仓库 include/linux/init.h:186-212 的顺序完全对应 */
enum init_level {
	LEVEL_EARLY   = 0,
	LEVEL_CORE    = 1,
	LEVEL_POSTCORE= 2,
	LEVEL_ARCH    = 3,
	LEVEL_SUBSYS  = 4,
	LEVEL_FS      = 5,
	LEVEL_DEVICE  = 6,
	LEVEL_LATE    = 7,
	NR_LEVELS
};

static const char *level_names[NR_LEVELS] = {
	"early", "core", "postcore", "arch", "subsys", "fs", "device", "late",
};

typedef int (*initcall_fn_t)(void);

struct init_entry {
	const char *name;
	enum init_level level;
	initcall_fn_t fn;
};

static struct init_entry g_registry[MAX_ENTRIES];
static int g_registry_count = 0;

/* register_initcall()：显式登记接口，等价于把自己塞进某个 initcallN.init section */
static void register_initcall(const char *name, enum init_level level, initcall_fn_t fn)
{
	if (g_registry_count >= MAX_ENTRIES) {
		fprintf(stderr, "[registry] FULL, cannot register '%s'\n", name);
		return;
	}
	g_registry[g_registry_count].name = name;
	g_registry[g_registry_count].level = level;
	g_registry[g_registry_count].fn = fn;
	g_registry_count++;
}

/* 宏：把"定义一个 initcall"和"用 constructor 自动登记它"封装起来，
 * 用户（模块作者）的使用体验与 core_initcall(fn) 几乎一样 */
#define DECLARE_INITCALL(fnname, lvl)                                  \
	static void __register_##fnname(void) __attribute__((constructor));\
	static void __register_##fnname(void) {                            \
		register_initcall(#fnname, lvl, fnname);                       \
	}

/* ------------------------------------------------------------------ */
/* 模拟多个"模块"各自定义自己的 initcall，级别各不相同、故意乱序声明     */
/* ------------------------------------------------------------------ */

static int late_driver_probe(void)
{
	printf("  [%-8s] late_driver_probe: probing optional hardware\n", level_names[LEVEL_LATE]);
	return 0;
}
DECLARE_INITCALL(late_driver_probe, LEVEL_LATE)

static int mm_init(void)
{
	printf("  [%-8s] mm_init: setting up memory zones\n", level_names[LEVEL_CORE]);
	return 0;
}
DECLARE_INITCALL(mm_init, LEVEL_CORE)

static int net_stack_init(void)
{
	printf("  [%-8s] net_stack_init: registering protocol families\n", level_names[LEVEL_SUBSYS]);
	return 0;
}
DECLARE_INITCALL(net_stack_init, LEVEL_SUBSYS)

static int rootfs_mount_init(void)
{
	printf("  [%-8s] rootfs_mount_init: mounting rootfs\n", level_names[LEVEL_FS]);
	return 0;
}
DECLARE_INITCALL(rootfs_mount_init, LEVEL_FS)

static int optional_device_init(void)
{
	printf("  [%-8s] optional_device_init: device not present\n", level_names[LEVEL_DEVICE]);
	return -ENODEV;   /* 模拟"设备不存在"，这是一个被内核明确容忍的错误码 */
}
DECLARE_INITCALL(optional_device_init, LEVEL_DEVICE)

static int broken_subsystem_init(void)
{
	printf("  [%-8s] broken_subsystem_init: something really went wrong\n", level_names[LEVEL_SUBSYS]);
	return -EINVAL;   /* 模拟真正的初始化失败（非 -ENODEV），do_one_initcall 会打印警告但仍继续 */
}
DECLARE_INITCALL(broken_subsystem_init, LEVEL_SUBSYS)

static int early_percpu_init(void)
{
	printf("  [%-8s] early_percpu_init: setting up per-CPU areas\n", level_names[LEVEL_EARLY]);
	return 0;
}
DECLARE_INITCALL(early_percpu_init, LEVEL_EARLY)

/* ------------------------------------------------------------------ */
/* do_initcalls()：boot 核心逻辑，对应 init/main.c do_initcalls()        */
/* ------------------------------------------------------------------ */

/* do_one_initcall() 的简化复刻：对错误码做区分处理，而不是 panic */
static void do_one_initcall(struct init_entry *e)
{
	int ret = e->fn();

	if (ret && ret != -ENODEV) {
		fprintf(stderr, "  [WARN] initcall '%s' returned error code %d\n", e->name, ret);
	} else if (ret == -ENODEV) {
		/* -ENODEV 是"正常的没找到设备"，内核认为这不算错误，不打印警告 */
	}
}

static void do_initcalls(void)
{
	int lvl, i;

	for (lvl = 0; lvl < NR_LEVELS; lvl++) {
		int printed_header = 0;
		for (i = 0; i < g_registry_count; i++) {
			if ((int)g_registry[i].level != lvl)
				continue;
			if (!printed_header) {
				printf("[do_initcalls] entering level '%s'\n", level_names[lvl]);
				printed_header = 1;
			}
			do_one_initcall(&g_registry[i]);
		}
	}
}

int main(void)
{
	printf("=== module_registry_levels demo ===\n\n");
	printf("-- 登记表内容（构造函数已在 main() 之前把 %d 个 initcall 登记进来）--\n",
	       g_registry_count);
	{
		int i;
		for (i = 0; i < g_registry_count; i++)
			printf("  entry[%d]: name='%-24s' level=%s\n",
			       i, g_registry[i].name, level_names[g_registry[i].level]);
	}

	printf("\n-- 执行 do_initcalls()，按 level 顺序执行，与声明顺序无关 --\n");
	do_initcalls();

	printf("\n=== done, executed %d initcalls across %d levels ===\n",
	       g_registry_count, NR_LEVELS);
	return 0;
}
