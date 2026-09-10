/*
 * tracepoint_enable_disable_runtime.c
 *
 * 场景：真实测量"探测点没有 probe 挂载"与"探测点挂载了 probe"两种情况下，
 * 插入点本身的执行耗时差异——这是 Tracepoint 机制存在的根本原因：诊断/调试
 * 用途的探测点绝大多数时间处于"没人订阅"状态，必须做到开销趋近于零，
 * 否则所有人（包括从不使用 tracing 的用户）都要为"万一有人订阅"付出代价。
 *
 * 真实定义/背景：
 *   - struct tracepoint 的 key 字段类型是 struct jump_label_key
 *     （include/linux/tracepoint.h:32），真正的零开销依赖
 *     include/linux/jump_label.h 里 `static_branch()` 在支持 asm-goto 的
 *     架构上被编译成一条可以被运行期"打洞"改写的 NOP/JMP 指令——
 *     完全不产生任何内存读取，是比"if 判断"更进一步的零开销。
 *   - 本文件测量的是同一个头文件里 `!HAVE_JUMP_LABEL` fallback 路径的开销
 *     （jump_label.h:71 `static_branch()`：`return unlikely(atomic_read(
 *     &key->enabled));`），这是一次原子读 + 一次分支判断，本身已经非常
 *     便宜，但仍然不是"零"——这里如实测量这条 fallback 路径的真实数字，
 *     不去凭空模拟或断言 asm-goto 版本的确切纳秒数（那需要真实的内联汇编
 *     和特定 CPU 架构，会脱离"如实测量"的原则）。
 *   - __DO_TRACE()（include/linux/tracepoint.h:117）：真正调用 probe 时的
 *     遍历逻辑，这部分开销只有在 enabled 时才会发生。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o tracepoint_enable_disable_runtime tracepoint_enable_disable_runtime.c
 * 运行：./tracepoint_enable_disable_runtime
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ITERATIONS	100000000L	/* 1 亿次，足够把纳秒级差异放大到可观测的毫秒级 */

struct tracepoint_func {
	void *func;
	void *data;
};

struct tracepoint {
	const char *name;
	int enabled;			/* 对应 !HAVE_JUMP_LABEL 分支的 atomic_t enabled */
	struct tracepoint_func *funcs;
};

static struct tracepoint __tracepoint_hot_path = { "hot_path", 0, NULL };

typedef void (*hot_path_probe_t)(void *data, long val);

static inline void __do_trace_hot_path(long val)
{
	struct tracepoint_func *it = __tracepoint_hot_path.funcs;

	if (!it)
		return;
	do {
		((hot_path_probe_t)it->func)(it->data, val);
	} while ((++it)->func);
}

/* 对应真实 static_branch(&key) 的 !HAVE_JUMP_LABEL fallback：
 * 一次读取 + 一次分支判断，这是本文件要测量的"插入点固定开销" */
static inline void trace_hot_path(long val)
{
	if (__tracepoint_hot_path.enabled)
		__do_trace_hot_path(val);
}

static int tp_register(struct tracepoint *tp, void *func, void *data)
{
	int n = 0;
	struct tracepoint_func *new_arr;

	if (tp->funcs)
		for (n = 0; tp->funcs[n].func; n++)
			;
	new_arr = realloc(tp->funcs, sizeof(*new_arr) * (n + 2));
	if (!new_arr)
		return -1;
	new_arr[n].func = func;
	new_arr[n].data = data;
	new_arr[n + 1].func = NULL;
	tp->funcs = new_arr;
	tp->enabled = 1;
	return 0;
}

static void tp_unregister_all(struct tracepoint *tp)
{
	free(tp->funcs);
	tp->funcs = NULL;
	tp->enabled = 0;
}

/* 模拟"业务热路径"：每次迭代都做一点点真实工作（累加），
 * 再无条件调用一次 trace_hot_path() —— 与 static_tracepoint_sim.c 的
 * handle_page_fault() 是同一个思路，这里用 volatile 汇聚变量防止编译器
 * 把整个循环当成死代码优化掉 */
static volatile long g_sink;

