/*
 * mpmc_queue.h
 *
 * 多生产者多消费者（MPMC）无锁有界队列。
 * 算法来自 Dmitry Vyukov 的经典 "bounded MPMC queue"：
 *
 *   - 每个槽位除了数据外，还带一个 sequence 计数器。
 *   - enqueue 用 CAS 争抢一个全局递增的 position；只有当
 *     slot.sequence == position 时，才说明这个槽位"空且轮到你"。
 *   - 写完数据后把 slot.sequence 设为 position + 1，
 *     dequeue 方就能通过 slot.sequence == position + 1 判断"已就绪"。
 *   - 全程只用 CAS + load/store，不用 mutex。
 *
 * 与 01_ring_buffer_spsc 的关键区别：
 *   SPSC 只有一个 writer/一个 reader，用普通原子变量即可；
 *   MPMC 有多个 writer 竞争同一个槽位，必须用 CAS 解决竞争。
 */
#ifndef MPMC_QUEUE_H
#define MPMC_QUEUE_H

#include <stdint.h>
#include <stdatomic.h>
#include "../common/shm_common.h"

#define QUEUE_CAPACITY 4096u	/* 必须是 2 的幂 */
#define MAX_PRODUCERS  16u
#define MAX_ITEMS_PER_PRODUCER 100000u
#define BITMAP_BYTES_PER_PRODUCER ((MAX_ITEMS_PER_PRODUCER + 7) / 8)

#define SENTINEL_DATA  UINT64_MAX

struct queue_cell {
	_Atomic size_t sequence;
	uint64_t data;
};

/*
 * 校验思路：MPMC 队列只保证"每个入队的元素恰好被消费一次"（不丢、不重），
 * 并不保证多个并发消费者进程完成处理的先后顺序与生产顺序一致
 * （claim 槽位的顺序是严格递增的，但 claim 之后的业务处理耗时不确定，
 * 可能被调度打乱）。因此用一个共享 bitmap 记录"每个 producer 的每个 seq
 * 是否已经被消费"，而不是检查消费到达的时间顺序。
 */
struct mpmc_queue {
	struct queue_cell buffer[QUEUE_CAPACITY];
	size_t buffer_mask;

	_Atomic size_t enqueue_pos CACHE_ALIGNED;
	_Atomic size_t dequeue_pos CACHE_ALIGNED;

	_Atomic uint32_t active_producers CACHE_ALIGNED;
	uint32_t num_consumers;

	_Atomic uint64_t total_received CACHE_ALIGNED;
	_Atomic uint32_t duplicate_count CACHE_ALIGNED;

	_Atomic uint8_t seen_bitmap[MAX_PRODUCERS][BITMAP_BYTES_PER_PRODUCER];
};

#define QUEUE_SHM_NAME "/shm_examples_mpmc_queue"

/* data 编码: 高 8 位 producer_id，低 56 位 seq */
static inline uint64_t
queue_pack(uint32_t producer_id, uint64_t seq)
{
	return ((uint64_t)producer_id << 56) | (seq & 0x00FFFFFFFFFFFFFFULL);
}

static inline uint32_t
queue_unpack_pid(uint64_t data)
{
	return (uint32_t)(data >> 56);
}

static inline uint64_t
queue_unpack_seq(uint64_t data)
{
	return data & 0x00FFFFFFFFFFFFFFULL;
}

static inline struct mpmc_queue *
queue_create(uint32_t num_producers, uint32_t num_consumers)
{
	struct mpmc_queue *q;
	size_t i;

	q = shm_map(QUEUE_SHM_NAME, sizeof(*q), 1);

	q->buffer_mask = QUEUE_CAPACITY - 1;
	for (i = 0; i < QUEUE_CAPACITY; i++)
		atomic_store_explicit(&q->buffer[i].sequence, i,
			memory_order_relaxed);

	atomic_store_explicit(&q->enqueue_pos, 0, memory_order_relaxed);
	atomic_store_explicit(&q->dequeue_pos, 0, memory_order_relaxed);
	atomic_store_explicit(&q->active_producers, num_producers,
		memory_order_relaxed);
	q->num_consumers = num_consumers;
	atomic_store_explicit(&q->total_received, 0, memory_order_relaxed);
	atomic_store_explicit(&q->duplicate_count, 0, memory_order_relaxed);

	for (i = 0; i < MAX_PRODUCERS; i++) {
		size_t j;

		for (j = 0; j < BITMAP_BYTES_PER_PRODUCER; j++)
			atomic_store_explicit(&q->seen_bitmap[i][j], 0,
				memory_order_relaxed);
	}

	return q;
}

/*
 * 原子地把 producer pid 的第 idx 个 bit 置 1。
 * 返回 1 表示这是第一次置位（正常），返回 0 表示该 bit 之前已经被置过
 * （说明同一个元素被消费了两次——重复消费，属于 bug）。
 */
static inline int
queue_bitmap_mark(struct mpmc_queue *q, uint32_t pid, uint64_t idx)
{
	uint8_t mask;
	uint8_t old;

	mask = (uint8_t)(1u << (idx % 8));
	old = atomic_fetch_or_explicit(&q->seen_bitmap[pid][idx / 8], mask,
		memory_order_relaxed);

	return (old & mask) == 0;
}

static inline int
queue_bitmap_test(struct mpmc_queue *q, uint32_t pid, uint64_t idx)
{
	uint8_t mask = (uint8_t)(1u << (idx % 8));
	uint8_t byte = atomic_load_explicit(&q->seen_bitmap[pid][idx / 8],
		memory_order_relaxed);

	return (byte & mask) != 0;
}

static inline struct mpmc_queue *
queue_attach(void)
{
	return shm_map(QUEUE_SHM_NAME, sizeof(struct mpmc_queue), 0);
}

static inline int
queue_enqueue(struct mpmc_queue *q, uint64_t data)
{
	struct queue_cell *cell;
	size_t pos;

	pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);

	for (;;) {
		intptr_t diff;
		size_t seq;

		cell = &q->buffer[pos & q->buffer_mask];
		seq = atomic_load_explicit(&cell->sequence,
			memory_order_acquire);
		diff = (intptr_t)seq - (intptr_t)pos;

		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(
				&q->enqueue_pos, &pos, pos + 1,
				memory_order_relaxed, memory_order_relaxed))
				break;
		} else if (diff < 0) {
			return 0;	/* full */
		} else {
			pos = atomic_load_explicit(&q->enqueue_pos,
				memory_order_relaxed);
		}
	}

	cell->data = data;
	atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release);

	return 1;
}

static inline int
queue_dequeue(struct mpmc_queue *q, uint64_t *data)
{
	struct queue_cell *cell;
	size_t pos;

	pos = atomic_load_explicit(&q->dequeue_pos, memory_order_relaxed);

	for (;;) {
		intptr_t diff;
		size_t seq;

		cell = &q->buffer[pos & q->buffer_mask];
		seq = atomic_load_explicit(&cell->sequence,
			memory_order_acquire);
		diff = (intptr_t)seq - (intptr_t)(pos + 1);

		if (diff == 0) {
			if (atomic_compare_exchange_weak_explicit(
				&q->dequeue_pos, &pos, pos + 1,
				memory_order_relaxed, memory_order_relaxed))
				break;
		} else if (diff < 0) {
			return 0;	/* empty */
		} else {
			pos = atomic_load_explicit(&q->dequeue_pos,
				memory_order_relaxed);
		}
	}

	*data = cell->data;
	atomic_store_explicit(&cell->sequence, pos + q->buffer_mask + 1,
		memory_order_release);

	return 1;
}

#endif /* MPMC_QUEUE_H */
