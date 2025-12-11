# Character Device Driver Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel character device driver internals, data structures, and APIs
> **Language**: English terms with Chinese explanations
> **Total Cards**: 80+

---

## 1. Core Concepts (核心概念)

---

Q: What is a character device in Linux?
A: 字符设备是Linux设备模型中的一种设备类型，特点是：
- 按字节流访问（非块访问）
- 通常不可随机访问（顺序读写）
- 通过设备文件（/dev/xxx）访问
- 使用主设备号(major)和次设备号(minor)标识

常见字符设备：终端(/dev/tty)、串口(/dev/ttyS0)、键盘、鼠标、GPU(/dev/dri)等。
[Basic]

---

Q: What is the difference between character devices and block devices?
A: 
| 特性 | Character Device | Block Device |
|------|------------------|--------------|
| 访问方式 | 字节流，顺序访问 | 固定大小块，随机访问 |
| 缓冲 | 无缓冲（直接I/O） | 有缓冲（通过page cache） |
| 典型设备 | 终端、串口、键盘 | 硬盘、SSD、U盘 |
| 文件系统 | 不能挂载文件系统 | 可挂载文件系统 |
| 数据结构 | `struct cdev` | `struct gendisk` |

字符设备用`c`表示，块设备用`b`表示（`ls -l /dev`）。
[Basic]

---

Q: What is a device number (dev_t) and how is it composed?
A: `dev_t`是32位设备号，由主设备号和次设备号组成：
```c
// include/linux/kdev_t.h
#define MAJOR(dev)    ((dev) >> 20)           // 高12位：主设备号
#define MINOR(dev)    ((dev) & ((1 << 20) - 1)) // 低20位：次设备号
#define MKDEV(ma, mi) ((ma) << 20 | (mi))      // 组合设备号

// 示例
dev_t dev = MKDEV(250, 0);  // 主250，次0
int major = MAJOR(dev);      // 250
int minor = MINOR(dev);      // 0
```
主设备号标识驱动程序，次设备号标识该驱动管理的具体设备实例。
[Basic]

---

Q: What is the difference between static and dynamic device number allocation?
A: 
**静态分配**：使用固定的主设备号
```c
#define MY_MAJOR 250
register_chrdev_region(MKDEV(MY_MAJOR, 0), count, "mydev");
```
缺点：可能与其他驱动冲突

**动态分配**：由内核分配可用的主设备号
```c
dev_t dev;
alloc_chrdev_region(&dev, 0, count, "mydev");
int major = MAJOR(dev);  // 获取分配的主设备号
```
优点：避免冲突，推荐使用
[Basic]

---

Q: What is the complete lifecycle of a character device driver?
A: 
```
module_init()
    |
    v
+-------------------+
| 1. 分配设备号      |  alloc_chrdev_region()
+-------------------+
    |
    v
+-------------------+
| 2. 初始化cdev     |  cdev_init()
+-------------------+
    |
    v
+-------------------+
| 3. 注册cdev       |  cdev_add()
+-------------------+
    |
    v
+-------------------+
| 4. 创建设备类     |  class_create()  (可选，用于udev)
+-------------------+
    |
    v
+-------------------+
| 5. 创建设备节点   |  device_create() (可选，自动创建/dev节点)
+-------------------+
    |
    |  <--- 设备可用，处理用户请求 --->
    |
    v
module_exit()
    |
    v
+-------------------+
| 6. 删除设备节点   |  device_destroy()
| 7. 删除设备类     |  class_destroy()
| 8. 注销cdev       |  cdev_del()
| 9. 释放设备号     |  unregister_chrdev_region()
+-------------------+
```
[Intermediate]

---

## 2. Key Data Structures (核心数据结构)

---

Q: What is `struct cdev` and what are its key fields?
A: `cdev`是字符设备的内核表示：
```c
// include/linux/cdev.h
struct cdev {
    struct kobject kobj;              // 内嵌kobject，用于sysfs
    struct module *owner;             // 所属模块（THIS_MODULE）
    const struct file_operations *ops; // 文件操作函数表
    struct list_head list;            // 链表节点
    dev_t dev;                        // 设备号
    unsigned int count;               // 次设备号数量
};
```
每个字符设备实例对应一个cdev结构。
[Basic]

---

Q: What is `struct file_operations` and list its key callbacks?
A: `file_operations`定义设备文件的操作函数：
```c
struct file_operations {
    struct module *owner;             // THIS_MODULE
    
    // 文件操作
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    
    // 读写操作
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    
    // 定位
    loff_t (*llseek)(struct file *, loff_t, int);
    
    // IO控制
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    long (*compat_ioctl)(struct file *, unsigned int, unsigned long);
    
    // 内存映射
    int (*mmap)(struct file *, struct vm_area_struct *);
    
    // 异步通知
    unsigned int (*poll)(struct file *, struct poll_table_struct *);
    int (*fasync)(int, struct file *, int);
};
```
[Basic]

---

Q: What is `struct file` and what does it represent?
A: `struct file`表示一个打开的文件实例：
```c
struct file {
    struct path f_path;           // 文件路径（dentry + vfsmount）
    struct inode *f_inode;        // 关联的inode
    const struct file_operations *f_op;  // 文件操作函数
    
    spinlock_t f_lock;            // 保护f_flags等
    atomic_long_t f_count;        // 引用计数
    unsigned int f_flags;         // 打开标志 (O_RDONLY, O_NONBLOCK等)
    fmode_t f_mode;               // 访问模式 (FMODE_READ, FMODE_WRITE)
    loff_t f_pos;                 // 当前读写位置
    
    void *private_data;           // 驱动私有数据 ★重要
    
    struct address_space *f_mapping; // 页面缓存映射
};
```
每次`open()`创建新的file结构，`private_data`用于存储驱动特定数据。
[Intermediate]

---

Q: What is `struct inode` and how does it relate to `struct file`?
A: `struct inode`表示文件系统中的一个文件（唯一）：
```c
struct inode {
    umode_t i_mode;               // 文件类型和权限
    uid_t i_uid;                  // 所有者UID
    gid_t i_gid;                  // 所有者GID
    dev_t i_rdev;                 // 设备号（设备文件）★重要
    
    loff_t i_size;                // 文件大小
    struct timespec i_atime;      // 访问时间
    struct timespec i_mtime;      // 修改时间
    
    const struct file_operations *i_fop;  // 文件操作
    
    union {
        struct cdev *i_cdev;      // 字符设备指针 ★重要
        struct block_device *i_bdev;
    };
    
    void *i_private;              // 文件系统私有数据
};
```
关系：一个inode可对应多个file（多次open同一文件）。
[Intermediate]

---

