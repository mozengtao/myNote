# File Related Structures and API Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] Linux内核中文件相关的核心数据结构有哪些？
A: 文件系统的核心数据结构层次：
```
用户空间
+-------------------+
|  文件描述符 (fd)  |  int型整数
+--------+----------+
         |
内核空间 |
         v
+-------------------+
| struct file       |  打开文件实例
+--------+----------+
         |
         v
+-------------------+
| struct dentry     |  目录项缓存
+--------+----------+
         |
         v
+-------------------+
| struct inode      |  文件元数据
+--------+----------+
         |
         v
+-------------------+
| struct super_block|  文件系统实例
+-------------------+

关系：
- fd → files_struct → fdtable → file
- file → dentry → inode
- 多个file可指向同一个dentry（多次打开同一文件）
- 多个dentry可指向同一个inode（硬链接）
```

Q: [Basic] 文件描述符(fd)是什么？与struct file的关系？
A: fd是用户空间访问文件的句柄：
```c
/* 进程的文件描述符表 */
struct task_struct {
    struct files_struct *files;  // 文件描述符表
};

struct files_struct {
    atomic_t count;              // 引用计数
    struct fdtable __rcu *fdt;   // 指向fdtable
    struct fdtable fdtab;        // 内嵌的小fdtable
    spinlock_t file_lock;
    int next_fd;                 // 下一个可用fd
    struct file __rcu *fd_array[NR_OPEN_DEFAULT]; // 默认64
};

struct fdtable {
    unsigned int max_fds;        // 最大fd数
    struct file __rcu **fd;      // file指针数组
    fd_set *close_on_exec;       // exec时关闭标志
    fd_set *open_fds;            // 打开标志位图
};

/* fd到file的转换 */
struct file *fget(unsigned int fd)
{
    struct files_struct *files = current->files;
    struct file *file;
    
    rcu_read_lock();
    file = fcheck_files(files, fd);
    if (file && !atomic_long_inc_not_zero(&file->f_count))
        file = NULL;
    rcu_read_unlock();
    
    return file;
}
```

Q: [Basic] 进程打开文件的数量限制是多少？
A: Linux有多层限制：
```bash
# 1. 系统级限制
$ cat /proc/sys/fs/file-max
9223372036854775807  # 系统最大打开文件数

$ cat /proc/sys/fs/file-nr
1234  0  9223372036854775807
# 已分配 | 空闲(always 0) | 最大

# 2. 进程级限制
$ ulimit -n
1024  # 软限制

$ ulimit -Hn
1048576  # 硬限制

# 3. 内核常量
NR_OPEN_DEFAULT = 64  # 内嵌fdtable初始大小
NR_OPEN = 1024*1024   # 单进程最大

/* 动态扩展fdtable */
int expand_files(struct files_struct *files, int nr)
{
    struct fdtable *fdt = files_fdtable(files);
    
    /* 如果当前表足够大 */
    if (nr < fdt->max_fds)
        return 0;
    
    /* 扩展表 */
    return expand_fdtable(files, nr);
}
```

---

## struct file (打开文件对象)

Q: [Intermediate] struct file的完整结构是什么？
A: struct file表示一个打开的文件实例：
```c
/* include/linux/fs.h */
struct file {
    union {
        struct list_head fu_list;   // 文件系统的文件链表
        struct rcu_head  fu_rcuhead; // RCU释放
    } f_u;
    
    struct path f_path;              // 路径(dentry + vfsmount)
    #define f_dentry f_path.dentry   // 便捷宏
    #define f_vfsmnt f_path.mnt
    
    const struct file_operations *f_op; // 文件操作函数表
    
    spinlock_t f_lock;               // 保护f_ep_links, f_flags, f_pos
    atomic_long_t f_count;           // 引用计数
    unsigned int f_flags;            // 打开标志(O_RDONLY等)
    fmode_t f_mode;                  // 访问模式(FMODE_READ等)
    loff_t f_pos;                    // 当前文件位置
    
    struct fown_struct f_owner;      // 异步I/O所有者
    const struct cred *f_cred;       // 打开时的凭证
    struct file_ra_state f_ra;       // 预读状态
    
    u64 f_version;                   // 版本号(用于目录)
    
#ifdef CONFIG_SECURITY
    void *f_security;                // LSM安全数据
#endif
    
    void *private_data;              // 驱动私有数据
    
#ifdef CONFIG_EPOLL
    struct list_head f_ep_links;     // epoll链表
    struct list_head f_tfile_llink;  // 文件链表
#endif
    
    struct address_space *f_mapping; // 页缓存映射
};

/* f_mode标志 */
#define FMODE_READ   0x1   // 可读
#define FMODE_WRITE  0x2   // 可写
#define FMODE_LSEEK  0x4   // 可seek
#define FMODE_PREAD  0x8   // 支持pread
#define FMODE_PWRITE 0x10  // 支持pwrite
#define FMODE_EXEC   0x20  // 执行权限
```

Q: [Intermediate] struct path的作用是什么？
A: struct path表示文件的完整路径：
```c
/* include/linux/path.h */
struct path {
    struct vfsmount *mnt;  // 挂载点
    struct dentry *dentry; // 目录项
};

/* 作用 */
1. 定位文件：dentry指向文件的目录项
2. 挂载信息：mnt指向文件所在的文件系统挂载实例

/* 使用示例 */
struct path path;
int error = kern_path("/etc/passwd", LOOKUP_FOLLOW, &path);
if (!error) {
    struct inode *inode = path.dentry->d_inode;
    /* 操作inode */
    path_put(&path);  // 释放引用
}

/* 路径操作函数 */
void path_get(const struct path *path);  // 增加引用
void path_put(const struct path *path);  // 减少引用
int kern_path(const char *name, unsigned int flags, struct path *path);
```

Q: [Intermediate] file的引用计数如何管理？
A: file使用引用计数控制生命周期：
```c
/* 获取file引用 */
struct file *fget(unsigned int fd);        // 从fd获取并增加引用
struct file *get_file(struct file *f);     // 直接增加引用
struct file *fget_light(unsigned int fd, int *fput_needed);

/* 释放file引用 */
void fput(struct file *file);              // 减少引用
void fput_light(struct file *file, int fput_needed);

/* fput实现 */
void fput(struct file *file)
{
    if (atomic_long_dec_and_test(&file->f_count)) {
        struct task_struct *task = current;
        
        /* 如果在中断上下文，延迟释放 */
        if (unlikely(in_interrupt() || task_work_add(...))) {
            /* 通过workqueue释放 */
            schedule_work(&file->f_u.fu_work);
        }
    }
}

/* 真正的释放函数 */
static void __fput(struct file *file)
{
    struct dentry *dentry = file->f_path.dentry;
    struct vfsmount *mnt = file->f_path.mnt;
    struct inode *inode = dentry->d_inode;

    if (file->f_op && file->f_op->release)
        file->f_op->release(inode, file);  // 调用驱动release
    
    if (file->f_op && file->f_op->fasync)
        file->f_op->fasync(-1, file, 0);
    
    path_put(&file->f_path);  // 释放path
    put_cred(file->f_cred);   // 释放凭证
    kmem_cache_free(filp_cachep, file);  // 释放内存
}
```

