/*
 * config_writer.c - 高频发布配置更新，压力测试 seqlock
 *
 * 用法: ./config_writer <update_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include "shared_config.h"

int
main(int argc, char *argv[])
{
	struct shared_config *cfg;
	uint32_t update_count, i;

	update_count = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10)
		: 20000;

	cfg = config_create();
	printf("[writer] config created, will publish %u updates\n",
		update_count);

	for (i = 1; i <= update_count; i++) {
		char label[LABEL_LEN];

		snprintf(label, sizeof(label), "config-v%u", i);
		config_publish(cfg, i, (i % 2 == 0) ? (int32_t)i : -(int32_t)i,
			i * 3, label);
	}

	printf("[writer] done, final version=%u\n", update_count);

	shm_unmap(cfg, sizeof(*cfg));

	return 0;
}