Q: How to get device number from `struct inode`?
A: 
```c
// 在open()回调中获取设备号
static int mydev_open(struct inode *inode, struct file *filp)
{
    // 方法1：直接从inode获取
    dev_t dev = inode->i_rdev;
    int major = MAJOR(dev);
    int minor = MINOR(dev);
    
    // 方法2：从cdev获取（推荐）
    struct cdev *cdev = inode->i_cdev;
    dev_t dev = cdev->dev;
    
    // 方法3：使用iminor()/imajor()宏
    int major = imajor(inode);
    int minor = iminor(inode);
    
    return 0;
}
```
`iminor()`和`imajor()`是推荐的获取方式。
[Basic]

---

Q: What is the purpose of `file->private_data`?
A: `private_data`用于存储驱动的私有数据，在open时设置，后续操作中使用：
```c
struct mydev_data {
    struct cdev cdev;
    char buffer[1024];
    spinlock_t lock;
    int open_count;
};

static int mydev_open(struct inode *inode, struct file *filp)
{
    struct mydev_data *dev;
    
    // 从cdev获取包含它的结构体
    dev = container_of(inode->i_cdev, struct mydev_data, cdev);
    
    // 保存到private_data，供其他操作使用
    filp->private_data = dev;
    
    return 0;
}

static ssize_t mydev_read(struct file *filp, char __user *buf, 
                          size_t count, loff_t *f_pos)
{
    // 从private_data获取设备数据
    struct mydev_data *dev = filp->private_data;
    
    // 访问设备数据
    spin_lock(&dev->lock);
    // ...
    spin_unlock(&dev->lock);
    
    return count;
}
```
[Intermediate]

---

Q: What is `container_of` macro and how is it used in drivers?
A: `container_of`从结构体成员指针获取包含它的结构体指针：
```c
// include/linux/kernel.h
#define container_of(ptr, type, member) ({                  \
    const typeof(((type *)0)->member) *__mptr = (ptr);      \
    (type *)((char *)__mptr - offsetof(type, member)); })

// 使用示例
struct mydev {
    int value;
    struct cdev cdev;  // cdev嵌入在mydev中
    char name[32];
};

// 从cdev指针获取mydev指针
struct cdev *cdev_ptr = inode->i_cdev;
struct mydev *dev = container_of(cdev_ptr, struct mydev, cdev);

// 原理：计算cdev在mydev中的偏移，然后减去偏移得到mydev地址
```
这是Linux内核实现面向对象的核心技巧。
[Intermediate]

---

## 3. Device Registration (设备注册)

---

Q: How to allocate device numbers using `alloc_chrdev_region()`?
A: 
```c
static dev_t dev_num;
static int dev_count = 1;

static int __init mydev_init(void)
{
    int ret;
    
    // 动态分配设备号
    // 参数：输出设备号、起始次设备号、设备数量、设备名
    ret = alloc_chrdev_region(&dev_num, 0, dev_count, "mydevice");
    if (ret < 0) {
        pr_err("Failed to allocate device number\n");
        return ret;
    }
    
    pr_info("Allocated major=%d, minor=%d\n", 
            MAJOR(dev_num), MINOR(dev_num));
    
    return 0;
}

static void __exit mydev_exit(void)
{
    // 释放设备号
    unregister_chrdev_region(dev_num, dev_count);
}
```
[Basic]

---

Q: How to initialize and register a cdev?
A: 
```c
static struct cdev my_cdev;

static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = mydev_open,
    .release = mydev_release,
    .read    = mydev_read,
    .write   = mydev_write,
};

static int __init mydev_init(void)
{
    int ret;
    
    // 1. 分配设备号
    ret = alloc_chrdev_region(&dev_num, 0, 1, "mydevice");
    if (ret < 0)
        return ret;
    
    // 2. 初始化cdev
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;
    
    // 3. 注册cdev到内核
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    
    return 0;
}

static void __exit mydev_exit(void)
{
    cdev_del(&my_cdev);                       // 注销cdev
    unregister_chrdev_region(dev_num, 1);     // 释放设备号
}
```
[Basic]

---

Q: How to automatically create device nodes in /dev?
A: 使用`class_create()`和`device_create()`让udev自动创建设备节点：
```c
static struct class *my_class;
static struct device *my_device;

static int __init mydev_init(void)
{
    // 1-3: 分配设备号，初始化并注册cdev (同上)
    
    // 4. 创建设备类 (在/sys/class/下创建目录)
    my_class = class_create(THIS_MODULE, "myclass");
    if (IS_ERR(my_class)) {
        ret = PTR_ERR(my_class);
        goto fail_class;
    }
    
    // 5. 创建设备 (触发udev创建/dev/mydevice)
    my_device = device_create(my_class, NULL, dev_num, NULL, "mydevice");
    if (IS_ERR(my_device)) {
        ret = PTR_ERR(my_device);
        goto fail_device;
    }
    
    return 0;
    
fail_device:
    class_destroy(my_class);
fail_class:
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit mydev_exit(void)
{
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
}
```
[Intermediate]

---

Q: What is the old `register_chrdev()` API and why is it deprecated?
A: 
```c
// 旧API（不推荐使用）
int register_chrdev(unsigned int major, const char *name,
                    const struct file_operations *fops);
void unregister_chrdev(unsigned int major, const char *name);
```
**缺点**：
1. 占用整个主设备号的所有256个次设备号
2. 无法使用超过256的次设备号
3. 使用静态主设备号可能冲突

**新API优势**：
- `alloc_chrdev_region()`: 动态分配，支持更多次设备号
- `cdev_init()/cdev_add()`: 更精细的控制
- 支持多个设备实例

旧API仅用于快速原型或兼容旧代码。
[Intermediate]

---

## 4. File Operations Implementation (文件操作实现)

---

Q: How to implement the `open()` callback?
A: 
```c
static int mydev_open(struct inode *inode, struct file *filp)
{
    struct mydev_data *dev;
    
    // 1. 获取设备私有数据
    dev = container_of(inode->i_cdev, struct mydev_data, cdev);
    filp->private_data = dev;
    
    // 2. 检查打开模式
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        // 只写模式
    }
    
    // 3. 检查是否非阻塞
    if (filp->f_flags & O_NONBLOCK) {
        // 非阻塞模式
    }
    
    // 4. 更新引用计数（可选）
    spin_lock(&dev->lock);
    dev->open_count++;
    spin_unlock(&dev->lock);
    
    // 5. 增加模块引用计数（通常由VFS自动处理）
    // try_module_get(THIS_MODULE);
    
    pr_info("Device opened, count=%d\n", dev->open_count);
    return 0;  // 成功返回0，失败返回负错误码
}
```
[Basic]

---

