/*
 * capture.c - 流水线第一级：产生"帧"数据，写入 stage1 环形缓冲区
 *
 * 用法: ./capture <frame_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include "pipeline_ring.h"

#define STAGE1_SHM_NAME "/shm_examples_pipeline_stage1"

int
main(int argc, char *argv[])
{
	struct pipe_ring *rb;
	uint64_t count, i;

	count = (argc > 1) ? strtoull(argv[1], NULL, 10) : 100000;

	rb = pipe_ring_create(STAGE1_SHM_NAME);
	printf("[capture] stage1 ring created, count=%lu\n",
		(unsigned long)count);

	for (i = 0; i < count; i++) {
		/* payload 就是原始"帧数据"，这里简化为帧序号本身 */
		while (!pipe_push(rb, i, i))
			;
	}

	while (!pipe_push(rb, count, PIPE_SENTINEL))
		;

	printf("[capture] done, produced %lu frames\n", (unsigned long)count);

	shm_unmap(rb, sizeof(*rb));

	return 0;
}
