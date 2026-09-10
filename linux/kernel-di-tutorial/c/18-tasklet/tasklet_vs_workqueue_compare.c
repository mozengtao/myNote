/*
 * tasklet_vs_workqueue_compare.c
 *
 * 场景：同一个"任务"（把一段日志刷到慢速存储）分别用
 *   (a) "tasklet 风格"——模拟软中断上下文，禁止阻塞
 *   (b) "workqueue 风格"——独立线程（模拟 process context），允许阻塞
 * 两种方式实现，对比二者在能做什么/不能做什么上的差异。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   kernel/softirq.c:504   "tasklet is running only on one CPU simultaneously"
 *                          注释同时说明 tasklet 执行环境等价于软中断上下文
 *   include/linux/interrupt.h:515-522   struct tasklet_struct（不可睡眠上下文的载体）
 *   kernel/workqueue.c（worker_thread）与 07-workqueue.md 已经建模的
 *                          "任务在独立内核线程里执行，可以睡眠"
 *   include/linux/kernel.h 的 might_sleep() / CONFIG_DEBUG_ATOMIC_SLEEP：
 *                          真实内核用 in_atomic()/in_softirq() 检查来 BUG()/警告
 *                          "调度或睡眠发生在不允许睡眠的上下文"，本文件的
 *                          in_softirq()/slow_blocking_io() 是这套机制的简化建模。
 *
 * 关键点：
 *   1. tasklet 的执行环境等价于软中断上下文——不能调用任何可能睡眠/阻塞的函数
 *      （拿 mutex、等 I/O、kmalloc(GFP_KERNEL) 等），这是它比 workqueue "受限"的地方。
 *   2. workqueue 的任务在独立的内核线程（可被调度出去）里执行，可以放心阻塞。
 *   3. 本例故意让 tasklet 风格版本尝试"阻塞"，模拟的 slow_blocking_io() 会
 *      检测到自己被在不允许睡眠的上下文调用，直接报错退出（不是真的让程序崩溃，
 *      而是返回错误码，模拟真实内核 CONFIG_DEBUG_ATOMIC_SLEEP 下的 WARN/BUG 效果）。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o tasklet_vs_workqueue_compare tasklet_vs_workqueue_compare.c
 * 运行：
 *   ./tasklet_vs_workqueue_compare
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* 1. 模拟"当前执行上下文"——对应真实内核的 in_softirq()/in_atomic()      */
/* ------------------------------------------------------------------ */

static __thread int g_softirq_depth;   /* 每个线程独立，对应 per-cpu softirq 计数 */

static inline bool in_softirq(void)
{
	return g_softirq_depth > 0;
}

static inline void local_bh_disable(void)
{
	g_softirq_depth++;
}

static inline void local_bh_enable(void)
{
	g_softirq_depth--;
}

/*
 * slow_blocking_io()：模拟一次"必须睡眠等待"的操作（比如等磁盘 I/O 完成）。
 * 真实内核里这类调用（mutex_lock、wait_event、kmalloc(GFP_KERNEL)...）
 * 在软中断/tasklet 上下文里调用是明确禁止的——CONFIG_DEBUG_ATOMIC_SLEEP
 * 会在 __might_sleep() 里检测到并打印 "BUG: sleeping function called
 * from invalid context" 然后 dump_stack()。本函数用返回错误值模拟这个检测。
 */
static int slow_blocking_io(const char *label, int simulated_ms)
{
	if (in_softirq()) {
		fprintf(stderr,
			"  [BUG] '%s' 试图在软中断/tasklet 上下文里阻塞 %dms —— "
			"等价于真实内核 CONFIG_DEBUG_ATOMIC_SLEEP 检测到的 "
			"\"scheduling while atomic\"，本次操作被拒绝执行\n",
			label, simulated_ms);
		return -1;
	}
	/* 真正睡眠：只有 process context（这里用独立线程模拟）才能安全调用 */
	usleep(simulated_ms * 1000);
	return 0;
}

/* ------------------------------------------------------------------ */
/* 2. 共享的业务数据                                                    */
/* ------------------------------------------------------------------ */

struct log_flush_job {
	char device[32];
	int  dirty_pages;
	int  result;          /* 0=成功, -1=失败(在软中断上下文里试图阻塞) */
};

/* ------------------------------------------------------------------ */
/* 3. (a) tasklet 风格实现：假装自己在软中断上下文里被 tasklet_action 调用 */
/* ------------------------------------------------------------------ */

