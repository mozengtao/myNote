/*
 * irq_free_and_race_demo.c
 *
 * 场景：演示 free_irq() 与"正在执行中"的 handler 之间的竞态，以及真实内核
 * synchronize_irq() 是如何消灭这个竞态窗口的。
 *
 * 对应真实内核：
 *   - kernel/irq/manage.c __free_irq()：unlink action 之后调用 synchronize_irq(irq)，
 *     再继续做后续清理（本系列 16 篇正文引用了真实行号）。
 *   - kernel/irq/manage.c synchronize_irq()：忙等 IRQD_IRQ_INPROGRESS 清除，
 *     再等待所有 threaded handler 的 desc->threads_active 归零。
 *
 * 如果驱动在 free_irq() 之后立即释放 dev_id 指向的内存（这是绝大多数驱动的
 * 正常写法：free_irq() 保证返回时不会再有 handler 在跑），但 free_irq() 内部
 * 如果*没有*正确等待"正在执行中的 handler"，就会出现：
 *
 *   CPU0（unbind 线程）                    CPU1（硬件中断线程，正在跑 handler）
 *   ----------------------                  ------------------------------
 *   unlink action from chain
 *   free(dev_id)  <---------- 竞态窗口 ---->  action->handler(irq, dev_id)
 *                                             仍在访问已经被释放/覆写的内存！
 *
 * 本示例分别实现"错误版本"（不等待）和"正确版本"（等待），用一个 magic
 * 校验值模拟"内存是否已被释放/覆写"，量化展示竞态是否真的发生。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o irq_free_and_race_demo irq_free_and_race_demo.c
 * 运行：
 *   ./irq_free_and_race_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define ALIVE_MAGIC  0xA1A1A1A1u
#define FREED_MAGIC  0xDEADDEADu

/* ------------------------------------------------------------------ */
/* 1. 被注入的私有数据：driver 自己的设备状态，handler 通过 dev_id 拿到它 */
/* ------------------------------------------------------------------ */

struct device_priv {
	char name[32];
	unsigned int magic;   /* ALIVE_MAGIC 表示还没被释放；FREED_MAGIC 表示已释放 */
};

/* ------------------------------------------------------------------ */
/* 2. Core：irq_desc_sim，跟踪"handler 是否正在执行中"（IRQD_IRQ_INPROGRESS）*/
/* ------------------------------------------------------------------ */

typedef int (*irq_handler_fn)(int irq, void *dev_id);

struct irqaction_sim {
	irq_handler_fn handler;
	void *dev_id;
	const char *name;
};

struct irq_desc_sim {
	int irq;
	struct irqaction_sim *action;
	pthread_mutex_t lock;
	pthread_cond_t  done_cond;
	int in_progress;            /* 对应真实内核的 IRQD_IRQ_INPROGRESS 标志位 */
};

static void irq_desc_init(struct irq_desc_sim *desc, int irq)
{
	desc->irq = irq;
	desc->action = NULL;
	desc->in_progress = 0;
	pthread_mutex_init(&desc->lock, NULL);
	pthread_cond_init(&desc->done_cond, NULL);
}

/*
 * handle_irq_event_sim() —— 对应真实内核 handle_irq_event()/handle_irq_event_percpu()：
 * 进入 handler 之前标记 in_progress，返回之后清除并广播——这正是
 * synchronize_irq() 要等待的那个标志位。
 */
static void handle_irq_event_sim(struct irq_desc_sim *desc)
{
	pthread_mutex_lock(&desc->lock);
	desc->in_progress = 1;      /* 对应 irqd_set(IRQD_IRQ_INPROGRESS) */
	pthread_mutex_unlock(&desc->lock);

	desc->action->handler(desc->irq, desc->action->dev_id);   /* <-- 盲调，可能耗时 */

	pthread_mutex_lock(&desc->lock);
	desc->in_progress = 0;      /* 对应 irqd_clear(IRQD_IRQ_INPROGRESS) */
	pthread_cond_broadcast(&desc->done_cond);
	pthread_mutex_unlock(&desc->lock);
}

/*
 * synchronize_irq_sim() —— 对应真实内核 kernel/irq/manage.c synchronize_irq()：
 * 阻塞直到当前没有任何一个 handler 正在执行。
 */
static void synchronize_irq_sim(struct irq_desc_sim *desc)
{
	pthread_mutex_lock(&desc->lock);
	while (desc->in_progress)
		pthread_cond_wait(&desc->done_cond, &desc->lock);
	pthread_mutex_unlock(&desc->lock);
}

