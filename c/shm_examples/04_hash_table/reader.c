/*
 * reader.c - 并发查询共享哈希表里的一段 key 区间并校验值
 *
 * 用法: ./reader <reader_id> <start_key> <end_key_exclusive>
 * 多个 reader 可以查询重叠甚至相同的区间，用来验证 bucket 锁下的
 * 并发读是安全的。
 */
#include <stdio.h>
#include <stdlib.h>
#include "hash_table.h"

int
main(int argc, char *argv[])
{
	struct shared_hash_table *ht;
	uint32_t reader_id;
	uint64_t start_key, end_key, key;
	uint64_t hits = 0, misses = 0, mismatches = 0;

	if (argc != 4) {
		fprintf(stderr,
			"usage: %s <reader_id> <start_key> <end_key_exclusive>\n",
			argv[0]);
		return 1;
	}

	reader_id = (uint32_t)strtoul(argv[1], NULL, 10);
	start_key = strtoull(argv[2], NULL, 10);
	end_key = strtoull(argv[3], NULL, 10);

	ht = ht_attach();

	for (key = start_key; key < end_key; key++) {
		uint64_t value;

		if (!ht_lookup(ht, key, &value)) {
			misses++;
			continue;
		}

		hits++;
		if (value != key * 2) {
			mismatches++;
			atomic_fetch_add_explicit(&ht->stat_value_mismatch,
				1, memory_order_relaxed);
		}
	}

	printf("[reader %u] range=[%lu,%lu) hits=%lu misses=%lu mismatches=%lu\n",
		reader_id, (unsigned long)start_key, (unsigned long)end_key,
		(unsigned long)hits, (unsigned long)misses,
		(unsigned long)mismatches);

	shm_unmap(ht, sizeof(*ht));

	return (mismatches == 0) ? 0 : 1;
}
