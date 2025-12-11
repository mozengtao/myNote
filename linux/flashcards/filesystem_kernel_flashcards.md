# File System Management Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel VFS and file system internals, data structures, and APIs
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. VFS Overview (虚拟文件系统概述)

---

Q: What is VFS (Virtual File System) and why is it needed?
A: VFS是Linux内核的文件系统抽象层：

```
+----------------------------------------------------------+
|                     User Space                            |
|        open() / read() / write() / close()               |
+----------------------------------------------------------+
                          |
                    System Call
                          |
                          v
+----------------------------------------------------------+
|                 VFS (Virtual File System)                 |
|     统一的文件操作接口，隐藏具体文件系统差异               |
|                                                          |
|   superblock | inode | dentry | file                     |
+----------------------------------------------------------+
          |           |           |           |
          v           v           v           v
     +--------+  +--------+  +--------+  +--------+
     |  ext4  |  |  xfs   |  |  nfs   |  | procfs |
     +--------+  +--------+  +--------+  +--------+
          |           |           |
          v           v           v
     +--------+  +--------+  +--------+
     | Block  |  | Block  |  | Network|
     | Device |  | Device |  |        |
     +--------+  +--------+  +--------+
```

**作用**：
1. 为用户提供统一的文件操作API
2. 支持多种文件系统共存
3. 抽象底层存储差异
[Basic]

---

Q: What are the four core VFS objects?
A: VFS的四个核心对象：

| 对象 | 作用 | 生命周期 |
|------|------|----------|
| **superblock** | 描述已挂载的文件系统 | 挂载到卸载 |
| **inode** | 描述文件元数据 | 文件存在期间 |
| **dentry** | 目录项，连接文件名和inode | 缓存在内存 |
| **file** | 描述打开的文件实例 | open到close |

```
关系：
superblock (文件系统)
    |
    +---> inode (文件1)
    |        |
    |        +---> dentry ("/home/user/file1")
    |                 |
    |                 +---> file (进程A打开)
    |                 +---> file (进程B打开)
    |
    +---> inode (文件2)
             |
             +---> dentry ("/etc/passwd")
```
[Basic]

---

Q: What is the relationship between these VFS structures?
A: 
```
+------------------+
|   struct file    |  每次open()创建一个
| +-------------+  |
| | f_path      |--+--> dentry
| | f_inode     |--+--> inode
| | f_op        |--+--> file_operations
| | f_pos       |  |   当前读写位置
| | private_data|  |   驱动私有数据
| +-------------+  |
+------------------+
        |
        v
+------------------+
|  struct dentry   |  目录项缓存
| +-------------+  |
| | d_name      |  |   文件名
| | d_inode     |--+--> inode
| | d_parent    |--+--> 父目录dentry
| | d_sb        |--+--> superblock
| | d_op        |--+--> dentry_operations
| +-------------+  |
+------------------+
        |
        v
+------------------+
|  struct inode    |  文件元数据
| +-------------+  |
| | i_mode      |  |   类型和权限
| | i_uid/i_gid |  |   所有者
| | i_size      |  |   文件大小
| | i_sb        |--+--> superblock
| | i_op        |--+--> inode_operations
| | i_fop       |--+--> file_operations
| +-------------+  |
+------------------+
        |
        v
+------------------+
| struct super_block| 文件系统实例
| +-------------+  |
| | s_type      |--+--> file_system_type
| | s_op        |--+--> super_operations
| | s_root      |--+--> 根目录dentry
| | s_bdev      |--+--> 块设备
| +-------------+  |
+------------------+
```
[Intermediate]

---

## 2. Superblock (超级块)

---

Q: What is `struct super_block` and its key fields?
A: `super_block`描述一个已挂载的文件系统实例：
```c
struct super_block {
    struct list_head    s_list;         // 全局超级块链表
    dev_t               s_dev;          // 设备号
    unsigned long       s_blocksize;    // 块大小
    loff_t              s_maxbytes;     // 最大文件大小
    struct file_system_type *s_type;    // 文件系统类型
    const struct super_operations *s_op; // 超级块操作
    
    unsigned long       s_flags;        // 挂载标志
    unsigned long       s_magic;        // 魔数
    struct dentry       *s_root;        // 根目录dentry
    
    struct rw_semaphore s_umount;       // 卸载信号量
    int                 s_count;        // 引用计数
    atomic_t            s_active;       // 活跃引用
    
    struct block_device *s_bdev;        // 块设备
    void                *s_fs_info;     // 文件系统私有数据
    
    struct list_head    s_inodes;       // 所有inode链表
    struct list_head    s_dirty;        // 脏inode链表
    // ...
};
```
每个挂载点对应一个super_block实例。
[Intermediate]

---

Q: What is `struct super_operations`?
A: `super_operations`定义超级块的操作函数：
```c
struct super_operations {
    // inode生命周期
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    void (*free_inode)(struct inode *);
    
    // inode同步
    void (*dirty_inode)(struct inode *, int flags);
    int (*write_inode)(struct inode *, struct writeback_control *);
    void (*drop_inode)(struct inode *);
    void (*evict_inode)(struct inode *);
    
    // 超级块操作
    void (*put_super)(struct super_block *);
    int (*sync_fs)(struct super_block *, int wait);
    int (*freeze_fs)(struct super_block *);
    int (*unfreeze_fs)(struct super_block *);
    int (*statfs)(struct dentry *, struct kstatfs *);
    int (*remount_fs)(struct super_block *, int *, char *);
    
    // 挂载/卸载
    void (*umount_begin)(struct super_block *);
    
    // 显示选项
    int (*show_options)(struct seq_file *, struct dentry *);
};
```

使用示例：
```c
static const struct super_operations myfs_super_ops = {
    .alloc_inode    = myfs_alloc_inode,
    .destroy_inode  = myfs_destroy_inode,
    .write_inode    = myfs_write_inode,
    .evict_inode    = myfs_evict_inode,
    .put_super      = myfs_put_super,
    .statfs         = myfs_statfs,
};
```
[Intermediate]

---

## 3. Inode (索引节点)

---

