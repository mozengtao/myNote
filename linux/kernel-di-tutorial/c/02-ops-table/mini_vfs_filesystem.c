/*
 * mini_vfs_filesystem.c
 *
 * 场景：模拟一个"迷你 VFS”—— 直接对应 Kernel 的
 * struct file { const struct file_operations *f_op; ... }
 *
 * 我们实现三种"文件系统"：
 *   - ramfs    ：数据全在内存 buffer 里，可读可写
 *   - devnull  ：写入直接丢弃、读取永远返回 0 字节（模拟 /dev/null）
 *   - readonly ：只能读，写入返回 -EROFS 风格错误
 *
 * VFS core（mini_vfs_read / mini_vfs_write）永远不知道背后是哪一种，
 * 只通过 file->f_op->read/write 调用 —— 与 vfs_read()/vfs_write() 调用
 * file->f_op->read/write 的结构完全一致。
 *
 * 编译：
 *   gcc -Wall -Wextra -O2 -o mini_vfs_filesystem mini_vfs_filesystem.c
 * 运行：
 *   ./mini_vfs_filesystem
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define EROFS_SIM   (-30)   /* 模拟 -EROFS，用户态没有该宏，这里手写一个数值 */

struct mini_file;

/* ------------------------------------------------------------------ */
/* 1. 契约：file_operations —— Kernel include/linux/fs.h:1583 的简化版 */
/* ------------------------------------------------------------------ */

struct mini_file_operations {
	const char *fs_name;
	int     (*open)(struct mini_file *f);
	ssize_t (*read)(struct mini_file *f, char *buf, size_t count);
	ssize_t (*write)(struct mini_file *f, const char *buf, size_t count);
	int     (*release)(struct mini_file *f);   /* 对应 f_op->release */
};

struct mini_file {
	char name[64];
	const struct mini_file_operations *f_op;   /* <-- 注入点，等价于 file->f_op */
	void *private_data;                         /* 等价于 file->private_data，见 03 篇 */
	size_t pos;                                  /* 读写游标 */
};

/* ------------------------------------------------------------------ */
/* 2. VFS Core：只认 f_op，从不 #include 任何具体文件系统实现          */
/* ------------------------------------------------------------------ */

static ssize_t mini_vfs_read(struct mini_file *f, char *buf, size_t count)
{
	if (!f->f_op->read) {
		fprintf(stderr, "[vfs] %s has no read(), -EINVAL\n", f->name);
		return -1;
	}
	return f->f_op->read(f, buf, count);   /* <-- 盲调 */
}

static ssize_t mini_vfs_write(struct mini_file *f, const char *buf, size_t count)
{
	if (!f->f_op->write) {
		fprintf(stderr, "[vfs] %s has no write(), -EINVAL\n", f->name);
		return -1;
	}
	return f->f_op->write(f, buf, count);   /* <-- 盲调 */
}

static struct mini_file *mini_vfs_open(const char *name, const struct mini_file_operations *fop)
{
	struct mini_file *f = calloc(1, sizeof(*f));
	strncpy(f->name, name, sizeof(f->name) - 1);
	f->f_op = fop;   /* <-- 关键的一行注入：VFS 只是把指针存起来 */

	printf("[vfs] open('%s') via fs='%s'\n", name, fop->fs_name);
	if (fop->open)
		fop->open(f);
	return f;
}

static void mini_vfs_close(struct mini_file *f)
{
	if (f->f_op->release)
		f->f_op->release(f);
	printf("[vfs] closed '%s'\n", f->name);
	free(f);
}

/* ------------------------------------------------------------------ */
/* 3. ramfs：内存文件系统实现                                          */
/* ------------------------------------------------------------------ */

struct ramfs_data {
	char buf[256];
	size_t len;
};

static int ramfs_open(struct mini_file *f)
{
	struct ramfs_data *d = calloc(1, sizeof(*d));
	f->private_data = d;
	printf("  [ramfs] allocated backing buffer for '%s'\n", f->name);
	return 0;
}

static ssize_t ramfs_read(struct mini_file *f, char *buf, size_t count)
{
	struct ramfs_data *d = f->private_data;
	size_t remain = d->len > f->pos ? d->len - f->pos : 0;
	size_t n = count < remain ? count : remain;

	memcpy(buf, d->buf + f->pos, n);
	f->pos += n;
	return (ssize_t)n;
}

