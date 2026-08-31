/*
 * shared_config.h
 *
 * "Write Once, Read Many" 的共享配置热更新，使用经典 seqlock 技巧
 * （Linux 内核 jiffies/时间子系统同款手法）：
 *
 *   写者：seq++（变奇数，表示"正在写"） -> 更新字段 -> seq++（变偶数）
 *   读者：读 seq1 -> 读字段 -> 读 seq2；
 *         若 seq1 是奇数，或 seq1 != seq2，说明读的过程中被写者打断了
 *         （读到了"一半新一半旧"的撕裂数据），必须重试。
 *
 * 心智模型要点：
 *   - 单纯"写完再改 version"是不够的：Worker 完全可能在 writer 正在
 *     写中间字段的时候读到"version 已经变了，但 threshold 还是旧的"
 *     这种撕裂状态。seqlock 用一个计数器的奇偶性 + 前后一致性检查，
 *     把这个问题彻底解决。
 *   - 读者永远不阻塞写者（无锁），写者也永远不用等读者，天然适合
 *     "极少写、频繁读"的配置场景。
 */
#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include "../common/shm_common.h"

#define CONFIG_MAGIC 0x43464731u	/* "CFG1" */
#define LABEL_LEN 32

struct shared_config {
	_Atomic uint32_t seq CACHE_ALIGNED;	/* seqlock: 偶=稳定 奇=写入中 */
	uint32_t magic;
	uint32_t version;
	int32_t threshold;
	uint32_t retry_count;
	char label[LABEL_LEN];
	uint32_t checksum;
};

#define CONFIG_SHM_NAME "/shm_examples_shared_config"

static inline uint32_t
config_checksum(uint32_t version, int32_t threshold, uint32_t retry_count,
	const char *label)
{
	uint32_t h = 2166136261u;	/* FNV-1a 32-bit offset basis */
	const unsigned char *p;
	size_t i;

	h = (h ^ version) * 16777619u;
	h = (h ^ (uint32_t)threshold) * 16777619u;
	h = (h ^ retry_count) * 16777619u;

	p = (const unsigned char *)label;
	for (i = 0; i < LABEL_LEN && p[i] != '\0'; i++)
		h = (h ^ p[i]) * 16777619u;

	return h;
}

static inline struct shared_config *
config_create(void)
{
	struct shared_config *cfg;

	cfg = shm_map(CONFIG_SHM_NAME, sizeof(*cfg), 1);

	atomic_store_explicit(&cfg->seq, 0, memory_order_relaxed);
	cfg->magic = CONFIG_MAGIC;
	cfg->version = 0;
	cfg->threshold = 0;
	cfg->retry_count = 0;
	memset(cfg->label, 0, sizeof(cfg->label));
	cfg->checksum = config_checksum(0, 0, 0, "");

	return cfg;
}

static inline struct shared_config *
config_attach(void)
{
	return shm_map(CONFIG_SHM_NAME, sizeof(struct shared_config), 0);
}

/*
 * 写者调用：以 seqlock 方式发布一份新配置。
 */
static inline void
config_publish(struct shared_config *cfg, uint32_t version,
	int32_t threshold, uint32_t retry_count, const char *label)
{
	atomic_fetch_add_explicit(&cfg->seq, 1, memory_order_acq_rel);

	cfg->version = version;
	cfg->threshold = threshold;
	cfg->retry_count = retry_count;
	strncpy(cfg->label, label, LABEL_LEN - 1);
	cfg->label[LABEL_LEN - 1] = '\0';
	cfg->checksum = config_checksum(version, threshold, retry_count,
		cfg->label);

	atomic_fetch_add_explicit(&cfg->seq, 1, memory_order_release);
}

/*
 * 读者调用：seqlock 一致性读取一份配置快照到 out。
 * 返回重试了多少次才读到一份完整快照（0 表示一次就读成功）。
 */
static inline uint32_t
config_read(struct shared_config *cfg, struct shared_config *out)
{
	uint32_t retries = 0;
	uint32_t seq1, seq2;

	for (;;) {
		seq1 = atomic_load_explicit(&cfg->seq, memory_order_acquire);

		if (seq1 & 1u) {
			retries++;
			continue;	/* 写者正在写，忙等重试 */
		}

		out->magic = cfg->magic;
		out->version = cfg->version;
		out->threshold = cfg->threshold;
		out->retry_count = cfg->retry_count;
		memcpy(out->label, cfg->label, sizeof(out->label));
		out->checksum = cfg->checksum;

		seq2 = atomic_load_explicit(&cfg->seq, memory_order_acquire);

		if (seq1 == seq2)
			break;	/* 读取期间没有被写者打断，快照有效 */

		retries++;
	}

	return retries;
}

#endif /* SHARED_CONFIG_H */