Q: What is `struct inode` and its key fields?
A: `inode`描述文件的元数据（不包含文件名）：
```c
struct inode {
    umode_t             i_mode;         // 文件类型和权限
    unsigned short      i_opflags;
    kuid_t              i_uid;          // 所有者UID
    kgid_t              i_gid;          // 所有者GID
    unsigned int        i_flags;
    
    const struct inode_operations *i_op;    // inode操作
    struct super_block  *i_sb;              // 所属超级块
    
    unsigned long       i_ino;          // inode号
    
    union {
        const unsigned int i_nlink;     // 硬链接数
        unsigned int __i_nlink;
    };
    dev_t               i_rdev;         // 设备号（设备文件）
    loff_t              i_size;         // 文件大小
    
    struct timespec64   i_atime;        // 访问时间
    struct timespec64   i_mtime;        // 修改时间
    struct timespec64   i_ctime;        // 状态改变时间
    
    spinlock_t          i_lock;
    unsigned long       i_state;        // 状态标志
    struct rw_semaphore i_rwsem;
    
    const struct file_operations *i_fop;    // 文件操作
    struct address_space *i_mapping;        // 页缓存
    struct address_space i_data;
    
    union {
        struct pipe_inode_info *i_pipe;     // 管道
        struct block_device *i_bdev;         // 块设备
        struct cdev *i_cdev;                 // 字符设备
    };
    
    void                *i_private;     // 私有数据
};
```
[Intermediate]

---

Q: What is `struct inode_operations`?
A: `inode_operations`定义inode的操作函数：
```c
struct inode_operations {
    // 查找
    struct dentry *(*lookup)(struct inode *, struct dentry *, unsigned int);
    
    // 创建
    int (*create)(struct inode *, struct dentry *, umode_t, bool);
    int (*mkdir)(struct inode *, struct dentry *, umode_t);
    int (*mknod)(struct inode *, struct dentry *, umode_t, dev_t);
    int (*symlink)(struct inode *, struct dentry *, const char *);
    
    // 链接
    int (*link)(struct dentry *, struct inode *, struct dentry *);
    int (*unlink)(struct inode *, struct dentry *);
    int (*rmdir)(struct inode *, struct dentry *);
    int (*rename)(struct inode *, struct dentry *, 
                  struct inode *, struct dentry *, unsigned int);
    
    // 符号链接
    const char *(*get_link)(struct dentry *, struct inode *,
                            struct delayed_call *);
    
    // 属性
    int (*permission)(struct inode *, int);
    int (*setattr)(struct dentry *, struct iattr *);
    int (*getattr)(const struct path *, struct kstat *, u32, unsigned int);
    
    // 扩展属性
    ssize_t (*listxattr)(struct dentry *, char *, size_t);
    
    // 文件范围操作
    int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64, u64);
    int (*update_time)(struct inode *, struct timespec64 *, int);
};
```

目录inode和普通文件inode使用不同的操作集。
[Intermediate]

---

Q: What is the difference between inode number and inode structure?
A: 
| 概念 | 说明 |
|------|------|
| **inode号** (i_ino) | 文件系统中唯一标识，持久化存储在磁盘 |
| **struct inode** | 内核内存中的inode表示，从磁盘读取后填充 |

```
磁盘上：
+------------------+
| Superblock       |
+------------------+
| Inode Table      |  inode号 = 表中偏移
| +-------------+  |
| | inode #1    |  |  磁盘上的inode结构
| | inode #2    |  |  （通常较小，如ext4=256字节）
| | inode #3    |  |
| +-------------+  |
+------------------+
| Data Blocks      |
+------------------+

内存中：
+------------------+
| struct inode     |  VFS通用结构（较大，~800字节）
| +-------------+  |
| | i_ino = 2   |  |  从磁盘读取的inode号
| | i_size      |  |  通用字段
| | i_private   |--+--> 文件系统特定数据
| +-------------+  |     (如ext4_inode_info)
+------------------+
```

通过inode号查找：
```c
struct inode *inode = iget_locked(sb, ino);
if (inode->i_state & I_NEW) {
    // 新分配的，需要从磁盘读取填充
    myfs_read_inode(inode);
    unlock_new_inode(inode);
}
```
[Intermediate]

---

## 4. Dentry (目录项)

---

Q: What is `struct dentry` and what is it used for?
A: `dentry`是目录项缓存，连接文件名和inode：
```c
struct dentry {
    unsigned int d_flags;               // 标志
    seqcount_t d_seq;
    struct hlist_bl_node d_hash;        // 哈希链表节点
    struct dentry *d_parent;            // 父目录
    struct qstr d_name;                 // 文件名
    struct inode *d_inode;              // 关联的inode（可为NULL）
    unsigned char d_iname[DNAME_INLINE_LEN]; // 短文件名内联存储
    
    struct lockref d_lockref;           // 引用计数和锁
    const struct dentry_operations *d_op; // dentry操作
    struct super_block *d_sb;           // 所属超级块
    
    unsigned long d_time;               // 重验证时间
    void *d_fsdata;                     // 文件系统私有数据
    
    union {
        struct list_head d_lru;         // LRU链表
        wait_queue_head_t *d_wait;
    };
    struct list_head d_child;           // 父目录的子项链表
    struct list_head d_subdirs;         // 子目录链表
    
    union {
        struct hlist_node d_alias;      // inode的别名链表
        struct rcu_head d_rcu;
    } d_u;
};
```

路径解析：`/home/user/file`
```
dentry "/"
   |
   +---> dentry "home" ---> inode(目录)
            |
            +---> dentry "user" ---> inode(目录)
                     |
                     +---> dentry "file" ---> inode(文件)
```
[Intermediate]

---

Q: What is the dentry cache (dcache)?
A: dentry缓存加速路径查找：
```
路径查找过程：/home/user/file

+------------------+
|   dcache (哈希表) |
|   +------------+ |
|   | "/"        |-+-> dentry -> inode
|   | "home"     |-+-> dentry -> inode
|   | "user"     |-+-> dentry -> inode
|   | "file"     |-+-> dentry -> inode
|   +------------+ |
+------------------+

命中：直接返回缓存的dentry
未命中：调用文件系统的lookup()，结果加入缓存
```

dcache状态：
| 状态 | 说明 |
|------|------|
| **使用中** | d_lockref.count > 0，有进程引用 |
| **未使用** | d_lockref.count = 0，在LRU链表 |
| **负dentry** | d_inode = NULL，缓存"不存在"的结果 |

查看dcache统计：
```bash
cat /proc/sys/fs/dentry-state
# 输出：nr_dentry nr_unused age_limit want_pages dummy dummy
```
[Intermediate]

---

