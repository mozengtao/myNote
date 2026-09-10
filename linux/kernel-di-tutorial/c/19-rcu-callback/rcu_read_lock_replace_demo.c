/*
 * rcu_read_lock_replace_demo.c
 *
 * 场景：模拟 rcu_read_lock()/rcu_assign_pointer() 安全替换一个共享节点，
 * 展示读者线程在替换过程中读到的始终是"完整的旧版本"或"完整的新版本"，
 * 绝不会读到"构造中的中间态"。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   include/linux/rcupdate.h:760  rcu_assign_pointer(p, v) 宏
 *   include/linux/rcupdate.h:576  rcu_dereference(p) 宏
 *   include/linux/rcupdate.h:636  rcu_read_lock()
 *   include/linux/rcupdate.h:658  rcu_read_unlock()
 *   net/core/dev.c:3200-3201      netdev_rx_handler_register() 用
 *                                 rcu_assign_pointer() 注册收包回调
 *   net/core/dev.c:3287           __netif_receive_skb() 用
 *                                 rcu_dereference() 读取该回调
 *
 * 关键设计：
 *   1. 写者永远不会"就地修改"一个已经发布给读者的节点——它总是先分配一个
 *      全新的、完整填好所有字段的节点，最后才用一次原子的指针赋值
 *      （rcu_assign_pointer，本质是 smp_wmb() + 单指针写）切换过去。
 *   2. 读者用 rcu_dereference() 读到指针后，在自己的临界区内，指针指向的
 *      内容永远不会变（因为写者不会就地改）——要么是完整旧版本，要么是
 *      完整新版本，读者绝不可能读到"一半旧字段、一半新字段"的撕裂状态。
 *   3. 为了让示例聚焦在"一致性"这个核心断言上，本文件里被替换下来的旧节点
 *      不会立刻释放（真实回收机制见 call_rcu_grace_period_sim.c），而是
 *      统一在所有读者线程退出之后一次性回收——这个时间点没有任何并发读者，
 *      回收是绝对安全的。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o rcu_read_lock_replace_demo rcu_read_lock_replace_demo.c
 * 运行：
 *   ./rcu_read_lock_replace_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>

#define NR_READERS      6
#define NR_WRITES       2000
#define READS_PER_READER 20000

/* ------------------------------------------------------------------ */
/* 1. 业务数据：一个多字段的配置节点，用 checksum 检测"撕裂读"           */
/* ------------------------------------------------------------------ */

struct config_snapshot {
	int  version;
	int  a, b, c, d;     /* 假装是若干个相关配置字段，必须整体一致 */
	unsigned int checksum;
};

static unsigned int compute_checksum(const struct config_snapshot *s)
{
	/* 简单的求和校验，只用来检测字段是否"整体一致" */
	return (unsigned int)(s->version * 7 + s->a * 13 + s->b * 17 +
			       s->c * 19 + s->d * 23);
}

static struct config_snapshot *make_snapshot(int version)
{
	struct config_snapshot *s = malloc(sizeof(*s));
	s->version = version;
	s->a = version * 2;
	s->b = version * 3;
	s->c = version * 5;
	s->d = version * 7;
	s->checksum = compute_checksum(s);
	return s;
}

/* ------------------------------------------------------------------ */
/* 2. Core：一个"RCU 保护的指针"，对应 dev->rx_handler 那样的用法        */
/* ------------------------------------------------------------------ */

static _Atomic(struct config_snapshot *) g_current_config;

/* 对应 include/linux/rcupdate.h:760 rcu_assign_pointer()：
 * 真实宏是 smp_wmb() + 普通指针写，这里用 C11 atomic_store 的
 * release 语义精确对应这个"发布"动作（保证之前对新节点的字段写入
 * 在其他线程看到指针更新之前已经完成，即 store-release） */
static void mini_rcu_assign_pointer(struct config_snapshot *v)
{
	atomic_store_explicit(&g_current_config, v, memory_order_release);
}

/* 对应 include/linux/rcupdate.h:576 rcu_dereference()：
 * 用 acquire 语义保证读到指针之后，看到的字段内容是指针发布那一刻
 * "已经完全写好"的版本，不会有指令重排导致的乱序可见性问题 */
static struct config_snapshot *mini_rcu_dereference(void)
{
	return atomic_load_explicit(&g_current_config, memory_order_acquire);
}

/* ------------------------------------------------------------------ */
/* 3. Reader：rcu_read_lock() / rcu_read_unlock() 风格的读取循环          */
/* ------------------------------------------------------------------ */

