/*
 * threaded_irq_demo.c
 *
 * 场景：模拟 request_threaded_irq() 的两段式处理：
 *
 *   primary handler   —— 模拟"硬中断上下文”：必须极快返回，不能睡眠，
 *                         只做最基本的判断（是不是我的中断）和"唤醒"，
 *                         对应真实内核 kernel/irq/handle.c
 *                         handle_irq_event_percpu() 里 action->handler()
 *                         返回 IRQ_WAKE_THREAD 的那一刻。
 *
 *   threaded handler  —— 模拟"进程上下文”：运行在一个专门为这个 irqaction
 *                         创建的内核线程里（真实内核 kernel/irq/manage.c
 *                         __setup_irq() 里的 kthread_create(irq_thread, ...)），
 *                         可以做重活、可以睡眠，对应 irq_thread_fn()。
 *
 * 关键教学点：primary handler 的响应时间和 threaded handler 的处理时间是
 * *完全解耦*的——硬件线可能比线程处理速度更快地连续触发多次，这时后面几次
 * "唤醒"会被合并（coalesce）成同一次线程处理，这与真实硬件"电平触发中断在
 * 服务前一直保持有效"的行为是一致的直觉。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o threaded_irq_demo threaded_irq_demo.c
 * 运行：
 *   ./threaded_irq_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define IRQ_NONE        0
#define IRQ_HANDLED     1
#define IRQ_WAKE_THREAD 2  /* 对应真实内核 include/linux/interrupt.h 的 IRQ_WAKE_THREAD */

/* ------------------------------------------------------------------ */
/* 1. 契约：primary handler 和 threaded handler 两个独立的函数指针字段    */
/* ------------------------------------------------------------------ */

struct irq_thread_action;

typedef int (*irq_primary_fn)(int irq, void *dev_id);
typedef int (*irq_thread_fn_t)(int irq, void *dev_id);

struct irq_thread_action {
	int irq;
	void *dev_id;
	const char *name;

	irq_primary_fn  primary;    /* 对应 action->handler，硬中断上下文 */
	irq_thread_fn_t thread_fn;  /* 对应 action->thread_fn，进程上下文 */

	/* 模拟 desc->threads_active / wake_up_process() 的最小同步原语 */
	pthread_t       thread;
	pthread_mutex_t lock;
	pthread_cond_t  cond;
	int             pending;     /* 有几次"唤醒"还没被线程消费掉 */
	int             stop;
	unsigned long   coalesced;   /* 统计：被合并掉的唤醒次数 */
	unsigned long   fired_total; /* 统计：primary 总共被触发次数 */
	unsigned long   serviced_total; /* 统计：thread_fn 总共执行次数 */
};

/* ------------------------------------------------------------------ */
/* 2. Core：irq_thread() 主循环 —— 对应 kernel/irq/manage.c irq_thread() */
/* ------------------------------------------------------------------ */

static void *irq_thread_main(void *arg)
{
	struct irq_thread_action *act = arg;

	for (;;) {
		pthread_mutex_lock(&act->lock);
		while (act->pending == 0 && !act->stop)
			pthread_cond_wait(&act->cond, &act->lock);  /* 对应 irq_wait_for_interrupt() */

		if (act->stop && act->pending == 0) {
			pthread_mutex_unlock(&act->lock);
			break;
		}

		int batch = act->pending; /* 可能 > 1：说明硬件在线程忙碌时又触发了几次 */
		act->pending = 0;
		pthread_mutex_unlock(&act->lock);

		if (batch > 1)
			act->coalesced += (batch - 1);

		printf("  [thread:%s] woke up, servicing (coalesced %d hardware fire(s) into 1 pass)\n",
		       act->name, batch);
		act->thread_fn(act->irq, act->dev_id);   /* <-- 盲调，进程上下文，可以睡眠 */
		act->serviced_total++;
	}
	return NULL;
}

/*
 * fire_hardware_irq() —— 模拟硬件触发一次中断，对应真实内核
 * handle_irq_event_percpu() 里调用 action->handler() 之后，若返回
 * IRQ_WAKE_THREAD 则调用 irq_wake_thread()（本质是 wake_up_process()）。
 * 这个函数本身必须极快返回，绝不能在这里睡眠——这里用"只做一次函数调用
 * + 一次锁 + 一次 cond_signal"来模拟这条硬约束。
 */