Q: What is `struct dentry_operations`?
A: `dentry_operations`定义dentry的操作：
```c
struct dentry_operations {
    // 重新验证dentry是否仍然有效
    int (*d_revalidate)(struct dentry *, unsigned int);
    
    // 弱别名处理
    int (*d_weak_revalidate)(struct dentry *, unsigned int);
    
    // 计算哈希值
    int (*d_hash)(const struct dentry *, struct qstr *);
    
    // 比较文件名
    int (*d_compare)(const struct dentry *,
                     unsigned int, const char *, const struct qstr *);
    
    // 删除时调用
    int (*d_delete)(const struct dentry *);
    
    // 初始化
    int (*d_init)(struct dentry *);
    
    // 释放
    void (*d_release)(struct dentry *);
    
    // inode被释放时
    void (*d_iput)(struct dentry *, struct inode *);
    
    // 生成路径名
    char *(*d_dname)(struct dentry *, char *, int);
    
    // 自动挂载
    struct vfsmount *(*d_automount)(struct path *);
    
    // 管理锁
    int (*d_manage)(const struct path *, bool);
};
```

典型用途：
- 网络文件系统（NFS）的`d_revalidate`检查远程文件是否变化
- 大小写不敏感文件系统的`d_compare`
[Advanced]

---

## 5. File Structure (文件结构)

---

Q: What is `struct file` and its key fields?
A: `struct file`表示一个打开的文件实例：
```c
struct file {
    union {
        struct llist_node fu_llist;
        struct rcu_head fu_rcuhead;
    } f_u;
    
    struct path f_path;                 // 文件路径(dentry + vfsmount)
    struct inode *f_inode;              // 关联的inode
    const struct file_operations *f_op; // 文件操作
    
    spinlock_t f_lock;
    atomic_long_t f_count;              // 引用计数
    unsigned int f_flags;               // 打开标志 (O_RDONLY等)
    fmode_t f_mode;                     // 访问模式
    struct mutex f_pos_lock;
    loff_t f_pos;                       // 当前读写位置
    
    struct fown_struct f_owner;         // 异步I/O所有者
    const struct cred *f_cred;          // 凭证
    struct file_ra_state f_ra;          // 预读状态
    
    void *private_data;                 // 驱动私有数据
    struct address_space *f_mapping;    // 页缓存映射
    // ...
};
```

每次`open()`创建新的file结构，`fork()`后共享file结构。
[Basic]

---

Q: What is `struct file_operations`?
A: `file_operations`定义文件操作回调：
```c
struct file_operations {
    struct module *owner;
    
    // 定位
    loff_t (*llseek)(struct file *, loff_t, int);
    
    // 读写
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*read_iter)(struct kiocb *, struct iov_iter *);
    ssize_t (*write_iter)(struct kiocb *, struct iov_iter *);
    
    // 异步I/O
    int (*iopoll)(struct kiocb *, bool spin);
    
    // 目录遍历
    int (*iterate)(struct file *, struct dir_context *);
    int (*iterate_shared)(struct file *, struct dir_context *);
    
    // 轮询
    __poll_t (*poll)(struct file *, struct poll_table_struct *);
    
    // ioctl
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
    
    // 内存映射
    int (*mmap)(struct file *, struct vm_area_struct *);
    
    // 打开/关闭
    int (*open)(struct inode *, struct file *);
    int (*flush)(struct file *, fl_owner_t id);
    int (*release)(struct inode *, struct file *);
    
    // 同步
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);
    int (*fasync)(int, struct file *, int);
    
    // 锁
    int (*lock)(struct file *, int, struct file_lock *);
    int (*flock)(struct file *, int, struct file_lock *);
    
    // splice
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *,
                            loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *,
                           struct pipe_inode_info *, size_t, unsigned int);
};
```
[Basic]

---

Q: How is a file opened from user space to kernel?
A: open()系统调用的完整路径：
```
用户空间: open("/path/to/file", O_RDWR)
              |
              v
系统调用: sys_open() / sys_openat()
              |
              v
         do_sys_open()
              |
              v
         do_filp_open()
              |
              +---> path_openat()
                        |
                        v
                   路径解析 (link_path_walk)
                        |
                        v
                   查找/创建inode (lookup_open)
                        |
                        v
                   分配file结构
                        |
                        v
                   调用f_op->open()
                        |
                        v
                   安装fd (fd_install)
                        |
                        v
返回: 文件描述符 fd
```

关键函数：
```c
// 路径解析
int path_lookupat(struct nameidata *nd, unsigned flags, struct path *path);

// 分配file结构
struct file *alloc_file(const struct path *path, int flags,
                        const struct file_operations *fop);

// 安装fd
void fd_install(unsigned int fd, struct file *file);
```
[Intermediate]

---

## 6. File System Registration (文件系统注册)

---

Q: What is `struct file_system_type`?
A: `file_system_type`描述一种文件系统类型：
```c
struct file_system_type {
    const char *name;                   // 文件系统名 ("ext4", "xfs"等)
    int fs_flags;                       // 标志
    
    // 挂载回调
    struct dentry *(*mount)(struct file_system_type *, int,
                            const char *, void *);
    
    // 卸载时清理超级块
    void (*kill_sb)(struct super_block *);
    
    struct module *owner;               // 所属模块
    struct file_system_type *next;      // 链表
    struct hlist_head fs_supers;        // 该类型的所有超级块
    
    // ...
};

// 常用fs_flags
#define FS_REQUIRES_DEV      1    // 需要块设备
#define FS_BINARY_MOUNTDATA  2    // 二进制挂载数据
#define FS_HAS_SUBTYPE       4    // 有子类型（如fuse.sshfs）
#define FS_USERNS_MOUNT      8    // 可在用户命名空间挂载
#define FS_RENAME_DOES_D_MOVE 32768
```
[Intermediate]

---

