/*
 * writer.c - 命名信号量 demo：生产者
 *
 * 用法: ./writer <item_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include "sem_common.h"

int
main(int argc, char *argv[])
{
	uint32_t item_count;
	struct sem_shared *shared;
	sem_t *sem;
	uint32_t i;

	item_count = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 5000;
	if (item_count > MAX_SEM_ITEMS)
		item_count = MAX_SEM_ITEMS;

	shared = shm_map(SEM_SHM_NAME, sizeof(*shared), 1);

	sem_unlink(SEM_NAME);	/* 清理上一次残留 */
	sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0);
	if (sem == SEM_FAILED)
		DIE_ERRNO("sem_open");

	for (i = 0; i < item_count; i++) {
		shared->items[i] = (uint64_t)i + 1;

		/* sem_post：计数器 +1，如果 reader 正阻塞在 sem_wait 上，
		 * 内核会唤醒它——效果与 eventfd(EFD_SEMAPHORE) 完全类似，
		 * 只是这里的"名字"是文件系统里的一个信号量对象，天生就能
		 * 被任意两个独立进程通过名字找到，不需要 fork 关系。 */
		if (sem_post(sem) != 0)
			DIE_ERRNO("sem_post");
	}

	printf("[sem writer] produced=%u items\n", item_count);

	sem_close(sem);
	shm_unmap(shared, sizeof(*shared));

	return 0;
}
