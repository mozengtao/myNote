/*
 * pipeline_ring.h
 *
 * 可复用的 SPSC Ring Buffer（与 01_ring_buffer_spsc 的算法完全相同，
 * 只是把共享内存名字做成了运行期参数，方便同一套代码在流水线里
 * 创建多个独立的环形缓冲区）。
 *
 * 心智模型：一条"多级流水线"本质上就是多个 SPSC Ring Buffer 首尾相连，
 * 每一级只关心"从上一级的环形缓冲区里读，处理后写进下一级的环形缓冲区"，
 * 模块之间完全解耦，且没有一次多余的内存拷贝进用户态堆。
 */
#ifndef PIPELINE_RING_H
#define PIPELINE_RING_H

#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include "../common/shm_common.h"

#define PIPE_RING_CAPACITY 1024u	/* 必须是 2 的幂 */
#define PIPE_SENTINEL UINT64_MAX

struct pipe_slot {
	uint64_t seq;
	uint64_t payload;
};

struct pipe_ring {
	_Atomic uint32_t head CACHE_ALIGNED;
	_Atomic uint32_t tail CACHE_ALIGNED;
	uint32_t capacity;
	struct pipe_slot slots[PIPE_RING_CAPACITY];
};

static inline struct pipe_ring *
pipe_ring_create(const char *name)
{
	struct pipe_ring *rb;

	rb = shm_map(name, sizeof(*rb), 1);
	atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
	atomic_store_explicit(&rb->tail, 0, memory_order_relaxed);
	rb->capacity = PIPE_RING_CAPACITY;

	return rb;
}

static inline struct pipe_ring *
pipe_ring_attach_wait(const char *name)
{
	char path[256];

	snprintf(path, sizeof(path), "/dev/shm%s", name);
	while (access(path, F_OK) != 0)
		usleep(1000);

	return shm_map(name, sizeof(struct pipe_ring), 0);
}

static inline int
pipe_push(struct pipe_ring *rb, uint64_t seq, uint64_t payload)
{
	uint32_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
	uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
	uint32_t next = (head + 1) & (rb->capacity - 1);

	if (next == tail)
		return 0;

	rb->slots[head].seq = seq;
	rb->slots[head].payload = payload;
	atomic_store_explicit(&rb->head, next, memory_order_release);

	return 1;
}

static inline int
pipe_pop(struct pipe_ring *rb, uint64_t *seq, uint64_t *payload)
{
	uint32_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
	uint32_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

	if (tail == head)
		return 0;

	*seq = rb->slots[tail].seq;
	*payload = rb->slots[tail].payload;
	atomic_store_explicit(&rb->tail,
		(tail + 1) & (rb->capacity - 1), memory_order_release);

	return 1;
}

#endif /* PIPELINE_RING_H */
