# HOW｜架构策略

## 1. VFS 如何解耦策略与实现

```
VFS DECOUPLING: POLICY VS IMPLEMENTATION
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  THE SEPARATION PRINCIPLE                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  VFS LAYER (Policy - WHAT to do)                          │   │    │ |
|  │  │  │  ─────────────────────────────────                        │   │    │ |
|  │  │  │  • Defines interfaces (file_operations, inode_operations)  │   │    │ |
|  │  │  │  • Manages namespace (dentry tree)                         │   │    │ |
|  │  │  │  • Enforces permissions (VFS-level checks)                 │   │    │ |
|  │  │  │  • Provides caching (dcache, icache, page cache)           │   │    │ |
|  │  │  │  • Coordinates lifecycle (mount, umount, sync)             │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  VFS DOES NOT KNOW:                                        │   │    │ |
|  │  │  │  • How data is stored on disk                              │   │    │ |
|  │  │  │  • Disk layout or block allocation                         │   │    │ |
|  │  │  │  • Network protocols (for NFS)                             │   │    │ |
|  │  │  │  • Compression, encryption                                 │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              │ ops->read()                       │    │ |
|  │  │                              │ ops->write()                      │    │ |
|  │  │                              │ ops->lookup()                     │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  ┌──────────────────────────────────────────────────────────┐   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  FILESYSTEM LAYER (Implementation - HOW to do it)          │   │    │ |
|  │  │  │  ───────────────────────────────────────────────           │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  │  ext4:        NFS:          procfs:                        │   │    │ |
|  │  │  │  ┌────────┐   ┌────────┐   ┌────────┐                      │   │    │ |
|  │  │  │  │ Extent │   │ RPC    │   │ Generate│                      │   │    │ |
|  │  │  │  │ trees  │   │ calls  │   │ from    │                      │   │    │ |
|  │  │  │  │ Journal│   │ Client │   │ kernel  │                      │   │    │ |
|  │  │  │  │ Blocks │   │ cache  │   │ data    │                      │   │    │ |
|  │  │  │  └────────┘   └────────┘   └────────┘                      │   │    │ |
|  │  │  │                                                           │   │    │ |
|  │  │  └──────────────────────────────────────────────────────────┘   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**分离原则**：

**VFS 层（策略层 - 做什么）**：
- 定义接口：`file_operations`、`inode_operations`
- 管理命名空间：dentry 树
- 强制权限：VFS 级别检查
- 提供缓存：dcache、icache、page cache
- 协调生命周期：mount、umount、sync

**VFS 不知道**：数据如何存储、磁盘布局、网络协议、压缩/加密

**文件系统层（实现层 - 怎么做）**：
- ext4：extent 树、日志、块
- NFS：RPC 调用、客户端缓存
- procfs：从内核数据生成

---

```
OPS-TABLE POLYMORPHISM MECHANISM
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  HOW VFS DISPATCHES TO FILESYSTEM                                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  User calls: read(fd, buf, 100)                                  │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐  │    │ |
|  │  │  │ sys_read(fd, buf, count)                                   │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │ struct file *file = fget(fd);   // Get file from fd        │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │ // file->f_op points to filesystem's file_operations       │  │    │ |
|  │  │  │ // This is set during open() by the filesystem             │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │ return vfs_read(file, buf, count, &file->f_pos);           │  │    │ |
|  │  │  └───────────────────────────────────────────────────────────┘  │    │ |
|  │  │                              │                                   │    │ |
|  │  │                              ▼                                   │    │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐  │    │ |
|  │  │  │ vfs_read(file, buf, count, pos)                            │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │ // Check permissions (VFS policy)                          │  │    │ |
|  │  │  │ ret = rw_verify_area(READ, file, pos, count);              │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  │ // Dispatch to filesystem implementation                   │  │    │ |
|  │  │  │ if (file->f_op->read)                                      │  │    │ |
|  │  │  │     return file->f_op->read(file, buf, count, pos);        │  │    │ |
|  │  │  │ else if (file->f_op->aio_read)                             │  │    │ |
|  │  │  │     return do_sync_read(file, buf, count, pos);            │  │    │ |
|  │  │  │                                                            │  │    │ |
|  │  │  └───────────────────────────────────────────────────────────┘  │    │ |
|  │  │                              │                                   │    │ |
|  │  │       ┌──────────────────────┼──────────────────────┐            │    │ |
|  │  │       │                      │                      │            │    │ |
|  │  │       ▼                      ▼                      ▼            │    │ |
|  │  │  ┌──────────┐         ┌──────────┐         ┌──────────┐         │    │ |
|  │  │  │ext4_file │         │nfs_file  │         │proc_file │         │    │ |
|  │  │  │_read()   │         │_read()   │         │_read()   │         │    │ |
|  │  │  │          │         │          │         │          │         │    │ |
|  │  │  │ Read from│         │ RPC to   │         │ Generate │         │    │ |
|  │  │  │ disk via │         │ server   │         │ from     │         │    │ |
|  │  │  │ page     │         │          │         │ kernel   │         │    │ |
|  │  │  │ cache    │         │          │         │ data     │         │    │ |
|  │  │  └──────────┘         └──────────┘         └──────────┘         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**操作表多态机制**：

