/*
 * verify.c - 在 deleter 跑完之后，扫描整个 key 区间做最终一致性校验
 *
 * 用法: ./verify <start_key> <end_key_exclusive>
 *
 * 期望：key % 3 == 0 的条目已被删除（lookup 应该 miss），
 *      其余条目应该还在，且 value == key*2。
 */
#include <stdio.h>
#include <stdlib.h>
#include "hash_table.h"

int
main(int argc, char *argv[])
{
	struct shared_hash_table *ht;
	uint64_t start_key, end_key, key;
	uint64_t errors = 0;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <start_key> <end_key_exclusive>\n",
			argv[0]);
		return 1;
	}

	start_key = strtoull(argv[1], NULL, 10);
	end_key = strtoull(argv[2], NULL, 10);

	ht = ht_attach();

	for (key = start_key; key < end_key; key++) {
		uint64_t value;
		int found = ht_lookup(ht, key, &value);
		int should_exist = (key % 3 != 0);

		if (found != should_exist) {
			fprintf(stderr,
				"[verify] key=%lu found=%d should_exist=%d\n",
				(unsigned long)key, found, should_exist);
			errors++;
			continue;
		}

		if (found && value != key * 2) {
			fprintf(stderr,
				"[verify] key=%lu value=%lu expected=%lu\n",
				(unsigned long)key, (unsigned long)value,
				(unsigned long)(key * 2));
			errors++;
		}
	}

	printf("[verify] range=[%lu,%lu) errors=%lu insert=%lu delete=%lu "
		"lookup_hit=%lu lookup_miss=%lu value_mismatch=%lu result=%s\n",
		(unsigned long)start_key, (unsigned long)end_key,
		(unsigned long)errors,
		(unsigned long)atomic_load_explicit(&ht->stat_insert,
			memory_order_relaxed),
		(unsigned long)atomic_load_explicit(&ht->stat_delete,
			memory_order_relaxed),
		(unsigned long)atomic_load_explicit(&ht->stat_lookup_hit,
			memory_order_relaxed),
		(unsigned long)atomic_load_explicit(&ht->stat_lookup_miss,
			memory_order_relaxed),
		(unsigned long)atomic_load_explicit(
			&ht->stat_value_mismatch, memory_order_relaxed),
		(errors == 0) ? "OK" : "FAILED");

	shm_unmap(ht, sizeof(*ht));
	shm_destroy(HT_SHM_NAME);

	return (errors == 0) ? 0 : 1;
}
