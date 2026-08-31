/*
 * reader.c - 命名信号量 demo：消费者
 *
 * 用法: ./reader <item_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include "sem_common.h"

int
main(int argc, char *argv[])
{
	uint32_t item_count;
	struct sem_shared *shared;
	sem_t *sem;
	uint32_t i;
	uint32_t mismatches = 0;

	item_count = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 5000;
	if (item_count > MAX_SEM_ITEMS)
		item_count = MAX_SEM_ITEMS;

	/* 等待 writer 先创建好共享内存与信号量 */
	while (access("/dev/shm" SEM_SHM_NAME, F_OK) != 0)
		usleep(1000);

	shared = shm_map(SEM_SHM_NAME, sizeof(*shared), 0);

	for (;;) {
		sem = sem_open(SEM_NAME, 0);
		if (sem != SEM_FAILED)
			break;
		usleep(1000);
	}

	for (i = 0; i < item_count; i++) {
		/* sem_wait：计数器为 0 时阻塞睡眠，被 writer 的 sem_post
		 * 唤醒后返回，计数器 -1 */
		if (sem_wait(sem) != 0)
			DIE_ERRNO("sem_wait");

		if (shared->items[i] != (uint64_t)i + 1)
			mismatches++;
	}

	printf("[sem reader] consumed=%u mismatches=%u\n",
		item_count, mismatches);

	sem_close(sem);
	shm_unmap(shared, sizeof(*shared));

	return mismatches == 0 ? 0 : 1;
}
