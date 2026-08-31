/*
 * producer.c - MPMC 队列生产者
 *
 * 用法: ./producer <producer_id> <count>
 *
 * 最后一个完成的生产者（active_producers 减到 0 的那个）负责
 * 推入 num_consumers 个哨兵，通知所有消费者退出。
 */
#include <stdio.h>
#include <stdlib.h>
#include "mpmc_queue.h"

int
main(int argc, char *argv[])
{
	struct mpmc_queue *q;
	uint32_t producer_id;
	uint64_t count, i;
	uint32_t remaining;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <producer_id> <count>\n", argv[0]);
		return 1;
	}

	producer_id = (uint32_t)strtoul(argv[1], NULL, 10);
	count = strtoull(argv[2], NULL, 10);

	if (producer_id >= MAX_PRODUCERS || count > MAX_ITEMS_PER_PRODUCER) {
		fprintf(stderr,
			"producer_id must be < %u, count must be <= %u\n",
			MAX_PRODUCERS, MAX_ITEMS_PER_PRODUCER);
		return 1;
	}

	q = queue_attach();

	for (i = 0; i < count; i++) {
		while (!queue_enqueue(q, queue_pack(producer_id, i)))
			;	/* 队列满，忙等重试 */
	}

	printf("[producer %u] pushed %lu items\n", producer_id,
		(unsigned long)count);

	remaining = atomic_fetch_sub_explicit(&q->active_producers, 1,
		memory_order_acq_rel);

	if (remaining == 1) {
		/* 我是最后一个完成的生产者，推入哨兵通知所有消费者 */
		uint32_t j;

		printf("[producer %u] last producer, pushing %u sentinels\n",
			producer_id, q->num_consumers);

		for (j = 0; j < q->num_consumers; j++) {
			while (!queue_enqueue(q, SENTINEL_DATA))
				;
		}
	}

	shm_unmap(q, sizeof(*q));

	return 0;
}
