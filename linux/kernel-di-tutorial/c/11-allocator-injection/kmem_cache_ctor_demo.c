/*
 * kmem_cache_ctor_demo.c
 *
 * 场景：从零实现一个简化版 kmem_cache，重点复刻真实 Kernel
 * include/linux/slab.h 的 kmem_cache_create() 签名——最后一个参数是
 * 一个"构造函数"回调 ctor，对象每次被"新分配"出来时都会被自动调用一次。
 *
 * 对应真实 Kernel 语义（mm/slab.c cache_init_objs()）：
 *   - ctor 只在"这块内存第一次被组织成一个对象"时调用一次
 *     （对应 slab 刚创建、对象第一次被放进 slab 页时）。
 *   - 之后对象被 kmem_cache_free() 归还、再被 kmem_cache_alloc() 取出复用时，
 *     ctor **不会**再被调用——因为它已经是一个"构造好的对象"，只是内容
 *     可能是上一次使用留下的脏数据，调用方要自己决定要不要重新初始化字段。
 *
 * 本示例包含两个对象类型：
 *   - task_obj：ctor 初始化一个"魔数"字段，用来验证"只在真正新分配时调用一次"。
 *   - buffer_obj：ctor 把缓冲区清零，模拟真实 Kernel 里"归还给内核的内存
 *     不能包含上一个使用者的敏感数据"这类安全动机。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o kmem_cache_ctor_demo kmem_cache_ctor_demo.c
 * 运行：./kmem_cache_ctor_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POOL_CAPACITY 8

/*
 * struct kmem_cache —— 对应真实 struct kmem_cache（mm/slab.c 内部定义）。
 * 这里只保留驱动本示例所需的字段：对象大小、ctor 回调、一个简化的
 * "空闲链表 + 已分配内存池"（真实 slab 用 slab 页 + bufctl 数组）。
 */
struct kmem_cache {
	char   name[32];
	size_t obj_size;
	void (*ctor)(void *);   /* 对应真实 kmem_cache_create() 的最后一个参数 */

	void  *pool[POOL_CAPACITY];       /* 已经从堆上批量申请好的原始内存块 */
	int    pool_used[POOL_CAPACITY];  /* 0=从未构造过，1=已构造过(可能空闲也可能在用) */
	int    in_use[POOL_CAPACITY];     /* 1=当前被调用者持有 */
	int    nr_slots;
	long   ctor_calls;                /* 统计 ctor 实际被调用了多少次 */
};

/*
 * kmem_cache_create()：签名严格对应真实 include/linux/slab.h:101
 *   struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
 *                                        unsigned long, void (*)(void *));
 * 这里省略 align/flags 两个与本示例无关的参数，保留核心的
 * name/size/ctor 三者。
 */
static struct kmem_cache *kmem_cache_create(const char *name, size_t size,
					    void (*ctor)(void *))
{
	struct kmem_cache *cachep = calloc(1, sizeof(*cachep));
	snprintf(cachep->name, sizeof(cachep->name), "%s", name);
	cachep->obj_size = size;
	cachep->ctor = ctor;   /* <-- DI 的核心动作：把"对象初始化逻辑"注入到通用分配框架里 */
	printf("[slab] kmem_cache_create('%s', size=%zu) ctor=%s\n",
	       name, size, ctor ? "yes" : "(none)");
	return cachep;
}

/* 对应 mm/slab.c cache_init_objs()：只在"第一次把内存组织成对象"时调用 ctor */
static void *kmem_cache_alloc(struct kmem_cache *cachep)
{
	int i;
	for (i = 0; i < cachep->nr_slots; i++) {
		if (!cachep->in_use[i]) {
			if (!cachep->pool_used[i]) {
				/* 第一次被组织成对象：调用 ctor（如果有） */
				if (cachep->ctor) {
					cachep->ctor(cachep->pool[i]);   /* <-- 盲调 */
					cachep->ctor_calls++;
				}
				cachep->pool_used[i] = 1;
			}
			cachep->in_use[i] = 1;
			return cachep->pool[i];
		}
	}
	/* 池已满，扩容一个新 slot（真实 slab 会分配新的一页） */
	if (cachep->nr_slots < POOL_CAPACITY) {
		int slot = cachep->nr_slots++;
		cachep->pool[slot] = malloc(cachep->obj_size);
		if (cachep->ctor) {
			cachep->ctor(cachep->pool[slot]);
			cachep->ctor_calls++;
		}
		cachep->pool_used[slot] = 1;
		cachep->in_use[slot] = 1;
		return cachep->pool[slot];
	}
	return NULL;
}