Q: How to implement the `release()` callback?
A: 
```c
static int mydev_release(struct inode *inode, struct file *filp)
{
    struct mydev_data *dev = filp->private_data;
    
    // 1. 更新引用计数
    spin_lock(&dev->lock);
    dev->open_count--;
    spin_unlock(&dev->lock);
    
    // 2. 释放资源（如果是最后一个关闭）
    if (dev->open_count == 0) {
        // 清理设备状态
    }
    
    // 3. 减少模块引用计数（通常由VFS自动处理）
    // module_put(THIS_MODULE);
    
    pr_info("Device closed, count=%d\n", dev->open_count);
    return 0;
}
```
注意：`release()`在最后一个引用关闭时调用（引用计数为0）。
[Basic]

---

Q: How to implement the `read()` callback correctly?
A: 
```c
static ssize_t mydev_read(struct file *filp, char __user *buf,
                          size_t count, loff_t *f_pos)
{
    struct mydev_data *dev = filp->private_data;
    ssize_t retval = 0;
    
    // 1. 加锁保护
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    
    // 2. 检查偏移量是否超出范围
    if (*f_pos >= dev->size) {
        retval = 0;  // EOF
        goto out;
    }
    
    // 3. 调整读取长度
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;
    
    // 4. 复制数据到用户空间（★关键）
    if (copy_to_user(buf, dev->buffer + *f_pos, count)) {
        retval = -EFAULT;
        goto out;
    }
    
    // 5. 更新文件位置
    *f_pos += count;
    retval = count;
    
out:
    mutex_unlock(&dev->mutex);
    return retval;  // 返回读取的字节数，或负错误码
}
```
[Intermediate]

---

Q: How to implement the `write()` callback correctly?
A: 
```c
static ssize_t mydev_write(struct file *filp, const char __user *buf,
                           size_t count, loff_t *f_pos)
{
    struct mydev_data *dev = filp->private_data;
    ssize_t retval = 0;
    
    // 1. 加锁保护
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    
    // 2. 检查写入位置和长度
    if (*f_pos >= BUFFER_SIZE) {
        retval = -ENOSPC;  // 空间不足
        goto out;
    }
    
    if (*f_pos + count > BUFFER_SIZE)
        count = BUFFER_SIZE - *f_pos;
    
    // 3. 从用户空间复制数据（★关键）
    if (copy_from_user(dev->buffer + *f_pos, buf, count)) {
        retval = -EFAULT;
        goto out;
    }
    
    // 4. 更新文件位置和大小
    *f_pos += count;
    if (*f_pos > dev->size)
        dev->size = *f_pos;
    
    retval = count;
    
out:
    mutex_unlock(&dev->mutex);
    return retval;  // 返回写入的字节数，或负错误码
}
```
[Intermediate]

---

Q: What is `copy_to_user()` and `copy_from_user()` and why are they necessary?
A: 这两个函数用于内核空间与用户空间之间安全地复制数据：
```c
unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);
```
**必须使用的原因**：
1. **地址验证**：验证用户空间地址是否有效
2. **权限检查**：确保进程有权访问该内存
3. **缺页处理**：处理用户页面被换出的情况
4. **架构差异**：某些架构内核不能直接访问用户空间

**返回值**：未复制的字节数（成功返回0）

**绝对禁止**：直接对用户空间指针解引用！
```c
// 错误！可能崩溃或安全漏洞
char c = *user_buf;  // 危险！
memcpy(kbuf, user_buf, len);  // 危险！
```
[Basic]

---

Q: How to implement `llseek()` for seekable devices?
A: 
```c
static loff_t mydev_llseek(struct file *filp, loff_t offset, int whence)
{
    struct mydev_data *dev = filp->private_data;
    loff_t newpos;
    
    switch (whence) {
    case SEEK_SET:  // 从文件开始
        newpos = offset;
        break;
        
    case SEEK_CUR:  // 从当前位置
        newpos = filp->f_pos + offset;
        break;
        
    case SEEK_END:  // 从文件末尾
        newpos = dev->size + offset;
        break;
        
    default:
        return -EINVAL;
    }
    
    // 检查新位置是否有效
    if (newpos < 0)
        return -EINVAL;
    if (newpos > dev->size)
        newpos = dev->size;
    
    // 更新文件位置
    filp->f_pos = newpos;
    return newpos;
}
```
对于不支持seek的设备（如串口），使用`no_llseek`或`noop_llseek`。
[Intermediate]

---

## 5. IOCTL Implementation (ioctl实现)

---

Q: What is ioctl and when should it be used?
A: `ioctl` (I/O Control) 用于设备特定的控制操作，不适合read/write的场景：
- 获取/设置设备参数
- 控制设备状态
- 执行设备特定命令

```c
// 用户空间调用
int ret = ioctl(fd, MYDEV_SET_SPEED, 115200);

// 内核处理
static long mydev_ioctl(struct file *filp, unsigned int cmd, 
                        unsigned long arg)
{
    switch (cmd) {
    case MYDEV_SET_SPEED:
        // 设置波特率
        break;
    case MYDEV_GET_STATUS:
        // 获取状态
        break;
    default:
        return -ENOTTY;  // 无效命令
    }
    return 0;
}
```
[Basic]

---

Q: How to define ioctl command numbers using macros?
A: 使用`<linux/ioctl.h>`中的宏定义命令号：
```c
#include <linux/ioctl.h>

// 定义幻数（magic number），用于区分驱动
#define MYDEV_IOC_MAGIC 'k'

// _IO:    无参数
// _IOR:   从驱动读取数据
// _IOW:   向驱动写入数据  
// _IOWR:  双向传输

#define MYDEV_RESET       _IO(MYDEV_IOC_MAGIC, 0)
#define MYDEV_GET_STATUS  _IOR(MYDEV_IOC_MAGIC, 1, int)
#define MYDEV_SET_SPEED   _IOW(MYDEV_IOC_MAGIC, 2, int)
#define MYDEV_XFER_DATA   _IOWR(MYDEV_IOC_MAGIC, 3, struct mydev_xfer)

#define MYDEV_IOC_MAXNR  3

// 命令号结构 (32位):
// | dir(2) | size(14) | type(8) | nr(8) |
// dir:  传输方向 (_IOC_NONE, _IOC_READ, _IOC_WRITE)
// size: 参数大小
// type: 幻数
// nr:   命令序号
```
[Intermediate]

---

