/*
 * producer.c - 独占写 back 缓冲区，写完后原子翻转 front_index 发布
 *
 * 用法: ./producer <frame_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include "double_buffer.h"

int
main(int argc, char *argv[])
{
	struct double_buffer *db;
	uint64_t frame_count, f;

	frame_count = (argc > 1) ? strtoull(argv[1], NULL, 10) : 20000;

	db = db_create();
	printf("[producer] double buffer created, publishing %lu frames\n",
		(unsigned long)frame_count);

	for (f = 1; f <= frame_count; f++) {
		volatile int busy;

		/* 模拟"生成一帧真实数据需要一点时间"，否则 producer 会在
		 * consumer 进程完成 fork/exec 之前就把全部帧发完，看不到
		 * 双缓冲的等待行为 */
		for (busy = 0; busy < 500; busy++)
			;

		db_publish(db, f);
	}

	printf("[producer] done, final frame=%lu, writer_wait_events=%u\n",
		(unsigned long)frame_count,
		atomic_load_explicit(&db->writer_wait_events,
			memory_order_relaxed));

	shm_unmap(db, sizeof(*db));

	return 0;
}