---

## struct inode (索引节点)

Q: [Intermediate] struct inode的核心字段有哪些？
A: inode存储文件的元数据：
```c
/* include/linux/fs.h */
struct inode {
    /* 权限和类型 */
    umode_t         i_mode;      // 文件类型和权限
    unsigned short  i_opflags;   // 操作标志
    uid_t           i_uid;       // 所有者UID
    gid_t           i_gid;       // 所有者GID
    unsigned int    i_flags;     // 文件系统标志
    
    /* ACL */
    struct posix_acl *i_acl;
    struct posix_acl *i_default_acl;
    
    /* 操作函数 */
    const struct inode_operations *i_op;  // inode操作
    struct super_block *i_sb;             // 所属超级块
    struct address_space *i_mapping;      // 页缓存
    
    /* 标识 */
    unsigned long   i_ino;       // inode号
    union {
        const unsigned int i_nlink; // 硬链接计数
        unsigned int __i_nlink;
    };
    dev_t           i_rdev;      // 设备号(设备文件)
    
    /* 时间戳 */
    struct timespec i_atime;     // 最后访问时间
    struct timespec i_mtime;     // 最后修改时间
    struct timespec i_ctime;     // 状态改变时间
    
    /* 大小 */
    spinlock_t      i_lock;
    unsigned short  i_bytes;     // 使用的字节(< 512)
    blkcnt_t        i_blocks;    // 使用的块数
    loff_t          i_size;      // 文件大小
    
    /* 状态 */
    unsigned long   i_state;     // 脏/锁定等状态
    struct mutex    i_mutex;     // inode互斥锁
    
    /* 缓存链表 */
    struct hlist_node i_hash;    // 哈希链表
    struct list_head  i_wb_list; // 写回链表
    struct list_head  i_lru;     // LRU链表
    struct list_head  i_sb_list; // 超级块链表
    
    /* 关联的dentry */
    union {
        struct hlist_head i_dentry;  // dentry链表(多个硬链接)
        struct rcu_head   i_rcu;
    };
    
    /* 特殊文件信息 */
    union {
        struct pipe_inode_info *i_pipe;  // 管道
        struct block_device *i_bdev;     // 块设备
        struct cdev *i_cdev;             // 字符设备
    };
    
    void *i_private;             // 文件系统私有数据
};

/* i_mode类型检查宏 */
S_ISREG(m)   // 普通文件
S_ISDIR(m)   // 目录
S_ISCHR(m)   // 字符设备
S_ISBLK(m)   // 块设备
S_ISFIFO(m)  // FIFO
S_ISLNK(m)   // 符号链接
S_ISSOCK(m)  // socket
```

Q: [Intermediate] inode状态(i_state)有哪些？
A: inode状态标志控制inode的行为：
```c
/* include/linux/fs.h */
#define I_DIRTY_SYNC        (1 << 0)  // 需要同步写回
#define I_DIRTY_DATASYNC    (1 << 1)  // 数据需要同步
#define I_DIRTY_PAGES       (1 << 2)  // 有脏页
#define I_NEW               (1 << 3)  // 新创建的inode
#define I_WILL_FREE         (1 << 4)  // 即将释放
#define I_FREEING           (1 << 5)  // 正在释放
#define I_CLEAR             (1 << 6)  // 已清理
#define I_SYNC              (1 << 7)  // 正在同步
#define I_REFERENCED        (1 << 8)  // 最近被访问

#define I_DIRTY (I_DIRTY_SYNC | I_DIRTY_DATASYNC | I_DIRTY_PAGES)

/* 状态操作 */
void mark_inode_dirty(struct inode *inode);
void mark_inode_dirty_sync(struct inode *inode);

/* inode锁定 */
void inode_lock(struct inode *inode);      // i_mutex
void inode_unlock(struct inode *inode);
void inode_lock_shared(struct inode *inode);
void inode_unlock_shared(struct inode *inode);
```

Q: [Advanced] inode的生命周期是什么？
A: inode经历分配、使用和释放阶段：
```
+------------------+
|  iget/iget_locked|  获取或创建inode
+--------+---------+
         |
         v
+------------------+
| I_NEW状态        |  新inode需要初始化
| 文件系统填充     |
+--------+---------+
         | unlock_new_inode()
         v
+------------------+
| 正常使用         |  i_count > 0
| 可能被标记为脏   |
+--------+---------+
         | iput() 减少引用
         v
+------------------+
| i_count == 0     |
| 移入LRU缓存      |
+--------+---------+
         | 内存压力/evict
         v
+------------------+
| evict_inode()    |
| 写回脏数据       |
| I_FREEING状态    |
+--------+---------+
         |
         v
+------------------+
| destroy_inode()  |
| 释放内存         |
+------------------+

/* 相关函数 */
struct inode *iget_locked(struct super_block *sb, unsigned long ino);
void unlock_new_inode(struct inode *inode);
void iput(struct inode *inode);
void evict_inode(struct inode *inode);
```

---

## struct dentry (目录项)

Q: [Intermediate] struct dentry的结构是什么？
A: dentry是VFS的目录项缓存：
```c
/* include/linux/dcache.h */
struct dentry {
    /* RCU查找访问的字段 */
    unsigned int d_flags;           // 标志
    seqcount_t d_seq;               // 顺序锁
    struct hlist_bl_node d_hash;    // 哈希链表
    struct dentry *d_parent;        // 父目录
    struct qstr d_name;             // 文件名
    struct inode *d_inode;          // 关联的inode
    unsigned char d_iname[DNAME_INLINE_LEN]; // 短名内嵌
    
    /* 引用计数访问 */
    unsigned int d_count;           // 引用计数
    spinlock_t d_lock;              // dentry锁
    const struct dentry_operations *d_op;  // 操作函数
    struct super_block *d_sb;       // 所属超级块
    unsigned long d_time;           // 用于revalidate
    void *d_fsdata;                 // 文件系统数据
    
    /* LRU管理 */
    struct list_head d_lru;         // LRU链表
    
    /* 目录树结构 */
    union {
        struct list_head d_child;   // 在父目录的子链表
        struct rcu_head d_rcu;      // RCU释放
    } d_u;
    struct list_head d_subdirs;     // 子目录链表
    struct list_head d_alias;       // inode的别名链表
};

/* d_flags标志 */
#define DCACHE_AUTOFS_PENDING   0x0001  // autofs挂起
#define DCACHE_NFSFS_RENAMED    0x0002  // NFS重命名
#define DCACHE_DISCONNECTED     0x0004  // 断开连接
#define DCACHE_REFERENCED       0x0008  // 最近使用
#define DCACHE_UNHASHED         0x0010  // 从哈希移除
#define DCACHE_OP_HASH          0x0020  // 有d_hash操作
#define DCACHE_OP_COMPARE       0x0040  // 有d_compare操作
#define DCACHE_OP_REVALIDATE    0x0080  // 有d_revalidate操作
#define DCACHE_OP_DELETE        0x0100  // 有d_delete操作
#define DCACHE_DIRECTORY_TYPE   0x0200  // 目录类型
#define DCACHE_REGULAR_TYPE     0x0400  // 普通文件类型
#define DCACHE_NEGATIVE         0x0800  // 负dentry
```