Q: How to register and unregister a file system?
A: 
```c
// 定义文件系统类型
static struct dentry *myfs_mount(struct file_system_type *fs_type,
                                  int flags, const char *dev_name, void *data)
{
    return mount_bdev(fs_type, flags, dev_name, data, myfs_fill_super);
    // 或 mount_nodev() 对于不需要设备的文件系统
    // 或 mount_single() 对于单实例文件系统
}

static void myfs_kill_sb(struct super_block *sb)
{
    kill_block_super(sb);  // 或 kill_litter_super, kill_anon_super
}

static struct file_system_type myfs_type = {
    .owner      = THIS_MODULE,
    .name       = "myfs",
    .mount      = myfs_mount,
    .kill_sb    = myfs_kill_sb,
    .fs_flags   = FS_REQUIRES_DEV,
};

// 模块初始化时注册
static int __init myfs_init(void)
{
    int ret;
    
    // 注册inode缓存（如果有）
    ret = myfs_init_inodecache();
    if (ret)
        return ret;
    
    // 注册文件系统
    ret = register_filesystem(&myfs_type);
    if (ret) {
        myfs_destroy_inodecache();
        return ret;
    }
    
    pr_info("myfs: registered\n");
    return 0;
}

// 模块卸载时注销
static void __exit myfs_exit(void)
{
    unregister_filesystem(&myfs_type);
    myfs_destroy_inodecache();
    pr_info("myfs: unregistered\n");
}

module_init(myfs_init);
module_exit(myfs_exit);
```
[Intermediate]

---

Q: How to implement the `fill_super` callback?
A: `fill_super`初始化超级块：
```c
static int myfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;
    struct dentry *root_dentry;
    struct myfs_sb_info *sbi;
    
    // 1. 分配文件系统私有数据
    sbi = kzalloc(sizeof(*sbi), GFP_KERNEL);
    if (!sbi)
        return -ENOMEM;
    sb->s_fs_info = sbi;
    
    // 2. 设置超级块参数
    sb->s_magic = MYFS_MAGIC;
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_op = &myfs_super_ops;
    sb->s_time_gran = 1;  // 时间精度（纳秒）
    
    // 3. 读取磁盘上的超级块（如果需要）
    // bh = sb_bread(sb, 0);
    // ...
    
    // 4. 创建根inode
    root_inode = myfs_iget(sb, MYFS_ROOT_INO);
    if (IS_ERR(root_inode)) {
        kfree(sbi);
        return PTR_ERR(root_inode);
    }
    
    // 5. 创建根dentry
    root_dentry = d_make_root(root_inode);
    if (!root_dentry) {
        iput(root_inode);
        kfree(sbi);
        return -ENOMEM;
    }
    sb->s_root = root_dentry;
    
    return 0;
}
```
[Intermediate]

---

## 7. Directory Operations (目录操作)

---

Q: How to implement directory lookup?
A: `lookup`在目录中查找文件名：
```c
static struct dentry *myfs_lookup(struct inode *dir, struct dentry *dentry,
                                   unsigned int flags)
{
    struct inode *inode = NULL;
    ino_t ino;
    
    // 检查文件名长度
    if (dentry->d_name.len > MYFS_NAME_LEN)
        return ERR_PTR(-ENAMETOOLONG);
    
    // 在目录中查找文件名
    ino = myfs_find_entry(dir, &dentry->d_name);
    
    if (ino) {
        // 找到了，获取inode
        inode = myfs_iget(dir->i_sb, ino);
        if (IS_ERR(inode))
            return ERR_CAST(inode);
    }
    // 没找到，inode = NULL，创建负dentry
    
    // 关联inode到dentry
    return d_splice_alias(inode, dentry);
}

// inode_operations
static const struct inode_operations myfs_dir_inode_ops = {
    .lookup     = myfs_lookup,
    .create     = myfs_create,
    .mkdir      = myfs_mkdir,
    .rmdir      = myfs_rmdir,
    .unlink     = myfs_unlink,
    .rename     = myfs_rename,
};
```
[Intermediate]

---

Q: How to implement directory iteration (readdir)?
A: 实现`iterate_shared`遍历目录：
```c
static int myfs_iterate(struct file *file, struct dir_context *ctx)
{
    struct inode *inode = file_inode(file);
    struct myfs_inode_info *mi = MYFS_I(inode);
    struct myfs_dir_entry *de;
    unsigned long offset = ctx->pos;
    
    // 发射 "." 和 ".."
    if (!dir_emit_dots(file, ctx))
        return 0;
    
    // 遍历目录项
    while (offset < inode->i_size) {
        de = myfs_get_dir_entry(inode, offset);
        if (!de)
            break;
        
        if (de->inode) {
            // 发射目录项
            if (!dir_emit(ctx, de->name, de->name_len,
                         de->inode, de->file_type))
                break;
        }
        
        offset += sizeof(*de);
        ctx->pos = offset;
    }
    
    return 0;
}

// file_operations
static const struct file_operations myfs_dir_ops = {
    .read           = generic_read_dir,
    .iterate_shared = myfs_iterate,
    .llseek         = generic_file_llseek,
};
```

用户空间使用：
```c
// getdents() 系统调用
// 或 readdir() / rewinddir() 库函数
```
[Intermediate]

---

Q: How to implement file creation?
A: 实现`create`和`mkdir`：
```c
static int myfs_create(struct inode *dir, struct dentry *dentry,
                        umode_t mode, bool excl)
{
    struct inode *inode;
    int err;
    
    // 1. 分配新inode
    inode = myfs_new_inode(dir, mode);
    if (IS_ERR(inode))
        return PTR_ERR(inode);
    
    // 2. 设置文件操作
    inode->i_op = &myfs_file_inode_ops;
    inode->i_fop = &myfs_file_ops;
    inode->i_mapping->a_ops = &myfs_aops;
    
    // 3. 在目录中添加条目
    err = myfs_add_entry(dir, dentry, inode);
    if (err) {
        inode_dec_link_count(inode);
        iput(inode);
        return err;
    }
    
    // 4. 关联dentry和inode
    d_instantiate(dentry, inode);
    
    // 5. 标记inode为脏
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);
    
    return 0;
}

static int myfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    struct inode *inode;
    int err;
    
    // 增加父目录链接数
    inode_inc_link_count(dir);
    
    // 创建目录inode
    inode = myfs_new_inode(dir, S_IFDIR | mode);
    if (IS_ERR(inode)) {
        inode_dec_link_count(dir);
        return PTR_ERR(inode);
    }
    
    inode->i_op = &myfs_dir_inode_ops;
    inode->i_fop = &myfs_dir_ops;
    
    // 初始链接数为2 (. 和父目录的条目)
    set_nlink(inode, 2);
    
    // 在目录中添加条目
    err = myfs_add_entry(dir, dentry, inode);
    if (err) {
        inode_dec_link_count(inode);
        inode_dec_link_count(inode);
        inode_dec_link_count(dir);
        iput(inode);
        return err;
    }
    
    d_instantiate(dentry, inode);
    
    return 0;
}
```
[Intermediate]