static void tasklet_style_flush(struct log_flush_job *job)
{
	/* 模拟 kernel/softirq.c:444 tasklet_action() 调用 t->func() 前的环境：
	 * 软中断上下文，不允许阻塞 —— 对应真实 tasklet 执行时机 */
	local_bh_disable();

	printf("  [tasklet] 开始处理 device='%s' dirty_pages=%d（假设身处软中断上下文）\n",
	       job->device, job->dirty_pages);

	/* 正确的 tasklet 写法：只做"不会阻塞"的快速工作，比如更新计数、
	 * 排一个真正的 workqueue 任务去做慢的部分（这正是 07 篇 workqueue 存在的原因） */
	job->dirty_pages = 0;

	/* 故意演示"错误的 tasklet 写法"：直接调用一个会阻塞的操作 */
	job->result = slow_blocking_io(job->device, 20);

	local_bh_enable();
}

/* ------------------------------------------------------------------ */
/* 4. (b) workqueue 风格实现：真正的独立线程，process context，可以阻塞  */
/* ------------------------------------------------------------------ */

struct wq_worker_ctx {
	struct log_flush_job *job;
};

static void *workqueue_style_worker(void *arg)
{
	struct wq_worker_ctx *ctx = arg;
	struct log_flush_job *job = ctx->job;

	/* g_softirq_depth 在这个新线程里从 0 开始，天然就是 process context，
	 * 对应真实 kernel worker_thread() 跑在自己的内核线程里，可以睡眠 */
	printf("  [workqueue] 开始处理 device='%s' dirty_pages=%d（独立线程，process context）\n",
	       job->device, job->dirty_pages);

	job->dirty_pages = 0;
	job->result = slow_blocking_io(job->device, 20);

	return NULL;
}

/* ------------------------------------------------------------------ */
/* 5. 计时 + 对比驱动                                                   */
/* ------------------------------------------------------------------ */

static double elapsed_ms(struct timespec *start, struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) * 1000.0 +
	       (end->tv_nsec - start->tv_nsec) / 1e6;
}

int main(void)
{
	struct timespec t0, t1;

	printf("=== tasklet_vs_workqueue_compare ===\n\n");

	/* ---- (a) tasklet 风格：在"主线程"里直接模拟软中断上下文调用 ---- */
	printf("-- (a) tasklet 风格：直接在软中断上下文里调用 --\n");
	struct log_flush_job job_a = { .device = "sda1", .dirty_pages = 128, .result = 0 };
	clock_gettime(CLOCK_MONOTONIC, &t0);
	tasklet_style_flush(&job_a);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("  结果: result=%d (0=成功 -1=被拒绝), dirty_pages剩余=%d, 耗时=%.3fms\n\n",
	       job_a.result, job_a.dirty_pages, elapsed_ms(&t0, &t1));

	/* ---- (b) workqueue 风格：独立线程，process context，允许阻塞 ---- */
	printf("-- (b) workqueue 风格：独立 worker 线程里调用 --\n");
	struct log_flush_job job_b = { .device = "sdb1", .dirty_pages = 128, .result = 0 };
	struct wq_worker_ctx ctx = { .job = &job_b };
	pthread_t worker;

	clock_gettime(CLOCK_MONOTONIC, &t0);
	pthread_create(&worker, NULL, workqueue_style_worker, &ctx);
	pthread_join(worker, NULL);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("  结果: result=%d (0=成功 -1=被拒绝), dirty_pages剩余=%d, 耗时=%.3fms\n\n",
	       job_b.result, job_b.dirty_pages, elapsed_ms(&t0, &t1));

	/* ---- 总结 ---- */
	printf("=== 对比总结 ===\n");
	printf("  tasklet   风格: %s（软中断上下文里的阻塞操作被设计上禁止）\n",
	       job_a.result == 0 ? "成功" : "失败——正如预期，验证了约束生效");
	printf("  workqueue 风格: %s（process context 下可以安全阻塞）\n",
	       job_b.result == 0 ? "成功" : "失败——不符合预期");

	if (job_a.result == 0 || job_b.result != 0) {
		fprintf(stderr, "!!! 断言失败：约束没有按预期生效 !!!\n");
		return 1;
	}
	printf("\n断言通过：tasklet 上下文正确地拒绝了阻塞操作，"
	       "workqueue 上下文正确地允许了阻塞操作。\n");
	return 0;
}
