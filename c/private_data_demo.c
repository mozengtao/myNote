/*
 * private_data 字段使用示例
 *
 * 目的：演示如何使用 void *private_data 存储文件系统特有的私有数据
 *
 * 编译：gcc -o private_data_demo private_data_demo.c -Wall
 * 运行：./private_data_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*============================================
 * 第一部分：通用框架定义（机制层）
 *============================================*/

#define MAX_DATA 256

struct file;  /* 前向声明 */

/* 文件操作表 */
struct file_operations {
	const char *fs_name;
	int (*open)(struct file *f);
	int (*read)(struct file *f, char *buf, int len);
	int (*write)(struct file *f, const char *buf, int len);
	int (*close)(struct file *f);
};

/* 通用文件结构 - 机制层只知道这个结构 */
struct file {
	char name[64];
	char data[MAX_DATA];
	int size;
	int pos;
	struct file_operations *f_op;
	void *private_data;   /* <-- 关键：策略层的私有数据 */
};

/*============================================
 * 第二部分：策略1 - 带统计信息的文件系统
 *============================================*/

/* 统计文件系统的私有数据结构 */
struct stats_private {
	int read_count;      /* 读取次数 */
	int write_count;     /* 写入次数 */
	int total_bytes_read;
	int total_bytes_written;
	time_t create_time;
};

static int stats_open(struct file *f)
{
	/* 分配并初始化私有数据 */
	struct stats_private *priv = malloc(sizeof(*priv));

	priv->read_count = 0;
	priv->write_count = 0;
	priv->total_bytes_read = 0;
	priv->total_bytes_written = 0;
	priv->create_time = time(NULL);

	/* 存入 private_data */
	f->private_data = priv;

	printf("[StatsFS] 打开文件，初始化统计数据\n");
	return 0;
}

static int stats_read(struct file *f, char *buf, int len)
{
	/* 从 private_data 取出私有数据 */
	struct stats_private *priv = f->private_data;

	int avail = f->size - f->pos;
	int n = (len < avail) ? len : avail;

	if (n > 0) {
		memcpy(buf, f->data + f->pos, n);
		f->pos += n;

		/* 更新统计 */
		priv->read_count++;
		priv->total_bytes_read += n;
	}

	printf("[StatsFS] 读取 %d 字节 (累计读取: %d次, %d字节)\n",
	       n, priv->read_count, priv->total_bytes_read);
	return n;
}

static int stats_write(struct file *f, const char *buf, int len)
{
	struct stats_private *priv = f->private_data;

	int space = MAX_DATA - f->pos;
	int n = (len < space) ? len : space;

	if (n > 0) {
		memcpy(f->data + f->pos, buf, n);
		f->pos += n;
		if (f->pos > f->size)
			f->size = f->pos;

		/* 更新统计 */
		priv->write_count++;
		priv->total_bytes_written += n;
	}

	printf("[StatsFS] 写入 %d 字节 (累计写入: %d次, %d字节)\n",
	       n, priv->write_count, priv->total_bytes_written);
	return n;
}

static int stats_close(struct file *f)
{
	struct stats_private *priv = f->private_data;

	/* 关闭时打印统计摘要 */
	printf("[StatsFS] 关闭文件，统计摘要:\n");
	printf("         读取: %d次, 共%d字节\n",
	       priv->read_count, priv->total_bytes_read);
	printf("         写入: %d次, 共%d字节\n",
	       priv->write_count, priv->total_bytes_written);
	printf("         文件存活时间: %ld秒\n",
	       time(NULL) - priv->create_time);

	/* 释放私有数据 */
	free(priv);
	f->private_data = NULL;

	return 0;
}

static struct file_operations stats_ops = {
	.fs_name = "StatsFS",
	.open    = stats_open,
	.read    = stats_read,
	.write   = stats_write,
	.close   = stats_close,
};

/*============================================
 * 第三部分：策略2 - 带配额限制的文件系统
 *============================================*/

/* 配额文件系统的私有数据结构 */
struct quota_private {
	int max_size;        /* 最大允许大小 */
	int used_size;       /* 已使用大小 */
	char owner[32];      /* 文件所有者 */
};

static int quota_open(struct file *f)
{
	struct quota_private *priv = malloc(sizeof(*priv));

	priv->max_size = 100;  /* 限制100字节 */
	priv->used_size = 0;
	strcpy(priv->owner, "user1");

	f->private_data = priv;

	printf("[QuotaFS] 打开文件，配额限制: %d字节, 所有者: %s\n",
	       priv->max_size, priv->owner);
	return 0;
}