/* ------------------------------------------------------------------ */
/* 3. 设备的 handler：模拟一次"耗时"的中断处理，处理前后都检查 magic       */
/* ------------------------------------------------------------------ */

static int device_irq_handler(int irq, void *dev_id)
{
	struct device_priv *priv = dev_id;

	(void)irq;
	printf("    [handler] start, dev='%s', magic=0x%08x (expect ALIVE=0x%08x)\n",
	       priv->name, priv->magic, ALIVE_MAGIC);

	usleep(30 * 1000); /* 模拟耗时处理：30ms —— 这段时间正是竞态窗口 */

	if (priv->magic != ALIVE_MAGIC) {
		printf("    [handler] !!! USE-AFTER-FREE DETECTED !!! magic is now 0x%08x, "
		       "memory was freed while handler was still running\n", priv->magic);
	} else {
		printf("    [handler] finished safely, magic still 0x%08x\n", priv->magic);
	}
	return 1; /* IRQ_HANDLED */
}

/* ------------------------------------------------------------------ */
/* 4. 错误版本：free_irq() 不等待正在执行中的 handler                    */
/* ------------------------------------------------------------------ */

static void *hardware_fire_thread(void *arg)
{
	struct irq_desc_sim *desc = arg;

	handle_irq_event_sim(desc);
	return NULL;
}

static void buggy_free_irq(struct irq_desc_sim *desc, struct device_priv *priv)
{
	printf("  [buggy_free_irq] unlinking action WITHOUT calling synchronize_irq()\n");
	desc->action = NULL; /* 从链表摘除：之后不会再有新的中断分发到这个 handler */

	usleep(5 * 1000); /* 模拟 unbind 流程走到"释放私有数据"这一步所需的一点时间 */

	printf("  [buggy_free_irq] freeing dev_id memory NOW (handler may still be running!)\n");
	priv->magic = FREED_MAGIC; /* 模拟内存被释放/被重新分配后覆写 */
}

/* ------------------------------------------------------------------ */
/* 5. 正确版本：free_irq() 先 synchronize_irq()，再释放内存                */
/* ------------------------------------------------------------------ */

static void safe_free_irq(struct irq_desc_sim *desc, struct device_priv *priv)
{
	printf("  [safe_free_irq] unlinking action\n");
	desc->action = NULL;

	printf("  [safe_free_irq] calling synchronize_irq(), blocking until in-flight handler done...\n");
	synchronize_irq_sim(desc);   /* <-- 关键一步：等到 in_progress==0 才继续 */

	printf("  [safe_free_irq] synchronize_irq() returned, now safe to free dev_id memory\n");
	priv->magic = FREED_MAGIC;
}

/* ------------------------------------------------------------------ */
/* 6. 两个场景的驱动代码                                                */
/* ------------------------------------------------------------------ */

static void run_scenario(const char *title, int use_safe_free)
{
	struct irq_desc_sim desc;
	struct irqaction_sim action;
	struct device_priv *priv = malloc(sizeof(*priv));
	pthread_t hw_thread;

	printf("\n== %s ==\n", title);

	irq_desc_init(&desc, 7);
	snprintf(priv->name, sizeof(priv->name), "%s", "demo-dev");
	priv->magic = ALIVE_MAGIC;

	action.handler = device_irq_handler;
	action.dev_id = priv;
	action.name = "demo-dev";
	desc.action = &action;

	/* 模拟"硬件触发中断，handler 在另一个执行上下文里跑起来" */
	pthread_create(&hw_thread, NULL, hardware_fire_thread, &desc);

	/* 模拟"几乎同时，用户做 rmmod / unbind"，驱动的 remove() 里调用 free_irq() */
	usleep(3 * 1000); /* 确保 handler 已经进入并开始 sleep，制造真实的竞态窗口 */
	if (use_safe_free)
		safe_free_irq(&desc, priv);
	else
		buggy_free_irq(&desc, priv);

	pthread_join(hw_thread, NULL);
	pthread_mutex_destroy(&desc.lock);
	pthread_cond_destroy(&desc.done_cond);
	free(priv);

	printf("== %s: scenario complete ==\n", title);
}

int main(void)
{
	printf("=== irq_free_and_race_demo ===\n");

	run_scenario("场景1：buggy_free_irq（不调用 synchronize_irq，存在竞态）", 0);
	run_scenario("场景2：safe_free_irq（调用 synchronize_irq，竞态被消灭）", 1);

	printf("\n=== done ===\n");
	return 0;
}
