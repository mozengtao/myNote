/*
 * static_tracepoint_sim.c
 *
 * 场景：模拟 Kernel Tracepoint 机制——`TRACE_EVENT`/`DECLARE_TRACE` 风格的
 * "静态插入点"，插入点代码本身（`trace_page_fault()`）从编译到运行永远不变，
 * 但它的实际行为完全取决于运行期有没有 probe 挂载在它身上。
 *
 * 真实定义：
 *   - struct tracepoint/struct tracepoint_func  见 include/linux/tracepoint.h:26,31
 *   - __DECLARE_TRACE()（生成 trace_##name() 这个静态插入点函数）
 *     见 include/linux/tracepoint.h:143
 *   - __DO_TRACE()（真正遍历 probe 数组并逐一调用）
 *     见 include/linux/tracepoint.h:117
 *   - tracepoint_probe_register()/tracepoint_probe_unregister()
 *     见 kernel/tracepoint.c:377,421
 *   - 真实调用点举例：kernel/sched.c:3160 `trace_sched_switch(prev, next)`，
 *     对应的 TRACE_EVENT(sched_switch, ...) 定义见
 *     include/trace/events/sched.h:113
 *
 * 关键简化说明（如实注明，不是编造）：本仓库版本的 include/linux/jump_label.h
 * 里确实存在真正的 jump label / asm-goto 机制（当编译器支持 asm-goto 且
 * CONFIG_JUMP_LABEL=y 时的分支，用运行期"代码打洞"——把 NOP 和 JMP 相互
 * 替换——做到完全不需要读内存、不需要分支判断的零开销探测点），但那是一段
 * 与具体 CPU 架构强耦合的内联汇编 + 链接器 section 技巧，无法在用户态可移植地
 * 复刻。本文件复刻的是同一个头文件里同样真实存在的另一条代码路径——
 * !HAVE_JUMP_LABEL 分支（jump_label.h:59-95）：用一个整数标志位的原子读做
 * 判断，static_branch() 退化为 atomic_read(&key->enabled)。这条 fallback
 * 路径在编译器不支持 asm-goto 或者 CONFIG_JUMP_LABEL=n 时就是真实生效的
 * 代码，不是本文档凭空发明的简化。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o static_tracepoint_sim static_tracepoint_sim.c
 * 运行：./static_tracepoint_sim
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 对应真实 include/linux/tracepoint.h:26 struct tracepoint_func */
struct tracepoint_func {
	void *func;	/* 具体 probe 的函数指针 */
	void *data;	/* Context Injection：probe 注册时携带的私有数据，见 03 篇 */
};

/* 对应真实 include/linux/tracepoint.h:31 struct tracepoint（简化掉了 module 相关字段），
 * enabled 对应真实的 jump_label_key，这里退化为普通 int（对应 jump_label.h 的
 * !HAVE_JUMP_LABEL fallback 路径，语义上等价于 atomic_t） */
struct tracepoint {
	const char *name;
	int enabled;			/* 0 表示没有任何 probe，>0 表示至少一个 */
	struct tracepoint_func *funcs;	/* NULL 结尾数组，NULL 表示没有 probe */
};

/* 具体的探测点：page_fault，签名故意贴近真实 Kernel 里常见的
 * "cpu id + 地址" 这类轻量参数组合 */
static struct tracepoint __tracepoint_page_fault = { "page_fault", 0, NULL };

/* 对应真实 __DO_TRACE()（include/linux/tracepoint.h:117）：遍历 probe 数组逐一盲调 */
static inline void __do_trace_page_fault(int cpu, unsigned long addr)
{
	struct tracepoint_func *it = __tracepoint_page_fault.funcs;

	if (!it)
		return;
	do {
		void (*probe)(void *data, int cpu, unsigned long addr) = it->func;
		probe(it->data, cpu, addr);
	} while ((++it)->func);
}

/*
 * 对应真实 __DECLARE_TRACE() 生成的 trace_##name()（include/linux/tracepoint.h:144）：
 * 这是"静态插入点"本身——从第一次编译到程序结束，这个函数体一个字节都不会变，
 * 变化的只是 __tracepoint_page_fault.enabled/.funcs 这两个运行期状态。
 */
static inline void trace_page_fault(int cpu, unsigned long addr)
{
	if (__tracepoint_page_fault.enabled)	/* 对应 static_branch(&key)，最快路径 */
		__do_trace_page_fault(cpu, addr);
}

