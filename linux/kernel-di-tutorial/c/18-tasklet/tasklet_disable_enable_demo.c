/*
 * tasklet_disable_enable_demo.c
 *
 * 场景：演示 tasklet_disable()/tasklet_enable() 与并发调度请求的交互，
 * 以及"同一个 tasklet 绝不会在多个 CPU 上并发执行"这条核心保证。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   include/linux/interrupt.h:590  tasklet_disable_nosync()  atomic_inc(&t->count)
 *   include/linux/interrupt.h:596  tasklet_disable()          disable_nosync + tasklet_unlock_wait
 *   include/linux/interrupt.h:603  tasklet_enable()           atomic_dec(&t->count)
 *   include/linux/interrupt.h:538  tasklet_trylock()          test_and_set_bit(TASKLET_STATE_RUN)
 *   kernel/softirq.c:459-467       tasklet_action() 里 count>0 时的"推迟"逻辑
 *   真实驱动用法：drivers/net/ethernet/silan/sc92031.c:370/378
 *     sc92031_disable_interrupts() 里 tasklet_disable(&priv->tasklet)，
 *     sc92031_enable_interrupts() 里 tasklet_enable(&priv->tasklet)
 *
 * 关键点：
 *   1. disable 只是让 count>0，并不会清掉 pending 链表里已经排队的调度请求——
 *      调度请求被"记住"，只是推迟到 enable 之后才真正执行。
 *   2. disable() 会等待"当前正在执行的这一次"跑完（tasklet_unlock_wait），
 *      但不会等待"未来才会被排队"的执行——这正是本例用多个"虚拟 CPU"线程
 *      反复调用 tasklet_action 风格代码时，需要用 RUN 位保证互斥的原因。
 *   3. 用原子计数器 currently_running 实测验证：不管有多少个"CPU"线程在跑
 *      tasklet_action 循环，同一个 tasklet 的 func() 永远不会被两个线程同时执行。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o tasklet_disable_enable_demo tasklet_disable_enable_demo.c
 * 运行：
 *   ./tasklet_disable_enable_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>

enum {
	TASKLET_BIT_SCHED = 0,
	TASKLET_BIT_RUN   = 1,
};

struct mini_tasklet {
	struct mini_tasklet *next;
	atomic_ulong state;           /* 多线程下必须是真正的原子操作，对应真实 unsigned long state */
	atomic_int count;             /* 对应真实 atomic_t count */
	void (*func)(unsigned long);
	unsigned long data;
	const char *name;
};

static void mini_tasklet_init(struct mini_tasklet *t, void (*fn)(unsigned long),
			       unsigned long data, const char *name)
{
	t->next = NULL;
	atomic_store(&t->state, 0);
	atomic_store(&t->count, 0);
	t->func = fn;
	t->data = data;
	t->name = name;
}

/* ------------------------------------------------------------------ */
/* Core：pending 链表 + 多个"虚拟 CPU"worker 线程共享的调度队列          */
/* ------------------------------------------------------------------ */

#define NR_VIRT_CPUS 4

struct tasklet_head {
	struct mini_tasklet *head;
	struct mini_tasklet **tail;
	pthread_mutex_t lock;   /* 真实内核用 local_irq_save；用户态用 mutex 代替 */
};

static struct tasklet_head g_tv;
static atomic_int g_stop;
static atomic_long g_exec_count;
static atomic_int g_currently_running;   /* 用于检测"是否被并发执行" */
static atomic_int g_max_concurrent_seen;

static void tv_init(struct tasklet_head *tv)
{
	tv->head = NULL;
	tv->tail = &tv->head;
	pthread_mutex_init(&tv->lock, NULL);
}

static void tv_enqueue(struct tasklet_head *tv, struct mini_tasklet *t)
{
	pthread_mutex_lock(&tv->lock);
	t->next = NULL;
	*tv->tail = t;
	tv->tail = &t->next;
	pthread_mutex_unlock(&tv->lock);
}

static struct mini_tasklet *tv_dequeue(struct tasklet_head *tv)
{
	struct mini_tasklet *t = NULL;

	pthread_mutex_lock(&tv->lock);
	if (tv->head) {
		t = tv->head;
		tv->head = t->next;
		if (!tv->head)
			tv->tail = &tv->head;
	}
	pthread_mutex_unlock(&tv->lock);
	return t;
}

/* 对应 include/linux/interrupt.h:561 tasklet_schedule() */
static void mini_tasklet_schedule(struct mini_tasklet *t)
{
	unsigned long old = atomic_fetch_or(&t->state, 1UL << TASKLET_BIT_SCHED);
	if (old & (1UL << TASKLET_BIT_SCHED))
		return;   /* 已经排队，去重 */
	tv_enqueue(&g_tv, t);
}

