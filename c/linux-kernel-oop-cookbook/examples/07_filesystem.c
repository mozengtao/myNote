/*
 * 07_filesystem.c - Mini VFS: EXT4 / XFS / FAT via super_operations style
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kobj.h"

struct super_block;
struct inode;

struct super_ops {
	struct inode *(*alloc_inode)(struct super_block *sb);
	int  (*write_inode)(struct inode *inode);
	void (*put_super)(struct super_block *sb);
};

struct inode_ops {
	int (*lookup)(struct inode *dir, const char *name);
	int (*create)(struct inode *dir, const char *name);
};

struct inode {
	unsigned long ino;
	struct super_block *sb;
	const struct inode_ops *i_op;
};

struct super_block {
	const char *fstype;
	const char *dev;
	const struct super_ops *s_op;
	void *fs_info; /* private per-fs state */
};

/* ---- EXT4 ---- */

struct ext4_sb_info {
	struct super_block sb; /* embedded base */
	int block_size;
	unsigned long next_ino;
};

struct ext4_inode {
	struct inode vfs_inode;
	int blocks;
};

static struct inode *ext4_alloc_inode(struct super_block *sb)
{
	struct ext4_sb_info *sbi = container_of(sb, struct ext4_sb_info, sb);
	struct ext4_inode *ei = calloc(1, sizeof(*ei));

	if (!ei)
		return NULL;
	ei->vfs_inode.ino = sbi->next_ino++;
	ei->vfs_inode.sb  = sb;
	ei->blocks = 0;
	printf("[EXT4] alloc_inode ino=%lu block_size=%d\n",
	       ei->vfs_inode.ino, sbi->block_size);
	return &ei->vfs_inode;
}

static int ext4_write_inode(struct inode *inode)
{
	struct ext4_inode *ei = container_of(inode, struct ext4_inode, vfs_inode);

	printf("[EXT4] write_inode ino=%lu blocks=%d\n", inode->ino, ei->blocks);
	return 0;
}

static void ext4_put_super(struct super_block *sb)
{
	struct ext4_sb_info *sbi = container_of(sb, struct ext4_sb_info, sb);

	printf("[EXT4] put_super dev=%s\n", sb->dev);
	free(sbi);
}

static const struct super_ops ext4_sops = {
	.alloc_inode = ext4_alloc_inode,
	.write_inode = ext4_write_inode,
	.put_super   = ext4_put_super,
};

/* ---- XFS ---- */

struct xfs_sb_info {
	struct super_block sb;
	unsigned long agcount;
	unsigned long next_ino;
};

struct xfs_inode {
	struct inode vfs_inode;
	unsigned long agno;
};

static struct inode *xfs_alloc_inode(struct super_block *sb)
{
	struct xfs_sb_info *xsb = container_of(sb, struct xfs_sb_info, sb);
	struct xfs_inode *xi = calloc(1, sizeof(*xi));

	if (!xi)
		return NULL;
	xi->vfs_inode.ino = xsb->next_ino++;
	xi->vfs_inode.sb  = sb;
	xi->agno = xi->vfs_inode.ino % xsb->agcount;
	printf("[XFS] alloc_inode ino=%lu agno=%lu agcount=%lu\n",
	       xi->vfs_inode.ino, xi->agno, xsb->agcount);
	return &xi->vfs_inode;
}

static int xfs_write_inode(struct inode *inode)
{
	struct xfs_inode *xi = container_of(inode, struct xfs_inode, vfs_inode);

	printf("[XFS] write_inode ino=%lu agno=%lu\n", inode->ino, xi->agno);
	return 0;
}

static void xfs_put_super(struct super_block *sb)
{
	struct xfs_sb_info *xsb = container_of(sb, struct xfs_sb_info, sb);

	printf("[XFS] put_super dev=%s\n", sb->dev);
	free(xsb);
}

static const struct super_ops xfs_sops = {
	.alloc_inode = xfs_alloc_inode,
	.write_inode = xfs_write_inode,
	.put_super   = xfs_put_super,
};

/* ---- FAT ---- */

struct fat_sb_info {
	struct super_block sb;
	int fat_bits; /* 12/16/32 */
	unsigned long next_ino;
};

struct fat_inode {
	struct inode vfs_inode;
	unsigned int cluster;
};

static struct inode *fat_alloc_inode(struct super_block *sb)
{
	struct fat_sb_info *fsi = container_of(sb, struct fat_sb_info, sb);
	struct fat_inode *fi = calloc(1, sizeof(*fi));

	if (!fi)
		return NULL;
	fi->vfs_inode.ino = fsi->next_ino++;
	fi->vfs_inode.sb  = sb;
	fi->cluster = (unsigned int)fi->vfs_inode.ino + 2;
	printf("[FAT] alloc_inode ino=%lu cluster=%u fat_bits=%d\n",
	       fi->vfs_inode.ino, fi->cluster, fsi->fat_bits);
	return &fi->vfs_inode;
}

static int fat_write_inode(struct inode *inode)
{
	struct fat_inode *fi = container_of(inode, struct fat_inode, vfs_inode);

	printf("[FAT] write_inode ino=%lu cluster=%u\n",
	       inode->ino, fi->cluster);
	return 0;
}

static void fat_put_super(struct super_block *sb)
{
	struct fat_sb_info *fsi = container_of(sb, struct fat_sb_info, sb);

	printf("[FAT] put_super dev=%s\n", sb->dev);
	free(fsi);
}

static const struct super_ops fat_sops = {
	.alloc_inode = fat_alloc_inode,
	.write_inode = fat_write_inode,
	.put_super   = fat_put_super,
};

/* VFS framework helpers */
static void vfs_mount_demo(struct super_block *sb)
{
	struct inode *ino;

	printf("--- mount %s on %s ---\n", sb->fstype, sb->dev);
	ino = sb->s_op->alloc_inode(sb);
	if (ino) {
		sb->s_op->write_inode(ino);
		/* free derived inode via knowing layout - demo only */
		free(ino);
	}
}

int main(void)
{
	struct ext4_sb_info *ext4;
	struct xfs_sb_info *xfs;
	struct fat_sb_info *fat;

	printf("=== 07 Filesystem (Mini VFS) ===\n");

	ext4 = calloc(1, sizeof(*ext4));
	ext4->sb.fstype = "ext4";
	ext4->sb.dev = "/dev/sda1";
	ext4->sb.s_op = &ext4_sops;
	ext4->block_size = 4096;
	ext4->next_ino = 1;
	vfs_mount_demo(&ext4->sb);
	ext4->sb.s_op->put_super(&ext4->sb);

	xfs = calloc(1, sizeof(*xfs));
	xfs->sb.fstype = "xfs";
	xfs->sb.dev = "/dev/sdb1";
	xfs->sb.s_op = &xfs_sops;
	xfs->agcount = 4;
	xfs->next_ino = 1;
	vfs_mount_demo(&xfs->sb);
	xfs->sb.s_op->put_super(&xfs->sb);

	fat = calloc(1, sizeof(*fat));
	fat->sb.fstype = "vfat";
	fat->sb.dev = "/dev/sdc1";
	fat->sb.s_op = &fat_sops;
	fat->fat_bits = 32;
	fat->next_ino = 1;
	vfs_mount_demo(&fat->sb);
	fat->sb.s_op->put_super(&fat->sb);

	printf("Done.\n");
	return 0;
}
