/*
 * irq_action_chain_sim.c
 *
 * 场景：模拟共享中断线（shared IRQ line）——多个设备的 struct irqaction
 * 挂在同一个 IRQ 号上，硬件触发一次中断时，Core（中断分发核心）并不知道
 * 到底是哪个设备触发的，只能把链上*所有*挂载的 handler 都调用一遍，
 * 由每个 handler 自己读取"自己设备的硬件寄存器"来判断"这次中断是不是我的”，
 * 通过返回值 IRQ_HANDLED / IRQ_NONE 告知 Core。
 *
 * 对应真实内核：include/linux/interrupt.h 的 struct irqaction（handler/next
 * 字段）+ kernel/irq/handle.c 的 handle_irq_event_percpu()（本系列 16 篇正文
 * 详细引用了真实行号）。这里用普通用户态代码还原同样的链式分发结构。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -std=gnu11 -o irq_action_chain_sim irq_action_chain_sim.c
 * 运行：
 *   ./irq_action_chain_sim
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 1. 契约：与 include/linux/interrupt.h 的 irq_handler_t 同构           */
/* ------------------------------------------------------------------ */

#define IRQ_NONE    0   /* 对应 kernel 的 IRQ_NONE：这个中断不是我触发的 */
#define IRQ_HANDLED 1   /* 对应 kernel 的 IRQ_HANDLED：我处理了 */

struct irqaction_sim;

/* handler 签名：与真实 irq_handler_t handler(int irq, void *dev_id) 同构 */
typedef int (*irq_handler_fn)(int irq, void *dev_id);

struct irqaction_sim {
	irq_handler_fn handler;
	void *dev_id;                 /* Context Injection：每个设备的私有数据 */
	const char *name;
	struct irqaction_sim *next;   /* 共享中断线：单链表 */
};

/* ------------------------------------------------------------------ */
/* 2. Core：模拟 irq_desc + handle_irq_event_percpu() 的分发逻辑          */
/* ------------------------------------------------------------------ */

struct irq_desc_sim {
	int irq;
	struct irqaction_sim *action;   /* 链表头，对应 desc->action */
	unsigned long spurious_count;   /* 对应真实 kernel note_interrupt() 的统计 */
};

/*
 * setup_irq_sim() —— 对应真实内核 __setup_irq()：
 * "add new interrupt at end of irq queue"，新的 action 插入链表末尾。
 */
static void setup_irq_sim(struct irq_desc_sim *desc, struct irqaction_sim *new)
{
	struct irqaction_sim **tail = &desc->action;

	while (*tail)
		tail = &(*tail)->next;
	*tail = new;
	new->next = NULL;

	printf("[setup_irq] registered '%s' on IRQ %d\n", new->name, desc->irq);
}

/*
 * free_irq_sim() —— 对应真实内核 __free_irq()：按 dev_id 找到并从链表摘除。
 */
static void free_irq_sim(struct irq_desc_sim *desc, void *dev_id)
{
	struct irqaction_sim **cur = &desc->action;

	while (*cur) {
		if ((*cur)->dev_id == dev_id) {
			struct irqaction_sim *victim = *cur;
			*cur = victim->next;
			printf("[free_irq] removed '%s' from IRQ %d\n", victim->name, desc->irq);
			return;
		}
		cur = &(*cur)->next;
	}
	fprintf(stderr, "[free_irq] warning: dev_id not found on IRQ %d\n", desc->irq);
}

/*
 * handle_irq_event_percpu_sim() —— 对应真实内核 kernel/irq/handle.c
 * handle_irq_event_percpu()：do { action->handler(...); action = action->next; } while (action);
 * 注意真实内核*不会*在某个 handler 返回 IRQ_HANDLED 后就提前退出循环——
 * 因为共享中断线上可能同时有多个设备都产生了中断，必须让每一个都有机会处理。
 */