Q: How to implement `unlocked_ioctl()` callback?
A: 
```c
static long mydev_ioctl(struct file *filp, unsigned int cmd, 
                        unsigned long arg)
{
    struct mydev_data *dev = filp->private_data;
    int err = 0, retval = 0;
    int tmp;
    
    // 1. 验证命令有效性
    if (_IOC_TYPE(cmd) != MYDEV_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > MYDEV_IOC_MAXNR)
        return -ENOTTY;
    
    // 2. 验证用户空间地址
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    if (_IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;
    
    // 3. 处理命令
    switch (cmd) {
    case MYDEV_RESET:
        dev->size = 0;
        memset(dev->buffer, 0, BUFFER_SIZE);
        break;
        
    case MYDEV_GET_STATUS:
        retval = put_user(dev->status, (int __user *)arg);
        break;
        
    case MYDEV_SET_SPEED:
        retval = get_user(tmp, (int __user *)arg);
        if (!retval)
            dev->speed = tmp;
        break;
        
    default:
        return -ENOTTY;
    }
    
    return retval;
}
```
[Intermediate]

---

Q: What is the difference between `unlocked_ioctl` and `compat_ioctl`?
A: 
| 回调 | 用途 |
|------|------|
| `unlocked_ioctl` | 原生ioctl，64位程序调用64位内核 |
| `compat_ioctl` | 兼容模式，32位程序调用64位内核 |

```c
static long mydev_ioctl(struct file *filp, unsigned int cmd, 
                        unsigned long arg)
{
    // 正常处理
}

#ifdef CONFIG_COMPAT
static long mydev_compat_ioctl(struct file *filp, unsigned int cmd,
                               unsigned long arg)
{
    // 处理32位兼容性
    // 可能需要转换数据结构大小/布局
    return mydev_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations my_fops = {
    .unlocked_ioctl = mydev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = mydev_compat_ioctl,
#endif
};
```
[Advanced]

---

## 6. Synchronization (同步与并发)

---

Q: Why is synchronization important in device drivers?
A: 驱动程序面临多种并发场景：
```
+------------------+     +------------------+
|   Process A      |     |   Process B      |
|   read()         |     |   write()        |
+--------+---------+     +--------+---------+
         |                        |
         +------------------------+
                    |
                    v
         +-------------------+
         |   Device Driver   |  <-- 必须保护共享数据！
         |   (shared data)   |
         +-------------------+
                    ^
                    |
         +----------+----------+
         |                     |
+--------+---------+  +--------+---------+
|   Interrupt      |  |   Kernel Thread  |
|   Handler        |  |   / Workqueue    |
+------------------+  +------------------+
```
竞态条件可能导致数据损坏、死锁、系统崩溃。
[Basic]

---

Q: What synchronization primitives are available in the kernel?
A: 
| 原语 | 用途 | 特点 |
|------|------|------|
| `spinlock_t` | 短临界区 | 忙等待，不可睡眠 |
| `mutex` | 长临界区 | 可睡眠，有所有者 |
| `semaphore` | 计数信号量 | 可睡眠，无所有者 |
| `rwlock_t` | 读写锁 | 多读单写，忙等待 |
| `rw_semaphore` | 读写信号量 | 多读单写，可睡眠 |
| `atomic_t` | 原子变量 | 无锁，单变量操作 |
| `completion` | 完成等待 | 事件通知 |

选择原则：
- 中断上下文 → spinlock
- 进程上下文 + 短临界区 → spinlock或mutex
- 进程上下文 + 可能睡眠 → mutex
[Intermediate]

---

Q: How to use mutex in a character driver?
A: 
```c
struct mydev_data {
    struct mutex mutex;   // 互斥锁
    char buffer[1024];
    size_t size;
};

static int mydev_init(void)
{
    // 初始化mutex
    mutex_init(&dev->mutex);
    return 0;
}

static ssize_t mydev_read(struct file *filp, char __user *buf,
                          size_t count, loff_t *f_pos)
{
    struct mydev_data *dev = filp->private_data;
    ssize_t retval;
    
    // 获取锁（可中断）
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;  // 被信号中断
    
    // 临界区：访问共享数据
    if (copy_to_user(buf, dev->buffer, count))
        retval = -EFAULT;
    else
        retval = count;
    
    // 释放锁
    mutex_unlock(&dev->mutex);
    
    return retval;
}
```
[Intermediate]

---

Q: When to use spinlock vs mutex?
A: 
```c
// Spinlock: 中断上下文或短临界区
spinlock_t my_lock;
unsigned long flags;

spin_lock_init(&my_lock);

// 进程上下文
spin_lock(&my_lock);
// 临界区（必须短！不能睡眠！）
spin_unlock(&my_lock);

// 中断上下文安全
spin_lock_irqsave(&my_lock, flags);
// 临界区
spin_unlock_irqrestore(&my_lock, flags);


// Mutex: 只能在进程上下文，可睡眠
struct mutex my_mutex;

mutex_init(&my_mutex);

mutex_lock(&my_mutex);        // 可能睡眠
// 临界区（可以调用可能睡眠的函数）
mutex_unlock(&my_mutex);

mutex_lock_interruptible(&my_mutex);  // 可被信号中断
```

**选择规则**：
- 能用mutex就用mutex（更高效）
- 中断上下文必须用spinlock
- 临界区可能睡眠必须用mutex
[Intermediate]

---

Q: How to use atomic operations?
A: 
```c
#include <linux/atomic.h>

struct mydev_data {
    atomic_t open_count;     // 原子计数器
    atomic_t bytes_written;
};

static int mydev_open(struct inode *inode, struct file *filp)
{
    struct mydev_data *dev = filp->private_data;
    
    // 原子增加，返回新值
    int count = atomic_inc_return(&dev->open_count);
    
    // 限制最大打开数
    if (count > MAX_OPENS) {
        atomic_dec(&dev->open_count);
        return -EBUSY;
    }
    
    return 0;
}

static ssize_t mydev_write(struct file *filp, const char __user *buf,
                           size_t count, loff_t *f_pos)
{
    struct mydev_data *dev = filp->private_data;
    
    // 原子加法
    atomic_add(count, &dev->bytes_written);
    
    return count;
}

// 常用原子操作
atomic_set(&v, 0);           // 设置值
atomic_read(&v);             // 读取值
atomic_inc(&v);              // 加1
atomic_dec(&v);              // 减1
atomic_add(n, &v);           // 加n
atomic_sub(n, &v);           // 减n
atomic_inc_and_test(&v);     // 加1并测试是否为0
atomic_dec_and_test(&v);     // 减1并测试是否为0
atomic_cmpxchg(&v, old, new); // 比较交换
```
[Intermediate]

---

## 7. Blocking I/O and Wait Queues (阻塞I/O与等待队列)

---

Q: What is a wait queue and when is it used?
A: 等待队列用于实现阻塞I/O，让进程等待某个条件：
```c
#include <linux/wait.h>

// 定义等待队列头
wait_queue_head_t my_queue;

// 初始化
init_waitqueue_head(&my_queue);
// 或静态初始化
DECLARE_WAIT_QUEUE_HEAD(my_queue);

// 使用场景：
// 1. read()等待数据到达
// 2. write()等待缓冲区有空间
// 3. 等待硬件完成操作
// 4. 等待资源可用
```
[Basic]

