/*
 * eventfd_demo.c
 *
 * eventfd 通知（父子进程，fd 天然共享）
 *
 * 心智模型：eventfd 本质是一个内核维护的 64 位计数器，配合一个可以被
 * poll/select/epoll 监视的文件描述符。`write()` 给计数器加值，`read()`
 * 取出并清零（或用 EFD_SEMAPHORE 模式每次只消耗 1，像信号量一样）。
 * fork() 会复制文件描述符表，所以父子进程天然共享同一个 eventfd 对象，
 * 不需要任何额外的跨进程传递手段——这是本示例特意选择"fork 出子进程"
 * 而不是"两个独立进程"的原因：突出 eventfd 最省事的使用场景。
 *
 * 数据本身放在 POSIX 共享内存里（与其他示例保持一致的风格），
 * eventfd 只负责"有新数据了，别再空转，醒过来吧"这件事。
 *
 * 用法: ./eventfd_demo <item_count>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../common/shm_common.h"

#define EVENTFD_SHM_NAME "/shm_examples_eventfd"
#define MAX_ITEMS 200000u

struct eventfd_shared {
	uint64_t items[MAX_ITEMS];
};

int
main(int argc, char *argv[])
{
	uint32_t item_count;
	int efd;
	struct eventfd_shared *shared;
	pid_t pid;

	item_count = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 5000;
	if (item_count > MAX_ITEMS)
		item_count = MAX_ITEMS;

	shared = shm_map(EVENTFD_SHM_NAME, sizeof(*shared), 1);

	/*
	 * EFD_SEMAPHORE：每次成功的 read() 只消耗计数器里的 1，并返回 1，
	 * 行为等价于一个进程间信号量。如果不加这个 flag，read() 会一次性
	 * 取出全部累计值并清零——那样消费者就没法知道"这次到底新增了
	 * 多少个 item"，还得额外一个计数器自己算，加上 EFD_SEMAPHORE 后
	 * 直接把 eventfd 当成"待处理 item 计数"来用最省事。
	 */
	efd = eventfd(0, EFD_SEMAPHORE);
	if (efd < 0)
		DIE_ERRNO("eventfd");

	pid = fork();
	if (pid < 0)
		DIE_ERRNO("fork");

	if (pid == 0) {
		/* 子进程：消费者。fork 之后 efd 是同一个内核对象的另一份
		 * 引用，read() 会在计数器为 0 时阻塞睡眠，被父进程 write()
		 * 唤醒——完全不用忙等（对比 01~08 里大量的 busy-loop）。 */
		uint32_t consumed = 0;
		uint32_t mismatches = 0;

		while (consumed < item_count) {
			uint64_t val;
			ssize_t n = read(efd, &val, sizeof(val));

			if (n != (ssize_t)sizeof(val))
				DIE_ERRNO("read(eventfd)");

			/* val 恒为 1（EFD_SEMAPHORE 模式），代表"又有一个
			 * item 可以消费了" */
			if (shared->items[consumed] != (uint64_t)consumed + 1)
				mismatches++;

			consumed++;
		}

		printf("[eventfd child] consumed=%u mismatches=%u\n",
			consumed, mismatches);
		fflush(stdout);	/* _exit() 不会自动 flush stdio 缓冲区 */

		shm_unmap(shared, sizeof(*shared));
		close(efd);

		_exit(mismatches == 0 ? 0 : 1);
	}

	/* 父进程：生产者 */
	{
		uint32_t i;

		for (i = 0; i < item_count; i++) {
			uint64_t one = 1;

			shared->items[i] = (uint64_t)i + 1;

			/* write() 给计数器 +1，如果子进程正阻塞在 read()
			 * 上，内核会立刻唤醒它 */
			if (write(efd, &one, sizeof(one)) != (ssize_t)sizeof(one))
				DIE_ERRNO("write(eventfd)");
		}

		printf("[eventfd parent] produced=%u items\n", item_count);
	}

	{
		int status;
		int child_ok;

		waitpid(pid, &status, 0);
		child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;

		shm_unmap(shared, sizeof(*shared));
		shm_destroy(EVENTFD_SHM_NAME);
		close(efd);

		return child_ok ? 0 : 1;
	}
}
