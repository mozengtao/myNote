# read/write 系统调用 - 内核执行路径详解

> 🎯 **学习目标**: 理解最基本的文件 I/O 如何从用户态到达硬件，建立 VFS 层次认知

---

## 🧩 第一部分：宏观架构图

```
User Space
├── Application
│   └── read(fd, buf, count) / write(fd, buf, count)
├── glibc
│   └── syscall(__NR_read / __NR_write)
└── Syscall Interface
    └── int 0x80 / syscall instruction

════════════════════════════════════════════════════════════════════════════════

Kernel Space
├── Syscall Dispatch
│   └── sys_read() / sys_write()
├── Virtual File System (VFS)
│   ├── struct file
│   ├── struct file_operations
│   └── Generic I/O Path
├── Concrete Filesystem Layer
│   ├── ext4_file_read_iter() / ext4_file_write_iter()
│   └── Filesystem-specific Logic
├── Block Layer
│   ├── submit_bio()
│   └── I/O Scheduler (mq-deadline / bfq)
├── Device Driver Layer
│   └── nvme_queue_rq() / scsi_queue_rq()

════════════════════════════════════════════════════════════════════════════════

Hardware Layer
└── Storage Device (NVMe SSD / SATA HDD / ...)
```

*图表说明：read/write 系统调用从用户态经 glibc 进入内核，依次穿过 VFS、具体文件系统、块层和设备驱动，最终到达存储硬件。读取路径为 Hardware → Driver → Block Layer → Filesystem → VFS → User Buffer；写入路径方向相反。*

---

## 🔬 第二部分：内核执行路径

### 2.1 read() 系统调用路径

```c
// 用户态调用
ssize_t read(int fd, void *buf, size_t count)

// 进入内核
SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
├── ksys_read(fd, buf, count)
├── vfs_read(file, buf, count, &pos)
│   ├── rw_verify_area(READ, file, pos, count)  // 权限检查
│   ├── file_read_iter(file, &kiocb)            // 核心读取
│   │   └── file->f_op->read_iter()             // 调用具体文件系统
│   │       └── ext4_file_read_iter()           // ext4 示例
│   │           └── generic_file_read_iter()    // 通用页缓存读取
│   │               ├── filemap_read()          // 页缓存查找
│   │               └── page_cache_sync_readahead() // 预读
│   └── fsnotify_access(file)                   // 访问通知
└── 返回用户态
```

### 2.2 write() 系统调用路径

```c
// 用户态调用  
ssize_t write(int fd, const void *buf, size_t count)

// 进入内核
SYSCALL_DEFINE3(write, unsigned int, fd, const char __user *, buf, size_t, count)
├── ksys_write(fd, buf, count)
├── vfs_write(file, buf, count, &pos)
│   ├── rw_verify_area(WRITE, file, pos, count) // 权限检查
│   ├── file_write_iter(file, &kiocb)           // 核心写入
│   │   └── file->f_op->write_iter()            // 调用具体文件系统
│   │       └── ext4_file_write_iter()          // ext4 示例
│   │           ├── ext4_buffered_write_iter()  // 缓冲写入
│   │           └── generic_perform_write()     // 通用写入逻辑
│   │               └── a_ops->write_begin/write_end() // 分配页面并写入
│   └── fsnotify_modify(file)                   // 修改通知
└── 返回用户态
```

### 2.3 write 回写路径 (Writeback Path)

写入的数据首先进入页缓存 (脏页)，不会立即落盘。内核通过回写机制异步刷盘：

```c
// 写入路径 (数据进入页缓存)
ext4_buffered_write_iter()
├── generic_perform_write()
│   ├── a_ops->write_begin()          // 分配/查找 page cache 页
│   ├── copy_from_user()              // 用户数据 → 页缓存
│   └── a_ops->write_end()            // 标记页面为 dirty
└── 返回 (此时数据仅在内存中)

// 回写触发 (脏页 → 磁盘)
// 触发条件: 1) 脏页比例超阈值  2) sync/fsync  3) 内存压力  4) 定时回写
balance_dirty_pages()                 // 脏页平衡
├── wb_writeback()                    // 回写工作线程
│   ├── queue_io()                    // 收集脏页
│   └── mpage_writepages()            // 批量写盘
│       └── submit_bio(WRITE)         // 提交块 I/O
└── 数据持久化到磁盘
```