Q: [Intermediate] 什么是负dentry(negative dentry)？
A: 负dentry缓存"文件不存在"的信息：
```c
/* 负dentry的特征 */
- d_inode == NULL
- DCACHE_NEGATIVE标志置位

/* 作用 */
1. 避免重复的磁盘查找
2. 加速"文件不存在"的判断

/* 创建负dentry */
struct dentry *d_alloc_negative(struct dentry *parent, 
                                 const struct qstr *name)
{
    struct dentry *dentry = d_alloc(parent, name);
    if (dentry) {
        d_set_d_op(dentry, dentry->d_parent->d_op);
        /* d_inode保持NULL */
    }
    return dentry;
}

/* 查找时遇到负dentry */
struct dentry *lookup_dcache(const struct qstr *name,
                             struct dentry *dir,
                             bool *need_lookup)
{
    struct dentry *dentry = d_lookup(dir, name);
    if (dentry) {
        if (d_is_negative(dentry)) {
            /* 文件不存在，但有缓存的负dentry */
            *need_lookup = false;
            return dentry;
        }
    }
    *need_lookup = true;
    return dentry;
}

/* 负dentry超时重新验证 */
// 通过d_revalidate操作决定是否需要重新查找
```

Q: [Intermediate] dentry缓存(dcache)如何工作？
A: dcache是VFS的核心缓存机制：
```c
/* dentry缓存组织 */
1. 哈希表：快速查找
   - dentry_hashtable全局哈希表
   - 以(parent, name)为key

2. LRU链表：内存回收
   - 未使用的dentry链入LRU
   - 内存压力时从LRU释放

3. 超级块链表：按文件系统组织
   - sb->s_dentry_lru

/* 查找dentry */
struct dentry *d_lookup(const struct dentry *parent, 
                        const struct qstr *name)
{
    unsigned int hash = name->hash;
    struct hlist_bl_head *b = d_hash(parent, hash);
    struct hlist_bl_node *node;
    struct dentry *dentry;
    
    rcu_read_lock();
    hlist_bl_for_each_entry_rcu(dentry, node, b, d_hash) {
        if (dentry->d_name.hash != hash)
            continue;
        if (dentry->d_parent != parent)
            continue;
        if (d_unhashed(dentry))
            continue;
        if (!d_same_name(dentry, parent, name))
            continue;
        /* 找到了 */
        spin_lock(&dentry->d_lock);
        if (d_unhashed(dentry)) {
            spin_unlock(&dentry->d_lock);
            continue;
        }
        dentry->d_count++;
        spin_unlock(&dentry->d_lock);
        rcu_read_unlock();
        return dentry;
    }
    rcu_read_unlock();
    return NULL;
}

/* dcache收缩 */
void shrink_dcache_sb(struct super_block *sb);
void shrink_dcache_parent(struct dentry *parent);
long prune_dcache_sb(struct super_block *sb, long nr_to_scan, int nid);
```

---

## struct file_operations (文件操作)

Q: [Basic] struct file_operations定义了哪些操作？
A: file_operations是文件操作的虚函数表：
```c
/* include/linux/fs.h */
struct file_operations {
    struct module *owner;
    
    /* 定位 */
    loff_t (*llseek)(struct file *, loff_t, int);
    
    /* 同步I/O */
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    
    /* 异步I/O */
    ssize_t (*aio_read)(struct kiocb *, const struct iovec *, 
                        unsigned long, loff_t);
    ssize_t (*aio_write)(struct kiocb *, const struct iovec *, 
                         unsigned long, loff_t);
    
    /* 新式异步I/O (5.x+) */
    ssize_t (*read_iter)(struct kiocb *, struct iov_iter *);
    ssize_t (*write_iter)(struct kiocb *, struct iov_iter *);
    
    /* 目录遍历 */
    int (*readdir)(struct file *, void *, filldir_t);
    int (*iterate)(struct file *, struct dir_context *);  // 新式
    
    /* 事件等待 */
    unsigned int (*poll)(struct file *, struct poll_table_struct *);
    
    /* 控制操作 */
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
    
    /* 内存映射 */
    int (*mmap)(struct file *, struct vm_area_struct *);
    
    /* 打开和关闭 */
    int (*open)(struct inode *, struct file *);
    int (*flush)(struct file *, fl_owner_t id);
    int (*release)(struct inode *, struct file *);
    
    /* 同步 */
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);
    int (*aio_fsync)(struct kiocb *, int datasync);
    
    /* 异步通知 */
    int (*fasync)(int, struct file *, int);
    
    /* 文件锁 */
    int (*lock)(struct file *, int, struct file_lock *);
    int (*flock)(struct file *, int, struct file_lock *);
    
    /* 零拷贝 */
    ssize_t (*sendpage)(struct file *, struct page *, int, size_t, 
                        loff_t *, int);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, 
                            loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, 
                           struct pipe_inode_info *, size_t, unsigned int);
    
    /* 空间预分配 */
    long (*fallocate)(struct file *, int mode, loff_t offset, loff_t len);
};
```

