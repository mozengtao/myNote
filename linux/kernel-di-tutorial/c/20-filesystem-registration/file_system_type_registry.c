/*
 * file_system_type_registry.c
 *
 * 场景：模拟 register_filesystem()/unregister_filesystem() 对全局链表
 * file_systems 的管理——插入、按名字查找、注销时的链表摘除。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   include/linux/fs.h:1859     struct file_system_type（本仓库已是 .mount 接口，
 *                                不是更老版本的 .get_sb，如实按仓库现状建模）
 *   fs/filesystems.c:32         static struct file_system_type *file_systems;
 *   fs/filesystems.c:46         find_filesystem()：返回"匹配项"或"链表末尾空槎"的
 *                                地址（struct file_system_type **）
 *   fs/filesystems.c:69         register_filesystem()
 *   fs/filesystems.c:102        unregister_filesystem()
 *
 * !!! 重要的真实性说明（容易被想象成"插入头部"，但实际不是）!!!
 *   find_filesystem() 的循环是 `for (p=&file_systems; *p; p=&(*p)->next)`——
 *   它会一直往后走，直到"找到同名项"或者"走到链表末尾（*p==NULL）"才停下。
 *   register_filesystem() 里 `if (*p) return -EBUSY; else *p = fs;`——
 *   当没有同名项时，`*p` 此时正是链表最后一个节点的 next 指针（或者链表为空时
 *   就是 file_systems 本身），所以新文件系统类型是被**追加到链表尾部**，
 *   而不是插入到头部。本文件严格照抄这个真实行为，不按直觉去"猜"。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o file_system_type_registry file_system_type_registry.c
 * 运行：./file_system_type_registry
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* 1. 契约：mini_file_system_type，对应 include/linux/fs.h:1859          */
/* ------------------------------------------------------------------ */

struct mini_file_system_type;

typedef void *(*mini_mount_fn)(struct mini_file_system_type *fs_type,
				int flags, const char *dev_name, void *data);
typedef void (*mini_kill_sb_fn)(void *sb);

struct mini_file_system_type {
	const char *name;              /* 对应真实 name 字段 */
	int fs_flags;                  /* 对应真实 fs_flags，比如 FS_REQUIRES_DEV */
	mini_mount_fn mount;           /* 对应真实 mount 字段（本仓库已是 mount 接口） */
	mini_kill_sb_fn kill_sb;       /* 对应真实 kill_sb 字段 */
	struct mini_file_system_type *next;   /* 对应真实 next，侵入式单链表指针 */
};

/* ------------------------------------------------------------------ */
/* 2. Core：全局链表 file_systems，对应 fs/filesystems.c:32              */
/* ------------------------------------------------------------------ */

static struct mini_file_system_type *g_file_systems;

/* 对应 fs/filesystems.c:46 find_filesystem()：
 * 返回值是"指向某个 next 字段（或 file_systems 本身）的指针"，
 * 而不是"指向某个 file_system_type 的指针"——这正是它能同时支撑
 * "查找"和"在正确位置插入"两种用途的关键设计 */
static struct mini_file_system_type **find_filesystem(const char *name)
{
	struct mini_file_system_type **p;
	for (p = &g_file_systems; *p; p = &(*p)->next) {
		if (strcmp((*p)->name, name) == 0)
			break;
	}
	return p;
}

/* 对应 fs/filesystems.c:69 register_filesystem() */
static int mini_register_filesystem(struct mini_file_system_type *fs)
{
	struct mini_file_system_type **p;

	if (fs->next) {
		printf("  [register_filesystem] '%s' 的 next 非 NULL，已经注册过，拒绝(-EBUSY)\n",
		       fs->name);
		return -1;
	}

	p = find_filesystem(fs->name);
	if (*p) {
		printf("  [register_filesystem] 名字 '%s' 已存在，拒绝(-EBUSY)\n", fs->name);
		return -1;
	}

	*p = fs;   /* 关键行：这里 p 指向的是链表末尾节点的 next（或 g_file_systems 本身），
		    * 所以新节点被"追加"到末尾，不是插入头部 */
	printf("  [register_filesystem] '%s' 注册成功，追加到链表末尾\n", fs->name);
	return 0;
}