---

## 8. Page Cache and I/O (页缓存与I/O)

---

Q: What is `struct address_space` and its role?
A: `address_space`管理文件的页缓存：
```c
struct address_space {
    struct inode *host;                 // 所属inode
    struct radix_tree_root page_tree;   // 页面基数树（或xarray）
    spinlock_t tree_lock;
    
    atomic_t i_mmap_writable;           // 可写映射计数
    struct rb_root i_mmap;              // 私有+共享映射
    struct rw_semaphore i_mmap_rwsem;
    
    unsigned long nrpages;              // 页面数
    pgoff_t writeback_index;            // 回写起始位置
    
    const struct address_space_operations *a_ops;
    
    unsigned long flags;
    errseq_t wb_err;                    // 写回错误
    spinlock_t private_lock;
    struct list_head private_list;
    void *private_data;
};
```

页缓存查找：
```c
// 查找页面
struct page *page = find_get_page(mapping, index);

// 查找或创建
struct page *page = find_or_create_page(mapping, index, gfp_mask);

// 锁定页面
lock_page(page);
// 操作页面
unlock_page(page);
// 释放引用
put_page(page);
```
[Intermediate]

---

Q: What is `struct address_space_operations`?
A: `address_space_operations`定义页缓存操作：
```c
struct address_space_operations {
    // 写单页到磁盘
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    
    // 读单页从磁盘
    int (*readpage)(struct file *, struct page *);
    
    // 批量读取
    int (*readpages)(struct file *, struct address_space *,
                     struct list_head *, unsigned);
    
    // 批量写入
    int (*writepages)(struct address_space *, struct writeback_control *);
    
    // 标记页面为脏
    int (*set_page_dirty)(struct page *page);
    
    // 写入前准备
    int (*write_begin)(struct file *, struct address_space *,
                       loff_t, unsigned, unsigned, struct page **, void **);
    
    // 写入后完成
    int (*write_end)(struct file *, struct address_space *,
                     loff_t, unsigned, unsigned, struct page *, void *);
    
    // 页面无效化
    void (*invalidatepage)(struct page *, unsigned int, unsigned int);
    
    // 释放页面
    int (*releasepage)(struct page *, gfp_t);
    
    // 直接I/O
    ssize_t (*direct_IO)(struct kiocb *, struct iov_iter *);
    
    // 页面迁移
    int (*migratepage)(struct address_space *, struct page *, 
                       struct page *, enum migrate_mode);
};

// 常用通用实现
static const struct address_space_operations myfs_aops = {
    .readpage       = myfs_readpage,
    .writepage      = myfs_writepage,
    .write_begin    = myfs_write_begin,
    .write_end      = generic_write_end,
    .set_page_dirty = __set_page_dirty_buffers,
};
```
[Advanced]

---

Q: How to implement basic read and write operations?
A: 使用通用函数简化实现：
```c
// 读操作
static ssize_t myfs_file_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
    // 通用实现：使用页缓存
    return generic_file_read_iter(iocb, to);
}

// 写操作
static ssize_t myfs_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
    // 通用实现：使用页缓存
    return generic_file_write_iter(iocb, from);
}

// 读取页面
static int myfs_readpage(struct file *file, struct page *page)
{
    struct inode *inode = page->mapping->host;
    loff_t offset = page_offset(page);
    void *kaddr;
    int ret = 0;
    
    // 如果超出文件大小，清零
    if (offset >= i_size_read(inode)) {
        zero_user(page, 0, PAGE_SIZE);
        goto out;
    }
    
    // 读取数据到页面
    kaddr = kmap_atomic(page);
    ret = myfs_read_block(inode, offset, kaddr);
    kunmap_atomic(kaddr);
    
    if (ret)
        SetPageError(page);
    
out:
    SetPageUptodate(page);
    unlock_page(page);
    return ret;
}

// 写入页面
static int myfs_writepage(struct page *page, struct writeback_control *wbc)
{
    struct inode *inode = page->mapping->host;
    loff_t offset = page_offset(page);
    void *kaddr;
    int ret;
    
    kaddr = kmap_atomic(page);
    ret = myfs_write_block(inode, offset, kaddr);
    kunmap_atomic(kaddr);
    
    if (ret) {
        SetPageError(page);
        mapping_set_error(page->mapping, ret);
    }
    
    unlock_page(page);
    return ret;
}

static const struct file_operations myfs_file_ops = {
    .llseek         = generic_file_llseek,
    .read_iter      = myfs_file_read_iter,
    .write_iter     = myfs_file_write_iter,
    .mmap           = generic_file_mmap,
    .fsync          = generic_file_fsync,
};
```
[Advanced]

---

## 9. Block Device Interaction (块设备交互)

---

Q: How to read and write blocks in a file system?
A: 使用buffer_head或bio接口：
```c
/* Buffer Head方式（简单） */
#include <linux/buffer_head.h>

// 读取一个块
struct buffer_head *bh = sb_bread(sb, block_nr);
if (!bh)
    return -EIO;

// 访问数据
char *data = bh->b_data;

// 修改数据
mark_buffer_dirty(bh);

// 同步写入
sync_dirty_buffer(bh);

// 释放
brelse(bh);


/* BIO方式（高性能） */
#include <linux/bio.h>

static void myfs_end_io(struct bio *bio)
{
    // I/O完成回调
    struct page *page = bio_first_page_all(bio);
    if (bio->bi_status)
        SetPageError(page);
    else
        SetPageUptodate(page);
    unlock_page(page);
    bio_put(bio);
}

static int myfs_submit_bio(struct inode *inode, struct page *page, int op)
{
    struct bio *bio;
    sector_t sector = page_offset(page) >> 9;
    
    bio = bio_alloc(GFP_NOIO, 1);
    bio_set_dev(bio, inode->i_sb->s_bdev);
    bio->bi_iter.bi_sector = sector;
    bio->bi_end_io = myfs_end_io;
    bio_set_op_attrs(bio, op, 0);
    
    bio_add_page(bio, page, PAGE_SIZE, 0);
    
    submit_bio(bio);
    return 0;
}
```
[Advanced]

---