Q: [Intermediate] read和write回调的参数含义是什么？
A: read/write是最基本的I/O操作：
```c
ssize_t (*read)(struct file *filp,     // 文件对象
                char __user *buf,       // 用户空间缓冲区
                size_t count,           // 请求读取字节数
                loff_t *ppos);          // 文件位置指针

ssize_t (*write)(struct file *filp,
                 const char __user *buf,
                 size_t count,
                 loff_t *ppos);

/* 实现示例 */
static ssize_t my_read(struct file *filp, char __user *buf,
                       size_t count, loff_t *ppos)
{
    struct my_device *dev = filp->private_data;
    size_t available = dev->size - *ppos;
    size_t to_read = min(count, available);
    
    if (*ppos >= dev->size)
        return 0;  // EOF
    
    /* 复制数据到用户空间 */
    if (copy_to_user(buf, dev->data + *ppos, to_read))
        return -EFAULT;
    
    *ppos += to_read;
    return to_read;  // 返回实际读取字节数
}

static ssize_t my_write(struct file *filp, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    struct my_device *dev = filp->private_data;
    size_t space = dev->size - *ppos;
    size_t to_write = min(count, space);
    
    if (to_write == 0)
        return -ENOSPC;
    
    /* 从用户空间复制数据 */
    if (copy_from_user(dev->data + *ppos, buf, to_write))
        return -EFAULT;
    
    *ppos += to_write;
    return to_write;
}
```

Q: [Intermediate] open和release回调的作用是什么？
A: open/release管理文件打开和关闭：
```c
int (*open)(struct inode *inode, struct file *filp);
int (*release)(struct inode *inode, struct file *filp);

/* open回调时机：每次打开文件时调用 */
static int my_open(struct inode *inode, struct file *filp)
{
    struct my_device *dev;
    
    /* 获取设备结构 */
    dev = container_of(inode->i_cdev, struct my_device, cdev);
    filp->private_data = dev;  // 保存到file->private_data
    
    /* 检查打开模式 */
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        /* 只写模式 */
    }
    
    /* 增加使用计数 */
    try_module_get(THIS_MODULE);
    
    return 0;  // 成功
}

/* release回调时机：最后一个引用关闭时调用 */
static int my_release(struct inode *inode, struct file *filp)
{
    struct my_device *dev = filp->private_data;
    
    /* 清理工作 */
    /* 注意：不是每次close都调用，只有f_count降为0时才调用 */
    
    module_put(THIS_MODULE);
    return 0;
}

/* open与release的调用次数关系 */
// 如果进程A打开文件，fork创建进程B
// 则open调用1次
// A关闭：不调用release（B还持有）
// B关闭：调用release
```

---

## struct inode_operations (inode操作)

Q: [Intermediate] struct inode_operations定义了哪些操作？
A: inode_operations处理inode级别的操作：
```c
/* include/linux/fs.h */
struct inode_operations {
    /* 查找 */
    struct dentry *(*lookup)(struct inode *, struct dentry *, 
                             unsigned int);
    
    /* 符号链接 */
    void *(*follow_link)(struct dentry *, struct nameidata *);
    const char *(*get_link)(struct dentry *, struct inode *,
                            struct delayed_call *);
    int (*readlink)(struct dentry *, char __user *, int);
    void (*put_link)(struct dentry *, struct nameidata *, void *);
    
    /* 权限检查 */
    int (*permission)(struct inode *, int);
    struct posix_acl *(*get_acl)(struct inode *, int);
    
    /* 创建和删除 */
    int (*create)(struct inode *, struct dentry *, umode_t, bool);
    int (*link)(struct dentry *, struct inode *, struct dentry *);
    int (*unlink)(struct inode *, struct dentry *);
    int (*symlink)(struct inode *, struct dentry *, const char *);
    int (*mkdir)(struct inode *, struct dentry *, umode_t);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*mknod)(struct inode *, struct dentry *, umode_t, dev_t);
    int (*rename)(struct inode *, struct dentry *,
                  struct inode *, struct dentry *);
    
    /* 属性操作 */
    int (*setattr)(struct dentry *, struct iattr *);
    int (*getattr)(struct vfsmount *, struct dentry *, struct kstat *);
    
    /* 扩展属性 */
    int (*setxattr)(struct dentry *, const char *, const void *,
                    size_t, int);
    ssize_t (*getxattr)(struct dentry *, const char *, void *, size_t);
    ssize_t (*listxattr)(struct dentry *, char *, size_t);
    int (*removexattr)(struct dentry *, const char *);
    
    /* 文件映射 */
    int (*fiemap)(struct inode *, struct fiemap_extent_info *, 
                  u64 start, u64 len);
    
    /* 原子打开 */
    int (*atomic_open)(struct inode *, struct dentry *,
                       struct file *, unsigned open_flag,
                       umode_t create_mode, int *opened);
};

/* 目录的典型inode_operations */
const struct inode_operations ext4_dir_inode_operations = {
    .create     = ext4_create,
    .lookup     = ext4_lookup,
    .link       = ext4_link,
    .unlink     = ext4_unlink,
    .symlink    = ext4_symlink,
    .mkdir      = ext4_mkdir,
    .rmdir      = ext4_rmdir,
    .mknod      = ext4_mknod,
    .rename     = ext4_rename,
    .setattr    = ext4_setattr,
    .getattr    = ext4_getattr,
    .setxattr   = generic_setxattr,
    .getxattr   = generic_getxattr,
    .listxattr  = ext4_listxattr,
    .removexattr = generic_removexattr,
    .permission = ext4_permission,
};
```

---

## 文件打开流程 (File Open Process)

Q: [Advanced] open系统调用的完整流程是什么？
A: open()经过VFS和具体文件系统：
```c
/* 系统调用入口 */
SYSCALL_DEFINE3(open, const char __user *, filename, 
                int, flags, umode_t, mode)
{
    return do_sys_open(AT_FDCWD, filename, flags, mode);
}

/* 主要流程 */
do_sys_open()
    │
    ├─→ get_unused_fd_flags()    // 分配fd
    │
    └─→ do_filp_open()           // 打开文件
            │
            ├─→ path_openat()    // 路径查找+打开
            │       │
            │       ├─→ path_init()      // 初始化查找
            │       ├─→ link_path_walk() // 逐级解析路径
            │       │       │
            │       │       └─→ walk_component()
            │       │               │
            │       │               └─→ lookup_fast()  // dcache查找
            │       │                   或 lookup_slow() // 磁盘查找
            │       │                       │
            │       │                       └─→ inode->i_op->lookup()
            │       │
            │       └─→ do_last()        // 处理最后一个组件
            │               │
            │               ├─→ lookup_open()    // 查找或创建
            │               │       │
            │               │       └─→ inode->i_op->create() // 如果O_CREAT
            │               │
            │               └─→ vfs_open()       // 实际打开
            │                       │
            │                       └─→ do_dentry_open()
            │                               │
            │                               ├─→ file->f_op->open()
            │                               └─→ 设置file结构
            │
            └─→ fd_install(fd, file)     // 安装到fd表

/* do_dentry_open */
static int do_dentry_open(struct file *f,
                          struct inode *inode,
                          int (*open)(struct inode *, struct file *))
{
    f->f_mode = OPEN_FMODE(f->f_flags) | FMODE_LSEEK | 
                FMODE_PREAD | FMODE_PWRITE;
    
    /* 设置file_operations */
    f->f_op = fops_get(inode->i_fop);
    
    /* 调用文件系统的open */
    if (open)
        error = open(inode, f);
    else if (f->f_op && f->f_op->open)
        error = f->f_op->open(inode, f);
    
    return error;
}
```

