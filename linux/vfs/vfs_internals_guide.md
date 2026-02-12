# The Linux Virtual File System (VFS): A Complete Guide for Programmers

> **Audience**: Proficient programmers who understand system calls (`open`, `read`,
> `write`, `close`) but are new to kernel internals — inodes, dentries, the page
> cache, and VFS data structures.
>
> **Source kernel**: Linux 3.x series (`include/linux/fs.h`, `include/linux/dcache.h`)

---

## Table of Contents

1. [The Foundational Metaphor](#1-the-foundational-metaphor)
2. [Architecture — From `read()` to Disk](#2-architecture--from-read-to-disk)
3. [The Four Pillars — Core Data Structures](#3-the-four-pillars--core-data-structures)
   - [Pillar 1: `struct super_block`](#pillar-1-struct-super_block)
   - [Pillar 2: `struct inode`](#pillar-2-struct-inode)
   - [Pillar 3: `struct dentry`](#pillar-3-struct-dentry)
   - [Pillar 4: `struct file`](#pillar-4-struct-file)
4. [Crucial Clarification — inode vs. dentry vs. file](#4-crucial-clarification--inode-vs-dentry-vs-file)
5. [Walkthrough — From `open()` to File Descriptor](#5-walkthrough--from-open-to-file-descriptor)
6. [Connecting to Known Concepts — The Complete `read()` Chain](#6-connecting-to-known-concepts--the-complete-read-chain)
7. [Summary — The VFS Design Philosophy](#7-summary--the-vfs-design-philosophy)

---

## 1. The Foundational Metaphor

Think of the Linux kernel as a building that houses dozens of different translation
agencies — one for ext4, one for XFS, one for NFS, one for procfs, one for tmpfs,
and so on. Each agency speaks its own language and stores documents in its own way.

**VFS is the universal front desk.**

When a user (a process) walks in and says *"I'd like to read this file,"* the front
desk doesn't know or care which agency actually stores the file. It takes the
request, translates it into a **standard internal form**, routes it to the right
agency, and hands the result back to the user — all through a single, uniform
window.

More precisely:

> **VFS (Virtual File System)** is the kernel's **common file API** — an
> abstraction layer that provides a uniform set of data structures and function
> pointers so that *every* filesystem looks the same to the rest of the kernel
> and to userspace.

This is why you can run `cat /proc/cpuinfo` with the same `read()` system call
you use to read a file on an ext4 disk. The system call is identical. Only the
*implementation behind VFS* differs.

**Why this matters:**

- Without VFS, every program would need to know *which* filesystem it's talking
  to and call filesystem-specific functions.
- With VFS, the kernel enforces a **contract** (a set of C structs with function
  pointers) that every filesystem must implement, and the rest of the kernel only
  speaks through that contract.

---

## 2. Architecture — From `read()` to Disk

Here is the full journey of a `read(fd, buf, size)` call, from your C program
down to the physical storage device:

```
  ┌─────────────────────────────────────────────────────┐
  │                    USERSPACE                        │
  │                                                     │
  │   your_program.c:  read(fd, buf, size)              │
  └───────────────────────┬─────────────────────────────┘
                          │  (trap into kernel via syscall)
                          ▼
  ┌─────────────────────────────────────────────────────┐
  │               SYSCALL INTERFACE                     │
  │                                                     │
  │   sys_read()  →  vfs_read()                         │
  │   Translates fd → struct file*                      │
  └───────────────────────┬─────────────────────────────┘
                          │
                          ▼
  ┌─────────────────────────────────────────────────────┐
  │                   VFS LAYER           <── THE FOCUS │
  │                                                     │
  │   Operates on four core objects:                    │
  │     • struct super_block  (filesystem instance)     │
  │     • struct inode        (file identity & metadata)│
  │     • struct dentry       (name → inode mapping)    │
  │     • struct file         (open file instance)      │
  │                                                     │
  │   Calls:  file->f_op->read(file, buf, size, &pos)   │
  │           ^^^^^^^^^^^^                              │
  │           This is a function POINTER — it dispatches│
  │           to the actual filesystem's read function. │
  └───────────────────────┬─────────────────────────────┘
                          │
                          ▼
  ┌─────────────────────────────────────────────────────┐
  │          FILESYSTEM IMPLEMENTATION                  │
  │                                                     │
  │   e.g., ext4_file_read()  /  nfs_file_read()        │
  │         xfs_file_read()   /  proc_reg_read()        │
  │                                                     │
  │   Knows the on-disk format. Translates VFS          │
  │   requests into block numbers, network RPCs, etc.   │
  └───────────────────────┬─────────────────────────────┘
                          │
                          ▼
  ┌─────────────────────────────────────────────────────┐
  │               PAGE CACHE                            │
  │                                                     │
  │   The kernel's file-data cache in RAM.              │
  │   Indexed by (inode, page_offset).                  │
  │   If the data is already cached → return it.        │
  │   If not → trigger a read from the block layer.     │
  │                                                     │
  │   Managed via:  struct address_space (per-inode)    │
  └───────────────────────┬─────────────────────────────┘
                          │  (cache miss)
                          ▼
  ┌─────────────────────────────────────────────────────┐
  │               BLOCK LAYER                           │
  │                                                     │
  │   Converts (inode, offset) into (device, sector).   │
  │   Builds I/O requests (struct bio), applies I/O     │
  │   schedulers, and submits them to the driver.       │
  └───────────────────────┬─────────────────────────────┘
                          │
                          ▼
  ┌─────────────────────────────────────────────────────┐
  │             STORAGE DEVICE                          │
  │                                                     │
  │   HDD  /  SSD  /  NVMe  /  Network (NFS)            │
  │   Actual bytes on a physical medium.                │
  └─────────────────────────────────────────────────────┘
```

**What each layer is responsible for:**

| Layer | Responsibility |
|-------|---------------|
| **Userspace** | Issues POSIX system calls (`read`, `write`, `open`, etc.) |
| **Syscall Interface** | Validates arguments; converts the file descriptor `fd` into a kernel `struct file*` |
| **VFS Layer** | The **universal router**. Holds the four core data structures. Dispatches to the correct filesystem via function pointers. |
| **Filesystem Impl.** | Knows the on-disk layout (ext4 journal, XFS B+ trees, procfs in-memory generation). Translates abstract requests into concrete operations. |
| **Page Cache** | RAM-based cache of file pages. Avoids hitting disk on repeated reads. Every regular file's data flows through here. |
| **Block Layer** | Translates file-level offsets into disk block addresses. Merges and schedules I/O requests. |
| **Storage Device** | The physical hardware that persists data. |

---

## 3. The Four Pillars — Core Data Structures

VFS stands on four primary C structures. Think of them as four different lenses
through which the kernel views a file:

```
  ┌──────────────────────────────────────────────────────────────────┐
  │                     VFS Object Relationships                     │
  │                                                                  │
  │  ┌──────────────┐     s_root       ┌──────────────┐              │
  │  │ super_block   │───────────────> │   dentry     │ (root "/")   │
  │  │ (filesystem   │                 │ (name->inode │              │
  │  │  instance)    │                 │  mapping)    │              │
  │  └──────┬───────┘                  └──────┬───────┘              │
  │         │ s_inodes (list)               │ d_inode                │
  │         ▼                               ▼                        │
  │  ┌──────────────┐              ┌──────────────┐                  │
  │  │   inode       │<────────────│   inode      │                  │
  │  │ (file identity│             │ (same or     │                  │
  │  │  & metadata)  │             │  different)  │                  │
  │  └──────┬───────┘              └──────────────┘                  │
  │         │ i_fop (file_operations)                                │
  │         ▼                                                        │
  │  ┌──────────────┐                                                │
  │  │   file       │  <── created on each open() call               │
  │  │ (open file   │                                                │
  │  │  instance)   │                                                │
  │  └──────────────┘                                                │
  └──────────────────────────────────────────────────────────────────┘
```

Let's examine each one.

---

### Pillar 1: `struct super_block`

#### ASCII Diagram

```
  struct super_block
  ┌─────────────────────────────────────────────────┐
  │  s_dev          : dev_t        (device ID)      │
  │  s_blocksize    : unsigned long                 │
  │  s_maxbytes     : loff_t       (max file size)  │
  │  s_type         : ───────────> file_system_type │
  │  s_op           : ───────────> super_operations │
  │  s_magic        : unsigned long (FS magic #)    │
  │  s_root         : ───────────> dentry (root)    │
  │  s_flags        : unsigned long (mount flags)   │
  │  s_inodes       : list_head    (all inodes)     │
  │  s_bdev         : ───────────> block_device     │
  │  s_dentry_lru   : list_head    (unused dentries)│
  │  s_inode_lru    : list_head    (unused inodes)  │
  └─────────────────────────────────────────────────┘
                    │
                    │ s_op (function pointers)
                    ▼
  struct super_operations
  ┌─────────────────────────────────────────────────┐
  │  alloc_inode()    write_inode()    drop_inode() │
  │  destroy_inode()  put_super()      sync_fs()    │
  │  statfs()         remount_fs()     ...          │
  └─────────────────────────────────────────────────┘
```

> *Source: `include/linux/fs.h`, line 1400*

#### What

**`super_block` represents a single mounted filesystem instance.** It is the
kernel's in-memory description of *an entire filesystem* — one per mount.

#### Why

Why can't the kernel just track individual files? Because a filesystem is more
than its files. The kernel needs to know: *What type of filesystem is this? What
is the block size? What is the maximum file size? How do I allocate a new inode
on this filesystem? How do I sync dirty data back to disk?*

The `super_block` is the **root anchor** for all of this. It is the entry point
into a mounted filesystem.

#### Lifecycle

| Event | What Happens |
|-------|-------------|
| **Created** | When a filesystem is mounted (`mount -t ext4 /dev/sda1 /mnt`). The kernel calls the filesystem's `fill_super()` to populate it. |
| **Active** | Exists for the entire duration of the mount. All inodes and dentries link back to it. |
| **Destroyed** | When the filesystem is unmounted (`umount /mnt`). The kernel calls `put_super()` and frees all associated objects. |

---

### Pillar 2: `struct inode`

#### ASCII Diagram

```
  struct inode
  ┌─────────────────────────────────────────────────────┐
  │  i_ino          : unsigned long  (inode number)     │
  │  i_mode         : umode_t        (file type+perms)  │
  │  i_uid / i_gid  : uid_t / gid_t  (ownership)        │
  │  i_size         : loff_t          (file size)       │
  │  i_atime        : struct timespec (last access)     │
  │  i_mtime        : struct timespec (last modify)     │
  │  i_ctime        : struct timespec (last change)     │
  │  i_nlink        : unsigned int    (hard link count) │
  │  i_blocks       : blkcnt_t        (512B blocks)     │
  │  i_sb           : ───────────────> super_block      │
  │  i_op           : ───────────────> inode_operations │
  │  i_fop          : ───────────────> file_operations  │
  │  i_mapping      : ───────────────> address_space    │
  │  i_dentry       : list_head        (alias dentries) │
  │  i_data         : struct address_space (page cache) │
  │  i_count        : atomic_t         (reference count)│
  └─────────────────────────────────────────────────────┘
           │                              │
           │ i_op                         │ i_fop
           ▼                              ▼
  ┌──────────────────────┐    ┌─────────────────────────┐
  │  inode_operations    │    │   file_operations       │
  │  (name operations)   │    │   (data operations)     │
  ├──────────────────────┤    ├─────────────────────────┤
  │  lookup()            │    │   read()                │
  │  create()            │    │   write()               │
  │  mkdir()             │    │   mmap()                │
  │  unlink()            │    │   llseek()              │
  │  rename()            │    │   open()                │
  │  permission()        │    │   release()             │
  │  setattr()           │    │   fsync()               │
  └──────────────────────┘    └─────────────────────────┘
```

> *Source: `include/linux/fs.h`, line 749*

#### What

**`inode` is the file's identity card.** It represents a single file (or
directory, or pipe, or device) and holds all metadata *except* the name.

#### Why

Why separate the file's identity from its name? Because **a single file can have
multiple names** (hard links). The file `/home/user/doc.txt` and
`/tmp/doc_link.txt` can be two different names for the exact same inode. The inode
holds the data that is intrinsic to the file itself — size, permissions,
timestamps, disk block locations — independent of what name(s) refer to it.

The inode also carries two critical sets of **function pointers**:

- **`i_op` (inode_operations)**: Operations on the *name/directory entry* —
  lookup, create, mkdir, unlink, rename. Used during path resolution.
- **`i_fop` (file_operations)**: Operations on the *data stream* — read, write,
  mmap, llseek. Used after the file is opened.

This split is key: `i_op` is about *navigating the namespace*, while `i_fop` is
about *reading/writing data*.

#### Lifecycle

| Event | What Happens |
|-------|-------------|
| **Created** | When a file is first accessed (or created). `super_operations->alloc_inode()` allocates it; the filesystem fills in on-disk metadata. |
| **Cached** | Stored in the **inode cache** (icache), a hash table indexed by `(super_block, i_ino)`. Stays cached as long as anything references it. |
| **Destroyed** | When the reference count (`i_count`) drops to zero *and* memory pressure triggers eviction. `super_operations->destroy_inode()` frees it. |

---

### Pillar 3: `struct dentry`

#### ASCII Diagram

```
  struct dentry
  ┌───────────────────────────────────────────────────┐
  │  d_name        : struct qstr     (filename string)│
  │  d_iname[36]   : char[]          (inline short    │
  │                                    name storage)  │
  │  d_inode       : ───────────────> inode (or NULL) │
  │  d_parent      : ───────────────> dentry (parent  │
  │                                    directory)     │
  │  d_sb          : ───────────────> super_block     │
  │  d_op          : ─────────────> dentry_operations │
  │  d_flags       : unsigned int                     │
  │  d_hash        : hlist_bl_node   (hash bucket)    │
  │  d_subdirs     : list_head       (children)       │
  │  d_child       : list_head       (sibling link)   │
  │  d_alias       : list_head       (inode's alias   │
  │                                    list)          │
  │  d_lru         : list_head       (LRU for reclaim)│
  │  d_count       : unsigned int    (reference count)│
  └───────────────────────────────────────────────────┘

  Directory Tree Example:
  ┌──────────┐
  │ dentry / │  (root)
  └────┬─────┘
       │ d_subdirs
       ├───────────────────┐
       ▼                   ▼
  ┌──────────┐      ┌──────────┐
  │  "home"  │      │  "etc"   │
  └────┬─────┘      └──────────┘
       │ d_subdirs
       ▼
  ┌──────────┐
  │  "user"  │
  └────┬─────┘
       │ d_subdirs
       ▼
  ┌───────────┐
  │ "doc.txt" │──────>  inode (i_ino=12345)
  └───────────┘
        d_inode
```

> *Source: `include/linux/dcache.h`, line 116*

#### What

**`dentry` (directory entry) is the glue between a filename and its inode.** It
represents one component of a pathname — a single name in the directory hierarchy.

#### Why

Why not just store names inside the inode? Three reasons:

1. **Hard links**: Multiple dentries can point to the same inode. The name is a
   property of the *directory*, not the *file*.
2. **Path lookup speed**: The kernel resolves paths like `/home/user/doc.txt` by
   walking one component at a time: `/` → `home` → `user` → `doc.txt`. Each
   component is a dentry. The **dentry cache (dcache)** makes repeated lookups
   near-instant.
3. **Negative dentries**: The kernel caches *failed* lookups too. If you try to
   `open("/tmp/nonexistent")`, the kernel creates a **negative dentry** (one with
   `d_inode = NULL`). The next time something asks for that path, the kernel
   immediately knows the file doesn't exist — without hitting disk.

#### Lifecycle

| Event | What Happens |
|-------|-------------|
| **Created** | During path resolution (`path_lookup`). One dentry is created for each component of the path. |
| **Cached** | Stored in the **dcache** — a global hash table indexed by `(parent dentry, name)`. This is one of the most performance-critical caches in the kernel. |
| **LRU** | When the reference count (`d_count`) drops to zero, the dentry moves to the **LRU list** instead of being freed. It stays cached for future lookups. |
| **Destroyed** | Only under memory pressure. The kernel reclaims LRU dentries (and their associated inode cache entries) when RAM is needed. |

---

### Pillar 4: `struct file`

#### ASCII Diagram

```
  struct file
  ┌───────────────────────────────────────────────────────┐
  │  f_path        : struct path                          │
  │    .dentry     : ───────────────> dentry              │
  │    .mnt        : ───────────────> vfsmount            │
  │  f_op          : ───────────────> file_operations     │
  │  f_flags       : unsigned int     (O_RDONLY, O_APPEND)│
  │  f_mode        : fmode_t          (FMODE_READ, etc.)  │
  │  f_pos         : loff_t           (current offset)    │
  │  f_count       : atomic_long_t    (reference count)   │
  │  f_cred        : ───────────────> cred (credentials)  │
  │  f_ra          : file_ra_state    (readahead state)   │
  │  f_owner       : fown_struct      (signal owner)      │
  └───────────────────────────────────────────────────────┘

  Relationship to process:

  Process A (task_struct)               Process B (task_struct)
       │                                     │
       │ task->files                         │ task->files
       ▼                                     ▼
  ┌──────────────┐                    ┌──────────────┐
  │ files_struct │                    │ files_struct │
  │  fd_array:   │                    │  fd_array:   │
  │   [0] -> file│ (stdin)            │   [0] -> file│
  │   [1] -> file│ (stdout)           │   [1] -> file│
  │   [3] -> file│──────┐             │   [4] -> file│──┐
  └──────────────┘      │             └──────────────┘  │
                        │                               │
                        ▼                               │
                   ┌──────────┐                         │
                   │ struct   │<────────────────────────┘
                   │ file     │  (two fds can share the
                   │          │   same struct file after
                   │ f_pos=42 │   dup() or fork())
                   │ f_op ────┤--> file_operations (ext4)
                   └────┬─────┘
                        │ f_path.dentry->d_inode
                        ▼
                   ┌──────────┐
                   │  inode   │  (one inode, many files)
                   │ i_ino=123│
                   └──────────┘
```

> *Source: `include/linux/fs.h`, line 964*

#### What

**`file` represents one open instance of a file.** It is the kernel-side object
behind a file descriptor.

#### Why

Why not just use the inode directly when a file is opened? Because **multiple
processes (or the same process) can open the same file independently**, and each
needs its own:

- **Current position** (`f_pos`): Process A is reading at byte 1000; process B
  is at byte 5000. They share the same inode, but each has its own `struct file`
  with its own `f_pos`.
- **Access mode** (`f_flags`, `f_mode`): Process A opened read-only; process B
  opened read-write.
- **Readahead state** (`f_ra`): The kernel tracks per-open-file readahead
  heuristics to optimize sequential reads.

The `struct file` is the **per-open, per-process** view. The inode is the
**per-file, global** identity.

#### Lifecycle

| Event | What Happens |
|-------|-------------|
| **Created** | On every `open()` system call. `get_empty_filp()` allocates a new `struct file`. |
| **Active** | Linked into the process's `files_struct->fd_array` via a file descriptor integer. |
| **Shared** | `dup()`, `dup2()`, or `fork()` increment `f_count`. Multiple file descriptors (even across processes) can point to the same `struct file`. |
| **Destroyed** | When `f_count` drops to zero (all file descriptors closed). `file_operations->release()` is called to notify the filesystem. |

---

## 4. Crucial Clarification — inode vs. dentry vs. file

This is where most newcomers get confused. Let's make it crystal clear.

### inode vs. dentry

```
  ANALOGY: Think of a person and their name(s).

  inode  =  the PERSON (unique identity: DNA, fingerprints, birthday)
  dentry =  a NAME TAG pointing to that person

  A person can have multiple name tags (nicknames, aliases = hard links).
  A name tag with no person attached = negative dentry ("this name doesn't exist").

  ┌─────────────────┐     d_inode     ┌─────────────────┐
  │ dentry "doc.txt" │───────────────>│  inode #12345   │
  └─────────────────┘                 │  size: 4096     │
                                      │  uid: 1000      │
  ┌─────────────────┐     d_inode     │  blocks: 8      │
  │ dentry "link.txt"│───────────────>│  i_nlink: 2     │
  └─────────────────┘                 └─────────────────┘
                                           (same file!)
  ┌─────────────────┐     d_inode
  │ dentry "ghost"   │───────────────>  NULL
  └─────────────────┘
       (negative dentry — file does not exist)
```

**Key insight**: The dentry lives in the **directory hierarchy** (it knows its
parent, its name, and its children). The inode lives in the **filesystem's flat
number space** (it knows its inode number, but has no idea what names point to it).

### file vs. inode

```
  ANALOGY: Think of a book (inode) and multiple bookmarks (files).

  inode  =  the BOOK  (one copy exists in the library)
  file   =  a BOOKMARK (each reader has their own, at a different page)

  ┌─────────────┐
  │ struct file │  Process A: fd=3, f_pos=0,    O_RDONLY
  │  f_pos = 0  │───────┐
  └─────────────┘       │
                        │    d_inode
  ┌─────────────┐       ├──────────>  ┌──────────────┐
  │ struct file │       │             │  inode #9999 │
  │  f_pos = 512│───────┘             │  i_size=4096 │
  └─────────────┘                     │  i_mode=0644 │
    Process B: fd=5, f_pos=512,       └──────────────┘
               O_RDWR                   (one file on disk)
```

**Key insight**: Closing one `struct file` (one file descriptor) does not affect
the inode or other open `struct file` objects pointing to the same inode. The
inode survives until the last hard link is removed *and* the last reference is
dropped.

### Summary Table

| Object | Represents | Unique Per... | Named By |
|--------|-----------|---------------|----------|
| `super_block` | A mounted filesystem | Mount point | Device + fstype |
| `inode` | A file's identity & metadata | Filesystem + inode number | Nothing (no name!) |
| `dentry` | A name in the directory tree | (parent dentry, name) pair | A path component |
| `file` | An open file instance | `open()` call | A file descriptor (int) |

---

## 5. Walkthrough — From `open()` to File Descriptor

Let's trace the simplified journey of:

```c
int fd = open("/home/user/doc.txt", O_RDONLY);
```

### Step-by-Step

```
  open("/home/user/doc.txt", O_RDONLY)
       │
       │  (1) Syscall entry
       ▼
  ┌──────────────────────────────────────────────┐
  │  sys_open()  →  do_sys_open()                │
  │                                              │
  │  Allocates an unused file descriptor (fd=3). │
  │  Calls do_filp_open() to do the real work.   │
  └──────────────────┬───────────────────────────┘
                     │
                     │  (2) Path resolution begins
                     ▼
  ┌──────────────────────────────────────────────┐
  │  path_openat()  →  link_path_walk()          │
  │                                              │
  │  The path "/home/user/doc.txt" is split into │
  │  components and resolved left-to-right:      │
  │                                              │
  │  ┌───────────────────────────────────────┐   │
  │  │  "/" → lookup dcache for root dentry  │   │
  │  │        ✓ Always cached (s_root)       │   │
  │  │                                       │   │
  │  │  "home" → lookup dcache:              │   │
  │  │        hash(root_dentry, "home")      │   │
  │  │        ✓ Cache hit → dentry found     │   │
  │  │        → d_inode gives us the inode   │   │
  │  │                                       │   │
  │  │  "user" → lookup dcache:              │   │
  │  │        hash(home_dentry, "user")      │   │
  │  │        ✓ Cache hit → dentry found     │   │
  │  │                                       │   │
  │  │  "doc.txt" → lookup dcache:           │   │
  │  │        hash(user_dentry, "doc.txt")   │   │
  │  │        ✗ Cache MISS                   │   │
  │  │        → call inode_operations->      │   │
  │  │          lookup(parent_inode, dentry) │   │
  │  │        → filesystem reads directory   │   │
  │  │          from disk                    │   │
  │  │        → creates new dentry + inode   │   │
  │  │        → inserts into dcache          │   │
  │  └───────────────────────────────────────┘   │
  └──────────────────┬───────────────────────────┘
                     │
                     │  (3) Permission check
                     ▼
  ┌──────────────────────────────────────────────┐
  │  inode_permission(inode, MAY_READ)           │
  │                                              │
  │  Checks i_mode against the process's uid/gid.│
  │  If denied → return -EACCES.                 │
  └──────────────────┬───────────────────────────┘
                     │
                     │  (4) Create struct file
                     ▼
  ┌──────────────────────────────────────────────┐
  │  get_empty_filp()                            │
  │                                              │
  │  Allocates a new struct file and fills it:   │
  │    file->f_path.dentry = found_dentry        │
  │    file->f_path.mnt    = mount_point         │
  │    file->f_op          = inode->i_fop        │
  │    file->f_flags       = O_RDONLY            │
  │    file->f_mode        = FMODE_READ          │
  │    file->f_pos         = 0                   │
  │                                              │
  │  Then calls:                                 │
  │    file->f_op->open(inode, file)             │
  │    (filesystem-specific open, e.g. ext4)     │
  └──────────────────┬───────────────────────────┘
                     │
                     │  (5) Install fd
                     ▼
  ┌──────────────────────────────────────────────┐
  │  fd_install(fd, file)                        │
  │                                              │
  │  Places the struct file* into the process's  │
  │  file descriptor table:                      │
  │    current->files->fd_array[3] = file        │
  │                                              │
  │  Returns fd=3 to userspace.                  │
  └──────────────────────────────────────────────┘
```

### The Resulting Object Graph

After `open()` completes, the kernel has built this chain:

```
  fd = 3 (integer in userspace)
    │
    │  current->files->fd_array[3]
    ▼
  struct file
    │  f_pos = 0
    │  f_flags = O_RDONLY
    │  f_op ──────────────────────────> ext4_file_operations
    │  f_path.dentry
    ▼
  struct dentry ("doc.txt")
    │  d_parent ──> dentry("user") ──> dentry("home") ──> dentry("/")
    │  d_inode
    ▼
  struct inode (i_ino = 12345)
    │  i_sb ──────> super_block (ext4, /dev/sda1)
    │  i_mapping ──> address_space (page cache for this file)
    │  i_op ──────> ext4_dir_inode_operations
    │  i_fop ─────> ext4_file_operations
    ▼
  struct super_block
    │  s_op ──────> ext4_sops
    │  s_bdev ────> block_device (/dev/sda1)
    │  s_root ────> dentry ("/") of the ext4 mount
    ▼
  (links back to the physical device)
```

---

## 6. Connecting to Known Concepts — The Complete `read()` Chain

Now let's close the loop. You already know `read()`. Here's what happens
*inside* the kernel:

> **When a process calls `read(fd, buf, size)`, the kernel uses the file
> descriptor `fd` to index into the process's file descriptor table and find the
> `struct file*`. From the `struct file*`, it follows `f_path.dentry->d_inode`
> to reach the `struct inode*`. It then calls
> `file->f_op->read(file, buf, size, &file->f_pos)`, which dispatches to the
> filesystem-specific read function (e.g., `ext4_file_read()`). That function
> consults the inode's `address_space` (via `inode->i_mapping`) to check the
> page cache. If the requested data is already cached in RAM, it is copied
> directly to the user's buffer — no disk I/O required. If it is a cache miss,
> the filesystem triggers a read from the block layer, which fetches the data
> from the storage device, populates the page cache, and then copies it to the
> user buffer.**

In diagram form:

```
  read(fd=3, buf, 4096)
       │
       ▼
  current->files->fd_array[3]  →  struct file* (f_pos = 0)
       │
       │  file->f_op->read(file, buf, 4096, &f_pos)
       ▼
  ext4_file_read(file, buf, 4096, &f_pos)
       │
       │  Check inode->i_mapping (address_space)
       ▼
  ┌─────────────────────────────────────────────────────┐
  │              PAGE CACHE LOOKUP                      │
  │                                                     │
  │  page = find_get_page(mapping, index)               │
  │                                                     │
  │  index = f_pos / PAGE_SIZE = 0 / 4096 = page 0      │
  │                                                     │
  │  ┌─────────────────┐                                │
  │  │ Cache HIT?       │───── YES ──> copy_to_user()   │
  │  └────────┬────────┘              (fast path, done) │
  │           │ NO                                      │
  │           ▼                                         │
  │  page = page_cache_alloc()                          │
  │  add_to_page_cache(page, mapping, index)            │
  │  mapping->a_ops->readpage(file, page)               │
  │       │                                             │
  │       ▼                                             │
  │  ext4_readpage() → submit_bio() → disk I/O          │
  │       │                                             │
  │       ▼  (I/O completion)                           │
  │  copy_to_user(buf, page_data, 4096)                 │
  │  f_pos += 4096  (now f_pos = 4096)                  │
  └─────────────────────────────────────────────────────┘
       │
       ▼
  return 4096  (bytes read)
```

**The page cache is the secret to Linux file I/O performance.** Once data is
read from disk, it stays in the page cache. Subsequent reads of the same region
— by the same process or any other process — are served from RAM at memory speed.
This is why `cat`-ing a file the second time is dramatically faster than the first.

---

## 7. Summary — The VFS Design Philosophy

The Linux Virtual File System is a masterclass in **abstraction through indirection**.
Its design philosophy rests on a single principle: **separate the "what" from the
"how."** The "what" — opening a file, reading data, listing a directory — is
defined by four universal data structures (`super_block`, `inode`, `dentry`,
`file`) and their associated operation tables (structs of function pointers). The
"how" — reading an ext4 extent tree, fetching an NFS file over the network,
generating `/proc/meminfo` on the fly — is implemented by each filesystem behind
those function pointers. This separation means that adding a new filesystem to
Linux requires implementing a handful of well-defined callbacks, not modifying the
kernel's core I/O path. It also means that the kernel's caching infrastructure
(the dentry cache and the page cache) benefits *every* filesystem equally. The
result is an architecture where `/proc/cpuinfo`, a file on an NFS server, and a
local ext4 document are all accessed through the same `open()`/`read()`/`close()`
interface — a universal front desk that makes the kernel's filesystem jungle look
like a single, coherent library.

---

## Appendix: Quick Reference Card

```
  ┌──────────────────────────────────────────────────────────────┐
  │                  VFS QUICK REFERENCE                         │
  ├──────────────┬───────────────────────────────────────────────┤
  │ super_block  │ "What filesystem am I on?"                    │
  │              │ One per mount. Anchor for all FS objects.     │
  │              │ Key ptr: s_op (super_operations)              │
  ├──────────────┼───────────────────────────────────────────────┤
  │ inode        │ "What IS this file?" (identity, not name)     │
  │              │ One per file. Cached in icache.               │
  │              │ Key ptrs: i_op, i_fop, i_mapping              │
  ├──────────────┼───────────────────────────────────────────────┤
  │ dentry       │ "What is this file CALLED?"                   │
  │              │ One per path component. Cached in dcache.     │
  │              │ Key ptrs: d_inode, d_parent, d_subdirs        │
  ├──────────────┼───────────────────────────────────────────────┤
  │ file         │ "How is this file being USED right now?"      │
  │              │ One per open(). Per-process state.            │
  │              │ Key ptrs: f_op, f_path.dentry, f_pos          │
  ├──────────────┼───────────────────────────────────────────────┤
  │ page cache   │ "Is this data already in RAM?"                │
  │              │ Per-inode. Indexed by (mapping, page_offset). │
  │              │ Managed via: address_space, a_ops             │
  └──────────────┴───────────────────────────────────────────────┘
```

---

*Further reading:*
- `include/linux/fs.h` — All four pillar structures defined here
- `include/linux/dcache.h` — The `struct dentry` definition
- `fs/namei.c` — Path resolution (the pathwalk engine)
- `fs/open.c` — `sys_open()` and `do_filp_open()`
- `fs/read_write.c` — `sys_read()`, `vfs_read()`
- `mm/filemap.c` — Page cache core (`do_generic_file_read()`)
