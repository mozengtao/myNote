# Linux Kernel v3.2 VFS Inode Methods: Code Reference and Snippets

Companion to **vfs_inode_design_patterns_v3.2.md**. This file provides runnable/annotated code references from the v3.2 kernel for inode methods, cache, and lifecycle. Use it when implementing or debugging inode operations in a kernel module.

---

## 1. Key Structures (v3.2)

### inode_operations (include/linux/fs.h)

```c
struct inode_operations {
	struct dentry * (*lookup) (struct inode *, struct dentry *, struct nameidata *);
	void * (*follow_link) (struct dentry *, struct nameidata *);
	int (*permission) (struct inode *, int);
	struct posix_acl * (*get_acl)(struct inode *, int);
	int (*readlink) (struct dentry *, char __user *, int);
	void (*put_link) (struct dentry *, struct nameidata *, void *);
	int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
	int (*link) (struct dentry *, struct inode *, struct dentry *);
	int (*unlink) (struct inode *, struct dentry *);
	int (*symlink) (struct inode *, struct dentry *, const char *);
	int (*mkdir) (struct inode *, struct dentry *, int);
	int (*rmdir) (struct inode *, struct dentry *);
	int (*mknod) (struct inode *, struct dentry *, int, dev_t);
	int (*rename) (struct inode *, struct dentry *, struct inode *, struct dentry *);
	void (*truncate) (struct inode *);
	int (*setattr) (struct dentry *, struct iattr *);
	int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
	int (*setxattr) (struct dentry *, const char *, const void *, size_t, int);
	ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
	ssize_t (*listxattr) (struct dentry *, char *, size_t);
	int (*removexattr) (struct dentry *, const char *);
	void (*truncate_range)(struct inode *, loff_t, loff_t);
	int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start, u64 len);
} ____cacheline_aligned;
```

### container_of (include/linux/kernel.h)

```c
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type, member) );})
```

---

## 2. Inode Initialisation (fs/inode.c)

### inode_init_always

Called for every inode after allocation. Sets default `i_op` and `i_fop` to empty tables so that no method is called uninitialised:

```c
int inode_init_always(struct super_block *sb, struct inode *inode)
{
	static const struct inode_operations empty_iops;
	static const struct file_operations empty_fops;
	struct address_space *const mapping = &inode->i_data;

	inode->i_sb = sb;
	inode->i_blkbits = sb->s_blocksize_bits;
	inode->i_flags = 0;
	atomic_set(&inode->i_count, 1);
	inode->i_op = &empty_iops;
	inode->i_fop = &empty_fops;
	inode->__i_nlink = 1;
	/* ... locks, mapping, security, etc. ... */
	return 0;
}
```

Your filesystem later overwrites `inode->i_op` (and often `i_fop`, `i_mapping->a_ops`) when it fills the inode (e.g. in `ext2_iget` or after `iget_locked`).

### alloc_inode

Used by the VFS to obtain a new inode; either the FS allocates a larger struct and returns the embedded inode, or the VFS uses the generic cache:

```c
static struct inode *alloc_inode(struct super_block *sb)
{
	struct inode *inode;

	if (sb->s_op->alloc_inode)
		inode = sb->s_op->alloc_inode(sb);
	else
		inode = kmem_cache_alloc(inode_cachep, GFP_KERNEL);

	if (!inode)
		return NULL;

	if (unlikely(inode_init_always(sb, inode))) {
		if (inode->i_sb->s_op->destroy_inode)
			inode->i_sb->s_op->destroy_inode(inode);
		else
			kmem_cache_free(inode_cachep, inode);
		return NULL;
	}
	return inode;
}
```

---

## 3. Inode Cache: iget_locked and unlock_new_inode (fs/inode.c)

### iget_locked

Returns an inode from cache or allocates a new one with `I_NEW` set; the caller (filesystem) must fill it and call `unlock_new_inode`:

```c
struct inode *iget_locked(struct super_block *sb, unsigned long ino)
{
	struct hlist_head *head = inode_hashtable + hash(sb, ino);
	struct inode *inode;

	spin_lock(&inode_hash_lock);
	inode = find_inode_fast(sb, head, ino);
	spin_unlock(&inode_hash_lock);
	if (inode) {
		wait_on_inode(inode);
		return inode;
	}

	inode = alloc_inode(sb);
	if (inode) {
		struct inode *old;

		spin_lock(&inode_hash_lock);
		old = find_inode_fast(sb, head, ino);
		if (!old) {
			inode->i_ino = ino;
			spin_lock(&inode->i_lock);
			inode->i_state = I_NEW;
			hlist_add_head(&inode->i_hash, head);
			spin_unlock(&inode->i_lock);
			inode_sb_list_add(inode);
			spin_unlock(&inode_hash_lock);
			return inode;   /* locked, I_NEW set */
		}
		spin_unlock(&inode_hash_lock);
		destroy_inode(inode);
		inode = old;
		wait_on_inode(inode);
	}
	return inode;
}
```