Q: [Intermediate] 路径查找(path lookup)的过程是什么？
A: 路径查找逐级解析目录：
```c
/* 路径 "/home/user/file" 的查找过程 */

1. path_init(): 从根目录或当前目录开始
   nd->path = current->fs->root (或pwd)

2. link_path_walk(): 循环处理每个组件
   
   处理 "home":
   ├─→ lookup_fast(): 查dcache
   │   └─→ __d_lookup(): 哈希表查找
   │
   └─→ lookup_slow(): dcache未命中
       └─→ dir->i_op->lookup(dir, dentry, flags)
           └─→ ext4_lookup(): 读取磁盘目录
   
   处理 "user":
   └─→ (同上)
   
3. do_last(): 处理最后的 "file"
   ├─→ 如果O_CREAT且不存在：创建
   └─→ 打开文件

/* 特殊情况处理 */
- 符号链接：follow_link()展开
- 挂载点：follow_mount()切换文件系统
- ".."：回到父目录
- 权限检查：inode->i_op->permission()

/* RCU路径查找优化 */
// 尝试在RCU保护下完成查找，避免锁
lookup_fast() → __d_lookup_rcu()
如果失败则退回到加锁路径
```

---

## 文件读写流程 (Read/Write Process)

Q: [Intermediate] read系统调用的内核实现流程是什么？
A: read()经过VFS、页缓存到具体文件系统：
```c
/* 系统调用入口 */
SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
    struct fd f = fdget_pos(fd);
    ssize_t ret;
    
    if (f.file) {
        loff_t pos = file_pos_read(f.file);
        ret = vfs_read(f.file, buf, count, &pos);
        if (ret >= 0)
            file_pos_write(f.file, pos);
        fdput_pos(f);
    }
    return ret;
}

/* VFS层处理 */
ssize_t vfs_read(struct file *file, char __user *buf, 
                 size_t count, loff_t *pos)
{
    /* 权限检查 */
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    if (!file->f_op->read && !file->f_op->aio_read)
        return -EINVAL;
    
    /* 调用具体实现 */
    if (file->f_op->read)
        ret = file->f_op->read(file, buf, count, pos);
    else
        ret = do_sync_read(file, buf, count, pos);
    
    /* 更新访问时间 */
    if (ret > 0)
        fsnotify_access(file);
    
    return ret;
}

/* 普通文件的read - 使用页缓存 */
ssize_t generic_file_aio_read(struct kiocb *iocb, 
                               const struct iovec *iov,
                               unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
    struct address_space *mapping = filp->f_mapping;
    
    /* 直接I/O */
    if (filp->f_flags & O_DIRECT) {
        return mapping->a_ops->direct_IO(READ, iocb, iov, pos, nr_segs);
    }
    
    /* 缓冲I/O - 通过页缓存 */
    return do_generic_file_read(filp, &pos, iov, nr_segs, pos);
}

/* 页缓存读取流程 */
do_generic_file_read()
    │
    ├─→ find_get_page()     // 在页缓存中查找
    │
    ├─→ page_cache_sync_readahead()  // 预读
    │
    └─→ 如果页不在缓存中
            │
            └─→ mapping->a_ops->readpage()  // 从磁盘读取
                    │
                    └─→ ext4_readpage()
                            │
                            └─→ submit_bio()  // 提交I/O请求
```

Q: [Intermediate] write系统调用的内核实现流程是什么？
A: write()通常先写入页缓存：
```c
/* VFS层处理 */
ssize_t vfs_write(struct file *file, const char __user *buf,
                  size_t count, loff_t *pos)
{
    /* 权限检查 */
    if (!(file->f_mode & FMODE_WRITE))
        return -EBADF;
    
    /* 文件大小限制检查 */
    ret = generic_write_checks(file, pos, &count, 0);
    if (ret)
        return ret;
    
    /* 调用具体实现 */
    if (file->f_op->write)
        ret = file->f_op->write(file, buf, count, pos);
    else
        ret = do_sync_write(file, buf, count, pos);
    
    /* 更新修改时间 */
    if (ret > 0)
        fsnotify_modify(file);
    
    return ret;
}

/* 普通文件的write - 使用页缓存 */
ssize_t generic_file_aio_write(struct kiocb *iocb, 
                                const struct iovec *iov,
                                unsigned long nr_segs, loff_t pos)
{
    struct file *file = iocb->ki_filp;
    struct address_space *mapping = file->f_mapping;
    
    /* 直接I/O */
    if (file->f_flags & O_DIRECT) {
        return generic_file_direct_write(iocb, iov, &nr_segs, pos);
    }
    
    /* 缓冲I/O */
    return __generic_file_aio_write(iocb, iov, nr_segs, &iocb->ki_pos);
}

/* 页缓存写入流程 */
__generic_file_aio_write()
    │
    ├─→ generic_perform_write()
    │       │
    │       └─→ for each page:
    │               ├─→ a_ops->write_begin()  // 准备页
    │               ├─→ iov_iter_copy_from_user_atomic()  // 复制数据
    │               └─→ a_ops->write_end()    // 标记脏页
    │
    └─→ generic_write_sync()  // 如果O_SYNC，同步写入
            │
            └─→ vfs_fsync_range()

/* 脏页最终写回 */
// 由pdflush/flush内核线程定期执行
// 或内存压力时由回收触发
// 或fsync显式触发
writeback_single_inode()
    └─→ do_writepages()
            └─→ mapping->a_ops->writepages()
                    └─→ ext4_writepages()
```

---

## 页缓存 (Page Cache)