Q: What is the difference between buffer_head and bio?
A: 
| 特性 | buffer_head | bio |
|------|-------------|-----|
| 粒度 | 单个块 | 多个块/页 |
| 复杂度 | 简单 | 较复杂 |
| 性能 | 较低 | 高（批量I/O） |
| 使用场景 | 元数据、小I/O | 数据块、大I/O |
| 缓存 | 自动缓存 | 需手动管理 |

```
Buffer Head:
+-------------+     +-------------+     +-------------+
| buffer_head |     | buffer_head |     | buffer_head |
|   block 0   |     |   block 1   |     |   block 2   |
+-------------+     +-------------+     +-------------+
      |                   |                   |
      v                   v                   v
  单独I/O请求        单独I/O请求         单独I/O请求


BIO:
+-------------------------------------------+
|                   bio                     |
|  +-------+  +-------+  +-------+         |
|  | page0 |  | page1 |  | page2 |  ...    |
|  +-------+  +-------+  +-------+         |
+-------------------------------------------+
                    |
                    v
              单个I/O请求（合并）
```
[Intermediate]

---

## 10. Special File Systems (特殊文件系统)

---

Q: What is procfs and how to create entries?
A: procfs提供内核信息的虚拟文件接口：
```c
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

// 简单proc文件
static int myproc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Hello from proc!\n");
    seq_printf(m, "Value: %d\n", my_value);
    return 0;
}

static int myproc_open(struct inode *inode, struct file *file)
{
    return single_open(file, myproc_show, PDE_DATA(inode));
}

static const struct file_operations myproc_fops = {
    .owner      = THIS_MODULE,
    .open       = myproc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};

// 创建proc条目
struct proc_dir_entry *entry;
entry = proc_create("myproc", 0444, NULL, &myproc_fops);

// 创建目录
struct proc_dir_entry *dir;
dir = proc_mkdir("mydir", NULL);
entry = proc_create("myproc", 0444, dir, &myproc_fops);

// 带私有数据
entry = proc_create_data("myproc", 0444, NULL, &myproc_fops, my_data);

// 删除
proc_remove(entry);
remove_proc_entry("myproc", NULL);
```
[Intermediate]

---

Q: What is sysfs and how to create entries?
A: sysfs通过kobject导出内核对象属性：
```c
#include <linux/kobject.h>
#include <linux/sysfs.h>

// 定义属性
static ssize_t my_attr_show(struct kobject *kobj,
                            struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", my_value);
}

static ssize_t my_attr_store(struct kobject *kobj,
                             struct kobj_attribute *attr,
                             const char *buf, size_t count)
{
    sscanf(buf, "%d", &my_value);
    return count;
}

static struct kobj_attribute my_attr = 
    __ATTR(my_value, 0644, my_attr_show, my_attr_store);

// 属性组
static struct attribute *my_attrs[] = {
    &my_attr.attr,
    NULL,
};

static struct attribute_group my_attr_group = {
    .attrs = my_attrs,
};

// 创建kobject
static struct kobject *my_kobj;

static int __init mymodule_init(void)
{
    int ret;
    
    // 在/sys/kernel/下创建目录
    my_kobj = kobject_create_and_add("mymodule", kernel_kobj);
    if (!my_kobj)
        return -ENOMEM;
    
    // 添加属性
    ret = sysfs_create_group(my_kobj, &my_attr_group);
    if (ret)
        kobject_put(my_kobj);
    
    return ret;
}

static void __exit mymodule_exit(void)
{
    kobject_put(my_kobj);
}
```

设备驱动使用`DEVICE_ATTR`更简洁：
```c
static DEVICE_ATTR_RW(my_value);  // 自动创建show/store原型

// 在probe中
device_create_file(&pdev->dev, &dev_attr_my_value);
```
[Intermediate]

---

Q: What is debugfs and when to use it?
A: debugfs用于内核调试信息导出：
```c
#include <linux/debugfs.h>

static struct dentry *debug_dir;
static u32 my_debug_value;

static int __init mymodule_init(void)
{
    // 创建目录 /sys/kernel/debug/mymodule
    debug_dir = debugfs_create_dir("mymodule", NULL);
    if (!debug_dir)
        return -ENOMEM;
    
    // 创建简单类型文件
    debugfs_create_u32("value", 0644, debug_dir, &my_debug_value);
    debugfs_create_x32("hex_value", 0644, debug_dir, &my_debug_value);
    debugfs_create_bool("enabled", 0644, debug_dir, &my_enabled);
    
    // 创建只读文件
    debugfs_create_u32("readonly", 0444, debug_dir, &my_value);
    
    // 创建自定义文件
    debugfs_create_file("custom", 0644, debug_dir, NULL, &my_fops);
    
    // 创建blob（二进制数据）
    static struct debugfs_blob_wrapper blob = {
        .data = my_data,
        .size = sizeof(my_data),
    };
    debugfs_create_blob("blob", 0444, debug_dir, &blob);
    
    return 0;
}

static void __exit mymodule_exit(void)
{
    debugfs_remove_recursive(debug_dir);
}
```

特点：
- 挂载在/sys/kernel/debug
- 仅用于调试，不保证稳定
- 简单易用的API
[Intermediate]

---

## 11. File Locking (文件锁)

---

Q: What types of file locks are available in Linux?
A: 
| 类型 | API | 特点 |
|------|-----|------|
| **POSIX锁** | fcntl(F_SETLK) | 字节范围锁，进程关联 |
| **BSD锁** | flock() | 整文件锁，文件描述符关联 |
| **强制锁** | O_MANDATORY | 自动阻止冲突操作 |
| **租约** | fcntl(F_SETLEASE) | 用于NFS等 |

```c
// POSIX锁
struct flock fl = {
    .l_type   = F_WRLCK,    // F_RDLCK, F_WRLCK, F_UNLCK
    .l_whence = SEEK_SET,
    .l_start  = 0,          // 起始偏移
    .l_len    = 100,        // 长度（0=到文件末尾）
};
fcntl(fd, F_SETLK, &fl);    // 非阻塞
fcntl(fd, F_SETLKW, &fl);   // 阻塞

// BSD锁
flock(fd, LOCK_SH);         // 共享锁
flock(fd, LOCK_EX);         // 独占锁
flock(fd, LOCK_UN);         // 解锁
flock(fd, LOCK_NB);         // 非阻塞标志
```
[Intermediate]

---

