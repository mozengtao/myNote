/*
 * sched_class_chain.c
 *
 * 场景：模拟真实 Kernel kernel/sched.c 里 pick_next_task() 的核心逻辑——
 * 多个 struct sched_class 按固定优先级用 ->next 指针串成一条链：
 *
 *   stop_sched_class -> rt_sched_class -> fair_sched_class -> idle_sched_class
 *
 * 每次调度时，Core 从链头开始依次调用每个 class 的 pick_next_task()，
 * 只要有一个 class 返回非 NULL（表示"这个 class 里有可运行的任务"），
 * 立刻停止遍历——这与 05 篇 notifier chain 的"广播 + STOP"语义高度相似，
 * 但语义相反：notifier chain 是"广播给所有人，谁反对就否决"；
 * sched_class 链是"依次尝试，谁先接受就归谁"（更接近"责任链模式"）。
 *
 * 本示例还原真实 Kernel 三个关键特性：
 *   1. 链的顺序是编译期固定的（这里用静态初始化模拟 .next 指针）。
 *   2. Core（pick_next_task）对 4 个 class 一视同仁，不知道具体是
 *      stop/rt/fair/idle 中的哪一个。
 *   3. idle class 永远兜底返回非 NULL（对应真实 Kernel 里
 *      "the idle class will always have a runnable task"的 BUG() 保证）。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o sched_class_chain sched_class_chain.c
 * 运行：./sched_class_chain
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct rq;
struct task;
struct sched_class;

#define MAX_TASKS 8

/* struct task —— 对应 struct task_struct 的极简版 */
struct task {
	char name[24];
	int  runnable;   /* 是否处于可运行状态 */
};

/*
 * struct sched_class —— 对应真实 include/linux/sched.h:1084 的
 * struct sched_class。这里只保留驱动本示例所需的两个回调：
 * pick_next_task（本篇核心）和 class 自身的名字/优先级说明。
 */
struct sched_class {
	const char *name;
	const struct sched_class *next;   /* 链式指针，对应真实字段 */
	struct task *tasks[MAX_TASKS];
	int nr_tasks;

	/* Core 唯一认识的契约：尝试从本 class 里挑一个任务出来 */
	struct task *(*pick_next_task)(struct sched_class *cls);
};

/* struct rq —— 对应 struct rq（per-CPU 运行队列），这里只是持有链头 */
struct rq {
	const struct sched_class *sched_class_highest;
};

/* ------------------------------------------------------------------ */
/* 4 种调度类各自的 pick_next_task 实现                                 */
/* ------------------------------------------------------------------ */

static struct task *pick_next_task_stop(struct sched_class *cls)
{
	int i;
	for (i = 0; i < cls->nr_tasks; i++)
		if (cls->tasks[i]->runnable)
			return cls->tasks[i];
	return NULL;   /* 通常没有 stop 任务，链会继续往下走 */
}

static struct task *pick_next_task_rt(struct sched_class *cls)
{
	int i;
	for (i = 0; i < cls->nr_tasks; i++)
		if (cls->tasks[i]->runnable)
			return cls->tasks[i];
	return NULL;
}

static struct task *pick_next_task_fair(struct sched_class *cls)
{
	int i;
	for (i = 0; i < cls->nr_tasks; i++)
		if (cls->tasks[i]->runnable)
			return cls->tasks[i];
	return NULL;
}

static struct task idle_task = { .name = "swapper/0", .runnable = 1 };

static struct task *pick_next_task_idle(struct sched_class *cls)
{
	(void)cls;
	/* 对应真实 Kernel："the idle class will always have a runnable task" */
	return &idle_task;
}

/* ------------------------------------------------------------------ */
/* Core：总线无关……不对，是"调度类无关"的 pick_next_task 遍历逻辑        */
/* 对应真实 kernel/sched.c:4368 pick_next_task() 里的 for_each_class()  */
/* ------------------------------------------------------------------ */

static struct task *core_pick_next_task(struct rq *rq)
{
	const struct sched_class *class;

	for (class = rq->sched_class_highest; class; class = class->next) {
		struct task *p = ((struct sched_class *)class)->pick_next_task((struct sched_class *)class);
		printf("  [core] trying class '%s'... %s\n", class->name,
		       p ? "accepted" : "declined (no runnable task), trying next");
		if (p)
			return p;
	}
	/* 真实 Kernel 用 BUG()，这里退化为 abort，因为 idle class 理论上不会到这一步 */
	fprintf(stderr, "fatal: no class accepted a task (should never happen)\n");
	abort();
}

int main(void)
{
	struct task t_stop_migrate = { .name = "migration/0", .runnable = 0 };
	struct task t_rt_irq       = { .name = "irq-handler", .runnable = 0 };
	struct task t_fair_bash    = { .name = "bash",        .runnable = 1 };
	struct task t_fair_make    = { .name = "make",        .runnable = 1 };

	struct sched_class idle_sched_class = {
		.name = "idle", .next = NULL, .pick_next_task = pick_next_task_idle,
	};
	struct sched_class fair_sched_class = {
		.name = "fair(CFS)", .next = &idle_sched_class, .pick_next_task = pick_next_task_fair,
	};
	struct sched_class rt_sched_class = {
		.name = "rt", .next = &fair_sched_class, .pick_next_task = pick_next_task_rt,
	};
	struct sched_class stop_sched_class = {
		.name = "stop", .next = &rt_sched_class, .pick_next_task = pick_next_task_stop,
	};

	struct rq rq = { .sched_class_highest = &stop_sched_class };

	printf("=== sched_class_chain demo ===\n\n");
	printf("链的顺序（编译期固定）：stop -> rt -> fair(CFS) -> idle\n\n");

	printf("-- 场景 1：所有 class 都没有可运行任务，只有 fair 里的 'bash' 可运行 --\n");
	fair_sched_class.tasks[fair_sched_class.nr_tasks++] = &t_fair_bash;
	fair_sched_class.tasks[fair_sched_class.nr_tasks++] = &t_fair_make;
	t_fair_make.runnable = 0;
	stop_sched_class.tasks[stop_sched_class.nr_tasks++] = &t_stop_migrate;
	rt_sched_class.tasks[rt_sched_class.nr_tasks++] = &t_rt_irq;

	struct task *picked = core_pick_next_task(&rq);
	printf("  => picked task: '%s'\n\n", picked->name);

	printf("-- 场景 2：rt 里的 irq-handler 变为可运行，应该抢先于 fair 被选中 --\n");
	t_rt_irq.runnable = 1;
	picked = core_pick_next_task(&rq);
	printf("  => picked task: '%s'\n\n", picked->name);

	printf("-- 场景 3：stop 里的 migration 也变为可运行，应该抢先于 rt/fair --\n");
	t_stop_migrate.runnable = 1;
	picked = core_pick_next_task(&rq);
	printf("  => picked task: '%s'\n\n", picked->name);

	printf("-- 场景 4：所有 class 都没有可运行任务，最终兜底到 idle --\n");
	t_stop_migrate.runnable = 0;
	t_rt_irq.runnable = 0;
	t_fair_bash.runnable = 0;
	picked = core_pick_next_task(&rq);
	printf("  => picked task: '%s'\n", picked->name);

	printf("\n=== done ===\n");
	return 0;
}
