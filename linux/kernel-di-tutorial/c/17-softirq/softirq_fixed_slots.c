/*
 * softirq_fixed_slots.c
 *
 * 场景：模拟真实内核 kernel/softirq.c 里"固定编号、编译期确定数量"的
 * softirq 注册与触发机制：
 *
 *   - softirq 的"槎位"数量在编译时就写死（对应真实内核的
 *     enum { HI_SOFTIRQ, TIMER_SOFTIRQ, NET_TX_SOFTIRQ, ..., NR_SOFTIRQS }，
 *     见 include/linux/interrupt.h:425-438），本示例用一个同构的 enum
 *     模拟固定编号。
 *   - open_softirq(nr, action) 只是"往一个固定大小的数组里填函数指针”，
 *     不存在动态注册/注销——这与 05 篇 notifier chain（动态链表，
 *     任意数量）、07 篇 workqueue（动态提交任意多任务）形成鲜明对比。
 *   - raise_softirq(nr) 只是把对应 bit 位在 pending 位图里置 1，
 *     真正执行发生在稍后统一的 do_softirq() 遍历里。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -std=gnu11 -o softirq_fixed_slots softirq_fixed_slots.c
 * 运行：
 *   ./softirq_fixed_slots
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* 1. 契约：固定编号的 softirq 类型 —— 编译期确定，不可运行期扩展         */
/*    对应真实内核 include/linux/interrupt.h:425-438                    */
/* ------------------------------------------------------------------ */

enum {
	SIM_HI_SOFTIRQ = 0,
	SIM_TIMER_SOFTIRQ,
	SIM_NET_TX_SOFTIRQ,
	SIM_NET_RX_SOFTIRQ,
	SIM_TASKLET_SOFTIRQ,
	SIM_NR_SOFTIRQS,   /* 固定上限——数组大小编译期确定，不能运行期 resize */
};

static const char *sim_softirq_to_name[SIM_NR_SOFTIRQS] = {
	"HI", "TIMER", "NET_TX", "NET_RX", "TASKLET",
};

/* 对应真实内核 struct softirq_action { void (*action)(struct softirq_action *); }
 * include/linux/interrupt.h:450-453 */
struct softirq_action_sim {
	void (*action)(struct softirq_action_sim *);
};

/* 对应真实内核 kernel/softirq.c:55
 * static struct softirq_action softirq_vec[NR_SOFTIRQS] __cacheline_aligned_in_smp; */
static struct softirq_action_sim softirq_vec[SIM_NR_SOFTIRQS];

/* 对应真实内核 kernel/softirq.c:50-53 的每 CPU pending 位图；
 * 这个示例先只模拟单 CPU，多 CPU 版本见 percpu_softirq_pending.c */
static uint32_t softirq_pending;

/* ------------------------------------------------------------------ */
/* 2. Core：open_softirq / raise_softirq —— 极简，只是数组填值 + 置位     */
/* ------------------------------------------------------------------ */

/* 对应真实内核 kernel/softirq.c:388-391
 * void open_softirq(int nr, void (*action)(struct softirq_action *))
 * { softirq_vec[nr].action = action; } */
static void open_softirq(int nr, void (*action)(struct softirq_action_sim *))
{
	softirq_vec[nr].action = action;
	printf("[open_softirq] slot %d (%s) <- %p\n", nr, sim_softirq_to_name[nr], (void *)(uintptr_t)action);
}

/* 对应真实内核 kernel/softirq.c:362-366 __raise_softirq_irqoff() /
 * or_softirq_pending(1UL << nr) */
static void raise_softirq(unsigned int nr)
{
	softirq_pending |= (1u << nr);
	printf("[raise_softirq] marked pending: %s (bit %u)\n", sim_softirq_to_name[nr], nr);
}

/*
 * do_softirq_sim() —— 对应真实内核 kernel/softirq.c __do_softirq()
 * (kernel/softirq.c:207-267)：清空 pending 位图快照，按位遍历固定大小的
 * softirq_vec[] 数组，逐个盲调已注册的 action。注意这里遍历的是"数组下标”，
 * 不是链表——这正是"固定编号"与"动态链表"（notifier chain）最本质的区别。
 */
static void do_softirq_sim(void)
{
	uint32_t pending = softirq_pending;

	softirq_pending = 0; /* 对应 set_softirq_pending(0)，在真实内核里发生在开中断之前 */

	printf("[do_softirq] pending bitmap = 0x%02x\n", pending);

	for (int i = 0; i < SIM_NR_SOFTIRQS; i++) {
		if (pending & (1u << i)) {
			if (softirq_vec[i].action) {
				printf("  [do_softirq] running slot %d (%s)\n", i, sim_softirq_to_name[i]);
				softirq_vec[i].action(&softirq_vec[i]);   /* <-- 盲调 */
			} else {
				printf("  [do_softirq] slot %d (%s) pending but no action registered!\n",
				       i, sim_softirq_to_name[i]);
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/* 3. 几个"子系统"各自注册自己的 softirq action                          */
/* ------------------------------------------------------------------ */

static int g_net_tx_processed;
static int g_net_rx_processed;
static int g_timer_processed;

static void net_tx_action(struct softirq_action_sim *h)
{
	(void)h;
	g_net_tx_processed++;
	printf("    [net_tx_action] flushed outgoing packet queue (run #%d)\n", g_net_tx_processed);
}

static void net_rx_action(struct softirq_action_sim *h)
{
	(void)h;
	g_net_rx_processed++;
	printf("    [net_rx_action] drained incoming packet ring (run #%d)\n", g_net_rx_processed);
}

static void timer_action(struct softirq_action_sim *h)
{
	(void)h;
	g_timer_processed++;
	printf("    [timer_action] ran expired timer list (run #%d)\n", g_timer_processed);
}

int main(void)
{
	printf("=== softirq_fixed_slots demo ===\n\n");

	printf("-- 注册阶段（编译期就已经决定有 %d 个固定槎位） --\n", SIM_NR_SOFTIRQS);
	open_softirq(SIM_NET_TX_SOFTIRQ, net_tx_action);
	open_softirq(SIM_NET_RX_SOFTIRQ, net_rx_action);
	open_softirq(SIM_TIMER_SOFTIRQ, timer_action);
	/* 注意：SIM_HI_SOFTIRQ 和 SIM_TASKLET_SOFTIRQ 这两个槎位故意留空未注册，
	 * 模拟"槎位存在但当前系统配置下没有子系统使用它"的真实情况 */

	printf("\n-- 触发阶段：模拟硬件事件/软件路径 raise_softirq() --\n");
	raise_softirq(SIM_NET_RX_SOFTIRQ);
	raise_softirq(SIM_TIMER_SOFTIRQ);

	printf("\n-- 统一执行：do_softirq() 一次性处理所有 pending 位 --\n");
	do_softirq_sim();

	printf("\n-- 再次触发：这次三个都触发，包括一个从未注册 action 的槎位 --\n");
	raise_softirq(SIM_NET_TX_SOFTIRQ);
	raise_softirq(SIM_NET_RX_SOFTIRQ);
	raise_softirq(SIM_HI_SOFTIRQ);   /* 这个槎位没有注册 action */

	printf("\n-- 统一执行 --\n");
	do_softirq_sim();

	printf("\n-- 验证：没有 pending 时 do_softirq() 应该什么都不做 --\n");
	do_softirq_sim();

	printf("\n=== done: net_tx=%d, net_rx=%d, timer=%d ===\n",
	       g_net_tx_processed, g_net_rx_processed, g_timer_processed);
	return 0;
}