Q: How are file locks implemented in the kernel?
A: 
```c
// include/linux/fs.h
struct file_lock {
    struct file_lock *fl_next;      // 链表
    struct list_head fl_list;
    struct list_head fl_block;      // 阻塞等待链表
    
    fl_owner_t fl_owner;            // 锁所有者
    unsigned int fl_flags;          // FL_POSIX, FL_FLOCK, FL_LEASE
    unsigned char fl_type;          // F_RDLCK, F_WRLCK
    
    loff_t fl_start;                // 起始位置
    loff_t fl_end;                  // 结束位置
    
    struct file *fl_file;           // 关联的文件
    struct fasync_struct *fl_fasync;
    wait_queue_head_t fl_wait;      // 等待队列
    
    const struct file_lock_operations *fl_ops;
    const struct lock_manager_operations *fl_lmops;
};

// 文件系统可实现的锁操作
static int myfs_lock(struct file *file, int cmd, struct file_lock *fl)
{
    // 自定义锁处理
    return posix_lock_file(file, fl, NULL);  // 或使用通用实现
}

static int myfs_flock(struct file *file, int cmd, struct file_lock *fl)
{
    return locks_lock_file_wait(file, fl);
}

static const struct file_operations myfs_file_ops = {
    .lock  = myfs_lock,
    .flock = myfs_flock,
};
```
[Advanced]

---

## 12. Extended Attributes (扩展属性)

---

Q: What are extended attributes and how to implement them?
A: 扩展属性允许存储额外的文件元数据：
```c
// 命名空间
// user.xxx    - 用户属性
// system.xxx  - 系统属性（如ACL）
// security.xxx - 安全模块（如SELinux）
// trusted.xxx  - 受信任属性

// 用户空间API
setxattr(path, name, value, size, flags);
getxattr(path, name, value, size);
listxattr(path, list, size);
removexattr(path, name);

// 内核实现
static const struct xattr_handler myfs_xattr_user_handler = {
    .prefix = XATTR_USER_PREFIX,  // "user."
    .get    = myfs_xattr_get,
    .set    = myfs_xattr_set,
};

static ssize_t myfs_xattr_get(const struct xattr_handler *handler,
                               struct dentry *dentry, struct inode *inode,
                               const char *name, void *buffer, size_t size)
{
    // 从inode读取xattr
    return myfs_get_xattr(inode, handler->prefix, name, buffer, size);
}

static int myfs_xattr_set(const struct xattr_handler *handler,
                           struct dentry *dentry, struct inode *inode,
                           const char *name, const void *value,
                           size_t size, int flags)
{
    // 设置xattr到inode
    return myfs_set_xattr(inode, handler->prefix, name, value, size, flags);
}

static const struct xattr_handler *myfs_xattr_handlers[] = {
    &myfs_xattr_user_handler,
    &myfs_xattr_security_handler,
    NULL,
};

// 在super_block中设置
sb->s_xattr = myfs_xattr_handlers;
```
[Advanced]

---

## 13. Mount and Namespace (挂载与命名空间)

---

Q: How does mount work in the kernel?
A: mount()系统调用流程：
```
sys_mount(source, target, fstype, flags, data)
    |
    v
do_mount()
    |
    +---> 解析目标路径
    |
    +---> 查找file_system_type
    |
    v
do_new_mount()
    |
    v
vfs_kern_mount()
    |
    +---> 调用fs_type->mount()
    |          |
    |          v
    |     mount_bdev() / mount_nodev()
    |          |
    |          v
    |     fill_super() - 初始化超级块
    |          |
    |          v
    |     返回root dentry
    |
    v
do_add_mount()
    |
    +---> 创建vfsmount结构
    |
    +---> 添加到挂载树
    |
    v
完成挂载
```

关键结构：
```c
struct vfsmount {
    struct dentry *mnt_root;        // 挂载点根dentry
    struct super_block *mnt_sb;     // 超级块
    int mnt_flags;                  // 挂载标志
};

struct mount {
    struct hlist_node mnt_hash;
    struct mount *mnt_parent;       // 父挂载
    struct dentry *mnt_mountpoint;  // 挂载点dentry
    struct vfsmount mnt;
    // ...
};
```
[Advanced]

---

Q: What is mount namespace?
A: 挂载命名空间隔离进程的文件系统视图：
```
+------------------+     +------------------+
|   Namespace A    |     |   Namespace B    |
+------------------+     +------------------+
|    /             |     |    /             |
|    ├── bin/      |     |    ├── bin/      |
|    ├── home/     |     |    ├── home/     |
|    └── mnt/ ─────|─X   |    └── mnt/ ─────|───> /dev/sdb1
|         (空)      |     |         (已挂载)  |
+------------------+     +------------------+

进程A看不到进程B的挂载
```

创建新命名空间：
```c
// unshare系统调用
unshare(CLONE_NEWNS);

// clone带命名空间标志
clone(..., CLONE_NEWNS, ...);

// 内核中切换命名空间
struct mnt_namespace *ns = copy_mnt_ns(flags, current->nsproxy->mnt_ns, ...);
```
[Advanced]

---

## 14. File System Utilities (文件系统工具)

---

Q: How to implement statfs for disk usage info?
A: 
```c
static int myfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    struct super_block *sb = dentry->d_sb;
    struct myfs_sb_info *sbi = MYFS_SB(sb);
    
    buf->f_type = MYFS_MAGIC;              // 文件系统类型
    buf->f_bsize = sb->s_blocksize;        // 块大小
    buf->f_blocks = sbi->total_blocks;     // 总块数
    buf->f_bfree = sbi->free_blocks;       // 空闲块数
    buf->f_bavail = sbi->free_blocks;      // 可用块数
    buf->f_files = sbi->total_inodes;      // 总inode数
    buf->f_ffree = sbi->free_inodes;       // 空闲inode数
    buf->f_namelen = MYFS_NAME_LEN;        // 最大文件名长度
    buf->f_frsize = sb->s_blocksize;       // 分片大小
    
    return 0;
}

static const struct super_operations myfs_super_ops = {
    .statfs = myfs_statfs,
    // ...
};
```

用户空间使用：
```c
struct statfs buf;
statfs("/path", &buf);
printf("Free: %ld blocks\n", buf.f_bfree);
```
[Basic]

---

