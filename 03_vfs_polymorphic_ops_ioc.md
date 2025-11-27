# VFS 多态 ops 中的依赖注入 (IoC) 模式

## 概述

Linux VFS (Virtual File System) 是面向对象设计在 C 语言中的经典实现。通过函数指针结构体 (`file_operations`, `inode_operations` 等)，VFS 将文件系统的具体实现与通用接口解耦。

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         VFS 多态 ops IoC 架构                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   用户空间                                                                   │
│   ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐                          │
│   │ open()  │ │ read()  │ │ write() │ │ close() │                          │
│   └────┬────┘ └────┬────┘ └────┬────┘ └────┬────┘                          │
│        │           │           │           │                                 │
│        └───────────┴───────────┴───────────┘                                 │
│                          │                                                   │
│  ════════════════════════╪═══════════════════════════════════════════════   │
│                          │  系统调用                                         │
│                          ▼                                                   │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                        VFS 层 (抽象层)                               │   │
│   │                                                                      │   │
│   │   sys_read(fd, buf, count)                                          │   │
│   │       │                                                              │   │
│   │       ▼                                                              │   │
│   │   struct file *f = fget(fd);                                        │   │
│   │   f->f_op->read(f, buf, count, &f->f_pos);  ◄── 调用注入的函数      │   │
│   │                                                                      │   │
│   └─────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│                                   │                                          │
│                                   │  f_op 指向具体实现                       │
│           ┌───────────────────────┼───────────────────────┐                 │
│           │                       │                       │                  │
│           ▼                       ▼                       ▼                  │
│   ┌───────────────┐       ┌───────────────┐       ┌───────────────┐         │
│   │   ext4_fops   │       │   nfs_fops    │       │  proc_fops    │         │
│   │               │       │               │       │               │         │
│   │ .read = ext4_ │       │ .read = nfs_  │       │ .read = proc_ │         │
│   │         read  │       │        read   │       │        read   │         │
│   │ .write = ext4_│       │ .write = nfs_ │       │ .write = proc_│         │
│   │         write │       │        write  │       │        write  │         │
│   └───────────────┘       └───────────────┘       └───────────────┘         │
│                                                                              │
│   控制反转体现:                                                              │
│   - VFS 不知道具体的读写实现                                                 │
│   - 具体实现通过 file->f_op 动态绑定                                         │
│   - 打开文件时由文件系统注入 ops                                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 核心代码片段

### 1. file_operations 结构 - 文件操作的"接口契约"

```c
// include/linux/fs.h

struct file_operations {
    struct module *owner;
    
    // 文件定位
    loff_t (*llseek) (struct file *, loff_t, int);
    
    // 同步读写
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    
    // 异步读写
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, 
                         unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, 
                          unsigned long, loff_t);
    
    // 目录读取
    int (*readdir) (struct file *, void *, filldir_t);
    
    // 多路复用
    unsigned int (*poll) (struct file *, struct poll_table_struct *);
    
    // 设备控制
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    
    // 内存映射
    int (*mmap) (struct file *, struct vm_area_struct *);
    
    // 文件打开/关闭
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    
    // 同步
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*aio_fsync) (struct kiocb *, int datasync);
    
    // 异步事件通知
    int (*fasync) (int, struct file *, int);
    
    // 文件锁
    int (*lock) (struct file *, int, struct file_lock *);
    
    // 零拷贝
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, 
                         loff_t *, int);
    unsigned long (*get_unmapped_area)(struct file *, unsigned long, 
                                       unsigned long, unsigned long, 
                                       unsigned long);
    
    // 标志检查
    int (*check_flags)(int);
    
    // 文件锁 (BSD风格)
    int (*flock) (struct file *, int, struct file_lock *);
    
    // Splice 操作
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, 
                            loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, 
                           struct pipe_inode_info *, size_t, unsigned int);
    
    // 租约
    int (*setlease)(struct file *, long, struct file_lock **);
    
    // 预分配
    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
};
```

**说明**: 每个文件系统只需实现自己需要的操作，未实现的保持 NULL，VFS 会有默认处理。

---

### 2. inode_operations 结构 - inode 操作的"接口契约"