---

Q: How to implement blocking read with wait queue?
A: 
```c
struct mydev_data {
    wait_queue_head_t read_queue;
    char buffer[1024];
    size_t data_len;
    spinlock_t lock;
};

static ssize_t mydev_read(struct file *filp, char __user *buf,
                          size_t count, loff_t *f_pos)
{
    struct mydev_data *dev = filp->private_data;
    
    // 1. 检查是否有数据
    spin_lock(&dev->lock);
    while (dev->data_len == 0) {
        spin_unlock(&dev->lock);
        
        // 非阻塞模式直接返回
        if (filp->f_flags & O_NONBLOCK)
            return -EAGAIN;
        
        // 2. 等待数据到达
        if (wait_event_interruptible(dev->read_queue, 
                                     dev->data_len > 0))
            return -ERESTARTSYS;  // 被信号中断
        
        spin_lock(&dev->lock);
    }
    
    // 3. 有数据了，复制给用户
    if (count > dev->data_len)
        count = dev->data_len;
    
    spin_unlock(&dev->lock);
    
    if (copy_to_user(buf, dev->buffer, count))
        return -EFAULT;
    
    // 4. 更新缓冲区
    spin_lock(&dev->lock);
    memmove(dev->buffer, dev->buffer + count, dev->data_len - count);
    dev->data_len -= count;
    spin_unlock(&dev->lock);
    
    return count;
}

// 中断处理程序或其他上下文唤醒等待者
static irqreturn_t mydev_interrupt(int irq, void *dev_id)
{
    struct mydev_data *dev = dev_id;
    
    // 数据到达，唤醒等待的进程
    wake_up_interruptible(&dev->read_queue);
    
    return IRQ_HANDLED;
}
```
[Advanced]

---

Q: What are the different wait_event variants?
A: 
```c
// 1. 不可中断等待（避免使用，可能导致进程无法杀死）
wait_event(wq, condition);

// 2. 可被信号中断（推荐）
int ret = wait_event_interruptible(wq, condition);
if (ret)
    return -ERESTARTSYS;

// 3. 带超时的等待
long timeout = wait_event_timeout(wq, condition, HZ * 5);
if (timeout == 0)
    return -ETIMEDOUT;

// 4. 可中断 + 超时
long timeout = wait_event_interruptible_timeout(wq, condition, HZ * 5);
if (timeout == 0)
    return -ETIMEDOUT;
if (timeout < 0)
    return -ERESTARTSYS;

// 5. 独占等待（只唤醒一个进程）
wait_event_interruptible_exclusive(wq, condition);

// 唤醒函数
wake_up(&wq);                    // 唤醒所有非独占进程
wake_up_interruptible(&wq);      // 只唤醒可中断的进程
wake_up_all(&wq);                // 唤醒所有进程
wake_up_interruptible_all(&wq);  // 唤醒所有可中断进程
```
[Intermediate]

---

## 8. Poll and Select (轮询机制)

---

Q: How to implement `poll()` callback for select/poll/epoll support?
A: 
```c
#include <linux/poll.h>

static unsigned int mydev_poll(struct file *filp, poll_table *wait)
{
    struct mydev_data *dev = filp->private_data;
    unsigned int mask = 0;
    
    // 1. 将等待队列注册到poll_table
    poll_wait(filp, &dev->read_queue, wait);
    poll_wait(filp, &dev->write_queue, wait);
    
    // 2. 检查当前状态，设置返回掩码
    spin_lock(&dev->lock);
    
    // 可读：有数据
    if (dev->data_len > 0)
        mask |= POLLIN | POLLRDNORM;
    
    // 可写：缓冲区有空间
    if (dev->data_len < BUFFER_SIZE)
        mask |= POLLOUT | POLLWRNORM;
    
    // 错误条件
    if (dev->error)
        mask |= POLLERR;
    
    // EOF/挂起
    if (dev->disconnected)
        mask |= POLLHUP;
    
    spin_unlock(&dev->lock);
    
    return mask;
}

// 用户空间使用
// struct pollfd fds[1];
// fds[0].fd = fd;
// fds[0].events = POLLIN;
// poll(fds, 1, timeout_ms);
```
[Intermediate]

---

Q: What poll flags should be returned and when?
A: 
| 标志 | 含义 | 何时设置 |
|------|------|----------|
| `POLLIN` | 可读（有数据） | 缓冲区有数据 |
| `POLLRDNORM` | 正常数据可读 | 通常与POLLIN一起 |
| `POLLRDBAND` | 优先数据可读 | 带外数据 |
| `POLLPRI` | 紧急数据 | 高优先级数据 |
| `POLLOUT` | 可写 | 缓冲区有空间 |
| `POLLWRNORM` | 正常写入 | 通常与POLLOUT一起 |
| `POLLWRBAND` | 优先写入 | 带外数据 |
| `POLLERR` | 错误 | 设备错误 |
| `POLLHUP` | 挂起 | 连接断开/EOF |
| `POLLNVAL` | 无效描述符 | 由内核设置 |

典型组合：
```c
// 可读
mask |= POLLIN | POLLRDNORM;
// 可写  
mask |= POLLOUT | POLLWRNORM;
// 错误
mask |= POLLERR;
// EOF
mask |= POLLHUP;
```
[Intermediate]

---

## 9. Memory Mapping (内存映射)

---

Q: What is memory mapping (mmap) in device drivers?
A: mmap允许用户空间直接访问设备内存或内核缓冲区：
```
用户空间                     内核空间
+-----------+               +-----------+
|  虚拟地址  |   mmap()      |  设备内存  |
|  0x7f...  | <-----------> |  或DMA缓冲|
+-----------+               +-----------+
    |                           |
    |    页表映射               |
    +---------------------------+
```
**优势**：
- 避免copy_to_user/copy_from_user的拷贝开销
- 用户程序可直接读写设备内存
- 高效的大数据量传输

**使用场景**：帧缓冲、视频采集、DMA缓冲区
[Basic]

---

Q: How to implement basic `mmap()` callback?
A: 
```c
static int mydev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct mydev_data *dev = filp->private_data;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    
    // 1. 检查映射大小
    if (size > BUFFER_SIZE)
        return -EINVAL;
    
    // 2. 设置页面属性（禁止缓存用于设备内存）
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    
    // 3. 建立映射
    // 方法A：映射物理内存（如设备寄存器）
    pfn = virt_to_phys(dev->buffer) >> PAGE_SHIFT;
    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
        return -EAGAIN;
    
    // 方法B：映射kmalloc分配的内存
    // if (remap_vmalloc_range(vma, dev->buffer, 0))
    //     return -EAGAIN;
    
    // 4. 设置VMA操作（可选，用于处理缺页等）
    vma->vm_ops = &mydev_vm_ops;
    vma->vm_private_data = dev;
    
    return 0;
}

static const struct vm_operations_struct mydev_vm_ops = {
    .open   = mydev_vma_open,
    .close  = mydev_vma_close,
    .fault  = mydev_vma_fault,  // 缺页处理
};
```
[Advanced]