/* 对应 kernel/softirq.c:444 tasklet_action()，本例里每个"虚拟 CPU"线程都跑这份逻辑 */
static void run_one_pending_tasklet(int cpu_id)
{
	struct mini_tasklet *t = tv_dequeue(&g_tv);
	if (!t)
		return;

	/* tasklet_trylock()：对应 include/linux/interrupt.h:538 */
	unsigned long old = atomic_fetch_or(&t->state, 1UL << TASKLET_BIT_RUN);
	if (old & (1UL << TASKLET_BIT_RUN)) {
		/* 已经在别的 CPU 上运行，重新入队等下一轮 */
		tv_enqueue(&g_tv, t);
		return;
	}

	if (atomic_load(&t->count) > 0) {
		/* 对应 tasklet_disable() 生效期间：count>0，暂不执行，重新入队 */
		atomic_fetch_and(&t->state, ~(1UL << TASKLET_BIT_RUN));
		tv_enqueue(&g_tv, t);
		return;
	}

	atomic_fetch_and(&t->state, ~(1UL << TASKLET_BIT_SCHED));

	/* ---- 并发检测区：验证同一 tasklet 绝不会被两个线程同时执行 ---- */
	int now = atomic_fetch_add(&g_currently_running, 1) + 1;
	int prev_max = atomic_load(&g_max_concurrent_seen);
	while (now > prev_max &&
	       !atomic_compare_exchange_weak(&g_max_concurrent_seen, &prev_max, now))
		;

	printf("    [cpu%d] 执行 '%s'->func(data=%lu)\n", cpu_id, t->name, t->data);
	t->func(t->data);
	usleep(2000);   /* 故意留一点执行窗口，放大"如果没有互斥会并发"的概率 */

	atomic_fetch_sub(&g_currently_running, 1);
	atomic_fetch_add(&g_exec_count, 1);
	/* ---------------------------------------------------------- */

	atomic_fetch_and(&t->state, ~(1UL << TASKLET_BIT_RUN));
}

struct worker_arg {
	int cpu_id;
};

static void *cpu_worker(void *arg)
{
	struct worker_arg *wa = arg;
	while (!atomic_load(&g_stop)) {
		run_one_pending_tasklet(wa->cpu_id);
		usleep(500);
	}
	return NULL;
}

/* ------------------------------------------------------------------ */
/* 业务回调：一个被反复调度的 tasklet                                   */
/* ------------------------------------------------------------------ */

static atomic_long g_flush_seq;

static void flush_tasklet_fn(unsigned long data)
{
	long seq = atomic_fetch_add(&g_flush_seq, 1);
	(void)data;
	(void)seq;
}

int main(void)
{
	struct mini_tasklet t;
	pthread_t cpus[NR_VIRT_CPUS];
	struct worker_arg args[NR_VIRT_CPUS];

	mini_tasklet_init(&t, flush_tasklet_fn, 0xdead, "log_flush_tasklet");
	tv_init(&g_tv);
	atomic_store(&g_stop, 0);
	atomic_store(&g_exec_count, 0);
	atomic_store(&g_currently_running, 0);
	atomic_store(&g_max_concurrent_seen, 0);

	printf("=== tasklet_disable_enable_demo ===\n\n");
	printf("-- 启动 %d 个模拟 CPU worker 线程，反复执行 pending tasklet --\n", NR_VIRT_CPUS);
	for (int i = 0; i < NR_VIRT_CPUS; i++) {
		args[i].cpu_id = i;
		pthread_create(&cpus[i], NULL, cpu_worker, &args[i]);
	}

	printf("\n-- 阶段1：正常调度，观察多个 CPU 线程互斥执行同一个 tasklet --\n");
	for (int i = 0; i < 20; i++) {
		mini_tasklet_schedule(&t);
		usleep(1000);
	}
	usleep(50 * 1000);
	printf("  执行次数=%ld，观测到的最大并发数=%d（预期恒为 1）\n",
	       atomic_load(&g_exec_count), atomic_load(&g_max_concurrent_seen));

	printf("\n-- 阶段2：tasklet_disable()，模拟驱动关中断期间暂停调度执行 --\n");
	atomic_fetch_add(&t.count, 1);   /* tasklet_disable_nosync */
	/* tasklet_unlock_wait：等待当前若有正在跑的实例结束（此处轮询模拟） */
	while (atomic_load(&t.state) & (1UL << TASKLET_BIT_RUN))
		usleep(100);
	printf("  disable 完成，count=%d\n", atomic_load(&t.count));

	long before = atomic_load(&g_exec_count);
	printf("  disable 期间连续调度 10 次（模拟 10 次网卡中断到来）...\n");
	for (int i = 0; i < 10; i++) {
		mini_tasklet_schedule(&t);
		usleep(1000);
	}
	usleep(50 * 1000);
	long after_disabled = atomic_load(&g_exec_count);
	printf("  disable 期间 exec_count 变化: %ld -> %ld（预期不变，调度请求被推迟而非丢弃）\n",
	       before, after_disabled);

	printf("\n-- 阶段3：tasklet_enable()，之前被推迟的调度请求应该立刻被执行 --\n");
	atomic_fetch_sub(&t.count, 1);
	usleep(50 * 1000);
	long after_enabled = atomic_load(&g_exec_count);
	printf("  enable 后 exec_count 变化: %ld -> %ld（预期立刻追上，说明请求没有丢失）\n",
	       after_disabled, after_enabled);

	atomic_store(&g_stop, 1);
	for (int i = 0; i < NR_VIRT_CPUS; i++)
		pthread_join(cpus[i], NULL);

	printf("\n=== done: 总执行次数=%ld, 观测到的最大并发执行数=%d ===\n",
	       atomic_load(&g_exec_count), atomic_load(&g_max_concurrent_seen));
	if (atomic_load(&g_max_concurrent_seen) > 1)
		printf("!!! 异常：同一 tasklet 被并发执行了，互斥机制失效 !!!\n");
	else
		printf("验证通过：同一 tasklet 在任意时刻只在一个 CPU 上运行。\n");
	return 0;
}