Q: How to implement fsync for data persistence?
A: 
```c
static int myfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
    struct inode *inode = file->f_mapping->host;
    int ret;
    
    // 1. 同步页缓存到设备
    ret = file_write_and_wait_range(file, start, end);
    if (ret)
        return ret;
    
    // 2. 如果不是datasync，还要同步元数据
    if (!datasync) {
        inode_lock(inode);
        ret = sync_inode_metadata(inode, 1);
        inode_unlock(inode);
    }
    
    // 3. 刷新设备缓存（可选）
    if (!ret && file->f_flags & O_SYNC)
        ret = blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL, NULL);
    
    return ret;
}

// 或使用通用实现
static const struct file_operations myfs_file_ops = {
    .fsync = generic_file_fsync,  // 通用实现
};
```

区别：
- `fsync`: 同步数据和元数据
- `fdatasync`: 仅同步数据（除非元数据影响数据检索）
[Intermediate]

---

## 15. Debugging File Systems (文件系统调试)

---

Q: What tools and techniques are available for debugging file systems?
A: 
```bash
# 1. 挂载选项
mount -o errors=continue  # 继续运行
mount -o errors=panic     # 内核panic
mount -o debug            # 调试输出

# 2. 查看信息
cat /proc/filesystems     # 已注册的文件系统
cat /proc/mounts          # 挂载点
cat /proc/fs/ext4/sda1/*  # ext4特定信息
ls /sys/fs/               # 文件系统sysfs

# 3. 跟踪
echo 1 > /sys/kernel/debug/tracing/events/ext4/enable
cat /sys/kernel/debug/tracing/trace

# 4. 磁盘工具
dumpe2fs /dev/sda1        # 查看ext2/3/4超级块
xfs_info /dev/sda1        # 查看XFS信息
debugfs /dev/sda1         # ext2/3/4调试shell

# 5. 一致性检查
fsck /dev/sda1            # 文件系统检查
e2fsck -f /dev/sda1       # 强制检查
```

内核调试：
```c
// printk调试
pr_debug("myfs: inode %lu, size %lld\n", inode->i_ino, inode->i_size);

// 动态调试
echo 'file myfs.c +p' > /sys/kernel/debug/dynamic_debug/control

// WARN/BUG
WARN_ON(condition);
BUG_ON(condition);

// 断言
MYFS_BUG_ON(inode == NULL, "NULL inode");
```
[Intermediate]

---

## 16. Common File System Patterns (常见模式)

---

Q: What is the typical structure of a simple file system module?
A: 
```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>

#define MYFS_MAGIC 0x12345678

/* 前向声明 */
static const struct super_operations myfs_super_ops;
static const struct inode_operations myfs_dir_inode_ops;
static const struct inode_operations myfs_file_inode_ops;
static const struct file_operations myfs_dir_ops;
static const struct file_operations myfs_file_ops;
static const struct address_space_operations myfs_aops;

/* inode缓存 */
static struct kmem_cache *myfs_inode_cachep;

struct myfs_inode_info {
    __u32 data_block;
    struct inode vfs_inode;
};

static inline struct myfs_inode_info *MYFS_I(struct inode *inode)
{
    return container_of(inode, struct myfs_inode_info, vfs_inode);
}

/* Inode操作 */
static struct inode *myfs_alloc_inode(struct super_block *sb)
{
    struct myfs_inode_info *mi;
    mi = kmem_cache_alloc(myfs_inode_cachep, GFP_KERNEL);
    if (!mi)
        return NULL;
    return &mi->vfs_inode;
}

static void myfs_destroy_inode(struct inode *inode)
{
    kmem_cache_free(myfs_inode_cachep, MYFS_I(inode));
}

/* 超级块操作 */
static const struct super_operations myfs_super_ops = {
    .alloc_inode    = myfs_alloc_inode,
    .destroy_inode  = myfs_destroy_inode,
    .statfs         = simple_statfs,
    .drop_inode     = generic_delete_inode,
};

/* 填充超级块 */
static int myfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;
    
    sb->s_magic = MYFS_MAGIC;
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_op = &myfs_super_ops;
    
    root_inode = new_inode(sb);
    if (!root_inode)
        return -ENOMEM;
    
    root_inode->i_ino = 1;
    root_inode->i_mode = S_IFDIR | 0755;
    root_inode->i_op = &myfs_dir_inode_ops;
    root_inode->i_fop = &simple_dir_operations;
    set_nlink(root_inode, 2);
    
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root)
        return -ENOMEM;
    
    return 0;
}

/* 挂载 */
static struct dentry *myfs_mount(struct file_system_type *type, int flags,
                                  const char *dev, void *data)
{
    return mount_nodev(type, flags, data, myfs_fill_super);
}

/* 文件系统类型 */
static struct file_system_type myfs_type = {
    .owner    = THIS_MODULE,
    .name     = "myfs",
    .mount    = myfs_mount,
    .kill_sb  = kill_litter_super,
};

/* 初始化/退出 */
static int __init myfs_init(void)
{
    myfs_inode_cachep = kmem_cache_create("myfs_inode",
                            sizeof(struct myfs_inode_info), 0,
                            SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD,
                            NULL);
    if (!myfs_inode_cachep)
        return -ENOMEM;
    
    return register_filesystem(&myfs_type);
}

static void __exit myfs_exit(void)
{
    unregister_filesystem(&myfs_type);
    kmem_cache_destroy(myfs_inode_cachep);
}

module_init(myfs_init);
module_exit(myfs_exit);
MODULE_LICENSE("GPL");
```
[Intermediate]

---

Q: What are common mistakes when implementing file systems?
A: 
| 错误 | 后果 | 正确做法 |
|------|------|----------|
| 不初始化inode字段 | 数据损坏 | 设置所有必要字段 |
| 忘记增加nlink | 目录损坏 | mkdir时nlink++ |
| 不正确的锁顺序 | 死锁 | 遵循锁层次 |
| 不同步脏数据 | 数据丢失 | 正确实现writepage |
| 不处理错误 | 数据损坏 | 检查所有返回值 |
| 内存泄漏 | 系统卡顿 | 配对的分配/释放 |
| 不正确的引用计数 | use-after-free | iget/iput配对 |
| 不保护并发访问 | 数据竞争 | 使用适当的锁 |

锁顺序示例：
```
i_mutex (inode锁)
  └── i_lock (inode自旋锁)
      └── tree_lock (页缓存锁)
          └── private_lock (私有数据锁)
```
[Intermediate]

---

*Total: 100+ cards covering Linux kernel file system implementation*