Q: [Intermediate] struct address_space是什么？
A: address_space管理文件的页缓存：
```c
/* include/linux/fs.h */
struct address_space {
    struct inode        *host;       // 所属inode
    struct radix_tree_root page_tree; // 页缓存radix tree
    spinlock_t          tree_lock;   // tree锁
    atomic_t            i_mmap_writable; // 可写映射计数
    struct rb_root      i_mmap;      // 私有映射
    struct list_head    i_mmap_nonlinear; // 非线性映射
    struct mutex        i_mmap_mutex;
    unsigned long       nrpages;     // 页数
    pgoff_t             writeback_index; // 写回起始
    const struct address_space_operations *a_ops; // 操作函数
    unsigned long       flags;       // 标志
    struct backing_dev_info *backing_dev_info;
    spinlock_t          private_lock;
    struct list_head    private_list;
    void                *private_data;
};

/* address_space_operations */
struct address_space_operations {
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    int (*readpage)(struct file *, struct page *);
    int (*writepages)(struct address_space *, struct writeback_control *);
    
    /* 页状态管理 */
    int (*set_page_dirty)(struct page *page);
    int (*readpages)(struct file *, struct address_space *,
                     struct list_head *, unsigned);
    
    /* 写操作 */
    int (*write_begin)(struct file *, struct address_space *,
                       loff_t, unsigned, unsigned,
                       struct page **, void **);
    int (*write_end)(struct file *, struct address_space *,
                     loff_t, unsigned, unsigned,
                     struct page *, void *);
    
    /* 直接I/O */
    ssize_t (*direct_IO)(int, struct kiocb *, const struct iovec *,
                         loff_t, unsigned long);
    
    /* 迁移和交换 */
    int (*migratepage)(struct address_space *, struct page *,
                       struct page *, enum migrate_mode);
    int (*launder_page)(struct page *);
    int (*is_partially_uptodate)(struct page *, read_descriptor_t *,
                                  unsigned long);
    int (*error_remove_page)(struct address_space *, struct page *);
    int (*swap_activate)(struct swap_info_struct *, struct file *,
                         sector_t *);
    void (*swap_deactivate)(struct file *);
};
```

Q: [Advanced] 页缓存查找和添加的过程是什么？
A: 使用radix tree管理页缓存：
```c
/* 查找页 */
struct page *find_get_page(struct address_space *mapping, pgoff_t offset)
{
    struct page *page;
    
    rcu_read_lock();
repeat:
    page = radix_tree_lookup(&mapping->page_tree, offset);
    if (page) {
        if (!page_cache_get_speculative(page))
            goto repeat;
        if (unlikely(page != radix_tree_lookup(&mapping->page_tree, offset))) {
            put_page(page);
            goto repeat;
        }
    }
    rcu_read_unlock();
    return page;
}

/* 添加页 */
int add_to_page_cache_lru(struct page *page, struct address_space *mapping,
                          pgoff_t offset, gfp_t gfp_mask)
{
    int ret;
    
    __SetPageLocked(page);
    ret = __add_to_page_cache_locked(page, mapping, offset, gfp_mask);
    if (unlikely(ret))
        __ClearPageLocked(page);
    else {
        /* 添加到LRU链表 */
        lru_cache_add(page);
    }
    return ret;
}

/* 页缓存操作示意 */
read请求 offset=4096 (第1页)
    │
    ├─→ find_get_page(mapping, 1)
    │       │
    │       └─→ radix_tree_lookup(&mapping->page_tree, 1)
    │
    ├─→ 如果找到：直接返回
    │
    └─→ 如果未找到：
            │
            ├─→ page = page_cache_alloc()
            ├─→ add_to_page_cache_lru(page, mapping, 1, gfp)
            └─→ mapping->a_ops->readpage(file, page)
```

---

## 文件锁 (File Locking)

Q: [Intermediate] Linux支持哪些类型的文件锁？
A: Linux支持多种文件锁机制：
```c
/* 1. POSIX锁 (fcntl) - 进程级 */
struct flock {
    short l_type;    // F_RDLCK, F_WRLCK, F_UNLCK
    short l_whence;  // SEEK_SET, SEEK_CUR, SEEK_END
    off_t l_start;   // 起始偏移
    off_t l_len;     // 长度(0=到EOF)
    pid_t l_pid;     // 锁持有进程
};
fcntl(fd, F_SETLK, &flock);   // 非阻塞
fcntl(fd, F_SETLKW, &flock);  // 阻塞
fcntl(fd, F_GETLK, &flock);   // 查询

/* 2. BSD锁 (flock) - 文件级 */
flock(fd, LOCK_SH);  // 共享锁
flock(fd, LOCK_EX);  // 排他锁
flock(fd, LOCK_UN);  // 解锁
flock(fd, LOCK_NB);  // 非阻塞(可与上述组合)

/* 3. 强制锁 (Mandatory Locking) */
// 需要文件系统支持: mount -o mand
// 文件设置setgid但不设置group-execute: chmod g+s,g-x file

/* 4. 租约锁 (Lease) - 用于缓存一致性 */
fcntl(fd, F_SETLEASE, F_RDLCK);  // 读租约
fcntl(fd, F_SETLEASE, F_WRLCK);  // 写租约
fcntl(fd, F_GETLEASE);           // 查询租约

/* 内核结构 */
struct file_lock {
    struct file_lock *fl_next;
    struct list_head fl_list;     // 链表
    struct hlist_node fl_link;    // 哈希链表
    struct list_head fl_block;    // 阻塞链表
    fl_owner_t fl_owner;          // 锁所有者
    unsigned int fl_flags;        // FL_POSIX, FL_FLOCK等
    unsigned char fl_type;        // 锁类型
    unsigned int fl_pid;          // 进程ID
    struct pid *fl_nspid;
    wait_queue_head_t fl_wait;    // 等待队列
    struct file *fl_file;         // 关联的文件
    loff_t fl_start;              // 起始位置
    loff_t fl_end;                // 结束位置
    /* ... */
};
```

Q: [Intermediate] POSIX锁和flock锁的区别是什么？
A: 两种锁机制有重要区别：
```
+------------------+-------------------+------------------+
|      特性        |    POSIX锁        |    flock锁       |
+------------------+-------------------+------------------+
| 锁粒度           | 字节范围          | 整个文件         |
| 锁所有者         | 进程级            | 文件描述符级     |
| fork继承         | 不继承            | 继承             |
| dup行为          | 共享锁(同一锁)    | 独立锁           |
| close释放        | 进程关闭任何fd    | 关闭该fd时       |
| NFS支持          | 是                | 通常不支持       |
+------------------+-------------------+------------------+

/* POSIX锁的特殊行为 */
// 进程打开同一文件两次
fd1 = open("file", O_RDWR);
fd2 = open("file", O_RDWR);

// 通过fd1加锁
fcntl(fd1, F_SETLK, &lock);

// 关闭fd2会释放fd1的锁！
close(fd2);  // 锁被释放

/* flock锁的行为 */
fd1 = open("file", O_RDWR);
fd2 = open("file", O_RDWR);

flock(fd1, LOCK_EX);

close(fd2);  // 不影响fd1的锁
flock(fd1, LOCK_UN);  // 显式解锁
```

---

## 内核文件操作API (Kernel File API)