1. 用户调用 `read(fd, buf, 100)`
2. `sys_read()` 获取 file 结构，调用 `vfs_read()`
3. `vfs_read()` 检查权限（VFS 策略），然后调用 `file->f_op->read()`
4. 根据文件所在文件系统，分发到：
   - `ext4_file_read()`：通过 page cache 从磁盘读取
   - `nfs_file_read()`：RPC 到服务器
   - `proc_file_read()`：从内核数据生成

---

## 2. 如何分离命名、缓存和持久化

```
SEPARATION: NAMING, CACHING, AND PERSISTENCE
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  THREE ORTHOGONAL CONCERNS                                               │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  PATH: /home/user/file.txt                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │ NAMING (dentry layer)                                       ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  "/"    →    dentry(/)                                      ││    │ |
|  │  │  │              │ d_parent = NULL                               ││    │ |
|  │  │  │              │ d_name = "/"                                  ││    │ |
|  │  │  │              │ d_inode → inode(root)                         ││    │ |
|  │  │  │              │                                               ││    │ |
|  │  │  │  "home" →    dentry(home)                                   ││    │ |
|  │  │  │              │ d_parent = dentry(/)                          ││    │ |
|  │  │  │              │ d_name = "home"                               ││    │ |
|  │  │  │              │ d_inode → inode(home_dir)                     ││    │ |
|  │  │  │              │                                               ││    │ |
|  │  │  │  "user" →    dentry(user)                                   ││    │ |
|  │  │  │              │ d_parent = dentry(home)                       ││    │ |
|  │  │  │              │ d_name = "user"                               ││    │ |
|  │  │  │              │ d_inode → inode(user_dir)                     ││    │ |
|  │  │  │              │                                               ││    │ |
|  │  │  │  "file.txt"→ dentry(file.txt)                               ││    │ |
|  │  │  │              │ d_parent = dentry(user)                       ││    │ |
|  │  │  │              │ d_name = "file.txt"                           ││    │ |
|  │  │  │              │ d_inode → inode(file)                         ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  DENTRY RESPONSIBILITY:                                      ││    │ |
|  │  │  │  • Name-to-inode mapping                                     ││    │ |
|  │  │  │  • Directory tree structure                                  ││    │ |
|  │  │  │  • Negative entries (name does not exist)                    ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ CACHING (VFS-managed caches)                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  DENTRY CACHE (dcache)                                      ││    │ |
|  │  │  │  • LRU-managed cache of dentries                            ││    │ |
|  │  │  │  • Hash table for fast lookup                               ││    │ |
|  │  │  │  • Includes negative dentries                               ││    │ |
|  │  │  │  • Avoids repeated path lookups                             ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  lookup("/home/user/file.txt") → dcache hit → instant!      ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  INODE CACHE (icache)                                       ││    │ |
|  │  │  │  • In-memory copy of inode metadata                         ││    │ |
|  │  │  │  • Reference counted                                        ││    │ |
|  │  │  │  • Dirty tracking for writeback                             ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  stat("/home/user/file.txt") → icache hit → no disk I/O     ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  PAGE CACHE                                                 ││    │ |
|  │  │  │  • File data cached in memory pages                         ││    │ |
|  │  │  │  • Indexed by (inode, offset)                               ││    │ |
|  │  │  │  • Writeback to disk on memory pressure or sync             ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  read(fd, buf, 4096) → page cache hit → memory copy only    ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │ PERSISTENCE (filesystem-specific)                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐│    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  Each filesystem decides HOW to persist:                    ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  ext4:                                                      ││    │ |
|  │  │  │  • Extent-based block allocation                            ││    │ |
|  │  │  │  • Journal for crash consistency                            ││    │ |
|  │  │  │  • Direct/indirect block pointers                           ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  Btrfs:                                                     ││    │ |
|  │  │  │  • Copy-on-write                                            ││    │ |
|  │  │  │  • B-tree for all metadata                                  ││    │ |
|  │  │  │  • Integrated checksumming                                  ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  NFS:                                                       ││    │ |
|  │  │  │  • RPC to remote server                                     ││    │ |
|  │  │  │  • Server handles actual persistence                        ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  tmpfs:                                                     ││    │ |
|  │  │  │  • NO persistence (RAM only)                                ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  │  procfs:                                                    ││    │ |
|  │  │  │  • NO persistence (generated on read)                       ││    │ |
|  │  │  │                                                             ││    │ |
|  │  │  └─────────────────────────────────────────────────────────────┘│    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**三个正交关注点**：

**命名（dentry 层）**：
- 名称到 inode 的映射
- 目录树结构（d_parent、d_name、d_inode）
- 负条目（名称不存在）

**缓存（VFS 管理的缓存）**：
- **Dentry 缓存（dcache）**：LRU 管理，哈希表快速查找，包含负 dentry
- **Inode 缓存（icache）**：内存中的 inode 元数据副本，引用计数，脏跟踪
- **Page 缓存**：文件数据缓存在内存页中，按（inode, offset）索引

**持久化（文件系统特定）**：
- **ext4**：基于 extent 的块分配，日志
- **Btrfs**：COW，B-tree，集成校验和
- **NFS**：RPC 到远程服务器
- **tmpfs**：无持久化（仅 RAM）
- **procfs**：无持久化（读取时生成）

---

## 3. 生命周期管理：inode 和 dentry

```
LIFECYCLE MANAGEMENT: INODE
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  INODE LIFECYCLE STATE MACHINE                                           │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │           ┌─────────────┐                                        │    │ |
|  │  │           │   CREATE    │ (via creat(), mkdir(), mknod())        │    │ |
|  │  │           └──────┬──────┘                                        │    │ |
|  │  │                  │ fs->new_inode()                               │    │ |
|  │  │                  │ inode->i_count = 1                            │    │ |
|  │  │                  ▼                                               │    │ |
|  │  │           ┌─────────────┐                                        │    │ |
|  │  │           │   ACTIVE    │ ◄─────────────────────────────┐        │    │ |
|  │  │           │             │                               │        │    │ |
|  │  │           │ i_count > 0 │ (in use by files/references)  │        │    │ |
|  │  │           │             │                               │        │    │ |
|  │  │           └──────┬──────┘                               │        │    │ |
|  │  │                  │                                      │        │    │ |
|  │  │    ┌─────────────┴─────────────┐                        │        │    │ |
|  │  │    │                           │                        │        │    │ |
|  │  │    ▼ close() / iput()          ▼ open() / iget()        │        │    │ |
|  │  │    i_count--                   i_count++                │        │    │ |
|  │  │    │                                                    │        │    │ |
|  │  │    │ if i_count == 0                                    │        │    │ |
|  │  │    ▼                                                    │        │    │ |
|  │  │  ┌─────────────┐                                        │        │    │ |
|  │  │  │   CACHED    │  (in icache, but no active refs)       │        │    │ |
|  │  │  │             │────────────────────────────────────────┘        │    │ |
|  │  │  │ i_count = 0 │  re-opened → back to ACTIVE                     │    │ |
|  │  │  │             │                                                 │    │ |
|  │  │  └──────┬──────┘                                                 │    │ |
|  │  │         │                                                        │    │ |
|  │  │         │ memory pressure or unlink (i_nlink == 0)               │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  ┌─────────────┐                                                 │    │ |
|  │  │  │   EVICT     │                                                 │    │ |
|  │  │  │             │                                                 │    │ |
|  │  │  │ evict_inode()│                                                │    │ |
|  │  │  │ • Writeback if dirty                                          │    │ |
|  │  │  │ • Free disk blocks if i_nlink == 0                            │    │ |
|  │  │  │ • Release memory                                              │    │ |
|  │  │  │             │                                                 │    │ |
|  │  │  └──────┬──────┘                                                 │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │  ┌─────────────┐                                                 │    │ |
|  │  │  │    FREE     │                                                 │    │ |
|  │  │  └─────────────┘                                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  KEY COUNTERS:                                                   │    │ |
|  │  │  • i_count: in-memory references (struct inode usage)            │    │ |
|  │  │  • i_nlink: on-disk links (hard link count)                      │    │ |
|  │  │                                                                  │    │ |
|  │  │  DELETION LOGIC:                                                 │    │ |
|  │  │  • unlink() decrements i_nlink                                   │    │ |
|  │  │  • Actual deletion when i_nlink == 0 AND i_count == 0            │    │ |
|  │  │  • Allows "delete while open" pattern                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**Inode 生命周期状态机**：