### unlock_new_inode

Must be called by the filesystem after filling a newly allocated inode returned by `iget_locked`:

```c
void unlock_new_inode(struct inode *inode)
{
	lockdep_annotate_inode_mutex_key(inode);
	spin_lock(&inode->i_lock);
	WARN_ON(!(inode->i_state & I_NEW));
	inode->i_state &= ~I_NEW;
	wake_up_bit(&inode->i_state, __I_NEW);
	spin_unlock(&inode->i_lock);
}
EXPORT_SYMBOL(unlock_new_inode);
```

---

## 4. VFS Calling inode Methods

### Lookup (fs/namei.c)

Path lookup calls the directory’s `lookup`; parent’s `i_mutex` is held:

```c
static struct dentry *d_inode_lookup(struct dentry *parent, struct dentry *dentry,
				     struct nameidata *nd)
{
	struct inode *inode = parent->d_inode;
	struct dentry *old;

	if (unlikely(IS_DEADDIR(inode)))
		return ERR_PTR(-ENOENT);

	old = inode->i_op->lookup(inode, dentry, nd);
	if (unlikely(old)) {
		dput(dentry);
		dentry = old;
	}
	return dentry;
}
```

### Permission (fs/namei.c)

Optional fast path; otherwise call FS `permission` or fall back to `generic_permission`:

```c
static inline int do_inode_permission(struct inode *inode, int mask)
{
	if (unlikely(!(inode->i_opflags & IOP_FASTPERM))) {
		if (likely(inode->i_op->permission))
			return inode->i_op->permission(inode, mask);
		spin_lock(&inode->i_lock);
		inode->i_opflags |= IOP_FASTPERM;
		spin_unlock(&inode->i_lock);
	}
	return generic_permission(inode, mask);
}
```

### xattr (fs/xattr.c)

Example of optional inode op; VFS checks for NULL:

```c
if (inode->i_op->setxattr)
	error = inode->i_op->setxattr(dentry, name, value, size, flags);
```

---

## 5. Filesystem Examples: ext2 (v3.2)

### Embedding and container_of (fs/ext2/ext2.h)

```c
struct ext2_inode_info {
	/* ... ext2-specific fields ... */
	struct inode vfs_inode;
	struct list_head i_orphan;
};

static inline struct ext2_inode_info *EXT2_I(struct inode *inode)
{
	return container_of(inode, struct ext2_inode_info, vfs_inode);
}
```

### alloc_inode and destroy_inode (fs/ext2/super.c)

```c
static struct inode *ext2_alloc_inode(struct super_block *sb)
{
	struct ext2_inode_info *ei;
	ei = (struct ext2_inode_info *)kmem_cache_alloc(ext2_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	ei->i_block_alloc_info = NULL;
	ei->vfs_inode.i_version = 1;
	return &ei->vfs_inode;
}

static void ext2_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	INIT_LIST_HEAD(&inode->i_dentry);
	kmem_cache_free(ext2_inode_cachep, EXT2_I(inode));
}

static void ext2_destroy_inode(struct inode *inode)
{
	call_rcu(&inode->i_rcu, ext2_i_callback);
}
```

### lookup and create (fs/ext2/namei.c)

```c
static struct dentry *ext2_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct inode *inode;
	ino_t ino;

	if (dentry->d_name.len > EXT2_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	ino = ext2_inode_by_name(dir, &dentry->d_name);
	inode = NULL;
	if (ino) {
		inode = ext2_iget(dir->i_sb, ino);
		/* ... error handling ... */
	}
	return d_splice_alias(inode, dentry);
}

static int ext2_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	struct inode *inode;

	dquot_initialize(dir);
	inode = ext2_new_inode(dir, mode, &dentry->d_name);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	inode->i_op = &ext2_file_inode_operations;
	/* ... set a_ops, i_fop ... */
	mark_inode_dirty(inode);
	return ext2_add_nondir(dentry, inode);
}
```

### evict_inode (fs/ext2/inode.c)

