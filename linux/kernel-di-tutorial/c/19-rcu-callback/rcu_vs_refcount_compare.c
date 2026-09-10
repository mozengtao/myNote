/*
 * rcu_vs_refcount_compare.c
 *
 * 场景：同一个"多读者、偶尔写者"场景，分别用
 *   (a) 传统读写锁方案（pthread_rwlock_t，读者也要加锁/解锁）
 *   (b) RCU 风格方案（读者完全无锁，只做一次原子指针读）
 * 实现，测量两者在高并发读场景下的真实吞吐量差异。
 *
 * 对照真实设计动机（本仓库 /home/morrism/linux/linux）：
 *   include/linux/rcupdate.h:82   注释："RCU read-side critical sections
 *                                  may be nested" —— 读者完全不需要和写者
 *                                  竞争任何锁，这是 RCU 相对读写锁最核心的优势
 *   include/linux/rcupdate.h:644  "So where is rcu_write_lock()? It does
 *                                  not exist ... this property is what
 *                                  provides RCU's performance benefits."
 *   net/core/dev.c:3287           rcu_dereference() 在收包热路径上被高频调用，
 *                                  如果这里要加锁，网络吞吐会明显下降
 *
 * 度量方法：
 *   固定时长内，让 N 个 reader 线程尽可能快地反复"读取当前共享数据"，
 *   同时有 1 个 writer 线程持续做更新；统计 reader 线程在这段时间内
 *   总共完成了多少次读取，换算成"每秒读取次数"，两种方案分别测一次。
 *   这是真实测量得到的数字，不是理论估算。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o rcu_vs_refcount_compare rcu_vs_refcount_compare.c
 * 运行：
 *   ./rcu_vs_refcount_compare
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NR_READER_THREADS 8
#define BENCHMARK_SECONDS  1

struct shared_data {
	long value;
	long generation;
};

/* ------------------------------------------------------------------ */
/* 方案 (a)：传统读写锁 —— 读者也必须持锁，与写者互相竞争                */
/* ------------------------------------------------------------------ */

static pthread_rwlock_t g_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static struct shared_data *g_rwlock_protected_ptr;

/* 用一个全局 stop flag 来统一控制两种 benchmark 的结束 */
static atomic_int g_stop_flag;

static void *rwlock_reader_fn(void *arg)
{
	(void)arg;
	long local = 0;
	for (;;) {
		pthread_rwlock_rdlock(&g_rwlock);
		volatile long v = g_rwlock_protected_ptr->value;
		(void)v;
		pthread_rwlock_unlock(&g_rwlock);
		local++;
		if ((local & 0xFFF) == 0 && atomic_load(&g_stop_flag))
			break;
	}
	return (void *)local;
}

/* ------------------------------------------------------------------ */
/* 方案 (b)：RCU 风格 —— 读者完全无锁，只做一次原子指针读                */
/* ------------------------------------------------------------------ */

static _Atomic(struct shared_data *) g_rcu_ptr;

static void *rcu_reader_fn(void *arg)
{
	(void)arg;
	long local = 0;
	for (;;) {
		struct shared_data *p = atomic_load_explicit(&g_rcu_ptr, memory_order_acquire);
		volatile long v = p->value;
		(void)v;
		local++;
		if ((local & 0xFFF) == 0 && atomic_load(&g_stop_flag))
			break;
	}
	return (void *)local;
}

static void *rwlock_writer_fn(void *arg)
{
	(void)arg;
	long gen = 0;
	while (!atomic_load(&g_stop_flag)) {
		struct shared_data *new_data = malloc(sizeof(*new_data));
		new_data->generation = ++gen;
		new_data->value = gen * 10;

		pthread_rwlock_wrlock(&g_rwlock);
		struct shared_data *old = g_rwlock_protected_ptr;
		g_rwlock_protected_ptr = new_data;
		pthread_rwlock_unlock(&g_rwlock);

		free(old);   /* 传统方案：写者持锁期间没有 reader 能看到 old，解锁后即可直接释放 */
		usleep(200);
	}
	return NULL;
}