static inline void hot_path_iteration(long i)
{
	g_sink += (i & 0xff);
	trace_hot_path(i);
}

/* probe A：最小化探测逻辑，只做一次自增，模拟"最省的可能情况" */
static long g_probe_minimal_count;
static void probe_minimal(void *data, long val)
{
	(void)data;
	(void)val;
	g_probe_minimal_count++;
}

/* probe B：更接近真实 ftrace/perf 行为的探测逻辑——把参数格式化进一段
 * 缓冲区，模拟"把事件写入 ring buffer 的记录格式化成本" */
static long g_probe_realistic_count;
static char g_probe_realistic_scratch[64];
static void probe_realistic(void *data, long val)
{
	(void)data;
	g_probe_realistic_count++;
	snprintf(g_probe_realistic_scratch, sizeof(g_probe_realistic_scratch),
		 "hot_path: val=%ld count=%ld", val, g_probe_realistic_count);
}

static double now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static double run_bench(const char *label)
{
	double t0, t1;
	long i;

	t0 = now_ns();
	for (i = 0; i < ITERATIONS; i++)
		hot_path_iteration(i);
	t1 = now_ns();

	printf("  [%s] %ld 次调用，总耗时 %.3f ms，平均每次调用 %.3f ns\n",
	       label, ITERATIONS, (t1 - t0) / 1e6, (t1 - t0) / (double)ITERATIONS);
	return (t1 - t0) / (double)ITERATIONS;
}

int main(void)
{
	double ns_disabled, ns_minimal, ns_realistic;

	printf("=== tracepoint_enable_disable_runtime demo ===\n\n");
	printf("每组测试跑 %ld 次 trace_hot_path() 调用，测量 CLOCK_MONOTONIC 下的真实耗时。\n"
	       "数字会随机器负载/CPU 型号波动，但相对大小关系（disabled < minimal probe\n"
	       "< realistic probe）应该在任何机器上都成立。\n\n", ITERATIONS);

	printf("-- 场景1：没有任何 probe 挂载（enabled=0，只经过一次分支判断） --\n");
	ns_disabled = run_bench("disabled     ");

	printf("\n-- 场景2：挂载 1 个最小化 probe（只做一次自增） --\n");
	tp_register(&__tracepoint_hot_path, probe_minimal, NULL);
	ns_minimal = run_bench("minimal probe");
	tp_unregister_all(&__tracepoint_hot_path);

	printf("\n-- 场景3：挂载 1 个更接近真实场景的 probe（格式化字符串） --\n");
	tp_register(&__tracepoint_hot_path, probe_realistic, NULL);
	ns_realistic = run_bench("realistic probe");
	tp_unregister_all(&__tracepoint_hot_path);

	printf("\n=== done ===\n");
	printf("汇总（真实测得，非估算）：\n");
	printf("  disabled        : %8.3f ns/call\n", ns_disabled);
	printf("  minimal probe   : %8.3f ns/call  (%.1fx disabled)\n",
	       ns_minimal, ns_disabled > 0 ? ns_minimal / ns_disabled : 0.0);
	printf("  realistic probe : %8.3f ns/call  (%.1fx disabled)\n",
	       ns_realistic, ns_disabled > 0 ? ns_realistic / ns_disabled : 0.0);
	printf("\n说明：g_sink=%ld（防止循环被优化掉的哨兵变量，无实际意义）；\n"
	       "      probe_minimal 触发了 %ld 次，probe_realistic 触发了 %ld 次，\n"
	       "      两者都应该正好等于 ITERATIONS=%ld，证明每次挂载期间的调用\n"
	       "      全部被正确送达 probe，没有丢事件。真实 Kernel 用 asm-goto\n"
	       "      jump label 把 disabled 场景的开销进一步降到\"零\"（连这里\n"
	       "      测得的一次分支判断都不需要），本文件如实测量的是同一头文件里\n"
	       "      !HAVE_JUMP_LABEL fallback 路径的开销，而不是编造 asm-goto\n"
	       "      版本的具体数字。\n",
	       g_sink, g_probe_minimal_count, g_probe_realistic_count, ITERATIONS);
	return 0;
}
