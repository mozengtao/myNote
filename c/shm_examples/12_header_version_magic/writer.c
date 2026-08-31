/*
 * writer.c - 创建/重新发布一份带 Header 的共享内存
 *
 * 用法: ./writer [major] [minor]
 * 不传参数时使用当前协议的正式版本号 SHM_MAJOR_VERSION/SHM_MINOR_VERSION。
 * 传参数是为了在 run_demo.sh 里模拟"用一个不同的 major 版本写入数据"
 * 这种场景，观察老版本 reader 如何拒绝它。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "shm_header.h"

int
main(int argc, char *argv[])
{
	struct shm_region *region;
	uint16_t major = SHM_MAJOR_VERSION;
	uint16_t minor = SHM_MINOR_VERSION;
	char payload[SHM_DATA_LEN];
	int is_creator;

	if (argc > 1)
		major = (uint16_t)strtoul(argv[1], NULL, 10);
	if (argc > 2)
		minor = (uint16_t)strtoul(argv[2], NULL, 10);

	/* 如果共享内存对象已存在（比如上一次 demo 场景残留），复用它并
	 * 重新发布，而不是报错退出——保持 run_demo.sh 里多轮场景切换的
	 * 简单性 */
	is_creator = (access("/dev/shm" SHM_HEADER_SHM_NAME, F_OK) != 0);
	region = shm_map(SHM_HEADER_SHM_NAME, sizeof(*region), is_creator);

	if (is_creator)
		memset(region, 0, sizeof(*region));

	snprintf(payload, sizeof(payload),
		"hello from writer, major=%u minor=%u", major, minor);

	shm_header_publish(region, payload, (uint32_t)strlen(payload) + 1,
		major, minor, 0);

	printf("[writer] published: magic=0x%08X major=%u minor=%u "
		"sequence=%" PRIu64 " length=%u crc=0x%08X\n",
		region->header.magic, region->header.major_version,
		region->header.minor_version, region->header.sequence,
		region->header.length, region->header.crc);

	shm_unmap(region, sizeof(*region));

	return 0;
}
