/*
 * io_scheduler_policy.c
 *
 * 场景：模拟 Linux Block 层的 I/O 调度器（elevator）可插拔策略机制。
 * 真实定义：
 *   - struct elevator_ops   见 include/linux/elevator.h:35
 *   - struct elevator_type  见 include/linux/elevator.h:80
 *   - elv_register()/elv_unregister()  见 block/elevator.c:895,914
 *   - elevator_change()/elv_iosched_store()（对应 /sys/block/<dev>/queue/scheduler
 *     的写入路径）见 block/elevator.c:1014,1038
 *   - chosen_elevator[]/__setup("elevator=", ...)（对应内核启动参数选择默认调度器）
 *     见 block/elevator.c:184-196
 *
 * 这里实现两种简化策略：
 *   - noop：不重排序，严格按到达（提交）顺序派发（真实 noop 调度器的核心特征）。
 *   - deadline：优先按扇区排序派发（类似电梯扫描），但一旦某个请求等待超过
 *     fifo_expire（本例设为 3 个"时间片"）就强制插队派发，防止饿死——
 *     这是真实 deadline 调度器"排序 + 防饿死"两条腿走路的核心思想的简化版。
 *
 * 关键点：block 层核心代码（本例中的 blk_run_queue()）只认识
 * struct io_sched_ops 这个契约（add_request/dispatch_request 两个函数指针），
 * 完全不知道 noop/deadline 内部怎么决定派发顺序——这正是 Policy Injection：
 * "何时派发"（机制）与"派发谁"（策略）彻底分离。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o io_scheduler_policy io_scheduler_policy.c
 * 运行：./io_scheduler_policy
 */

#include <stdio.h>
#include <string.h>

#define MAX_REQUESTS	32
#define MAX_ELEVATORS	4
#define FIFO_EXPIRE	3	/* 模拟的"到期时间片数"，对应 deadline 调度器的 fifo_expire */

/* 一次 I/O 请求，类似 struct request（block/blk-core.c）的极简版 */
struct io_request {
	int  id;
	long sector;
	int  arrival_seq;	/* 到达（提交）顺序，越小越先到 */
	int  dispatched;
};

/* ------------------------------------------------------------------ */
/* 契约：核心（block 层）只认识这份 ops，不知道具体是谁实现的           */
/* 对应真实的 struct elevator_ops（include/linux/elevator.h:35）        */
/* ------------------------------------------------------------------ */
struct io_sched_ops {
	const char *name;
	void  (*init)(void);
	void  (*add_request)(struct io_request *rq);
	struct io_request *(*dispatch_request)(void);	/* 返回 NULL 表示队列已空 */
};

/* ==================================================================== */
/* 策略 A：noop —— 严格 FIFO，不做任何重排序                              */
/* ==================================================================== */

static struct io_request *noop_queue[MAX_REQUESTS];
static int noop_head, noop_tail;

static void noop_init(void)
{
	noop_head = noop_tail = 0;
	printf("  [noop] queue initialized\n");
}

static void noop_add_request(struct io_request *rq)
{
	noop_queue[noop_tail++] = rq;
}

static struct io_request *noop_dispatch_request(void)
{
	if (noop_head == noop_tail)
		return NULL;
	return noop_queue[noop_head++];
}

static struct io_sched_ops noop_ops = {
	.name             = "noop",
	.init             = noop_init,
	.add_request      = noop_add_request,
	.dispatch_request = noop_dispatch_request,
};

/* ==================================================================== */
/* 策略 B：deadline —— 优先按扇区排序（sweep），到期请求强制插队           */
/* ==================================================================== */

static struct io_request *deadline_pending[MAX_REQUESTS];
static int deadline_count;
static int deadline_tick;	/* 模拟时间片，每次 dispatch 前进一格 */

static void deadline_init(void)
{
	deadline_count = 0;
	deadline_tick = 0;
	printf("  [deadline] pending list initialized, fifo_expire=%d ticks\n", FIFO_EXPIRE);
}

static void deadline_add_request(struct io_request *rq)
{
	deadline_pending[deadline_count++] = rq;
}

static struct io_request *deadline_dispatch_request(void)
{
	int i, oldest_idx = -1, min_sector_idx = -1;

	if (deadline_count == 0)
		return NULL;

	/* 找到"到达顺序最早"的那个未派发请求（用于饿死检测） */
	for (i = 0; i < deadline_count; i++) {
		if (deadline_pending[i]->dispatched)
			continue;
		if (oldest_idx < 0 ||
		    deadline_pending[i]->arrival_seq < deadline_pending[oldest_idx]->arrival_seq)
			oldest_idx = i;
	}
	if (oldest_idx < 0)
		return NULL;

