/*
 * hash_table.h
 *
 * 多进程共享的哈希表：链地址法解决冲突，每个 bucket 一把独立的
 * 进程间共享互斥锁（而不是给整张表加一把大锁），减少锁竞争。
 *
 * 心智模型要点：
 *   1. 链表节点存放在共享内存的固定数组里，节点之间用下标
 *      （而不是指针）互相链接——原因和 03_object_pool 一样：
 *      共享内存在不同进程里的映射基址不同。
 *   2. 节点的分配/回收复用 03_object_pool 里的无锁 tagged-index
 *      Free List 技巧（跨进程复用同一套心智模型）。
 *   3. bucket 级别加锁：只有落在同一个 bucket 的 key 才会互相阻塞，
 *      不同 bucket 之间完全并行，这是"减少锁竞争"的关键设计。
 *   4. pthread_mutex 要显式设置 PTHREAD_PROCESS_SHARED 属性，
 *      否则跨进程加锁是未定义行为（默认属性只保证进程内线程间可用）。
 */
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include "../common/shm_common.h"

#define NUM_BUCKETS  64u
#define MAX_NODES    16384u
#define EMPTY_IDX    0xFFFFFFFFu

struct hash_node {
	uint32_t next;		/* 同一个 bucket 里的下一个节点下标 */
	uint64_t key;
	uint64_t value;
};

struct shared_hash_table {
	pthread_mutex_t bucket_lock[NUM_BUCKETS];
	uint32_t bucket_head[NUM_BUCKETS];	/* 受 bucket_lock 保护 */

	_Atomic uint64_t node_pool_head_tagged CACHE_ALIGNED;
	struct hash_node nodes[MAX_NODES];

	_Atomic uint64_t stat_insert CACHE_ALIGNED;
	_Atomic uint64_t stat_delete CACHE_ALIGNED;
	_Atomic uint64_t stat_lookup_hit CACHE_ALIGNED;
	_Atomic uint64_t stat_lookup_miss CACHE_ALIGNED;
	_Atomic uint64_t stat_value_mismatch CACHE_ALIGNED;
};

#define HT_SHM_NAME "/shm_examples_hash_table"

static inline uint32_t
ht_hash(uint64_t key)
{
	/* 简单的乘法哈希，足够把 key 打散到各个 bucket */
	return (uint32_t)((key * 2654435761ull) >> 32) % NUM_BUCKETS;
}

static inline uint64_t
ht_pack(uint32_t gen, uint32_t idx)
{
	return ((uint64_t)gen << 32) | idx;
}

static inline struct shared_hash_table *
ht_create(void)
{
	struct shared_hash_table *ht;
	pthread_mutexattr_t attr;
	uint32_t i;

	ht = shm_map(HT_SHM_NAME, sizeof(*ht), 1);

	pthread_mutexattr_init(&attr);
	pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

	for (i = 0; i < NUM_BUCKETS; i++) {
		pthread_mutex_init(&ht->bucket_lock[i], &attr);
		ht->bucket_head[i] = EMPTY_IDX;
	}
	pthread_mutexattr_destroy(&attr);

	for (i = 0; i < MAX_NODES; i++)
		ht->nodes[i].next =
			(i + 1 == MAX_NODES) ? EMPTY_IDX : (i + 1);

	atomic_store_explicit(&ht->node_pool_head_tagged, ht_pack(0, 0),
		memory_order_relaxed);
	atomic_store_explicit(&ht->stat_insert, 0, memory_order_relaxed);
	atomic_store_explicit(&ht->stat_delete, 0, memory_order_relaxed);
	atomic_store_explicit(&ht->stat_lookup_hit, 0, memory_order_relaxed);
	atomic_store_explicit(&ht->stat_lookup_miss, 0, memory_order_relaxed);
	atomic_store_explicit(&ht->stat_value_mismatch, 0,
		memory_order_relaxed);

	return ht;
}

static inline struct shared_hash_table *
ht_attach(void)
{
	return shm_map(HT_SHM_NAME, sizeof(struct shared_hash_table), 0);
}

