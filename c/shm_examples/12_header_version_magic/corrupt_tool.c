/*
 * corrupt_tool.c - 故意破坏已存在的共享内存，模拟三类真实故障场景
 *
 * 用法: ./corrupt_tool <magic|version|data>
 *   magic   - 篡改 header.magic，模拟"附加到了错误/不相关的共享内存对象"
 *   version - 把 header.major_version 加 1，模拟"数据是用不兼容的新协议
 *             版本写入的"
 *   data    - 翻转数据区里的一个字节，不touch header，模拟"内存 bit
 *             flip / 写入过程中被意外改动"，只有 CRC 校验能发现
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shm_header.h"

int
main(int argc, char *argv[])
{
	struct shm_region *region;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <magic|version|data>\n", argv[0]);
		return 1;
	}

	region = shm_map(SHM_HEADER_SHM_NAME, sizeof(*region), 0);

	if (strcmp(argv[1], "magic") == 0) {
		region->header.magic ^= 0xFFFFFFFFu;
		printf("[corrupt] magic 已篡改为 0x%08X\n",
			region->header.magic);
	} else if (strcmp(argv[1], "version") == 0) {
		region->header.major_version++;
		printf("[corrupt] major_version 已改为 %u\n",
			region->header.major_version);
	} else if (strcmp(argv[1], "data") == 0) {
		region->data[0] ^= 0xFFu;
		printf("[corrupt] data[0] 已翻转，crc 字段未更新，"
			"制造 CRC 不匹配\n");
	} else {
		fprintf(stderr, "unknown corruption type: %s\n", argv[1]);
		shm_unmap(region, sizeof(*region));
		return 1;
	}

	shm_unmap(region, sizeof(*region));

	return 0;
}
