/*
 * call_rcu_grace_period_sim.c
 *
 * 场景：模拟 call_rcu() 注册的回调如何被"延后到 grace period 结束之后"才执行，
 * 而不是像普通回调一样"事件发生就立刻调用"。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   include/linux/types.h:246     struct rcu_head { next; func; }
 *   include/linux/rcupdate.h:85   call_rcu() 原型
 *   kernel/rcutree.c:1589         __call_rcu()：把 head 挂到本 CPU 的 rdp->nxtlist
 *   kernel/rcutree.c:1555         rcu_process_callbacks()：软中断上下文里驱动整个流程
 *   kernel/rcutree.c:1519         __rcu_process_callbacks()：检测 grace period 是否结束
 *   kernel/rcutree.c:1257         rcu_do_batch()：grace period 结束后真正调用回调
 *
 * 简化模型：
 *   真实 RCU 的 grace period 判定极其复杂（per-CPU quiescent state、动态 tick、
 *   NMI 处理等），本文件用一个"每个 reader 线程周期性做心跳/checkpoint"的简化模型
 *   代替：grace period 检测器给每个 reader 记一个"上次心跳序号"的快照，
 *   只有当**所有** reader 的心跳序号都比快照时刻新，才认为 grace period 结束——
 *   这正是真实 RCU"所有 CPU 都至少经过一次 quiescent state"的直接类比。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o call_rcu_grace_period_sim call_rcu_grace_period_sim.c
 * 运行：
 *   ./call_rcu_grace_period_sim
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) *__mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })

#define NR_READERS 4

/* ------------------------------------------------------------------ */
/* 1. 契约：mini_rcu_head，对应 include/linux/types.h:246 struct rcu_head */
/* ------------------------------------------------------------------ */

struct mini_rcu_head {
	struct mini_rcu_head *next;
	void (*func)(struct mini_rcu_head *head);   /* Function Pointer Callback，01篇 */
	const char *label;                          /* 演示用，方便打印 */
};

/* ------------------------------------------------------------------ */
/* 2. Core：callback pending 队列 + grace period 检测器                 */
/* ------------------------------------------------------------------ */

static pthread_mutex_t g_cb_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mini_rcu_head *g_cb_head, **g_cb_tail = &g_cb_head;

/* 每个 reader 线程独立的"心跳序号"：每完成一次 read-side critical section
 * （对应真实 rcu_read_unlock() 之后的一次 quiescent state）就自增一次 */
static atomic_long g_reader_heartbeat[NR_READERS];
static atomic_int g_readers_running = 1;

/* 对应 include/linux/rcupdate.h:85 call_rcu() —— 只是登记回调，立刻返回，不等待 */
static void mini_call_rcu(struct mini_rcu_head *head, void (*func)(struct mini_rcu_head *))
{
	head->func = func;
	head->next = NULL;

	pthread_mutex_lock(&g_cb_lock);
	*g_cb_tail = head;
	g_cb_tail = &head->next;
	pthread_mutex_unlock(&g_cb_lock);

	printf("  [call_rcu] 注册回调 '%s'，立刻返回（不等待 grace period）\n", head->label);
}

/* 模拟 reader 侧：rcu_read_lock() -> 访问数据 -> rcu_read_unlock()
 * 对应 include/linux/rcupdate.h:636 rcu_read_lock() / :658 rcu_read_unlock() */
static void mini_rcu_read_side_critical_section(int reader_id)
{
	/* rcu_read_lock()：本模拟里读者不需要真的持有任何锁 */
	usleep(200);   /* 模拟"正在读取一份 RCU 保护的数据"需要一点时间 */
	/* rcu_read_unlock()：退出临界区 == 经过一次 quiescent state */
	atomic_fetch_add(&g_reader_heartbeat[reader_id], 1);
}

static void *reader_thread_fn(void *arg)
{
	int id = (int)(long)arg;
	while (atomic_load(&g_readers_running)) {
		mini_rcu_read_side_critical_section(id);
		usleep(300);
	}
	return NULL;
}

/*
 * wait_for_grace_period()：对应真实 synchronize_rcu() 的"同步版本"简化模拟。
 * 真实 kernel 的 call_rcu() 是异步的（rcu_process_callbacks 在软中断里检测），
 * 这里为了让 main() 能清楚地打印"grace period 正在进行中"，用同步等待实现，
 * 但判定逻辑（快照 + 等所有 reader 心跳前进）与异步版本完全一致。
 */