static int quota_read(struct file *f, char *buf, int len)
{
	int avail = f->size - f->pos;
	int n = (len < avail) ? len : avail;

	if (n > 0) {
		memcpy(buf, f->data + f->pos, n);
		f->pos += n;
	}

	printf("[QuotaFS] 读取 %d 字节\n", n);
	return n;
}

static int quota_write(struct file *f, const char *buf, int len)
{
	struct quota_private *priv = f->private_data;

	/* 检查配额 */
	int remaining = priv->max_size - priv->used_size;
	if (len > remaining) {
		printf("[QuotaFS] 错误: 超出配额! 请求%d字节, 剩余%d字节\n",
		       len, remaining);
		len = remaining;  /* 只写入允许的部分 */
	}

	if (len > 0) {
		int space = MAX_DATA - f->pos;
		int n = (len < space) ? len : space;

		memcpy(f->data + f->pos, buf, n);
		f->pos += n;
		if (f->pos > f->size)
			f->size = f->pos;

		priv->used_size += n;
		printf("[QuotaFS] 写入 %d 字节 (已用: %d/%d)\n",
		       n, priv->used_size, priv->max_size);
		return n;
	}

	return 0;
}

static int quota_close(struct file *f)
{
	struct quota_private *priv = f->private_data;

	printf("[QuotaFS] 关闭文件，最终使用: %d/%d字节\n",
	       priv->used_size, priv->max_size);

	free(priv);
	f->private_data = NULL;
	return 0;
}

static struct file_operations quota_ops = {
	.fs_name = "QuotaFS",
	.open    = quota_open,
	.read    = quota_read,
	.write   = quota_write,
	.close   = quota_close,
};

/*============================================
 * 第四部分：VFS机制层（不关心private_data内容）
 *============================================*/

struct file *vfs_create(const char *name, struct file_operations *ops)
{
	struct file *f = calloc(1, sizeof(*f));

	strncpy(f->name, name, sizeof(f->name) - 1);
	f->f_op = ops;
	f->private_data = NULL;  /* 初始为空，由具体FS的open填充 */
	printf("VFS: 创建文件 '%s' (使用 %s)\n", name, ops->fs_name);
	return f;
}

int vfs_open(struct file *f)
{
	if (f->f_op && f->f_op->open)
		return f->f_op->open(f);
	return -1;
}

int vfs_read(struct file *f, char *buf, int len)
{
	if (f->f_op && f->f_op->read)
		return f->f_op->read(f, buf, len);
	return -1;
}

int vfs_write(struct file *f, const char *buf, int len)
{
	if (f->f_op && f->f_op->write)
		return f->f_op->write(f, buf, len);
	return -1;
}

int vfs_close(struct file *f)
{
	int ret = 0;

	if (f->f_op && f->f_op->close)
		ret = f->f_op->close(f);
	free(f);
	return ret;
}

/*============================================
 * 第五部分：测试主函数
 *============================================*/

int main(void)
{
	char buf[64];

	printf("========================================\n");
	printf("  private_data 使用示例\n");
	printf("========================================\n\n");

	/* 测试1：带统计的文件系统 */
	printf("【测试1】StatsFS - 统计文件系统\n");
	printf("----------------------------------------\n");
	struct file *f1 = vfs_create("stats.txt", &stats_ops);
	vfs_open(f1);
	vfs_write(f1, "Hello", 5);
	vfs_write(f1, " World!", 7);
	f1->pos = 0;
	vfs_read(f1, buf, 5);
	vfs_read(f1, buf, 7);
	vfs_read(f1, buf, 10);
	vfs_close(f1);

	printf("\n");

	/* 测试2：带配额的文件系统 */
	printf("【测试2】QuotaFS - 配额文件系统\n");
	printf("----------------------------------------\n");
	struct file *f2 = vfs_create("quota.txt", &quota_ops);
	vfs_open(f2);
	vfs_write(f2, "First write (30 bytes)........", 30);
	vfs_write(f2, "Second write (30 bytes).......", 30);
	vfs_write(f2, "Third write (30 bytes)........", 30);
	vfs_write(f2, "Fourth - should be limited....", 30);  /* 超出配额 */
	vfs_close(f2);

	printf("\n========================================\n");
	printf("  演示完成\n");
	printf("========================================\n");

	return 0;
}

