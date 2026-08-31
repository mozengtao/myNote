/*
 * init.c - 创建并初始化共享对象池
 */
#include <stdio.h>
#include "object_pool.h"

int
main(void)
{
	struct object_pool *pool;

	pool = pool_create();

	printf("[init] object pool created, capacity=%u\n", pool->capacity);

	shm_unmap(pool, sizeof(*pool));

	return 0;
}
