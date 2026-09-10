/*
 * softirq_vs_hardirq_demo.c
 *
 * 场景：模拟真实内核"硬中断触发 -> 只标记 pending，立即返回 -> 稍后统一执行
 * softirq"的两级处理流程，展示为什么要拆成两级：硬中断上下文必须尽快返回
 * （对应 16 篇 IRQ Handler 的硬约束），真正的批量处理工作被推迟到一个更宽松
 * 的时间点（`irq_exit()` 末尾调用 `do_softirq()`，对应真实内核
 * kernel/softirq.c 的 irq_exit() 附近逻辑）一次性做完。
 *
 * 与 16 篇 threaded_irq_demo.c 的"两段式"表面相似，但机制完全不同：
 *   threaded_irq_demo:  唤醒一个*专属的内核线程*，异步、独立调度。
 *   本示例（softirq）:  不创建线程，softirq 仍然运行在"中断返回路径"上，
 *                       只是被推迟到本次硬中断处理完毕、开中断之后，
 *                       且仍然禁止睡眠——这是"轻量级的推迟"，不是"异步化"。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -std=gnu11 -o softirq_vs_hardirq_demo softirq_vs_hardirq_demo.c
 * 运行：
 *   ./softirq_vs_hardirq_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

enum { SIM_NET_RX_SOFTIRQ = 0, SIM_NR_SOFTIRQS };

static const char *name_of(int nr) { return nr == SIM_NET_RX_SOFTIRQ ? "NET_RX" : "?"; }

struct softirq_action_sim { void (*action)(void); };
static struct softirq_action_sim softirq_vec[SIM_NR_SOFTIRQS];
static uint32_t softirq_pending;

static void open_softirq(int nr, void (*action)(void)) { softirq_vec[nr].action = action; }
static void raise_softirq(unsigned int nr) { softirq_pending |= (1u << nr); }

static long elapsed_us(struct timespec *t0, struct timespec *t1)
{
	return (t1->tv_sec - t0->tv_sec) * 1000000L + (t1->tv_nsec - t0->tv_nsec) / 1000L;
}

/* ------------------------------------------------------------------ */
/* 1. "重活"：批量处理网卡收包环——如果直接放在硬中断里做，会拖慢中断响应   */
/* ------------------------------------------------------------------ */

#define RING_SIZE 8

static int rx_ring[RING_SIZE];
static int rx_ring_count;
static int total_packets_drained;
static int net_rx_softirq_runs;

static void net_rx_softirq_action(void)
{
	int n = rx_ring_count;

	net_rx_softirq_runs++;
	/* 模拟"批量处理环里累积的所有包"——这正是推迟到 softirq 的价值：
	 * 一次 softirq 执行可以把多次硬中断攒下来的工作一次性处理掉 */
	for (int i = 0; i < n; i++) {
		total_packets_drained++;
		(void)rx_ring[i];
	}
	rx_ring_count = 0;
	printf("    [net_rx_softirq_action] drained %d packet(s) from ring in one pass "
	       "(softirq run #%d)\n", n, net_rx_softirq_runs);
}

/* ------------------------------------------------------------------ */
/* 2. 硬中断处理函数：只做最少的事，把包塞进环，标记 pending，立即返回     */
/* ------------------------------------------------------------------ */

static int hardirq_total_fires;

static void nic_hardirq_handler(int packet_id)
{
	struct timespec t0, t1;

	clock_gettime(CLOCK_MONOTONIC, &t0);

	hardirq_total_fires++;
	/* 硬中断上下文允许做的事：极少量、O(1) 的工作 */
	if (rx_ring_count < RING_SIZE)
		rx_ring[rx_ring_count++] = packet_id;
	raise_softirq(SIM_NET_RX_SOFTIRQ);   /* 标记"有活要做"，但不在这里做 */

	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("  [hardirq] packet #%d arrived, enqueued + raised softirq, "
	       "hardirq handler took %ld us\n", packet_id, elapsed_us(&t0, &t1));
}

/*
 * irq_exit_sim() —— 对应真实内核 kernel/softirq.c irq_exit() 附近的调用点：
 * 每次硬中断处理返回之前，检查是否有 pending 的 softirq，有就统一执行一次。
 */
static void irq_exit_sim(void)
{
	if (softirq_pending) {
		uint32_t pending = softirq_pending;
		softirq_pending = 0;
		for (int i = 0; i < SIM_NR_SOFTIRQS; i++) {
			if ((pending & (1u << i)) && softirq_vec[i].action) {
				printf("  [irq_exit] running pending softirq: %s\n", name_of(i));
				softirq_vec[i].action();
			}
		}
	}
}

int main(void)
{
	printf("=== softirq_vs_hardirq_demo ===\n\n");

	open_softirq(SIM_NET_RX_SOFTIRQ, net_rx_softirq_action);

	printf("-- 场景 A：每次硬中断后立即触发一次 irq_exit()（模拟中断稀疏到达） --\n");
	for (int i = 1; i <= 3; i++) {
		nic_hardirq_handler(i);
		irq_exit_sim();
	}

	printf("\n-- 场景 B：连续 5 次硬中断密集到达（模拟突发流量），"
	       "只在最后统一调用一次 irq_exit() --\n");
	for (int i = 4; i <= 8; i++)
		nic_hardirq_handler(i);
	printf("  (5 次硬中断已全部返回，ring 里累积了 %d 个包，pending 位仍然是 1)\n", rx_ring_count);
	irq_exit_sim();

	printf("\n-- 对比：如果把批量处理直接搬进硬中断处理函数里会怎样（反面教材，"
	       "仅打印耗时对比，不实际改变处理路径） --\n");
	printf("  每次硬中断真实耗时应保持微秒级（如上所示），"
	       "\n  而 net_rx_softirq_action 处理 %d 个包的批量耗时是被推迟、"
	       "允许更久的（对应 do_softirq，仍在中断返回路径上，但不逐包中断响应）\n",
	       rx_ring_count > 0 ? rx_ring_count : 0);

	printf("\n=== done: hardirq_total_fires=%d, total_packets_drained=%d, "
	       "net_rx_softirq_runs=%d ===\n",
	       hardirq_total_fires, total_packets_drained, net_rx_softirq_runs);
	return 0;
}
