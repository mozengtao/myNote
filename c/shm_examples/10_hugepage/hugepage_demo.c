/*
 * hugepage_demo.c
 *
 * Shared Memory + HugePage
 *
 * 心智模型：普通页是 4KB，TLB（页表缓存）能覆盖的内存范围有限；当共享
 * 内存区域很大（几百 MB 的行情缓存、机器学习模型权重、DPDK 风格的收发
 * 包缓冲区）时，4KB 页会导致大量 TLB miss，每次 miss 都要走一次页表
 * 遍历。用 2MB/1GB 的 HugePage 能把同样大小内存所需的页表项数量减少
 * 512~262144 倍，显著降低 TLB miss 率。
 *
 * 本机环境验证结果（见 plan）：只有 1 个 NUMA node，
 * `HugePages_Total=0`（没有预留传统意义上的 HugeTLB 页），但内核开启了
 * Transparent HugePage（THP），模式是 `madvise`——默认不用，但程序显式
 * `madvise(MADV_HUGEPAGE)` 后，内核会在缺页处理时尝试直接分配 2MB 大页。
 *
 * 排查中发现的一个容易踩的坑：`MAP_SHARED | MAP_ANONYMOUS` 在 Linux 上
 * 实际是由 tmpfs/shmem 承载的，它的大页开关是独立的
 * `/sys/kernel/mm/transparent_hugepage/shmem_enabled`（本机是 `never`），
 * 不受这段 VMA 上 `madvise(MADV_HUGEPAGE)` 提示的影响——不管怎么
 * madvise，`AnonHugePages` 永远是 0。真正遵从每-VMA `madvise` 提示、
 * 由 `/proc/self/smaps` 里 `AnonHugePages` 字段体现效果的，是
 * `MAP_PRIVATE | MAP_ANONYMOUS` 这种"纯匿名私有页"。
 *
 * 这个限制换来的启发是一个非常经典的生产实践：**预加载一份大的只读
 * 数据（模型权重、词典、行情快照），用 HugePage 承载，然后 `fork()`
 * 出多个 worker 进程**。`fork()` 后子进程通过 COW（Copy-On-Write）
 * 继续读取同一批物理大页，只要子进程不写它，物理内存和大页承载都不会
 * 重复分配——相当于用"预加载 + fork 只读 workers"变相实现了"多进程
 * 共享同一份大页支持的大内存"，这正是 Unicorn/Gunicorn 的
 * `preload_app`、Redis 的 `fork()` 做 BGSAVE、多进程模型推理服务
 * 预加载权重后 fork worker 的标准套路。
 *
 * 如果 `HugePages_Total > 0`（生产环境常见配置），更严格的做法是用
 * `mmap(..., MAP_SHARED | MAP_HUGETLB, ...)` 直接从 HugeTLB 页池分配，
 * 那是"预留、确定生效、且真正跨进程可写共享"的方案，缺点是需要提前用
 * `sysctl vm.nr_hugepages=N` 预留内存；本机环境没有预留，因此不在本
 * demo 里强行演示。
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../common/shm_common.h"

#define HUGE_PAGE_SIZE (2u * 1024 * 1024)
#define REGION_SIZE (32u * HUGE_PAGE_SIZE)	/* 64MB，32 个 2MB 大页 */
#define NUM_WORKERS 3

/*
 * 分配一段 2MB 对齐的 MAP_PRIVATE|MAP_ANONYMOUS 内存。做法：先多映射
 * HUGE_PAGE_SIZE 长度用于对齐余量，找到 2MB 对齐的起始地址后，把头尾
 * 多余部分 munmap 掉，只留下 [aligned, aligned+size)。
 */
