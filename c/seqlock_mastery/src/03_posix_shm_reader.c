/*
 * 03_posix_shm_reader.c
 *
 * 示例 3（读者进程）：基于 POSIX Shared Memory（shm_open + mmap）的只读客户端
 *
 * 核心技术亮点：
 * 1. 真正的只读内存保护：mmap 可以使用纯 PROT_READ 权限映射！
 *    - 在传统 Mutex / RWLock 下，读者必须以 PROT_READ | PROT_WRITE 映射，
 *      因为获取读锁必须执行原子写（如 lock xadd），这给沙箱和安全权限审计带来极大漏洞。
 *    - 在 Seqlock 架构下，读者只需要 PROT_READ 权限，从操作系统内核级杜绝任何写越界污染！
 * 2. 崩溃完全免疫：本进程可以随时被外部 SIGKILL（kill -9）强杀，写者端毫无影响。
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
 * 信号处理函数
 */
static void
sig_handler(int sig)
{
	(void)sig;
	g_stop = 1;
}

/*
 * 打开并只读映射 POSIX 共享内存区域
 */
static const shm_channel_t *
open_shm_reader(int *out_fd)
{
	int fd;
	const shm_channel_t *chan;
	int retries = 0;

	/* 等待写者进程创建共享内存 */
	while ((fd = shm_open(SHM_NAME, O_RDONLY, 0)) < 0) {
		if (retries++ > 20) {
			perror("[读者错误] 等待共享内存 /shm_seqlock_demo 超时");
			return NULL;
		}
		usleep(100000); /* 等待 100 毫秒 */
	}

	/*
	 * 以纯只读方式（PROT_READ）进行内存映射
	 * 证明 Seqlock 读者不需要任何写入权限
	 */
	chan = (const shm_channel_t *)mmap(NULL, sizeof(shm_channel_t),
					   PROT_READ, MAP_SHARED, fd, 0);
	if (chan == MAP_FAILED) {
		perror("[读者错误] mmap 失败");
		close(fd);
		return NULL;
	}

	/* 校验魔数确认写者已完成基础初始化 */
	if (chan->magic != MAGIC_SEQLOCK) {
		fprintf(stderr, "[读者错误] 共享内存魔数不匹配 (0x%08X)\n", chan->magic);
		munmap((void *)chan, sizeof(shm_channel_t));
		close(fd);
		return NULL;
	}

	*out_fd = fd;
	return chan;
}

/*
 * 循环采集快照
 */
static void
run_reader_loop(const shm_channel_t *chan)
{
	service_metrics_t snap;
	uint32_t seq;
	uint64_t success_reads = 0;
	uint64_t retry_count = 0;
	uint64_t torn_errors = 0;

	printf("[读者] 共享内存映射成功（只读权限），启动无锁采集...\n");

	while (!g_stop) {
		bool valid = false;

		/*
		 * Seqlock 读者核心无锁读取循环
		 */
		do {
			seq = read_seqbegin(&chan->lock);

			/*
			 * 直接按值拷贝（由于是只读映射，任何写操作都会直接触发 SIGSEGV，
			 * 从硬件层面严格证明了这里只有读操作）
			 */
			snap = chan->metrics;

			if (!read_seqretry(&chan->lock, seq)) {
				valid = true;
				break;
			}

			retry_count++;
			seqlock_cpu_relax();
		} while (!g_stop);

		if (!valid)
			break;

		success_reads++;

		/*
		 * 业务合法性校验：metric_c 必须恒等于 metric_a + metric_b
		 */
		if (snap.metric_c != (snap.metric_a + snap.metric_b)) {
			torn_errors++;
			fprintf(stderr, "[严重错误] 观测到跨进程数据撕裂! a=%lu, b=%lu, c=%lu\n",
				snap.metric_a, snap.metric_b, snap.metric_c);
		}

		if ((success_reads % 50000000) == 0) {
			printf("[读者采集] 成功: %lu 次 | 冲突重试: %lu 次 | 撕裂: %lu | 当前指标: a=%lu, b=%lu, 状态=%s\n",
				success_reads, retry_count, torn_errors,
				snap.metric_a, snap.metric_b, snap.status_msg);
		}
	}

	printf("\n[读者总结] 累计成功采集: %lu 次, 冲突重试: %lu 次, 撕裂错误: %lu\n",
		success_reads, retry_count, torn_errors);
}

/*
 * 主函数
 */
int
main(int argc, char *argv[])
{
	int shm_fd = -1;
	const shm_channel_t *chan;
	struct sigaction sa;
	int run_duration = 0;

	if (argc > 1)
		run_duration = atoi(argv[1]);

	printf("========================================================\n");
	printf("  示例 3：POSIX 共享内存 Seqlock 读者进程（Reader）     \n");
	printf("========================================================\n");

	/* 注册信号处理器 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	chan = open_shm_reader(&shm_fd);
	if (!chan)
		return EXIT_FAILURE;

	if (run_duration > 0) {
		alarm(run_duration);
		signal(SIGALRM, sig_handler);
	}

	run_reader_loop(chan);

	munmap((void *)chan, sizeof(shm_channel_t));
	close(shm_fd);

	printf("[读者] 安全退出。\n");
	return EXIT_SUCCESS;
}
