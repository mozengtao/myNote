# VFS 多态 ops 中的依赖注入模式

> 文件路径: `/tmp/linux-ioc-patterns/03_vfs_file_operations.md`
> 内核版本: Linux 3.2
> 难度: ⭐⭐⭐

---

## 1. 模式概述

VFS (Virtual File System) 是 Linux 内核中**最经典的面向对象设计**。通过 `file_operations`、`inode_operations` 等结构体，VFS 将文件操作的接口与具体实现完全分离，实现了"一个接口，多种实现"的多态效果。

### DI/IoC 的具体表现形式

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         VFS 层的依赖注入架构                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   用户空间                                                                   │
│   ┌────────────────────────────────────────────────────────────────────┐    │
│   │    open("/home/file.txt")    open("/dev/null")    open("/proc/1")  │    │
│   │           │                        │                     │         │    │
│   └───────────┼────────────────────────┼─────────────────────┼─────────┘    │
│               │                        │                     │              │
│   ════════════╪════════════════════════╪═════════════════════╪════════════  │
│               │      系统调用边界      │                     │              │
│               ▼                        ▼                     ▼              │
│   ┌─────────────────────────────────────────────────────────────────────┐   │
│   │                              VFS 层                                  │   │
│   │                                                                      │   │
│   │   vfs_open() / vfs_read() / vfs_write()                             │   │
│   │                     │                                                │   │
│   │                     │  file->f_op->read(...)  ◄── 统一接口          │   │
│   │                     │                                                │   │
│   └─────────────────────┼────────────────────────────────────────────────┘   │
│                         │                                                    │
│           不同的 f_op   │                                                    │
│           ┌─────────────┼─────────────┬─────────────┐                       │
│           │             │             │             │                        │
│           ▼             ▼             ▼             ▼                        │
│   ┌───────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐               │
│   │ ext4_fops │  │ null_fops │  │ proc_fops │  │ nfs_fops  │               │
│   │           │  │           │  │           │  │           │               │
│   │.read = ext│  │.read = 0  │  │.read = seq│  │.read = nfs│               │
│   │  4_read   │  │  返回     │  │  _read    │  │  _read    │               │
│   └───────────┘  └───────────┘  └───────────┘  └───────────┘               │
│        │              │              │              │                        │
│        ▼              ▼              ▼              ▼                        │
│   ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐                    │
│   │  磁盘   │   │  无操作  │   │ 内核数据│   │  网络   │                    │
│   └─────────┘   └─────────┘   └─────────┘   └─────────┘                    │
│                                                                              │
│   控制反转:                                                                  │
│   • 用户调用 read() → VFS 不知道数据来自哪里                                │
│   • 打开文件时，由文件系统注入具体的 f_op                                   │
│   • 同样是 read()，根据文件类型路由到不同实现                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. 设计动机

### 要解决的问题

| 问题 | VFS 的解决方案 |
|------|----------------|
| **多种文件系统共存** | 统一接口，不同实现 |
| **特殊文件 (/dev/xxx)** | 设备文件有自己的 ops |
| **虚拟文件系统 (/proc, /sys)** | 数据来自内核，不是磁盘 |
| **网络文件系统 (NFS)** | 数据来自网络 |
| **应用程序兼容性** | 用户程序无需关心底层实现 |

### 设计目标

1. **POSIX 兼容**: 提供标准的文件操作语义
2. **可扩展**: 新文件系统只需实现 ops 接口
3. **透明性**: 应用程序看到统一的文件接口
4. **高性能**: 最小化间接调用开销

---

## 3. 核心数据结构

### 3.1 file_operations - 文件操作接口

