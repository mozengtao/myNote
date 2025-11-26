# Linux 内核如何根据 `/dev/xxx` 找到对应的设备驱动

基于 Linux 3.2 内核源码分析

---

## 核心答案

**设备号 (dev_t)** 是决定 `/dev/xxx` 对应哪个驱动的关键因素。

```
/dev/null  →  设备号 1:3  →  查找 cdev_map  →  找到 null_fops
              (主:次)
```

**决定因素**:
1. **设备号** (`dev_t` = 主设备号 + 次设备号) 存储在设备文件的 inode 中
2. 内核通过设备号在 `cdev_map` 中查找对应的 `struct cdev`
3. `cdev` 包含 `file_operations` 指针，即驱动的操作函数

---

## 目录

- [完整流程图](#完整流程图)
- [关键代码分析](#关键代码分析)
- [设备号的结构](#设备号的结构)
- [驱动注册过程](#驱动注册过程)
- [查找过程详解](#查找过程详解)
- [总结](#总结)

---

## 完整流程图

### 打开设备文件的完整流程

```
┌────────────────────────────────────────────────────────────────────────────┐
│                        用户程序: open("/dev/null")                          │
└───────────────────────────────────┬────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│  1. VFS 层: 路径解析                                                        │
│     path_lookup("/dev/null") → 找到 inode                                  │
│                                                                             │
│     inode 包含:                                                             │
│       - i_mode = S_IFCHR (字符设备标志)                                      │
│       - i_rdev = MKDEV(1, 3)  ← 设备号 (主设备号=1, 次设备号=3)              │
│       - i_fop = &def_chr_fops  ← 默认字符设备 fops                          │
└───────────────────────────────────┬────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│  2. VFS: __dentry_open()                                                    │
│     f->f_op = fops_get(inode->i_fop);  // 获取 def_chr_fops                 │
│     f->f_op->open(inode, f);           // 调用 chrdev_open()                │
└───────────────────────────────────┬────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│  3. chrdev_open() [fs/char_dev.c:369]                                       │
│                                                                             │
│     // 用设备号查找 cdev                                                    │
│     kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);                     │
│                        ~~~~~~~~   ~~~~~~~~~~~~                              │
│                        设备号映射表   设备号(1:3)                            │
│                                                                             │
│     // 从 kobject 获取 cdev                                                 │
│     cdev = container_of(kobj, struct cdev, kobj);                          │
│                                                                             │
│     // 用驱动的 fops 替换                                                   │
│     filp->f_op = fops_get(cdev->ops);   // 获取 null_fops                  │
│                                                                             │
│     // 调用驱动的 open                                                      │
│     filp->f_op->open(inode, filp);      // 调用驱动自己的 open()            │
└───────────────────────────────────┬────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────────┐
│  4. 后续操作直接使用驱动的 file_operations                                  │
│                                                                             │
│     read(fd, buf, len)  →  null_fops.read  →  read_null()                  │
│     write(fd, buf, len) →  null_fops.write →  write_null()                 │
└────────────────────────────────────────────────────────────────────────────┘
```

### 关键数据结构关系

```
                           用户空间
                              │
                         open("/dev/xxx")
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              VFS 层                                          │
│                                                                              │
│   ┌──────────────────┐                                                       │
│   │     dentry       │                                                       │
│   │  (目录项缓存)     │                                                       │
│   │   d_name="xxx"   │                                                       │
│   └────────┬─────────┘                                                       │
│            │ d_inode                                                         │
│            ▼                                                                 │
│   ┌──────────────────┐        ┌─────────────────────────────────────────┐   │
│   │      inode       │        │           def_chr_fops                  │   │
│   │                  │        │                                         │   │
│   │  i_mode=S_IFCHR  │  ───►  │  .open = chrdev_open ◄── 关键入口!      │   │
│   │  i_rdev=1:3      │  ───►  │  .llseek = noop_llseek                  │   │
│   │  i_fop ──────────┼───►    │                                         │   │
│   │  i_cdev ─────────┼──┐     └─────────────────────────────────────────┘   │
│   └──────────────────┘  │                                                    │
│                         │                                                    │
└─────────────────────────┼────────────────────────────────────────────────────┘
                          │
                          │ (首次打开时通过 kobj_lookup 查找并缓存)
                          │
┌─────────────────────────┼────────────────────────────────────────────────────┐
│                         ▼          字符设备子系统                             │
│                                                                              │
│   ┌──────────────────────────────────────────────────────────────────────┐   │
│   │                        cdev_map (设备号映射表)                        │   │
│   │                                                                      │   │
│   │    probes[0] ──► probe ──► probe ──► ...                            │   │
│   │    probes[1] ──► probe ──► probe ──► ...                            │   │
│   │        ...            │                                              │   │
│   │    probes[254] ──► ...│                                              │   │
│   │                       │                                              │   │
│   │                       │ (按 MAJOR(dev) % 255 索引)                   │   │
│   └───────────────────────┼──────────────────────────────────────────────┘   │
│                           │                                                  │
│                           ▼                                                  │
│   ┌──────────────────────────────────────────────────────────────────────┐   │
│   │                         struct probe                                 │   │
│   │                                                                      │   │
│   │     dev = MKDEV(1, 0)        // 设备号起始                           │   │
│   │     range = 256              // 设备号范围                           │   │
│   │     data ────────────────────┐  // 指向 cdev                        │   │
│   │     get = exact_match        │  // 获取 kobject 的函数              │   │
│   │                              │                                       │   │
│   └──────────────────────────────┼───────────────────────────────────────┘   │
│                                  │                                           │
│                                  ▼                                           │
│   ┌──────────────────────────────────────────────────────────────────────┐   │
│   │                         struct cdev                                  │   │
│   │                                                                      │   │
│   │     kobj                     // 内核对象 (用于引用计数)              │   │
│   │     owner = THIS_MODULE      // 所属模块                             │   │
│   │     ops ─────────────────────┐  // file_operations ◄── 驱动接口!    │   │
│   │     dev = MKDEV(1, 0)        │  // 设备号                           │   │
│   │     count = 256              │  // 次设备号数量                      │   │
│   │                              │                                       │   │
│   └──────────────────────────────┼───────────────────────────────────────┘   │
│                                  │                                           │
└──────────────────────────────────┼───────────────────────────────────────────┘
                                   │
                                   ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                        具体驱动的 file_operations                             │
│                                                                              │
│   static const struct file_operations null_fops = {                          │
│       .llseek      = null_lseek,                                             │
│       .read        = read_null,      ◄── 实际的驱动函数                      │
│       .write       = write_null,     ◄── 实际的驱动函数                      │
│       .splice_write = splice_write_null,                                     │
│   };                                                                         │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

## 关键代码分析

### 1. 设备文件 inode 的初始化

当创建设备文件时 (mknod 或 device_create)，`init_special_inode()` 被调用：

```c
// fs/inode.c:1623
void init_special_inode(struct inode *inode, umode_t mode, dev_t rdev)
{
    inode->i_mode = mode;
    if (S_ISCHR(mode)) {
        inode->i_fop = &def_chr_fops;    // ★ 设置默认字符设备 fops
        inode->i_rdev = rdev;            // ★ 存储设备号!
    } else if (S_ISBLK(mode)) {
        inode->i_fop = &def_blk_fops;    // 块设备
        inode->i_rdev = rdev;
    } else if (S_ISFIFO(mode))
        inode->i_fop = &def_fifo_fops;
    else if (S_ISSOCK(mode))
        inode->i_fop = &bad_sock_fops;
}
```

**关键点**:
- `i_rdev`: 存储设备号 (主设备号 + 次设备号)
- `i_fop`: 设置为 `def_chr_fops`，其 `.open` 是 `chrdev_open`

### 2. def_chr_fops - 字符设备默认操作

```c
// fs/char_dev.c:445
const struct file_operations def_chr_fops = {
    .open = chrdev_open,    // ★ 这是入口函数!
    .llseek = noop_llseek,
};
```

所有字符设备文件首次打开时，都会先调用 `chrdev_open()`。

### 3. chrdev_open - 查找真正的驱动

这是最核心的函数，负责根据设备号找到对应的驱动：

```c
// fs/char_dev.c:369
static int chrdev_open(struct inode *inode, struct file *filp)
{
    struct cdev *p;
    struct cdev *new = NULL;
    int ret = 0;

    spin_lock(&cdev_lock);
    p = inode->i_cdev;    // 检查 inode 中是否已缓存 cdev
    
    if (!p) {
        // ★ 首次打开：需要查找 cdev ★
        struct kobject *kobj;
        int idx;
        spin_unlock(&cdev_lock);
        
        // ★★★ 核心: 用设备号在 cdev_map 中查找 ★★★
        kobj = kobj_lookup(cdev_map, inode->i_rdev, &idx);
        //                 ~~~~~~~~  ~~~~~~~~~~~~~
        //                 全局映射表  设备号 (如 1:3)
        
        if (!kobj)
            return -ENXIO;    // 找不到驱动，返回错误
        
        // 从 kobject 获取 cdev 结构
        new = container_of(kobj, struct cdev, kobj);
        
        spin_lock(&cdev_lock);
        p = inode->i_cdev;
        if (!p) {
            inode->i_cdev = p = new;    // ★ 缓存到 inode，下次直接使用
            list_add(&inode->i_devices, &p->list);
            new = NULL;
        } else if (!cdev_get(p))
            ret = -ENXIO;
    } else if (!cdev_get(p))
        ret = -ENXIO;
    spin_unlock(&cdev_lock);
    cdev_put(new);
    
    if (ret)
        return ret;

    ret = -ENXIO;
    
    // ★★★ 用驱动的 fops 替换默认的 def_chr_fops ★★★
    filp->f_op = fops_get(p->ops);    // p->ops 就是驱动的 file_operations
    
    if (!filp->f_op)
        goto out_cdev_put;

    // 调用驱动自己的 open 函数
    if (filp->f_op->open) {
        ret = filp->f_op->open(inode, filp);
        if (ret)
            goto out_cdev_put;
    }

    return 0;

out_cdev_put:
    cdev_put(p);
    return ret;
}
```

### 4. cdev_map - 设备号映射表

```c
// drivers/base/map.c:19
struct kobj_map {
    struct probe {
        struct probe *next;        // 链表指针
        dev_t dev;                 // 设备号起始
        unsigned long range;       // 设备号范围
        struct module *owner;      // 所属模块
        kobj_probe_t *get;         // 获取 kobject 的函数
        int (*lock)(dev_t, void *);
        void *data;                // ★ 指向 struct cdev
    } *probes[255];                // ★ 按主设备号 % 255 索引
    struct mutex *lock;
};
```

### 5. kobj_lookup - 根据设备号查找 cdev

```c
// drivers/base/map.c:96
struct kobject *kobj_lookup(struct kobj_map *domain, dev_t dev, int *index)
{
    struct kobject *kobj;
    struct probe *p;
    unsigned long best = ~0UL;

retry:
    mutex_lock(domain->lock);
    
    // ★ 用主设备号作为哈希索引 ★
    for (p = domain->probes[MAJOR(dev) % 255]; p; p = p->next) {
        struct kobject *(*probe)(dev_t, int *, void *);
        struct module *owner;
        void *data;

        // 检查设备号是否在此 probe 的范围内
        if (p->dev > dev || p->dev + p->range - 1 < dev)
            continue;
            
        // 找到匹配范围最小的 (更精确的匹配)
        if (p->range - 1 >= best)
            break;
            
        if (!try_module_get(p->owner))
            continue;
            
        owner = p->owner;
        data = p->data;        // ★ 这就是 cdev 指针
        probe = p->get;        // exact_match 函数
        best = p->range - 1;
        *index = dev - p->dev; // 计算次设备号偏移
        
        if (p->lock && p->lock(dev, data) < 0) {
            module_put(owner);
            continue;
        }
        
        mutex_unlock(domain->lock);
        
        // 调用 exact_match，返回 cdev->kobj
        kobj = probe(dev, index, data);
        
        module_put(owner);
        if (kobj)
            return kobj;    // ★ 找到了!
        goto retry;
    }
    mutex_unlock(domain->lock);
    return NULL;
}
```

### 6. cdev_add - 驱动注册到 cdev_map

驱动通过 `cdev_add()` 将自己注册到 `cdev_map`：

```c
// fs/char_dev.c:472
int cdev_add(struct cdev *p, dev_t dev, unsigned count)
{
    p->dev = dev;        // 保存设备号
    p->count = count;    // 次设备号数量
    
    // ★ 将 cdev 注册到 cdev_map ★
    return kobj_map(cdev_map, dev, count, NULL, exact_match, exact_lock, p);
    //              ~~~~~~~~  ~~~  ~~~~~                               ~
    //              映射表    设备号 范围                              cdev指针
}
```

### 7. kobj_map - 将设备号和 cdev 关联

```c
// drivers/base/map.c:32
int kobj_map(struct kobj_map *domain, dev_t dev, unsigned long range,
             struct module *module, kobj_probe_t *probe,
             int (*lock)(dev_t, void *), void *data)
{
    unsigned n = MAJOR(dev + range - 1) - MAJOR(dev) + 1;
    unsigned index = MAJOR(dev);    // 主设备号
    unsigned i;
    struct probe *p;

    p = kmalloc(sizeof(struct probe) * n, GFP_KERNEL);
    if (p == NULL)
        return -ENOMEM;

    // 初始化 probe 结构
    for (i = 0; i < n; i++, p++) {
        p->owner = module;
        p->get = probe;         // exact_match
        p->lock = lock;         // exact_lock
        p->dev = dev;           // 设备号
        p->range = range;       // 范围
        p->data = data;         // ★ cdev 指针
    }
    
    mutex_lock(domain->lock);
    
    // 插入到 probes 数组的对应链表中
    for (i = 0, p -= n; i < n; i++, p++, index++) {
        struct probe **s = &domain->probes[index % 255];
        
        // 按 range 大小排序插入 (小的在前，更精确匹配)
        while (*s && (*s)->range < range)
            s = &(*s)->next;
        p->next = *s;
        *s = p;
    }
    
    mutex_unlock(domain->lock);
    return 0;
}
```

---

## 设备号的结构

### dev_t 定义

```c
// include/linux/kdev_t.h
typedef __u32 dev_t;    // 32 位无符号整数

#define MINORBITS    20                          // 次设备号占 20 位
#define MINORMASK    ((1U << MINORBITS) - 1)     // 0xFFFFF

#define MAJOR(dev)   ((unsigned int) ((dev) >> MINORBITS))    // 高 12 位 = 主设备号
#define MINOR(dev)   ((unsigned int) ((dev) & MINORMASK))     // 低 20 位 = 次设备号
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))             // 合成设备号
```

### 设备号结构图

```
              dev_t (32 bits)
┌────────────────┬────────────────────────────────────┐
│   主设备号      │           次设备号                  │
│   (12 bits)    │           (20 bits)                │
│   0-4095       │           0-1048575               │
└────────────────┴────────────────────────────────────┘
         ↑                      ↑
      MAJOR(dev)             MINOR(dev)
```

### 常见设备号示例

| 设备文件 | 主设备号 | 次设备号 | dev_t 值 |
|---------|---------|---------|----------|
| `/dev/null` | 1 | 3 | `0x00100003` |
| `/dev/zero` | 1 | 5 | `0x00100005` |
| `/dev/mem` | 1 | 1 | `0x00100001` |
| `/dev/random` | 1 | 8 | `0x00100008` |
| `/dev/tty0` | 4 | 0 | `0x00400000` |
| `/dev/sda` | 8 | 0 | `0x00800000` |
| `/dev/sda1` | 8 | 1 | `0x00800001` |

可以用以下命令查看设备号：
```bash
ls -la /dev/null
# crw-rw-rw- 1 root root 1, 3 Nov 26 00:00 /dev/null
#                        ~~~~
#                        主,次 设备号
```

---

## 驱动注册过程

### 完整的驱动注册流程

```c
// 驱动初始化代码
static int __init my_driver_init(void)
{
    int ret;
    
    // ============================================
    // 步骤 1: 分配设备号
    // ============================================
    ret = alloc_chrdev_region(&dev_num, 0, 1, "mydev");
    // 内核会从空闲的主设备号中分配一个
    // dev_num 返回分配的设备号
    
    // ============================================
    // 步骤 2: 初始化 cdev，关联 file_operations
    // ============================================
    cdev_init(&my_cdev, &my_fops);
    my_cdev.owner = THIS_MODULE;
    // my_cdev.ops 现在指向 my_fops
    
    // ============================================
    // 步骤 3: 将 cdev 添加到 cdev_map
    // ============================================
    ret = cdev_add(&my_cdev, dev_num, 1);
    // 这会在 cdev_map 中创建映射:
    //   设备号 dev_num → &my_cdev
    
    // ============================================
    // 步骤 4: 创建设备类和设备文件
    // ============================================
    my_class = class_create(THIS_MODULE, "my_class");
    // 创建 /sys/class/my_class/
    
    my_device = device_create(my_class, NULL, dev_num, NULL, "mydev");
    // 创建 /dev/mydev
    // 设备文件的 inode->i_rdev = dev_num
    
    return 0;
}
```

### 注册过程图示

```
                    驱动加载
                       │
                       ▼
         ┌─────────────────────────────┐
         │  alloc_chrdev_region()      │
         │  分配设备号: dev_num = 250:0│
         └─────────────┬───────────────┘
                       │
                       ▼
         ┌─────────────────────────────┐
         │  cdev_init(&my_cdev, &fops) │
         │  my_cdev.ops = &my_fops     │
         └─────────────┬───────────────┘
                       │
                       ▼
         ┌─────────────────────────────┐
         │  cdev_add(&my_cdev, 250:0)  │──────────────┐
         └─────────────┬───────────────┘              │
                       │                              │
                       ▼                              ▼
         ┌─────────────────────────────┐    ┌─────────────────────┐
         │  device_create(..., 250:0,  │    │     cdev_map        │
         │                "mydev")     │    │                     │
         └─────────────┬───────────────┘    │  probes[250%255]:   │
                       │                    │    ↓                │
                       ▼                    │  ┌─────────────┐    │
         ┌─────────────────────────────┐    │  │ probe       │    │
         │  /dev/mydev                 │    │  │ dev=250:0   │    │
         │  inode->i_rdev = 250:0      │    │  │ data=&cdev ─┼────┼──┐
         │  inode->i_fop = def_chr_fops│    │  └─────────────┘    │  │
         └─────────────────────────────┘    └─────────────────────┘  │
                                                                      │
                                            ┌─────────────────────────┘
                                            │
                                            ▼
                                   ┌─────────────────────┐
                                   │  struct cdev        │
                                   │  my_cdev            │
                                   │                     │
                                   │  ops = &my_fops ────┼──┐
                                   │  dev = 250:0        │  │
                                   └─────────────────────┘  │
                                                            │
                                            ┌───────────────┘
                                            │
                                            ▼
                                   ┌─────────────────────┐
                                   │ file_operations     │
                                   │ my_fops             │
                                   │                     │
                                   │ .open = my_open     │
                                   │ .read = my_read     │
                                   │ .write = my_write   │
                                   └─────────────────────┘
```

---

## 查找过程详解

### 用户调用 open("/dev/mydev") 的完整流程

```
用户空间                           内核空间
    │
    │ open("/dev/mydev", O_RDWR)
    │
    └─────────────────────────────────►  sys_open()
                                              │
                                              ▼
                                         do_sys_open()
                                              │
                                              ▼
                                         do_filp_open()
                                              │
                                              ▼
                                         path_openat()
                                              │
                                              ▼
                                         link_path_walk("/dev/mydev")
                                              │
                                              │ 路径解析，找到 inode
                                              │ inode->i_rdev = 250:0
                                              │ inode->i_fop = &def_chr_fops
                                              ▼
                                         do_last()
                                              │
                                              ▼
                                         __dentry_open()
                                              │
                                              │ f->f_op = inode->i_fop
                                              │        = &def_chr_fops
                                              ▼
                                         f->f_op->open(inode, f)
                                              │
                                              │ 即 chrdev_open()
                                              ▼
                                    ┌─────────────────────────┐
                                    │     chrdev_open()       │
                                    │                         │
                                    │  // 用设备号查找        │
                                    │  kobj = kobj_lookup(    │
                                    │      cdev_map,          │
                                    │      inode->i_rdev,     │◄── 250:0
                                    │      &idx);             │
                                    │                         │
                                    │  cdev = container_of(   │
                                    │      kobj, cdev, kobj); │
                                    │                         │
                                    │  // 替换 f_op           │
                                    │  f->f_op = cdev->ops;   │◄── &my_fops
                                    │                         │
                                    │  // 调用驱动的 open     │
                                    │  f->f_op->open();       │◄── my_open()
                                    └─────────────────────────┘
                                              │
                                              ▼
                                         my_open(inode, f)
                                              │
                                              │ 驱动自己的初始化
                                              ▼
                                         返回 fd
    ◄─────────────────────────────────────────┘
```

### 查找过程的时间复杂度

```
1. 路径解析: O(n)，n 为路径深度
   /dev/mydev → 2 级

2. cdev_map 查找: O(m)
   - 哈希索引: O(1)，probes[MAJOR(dev) % 255]
   - 链表遍历: O(m)，m 为该索引下的 probe 数量
   
3. 缓存优化:
   - 首次打开: 需要 kobj_lookup
   - 后续打开: 直接从 inode->i_cdev 获取，O(1)
```

---

## 总结

### 核心机制一览

| 组件 | 作用 | 关键字段 |
|------|------|---------|
| inode | 存储设备文件元数据 | `i_rdev` (设备号), `i_fop`, `i_cdev` |
| def_chr_fops | 字符设备默认操作 | `.open = chrdev_open` |
| chrdev_open | 查找并切换到真正的驱动 | 调用 `kobj_lookup` |
| cdev_map | 设备号 → cdev 的映射表 | `probes[255]` 哈希表 |
| struct probe | 映射表项 | `dev`, `range`, `data` (cdev) |
| struct cdev | 字符设备结构 | `ops` (file_operations) |
| file_operations | 驱动操作函数集 | `open`, `read`, `write` 等 |

### 决定设备文件对应哪个驱动的因素

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        决定性因素                                        │
│                                                                          │
│  1. 设备文件的设备号 (存储在 inode->i_rdev)                              │
│     ↓                                                                    │
│     设备号在创建设备文件时确定 (mknod 或 device_create)                   │
│                                                                          │
│  2. 驱动通过 cdev_add() 注册的设备号范围                                 │
│     ↓                                                                    │
│     驱动加载时将 {设备号, cdev} 映射关系注册到 cdev_map                  │
│                                                                          │
│  匹配规则: inode->i_rdev 落在某个 cdev 注册的 [dev, dev+count) 范围内   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 简化的查找公式

```
/dev/xxx 
    → inode->i_rdev (设备号)
    → kobj_lookup(cdev_map, 设备号)
    → struct cdev
    → cdev->ops (file_operations)
    → 驱动的各种操作函数
```

### 为什么这样设计?

1. **解耦**: 设备文件与驱动代码完全分离，通过设备号间接关联
2. **灵活**: 同一驱动可以处理多个设备 (通过次设备号区分)
3. **动态**: 驱动可以在运行时加载/卸载，设备号动态分配
4. **高效**: 哈希表 + 缓存机制，查找速度快
5. **模块化**: 符合 Linux 的模块化设计理念

---

## 附录: 相关源码文件

| 文件 | 内容 |
|------|------|
| `fs/char_dev.c` | 字符设备核心，chrdev_open, cdev_add 等 |
| `fs/inode.c` | init_special_inode |
| `fs/open.c` | __dentry_open |
| `fs/namei.c` | 路径解析 |
| `drivers/base/map.c` | kobj_map, kobj_lookup |
| `include/linux/cdev.h` | struct cdev 定义 |
| `include/linux/fs.h` | file_operations 定义 |
| `include/linux/kdev_t.h` | dev_t, MAJOR, MINOR 宏 |

---

*本文档基于 Linux 3.2 内核源码分析*

