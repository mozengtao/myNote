/*
 * writer.c - 高频发布快照
 *
 * 用法: ./writer <version_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include "snapshot.h"

int
main(int argc, char *argv[])
{
	struct snapshot_store *store;
	uint32_t version_count, v;

	version_count = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10)
		: 20000;

	store = snapshot_create();
	printf("[writer] snapshot store created (%u buffers), publishing %u versions\n",
		NUM_SNAPSHOTS, version_count);

	for (v = 1; v <= version_count; v++)
		snapshot_publish(store, v);

	printf("[writer] done, final version=%u, writer_wait_events=%u\n",
		version_count,
		atomic_load_explicit(&store->writer_wait_events,
			memory_order_relaxed));

	shm_unmap(store, sizeof(*store));

	return 0;
}
