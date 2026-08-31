/*
 * shm_common.h
 *
 * 所有示例共用的最小工具集：
 *   - POSIX shared memory 创建/打开/映射/清理
 *   - 出错处理宏
 *   - cache line 对齐宏（避免 False Sharing）
 *
 * 使用方式：
 *   void *base = shm_map(name, size, is_creator);
 *   ...
 *   shm_unmap(base, size);
 *   if (is_creator) shm_destroy(name);
 */
#ifndef SHM_COMMON_H
#define SHM_COMMON_H

/* POSIX feature macros: ftruncate/usleep/shm_open 等在 -std=c11 下需要显式声明 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))

#define DIE_ERRNO(msg) \
	do { \
		fprintf(stderr, "%s:%d: %s: %s\n", \
			__FILE__, __LINE__, (msg), strerror(errno)); \
		exit(EXIT_FAILURE); \
	} while (0)

/*
 * 创建（is_creator=1）或打开（is_creator=0）一个 POSIX 共享内存对象，
 * 并映射到当前进程地址空间，返回映射基址。
 */
static inline void *
shm_map(const char *name, size_t size, int is_creator)
{
	int fd;
	int flags = O_RDWR;
	void *addr;

	if (is_creator)
		flags |= O_CREAT | O_EXCL;

	fd = shm_open(name, flags, 0666);
	if (fd < 0 && is_creator && errno == EEXIST) {
		/* 上一次运行残留，先清理再创建 */
		shm_unlink(name);
		fd = shm_open(name, flags, 0666);
	}
	if (fd < 0)
		DIE_ERRNO("shm_open");

	if (is_creator) {
		if (ftruncate(fd, (off_t)size) != 0)
			DIE_ERRNO("ftruncate");
	}

	addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
		DIE_ERRNO("mmap");

	close(fd);

	return addr;
}

static inline void
shm_unmap(void *addr, size_t size)
{
	if (munmap(addr, size) != 0)
		DIE_ERRNO("munmap");
}

static inline void
shm_destroy(const char *name)
{
	shm_unlink(name);
}

#endif /* SHM_COMMON_H */
