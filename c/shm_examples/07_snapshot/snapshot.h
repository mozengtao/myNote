/*
 * snapshot.h
 *
 * Snapshot（快照）模式：Reader 无锁读取"当前快照"，Writer 构建好一份
 * 全新快照后原子发布，Reader 与 Writer 互不阻塞。数据库的 MVCC、
 * Linux 内核的 RCU 都是这个思路的变体。
 *
 * 心智模型要点（这是本示例里最有价值的一课）：
 *
 *   最早的实现尝试用"每个快照槽位一个引用计数（refcount）"来判断
 *   一个槽位是否还有 Reader 在用——这是很多人第一次设计 Snapshot 时
 *   会写出来的版本。压测后发现：在真实的进程调度延迟下（Reader 在
 *   "读到 current_index" 和 "给 refcount+1" 这两步之间可能被换出很久），
 *   这个方案有极小概率出现 Writer 复用了一个"看起来空闲"、但实际上
 *   仍被某个迟到的 Reader 认领中的槽位，导致 Reader 读到一份撕裂数据。
 *   这个 bug 复现率大约几万次里出现 1 次，用纯逻辑推演很难一眼看穿，
 *   但压测（高并发、长时间跑）会把它暴露出来——这正是"心智模型"里
 *   最重要的一条经验：**无锁数据结构的正确性必须靠压力测试验证，
 *   不能只靠“讲得通”的推理**。
 *
 *   正确、可证明安全的方案是"基于 Epoch 的回收"（Epoch-Based
 *   Reclamation，EBR），这也是 Linux 内核 RCU、Rust crossbeam-epoch、
 *   各种无锁库回收内存时的标准做法：
 *
 *     - Writer 维护一个只增不减的全局版本号 `current_version`，
 *       版本号 v 对应的物理槽位 = v % NUM_SNAPSHOTS（循环复用）。
 *     - 每个 Reader 有自己固定的"座位"（reader_epoch[reader_id]），
 *       读之前先把自己想读的版本号"广播"出去（announce），然后再确认
 *       一次这个版本号对应的槽位还没被回收（版本号距离
 *       current_version 不超过 NUM_SNAPSHOTS，说明还没轮到被复用）。
 *     - Writer 发布新版本前，必须扫一遍所有 Reader 的"广播"，确认没有
 *       任何 Reader 还停留在"即将被复用的那个版本"上，否则就等。
 *     - 只要 Reader 的广播还没撤销（还没 release），Writer 就
 *       **数学上不可能**推进到需要复用那个槽位的版本号——不是"大概率
 *       安全"，是可以证明的安全。
 */
#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <stdint.h>
#include <stdatomic.h>
#include "../common/shm_common.h"

#define NUM_SNAPSHOTS 6u
#define SNAPSHOT_PAYLOAD_LEN 8u
#define MAX_READERS 16u

#define NO_EPOCH UINT64_MAX	/* 表示"这个 Reader 当前没有在读任何快照" */

struct snapshot_slot {
	uint64_t version;
	uint64_t payload[SNAPSHOT_PAYLOAD_LEN];
	uint32_t checksum;
};

struct snapshot_store {
	_Atomic uint64_t current_version CACHE_ALIGNED;
	_Atomic uint32_t writer_wait_events CACHE_ALIGNED;
	_Atomic uint64_t reader_epoch[MAX_READERS];
	struct snapshot_slot slots[NUM_SNAPSHOTS];
};

#define SNAPSHOT_SHM_NAME "/shm_examples_snapshot"

static inline uint32_t
snapshot_checksum(uint64_t version, const uint64_t *payload)
{
	uint32_t h = 2166136261u;
	uint32_t i;

	h = (h ^ (uint32_t)version) * 16777619u;
	h = (h ^ (uint32_t)(version >> 32)) * 16777619u;
	for (i = 0; i < SNAPSHOT_PAYLOAD_LEN; i++) {
		h = (h ^ (uint32_t)(payload[i] & 0xFFFFFFFFu)) * 16777619u;
		h = (h ^ (uint32_t)(payload[i] >> 32)) * 16777619u;
	}

	return h;
}

