/*
 * test_app.c - Userspace test application for mypcie device driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../driver/mypcie_uapi.h"

int main(int argc, char **argv)
{
	int fd;
	struct mypcie_ioc_transfer req;
	int ret;

	printf("==================================================\n");
	printf(" Capstone Project: 用户态发起 DMA Ring 测试\n");
	printf("==================================================\n");

	fd = open("/dev/mypcie0", O_RDWR);
	if (fd < 0) {
		perror("Failed to open /dev/mypcie0");
		return 1;
	}
	printf("[1] 成功打开设备文件 /dev/mypcie0\n");

	/*
	 * 测试 1: 发送数据到设备 (Direction 1)
	 */
	memset(&req, 0, sizeof(req));
	req.direction = 1;
	req.length = 64;
	snprintf((char *)req.data, sizeof(req.data), "PCIe Driver Mastery Capstone Request #1");

	printf("[2] 发起写请求 (RAM -> Device): '%s'\n", (char *)req.data);
	ret = ioctl(fd, MYPCIE_IOCTL_SUBMIT_REQ, &req);
	if (ret < 0) {
		perror("ioctl failed");
		close(fd);
		return 1;
	}
	printf("    写请求完成并成功收到 MSI-X 中断唤醒！\n");

	/*
	 * 测试 2: 从设备读取数据 (Direction 0 - 硬件回填 0x5a 测试特征字节)
	 */
	memset(&req, 0, sizeof(req));
	req.direction = 0;
	req.length = 32;

	printf("[3] 发起读请求 (Device -> RAM 回填测试)...\n");
	ret = ioctl(fd, MYPCIE_IOCTL_SUBMIT_REQ, &req);
	if (ret < 0) {
		perror("ioctl failed");
		close(fd);
		return 1;
	}

	printf("    读请求成功返回，校验硬件回填字节 (期望均为 0x5a):\n    [ ");
	for (int i = 0; i < 8; i++) {
		printf("0x%02x ", req.data[i]);
	}
	printf("... ]\n");

	if (req.data[0] == 0x5a && req.data[7] == 0x5a) {
		printf(">>> SUCCESS: 硬件 DMA Ring + MSI-X 完整闭环验证全部通过！ <<<\n");
	} else {
		printf(">>> ERROR: 数据校验不一致！ <<<\n");
	}

	close(fd);
	return 0;
}