/* kmem_cache_free()：归还对象，但内存本身不释放，ctor 不会被再次调用 */
static void kmem_cache_free(struct kmem_cache *cachep, void *objp)
{
	int i;
	for (i = 0; i < cachep->nr_slots; i++) {
		if (cachep->pool[i] == objp) {
			cachep->in_use[i] = 0;   /* 归还，但 pool_used 保持 1 */
			return;
		}
	}
}

static void kmem_cache_destroy(struct kmem_cache *cachep)
{
	int i;
	for (i = 0; i < cachep->nr_slots; i++)
		free(cachep->pool[i]);
	printf("[slab] kmem_cache_destroy('%s'), ctor was called %ld time(s) total\n",
	       cachep->name, cachep->ctor_calls);
	free(cachep);
}

/* ------------------------------------------------------------------ */
/* 业务对象 1：task_obj，用魔数验证"只在真正新分配时构造一次"           */
/* ------------------------------------------------------------------ */

#define TASK_MAGIC 0xA5A5

struct task_obj {
	int  magic;
	char name[16];
	int  run_count;
};

static void task_obj_ctor(void *p)
{
	struct task_obj *t = p;
	t->magic = TASK_MAGIC;
	t->run_count = 0;
	printf("      [ctor] task_obj constructed at %p (magic set)\n", p);
}

/* ------------------------------------------------------------------ */
/* 业务对象 2：buffer_obj，ctor 负责清零，模拟安全动机                  */
/* ------------------------------------------------------------------ */

struct buffer_obj {
	unsigned char data[32];
};

static void buffer_obj_ctor(void *p)
{
	struct buffer_obj *b = p;
	memset(b->data, 0, sizeof(b->data));
	printf("      [ctor] buffer_obj constructed at %p (zeroed)\n", p);
}

int main(void)
{
	struct kmem_cache *task_cache = kmem_cache_create("task_obj", sizeof(struct task_obj), task_obj_ctor);
	struct kmem_cache *buf_cache  = kmem_cache_create("buffer_obj", sizeof(struct buffer_obj), buffer_obj_ctor);

	printf("\n=== kmem_cache_ctor_demo ===\n\n");

	printf("-- 第一次分配 3 个 task_obj：应该触发 3 次 ctor --\n");
	struct task_obj *t1 = kmem_cache_alloc(task_cache);
	struct task_obj *t2 = kmem_cache_alloc(task_cache);
	struct task_obj *t3 = kmem_cache_alloc(task_cache);
	snprintf(t1->name, sizeof(t1->name), "alpha");
	snprintf(t2->name, sizeof(t2->name), "beta");
	snprintf(t3->name, sizeof(t3->name), "gamma");
	t1->run_count = 5;

	printf("\n-- 归还 t1，再次分配一个新对象：期望复用 t1 的内存，且 ctor 不再被调用 --\n");
	kmem_cache_free(task_cache, t1);
	struct task_obj *t4 = kmem_cache_alloc(task_cache);
	printf("  t4 == t1(旧地址复用)? %s ; t4->run_count（未被 ctor 重置，应保留旧值 5）= %d\n",
	       (t4 == t1) ? "yes" : "no", t4->run_count);
	printf("  ctor_calls so far (预期仍是 3，因为复用没有触发新的构造) = %ld\n", task_cache->ctor_calls);

	printf("\n-- buffer_obj：验证 ctor 只在第一次构造时清零，复用时不会重新清零 --\n");
	struct buffer_obj *b1 = kmem_cache_alloc(buf_cache);      /* 新 slot，ctor 清零 */
	printf("  b1->data[0] (首次构造，ctor 清零后应为 0) = %d\n", b1->data[0]);
	memset(b1->data, 0xFF, sizeof(b1->data));                 /* 业务代码把缓冲区填满脏数据 */
	kmem_cache_free(buf_cache, b1);
	struct buffer_obj *b2 = kmem_cache_alloc(buf_cache);      /* 复用 b1 的 slot，ctor 不会再被调用 */
	printf("  b2->data[0] (复用旧内存，ctor 未被重新调用，应残留脏数据 255) = %d\n", b2->data[0]);
	printf("  ctor_calls so far (预期仍是 1，复用没有触发新的构造) = %ld\n", buf_cache->ctor_calls);
	struct buffer_obj *b3 = kmem_cache_alloc(buf_cache);      /* 池里没有空闲 slot，扩容出新 slot */
	printf("  b3->data[0] (全新 slot，ctor 清零后应为 0) = %d\n", b3->data[0]);
	printf("  ctor_calls so far (预期变为 2，新 slot 触发了新的构造) = %ld\n", buf_cache->ctor_calls);

	printf("\n=== done ===\n");
	kmem_cache_destroy(task_cache);
	kmem_cache_destroy(buf_cache);
	return 0;
}