	/* 到期检测：等待时间 >= FIFO_EXPIRE，强制插队派发，防止饿死 */
	if (deadline_tick - deadline_pending[oldest_idx]->arrival_seq >= FIFO_EXPIRE) {
		deadline_pending[oldest_idx]->dispatched = 1;
		deadline_tick++;
		printf("    [deadline] request expired, force dispatch to avoid starvation\n");
		return deadline_pending[oldest_idx];
	}

	/* 否则按扇区从小到大排序派发（sweep，模拟磁头单向移动，减少寻道） */
	for (i = 0; i < deadline_count; i++) {
		if (deadline_pending[i]->dispatched)
			continue;
		if (min_sector_idx < 0 ||
		    deadline_pending[i]->sector < deadline_pending[min_sector_idx]->sector)
			min_sector_idx = i;
	}
	deadline_pending[min_sector_idx]->dispatched = 1;
	deadline_tick++;
	return deadline_pending[min_sector_idx];
}

static struct io_sched_ops deadline_ops = {
	.name             = "deadline",
	.init             = deadline_init,
	.add_request      = deadline_add_request,
	.dispatch_request = deadline_dispatch_request,
};

/* ==================================================================== */
/* Core：block 层的极简复刻，只认识 io_sched_ops 契约                     */
/* 对应真实的 elv_register()/elevator_change()                          */
/* ==================================================================== */

static struct io_sched_ops *g_registry[MAX_ELEVATORS];
static int g_registry_count;
static struct io_sched_ops *g_active;	/* 对应真实 request_queue->elevator */

static void elv_register(struct io_sched_ops *e)
{
	g_registry[g_registry_count++] = e;
	printf("[elv_register] io scheduler '%s' registered\n", e->name);
}

/* 对应真实的 elevator_change()：运行期把当前生效策略换成另一个 */
static int elv_switch(const char *name)
{
	int i;

	for (i = 0; i < g_registry_count; i++) {
		if (strcmp(g_registry[i]->name, name) == 0) {
			printf("[elv_switch] switching active io scheduler -> '%s'\n", name);
			g_active = g_registry[i];
			g_active->init();
			return 0;
		}
	}
	fprintf(stderr, "[elv_switch] unknown io scheduler '%s'\n", name);
	return -1;
}

/* 对应真实 block 层核心提交路径（__elv_add_request()），
 * 核心代码只调用 g_active->add_request()，完全不知道具体策略实现 */
static void blk_submit_bio(struct io_request *rq)
{
	g_active->add_request(rq);
}

/* 对应真实 blk_run_queue() -> elevator 的 dispatch_fn，反复取出请求下发给设备 */
static void blk_run_queue(void)
{
	struct io_request *rq;

	printf("  -- blk_run_queue() using active scheduler '%s' --\n", g_active->name);
	while ((rq = g_active->dispatch_request()) != NULL)
		printf("    dispatch: request id=%d sector=%-4ld (arrival_seq=%d)\n",
		       rq->id, rq->sector, rq->arrival_seq);
}

int main(void)
{
	struct io_request pool[6] = {
		{ .id = 1, .sector = 500, .arrival_seq = 0, .dispatched = 0 },
		{ .id = 2, .sector = 100, .arrival_seq = 1, .dispatched = 0 },
		{ .id = 3, .sector = 300, .arrival_seq = 2, .dispatched = 0 },
		{ .id = 4, .sector = 700, .arrival_seq = 3, .dispatched = 0 },
		{ .id = 5, .sector = 200, .arrival_seq = 4, .dispatched = 0 },
		{ .id = 6, .sector = 900, .arrival_seq = 5, .dispatched = 0 },
	};
	int i;

	printf("=== io_scheduler_policy demo ===\n\n");

	elv_register(&noop_ops);
	elv_register(&deadline_ops);

	printf("\n-- 阶段1：切换到 noop，提交 6 个乱序 sector 的请求 --\n");
	elv_switch("noop");
	for (i = 0; i < 6; i++)
		blk_submit_bio(&pool[i]);
	blk_run_queue();

	/* 重置 dispatched 标记，模拟同一批请求重新提交给另一种策略 */
	for (i = 0; i < 6; i++)
		pool[i].dispatched = 0;

	printf("\n-- 阶段2：切换到 deadline，提交同样 6 个请求 --\n");
	elv_switch("deadline");
	for (i = 0; i < 6; i++)
		blk_submit_bio(&pool[i]);
	blk_run_queue();

	printf("\n=== done ===\n");
	printf("说明：noop 严格按提交顺序（500,100,300,700,200,900）派发，\n"
	       "      deadline 前几次按 sector 升序派发（sweep），\n"
	       "      后几个请求等待超过 fifo_expire 后被强制插队派发防止饿死。\n"
	       "      blk_submit_bio()/blk_run_queue() 全程没有一行 if (policy==noop)，\n"
	       "      完全通过 g_active->ops 盲调，这就是 Policy Injection。\n");
	return 0;
}