static ssize_t ramfs_write(struct mini_file *f, const char *buf, size_t count)
{
	struct ramfs_data *d = f->private_data;
	size_t space = sizeof(d->buf) - d->len;
	size_t n = count < space ? count : space;

	memcpy(d->buf + d->len, buf, n);
	d->len += n;
	return (ssize_t)n;
}

static int ramfs_release(struct mini_file *f)
{
	printf("  [ramfs] freeing backing buffer for '%s'\n", f->name);
	free(f->private_data);
	f->private_data = NULL;
	return 0;
}

static const struct mini_file_operations ramfs_fops = {
	.fs_name = "ramfs",
	.open    = ramfs_open,
	.read    = ramfs_read,
	.write   = ramfs_write,
	.release = ramfs_release,
};

/* ------------------------------------------------------------------ */
/* 4. devnull：模拟 /dev/null                                          */
/* ------------------------------------------------------------------ */

static ssize_t devnull_read(struct mini_file *f, char *buf, size_t count)
{
	(void)f; (void)buf; (void)count;
	return 0;   /* 永远 EOF */
}

static ssize_t devnull_write(struct mini_file *f, const char *buf, size_t count)
{
	(void)f; (void)buf;
	return (ssize_t)count;   /* 假装全部写成功，但什么都没存 */
}

static const struct mini_file_operations devnull_fops = {
	.fs_name = "devnull",
	.read    = devnull_read,
	.write   = devnull_write,
	/* 注意：没有 open/release，VFS 必须能容忍这两个字段是 NULL */
};

/* ------------------------------------------------------------------ */
/* 5. readonly：只读文件系统，write 返回错误                            */
/* ------------------------------------------------------------------ */

static ssize_t readonly_read(struct mini_file *f, char *buf, size_t count)
{
	static const char content[] = "static readonly content\n";
	size_t len = sizeof(content) - 1;
	size_t remain = len > f->pos ? len - f->pos : 0;
	size_t n = count < remain ? count : remain;

	memcpy(buf, content + f->pos, n);
	f->pos += n;
	return (ssize_t)n;
}

static ssize_t readonly_write(struct mini_file *f, const char *buf, size_t count)
{
	(void)f; (void)buf; (void)count;
	fprintf(stderr, "  [readonly] write rejected, -EROFS(%d)\n", EROFS_SIM);
	return EROFS_SIM;
}

static const struct mini_file_operations readonly_fops = {
	.fs_name = "readonly",
	.read    = readonly_read,
	.write   = readonly_write,
};

int main(void)
{
	struct mini_file *fa, *fb, *fc;
	char rbuf[128];
	ssize_t n;

	printf("=== mini_vfs_filesystem demo ===\n\n");

	/* -- ramfs -- */
	fa = mini_vfs_open("memo.txt", &ramfs_fops);
	mini_vfs_write(fa, "hello ramfs\n", 12);
	memset(rbuf, 0, sizeof(rbuf));
	n = mini_vfs_read(fa, rbuf, sizeof(rbuf));
	printf("[core] read %zd bytes from memo.txt: %s", n, rbuf);
	mini_vfs_close(fa);

	printf("\n");

	/* -- devnull -- */
	fb = mini_vfs_open("null", &devnull_fops);
	n = mini_vfs_write(fb, "this goes nowhere\n", 18);
	printf("[core] devnull write() returned %zd (accepted but discarded)\n", n);
	n = mini_vfs_read(fb, rbuf, sizeof(rbuf));
	printf("[core] devnull read() returned %zd (always EOF)\n", n);
	mini_vfs_close(fb);

	printf("\n");

	/* -- readonly -- */
	fc = mini_vfs_open("etc-hosts", &readonly_fops);
	memset(rbuf, 0, sizeof(rbuf));
	n = mini_vfs_read(fc, rbuf, sizeof(rbuf));
	printf("[core] read %zd bytes from readonly fs: %s", n, rbuf);
	n = mini_vfs_write(fc, "try to modify\n", 14);
	printf("[core] write to readonly fs returned %zd (expected negative)\n", n);
	mini_vfs_close(fc);

	printf("\n=== done ===\n");
	return 0;
}
