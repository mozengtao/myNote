/*
 * custom_arena_allocator.c
 *
 * 场景：一个通用链表容器（generic_list）需要不断分配/释放节点，
 * "节点用什么分配器分配"被做成一个可注入的契约（struct node_allocator），
 * 容器代码本身完全不关心背后是 libc malloc/free 还是一个自定义的
 * arena（线性分配、批量释放）。
 *
 * 这是运行期 DI（02 篇 Ops Table 的直接应用）与本篇"分配器"主题的交叉：
 * 09/10 篇讨论的是内核对象模型和调度器，本例把同样的"契约 + 注入"思路
 * 落到"内存分配"这个更底层的关注点上——对应真实 Kernel 里
 * `kmalloc()`（走 slab/slub） vs 某些子系统自己维护的 per-CPU/per-request
 * 内存池（比如网络子系统的 skb 池、块设备层的 bio 池）：
 * 通用容器/协议栈代码只认识 "alloc_node/free_node" 这套契约，
 * 不关心内存来自哪里。
 *
 * 两种分配器实现：
 *   - malloc_backed_allocator：直接转发到 libc malloc/free（对应
 *     `kmalloc`/`kfree` 直接使用通用 slab 分配器的场景）。
 *   - arena_allocator：预先申请一大块内存，用简单的 bump pointer
 *     线性分配，free() 只是标记，真正的内存回收要等到 arena_reset()
 *     批量归还（对应某些高频分配场景为了避免频繁 kmalloc/kfree 开销，
 *     自己维护一个"请求生命周期内"的 arena，请求结束时一次性释放）。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o custom_arena_allocator custom_arena_allocator.c
 * 运行：./custom_arena_allocator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* 通用容器：单链表节点 + 契约                                          */
/* ------------------------------------------------------------------ */

struct list_node {
	int  value;
	struct list_node *next;
};

/* struct node_allocator —— 容器唯一认识的契约，类似一份微型 kmem_cache */
struct node_allocator {
	const char *name;
	void *(*alloc)(struct node_allocator *self, size_t size);
	void  (*free)(struct node_allocator *self, void *ptr);
	void  *priv;   /* Context Injection（03 篇）：分配器自己的私有状态 */
};

struct generic_list {
	struct list_node *head;
	int nr_nodes;
	struct node_allocator *allocator;   /* <-- 被注入的分配策略 */
};

static void list_init(struct generic_list *list, struct node_allocator *allocator)
{
	list->head = NULL;
	list->nr_nodes = 0;
	list->allocator = allocator;
}

static void list_push_front(struct generic_list *list, int value)
{
	struct list_node *n = list->allocator->alloc(list->allocator, sizeof(*n)); /* <-- 盲调 */
	n->value = value;
	n->next = list->head;
	list->head = n;
	list->nr_nodes++;
}

static int list_pop_front(struct generic_list *list)
{
	struct list_node *n = list->head;
	int value = n->value;
	list->head = n->next;
	list->allocator->free(list->allocator, n);   /* <-- 盲调 */
	list->nr_nodes--;
	return value;
}

static void list_print(struct generic_list *list)
{
	struct list_node *n;
	printf("  [");
	for (n = list->head; n; n = n->next)
		printf("%d%s", n->value, n->next ? "," : "");
	printf("] (nr_nodes=%d)\n", list->nr_nodes);
}

/* ------------------------------------------------------------------ */
/* 分配器实现 1：直接转发到 libc malloc/free                            */
/* ------------------------------------------------------------------ */

struct malloc_stats { long alloc_calls, free_calls; };

static void *malloc_backed_alloc(struct node_allocator *self, size_t size)
{
	struct malloc_stats *st = self->priv;
	st->alloc_calls++;
	return malloc(size);
}
static void malloc_backed_free(struct node_allocator *self, void *ptr)
{
	struct malloc_stats *st = self->priv;
	st->free_calls++;
	free(ptr);
}

/* ------------------------------------------------------------------ */
/* 分配器实现 2：arena —— 线性分配 + 批量重置，free() 只是记账            */
/* ------------------------------------------------------------------ */

#define ARENA_CAPACITY 4096

struct arena_state {
	unsigned char buf[ARENA_CAPACITY];
	size_t offset;
	long   alloc_calls, free_calls_ignored, reset_count;
};

static void *arena_alloc(struct node_allocator *self, size_t size)
{
	struct arena_state *a = self->priv;
	a->alloc_calls++;
	if (a->offset + size > ARENA_CAPACITY) {
		fprintf(stderr, "arena_alloc: out of space\n");
		return NULL;
	}
	void *p = a->buf + a->offset;
	a->offset += size;
	return p;
}

/* arena 的 free() 故意什么都不做：内存只能通过 arena_reset() 批量归还，
 * 这正是本示例要强调的"分配策略差异"——容器代码调用 free() 的方式完全一样，
 * 但底层行为可以天差地别 */
static void arena_free(struct node_allocator *self, void *ptr)
{
	(void)ptr;
	struct arena_state *a = self->priv;
	a->free_calls_ignored++;
}

static void arena_reset(struct arena_state *a)
{
	a->offset = 0;
	a->reset_count++;
	printf("  [arena] reset: all %ld previous alloc(s) reclaimed in O(1)\n", a->alloc_calls);
}

int main(void)
{
	struct malloc_stats mstats = {0};
	struct arena_state  astate = {0};

	struct node_allocator malloc_allocator = {
		.name = "malloc-backed", .alloc = malloc_backed_alloc, .free = malloc_backed_free, .priv = &mstats,
	};
	struct node_allocator arena_alloc_backend = {
		.name = "arena", .alloc = arena_alloc, .free = arena_free, .priv = &astate,
	};

	printf("=== custom_arena_allocator demo ===\n\n");

	printf("-- 场景 1：容器使用 malloc-backed 分配器 --\n");
	struct generic_list list1;
	list_init(&list1, &malloc_allocator);
	list_push_front(&list1, 1);
	list_push_front(&list1, 2);
	list_push_front(&list1, 3);
	list_print(&list1);
	list_pop_front(&list1);
	list_print(&list1);
	printf("  malloc_stats: alloc_calls=%ld free_calls=%ld\n", mstats.alloc_calls, mstats.free_calls);

	printf("\n-- 场景 2：另一个容器实例使用 arena 分配器（同一份 generic_list 代码）--\n");
	struct generic_list list2;
	list_init(&list2, &arena_alloc_backend);
	list_push_front(&list2, 10);
	list_push_front(&list2, 20);
	list_push_front(&list2, 30);
	list_push_front(&list2, 40);
	list_print(&list2);
	list_pop_front(&list2);   /* arena 的 free 只是记账，内存并未真正回收 */
	list_pop_front(&list2);
	list_print(&list2);
	printf("  arena_state: alloc_calls=%ld free_calls_ignored=%ld offset=%zu/%d bytes\n",
	       astate.alloc_calls, astate.free_calls_ignored, astate.offset, ARENA_CAPACITY);

	printf("\n-- arena 特有操作：批量重置，一次性回收所有节点占用的内存 --\n");
	arena_reset(&astate);
	printf("  arena_state after reset: offset=%zu (回到 0，可以重新线性分配)\n", astate.offset);

	printf("\n-- 验证：list1 (malloc) 与 list2 (arena) 使用同一份 list_push_front/list_pop_front 代码 --\n");
	printf("  list1.allocator->name = %s\n", list1.allocator->name);
	printf("  list2.allocator->name = %s\n", list2.allocator->name);

	printf("\n=== done ===\n");
	return 0;
}
