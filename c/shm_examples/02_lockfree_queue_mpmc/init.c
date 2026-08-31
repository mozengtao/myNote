/*
 * init.c - 创建并初始化 MPMC 队列的共享内存
 *
 * 用法: ./init <num_producers> <num_consumers>
 *
 * 心智模型：先由一个"协调者"进程创建并初始化共享内存结构，
 * 各 worker 进程随后 attach，避免多个 worker 争相创建/初始化的竞态。
 */
#include <stdio.h>
#include <stdlib.h>
#include "mpmc_queue.h"

int
main(int argc, char *argv[])
{
	struct mpmc_queue *q;
	uint32_t num_producers;
	uint32_t num_consumers;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <num_producers> <num_consumers>\n",
			argv[0]);
		return 1;
	}

	num_producers = (uint32_t)strtoul(argv[1], NULL, 10);
	num_consumers = (uint32_t)strtoul(argv[2], NULL, 10);

	if (num_producers == 0 || num_producers > MAX_PRODUCERS) {
		fprintf(stderr, "num_producers must be in [1, %u]\n",
			MAX_PRODUCERS);
		return 1;
	}

	q = queue_create(num_producers, num_consumers);

	printf("[init] queue created: capacity=%u producers=%u consumers=%u\n",
		QUEUE_CAPACITY, num_producers, num_consumers);

	shm_unmap(q, sizeof(*q));

	return 0;
}
