/*
 * reader.c - futex demo：消费者
 *
 * 用法: ./reader <item_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "futex_common.h"

int
main(int argc, char *argv[])
{
	uint32_t item_count, i;
	uint32_t mismatches = 0;
	struct futex_shared *shared;

	item_count = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 5000;

	while (access("/dev/shm" FUTEX_SHM_NAME, F_OK) != 0)
		usleep(1000);

	shared = shm_map(FUTEX_SHM_NAME, sizeof(*shared), 0);

	for (i = 0; i < item_count; i++) {
		uint64_t item;
		uint32_t checksum;

		/* 等数据到达（data_ready: 1 -> 0），没等到就在内核里睡眠 */
		futex_wait_until_eq(&shared->data_ready, 1, 0);

		item = shared->item;
		checksum = shared->checksum;

		if (checksum != futex_item_checksum(item) ||
				item != (uint64_t)i + 1)
			mismatches++;

		atomic_store_explicit(&shared->slot_free, 1,
			memory_order_release);
		futex_wake(&shared->slot_free, 1);
	}

	printf("[futex reader] consumed=%u mismatches=%u\n",
		item_count, mismatches);

	shm_unmap(shared, sizeof(*shared));
	shm_destroy(FUTEX_SHM_NAME);

	return mismatches == 0 ? 0 : 1;
}
