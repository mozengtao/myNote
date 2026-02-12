# Linux Kernel v3.2 VFS: Inode Methods and Design Patterns

A cross-filesystem overview of VFS design patterns for kernel developers targeting **Linux 3.2**, with inode methods as the central focus. All content is restricted to kernel 3.2 and kernel-space only (no FUSE/user-space).

---

## 1. Core Design Patterns in the VFS Layer

The VFS (Virtual File System) provides a single abstraction over many filesystem implementations (ext2, ext3, ext4, NFS, CIFS, etc.). It achieves this with three patterns implemented in plain C:

| Pattern | Mechanism in kernel | Where you see it |
|--------|----------------------|-------------------|
| **Polymorphism** | Function pointer tables (`inode_operations`, `file_operations`, `super_operations`) | VFS calls `inode->i_op->lookup(...)` without knowing the concrete FS |
| **Inheritance** | Embedded struct: FS-specific struct contains `struct inode` (or vice versa) | `struct ext2_inode_info { ...; struct inode vfs_inode; };` |
| **Upcast / “subclass” access** | `container_of(ptr, type, member)` | Getting `struct ext2_inode_info *` from `struct inode *` |

The inode is the primary object that ties these together: each mounted filesystem provides its own `struct inode` (or a wrapper that embeds it), its own `inode_operations` table, and the VFS calls methods through `inode->i_op` without caring which filesystem owns the inode.

---

## 2. struct inode and inode_operations (v3.2)

### 2.1 The inode structure (excerpt)

From `include/linux/fs.h` (v3.2), the inode is the VFS’s generic “file object” for a single file or directory:

- **Identity**: `i_ino`, `i_sb` (superblock)
- **Type and permissions**: `i_mode`, `i_uid`, `i_gid`
- **Size and layout**: `i_size`, `i_blocks`, `i_blkbits`
- **Times**: `i_atime`, `i_mtime`, `i_ctime`
- **Methods**: `const struct inode_operations *i_op`, `struct address_space *i_mapping` (with its `a_ops`)
- **Lifecycle and locking**: `i_state`, `i_count`, `i_lock`, `i_mutex`, hash and LRU list links

The key for polymorphism is:

```c
const struct inode_operations *i_op;
```

The VFS never calls a “ext2_lookup” directly; it always goes through `inode->i_op->lookup(...)`.

### 2.2 struct inode_operations (v3.2)

From `include/linux/fs.h` (lines 1613–1641) in kernel 3.2:

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

**v3.2-specific:** Many operations take `struct nameidata *` (e.g. `lookup`, `follow_link`, `put_link`, `create`). In later kernels (3.x refactor and 4.x) nameidata is reduced or removed from these paths; in 3.2 it is still the standard for path lookup context.

Any method may be `NULL`. The VFS checks before calling (e.g. `if (inode->i_op->permission) return inode->i_op->permission(...);`). A filesystem only fills in the operations it supports.

---

## 3. Polymorphism via Function Pointers

The VFS performs path lookup and other operations without knowing the concrete filesystem. Example from `fs/namei.c` (v3.2):

```c
/* fs/namei.c: d_inode_lookup() - parent->d_inode->i_mutex held */
static struct dentry *d_inode_lookup(struct dentry *parent, struct dentry *dentry,
				     struct nameidata *nd)
{
	struct inode *inode = parent->d_inode;
	struct dentry *old;

	if (unlikely(IS_DEADDIR(inode)))
		return ERR_PTR(-ENOENT);

	old = inode->i_op->lookup(inode, dentry, nd);   /* polymorphic call */
	if (unlikely(old)) {
		dput(dentry);
		dentry = old;
	}
	return dentry;
}
```

Same pattern for permission (from `fs/namei.c`):

```c
if (likely(inode->i_op->permission))
	return inode->i_op->permission(inode, mask);
```

So:

- **ext2**: `inode->i_op` points to `ext2_dir_inode_operations` (or file/symlink variants), so `lookup` is `ext2_lookup`.
- **NFS**: `inode->i_op` points to NFS’s table, so `lookup` is NFS’s lookup.
- **CIFS, XFS, etc.**: Each sets its own `i_op`; the VFS code is shared.

This is “object-oriented” polymorphism in C: the same call site invokes different implementations depending on the object (inode) and its method table (`i_op`).

---

## 4. Inheritance via Embedded struct and container_of