static void *rcu_writer_fn(void *arg)
{
	(void)arg;
	long gen = 0;
	/* RCU 方案为了公平对比"读吞吐"，本示例里写者不做真正的 grace-period
	 * 延迟释放（那部分机制已经在 call_rcu_grace_period_sim.c 里单独演示过），
	 * 而是简单地"永不释放旧版本、benchmark 结束后统一清理"，这对本文件
	 * 要测量的"读吞吐"这个指标没有影响（读者从不访问被释放的内存）。 */
	while (!atomic_load(&g_stop_flag)) {
		struct shared_data *new_data = malloc(sizeof(*new_data));
		new_data->generation = ++gen;
		new_data->value = gen * 10;
		atomic_store_explicit(&g_rcu_ptr, new_data, memory_order_release);
		usleep(200);
	}
	return NULL;
}

static double run_rwlock_benchmark(void)
{
	pthread_t readers[NR_READER_THREADS];
	pthread_t writer;
	void *results[NR_READER_THREADS];

	g_rwlock_protected_ptr = malloc(sizeof(*g_rwlock_protected_ptr));
	g_rwlock_protected_ptr->value = 0;
	g_rwlock_protected_ptr->generation = 0;
	atomic_store(&g_stop_flag, 0);

	for (int i = 0; i < NR_READER_THREADS; i++)
		pthread_create(&readers[i], NULL, rwlock_reader_fn, NULL);
	pthread_create(&writer, NULL, rwlock_writer_fn, NULL);

	sleep(BENCHMARK_SECONDS);
	atomic_store(&g_stop_flag, 1);

	long total = 0;
	for (int i = 0; i < NR_READER_THREADS; i++) {
		pthread_join(readers[i], &results[i]);
		total += (long)results[i];
	}
	pthread_join(writer, NULL);
	free(g_rwlock_protected_ptr);

	return (double)total / BENCHMARK_SECONDS;
}

static double run_rcu_benchmark(void)
{
	pthread_t readers[NR_READER_THREADS];
	pthread_t writer;
	void *results[NR_READER_THREADS];

	struct shared_data *init = malloc(sizeof(*init));
	init->value = 0;
	init->generation = 0;
	atomic_store(&g_rcu_ptr, init);
	atomic_store(&g_stop_flag, 0);

	for (int i = 0; i < NR_READER_THREADS; i++)
		pthread_create(&readers[i], NULL, rcu_reader_fn, NULL);
	pthread_create(&writer, NULL, rcu_writer_fn, NULL);

	sleep(BENCHMARK_SECONDS);
	atomic_store(&g_stop_flag, 1);

	long total = 0;
	for (int i = 0; i < NR_READER_THREADS; i++) {
		pthread_join(readers[i], &results[i]);
		total += (long)results[i];
	}
	pthread_join(writer, NULL);
	/* benchmark 结束后没有并发读者，统一回收当前指针指向的最后一份数据；
	 * 中间版本按设计故意不追踪/不释放（见 rcu_writer_fn 里的说明），
	 * 这是本示例"只测读吞吐、不测内存管理"的合理简化，不影响吞吐量的度量结果 */
	free(atomic_load(&g_rcu_ptr));

	return (double)total / BENCHMARK_SECONDS;
}

int main(void)
{
	printf("=== rcu_vs_refcount_compare ===\n");
	printf("(reader 线程数=%d, 每种方案测量 %d 秒)\n\n", NR_READER_THREADS, BENCHMARK_SECONDS);

	printf("-- 方案 (a)：pthread_rwlock 读写锁，读者也要 rdlock/rdunlock --\n");
	double rwlock_ops_per_sec = run_rwlock_benchmark();
	printf("  总读取吞吐: %.0f 次/秒\n\n", rwlock_ops_per_sec);

	printf("-- 方案 (b)：RCU 风格，读者只做一次原子指针读，完全无锁 --\n");
	double rcu_ops_per_sec = run_rcu_benchmark();
	printf("  总读取吞吐: %.0f 次/秒\n\n", rcu_ops_per_sec);

	printf("=== 对比结果 ===\n");
	printf("  rwlock 方案: %.0f 次/秒\n", rwlock_ops_per_sec);
	printf("  RCU    方案: %.0f 次/秒\n", rcu_ops_per_sec);
	if (rwlock_ops_per_sec > 0) {
		printf("  RCU 方案吞吐倍数: %.2fx\n", rcu_ops_per_sec / rwlock_ops_per_sec);
	}
	printf("\n(实测数字受运行环境 CPU 核数/负载影响会有波动，"
	       "但 RCU 方案读吞吐显著更高这一趋势应当稳定重现，"
	       "因为读者完全不需要和写者/其它读者竞争任何锁。)\n");
	return 0;
}
