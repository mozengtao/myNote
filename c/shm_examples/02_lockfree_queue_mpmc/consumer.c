/*
 * consumer.c - MPMC 队列消费者
 *
 * 用法: ./consumer <consumer_id>
 *
 * 收到哨兵值即退出。用共享 bitmap 记录"某 producer 的某个 seq
 * 是否已被消费"：
 *   - 首次置位 -> 正常
 *   - 重复置位 -> 说明同一个元素被消费了两次（bug），计入 duplicate_count
 *
 * 注意：MPMC 队列只保证元素"恰好被消费一次"，不保证多个并发消费者
 * 完成业务处理的时间顺序与生产顺序一致，因此这里不检查到达顺序。
 */
#include <stdio.h>
#include <stdlib.h>
#include "mpmc_queue.h"

int
main(int argc, char *argv[])
{
	struct mpmc_queue *q;
	uint32_t consumer_id;
	uint64_t local_count = 0;

	consumer_id = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 0;

	q = queue_attach();

	for (;;) {
		uint64_t data;

		if (!queue_dequeue(q, &data))
			continue;	/* 队列暂时空，忙等重试 */

		if (data == SENTINEL_DATA)
			break;

		{
			uint32_t pid = queue_unpack_pid(data);
			uint64_t seq = queue_unpack_seq(data);

			if (pid >= MAX_PRODUCERS ||
				seq >= MAX_ITEMS_PER_PRODUCER) {
				fprintf(stderr,
					"[consumer %u] bad item pid=%u seq=%lu\n",
					consumer_id, pid, (unsigned long)seq);
				continue;
			}

			if (!queue_bitmap_mark(q, pid, seq)) {
				fprintf(stderr,
					"[consumer %u] DUPLICATE pid=%u seq=%lu\n",
					consumer_id, pid, (unsigned long)seq);
				atomic_fetch_add_explicit(&q->duplicate_count,
					1, memory_order_relaxed);
			}
		}

		atomic_fetch_add_explicit(&q->total_received, 1,
			memory_order_relaxed);
		local_count++;
	}

	printf("[consumer %u] done, received %lu items\n",
		consumer_id, (unsigned long)local_count);

	shm_unmap(q, sizeof(*q));

	return 0;
}