```c
// include/linux/fs.h

struct inode_operations {
    // 查找目录项
    struct dentry * (*lookup) (struct inode *, struct dentry *, 
                               struct nameidata *);
    
    // 符号链接操作
    void * (*follow_link) (struct dentry *, struct nameidata *);
    int (*readlink) (struct dentry *, char __user *, int);
    void (*put_link) (struct dentry *, struct nameidata *, void *);
    
    // 权限检查
    int (*permission) (struct inode *, int);
    struct posix_acl * (*get_acl)(struct inode *, int);

    // 文件创建/删除
    int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
    int (*link) (struct dentry *, struct inode *, struct dentry *);
    int (*unlink) (struct inode *, struct dentry *);
    int (*symlink) (struct inode *, struct dentry *, const char *);
    
    // 目录操作
    int (*mkdir) (struct inode *, struct dentry *, int);
    int (*rmdir) (struct inode *, struct dentry *);
    
    // 特殊文件
    int (*mknod) (struct inode *, struct dentry *, int, dev_t);
    
    // 重命名
    int (*rename) (struct inode *, struct dentry *,
                   struct inode *, struct dentry *);
    
    // 截断
    void (*truncate) (struct inode *);
    
    // 属性操作
    int (*setattr) (struct dentry *, struct iattr *);
    int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
    
    // 扩展属性
    int (*setxattr) (struct dentry *, const char *, const void *, size_t, int);
    ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
    ssize_t (*listxattr) (struct dentry *, char *, size_t);
    int (*removexattr) (struct dentry *, const char *);
    
    // 范围截断
    void (*truncate_range)(struct inode *, loff_t, loff_t);
    
    // 文件区间映射
    int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start, u64 len);
} ____cacheline_aligned;
```

---

### 3. VFS 如何调用注入的 ops

```c
// fs/read_write.c

ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;

    // 权限检查
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
        return -EINVAL;
    if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
        return -EFAULT;

    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;
        
        // 控制反转: 调用文件系统注入的 read 函数
        if (file->f_op->read)
            ret = file->f_op->read(file, buf, count, pos);
        else
            ret = do_sync_read(file, buf, count, pos);
            
        if (ret > 0) {
            fsnotify_access(file);
            add_rchar(current, ret);
        }
        inc_syscr(current);
    }

    return ret;
}

ssize_t vfs_write(struct file *file, const char __user *buf, 
                  size_t count, loff_t *pos)
{
    ssize_t ret;

    if (!(file->f_mode & FMODE_WRITE))
        return -EBADF;
    if (!file->f_op || (!file->f_op->write && !file->f_op->aio_write))
        return -EINVAL;

    ret = rw_verify_area(WRITE, file, pos, count);
    if (ret >= 0) {
        count = ret;
        
        // 控制反转: 调用文件系统注入的 write 函数
        if (file->f_op->write)
            ret = file->f_op->write(file, buf, count, pos);
        else
            ret = do_sync_write(file, buf, count, pos);
            
        if (ret > 0) {
            fsnotify_modify(file);
            add_wchar(current, ret);
        }
        inc_syscw(current);
    }

    return ret;
}
```

---

### 4. ext4 文件系统 - 注入实现示例

```c
// fs/ext4/file.c

// ext4 的读写实现
static ssize_t ext4_file_write(struct file *file, const char __user *buf,
                               size_t len, loff_t *ppos)
{
    struct inode *inode = file->f_mapping->host;
    ssize_t ret;

    // ext4 特有的写逻辑
    ret = generic_file_aio_write(file, buf, len, ppos);
    
    // 处理 ext4 特有的扩展
    if (ret > 0)
        ext4_update_file_size(inode, *ppos);
        
    return ret;
}

// 注入 file_operations
const struct file_operations ext4_file_operations = {
    .llseek         = ext4_llseek,
    .read           = do_sync_read,
    .write          = do_sync_write,
    .aio_read       = generic_file_aio_read,
    .aio_write      = ext4_file_write,          // 注入: ext4 写实现
    .unlocked_ioctl = ext4_ioctl,               // 注入: ext4 ioctl
    .mmap           = ext4_file_mmap,           // 注入: ext4 mmap
    .open           = ext4_file_open,           // 注入: ext4 打开
    .release        = ext4_release_file,
    .fsync          = ext4_sync_file,           // 注入: ext4 同步
    .splice_read    = generic_file_splice_read,
    .splice_write   = generic_file_splice_write,
    .fallocate      = ext4_fallocate,           // 注入: ext4 预分配
};

// 注入 inode_operations
const struct inode_operations ext4_file_inode_operations = {
    .truncate       = ext4_truncate,
    .setattr        = ext4_setattr,
    .getattr        = ext4_getattr,
    .setxattr       = generic_setxattr,
    .getxattr       = generic_getxattr,
    .listxattr      = ext4_listxattr,
    .removexattr    = generic_removexattr,
    .get_acl        = ext4_get_acl,
    .fiemap         = ext4_fiemap,
};
```

---

### 5. procfs - 另一种注入示例

```c
// fs/proc/base.c

// 每个 /proc/pid/xxx 文件可以有不同的 ops
static const struct file_operations proc_pid_cmdline_ops = {
    .read       = proc_pid_cmdline_read,    // 只有 read
};

static const struct file_operations proc_pid_status_ops = {
    .open       = proc_pid_status_open,
    .read       = seq_read,                  // 使用 seq_file
    .llseek     = seq_lseek,
    .release    = single_release,
};

static const struct file_operations proc_pid_maps_ops = {
    .open       = proc_maps_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = proc_maps_release,
};

// 在创建 inode 时注入对应的 ops
struct dentry *proc_pid_lookup(struct inode *dir, struct dentry *dentry,
                               struct nameidata *nd)
{
    // 根据文件名选择对应的 ops
    if (strcmp(name, "cmdline") == 0) {
        inode->i_fop = &proc_pid_cmdline_ops;
    } else if (strcmp(name, "status") == 0) {
        inode->i_fop = &proc_pid_status_ops;
    } else if (strcmp(name, "maps") == 0) {
        inode->i_fop = &proc_pid_maps_ops;
    }
    // ...
}
```

