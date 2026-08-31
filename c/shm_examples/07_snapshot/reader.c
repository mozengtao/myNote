/*
 * reader.c - 反复认领当前快照、校验、持有一小段时间再释放
 *
 * 用法: ./reader <reader_id> <expected_final_version> <max_polls>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "snapshot.h"

int
main(int argc, char *argv[])
{
	struct snapshot_store *store;
	uint32_t reader_id;
	uint64_t expected_final;
	uint64_t max_polls, polls;
	uint64_t last_version = 0;
	uint32_t observed_versions = 0;
	uint32_t checksum_mismatches = 0;
	int reached_final = 0;

	if (argc != 4) {
		fprintf(stderr,
			"usage: %s <reader_id> <expected_final_version> <max_polls>\n",
			argv[0]);
		return 1;
	}

	reader_id = (uint32_t)strtoul(argv[1], NULL, 10);
	expected_final = strtoull(argv[2], NULL, 10);
	max_polls = strtoull(argv[3], NULL, 10);

	if (reader_id >= MAX_READERS) {
		fprintf(stderr, "reader_id must be < %u\n", MAX_READERS);
		return 1;
	}

	while (access("/dev/shm" SNAPSHOT_SHM_NAME, F_OK) != 0)
		usleep(1000);

	store = snapshot_attach();

	for (polls = 0; polls < max_polls; polls++) {
		uint64_t version = snapshot_acquire(store, reader_id);
		uint32_t physical = (uint32_t)(version % NUM_SNAPSHOTS);
		/*
		 * 认领成功后立刻把整块快照拷贝到本地变量，后续所有校验只
		 * 针对这份本地拷贝操作，不再回头读共享内存里的字段。
		 */
		struct snapshot_slot local = store->slots[physical];
		uint32_t recomputed = snapshot_checksum(local.version,
			local.payload);
		volatile int busy;

		/* 模拟"正在使用这份快照做一点计算"，故意拖慢一点，
		 * 增大 Writer 需要等待的概率 */
		for (busy = 0; busy < 3000; busy++)
			;

		if (recomputed != local.checksum || local.version != version) {
			checksum_mismatches++;
			fprintf(stderr,
				"[reader %u] CHECKSUM MISMATCH acquired_version=%lu slot_version=%lu\n",
				reader_id, (unsigned long)version,
				(unsigned long)local.version);
		}

		snapshot_release(store, reader_id);

		if (local.version != last_version) {
			last_version = local.version;
			observed_versions++;
		}

		if (local.version == expected_final) {
			reached_final = 1;
			break;
		}
	}

	printf("[reader %u] observed_versions=%u last_version=%lu "
		"checksum_mismatches=%u reached_final=%s\n",
		reader_id, observed_versions, (unsigned long)last_version,
		checksum_mismatches, reached_final ? "yes" : "no");

	shm_unmap(store, sizeof(*store));

	return (checksum_mismatches == 0 && reached_final) ? 0 : 1;
}
