/*
 * mount_dispatch_demo.c
 *
 * 场景：模拟 mount 系统调用按文件系统名分发到对应的 mount 回调，
 * 展示"名字查找 + 回调分发"这两步流程——这正是 02 篇 Ops Table 与
 * 05 篇 Notifier Chain 风格的注册表在"挂载"这个具体场景下的结合。
 *
 * 对照真实实现（本仓库 /home/morrism/linux/linux）：
 *   fs/namespace.c:1961     do_kern_mount() 里 `get_fs_type(fstype)`   <-- 第一步：按名字查找
 *   fs/filesystems.c:270    get_fs_type()：找不到会先 request_module() 再重试一次
 *   fs/super.c:1075         mount_fs(type, flags, name, data)
 *   fs/super.c:1092         `root = type->mount(type, flags, name, data);` <-- 第二步：盲调
 *   fs/ext2/super.c:1490    真实 ext2_fs_type 的定义（.mount = ext2_mount）
 *
 * 关键点：VFS 核心（do_mount 路径）完全不知道 ext2/proc/tmpfs 各自怎么解析
 * 超级块、怎么初始化 root dentry——它只知道"给我一个名字，我找到对应的
 * file_system_type，然后调用它的 mount 回调"，两步之间用字符串 name 解耦。
 *
 * 编译：gcc -Wall -Wextra -O2 -std=gnu11 -o mount_dispatch_demo mount_dispatch_demo.c
 * 运行：./mount_dispatch_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ------------------------------------------------------------------ */
/* 1. 契约                                                              */
/* ------------------------------------------------------------------ */

struct mini_super_block {
	char fs_name[32];
	char dev_name[32];
	int  block_size;
	char root_content[64];   /* 演示用：这个"文件系统"的根目录内容摘要 */
};

struct mini_file_system_type;

/* 对应真实 struct dentry *(*mount)(struct file_system_type *, int, const char *, void *) */
typedef int (*mini_mount_fn)(struct mini_file_system_type *fs_type, int flags,
			      const char *dev_name, void *data, struct mini_super_block *out_sb);

struct mini_file_system_type {
	const char *name;
	int fs_flags;
	mini_mount_fn mount;
	struct mini_file_system_type *next;
};

#define FS_REQUIRES_DEV 0x1

/* ------------------------------------------------------------------ */
/* 2. Core：注册表 + 两步分发逻辑（对应 get_fs_type + mount_fs）          */
/* ------------------------------------------------------------------ */

static struct mini_file_system_type *g_file_systems;

static int mini_register_filesystem(struct mini_file_system_type *fs)
{
	struct mini_file_system_type **p = &g_file_systems;
	while (*p)
		p = &(*p)->next;
	*p = fs;
	return 0;
}

/* 对应 fs/filesystems.c:270 get_fs_type() 的简化版（省略了 request_module 重试） */
static struct mini_file_system_type *mini_get_fs_type(const char *name)
{
	struct mini_file_system_type *p;
	for (p = g_file_systems; p; p = p->next) {
		if (strcmp(p->name, name) == 0)
			return p;
	}
	return NULL;
}

/*
 * mini_do_mount()：对应真实 do_kern_mount()（fs/namespace.c:1958）+
 * mount_fs()（fs/super.c:1075）的合并简化版——
 * 第一步：按名字在注册表里查找 file_system_type（字符串解耦）；
 * 第二步：拿到 file_system_type 后，盲调它的 mount 回调（Ops Table 风格分发）。
 * 本函数全程不知道 ext2_mount/procfs_mount/tmpfs_mount 内部做了什么。
 */
static int mini_do_mount(const char *fstype, const char *dev_name, void *data,
			  struct mini_super_block *out_sb)
{
	printf("  [do_mount] 请求挂载 fstype='%s' dev_name='%s'\n", fstype, dev_name ? dev_name : "(none)");

	/* 第一步：名字查找 */
	struct mini_file_system_type *type = mini_get_fs_type(fstype);
	if (!type) {
		printf("  [do_mount] 未找到文件系统类型 '%s'，返回 -ENODEV\n", fstype);
		return -19;   /* -ENODEV */
	}
	printf("  [do_mount] 命中注册表: file_system_type{name='%s', fs_flags=0x%x}\n",
	       type->name, type->fs_flags);

	if ((type->fs_flags & FS_REQUIRES_DEV) && !dev_name) {
		printf("  [do_mount] '%s' 要求块设备但 dev_name 为空，返回 -EINVAL\n", fstype);
		return -22;   /* -EINVAL */
	}