```c
// include/linux/fs.h (第 1583-1611 行)

struct file_operations {
    struct module *owner;               // 所属模块

    // ===== 定位操作 =====
    loff_t (*llseek) (struct file *, loff_t, int);

    // ===== 读写操作 =====
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);

    // ===== 异步 I/O =====
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *,
                         unsigned long, loff_t);
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *,
                          unsigned long, loff_t);

    // ===== 目录操作 =====
    int (*readdir) (struct file *, void *, filldir_t);

    // ===== 多路复用 =====
    unsigned int (*poll) (struct file *, struct poll_table_struct *);

    // ===== 设备控制 =====
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);

    // ===== 内存映射 =====
    int (*mmap) (struct file *, struct vm_area_struct *);

    // ===== 文件打开/关闭 =====
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);

    // ===== 同步操作 =====
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*aio_fsync) (struct kiocb *, int datasync);

    // ===== 异步通知 =====
    int (*fasync) (int, struct file *, int);

    // ===== 文件锁 =====
    int (*lock) (struct file *, int, struct file_lock *);
    int (*flock) (struct file *, int, struct file_lock *);

    // ===== 零拷贝 =====
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t,
                         loff_t *, int);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *,
                            loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *,
                           struct pipe_inode_info *, size_t, unsigned int);

    // ===== 预分配 =====
    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
};
```

### 3.2 inode_operations - inode 操作接口

```c
// include/linux/fs.h (第 1613-1641 行)

struct inode_operations {
    // ===== 目录项查找 =====
    struct dentry * (*lookup) (struct inode *, struct dentry *,
                               struct nameidata *);

    // ===== 符号链接 =====
    void * (*follow_link) (struct dentry *, struct nameidata *);
    int (*readlink) (struct dentry *, char __user *, int);
    void (*put_link) (struct dentry *, struct nameidata *, void *);

    // ===== 权限检查 =====
    int (*permission) (struct inode *, int);
    struct posix_acl * (*get_acl)(struct inode *, int);

    // ===== 文件创建 =====
    int (*create) (struct inode *, struct dentry *, int, struct nameidata *);
    int (*link) (struct dentry *, struct inode *, struct dentry *);
    int (*unlink) (struct inode *, struct dentry *);
    int (*symlink) (struct inode *, struct dentry *, const char *);

    // ===== 目录操作 =====
    int (*mkdir) (struct inode *, struct dentry *, int);
    int (*rmdir) (struct inode *, struct dentry *);

    // ===== 特殊文件 =====
    int (*mknod) (struct inode *, struct dentry *, int, dev_t);

    // ===== 重命名 =====
    int (*rename) (struct inode *, struct dentry *,
                   struct inode *, struct dentry *);

    // ===== 属性操作 =====
    int (*setattr) (struct dentry *, struct iattr *);
    int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);

    // ===== 扩展属性 =====
    int (*setxattr) (struct dentry *, const char *, const void *, size_t, int);
    ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
    ssize_t (*listxattr) (struct dentry *, char *, size_t);
    int (*removexattr) (struct dentry *, const char *);
};
```

### 3.3 关键结构体关系

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          VFS 核心对象关系                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   进程                                                                       │
│   ┌─────────────────────────┐                                               │
│   │ task_struct             │                                               │
│   │   └── files ────────────┼──────┐                                        │
│   └─────────────────────────┘      │                                        │
│                                    ▼                                        │
│                           ┌──────────────────┐                              │
│                           │ files_struct     │                              │
│                           │   └── fd_array[] │                              │
│                           │        [0] ──────┼───┐                          │
│                           │        [1] ──────┼───┼───┐                      │
│                           │        [2] ──────┼───┼───┼───┐                  │
│                           └──────────────────┘   │   │   │                  │
│                                                  │   │   │                  │
│                                ┌─────────────────┘   │   │                  │
│                                │                     │   │                  │
│                                ▼                     ▼   ▼                  │
│                           ┌──────────────────────────────────┐              │
│                           │         struct file              │              │
│                           │                                  │              │
│                           │  f_op ───────────────────────────┼──► file_     │
│                           │  (const struct file_operations *)│   operations │
│                           │                                  │              │
│                           │  f_path.dentry ──────────────────┼──┐           │
│                           │                                  │  │           │
│                           └──────────────────────────────────┘  │           │
│                                                                 │           │
│                                ┌────────────────────────────────┘           │
│                                ▼                                            │
│                           ┌──────────────────────────────────┐              │
│                           │         struct dentry            │              │
│                           │                                  │              │
│                           │  d_inode ────────────────────────┼──┐           │
│                           │  d_op ───────────────────────────┼──► dentry_   │
│                           │                                  │   operations │
│                           └──────────────────────────────────┘  │           │
│                                                                 │           │
│                                ┌────────────────────────────────┘           │
│                                ▼                                            │
│                           ┌──────────────────────────────────┐              │
│                           │         struct inode             │              │
│                           │                                  │              │
│                           │  i_op ───────────────────────────┼──► inode_    │
│                           │  (const struct inode_operations *)   operations │
│                           │                                  │              │
│                           │  i_fop ──────────────────────────┼──► file_     │
│                           │  (const struct file_operations *)│   operations │
│                           │                                  │              │
│                           │  i_sb ───────────────────────────┼──┐           │
│                           └──────────────────────────────────┘  │           │
│                                                                 │           │
│                                ┌────────────────────────────────┘           │
│                                ▼                                            │
│                           ┌──────────────────────────────────┐              │
│                           │       struct super_block         │              │
│                           │                                  │              │
│                           │  s_op ───────────────────────────┼──► super_    │
│                           │                                  │   operations │
│                           └──────────────────────────────────┘              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. 代码流程分析

