# Linux 字符设备驱动框架深入讲解

基于 Linux 3.2 内核源码分析

---

## 目录

- [驱动框架整体架构](#驱动框架整体架构)
- [核心数据结构](#核心数据结构)
- [实例分析：/dev/null, /dev/zero, /dev/mem](#实例分析devnull-devzero-devmem)
- [杂项设备驱动框架 (misc)](#杂项设备驱动框架-misc)
- [驱动核心框架 (drivers/base/)](#驱动核心框架-driversbase)
- [编写字符设备驱动的三种方式](#编写字符设备驱动的三种方式)
- [用户空间与内核空间数据传输](#用户空间与内核空间数据传输)
- [驱动与系统的交互流程](#驱动与系统的交互流程)
- [驱动开发要点总结](#驱动开发要点总结)

---

## 驱动框架整体架构

### 架构层次图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            用户空间 (User Space)                             │
│         open("/dev/xxx") → read() → write() → ioctl() → close()             │
├─────────────────────────────────────────────────────────────────────────────┤
│                           系统调用层 (VFS)                                   │
│                    sys_open → sys_read → sys_write → sys_ioctl              │
├─────────────────────────────────────────────────────────────────────────────┤
│                        字符设备子系统 (Char Device)                          │
│    ┌─────────────────────────────────────────────────────────────────┐      │
│    │  chrdev_open() → 查找 file_operations → 调用驱动的 open()        │      │
│    └─────────────────────────────────────────────────────────────────┘      │
├─────────────────────────────────────────────────────────────────────────────┤
│                        具体驱动层 (Your Driver)                              │
│    ┌─────────────────────────────────────────────────────────────────┐      │
│    │  struct file_operations {                                        │      │
│    │      .open    = my_open,                                         │      │
│    │      .read    = my_read,                                         │      │
│    │      .write   = my_write,                                        │      │
│    │      .release = my_release,                                      │      │
│    │  };                                                              │      │
│    └─────────────────────────────────────────────────────────────────┘      │
├─────────────────────────────────────────────────────────────────────────────┤
│                              硬件层                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 调用链路

```
用户程序 open("/dev/xxx")
        │
        ▼
    系统调用 sys_open()
        │
        ▼
    VFS: do_sys_open() → do_filp_open()
        │
        ▼
    字符设备: chrdev_open()
        │
        ▼
    查找 cdev 结构
        │
        ▼
    获取 file_operations
        │
        ▼
    调用 driver->open(inode, file)
```

---

## 核心数据结构

### 1. struct file_operations - 驱动操作接口

这是驱动最核心的结构，定义了驱动支持的所有操作。

**位置**: `include/linux/fs.h`

```c
struct file_operations {
    struct module *owner;                                           // 所属模块
    loff_t (*llseek) (struct file *, loff_t, int);                 // 定位
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);   // 读
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *); // 写
    ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);  // 异步读
    ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t); // 异步写
    int (*readdir) (struct file *, void *, filldir_t);             // 读目录
    unsigned int (*poll) (struct file *, struct poll_table_struct *);  // 多路复用
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);  // 控制
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);    // 32位兼容
    int (*mmap) (struct file *, struct vm_area_struct *);          // 内存映射
    int (*open) (struct inode *, struct file *);                   // 打开
    int (*flush) (struct file *, fl_owner_t id);                   // 刷新
    int (*release) (struct inode *, struct file *);                // 关闭
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);    // 同步
    int (*aio_fsync) (struct kiocb *, int datasync);               // 异步同步
    int (*fasync) (int, struct file *, int);                       // 异步通知
    int (*lock) (struct file *, int, struct file_lock *);          // 文件锁
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
    unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, 
                                       unsigned long, unsigned long);
    int (*check_flags)(int);
    int (*flock) (struct file *, int, struct file_lock *);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, 
                            size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, 
                           size_t, unsigned int);
    int (*setlease)(struct file *, long, struct file_lock **);
    long (*fallocate)(struct file *file, int mode, loff_t offset, loff_t len);
};
```

#### 关键函数说明

| 函数指针 | 对应系统调用 | 功能说明 | 必须实现 |
|---------|-------------|---------|---------|
| `owner` | - | 指向拥有该模块的指针，防止模块被卸载时仍在使用 | ✓ |
| `open` | `open()` | 打开设备，分配资源，初始化硬件 | 可选 |
| `release` | `close()` | 关闭设备，释放资源 | 可选 |
| `read` | `read()` | 从设备读取数据到用户空间 | 通常需要 |
| `write` | `write()` | 从用户空间写入数据到设备 | 通常需要 |
| `unlocked_ioctl` | `ioctl()` | 设备控制命令 | 可选 |
| `mmap` | `mmap()` | 将设备内存映射到用户空间 | 可选 |
| `poll` | `poll()/select()` | 多路复用查询设备状态 | 可选 |
| `llseek` | `lseek()` | 改变文件读写位置 | 可选 |
| `fasync` | - | 异步通知机制 | 可选 |

### 2. struct cdev - 字符设备结构

**位置**: `include/linux/cdev.h`

```c
struct cdev {
    struct kobject kobj;                    // 内核对象，用于 sysfs
    struct module *owner;                   // 所属模块
    const struct file_operations *ops;      // 操作函数集
    struct list_head list;                  // 链表节点
    dev_t dev;                              // 设备号 (主设备号+次设备号)
    unsigned int count;                     // 次设备号数量
};
```

#### cdev 相关 API

```c
// 初始化 cdev 结构
void cdev_init(struct cdev *cdev, const struct file_operations *fops);

// 动态分配 cdev 结构
struct cdev *cdev_alloc(void);

// 将 cdev 添加到系统
int cdev_add(struct cdev *p, dev_t dev, unsigned count);

// 从系统中删除 cdev
void cdev_del(struct cdev *p);

// 减少引用计数
void cdev_put(struct cdev *p);
```

### 3. struct miscdevice - 杂项设备

**位置**: `include/linux/miscdevice.h`

```c
struct miscdevice {
    int minor;                              // 次设备号 (MISC_DYNAMIC_MINOR 表示动态分配)
    const char *name;                       // 设备名称
    const struct file_operations *fops;     // 操作函数集
    struct list_head list;                  // 链表节点
    struct device *parent;                  // 父设备
    struct device *this_device;             // 对应的 struct device
    const char *nodename;                   // /dev 下的节点名
    mode_t mode;                            // 权限模式
};
```

### 4. struct file - 打开的文件实例

```c
struct file {
    struct path         f_path;             // 文件路径
    const struct file_operations *f_op;     // 操作函数
    unsigned int        f_flags;            // 打开标志 (O_RDONLY, O_NONBLOCK 等)
    fmode_t             f_mode;             // 访问模式
    loff_t              f_pos;              // 当前读写位置
    void               *private_data;       // 驱动私有数据
    // ...
};
```

### 5. struct inode - 文件的 inode

```c
struct inode {
    dev_t               i_rdev;             // 设备号
    union {
        struct cdev     *i_cdev;            // 字符设备
        struct block_device *i_bdev;        // 块设备
    };
    // ...
};
```

---

## 实例分析：/dev/null, /dev/zero, /dev/mem

### 源码位置: `drivers/char/mem.c`

这是最经典的字符设备驱动示例，由 Linus Torvalds 亲自编写。

### 1. /dev/null 的实现

#### 读操作

```c
static ssize_t read_null(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    return 0;  // 返回 0 表示 EOF (文件结束)
}
```

**设计思想**: 读取 `/dev/null` 永远返回 EOF。

#### 写操作

```c
static ssize_t write_null(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    return count;  // 假装成功写入，实际丢弃所有数据
}
```

**设计思想**: 写入 `/dev/null` 的数据被丢弃，但返回成功。

#### splice_write 操作

```c
static int pipe_to_null(struct pipe_inode_info *info, struct pipe_buffer *buf,
                        struct splice_desc *sd)
{
    return sd->len;
}

static ssize_t splice_write_null(struct pipe_inode_info *pipe, struct file *out,
                                 loff_t *ppos, size_t len, unsigned int flags)
{
    return splice_from_pipe(pipe, out, ppos, len, flags, pipe_to_null);
}
```

#### file_operations 定义

```c
static const struct file_operations null_fops = {
    .llseek      = null_lseek,
    .read        = read_null,
    .write       = write_null,
    .splice_write = splice_write_null,
};
```

### 2. /dev/zero 的实现

#### 读操作 - 返回无限的零

```c
static ssize_t read_zero(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    size_t written;

    if (!count)
        return 0;

    // 验证用户空间地址是否可写
    if (!access_ok(VERIFY_WRITE, buf, count))
        return -EFAULT;

    written = 0;
    while (count) {
        unsigned long unwritten;
        size_t chunk = count;

        // 每次最多处理一页，避免长时间占用 CPU
        if (chunk > PAGE_SIZE)
            chunk = PAGE_SIZE;

        // 将用户空间内存清零
        unwritten = __clear_user(buf, chunk);
        written += chunk - unwritten;
        
        if (unwritten)
            break;

        // 检查是否有信号待处理
        if (signal_pending(current))
            return written ? written : -ERESTARTSYS;

        buf += chunk;
        count -= chunk;

        // 让出 CPU，防止长时间占用
        cond_resched();
    }
    return written ? written : -EFAULT;
}
```

**关键技术点**:

1. `access_ok(VERIFY_WRITE, buf, count)` - 验证用户空间地址有效性
2. `__clear_user(buf, chunk)` - 将用户空间内存清零
3. `signal_pending(current)` - 检查是否有信号待处理
4. `cond_resched()` - 条件性让出 CPU，防止长时间占用

#### mmap 操作

```c
static int mmap_zero(struct file *file, struct vm_area_struct *vma)
{
#ifndef CONFIG_MMU
    return -ENOSYS;
#endif
    if (vma->vm_flags & VM_SHARED)
        return shmem_zero_setup(vma);  // 共享映射使用 tmpfs
    return 0;  // 私有映射返回匿名零页
}
```

### 3. /dev/mem 的实现

#### 读物理内存

```c
static ssize_t read_mem(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
    unsigned long p = *ppos;  // 物理地址
    ssize_t read, sz;
    char *ptr;

    // 验证物理地址范围
    if (!valid_phys_addr_range(p, count))
        return -EFAULT;

    read = 0;

    while (count > 0) {
        unsigned long remaining;

        sz = size_inside_page(p, count);

        // 检查是否允许访问该内存区域
        if (!range_is_allowed(p >> PAGE_SHIFT, count))
            return -EPERM;

        // 将物理地址映射为虚拟地址
        ptr = xlate_dev_mem_ptr(p);
        if (!ptr)
            return -EFAULT;

        // 复制到用户空间
        remaining = copy_to_user(buf, ptr, sz);
        unxlate_dev_mem_ptr(p, ptr);

        if (remaining)
            return -EFAULT;

        buf += sz;
        p += sz;
        count -= sz;
        read += sz;
    }

    *ppos += read;
    return read;
}
```

### 4. 设备表定义

```c
static const struct memdev {
    const char *name;
    mode_t mode;
    const struct file_operations *fops;
    struct backing_dev_info *dev_info;
} devlist[] = {
     [1] = { "mem",     0,    &mem_fops,    &directly_mappable_cdev_bdi },
#ifdef CONFIG_DEVKMEM
     [2] = { "kmem",    0,    &kmem_fops,   &directly_mappable_cdev_bdi },
#endif
     [3] = { "null",    0666, &null_fops,   NULL },
#ifdef CONFIG_DEVPORT
     [4] = { "port",    0,    &port_fops,   NULL },
#endif
     [5] = { "zero",    0666, &zero_fops,   &zero_bdi },
     [7] = { "full",    0666, &full_fops,   NULL },
     [8] = { "random",  0666, &random_fops, NULL },
     [9] = { "urandom", 0666, &urandom_fops, NULL },
    [11] = { "kmsg",    0,    &kmsg_fops,   NULL },
#ifdef CONFIG_CRASH_DUMP
    [12] = { "oldmem",  0,    &oldmem_fops, NULL },
#endif
};
```

**设备号分配**:

| 次设备号 | 设备名 | 功能 |
|---------|-------|------|
| 1 | `/dev/mem` | 物理内存访问 |
| 2 | `/dev/kmem` | 内核虚拟内存访问 |
| 3 | `/dev/null` | 数据黑洞 |
| 4 | `/dev/port` | I/O 端口访问 |
| 5 | `/dev/zero` | 零源 |
| 7 | `/dev/full` | 永远满的设备 (写入返回 ENOSPC) |
| 8 | `/dev/random` | 阻塞式随机数 |
| 9 | `/dev/urandom` | 非阻塞式随机数 |
| 11 | `/dev/kmsg` | 内核消息 |

### 5. /dev/full 的实现

```c
static ssize_t write_full(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos)
{
    return -ENOSPC;  // 永远返回 "磁盘已满"
}

#define read_full   read_zero  // 读取返回零，和 /dev/zero 相同
```

### 6. 驱动初始化

```c
static int __init chr_dev_init(void)
{
    int minor;
    int err;

    // 初始化 backing_dev_info
    err = bdi_init(&zero_bdi);
    if (err)
        return err;

    // 注册字符设备 (主设备号为 MEM_MAJOR = 1)
    if (register_chrdev(MEM_MAJOR, "mem", &memory_fops))
        printk("unable to get major %d for memory devs\n", MEM_MAJOR);

    // 创建设备类 (/sys/class/mem)
    mem_class = class_create(THIS_MODULE, "mem");
    if (IS_ERR(mem_class))
        return PTR_ERR(mem_class);

    mem_class->devnode = mem_devnode;

    // 创建各个设备节点
    for (minor = 1; minor < ARRAY_SIZE(devlist); minor++) {
        if (!devlist[minor].name)
            continue;
        // 创建 /dev/xxx 设备节点
        device_create(mem_class, NULL, MKDEV(MEM_MAJOR, minor),
                      NULL, devlist[minor].name);
    }

    return tty_init();
}

// 使用 fs_initcall 确保在文件系统初始化后执行
fs_initcall(chr_dev_init);
```

**初始化流程**:

```
chr_dev_init()
    │
    ├── bdi_init(&zero_bdi)
    │       └── 初始化 backing_dev_info
    │
    ├── register_chrdev(MEM_MAJOR, "mem", &memory_fops)
    │       └── 注册主设备号 1
    │
    ├── class_create(THIS_MODULE, "mem")
    │       └── 创建 /sys/class/mem
    │
    └── for each device in devlist:
            device_create(...)
                └── 创建 /dev/null, /dev/zero, /dev/mem 等
```

---

## 杂项设备驱动框架 (misc)

### 源码位置: `drivers/char/misc.c`

杂项设备框架提供了更简单的驱动注册方式，所有杂项设备共享主设备号 10。

### 核心数据结构

```c
// 杂项设备链表头
static LIST_HEAD(misc_list);
static DEFINE_MUTEX(misc_mtx);

// 动态次设备号位图
#define DYNAMIC_MINORS 64
static DECLARE_BITMAP(misc_minors, DYNAMIC_MINORS);
```

### 注册函数: misc_register()

```c
int misc_register(struct miscdevice *misc)
{
    struct miscdevice *c;
    dev_t dev;
    int err = 0;

    // 初始化链表节点
    INIT_LIST_HEAD(&misc->list);

    mutex_lock(&misc_mtx);

    // 检查次设备号是否冲突
    list_for_each_entry(c, &misc_list, list) {
        if (c->minor == misc->minor) {
            mutex_unlock(&misc_mtx);
            return -EBUSY;
        }
    }

    // 如果请求动态分配次设备号
    if (misc->minor == MISC_DYNAMIC_MINOR) {
        int i = find_first_zero_bit(misc_minors, DYNAMIC_MINORS);
        if (i >= DYNAMIC_MINORS) {
            mutex_unlock(&misc_mtx);
            return -EBUSY;
        }
        misc->minor = DYNAMIC_MINORS - i - 1;
        set_bit(i, misc_minors);
    }

    // 构造设备号
    dev = MKDEV(MISC_MAJOR, misc->minor);

    // 创建设备节点
    misc->this_device = device_create(misc_class, misc->parent, dev,
                                      misc, "%s", misc->name);
    if (IS_ERR(misc->this_device)) {
        int i = DYNAMIC_MINORS - misc->minor - 1;
        if (i < DYNAMIC_MINORS && i >= 0)
            clear_bit(i, misc_minors);
        err = PTR_ERR(misc->this_device);
        goto out;
    }

    // 添加到链表头部
    list_add(&misc->list, &misc_list);
out:
    mutex_unlock(&misc_mtx);
    return err;
}

EXPORT_SYMBOL(misc_register);
```

### 注销函数: misc_deregister()

```c
int misc_deregister(struct miscdevice *misc)
{
    int i = DYNAMIC_MINORS - misc->minor - 1;

    if (WARN_ON(list_empty(&misc->list)))
        return -EINVAL;

    mutex_lock(&misc_mtx);

    // 从链表中删除
    list_del(&misc->list);

    // 销毁设备节点
    device_destroy(misc_class, MKDEV(MISC_MAJOR, misc->minor));

    // 释放动态次设备号
    if (i < DYNAMIC_MINORS && i >= 0)
        clear_bit(i, misc_minors);

    mutex_unlock(&misc_mtx);
    return 0;
}

EXPORT_SYMBOL(misc_deregister);
```

### 打开操作: misc_open()

```c
static int misc_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    struct miscdevice *c;
    int err = -ENODEV;
    const struct file_operations *old_fops, *new_fops = NULL;

    mutex_lock(&misc_mtx);

    // 在链表中查找对应的设备
    list_for_each_entry(c, &misc_list, list) {
        if (c->minor == minor) {
            new_fops = fops_get(c->fops);
            break;
        }
    }

    // 如果没找到，尝试加载模块
    if (!new_fops) {
        mutex_unlock(&misc_mtx);
        request_module("char-major-%d-%d", MISC_MAJOR, minor);
        mutex_lock(&misc_mtx);

        list_for_each_entry(c, &misc_list, list) {
            if (c->minor == minor) {
                new_fops = fops_get(c->fops);
                break;
            }
        }
        if (!new_fops)
            goto fail;
    }

    err = 0;
    old_fops = file->f_op;
    file->f_op = new_fops;  // 替换为具体驱动的 fops

    // 调用具体驱动的 open
    if (file->f_op->open) {
        file->private_data = c;
        err = file->f_op->open(inode, file);
        if (err) {
            fops_put(file->f_op);
            file->f_op = fops_get(old_fops);
        }
    }
    fops_put(old_fops);
fail:
    mutex_unlock(&misc_mtx);
    return err;
}
```

### 预定义的次设备号

```c
#define PSMOUSE_MINOR       1
#define MS_BUSMOUSE_MINOR   2
#define WATCHDOG_MINOR      130
#define TEMP_MINOR          131
#define RTC_MINOR           135
#define NVRAM_MINOR         144
#define TUN_MINOR           200
#define HPET_MINOR          228
#define FUSE_MINOR          229
#define KVM_MINOR           232
#define BTRFS_MINOR         234
#define LOOP_CTRL_MINOR     237
#define MISC_DYNAMIC_MINOR  255  // 动态分配
```

---

## 驱动核心框架 (drivers/base/)

### 设备模型层次

```
                    ┌─────────────────────────┐
                    │       kobject           │
                    │   (内核对象基类)         │
                    └───────────┬─────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
        ▼                       ▼                       ▼
┌───────────────┐       ┌───────────────┐       ┌───────────────┐
│    device     │       │    driver     │       │     bus       │
│  (设备实例)   │       │  (驱动程序)   │       │    (总线)     │
│               │       │               │       │               │
│ - 硬件信息    │       │ - probe       │       │ - match       │
│ - 设备号      │       │ - remove      │       │ - uevent      │
│ - 电源状态    │       │ - shutdown    │       │ - 设备列表    │
└───────┬───────┘       └───────┬───────┘       └───────┬───────┘
        │                       │                       │
        └───────────────────────┴───────────────────────┘
                                │
                         ┌──────┴──────┐
                         │  matching   │
                         │  (probe)    │
                         └─────────────┘
```

### struct device 核心结构

```c
struct device {
    struct device       *parent;        // 父设备
    struct device_private *p;           // 私有数据
    struct kobject      kobj;           // 内核对象
    const char          *init_name;     // 初始名称
    const struct device_type *type;     // 设备类型
    struct mutex        mutex;          // 互斥锁
    struct bus_type     *bus;           // 所属总线
    struct device_driver *driver;       // 绑定的驱动
    void                *platform_data; // 平台数据
    struct dev_pm_info  power;          // 电源管理
    dev_t               devt;           // 设备号
    struct class        *class;         // 设备类
    void (*release)(struct device *dev); // 释放函数
    // ...
};
```

### device_add() - 设备注册核心流程

```c
int device_add(struct device *dev)
{
    struct device *parent = NULL;
    int error = -EINVAL;

    dev = get_device(dev);
    if (!dev)
        goto done;

    // 初始化私有数据
    if (!dev->p) {
        error = device_private_init(dev);
        if (error)
            goto done;
    }

    // 设置设备名称
    if (dev->init_name) {
        dev_set_name(dev, "%s", dev->init_name);
        dev->init_name = NULL;
    }

    // 设置父设备
    parent = get_device(dev->parent);
    setup_parent(dev, parent);

    // 1. 添加到 kobject 层次结构
    error = kobject_add(&dev->kobj, dev->kobj.parent, NULL);
    if (error)
        goto Error;

    // 2. 创建 sysfs 属性文件
    error = device_create_file(dev, &uevent_attr);
    if (error)
        goto attrError;

    // 3. 创建设备号相关文件
    if (MAJOR(dev->devt)) {
        error = device_create_file(dev, &devt_attr);
        if (error)
            goto ueventattrError;

        error = device_create_sys_dev_entry(dev);
        if (error)
            goto devtattrError;

        // 4. 通过 devtmpfs 创建 /dev 节点
        devtmpfs_create_node(dev);
    }

    // 5. 创建类符号链接
    error = device_add_class_symlinks(dev);

    // 6. 添加设备属性
    error = device_add_attrs(dev);

    // 7. 添加到总线
    error = bus_add_device(dev);

    // 8. 添加电源管理
    device_pm_add(dev);

    // 9. 发送 uevent 到用户空间 (udev)
    kobject_uevent(&dev->kobj, KOBJ_ADD);

    // 10. 触发驱动匹配 (probe)
    bus_probe_device(dev);

    return 0;
    // ... 错误处理 ...
}
```

### device_register() - 完整的设备注册

```c
int device_register(struct device *dev)
{
    device_initialize(dev);  // 初始化设备
    return device_add(dev);  // 添加设备
}
```

### device_create() - 简化的设备创建

```c
struct device *device_create(struct class *class, struct device *parent,
                             dev_t devt, void *drvdata, const char *fmt, ...)
{
    va_list vargs;
    struct device *dev;

    va_start(vargs, fmt);
    dev = device_create_vargs(class, parent, devt, drvdata, fmt, vargs);
    va_end(vargs);
    return dev;
}
```

---

## 编写字符设备驱动的三种方式

### 方式一：传统字符设备 (使用 cdev)

这是最灵活但也最复杂的方式。

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mydev"
#define CLASS_NAME  "myclass"

static dev_t dev_num;           // 设备号
static struct cdev my_cdev;     // cdev 结构
static struct class *my_class;  // 设备类
static struct device *my_device;// 设备

// ============= 驱动操作函数 =============

static int my_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mydev: device opened\n");
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mydev: device closed\n");
    return 0;
}

static ssize_t my_read(struct file *file, char __user *buf,
                       size_t count, loff_t *ppos)
{
    char kernel_buf[] = "Hello from kernel!\n";
    size_t len = strlen(kernel_buf);

    if (*ppos >= len)
        return 0;

    if (count > len - *ppos)
        count = len - *ppos;

    if (copy_to_user(buf, kernel_buf + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static ssize_t my_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    char kernel_buf[256];

    if (count > sizeof(kernel_buf) - 1)
        count = sizeof(kernel_buf) - 1;

    if (copy_from_user(kernel_buf, buf, count))
        return -EFAULT;

    kernel_buf[count] = '\0';
    printk(KERN_INFO "mydev: received %zu bytes: %s\n", count, kernel_buf);

    return count;
}

// ============= file_operations =============

static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .write   = my_write,
};

// ============= 模块初始化 =============

static int __init my_init(void)
{
    int ret;

    // 1. 动态分配设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "Failed to allocate device number\n");
        return ret;
    }
    printk(KERN_INFO "mydev: major=%d, minor=%d\n",
           MAJOR(dev_num), MINOR(dev_num));

    // 2. 初始化 cdev
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;

    // 3. 添加 cdev 到系统
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "Failed to add cdev\n");
        goto fail_cdev;
    }

    // 4. 创建设备类 (/sys/class/myclass)
    my_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_class)) {
        printk(KERN_ERR "Failed to create class\n");
        ret = PTR_ERR(my_class);
        goto fail_class;
    }

    // 5. 创建设备节点 (/dev/mydev)
    my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        printk(KERN_ERR "Failed to create device\n");
        ret = PTR_ERR(my_device);
        goto fail_device;
    }

    printk(KERN_INFO "mydev: driver loaded\n");
    return 0;

fail_device:
    class_destroy(my_class);
fail_class:
    cdev_del(&my_cdev);
fail_cdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

// ============= 模块退出 =============

static void __exit my_exit(void)
{
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "mydev: driver unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple character device driver");
```

### 方式二：杂项设备 (最简单推荐)

```c
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

static ssize_t my_read(struct file *file, char __user *buf,
                       size_t count, loff_t *ppos)
{
    char msg[] = "Hello from misc device!\n";
    size_t len = strlen(msg);

    if (*ppos >= len)
        return 0;

    if (count > len - *ppos)
        count = len - *ppos;

    if (copy_to_user(buf, msg + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static ssize_t my_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    char kernel_buf[256];

    if (count > sizeof(kernel_buf) - 1)
        count = sizeof(kernel_buf) - 1;

    if (copy_from_user(kernel_buf, buf, count))
        return -EFAULT;

    kernel_buf[count] = '\0';
    printk(KERN_INFO "mymisc: received: %s\n", kernel_buf);

    return count;
}

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .read  = my_read,
    .write = my_write,
};

static struct miscdevice my_misc = {
    .minor = MISC_DYNAMIC_MINOR,  // 动态分配次设备号
    .name  = "mymisc",            // /dev/mymisc
    .fops  = &my_fops,
};

static int __init my_init(void)
{
    int ret = misc_register(&my_misc);
    if (ret) {
        printk(KERN_ERR "Failed to register misc device\n");
        return ret;
    }
    printk(KERN_INFO "mymisc: registered with minor=%d\n", my_misc.minor);
    return 0;
}

static void __exit my_exit(void)
{
    misc_deregister(&my_misc);
    printk(KERN_INFO "mymisc: unregistered\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple misc device driver");
```

### 方式三：直接注册 (老式方法，不推荐)

```c
#include <linux/module.h>
#include <linux/fs.h>

#define MY_MAJOR 240  // 手动指定主设备号

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    // ... 其他操作
};

static int __init my_init(void)
{
    int ret = register_chrdev(MY_MAJOR, "mydev", &my_fops);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register device\n");
        return ret;
    }
    return 0;
}

static void __exit my_exit(void)
{
    unregister_chrdev(MY_MAJOR, "mydev");
}

module_init(my_init);
module_exit(my_exit);
```

**注意**: 这种方式需要手动指定主设备号，且不会自动创建 /dev 节点。

### 三种方式比较

| 特性 | 传统 cdev | 杂项设备 | 直接注册 |
|-----|----------|---------|---------|
| 复杂度 | 高 | 低 | 中 |
| 设备号 | 动态/静态 | 动态 | 静态 |
| 自动创建 /dev | ✓ | ✓ | ✗ |
| 灵活性 | 高 | 中 | 低 |
| 多设备支持 | ✓ | 有限 | ✓ |
| 推荐场景 | 复杂驱动 | 简单驱动 | 不推荐 |

---

## 用户空间与内核空间数据传输

### 地址空间隔离

```
┌─────────────────────────────────────────────────────────────────┐
│                     用户空间 (0 ~ 3GB/0 ~ 128TB)                 │
│                                                                  │
│    用户程序可以访问                                               │
│    char user_buf[1024];  // 用户缓冲区                           │
│                                                                  │
├─────────────────────────────────────────────────────────────────┤
│                     内核空间 (3GB ~ 4GB / 高地址)                 │
│                                                                  │
│    只有内核代码可以访问                                           │
│    char kernel_buf[1024];  // 内核缓冲区                         │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 关键函数

| 函数 | 方向 | 用途 | 返回值 |
|------|------|------|--------|
| `copy_to_user(to, from, n)` | 内核 → 用户 | read 操作 | 未复制字节数 |
| `copy_from_user(to, from, n)` | 用户 → 内核 | write 操作 | 未复制字节数 |
| `get_user(x, ptr)` | 用户 → 内核 | 读取单个值 | 0 成功，非0 失败 |
| `put_user(x, ptr)` | 内核 → 用户 | 写入单个值 | 0 成功，非0 失败 |
| `access_ok(type, addr, size)` | - | 验证地址 | 1 有效，0 无效 |
| `__copy_to_user()` | 内核 → 用户 | 无检查版本 | 未复制字节数 |
| `__copy_from_user()` | 用户 → 内核 | 无检查版本 | 未复制字节数 |

### copy_to_user 使用示例

```c
static ssize_t my_read(struct file *file, char __user *buf,
                       size_t count, loff_t *ppos)
{
    char kernel_buf[128] = "Data from kernel";
    size_t len = strlen(kernel_buf) + 1;

    // 检查偏移量
    if (*ppos >= len)
        return 0;  // EOF

    // 调整 count
    if (count > len - *ppos)
        count = len - *ppos;

    // 复制到用户空间
    if (copy_to_user(buf, kernel_buf + *ppos, count)) {
        return -EFAULT;  // 地址错误
    }

    *ppos += count;
    return count;  // 返回实际读取的字节数
}
```

### copy_from_user 使用示例

```c
static ssize_t my_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    char kernel_buf[256];

    // 限制大小
    if (count > sizeof(kernel_buf) - 1)
        count = sizeof(kernel_buf) - 1;

    // 从用户空间复制
    if (copy_from_user(kernel_buf, buf, count)) {
        return -EFAULT;
    }

    kernel_buf[count] = '\0';

    // 处理数据
    printk(KERN_INFO "Received: %s\n", kernel_buf);

    return count;  // 返回实际写入的字节数
}
```

### get_user / put_user 示例

```c
// 读取单个整数
int val;
if (get_user(val, (int __user *)arg))
    return -EFAULT;

// 写入单个整数
if (put_user(result, (int __user *)arg))
    return -EFAULT;
```

---

## 驱动与系统的交互流程

### 完整的打开流程

```
用户程序                    VFS 层                       驱动层
    │                         │                            │
    │  fd = open("/dev/xxx", O_RDWR)                       │
    ├────────────────────────►│                            │
    │                         │                            │
    │                    sys_open()                        │
    │                         │                            │
    │                    do_sys_open()                     │
    │                         │                            │
    │                    do_filp_open()                    │
    │                         │                            │
    │                    path_openat()                     │
    │                         │                            │
    │                    do_last()                         │
    │                         │                            │
    │                    vfs_open()                        │
    │                         │                            │
    │                    chrdev_open()                     │
    │                         │                            │
    │                    查找 cdev 结构                    │
    │                         │                            │
    │                    获取 file_operations              │
    │                         │                            │
    │                    调用 fops->open()                 │
    │                         ├───────────────────────────►│
    │                         │                            │ my_open(inode, file)
    │                         │                            │   - 分配资源
    │                         │                            │   - 初始化硬件
    │                         │◄───────────────────────────┤
    │                         │                            │
    │◄────────────────────────┤                            │
    │  返回 fd                 │                            │
```

### 读操作流程

```
用户程序                    VFS 层                       驱动层
    │                         │                            │
    │  read(fd, buf, 1024)    │                            │
    ├────────────────────────►│                            │
    │                         │                            │
    │                    sys_read()                        │
    │                         │                            │
    │                    vfs_read()                        │
    │                         │                            │
    │                    do_sync_read() 或直接调用         │
    │                         │                            │
    │                    file->f_op->read()                │
    │                         ├───────────────────────────►│
    │                         │                            │ my_read(file, buf, count, ppos)
    │                         │                            │   - 准备数据
    │                         │                            │   - copy_to_user(buf, data, n)
    │                         │◄───────────────────────────┤
    │                         │                            │
    │◄────────────────────────┤                            │
    │  返回读取的字节数         │                            │
```

### ioctl 流程

```
用户程序                    VFS 层                       驱动层
    │                         │                            │
    │  ioctl(fd, cmd, arg)    │                            │
    ├────────────────────────►│                            │
    │                         │                            │
    │                    sys_ioctl()                       │
    │                         │                            │
    │                    do_vfs_ioctl()                    │
    │                         │                            │
    │                    vfs_ioctl()                       │
    │                         │                            │
    │                    file->f_op->unlocked_ioctl()      │
    │                         ├───────────────────────────►│
    │                         │                            │ my_ioctl(file, cmd, arg)
    │                         │                            │   switch (cmd) {
    │                         │                            │     case CMD1: ...
    │                         │                            │     case CMD2: ...
    │                         │                            │   }
    │                         │◄───────────────────────────┤
    │                         │                            │
    │◄────────────────────────┤                            │
    │  返回结果                 │                            │
```

---

## 驱动开发要点总结

### 1. 核心接口

- 必须实现 `struct file_operations` 中的相关函数
- `owner` 字段必须设置为 `THIS_MODULE`

### 2. 设备号管理

```c
// 推荐：动态分配
alloc_chrdev_region(&dev, 0, count, "name");

// 静态分配（知道主设备号）
register_chrdev_region(MKDEV(major, 0), count, "name");

// 释放
unregister_chrdev_region(dev, count);
```

### 3. 设备注册流程

```c
// 方式1: cdev
cdev_init(&cdev, &fops);
cdev_add(&cdev, dev, count);

// 方式2: misc
misc_register(&miscdev);

// 方式3: 老式（不推荐）
register_chrdev(major, "name", &fops);
```

### 4. 创建设备节点

```c
// 创建类
class = class_create(THIS_MODULE, "classname");

// 创建设备（自动创建 /dev/xxx）
device = device_create(class, NULL, dev, NULL, "devname");

// 销毁
device_destroy(class, dev);
class_destroy(class);
```

### 5. 数据传输

- 始终使用 `copy_to_user()` / `copy_from_user()`
- 检查返回值
- 处理偏移量 `*ppos`

### 6. 错误处理

- 返回负的错误码：`-EFAULT`, `-ENOMEM`, `-EINVAL` 等
- 使用 `goto` 进行资源清理

### 7. 并发控制

```c
// 互斥锁
static DEFINE_MUTEX(my_mutex);
mutex_lock(&my_mutex);
// 临界区
mutex_unlock(&my_mutex);

// 自旋锁（不能睡眠）
static DEFINE_SPINLOCK(my_lock);
spin_lock(&my_lock);
// 临界区
spin_unlock(&my_lock);
```

### 8. 内存管理

```c
// 分配内存
ptr = kmalloc(size, GFP_KERNEL);
ptr = kzalloc(size, GFP_KERNEL);  // 清零

// 释放内存
kfree(ptr);
```

### 9. 调试技巧

```c
// 打印调试信息
printk(KERN_DEBUG "debug: value=%d\n", val);
printk(KERN_INFO  "info: ...\n");
printk(KERN_ERR   "error: ...\n");

// 或使用 pr_* 系列
pr_debug("...");
pr_info("...");
pr_err("...");

// 设备相关打印
dev_dbg(dev, "...");
dev_info(dev, "...");
dev_err(dev, "...");
```

---

## 附录：常用头文件

| 头文件 | 用途 |
|-------|------|
| `<linux/module.h>` | 模块相关 |
| `<linux/init.h>` | 初始化宏 |
| `<linux/fs.h>` | 文件系统、file_operations |
| `<linux/cdev.h>` | 字符设备 |
| `<linux/device.h>` | 设备模型 |
| `<linux/uaccess.h>` | 用户空间访问 |
| `<linux/slab.h>` | 内存分配 |
| `<linux/mutex.h>` | 互斥锁 |
| `<linux/spinlock.h>` | 自旋锁 |
| `<linux/miscdevice.h>` | 杂项设备 |
| `<linux/ioctl.h>` | ioctl 相关 |
| `<linux/errno.h>` | 错误码 |

---

## 附录：Makefile 模板

```makefile
# 内核模块 Makefile

obj-m := mydriver.o

# 如果有多个源文件
# mydriver-objs := file1.o file2.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# 加载模块
load:
	sudo insmod mydriver.ko

# 卸载模块
unload:
	sudo rmmod mydriver

# 查看日志
log:
	dmesg | tail -20
```

---

*本文档基于 Linux 3.2 内核源码分析生成*