---

Q: What is `struct vm_area_struct` and its key fields?
A: 
```c
struct vm_area_struct {
    unsigned long vm_start;       // 虚拟地址起始
    unsigned long vm_end;         // 虚拟地址结束
    
    unsigned long vm_flags;       // 访问权限标志
    // VM_READ, VM_WRITE, VM_EXEC, VM_SHARED, VM_IO等
    
    pgprot_t vm_page_prot;        // 页面保护属性
    
    const struct vm_operations_struct *vm_ops;  // VMA操作
    
    void *vm_private_data;        // 驱动私有数据
    
    struct file *vm_file;         // 关联的文件
    unsigned long vm_pgoff;       // 文件偏移（页为单位）
};
```
驱动在mmap中检查这些字段并建立页表映射。
[Advanced]

---

## 10. Interrupt Handling (中断处理)

---

Q: How to register an interrupt handler in a device driver?
A: 
```c
#include <linux/interrupt.h>

// 中断处理函数
static irqreturn_t mydev_interrupt(int irq, void *dev_id)
{
    struct mydev_data *dev = dev_id;
    
    // 1. 检查是否是本设备的中断
    u32 status = ioread32(dev->regs + STATUS_REG);
    if (!(status & IRQ_PENDING))
        return IRQ_NONE;  // 不是我们的中断
    
    // 2. 清除中断标志
    iowrite32(IRQ_CLEAR, dev->regs + STATUS_REG);
    
    // 3. 处理中断（尽量简短）
    dev->irq_count++;
    
    // 4. 唤醒等待的进程
    wake_up_interruptible(&dev->wait_queue);
    
    return IRQ_HANDLED;
}

static int mydev_probe(struct platform_device *pdev)
{
    int irq, ret;
    
    // 获取IRQ号
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;
    
    // 注册中断处理程序
    ret = request_irq(irq,           // IRQ号
                      mydev_interrupt, // 处理函数
                      IRQF_SHARED,    // 标志（共享中断）
                      "mydevice",     // /proc/interrupts中的名称
                      dev);           // 传递给处理函数的参数
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ %d\n", irq);
        return ret;
    }
    
    dev->irq = irq;
    return 0;
}

static int mydev_remove(struct platform_device *pdev)
{
    // 注销中断
    free_irq(dev->irq, dev);
    return 0;
}
```
[Intermediate]

---

Q: What is the difference between top half and bottom half?
A: 
```
中断触发
    |
    v
+------------------+
|    Top Half      |  <- 中断上下文，快速执行
|  (硬中断处理)     |     - 不可睡眠
|                  |     - 关中断或禁止抢占
|  - 确认中断      |     - 尽可能短
|  - 读取状态      |
|  - 调度Bottom Half|
+------------------+
    |
    v
+------------------+
|   Bottom Half    |  <- 延迟执行，可做更多工作
|  (软中断/tasklet/ |     - 可以睡眠（工作队列）
|   workqueue)     |     - 可被中断
|                  |
|  - 数据处理      |
|  - 协议栈处理    |
|  - 唤醒进程      |
+------------------+
```

| 机制 | 上下文 | 可睡眠 | 用途 |
|------|--------|--------|------|
| Softirq | 软中断 | 否 | 高性能，如网络 |
| Tasklet | 软中断 | 否 | 简单延迟处理 |
| Workqueue | 进程 | 是 | 需要睡眠的处理 |
| Threaded IRQ | 进程 | 是 | 完全线程化 |
[Intermediate]

---

Q: How to use workqueue for bottom half processing?
A: 
```c
#include <linux/workqueue.h>

struct mydev_data {
    struct work_struct work;      // 工作项
    int pending_data;
};

// 工作函数（进程上下文，可睡眠）
static void mydev_work_handler(struct work_struct *work)
{
    struct mydev_data *dev = container_of(work, struct mydev_data, work);
    
    // 可以调用可能睡眠的函数
    mutex_lock(&dev->mutex);
    
    // 处理数据
    process_data(dev);
    
    mutex_unlock(&dev->mutex);
}

static int mydev_init(void)
{
    // 初始化工作项
    INIT_WORK(&dev->work, mydev_work_handler);
    return 0;
}

// 中断处理程序中调度工作
static irqreturn_t mydev_interrupt(int irq, void *dev_id)
{
    struct mydev_data *dev = dev_id;
    
    // 快速处理
    dev->pending_data = ioread32(dev->regs);
    
    // 调度工作队列处理（延迟到进程上下文）
    schedule_work(&dev->work);
    
    return IRQ_HANDLED;
}
```
[Intermediate]

---

## 11. Platform Devices (平台设备)

---

Q: What is a platform device and when is it used?
A: 平台设备用于不可发现的设备（如嵌入式SoC上的外设）：
```c
// 设备在设备树或板级代码中描述
// 不像PCI/USB可以自动枚举

// 特点：
// - 使用MMIO（内存映射I/O）访问寄存器
// - IRQ号固定或从设备树获取
// - 无标准总线协议

// 常见平台设备：
// - SoC内部外设（UART、SPI、I2C控制器）
// - GPIO控制器
// - 定时器
// - DMA控制器
```
[Basic]

---

Q: How to implement a platform driver?
A: 
```c
#include <linux/platform_device.h>
#include <linux/of.h>  // 设备树支持

static int mydev_probe(struct platform_device *pdev)
{
    struct mydev_data *dev;
    struct resource *res;
    int irq, ret;
    
    // 1. 分配设备数据
    dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    
    // 2. 获取并映射寄存器资源
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    dev->regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(dev->regs))
        return PTR_ERR(dev->regs);
    
    // 3. 获取并注册中断
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;
    
    ret = devm_request_irq(&pdev->dev, irq, mydev_interrupt,
                           0, "mydevice", dev);
    if (ret)
        return ret;
    
    // 4. 初始化设备
    // ...
    
    // 5. 保存到平台设备
    platform_set_drvdata(pdev, dev);
    
    return 0;
}

static int mydev_remove(struct platform_device *pdev)
{
    struct mydev_data *dev = platform_get_drvdata(pdev);
    
    // 清理资源（devm_*分配的会自动释放）
    
    return 0;
}

// 设备树匹配表
static const struct of_device_id mydev_of_match[] = {
    { .compatible = "vendor,mydevice" },
    { }
};
MODULE_DEVICE_TABLE(of, mydev_of_match);

static struct platform_driver mydev_driver = {
    .probe  = mydev_probe,
    .remove = mydev_remove,
    .driver = {
        .name = "mydevice",
        .of_match_table = mydev_of_match,
    },
};

module_platform_driver(mydev_driver);
```
[Intermediate]