Filesystems need extra data per inode (e.g. ext2 block group, xattr state). In 3.2 the common pattern is to **embed** `struct inode` inside a larger, FS-specific struct.

### 4.1 Embedding: ext2 example

From `fs/ext2/ext2.h`:

```c
struct ext2_inode_info {
	__le32   i_data[15];
	__u32    i_flags;
	/* ... other ext2-specific fields ... */
	struct mutex truncate_mutex;
	struct inode vfs_inode;   /* VFS inode at end */
	struct list_head i_orphan;
};
```

So each inode in memory is really an `ext2_inode_info`; the VFS sees the `struct inode` part.

### 4.2 container_of: from VFS inode to FS-specific inode

From `include/linux/kernel.h`:

```c
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type, member) );})
```

Given a pointer to a member inside a struct, `container_of` returns a pointer to the containing struct. So from `struct inode *inode` you get the owning `struct ext2_inode_info *`:

From `fs/ext2/ext2.h`:

```c
static inline struct ext2_inode_info *EXT2_I(struct inode *inode)
{
	return container_of(inode, struct ext2_inode_info, vfs_inode);
}
```

Usage in ext2 (e.g. in `fs/ext2/inode.c`):

```c
void ext2_evict_inode(struct inode *inode)
{
	/* ... */
	EXT2_I(inode)->i_dtime = get_seconds();
	/* ... */
	rsv = EXT2_I(inode)->i_block_alloc_info;
	EXT2_I(inode)->i_block_alloc_info = NULL;
	/* ... */
}
```

So:

- **Inheritance**: The “base” is `struct inode`; the “subclass” is `struct ext2_inode_info` that contains it.
- **Upcast**: VFS and callbacks receive `struct inode *`; the FS uses `EXT2_I(inode)` to get the “subclass” and access FS-specific state.

Every filesystem that embeds `struct inode` does the same: define a `FOO_I(inode)` macro with `container_of(inode, struct foo_inode_info, vfs_inode)`.

### 4.3 Allocating the “subclass”: super_operations

The superblock’s `alloc_inode` decides who allocates the inode. If it’s the FS, it allocates the **larger** struct (which embeds `struct inode`) and returns a pointer to the embedded inode. From `fs/ext2/super.c`:

```c
static struct inode *ext2_alloc_inode(struct super_block *sb)
{
	struct ext2_inode_info *ei;
	ei = (struct ext2_inode_info *)kmem_cache_alloc(ext2_inode_cachep, GFP_KERNEL);
	if (!ei)
		return NULL;
	ei->i_block_alloc_info = NULL;
	ei->vfs_inode.i_version = 1;
	return &ei->vfs_inode;   /* return address of embedded inode */
}
```

So the VFS and the rest of the kernel always work with `struct inode *`; the filesystem uses `container_of` to get its private wrapper. That’s the “inheritance” pattern in VFS.

---

## 5. Inode Lifecycle (v3.2)

Understanding the lifecycle is essential for implementing or overriding inode methods and for cache behaviour.

### 5.1 Birth: allocation and I_NEW

1. **Allocation**  
   `alloc_inode(sb)` is used (from `fs/inode.c`). It either calls `sb->s_op->alloc_inode(sb)` (e.g. ext2’s `ext2_alloc_inode`) or falls back to `kmem_cache_alloc(inode_cachep, GFP_KERNEL)` for plain VFS inodes.

2. **Initialization**  
   Every inode goes through `inode_init_always(sb, inode)` (same file). That sets:
   - `inode->i_op = &empty_iops` (and similar defaults)
   - `i_sb`, `i_count`, `i_state`, locks, mapping, etc.

3. **Caching a new inode (e.g. iget_locked)**  
   For inode-number-based filesystems, `iget_locked(sb, ino)` (in `fs/inode.c`):
   - Looks up the inode in the inode hash table.
   - If not found, allocates a new inode, sets `inode->i_state = I_NEW`, hashes it, adds it to the superblock list, and returns it **locked** (I_NEW acts as a mutex).
   - The **filesystem** must fill in the inode (e.g. read from disk) and then call `unlock_new_inode(inode)`.

From `fs/inode.c`:

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

So: **I_NEW** means “inode is being initialised by the FS”; no one else should use it until I_NEW is cleared.

### 5.2 Lifecycle state bits (fs.h)

From `include/linux/fs.h` (v3.2), the main states relevant to inode methods and eviction:

- **I_NEW**: New inode; initialisation in progress; waiters sleep on this bit.
- **I_WILL_FREE**: Intent to free (e.g. before `write_inode_now` when refcount is 0).
- **I_FREEING**: Inode is being torn down (e.g. evict in progress).
- **I_CLEAR**: Set by `end_writeback()`; inode is clean and can be destroyed.

Your inode methods (e.g. `getattr`, `permission`) should not assume the inode is valid when it’s in I_FREEING / I_CLEAR; the VFS and FS try to avoid calling you in those states, but locking and ordering matter.

### 5.3 Reference counting and iput

- **iget / ihold**: Increase `i_count`; `iget_locked` (or `iget5_locked`) returns an inode with a reference.
- **iput**: Decrements `i_count`. When the count drops to zero, `iput_final()` is called (under `inode->i_lock`).

In `iput_final()` (fs/inode.c):

1. **drop_inode**  
   If `sb->s_op->drop_inode` exists, it’s called; otherwise `generic_drop_inode(inode)` is used:

   ```c
   int generic_drop_inode(struct inode *inode)
   {
       return !inode->i_nlink || inode_unhashed(inode);
   }
   ```

   So by default, the inode is “dropped” (candidate for eviction) when nlink is zero or it’s unhashed.

2. **Retain in cache**  
   If `drop` is false and the superblock is still active (`MS_ACTIVE`), the inode is marked referenced and possibly added to the LRU; it stays in cache.

3. **Eviction**  
   If the inode should be freed, the code sets `I_FREEING`, removes it from the LRU, and calls `evict(inode)`.

### 5.4 Eviction and destroy

In `evict()` (fs/inode.c):

1. Removes inode from writeback and superblock lists.
2. Calls `op->evict_inode(inode)` if present; otherwise truncates pages and calls `end_writeback(inode)`.
3. Handles block/char device forget.
4. Removes inode from hash, clears I_NEW wait bit, then calls `destroy_inode(inode)`.

`destroy_inode()`:

- Calls `__destroy_inode(inode)` (security, fsnotify, ACL release, etc.).
- If `sb->s_op->destroy_inode` is set, calls it (e.g. ext2’s RCU free of `ext2_inode_info`); otherwise frees via RCU to `inode_cachep`.

So the **lifecycle** is: **alloc → init_always → (iget_locked + FS fill + unlock_new_inode) → use (i_count ≥ 1) → iput → drop_inode decision → evict → destroy_inode**. Inode methods (e.g. `lookup`, `create`, `permission`, `getattr`) operate in the “use” phase; `evict_inode` is the FS hook for tear-down.

### 5.5 generic_delete_inode (v3.2)

From `fs/inode.c`:

```c
int generic_delete_inode(struct inode *inode)
{
	return 1;
}
EXPORT_SYMBOL(generic_delete_inode);
```

This is the default “delete” behaviour: when the inode is dropped and evicted, it’s simply freed. Filesystems that need to do more (e.g. mark on-disk inode as deleted) may use or emulate different behaviour via `drop_inode` and `evict_inode`; in 3.2 `generic_delete_inode` is a simple “yes, delete” return for the legacy path.

---

## 6. Inode Cache (v3.2)

- **Hash**: Inodes are hashed by `(super_block, inode number)` (or custom hash for `iget5_locked`). `inode_hashtable`, `inode_hash_lock`, and `inode->i_hash` are used for lookup/insert/remove.
- **Per-superblock list**: `sb->s_inodes`, `inode->i_sb_list`; protected by `inode_sb_list_lock`.
- **LRU**: Unused inodes are on `sb->s_inode_lru` (and per-CPU `nr_unused`); `inode->i_lru`; protected by `sb->s_inode_lru_lock`.

Lock ordering (from comments in fs/inode.c) must be respected when touching these lists and inode state:

- `inode_sb_list_lock` before `inode->i_lock` before `inode->i_sb->s_inode_lru_lock`
- `inode_hash_lock` and `inode->i_lock` ordering as documented

When implementing or debugging inode methods, be aware that the VFS may look up inodes from the cache without hitting the disk; your `lookup`/`getattr`/revalidate logic must be consistent with when the inode is considered valid and when it must be refreshed (e.g. NFS revalidation).

---

## 7. How This Enables Interoperability (ext3, ext4, NFS, …)