1. **CREATE**：通过 `creat()`、`mkdir()`、`mknod()` 创建
2. **ACTIVE**：`i_count > 0`，被文件/引用使用中
3. **CACHED**：`i_count == 0`，在 icache 中但无活动引用
4. **EVICT**：内存压力或 unlink（`i_nlink == 0`）
5. **FREE**：释放

**关键计数器**：
- `i_count`：内存中引用（struct inode 使用）
- `i_nlink`：磁盘上链接（硬链接计数）

**删除逻辑**：
- `unlink()` 减少 `i_nlink`
- 实际删除：当 `i_nlink == 0` 且 `i_count == 0`
- 允许"打开时删除"模式

---

```
LIFECYCLE MANAGEMENT: DENTRY
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  DENTRY LIFECYCLE STATE MACHINE                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │           ┌─────────────┐                                        │    │ |
|  │  │           │   LOOKUP    │ (path resolution)                      │    │ |
|  │  │           └──────┬──────┘                                        │    │ |
|  │  │                  │                                               │    │ |
|  │  │     ┌────────────┴────────────┐                                  │    │ |
|  │  │     │                         │                                  │    │ |
|  │  │     ▼ found                   ▼ not found                        │    │ |
|  │  │  ┌─────────────┐         ┌─────────────┐                         │    │ |
|  │  │  │  POSITIVE   │         │  NEGATIVE   │                         │    │ |
|  │  │  │             │         │             │                         │    │ |
|  │  │  │ d_inode !=  │         │ d_inode ==  │                         │    │ |
|  │  │  │ NULL        │         │ NULL        │                         │    │ |
|  │  │  │             │         │             │                         │    │ |
|  │  │  │ Points to   │         │ "Name does  │                         │    │ |
|  │  │  │ valid inode │         │ not exist"  │                         │    │ |
|  │  │  │             │         │             │                         │    │ |
|  │  │  └──────┬──────┘         └──────┬──────┘                         │    │ |
|  │  │         │                       │                                │    │ |
|  │  │         │ d_count--             │ d_count--                      │    │ |
|  │  │         │ (usage done)          │ (or name created)              │    │ |
|  │  │         ▼                       ▼                                │    │ |
|  │  │  ┌─────────────────────────────────────────────┐                 │    │ |
|  │  │  │               UNUSED (d_count == 0)          │                 │    │ |
|  │  │  │                                              │                 │    │ |
|  │  │  │  On LRU list, may be evicted on memory       │                 │    │ |
|  │  │  │  pressure, or reused if path accessed again  │                 │    │ |
|  │  │  │                                              │                 │    │ |
|  │  │  └───────────────────────┬─────────────────────┘                 │    │ |
|  │  │                          │                                       │    │ |
|  │  │    ┌─────────────────────┼─────────────────────┐                 │    │ |
|  │  │    │                     │                     │                 │    │ |
|  │  │    ▼ reused              ▼ evicted             ▼ revalidate      │    │ |
|  │  │  back to                ┌─────────────┐       (NFS etc.)         │    │ |
|  │  │  POSITIVE/              │    FREE     │       re-lookup from     │    │ |
|  │  │  NEGATIVE               └─────────────┘       server             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  DCACHE ORGANIZATION:                                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Hash Table (for fast lookup):                                   │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ hash(parent_dentry, name) → dentry                          │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ bucket[0] → dentry("bin") → dentry("boot") → ...            │ │    │ |
|  │  │  │ bucket[1] → dentry("home") → ...                            │ │    │ |
|  │  │  │ ...                                                         │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  LRU List (for eviction):                                        │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ MRU → dentry → dentry → dentry → ... → dentry → LRU        │ │    │ |
|  │  │  │ (most recent)                           (least recent)      │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │ Memory pressure → evict from LRU end                        │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**Dentry 生命周期状态机**：

1. **LOOKUP**：路径解析
2. **POSITIVE**：`d_inode != NULL`，指向有效 inode
3. **NEGATIVE**：`d_inode == NULL`，"名称不存在"
4. **UNUSED**：`d_count == 0`，在 LRU 列表上
5. **FREE**：被驱逐

**Dcache 组织**：
- **哈希表**：快速查找，`hash(parent_dentry, name) → dentry`
- **LRU 列表**：驱逐策略，内存压力时从 LRU 端驱逐

---

## 4. 如何强制一致性

```
CONSISTENCY ENFORCEMENT
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  CONSISTENCY LAYERS                                                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  LAYER 1: VFS LOCKING (Namespace Consistency)                    │    │ |
|  │  │  ─────────────────────────────────────────                       │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  i_mutex (inode->i_mutex):                                  │ │    │ |
|  │  │  │  • Protects directory operations                            │ │    │ |
|  │  │  │  • Serializes create/unlink/rename in same directory        │ │    │ |
|  │  │  │  • Held during file write (prevents concurrent truncate)    │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  d_lock (dentry->d_lock):                                   │ │    │ |
|  │  │  │  • Protects dentry fields                                   │ │    │ |
|  │  │  │  • Short-term spinlock for d_count, d_flags                 │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  rename_lock:                                               │ │    │ |
|  │  │  │  • Global seqlock for rename operations                     │ │    │ |
|  │  │  │  • Ensures atomic rename across directories                 │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  LAYER 2: PAGE CACHE CONSISTENCY (Memory vs Disk)                │    │ |
|  │  │  ───────────────────────────────────────────────                 │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Write path:                                                │ │    │ |
|  │  │  │  1. Write to page cache (mark page dirty)                   │ │    │ |
|  │  │  │  2. Return to user (data in memory only)                    │ │    │ |
|  │  │  │  3. Background writeback OR fsync()                         │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  ┌────────────────────────────────────────────────────┐     │ │    │ |
|  │  │  │  │ User write → [Page Cache] ───writeback───→ [Disk]  │     │ │    │ |
|  │  │  │  │                   │                                │     │ │    │ |
|  │  │  │  │            dirty pages                             │     │ │    │ |
|  │  │  │  │                   │                                │     │ │    │ |
|  │  │  │  │              pdflush/                              │     │ │    │ |
|  │  │  │  │             bdi-flusher                            │     │ │    │ |
|  │  │  │  └────────────────────────────────────────────────────┘     │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  fsync() semantics:                                         │ │    │ |
|  │  │  │  • Flush dirty pages to disk                                │ │    │ |
|  │  │  │  • Wait for I/O completion                                  │ │    │ |
|  │  │  │  • Durability guarantee after return                        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  LAYER 3: FILESYSTEM CONSISTENCY (Crash Recovery)                │    │ |
|  │  │  ──────────────────────────────────────────────                  │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Journaling (ext3/ext4):                                    │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  │  Transaction:                                         │  │ │    │ |
|  │  │  │  │  1. Write changes to JOURNAL first                    │  │ │    │ |
|  │  │  │  │  2. Write commit record                               │  │ │    │ |
|  │  │  │  │  3. Write actual data to filesystem                   │  │ │    │ |
|  │  │  │  │  4. Mark transaction complete                         │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  │  Crash recovery:                                      │  │ │    │ |
|  │  │  │  │  • Replay committed but incomplete transactions       │  │ │    │ |
|  │  │  │  │  • Discard uncommitted transactions                   │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Copy-on-Write (Btrfs, ZFS):                                │ │    │ |
|  │  │  │  ┌──────────────────────────────────────────────────────┐  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  │  Never overwrite in place:                            │  │ │    │ |
|  │  │  │  │  1. Write new data to new location                    │  │ │    │ |
|  │  │  │  │  2. Update parent pointers atomically                 │  │ │    │ |
|  │  │  │  │  3. Old data remains until new tree committed         │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  │  Crash recovery:                                      │  │ │    │ |
|  │  │  │  │  • Old tree always consistent                         │  │ │    │ |
|  │  │  │  │  • Incomplete writes simply orphaned                  │  │ │    │ |
|  │  │  │  │                                                       │  │ │    │ |
|  │  │  │  └──────────────────────────────────────────────────────┘  │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**一致性强制层次**：

