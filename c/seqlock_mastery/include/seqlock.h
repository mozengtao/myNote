/*
 * seqlock.h - 工业级 C 语言共享内存顺序锁（Seqlock）与序列计数器（seqcount）
 *
 * 核心设计目标：
 * 1. 适用于多线程及基于 POSIX 共享内存（mmap）的多进程 IPC 场景。
 * 2. 纯读操作绝对无锁（Lock-Free）：读者不执行任何原子写指令（零总线锁、零缓存行失效）。
 * 3. 严格遵循 C11 / GCC 内存模型（Memory Model），通过 Acquire/Release 与内存屏障
 *    建立严谨的先行发生（Happens-Before）关系，杜绝编译器与 CPU 乱序导致的脏读。
 * 4. 提供 Linux 内核级 seqcount 抽象与集成写者互斥保护的完整 seqlock 结构。
 */

#ifndef SEQLOCK_H
#define SEQLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include <sched.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CPU 暂停指令宏，用于读者自旋等待写者完成时的退让，降低功耗并减轻流水线惩罚。
 * x86/x86_64 下为 pause 指令；ARM64 下为 yield 指令。
 */
#if defined(__x86_64__) || defined(__i386__)
#define seqlock_cpu_relax() __asm__ __volatile__("pause" ::: "memory")
#elif defined(__aarch64__)
#define seqlock_cpu_relax() __asm__ __volatile__("yield" ::: "memory")
#else
#define seqlock_cpu_relax() sched_yield()
#endif

/*
 * 序列计数器结构体（seqcount_t）
 * 仅包含一个 32 位或 64 位的序列号，用于写者版本标记与读者一致性校验。
 *
 * 约定：
 * - 偶数（Even）：数据处于稳定一致状态，无写者介入。
 * - 奇数（Odd）：写者正在修改数据，数据可能处于部分更新状态（Torn Snapshot）。
 */
typedef struct {
	uint32_t sequence;
} seqcount_t;

/*
 * 顺序锁结构体（seqlock_t）
 * 将 seqcount_t 与写者互斥锁（自旋锁）封装在一起，确保多个并发写者之间的串行化。
 * 适用于多写者 + 多读者场景。
 */
typedef struct {
	seqcount_t seqcount;
	pthread_spinlock_t lock;
} seqlock_t;

/* 静态初始化宏 */
#define SEQCOUNT_INITIALIZER { .sequence = 0 }

/*
 * 初始化序列计数器
 */
static inline void
seqcount_init(seqcount_t *s)
{
	__atomic_store_n(&s->sequence, 0, __ATOMIC_RELAXED);
}

/*
 * 读者开始读取快照：返回当前的序列号
 *
 * 算法步骤：
 * 1. 使用 __ATOMIC_ACQUIRE 读取 sequence。
 * 2. 如果 sequence 为奇数（即 seq & 1），说明写者正在修改数据，调用 cpu_relax() 自旋等待直至为偶数。
 * 3. 内存序语义：ACQUIRE 保证后续对共享数据的读操作绝对不会被编译器或 CPU 乱序提升到该原子加载之前。
 */
static inline uint32_t
read_seqbegin(const seqcount_t *s)
{
	uint32_t seq;

	for (;;) {
		seq = __atomic_load_n(&s->sequence, __ATOMIC_ACQUIRE);
		if ((seq & 1) == 0)
			break;
		seqlock_cpu_relax();
	}

	return seq;
}

/*
 * 读者校验读取是否有效（是否发生冲突或脏读）
 *
 * 算法步骤：
 * 1. 显式插入 __atomic_thread_fence(__ATOMIC_ACQUIRE) 屏障。
 *    这一步极其关键：防止前面读取共享数据的操作被 CPU 投机或编译器乱序重排到步骤 2 的原子加载之后（Load-Load 乱序）。
 * 2. 使用 __ATOMIC_RELAXED 重新读取当前的 sequence。
 * 3. 若当前 sequence != read_seqbegin 返回的 start_seq，说明读取期间被写者打断或覆盖，返回 true（需重试）。
 *
 * 返回值：
 *   true: 发生并发写入，数据撕裂，必须丢弃并重试（Retry）。
 *   false: 数据完全一致，快照获取成功。
 */
