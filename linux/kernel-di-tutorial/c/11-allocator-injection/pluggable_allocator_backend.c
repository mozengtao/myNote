/*
 * pluggable_allocator_backend.c
 *
 * 场景：模拟真实 Kernel "SLAB vs SLUB" 二选一的编译期 DI——
 * 本仓库 init/Kconfig 里有一个 `choice "Choose SLAB allocator"`，
 * SLAB（mm/slab.c）和 SLUB（mm/slub.c）是两套完全不同的内部实现
 * （SLAB 维护 per-CPU/per-node 队列做"缓存热"对象复用；SLUB 尽量减少
 * 元数据、依赖 cache line friendly 的紧凑布局），但对外导出**完全相同的
 * 函数签名**：`kmem_cache_create()`/`kmem_cache_alloc()`/`kmem_cache_free()`。
 * mm/Makefile 里用 `obj-$(CONFIG_SLAB) += slab.o` / `obj-$(CONFIG_SLUB) += slub.o`
 * 做二选一编译，两个 .o 文件永远不会同时链接进最终的 vmlinux。
 *
 * 本示例用 C 里最接近"编译期二选一，同一套接口"的手法复刻这个场景：
 *   - 用一个函数指针表 `struct allocator_backend`（对应"分配器 vtable"）
 *     在**程序启动时**（模拟"链接时选择了哪个 .o"）被赋值为其中一种实现，
 *     此后调用方统一通过 `active_backend->alloc(...)` 访问，完全不知道
 *     具体是哪种后端。
 *   - "SLAB 风格"后端：维护一个简单的空闲链表（模拟"队列化缓存"思路）。
 *   - "SLUB 风格"后端：直接用紧凑数组 + bitmap，强调"最小元数据开销"。
 *   - 用一个环境变量/命令行参数模拟 Kconfig 的"编译期选择"（因为真正的
 *     编译期条件编译在用户态演示意义不大，这里用"启动时刻只允许选一次、
 *     此后不可变"来传达同样的设计意图）。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o pluggable_allocator_backend pluggable_allocator_backend.c
 * 运行：./pluggable_allocator_backend          (默认 slab 风格)
 *      ./pluggable_allocator_backend slub      (slub 风格)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POOL_SIZE 8

/* 对外统一契约：调用方永远只认识这一套函数指针签名 */
struct allocator_backend {
	const char *name;
	void *(*alloc)(size_t size);
	void  (*free)(void *ptr);
	void  (*stats)(void);
};

/* ------------------------------------------------------------------ */
/* 后端 1："SLAB 风格" —— 维护一条空闲链表，强调对象复用（队列化缓存）    */
/* ------------------------------------------------------------------ */

struct slab_free_node {
	struct slab_free_node *next;
};

static unsigned char slab_pool[POOL_SIZE][64];
static struct slab_free_node *slab_freelist;
static int slab_pool_next = 0;
static long slab_alloc_count, slab_free_count, slab_reuse_count;

static void slab_style_init(void)
{
	slab_freelist = NULL;
	slab_pool_next = 0;
	slab_alloc_count = slab_free_count = slab_reuse_count = 0;
}

static void *slab_style_alloc(size_t size)
{
	(void)size;
	slab_alloc_count++;
	if (slab_freelist) {
		struct slab_free_node *n = slab_freelist;
		slab_freelist = n->next;
		slab_reuse_count++;
		return n;
	}
	if (slab_pool_next < POOL_SIZE)
		return slab_pool[slab_pool_next++];
	return NULL;
}

static void slab_style_free(void *ptr)
{
	struct slab_free_node *n = ptr;
	slab_free_count++;
	n->next = slab_freelist;   /* 挂回空闲链表头，下次优先复用（"缓存热"思路） */
	slab_freelist = n;
}

static void slab_style_stats(void)
{
	printf("  [slab-style] alloc=%ld free=%ld reused_from_freelist=%ld pool_used=%d/%d\n",
	       slab_alloc_count, slab_free_count, slab_reuse_count, slab_pool_next, POOL_SIZE);
}