```c
void ext2_evict_inode(struct inode *inode)
{
	struct ext2_block_alloc_info *rsv;
	int want_delete = 0;

	if (!inode->i_nlink && !is_bad_inode(inode)) {
		want_delete = 1;
		dquot_initialize(inode);
	} else {
		dquot_drop(inode);
	}
	truncate_inode_pages(&inode->i_data, 0);
	if (want_delete) {
		EXT2_I(inode)->i_dtime = get_seconds();
		mark_inode_dirty(inode);
		__ext2_write_inode(inode, inode_needs_sync(inode));
		inode->i_size = 0;
		if (inode->i_blocks)
			ext2_truncate_blocks(inode, 0);
	}
	invalidate_inode_buffers(inode);
	end_writeback(inode);
	ext2_discard_reservation(inode);
	rsv = EXT2_I(inode)->i_block_alloc_info;
	EXT2_I(inode)->i_block_alloc_info = NULL;
	if (unlikely(rsv))
		kfree(rsv);
	if (want_delete)
		ext2_free_inode(inode);
}
```

---

## 6. Eviction and iput (fs/inode.c)

### generic_drop_inode and iput_final

```c
int generic_drop_inode(struct inode *inode)
{
	return !inode->i_nlink || inode_unhashed(inode);
}

static void iput_final(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	const struct super_operations *op = inode->i_sb->s_op;
	int drop;

	WARN_ON(inode->i_state & I_NEW);
	if (op->drop_inode)
		drop = op->drop_inode(inode);
	else
		drop = generic_drop_inode(inode);

	if (!drop && (sb->s_flags & MS_ACTIVE)) {
		inode->i_state |= I_REFERENCED;
		if (!(inode->i_state & (I_DIRTY|I_SYNC)))
			inode_lru_list_add(inode);
		spin_unlock(&inode->i_lock);
		return;
	}
	/* ... else evict path: I_FREEING, remove from LRU, evict(inode) ... */
}
```

### evict and destroy_inode

```c
static void evict(struct inode *inode)
{
	const struct super_operations *op = inode->i_sb->s_op;

	BUG_ON(!(inode->i_state & I_FREEING));
	BUG_ON(!list_empty(&inode->i_lru));
	/* ... list removal ... */
	if (op->evict_inode)
		op->evict_inode(inode);
	else {
		if (inode->i_data.nrpages)
			truncate_inode_pages(&inode->i_data, 0);
		end_writeback(inode);
	}
	/* ... bdev/cdev forget, remove from hash, wake I_NEW ... */
	destroy_inode(inode);
}

static void destroy_inode(struct inode *inode)
{
	BUG_ON(!list_empty(&inode->i_lru));
	__destroy_inode(inode);
	if (inode->i_sb->s_op->destroy_inode)
		inode->i_sb->s_op->destroy_inode(inode);
	else
		call_rcu(&inode->i_rcu, i_callback);
}
```

### generic_delete_inode (legacy)

```c
int generic_delete_inode(struct inode *inode)
{
	return 1;
}
EXPORT_SYMBOL(generic_delete_inode);
```

---

## 7. Inode Locking (fs/inode.c comments)

Relevant lock ordering in v3.2:

- **inode->i_lock**: protects `i_state`, `i_hash`, and `__iget()`.
- **inode_sb_list_lock**: protects `sb->s_inodes`, `inode->i_sb_list`.
- **inode->i_sb->s_inode_lru_lock**: protects `sb->s_inode_lru`, `inode->i_lru`.
- **inode_hash_lock**: protects `inode_hashtable`, `inode->i_hash`.

Ordering: e.g. `inode_sb_list_lock` → `inode->i_lock` → `s_inode_lru_lock`; `inode_hash_lock` with `inode_sb_list_lock` and `inode->i_lock` as documented in the file. When adding new code, follow this order to avoid deadlocks.

---

## 8. Quick Checklist for Custom Inode Methods (v3.2)

- [ ] Use v3.2 signatures (e.g. `lookup(inode, dentry, struct nameidata *nd)`).
- [ ] Set `inode->i_op` (and dir vs file/symlink variants) when the inode is created or read.
- [ ] After `iget_locked`, fill inode and call `unlock_new_inode(inode)`.
- [ ] If you embed `struct inode`, implement `alloc_inode`/`destroy_inode` and use `container_of` for your private data.
- [ ] Implement `evict_inode` if you have private data or on-disk teardown; call `truncate_inode_pages` and `end_writeback` as needed, then release your resources.
- [ ] Respect directory `i_mutex` and inode lock ordering when implementing `create`/`mkdir`/`lookup`.
- [ ] Handle `nameidata` where your op receives it (e.g. for LOOKUP_* flags or revalidation).

All references are to Linux kernel v3.2 source tree.
