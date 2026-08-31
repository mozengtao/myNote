/*
 * shm_header.h
 *
 * Header + Version + Magic + CRC（工业界必备的"可靠性层"）
 *
 * 心智模型：前面 01~11 的每个示例都在解决"数据怎么高效地被多个进程
 * 读写"，但没有一个回答"我怎么知道对面这段共享内存是不是我期望的那种
 * 格式、版本、有没有被打乱"。任何工业级共享内存协议几乎都会在数据区
 * 前面加一个固定的 Header：
 *
 *   - Magic：一个固定的魔数（比如 "SHM1" 的 ASCII 值）。附加进程一旦
 *     看到 magic 不对，立刻能判断"这根本不是我认识的共享内存格式"
 *     （常见场景：附加到了一段已经被其他程序复用的 /dev/shm 对象、
 *     或者共享内存对象名冲突）。
 *   - Version（major/minor）：协议演进后，老版本 reader 遇到新版本
 *     writer 产生的数据，或者反过来，能明确判断"能不能读"，而不是
 *     "读出来数据对不对但语义已经变了"这种更隐蔽的 bug。约定：
 *     major 变化 = 不兼容（字段布局变了，老 reader 必须拒绝）；
 *     minor 变化 = 向后兼容（只是新增了字段，老 reader 忽略新增部分
 *     依然能正确工作）。
 *   - CRC：对数据区做校验和，检测"内容是否在写入之后被意外改动/损坏
 *     过"——内存 bit flip、写入过程中进程崩溃导致数据只写了一半、
 *     错误的指针越界写入，都有机会被 CRC 校验揪出来。
 *
 * 与 06_shared_config 的关系：06 演示的是"高频热更新场景下用 seqlock
 * 保证读者不会读到撕裂数据"；本示例更关注"启动时/附加时的一次性合法性
 * 校验"——两者可以叠加使用（先做 Header 校验确认版本和数据没损坏，
 * 再决定要不要用 seqlock/EBR 之类的协议去读热更新的内容）。
 */
#ifndef SHM_HEADER_H
#define SHM_HEADER_H

#include <stdint.h>
#include <time.h>
#include "../common/shm_common.h"

#define SHM_MAGIC 0x314D4853u	/* little-endian 上显示为 "SHM1" */
#define SHM_MAJOR_VERSION 1u
#define SHM_MINOR_VERSION 0u

#define SHM_HEADER_SHM_NAME "/shm_examples_header_magic"
#define SHM_DATA_LEN 256u

struct shm_header {
	uint32_t magic;
	uint16_t major_version;
	uint16_t minor_version;
	uint64_t sequence;
	uint64_t timestamp;
	uint32_t flags;
	uint32_t length;	/* 数据区有效字节数 */
	uint32_t crc;		/* 对 data[0..length) 的 CRC32 */
};

struct shm_region {
	struct shm_header header;
	uint8_t data[SHM_DATA_LEN];
};

enum shm_header_check_result {
	SHM_HEADER_OK = 0,
	SHM_HEADER_ERR_MAGIC,
	SHM_HEADER_ERR_VERSION,
	SHM_HEADER_ERR_CRC,
};

static inline const char *
shm_header_check_result_str(enum shm_header_check_result r)
{
	switch (r) {
	case SHM_HEADER_OK:
		return "OK";
	case SHM_HEADER_ERR_MAGIC:
		return "MAGIC_MISMATCH（不是本程序认识的共享内存格式）";
	case SHM_HEADER_ERR_VERSION:
		return "VERSION_INCOMPATIBLE（主版本号不兼容）";
	case SHM_HEADER_ERR_CRC:
		return "CRC_MISMATCH（数据已损坏）";
	default:
		return "UNKNOWN";
	}
}

/*
 * 标准 CRC32（IEEE 802.3 多项式 0xEDB88320），逐位实现，不用查表——
 * 本示例数据量很小（SHM_DATA_LEN 字节级别），性能不是重点，逐位实现
 * 更直观地展示 CRC 的计算过程。
 */
static inline uint32_t
shm_crc32(const void *data, size_t len)
{
	const uint8_t *p = data;
	uint32_t crc = 0xFFFFFFFFu;
	size_t i;
	int bit;

	for (i = 0; i < len; i++) {
		crc ^= p[i];
		for (bit = 0; bit < 8; bit++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320u;
			else
				crc >>= 1;
		}
	}

	return ~crc;
}

/*
 * Writer: 把 payload 写入数据区，并原子性地（对单次一次性发布场景而言，
 * 顺序即可，不需要 06 那种 seqlock）填好整份 Header，magic 最后写，
 * 让"看到 magic 正确"隐含"其余字段都已经写完"这层近似保证。
 */
static inline void
shm_header_publish(struct shm_region *region, const void *payload,
	uint32_t payload_len, uint16_t major, uint16_t minor, uint32_t flags)
{
	if (payload_len > SHM_DATA_LEN)
		payload_len = SHM_DATA_LEN;

	memcpy(region->data, payload, payload_len);

	region->header.major_version = major;
	region->header.minor_version = minor;
	region->header.sequence++;
	region->header.timestamp = (uint64_t)time(NULL);
	region->header.flags = flags;
	region->header.length = payload_len;
	region->header.crc = shm_crc32(region->data, payload_len);

	/* magic 最后写：reader 只要看到 magic 正确，就认为上面这些字段
	 * 也都已经写完了（单次发布场景下的一个简单心智模型，严格的并发
	 * 热更新场景应参考 06_shared_config 的 seqlock 协议） */
	region->header.magic = SHM_MAGIC;
}

/*
 * Reader: 按 Magic → Version → CRC 的顺序依次校验，一旦某一步失败立刻
 * 返回对应的错误码，不再继续检查后面的字段（顺序本身就是"信任递进"：
 * 连 magic 都不对，再检查 CRC 没有意义）。
 */
static inline enum shm_header_check_result
shm_header_verify(const struct shm_region *region, uint16_t expected_major)
{
	uint32_t recomputed_crc;

	if (region->header.magic != SHM_MAGIC)
		return SHM_HEADER_ERR_MAGIC;

	if (region->header.major_version != expected_major)
		return SHM_HEADER_ERR_VERSION;

	recomputed_crc = shm_crc32(region->data, region->header.length);
	if (recomputed_crc != region->header.crc)
		return SHM_HEADER_ERR_CRC;

	return SHM_HEADER_OK;
}

#endif /* SHM_HEADER_H */
