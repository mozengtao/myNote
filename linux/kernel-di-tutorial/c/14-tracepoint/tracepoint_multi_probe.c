/*
 * tracepoint_multi_probe.c
 *
 * 场景：同一个 tracepoint 同时挂载多个 probe——类似真实系统里 ftrace、perf、
 * 自定义内核模块可以同时订阅同一个探测点（例如 `sched_switch`），互不干扰，
 * 各自维护自己的私有数据（Context Injection，见 03 篇）。
 *
 * 真实定义：
 *   - tracepoint_entry_add_probe()（把新 probe 追加到已有数组尾部，
 *     数组以 NULL 结尾）见 kernel/tracepoint.c:112
 *   - allocate_probes()（每次注册都重新分配一块 "现有 N 个 + 新增 1 个 +
 *     NULL 哨兵" 大小的数组，旧数组通过 RCU 延迟释放）见 kernel/tracepoint.c:71
 *   - __DO_TRACE() 里 `do { ...; } while ((++it_func_ptr)->func);` 的
 *     遍历写法（正是"数组以 NULL 结尾"这一约定的直接体现）
 *     见 include/linux/tracepoint.h:127-132
 *   - 真实多消费者共存的例子：`trace_sched_switch()`
 *     （kernel/sched.c:3160）这一个探测点，可以同时被 ftrace 的
 *     sched_switch 事件、perf sched:sched_switch 事件、以及任何自定义
 *     `register_trace_sched_switch()` 的内核模块同时挂载，三者互不知道
 *     对方的存在。
 *
 * 本文件用户态没有真正的 RCU，用普通数组 + realloc 简化"追加/移除"，
 * 但保留了"NULL 结尾数组 + 遍历时无需额外传递数量"这一真实的设计惊喜——
 * 这也是本篇 Failure 小节会提到的"新增 probe 时数组扩容必须正确维护
 * 哨兵"这一类真实存在的实现细节。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o tracepoint_multi_probe tracepoint_multi_probe.c
 * 运行：./tracepoint_multi_probe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct tracepoint_func {
	void *func;
	void *data;
};

struct tracepoint {
	const char *name;
	int enabled;
	struct tracepoint_func *funcs;	/* NULL 结尾数组 */
};

/* 探测点：sched_switch 风格，签名贴近真实 TRACE_EVENT(sched_switch, ...) 的简化版 */
static struct tracepoint __tracepoint_sched_switch = { "sched_switch", 0, NULL };

typedef void (*sched_switch_probe_t)(void *data, const char *prev, const char *next);

static inline void __do_trace_sched_switch(const char *prev, const char *next)
{
	struct tracepoint_func *it = __tracepoint_sched_switch.funcs;

	if (!it)
		return;
	do {
		((sched_switch_probe_t)it->func)(it->data, prev, next);
	} while ((++it)->func);
}

static inline void trace_sched_switch(const char *prev, const char *next)
{
	if (__tracepoint_sched_switch.enabled)
		__do_trace_sched_switch(prev, next);
}

/* ------------------------------------------------------------------ */
/* Core：register/unregister，对应 kernel/tracepoint.c 的真实实现骨架     */
/* ------------------------------------------------------------------ */

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
	new_arr[n + 1].func = NULL;	/* 维护 NULL 哨兵，__DO_TRACE 的遍历依赖它 */
	tp->funcs = new_arr;
	tp->enabled = 1;
	return n + 1;	/* 返回挂载后 probe 总数 */
}