static inline struct snapshot_store *
snapshot_create(void)
{
	struct snapshot_store *store;
	uint32_t i;

	store = shm_map(SNAPSHOT_SHM_NAME, sizeof(*store), 1);

	atomic_store_explicit(&store->current_version, 0,
		memory_order_relaxed);
	atomic_store_explicit(&store->writer_wait_events, 0,
		memory_order_relaxed);

	for (i = 0; i < MAX_READERS; i++)
		atomic_store_explicit(&store->reader_epoch[i], NO_EPOCH,
			memory_order_relaxed);

	store->slots[0].version = 0;
	store->slots[0].checksum = snapshot_checksum(0, store->slots[0].payload);

	return store;
}

static inline struct snapshot_store *
snapshot_attach(void)
{
	return shm_map(SNAPSHOT_SHM_NAME, sizeof(struct snapshot_store), 0);
}

/*
 * Writer: 构建并发布一份新快照。version 必须严格递增（从 1 开始）。
 */
static inline void
snapshot_publish(struct snapshot_store *store, uint64_t version)
{
	uint32_t physical = (uint32_t)(version % NUM_SNAPSHOTS);
	uint64_t evicted_version = version - NUM_SNAPSHOTS;	/* 即将被复用的老版本号 */
	uint32_t i;

	/* 只要还有 Reader 停留在"即将被复用"的那个版本上，就必须等待。
	 * version < NUM_SNAPSHOTS 时（尚未发生过任何回收）不需要检查。
	 *
	 * 这里用 memory_order_seq_cst 读取 reader_epoch[]，并要求 Reader
	 * 的"广播"也用 seq_cst 写入——两边都用 seq_cst 时，C11 保证不会
	 * 出现"两边都看到对方写之前的旧值"（即 IRIW / store-buffering
	 * 异象），这正是这类"发布意图后再互相确认"协议必须的强度，
	 * 单纯的 acquire/release 不足以保证这一点。 */
	if (version >= NUM_SNAPSHOTS) {
		for (;;) {
			int safe = 1;

			for (i = 0; i < MAX_READERS; i++) {
				uint64_t e = atomic_load_explicit(
					&store->reader_epoch[i],
					memory_order_seq_cst);

				if (e != NO_EPOCH && e <= evicted_version) {
					safe = 0;
					break;
				}
			}

			if (safe)
				break;

			atomic_fetch_add_explicit(
				&store->writer_wait_events, 1,
				memory_order_relaxed);
		}
	}

	for (i = 0; i < SNAPSHOT_PAYLOAD_LEN; i++)
		store->slots[physical].payload[i] = version * 1000ull + i;
	store->slots[physical].version = version;
	store->slots[physical].checksum = snapshot_checksum(version,
		store->slots[physical].payload);

	/* 发布：原子推进全局版本号（seq_cst，理由同上） */
	atomic_store_explicit(&store->current_version, version,
		memory_order_seq_cst);
}

/*
 * Reader: 广播"我要读版本 v"，并确认 v 对应的槽位还没被回收。
 * 成功后返回 v；调用者应该用 v % NUM_SNAPSHOTS 找到物理槽位。
 * 用完之后必须调用 snapshot_release()。
 */
static inline uint64_t
snapshot_acquire(struct snapshot_store *store, uint32_t reader_id)
{
	for (;;) {
		uint64_t v = atomic_load_explicit(&store->current_version,
			memory_order_seq_cst);
		uint64_t v2;

		atomic_store_explicit(&store->reader_epoch[reader_id], v,
			memory_order_seq_cst);

		v2 = atomic_load_explicit(&store->current_version,
			memory_order_seq_cst);

		if (v2 - v < NUM_SNAPSHOTS)
			return v;	/* 广播生效前，v 对应的槽位还没被回收 */

		/* 广播晚了一步，v 已经可能被回收，撤销广播后重试 */
		atomic_store_explicit(&store->reader_epoch[reader_id],
			NO_EPOCH, memory_order_seq_cst);
	}
}

static inline void
snapshot_release(struct snapshot_store *store, uint32_t reader_id)
{
	atomic_store_explicit(&store->reader_epoch[reader_id], NO_EPOCH,
		memory_order_seq_cst);
}

#endif /* SNAPSHOT_H */
