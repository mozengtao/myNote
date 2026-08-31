/*
 * double_buffer.h
 *
 * Double Buffer（双缓冲）模式：单生产者独占写"背面（back）"缓冲区，单消费者
 * 独占读"正面（front）"缓冲区；生产者写完后原子翻转 front_index，完成一次
 * "角色互换"。图形渲染的前后台缓冲区、仿真系统的双态切换都是这个思路。
 *
 * 心智模型要点：这是 07_snapshot 的一个特化/简化版本——把 EBR 协议里
 * "N 个槽位、最多 MAX_READERS 个并发广播者"收窄成"恰好 2 个槽位、恰好 1 个
 * 读者"。槽位少带来的直接后果是：Writer 能接受的"Reader 落后"窗口从 07 里
 * 的 N 个版本收窄到了几乎为 0——只要 Reader 还没读完当前这份，Writer 就必须
 * 原地等待，不能像 Snapshot 那样"多攒几个版本再回收"。这是 Double Buffer
 * 用内存换来的"更简单"，代价是"生产者更容易被消费者拖慢"。
 *
 * 具体协议（本质是 07_snapshot 的 EBR 思路的 SPSC 特例）：
 *   - Writer 只写 back = 1 - front_index 指向的槽位；写之前必须确认当前
 *     唯一的 Reader 没有正在读这个 back 槽位（Reader 上一轮读的是"旧的
 *     front"，如果 Writer 已经连续翻转过一次而 Reader 还没读完，那个旧
 *     front 正好就是新的 back，这就是必须等待的场景）。
 *   - Reader 用与 07 完全相同的"广播 + 复核"两步协议宣告自己正在读哪个
 *     槽位，读完后清除广播。
 *   - 只要 Reader 的广播没有撤销，Writer 就不会覆盖它正在读的槽位——
 *     依然是可证明安全，不是"概率上大概率安全"。
 */
#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stdint.h>
#include <stdatomic.h>
#include "../common/shm_common.h"

#define DB_PAYLOAD_LEN 8u
#define NO_EPOCH UINT32_MAX	/* Reader 当前没有在读任何槽位 */

struct db_slot {
	uint64_t frame_id;
	uint64_t payload[DB_PAYLOAD_LEN];
	uint32_t checksum;
};

struct double_buffer {
	_Atomic uint32_t front_index CACHE_ALIGNED;	/* 0 或 1：Reader 该读哪个槽位 */
	_Atomic uint32_t reader_epoch CACHE_ALIGNED;	/* Reader 正在读的 front_index 快照，NO_EPOCH=空闲 */
	_Atomic uint32_t writer_wait_events CACHE_ALIGNED;
	struct db_slot slots[2];
};

#define DB_SHM_NAME "/shm_examples_double_buffer"

static inline uint32_t
db_checksum(uint64_t frame_id, const uint64_t *payload)
{
	uint32_t h = 2166136261u;
	uint32_t i;

	h = (h ^ (uint32_t)frame_id) * 16777619u;
	h = (h ^ (uint32_t)(frame_id >> 32)) * 16777619u;
	for (i = 0; i < DB_PAYLOAD_LEN; i++) {
		h = (h ^ (uint32_t)(payload[i] & 0xFFFFFFFFu)) * 16777619u;
		h = (h ^ (uint32_t)(payload[i] >> 32)) * 16777619u;
	}

	return h;
}

static inline struct double_buffer *
db_create(void)
{
	struct double_buffer *db;

	db = shm_map(DB_SHM_NAME, sizeof(*db), 1);

	atomic_store_explicit(&db->front_index, 0, memory_order_relaxed);
	atomic_store_explicit(&db->reader_epoch, NO_EPOCH,
		memory_order_relaxed);
	atomic_store_explicit(&db->writer_wait_events, 0,
		memory_order_relaxed);

	db->slots[0].frame_id = 0;
	db->slots[0].checksum = db_checksum(0, db->slots[0].payload);

	return db;
}

static inline struct double_buffer *
db_attach(void)
{
	return shm_map(DB_SHM_NAME, sizeof(struct double_buffer), 0);
}

/*
 * Writer: 生成一帧新数据并发布。frame_id 从 1 开始严格递增。
 */
static inline void
db_publish(struct double_buffer *db, uint64_t frame_id)
{
	uint32_t front = atomic_load_explicit(&db->front_index,
		memory_order_seq_cst);
	uint32_t back = 1u - front;
	uint32_t i;

	/* back 槽位正是"上一次的 front"；如果 Reader 还广播着自己在读它，
	 * 必须等它读完再写，否则就是撤销一份仍在被读的数据。 */
	for (;;) {
		uint32_t e = atomic_load_explicit(&db->reader_epoch,
			memory_order_seq_cst);

		if (e != back)
			break;

		atomic_fetch_add_explicit(&db->writer_wait_events, 1,
			memory_order_relaxed);
	}

	for (i = 0; i < DB_PAYLOAD_LEN; i++)
		db->slots[back].payload[i] = frame_id * 1000ull + i;
	db->slots[back].frame_id = frame_id;
	db->slots[back].checksum = db_checksum(frame_id,
		db->slots[back].payload);

	/* 发布：原子翻转 front_index，完成一次角色互换 */
	atomic_store_explicit(&db->front_index, back, memory_order_seq_cst);
}

/*
 * Reader: 广播"我要读 front_index 指向的槽位"，确认没有在广播期间被
 * Writer 翻转过（否则重试），成功后返回槽位下标。用完必须 db_release()。
 */
static inline uint32_t
db_acquire(struct double_buffer *db)
{
	for (;;) {
		uint32_t idx = atomic_load_explicit(&db->front_index,
			memory_order_seq_cst);

		atomic_store_explicit(&db->reader_epoch, idx,
			memory_order_seq_cst);

		uint32_t idx2 = atomic_load_explicit(&db->front_index,
			memory_order_seq_cst);

		if (idx2 == idx)
			return idx;

		/* 广播期间 Writer 已经翻转过，撤销广播后重试 */
		atomic_store_explicit(&db->reader_epoch, NO_EPOCH,
			memory_order_seq_cst);
	}
}

static inline void
db_release(struct double_buffer *db)
{
	atomic_store_explicit(&db->reader_epoch, NO_EPOCH,
		memory_order_seq_cst);
}

#endif /* DOUBLE_BUFFER_H */