---

Q: What is `devm_*` (managed) API and why should it be used?
A: `devm_*`（设备管理）API自动在设备移除时释放资源：
```c
// 传统API（需要手动释放）           // devm API（自动释放）
kzalloc() / kfree()                  devm_kzalloc()
ioremap() / iounmap()                devm_ioremap()
request_irq() / free_irq()           devm_request_irq()
request_region() / release_region()  devm_request_region()
clk_get() / clk_put()                devm_clk_get()
```

**优点**：
1. 简化错误处理路径（无需级联goto）
2. 防止资源泄漏
3. 移除时自动逆序释放

```c
// 传统方式（复杂的错误处理）
static int mydev_probe(...)
{
    ptr = kzalloc(...);
    if (!ptr)
        return -ENOMEM;
    
    res = ioremap(...);
    if (!res) {
        ret = -ENOMEM;
        goto err_free_ptr;
    }
    
    ret = request_irq(...);
    if (ret)
        goto err_unmap;
    
    return 0;
    
err_unmap:
    iounmap(res);
err_free_ptr:
    kfree(ptr);
    return ret;
}

// devm方式（简洁）
static int mydev_probe(...)
{
    ptr = devm_kzalloc(...);
    if (!ptr)
        return -ENOMEM;
    
    res = devm_ioremap(...);
    if (!res)
        return -ENOMEM;  // ptr会自动释放
    
    ret = devm_request_irq(...);
    if (ret)
        return ret;  // res和ptr会自动释放
    
    return 0;
}
```
[Intermediate]

---

## 12. Error Handling (错误处理)

---

Q: What are common error codes returned by driver functions?
A: 
| 错误码 | 值 | 含义 |
|--------|-----|------|
| `-EPERM` | -1 | 操作不允许 |
| `-ENOENT` | -2 | 文件不存在 |
| `-ESRCH` | -3 | 进程不存在 |
| `-EINTR` | -4 | 系统调用被中断 |
| `-EIO` | -5 | I/O错误 |
| `-ENOMEM` | -12 | 内存不足 |
| `-EACCES` | -13 | 权限不足 |
| `-EFAULT` | -14 | 地址错误 |
| `-EBUSY` | -16 | 资源忙 |
| `-ENODEV` | -19 | 设备不存在 |
| `-EINVAL` | -22 | 参数无效 |
| `-ENOSPC` | -28 | 空间不足 |
| `-EAGAIN` | -11 | 资源暂时不可用（非阻塞） |
| `-ERESTARTSYS` | -512 | 需要重启系统调用 |
| `-ENOTTY` | -25 | 不支持的ioctl |
| `-ETIMEDOUT` | -110 | 超时 |
[Basic]

---

Q: How to use ERR_PTR, PTR_ERR, and IS_ERR?
A: 用于将错误码编码在指针中返回：
```c
#include <linux/err.h>

// 将错误码转换为指针
void *ERR_PTR(long error);

// 将指针转换回错误码
long PTR_ERR(const void *ptr);

// 检查指针是否是错误码
bool IS_ERR(const void *ptr);
bool IS_ERR_OR_NULL(const void *ptr);

// 使用示例
static void __iomem *my_ioremap(resource_size_t phys_addr, size_t size)
{
    void __iomem *addr;
    
    addr = ioremap(phys_addr, size);
    if (!addr)
        return ERR_PTR(-ENOMEM);
    
    return addr;
}

static int my_probe(...)
{
    void __iomem *regs;
    
    regs = my_ioremap(phys, size);
    if (IS_ERR(regs))
        return PTR_ERR(regs);  // 返回-ENOMEM
    
    // 使用regs
}
```
**原理**：Linux内核地址空间顶部是无效的，用于编码错误。
[Intermediate]

---

## 13. Debugging (调试技术)

---

Q: What are the kernel debug printing functions?
A: 
```c
#include <linux/kernel.h>
#include <linux/printk.h>

// 基本printk
printk(KERN_INFO "Hello, kernel!\n");
printk(KERN_ERR "Error: %d\n", err);

// 便捷宏
pr_emerg("Emergency!\n");     // 系统不可用
pr_alert("Alert!\n");         // 必须立即处理
pr_crit("Critical!\n");       // 严重错误
pr_err("Error!\n");           // 错误
pr_warn("Warning!\n");        // 警告
pr_notice("Notice!\n");       // 正常但重要
pr_info("Info!\n");           // 信息
pr_debug("Debug!\n");         // 调试（需要定义DEBUG）

// 设备相关打印（自动添加设备信息）
dev_err(&pdev->dev, "Error: %d\n", err);
dev_warn(&pdev->dev, "Warning!\n");
dev_info(&pdev->dev, "Info!\n");
dev_dbg(&pdev->dev, "Debug!\n");

// 限速打印（避免日志洪水）
if (printk_ratelimit())
    pr_warn("Something happened\n");
// 或
pr_warn_ratelimited("Something happened\n");

// 打印一次
pr_info_once("Module loaded\n");
```
[Basic]

---

Q: How to use dynamic debug (dyndbg)?
A: 动态调试允许在运行时启用/禁用pr_debug和dev_dbg：
```bash
# 查看当前状态
cat /sys/kernel/debug/dynamic_debug/control

# 启用特定文件的调试
echo 'file mydriver.c +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定函数的调试
echo 'func my_probe +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定模块
echo 'module mydriver +p' > /sys/kernel/debug/dynamic_debug/control

# 启用特定行号
echo 'file mydriver.c line 100 +p' > /sys/kernel/debug/dynamic_debug/control

# 禁用
echo 'file mydriver.c -p' > /sys/kernel/debug/dynamic_debug/control

# 标志：
# p: 打印消息
# f: 包含函数名
# l: 包含行号
# m: 包含模块名
# t: 包含线程ID
```
[Intermediate]

---

