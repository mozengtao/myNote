/*
 * percpu_softirq_pending.c
 *
 * 场景：模拟真实内核"每 CPU 独立的 pending softirq 位图"这一设计
 * （对应真实内核 kernel/softirq.c:50-53 的
 * `irq_cpustat_t irq_stat[NR_CPUS]` + `arch/x86/include/asm/hardirq.h`
 * 的 `__softirq_pending` 字段，通过 `local_softirq_pending()` 宏访问）。
 *
 * 核心教学点：softirq_vec[] 这张"函数指针表"是**全局共享、只读**的
 * （对应 kernel/softirq.c:55 的 `static struct softirq_action softirq_vec[]`），
 * 但每个 CPU 有*自己独立*的 pending 位图（`irq_stat[cpu].__softirq_pending`）
 * 和自己独立的 `do_softirq()` 执行——一个 CPU 触发/处理 softirq 完全不影响
 * 另一个 CPU 的 pending 状态，天然免锁（每 CPU 数据不需要跨 CPU 同步），
 * 这是真实注释里"No shared variables, all the data are CPU local"这条设计
 * 原则的直接体现（kernel/softirq.c:32-48 附近的大段注释）。
 *
 * 用 pthread 模拟"多个 CPU"，每个线程独立触发/处理自己的 softirq，
 * 用互不干扰的计数器验证隔离性。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o percpu_softirq_pending percpu_softirq_pending.c
 * 运行：
 *   ./percpu_softirq_pending
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_CPUS      4
#define NR_SOFTIRQS   2
#define ITER_PER_CPU  50

enum { SIM_NET_RX_SOFTIRQ = 0, SIM_TIMER_SOFTIRQ = 1 };

/* ------------------------------------------------------------------ */
/* 1. 全局共享、只读的函数指针表 —— 所有 CPU 共用同一份，不需要每 CPU 复制 */
/*    对应真实内核 kernel/softirq.c:55                                  */
/* ------------------------------------------------------------------ */

struct softirq_action_sim { void (*action)(int cpu); };
static struct softirq_action_sim softirq_vec[NR_SOFTIRQS];

static void open_softirq(int nr, void (*action)(int cpu))
{
	softirq_vec[nr].action = action;
}

/* ------------------------------------------------------------------ */
/* 2. 每 CPU 独立的 pending 位图 + 统计 —— 对应 irq_stat[cpu].__softirq_pending*/
/* ------------------------------------------------------------------ */

struct percpu_softirq_state {
	int cpu_id;
	uint32_t pending;             /* 对应 irq_stat[cpu].__softirq_pending */
	unsigned long net_rx_runs;
	unsigned long timer_runs;
	unsigned long total_raises;
	pthread_mutex_t lock;         /* 只保护"这个 CPU 自己的" pending，
	                                * 不与其它 CPU 共享，所以没有跨 CPU 竞争 */
};

static struct percpu_softirq_state percpu[NUM_CPUS];

/* 对应真实内核的 raise_softirq()：只操作 *当前 CPU* 自己的 pending 位图 */
static void raise_softirq_on_cpu(struct percpu_softirq_state *st, unsigned int nr)
{
	pthread_mutex_lock(&st->lock);
	st->pending |= (1u << nr);
	st->total_raises++;
	pthread_mutex_unlock(&st->lock);
}

/* 对应真实内核的 do_softirq()：只处理 *当前 CPU* 自己的 pending 位图 */
static void do_softirq_on_cpu(struct percpu_softirq_state *st)
{
	pthread_mutex_lock(&st->lock);
	uint32_t pending = st->pending;
	st->pending = 0;
	pthread_mutex_unlock(&st->lock);

	for (int i = 0; i < NR_SOFTIRQS; i++) {
		if ((pending & (1u << i)) && softirq_vec[i].action)
			softirq_vec[i].action(st->cpu_id);   /* <-- 盲调，共享的函数指针表 */
	}
}

/* ------------------------------------------------------------------ */
/* 3. softirq action 实现：读取"当前是哪个 CPU 在跑"，各自累加各自的计数器  */
/* ------------------------------------------------------------------ */

