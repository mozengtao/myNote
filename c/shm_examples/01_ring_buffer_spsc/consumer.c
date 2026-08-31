/*
 * consumer.c - Ring Buffer SPSC 消费者
 *
 * 用法: ./consumer
 * 会一直轮询直到收到 payload == UINT64_MAX 的哨兵，然后校验
 * 收到的序列号是否严格连续、无丢失、无重复。
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "ring_buffer.h"

int
main(void)
{
	struct ring_buffer *rb;
	uint64_t seq, payload;
	uint64_t expected = 0;
	int ok = 1;

	/* 等待生产者先创建好共享内存 */
	while (access("/dev/shm" RING_SHM_NAME, F_OK) != 0)
		usleep(1000);

	rb = ring_attach();
	printf("[consumer] attached, capacity=%u\n", rb->capacity);

	for (;;) {
		if (!ring_pop(rb, &seq, &payload)) {
			usleep(0);
			continue;
		}

		if (payload == UINT64_MAX) {
			printf("[consumer] got sentinel at seq=%lu, stop\n",
				(unsigned long)seq);
			break;
		}

		if (seq != expected || payload != seq * 2) {
			fprintf(stderr,
				"[consumer] MISMATCH expected=%lu got seq=%lu payload=%lu\n",
				(unsigned long)expected, (unsigned long)seq,
				(unsigned long)payload);
			ok = 0;
		}
		expected++;
	}

	printf("[consumer] received %lu items, integrity=%s\n",
		(unsigned long)expected, ok ? "OK" : "FAILED");

	shm_unmap(rb, sizeof(*rb));
	shm_destroy(RING_SHM_NAME);

	return ok ? 0 : 1;
}