static void handle_irq_event_percpu_sim(struct irq_desc_sim *desc)
{
	struct irqaction_sim *action = desc->action;
	int any_handled = 0;
	int n = 0;

	printf("[core] hardware IRQ %d fired, walking action chain:\n", desc->irq);

	if (!action) {
		fprintf(stderr, "[core] IRQ %d fired but no action registered!\n", desc->irq);
		return;
	}

	do {
		int ret = action->handler(desc->irq, action->dev_id);  /* <-- 盲调 */

		n++;
		printf("  [%d] '%s' returned %s\n", n, action->name,
		       ret == IRQ_HANDLED ? "IRQ_HANDLED" : "IRQ_NONE");
		if (ret == IRQ_HANDLED)
			any_handled = 1;

		action = action->next;
	} while (action);

	if (!any_handled) {
		desc->spurious_count++;
		fprintf(stderr,
			"[core] WARNING: spurious interrupt on IRQ %d (nobody claimed it, "
			"total spurious so far: %lu)\n",
			desc->irq, desc->spurious_count);
	}
}

/* ------------------------------------------------------------------ */
/* 3. 两个共享同一 IRQ 4 的模拟串口设备                                  */
/* ------------------------------------------------------------------ */

struct serial_port {
	const char *name;
	volatile int rx_pending; /* 模拟硬件寄存器：有数据到达时置 1 */
};

static struct serial_port ttyS0 = { .name = "ttyS0", .rx_pending = 0 };
static struct serial_port ttyS1 = { .name = "ttyS1", .rx_pending = 0 };

static int serial_irq_handler(int irq, void *dev_id)
{
	struct serial_port *port = dev_id;

	(void)irq;
	if (!port->rx_pending)
		return IRQ_NONE; /* 不是我的中断：我的寄存器没有 pending 标志 */

	printf("    [%s] servicing RX interrupt, clearing pending bit\n", port->name);
	port->rx_pending = 0; /* 模拟"读一次数据寄存器清中断" */
	return IRQ_HANDLED;
}

int main(void)
{
	struct irq_desc_sim irq4 = { .irq = 4, .action = NULL, .spurious_count = 0 };

	printf("=== irq_action_chain_sim demo ===\n\n");

	struct irqaction_sim act0 = { .handler = serial_irq_handler, .dev_id = &ttyS0, .name = "ttyS0" };
	struct irqaction_sim act1 = { .handler = serial_irq_handler, .dev_id = &ttyS1, .name = "ttyS1" };

	setup_irq_sim(&irq4, &act0);
	setup_irq_sim(&irq4, &act1);

	printf("\n-- 场景1：只有 ttyS0 有数据到达 --\n");
	ttyS0.rx_pending = 1;
	handle_irq_event_percpu_sim(&irq4);

	printf("\n-- 场景2：只有 ttyS1 有数据到达 --\n");
	ttyS1.rx_pending = 1;
	handle_irq_event_percpu_sim(&irq4);

	printf("\n-- 场景3：两个设备同时有数据到达（真实硬件上很常见） --\n");
	ttyS0.rx_pending = 1;
	ttyS1.rx_pending = 1;
	handle_irq_event_percpu_sim(&irq4);

	printf("\n-- 场景4：虚假中断（两个设备都没有 pending，触发 spurious 警告） --\n");
	handle_irq_event_percpu_sim(&irq4);

	printf("\n-- 卸载 ttyS0 驱动 --\n");
	free_irq_sim(&irq4, &ttyS0);

	printf("\n-- 场景5：ttyS0 已卸载，即使它的硬件寄存器意外置位也不会再被处理到 --\n");
	ttyS0.rx_pending = 1; /* 这个标志现在再也不会被任何 handler 看到 */
	ttyS1.rx_pending = 1;
	handle_irq_event_percpu_sim(&irq4);

	printf("\n=== done, spurious_count=%lu ===\n", irq4.spurious_count);
	return 0;
}
