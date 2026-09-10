/*
 * scheduler_enqueue_dequeue.c
 *
 * 场景：模拟真实 Kernel kernel/sched.c 里 enqueue_task()/dequeue_task()/
 * put_prev_task() 三个"总入口"函数——它们本身是 sched_class 无关的薄包装，
 * 内部统一转发到 task->sched_class->enqueue_task(...) 这一次盲调
 * （对应 kernel/sched.c:1943 一带的真实代码，本仓库版本）。
 *
 * 本示例额外演示一个 sched_class 体系特有的场景：**任务在不同调度类之间
 * 迁移**（对应真实 Kernel 的 sched_setscheduler()，比如一个任务从普通的
 * SCHED_OTHER 被 chrt 命令改成 SCHED_FIFO）——这会触发：
 *
 *   1. 旧 class 的 dequeue_task()（从旧队列摘除）
 *   2. task->sched_class 指针被重新指向新 class（DI 的"重新绑定"）
 *   3. 新 class 的 enqueue_task()（加入新队列）
 *   4. 新 class 的 switched_to() 回调（对应真实字段，做一些迁移后处理）
 *
 * 完整生命周期：alloc(创建任务) -> init(赋初始 class) -> enqueue(入队)
 *   -> pick/put_prev(参与调度) -> dequeue(出队/睡眠) -> destroy(任务退出)
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o scheduler_enqueue_dequeue scheduler_enqueue_dequeue.c
 * 运行：./scheduler_enqueue_dequeue
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_QUEUE 8

struct sched_class;
struct sched_task;

/* struct sched_class —— 本示例聚焦 enqueue/dequeue/put_prev/switched_to 四个回调 */
struct sched_class {
	const char *name;
	struct sched_task *runqueue[MAX_QUEUE];
	int nr_queued;

	void (*enqueue_task)(struct sched_class *cls, struct sched_task *t);
	void (*dequeue_task)(struct sched_class *cls, struct sched_task *t);
	void (*put_prev_task)(struct sched_class *cls, struct sched_task *t);
	void (*switched_to)(struct sched_class *cls, struct sched_task *t);
};

struct sched_task {
	char name[24];
	struct sched_class *sched_class;   /* 对应 task_struct->sched_class，见 include/linux/sched.h:1235 */
	int  on_rq;                        /* 对应 task_struct->on_rq */
	int  run_count;
};

/* ------------------------------------------------------------------ */
/* Core：class 无关的三个总入口，对应 kernel/sched.c 的同名静态函数      */
/* ------------------------------------------------------------------ */

static void core_enqueue_task(struct sched_task *p)
{
	printf("  [core] enqueue_task('%s') -> sched_class='%s'\n", p->name, p->sched_class->name);
	p->sched_class->enqueue_task(p->sched_class, p);   /* <-- 盲调 */
	p->on_rq = 1;
}

static void core_dequeue_task(struct sched_task *p)
{
	printf("  [core] dequeue_task('%s') -> sched_class='%s'\n", p->name, p->sched_class->name);
	p->sched_class->dequeue_task(p->sched_class, p);   /* <-- 盲调 */
	p->on_rq = 0;
}

static void core_put_prev_task(struct sched_task *p)
{
	p->sched_class->put_prev_task(p->sched_class, p);  /* <-- 盲调 */
}

/* 对应真实 Kernel sched_setscheduler() 内部会走的 class 迁移路径 */
static void core_migrate_class(struct sched_task *p, struct sched_class *new_cls)
{
	struct sched_class *old_cls = p->sched_class;
	int was_queued = p->on_rq;

	printf("[core] migrating '%s' from class '%s' to class '%s'\n",
	       p->name, old_cls->name, new_cls->name);

	if (was_queued)
		core_dequeue_task(p);       /* 先从旧队列摘除 */

	p->sched_class = new_cls;       /* <-- DI 的核心动作：重新绑定契约实现 */

	if (was_queued)
		core_enqueue_task(p);       /* 加入新队列 */

	new_cls->switched_to(new_cls, p);
}

/* ------------------------------------------------------------------ */
/* 两个具体调度类的回调实现：generic_rt / generic_fair                  */
/* ------------------------------------------------------------------ */

static void rq_add(struct sched_class *cls, struct sched_task *t)
{
	cls->runqueue[cls->nr_queued++] = t;
}

static void rq_remove(struct sched_class *cls, struct sched_task *t)
{
	int i, j;
	for (i = 0; i < cls->nr_queued; i++) {
		if (cls->runqueue[i] == t) {
			for (j = i; j < cls->nr_queued - 1; j++)
				cls->runqueue[j] = cls->runqueue[j + 1];
			cls->nr_queued--;
			return;
		}
	}
}