Q: [Intermediate] 内核如何操作文件？
A: 内核提供专用的文件操作API：
```c
/* 打开文件 */
struct file *filp_open(const char *filename, int flags, umode_t mode);
struct file *file_open_root(struct dentry *root, struct vfsmount *mnt,
                            const char *filename, int flags);

/* 关闭文件 */
int filp_close(struct file *filp, fl_owner_t id);

/* 读取文件 */
ssize_t kernel_read(struct file *file, loff_t offset,
                    char *addr, unsigned long count);
ssize_t vfs_read(struct file *file, char __user *buf,
                 size_t count, loff_t *pos);

/* 写入文件 */
ssize_t kernel_write(struct file *file, const char *buf,
                     size_t count, loff_t pos);
ssize_t vfs_write(struct file *file, const char __user *buf,
                  size_t count, loff_t *pos);

/* 完整示例：内核读取文件 */
static int read_config_file(const char *path, char *buf, size_t size)
{
    struct file *f;
    mm_segment_t fs;
    loff_t pos = 0;
    ssize_t ret;

    /* 打开文件 */
    f = filp_open(path, O_RDONLY, 0);
    if (IS_ERR(f))
        return PTR_ERR(f);

    /* 切换地址空间限制 */
    fs = get_fs();
    set_fs(KERNEL_DS);

    /* 读取 */
    ret = vfs_read(f, buf, size - 1, &pos);
    if (ret > 0)
        buf[ret] = '\0';

    /* 恢复地址空间 */
    set_fs(fs);

    /* 关闭文件 */
    filp_close(f, NULL);

    return ret;
}

/* 新式API (5.x+) - 使用kernel_read不需要set_fs */
ssize_t ret = kernel_read(f, buf, size, &pos);
```

Q: [Advanced] get_fs/set_fs的作用是什么？
A: 控制用户/内核地址空间边界：
```c
/* 问题：vfs_read/vfs_write期望用户空间地址 */
ssize_t vfs_read(struct file *file, char __user *buf, ...)

/* 但内核传递的是内核空间地址 */
char kernel_buf[100];
vfs_read(f, kernel_buf, 100, &pos);  // 错误！

/* 解决方案：临时修改地址空间限制 */
mm_segment_t old_fs = get_fs();  // 保存当前限制
set_fs(KERNEL_DS);               // 允许内核地址

// 现在可以传递内核地址
vfs_read(f, kernel_buf, 100, &pos);

set_fs(old_fs);  // 恢复限制

/* KERNEL_DS vs USER_DS */
// USER_DS: 只允许用户空间地址
// KERNEL_DS: 允许内核空间地址

/* 安全警告 */
// 使用set_fs(KERNEL_DS)时要小心：
// 1. 可能被利用做权限提升攻击
// 2. 需要确保恢复old_fs
// 3. 新内核(5.10+)已移除set_fs，使用kernel_read/kernel_write
```

---

## 异步I/O (Asynchronous I/O)

Q: [Intermediate] Linux内核支持哪些异步I/O机制？
A: Linux提供多种异步I/O方式：
```c
/* 1. POSIX AIO (libaio) */
struct iocb {
    __u64 aio_data;      // 用户数据
    __u32 aio_key;       // 内核使用
    __u32 aio_reserved1;
    __u16 aio_lio_opcode; // 操作码
    __s16 aio_reqprio;   // 优先级
    __u32 aio_fildes;    // 文件描述符
    __u64 aio_buf;       // 缓冲区
    __u64 aio_nbytes;    // 字节数
    __s64 aio_offset;    // 偏移
    /* ... */
};

io_setup(nr_events, &ctx);  // 创建上下文
io_submit(ctx, nr, iocbs);  // 提交I/O
io_getevents(ctx, min, max, events, timeout);  // 等待完成
io_destroy(ctx);            // 销毁上下文

/* 2. io_uring (5.1+) - 高性能异步I/O */
// 基于共享内存的提交/完成队列
// 支持polling模式
// 减少系统调用开销

/* 内核结构 */
struct kiocb {
    struct file     *ki_filp;    // 文件
    loff_t          ki_pos;      // 位置
    void (*ki_complete)(struct kiocb *, long, long);  // 完成回调
    void            *private;
    int             ki_flags;    // 标志
};

/* file_operations中的AIO接口 */
ssize_t (*aio_read)(struct kiocb *, const struct iovec *,
                    unsigned long, loff_t);
ssize_t (*aio_write)(struct kiocb *, const struct iovec *,
                     unsigned long, loff_t);

/* 新式接口 */
ssize_t (*read_iter)(struct kiocb *, struct iov_iter *);
ssize_t (*write_iter)(struct kiocb *, struct iov_iter *);
```

Q: [Advanced] 直接I/O (Direct I/O)是什么？
A: 直接I/O绕过页缓存：
```c
/* 打开时指定O_DIRECT */
int fd = open("/path/file", O_RDWR | O_DIRECT);

/* 要求 */
1. 用户缓冲区地址对齐（通常512或4096字节）
2. 读写长度对齐
3. 文件偏移对齐
4. 文件系统支持

/* 内核实现 */
// generic_file_aio_read/write中
if (filp->f_flags & O_DIRECT) {
    // 调用direct_IO
    return mapping->a_ops->direct_IO(rw, iocb, iov, pos, nr_segs);
}

/* address_space_operations::direct_IO */
// ext4的实现
static ssize_t ext4_direct_IO(int rw, struct kiocb *iocb,
                               const struct iovec *iov,
                               loff_t offset, unsigned long nr_segs)
{
    struct file *file = iocb->ki_filp;
    struct inode *inode = file->f_mapping->host;
    
    /* 使用块设备层直接I/O */
    return blockdev_direct_IO(rw, iocb, inode, iov, offset, nr_segs,
                              ext4_get_block);
}

/* 优缺点 */
优点：
- 避免双重缓冲（数据库有自己的缓存）
- 减少内存拷贝
- 控制I/O时机

缺点：
- 不能利用页缓存
- 对齐要求严格
- 预读优化失效
```

---

## 错误处理和调试 (Error Handling and Debugging)

Q: [Intermediate] 文件操作中常见的错误码有哪些？
A: 文件系统相关的错误码：
```c
/* 常见错误码 */
ENOENT   2   // 文件不存在
EACCES  13   // 权限拒绝
EEXIST  17   // 文件已存在
ENOTDIR 20   // 不是目录
EISDIR  21   // 是目录
EINVAL  22   // 无效参数
EMFILE  24   // 进程打开太多文件
ENFILE  23   // 系统打开太多文件
EFBIG   27   // 文件过大
ENOSPC  28   // 空间不足
EROFS   30   // 只读文件系统
ENAMETOOLONG 36 // 文件名过长
ENOTEMPTY 39 // 目录非空
ELOOP   40   // 符号链接循环
ESTALE 116   // NFS过期句柄

/* 错误检查宏 */
if (IS_ERR(ptr))         // 检查错误指针
    return PTR_ERR(ptr); // 转换为错误码

ptr = ERR_PTR(-ENOMEM);  // 创建错误指针
err = PTR_ERR(ptr);      // 提取错误码

/* 示例 */
struct file *f = filp_open("/path", O_RDONLY, 0);
if (IS_ERR(f)) {
    pr_err("Failed to open: %ld\n", PTR_ERR(f));
    return PTR_ERR(f);
}
```