/* ------------------------------------------------------------------ */
/* 后端 2："SLUB 风格" —— 紧凑数组 + bitmap，强调最小元数据开销          */
/* ------------------------------------------------------------------ */

static unsigned char slub_pool[POOL_SIZE][64];
static unsigned char slub_used_bitmap[POOL_SIZE];
static long slub_alloc_count, slub_free_count, slub_bitmap_scans;

static void slub_style_init(void)
{
	memset(slub_used_bitmap, 0, sizeof(slub_used_bitmap));
	slub_alloc_count = slub_free_count = slub_bitmap_scans = 0;
}

static void *slub_style_alloc(size_t size)
{
	(void)size;
	int i;
	slub_alloc_count++;
	for (i = 0; i < POOL_SIZE; i++) {
		slub_bitmap_scans++;
		if (!slub_used_bitmap[i]) {
			slub_used_bitmap[i] = 1;
			return slub_pool[i];
		}
	}
	return NULL;
}

static void slub_style_free(void *ptr)
{
	int i;
	slub_free_count++;
	for (i = 0; i < POOL_SIZE; i++) {
		if (slub_pool[i] == ptr) {
			slub_used_bitmap[i] = 0;
			return;
		}
	}
}

static void slub_style_stats(void)
{
	int used = 0, i;
	for (i = 0; i < POOL_SIZE; i++)
		used += slub_used_bitmap[i];
	printf("  [slub-style] alloc=%ld free=%ld bitmap_scans=%ld currently_used=%d/%d\n",
	       slub_alloc_count, slub_free_count, slub_bitmap_scans, used, POOL_SIZE);
}

/* ------------------------------------------------------------------ */
/* "编译期选择"的模拟：一旦选定，运行期不可再切换                       */
/* ------------------------------------------------------------------ */

static const struct allocator_backend backend_slab_style = {
	.name = "SLAB-style", .alloc = slab_style_alloc, .free = slab_style_free, .stats = slab_style_stats,
};
static const struct allocator_backend backend_slub_style = {
	.name = "SLUB-style", .alloc = slub_style_alloc, .free = slub_style_free, .stats = slub_style_stats,
};

/* 对应 kmem_cache_alloc()/kmem_cache_free()：调用方统一入口，不知道具体后端 */
static const struct allocator_backend *active_backend;

static void *generic_alloc(size_t size) { return active_backend->alloc(size); }
static void  generic_free(void *ptr)    { active_backend->free(ptr); }

int main(int argc, char **argv)
{
	const char *choice = (argc > 1) ? argv[1] : "slab";

	printf("=== pluggable_allocator_backend demo ===\n\n");

	/*
	 * 这一步模拟真实 Kernel 里 Kconfig "choice" 只能二选一、
	 * 且选择结果在整个内核镜像的生命周期内不可更改——这里用
	 * "启动时刻决定一次，此后 active_backend 不再改变"来对应。
	 */
	if (strcmp(choice, "slub") == 0) {
		slub_style_init();
		active_backend = &backend_slub_style;
	} else {
		slab_style_init();
		active_backend = &backend_slab_style;
	}
	printf("[config] selected backend = %s (等价于 CONFIG_SLAB / CONFIG_SLUB 二选一)\n\n",
	       active_backend->name);

	printf("-- 分配 5 个对象 --\n");
	void *objs[5];
	int i;
	for (i = 0; i < 5; i++) {
		objs[i] = generic_alloc(64);
		printf("  generic_alloc() -> %p\n", objs[i]);
	}

	printf("\n-- 归还其中 2 个，再分配 2 个新的（验证复用/分配策略的差异）--\n");
	generic_free(objs[1]);
	generic_free(objs[3]);
	void *o5 = generic_alloc(64);
	void *o6 = generic_alloc(64);
	printf("  new alloc -> %p, %p\n", o5, o6);

	printf("\n-- 后端内部统计（不同后端记录的指标完全不同，调用方从不关心）--\n");
	active_backend->stats();

	printf("\n=== done ===\n");
	printf("调用方代码（generic_alloc/generic_free）从头到尾没有出现任何\n");
	printf("if (backend == SLAB) ... else ... 的分支判断。\n");
	return 0;
}
