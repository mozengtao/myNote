/*
 * worker.c - 反复 alloc -> 使用 -> free 一个共享对象池中的对象
 *
 * 用法: ./worker <worker_id> <iterations>
 *
 * 用 slots[idx].in_use 做"重复分配"检测：正常情况下，alloc 到的
 * 下标一定是当前没有被任何进程持有的，如果检测到 in_use 已经是 1，
 * 说明 Free List 实现有 bug（两个进程同时拿到了同一个对象）。
 */
#include <stdio.h>
#include <stdlib.h>
#include "object_pool.h"

int
main(int argc, char *argv[])
{
	struct object_pool *pool;
	uint32_t worker_id;
	uint64_t iterations, i;
	uint64_t exhausted_local = 0;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <worker_id> <iterations>\n",
			argv[0]);
		return 1;
	}

	worker_id = (uint32_t)strtoul(argv[1], NULL, 10);
	iterations = strtoull(argv[2], NULL, 10);

	pool = pool_attach();

	for (i = 0; i < iterations; i++) {
		uint32_t idx;
		uint32_t was_in_use;

		do {
			idx = pool_alloc(pool);
			if (idx == EMPTY_IDX) {
				exhausted_local++;
				atomic_fetch_add_explicit(
					&pool->pool_exhausted_events, 1,
					memory_order_relaxed);
			}
		} while (idx == EMPTY_IDX);

		was_in_use = atomic_exchange_explicit(
			&pool->slots[idx].in_use, 1, memory_order_acq_rel);
		if (was_in_use != 0) {
			fprintf(stderr,
				"[worker %u] BUG: double-alloc idx=%u\n",
				worker_id, idx);
			atomic_fetch_add_explicit(&pool->double_alloc_bugs,
				1, memory_order_relaxed);
		}

		/* 模拟"使用对象" */
		pool->slots[idx].payload = worker_id * 1000000ull + i;

		was_in_use = atomic_exchange_explicit(
			&pool->slots[idx].in_use, 0, memory_order_acq_rel);
		if (was_in_use != 1) {
			fprintf(stderr,
				"[worker %u] BUG: free-without-alloc idx=%u\n",
				worker_id, idx);
			atomic_fetch_add_explicit(&pool->double_alloc_bugs,
				1, memory_order_relaxed);
		}

		pool_free(pool, idx);
	}

	printf("[worker %u] done, iterations=%lu pool_exhausted_hits=%lu\n",
		worker_id, (unsigned long)iterations,
		(unsigned long)exhausted_local);

	shm_unmap(pool, sizeof(*pool));

	return 0;
}