- **Single API**: All filesystems export the same `inode_operations` (and `file_operations`, `super_operations`). System calls and VFS path walk only see `struct inode *` and `inode->i_op->...`.
- **Pluggable behaviour**: Each FS sets `inode->i_op` (and `i_fop`, `i_mapping->a_ops`) when it creates or reads an inode (e.g. in `ext2_iget`, `ext2_create`, or NFS/CIFS equivalents). So the same `open()`, `stat()`, `create()` code paths work for local disk FS and network FS.
- **Optional methods**: A FS can leave many `inode_operations` as NULL and only implement what it needs; the VFS checks for NULL and often has a default (e.g. `generic_permission` when `permission` is NULL, or generic xattr helpers).

So interoperability is “same VFS layer, different method tables and different private inode data (via container_of).”

---

## 8. Practical Use: Implementing Custom Inode Methods (e.g. in a Module)

If you are adding or overriding inode methods for a kernel module (e.g. a stacked or small filesystem):

1. **Provide a `struct inode_operations`**  
   Fill only the ops you need; set the rest to NULL. Use the **v3.2** signatures (including `struct nameidata *` where required).

2. **Set `inode->i_op`**  
   Do this when the inode is created or read (e.g. in your `alloc_inode` or in your equivalent of `ext2_iget` / after `iget_locked`). For directories, often you set a different table (e.g. `dir_inode_operations`) than for regular files.

3. **If you embed `struct inode`**  
   Allocate your own struct (e.g. `my_inode_info`), embed `struct inode` in it, return `&my_inode_info->vfs_inode` from `alloc_inode`, and use `container_of(inode, struct my_inode_info, vfs_inode)` in your methods to get private data.

4. **Lifecycle**  
   If you use `iget_locked`, fill the inode and call `unlock_new_inode(inode)`. If you implement `evict_inode`, clean up your private data and then call the VFS helpers (e.g. `truncate_inode_pages`, `end_writeback`) or let the default evict path run if you don’t need special behaviour.

5. **Locks**  
   In 3.2, directory `i_mutex` is often held when `lookup`/`create` are called (as in `d_inode_lookup`). Respect the same locking rules as other FS (e.g. don’t take the same mutex recursively unless your lock class allows it).

---

## 9. v3.2-Specific Nuances and Pitfalls

- **nameidata**: Many inode ops take `struct nameidata *nd`. In 3.2 it’s still the main path-walk context (flags, intent, etc.). Don’t assume it’s NULL; use it if your method needs lookup flags or parent path.
- **Lock handling**: Inode state is protected by `inode->i_lock`; directory operations often hold `dir->i_mutex`. When implementing `create`/`mkdir`/etc., follow the same locking order as other FS (e.g. as in ext2 or namei.c) to avoid deadlocks.
- **I_NEW**: After `iget_locked`, always call `unlock_new_inode` once the inode is fully initialised. Until then, other threads will wait on I_NEW.
- **generic_delete_inode**: It simply returns 1; real “delete” semantics are in `drop_inode` and `evict_inode`. Rely on `evict_inode` for freeing FS-specific resources and on-disk teardown.
- **Older drivers**: Some legacy or out-of-tree FS might still assume 2.6-style APIs; in 3.2 the main visible difference from 2.6 is the continued use of nameidata and the current form of inode cache and eviction. Stick to 3.2 APIs when writing new code.

---

## 10. Summary

- **Polymorphism**: VFS calls `inode->i_op-><method>(...)` so the same path works for all filesystems.
- **Inheritance**: FS-specific “subclass” struct embeds `struct inode`; `container_of` recovers the subclass pointer from `struct inode *`.
- **Lifecycle**: Alloc → init_always → (iget_locked + FS init + unlock_new_inode) → use → iput → drop_inode → evict → destroy_inode. Inode methods run in the “use” phase; `evict_inode` is for tear-down.
- **Cache**: Hash + per-sb list + LRU; lock ordering and I_NEW/I_FREEING/I_CLEAR matter for correctness.
- **v3.2**: Use `struct nameidata *` in inode op signatures where defined, and follow 3.2 locking and lifecycle behaviour when implementing or modifying inode methods in kernel modules.

All code references above are to Linux kernel v3.2 source (e.g. `include/linux/fs.h`, `fs/inode.c`, `fs/namei.c`, `fs/ext2/ext2.h`, `fs/ext2/super.c`, `fs/ext2/inode.c`). For deeper single-FS behaviour, study one of ext2, ext3, or NFS in the same tree.