**层次 1：VFS 锁定（命名空间一致性）**
- `i_mutex`：保护目录操作，序列化同目录的 create/unlink/rename
- `d_lock`：保护 dentry 字段，短期自旋锁
- `rename_lock`：全局 seqlock，确保跨目录重命名原子性

**层次 2：Page Cache 一致性（内存 vs 磁盘）**
- 写入路径：写入 page cache（标记脏）→ 返回用户 → 后台写回或 `fsync()`
- `fsync()` 语义：刷新脏页到磁盘，等待 I/O 完成，返回后保证持久性

**层次 3：文件系统一致性（崩溃恢复）**
- **日志（ext3/ext4）**：先写日志 → 提交记录 → 写实际数据 → 标记完成
- **写时复制（Btrfs）**：从不原地覆盖，新数据写新位置，原子更新父指针

---

```
ATOMIC OPERATIONS AND ORDERING
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ATOMIC RENAME GUARANTEE                                                 │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  rename("/tmp/file.new", "/tmp/file")                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  POSIX GUARANTEE:                                                │    │ |
|  │  │  • Either old name or new name visible, never neither, never both│    │ |
|  │  │  • Atomicity even across power failure (with journaling)         │    │ |
|  │  │                                                                  │    │ |
|  │  │  Implementation (simplified):                                    │    │ |
|  │  │  1. Lock both source and target directories                      │    │ |
|  │  │  2. Journal the operation (if journaling fs)                     │    │ |
|  │  │  3. Update directory entries atomically                          │    │ |
|  │  │  4. Unlock                                                       │    │ |
|  │  │                                                                  │    │ |
|  │  │  USE CASE: Safe config file update                               │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │ 1. write(fd_new, new_config);   // Write to temp file      │ │    │ |
|  │  │  │ 2. fsync(fd_new);               // Ensure durability       │ │    │ |
|  │  │  │ 3. rename("config.new", "config"); // Atomic replace       │ │    │ |
|  │  │  │ 4. fsync(dir_fd);               // Ensure rename durable   │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ORDERING REQUIREMENTS                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  File creation ordering:                                         │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  creat("newfile") must ensure:                              │ │    │ |
|  │  │  │  1. Inode allocated and initialized                         │ │    │ |
|  │  │  │  2. Directory entry created pointing to inode               │ │    │ |
|  │  │  │  3. Both on disk before returning (if O_SYNC)               │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Without ordering: crash could leave orphan inode or        │ │    │ |
|  │  │  │  dangling directory entry                                   │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  │  File deletion ordering:                                         │    │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  unlink("file") must ensure:                                │ │    │ |
|  │  │  │  1. Directory entry removed                                 │ │    │ |
|  │  │  │  2. i_nlink decremented                                     │ │    │ |
|  │  │  │  3. If i_nlink == 0 and i_count == 0: free blocks           │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  │  Ordering: entry removal must commit before block free      │ │    │ |
|  │  │  │  (prevents reusing blocks while entry still visible)        │ │    │ |
|  │  │  │                                                             │ │    │ |
|  │  │  └────────────────────────────────────────────────────────────┘ │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**原子重命名保证**：
- `rename()` 保证：要么看到旧名要么看到新名，绝不会两者都没有或都有
- 实现：锁定两个目录 → 日志操作 → 原子更新目录条目 → 解锁
- 用例：安全配置文件更新（写入临时文件 → fsync → rename → fsync 目录）

**排序要求**：
- **文件创建**：先分配 inode，再创建目录条目
- **文件删除**：先移除目录条目，再减少 i_nlink，最后释放块
- 排序保证：条目移除必须在块释放之前提交