**关键时机**:
- `write()` 返回只表示数据进入页缓存，不保证落盘
- `fsync(fd)` / `fdatasync(fd)` 强制回写该文件的脏页
- `sync()` 回写所有脏页

### 2.4 新式 I/O: read_iter / write_iter 与 iov_iter

Linux 4.x 后内核统一使用 `read_iter`/`write_iter` 替代旧的 `read`/`write`：

```c
// 旧接口 (仍被 VFS 包装调用)
ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);

// 新接口 (支持向量 I/O、零拷贝)
ssize_t (*read_iter)(struct kiocb *, struct iov_iter *);
ssize_t (*write_iter)(struct kiocb *, struct iov_iter *);

// struct iov_iter - 统一的 I/O 向量描述
struct iov_iter {
    u8 type;                    // ITER_IOVEC / ITER_KVEC / ITER_BVEC / ITER_PIPE
    bool data_source;           // 读(false) 或 写(true)
    size_t count;               // 剩余字节数
    union {
        const struct iovec *iov;     // 用户态 iovec 数组 (readv/writev)
        const struct kvec *kvec;     // 内核态向量
        struct bio_vec *bvec;        // 块 I/O 向量
    };
    // ...
};

// readv/writev 系统调用路径
SYSCALL_DEFINE3(readv, ...)
└── do_readv() → vfs_readv() → file->f_op->read_iter()
    └── generic_file_read_iter()  // 最终仍走页缓存
```

**readv/writev 优势**: 一次系统调用传输多个不连续缓冲区，减少 syscall 次数。

### 2.5 关键内核子系统协作

1. **VFS 层**: 提供统一的文件操作接口
2. **页缓存 (Page Cache)**: 缓存文件数据，提高性能
3. **回写子系统 (Writeback)**: 异步将脏页刷到磁盘
4. **块设备层**: 处理块 I/O 请求
5. **I/O 调度器**: 优化磁盘访问顺序

---

## 🧱 第三部分：核心数据结构

### 3.1 struct file - 文件描述符核心

```c
struct file {
    union {
        struct llist_node   fu_llist;    // 释放链表
        struct rcu_head     fu_rcuhead;  // RCU 释放
    } f_u;
    struct path         f_path;          // 文件路径 (dentry + vfsmount)
    struct inode        *f_inode;        // 指向 inode
    const struct file_operations *f_op;  // 文件操作函数表
    
    spinlock_t          f_lock;          // 保护文件状态
    atomic_long_t       f_count;         // 引用计数
    unsigned int        f_flags;         // 打开标志 (O_RDONLY, O_WRONLY...)
    fmode_t             f_mode;          // 访问模式
    struct mutex        f_pos_lock;      // 位置锁
    loff_t              f_pos;           // 文件当前位置
    struct fown_struct  f_owner;         // 文件所有者信息
    
    /* 缓存相关 */
    struct address_space *f_mapping;     // 页缓存映射
    
    /* 私有数据 */
    void                *private_data;   // 文件系统私有数据
};
```

**作用**: 表示一个打开的文件，连接用户态 fd 和内核文件系统  
**生命周期**: open() 创建 → 多次 read/write → close() 销毁  
**关键字段**:
- `f_op`: 决定具体的 read/write 实现
- `f_pos`: 当前读写位置
- `f_mapping`: 连接到页缓存

### 3.2 struct file_operations - 操作函数表

```c
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);    // 新式异步读取
    ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);   // 新式异步写入
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    // ... 更多操作
};
```

**作用**: 定义具体文件类型的操作方法，实现多态  
**位置**: VFS 层到具体文件系统的桥梁

### 3.3 struct inode - 文件元数据

