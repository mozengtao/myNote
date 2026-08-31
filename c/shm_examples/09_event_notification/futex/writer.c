/*
 * writer.c - futex demo：生产者
 *
 * 用法: ./writer <item_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include "futex_common.h"

int
main(int argc, char *argv[])
{
	uint32_t item_count, i;
	struct futex_shared *shared;

	item_count = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 5000;

	shared = shm_map(FUTEX_SHM_NAME, sizeof(*shared), 1);
	atomic_store_explicit(&shared->slot_free, 1, memory_order_relaxed);
	atomic_store_explicit(&shared->data_ready, 0, memory_order_relaxed);

	printf("[futex writer] shared memory ready, producing %u items\n",
		item_count);

	for (i = 0; i < item_count; i++) {
		uint64_t item = (uint64_t)i + 1;

		/* 等槽位空闲（slot_free: 1 -> 0），没等到就在内核里睡眠，
		 * 不会像 08_double_buffer 那样忙等烧 CPU */
		futex_wait_until_eq(&shared->slot_free, 1, 0);

		shared->item = item;
		shared->checksum = futex_item_checksum(item);

		atomic_store_explicit(&shared->data_ready, 1,
			memory_order_release);
		futex_wake(&shared->data_ready, 1);
	}

	printf("[futex writer] done, produced=%u items\n", item_count);

	shm_unmap(shared, sizeof(*shared));

	return 0;
}
