/*
 * 02_pool_freelist_reuse.c
 *
 * 场景：网络会话(session)对象，按 32 位 session id 建哈希索引。
 *
 * 模式：内存池 + freelist，按需分块扩容（不逐个 malloc），并且让
 * "空闲链(freelist)"和"哈希桶链"复用同一个 next 指针字段——因为一个
 * session 在任意时刻只可能处于"空闲"或"使用中"两种互斥状态之一，
 * 两条链表不会同时需要这个字段。
 *
 * 编译： gcc -std=c11 -Wall -Wextra -O2 -o /tmp/ex2 02_pool_freelist_reuse.c
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	NSESSHASH = 32,	/* 哈希桶个数，2 的幂 */
	CHUNK     = 4,	/* 每次扩容分配的节点个数 */
};

typedef struct Session Session;

struct Session {
	uint32_t id;
	int      active;
	Session *next;	/* 复用字段：空闲时是 freelist 的 next，使用中是哈希桶链的 next */
};

static Session *sesshash[NSESSHASH];
static Session *freelist;
static int       growcount;	/* 记录整块扩容发生了几次，便于自测 */

static unsigned
sesshashfn(uint32_t id)
{
	return id & (NSESSHASH - 1);
}

/*
 * pool_grow - 整块申请 CHUNK 个节点，串成一条 freelist
 *
 * 这块内存故意不记录到任何"可释放"的列表里：这类池在真实系统里
 * 通常和进程生命周期等长，从不整体回收给 OS。
 */
static int
pool_grow(void)
{
	Session *chunk;
	int i;

	chunk = malloc(CHUNK * sizeof(*chunk));
	if(chunk == NULL)
		return -1;

	memset(chunk, 0, CHUNK * sizeof(*chunk));

	for(i = 0; i < CHUNK - 1; i++)
		chunk[i].next = &chunk[i + 1];
	chunk[CHUNK - 1].next = freelist;
	freelist = &chunk[0];

	growcount++;
	return 0;
}

/*
 * sessionalloc - 从 freelist 摘一个节点，freelist 空了就整块扩容
 */
Session*
sessionalloc(uint32_t id)
{
	Session *s, **pp;

	if(freelist == NULL) {
		if(pool_grow() != 0)
			return NULL;
	}

	s = freelist;
	freelist = s->next;

	s->id = id;
	s->active = 1;

	pp = &sesshash[sesshashfn(id)];
	s->next = *pp;
	*pp = s;

	return s;
}

/*
 * sessionlook - 按 id 查找活跃 session
 */
Session*
sessionlook(uint32_t id)
{
	Session *s;

	for(s = sesshash[sesshashfn(id)]; s != NULL; s = s->next) {
		if(s->id == id)
			break;
	}

	return s;
}

/*
 * sessionfree - 从哈希桶链摘除，放回 freelist（复用同一个 next 字段）
 */
void
sessionfree(Session *s)
{
	Session *f, **pp;

	if(s == NULL)
		return;

	pp = &sesshash[sesshashfn(s->id)];
	for(f = *pp; f != NULL; f = f->next) {
		if(f == s) {
			*pp = s->next;
			break;
		}
		pp = &f->next;
	}
	if(f == NULL)
		return;	/* 不在哈希表里，大概率是重复释放，直接忽略 */

	s->active = 0;
	s->next = freelist;
	freelist = s;
}

int
main(void)
{
	Session *sessions[10];
	int i;

	assert(growcount == 0);

	/* CHUNK=4，分配 10 个 session 需要触发 3 次整块扩容(4+4+2) */
	for(i = 0; i < 10; i++) {
		sessions[i] = sessionalloc(1000 + (uint32_t)i);
		assert(sessions[i] != NULL);
	}
	assert(growcount == 3);
	printf("allocated 10 sessions with %d pool_grow() calls\n", growcount);

	for(i = 0; i < 10; i++)
		assert(sessionlook(1000 + (uint32_t)i) == sessions[i]);

	/* 释放 5 个，验证它们从哈希表里消失 */
	for(i = 0; i < 5; i++)
		sessionfree(sessions[i]);
	for(i = 0; i < 5; i++)
		assert(sessionlook(1000 + (uint32_t)i) == NULL);
	for(i = 5; i < 10; i++)
		assert(sessionlook(1000 + (uint32_t)i) == sessions[i]);

	/* 重新分配 5 个，应该复用刚才释放的节点，不应该再触发扩容 */
	for(i = 0; i < 5; i++) {
		Session *s = sessionalloc(2000 + (uint32_t)i);
		assert(s != NULL);
		assert(sessionlook(2000 + (uint32_t)i) == s);
	}
	assert(growcount == 3);
	printf("reused freed nodes without any extra pool_grow() call\n");

	printf("02_pool_freelist_reuse: all assertions passed\n");
	return 0;
}

/*
 * 心智模型
 *
 * 1. 生命周期状态互斥 ⇒ 内存可复用：session 只可能"空闲"或"使用中"，
 *    从来不会两者皆是，所以 freelist 和哈希桶链可以共用 next 字段——
 *    这是没有 tagged union 语法糖时，用状态机保证内存安全复用的手工技巧。
 * 2. 静态内存预算，但允许分块增长：不逐个 malloc（避免频繁系统调用和
 *    堆碎片），也不一次性预留最大容量（避免浪费），用整块 CHUNK 扩容
 *    在两者之间取一个折中。
 */