static void net_rx_action(int cpu)
{
	pthread_mutex_lock(&percpu[cpu].lock);
	percpu[cpu].net_rx_runs++;
	pthread_mutex_unlock(&percpu[cpu].lock);
}

static void timer_action(int cpu)
{
	pthread_mutex_lock(&percpu[cpu].lock);
	percpu[cpu].timer_runs++;
	pthread_mutex_unlock(&percpu[cpu].lock);
}

/* ------------------------------------------------------------------ */
/* 4. 每个模拟 CPU 的工作线程：独立触发 + 独立处理，互不通信                */
/* ------------------------------------------------------------------ */

static void *cpu_worker(void *arg)
{
	struct percpu_softirq_state *st = arg;

	for (int i = 0; i < ITER_PER_CPU; i++) {
		/* CPU 0/2 更偏重网络负载，CPU 1/3 更偏重定时器负载，
		 * 模拟真实系统里不同 CPU 上运行的中断亲和性/负载不均 */
		if (st->cpu_id % 2 == 0)
			raise_softirq_on_cpu(st, SIM_NET_RX_SOFTIRQ);
		else
			raise_softirq_on_cpu(st, SIM_TIMER_SOFTIRQ);

		if (i % 3 == 0) /* 偶尔两种都触发，模拟同一批里多种事件都发生 */
			raise_softirq_on_cpu(st, st->cpu_id % 2 == 0 ? SIM_TIMER_SOFTIRQ : SIM_NET_RX_SOFTIRQ);

		do_softirq_on_cpu(st); /* 立即统一处理这个 CPU 自己的 pending 位图 */
	}
	return NULL;
}

int main(void)
{
	pthread_t threads[NUM_CPUS];

	printf("=== percpu_softirq_pending demo ===\n\n");

	open_softirq(SIM_NET_RX_SOFTIRQ, net_rx_action);
	open_softirq(SIM_TIMER_SOFTIRQ, timer_action);

	for (int i = 0; i < NUM_CPUS; i++) {
		percpu[i].cpu_id = i;
		percpu[i].pending = 0;
		percpu[i].net_rx_runs = 0;
		percpu[i].timer_runs = 0;
		percpu[i].total_raises = 0;
		pthread_mutex_init(&percpu[i].lock, NULL);
	}

	printf("-- 启动 %d 个模拟 CPU，每个各自独立触发/处理 %d 轮 softirq --\n",
	       NUM_CPUS, ITER_PER_CPU);
	for (int i = 0; i < NUM_CPUS; i++)
		pthread_create(&threads[i], NULL, cpu_worker, &percpu[i]);

	for (int i = 0; i < NUM_CPUS; i++)
		pthread_join(threads[i], NULL);

	printf("\n-- 每个 CPU 的最终统计（验证互不干扰：偶数 CPU 应以 NET_RX 为主，"
	       "奇数 CPU 应以 TIMER 为主） --\n");
	unsigned long total_net_rx = 0, total_timer = 0, total_raises = 0;
	for (int i = 0; i < NUM_CPUS; i++) {
		printf("  CPU%d: net_rx_runs=%-3lu timer_runs=%-3lu total_raises=%-3lu "
		       "final pending=0x%02x\n",
		       i, percpu[i].net_rx_runs, percpu[i].timer_runs,
		       percpu[i].total_raises, percpu[i].pending);
		total_net_rx += percpu[i].net_rx_runs;
		total_timer += percpu[i].timer_runs;
		total_raises += percpu[i].total_raises;
	}

	printf("\n-- 验证：所有 CPU 处理完后，pending 位图必须全部清零"
	       "（不会有一个 CPU 的活漏到另一个 CPU 头上） --\n");
	int all_clean = 1;
	for (int i = 0; i < NUM_CPUS; i++) {
		if (percpu[i].pending != 0)
			all_clean = 0;
	}
	printf("  all_pending_cleared = %s\n", all_clean ? "YES" : "NO");

	for (int i = 0; i < NUM_CPUS; i++)
		pthread_mutex_destroy(&percpu[i].lock);

	printf("\n=== done: total_net_rx=%lu, total_timer=%lu, total_raises=%lu ===\n",
	       total_net_rx, total_timer, total_raises);
	return 0;
}