### 4.1 open() 系统调用的依赖注入路径

```c
// 完整调用链:
// sys_open() → do_sys_open() → do_filp_open() → path_openat()
//           → do_last() → vfs_open() → do_dentry_open()

// fs/open.c (第 730-790 行)
static int do_dentry_open(struct file *f,
                          int (*open)(struct inode *, struct file *),
                          const struct cred *cred)
{
    struct inode *inode;
    int error;

    // 获取 inode
    inode = f->f_path.dentry->d_inode;

    // 关键: 从 inode 获取 file_operations (依赖注入点)
    f->f_op = fops_get(inode->i_fop);
    if (!f->f_op) {
        error = -ENODEV;
        goto cleanup_all;
    }

    // 设置文件模式
    f->f_mapping = inode->i_mapping;
    f->f_pos = 0;

    // 调用注入的 open 函数
    if (!open)
        open = f->f_op->open;
    if (open) {
        error = open(inode, f);  // 控制反转: 调用具体实现
        if (error)
            goto cleanup_all;
    }

    // 文件打开成功
    file_ra_state_init(&f->f_ra, f->f_mapping->host->i_mapping);
    return 0;

cleanup_all:
    // 错误处理...
    return error;
}
```

### 4.2 read() 系统调用的依赖注入路径

```c
// fs/read_write.c (第 360-400 行)

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

    // 安全模块检查
    ret = rw_verify_area(READ, file, pos, count);
    if (ret >= 0) {
        count = ret;

        // 控制反转: 调用注入的 read 函数
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
EXPORT_SYMBOL(vfs_read);
```

### 4.3 完整调用流程图

```
用户空间:
    fd = open("/home/user/file.txt", O_RDONLY);
    read(fd, buf, 1024);

                    │
                    ▼
════════════════════════════════════════════════════════════════════
                    │  系统调用
                    ▼
┌────────────────────────────────────────────────────────────────────┐
│  sys_open("/home/user/file.txt", O_RDONLY)                        │
│      │                                                             │
│      ▼                                                             │
│  do_sys_open()                                                     │
│      │                                                             │
│      ▼                                                             │
│  do_filp_open()                                                    │
│      │                                                             │
│      ├──► path_openat()   ──► 解析路径 "/home/user/file.txt"      │
│      │        │                                                    │
│      │        ▼                                                    │
│      │    do_last()       ──► 获取目标 dentry 和 inode            │
│      │        │                                                    │
│      │        ▼                                                    │
│      │    vfs_open()                                               │
│      │        │                                                    │
│      │        ▼                                                    │
│      │    do_dentry_open()                                         │
│      │        │                                                    │
│      │        │  // 关键: 依赖注入                                 │
│      │        │  f->f_op = fops_get(inode->i_fop);                │
│      │        │                                                    │
│      │        │  // inode->i_fop 来自哪里?                         │
│      │        │  // 答案: 文件系统在创建 inode 时设置              │
│      │        │  // ext4: inode->i_fop = &ext4_file_operations;   │
│      │        │                                                    │
│      │        ▼                                                    │
│      │    f->f_op->open(inode, f)  ──► ext4_file_open()           │
│      │                                                             │
│      └──► 返回 fd                                                  │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
                    │
                    ▼
┌────────────────────────────────────────────────────────────────────┐
│  sys_read(fd, buf, 1024)                                          │
│      │                                                             │
│      ▼                                                             │
│  vfs_read(file, buf, count, pos)                                  │
│      │                                                             │
│      │  // 控制反转: 调用注入的函数                                │
│      │                                                             │
│      ▼                                                             │
│  file->f_op->read(file, buf, count, pos)                          │
│      │                                                             │
│      │  // f_op 指向 ext4_file_operations                         │
│      │  // 所以调用的是 ext4 的 read 实现                         │
│      │                                                             │
│      ▼                                                             │
│  generic_file_aio_read()  ──► Page Cache / 磁盘 I/O               │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

## 5. 实际案例

### 案例1: ext4 文件系统

```c
// fs/ext4/file.c

