/*
 * ring_buffer.h
 *
 * 单生产者单消费者（SPSC）无锁环形缓冲区。
 *
 * 心智模型：
 *   - 只有一个 writer 改 head，只有一个 reader 改 tail，
 *     所以不需要 CAS，只需要 acquire/release 语义就足够安全。
 *   - capacity 必须是 2 的幂，用位运算 (idx & (capacity - 1)) 代替取模。
 *   - head == tail 表示空；(head + 1) % capacity == tail 表示满
 *     （牺牲一个槽位来区分空/满，避免额外的计数字段）。
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdatomic.h>
#include "../common/shm_common.h"

#define RING_CAPACITY 1024u	/* 必须是 2 的幂 */

struct ring_slot {
	uint64_t seq;
	uint64_t payload;
};

struct ring_buffer {
	_Atomic uint32_t head CACHE_ALIGNED;	/* 生产者写位置 */
	_Atomic uint32_t tail CACHE_ALIGNED;	/* 消费者读位置 */
	uint32_t capacity;
	struct ring_slot slots[RING_CAPACITY];
};

#define RING_SHM_NAME "/shm_examples_ring_spsc"

static inline struct ring_buffer *
ring_create(void)
{
	struct ring_buffer *rb;

	rb = shm_map(RING_SHM_NAME, sizeof(*rb), 1);
	atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
	atomic_store_explicit(&rb->tail, 0, memory_order_relaxed);
	rb->capacity = RING_CAPACITY;

	return rb;
}

static inline struct ring_buffer *
ring_attach(void)
{
	return shm_map(RING_SHM_NAME, sizeof(struct ring_buffer), 0);
}

/*
 * 生产者调用：满则返回 0，成功写入返回 1。
 */
static inline int
ring_push(struct ring_buffer *rb, uint64_t seq, uint64_t payload)
{
	uint32_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
	uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
	uint32_t next = (head + 1) & (rb->capacity - 1);

	if (next == tail)
		return 0;	/* full */

	rb->slots[head].seq = seq;
	rb->slots[head].payload = payload;

	/* release：保证上面对 slots 的写入先于 head 更新对消费者可见 */
	atomic_store_explicit(&rb->head, next, memory_order_release);

	return 1;
}

/*
 * 消费者调用：空则返回 0，成功读到返回 1。
 */
static inline int
ring_pop(struct ring_buffer *rb, uint64_t *seq, uint64_t *payload)
{
	uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
	uint32_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

	if (tail == head)
		return 0;	/* empty */

	*seq = rb->slots[tail].seq;
	*payload = rb->slots[tail].payload;

	atomic_store_explicit(&rb->tail,
		(tail + 1) & (rb->capacity - 1), memory_order_release);

	return 1;
}

#endif /* RING_BUFFER_H */
