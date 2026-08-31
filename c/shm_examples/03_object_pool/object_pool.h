/*
 * object_pool.h
 *
 * 无锁共享对象池：对象本身预先分配在共享内存里，分配/释放只是
 * 从一个无锁 Free List 上 pop/push 一个下标，彻底避免运行期
 * malloc()/free()（这两个系统调用在共享内存场景下也用不了——
 * 共享内存里天然没有堆分配器）。
 *
 * 心智模型要点：
 *   1. 共享内存会被不同进程映射到不同的虚拟地址，所以 Free List
 *      绝不能用裸指针串联，必须用数组下标（等价于 offset）代替指针。
 *   2. 经典 Treiber Stack（CAS 单链表）在多进程下有 ABA 问题：
 *      线程 A 读到 head=X, next=Y 之后被换出；期间 X 被别人 pop 出去
 *      又 push 回来（可能 next 已经变了）；A 醒来后 CAS 仍然"看起来"
 *      成功，但链表已经被破坏。
 *   3. 解决办法：给 head 附加一个只增不减的 generation 计数器，
 *      一起塞进同一个 64 位原子变量里做 CAS（"tagged index"）。
 *      只要 generation 没被 32 位整数环绕撞车（现实中几乎不可能），
 *      就能保证"链表没被动过"。
 */
#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

#include <stdint.h>
#include <stdatomic.h>
#include "../common/shm_common.h"

#define POOL_CAPACITY 4u
#define EMPTY_IDX 0xFFFFFFFFu

struct object_slot {
	uint32_t next;		/* 仅在空闲时有意义：free list 链接 */
	_Atomic uint32_t in_use;	/* 0=空闲 1=已分配，用于检测重复分配的 bug */
	uint64_t payload;
};

struct object_pool {
	_Atomic uint64_t head_tagged CACHE_ALIGNED;	/* 高32位=generation，低32位=index */
	uint32_t capacity;

	_Atomic uint64_t total_alloc CACHE_ALIGNED;
	_Atomic uint64_t total_free CACHE_ALIGNED;
	_Atomic uint32_t double_alloc_bugs CACHE_ALIGNED;
	_Atomic uint32_t pool_exhausted_events CACHE_ALIGNED;

	struct object_slot slots[POOL_CAPACITY];
};

#define POOL_SHM_NAME "/shm_examples_object_pool"

static inline uint64_t
pool_pack(uint32_t gen, uint32_t idx)
{
	return ((uint64_t)gen << 32) | idx;
}

static inline struct object_pool *
pool_create(void)
{
	struct object_pool *pool;
	uint32_t i;

	pool = shm_map(POOL_SHM_NAME, sizeof(*pool), 1);

	pool->capacity = POOL_CAPACITY;
	atomic_store_explicit(&pool->total_alloc, 0, memory_order_relaxed);
	atomic_store_explicit(&pool->total_free, 0, memory_order_relaxed);
	atomic_store_explicit(&pool->double_alloc_bugs, 0,
		memory_order_relaxed);
	atomic_store_explicit(&pool->pool_exhausted_events, 0,
		memory_order_relaxed);

	/* 初始把所有槽位串成一条 free list：0 -> 1 -> 2 -> ... -> EMPTY_IDX */
	for (i = 0; i < POOL_CAPACITY; i++) {
		pool->slots[i].next =
			(i + 1 == POOL_CAPACITY) ? EMPTY_IDX : (i + 1);
		atomic_store_explicit(&pool->slots[i].in_use, 0,
			memory_order_relaxed);
		pool->slots[i].payload = 0;
	}

	atomic_store_explicit(&pool->head_tagged, pool_pack(0, 0),
		memory_order_relaxed);

	return pool;
}

static inline struct object_pool *
pool_attach(void)
{
	return shm_map(POOL_SHM_NAME, sizeof(struct object_pool), 0);
}

/*
 * 分配一个对象，返回其下标；池空时返回 EMPTY_IDX。
 */
static inline uint32_t
pool_alloc(struct object_pool *pool)
{
	uint64_t old, new_val;
	uint32_t idx, gen, next_idx;

	old = atomic_load_explicit(&pool->head_tagged, memory_order_acquire);

	for (;;) {
		idx = (uint32_t)(old & 0xFFFFFFFFu);
		if (idx == EMPTY_IDX)
			return EMPTY_IDX;	/* 池已耗尽 */

		gen = (uint32_t)(old >> 32);
		next_idx = pool->slots[idx].next;
		new_val = pool_pack(gen + 1, next_idx);

		if (atomic_compare_exchange_weak_explicit(&pool->head_tagged,
			&old, new_val, memory_order_acq_rel,
			memory_order_acquire))
			break;
	}

	atomic_fetch_add_explicit(&pool->total_alloc, 1,
		memory_order_relaxed);

	return idx;
}

/*
 * 释放一个对象下标回池中。
 */
static inline void
pool_free(struct object_pool *pool, uint32_t idx)
{
	uint64_t old, new_val;
	uint32_t gen;

	old = atomic_load_explicit(&pool->head_tagged, memory_order_acquire);

	for (;;) {
		gen = (uint32_t)(old >> 32);
		pool->slots[idx].next = (uint32_t)(old & 0xFFFFFFFFu);
		new_val = pool_pack(gen + 1, idx);

		if (atomic_compare_exchange_weak_explicit(&pool->head_tagged,
			&old, new_val, memory_order_acq_rel,
			memory_order_acquire))
			break;
	}

	atomic_fetch_add_explicit(&pool->total_free, 1, memory_order_relaxed);
}

#endif /* OBJECT_POOL_H */