---

### 6. 字符设备 - 特殊文件的 ops 注入

```c
// drivers/char/mem.c - /dev/null 示例

// /dev/null 的特殊读写
static ssize_t read_null(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    return 0;  // 永远返回 EOF
}

static ssize_t write_null(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    return count;  // 吞掉所有数据
}

// /dev/null 的 ops
static const struct file_operations null_fops = {
    .llseek     = null_lseek,
    .read       = read_null,        // 注入: 读返回 0
    .write      = write_null,       // 注入: 写返回 count
    .splice_write = splice_write_null,
};
```

---

## 这样做的好处

### 1. 统一接口，多态实现

```
用户程序只使用统一的系统调用:
    read(fd, buf, count)
    write(fd, buf, count)
    
底层可以是任何实现:
    - 本地磁盘文件 (ext4, xfs, btrfs)
    - 网络文件系统 (nfs, cifs, fuse)
    - 虚拟文件系统 (proc, sysfs, debugfs)
    - 设备文件 (/dev/null, /dev/zero)
    - 管道和套接字
```

### 2. 易于扩展

| 新增文件系统 | 需要的工作 |
|--------------|------------|
| exFAT | 实现 file_operations, inode_operations |
| 新网络文件系统 | 实现 file_operations, inode_operations |
| FUSE 用户态文件系统 | 实现代理 ops，转发到用户态 |

VFS 层代码完全不需要修改！

### 3. 组合复用

```c
// 复用通用实现
const struct file_operations my_file_ops = {
    .read           = generic_file_aio_read,    // 复用通用读
    .write          = my_special_write,         // 自定义写
    .mmap           = generic_file_mmap,        // 复用通用 mmap
    .fsync          = my_special_fsync,         // 自定义同步
};
```

### 4. 空操作优雅处理

```c
// VFS 检查 NULL 并提供默认行为
if (file->f_op->read)
    ret = file->f_op->read(file, buf, count, pos);
else if (file->f_op->aio_read)
    ret = do_sync_read(file, buf, count, pos);  // 用 aio_read 模拟
else
    return -EINVAL;  // 不支持读操作
```

### 5. 对象生命周期管理

```
                    open()
                      │
                      ▼
              ┌───────────────┐
              │   分配 file   │
              │   绑定 f_op   │◄── inode->i_fop
              └───────────────┘
                      │
                      ▼
              ┌───────────────┐
              │  read/write   │
              │  使用 f_op    │
              └───────────────┘
                      │
                      ▼
              ┌───────────────┐
              │    close()    │
              │  调用 release │
              │    释放 file  │
              └───────────────┘
```

---

## 多层 ops 协作

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              VFS 多层 ops                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   struct file {                                                             │
│       const struct file_operations *f_op;  ──► 文件操作                    │
│       struct inode *f_inode ─────────────────┐                              │
│   }                                          │                              │
│                                              │                              │
│   struct inode {                             ◄                              │
│       const struct inode_operations *i_op; ──► inode 操作                  │
│       const struct file_operations *i_fop; ──► 文件默认 ops                │
│       struct super_block *i_sb ──────────────┐                              │
│   }                                          │                              │
│                                              │                              │
│   struct super_block {                       ◄                              │
│       const struct super_operations *s_op; ──► 超级块操作                  │
│   }                                                                         │
│                                                                              │
│   struct dentry {                                                           │
│       const struct dentry_operations *d_op; ──► 目录项操作                 │
│   }                                                                         │
│                                                                              │
│   struct address_space {                                                    │
│       const struct address_space_operations *a_ops; ──► 地址空间操作       │
│   }                                                                         │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 核心源码文件

| 文件 | 功能 |
|------|------|
| `include/linux/fs.h` | file_operations, inode_operations 等定义 |
| `fs/read_write.c` | vfs_read, vfs_write 实现 |
| `fs/open.c` | vfs_open 实现 |
| `fs/namei.c` | 路径解析，使用 i_op->lookup |
| `fs/ext4/file.c` | ext4 文件操作实现 |
| `fs/proc/base.c` | procfs 实现 |

---

## 总结

VFS 多态 ops 的 IoC 模式:

1. **接口契约**: `file_operations`, `inode_operations` 等结构定义接口
2. **依赖注入**: 文件系统在 inode 创建时注入自己的 ops
3. **多态调用**: VFS 通过 `f->f_op->xxx()` 调用，不关心具体实现
4. **组合复用**: 可以混合使用通用实现和自定义实现
5. **分层设计**: 不同层级 (file, inode, super_block) 各有各的 ops

