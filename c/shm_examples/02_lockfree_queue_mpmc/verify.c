/*
 * verify.c - 校验完整性并清理共享内存
 *
 * 用法: ./verify <num_producers> <count_per_producer>
 *
 * 校验三件事：
 *   1. total_received 是否等于 num_producers * count_per_producer（无丢失）
 *   2. duplicate_count 是否为 0（无重复消费）
 *   3. 每个 producer 的 [0, count_per_producer) 范围内的 bitmap 是否全部置位
 *      （没有漏消费的空洞）
 */
#include <stdio.h>
#include <stdlib.h>
#include "mpmc_queue.h"

int
main(int argc, char *argv[])
{
	struct mpmc_queue *q;
	uint32_t num_producers;
	uint64_t count_per_producer;
	uint64_t expected_total, actual_total;
	uint32_t duplicates;
	uint64_t missing = 0;
	uint32_t pid;
	int ok;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <num_producers> <count_per_producer>\n",
			argv[0]);
		return 1;
	}

	num_producers = (uint32_t)strtoul(argv[1], NULL, 10);
	count_per_producer = strtoull(argv[2], NULL, 10);
	expected_total = (uint64_t)num_producers * count_per_producer;

	q = queue_attach();

	actual_total = atomic_load_explicit(&q->total_received,
		memory_order_relaxed);
	duplicates = atomic_load_explicit(&q->duplicate_count,
		memory_order_relaxed);

	for (pid = 0; pid < num_producers; pid++) {
		uint64_t seq;

		for (seq = 0; seq < count_per_producer; seq++) {
			if (!queue_bitmap_test(q, pid, seq))
				missing++;
		}
	}

	ok = (actual_total == expected_total) && (duplicates == 0) &&
		(missing == 0);

	printf("[verify] expected_total=%lu actual_total=%lu "
		"duplicates=%u missing=%lu result=%s\n",
		(unsigned long)expected_total, (unsigned long)actual_total,
		duplicates, (unsigned long)missing, ok ? "OK" : "FAILED");

	shm_unmap(q, sizeof(*q));
	shm_destroy(QUEUE_SHM_NAME);

	return ok ? 0 : 1;
}
