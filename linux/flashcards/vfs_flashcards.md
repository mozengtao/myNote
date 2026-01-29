# Linux Kernel v3.2 VFS (Virtual File System) Flashcards

> Comprehensive Anki-style flashcards covering VFS architecture, data structures, operations tables, pathname resolution, and filesystem integration.

---

## Section 1: VFS Core Concepts

---

Q: What is the VFS (Virtual File System) in Linux?
A: VFS is an abstraction layer in the kernel that provides a uniform interface for all filesystems. It allows applications to use the same system calls (open, read, write, close) regardless of the underlying filesystem (ext4, NFS, procfs, etc.).
[Basic]

---

Q: What problem does VFS solve?
A: VFS solves the problem of filesystem diversity. Without VFS, applications would need different code paths for each filesystem type. VFS provides a common API, allowing "write once, work with any filesystem."
[Basic]

---

Q: (Understanding) Why is VFS called a "virtual" filesystem?
A: Because VFS itself doesn't store any data on disk. It's a software abstraction layer that defines interfaces and dispatches operations to concrete filesystem implementations. The "virtual" aspect is the uniform view it presents to userspace.
[Basic]

---

Q: What is the operations table polymorphism pattern in VFS?
A: VFS defines interfaces as structs of function pointers (e.g., `file_operations`, `inode_operations`). Each filesystem provides its own implementation by filling in these function pointers. This achieves runtime polymorphism in C.
[Basic]

---

Q: (ASCII Diagram) Show the VFS abstraction layer architecture.
A:
```
User Space
─────────────────────────────────────────────────────
    open()      read()      write()     close()
        │          │           │           │
        └──────────┴───────────┴───────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────┐
│                     VFS                          │
│  ┌─────────────────────────────────────────┐    │
│  │ Uniform Interface (file_operations, etc.)│    │
│  └─────────────────────────────────────────┘    │
│         │              │              │          │
│         ▼              ▼              ▼          │
│    ┌────────┐    ┌────────┐    ┌────────┐       │
│    │  ext4  │    │  NFS   │    │ procfs │       │
│    └────────┘    └────────┘    └────────┘       │
└─────────────────────────────────────────────────┘
─────────────────────────────────────────────────────
Kernel Space
```
[Basic]

---

Q: What are the four primary VFS objects?
A:
1. **Superblock** - Represents a mounted filesystem instance
2. **Inode** - Represents a file's metadata (not its name)
3. **Dentry** - Represents a directory entry (name → inode mapping)
4. **File** - Represents an open file (per-process state)
[Basic]

---

Q: (Cloze) The VFS object that represents a mounted filesystem is the _____, while the object representing file metadata is the _____.
A: **superblock**, **inode**. The superblock contains filesystem-wide information, while each file has an inode containing its metadata (size, permissions, timestamps).
[Basic]

---

Q: What is the relationship between inodes and dentries?
A: An inode contains file metadata but not the filename. A dentry (directory entry) maps a name to an inode. Multiple dentries can point to the same inode (hard links). One inode can have many names, but each name (dentry) points to exactly one inode.
[Basic]

---

Q: (Understanding) Why does VFS separate dentries from inodes?
A: Separation enables:
1. Hard links - multiple names for one file (multiple dentries → one inode)
2. Efficient caching - dentry cache (dcache) speeds up pathname lookup
3. Filesystem independence - some filesystems (like FAT) don't have native inodes
[Intermediate]

---

Q: What is the difference between an inode and a file object in VFS?
A: An **inode** represents the file itself (metadata, exists as long as file exists). A **file** object represents an open instance of a file (process-specific state like position, flags). Multiple file objects can reference the same inode.
[Basic]

---

Q: (ASCII Diagram) Show how multiple processes can open the same file.
A:
```
Process A                    Process B
fd_table[3]                  fd_table[5]
    │                            │
    ▼                            ▼
┌──────────────┐          ┌──────────────┐
│ struct file  │          │ struct file  │
│ f_pos = 100  │          │ f_pos = 0    │
│ f_flags=RDWR │          │ f_flags=RDONLY│
└──────┬───────┘          └──────┬───────┘
       │                         │
       └───────────┬─────────────┘
                   │
                   ▼
            ┌─────────────┐
            │ struct inode│
            │ i_size=4096 │
            │ i_mode=0644 │
            └─────────────┘

Two file objects, one inode
Different positions, different flags
```
[Intermediate]

---

Q: Where are the main VFS data structures defined?
A: Primary header files:
- `include/linux/fs.h` - inode, file, super_block, file_operations, inode_operations
- `include/linux/dcache.h` - dentry, dentry_operations
- `include/linux/mount.h` - vfsmount
[Basic]

---

Q: Where is the VFS implementation code located?
A: Key source files in `fs/` directory:
- `fs/namei.c` - pathname resolution
- `fs/open.c` - open() system call
- `fs/read_write.c` - read/write system calls
- `fs/dcache.c` - dentry cache
- `fs/inode.c` - inode management
- `fs/super.c` - superblock handling
[Intermediate]

---

Q: What is the "everything is a file" philosophy and how does VFS support it?
A: Unix treats devices, pipes, sockets, and even kernel data as files. VFS supports this by allowing any kernel subsystem to implement file_operations. procfs exposes kernel data, sysfs exposes devices, all through the file interface.
[Basic]

---

Q: (Understanding) How does VFS achieve filesystem independence?
A: VFS defines abstract interfaces (function pointer tables). Each filesystem:
1. Registers with VFS via `register_filesystem()`
2. Provides implementations for operations (read, write, lookup)
3. VFS dispatches calls to the appropriate implementation
The application never knows which filesystem it's using.
[Intermediate]

---

Q: What happens when you call open("/home/user/file.txt")?
A: VFS performs:
1. **Pathname resolution** - Walk each component (home → user → file.txt)
2. **Dentry lookup** - Check dcache, call filesystem's lookup() if miss
3. **Permission check** - Verify access rights
4. **File allocation** - Create struct file, set up f_op
5. **FD allocation** - Assign file descriptor, return to userspace
[Intermediate]

---

Q: What is the dcache (dentry cache)?
A: A kernel cache that stores recently used dentries (name → inode mappings). It dramatically speeds up pathname resolution by avoiding disk reads for commonly accessed paths. The dcache is one of the most performance-critical VFS components.
[Basic]

---

Q: (Understanding) Why is the dcache so important for performance?
A: Every file operation starts with pathname resolution. Without dcache, each path component would require disk I/O. The dcache keeps hot dentries in memory, making common operations like `ls`, `cd`, and file access nearly instantaneous.
[Intermediate]

---

Q: What is a negative dentry?
A: A dentry that caches the fact that a file does NOT exist. When lookup fails, VFS creates a negative dentry. Future lookups for the same non-existent path return immediately without hitting the filesystem. Speeds up repeated failed lookups.
[Intermediate]

---

Q: (Cloze) VFS uses _____ for runtime polymorphism, where each filesystem fills in function pointers like _____ and _____.
A: **structs of function pointers** (operations tables), **read**, **write** (or lookup, create, etc.). This is C's equivalent of virtual method tables in object-oriented languages.
[Basic]

---

Q: What are the main operations table types in VFS?
A:
- `file_operations` - Per-file ops (read, write, mmap, fsync)
- `inode_operations` - Per-inode ops (lookup, create, unlink, mkdir)
- `super_operations` - Per-superblock ops (alloc_inode, sync_fs)
- `dentry_operations` - Per-dentry ops (d_revalidate, d_hash)
- `address_space_operations` - Page cache ops (readpage, writepage)
[Intermediate]

---

Q: (Reverse) This VFS component caches name-to-inode mappings to speed up pathname resolution.
A: Q: What is the dentry cache (dcache)?
[Basic]

---

Q: How does VFS handle different filesystem semantics?
A: Through optional callbacks. If a filesystem doesn't support an operation (e.g., symbolic links on FAT), it sets that function pointer to NULL or a generic error function. VFS checks and handles appropriately.
[Intermediate]

---

