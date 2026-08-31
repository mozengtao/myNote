/*
 * numa_demo.c
 *
 * NUMA-Aware Shared Memory
 *
 * 心智模型：多路服务器上每个 CPU 插槽（socket）通常挂着一部分"本地"
 * 内存，访问本地内存比访问挂在别的 CPU 插槽下的"远端"内存快得多
 * （NUMA = Non-Uniform Memory Access）。原文档第 12 节画的图示是：
 * Node0 的 CPU 配一份绑定在 Node0 上的共享内存，Node1 配一份绑定在
 * Node1 上的——原则是"CPU 尽量只访问本地 NUMA 节点的内存"，减少
 * Remote Memory Access。
 *
 * 标准做法是用 `libnuma` 提供的 `numa_alloc_onnode(size, node)`
 * 在指定节点分配内存，再用 `numa_run_on_node(node)` /
 * `sched_setaffinity` 把处理这份内存的线程/进程也绑定到同一个节点的
 * CPU 上，让"计算"和"数据"在物理上尽量靠在一起。
 *
 * 本机环境（见 plan）：只有 1 个 NUMA node，且未安装 libnuma。
 * 因此本示例用 `#ifdef HAVE_LIBNUMA` 条件编译：
 *   - 装了 libnuma：调用真正的 `numa_alloc_onnode()` /
 *     `numa_run_on_node()`，按检测到的节点数量各分配一份内存并各自
 *     验证；
 *   - 没装 libnuma（本机情况）：自动降级为普通 `malloc()`，把节点数
 *     视为 1，并打印明确的提示——"当前环境仅 1 个 NUMA node / 未安装
 *     libnuma，本示例退化为单节点模式"。
 *
 * 无论走哪条路径，代码结构（"按节点分配 → fork 出绑定到该节点的 worker
 * → 验证数据"）完全一致，这正是本示例想传达的心智模型：NUMA-aware
 * 只是在"分配内存"和"调度执行"这两步各加一个"绑定到节点 N"的提示，
 * 不需要改变共享内存本身的读写逻辑。
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../common/shm_common.h"

#ifdef HAVE_LIBNUMA
#include <numa.h>
#endif

#define REGION_SIZE (16u * 1024 * 1024)	/* 16MB per-node */

static void *
node_alloc(size_t size, int node)
{
#ifdef HAVE_LIBNUMA
	void *p = numa_alloc_onnode(size, node);

	if (p == NULL)
		DIE_ERRNO("numa_alloc_onnode");
	return p;
#else
	(void)node;
	return malloc(size);
#endif
}

static void
node_free(void *p, size_t size)
{
#ifdef HAVE_LIBNUMA
	numa_free(p, size);
#else
	(void)size;
	free(p);
#endif
}

/* 把当前进程（在 worker 里调用，fork 之后）绑定到指定 NUMA 节点上的
 * CPU，让计算尽量靠近数据所在的节点。 */
static void
bind_current_process_to_node(int node)
{
#ifdef HAVE_LIBNUMA
	if (numa_run_on_node(node) != 0)
		fprintf(stderr,
			"[numa] warning: numa_run_on_node(%d) failed\n",
			node);
#else
	(void)node;
	/* 没有 libnuma 时，退化为不做任何 CPU 亲和性绑定 */
#endif
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
	int num_nodes;
	int node;
	int all_ok = 1;

	setvbuf(stdout, NULL, _IOLBF, 0);	/* 避免 fork 复制未 flush 的缓冲区，见 10_hugepage 的踩坑记录 */

#ifdef HAVE_LIBNUMA
	if (numa_available() < 0) {
		printf("[numa] libnuma 已链接，但内核报告 NUMA 不可用，"
			"退化为单节点模式\n");
		num_nodes = 1;
	} else {
		num_nodes = numa_max_node() + 1;
		printf("[numa] libnuma 可用，检测到 %d 个 NUMA node\n",
			num_nodes);
	}
#else
	printf("[numa] 当前环境未安装 libnuma（或编译时未启用），"
		"自动降级为 malloc + 单节点模式\n");
	num_nodes = 1;
#endif

	for (node = 0; node < num_nodes; node++) {
		size_t count = REGION_SIZE / sizeof(uint64_t);
		uint64_t *region = node_alloc(REGION_SIZE, node);
		pid_t pid;

		if (region == NULL)
			DIE_ERRNO("allocate per-node region");

		fill_pattern(region, count, (uint64_t)(node + 1) * 1000ull);

		printf("[numa] node %d: allocated %u bytes, filled pattern\n",
			node, REGION_SIZE);

		pid = fork();
		if (pid < 0)
			DIE_ERRNO("fork");

		if (pid == 0) {
			int ok;

			bind_current_process_to_node(node);
			ok = verify_pattern(region, count,
				(uint64_t)(node + 1) * 1000ull);

			printf("[numa worker node %d] pattern=%s\n",
				node, ok ? "OK" : "MISMATCH");

			node_free(region, REGION_SIZE);
			_exit(ok ? 0 : 1);
		}

		{
			int status;

			waitpid(pid, &status, 0);
			if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0))
				all_ok = 0;
		}

		node_free(region, REGION_SIZE);
	}

	printf("[numa] all_nodes_ok=%s\n", all_ok ? "yes" : "no");

	return all_ok ? 0 : 1;
}
