/*
 * deleter.c - 删除区间内 key % 3 == 0 的条目
 *
 * 用法: ./deleter <start_key> <end_key_exclusive>
 */
#include <stdio.h>
#include <stdlib.h>
#include "hash_table.h"

int
main(int argc, char *argv[])
{
	struct shared_hash_table *ht;
	uint64_t start_key, end_key, key;
	uint64_t deleted = 0;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <start_key> <end_key_exclusive>\n",
			argv[0]);
		return 1;
	}

	start_key = strtoull(argv[1], NULL, 10);
	end_key = strtoull(argv[2], NULL, 10);

	ht = ht_attach();

	for (key = start_key; key < end_key; key++) {
		if (key % 3 == 0 && ht_delete(ht, key))
			deleted++;
	}

	printf("[deleter] deleted %lu keys (key %% 3 == 0) in [%lu,%lu)\n",
		(unsigned long)deleted, (unsigned long)start_key,
		(unsigned long)end_key);

	shm_unmap(ht, sizeof(*ht));

	return 0;
}
