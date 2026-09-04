/*
 * 03_posix_shm_writer.c
 *
 * 示例 3（写者进程）：基于 POSIX Shared Memory（shm_open + mmap）的高可靠跨进程通信
 *
 * 核心原理说明：
 * 1. 使用 shm_open 创建具名共享内存对象 /shm_seqlock_demo。
 * 2. 使用 ftruncate 设定空间大小并 mmap 映射到写者进程地址空间。
 * 3. 为什么 Seqlock 极度适合 Shared Memory IPC？
 *    - 进程崩溃隔离：在传统进程互斥锁（PTHREAD_PROCESS_SHARED Mutex）中，
 *      若某个读者进程正在读临界区时被 kill -9、收到 SIGSEGV 崩溃、或卡在 GDB 断点，
 *      锁将永远无法释放，写者进程后续加锁必将永久死锁（Deadlock）！
 *    - Seqlock 机制下，读者绝对不写共享内存、绝对不占有任何锁。
 *      即使 1000 个读者进程被 kill -9 强杀，写者也完全无感知、零受阻，永远保证高可用。
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "../include/seqlock.h"

#define SHM_NAME "/shm_seqlock_demo"
#define MAGIC_SEQLOCK 0x5345514CU /* 'SEQL' */

/*
 * 跨进程共享业务数据
 */
typedef struct {
	uint64_t metric_a;
	uint64_t metric_b;
	uint64_t metric_c;
	uint64_t timestamp_ns;
	char status_msg[64];
} service_metrics_t;

/*
 * 共享内存控制块布局
 */
typedef struct {
	uint32_t magic;
	seqcount_t lock;
	service_metrics_t metrics;
} shm_channel_t;

static volatile sig_atomic_t g_stop = 0;

/*
 * 信号处理函数：优雅捕获 Ctrl+C 或 SIGTERM
 */
static void
sig_handler(int sig)
{
	(void)sig;
	g_stop = 1;
}

/*
 * 获取当前单调纳秒时间戳
 */
static inline uint64_t
get_monotonic_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/*
 * 初始化并打开 POSIX 共享内存区域
 */
static shm_channel_t *
init_shm_writer(int *out_fd)
{
	int fd;
	shm_channel_t *chan;

	/* 确保先清理之前可能残留的旧共享内存 */
	shm_unlink(SHM_NAME);

	/* 创建读写权限的共享内存段 */
	fd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (fd < 0) {
		perror("[写者错误] shm_open 失败");
		return NULL;
	}

	/* 截断分配指定字节大小 */
	if (ftruncate(fd, sizeof(shm_channel_t)) != 0) {
		perror("[写者错误] ftruncate 失败");
		close(fd);
		shm_unlink(SHM_NAME);
		return NULL;
	}

	/* 映射到当前进程的虚拟地址空间 */
	chan = (shm_channel_t *)mmap(NULL, sizeof(shm_channel_t),
				     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (chan == MAP_FAILED) {
		perror("[写者错误] mmap 失败");
		close(fd);
		shm_unlink(SHM_NAME);
		return NULL;
	}

	/* 初始化通道头与 Seqlock */
	chan->magic = MAGIC_SEQLOCK;
	seqcount_init(&chan->lock);

	memset(&chan->metrics, 0, sizeof(chan->metrics));
	snprintf(chan->metrics.status_msg, sizeof(chan->metrics.status_msg), "INIT");

	*out_fd = fd;
	return chan;
}

/*
 * 循环写入业务指标
 */
static void
run_writer_loop(shm_channel_t *chan)
{
	uint64_t counter = 0;

	printf("[写者] 共享内存通道就绪，进入高速指标广播循环...\n");

	while (!g_stop) {
		counter++;

		/*
		 * 关键步骤 1：标记写入开始（seq++ 变为奇数 + Release Fence）
		 */
		write_seqbegin(&chan->lock);

		/*
		 * 关键步骤 2：更新多字段指标（保持 metric_c = metric_a + metric_b 一致性）
		 */
		chan->metrics.metric_a = counter;
		chan->metrics.metric_b = counter * 10;
		chan->metrics.metric_c = chan->metrics.metric_a + chan->metrics.metric_b;
		chan->metrics.timestamp_ns = get_monotonic_ns();
		snprintf(chan->metrics.status_msg, sizeof(chan->metrics.status_msg),
			 "STATUS_RUNNING_ITER_%lu", counter);

		/*
		 * 关键步骤 3：标记写入完成（seq++ 变为偶数 + Release Store）
		 */
		write_seqend(&chan->lock);

		/* 每 1000 次让出 100 微秒，兼顾高频写入与可观测性 */
		if ((counter % 1000) == 0)
			usleep(100);
	}

	printf("[写者] 收到退出信号，累计写入总次数: %lu\n", counter);
}

/*
 * 主函数
 */
int
main(int argc, char *argv[])
{
	int shm_fd = -1;
	shm_channel_t *chan;
	struct sigaction sa;
	int run_duration = 0;

	if (argc > 1)
		run_duration = atoi(argv[1]);

	printf("========================================================\n");
	printf("  示例 3：POSIX 共享内存 Seqlock 写者进程（Writer）     \n");
	printf("========================================================\n");

	/* 注册信号处理器 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	chan = init_shm_writer(&shm_fd);
	if (!chan)
		return EXIT_FAILURE;

	/* 如果指定了限时运行参数，例如自动测试场景 */
	if (run_duration > 0) {
		alarm(run_duration);
		signal(SIGALRM, sig_handler);
	}

	run_writer_loop(chan);

	/* 资源清理 */
	munmap(chan, sizeof(shm_channel_t));
	close(shm_fd);
	shm_unlink(SHM_NAME);

	printf("[写者] 共享内存成功解除映射并释放 /shm_seqlock_demo。\n");
	return EXIT_SUCCESS;
}
