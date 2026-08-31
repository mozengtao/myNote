/*
 * decode.c - 流水线第二级：从 stage1 读帧，"解码"（模拟为 payload*2），
 *            写入 stage2 环形缓冲区
 */
#include <stdio.h>
#include "pipeline_ring.h"

#define STAGE1_SHM_NAME "/shm_examples_pipeline_stage1"
#define STAGE2_SHM_NAME "/shm_examples_pipeline_stage2"

int
main(void)
{
	struct pipe_ring *in;
	struct pipe_ring *out;
	uint64_t processed = 0;

	in = pipe_ring_attach_wait(STAGE1_SHM_NAME);
	out = pipe_ring_create(STAGE2_SHM_NAME);

	printf("[decode] stage2 ring created, waiting for frames\n");

	for (;;) {
		uint64_t seq, payload;

		if (!pipe_pop(in, &seq, &payload))
			continue;

		if (payload == PIPE_SENTINEL) {
			while (!pipe_push(out, seq, PIPE_SENTINEL))
				;
			break;
		}

		/* 模拟解码工作：payload 乘以 2 */
		while (!pipe_push(out, seq, payload * 2))
			;

		processed++;
	}

	printf("[decode] done, processed %lu frames\n",
		(unsigned long)processed);

	shm_unmap(in, sizeof(*in));
	shm_unmap(out, sizeof(*out));
	shm_destroy(STAGE1_SHM_NAME);

	return 0;
}
