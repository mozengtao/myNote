/*
 * verify.c - 校验对象池计数并清理共享内存
 *
 * 用法: ./verify <expected_total_ops>
 */
#include <stdio.h>
#include <stdlib.h>
#include "object_pool.h"

int
main(int argc, char *argv[])
{
	struct object_pool *pool;
	uint64_t expected;
	uint64_t total_alloc, total_free;
	uint32_t bugs, exhausted;
	int ok;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <expected_total_ops>\n", argv[0]);
		return 1;
	}

	expected = strtoull(argv[1], NULL, 10);

	pool = pool_attach();

	total_alloc = atomic_load_explicit(&pool->total_alloc,
		memory_order_relaxed);
	total_free = atomic_load_explicit(&pool->total_free,
		memory_order_relaxed);
	bugs = atomic_load_explicit(&pool->double_alloc_bugs,
		memory_order_relaxed);
	exhausted = atomic_load_explicit(&pool->pool_exhausted_events,
		memory_order_relaxed);

	ok = (total_alloc == expected) && (total_alloc == total_free) &&
		(bugs == 0);

	printf("[verify] expected_ops=%lu total_alloc=%lu total_free=%lu "
		"double_alloc_bugs=%u pool_exhausted_events=%u result=%s\n",
		(unsigned long)expected, (unsigned long)total_alloc,
		(unsigned long)total_free, bugs, exhausted,
		ok ? "OK" : "FAILED");

	shm_unmap(pool, sizeof(*pool));
	shm_destroy(POOL_SHM_NAME);

	return ok ? 0 : 1;
}
