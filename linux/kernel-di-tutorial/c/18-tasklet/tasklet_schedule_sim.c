/*
 * tasklet_schedule_sim.c
 *
 * 场景：模拟 kernel/softirq.c 里 tasklet 的核心调度机制——
 *   tasklet_schedule() 只是"把 tasklet 挂到 per-cpu 的 pending 链表尾部，
 *   然后 raise_softirq(TASKLET_SOFTIRQ)"，真正执行发生在软中断上下文的
 *   tasklet_action() 里，从链表头部逐个取出并调用 t->func(t->data)。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   include/linux/interrupt.h:515  struct tasklet_struct
 *   include/linux/interrupt.h:561  tasklet_schedule()
 *   kernel/softirq.c:397           struct tasklet_head + per-cpu tasklet_vec
 *   kernel/softirq.c:405           __tasklet_schedule()
 *   kernel/softirq.c:444           tasklet_action()
 *
 * 本文件用单线程模拟"中断上下文调度 + 软中断上下文执行"两阶段，重点展示：
 *   1. 同一个 tasklet 多次 schedule，只要还未执行完，只会被排队一次
 *      （对应 TASKLET_STATE_SCHED 位的 test_and_set_bit 语义，"去重"特性）。
 *   2. pending 链表严格 FIFO，与真实 tasklet_vec 完全一致。
 *   3. disable（count>0）时被取出的 tasklet 会被重新排回队列，而不是丢弃。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o tasklet_schedule_sim tasklet_schedule_sim.c
 * 运行：./tasklet_schedule_sim
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* 1. 契约：mini_tasklet，对应 include/linux/interrupt.h:515 struct tasklet_struct */
/* ------------------------------------------------------------------ */

enum {
	TASKLET_BIT_SCHED = 0,   /* 对应真实 TASKLET_STATE_SCHED */
	TASKLET_BIT_RUN   = 1,   /* 对应真实 TASKLET_STATE_RUN（SMP only）*/
};

struct mini_tasklet {
	struct mini_tasklet *next;    /* 侵入式单链表指针，对应真实 next 字段 */
	unsigned long state;          /* bit0=SCHED, bit1=RUN */
	int count;                    /* >0 表示被 disable，对应真实 atomic_t count */
	void (*func)(unsigned long);  /* 回调，对应真实 func 字段（Function Pointer Callback，01 篇）*/
	unsigned long data;           /* 上下文，对应真实 data 字段（Context Injection，03 篇的"手段A"）*/
	const char *name;             /* 演示用，方便打印，真实 tasklet_struct 没有这个字段 */
};