static int tp_unregister(struct tracepoint *tp, void *func, void *data)
{
	int i, n = 0, found = -1;

	if (!tp->funcs)
		return -1;
	for (n = 0; tp->funcs[n].func; n++)
		if (tp->funcs[n].func == func && tp->funcs[n].data == data)
			found = n;
	if (found < 0)
		return -1;
	for (i = found; tp->funcs[i].func; i++)
		tp->funcs[i] = tp->funcs[i + 1];
	if (!tp->funcs[0].func) {
		free(tp->funcs);
		tp->funcs = NULL;
		tp->enabled = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* 三个互不相识的"订阅者"：ftrace 风格、perf 风格、自定义模块风格           */
/* ------------------------------------------------------------------ */

struct ftrace_ctx {
	long event_count;
};

static void probe_ftrace(void *data, const char *prev, const char *next)
{
	struct ftrace_ctx *ctx = data;
	ctx->event_count++;
	printf("    [ftrace ] #%-3ld sched_switch: '%s' -> '%s'\n", ctx->event_count, prev, next);
}

struct perf_ctx {
	long sample_count;
	const char *ring_buffer_name;
};

static void probe_perf(void *data, const char *prev, const char *next)
{
	struct perf_ctx *ctx = data;
	ctx->sample_count++;
	printf("    [perf   ] sample #%ld written to ring buffer '%s' (prev='%s' next='%s')\n",
	       ctx->sample_count, ctx->ring_buffer_name, prev, next);
}

struct custom_mod_ctx {
	const char *watch_task;
	int hits;
};

static void probe_custom_module(void *data, const char *prev, const char *next)
{
	struct custom_mod_ctx *ctx = data;
	/* 自定义模块只关心一个特定任务是否参与了本次切换 */
	if (strcmp(prev, ctx->watch_task) == 0 || strcmp(next, ctx->watch_task) == 0) {
		ctx->hits++;
		printf("    [custom ] watched task '%s' involved in switch (%s -> %s), hit #%d\n",
		       ctx->watch_task, prev, next, ctx->hits);
	}
}

int main(void)
{
	struct ftrace_ctx ftrace_data = { 0 };
	struct perf_ctx perf_data = { 0, "cpu0_ring" };
	struct custom_mod_ctx custom_data = { "nginx", 0 };
	int n;

	printf("=== tracepoint_multi_probe demo ===\n\n");

	printf("-- 触发 2 次 sched_switch，此刻没有任何 probe，Core 仍然无条件调用 trace_sched_switch() --\n");
	trace_sched_switch("idle", "bash");
	trace_sched_switch("bash", "idle");
	printf("  (预期：静默，enabled=%d)\n\n", __tracepoint_sched_switch.enabled);

	printf("-- 依次挂载 3 个互不相识的 probe --\n");
	n = tp_register(&__tracepoint_sched_switch, probe_ftrace, &ftrace_data);
	printf("  [register] probe_ftrace attached, total probes = %d\n", n);
	n = tp_register(&__tracepoint_sched_switch, probe_perf, &perf_data);
	printf("  [register] probe_perf attached, total probes = %d\n", n);
	n = tp_register(&__tracepoint_sched_switch, probe_custom_module, &custom_data);
	printf("  [register] probe_custom_module attached, total probes = %d\n\n", n);

	printf("-- 触发 4 次 sched_switch，三个 probe 应该按注册顺序依次收到同一份事件 --\n");
	trace_sched_switch("idle", "nginx");
	trace_sched_switch("nginx", "sshd");
	trace_sched_switch("sshd", "idle");
	trace_sched_switch("idle", "nginx");

	printf("\n-- 注销中间的 perf probe，只留下 ftrace 与 custom_module --\n");
	tp_unregister(&__tracepoint_sched_switch, probe_perf, &perf_data);
	trace_sched_switch("nginx", "idle");

	printf("\n=== done ===\n");
	printf("统计：ftrace 记录了 %ld 个事件，custom_module 命中 nginx 相关切换 %d 次，\n"
	       "      perf 在被注销前只记录了它存在期间发生的样本，注销之后的最后一次\n"
	       "      切换完全不会再进入 probe_perf()——三个 probe 全程互不知道对方存在，\n"
	       "      trace_sched_switch() 这个 Core 函数也完全不知道挂了几个、是谁。\n",
	       ftrace_data.event_count, custom_data.hits);
	return 0;
}
