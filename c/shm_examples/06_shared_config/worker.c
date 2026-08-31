/*
 * worker.c - 轮询共享配置，检测 version 变化即"reload"，并用 checksum
 *            验证每次读到的都是完整快照（不是撕裂数据）。
 *
 * 用法: ./worker <worker_id> <expected_final_version> <max_polls>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "shared_config.h"

int
main(int argc, char *argv[])
{
	struct shared_config *cfg;
	uint32_t worker_id;
	uint32_t expected_final;
	uint64_t max_polls, polls;
	uint32_t last_seen_version = 0;
	uint32_t reload_count = 0;
	uint32_t checksum_mismatches = 0;
	uint32_t max_retries_seen = 0;
	uint64_t total_retries = 0;
	int reached_final = 0;

	if (argc != 4) {
		fprintf(stderr,
			"usage: %s <worker_id> <expected_final_version> <max_polls>\n",
			argv[0]);
		return 1;
	}

	worker_id = (uint32_t)strtoul(argv[1], NULL, 10);
	expected_final = (uint32_t)strtoul(argv[2], NULL, 10);
	max_polls = strtoull(argv[3], NULL, 10);

	while (access("/dev/shm" CONFIG_SHM_NAME, F_OK) != 0)
		usleep(1000);

	cfg = config_attach();

	for (polls = 0; polls < max_polls; polls++) {
		struct shared_config snap;
		uint32_t retries = config_read(cfg, &snap);

		if (retries > max_retries_seen)
			max_retries_seen = retries;
		total_retries += retries;

		if (snap.checksum != config_checksum(snap.version,
			snap.threshold, snap.retry_count, snap.label)) {
			checksum_mismatches++;
			fprintf(stderr,
				"[worker %u] CHECKSUM MISMATCH at version=%u (torn read leaked through seqlock!)\n",
				worker_id, snap.version);
		}

		if (snap.version != last_seen_version) {
			last_seen_version = snap.version;
			reload_count++;
		}

		if (snap.version == expected_final) {
			reached_final = 1;
			break;
		}
	}

	printf("[worker %u] reload_count=%u last_version=%u "
		"max_retries_per_read=%u total_retries=%lu "
		"checksum_mismatches=%u reached_final=%s\n",
		worker_id, reload_count, last_seen_version, max_retries_seen,
		(unsigned long)total_retries, checksum_mismatches,
		reached_final ? "yes" : "no");

	shm_unmap(cfg, sizeof(*cfg));

	return (checksum_mismatches == 0 && reached_final) ? 0 : 1;
}
