/*
 * consumer.c - 独占读 front 缓冲区：认领、拷贝到本地、校验、释放
 *
 * 用法: ./consumer <expected_final_frame> <max_polls>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "double_buffer.h"

int
main(int argc, char *argv[])
{
	struct double_buffer *db;
	uint64_t expected_final;
	uint64_t max_polls, polls;
	uint64_t last_frame = 0;
	uint32_t observed_frames = 0;
	uint32_t checksum_mismatches = 0;
	int reached_final = 0;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <expected_final_frame> <max_polls>\n",
			argv[0]);
		return 1;
	}

	expected_final = strtoull(argv[1], NULL, 10);
	max_polls = strtoull(argv[2], NULL, 10);

	while (access("/dev/shm" DB_SHM_NAME, F_OK) != 0)
		usleep(1000);

	db = db_attach();

	for (polls = 0; polls < max_polls; polls++) {
		uint32_t idx = db_acquire(db);
		/* 立刻拍一份本地快照，避免依赖"临界区内普通读不被重排"的假设 */
		struct db_slot local = db->slots[idx];
		uint32_t recomputed = db_checksum(local.frame_id,
			local.payload);
		volatile int busy;

		for (busy = 0; busy < 3000; busy++)
			;

		if (recomputed != local.checksum) {
			checksum_mismatches++;
			fprintf(stderr,
				"[consumer] CHECKSUM MISMATCH idx=%u frame=%lu\n",
				idx, (unsigned long)local.frame_id);
		}

		db_release(db);

		if (local.frame_id != last_frame) {
			last_frame = local.frame_id;
			observed_frames++;
		}

		if (local.frame_id == expected_final) {
			reached_final = 1;
			break;
		}
	}

	printf("[consumer] observed_frames=%u last_frame=%lu "
		"checksum_mismatches=%u reached_final=%s\n",
		observed_frames, (unsigned long)last_frame,
		checksum_mismatches, reached_final ? "yes" : "no");

	shm_unmap(db, sizeof(*db));

	return (checksum_mismatches == 0 && reached_final) ? 0 : 1;
}