static void rt_enqueue(struct sched_class *cls, struct sched_task *t)
{
	rq_add(cls, t);
	printf("      [rt] '%s' added to RT runqueue (nr_queued=%d)\n", t->name, cls->nr_queued);
}
static void rt_dequeue(struct sched_class *cls, struct sched_task *t)
{
	rq_remove(cls, t);
	printf("      [rt] '%s' removed from RT runqueue (nr_queued=%d)\n", t->name, cls->nr_queued);
}
static void rt_put_prev(struct sched_class *cls, struct sched_task *t)
{
	(void)cls;
	t->run_count++;
	printf("      [rt] '%s' yielded CPU after running (run_count=%d)\n", t->name, t->run_count);
}
static void rt_switched_to(struct sched_class *cls, struct sched_task *t)
{
	(void)cls;
	printf("      [rt] '%s' switched_to RT: boosting priority bookkeeping\n", t->name);
}

static void fair_enqueue(struct sched_class *cls, struct sched_task *t)
{
	rq_add(cls, t);
	printf("      [fair] '%s' added to CFS runqueue (nr_queued=%d)\n", t->name, cls->nr_queued);
}
static void fair_dequeue(struct sched_class *cls, struct sched_task *t)
{
	rq_remove(cls, t);
	printf("      [fair] '%s' removed from CFS runqueue (nr_queued=%d)\n", t->name, cls->nr_queued);
}
static void fair_put_prev(struct sched_class *cls, struct sched_task *t)
{
	(void)cls;
	t->run_count++;
	printf("      [fair] '%s' yielded CPU after running (run_count=%d)\n", t->name, t->run_count);
}
static void fair_switched_to(struct sched_class *cls, struct sched_task *t)
{
	(void)cls;
	printf("      [fair] '%s' switched_to CFS: resetting vruntime baseline\n", t->name);
}

int main(void)
{
	struct sched_class rt_cls = {
		.name = "rt", .enqueue_task = rt_enqueue, .dequeue_task = rt_dequeue,
		.put_prev_task = rt_put_prev, .switched_to = rt_switched_to,
	};
	struct sched_class fair_cls = {
		.name = "fair", .enqueue_task = fair_enqueue, .dequeue_task = fair_dequeue,
		.put_prev_task = fair_put_prev, .switched_to = fair_switched_to,
	};

	struct sched_task t1 = { .name = "worker1", .sched_class = &fair_cls };
	struct sched_task t2 = { .name = "worker2", .sched_class = &fair_cls };

	printf("=== scheduler_enqueue_dequeue demo ===\n\n");

	printf("-- alloc/init：worker1/worker2 初始归属 fair class --\n\n");

	printf("-- enqueue：两个任务加入 fair 运行队列 --\n");
	core_enqueue_task(&t1);
	core_enqueue_task(&t2);

	printf("\n-- 参与调度：模拟 worker1 被选中运行一个时间片后 put_prev_task --\n");
	core_put_prev_task(&t1);

	printf("\n-- dequeue：worker2 睡眠等待 I/O，从队列移除 --\n");
	core_dequeue_task(&t2);
	printf("  fair_cls.nr_queued (预期 1，只剩 worker1) = %d\n", fair_cls.nr_queued);

	printf("\n-- 迁移：chrt 把 worker1 改成 SCHED_FIFO，从 fair 迁移到 rt --\n");
	core_migrate_class(&t1, &rt_cls);
	printf("  fair_cls.nr_queued (预期 0) = %d, rt_cls.nr_queued (预期 1) = %d\n",
	       fair_cls.nr_queued, rt_cls.nr_queued);

	printf("\n-- worker2 唤醒，重新 enqueue 回它当前的 class（仍是 fair）--\n");
	core_enqueue_task(&t2);

	printf("\n-- destroy：两个任务退出前先各自 dequeue --\n");
	core_dequeue_task(&t1);
	core_dequeue_task(&t2);

	printf("\n=== done ===\n");
	printf("worker1.run_count=%d (在 fair 里跑过一次), worker1 最终所属 class = %s\n",
	       t1.run_count, t1.sched_class->name);
	printf("worker2.run_count=%d, rt_cls.nr_queued=%d, fair_cls.nr_queued=%d (全部应为 0)\n",
	       t2.run_count, rt_cls.nr_queued, fair_cls.nr_queued);
	return 0;
}