```c
struct inode {
    umode_t                 i_mode;      // 文件类型和权限
    unsigned short          i_opflags;   // 操作标志
    kuid_t                  i_uid;       // 所有者 UID
    kgid_t                  i_gid;       // 所有者 GID
    unsigned int            i_flags;     // 文件标志
    
    const struct inode_operations   *i_op;    // inode 操作
    struct super_block      *i_sb;            // 所属超级块
    struct address_space    *i_mapping;       // 页缓存映射
    
    /* 文件属性 */
    loff_t                  i_size;      // 文件大小
    struct timespec64       i_atime;     // 最后访问时间
    struct timespec64       i_mtime;     // 最后修改时间
    struct timespec64       i_ctime;     // 最后状态改变时间
    
    spinlock_t              i_lock;      // 保护 inode 字段
    unsigned long           i_state;     // inode 状态
    
    /* 块设备相关 */
    dev_t                   i_rdev;      // 设备号
    
    union {
        struct pipe_inode_info  *i_pipe;     // 管道
        struct block_device     *i_bdev;     // 块设备
        struct cdev             *i_cdev;     // 字符设备
        char                    *i_link;     // 符号链接
    };
};
```

**作用**: 存储文件的元数据信息，文件系统的核心  
**生命周期**: 文件创建时分配 → 缓存在内存 → 引用计数归零后释放

---

## ⚙️ 第四部分：最小可运行实验

### 4.1 基础 read/write 测试

```c
// demo_read_write.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int main() {
    char *filename = "test_file.txt";
    char write_buf[] = "Hello, Linux Kernel!";
    char read_buf[256];
    int fd;
    ssize_t bytes_written, bytes_read;

    printf("=== Linux read/write 内核路径测试 ===\n");

    // 1. 创建并写入文件
    printf("1. 打开文件进行写入...\n");
    fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open for write");
        exit(1);
    }
    
    printf("   文件描述符: %d\n", fd);
    
    bytes_written = write(fd, write_buf, strlen(write_buf));
    if (bytes_written == -1) {
        perror("write");
        close(fd);
        exit(1);
    }
    
    printf("   写入字节数: %ld\n", bytes_written);
    close(fd);

    // 2. 读取文件
    printf("2. 打开文件进行读取...\n");
    fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("open for read");
        exit(1);
    }

    bytes_read = read(fd, read_buf, sizeof(read_buf) - 1);
    if (bytes_read == -1) {
        perror("read");
        close(fd);
        exit(1);
    }
    
    read_buf[bytes_read] = '\0';
    printf("   读取字节数: %ld\n", bytes_read);
    printf("   读取内容: %s\n", read_buf);
    close(fd);

    // 3. 清理
    unlink(filename);
    
    return 0;
}
```

### 4.2 编译和运行

```bash
# 编译
gcc -o demo_read_write demo_read_write.c

# 运行
./demo_read_write
```

### 4.3 触发的内核行为

此代码会触发：
1. **sys_openat()** - 两次文件打开
2. **sys_write()** - 写入数据到页缓存  
3. **sys_read()** - 从页缓存读取数据
4. **sys_close()** - 释放文件描述符
5. **sys_unlink()** - 删除文件

---

## 🔍 第五部分：可观测性 & Debug 方法

### 5.1 使用 strace 观察系统调用

```bash
# 跟踪所有系统调用
strace -e trace=file,desc ./demo_read_write

# 只跟踪文件相关调用，显示详细信息
strace -e trace=openat,read,write,close,unlink -v ./demo_read_write
```

**期望输出**：
```
openat(AT_FDCWD, "test_file.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644) = 3
write(3, "Hello, Linux Kernel!", 20) = 20
close(3) = 0
openat(AT_FDCWD, "test_file.txt", O_RDONLY) = 3  
read(3, "Hello, Linux Kernel!", 255) = 20
close(3) = 0
unlink("test_file.txt") = 0
```

### 5.2 使用 perf 观察内核函数

```bash
# 记录内核函数调用
sudo perf record -e syscalls:sys_enter_read,syscalls:sys_enter_write \
    -g ./demo_read_write

# 查看调用栈
sudo perf script
```