static inline bool
read_seqretry(const seqcount_t *s, uint32_t start_seq)
{
	uint32_t current_seq;

	/* 保证所有的共享数据读操作在读取 current_seq 之前完成 */
	__atomic_thread_fence(__ATOMIC_ACQUIRE);

	current_seq = __atomic_load_n(&s->sequence, __ATOMIC_RELAXED);
	return (current_seq != start_seq);
}

/*
 * 写者开始写入数据
 *
 * 算法步骤：
 * 1. 读取当前 sequence（必须为偶数）。
 * 2. 将 sequence 递增为奇数（seq + 1），宣告写入开始。
 * 3. 显式插入 __atomic_thread_fence(__ATOMIC_RELEASE) 屏障。
 *    这一步确保将 sequence 标记为奇数的写操作，先行发生（Happens-Before）于后续对共享数据的写操作。
 *    杜绝将未写完的数据提前暴露给读者。
 */
static inline void
write_seqbegin(seqcount_t *s)
{
	uint32_t seq = __atomic_load_n(&s->sequence, __ATOMIC_RELAXED);

	__atomic_store_n(&s->sequence, seq + 1, __ATOMIC_RELAXED);
	__atomic_thread_fence(__ATOMIC_RELEASE);
}

/*
 * 写者结束写入数据
 *
 * 算法步骤：
 * 1. 获取当前奇数 sequence。
 * 2. 将 sequence 再次递增变为偶数（seq + 1 即原偶数 + 2）。
 * 3. 使用 __ATOMIC_RELEASE 存储新的偶数 sequence。
 *    这构成了与读者的同步点：在此之前对共享数据的所有写入全部提交到内存，对后续执行 ACQUIRE 加载的读者完全可见。
 */
static inline void
write_seqend(seqcount_t *s)
{
	uint32_t seq = __atomic_load_n(&s->sequence, __ATOMIC_RELAXED);

	__atomic_store_n(&s->sequence, seq + 1, __ATOMIC_RELEASE);
}

/*
 * 检查写者是否正在修改（非阻塞查询）
 */
static inline bool
seqcount_is_writing(const seqcount_t *s)
{
	return (__atomic_load_n(&s->sequence, __ATOMIC_RELAXED) & 1) != 0;
}

/*
 * 获取当前已完成的写入代际版本（Generation）
 * 因为每次完整写操作使 sequence + 2，因此 sequence >> 1 即为当前版本号。
 */
static inline uint32_t
seqcount_generation(const seqcount_t *s)
{
	return __atomic_load_n(&s->sequence, __ATOMIC_RELAXED) >> 1;
}

/* -------------------------------------------------------------------------
 * 多写者集成的完整 seqlock_t 接口
 * 内部结合自旋锁（pthread_spinlock_t），串行化多个并发写者。
 * ------------------------------------------------------------------------- */

/*
 * 初始化带自旋锁的 seqlock
 * pshared:
 *   PTHREAD_PROCESS_SHARED: 用于多进程共享内存
 *   PTHREAD_PROCESS_PRIVATE: 用于进程内多线程
 */
static inline int
seqlock_init(seqlock_t *sl, int pshared)
{
	seqcount_init(&sl->seqcount);
	return pthread_spin_init(&sl->lock, pshared);
}

/*
 * 销毁 seqlock
 */
static inline int
seqlock_destroy(seqlock_t *sl)
{
	return pthread_spin_destroy(&sl->lock);
}

/*
 * 读者开始（seqlock 版本）
 */
static inline uint32_t
seqlock_read_begin(const seqlock_t *sl)
{
	return read_seqbegin(&sl->seqcount);
}

/*
 * 读者重试检查（seqlock 版本）
 */
static inline bool
seqlock_read_retry(const seqlock_t *sl, uint32_t start_seq)
{
	return read_seqretry(&sl->seqcount, start_seq);
}

/*
 * 写者进入临界区（加自旋锁并递增 seq 为奇数）
 */
static inline void
seqlock_write_lock(seqlock_t *sl)
{
	pthread_spin_lock(&sl->lock);
	write_seqbegin(&sl->seqcount);
}

/*
 * 写者退出临界区（递增 seq 为偶数并解自旋锁）
 */
static inline void
seqlock_write_unlock(seqlock_t *sl)
{
	write_seqend(&sl->seqcount);
	pthread_spin_unlock(&sl->lock);
}

#ifdef __cplusplus
}
#endif

#endif /* SEQLOCK_H */