// ext4 特有的文件打开
static int ext4_file_open(struct inode *inode, struct file *filp)
{
    struct super_block *sb = inode->i_sb;
    struct ext4_sb_info *sbi = EXT4_SB(sb);
    struct ext4_inode_info *ei = EXT4_I(inode);

    // 检查文件系统状态
    if (unlikely(ext4_forced_shutdown(EXT4_SB(sb))))
        return -EIO;

    // 记录打开时间
    filp->f_mode |= FMODE_NOWAIT;

    return generic_file_open(inode, filp);
}

// ext4 文件操作集 - 依赖注入
const struct file_operations ext4_file_operations = {
    .llseek         = ext4_llseek,
    .read           = do_sync_read,
    .write          = do_sync_write,
    .aio_read       = generic_file_aio_read,
    .aio_write      = ext4_file_write,        // ext4 特有的写
    .unlocked_ioctl = ext4_ioctl,             // ext4 ioctl
    .mmap           = ext4_file_mmap,         // ext4 mmap
    .open           = ext4_file_open,         // ext4 open
    .release        = ext4_release_file,
    .fsync          = ext4_sync_file,         // ext4 同步
    .splice_read    = generic_file_splice_read,
    .splice_write   = generic_file_splice_write,
    .fallocate      = ext4_fallocate,         // ext4 预分配
};

// ext4 inode 操作集
const struct inode_operations ext4_file_inode_operations = {
    .setattr        = ext4_setattr,
    .getattr        = ext4_getattr,
    .setxattr       = generic_setxattr,
    .getxattr       = generic_getxattr,
    .listxattr      = ext4_listxattr,
    .removexattr    = generic_removexattr,
    .get_acl        = ext4_get_acl,
    .fiemap         = ext4_fiemap,
};

// 在创建 inode 时注入 ops
// fs/ext4/inode.c
struct inode *ext4_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode;
    struct ext4_inode_info *ei;

    inode = iget_locked(sb, ino);
    // ...

    if (S_ISREG(inode->i_mode)) {
        // 普通文件
        inode->i_op = &ext4_file_inode_operations;  // 注入 inode ops
        inode->i_fop = &ext4_file_operations;       // 注入 file ops
    } else if (S_ISDIR(inode->i_mode)) {
        // 目录
        inode->i_op = &ext4_dir_inode_operations;
        inode->i_fop = &ext4_dir_operations;
    } else if (S_ISLNK(inode->i_mode)) {
        // 符号链接
        inode->i_op = &ext4_symlink_inode_operations;
    }
    // ...
}
```

### 案例2: /dev/null 字符设备

```c
// drivers/char/mem.c

// /dev/null 读操作 - 永远返回 EOF
static ssize_t read_null(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    return 0;  // 返回 0 表示 EOF
}

// /dev/null 写操作 - 吞掉所有数据
static ssize_t write_null(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    return count;  // 假装写入成功，实际什么也不做
}

// /dev/null 的 file_operations
static const struct file_operations null_fops = {
    .llseek     = null_lseek,
    .read       = read_null,        // 注入: 返回 EOF
    .write      = write_null,       // 注入: 吞掉数据
    .splice_write = splice_write_null,
};

