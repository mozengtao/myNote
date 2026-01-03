# WHY｜为什么需要文件系统子系统

## 1. VFS 解决的问题

```
PROBLEMS SOLVED BY VFS (Virtual Filesystem Switch)
+=============================================================================+
|                                                                              |
|  THE FUNDAMENTAL ABSTRACTION: EVERYTHING IS A FILE                           |
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  User Space                                                              │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  Applications see a UNIFIED interface:                             │   │ |
|  │  │                                                                    │   │ |
|  │  │    open("/home/user/doc.txt", O_RDWR);                            │   │ |
|  │  │    read(fd, buffer, size);                                        │   │ |
|  │  │    write(fd, data, size);                                         │   │ |
|  │  │    close(fd);                                                     │   │ |
|  │  │                                                                    │   │ |
|  │  │  Same API for:                                                     │   │ |
|  │  │  • Regular files on disk                                          │   │ |
|  │  │  • Network sockets                                                 │   │ |
|  │  │  • Pipes and FIFOs                                                 │   │ |
|  │  │  • Device files (/dev/*)                                          │   │ |
|  │  │  • Pseudo-filesystems (/proc, /sys)                               │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                              │                                           │ |
|  │                              │ System Call                               │ |
|  │                              ▼                                           │ |
|  │  ════════════════════════════════════════════════════════════════════   │ |
|  │                                                                          │ |
|  │  Kernel Space                                                            │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │                    VFS LAYER (fs/)                                 │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  UNIFIED INTERFACE:                                         │   │   │ |
|  │  │  │  • vfs_read(), vfs_write(), vfs_open()                      │   │   │ |
|  │  │  │  • Pathname resolution                                      │   │   │ |
|  │  │  │  • Dentry/inode caching                                     │   │   │ |
|  │  │  │  • Permission checking                                      │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │         ┌────────────────────┼────────────────────┐                │   │ |
|  │  │         │                    │                    │                │   │ |
|  │  │         ▼                    ▼                    ▼                │   │ |
|  │  │  ┌────────────┐      ┌────────────┐      ┌────────────┐           │   │ |
|  │  │  │   ext4     │      │   NFS      │      │   procfs   │           │   │ |
|  │  │  │ filesystem │      │ filesystem │      │ filesystem │           │   │ |
|  │  │  └────────────┘      └────────────┘      └────────────┘           │   │ |
|  │  │         │                    │                    │                │   │ |
|  │  │         ▼                    ▼                    ▼                │   │ |
|  │  │     Local Disk           Network            Kernel Data            │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

VFS 解决的根本问题是**通过统一接口抽象不同类型的 I/O**：
- 用户空间看到的是统一的 API：`open()`、`read()`、`write()`、`close()`
- 同一 API 适用于：普通文件、网络套接字、管道、设备文件、伪文件系统
- VFS 层提供：统一接口、路径名解析、dentry/inode 缓存、权限检查
- 底层可以是完全不同的实现：ext4（本地磁盘）、NFS（网络）、procfs（内核数据）

---

```
PROBLEM 1: HETEROGENEOUS STORAGE ABSTRACTION (异构存储抽象)
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Without VFS abstraction:                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Application must know about each storage type:                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  if (storage == LOCAL_DISK) {                                    │    │ |
|  │  │      ext4_open(...);                                             │    │ |
|  │  │      ext4_read(...);                                             │    │ |
|  │  │  } else if (storage == NETWORK) {                                │    │ |
|  │  │      nfs_open(...);                                              │    │ |
|  │  │      nfs_read(...);                                              │    │ |
|  │  │  } else if (storage == USB) {                                    │    │ |
|  │  │      fat32_open(...);                                            │    │ |
|  │  │      fat32_read(...);                                            │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  NIGHTMARE: Every app needs to support every filesystem!         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  With VFS abstraction:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Application uses ONE interface:                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  fd = open(path, flags);      // VFS figures out the rest       │    │ |
|  │  │  read(fd, buf, size);         // Works for any filesystem       │    │ |
|  │  │  write(fd, buf, size);        // Transparent to app             │    │ |
|  │  │  close(fd);                                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  VFS handles:                                                    │    │ |
|  │  │  • Mapping path → filesystem type                                │    │ |
|  │  │  • Dispatching to correct filesystem driver                      │    │ |
|  │  │  • Caching for performance                                       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**问题 1：异构存储抽象**
- 没有 VFS：每个应用必须知道每种存储类型（ext4、NFS、FAT32...）
- 有 VFS：应用使用统一接口，VFS 处理路径映射、分发和缓存

---

```
PROBLEM 2: NAMESPACE UNIFICATION (命名空间统一)
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  THE SINGLE NAMESPACE TREE                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                         / (root)                                 │    │ |
|  │  │                            │                                     │    │ |
|  │  │    ┌───────────┬───────────┼───────────┬───────────┬─────────┐  │    │ |
|  │  │    │           │           │           │           │         │  │    │ |
|  │  │    ▼           ▼           ▼           ▼           ▼         ▼  │    │ |
|  │  │  /home       /dev        /proc       /sys       /mnt      /tmp  │    │ |
|  │  │  (ext4)     (devtmpfs)  (procfs)   (sysfs)    (various) (tmpfs)│    │ |
|  │  │    │           │           │           │           │            │    │ |
|  │  │    │           │           │           │           │            │    │ |
|  │  │  user/       sda        1234/       block/     usb/            │    │ |
|  │  │  data        null       status      sda        disk1           │    │ |
|  │  │              tty        maps        ...        ...             │    │ |
|  │  │                                                                  │    │ |
|  │  │  DIFFERENT FILESYSTEMS, SAME NAMESPACE!                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  VFS provides:                                                           │ |
|  │  • Mount mechanism: attach filesystem to directory                      │ |
|  │  • Path resolution: traverse across mount points                        │ |
|  │  • Unified navigation: cd, ls work everywhere                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**问题 2：命名空间统一**
- 单一命名空间树：`/`（根）下挂载不同文件系统
- `/home`（ext4）、`/dev`（devtmpfs）、`/proc`（procfs）、`/sys`（sysfs）
- 不同文件系统，相同命名空间！
- VFS 提供：挂载机制、路径解析、统一导航

---

```
PROBLEM 3: PERFORMANCE THROUGH CACHING (通过缓存提升性能)
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  VFS CACHING LAYERS                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Application: open("/home/user/project/src/main.c")              │    │ |
|  │  │                                                                  │    │ |
|  │  │  Without caching:                                                │    │ |
|  │  │  ─────────────────                                               │    │ |
|  │  │  Every path lookup → disk read for each component               │    │ |
|  │  │  /home → disk, user → disk, project → disk, src → disk, main.c  │    │ |
|  │  │  5 disk reads just to find the file!                             │    │ |
|  │  │                                                                  │    │ |
|  │  │  With VFS caching:                                               │    │ |
|  │  │  ──────────────────                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ DENTRY CACHE (dcache)                                       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ "home" → dentry → inode                                     │ │    │ |
|  │  │  │ "user" → dentry → inode     ◄── Name-to-inode mapping       │ │    │ |
|  │  │  │ "project" → dentry → inode                                  │ │    │ |
|  │  │  │ ...                                                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ Negative cache: "nonexistent" → ENOENT (avoid disk hit)     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ INODE CACHE (icache)                                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ inode 12345 → { size, mode, uid, gid, times, blocks }       │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ ◄── Metadata cached in memory                               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ PAGE CACHE                                                  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ inode 12345, page 0 → ████████ (4KB of file data)           │ │    │ |
|  │  │  │ inode 12345, page 1 → ████████                              │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ ◄── File contents cached in memory                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  Result: Most operations hit cache → 1000x faster than disk     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**问题 3：通过缓存提升性能**
- 没有缓存：每次路径查找都需要磁盘读取（5 个组件 = 5 次磁盘读取）
- VFS 缓存层：
  - **Dentry 缓存（dcache）**：名称到 inode 的映射，包括负缓存
  - **Inode 缓存（icache）**：元数据缓存在内存中
  - **Page 缓存**：文件内容缓存在内存中
- 结果：大多数操作命中缓存，比磁盘快 1000 倍

---

## 2. 为什么多个文件系统必须共存

```
WHY MULTIPLE FILESYSTEMS MUST COEXIST
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  DIFFERENT FILESYSTEMS FOR DIFFERENT PURPOSES                            │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Filesystem       Purpose                   Characteristics      │    │ |
|  │  │  ──────────       ───────                   ───────────────      │    │ |
|  │  │  ext4             General Linux storage     Journaling, fast     │    │ |
|  │  │  XFS              Large files, enterprise   Scalable, extent-based│   │ |
|  │  │  Btrfs            Modern features           COW, snapshots       │    │ |
|  │  │  FAT32            USB drives, compat        Simple, portable     │    │ |
|  │  │  NTFS             Windows compatibility     Journal, permissions │    │ |
|  │  │  NFS              Network storage           Remote access        │    │ |
|  │  │  tmpfs            RAM-backed temp           Fast, volatile       │    │ |
|  │  │  procfs           Process information       Virtual, read-only   │    │ |
|  │  │  sysfs            Device/driver info        Virtual, hierarchical│    │ |
|  │  │  devtmpfs         Device nodes              Auto-populated       │    │ |
|  │  │  overlayfs        Container layers          Union mount          │    │ |
|  │  │  fuse             Userspace filesystems     Extensible           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ONE SYSTEM, MANY FILESYSTEMS:                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Typical Linux system mounts:                                    │    │ |
|  │  │                                                                  │    │ |
|  │  │  /           ext4      (root filesystem)                         │    │ |
|  │  │  /boot       ext4      (bootloader)                              │    │ |
|  │  │  /home       ext4      (user data)                               │    │ |
|  │  │  /tmp        tmpfs     (temp files in RAM)                       │    │ |
|  │  │  /dev        devtmpfs  (device nodes)                            │    │ |
|  │  │  /proc       procfs    (process info)                            │    │ |
|  │  │  /sys        sysfs     (device/driver info)                      │    │ |
|  │  │  /run        tmpfs     (runtime data)                            │    │ |
|  │  │  /mnt/usb    vfat      (USB drive)                               │    │ |
|  │  │  /mnt/nas    nfs       (network storage)                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  10+ different filesystems on one system!                        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**为什么多个文件系统必须共存**：

不同文件系统用于不同目的：
- **ext4**：通用 Linux 存储，日志，快速
- **XFS**：大文件，企业级，可扩展
- **tmpfs**：RAM 支持的临时存储，快速但易失
- **procfs/sysfs**：虚拟文件系统，暴露内核信息
- **NFS**：网络存储
- **overlayfs**：容器分层

典型 Linux 系统同时挂载 10+ 种不同文件系统！

---

## 3. 复杂度驱动因素

```
COMPLEXITY DRIVERS IN VFS
+=============================================================================+
|                                                                              |
|  DRIVER 1: EXTENSIBILITY (可扩展性)                              Priority: ★★★★★|
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  VFS must support filesystems that don't exist yet:                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1991: Linux 0.01 - only Minix filesystem                        │    │ |
|  │  │  1993: ext2 added                                                │    │ |
|  │  │  2001: ext3 (journaling)                                         │    │ |
|  │  │  2006: ext4                                                      │    │ |
|  │  │  2009: Btrfs                                                     │    │ |
|  │  │  2016: overlayfs (containers)                                    │    │ |
|  │  │  202x: ??? (future filesystems)                                  │    │ |
|  │  │                                                                  │    │ |
|  │  │  VFS design allows adding new filesystems without changing VFS!  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  MECHANISM: OPS-TABLE POLYMORPHISM                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  struct file_operations {          /* VFS defines interface */   │    │ |
|  │  │      ssize_t (*read)(...);                                       │    │ |
|  │  │      ssize_t (*write)(...);                                      │    │ |
|  │  │      int (*open)(...);                                           │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  };                                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Each filesystem provides implementation */                   │    │ |
|  │  │  const struct file_operations ext4_file_ops = { ... };           │    │ |
|  │  │  const struct file_operations nfs_file_ops = { ... };            │    │ |
|  │  │  const struct file_operations fuse_file_ops = { ... };           │    │ |
|  │  │                                                                  │    │ |
|  │  │  VFS calls: file->f_op->read(file, buf, count, pos);             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  DRIVER 2: CORRECTNESS (正确性)                                  Priority: ★★★★★|
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Filesystem corruption = DATA LOSS (the worst bug)                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  CORRECTNESS CHALLENGES:                                         │    │ |
|  │  │                                                                  │    │ |
|  │  │  1. Crash consistency                                            │    │ |
|  │  │     • Power failure mid-write → filesystem must recover         │    │ |
|  │  │     • Solution: journaling, COW, soft updates                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. Concurrent access                                            │    │ |
|  │  │     • Multiple processes writing same file                       │    │ |
|  │  │     • Solution: locking (inode lock, dentry lock)               │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. Cache coherency                                              │    │ |
|  │  │     • Memory and disk must stay in sync                          │    │ |
|  │  │     • Solution: careful writeback, fsync()                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. Namespace consistency                                        │    │ |
|  │  │     • Rename must be atomic                                      │    │ |
|  │  │     • Delete while file is open                                  │    │ |
|  │  │     • Solution: reference counting, careful ordering             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  FSCK exists because crashes WILL happen:                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  $ fsck /dev/sda1                                                │    │ |
|  │  │  Checking for orphaned inodes...                                 │    │ |
|  │  │  Checking directory connectivity...                              │    │ |
|  │  │  Checking reference counts...                                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**复杂度驱动因素**：

**驱动因素 1：可扩展性**（★★★★★）
- VFS 必须支持尚不存在的文件系统
- 从 1991 年只有 Minix 到现在的 ext4、Btrfs、overlayfs...
- 机制：操作表多态（`file_operations`、`inode_operations` 等）
- 添加新文件系统无需修改 VFS

**驱动因素 2：正确性**（★★★★★）
- 文件系统损坏 = 数据丢失（最严重的 bug）
- 挑战：
  - 崩溃一致性：断电中途写入必须能恢复（日志、COW）
  - 并发访问：多进程写同一文件（锁）
  - 缓存一致性：内存和磁盘必须同步（写回、fsync）
  - 命名空间一致性：重命名必须原子（引用计数）

---

## 4. 历史 UNIX 文件系统模型

```
HISTORICAL UNIX FILESYSTEM MODEL
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  UNIX DESIGN PRINCIPLES (1970s)                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. "EVERYTHING IS A FILE"                                       │    │ |
|  │  │     • Devices → files (/dev/tty, /dev/disk)                      │    │ |
|  │  │     • Pipes → files                                              │    │ |
|  │  │     • Sockets → files (later addition)                           │    │ |
|  │  │     • Directories → special files                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. FILE = DATA + METADATA (INODE)                               │    │ |
|  │  │     • Data: bytes (unstructured)                                 │    │ |
|  │  │     • Inode: owner, permissions, size, timestamps, block map     │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. NAMES ARE SEPARATE FROM FILES                                │    │ |
|  │  │     • Multiple names can point to same file (hard links)         │    │ |
|  │  │     • Directory = list of (name, inode number) pairs            │    │ |
|  │  │                                                                  │    │ |
|  │  │  4. HIERARCHICAL NAMESPACE                                       │    │ |
|  │  │     • Single root (/)                                            │    │ |
|  │  │     • Directories can contain files and other directories        │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  CLASSIC UNIX FILESYSTEM STRUCTURE                                           |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ SUPERBLOCK                                                  │ │    │ |
|  │  │  │ • Filesystem size                                           │ │    │ |
|  │  │  │ • Number of inodes                                          │ │    │ |
|  │  │  │ • Free block count                                          │ │    │ |
|  │  │  │ • Filesystem type                                           │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ INODE TABLE                                                 │ │    │ |
|  │  │  │ ┌─────────────────────────────────────────────────────────┐│ │    │ |
|  │  │  │ │ Inode 0: (unused)                                        ││ │    │ |
|  │  │  │ │ Inode 1: (bad blocks)                                    ││ │    │ |
|  │  │  │ │ Inode 2: root directory                                  ││ │    │ |
|  │  │  │ │ Inode 3: { mode=0644, uid=1000, size=1234, blocks=[..] } ││ │    │ |
|  │  │  │ │ ...                                                      ││ │    │ |
|  │  │  │ └─────────────────────────────────────────────────────────┘│ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ DATA BLOCKS                                                 │ │    │ |
|  │  │  │ ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐        │ │    │ |
|  │  │  │ │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │ 8  │... │        │ │    │ |
|  │  │  │ │dir │data│data│free│data│free│dir │data│free│    │        │ │    │ |
|  │  │  │ └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘        │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  UNIX TO LINUX VFS EVOLUTION                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  1970s UNIX:                                                             │ |
|  │  • One filesystem type (usually UFS variant)                            │ |
|  │  • Direct inode manipulation                                            │ |
|  │  • Simple but inflexible                                                │ |
|  │                                                                          │ |
|  │  1990s Sun VFS:                                                          │ |
|  │  • First VFS abstraction in SunOS                                       │ |
|  │  • vnode/vfs interface                                                  │ |
|  │  • Multiple filesystem support                                          │ |
|  │                                                                          │ |
|  │  Linux VFS:                                                              │ |
|  │  • Inspired by Sun VFS                                                  │ |
|  │  • Added dentry cache (Linux innovation)                                │ |
|  │  • Highly optimized for caching                                         │ |
|  │  • Extensive ops-table design                                           │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**UNIX 设计原则（1970 年代）**：
1. **"一切皆文件"**：设备、管道、套接字都是文件
2. **文件 = 数据 + 元数据（inode）**：数据是无结构字节，inode 包含所有者、权限、大小等
3. **名称与文件分离**：多个名称可指向同一文件（硬链接），目录是（名称，inode 号）对列表
4. **层次命名空间**：单一根目录 `/`

**经典 UNIX 文件系统结构**：
- **超级块**：文件系统大小、inode 数量、空闲块数
- **Inode 表**：每个 inode 包含元数据
- **数据块**：实际文件内容

**UNIX 到 Linux VFS 演进**：
- 1970s UNIX：一种文件系统类型，直接 inode 操作
- 1990s Sun VFS：首个 VFS 抽象，vnode/vfs 接口
- Linux VFS：受 Sun VFS 启发，添加 dentry 缓存（Linux 创新），高度优化的缓存