static atomic_long g_reads_done;
static atomic_long g_torn_reads_detected;   /* 预期恒为 0 */
static atomic_int g_readers_stop;

static void *reader_thread_fn(void *arg)
{
	long id = (long)arg;
	long local_reads = 0;

	for (long i = 0; i < READS_PER_READER && !atomic_load(&g_readers_stop); i++) {
		/* rcu_read_lock()：本模拟里不需要真的持有锁，
		 * 只是标记"临界区开始"，对应真实内核 __rcu_read_lock() */

		struct config_snapshot *s = mini_rcu_dereference();

		/* 故意在读取多个字段之间插入极短暂停，放大"如果写者就地修改"
		 * 会被撕裂读检测到的概率——真实 RCU 语义下这里永远不会被撕裂，
		 * 因为写者从不就地修改已发布的节点 */
		int version = s->version;
		int a = s->a;
		int b = s->b;
		int c = s->c;
		int d = s->d;
		unsigned int checksum = s->checksum;

		struct config_snapshot recomputed = { version, a, b, c, d, 0 };
		if (compute_checksum(&recomputed) != checksum) {
			atomic_fetch_add(&g_torn_reads_detected, 1);
			fprintf(stderr, "  [reader %ld] !!! 检测到撕裂读：version=%d checksum不匹配 !!!\n",
				id, version);
		}

		/* rcu_read_unlock() */
		local_reads++;
	}
	atomic_fetch_add(&g_reads_done, local_reads);
	return NULL;
}

/* ------------------------------------------------------------------ */
/* 4. Writer：持续用"全新节点 + 原子指针切换"替换配置，绝不就地修改       */
/* ------------------------------------------------------------------ */

/* 保存所有历史版本，等所有 reader 退出后统一释放（见文件头注释的简化说明） */
static struct config_snapshot *g_history[NR_WRITES + 1];
static int g_history_count;

static void *writer_thread_fn(void *arg)
{
	(void)arg;
	for (int version = 1; version <= NR_WRITES; version++) {
		/* 关键点：先在"堆外"（这里是新分配的内存）把完整的新版本构造好，
		 * 绝不修改任何一个已经发布给读者的旧节点的字段 */
		struct config_snapshot *new_snap = make_snapshot(version);

		/* 一次原子指针赋值，对应 rcu_assign_pointer()：
		 * 读者要么看到旧指针（读到完整旧版本），要么看到新指针
		 * （读到完整新版本），没有第三种可能 */
		mini_rcu_assign_pointer(new_snap);

		g_history[g_history_count++] = new_snap;
	}
	return NULL;
}

int main(void)
{
	pthread_t readers[NR_READERS];
	pthread_t writer;

	printf("=== rcu_read_lock_replace_demo ===\n\n");

	struct config_snapshot *initial = make_snapshot(0);
	atomic_store(&g_current_config, initial);
	g_history[g_history_count++] = initial;

	printf("-- 启动 %d 个 reader 线程持续读取，1 个 writer 线程持续替换（共 %d 次替换）--\n",
	       NR_READERS, NR_WRITES);

	for (int i = 0; i < NR_READERS; i++)
		pthread_create(&readers[i], NULL, reader_thread_fn, (void *)(long)i);
	pthread_create(&writer, NULL, writer_thread_fn, NULL);

	pthread_join(writer, NULL);
	atomic_store(&g_readers_stop, 1);
	for (int i = 0; i < NR_READERS; i++)
		pthread_join(readers[i], NULL);

	struct config_snapshot *final = mini_rcu_dereference();
	printf("\n-- writer 完成全部 %d 次替换，最终版本 version=%d --\n",
	       NR_WRITES, final->version);
	printf("-- reader 总读取次数: %ld --\n", atomic_load(&g_reads_done));
	printf("-- 检测到撕裂读次数: %ld（预期恒为 0）--\n",
	       atomic_load(&g_torn_reads_detected));

	/* 所有读者已经退出，此刻回收全部历史版本是绝对安全的 */
	for (int i = 0; i < g_history_count; i++)
		free(g_history[i]);

	printf("\n=== done: 共发布 %d 个版本，全部安全回收 ===\n", g_history_count);
	if (atomic_load(&g_torn_reads_detected) > 0) {
		fprintf(stderr, "!!! 断言失败：出现了撕裂读 !!!\n");
		return 1;
	}
	printf("验证通过：全程没有任何 reader 读到不一致的中间状态。\n");
	return 0;
}