static inline uint32_t
ht_node_alloc(struct shared_hash_table *ht)
{
	uint64_t old, new_val;
	uint32_t idx, gen, next_idx;

	old = atomic_load_explicit(&ht->node_pool_head_tagged,
		memory_order_acquire);

	for (;;) {
		idx = (uint32_t)(old & 0xFFFFFFFFu);
		if (idx == EMPTY_IDX)
			return EMPTY_IDX;

		gen = (uint32_t)(old >> 32);
		next_idx = ht->nodes[idx].next;
		new_val = ht_pack(gen + 1, next_idx);

		if (atomic_compare_exchange_weak_explicit(
			&ht->node_pool_head_tagged, &old, new_val,
			memory_order_acq_rel, memory_order_acquire))
			break;
	}

	return idx;
}

static inline void
ht_node_free(struct shared_hash_table *ht, uint32_t idx)
{
	uint64_t old, new_val;
	uint32_t gen;

	old = atomic_load_explicit(&ht->node_pool_head_tagged,
		memory_order_acquire);

	for (;;) {
		gen = (uint32_t)(old >> 32);
		ht->nodes[idx].next = (uint32_t)(old & 0xFFFFFFFFu);
		new_val = ht_pack(gen + 1, idx);

		if (atomic_compare_exchange_weak_explicit(
			&ht->node_pool_head_tagged, &old, new_val,
			memory_order_acq_rel, memory_order_acquire))
			break;
	}
}

/*
 * 插入或更新 key/value。返回 1 表示新插入，0 表示更新了已存在的 key。
 */
static inline int
ht_insert(struct shared_hash_table *ht, uint64_t key, uint64_t value)
{
	uint32_t bucket = ht_hash(key);
	uint32_t cur;
	int inserted = 0;

	pthread_mutex_lock(&ht->bucket_lock[bucket]);

	cur = ht->bucket_head[bucket];
	while (cur != EMPTY_IDX) {
		if (ht->nodes[cur].key == key) {
			ht->nodes[cur].value = value;
			break;
		}
		cur = ht->nodes[cur].next;
	}

	if (cur == EMPTY_IDX) {
		uint32_t idx = ht_node_alloc(ht);

		if (idx != EMPTY_IDX) {
			ht->nodes[idx].key = key;
			ht->nodes[idx].value = value;
			ht->nodes[idx].next = ht->bucket_head[bucket];
			ht->bucket_head[bucket] = idx;
			inserted = 1;
		}
	}

	pthread_mutex_unlock(&ht->bucket_lock[bucket]);

	if (inserted)
		atomic_fetch_add_explicit(&ht->stat_insert, 1,
			memory_order_relaxed);

	return inserted;
}

static inline int
ht_lookup(struct shared_hash_table *ht, uint64_t key, uint64_t *value)
{
	uint32_t bucket = ht_hash(key);
	uint32_t cur;
	int found = 0;

	pthread_mutex_lock(&ht->bucket_lock[bucket]);

	cur = ht->bucket_head[bucket];
	while (cur != EMPTY_IDX) {
		if (ht->nodes[cur].key == key) {
			*value = ht->nodes[cur].value;
			found = 1;
			break;
		}
		cur = ht->nodes[cur].next;
	}

	pthread_mutex_unlock(&ht->bucket_lock[bucket]);

	atomic_fetch_add_explicit(
		found ? &ht->stat_lookup_hit : &ht->stat_lookup_miss, 1,
		memory_order_relaxed);

	return found;
}

static inline int
ht_delete(struct shared_hash_table *ht, uint64_t key)
{
	uint32_t bucket = ht_hash(key);
	uint32_t cur, prev = EMPTY_IDX;
	int deleted = 0;

	pthread_mutex_lock(&ht->bucket_lock[bucket]);

	cur = ht->bucket_head[bucket];
	while (cur != EMPTY_IDX) {
		if (ht->nodes[cur].key == key) {
			if (prev == EMPTY_IDX)
				ht->bucket_head[bucket] = ht->nodes[cur].next;
			else
				ht->nodes[prev].next = ht->nodes[cur].next;

			ht_node_free(ht, cur);
			deleted = 1;
			break;
		}
		prev = cur;
		cur = ht->nodes[cur].next;
	}

	pthread_mutex_unlock(&ht->bucket_lock[bucket]);

	if (deleted)
		atomic_fetch_add_explicit(&ht->stat_delete, 1,
			memory_order_relaxed);

	return deleted;
}

#endif /* HASH_TABLE_H */
