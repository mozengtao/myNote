/*
 * futex_common.h - 两个独立进程通过共享内存里的 futex word 同步
 *
 * 心智模型：futex（fast userspace mutex）是 Linux 提供的最底层的
 * "阻塞等待某个内存字变成期望值"原语——glibc 的 mutex/条件变量、
 * sem_open 的信号量，底层最终都是靠 futex 实现的。它的特点是：
 *   - 用户态先用普通的原子读写/CAS 尝试快速路径（没有竞争时完全不
 *     进内核，比 sem_open 更轻）；
 *   - 只有确认需要等待时才调用 `futex(FUTEX_WAIT)` 陷入内核睡眠，
 *     被 `futex(FUTEX_WAKE)` 唤醒，避免像 01~08 里大量示例那样
 *     "busy-loop 空转烧 CPU"。
 *
 * 本示例用两个 futex word 实现一个单槽位的握手协议（对比 08 里
 * "双缓冲 + 忙等"的做法）：
 *   - slot_free：1 表示槽位空闲可写，0 表示已被占用等待消费
 *   - data_ready：1 表示槽位里有数据等待读取，0 表示已被取走
 *
 * futex word 必须放在共享内存里（两个独立进程共享的地址空间），
 * 且必须是 32 位整数（Linux futex 的硬性要求）。
 */
#ifndef FUTEX_COMMON_H
#define FUTEX_COMMON_H

#include <stdint.h>
#include <stdatomic.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "../../common/shm_common.h"

#define FUTEX_SHM_NAME "/shm_examples_futex"

struct futex_shared {
	_Atomic uint32_t slot_free;	/* 1=空闲可写 0=占用中 */
	_Atomic uint32_t data_ready;	/* 1=有数据待读 0=已取走 */
	uint64_t item;
	uint32_t checksum;
};

static inline uint32_t
futex_item_checksum(uint64_t item)
{
	uint32_t h = 2166136261u;

	h = (h ^ (uint32_t)item) * 16777619u;
	h = (h ^ (uint32_t)(item >> 32)) * 16777619u;

	return h;
}

static inline long
futex_wait(_Atomic uint32_t *addr, uint32_t expected)
{
	return syscall(SYS_futex, addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static inline long
futex_wake(_Atomic uint32_t *addr, int n)
{
	return syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}

/*
 * 等待 *addr 从 from 变为 to（CAS 失败就 futex_wait 睡眠，被唤醒后重试），
 * 成功后返回。这是"快速路径先 CAS，失败再陷入内核"的标准 futex 用法。
 */
static inline void
futex_wait_until_eq(_Atomic uint32_t *addr, uint32_t from, uint32_t to)
{
	for (;;) {
		uint32_t expected = from;

		if (atomic_compare_exchange_strong_explicit(addr, &expected,
				to, memory_order_acq_rel,
				memory_order_relaxed))
			return;

		/* expected 现在是 CAS 失败那一刻观察到的实际值；用它调用
		 * futex_wait，内核会原子地检查 *addr == expected 是否仍
		 * 成立——不成立说明对端又更新了，立即返回，不会错过唤醒；
		 * 成立就真正陷入内核睡眠，直到对端 futex_wake 唤醒我们 */
		futex_wait(addr, expected);
	}
}

#endif /* FUTEX_COMMON_H */
