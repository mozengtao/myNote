/*
 * display.c - 流水线第三级：从 stage2 读取"解码后的帧"，校验并"显示"
 */
#include <stdio.h>
#include "pipeline_ring.h"

#define STAGE2_SHM_NAME "/shm_examples_pipeline_stage2"

int
main(void)
{
	struct pipe_ring *in;
	uint64_t expected = 0;
	uint64_t received = 0;
	int ok = 1;

	in = pipe_ring_attach_wait(STAGE2_SHM_NAME);
	printf("[display] attached to stage2, waiting for frames\n");

	for (;;) {
		uint64_t seq, payload;

		if (!pipe_pop(in, &seq, &payload))
			continue;

		if (payload == PIPE_SENTINEL) {
			printf("[display] got sentinel at seq=%lu, stop\n",
				(unsigned long)seq);
			break;
		}

		/* capture 产出 seq，decode 乘 2，所以这里期望 payload == seq*2 */
		if (seq != expected || payload != seq * 2) {
			fprintf(stderr,
				"[display] MISMATCH expected_seq=%lu got seq=%lu payload=%lu\n",
				(unsigned long)expected, (unsigned long)seq,
				(unsigned long)payload);
			ok = 0;
		}

		expected++;
		received++;
	}

	printf("[display] received %lu frames, integrity=%s\n",
		(unsigned long)received, ok ? "OK" : "FAILED");

	shm_unmap(in, sizeof(*in));
	shm_destroy(STAGE2_SHM_NAME);

	return ok ? 0 : 1;
}