Q: [Intermediate] 如何调试VFS问题？
A: VFS调试技术：
```bash
# 1. ftrace跟踪VFS函数
$ echo 'vfs_*' >> /sys/kernel/debug/tracing/set_ftrace_filter
$ echo function > /sys/kernel/debug/tracing/current_tracer
$ cat /sys/kernel/debug/tracing/trace

# 2. 跟踪文件操作
$ echo 1 > /sys/kernel/debug/tracing/events/syscalls/sys_enter_open/enable
$ echo 1 > /sys/kernel/debug/tracing/events/syscalls/sys_enter_read/enable

# 3. 查看文件打开信息
$ cat /proc/<pid>/fd      # 进程打开的fd
$ cat /proc/<pid>/fdinfo/<fd>  # fd详细信息

# 4. inode信息
$ stat <file>
$ ls -li <file>          # 显示inode号

# 5. 内核调试选项
CONFIG_DEBUG_FS=y
CONFIG_VFS_DEBUG=y

# 6. 使用debugfs
$ mount -t debugfs none /sys/kernel/debug
$ ls /sys/kernel/debug/block/
$ cat /sys/kernel/debug/bdi/*/stats

# 7. bpftrace追踪
$ bpftrace -e 'kprobe:vfs_read { @[comm] = count(); }'
```

---

## 最佳实践 (Best Practices)

Q: [Advanced] 实现file_operations的最佳实践是什么？
A: 驱动程序实现file_operations的模式：
```c
/* 1. 基本模板 */
static const struct file_operations my_fops = {
    .owner          = THIS_MODULE,
    .open           = my_open,
    .release        = my_release,
    .read           = my_read,
    .write          = my_write,
    .unlocked_ioctl = my_ioctl,
    .llseek         = my_llseek,
    .poll           = my_poll,
    .mmap           = my_mmap,
};

/* 2. 使用通用实现 */
// 对于简单需求，使用内核提供的通用函数
.llseek  = default_llseek,      // 或 no_llseek, noop_llseek
.read    = simple_read_from_buffer,
.write   = simple_write_to_buffer,

/* 3. open中保存设备上下文 */
static int my_open(struct inode *inode, struct file *filp)
{
    struct my_device *dev;
    
    dev = container_of(inode->i_cdev, struct my_device, cdev);
    filp->private_data = dev;
    
    return nonseekable_open(inode, filp);  // 如果不支持seek
}

/* 4. 正确的引用计数 */
static int my_open(struct inode *inode, struct file *filp)
{
    if (!try_module_get(THIS_MODULE))
        return -ENODEV;
    /* ... */
    return 0;
}

static int my_release(struct inode *inode, struct file *filp)
{
    /* 清理 */
    module_put(THIS_MODULE);
    return 0;
}

/* 5. 正确处理信号 */
static ssize_t my_read(struct file *filp, char __user *buf,
                       size_t count, loff_t *ppos)
{
    if (wait_event_interruptible(dev->waitq, data_available))
        return -ERESTARTSYS;  // 可重启的系统调用
    /* ... */
}

/* 6. 用户空间数据复制 */
// 总是检查copy_to_user/copy_from_user的返回值
if (copy_to_user(buf, kernel_buf, len))
    return -EFAULT;

/* 7. 支持poll/select */
static unsigned int my_poll(struct file *filp, poll_table *wait)
{
    unsigned int mask = 0;
    
    poll_wait(filp, &dev->inq, wait);
    poll_wait(filp, &dev->outq, wait);
    
    if (data_available)
        mask |= POLLIN | POLLRDNORM;
    if (write_space_available)
        mask |= POLLOUT | POLLWRNORM;
    
    return mask;
}
```

Q: [Intermediate] 文件系统实现中inode和dentry操作的最佳实践？
A: 文件系统实现模式：
```c
/* 1. inode分配和释放 */
static struct inode *my_alloc_inode(struct super_block *sb)
{
    struct my_inode *mi;
    mi = kmem_cache_alloc(my_inode_cachep, GFP_KERNEL);
    if (!mi)
        return NULL;
    return &mi->vfs_inode;
}

static void my_destroy_inode(struct inode *inode)
{
    struct my_inode *mi = MY_INODE(inode);
    kmem_cache_free(my_inode_cachep, mi);
}

/* 2. 正确初始化inode */
static struct inode *my_get_inode(struct super_block *sb,
                                   umode_t mode, dev_t dev)
{
    struct inode *inode = new_inode(sb);
    if (!inode)
        return NULL;
    
    inode->i_ino = get_next_ino();
    inode_init_owner(inode, NULL, mode);
    inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
    
    switch (mode & S_IFMT) {
    case S_IFREG:
        inode->i_op = &my_file_inode_operations;
        inode->i_fop = &my_file_operations;
        break;
    case S_IFDIR:
        inode->i_op = &my_dir_inode_operations;
        inode->i_fop = &my_dir_operations;
        set_nlink(inode, 2);
        break;
    default:
        init_special_inode(inode, mode, dev);
        break;
    }
    
    return inode;
}

/* 3. lookup实现 */
static struct dentry *my_lookup(struct inode *dir, struct dentry *dentry,
                                unsigned int flags)
{
    struct inode *inode = NULL;
    
    /* 在目录中查找名字 */
    struct my_entry *entry = my_find_entry(dir, &dentry->d_name);
    
    if (entry) {
        /* 找到了，获取或创建inode */
        inode = my_iget(dir->i_sb, entry->ino);
        if (IS_ERR(inode))
            return ERR_CAST(inode);
    }
    
    /* 关联dentry和inode (inode可为NULL表示负dentry) */
    return d_splice_alias(inode, dentry);
}

/* 4. create实现 */
static int my_create(struct inode *dir, struct dentry *dentry,
                     umode_t mode, bool excl)
{
    struct inode *inode;
    int ret;
    
    inode = my_get_inode(dir->i_sb, mode, 0);
    if (!inode)
        return -ENOMEM;
    
    /* 添加到目录 */
    ret = my_add_entry(dir, &dentry->d_name, inode->i_ino);
    if (ret) {
        iput(inode);
        return ret;
    }
    
    /* 关联dentry */
    d_instantiate(dentry, inode);
    mark_inode_dirty(dir);
    
    return 0;
}
```

