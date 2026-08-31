/*
 * writer.c - 往共享哈希表插入一段独占的 key 区间
 *
 * 用法: ./writer <writer_id> <count>
 * 插入 key = writer_id*count .. writer_id*count+count-1, value = key*2
 */
#include <stdio.h>
#include <stdlib.h>
#include "hash_table.h"

int
main(int argc, char *argv[])
{
	struct shared_hash_table *ht;
	uint32_t writer_id;
	uint64_t count, i;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <writer_id> <count>\n", argv[0]);
		return 1;
	}

	writer_id = (uint32_t)strtoul(argv[1], NULL, 10);
	count = strtoull(argv[2], NULL, 10);

	ht = ht_attach();

	for (i = 0; i < count; i++) {
		uint64_t key = writer_id * count + i;

		ht_insert(ht, key, key * 2);
	}

	printf("[writer %u] inserted %lu keys\n", writer_id,
		(unsigned long)count);

	shm_unmap(ht, sizeof(*ht));

	return 0;
}