Q: How to use `/proc` and `/sys` for driver debugging?
A: 
```c
// 1. 创建/proc文件
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int mydev_proc_show(struct seq_file *m, void *v)
{
    struct mydev_data *dev = m->private;
    
    seq_printf(m, "open_count: %d\n", dev->open_count);
    seq_printf(m, "bytes_read: %lu\n", dev->bytes_read);
    
    return 0;
}

static int mydev_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, mydev_proc_show, PDE_DATA(inode));
}

static const struct file_operations mydev_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = mydev_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

// 创建proc文件
proc_create_data("mydevice", 0444, NULL, &mydev_proc_fops, dev);


// 2. 使用sysfs属性
static ssize_t status_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    struct mydev_data *mydev = dev_get_drvdata(dev);
    return sprintf(buf, "%d\n", mydev->status);
}

static ssize_t status_store(struct device *dev,
                            struct device_attribute *attr,
                            const char *buf, size_t count)
{
    struct mydev_data *mydev = dev_get_drvdata(dev);
    sscanf(buf, "%d", &mydev->status);
    return count;
}

static DEVICE_ATTR_RW(status);  // 创建/sys/devices/.../status
```
[Intermediate]

---

## 14. Complete Driver Template (完整驱动模板)

---

Q: Provide a complete minimal character driver template.
A: 
```c
/*
 * Minimal Character Device Driver Template
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#define DRIVER_NAME "mychardev"
#define BUFFER_SIZE 4096

struct mychardev_data {
    struct cdev cdev;
    struct mutex mutex;
    char buffer[BUFFER_SIZE];
    size_t size;
};

static dev_t dev_num;
static struct class *dev_class;
static struct mychardev_data *mydev;

/* File Operations */
static int mychardev_open(struct inode *inode, struct file *filp)
{
    filp->private_data = container_of(inode->i_cdev, 
                                      struct mychardev_data, cdev);
    return 0;
}

static int mychardev_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t mychardev_read(struct file *filp, char __user *buf,
                              size_t count, loff_t *f_pos)
{
    struct mychardev_data *dev = filp->private_data;
    ssize_t retval = 0;
    
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    
    if (*f_pos >= dev->size)
        goto out;
    
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;
    
    if (copy_to_user(buf, dev->buffer + *f_pos, count)) {
        retval = -EFAULT;
        goto out;
    }
    
    *f_pos += count;
    retval = count;
    
out:
    mutex_unlock(&dev->mutex);
    return retval;
}

static ssize_t mychardev_write(struct file *filp, const char __user *buf,
                               size_t count, loff_t *f_pos)
{
    struct mychardev_data *dev = filp->private_data;
    ssize_t retval = 0;
    
    if (mutex_lock_interruptible(&dev->mutex))
        return -ERESTARTSYS;
    
    if (*f_pos >= BUFFER_SIZE) {
        retval = -ENOSPC;
        goto out;
    }
    
    if (*f_pos + count > BUFFER_SIZE)
        count = BUFFER_SIZE - *f_pos;
    
    if (copy_from_user(dev->buffer + *f_pos, buf, count)) {
        retval = -EFAULT;
        goto out;
    }
    
    *f_pos += count;
    if (*f_pos > dev->size)
        dev->size = *f_pos;
    retval = count;
    
out:
    mutex_unlock(&dev->mutex);
    return retval;
}

static const struct file_operations mychardev_fops = {
    .owner   = THIS_MODULE,
    .open    = mychardev_open,
    .release = mychardev_release,
    .read    = mychardev_read,
    .write   = mychardev_write,
};

/* Module Init/Exit */
static int __init mychardev_init(void)
{
    int ret;
    
    /* Allocate device number */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DRIVER_NAME);
    if (ret < 0)
        return ret;
    
    /* Allocate device data */
    mydev = kzalloc(sizeof(*mydev), GFP_KERNEL);
    if (!mydev) {
        ret = -ENOMEM;
        goto fail_alloc;
    }
    
    /* Initialize cdev */
    mutex_init(&mydev->mutex);
    cdev_init(&mydev->cdev, &mychardev_fops);
    mydev->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&mydev->cdev, dev_num, 1);
    if (ret < 0)
        goto fail_cdev;
    
    /* Create device class and node */
    dev_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(dev_class)) {
        ret = PTR_ERR(dev_class);
        goto fail_class;
    }
    
    if (IS_ERR(device_create(dev_class, NULL, dev_num, NULL, DRIVER_NAME))) {
        ret = -ENOMEM;
        goto fail_device;
    }
    
    pr_info("%s: registered with major %d\n", DRIVER_NAME, MAJOR(dev_num));
    return 0;

fail_device:
    class_destroy(dev_class);
fail_class:
    cdev_del(&mydev->cdev);
fail_cdev:
    kfree(mydev);
fail_alloc:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit mychardev_exit(void)
{
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    cdev_del(&mydev->cdev);
    kfree(mydev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("%s: unregistered\n", DRIVER_NAME);
}

module_init(mychardev_init);
module_exit(mychardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Minimal Character Device Driver");
```
[Basic]

---

## 15. Common Mistakes (常见错误)

---

Q: What are common mistakes in character driver development?
A: 
| 错误 | 后果 | 正确做法 |
|------|------|----------|
| 直接访问用户空间指针 | 内核崩溃、安全漏洞 | 使用copy_to/from_user |
| 在中断中调用可睡眠函数 | 死锁、崩溃 | 使用workqueue延迟处理 |
| 忘记检查返回值 | 资源泄漏、错误传播 | 始终检查并处理错误 |
| 模块卸载不完整 | 资源泄漏、oops | 完整清理所有资源 |
| 竞态条件 | 数据损坏、死锁 | 正确使用锁 |
| 忘记设置file_operations.owner | 模块可能被提前卸载 | 设置.owner = THIS_MODULE |
| ioctl中未验证参数 | 安全漏洞 | 验证所有用户输入 |
| 中断处理程序太长 | 系统响应差 | 使用bottom half |
[Intermediate]

---

Q: How to properly handle the module removal race condition?
A: 
```c
// 问题：模块卸载时可能有进程正在使用设备

// 解决方案1：使用引用计数
static int mydev_open(struct inode *inode, struct file *filp)
{
    // 增加模块引用计数（由VFS自动处理，如果设置了owner）
    // 或手动：
    if (!try_module_get(THIS_MODULE))
        return -ENODEV;
    
    return 0;
}

static int mydev_release(struct inode *inode, struct file *filp)
{
    // 减少模块引用计数
    module_put(THIS_MODULE);
    return 0;
}

// 解决方案2：使用cdev的kobj引用计数
// cdev_add()后，只要设备被打开就不能卸载

// 解决方案3：在exit中等待所有操作完成
static void __exit mydev_exit(void)
{
    // 停止接受新的open
    device_destroy(dev_class, dev_num);
    class_destroy(dev_class);
    
    // 删除cdev会等待所有file引用释放
    cdev_del(&mydev->cdev);
    
    // 现在可以安全释放资源
    kfree(mydev);
    unregister_chrdev_region(dev_num, 1);
}
```
[Advanced]

---

*Total: 80+ cards covering Linux kernel character device driver implementation*

