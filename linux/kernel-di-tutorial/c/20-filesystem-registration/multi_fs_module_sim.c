/*
 * multi_fs_module_sim.c
 *
 * 场景：模拟多个"文件系统模块"（比如真实的 ext4.ko、proc.ko、btrfs.ko）
 * 在程序运行的不同阶段动态注册/注销自己进入同一个全局注册表，
 * 注册表核心代码完全不知道具体有哪些文件系统类型存在、也不知道它们
 * 什么时候会出现或消失——这正是 04 篇 Driver Registration"热插拔"特性
 * 与 06 篇 Initcall"模块自注册"思路，在文件系统场景下的综合应用。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   fs/ext2/super.c:1498    init_ext2_fs()：module_init 时机调用 register_filesystem()
 *   fs/ext2/super.c:1517    exit_ext2_fs()：module_exit 时机调用 unregister_filesystem()
 *   fs/ext2/super.c:1524    module_init(init_ext2_fs) / module_exit(exit_ext2_fs)
 *   fs/proc/root.c:97       proc 文件系统通常内置（builtin），不会被卸载
 *   fs/filesystems.c:32     全局 file_systems 链表——本文件用互斥锁保护的
 *                            版本模拟真实实现里 rwlock_t file_systems_lock
 *                            （fs/filesystems.c:33）保护并发访问的方式
 *
 * 时间线设计：
 *   - "proc" 模块在程序开始时就注册（模拟内置/早期 initcall），永不卸载。
 *   - "ext4" 模块延迟一段时间后才 insmod（注册），运行一段时间后 rmmod（注销）。
 *   - "btrfs" 模块在更晚的时间点才 insmod，且之后一直保持加载。
 *   - 一个独立的"用户操作"线程在整个过程中反复尝试 mount 各种文件系统，
 *     完全不知道也不关心此刻哪些模块处于加载状态——它只是查注册表。
 *
 * 编译（需要 pthread）：
 *   gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o multi_fs_module_sim multi_fs_module_sim.c
 * 运行：
 *   ./multi_fs_module_sim
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* 1. 契约 + Core：注册表（带锁，模拟 fs/filesystems.c:33 rwlock_t）      */
/* ------------------------------------------------------------------ */

struct mini_file_system_type {
	const char *name;
	int (*mount)(const char *dev_name);
	struct mini_file_system_type *next;
};

static pthread_mutex_t g_fs_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mini_file_system_type *g_file_systems;

static int mini_register_filesystem(struct mini_file_system_type *fs)
{
	pthread_mutex_lock(&g_fs_lock);
	struct mini_file_system_type **p = &g_file_systems;
	while (*p) {
		if (strcmp((*p)->name, fs->name) == 0) {
			pthread_mutex_unlock(&g_fs_lock);
			return -1;   /* -EBUSY */
		}
		p = &(*p)->next;
	}
	*p = fs;
	pthread_mutex_unlock(&g_fs_lock);
	return 0;
}

static int mini_unregister_filesystem(struct mini_file_system_type *fs)
{
	pthread_mutex_lock(&g_fs_lock);
	struct mini_file_system_type **p = &g_file_systems;
	while (*p) {
		if (*p == fs) {
			*p = fs->next;
			fs->next = NULL;
			pthread_mutex_unlock(&g_fs_lock);
			return 0;
		}
		p = &(*p)->next;
	}
	pthread_mutex_unlock(&g_fs_lock);
	return -1;   /* -EINVAL */
}

/* 对应 fs/namespace.c:1961 do_kern_mount() 里的 get_fs_type() + 分发调用 */
static int mini_try_mount(const char *name, const char *dev_name)
{
	pthread_mutex_lock(&g_fs_lock);
	struct mini_file_system_type *p;
	for (p = g_file_systems; p; p = p->next)
		if (strcmp(p->name, name) == 0)
			break;
	pthread_mutex_unlock(&g_fs_lock);

	if (!p) {
		printf("    [mount] mount -t %-6s %-12s -> 失败: -ENODEV（未注册或已卸载）\n",
		       name, dev_name ? dev_name : "");
		return -19;
	}
	int ret = p->mount(dev_name);
	printf("    [mount] mount -t %-6s %-12s -> 成功 (ret=%d)\n",
	       name, dev_name ? dev_name : "", ret);
	return ret;
}

/* ------------------------------------------------------------------ */
/* 2. 三个"模块"各自的实现（Core 完全不 #include 这些细节）              */
/* ------------------------------------------------------------------ */

