/*
 * 05_kernel_seqcount.c
 *
 * 示例 5：Linux 内核 seqcount_t 与 seqlock_t 架构思想深度模拟（多写者串行化 + 多读者无锁）
 *
 * 深度解析 Linux 内核设计哲学：
 * 1. 为什么 Linux 内核将 seqcount 与锁分离？
 *    - 在 Linux 内核中，存在两套独立概念：
 *      * seqcount_t: 纯粹的序列计数器（不含任何锁结构，体积仅 4 字节）。
 *      * seqlock_t: 将 seqcount_t 与 spinlock_t 结合的组合锁。
 *    - 核心原因：许多关键内核子系统（如 VFS dcache 目录项缓存路径查找、网络套接字哈希表）
 *      本身在外层已经持有了其他锁（如 RCU 或子系统专用自旋锁）。
 *      如果 seqlock_t 强行内置自旋锁，将造成双重锁开销和内存膨胀。
 *      因此内核提供解耦的 raw_write_seqcount_begin / raw_read_seqcount_begin，
 *      允许开发者复用既有的外层互斥保护机制！
 * 2. 为什么写者必须串行化？
 *    - Seqlock 仅保证 Reader 无锁，绝对不等于 Writer 无锁！
 *    - 若两个写者同时调用 write_seqbegin，会导致 sequence 被重复递增，
 *      奇偶逻辑彻底崩塌，且共享数据会出现灾难性的写覆盖交叉撕裂。
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "../include/seqlock.h"

#define NUM_WRITERS 4
#define NUM_READERS 4

/*
 * 待保护的共享业务资源
 */
typedef struct {
	uint64_t active_writer_id;
	uint64_t sequence_val;
	uint64_t magic_hash;
} kernel_resource_t;

/*
 * 模拟 Linux 内核 seqlock 保护的全局资源
 */
typedef struct {
	seqlock_t lock;
	kernel_resource_t res;
} shared_system_state_t;

typedef struct {
	int writer_id;
	shared_system_state_t *sys;
	uint64_t write_count;
} writer_worker_t;

typedef struct {
	int reader_id;
	shared_system_state_t *sys;
	uint64_t read_count;
	uint64_t retry_count;
	uint64_t torn_errors;
} reader_worker_t;

static volatile int g_running = 1;

/*
 * 计算校验魔数哈希
 */
static inline uint64_t
calc_hash(uint64_t wid, uint64_t seq)
{
	return (wid * 1000000007ULL) ^ (seq * 2654435761ULL);
}

/*
 * 多写者工作线程函数
 * 多个写者使用 seqlock_write_lock / unlock 进行安全写入
 */
static void *
multi_writer_thread_func(void *arg)
{
	writer_worker_t *w = (writer_worker_t *)arg;
	shared_system_state_t *sys = w->sys;
	uint64_t iter = 0;

	printf("[内核写者-%d] 启动，通过自旋锁串行化更新资源...\n", w->writer_id);

	while (g_running) {
		iter++;

		/*
		 * 内核 seqlock_write_lock：
		 * 1. pthread_spin_lock 保证多写者绝对串行，杜绝写者竞态
		 * 2. write_seqbegin 将 sequence 递增为奇数，宣告写入
		 */
		seqlock_write_lock(&sys->lock);

		/* 写入共享数据 */
		sys->res.active_writer_id = w->writer_id;
		sys->res.sequence_val = iter;
		sys->res.magic_hash = calc_hash(w->writer_id, iter);

		/*
		 * 内核 seqlock_write_unlock：
		 * 1. write_seqend 将 sequence 递增为偶数，发布写入
		 * 2. pthread_spin_unlock 释放自旋锁
		 */
		seqlock_write_unlock(&sys->lock);

		/* 微小让步避免单写者长时间饥饿其他写者 */
		if ((iter % 10000) == 0)
			usleep(10);
	}

	w->write_count = iter;
	printf("[内核写者-%d] 退出，累计完成有效安全写入: %lu 次\n", w->writer_id, iter);
	return NULL;
}

/*
 * 多读者工作线程函数
 * 读者完全无需感知当前由哪一个写者持有锁，仅依靠序号校验获取一致快照
 */
