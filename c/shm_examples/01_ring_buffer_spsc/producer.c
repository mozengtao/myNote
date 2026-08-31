/*
 * producer.c - Ring Buffer SPSC 生产者
 *
 * 用法: ./producer <count>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ring_buffer.h"

int
main(int argc, char *argv[])
{
	struct ring_buffer *rb;
	uint64_t count;
	uint64_t i;
	uint32_t retries;

	count = (argc > 1) ? strtoull(argv[1], NULL, 10) : 100000;

	rb = ring_create();
	printf("[producer] ring created, capacity=%u, count=%lu\n",
		rb->capacity, (unsigned long)count);

	for (i = 0; i < count; i++) {
		retries = 0;
		while (!ring_push(rb, i, i * 2)) {
			retries++;
			if (retries % 1000000 == 0)
				usleep(100); /* 消费者太慢时让出 CPU */
		}
	}

	/* 用 payload == UINT64_MAX 的哨兵值告知消费者结束 */
	while (!ring_push(rb, count, UINT64_MAX))
		;

	printf("[producer] done, pushed %lu items + 1 sentinel\n",
		(unsigned long)count);

	shm_unmap(rb, sizeof(*rb));

	return 0;
}
