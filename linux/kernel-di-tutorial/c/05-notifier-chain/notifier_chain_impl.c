/*
 * notifier_chain_impl.c
 *
 * 场景：从零实现一个"notifier chain"，尽量贴近 Kernel 真实实现
 * kernel/notifier.c 的结构（notifier_chain_register / notifier_call_chain），
 * 包括：
 *   - 按 priority 插入排序
 *   - NOTIFY_STOP_MASK 语义：某个 notifier 返回"停止"，链就不再往下传播
 *   - 注册/注销的动态性
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o notifier_chain_impl notifier_chain_impl.c
 * 运行：
 *   ./notifier_chain_impl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 1. 契约：notifier_block —— 与 include/linux/notifier.h:50 同构      */
/* ------------------------------------------------------------------ */

struct notifier_block {
	int (*notifier_call)(struct notifier_block *self, unsigned long action, void *data);
	struct notifier_block *next;
	int priority;   /* 数字越大越先被调用，与真实 Kernel 一致 */
};

/* 返回值语义，对应 Kernel NOTIFY_* 常量 */
#define NOTIFY_DONE          0x0000  /* 不关心这个事件 */
#define NOTIFY_OK            0x0001  /* 已处理，继续往下传播 */
#define NOTIFY_STOP_MASK     0x8000  /* 与其它值 OR 起来表示"停止传播" */
#define NOTIFY_BAD           (0x0002 | NOTIFY_STOP_MASK)
#define NOTIFY_STOP          (NOTIFY_OK | NOTIFY_STOP_MASK)

struct notifier_head {
	struct notifier_block *head;
};

/* ------------------------------------------------------------------ */
/* 2. Core：注册（按 priority 排序插入）与广播调用                     */
/* ------------------------------------------------------------------ */

static int notifier_chain_register(struct notifier_head *nh, struct notifier_block *n)
{
	struct notifier_block **pp = &nh->head;

	while (*pp) {
		if (n->priority > (*pp)->priority)
			break;
		pp = &(*pp)->next;
	}
	n->next = *pp;
	*pp = n;
	return 0;
}

static int notifier_chain_unregister(struct notifier_head *nh, struct notifier_block *n)
{
	struct notifier_block **pp = &nh->head;

	while (*pp) {
		if (*pp == n) {
			*pp = n->next;
			n->next = NULL;
			return 0;
		}
		pp = &(*pp)->next;
	}
	return -1;
}

/* notifier_call_chain()：逐一广播，遇到 NOTIFY_STOP_MASK 立即停止 */
static int notifier_call_chain(struct notifier_head *nh, unsigned long action, void *data)
{
	struct notifier_block *nb = nh->head;
	int ret = NOTIFY_DONE;

	while (nb) {
		ret = nb->notifier_call(nb, action, data);   /* <-- 盲调，不知道具体是谁 */
		if (ret & NOTIFY_STOP_MASK) {
			printf("    [chain] notifier returned STOP (0x%x), halting propagation\n", ret);
			break;
		}
		nb = nb->next;
	}
	return ret;
}

/* ------------------------------------------------------------------ */
/* 3. 事件定义：模拟 CPU 上线/下线事件（对应 Kernel CPU hotplug notifier）*/
/* ------------------------------------------------------------------ */

#define CPU_ONLINE   1
#define CPU_DOWN_PREPARE 2

static struct notifier_head cpu_notifier_head;

/* ------------------------------------------------------------------ */
/* 4. 订阅者 A：调度器子系统（高优先级，必须最先响应）                  */
/* ------------------------------------------------------------------ */

static int sched_cpu_notifier(struct notifier_block *self, unsigned long action, void *data)
{
	(void)self;
	int cpu = *(int *)data;
	if (action == CPU_ONLINE) {
		printf("  [sched] rebalancing runqueues for new CPU %d\n", cpu);
		return NOTIFY_OK;
	} else if (action == CPU_DOWN_PREPARE) {
		printf("  [sched] migrating tasks off CPU %d\n", cpu);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block sched_nb = { .notifier_call = sched_cpu_notifier, .priority = 100 };

/* ------------------------------------------------------------------ */
/* 5. 订阅者 B：性能计数器子系统                                        */
/* ------------------------------------------------------------------ */

static int perf_cpu_notifier(struct notifier_block *self, unsigned long action, void *data)
{
	(void)self;
	int cpu = *(int *)data;
	if (action == CPU_ONLINE) {
		printf("  [perf] initializing PMU counters for CPU %d\n", cpu);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block perf_nb = { .notifier_call = perf_cpu_notifier, .priority = 50 };

/* ------------------------------------------------------------------ */
/* 6. 订阅者 C：一个"看门人"，对某些不合法的 CPU 编号直接拒绝并停止链  */
/* ------------------------------------------------------------------ */

static int guard_cpu_notifier(struct notifier_block *self, unsigned long action, void *data)
{
	(void)self;
	int cpu = *(int *)data;
	if (action == CPU_ONLINE && cpu >= 64) {
		fprintf(stderr, "  [guard] CPU %d exceeds supported max, rejecting (STOP chain)\n", cpu);
		return NOTIFY_BAD;   /* 触发 STOP_MASK，后面的 notifier 不会被调用 */
	}
	return NOTIFY_DONE;
}

/* 优先级最高：先做安全校验，再决定是否放行给其它子系统 */
static struct notifier_block guard_nb = { .notifier_call = guard_cpu_notifier, .priority = 1000 };

int main(void)
{
	int cpu1 = 3, cpu2 = 99;
	int ret;

	printf("=== notifier_chain_impl demo ===\n\n");

	notifier_chain_register(&cpu_notifier_head, &perf_nb);   /* 故意乱序注册 */
	notifier_chain_register(&cpu_notifier_head, &guard_nb);
	notifier_chain_register(&cpu_notifier_head, &sched_nb);

	printf("-- 链上顺序（按 priority 从高到低）--\n");
	{
		struct notifier_block *nb = cpu_notifier_head.head;
		while (nb) {
			printf("  priority=%-5d\n", nb->priority);
			nb = nb->next;
		}
	}

	printf("\n-- 广播 CPU_ONLINE for cpu=%d（合法）--\n", cpu1);
	ret = notifier_call_chain(&cpu_notifier_head, CPU_ONLINE, &cpu1);
	printf("  final ret = 0x%x\n", ret);

	printf("\n-- 广播 CPU_ONLINE for cpu=%d（超出上限，guard 会 STOP 链）--\n", cpu2);
	ret = notifier_call_chain(&cpu_notifier_head, CPU_ONLINE, &cpu2);
	printf("  final ret = 0x%x (sched/perf 不应该被调用到)\n", ret);

	printf("\n-- 注销 perf 订阅，再广播一次 CPU_DOWN_PREPARE --\n");
	notifier_chain_unregister(&cpu_notifier_head, &perf_nb);
	ret = notifier_call_chain(&cpu_notifier_head, CPU_DOWN_PREPARE, &cpu1);
	printf("  final ret = 0x%x\n", ret);

	printf("\n=== done ===\n");
	return 0;
}