/* 对应 fs/filesystems.c:102 unregister_filesystem() */
static int mini_unregister_filesystem(struct mini_file_system_type *fs)
{
	struct mini_file_system_type **tmp = &g_file_systems;

	while (*tmp) {
		if (*tmp == fs) {
			*tmp = fs->next;
			fs->next = NULL;
			printf("  [unregister_filesystem] '%s' 已从链表摘除\n", fs->name);
			return 0;
		}
		tmp = &(*tmp)->next;
	}
	printf("  [unregister_filesystem] 没有找到 '%s'，返回 -EINVAL\n", fs->name);
	return -1;
}

/* 对应 fs/filesystems.c:224 get_filesystem_list() / :222 filesystems_proc_show()，
 * 即 /proc/filesystems 背后的真实实现 */
static void mini_print_filesystems_proc(void)
{
	struct mini_file_system_type *tmp;
	printf("  ---- /proc/filesystems 风格输出（按链表顺序） ----\n");
	for (tmp = g_file_systems; tmp; tmp = tmp->next) {
		printf("  %s\t%s\n",
		       (tmp->fs_flags & 0x1) ? "" : "nodev", tmp->name);
	}
	printf("  --------------------------------------------------\n");
}

/* ------------------------------------------------------------------ */
/* 3. 三种"文件系统实现"，只填契约字段，不关心自己在链表里的位置          */
/* ------------------------------------------------------------------ */

#define FS_REQUIRES_DEV 0x1

static void *dummy_mount(struct mini_file_system_type *t, int flags,
			  const char *dev_name, void *data)
{
	(void)flags; (void)data;
	printf("    [%s] mount(dev_name='%s')\n", t->name, dev_name ? dev_name : "(none)");
	return (void *)1;
}

static void dummy_kill_sb(void *sb) { (void)sb; }

static struct mini_file_system_type ext2_fs_type = {
	.name = "ext2", .fs_flags = FS_REQUIRES_DEV, .mount = dummy_mount, .kill_sb = dummy_kill_sb,
};
static struct mini_file_system_type proc_fs_type = {
	.name = "proc", .fs_flags = 0, .mount = dummy_mount, .kill_sb = dummy_kill_sb,
};
static struct mini_file_system_type tmpfs_fs_type = {
	.name = "tmpfs", .fs_flags = 0, .mount = dummy_mount, .kill_sb = dummy_kill_sb,
};

int main(void)
{
	printf("=== file_system_type_registry ===\n\n");

	printf("-- 依次注册 ext2 / proc / tmpfs --\n");
	mini_register_filesystem(&ext2_fs_type);
	mini_register_filesystem(&proc_fs_type);
	mini_register_filesystem(&tmpfs_fs_type);
	printf("\n");
	mini_print_filesystems_proc();

	printf("\n-- 重复注册 'ext2'，验证 -EBUSY --\n");
	struct mini_file_system_type dup_ext2 = { .name = "ext2", .mount = dummy_mount };
	mini_register_filesystem(&dup_ext2);

	printf("\n-- 按名字查找 'proc' --\n");
	struct mini_file_system_type **found = find_filesystem("proc");
	if (*found)
		printf("  找到: name='%s'\n", (*found)->name);

	printf("\n-- 注销 'proc'（链表中间节点），验证链表结构仍然完整 --\n");
	mini_unregister_filesystem(&proc_fs_type);
	mini_print_filesystems_proc();

	printf("\n-- 再注册一个新的 'proc2'，验证它被追加到当前末尾（而不是头部）--\n");
	struct mini_file_system_type proc2_fs_type = {
		.name = "proc2", .fs_flags = 0, .mount = dummy_mount, .kill_sb = dummy_kill_sb,
	};
	mini_register_filesystem(&proc2_fs_type);
	mini_print_filesystems_proc();

	printf("\n=== done ===\n");
	return 0;
}