### 5.3 使用 ftrace 深入内核

```bash
# 启用函数跟踪
echo function > /sys/kernel/debug/tracing/current_tracer
echo 'vfs_read vfs_write' > /sys/kernel/debug/tracing/set_ftrace_filter
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 运行程序
./demo_read_write

# 查看跟踪结果
cat /sys/kernel/debug/tracing/trace

# 清理
echo 0 > /sys/kernel/debug/tracing/tracing_on
echo > /sys/kernel/debug/tracing/set_ftrace_filter
```

### 5.4 观察页缓存状态

```bash
# 运行前后对比页缓存
cat /proc/meminfo | grep -E "^(Cached|Buffers|Dirty)"

# 查看具体文件的页缓存
# （需要 root 权限）
echo 3 > /proc/sys/vm/drop_caches  # 清空缓存
./demo_read_write
cat /proc/meminfo | grep Cached
```

---

## ⚡ 第六部分：性能与设计权衡

### 6.1 性能瓶颈分析

1. **系统调用开销**
   - 用户态 ↔ 内核态切换成本
   - 约 100-300 个 CPU 周期

2. **页缓存命中率**
   - 命中：直接内存拷贝，极快
   - 未命中：需要磁盘 I/O，慢 1000 倍以上

3. **文件系统开销**
   - 元数据操作（inode 查找）
   - 日志写入（ext4 的 journal）

### 6.2 设计权衡

**为什么需要 VFS 层？**
- **优势**: 统一接口，支持多种文件系统
- **代价**: 额外的间接调用开销

**为什么使用页缓存？**
- **优势**: 
  - 减少磁盘 I/O
  - 预读优化
  - 写入合并
- **代价**: 内存占用，一致性复杂度

**缓冲 I/O vs 直接 I/O**
```c
// 缓冲 I/O (默认)
fd = open("file", O_RDONLY);

// 直接 I/O (绕过页缓存)
fd = open("file", O_RDONLY | O_DIRECT);
```

---

## 🔗 第七部分：横向对比

### 7.1 read/write vs mmap

| 特性 | read/write | mmap |
|------|------------|------|
| **系统调用次数** | 每次 I/O 一次 | 仅映射时一次 |
| **内存拷贝** | 内核 → 用户空间 | 零拷贝 |
| **适用场景** | 流式处理 | 随机访问 |
| **内存使用** | 用户缓冲区 + 页缓存 | 仅页缓存 |

### 7.2 阻塞 vs 非阻塞 I/O

```c
// 阻塞 I/O (默认)
read(fd, buf, count);  // 会等待数据

// 非阻塞 I/O  
fcntl(fd, F_SETFL, O_NONBLOCK);
read(fd, buf, count);  // 立即返回，可能 EAGAIN
```

### 7.3 同步 vs 异步 I/O

```c
// 同步 I/O (read/write)
ssize_t result = read(fd, buf, count);

// 异步 I/O (Linux AIO)
struct aiocb aio;
aio_read(&aio);  // 立即返回
// ... 其他工作 ...
aio_suspend(&aio);  // 等待完成
```

---

## 🧠 第八部分：一句话本质总结

> **read/write 的本质是通过 VFS 抽象层实现的统一文件访问机制，将用户态的简单接口转换为内核态的复杂多层调用，并通过页缓存优化 I/O 性能。**

---

## 📌 下一步学习

掌握了基础的 read/write 后，建议继续学习：
1. **[VFS open 机制](02-vfs-open.md)** - 深入理解文件系统抽象
2. **[mmap 内存映射](03-mmap.md)** - 学习零拷贝机制

---

## 🔖 关键要点回顾

- ✅ read/write 通过 VFS 层提供统一接口
- ✅ 页缓存是性能的关键优化
- ✅ 系统调用开销是小数据 I/O 的主要瓶颈  
- ✅ file_operations 结构实现了面向对象的多态
- ✅ 理解了从用户态到硬件的完整数据路径