static void wait_for_grace_period(void)
{
	long snapshot[NR_READERS];
	for (int i = 0; i < NR_READERS; i++)
		snapshot[i] = atomic_load(&g_reader_heartbeat[i]);

	printf("  [grace_period] 记录快照: ");
	for (int i = 0; i < NR_READERS; i++)
		printf("r%d=%ld ", i, snapshot[i]);
	printf("\n");

	for (;;) {
		bool all_passed = true;
		for (int i = 0; i < NR_READERS; i++) {
			if (atomic_load(&g_reader_heartbeat[i]) <= snapshot[i]) {
				all_passed = false;
				break;
			}
		}
		if (all_passed)
			break;
		usleep(500);
	}
	printf("  [grace_period] 所有 reader 都已经至少经过一次 quiescent state，grace period 结束\n");
}

/* 对应 kernel/rcutree.c:1257 rcu_do_batch() —— grace period 结束后真正调用回调 */
static void run_ready_callbacks(void)
{
	struct mini_rcu_head *list;

	pthread_mutex_lock(&g_cb_lock);
	list = g_cb_head;
	g_cb_head = NULL;
	g_cb_tail = &g_cb_head;
	pthread_mutex_unlock(&g_cb_lock);

	while (list) {
		struct mini_rcu_head *h = list;
		list = list->next;
		printf("  [rcu_do_batch] grace period 已结束，安全调用回调 '%s'\n", h->label);
		h->func(h);
	}
}

/* ------------------------------------------------------------------ */
/* 3. 业务场景：一个"节点被摘除后安全释放"的回调                        */
/* ------------------------------------------------------------------ */

struct config_node {
	char name[32];
	int  value;
	struct mini_rcu_head rcu;   /* Context Injection（03篇）：内嵌 rcu_head */
};

static atomic_long g_freed_count;

static void config_node_free_callback(struct mini_rcu_head *head)
{
	/* container_of：从通用的 rcu_head 指针反推出完整业务对象，见 03 篇 */
	struct config_node *node = container_of(head, struct config_node, rcu);
	printf("    [free] 真正释放 config_node '%s' (value=%d) 的内存\n",
	       node->name, node->value);
	free(node);
	atomic_fetch_add(&g_freed_count, 1);
}

int main(void)
{
	pthread_t readers[NR_READERS];

	printf("=== call_rcu_grace_period_sim ===\n\n");

	for (int i = 0; i < NR_READERS; i++)
		atomic_store(&g_reader_heartbeat[i], 0);

	printf("-- 启动 %d 个 reader 线程，持续做 rcu_read_lock/unlock --\n\n", NR_READERS);
	for (int i = 0; i < NR_READERS; i++)
		pthread_create(&readers[i], NULL, reader_thread_fn, (void *)(long)i);

	usleep(50 * 1000);   /* 让 reader 先跑起来 */

	printf("-- 场景：writer 摘除一个旧的 config_node，注册 call_rcu 延迟释放 --\n");
	struct config_node *old_node = malloc(sizeof(*old_node));
	snprintf(old_node->name, sizeof(old_node->name), "old-config");
	old_node->value = 42;
	old_node->rcu.label = old_node->name;

	printf("  (摘除动作：假设已经有另一个线程把全局指针指向了新节点，\n"
	       "   old_node 现在对新的 reader 已经不可达，但可能还有正在读它的 reader)\n");
	mini_call_rcu(&old_node->rcu, config_node_free_callback);

	printf("\n-- 等待 grace period（所有 reader 至少完成一次心跳）--\n");
	wait_for_grace_period();

	printf("\n-- grace period 结束，执行 rcu_do_batch 风格的回调调用 --\n");
	run_ready_callbacks();

	printf("\n-- 再注册两个回调，验证批量场景 --\n");
	struct config_node *n1 = malloc(sizeof(*n1));
	struct config_node *n2 = malloc(sizeof(*n2));
	snprintf(n1->name, sizeof(n1->name), "cfg-1");
	n1->value = 1;
	n1->rcu.label = n1->name;
	snprintf(n2->name, sizeof(n2->name), "cfg-2");
	n2->value = 2;
	n2->rcu.label = n2->name;
	mini_call_rcu(&n1->rcu, config_node_free_callback);
	mini_call_rcu(&n2->rcu, config_node_free_callback);
	wait_for_grace_period();
	run_ready_callbacks();

	atomic_store(&g_readers_running, 0);
	for (int i = 0; i < NR_READERS; i++)
		pthread_join(readers[i], NULL);

	printf("\n=== done, 总共安全释放了 %ld 个节点 ===\n", atomic_load(&g_freed_count));
	return 0;
}