static void *
map_huge_aligned_private(size_t size)
{
	size_t map_size = size + HUGE_PAGE_SIZE;
	uintptr_t raw, aligned;
	size_t head_trim, tail_trim;

	void *p = mmap(NULL, map_size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		DIE_ERRNO("mmap(anonymous)");

	raw = (uintptr_t)p;
	aligned = (raw + HUGE_PAGE_SIZE - 1) & ~((uintptr_t)HUGE_PAGE_SIZE - 1);

	head_trim = aligned - raw;
	tail_trim = map_size - head_trim - size;

	if (head_trim > 0 && munmap(p, head_trim) != 0)
		DIE_ERRNO("munmap(head)");
	if (tail_trim > 0 &&
			munmap((void *)(aligned + size), tail_trim) != 0)
		DIE_ERRNO("munmap(tail)");

	return (void *)aligned;
}

/*
 * 解析调用者自己的 /proc/self/smaps，累加 [addr, addr+size) 这段映射
 * 对应 VMA 的 AnonHugePages 字段（单位 KB），返回值单位是字节。
 *
 * 注意：必须在当前进程里直接 fopen("/proc/self/smaps")，不能通过
 * system()/popen() 起子进程再 grep——那样 self 指向的是新起的 grep
 * 进程，不是我们自己，会永远读到不相关的地址空间。
 */
static uint64_t
read_anon_hugepages_for_range(void *addr, size_t size)
{
	FILE *f;
	char line[256];
	uintptr_t start = (uintptr_t)addr;
	uintptr_t end = start + size;
	int in_range = 0;
	uint64_t total_kb = 0;

	f = fopen("/proc/self/smaps", "r");
	if (f == NULL)
		DIE_ERRNO("fopen(/proc/self/smaps)");

	while (fgets(line, sizeof(line), f) != NULL) {
		unsigned long long range_start, range_end;

		if (sscanf(line, "%llx-%llx", &range_start, &range_end) == 2) {
			in_range = (range_start <= start && range_end >= end);
			continue;
		}

		if (in_range) {
			unsigned long long kb;

			if (sscanf(line, "AnonHugePages: %llu kB", &kb) == 1)
				total_kb += kb;
		}
	}

	fclose(f);

	return total_kb * 1024;
}

static void
fill_pattern(uint64_t *region, size_t count, uint64_t seed)
{
	size_t i;

	for (i = 0; i < count; i++)
		region[i] = seed + i;
}

static int
verify_pattern(const uint64_t *region, size_t count, uint64_t seed)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (region[i] != seed + i)
			return 0;
	}

	return 1;
}

int
main(void)
{
	uint64_t *region;
	size_t count = REGION_SIZE / sizeof(uint64_t);
	uint64_t hugepages_bytes;
	pid_t pids[NUM_WORKERS];
	int i;
	int all_ok = 1;

	/*
	 * 关键点：stdout 默认在非终端环境下是全缓冲的。如果 fork() 之前
	 * 父进程的 printf 还停留在 libc 缓冲区里没有真正 write() 出去，
	 * fork 会把这份缓冲区原样复制一份给每个子进程；子进程后面任何一次
	 * flush（包括 exit()/_exit() 前手动 fflush）都会把"父进程那些本该
	 * 只打印一次的内容"重复打印出来。改成行缓冲，让每条以 \n 结尾的
	 * printf 立刻落地，避免这个经典的"fork 复制了未刷新缓冲区"陷阱。
	 */
	setvbuf(stdout, NULL, _IOLBF, 0);

	printf("[hugepage] mapping %u MB (%u huge pages worth of alignment)\n",
		REGION_SIZE / (1024 * 1024), REGION_SIZE / HUGE_PAGE_SIZE);

	region = map_huge_aligned_private(REGION_SIZE);

	if (madvise(region, REGION_SIZE, MADV_HUGEPAGE) != 0)
		DIE_ERRNO("madvise(MADV_HUGEPAGE)");

	/* 预加载：一次性写满整段"只读数据"，之后所有 worker 都只读它 */
	fill_pattern(region, count, 42);

	hugepages_bytes = read_anon_hugepages_for_range(region, REGION_SIZE);
	printf("[hugepage] parent AnonHugePages covering this mapping = %"
		PRIu64 " / %u bytes (%.1f%%)\n",
		hugepages_bytes, REGION_SIZE,
		100.0 * (double)hugepages_bytes / (double)REGION_SIZE);

	if (hugepages_bytes > 0) {
		printf("[hugepage] SUCCESS: Transparent HugePage 生效，"
			"至少部分区域由 2MB 大页支持\n");
	} else {
		printf("[hugepage] NOTE: 本次运行未观察到大页生效（THP 依赖"
			"当时的内存碎片状况和内核决策，属已知的正常波动，"
			"不代表 mmap/madvise 调用失败）\n");
	}

	/*
	 * fork 出 N 个"只读 worker"：经典的预加载 + fork 模式。只要 worker
	 * 不写这段内存，物理页（以及大页承载）在 fork 之后不会被复制，
	 * 大家看到的是同一批物理大页。
	 */
	for (i = 0; i < NUM_WORKERS; i++) {
		pid_t pid = fork();

		if (pid < 0)
			DIE_ERRNO("fork");

		if (pid == 0) {
			int ok = verify_pattern(region, count, 42);
			uint64_t worker_hugepages =
				read_anon_hugepages_for_range(region,
					REGION_SIZE);

			printf("[hugepage worker %d] pattern=%s "
				"AnonHugePages=%" PRIu64 " bytes\n",
				i, ok ? "OK" : "MISMATCH", worker_hugepages);
			fflush(stdout);	/* _exit() 不会自动 flush stdio 缓冲区 */

			munmap(region, REGION_SIZE);
			_exit(ok ? 0 : 1);
		}

		pids[i] = pid;
	}

	for (i = 0; i < NUM_WORKERS; i++) {
		int status;

		waitpid(pids[i], &status, 0);
		if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0))
			all_ok = 0;
	}

	printf("[hugepage parent] all_workers_ok=%s\n",
		all_ok ? "yes" : "no");

	munmap(region, REGION_SIZE);

	return all_ok ? 0 : 1;
}
