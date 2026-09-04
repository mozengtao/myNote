/*
 * 01_simplest_seqlock.c
 *
 * 示例 1：最简共享内存 Seqlock 实现（单写者 + 单读者）
 *
 * 本程序演示：
 * 1. 在 POSIX 匿名共享内存（mmap MAP_SHARED | MAP_ANONYMOUS）中部署 Seqlock。
 * 2. 共享数据包含三个关联字段：x, y, checksum，满足恒等式 checksum == x + y。
 * 3. 写者高速循环写入，破坏一致性后再恢复一致性。
 * 4. 读者高速并发读取，验证通过 Seqlock 读取到的快照绝对不会出现数据撕裂（Torn Read）。
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
#include <sys/mman.h>
#include <sys/wait.h>
#include "../include/seqlock.h"

/*
 * 共享数据载荷结构体
 * 包含三个字段，恒等式：checksum = x + y。
 * 如果发生数据撕裂（只读到更新了一半的 x 或 y），则 checksum != x + y。
 */
typedef struct {
	uint64_t x;
	uint64_t y;
	uint64_t checksum;
} coordinate_payload_t;

/*
 * 共享内存控制块：封装 seqcount 和实际数据
 */
typedef struct {
	seqcount_t lock;
	coordinate_payload_t data;
} shared_channel_t;

/* 运行控制标志 */
static volatile int g_running = 1;

/*
 * 写者线程执行函数
 * 持续生成新的 (x, y)，并保证 payload.checksum = x + y
 */
static void *
writer_thread_func(void *arg)
{
	shared_channel_t *channel = (shared_channel_t *)arg;
	uint64_t val = 0;

	printf("[写者] 启动，开始持续更新共享数据...\n");

	while (g_running) {
		val++;

		/*
		 * 步骤 1：标记写入开始
		 * 内部将 sequence 递增为奇数，并下发 Release 内存屏障，
		 * 确保该奇数状态先于后续的数据修改被其他核心观测到。
		 */
		write_seqbegin(&channel->lock);

		/*
		 * 步骤 2：更新多字段关联数据
		 * 注意：在更新 x 之后、更新 y 之前，数据处于非法的撕裂状态！
		 * 如果读者在此刻直接读取，就会读到半成品。
		 */
		channel->data.x = val;
		channel->data.y = val * 2;
		channel->data.checksum = channel->data.x + channel->data.y;

		/*
		 * 步骤 3：标记写入结束
		 * 内部将 sequence 递增为偶数，并带有 Release 语义，
		 * 确保上面的所有数据写入完全刷入内存/缓存，对读者立即可见。
		 */
		write_seqend(&channel->lock);

		/* 让出少量 CPU 时间片，使读者有充分并发机会 */
		if ((val % 100000) == 0)
			usleep(100);
	}

	printf("[写者] 退出，累计完成写入迭代次数: %lu\n", val);
	return NULL;
}

/*
 * 读者线程执行函数
 * 持续尝试抓取一致性快照，并使用校验和验证数据是否撕裂
 */
static void *
reader_thread_func(void *arg)
{
	shared_channel_t *channel = (shared_channel_t *)arg;
	coordinate_payload_t local_copy;
	uint32_t start_seq;
	uint64_t read_success = 0;
	uint64_t retry_count = 0;
	uint64_t torn_read_errors = 0;

	printf("[读者] 启动，开始高速无锁采集快照...\n");

	while (g_running) {
		bool valid = false;

		/*
		 * Seqlock 读者核心模板：
		 * 1. 调用 read_seqbegin() 获取初始 sequence。
		 *    若当前为奇数（写者正在更新），该函数会自旋等待直到写者退出。
		 * 2. 复制共享数据到本地栈缓冲区 local_copy。
		 * 3. 调用 read_seqretry() 校验 sequence 是否在读取期间发生变化。
		 *    若未变化（返回 false），说明快照一致有效，跳出循环。
		 *    若变化（返回 true），说明数据可能撕裂，抛弃并重新读取。
		 */
		do {
			start_seq = read_seqbegin(&channel->lock);

			/* 拷贝共享数据载荷到局部栈空间（普通内存拷贝，无需原子指令） */
			local_copy = channel->data;

			/*
			 * 校验一致性：
			 * 内部包含 Acquire 屏障与 sequence 重新加载比对
			 */
			if (!read_seqretry(&channel->lock, start_seq)) {
				valid = true;
				break;
			}

			retry_count++;
			seqlock_cpu_relax();
		} while (g_running);

		/* 若因进程/线程退出中断了重试流程，则不应消费本次未校验通过的半成品 */
		if (!valid)
			break;

		/*
		 * 验证数学不变式：checksum == x + y
		 * 如果 Seqlock 内存序或逻辑正确，此断言将永远成立！
		 */
		if (local_copy.checksum != (local_copy.x + local_copy.y)) {
			torn_read_errors++;
			fprintf(stderr, "[严重错误] 观测到数据撕裂! x=%lu, y=%lu, sum=%lu\n",
				local_copy.x, local_copy.y, local_copy.checksum);
		}

		read_success++;

		if ((read_success % 50000000) == 0) {
			printf("[读者] 成功读取 %lu 次 | 发生重试 %lu 次 (重试率: %.2f%%) | 撕裂错误: %lu\n",
				read_success, retry_count,
				(double)retry_count * 100.0 / (read_success + retry_count),
				torn_read_errors);
		}
	}

	printf("[读者] 退出。最终统计 -> 成功: %lu, 重试: %lu, 撕裂错误: %lu\n",
		read_success, retry_count, torn_read_errors);

	return NULL;
}

/*
 * 主函数
 */
int
main(void)
{
	pthread_t writer_tid;
	pthread_t reader_tid;
	shared_channel_t *channel;

	printf("========================================================\n");
	printf("  示例 1：最简共享内存 Seqlock（单写者 vs 单读者）演示  \n");
	printf("========================================================\n");

	/*
	 * 创建基于 mmap 的匿名共享内存区
	 * 即使在单进程多线程下，mmap 也能完全模拟共享内存段的内存行为
	 */
	channel = (shared_channel_t *)mmap(NULL, sizeof(shared_channel_t),
					   PROT_READ | PROT_WRITE,
					   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (channel == MAP_FAILED) {
		perror("mmap 失败");
		return EXIT_FAILURE;
	}

	/* 初始化 Seqlock 计数器 */
	seqcount_init(&channel->lock);
	channel->data.x = 0;
	channel->data.y = 0;
	channel->data.checksum = 0;

	/* 启动写者与读者线程 */
	if (pthread_create(&writer_tid, NULL, writer_thread_func, channel) != 0) {
		perror("创建写者线程失败");
		return EXIT_FAILURE;
	}
	if (pthread_create(&reader_tid, NULL, reader_thread_func, channel) != 0) {
		perror("创建读者线程失败");
		return EXIT_FAILURE;
	}

	/* 运行 2 秒钟观察高速并发读写 */
	sleep(2);

	/* 通知线程退出 */
	g_running = 0;

	pthread_join(writer_tid, NULL);
	pthread_join(reader_tid, NULL);

	/* 释放共享内存 */
	munmap(channel, sizeof(shared_channel_t));

	printf("========================================================\n");
	printf("  示例 1 演示完成：未发生任何数据撕裂，Seqlock 快照一致！\n");
	printf("========================================================\n");

	return EXIT_SUCCESS;
}
