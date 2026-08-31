/*
 * init.c - 创建并初始化共享哈希表
 */
#include <stdio.h>
#include "hash_table.h"

int
main(void)
{
	struct shared_hash_table *ht;

	ht = ht_create();

	printf("[init] hash table created, buckets=%u max_nodes=%u\n",
		NUM_BUCKETS, MAX_NODES);

	shm_unmap(ht, sizeof(*ht));

	return 0;
}