	/* 第二步：盲调 —— 对应 fs/super.c:1092 root = type->mount(type, flags, name, data); */
	printf("  [do_mount] 分发到 type->mount() 回调...\n");
	int ret = type->mount(type, 0, dev_name, data, out_sb);
	if (ret == 0)
		printf("  [do_mount] 挂载成功: root_content=\"%s\"\n", out_sb->root_content);
	else
		printf("  [do_mount] 挂载失败, ret=%d\n", ret);
	return ret;
}

/* ------------------------------------------------------------------ */
/* 3. 三种文件系统各自的 mount 回调实现（对应 ext2_mount/proc_mount/...） */
/* ------------------------------------------------------------------ */

/* 对应 fs/ext2/super.c 里 ext2_mount -> mount_bdev -> ext2_fill_super 这条链路的简化版 */
static int ext2_style_mount(struct mini_file_system_type *t, int flags,
			     const char *dev_name, void *data, struct mini_super_block *out_sb)
{
	(void)flags; (void)data;
	if (!dev_name)
		return -22;
	snprintf(out_sb->fs_name, sizeof(out_sb->fs_name), "%s", t->name);
	snprintf(out_sb->dev_name, sizeof(out_sb->dev_name), "%s", dev_name);
	out_sb->block_size = 4096;
	snprintf(out_sb->root_content, sizeof(out_sb->root_content),
		 "ext2 root inode from %s (block_size=4096)", dev_name);
	return 0;
}

/* 对应 fs/proc/root.c 里 proc_mount -> mount_ns 这条链路（不需要块设备）的简化版 */
static int proc_style_mount(struct mini_file_system_type *t, int flags,
			     const char *dev_name, void *data, struct mini_super_block *out_sb)
{
	(void)flags; (void)dev_name; (void)data;
	snprintf(out_sb->fs_name, sizeof(out_sb->fs_name), "%s", t->name);
	out_sb->dev_name[0] = '\0';
	out_sb->block_size = 0;
	snprintf(out_sb->root_content, sizeof(out_sb->root_content),
		 "procfs pseudo-root (pid entries generated on the fly)");
	return 0;
}

/* 对应 mount_nodev 风格：tmpfs 也不需要真实块设备 */
static int tmpfs_style_mount(struct mini_file_system_type *t, int flags,
			      const char *dev_name, void *data, struct mini_super_block *out_sb)
{
	(void)flags; (void)dev_name;
	const char *size_opt = data ? (const char *)data : "default";
	snprintf(out_sb->fs_name, sizeof(out_sb->fs_name), "%s", t->name);
	out_sb->dev_name[0] = '\0';
	out_sb->block_size = 4096;
	snprintf(out_sb->root_content, sizeof(out_sb->root_content),
		 "tmpfs in-memory root (size=%s)", size_opt);
	return 0;
}

static struct mini_file_system_type ext2_fs_type = {
	.name = "ext2", .fs_flags = FS_REQUIRES_DEV, .mount = ext2_style_mount,
};
static struct mini_file_system_type proc_fs_type = {
	.name = "proc", .fs_flags = 0, .mount = proc_style_mount,
};
static struct mini_file_system_type tmpfs_fs_type = {
	.name = "tmpfs", .fs_flags = 0, .mount = tmpfs_style_mount,
};

int main(void)
{
	struct mini_super_block sb;

	printf("=== mount_dispatch_demo ===\n\n");

	mini_register_filesystem(&ext2_fs_type);
	mini_register_filesystem(&proc_fs_type);
	mini_register_filesystem(&tmpfs_fs_type);

	printf("-- 挂载 1: mount -t ext2 /dev/sda1 /mnt --\n");
	mini_do_mount("ext2", "/dev/sda1", NULL, &sb);

	printf("\n-- 挂载 2: mount -t proc proc /proc --\n");
	mini_do_mount("proc", NULL, NULL, &sb);

	printf("\n-- 挂载 3: mount -t tmpfs -o size=64m tmpfs /tmp --\n");
	mini_do_mount("tmpfs", NULL, "64m", &sb);

	printf("\n-- 挂载 4: mount -t ext2 (忘了指定设备) --\n");
	mini_do_mount("ext2", NULL, NULL, &sb);

	printf("\n-- 挂载 5: mount -t btrfs /dev/sdb1 /data（未注册的文件系统）--\n");
	mini_do_mount("btrfs", "/dev/sdb1", NULL, &sb);

	printf("\n=== done ===\n");
	return 0;
}