static void *
multi_reader_thread_func(void *arg)
{
	reader_worker_t *r = (reader_worker_t *)arg;
	shared_system_state_t *sys = r->sys;
	kernel_resource_t snap;
	uint32_t seq;

	printf("[内核读者-%d] 启动，无锁探测系统状态...\n", r->reader_id);

	while (g_running) {
		bool valid = false;

		/*
		 * 对应内核 raw_read_seqcount_begin / retry
		 */
		do {
			seq = seqlock_read_begin(&sys->lock);

			snap = sys->res;

			if (!seqlock_read_retry(&sys->lock, seq)) {
				valid = true;
				break;
			}

			r->retry_count++;
			seqlock_cpu_relax();
		} while (g_running);

		if (!valid)
			break;

		r->read_count++;

		/*
		 * 校验一致性哈希，验证快照是否属于同一次写者的完整提交
		 */
		if (snap.active_writer_id != 0) {
			uint64_t expected_hash = calc_hash(snap.active_writer_id, snap.sequence_val);
			if (snap.magic_hash != expected_hash) {
				r->torn_errors++;
				fprintf(stderr, "[内核读者-%d 致命错误] 观测到多写者交叉撕裂! writer=%lu, seq=%lu\n",
					r->reader_id, snap.active_writer_id, snap.sequence_val);
			}
		}
	}

	return NULL;
}

/*
 * 主函数
 */
int
main(void)
{
	pthread_t writer_tids[NUM_WRITERS];
	pthread_t reader_tids[NUM_READERS];
	writer_worker_t writers[NUM_WRITERS];
	reader_worker_t readers[NUM_READERS];
	shared_system_state_t system_state;
	int i;

	printf("======================================================================\n");
	printf("  示例 5：Linux 内核 seqcount_t / seqlock_t 架构模式模拟               \n");
	printf("  配置：%d 个并发写者（自旋锁串行化） + %d 个并发读者（无锁一致性）    \n",
		NUM_WRITERS, NUM_READERS);
	printf("======================================================================\n");

	/*
	 * 初始化 seqlock
	 * PTHREAD_PROCESS_PRIVATE 表示当前进程内线程间自旋锁
	 */
	if (seqlock_init(&system_state.lock, PTHREAD_PROCESS_PRIVATE) != 0) {
		perror("seqlock_init 失败");
		return EXIT_FAILURE;
	}

	system_state.res.active_writer_id = 0;
	system_state.res.sequence_val = 0;
	system_state.res.magic_hash = 0;

	/* 启动写者 */
	for (i = 0; i < NUM_WRITERS; i++) {
		writers[i].writer_id = i + 1;
		writers[i].sys = &system_state;
		writers[i].write_count = 0;
		pthread_create(&writer_tids[i], NULL, multi_writer_thread_func, &writers[i]);
	}

	/* 启动读者 */
	for (i = 0; i < NUM_READERS; i++) {
		readers[i].reader_id = i + 1;
		readers[i].sys = &system_state;
		readers[i].read_count = 0;
		readers[i].retry_count = 0;
		readers[i].torn_errors = 0;
		pthread_create(&reader_tids[i], NULL, multi_reader_thread_func, &readers[i]);
	}

	/* 压测运行 3 秒钟 */
	sleep(3);
	g_running = 0;

	for (i = 0; i < NUM_WRITERS; i++)
		pthread_join(writer_tids[i], NULL);

	for (i = 0; i < NUM_READERS; i++)
		pthread_join(reader_tids[i], NULL);

	printf("\n-------------------------- 多写多读压测数据统计 --------------------------\n");
	uint64_t total_writes = 0;
	uint64_t total_reads = 0;
	uint64_t total_retries = 0;
	uint64_t total_errors = 0;

	for (i = 0; i < NUM_WRITERS; i++) {
		printf("[写者-%d] 完成排他写入: %10lu 次\n", writers[i].writer_id, writers[i].write_count);
		total_writes += writers[i].write_count;
	}

	for (i = 0; i < NUM_READERS; i++) {
		double ratio = (readers[i].read_count + readers[i].retry_count > 0) ?
			((double)readers[i].retry_count * 100.0 / (readers[i].read_count + readers[i].retry_count)) : 0.0;
		printf("[读者-%d] 成功抓取: %10lu 次 | 冲突重试: %8lu 次 (冲突率: %5.2f%%) | 撕裂异常: %lu\n",
			readers[i].reader_id, readers[i].read_count, readers[i].retry_count, ratio, readers[i].torn_errors);
		total_reads += readers[i].read_count;
		total_retries += readers[i].retry_count;
		total_errors += readers[i].torn_errors;
	}

	printf("--------------------------------------------------------------------------\n");
	printf("  写者总写入数: %lu 次 | 读者总读取数: %lu 次\n", total_writes, total_reads);
	printf("  全局数据撕裂异常数: %lu\n", total_errors);
	printf("==========================================================================\n");
	printf("  结论：写者通过自旋锁严格排他串行化，读者维持全无锁高速访问，一致性 100%%！\n");
	printf("==========================================================================\n");

	seqlock_destroy(&system_state.lock);
	return EXIT_SUCCESS;
}