// /dev/zero 读操作 - 返回无限的零
static ssize_t read_zero(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    size_t written = 0;

    while (count) {
        // 填充零
        size_t chunk = min_t(size_t, count, PAGE_SIZE);
        if (clear_user(buf, chunk))
            return -EFAULT;
        buf += chunk;
        count -= chunk;
        written += chunk;
    }
    return written;
}

static const struct file_operations zero_fops = {
    .llseek     = null_lseek,
    .read       = read_zero,        // 注入: 返回零
    .write      = write_null,       // 注入: 吞掉数据
    .mmap       = mmap_zero,
};

// 设备列表
static const struct memdev {
    const char *name;
    umode_t mode;
    const struct file_operations *fops;
} devlist[] = {
    [1] = { "mem",     0,    &mem_fops },     // /dev/mem
    [3] = { "null",    0666, &null_fops },    // /dev/null
    [5] = { "zero",    0666, &zero_fops },    // /dev/zero
    [7] = { "full",    0666, &full_fops },    // /dev/full
    [8] = { "random",  0666, &random_fops },  // /dev/random
    [9] = { "urandom", 0666, &urandom_fops }, // /dev/urandom
    // ...
};
```

### 案例3: procfs 虚拟文件系统

```c
// fs/proc/base.c

// /proc/[pid]/cmdline 的 read 操作
static ssize_t proc_pid_cmdline_read(struct file *file, char __user *buf,
                                     size_t count, loff_t *pos)
{
    struct task_struct *tsk;
    struct mm_struct *mm;
    char *page;
    unsigned long arg_start, arg_end;
    ssize_t len;

    // 获取目标进程
    tsk = get_proc_task(file->f_path.dentry->d_inode);
    if (!tsk)
        return -ESRCH;

    // 获取进程内存描述符
    mm = get_task_mm(tsk);
    if (!mm)
        return 0;

    // 读取命令行参数
    arg_start = mm->arg_start;
    arg_end = mm->arg_end;

    // 从进程内存中复制命令行
    page = (char *)__get_free_page(GFP_TEMPORARY);
    len = access_remote_vm(mm, arg_start, page, arg_end - arg_start, 0);

    // 复制到用户空间
    if (copy_to_user(buf, page, len))
        len = -EFAULT;

    free_page((unsigned long)page);
    mmput(mm);
    put_task_struct(tsk);
    return len;
}

// /proc/[pid]/cmdline 的 ops
static const struct file_operations proc_pid_cmdline_ops = {
    .read = proc_pid_cmdline_read,  // 只有 read
};

// /proc/[pid]/status 使用 seq_file
static int proc_pid_status_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_pid_status, inode);
}

static const struct file_operations proc_pid_status_ops = {
    .open       = proc_pid_status_open,
    .read       = seq_read,         // 使用通用的 seq_read
    .llseek     = seq_lseek,
    .release    = single_release,
};

// 根据文件名选择不同的 ops
static struct dentry *proc_pident_lookup(struct inode *dir,
                                         struct dentry *dentry,
                                         const struct pid_entry *ents,
                                         unsigned int nents)
{
    struct inode *inode;
    const struct pid_entry *p;

    // 查找匹配的条目
    for (p = ents; p < ents + nents; p++) {
        if (strcmp(dentry->d_name.name, p->name) == 0) {
            inode = proc_pid_make_inode(dir->i_sb, ...);
            if (!inode)
                return ERR_PTR(-ENOMEM);

            // 注入对应的 ops
            inode->i_fop = p->fop;
            // ...
        }
    }
}
```

---

## 6. 优势分析

### 6.1 统一接口，多态实现

```c
// 用户程序完全相同的代码，操作不同类型的文件

int fd1 = open("/home/user/file.txt", O_RDONLY);  // ext4 文件
int fd2 = open("/dev/null", O_RDONLY);            // 设备文件
int fd3 = open("/proc/self/status", O_RDONLY);   // proc 文件

