/*
 * reader.c - 附加共享内存，按 Magic -> Version -> CRC 顺序校验后再读取
 *
 * 用法: ./reader [expected_major]
 * 不传参数时使用当前协议的正式版本号 SHM_MAJOR_VERSION，模拟"正常版本
 * 匹配的 reader"；传一个与写入版本不同的值可以模拟"老版本 reader 遇到
 * 新协议数据"的场景。
 */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "shm_header.h"

int
main(int argc, char *argv[])
{
	struct shm_region *region;
	uint16_t expected_major = SHM_MAJOR_VERSION;
	enum shm_header_check_result result;

	if (argc > 1)
		expected_major = (uint16_t)strtoul(argv[1], NULL, 10);

	region = shm_map(SHM_HEADER_SHM_NAME, sizeof(*region), 0);

	result = shm_header_verify(region, expected_major);

	printf("[reader] check result: %s\n",
		shm_header_check_result_str(result));

	if (result != SHM_HEADER_OK) {
		shm_unmap(region, sizeof(*region));
		return 1;
	}

	printf("[reader] magic=0x%08X major=%u minor=%u sequence=%" PRIu64
		" timestamp=%" PRIu64 " length=%u crc=0x%08X payload=\"%s\"\n",
		region->header.magic, region->header.major_version,
		region->header.minor_version, region->header.sequence,
		region->header.timestamp, region->header.length,
		region->header.crc, (const char *)region->data);

	shm_unmap(region, sizeof(*region));

	return 0;
}
