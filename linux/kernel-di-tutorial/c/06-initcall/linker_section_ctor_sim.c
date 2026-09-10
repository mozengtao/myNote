/*
 * linker_section_ctor_sim.c
 *
 * 场景：还原 Kernel initcall 机制的核心技巧 —— 用户态程序里同样可以用
 * "自定义 section + 链接器自动生成的 __start_SECNAME/__stop_SECNAME 符号"
 * 把一批函数指针"隐式收集"到一个数组里，不需要手写任何注册表代码。
 *
 * 这与 Kernel 的真实机制几乎一样：
 *
 *   include/linux/init.h:
 *     #define __define_initcall(level,fn,id) \
 *         static initcall_t __initcall_##fn##id __used \
 *         __attribute__((__section__(".initcall" level ".init"))) = fn
 *
 *   vmlinux.lds.S（链接脚本）显式定义：
 *     __initcall_start = .;  ...  __initcall_end = .;
 *
 * 用户态没有自定义链接脚本，但 GNU ld 对"合法 C 标识符"形式的 section 名
 * 会自动生成 __start_<section>/__stop_<section> 符号 —— 这正是本文件依赖的机制。
 *
 * 同时对比 __attribute__((constructor))：另一种"链接器/运行时自动收集初始化函数"
 * 的手段，语义不同（constructor 在 main() 之前自动运行，initcall 是显式调用 do_initcalls()）。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o linker_section_ctor_sim linker_section_ctor_sim.c
 * 运行：
 *   ./linker_section_ctor_sim
 */

#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* 1. 手段 A：__attribute__((constructor))                             */
/*    运行时机：main() 之前，由 C runtime（.init_array）自动调用          */
/* ------------------------------------------------------------------ */

__attribute__((constructor))
static void ctor_early(void)
{
	printf("[ctor default priority] early constructor ran (before main)\n");
}

/* GCC 支持给 constructor 指定优先级，数字越小越先执行（101 是用户可用的最小值） */
__attribute__((constructor(101)))
static void ctor_priority_101(void)
{
	printf("[ctor priority=101] runs first among prioritized constructors\n");
}

__attribute__((constructor(200)))
static void ctor_priority_200(void)
{
	printf("[ctor priority=200] runs after priority=101\n");
}

/* ------------------------------------------------------------------ */
/* 2. 手段 B：Linker Section Registration —— 模拟 Kernel initcall        */
/* ------------------------------------------------------------------ */

typedef int (*initcall_t)(void);

/* 对应 include/linux/init.h 的 __define_initcall 宏 */
#define DEFINE_INITCALL(fn, level, id)					\
	static initcall_t __initcall_##fn##id __attribute__((used))	\
		__attribute__((section("initcall" level))) = fn

/* 三个不同"级别"的 section，模拟 core_initcall / subsys_initcall / late_initcall */
static int core_subsystem_init(void)
{
	printf("  [core_initcall] core subsystem initialized\n");
	return 0;
}
DEFINE_INITCALL(core_subsystem_init, "1", _a);

static int mm_subsystem_init(void)
{
	printf("  [core_initcall] mm-like subsystem initialized\n");
	return 0;
}
DEFINE_INITCALL(mm_subsystem_init, "1", _b);

static int net_subsystem_init(void)
{
	printf("  [subsys_initcall] net-like subsystem initialized\n");
	return 0;
}
DEFINE_INITCALL(net_subsystem_init, "4", _a);

static int driver_probe_init(void)
{
	printf("  [late_initcall] a late-registered driver probes hardware\n");
	return 0;
}
DEFINE_INITCALL(driver_probe_init, "7", _a);

/* 链接器自动生成的 section 边界符号 —— 完全不需要我们自己写链接脚本 */
extern initcall_t __start_initcall1[], __stop_initcall1[];
extern initcall_t __start_initcall4[], __stop_initcall4[];
extern initcall_t __start_initcall7[], __stop_initcall7[];

/* do_initcalls()：对应 init/main.c do_initcalls()，按 level 顺序遍历各个 section */
static void do_one_level(const char *level_name, initcall_t *start, initcall_t *stop)
{
	initcall_t *fn;
	int n = 0;
	printf("[do_initcalls] running level '%s' (%ld entries)\n",
	       level_name, (long)(stop - start));
	for (fn = start; fn < stop; fn++) {
		(*fn)();
		n++;
	}
	(void)n;
}

static void do_initcalls(void)
{
	do_one_level("core (level 1)",   __start_initcall1, __stop_initcall1);
	do_one_level("subsys (level 4)", __start_initcall4, __stop_initcall4);
	do_one_level("late (level 7)",   __start_initcall7, __stop_initcall7);
}

int main(void)
{
	printf("=== linker_section_ctor_sim demo ===\n");
	printf("(constructor 输出应该已经出现在上面，因为它们在 main() 之前运行)\n\n");

	printf("-- 显式调用 do_initcalls()，模拟内核启动时 init/main.c 的行为 --\n");
	do_initcalls();

	printf("\n=== done ===\n");
	printf("说明：constructor 是\"运行时机确定但顺序需要显式指定优先级\"；\n");
	printf("      initcall 是\"运行时机由 Core 显式决定 (do_initcalls())，\n");
	printf("      顺序由 section 分组（level）决定，同一 level 内顺序不保证\"。\n");
	return 0;
}