Q: (Understanding) What is the tradeoff of VFS abstraction?
A: **Benefits**: Uniform API, code reuse, easy to add new filesystems
**Costs**: Indirection overhead (function pointer calls can't be inlined), must support lowest common denominator features, complexity in handling filesystem-specific semantics
[Intermediate]

---

Q: What is the page cache and how does it relate to VFS?
A: The page cache stores file contents in memory. VFS integrates with it via `address_space_operations`. Most filesystems use the page cache for buffered I/O - reads check cache first, writes go to cache and are flushed later.
[Intermediate]

---

Q: (Understanding) Why does VFS exist as a separate layer rather than being built into each filesystem?
A: Code reuse and maintainability. Common functionality (pathname resolution, dcache, page cache integration, permission checking) is implemented once in VFS. Filesystems only implement storage-specific operations, reducing code duplication.
[Intermediate]

---

## Section 2: Core Data Structures

---

Q: What is `struct inode` in VFS?
A: The in-memory representation of a file's metadata. Contains file type, permissions, owner, size, timestamps, link count, and pointers to operations tables. Defined in `include/linux/fs.h`. One inode exists per file (not per open).
[Basic]

---

Q: What are the key fields in `struct inode`?
A: Identity and metadata fields:
- `i_mode` - File type and permissions (umode_t)
- `i_uid`, `i_gid` - Owner user and group ID
- `i_ino` - Inode number (unique within filesystem)
- `i_size` - File size in bytes
- `i_blocks` - Number of blocks allocated
- `i_atime`, `i_mtime`, `i_ctime` - Access, modify, change times
[Basic]

---

Q: What VFS linkage fields does `struct inode` contain?
A: Key linkage fields:
- `i_sb` - Pointer to owning superblock
- `i_mapping` - Associated address_space (page cache)
- `i_op` - Inode operations table pointer
- `i_fop` - Default file operations table pointer
[Intermediate]

---

Q: What is the difference between `i_op` and `i_fop` in struct inode?
A: `i_op` (inode_operations) handles inode-level operations like lookup, create, unlink, mkdir. `i_fop` (file_operations) provides default file operations (read, write) that are copied to struct file on open.
[Intermediate]

---

Q: (Cloze) In struct inode, the field _____ contains the file type and permissions, while _____ holds the file size in bytes.
A: `i_mode`, `i_size`. The `i_mode` field uses macros like `S_ISREG()`, `S_ISDIR()` to check file type.
[Basic]

---

Q: What is `i_nlink` in struct inode?
A: The hard link count - number of directory entries pointing to this inode. When `i_nlink` reaches 0 and no processes have the file open, the inode and its data can be freed.
[Basic]

---

Q: What is `i_count` in struct inode?
A: The in-memory reference count - number of kernel references to this inode structure. Different from `i_nlink` (disk links). When `i_count` reaches 0, the inode can be evicted from memory (but may still exist on disk).
[Intermediate]

---

Q: (Understanding) What's the difference between `i_nlink` and `i_count`?
A: `i_nlink` = disk references (hard links). Controls when file data is deleted.
`i_count` = memory references. Controls when inode struct is freed from RAM.
A file with i_nlink=0 but i_count>0 is "unlinked but still open" - data exists until last close.
[Intermediate]

---

Q: (ASCII Diagram) Show struct inode key fields layout.
A:
```
struct inode
+----------------------------------+
| Identity                         |
|   i_mode    (type + permissions) |
|   i_uid     (owner user)         |
|   i_gid     (owner group)        |
|   i_ino     (inode number)       |
+----------------------------------+
| Size/Allocation                  |
|   i_size    (bytes)              |
|   i_blocks  (blocks)             |
+----------------------------------+
| Timestamps                       |
|   i_atime   (access time)        |
|   i_mtime   (modify time)        |
|   i_ctime   (change time)        |
+----------------------------------+
| References                       |
|   i_count   (memory refs)        |
|   i_nlink   (hard links)         |
+----------------------------------+
| VFS Linkage                      |
|   i_sb      → superblock         |
|   i_mapping → address_space      |
+----------------------------------+
| Operations (polymorphism)        |
|   i_op      → inode_operations   |
|   i_fop     → file_operations    |
+----------------------------------+
| Locking                          |
|   i_mutex   (directory ops)      |
+----------------------------------+
| FS-specific                      |
|   i_private (void *)             |
+----------------------------------+
```
[Intermediate]

---

Q: What is `struct file` in VFS?
A: Represents an open file instance. Created when a process opens a file, destroyed on close. Contains per-open state: current position, access flags, credentials at open time. Multiple struct file objects can reference the same inode.
[Basic]

---

Q: What are the key fields in `struct file`?
A:
- `f_path` - Path (dentry + vfsmount)
- `f_inode` - Cached inode pointer
- `f_op` - File operations table
- `f_flags` - Open flags (O_RDONLY, O_APPEND, etc.)
- `f_mode` - Access mode (FMODE_READ, FMODE_WRITE)
- `f_pos` - Current file position
- `f_count` - Reference count
[Basic]

---

Q: What is `f_pos` in struct file?
A: The current read/write position (file offset). Each open file has its own position. Updated by read/write, can be set by lseek(). This is why two processes opening the same file have independent positions.
[Basic]

---

Q: What is `f_path` in struct file?
A: A `struct path` containing pointers to both the dentry and vfsmount. Provides the full path context - which directory entry and which mount point this file belongs to.
[Intermediate]

---

Q: (Code Interpretation) What does this code show about struct file?
```c
struct file *filp = fget(fd);
loff_t pos = filp->f_pos;
if (filp->f_mode & FMODE_READ)
    bytes = filp->f_op->read(filp, buf, count, &pos);
filp->f_pos = pos;
fput(filp);
```
A: Shows file operations pattern:
1. Get file struct from fd (`fget`)
2. Get current position from `f_pos`
3. Check read permission in `f_mode`
4. Call filesystem's read via `f_op` function pointer
5. Update position, release reference (`fput`)
[Intermediate]

---

Q: What is `f_count` in struct file used for?
A: Reference counting for the file structure. Incremented when file is shared (dup, fork, passed via socket). Decremented on close. When `f_count` reaches 0, the file structure is freed and `release()` is called.
[Intermediate]

---

Q: (Understanding) Why does struct file have both `f_flags` and `f_mode`?
A: `f_flags` stores the original open() flags (O_RDONLY, O_APPEND, O_NONBLOCK - may be changed by fcntl). `f_mode` is the resolved access mode (FMODE_READ, FMODE_WRITE) - simpler for permission checks. They're related but serve different purposes.
[Intermediate]

---

Q: What is `struct dentry` in VFS?
A: Directory entry - represents a name in the filesystem. Maps a filename component to an inode. Cached in the dcache for fast pathname lookup. Contains the name, pointer to inode, pointer to parent dentry.
[Basic]

---

Q: What are the key fields in `struct dentry`?
A:
- `d_name` - The name (struct qstr: hash + len + name string)
- `d_inode` - Pointer to inode (NULL for negative dentry)
- `d_parent` - Pointer to parent dentry
- `d_op` - Dentry operations table
- `d_sb` - Superblock pointer
- `d_flags` - Dentry flags
- `d_lockref` - Lock and reference count combined
[Intermediate]

---

Q: What is `struct qstr` used in dentries?
A: Quick string - an optimized string representation with:
- `hash` - Precomputed hash value for fast comparison
- `len` - String length
- `name` - Pointer to the actual name string
Used for efficient dcache lookups.
[Intermediate]

---

Q: (Cloze) A dentry with `d_inode == NULL` is called a _____ dentry and caches the fact that a file _____.
A: **negative**, **does not exist**. Negative dentries speed up repeated lookups for non-existent files.
[Basic]

---

Q: What is `d_parent` in struct dentry?
A: Pointer to the parent directory's dentry. Forms a tree structure. The root dentry has `d_parent` pointing to itself. Used for pathname construction and directory traversal.
[Basic]

---

Q: (ASCII Diagram) Show dentry tree structure for /home/user/file.txt.
A:
```
Dentry Tree for "/home/user/file.txt"

     d_parent
         ▲
┌────────┴────────┐
│  dentry: "/"    │ ◄─── d_parent points to self
│  d_inode: inode1│
└────────┬────────┘
         │ d_subdirs
         ▼
┌────────────────┐
│ dentry: "home" │
│ d_inode: inode2│
│ d_parent: "/"  │
└────────┬───────┘
         │
         ▼
┌────────────────┐
│ dentry: "user" │
│ d_inode: inode3│
│ d_parent:"home"│
└────────┬───────┘
         │
         ▼
┌──────────────────┐
│dentry: "file.txt"│
│ d_inode: inode4  │
│ d_parent: "user" │
└──────────────────┘
```
[Intermediate]

---

Q: What is `struct super_block` in VFS?
A: Represents a mounted filesystem instance. Contains filesystem-wide information: block size, mount flags, root dentry, operations table. One superblock per mount (same filesystem mounted twice = two superblocks).
[Basic]

---

Q: What are the key fields in `struct super_block`?
A:
- `s_dev` - Device identifier
- `s_blocksize` - Block size in bytes
- `s_type` - Filesystem type pointer
- `s_op` - Super operations table
- `s_root` - Root dentry of this mount
- `s_flags` - Mount flags (MS_RDONLY, MS_NOSUID)
- `s_fs_info` - Filesystem-specific data
- `s_inodes` - List of all inodes on this filesystem
[Intermediate]

---

Q: What is `s_root` in struct super_block?
A: Pointer to the root dentry of the mounted filesystem. The starting point for pathname resolution within this mount. For the root filesystem, this is the "/" dentry.
[Basic]

---

Q: What is `s_fs_info` in struct super_block?
A: A void pointer for filesystem-specific data. Each filesystem stores its private superblock information here. For ext4, it points to `struct ext4_sb_info`. Allows VFS to be filesystem-agnostic.
[Intermediate]

---

Q: (Code Interpretation) What does this code do?
```c
struct super_block *sb = inode->i_sb;
if (sb->s_op->write_inode)
    sb->s_op->write_inode(inode, wbc);
```
A: Writes an inode to disk using the filesystem's implementation:
1. Get superblock from inode
2. Check if filesystem implements `write_inode`
3. Call filesystem-specific write_inode through operations table
This is the polymorphism pattern in action.
[Intermediate]

---

Q: What is `struct vfsmount`?
A: Represents a mount instance - where a filesystem is mounted in the directory tree. Contains the mount point, the mounted filesystem's superblock, mount flags. Used for mount namespace and mount point traversal.
[Intermediate]

---

Q: What are the key fields in `struct vfsmount` (v3.2)?
A:
- `mnt_root` - Dentry of root of mounted filesystem
- `mnt_sb` - Superblock of mounted filesystem
- `mnt_parent` - Parent mount (where we're mounted)
- `mnt_mountpoint` - Dentry of mount point in parent
- `mnt_flags` - Per-mount flags
[Intermediate]

---

Q: (Understanding) What's the difference between superblock and vfsmount?
A: **Superblock** represents the filesystem itself (ext4 data).
**Vfsmount** represents where it's mounted (location in namespace).
Same filesystem mounted twice = one or two superblocks (depends on device), two vfsmounts.
[Intermediate]

---

Q: What is `struct path` in VFS?
A: A simple structure combining a dentry and vfsmount:
```c
struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};
```
Represents a fully resolved filesystem location - both the name and which mount it's on.
[Intermediate]

---

Q: (ASCII Diagram) Show relationships between VFS core structures.
A:
```
                        fd (int)
                           │
                           ▼
┌─────────────────────────────────────────────┐
│              struct file                     │
│  f_path ─────────────────────────┐          │
│  f_op ──────────────────────┐    │          │
│  f_pos, f_flags             │    │          │
└─────────────────────────────┼────┼──────────┘
                              │    │
        ┌─────────────────────┘    │
        ▼                          ▼
┌──────────────────┐    ┌─────────────────────┐
│ file_operations  │    │    struct path       │
│  .read           │    │  mnt ────┬──────────│───► vfsmount
│  .write          │    │  dentry ─┼──────────│───► dentry
│  .open           │    └──────────┼──────────┘        │
└──────────────────┘               │                   │
                                   │                   ▼
                                   │            ┌────────────┐
                                   │            │   dentry   │
                                   │            │  d_inode ──┼──► inode
                                   │            │  d_name    │       │
                                   ▼            │  d_parent  │       │
                            ┌───────────┐       └────────────┘       │
                            │ vfsmount  │                            │
                            │ mnt_sb ───┼──► super_block ◄───────────┘
                            │ mnt_root  │       │                i_sb
                            └───────────┘       │
                                                ▼
                                         ┌────────────────┐
                                         │ super_operations│
                                         │  .alloc_inode   │
                                         │  .write_inode   │
                                         └────────────────┘
```
[Advanced]

---

Q: What is `struct address_space` in VFS?
A: Manages the page cache for a file. Contains the radix tree of cached pages, operations for reading/writing pages, and the host inode pointer. Each inode has an embedded or associated address_space.
[Intermediate]

---

Q: What are key fields in `struct address_space`?
A:
- `host` - Owning inode
- `page_tree` - Radix tree of cached pages
- `nrpages` - Number of cached pages
- `a_ops` - Address space operations (readpage, writepage)
- `backing_dev_info` - Info about backing device
[Intermediate]

---

Q: What is `i_mapping` vs `i_data` in struct inode?
A: `i_data` is an embedded `struct address_space` within the inode.
`i_mapping` points to the address_space to use (usually `&i_data`).
For some cases (like block devices), `i_mapping` can point elsewhere to share page cache.
[Advanced]

---

Q: (Reverse) This structure contains f_pos (current position) and f_flags (open flags) and is created on open().
A: Q: What is `struct file`?
[Basic]

---

Q: (Reverse) This structure contains d_name, d_inode, and d_parent, and is cached in the dcache.
A: Q: What is `struct dentry`?
[Basic]

---

Q: How are inodes allocated?
A: The superblock's `s_op->alloc_inode()` callback allocates inodes. Most filesystems embed the VFS inode in a larger filesystem-specific structure and use `kmem_cache_alloc()` from a dedicated slab cache.
[Intermediate]

---

Q: (Code Interpretation) What pattern does this show?
```c
struct ext4_inode_info {
    /* ext4-specific fields */
    __le32 i_data[15];
    __u32  i_flags;
    /* ... */
    struct inode vfs_inode;  /* VFS inode embedded at end */
};
```
A: Container pattern - filesystem-specific inode info contains the VFS inode. Use `container_of()` to get ext4_inode_info from vfs_inode pointer. Allows VFS to work with generic inode while filesystem accesses extended data.
[Advanced]

---

Q: What is `container_of()` and how is it used with VFS structures?
A: A macro that finds the containing structure given a member pointer:
```c
#define container_of(ptr, type, member) ...
struct ext4_inode_info *ei = container_of(inode, struct ext4_inode_info, vfs_inode);
```
Filesystems use this to go from VFS inode to their private inode structure.
[Intermediate]

---

Q: What is the inode cache (icache)?
A: A cache of in-memory inodes, similar to dcache for dentries. Recently used inodes are kept in memory to avoid reading from disk. Managed via LRU lists and memory pressure callbacks.
[Intermediate]

---

Q: How does VFS know when to free an inode from memory?
A: When `i_count` (memory references) drops to 0, the inode is placed on an LRU list. Under memory pressure, `s_op->drop_inode()` is called. If `i_nlink` is also 0, the inode and data are deleted; otherwise just evicted from memory.
[Advanced]

---

## Section 3: Operations Tables

---

Q: What is `struct file_operations` in VFS?
A: The operations table for open files. Contains function pointers for file operations like read, write, llseek, mmap, open, release, fsync. Each filesystem provides its implementation. Pointed to by `file->f_op`.
[Basic]

---

Q: What are the key callbacks in `struct file_operations`?
A:
- `llseek` - Change file position
- `read` / `write` - Synchronous I/O
- `aio_read` / `aio_write` - Async I/O (v3.2)
- `open` - Called when file is opened
- `release` - Called when last reference closed
- `mmap` - Memory map the file
- `fsync` - Flush to disk
- `poll` - Check for I/O readiness
- `unlocked_ioctl` - Device control
[Intermediate]

---

Q: (Code Interpretation) What does this file_operations definition tell us?
```c
const struct file_operations ext4_file_operations = {
    .llseek     = ext4_llseek,
    .read       = do_sync_read,
    .write      = do_sync_write,
    .aio_read   = generic_file_aio_read,
    .aio_write  = ext4_file_write,
    .open       = ext4_file_open,
    .release    = ext4_release_file,
    .fsync      = ext4_sync_file,
    .mmap       = ext4_file_mmap,
};
```
A: ext4 file operations showing:
- Custom llseek, open, release, fsync, mmap (ext4-specific)
- Generic sync wrappers for read/write that call aio versions
- Generic page-cache-based aio_read
- Custom aio_write (ext4 needs journal handling)
[Intermediate]

---

Q: What is the difference between `read` and `aio_read` in file_operations?
A: `read` is the older synchronous interface (direct buffer, position pointer). `aio_read` uses kiocb (kernel I/O control block) for async support. In v3.2, `do_sync_read()` wraps `aio_read` for backward compatibility. Most filesystems implement `aio_read`.
[Intermediate]

---

Q: What is `release` in file_operations?
A: Called when the last reference to a file is closed. NOT called on every close() - only when `f_count` drops to 0. Used for cleanup like flushing buffers, freeing resources. The counterpart to `open`.
[Intermediate]

---

Q: (Understanding) Why is there both `open` and `release` but not `close`?
A: Because of reference counting. Multiple processes can share a file (fork, dup). `open` is called once per open() syscall. `close()` decrements refcount. `release` is called only when refcount hits 0 - the true "last close."
[Intermediate]

---

Q: What is `fsync` in file_operations?
A: Flushes file data and metadata to persistent storage. Called by fsync() syscall. Must ensure all dirty data is written to disk before returning. Different from page cache writeout - fsync guarantees durability.
[Basic]

---

Q: What is `struct inode_operations` in VFS?
A: Operations table for inode (file metadata) manipulation. Contains function pointers for creating, removing, and looking up files in directories. Pointed to by `inode->i_op`. Different operations for files vs directories.
[Basic]

---

Q: What are key callbacks in `struct inode_operations` for directories?
A:
- `lookup` - Find a file in directory
- `create` - Create a regular file
- `link` - Create hard link
- `unlink` - Remove file
- `symlink` - Create symbolic link
- `mkdir` - Create directory
- `rmdir` - Remove directory
- `rename` - Rename/move file
- `mknod` - Create special file
[Intermediate]

---

Q: What does `lookup` in inode_operations do?
A: Resolves a filename to its inode within a directory. Called during pathname resolution when dcache misses. Returns a dentry (found or negative). This is the key callback for pathname resolution.
[Basic]

---

Q: (Code Interpretation) What does this lookup implementation show?
```c
static struct dentry *ext4_lookup(struct inode *dir, struct dentry *dentry,
                                   unsigned int flags)
{
    struct inode *inode;
    ino_t ino = ext4_inode_by_name(dir, &dentry->d_name);
    if (ino) {
        inode = ext4_iget(dir->i_sb, ino);
        return d_splice_alias(inode, dentry);
    }
    return NULL;  /* negative dentry */
}
```
A: ext4's lookup:
1. Search directory for name, get inode number
2. If found, read inode from disk (`ext4_iget`)
3. Splice inode into dentry (`d_splice_alias`)
4. If not found, return NULL (creates negative dentry)
[Advanced]

---

Q: What is `create` in inode_operations?
A: Creates a new regular file in a directory. Called when O_CREAT is used with open(). Must allocate inode, initialize it, add directory entry, and link dentry to inode. Returns error code.
[Intermediate]

---

Q: What is `unlink` in inode_operations?
A: Removes a file's directory entry (hard link). Decrements `i_nlink`. If `i_nlink` becomes 0 and no processes have file open, the inode and data are freed. Directory must be updated to remove entry.
[Basic]

---

Q: (Understanding) What's the difference between `unlink` and file deletion?
A: `unlink` removes ONE hard link (directory entry). File data is only deleted when:
1. `i_nlink` = 0 (no more links)
2. `i_count` = 0 (no open file handles)
This is why you can delete a file while a process has it open.
[Intermediate]

---

Q: What is `struct super_operations` in VFS?
A: Operations table for filesystem-wide operations. Contains callbacks for inode allocation, writing inodes to disk, syncing filesystem. Pointed to by `super_block->s_op`.
[Basic]

---

Q: What are key callbacks in `struct super_operations`?
A:
- `alloc_inode` - Allocate new inode structure
- `destroy_inode` - Free inode structure
- `dirty_inode` - Mark inode as needing writeback
- `write_inode` - Write inode to disk
- `drop_inode` - Called when inode refcount hits 0
- `evict_inode` - Remove inode from memory
- `sync_fs` - Sync entire filesystem
- `statfs` - Get filesystem statistics
[Intermediate]

---

Q: What is `alloc_inode` in super_operations?
A: Allocates a new inode structure. Filesystems typically allocate their extended inode structure (with VFS inode embedded) from a dedicated slab cache. Returns pointer to VFS inode portion.
[Intermediate]

---

Q: (Code Interpretation) What does this alloc_inode show?
```c
static struct inode *ext4_alloc_inode(struct super_block *sb)
{
    struct ext4_inode_info *ei;
    ei = kmem_cache_alloc(ext4_inode_cachep, GFP_NOFS);
    if (!ei)
        return NULL;
    /* initialize ext4-specific fields */
    return &ei->vfs_inode;
}
```
A: ext4's inode allocation:
1. Allocate from dedicated slab cache (`ext4_inode_cachep`)
2. Use GFP_NOFS to avoid recursion into filesystem
3. Initialize ext4-specific fields
4. Return pointer to embedded VFS inode
[Intermediate]

---

Q: What is `write_inode` in super_operations?
A: Writes inode metadata to persistent storage. Called during sync or when inode is evicted under memory pressure. Must write i_mode, i_size, timestamps, etc. to disk structures.
[Intermediate]

---

Q: What is `sync_fs` in super_operations?
A: Synchronizes entire filesystem to disk. Called by sync() syscall. Must flush all dirty inodes, dirty pages, and any filesystem-specific structures (like journal in ext4). Ensures durability.
[Intermediate]

---

Q: What is `struct dentry_operations` in VFS?
A: Operations table for dentry manipulation. Contains callbacks for dentry validation, hashing, and comparison. Pointed to by `dentry->d_op`. Most filesystems don't need custom dentry operations.
[Intermediate]

---

Q: What are key callbacks in `struct dentry_operations`?
A:
- `d_revalidate` - Check if cached dentry is still valid
- `d_hash` - Custom hash function for dcache
- `d_compare` - Custom name comparison
- `d_delete` - Called when dentry refcount hits 0
- `d_release` - Free dentry resources
- `d_iput` - Called when dentry loses its inode
[Intermediate]

---

Q: What is `d_revalidate` in dentry_operations?
A: Checks if a cached dentry is still valid. Critical for network filesystems (NFS) where the server may have changed. Called before using a cached dentry. Returns 1 if valid, 0 if invalid (triggers new lookup).
[Intermediate]

---

Q: (Understanding) Why do network filesystems need d_revalidate?
A: On local filesystems, dcache is authoritative. On NFS, another client may have deleted or modified a file. `d_revalidate` lets NFS check with the server (via GETATTR) that the cached dentry is still accurate.
[Intermediate]

---

Q: What is `d_hash` in dentry_operations used for?
A: Custom hash function for dcache lookup. Default is case-sensitive. Case-insensitive filesystems (like CIFS) provide custom d_hash to hash lowercase. Must be paired with custom `d_compare`.
[Advanced]

---

Q: What is `struct address_space_operations` in VFS?
A: Operations table for page cache management. Contains callbacks for reading/writing pages, preparing/committing writes. Pointed to by `address_space->a_ops`. Central to VFS I/O.
[Intermediate]

---

Q: What are key callbacks in `struct address_space_operations`?
A:
- `readpage` - Read a page from storage
- `writepage` - Write a single page
- `writepages` - Write multiple pages (more efficient)
- `write_begin` - Prepare for write (allocate blocks)
- `write_end` - Finish write (mark dirty)
- `direct_IO` - Bypass page cache
- `releasepage` - Release page cache page
[Intermediate]

---

Q: What is `readpage` in address_space_operations?
A: Reads a page from storage into memory. Called on page cache miss. Must read data from disk/network and fill the page. Sets page Uptodate when complete. Core of buffered read path.
[Intermediate]

---

Q: (Code Interpretation) What is the typical readpage flow?
```c
static int ext4_readpage(struct file *file, struct page *page)
{
    return mpage_readpage(page, ext4_get_block);
}
```
A: ext4's readpage delegates to `mpage_readpage`:
1. `ext4_get_block` maps logical block to physical
2. `mpage_readpage` submits bio for the block(s)
3. Page marked Uptodate when I/O completes
Most filesystems use this pattern with their get_block function.
[Advanced]

---

Q: What are `write_begin` and `write_end` in address_space_operations?
A: Two-phase write interface:
- `write_begin` - Prepare for write: lock page, allocate blocks, read partial page if needed
- `write_end` - Complete write: mark page dirty, unlock, update inode size
Data copied between the two calls.
[Intermediate]

---

Q: (Code Interpretation) How does buffered write use write_begin/write_end?
```c
a_ops->write_begin(file, mapping, pos, len, flags, &page, &fsdata);
copied = copy_from_user(kaddr + offset, buf, len);
a_ops->write_end(file, mapping, pos, len, copied, page, fsdata);
```
A:
1. `write_begin` prepares: gets page, may allocate disk blocks
2. `copy_from_user` copies data to page cache
3. `write_end` finishes: marks dirty, updates size, unlocks
The page is now dirty in cache, not yet on disk.
[Intermediate]

---

Q: What is `writepage` in address_space_operations?
A: Writes a single dirty page to storage. Called during writeback or memory pressure. Must submit I/O to write page contents to disk. Page cleared dirty when I/O completes.
[Intermediate]

---

Q: What is `writepages` vs `writepage`?
A: `writepage` writes one page at a time (inefficient for sequential data). `writepages` writes multiple dirty pages, allowing I/O coalescing. If `writepages` is NULL, VFS falls back to calling `writepage` repeatedly.
[Intermediate]

---

Q: What is `direct_IO` in address_space_operations?
A: Bypasses page cache - reads/writes directly between user buffer and storage. Used with O_DIRECT flag. Must handle alignment requirements. More efficient for large sequential I/O, worse for random/small I/O.
[Intermediate]

---

Q: (Reverse) This operations table callback checks if a cached dentry is still valid, crucial for NFS.
A: Q: What is `d_revalidate` in dentry_operations?
[Intermediate]

---

Q: (Reverse) This operations table contains readpage, writepage, and write_begin/write_end callbacks.
A: Q: What is `struct address_space_operations`?
[Intermediate]

---

Q: (ASCII Diagram) Show how operations tables enable filesystem polymorphism.
A:
```
VFS Layer (generic code)
═════════════════════════════════════════════════════════
                    │
    vfs_read(file, ...)
                    │
                    ▼
            file->f_op->read()
                    │
        ┌───────────┼───────────┐
        │           │           │
        ▼           ▼           ▼
┌─────────────┐ ┌─────────┐ ┌──────────┐
│ext4_file_ops│ │nfs_f_ops│ │proc_f_ops│
│ .read=...   │ │ .read=..│ │ .read=.. │
└──────┬──────┘ └────┬────┘ └────┬─────┘
       │             │           │
       ▼             ▼           ▼
   Read from     RPC to      Generate
   disk/cache    server      on-the-fly
═════════════════════════════════════════════════════════
Filesystem-specific implementations
```
[Intermediate]

---

Q: Why are operations tables declared as `const`?
A: Security and safety. Operations tables are typically static read-only data. `const` prevents accidental modification and allows the tables to be placed in read-only memory, preventing kernel exploits from redirecting function pointers.
[Intermediate]

---

Q: How does a filesystem set up its operations tables?
A: During inode initialization:
1. `alloc_inode` allocates the inode
2. Filesystem sets `inode->i_op` and `inode->i_fop` based on file type
3. For directories: use directory operations
4. For regular files: use file operations
5. These are copied to `file->f_op` on open
[Intermediate]

---

Q: (Code Interpretation) How does ext4 set operations based on file type?
```c
void ext4_set_inode_ops(struct inode *inode)
{
    if (S_ISREG(inode->i_mode)) {
        inode->i_op = &ext4_file_inode_operations;
        inode->i_fop = &ext4_file_operations;
    } else if (S_ISDIR(inode->i_mode)) {
        inode->i_op = &ext4_dir_inode_operations;
        inode->i_fop = &ext4_dir_operations;
    } else if (S_ISLNK(inode->i_mode)) {
        inode->i_op = &ext4_symlink_inode_operations;
    }
}
```
A: Different operations for different file types:
- Regular files: file read/write operations
- Directories: lookup/create/unlink operations
- Symlinks: readlink/follow_link operations
Operations tables are chosen at inode initialization.
[Advanced]

---

## Section 4: Pathname Resolution and Lookup

---

Q: What is pathname resolution (namei)?
A: The process of converting a pathname string (like "/home/user/file.txt") into the corresponding dentry and inode. VFS walks through each component, looking up dentries and crossing mount points. Implemented in `fs/namei.c`.
[Basic]

---

Q: What is the main entry point for pathname resolution?
A: `path_openat()` for open(), `user_path_at()` for stat-like calls. These set up a `struct nameidata` and call `link_path_walk()` to process each path component. Defined in `fs/namei.c`.
[Intermediate]

---

Q: What is `struct nameidata`?
A: Holds the current state during pathname resolution:
- `path` - Current resolved path (dentry + mnt)
- `last` - Last component name (for final lookups)
- `root` - Root directory for this lookup
- `inode` - Current inode
- `flags` - Lookup flags (LOOKUP_FOLLOW, etc.)
- `depth` - Symlink recursion depth
[Intermediate]

---

Q: (ASCII Diagram) Show pathname resolution for "/home/user/file.txt".
A:
```
Path: "/home/user/file.txt"

Start at root "/"
       │
       ▼ lookup "home"
┌──────────────────┐
│ Check dcache     │ ──hit──► dentry for "home"
│ for "home"       │            │
└────────┬─────────┘            │
         │ miss                 │
         ▼                      │
┌──────────────────┐            │
│ i_op->lookup()   │            │
│ (filesystem call)│            │
└────────┬─────────┘            │
         │                      │
         └──────────────────────┘
                    │
                    ▼ lookup "user"
                   ...
                    │
                    ▼ lookup "file.txt"
               ┌────────────┐
               │ Final      │
               │ dentry +   │
               │ inode      │
               └────────────┘
```
[Intermediate]

---

Q: What is `link_path_walk()`?
A: The core pathname resolution function. Loops through each component of the path, performing dcache lookups and calling filesystem lookup on misses. Handles "." and "..", mount point crossing, and symlink following.
[Intermediate]

---

Q: What is the difference between `lookup_fast()` and `lookup_slow()`?
A: `lookup_fast()` - Tries dcache lookup using RCU (lockless). Very fast if cached.
`lookup_slow()` - Takes locks, calls filesystem's `i_op->lookup()`. Used on dcache miss.
Fast path avoids locks for common case; slow path needed for disk access.
[Intermediate]

---

Q: (Cloze) During pathname resolution, VFS first tries _____ for cached lookups, and falls back to _____ which calls the filesystem's lookup callback.
A: `lookup_fast()`, `lookup_slow()`. The dcache hit rate is typically >90%, so the fast path handles most lookups.
[Intermediate]

---

Q: What happens during dcache lookup?
A: 1. Compute hash of component name
2. Search hash bucket for matching dentry
3. Compare names (may use d_compare for custom)
4. Verify dentry is still valid (d_revalidate for NFS)
5. Return dentry (positive, negative, or miss)
[Intermediate]

---

Q: What is RCU-walk in pathname resolution?
A: A lockless lookup mode that uses RCU (Read-Copy-Update) to traverse dentries without taking any locks. If it can't complete (needs blocking operation), it falls back to REF-walk (traditional locked mode). Huge performance win.
[Advanced]

---

Q: (Understanding) Why is RCU-walk faster than REF-walk?
A: RCU-walk avoids all lock acquisitions and atomic operations on the fast path. In REF-walk, each dentry lookup requires taking and releasing the dcache lock. For deep paths, this lock contention limits scalability.
[Advanced]

---

Q: What causes RCU-walk to fall back to REF-walk?
A: Any operation that might block or needs stronger guarantees:
- dcache miss (need to call filesystem)
- d_revalidate returns needs-revalidation
- Symlink that needs following
- Mount point that needs crossing
- Memory pressure during walk
[Advanced]

---

Q: How does VFS handle mount point crossing?
A: When walking into a directory that is a mount point:
1. `lookup_mnt()` checks if current dentry is mounted on
2. If yes, switch to mounted filesystem's root dentry
3. Continue resolution in new filesystem
Mount points are transparent to pathname resolution.
[Intermediate]

---

Q: What is `follow_mount()`?
A: Follows mount points - if the current dentry is a mount point, switches to the mounted filesystem's root. Called repeatedly because mounts can stack (A mounted on B mounted on C).
[Intermediate]

---

Q: How does VFS handle symbolic links during pathname resolution?
A: When encountering a symlink:
1. Read link target via `i_op->readlink` or `follow_link`
2. Restart resolution with link target
3. Track recursion depth (max 40) to prevent loops
4. Handle relative vs absolute symlinks differently
[Intermediate]

---

Q: What is `MAXSYMLINKS` and why does it exist?
A: Maximum symlink recursion depth (40 in v3.2). Prevents infinite loops from circular symlinks (a→b→a). If exceeded, resolution fails with ELOOP.
[Basic]

---

Q: What LOOKUP_* flags affect pathname resolution?
A:
- `LOOKUP_FOLLOW` - Follow symlinks on final component
- `LOOKUP_DIRECTORY` - Final component must be directory
- `LOOKUP_CREATE` - Creating file (affects final lookup)
- `LOOKUP_OPEN` - Opening file
- `LOOKUP_PARENT` - Stop at parent, return last component
- `LOOKUP_REVAL` - Force d_revalidate
[Intermediate]

---

Q: What is `LOOKUP_FOLLOW` vs not following symlinks?
A: With `LOOKUP_FOLLOW`: stat() follows symlinks to target.
Without: lstat() operates on symlink itself.
For open(), O_NOFOLLOW prevents following.
Controls whether final component symlink is resolved.
[Basic]

---

Q: (Code Interpretation) What does this pathname resolution check?
```c
if (nd->depth >= MAX_NESTED_LINKS) {
    return -ELOOP;
}
nd->depth++;
error = follow_link(&link, nd, &cookie);
nd->depth--;
```
A: Symlink recursion protection:
1. Check if too many nested symlinks (prevent infinite loop)
2. Return ELOOP if limit exceeded
3. Increment depth counter before following
4. Decrement after returning from recursive resolution
[Intermediate]

---

Q: What is "." handling during pathname resolution?
A: "." refers to current directory. During resolution, it's a no-op - VFS simply continues with current dentry. Optimized to avoid unnecessary lookup.
[Basic]

---

Q: What is ".." handling during pathname resolution?
A: ".." refers to parent directory. VFS:
1. Gets `dentry->d_parent`
2. Checks for mount point crossing (need to go to mounted-on filesystem)
3. At root, ".." stays at root
Special handling needed for mount boundaries.
[Intermediate]

---

Q: (Understanding) Why is ".." handling complex with mount points?
A: "/mnt/usb/.." should go to "/mnt", not usb's root parent. When crossing from mounted filesystem back to parent, must switch vfsmount too. `follow_dotdot()` handles this by checking mount boundaries.
[Advanced]

---

Q: What is `kern_path()` used for?
A: Kernel function to resolve a pathname to a `struct path`:
```c
int error = kern_path("/etc/passwd", LOOKUP_FOLLOW, &path);
```
Common utility for kernel code that needs to open files. Returns dentry + vfsmount in path structure.
[Intermediate]

---

Q: What is `user_path_at()` used for?
A: Like `kern_path()` but for user-provided paths:
```c
int error = user_path_at(AT_FDCWD, pathname, LOOKUP_FOLLOW, &path);
```
Handles AT_FDCWD (relative to cwd), validates user pointer, performs resolution.
[Intermediate]

---

Q: What is `AT_FDCWD` in pathname resolution?
A: Special fd value (-100) meaning "current working directory." Used with *at() syscalls (openat, fstatat). `user_path_at(AT_FDCWD, path, ...)` is equivalent to starting resolution at process cwd.
[Intermediate]

---

Q: What happens if a lookup encounters a negative dentry?
A: Negative dentries cache "file not found." If O_CREAT is set, the create path handles this. Otherwise, returns -ENOENT immediately without calling filesystem. Speeds up repeated lookups for non-existent files.
[Intermediate]

---

Q: What is `d_lookup()` vs `__d_lookup()`?
A: `d_lookup()` - Takes dcache lock, safe but slower
`__d_lookup()` - RCU-protected, lockless, faster
`__d_lookup_rcu()` - Full RCU-walk variant
Choose based on context: RCU-walk uses lockless variants.
[Advanced]

---

Q: (Reverse) This structure holds current state during pathname resolution, including path, last component, and recursion depth.
A: Q: What is `struct nameidata`?
[Intermediate]

---

Q: How does VFS handle "//" in paths?
A: Multiple slashes are treated as single slash. "//home//user" equals "/home/user". Handled by skipping consecutive slashes during component extraction.
[Basic]

---

Q: What is the complexity of pathname resolution?
A: O(n) where n is path depth (number of components). Each component requires dcache lookup (O(1) on hit) or filesystem lookup (varies by fs). Deep paths with many symlinks can be expensive.
[Intermediate]

---

Q: (Code Interpretation) What optimization is this?
```c
if (nd->last_type == LAST_DOT)
    return;
if (nd->last_type == LAST_DOTDOT)
    return follow_dotdot(nd);
```
A: Optimizes "." and ".." handling:
- "." (LAST_DOT) - no-op, stay at current dentry
- ".." (LAST_DOTDOT) - special parent handling
These common cases avoid full dcache lookup.
[Intermediate]

---

## Section 5: File Descriptors and File Tables

---

Q: What is a file descriptor (fd)?
A: A small non-negative integer that userspace uses to refer to an open file. The kernel translates fd to the actual `struct file` pointer via the process's file descriptor table. Common fds: 0=stdin, 1=stdout, 2=stderr.
[Basic]

---

Q: What is `struct files_struct`?
A: The per-process structure holding all open file descriptors. Pointed to by `task_struct->files`. Contains the fd table, count of open files, next fd to allocate, and lock. Shared between threads of same process.
[Basic]

---

Q: What are the key fields in `struct files_struct`?
A:
- `count` - Reference count (for sharing)
- `fdt` - Pointer to file descriptor table
- `fdtab` - Embedded small fd table
- `next_fd` - Hint for next available fd
- `file_lock` - Spinlock protecting the table
- `close_on_exec` - Bitmap of fds to close on exec
[Intermediate]

---

Q: What is `struct fdtable`?
A: The actual fd-to-file mapping table:
- `max_fds` - Maximum number of fds
- `fd` - Array of struct file pointers
- `close_on_exec` - Bitmap for close-on-exec flag
- `open_fds` - Bitmap of which fds are in use
The table can grow dynamically as more files are opened.
[Intermediate]

---

Q: (ASCII Diagram) Show the file descriptor table structure.
A:
```
task_struct
     │
     │ files
     ▼
struct files_struct
┌─────────────────────┐
│ count = 1           │
│ fdt ────────────────┼──► struct fdtable
│ next_fd = 5         │    ┌──────────────────┐
│ file_lock           │    │ max_fds = 256    │
└─────────────────────┘    │ fd ──────────────┼──► [file *array]
                           │ open_fds bitmap  │    ┌───┬───┬───┬───┐
                           │ close_on_exec    │    │ 0 │ 1 │ 2 │ 3 │...
                           └──────────────────┘    └─┬─┴─┬─┴─┬─┴─┬─┘
                                                     │   │   │   │
                                                     ▼   ▼   ▼   ▼
                                                   stdin stdout stderr file
```
[Intermediate]

---

Q: How does the fd table grow when needed?
A: When all fds are used, the kernel allocates a larger fdtable (double the size or minimum needed). The old table entries are copied to the new one, and the old table is freed via RCU after readers are done.
[Intermediate]

---

Q: (Cloze) The default limit for open files per process is _____, while the hard limit is typically _____.
A: **soft limit** (often 1024), **hard limit** (varies, often 4096 or higher). Can be changed with `ulimit -n` or setrlimit(). System-wide limit in `/proc/sys/fs/file-max`.
[Basic]

---

Q: What is `fget()` and when is it used?
A: Gets a reference to `struct file` from an fd:
```c
struct file *f = fget(fd);
// use f
fput(f);
```
Increments `f_count`. Returns NULL if fd is invalid. Must call `fput()` when done to release reference.
[Intermediate]

---

Q: What is `fput()` and when is it used?
A: Releases a reference to `struct file`:
```c
fput(file);
```
Decrements `f_count`. If count reaches 0, the file is closed - calls `f_op->release`, releases dentry/inode references. Must match every `fget()`.
[Intermediate]

---

Q: What is `fdget()` vs `fget()`?
A: `fdget()` is an optimized version that avoids refcount increment for simple cases:
```c
struct fd f = fdget(fd);
// use f.file
fdput(f);
```
Uses lighter synchronization when file won't escape current context. Faster for common syscall path.
[Intermediate]

---

Q: What is `struct fd` returned by fdget()?
A: A small structure:
```c
struct fd {
    struct file *file;
    unsigned int flags;  /* FDPUT_FPUT if need real fput */
};
```
The flags indicate whether `fdput()` needs to do real reference release or can skip it.
[Intermediate]

---

Q: What is `fd_install()` and when is it used?
A: Installs a `struct file` into an fd slot:
```c
fd_install(fd, file);
```
Called at end of open() after file struct is fully initialized. Makes the file visible to userspace. Must have already reserved the fd with `get_unused_fd_flags()`.
[Intermediate]

---

Q: (Code Interpretation) What does this fd allocation pattern show?
```c
int fd = get_unused_fd_flags(flags);
if (fd >= 0) {
    struct file *f = do_filp_open(dfd, pathname, &op);
    if (IS_ERR(f)) {
        put_unused_fd(fd);
        return PTR_ERR(f);
    }
    fd_install(fd, f);
}
return fd;
```
A: Standard open() pattern:
1. Reserve fd slot first
2. Open the file (may fail)
3. On failure, release reserved fd
4. On success, install file into fd slot
5. Return fd to userspace
[Intermediate]

---

Q: What is `get_unused_fd_flags()`?
A: Allocates a new file descriptor number:
```c
int fd = get_unused_fd_flags(O_CLOEXEC);
```
Returns lowest available fd (above any currently open). Handles O_CLOEXEC flag. Returns negative error on failure (too many open files).
[Intermediate]

---

Q: What is `put_unused_fd()`?
A: Releases an unused fd that was reserved but not installed:
```c
put_unused_fd(fd);
```
Called when open fails after reserving fd. Marks the fd as available again. Must not be called if `fd_install()` was successful.
[Intermediate]

---

Q: What is close-on-exec (CLOEXEC)?
A: Flag indicating fd should be automatically closed when exec() is called. Prevents file descriptors from leaking into child programs. Set with O_CLOEXEC flag on open() or fcntl(F_SETFD, FD_CLOEXEC).
[Basic]

---

Q: How does `dup()` work internally?
A: `dup(oldfd)`:
1. Get file from oldfd via `fget()`
2. Allocate new fd via `get_unused_fd_flags()`
3. Increment file's refcount
4. Install file at new fd via `fd_install()`
5. Return new fd
Both fds now reference same struct file.
[Intermediate]

---

Q: How does `dup2(oldfd, newfd)` work?
A: Forces newfd to point to same file as oldfd:
1. If newfd is open, close it first
2. Get file from oldfd
3. Install at newfd
If oldfd == newfd, just returns newfd (no-op).
[Intermediate]

---

Q: (Understanding) What happens when you dup() and then seek on one fd?
A: The position changes for BOTH fds. Both point to the same `struct file`, which has a single `f_pos`. Seeking via one fd affects reads/writes via the other. This is different from opening the same file twice.
[Intermediate]

---

Q: (ASCII Diagram) Show the difference between dup() and opening twice.
A:
```
dup(fd)                         open() twice
═══════                         ════════════

fd 3  ──┐                       fd 3  ──► struct file A
        ├──► struct file         │        f_pos = 100
fd 4  ──┘    f_pos = 100        │
                                fd 4  ──► struct file B
Both fds share same                       f_pos = 0
file struct and position
                                Different file structs,
                                independent positions
```
[Intermediate]

---

Q: How does fork() handle file descriptors?
A: Child inherits parent's fd table - same fds point to same `struct file` objects (refcount incremented). Like dup() for all fds. Parent and child share file positions. `files_struct` is copied, not shared.
[Intermediate]

---

Q: (Understanding) Why is fork() + file sharing sometimes problematic?
A: Parent and child share `f_pos` for inherited fds. If both read/write, they interfere with each other's position. Solution: close unneeded fds after fork, or use separate opens, or use pread/pwrite (explicit offset).
[Intermediate]

---

Q: How does close() work internally?
A: `close(fd)`:
1. Get and remove file from fd table
2. Clear fd slot (mark available)
3. Call `fput()` to release reference
4. If refcount hits 0, call `f_op->release()`
The fd number is immediately reusable.
[Intermediate]

---

Q: What is the file table vs fd table?
A: **FD table** - Per-process mapping of fd→file pointer (`files_struct`)
**File table** - Global pool of all open `struct file` objects
Each fd table entry points into the global file table.
[Basic]

---

Q: What is `fs/file_table.c` responsible for?
A: Global file structure management:
- `get_empty_filp()` - Allocate new struct file
- `fput()` - Release file reference
- File count tracking (`nr_files`)
- Slab cache for file structures
[Intermediate]

---

Q: (Reverse) This per-process structure contains fdt, next_fd, and count, managing all open files for a process.
A: Q: What is `struct files_struct`?
[Basic]

---

Q: What is the maximum fd value?
A: Typically limited by RLIMIT_NOFILE (soft/hard limit per process) and `/proc/sys/fs/nr_open` (system max). Default soft is often 1024. Can be millions on modern systems with increased limits.
[Intermediate]

---

Q: How do threads share file descriptors?
A: All threads in a process share the same `files_struct` (via `clone(CLONE_FILES)`). A file opened by one thread is immediately accessible by all threads. The `file_lock` spinlock protects concurrent access to the fd table.
[Intermediate]

---

## Section 6: open/read/write Call Paths

---

Q: What is the call path for `open()` system call?
A: `sys_open()` → `do_sys_open()` → `get_unused_fd_flags()` → `do_filp_open()` → `path_openat()` → `vfs_open()` → `f_op->open()` → `fd_install()` → return fd
Key stages: allocate fd, resolve path, create file struct, call filesystem.
[Intermediate]

---

Q: What does `do_sys_open()` do?
A: Main body of open() syscall:
1. `getname()` - Copy pathname from userspace
2. `get_unused_fd_flags()` - Reserve fd
3. `do_filp_open()` - Do the actual open
4. `fd_install()` - Make file visible at fd
5. Return fd or error
[Intermediate]

---

Q: What does `do_filp_open()` do?
A: Opens a file and returns `struct file`:
1. Set up `struct open_flags` from O_* flags
2. Set up `struct nameidata` for path walk
3. Call `path_openat()` to resolve and open
4. Return file pointer (or ERR_PTR on error)
[Intermediate]

---

Q: (ASCII Diagram) Show the open() call path.
A:
```
User: open("/home/user/file", O_RDWR)
          │
          ▼
     sys_open()
          │
          ▼
    do_sys_open()
          │
    ┌─────┴─────────────────────────────────────┐
    │                                           │
    ▼                                           ▼
get_unused_fd()                          do_filp_open()
    │                                           │
    │                                           ▼
    │                                    path_openat()
    │                                           │
    │                              ┌────────────┴────────────┐
    │                              │                         │
    │                              ▼                         ▼
    │                       link_path_walk()           do_last()
    │                       (resolve path)             (open final)
    │                                                        │
    │                                                        ▼
    │                                                  vfs_open()
    │                                                        │
    │                                                        ▼
    │                                               f_op->open()
    │                                               (filesystem)
    │                                                        │
    └──────────────────────────┬─────────────────────────────┘
                               │
                               ▼
                         fd_install()
                               │
                               ▼
                          return fd
```
[Intermediate]

---

Q: What does `path_openat()` do?
A: Core of file opening:
1. Initialize nameidata with starting point
2. Call `link_path_walk()` to resolve directory components
3. Call `do_last()` to handle final component
4. Create `struct file`, call `vfs_open()`
5. Handle O_CREAT, O_EXCL, O_TRUNC flags
[Intermediate]

---

Q: What does `do_last()` handle?
A: Final component of open:
1. Look up final filename
2. Handle O_CREAT (create if not exists)
3. Handle O_EXCL (fail if exists)
4. Permission checks
5. Allocate struct file
6. Call `vfs_open()`
[Intermediate]

---

Q: What does `vfs_open()` do?
A: Finalizes file structure and calls filesystem:
1. Set `file->f_op = inode->i_fop`
2. Set `file->f_mapping = inode->i_mapping`
3. If `f_op->open` exists, call it
4. Filesystem can reject open (return error)
5. Return 0 on success
[Intermediate]

---

Q: (Code Interpretation) What does this vfs_open snippet show?
```c
int vfs_open(const struct path *path, struct file *file, ...)
{
    file->f_path = *path;
    file->f_inode = path->dentry->d_inode;
    file->f_mapping = file->f_inode->i_mapping;
    file->f_op = fops_get(file->f_inode->i_fop);
    
    if (file->f_op->open)
        return file->f_op->open(file->f_inode, file);
    return 0;
}
```
A: Setting up struct file:
1. Copy path (dentry + mount)
2. Cache inode pointer
3. Set page cache mapping
4. Get file operations from inode
5. Call filesystem's open callback if present
[Intermediate]

---

Q: What is the call path for `read()` system call?
A: `sys_read()` → `fget_light()` → `vfs_read()` → `f_op->read()` (or `do_sync_read()` → `f_op->aio_read()`)
Then for buffered I/O: `generic_file_aio_read()` → `do_generic_file_read()` → page cache lookup/readpage
[Intermediate]

---

Q: What does `vfs_read()` do?
A: Main read logic:
1. Check file mode allows reading
2. `rw_verify_area()` - Check access, update atime
3. If `f_op->read` exists, call it directly
4. Otherwise call `do_sync_read()` → `aio_read()`
5. Update file position
[Intermediate]

---

Q: (ASCII Diagram) Show the read() call path for a buffered file read.
A:
```
User: read(fd, buf, count)
          │
          ▼
     sys_read()
          │
          ▼
     fget_light(fd) ──► struct file
          │
          ▼
      vfs_read()
          │
          ▼
    do_sync_read()
          │
          ▼
generic_file_aio_read()
          │
          ▼
do_generic_file_read()
          │
    ┌─────┴─────┐
    │           │
    ▼           ▼
Page in      Page NOT
cache?       in cache
    │           │
    ▼           ▼
 copy to    a_ops->readpage()
 user buf        │
    │           ▼
    │      read from disk
    │           │
    │           ▼
    │       add to cache
    │           │
    └─────┬─────┘
          ▼
  copy_to_user(buf)
          │
          ▼
    return bytes
```
[Intermediate]

---

Q: What is `do_generic_file_read()` (v3.2)?
A: Page-cache-based read implementation:
1. For each page of data needed:
   - `find_get_page()` - Check page cache
   - If not cached: `page_cache_sync_readahead()` + `a_ops->readpage()`
   - Wait for page to be uptodate
   - `copy_page_to_iter()` - Copy to user buffer
[Intermediate]

---

Q: What is `find_get_page()`?
A: Looks up a page in the page cache:
```c
struct page *page = find_get_page(mapping, index);
```
Returns NULL on cache miss. If found, increments page refcount. The index is page offset in file.
[Intermediate]

---

Q: What is read-ahead and why is it used?
A: Reading more pages than requested, anticipating future sequential reads. Triggered by `page_cache_sync_readahead()`. Dramatically improves sequential read performance by having data ready before requested.
[Intermediate]

---

Q: What is the call path for `write()` system call?
A: `sys_write()` → `fget_light()` → `vfs_write()` → `f_op->write()` (or `do_sync_write()` → `f_op->aio_write()`)
Then: `generic_file_aio_write()` → `generic_file_buffered_write()` → `a_ops->write_begin/write_end()`
[Intermediate]

---

Q: What does `vfs_write()` do?
A: Main write logic:
1. Check file mode allows writing
2. `rw_verify_area()` - Security module checks
3. If `f_op->write`, call it
4. Otherwise `do_sync_write()` → `aio_write()`
5. Update file position
6. `fsnotify_modify()` - Notify watchers
[Intermediate]

---

Q: What does `generic_file_aio_write()` do?
A: Buffered write implementation:
1. Take `i_mutex` for serialization
2. `__generic_file_aio_write()` - Do the actual write
3. Release `i_mutex`
4. If O_SYNC: call `vfs_fsync_range()` to flush
[Intermediate]

---

Q: (Code Interpretation) What does this buffered write loop show?
```c
do {
    page = grab_cache_page_write_begin(mapping, index, ...);
    a_ops->write_begin(..., &page, &fsdata);
    copied = iov_iter_copy_from_user(page, iter, offset, bytes);
    a_ops->write_end(..., copied, page, fsdata);
} while (count);
```
A: Page-at-a-time buffered write:
1. Get/create page in cache
2. `write_begin` - Filesystem prepares (allocate blocks)
3. Copy user data into page
4. `write_end` - Mark dirty, update size
5. Repeat until all data written
Data is now in cache, not on disk yet!
[Intermediate]

---

Q: What is the difference between buffered and direct I/O?
A: **Buffered I/O**: Goes through page cache. Write returns immediately (data in cache). Actual disk write happens later (writeback).
**Direct I/O** (O_DIRECT): Bypasses cache. Write goes directly to disk. Returns when data is on disk. Requires aligned buffers.
[Basic]

---

Q: What triggers writeback of dirty pages?
A: Several mechanisms:
1. `fsync()` / `fdatasync()` - Explicit sync
2. pdflush/flusher threads - Background periodic
3. Memory pressure - Need to reclaim pages
4. Unmount - Must flush all data
5. `sync()` - System-wide flush
[Intermediate]

---

Q: What is the writeback path for dirty pages?
A: `bdi_writeback` thread or direct reclaim → `writeback_inodes()` → `do_writepages()` → `a_ops->writepages()` (or `writepage()` per page) → `submit_bio()` → block layer → disk
[Intermediate]

---

Q: (Understanding) Why does write() return before data is on disk?
A: Performance. Waiting for disk (milliseconds) would make writes very slow. Buffered writes:
1. Copy to page cache (microseconds)
2. Return to application
3. Flush to disk later (asynchronously)
Use fsync() when durability is required.
[Intermediate]

---

Q: What is `fsync()` and what does it guarantee?
A: `fsync(fd)` ensures all file data and metadata is on persistent storage before returning. Calls `f_op->fsync()` which flushes dirty pages and inode. Essential for data integrity in databases, logs.
[Basic]

---

Q: What is `fdatasync()` vs `fsync()`?
A: `fdatasync()` only ensures data is written, may skip metadata that doesn't affect data retrieval (like atime). Faster than `fsync()` which also syncs all metadata. Use `fdatasync()` when only data integrity matters.
[Intermediate]

---

Q: What does `sync_file_range()` allow?
A: Fine-grained control over sync:
- Specify byte range to sync
- SYNC_FILE_RANGE_WAIT_BEFORE/AFTER - Wait options
- SYNC_FILE_RANGE_WRITE - Start writeback
Can initiate writeback without waiting, or wait for completion.
[Advanced]

---

Q: (Reverse) This function looks up a page in the page cache by mapping and index.
A: Q: What is `find_get_page()`?
[Intermediate]

---

Q: (Understanding) What happens during O_APPEND write?
A: Before each write:
1. Take `i_mutex`
2. Set `f_pos` to `i_size` (end of file)
3. Do the write
4. Release `i_mutex`
This ensures atomic append - multiple appenders don't overwrite each other.
[Intermediate]

---

Q: What is O_TRUNC handling in open?
A: When O_TRUNC is specified:
1. After successful open
2. Call `do_truncate()` → `i_op->setattr()`
3. Truncates file to zero length
4. Releases data blocks
Must have write permission.
[Intermediate]

---

Q: What is `copy_to_user()` / `copy_from_user()` role in read/write?
A: Safely copy data between kernel and userspace:
- `copy_from_user(kernel_buf, user_buf, size)` - For write
- `copy_to_user(user_buf, kernel_buf, size)` - For read
Handle page faults on user pages, validate user pointers. Return bytes NOT copied.
[Basic]

---

## Section 7: Mounting and Filesystem Registration

---

Q: What is `struct file_system_type`?
A: Describes a filesystem type (ext4, nfs, procfs). Contains the filesystem name, flags, mount callback, and module owner. One per filesystem type, registered with VFS via `register_filesystem()`.
[Basic]

---

Q: What are the key fields in `struct file_system_type`?
A:
- `name` - Filesystem name ("ext4", "nfs")
- `fs_flags` - Flags (FS_REQUIRES_DEV, etc.)
- `mount` - Mount callback function
- `kill_sb` - Unmount/destroy superblock callback
- `owner` - Module owner (for reference counting)
- `next` - Link in registered filesystem list
[Intermediate]

---

Q: (Code Interpretation) What does this file_system_type show?
```c
static struct file_system_type ext4_fs_type = {
    .owner      = THIS_MODULE,
    .name       = "ext4",
    .mount      = ext4_mount,
    .kill_sb    = kill_block_super,
    .fs_flags   = FS_REQUIRES_DEV,
};
```
A: ext4 filesystem registration:
- Module ownership for refcounting
- Filesystem name "ext4"
- Mount callback `ext4_mount`
- Uses standard block device cleanup
- Requires a block device (can't be memory-only)
[Intermediate]

---

Q: What is `register_filesystem()`?
A: Registers a filesystem type with VFS:
```c
int err = register_filesystem(&my_fs_type);
```
Adds to global `file_systems` list. After registration, the filesystem can be mounted. Usually called from module init.
[Basic]

---

Q: What is `unregister_filesystem()`?
A: Removes filesystem from VFS registry:
```c
unregister_filesystem(&my_fs_type);
```
Must be called before module unload. Fails if filesystem is still mounted. Usually called from module exit.
[Basic]

---

Q: How do you view registered filesystems?
A: Read `/proc/filesystems`:
```
nodev   sysfs
nodev   proc
nodev   tmpfs
        ext4
        ext3
```
"nodev" = no block device needed. Without "nodev" = requires block device.
[Basic]

---

Q: What is the `mount` callback in file_system_type?
A: Called when mounting the filesystem. Signature:
```c
struct dentry *mount(struct file_system_type *type, int flags,
                     const char *dev_name, void *data);
```
Must create/get superblock and return root dentry. Usually calls helper like `mount_bdev()`.
[Intermediate]

---

Q: What are the mount helper functions?
A:
- `mount_bdev()` - Mount from block device
- `mount_nodev()` - Mount without device (tmpfs, proc)
- `mount_single()` - Single instance (no multiple mounts)
- `mount_ns()` - Namespace filesystem (proc)
Each handles superblock allocation and calls `fill_super()`.
[Intermediate]

---

Q: (Code Interpretation) What does this mount callback show?
```c
static struct dentry *myfs_mount(struct file_system_type *type,
                                  int flags, const char *dev,
                                  void *data)
{
    return mount_bdev(type, flags, dev, data, myfs_fill_super);
}
```
A: Standard block device mount:
1. `mount_bdev()` handles device opening
2. Gets or creates superblock for the device
3. Calls `myfs_fill_super()` to initialize superblock
4. Returns root dentry
[Intermediate]

---

Q: What is `fill_super()` callback?
A: Initializes a newly created superblock:
```c
int fill_super(struct super_block *sb, void *data, int silent)
```
Must:
1. Read superblock from disk
2. Set `sb->s_op` (operations)
3. Create root inode
4. Set `sb->s_root` (root dentry)
[Intermediate]

---

Q: (Code Interpretation) What does fill_super need to do?
```c
static int myfs_fill_super(struct super_block *sb, void *data, int silent)
{
    sb->s_blocksize = 4096;
    sb->s_blocksize_bits = 12;
    sb->s_magic = MYFS_MAGIC;
    sb->s_op = &myfs_super_ops;
    
    root_inode = myfs_get_root_inode(sb);
    sb->s_root = d_make_root(root_inode);
    return 0;
}
```
A: Superblock initialization:
1. Set block size (for I/O)
2. Set filesystem magic number
3. Set operations table
4. Get/create root inode
5. Create root dentry via `d_make_root()`
[Intermediate]

---

Q: What is `kill_sb` callback in file_system_type?
A: Called during unmount to destroy superblock:
- `kill_block_super()` - For block device filesystems
- `kill_anon_super()` - For anonymous superblocks
- `kill_litter_super()` - Generic cleanup
Releases resources, writes back dirty data.
[Intermediate]

---

Q: What is FS_REQUIRES_DEV flag?
A: Indicates filesystem requires a block device. Without this flag, filesystem can be mounted without specifying a device (like procfs, tmpfs). Affects mount() behavior and error checking.
[Intermediate]

---

Q: What is a mount namespace?
A: Isolation of mount points between processes. Each namespace has independent mount tree. Created via `clone(CLONE_NEWNS)` or `unshare(CLONE_NEWNS)`. Used in containers for filesystem isolation.
[Intermediate]

---

Q: What is `struct mnt_namespace`?
A: Represents a mount namespace:
- `root` - Root vfsmount
- `list` - List of all mounts in namespace
- `count` - Reference count
- `user_ns` - Associated user namespace
Processes in same namespace see same mounts.
[Intermediate]

---

Q: What is `/proc/mounts` vs `/etc/mtab`?
A: `/proc/mounts` - Kernel's authoritative view of mounts (proc filesystem)
`/etc/mtab` - Userspace-maintained (legacy, often symlink to /proc/mounts)
`/proc/self/mounts` shows current process's namespace mounts.
[Basic]

---

Q: What happens during `mount()` system call?
A: 1. `sys_mount()` → `do_mount()`
2. Pathname resolution for mount point
3. Find/create superblock
4. Create vfsmount
5. Attach to mount tree
6. Update namespace
[Intermediate]

---

Q: (Reverse) This callback in file_system_type initializes the superblock when mounting.
A: Q: What is `fill_super()`?
[Intermediate]

---

Q: What are mount flags (MS_* constants)?
A: Control mount behavior:
- `MS_RDONLY` - Read-only mount
- `MS_NOSUID` - Ignore setuid bits
- `MS_NOEXEC` - No execution
- `MS_NODEV` - No device files
- `MS_REMOUNT` - Change existing mount
- `MS_BIND` - Bind mount
[Basic]

---

Q: What is a bind mount?
A: Mounts an existing directory at another location:
```bash
mount --bind /source /target
```
Same filesystem, different paths. Both paths access same files. Created with MS_BIND flag. No new superblock created.
[Intermediate]

---

## Section 8: Special Filesystems

---

Q: What is procfs (/proc)?
A: A virtual filesystem exposing kernel and process information as files. No disk storage - content generated dynamically when read. Used for system monitoring, configuration, and debugging. Implemented in `fs/proc/`.
[Basic]

---

Q: What kind of information does /proc expose?
A: Process info: `/proc/[pid]/` - status, maps, fd, cmdline
System info: `/proc/meminfo`, `/proc/cpuinfo`, `/proc/uptime`
Kernel config: `/proc/sys/` - tunable parameters
Misc: `/proc/filesystems`, `/proc/mounts`
[Basic]

---

Q: How does procfs generate file contents?
A: No storage - data generated when read:
1. open() creates minimal state
2. read() calls custom handler
3. Handler queries kernel data structures
4. Formats output and returns to user
Uses `seq_file` interface for larger outputs.
[Intermediate]

---

Q: What is the seq_file interface?
A: Helper for implementing /proc files that produce multi-line output:
- Handles iteration over data
- Manages buffer filling
- Handles seeking and partial reads
Functions: `seq_open()`, `seq_read()`, `seq_printf()`, `seq_file_operations`
[Intermediate]

---

Q: (Code Interpretation) What does this seq_file setup show?
```c
static const struct seq_operations myproc_seq_ops = {
    .start = myproc_seq_start,
    .next  = myproc_seq_next,
    .stop  = myproc_seq_stop,
    .show  = myproc_seq_show,
};

static int myproc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &myproc_seq_ops);
}
```
A: seq_file iterator pattern:
- `start` - Begin iteration, return first element
- `next` - Return next element
- `stop` - End iteration, cleanup
- `show` - Format current element to buffer
- `seq_open` connects file to these operations
[Intermediate]

---

Q: How do you create a new /proc entry?
A: In v3.2, use `proc_create()`:
```c
proc_create("myfile", 0444, NULL, &myproc_fops);
```
Parameters: name, mode, parent dir (NULL = /proc), file_operations.
Returns proc_dir_entry pointer.
[Intermediate]

---

Q: What is sysfs (/sys)?
A: Filesystem exposing kernel device model. Represents devices, drivers, buses as directories. Attributes are files. Used by udev for device management. Implemented in `fs/sysfs/`.
[Basic]

---

Q: What is the structure of sysfs?
A: Hierarchical device model:
- `/sys/devices/` - All devices by physical hierarchy
- `/sys/bus/` - Devices organized by bus type
- `/sys/class/` - Devices by class (net, block)
- `/sys/block/` - Block devices
- `/sys/module/` - Loaded modules
[Intermediate]

---

Q: How is sysfs different from procfs?
A: **procfs**: Process and kernel info, ad-hoc structure, older
**sysfs**: Device model, strict hierarchy via kobjects, cleaner
Rule: Procfs for process info, sysfs for device info. Some overlap exists for historical reasons.
[Intermediate]

---

Q: What is a kobject in relation to sysfs?
A: Base object that creates sysfs directories. Every sysfs directory corresponds to a kobject. Devices, drivers contain kobjects. Adding kobject to hierarchy creates sysfs entries automatically.
[Intermediate]

---

Q: What is tmpfs?
A: RAM-based filesystem. Files exist only in memory (page cache). Fast but data lost on reboot. Used for `/tmp`, `/run`, `/dev/shm`. Can be size-limited.
[Basic]

---

Q: How does tmpfs differ from ramfs?
A: Both RAM-based, but:
**tmpfs**: Can swap to disk, has size limits, uses swap
**ramfs**: Never swaps, can grow unbounded, simpler
tmpfs is generally preferred for its limits.
[Intermediate]

---

Q: What is devtmpfs?
A: Kernel-managed device filesystem. Automatically creates device nodes when devices are registered. Mounted at `/dev`. Reduces need for udev to create basic nodes.
[Intermediate]

---

Q: What problem does devtmpfs solve?
A: Bootstrap problem: udev needs device nodes to access devices, but creates device nodes when devices appear. devtmpfs creates initial nodes, udev can then manage permissions and symlinks.
[Intermediate]

---

Q: What is debugfs (/sys/kernel/debug)?
A: Filesystem for kernel debugging. Developers export debug info without polluting procfs. Not for stable interfaces. May not be mounted in production.
[Intermediate]

---

Q: (Understanding) Why have multiple pseudo-filesystems?
A: Separation of concerns:
- **procfs** - Process/kernel info (historical)
- **sysfs** - Device model (structured)
- **debugfs** - Debugging (unstable APIs)
- **configfs** - Userspace-driven configuration
Each has different stability and purpose guarantees.
[Intermediate]

---

Q: What is configfs?
A: Userspace-driven kernel configuration. Unlike sysfs (kernel creates structure), configfs lets userspace create objects (mkdir) that kernel populates with attributes. Used for USB gadgets, targets.
[Advanced]

---

Q: (Code Interpretation) What makes procfs files special?
```c
static const struct file_operations proc_stat_operations = {
    .open    = stat_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};
```
A: Procfs files use special file_operations:
- `stat_open` - Initialize/generate data
- `seq_read` - Generic seq_file read
- No write (read-only stat)
- `single_release` - Single-shot seq_file cleanup
Content generated on open, read serves cached data.
[Intermediate]

---

Q: (Reverse) This RAM-based filesystem is commonly mounted at /tmp and can swap to disk.
A: Q: What is tmpfs?
[Basic]

---

Q: What is `/proc/[pid]/fd/`?
A: Directory containing symlinks to process's open files. Link name is fd number, target is file path. Useful for seeing what files a process has open. Reading symlink uses `proc_fd_link()`.
[Intermediate]

---

Q: What is `/proc/[pid]/maps`?
A: Shows memory mappings for a process. Each line: address range, permissions, offset, device, inode, pathname. Used by debuggers, memory analysis tools.
[Basic]

---