#define MINI_DECLARE_TASKLET(name_, fn_, data_) \
	struct mini_tasklet name_ = { NULL, 0, 0, fn_, data_, #name_ }

static inline bool test_and_set_bit(int bit, unsigned long *addr)
{
	unsigned long mask = 1UL << bit;
	unsigned long old = *addr;
	*addr |= mask;
	return (old & mask) != 0;
}

static inline void clear_bit(int bit, unsigned long *addr)
{
	*addr &= ~(1UL << bit);
}

/* ------------------------------------------------------------------ */
/* 2. Core：per-cpu pending 链表（这里简化为单个全局链表），对应
 *    kernel/softirq.c:397 struct tasklet_head + DEFINE_PER_CPU(tasklet_vec)  */
/* ------------------------------------------------------------------ */

struct tasklet_head {
	struct mini_tasklet *head;
	struct mini_tasklet **tail;
};

static struct tasklet_head g_tasklet_vec;
static long g_softirq_raised;   /* 模拟 raise_softirq(TASKLET_SOFTIRQ) 被调用次数 */

static void tasklet_vec_init(struct tasklet_head *tv)
{
	tv->head = NULL;
	tv->tail = &tv->head;
}

/* 对应 kernel/softirq.c:405 __tasklet_schedule() + include/linux/interrupt.h:561 tasklet_schedule() */
static void mini_tasklet_schedule(struct mini_tasklet *t)
{
	/* 真实内核：tasklet_schedule() 先 test_and_set_bit(TASKLET_STATE_SCHED)，
	 * 只有第一次成功置位的调用才会真正进入 __tasklet_schedule() 入队；
	 * 后续在"还没被执行"期间重复调用会被这里挡掉——tasklet 天生"去重"，
	 * 这正是 kernel/softirq.c:504 注释里 "If the tasklet is already scheduled,
	 * but its execution is still not started, it will be executed only once" 这条性质。 */
	if (test_and_set_bit(TASKLET_BIT_SCHED, &t->state)) {
		printf("  [tasklet_schedule] '%s' 已经在 pending 链表里，本次调度被合并（去重）\n",
		       t->name);
		return;
	}

	t->next = NULL;
	*g_tasklet_vec.tail = t;
	g_tasklet_vec.tail = &t->next;
	g_softirq_raised++;
	printf("  [tasklet_schedule] '%s' 入队 pending 链表，raise_softirq(TASKLET_SOFTIRQ)\n",
	       t->name);
}

/* 对应 kernel/softirq.c:444 tasklet_action() */
static void mini_tasklet_action(void)
{
	struct mini_tasklet *list;

	/* 真实实现在 local_irq_disable() 下"摘走整条链表"，单线程模拟不需要额外加锁 */
	list = g_tasklet_vec.head;
	g_tasklet_vec.head = NULL;
	g_tasklet_vec.tail = &g_tasklet_vec.head;

	while (list) {
		struct mini_tasklet *t = list;
		list = list->next;

		/* 对应真实 tasklet_trylock()：SMP 下用 test_and_set_bit(TASKLET_STATE_RUN)
		 * 保证同一个 tasklet 不会被多个 CPU 并发执行——本文件单线程模拟里这里恒真，
		 * 真正的并发场景见 tasklet_disable_enable_demo.c */
		if (test_and_set_bit(TASKLET_BIT_RUN, &t->state)) {
			printf("  [tasklet_action] '%s' 正在别处运行，重新入队\n", t->name);
			t->next = NULL;
			*g_tasklet_vec.tail = t;
			g_tasklet_vec.tail = &t->next;
			continue;
		}

		if (t->count > 0) {
			/* 对应真实 !atomic_read(&t->count) 判断失败：已被 disable，
			 * 重新排回队列，等 enable 之后的下一轮 tasklet_action 处理，
			 * 注意 SCHED 位这里没有清掉——调度请求被"记住"而不是丢弃 */
			printf("  [tasklet_action] '%s' 处于 disable 状态（count=%d），推迟执行\n",
			       t->name, t->count);
			clear_bit(TASKLET_BIT_RUN, &t->state);
			t->next = NULL;
			*g_tasklet_vec.tail = t;
			g_tasklet_vec.tail = &t->next;
			continue;
		}

		/* 清 SCHED 位，允许下一次 tasklet_schedule() 重新入队 */
		clear_bit(TASKLET_BIT_SCHED, &t->state);
		printf("  [tasklet_action] 执行 '%s'->func(data=%lu)\n", t->name, t->data);
		t->func(t->data);
		clear_bit(TASKLET_BIT_RUN, &t->state);
	}
}

/* ------------------------------------------------------------------ */
/* 3. 业务回调：两个不同"驱动"的 tasklet 函数，风格对照
 *    drivers/mmc/host/wbsd.c:1462 tasklet_init(&host->card_tasklet, ...)  */
/* ------------------------------------------------------------------ */

static void net_rx_tasklet_fn(unsigned long data)
{
	printf("    [net_rx] 处理网卡 #%lu 的收包 tasklet（模拟 drivers/net/usb/usbnet.c 的 dev->bh）\n",
	       data);
}

static void mmc_finish_tasklet_fn(unsigned long data)
{
	printf("    [mmc_finish] 处理 MMC 主机 #%lu 的命令完成 tasklet（模拟 wbsd.c finish_tasklet）\n",
	       data);
}

int main(void)
{
	MINI_DECLARE_TASKLET(net_rx_t, net_rx_tasklet_fn, 0);
	MINI_DECLARE_TASKLET(mmc_finish_t, mmc_finish_tasklet_fn, 1);

	tasklet_vec_init(&g_tasklet_vec);

	printf("=== tasklet_schedule_sim ===\n\n");

	printf("-- 阶段1：模拟中断处理函数（ISR）里调用 tasklet_schedule --\n");
	mini_tasklet_schedule(&net_rx_t);
	mini_tasklet_schedule(&mmc_finish_t);
	/* 关键点：同一个 tasklet 在还没执行完之前再次 schedule，会被去重 */
	mini_tasklet_schedule(&net_rx_t);

	printf("\n-- 阶段2：模拟软中断上下文执行 tasklet_action（遍历 pending 链表）--\n");
	mini_tasklet_action();

	printf("\n-- 阶段3：net_rx tasklet 再次被调度（模拟第二次网卡中断）--\n");
	mini_tasklet_schedule(&net_rx_t);
	mini_tasklet_action();

	printf("\n-- 阶段4：验证 SCHED 位已清除，两个 tasklet 都处于空闲状态 --\n");
	printf("  net_rx_t.state=%lu mmc_finish_t.state=%lu (0 表示空闲，可再次调度)\n",
	       net_rx_t.state, mmc_finish_t.state);

	printf("\n=== done, softirq raised %ld time(s) total ===\n", g_softirq_raised);
	return 0;
}