static int proc_mount_cb(const char *dev_name) { (void)dev_name; return 0; }
static int ext4_mount_cb(const char *dev_name) { (void)dev_name; return 0; }
static int btrfs_mount_cb(const char *dev_name) { (void)dev_name; return 0; }

static struct mini_file_system_type proc_fs_type  = { .name = "proc",  .mount = proc_mount_cb };
static struct mini_file_system_type ext4_fs_type  = { .name = "ext4",  .mount = ext4_mount_cb };
static struct mini_file_system_type btrfs_fs_type = { .name = "btrfs", .mount = btrfs_mount_cb };

/* ------------------------------------------------------------------ */
/* 3. 模拟各模块的加载/卸载时间线（各自跑在独立线程里，互不相知）         */
/* ------------------------------------------------------------------ */

static void *proc_module_lifetime(void *arg)
{
	(void)arg;
	printf("  [t=  0ms] proc.ko: module_init -> register_filesystem('proc')\n");
	mini_register_filesystem(&proc_fs_type);
	/* 内置文件系统，永不卸载，线程直接结束（但注册状态一直保留） */
	return NULL;
}

static void *ext4_module_lifetime(void *arg)
{
	(void)arg;
	usleep(100 * 1000);
	printf("  [t=100ms] ext4.ko: insmod -> module_init -> register_filesystem('ext4')\n");
	mini_register_filesystem(&ext4_fs_type);

	usleep(250 * 1000);
	printf("  [t=350ms] ext4.ko: rmmod -> module_exit -> unregister_filesystem('ext4')\n");
	mini_unregister_filesystem(&ext4_fs_type);
	return NULL;
}

static void *btrfs_module_lifetime(void *arg)
{
	(void)arg;
	usleep(300 * 1000);
	printf("  [t=300ms] btrfs.ko: insmod -> module_init -> register_filesystem('btrfs')\n");
	mini_register_filesystem(&btrfs_fs_type);
	/* 之后一直保持加载状态直到程序结束 */
	return NULL;
}

/* 独立的"用户操作"线程：定期尝试挂载，完全不知道模块的加载/卸载时间线 */
static void *mount_attempts_thread(void *arg)
{
	(void)arg;
	struct { int delay_ms; const char *fstype; const char *dev; } attempts[] = {
		{  50, "proc",  NULL },
		{ 150, "ext4",  "/dev/sda1" },
		{ 200, "btrfs", "/dev/sdb1" },
		{ 320, "btrfs", "/dev/sdb1" },
		{ 400, "ext4",  "/dev/sda1" },   /* 此刻 ext4 已经被 rmmod，预期失败 */
		{ 450, "proc",  NULL },
	};
	int last = 0;
	for (size_t i = 0; i < sizeof(attempts) / sizeof(attempts[0]); i++) {
		usleep((attempts[i].delay_ms - last) * 1000);
		last = attempts[i].delay_ms;
		printf("  [t=%3dms] 用户线程尝试挂载:\n", attempts[i].delay_ms);
		mini_try_mount(attempts[i].fstype, attempts[i].dev);
	}
	return NULL;
}

int main(void)
{
	pthread_t t_proc, t_ext4, t_btrfs, t_mount;

	printf("=== multi_fs_module_sim ===\n\n");
	printf("-- 启动 3 个独立的\"模块\"生命周期线程 + 1 个用户挂载操作线程 --\n\n");

	pthread_create(&t_proc, NULL, proc_module_lifetime, NULL);
	pthread_create(&t_ext4, NULL, ext4_module_lifetime, NULL);
	pthread_create(&t_btrfs, NULL, btrfs_module_lifetime, NULL);
	pthread_create(&t_mount, NULL, mount_attempts_thread, NULL);

	pthread_join(t_proc, NULL);
	pthread_join(t_ext4, NULL);
	pthread_join(t_btrfs, NULL);
	pthread_join(t_mount, NULL);

	printf("\n-- 最终注册表状态 --\n");
	pthread_mutex_lock(&g_fs_lock);
	printf("  当前仍注册的文件系统: ");
	for (struct mini_file_system_type *p = g_file_systems; p; p = p->next)
		printf("%s ", p->name);
	printf("\n");
	pthread_mutex_unlock(&g_fs_lock);

	printf("\n=== done: 注册表核心代码全程没有 #include 任何一个具体文件系统的实现 ===\n");
	return 0;
}
