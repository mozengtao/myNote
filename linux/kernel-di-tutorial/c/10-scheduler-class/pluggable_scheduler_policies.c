/*
 * pluggable_scheduler_policies.c
 *
 * 场景：验证"调度策略是可插拔的 ops table"——同一组任务，分别交给两种
 * 完全不同的调度策略类去排序，观察调度顺序的差异。对应真实 Kernel里
 * fair_sched_class（CFS，按虚拟运行时间 vruntime 排序，力求"公平"）
 * 与 rt_sched_class（FIFO/RR，严格按优先级排序，高优先级永远先跑）
 * 两种策略共存、但排序逻辑完全不同的事实。
 *
 * 本示例实现两个策略类：
 *   - fifo_policy：简化版"实时 FIFO"——严格按 priority 字段排序，
 *     priority 相同则按到达顺序（类似 rt_sched_class 的做法）。
 *   - weighted_fair_policy：简化版 CFS——每个任务有一个 weight，
 *     按"累计已运行时间 / weight"（近似 vruntime）挑选最小的一个，
 *     每次被选中后累加运行时间，模拟"公平"轮转。
 *
 * 两种策略共用同一个 struct policy_class 契约（pick_next 函数指针），
 * Core（run_scheduler_rounds）完全不知道内部排序算法，只调用 pick_next。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o pluggable_scheduler_policies pluggable_scheduler_policies.c
 * 运行：./pluggable_scheduler_policies
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TASKS 6

struct sched_task {
	char name[24];
	int  priority;       /* fifo_policy 用：数字越小优先级越高 */
	int  weight;         /* weighted_fair_policy 用：权重越大越该多跑 */
	long vruntime;       /* weighted_fair_policy 用：累计"虚拟运行时间" */
	int  finished_rounds;
};

/*
 * struct policy_class —— 对应真实 struct sched_class 的"策略"维度。
 * pick_next：从任务集合里选出下一个该运行的任务（不出队，只是"选择"）。
 * on_ran：任务运行了一个时间片之后，策略自己更新内部状态（如 vruntime）。
 */
struct policy_class {
	const char *name;
	struct sched_task *(*pick_next)(struct policy_class *pc);
	void (*on_ran)(struct policy_class *pc, struct sched_task *t);
	struct sched_task *tasks[MAX_TASKS];
	int nr_tasks;
};

/* ------------------------------------------------------------------ */
/* 策略 1：FIFO —— 严格按 priority 排序，对应 rt_sched_class 的思路      */
/* ------------------------------------------------------------------ */

static struct sched_task *fifo_pick_next(struct policy_class *pc)
{
	struct sched_task *best = NULL;
	int i;
	for (i = 0; i < pc->nr_tasks; i++) {
		struct sched_task *t = pc->tasks[i];
		if (!best || t->priority < best->priority)
			best = t;
	}
	return best;
}

static void fifo_on_ran(struct policy_class *pc, struct sched_task *t)
{
	(void)pc;
	t->finished_rounds++;
	/* FIFO：只要还是最高优先级，会一直连续被选中，直到它自己让出（这里简化为跑 3 轮后降级） */
	if (t->finished_rounds >= 3)
		t->priority += 100; /* 模拟"跑完自己的份额后主动让出"，避免示例死循环 */
}

/* ------------------------------------------------------------------ */
/* 策略 2：加权公平 —— 近似 CFS 的 vruntime 排序思路                     */
/* ------------------------------------------------------------------ */

static struct sched_task *wfair_pick_next(struct policy_class *pc)
{
	struct sched_task *best = NULL;
	int i;
	for (i = 0; i < pc->nr_tasks; i++) {
		struct sched_task *t = pc->tasks[i];
		if (!best || t->vruntime < best->vruntime)
			best = t;
	}
	return best;
}

static void wfair_on_ran(struct policy_class *pc, struct sched_task *t)
{
	(void)pc;
	/* vruntime 增量与 weight 成反比：权重越大，vruntime 增长越慢，越容易再次被选中 */
	t->vruntime += 1000 / t->weight;
	t->finished_rounds++;
}

/* ------------------------------------------------------------------ */
/* Core：策略无关的调度轮转，对应 __schedule() 里 pick_next_task() 之后  */
/* 紧跟的 "运行一个时间片 -> 更新状态 -> 再次调度" 循环                   */
/* ------------------------------------------------------------------ */

static void run_scheduler_rounds(struct policy_class *pc, int rounds)
{
	int r;
	printf("-- 策略 '%s'，运行 %d 轮调度 --\n", pc->name, rounds);
	for (r = 0; r < rounds; r++) {
		struct sched_task *t = pc->pick_next(pc);   /* <-- 盲调 */
		printf("  round %2d: picked '%-10s'", r + 1, t->name);
		if (strcmp(pc->name, "weighted-fair") == 0)
			printf(" (vruntime=%ld)", t->vruntime);
		else
			printf(" (priority=%d)", t->priority);
		printf("\n");
		pc->on_ran(pc, t);                          /* <-- 盲调 */
	}
}

int main(void)
{
	struct sched_task fifo_high = { .name = "irq-relay", .priority = 1 };
	struct sched_task fifo_mid  = { .name = "audio-out", .priority = 5 };
	struct sched_task fifo_low  = { .name = "logger",    .priority = 9 };

	struct sched_task wf_heavy  = { .name = "video-enc", .weight = 4, .vruntime = 0 };
	struct sched_task wf_normal = { .name = "browser",   .weight = 2, .vruntime = 0 };
	struct sched_task wf_light  = { .name = "idle-timer",.weight = 1, .vruntime = 0 };

	struct policy_class fifo_policy = {
		.name = "fifo", .pick_next = fifo_pick_next, .on_ran = fifo_on_ran,
	};
	struct policy_class wfair_policy = {
		.name = "weighted-fair", .pick_next = wfair_pick_next, .on_ran = wfair_on_ran,
	};

	fifo_policy.tasks[fifo_policy.nr_tasks++] = &fifo_high;
	fifo_policy.tasks[fifo_policy.nr_tasks++] = &fifo_mid;
	fifo_policy.tasks[fifo_policy.nr_tasks++] = &fifo_low;

	wfair_policy.tasks[wfair_policy.nr_tasks++] = &wf_heavy;
	wfair_policy.tasks[wfair_policy.nr_tasks++] = &wf_normal;
	wfair_policy.tasks[wfair_policy.nr_tasks++] = &wf_light;

	printf("=== pluggable_scheduler_policies demo ===\n\n");

	run_scheduler_rounds(&fifo_policy, 6);
	printf("\n");
	run_scheduler_rounds(&wfair_policy, 8);

	printf("\n=== 结果对比 ===\n");
	printf("FIFO 策略：irq-relay(priority=1) 连续跑 3 轮后让出，才轮到 audio-out\n");
	printf("  irq-relay finished_rounds=%d, audio-out finished_rounds=%d, logger finished_rounds=%d\n",
	       fifo_high.finished_rounds, fifo_mid.finished_rounds, fifo_low.finished_rounds);
	printf("加权公平策略：weight 越大，被选中次数越多，且分布是交替的而非连续独占\n");
	printf("  video-enc(weight=4) finished_rounds=%d, browser(weight=2) finished_rounds=%d, idle-timer(weight=1) finished_rounds=%d\n",
	       wf_heavy.finished_rounds, wf_normal.finished_rounds, wf_light.finished_rounds);

	printf("\n=== done ===\n");
	return 0;
}