static void fire_hardware_irq(struct irq_thread_action *act)
{
	act->fired_total++;
	int ret = act->primary(act->irq, act->dev_id);  /* <-- 盲调，硬中断上下文 */

	if (ret == IRQ_WAKE_THREAD) {
		pthread_mutex_lock(&act->lock);
		act->pending++;
		pthread_cond_signal(&act->cond);   /* 对应 wake_up_process(action->thread) */
		pthread_mutex_unlock(&act->lock);
	}
}

static void request_threaded_irq_sim(struct irq_thread_action *act)
{
	pthread_mutex_init(&act->lock, NULL);
	pthread_cond_init(&act->cond, NULL);
	act->pending = 0;
	act->stop = 0;
	act->coalesced = 0;
	act->fired_total = 0;
	act->serviced_total = 0;
	pthread_create(&act->thread, NULL, irq_thread_main, act);
	printf("[request_threaded_irq] registered '%s' on IRQ %d (primary=%p, thread_fn=%p)\n",
	       act->name, act->irq, (void *)(uintptr_t)act->primary, (void *)(uintptr_t)act->thread_fn);
}

static void free_threaded_irq_sim(struct irq_thread_action *act)
{
	pthread_mutex_lock(&act->lock);
	act->stop = 1;
	pthread_cond_signal(&act->cond);
	pthread_mutex_unlock(&act->lock);
	pthread_join(act->thread, NULL);
	pthread_mutex_destroy(&act->lock);
	pthread_cond_destroy(&act->cond);
	printf("[free_irq] '%s' thread stopped (fired=%lu, serviced=%lu, coalesced=%lu)\n",
	       act->name, act->fired_total, act->serviced_total, act->coalesced);
}

/* ------------------------------------------------------------------ */
/* 3. 一个模拟的 NIC 设备：primary 只读一个"中断状态寄存器"，快速判断+唤醒 */
/* ------------------------------------------------------------------ */

struct nic_device {
	const char *name;
	volatile int rx_ready; /* 硬件寄存器：有包到达 */
	int packets_processed;
};

static struct nic_device eth0 = { .name = "eth0", .rx_ready = 0, .packets_processed = 0 };

/* 硬中断上下文：只允许做最少的事情，这里只是读寄存器 + 决定是否唤醒线程 */
static int nic_primary_handler(int irq, void *dev_id)
{
	struct nic_device *nic = dev_id;

	(void)irq;
	if (!nic->rx_ready)
		return IRQ_NONE;

	/* 不在这里处理收包！只做最快的判断，把重活丢给线程 */
	return IRQ_WAKE_THREAD;
}

/* 进程上下文：可以做"重活"——这里用 usleep 模拟真实的收包处理耗时 */
static int nic_threaded_handler(int irq, void *dev_id)
{
	struct nic_device *nic = dev_id;

	(void)irq;
	nic->rx_ready = 0; /* 模拟"处理完，清中断标志" */
	usleep(40 * 1000);  /* 模拟耗时的收包处理：40ms */
	nic->packets_processed++;
	printf("    [%s] finished processing packet batch #%d\n", nic->name, nic->packets_processed);
	return IRQ_HANDLED;
}

int main(void)
{
	struct irq_thread_action act = {
		.irq = 11,
		.dev_id = &eth0,
		.name = "eth0",
		.primary = nic_primary_handler,
		.thread_fn = nic_threaded_handler,
	};

	printf("=== threaded_irq_demo ===\n\n");

	request_threaded_irq_sim(&act);

	printf("\n-- 模拟硬件在 100ms 内密集触发 5 次中断（每 10ms 一次），"
	       "但每次线程处理需要 40ms，必然发生合并 --\n");
	for (int i = 0; i < 5; i++) {
		eth0.rx_ready = 1;
		struct timespec ts0;
		clock_gettime(CLOCK_MONOTONIC, &ts0);

		fire_hardware_irq(&act);

		struct timespec ts1;
		clock_gettime(CLOCK_MONOTONIC, &ts1);
		long us = (ts1.tv_sec - ts0.tv_sec) * 1000000L + (ts1.tv_nsec - ts0.tv_nsec) / 1000L;
		printf("  [hw] fired interrupt #%d, primary handler returned in %ld us "
		       "(hard-irq context must be fast)\n", i + 1, us);

		usleep(10 * 1000); /* 硬件每 10ms 触发一次 */
	}

	printf("\n-- 等待线程处理完所有已排队的工作 --\n");
	usleep(250 * 1000); /* 给线程足够时间把剩余的批次处理完 */

	printf("\n-- 卸载中断线程 --\n");
	free_threaded_irq_sim(&act);

	printf("\n=== done, eth0.packets_processed=%d ===\n", eth0.packets_processed);
	return 0;
}