/* ------------------------------------------------------------------ */
/* Core：对应 kernel/tracepoint.c 的 tracepoint_probe_register()/unregister() */
/* ------------------------------------------------------------------ */

static int tp_probe_register(struct tracepoint *tp, void *func, void *data)
{
	int n = 0;
	struct tracepoint_func *new_arr;

	if (tp->funcs) {
		for (n = 0; tp->funcs[n].func; n++) {
			if (tp->funcs[n].func == func && tp->funcs[n].data == data)
				return -1;	/* 对应真实实现里的 -EEXIST */
		}
	}
	/* +2：新 probe 一个，NULL 结尾哨兵一个，对应真实 allocate_probes() 的注释 */
	new_arr = realloc(tp->funcs, sizeof(*new_arr) * (n + 2));
	if (!new_arr)
		return -1;
	new_arr[n].func = func;
	new_arr[n].data = data;
	new_arr[n + 1].func = NULL;
	tp->funcs = new_arr;
	tp->enabled = 1;
	printf("  [tracepoint_probe_register] '%s' now has %d probe(s) attached\n",
	       tp->name, n + 1);
	return 0;
}

static int tp_probe_unregister(struct tracepoint *tp, void *func, void *data)
{
	int i, n = 0, found = -1;

	if (!tp->funcs)
		return -1;
	for (n = 0; tp->funcs[n].func; n++) {
		if (tp->funcs[n].func == func && tp->funcs[n].data == data)
			found = n;
	}
	if (found < 0)
		return -1;
	for (i = found; tp->funcs[i].func; i++)
		tp->funcs[i] = tp->funcs[i + 1];
	if (!tp->funcs[0].func) {
		free(tp->funcs);
		tp->funcs = NULL;
		tp->enabled = 0;
	}
	printf("  [tracepoint_probe_unregister] probe detached from '%s', %d remaining\n",
	       tp->name, tp->funcs ? n : 0);
	return 0;
}

/* ------------------------------------------------------------------ */
/* 模拟一个真实的"业务热路径"：page fault 处理函数，内部无条件调用一次 trace_page_fault() */
/* ------------------------------------------------------------------ */

static int g_page_fault_count;

static void handle_page_fault(unsigned long addr)
{
	g_page_fault_count++;
	/* ... 真实的缺页处理逻辑（分配物理页、建立映射……）省略 ... */
	trace_page_fault(0 /* cpu */, addr);	/* <-- 这一行代码，注册/注销 probe 前后一字未改 */
}

/* 一个"probe"实现：模拟 ftrace 风格的探测器，只是打印一行诊断信息 */
static void probe_ftrace_style(void *data, int cpu, unsigned long addr)
{
	const char *tag = data;
	printf("    [probe:%s] page_fault on cpu=%d addr=0x%08lx\n", tag, cpu, addr);
}

int main(void)
{
	printf("=== static_tracepoint_sim demo ===\n\n");

	printf("-- 阶段1：没有任何 probe 挂载，触发 3 次 page fault --\n");
	handle_page_fault(0x1000);
	handle_page_fault(0x2000);
	handle_page_fault(0x3000);
	printf("  (预期：trace_page_fault() 内部的 if 判断为假，什么都不会打印)\n");

	printf("\n-- 阶段2：注册一个 probe，再触发 3 次 page fault --\n");
	tp_probe_register(&__tracepoint_page_fault, probe_ftrace_style, "ftrace");
	handle_page_fault(0x4000);
	handle_page_fault(0x5000);
	handle_page_fault(0x6000);

	printf("\n-- 阶段3：注销 probe，再触发 2 次 page fault --\n");
	tp_probe_unregister(&__tracepoint_page_fault, probe_ftrace_style, "ftrace");
	handle_page_fault(0x7000);
	handle_page_fault(0x8000);
	printf("  (预期：注销之后又恢复到\"静默\"状态)\n");

	printf("\n=== done ===\n");
	printf("总共触发 %d 次 page fault，handle_page_fault() 里那一行 trace_page_fault(0, addr)\n"
	       "从阶段1到阶段3从未被修改过一个字节——这就是 Tracepoint 作为 DI 手法的核心：\n"
	       "静态插入点的源代码是固定不变的契约，探测行为完全由运行期挂载的 probe 决定。\n",
	       g_page_fault_count);
	return 0;
}