char buf[1024];
read(fd1, buf, sizeof(buf));  // → ext4_file_operations.read
read(fd2, buf, sizeof(buf));  // → null_fops.read (返回 0)
read(fd3, buf, sizeof(buf));  // → proc_pid_status_ops.read
```

### 6.2 组合复用

```c
// 可以混合使用通用实现和自定义实现
const struct file_operations my_file_ops = {
    .read           = generic_file_aio_read,  // 复用通用读
    .write          = my_special_write,       // 自定义写
    .mmap           = generic_file_mmap,      // 复用通用 mmap
    .fsync          = my_special_fsync,       // 自定义同步
    .open           = generic_file_open,      // 复用通用 open
};
```

### 6.3 易于扩展

| 新增文件系统 | 需要的工作 |
|--------------|------------|
| 新本地文件系统 | 实现 file_operations, inode_operations |
| 新网络文件系统 | 实现 file_operations, inode_operations |
| 新虚拟文件系统 | 实现 file_operations |
| FUSE 用户态文件系统 | 实现代理 ops，转发到用户态 |

VFS 核心代码完全不需要修改！

---

## 7. 对比思考

### 如果不使用 VFS

```c
// 假设没有 VFS，应用程序必须:

// 1. 知道文件在哪个文件系统上
if (is_ext4_file(path)) {
    fd = ext4_open(path, flags);
} else if (is_device_file(path)) {
    fd = devfs_open(path, flags);
} else if (is_proc_file(path)) {
    fd = proc_open(path, flags);
}

// 2. 使用不同的 API 读写
if (is_ext4_file(path)) {
    ext4_read(fd, buf, count);
} else if (is_device_file(path)) {
    device_read(fd, buf, count);
}

// 问题:
// 1. 应用程序需要知道文件系统类型
// 2. 每种文件系统有不同的 API
// 3. 无法透明地访问网络文件系统
// 4. 软链接跨文件系统会很复杂
```

---

## 8. 相关 API

### 文件操作

```c
// VFS 层文件操作
ssize_t vfs_read(struct file *, char __user *, size_t, loff_t *);
ssize_t vfs_write(struct file *, const char __user *, size_t, loff_t *);
int vfs_open(const struct path *, struct file *, const struct cred *);
int vfs_create(struct inode *, struct dentry *, int, struct nameidata *);
int vfs_mkdir(struct inode *, struct dentry *, int);
int vfs_unlink(struct inode *, struct dentry *);
int vfs_rename(struct inode *, struct dentry *, struct inode *, struct dentry *);
```

### inode 操作

```c
// 获取 inode
struct inode *iget_locked(struct super_block *, unsigned long);
void iput(struct inode *);
void ihold(struct inode *);

// 创建特殊 inode
void init_special_inode(struct inode *, umode_t, dev_t);
```

### 文件系统注册

```c
// 注册文件系统
int register_filesystem(struct file_system_type *);
int unregister_filesystem(struct file_system_type *);

// 挂载
struct dentry *mount_bdev(struct file_system_type *fs_type, int flags,
                          const char *dev_name, void *data,
                          int (*fill_super)(struct super_block *, void *, int));
```

---

## 🤔 思考题

1. **打开 `/dev/null` 和 `/home/user/file.txt` 时，f_op 是在哪个时机绑定的？**
   - 提示: 查看 `do_dentry_open` 和 inode 创建过程

2. **如果一个文件系统没有实现 `read`，但实现了 `aio_read`，VFS 如何处理普通的 read() 调用？**
   - 提示: 查看 `vfs_read` 中的 `do_sync_read`

3. **为什么 inode 有 `i_fop` 而 file 也有 `f_op`？它们的关系是什么？**
   - 提示: 考虑打开同一个文件多次的情况

4. **procfs 如何做到每个 `/proc/[pid]/xxx` 文件有不同的 ops？**
   - 提示: 查看 `proc_pident_lookup`

---

## 📚 相关源码文件

| 文件 | 行数 | 内容 |
|------|------|------|
| `include/linux/fs.h` | 1-2700 | VFS 核心结构定义 |
| `fs/read_write.c` | 1-1100 | vfs_read/vfs_write |
| `fs/open.c` | 1-1200 | vfs_open/do_dentry_open |
| `fs/namei.c` | 1-3500 | 路径解析 |
| `fs/ext4/file.c` | 1-600 | ext4 文件操作 |
| `fs/proc/base.c` | 1-3000 | procfs 实现 |
| `drivers/char/mem.c` | 1-900 | /dev/null 等实现 |

