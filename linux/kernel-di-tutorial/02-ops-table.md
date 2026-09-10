# 02. Operations Table（ops）

> 上一篇 [01-function-pointer-callback.md](01-function-pointer-callback.md) 建立了"单个函数指针注入"的直觉。
> 本篇讲的是把**一组相关的函数指针打包成一个结构体**，即 Kernel 里最著名的设计范式："ops table"。

---

## 1. Why

一个对象往往不是只有"一个动作"，而是有一整套相关行为：文件要能 `read`/`write`/`llseek`/`mmap`/`release`；
网卡要能 `ndo_open`/`ndo_stop`/`ndo_start_xmit`。如果每个动作都用一个独立的函数指针字段：

```c
struct file {
	ssize_t (*do_read)(...);
	ssize_t (*do_write)(...);
	int (*do_open)(...);
	int (*do_release)(...);
	loff_t (*do_llseek)(...);
	/* ... 20 多个字段，散落在 struct file 里 */
};
```

问题：

1. **语义分散**：一个"文件系统实现"要填一堆散落的字段，容易漏填、容易与其它字段混淆归属。
2. **不能表达"这是同一套实现"**：ext4 的 20 个函数指针在概念上属于同一个"ext4 file_operations"，
   分散成字段后无法整体替换、整体比较、整体判断"是不是同一种实现"。
3. **每个 `struct file` 实例都要存这 20 个指针**：内存浪费——同一种文件系统的所有打开文件，
   这 20 个函数其实完全一样。

Ops Table 的设计目标：

> **把"一组相关能力"打包成一个只读、共享的常量结构体，对象只存一个指向它的指针。**

```c
struct file {
	const struct file_operations *f_op;   /* 一个指针代替 20 个字段 */
};

static const struct file_operations ext4_file_operations = { .read = ..., .write = ..., ... };
```

好处立刻体现：**内存**（N 个打开的 ext4 文件共享同一份 `ext4_file_operations`，而不是各自持有 20 个指针的拷贝）、
**语义**（一份 ops table = 一种完整实现）、**替换的原子性**（换实现只需换一个指针，不会出现"改了一半"的中间态）。

---

## 2. Architecture

```
        Core (VFS / net core / mm core ...)
                     |
                     | 持有一个指针字段
                     v
             +----------------+
             |  const struct  |
             |  xxx_ops *ops  |        <-- 契约：一组函数指针的集合类型
             +----------------+
                     |
        在对象创建/挂载时被整体注入
                     |
                     v
     +---------------+----------------+
     |                                |
static const struct xxx_ops      static const struct xxx_ops
       impl_A_ops                        impl_B_ops
   (只读、编译期常量，                (只读、编译期常量，
    被所有该类型实例共享)              被所有该类型实例共享)
```

Kernel 里最经典的五个 ops table：

```
file_operations          -> fs/*, drivers/char/*     (open/read/write/release/mmap/...)
inode_operations         -> fs/*                     (lookup/create/mkdir/unlink/...)
net_device_ops           -> drivers/net/*             (ndo_open/ndo_stop/ndo_start_xmit/...)
vm_operations_struct      -> mm, drivers                (fault/open/close/...)
address_space_operations -> fs/*, mm                   (readpage/writepage/...)
```

---

## 3. Lifecycle

```
   allocate
   (定义 struct xxx_operations 类型；具体实现写一份 static const 实例)
        |
        v
   initialize
   (用指定初始化器填好每个函数指针字段：.read = my_read, .write = my_write, ...)
        |
        v
   inject
   (在对象创建时，把 &my_ops 赋给对象的 ops 指针字段：
    inode->i_fop = &my_file_operations;)
        |
        v
   framework owns dispatch
   (Core 在生命周期内反复调用 obj->ops->xxx(...)，自己决定调用时机)
        |
        v
   callback
   (进入具体实现函数体)
        |
        v
   destroy
   (对象销毁时，ops 指针字段随对象一起消失；ops table 本身是 static 数据，
    通常与模块生命周期绑定，不需要单独释放)
```

关键差异 vs 01 篇：ops table 本身几乎不会"注册到全局表"（这是 04 篇 Driver Registration 要做的事）；
它是"**在创建单个对象的那一刻**，把一整套行为一次性焊死到这个对象上"。

---

## 4. Relationship

```
   file                        (VFS 对象，代表一个打开的文件描述符)
     |
     |  f_op 字段指向
     v
   file_operations              (ops table，一种"类型的行为集合")
     |
     |  各字段函数指针指向
     v
   driver / filesystem 的具体实现函数
     |
     |  可能进一步依赖
     v
   inode / super_block / 具体硬件寄存器 ...
```

在本文档三个用户态示例里：

```
vtable_shape_polymorphism:   struct shape.ops       -> struct shape_ops       -> circle/rect/triangle 实现
mini_vfs_filesystem:         struct mini_file.f_op   -> mini_file_operations   -> ramfs/devnull/readonly 实现
strategy_pattern_ops:        struct codec.ops        -> struct codec_ops       -> none/rle/xor 实现
```

---

## 5. Data Flow

以 `mini_vfs_filesystem.c` 为例，对照真实 VFS 调用链：

```
用户态示例：                          真实 Kernel（fs/read_write.c）：

main()                                 sys_read(fd, buf, count)
  |                                       |
  v                                       v
mini_vfs_read(f, buf, n)               vfs_read(file, buf, count, pos)
  |                                       |
  v                                       v
f->f_op->read(f, buf, n)               file->f_op->read(file, buf, count, pos)
  |   <-- 盲调，不知道是 ramfs                |   <-- 盲调，不知道是 ext4/nfs/pipe
  v   还是 devnull                            v
ramfs_read() / devnull_read()          ext4_file_read() / pipe_read() / ...
```

一次性看清"新增一种文件系统需要做什么"：

```
1. 定义一份 static const struct file_operations my_fs_fops = { .read = ..., .write = ... };
2. 在 open()/mount() 路径里把 inode->i_fop = &my_fs_fops;
3. VFS core 一行代码都不用改。
```

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `static const struct xxx_ops` 本身 | **不可变**（编译期常量，通常放在 `.rodata`） | 多个对象实例共享同一份，天然线程安全、cache 友好 |
| 对象里的 `ops` 指针字段 | **对象生命周期内一般不变**，但**允许**运行期切换 | 例如 `strategy_pattern_ops.c` 里同一个 `codec` 变量在运行期从 `none_ops` 切到 `rle_ops` 再切到 `xor_ops` |
| 对象的其它数据字段（如 `private_data`） | **可变** | 与 ops table 本身无关，属于对象自己的状态 |
| ops table 里某个字段是否为 `NULL` | **实现决定，Core 必须检查** | 如 `devnull_fops` 没有 `open`/`release`，`mini_vfs_open()` 必须判空后才调用 |

---

## 7. Failure

1. **字段未填导致 NULL 调用**：`triangle_ops` 忘记填 `destroy` 字段会导致 `shape_destroy()` 调用 NULL 指针 crash。
   -> 防御手段：Core 侧统一判空（如示例中 `mini_vfs_open`/`mini_vfs_read`），或者提供"默认 ops"作为兜底
   （Kernel 里很多子系统会有 `generic_file_xxx()` 之类的通用实现，实现者可以选择性覆盖）。
2. **ops table 被声明为非 const，被意外修改**：如果不加 `const`，某个 bug 可能在运行期把
   `ext4_file_operations.read` 改写掉，所有共享这份 ops 的文件全部受影响——这是"不可变性"被破坏的典型灾难。
   `static const` 是**故意**的设计约束，不是随手加的关键字。
3. **对象存活期长于 ops table 所在模块**：如果 ops table 定义在一个可以被卸载的内核模块里，模块被 `rmmod`
   之后，仍然存活的对象继续持有指向已释放代码段的 `ops` 指针——这是模块引用计数（`module_get`/`try_module_get`）
   存在的核心原因之一。
4. **强制类型转换（downcast）出错**：`vtable_shape_polymorphism.c` 里 `(const struct circle *)self` 依赖
   "`struct shape base` 必须是第一个字段"这个隐含契约，一旦某个实现把 `base` 放到第二个字段，
   转换后的地址就是错的——Kernel 里正式的解法是 `container_of()`（见 03 篇），比"强转 + 假设首字段"更安全、更明确。
5. **调试方法**：
   - 用户态：`gdb` 里 `p *shapes[0]->ops` 可以直接把整个 ops table 打印出来，逐字段核对是否被正确注入；
     `valgrind` 能捕捉到"调用未初始化函数指针"这类问题。
   - Kernel：`crash` 工具解析 core dump 时，`struct file_operations` 的内容会显示每个字段指向的符号名；
     `WARN_ON(!ops->read)` 之类的防御性检查是标准做法；`CONFIG_DEBUG_VIRTUAL`/`lockdep` 可以协助抓生命周期错误。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — `struct file_operations`：VFS 读写分派

真实定义：[include/linux/fs.h:1583](../../include/linux/fs.h)

```c
struct file_operations {
	struct module *owner;
	loff_t (*llseek)(struct file *, loff_t, int);
	ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
	int (*open)(struct inode *, struct file *);
	int (*release)(struct inode *, struct file *);
	/* ... 还有 mmap/fsync/aio_read/unlocked_ioctl 等 20+ 字段 */
};

/* core: fs/read_write.c */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	if (!file->f_op->read)
		return -EINVAL;
	return file->f_op->read(file, buf, count, pos);   /* <-- 盲调 */
}
```
为什么是 DI：VFS 编译时不 `#include` 任何文件系统实现；`f_op` 由具体文件系统在 `open()` 路径里注入。

### Example 2 — `struct net_device_ops`：网络设备发包

真实位置：`include/linux/netdevice.h`（本仓库中为 `struct net_device_ops`，字段包括 `ndo_open`/`ndo_start_xmit`）。

```c
struct net_device_ops {
	int (*ndo_open)(struct net_device *dev);
	int (*ndo_stop)(struct net_device *dev);
	netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
	/* ... */
};

/* core: net/core/dev.c dev_hard_start_xmit() 简化 */
netdev_tx_t dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	const struct net_device_ops *ops = dev->netdev_ops;
	return ops->ndo_start_xmit(skb, dev);   /* <-- 盲调，不知道是 e1000 还是 virtio-net */
}
```
为什么是 DI：同一份网络协议栈代码可以驱动任意网卡驱动，只要驱动填好 `net_device_ops` 并挂到 `dev->netdev_ops`。

### Example 3 — `struct vm_operations_struct`：缺页异常处理

场景：进程访问一段 mmap 出来的内存但物理页还未映射，触发 page fault，MM core 调用该 vma 的 `fault` 回调。

```c
struct vm_operations_struct {
	void (*open)(struct vm_area_struct *area);
	void (*close)(struct vm_area_struct *area);
	int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
};

/* core: mm/memory.c do_linear_fault() 简化 */
static int do_linear_fault(struct mm_struct *mm, struct vm_area_struct *vma, ...)
{
	if (vma->vm_ops && vma->vm_ops->fault)
		return vma->vm_ops->fault(vma, &vmf);   /* <-- 盲调，可能是驱动的 mmap 实现 */
	return VM_FAULT_SIGBUS;
}
```
为什么是 DI：一个字符设备驱动实现 `mmap()` 时，只需要提供自己的 `vm_operations_struct`，MM 核心的缺页处理逻辑完全复用。

### Example 4 — `struct address_space_operations`：页缓存回写

场景：脏页需要写回磁盘（`writeback`），MM/VFS 核心不知道具体文件系统怎么把一页数据落盘。

```c
struct address_space_operations {
	int (*writepage)(struct page *page, struct writeback_control *wbc);
	int (*readpage)(struct file *, struct page *);
};

/* core: mm/page-writeback.c 简化 */
static int write_one_page(struct page *page)
{
	struct address_space *mapping = page->mapping;
	return mapping->a_ops->writepage(page, &wbc);   /* <-- 盲调，ext4/xfs/btrfs 各自实现 */
}
```
为什么是 DI：脏页回写这条核心路径与"怎么写到 ext4 还是 btrfs"完全解耦。

### Example 5 — `struct inode_operations`：路径查找

场景：`open("/a/b/c")` 需要逐级查找目录项，VFS 核心的 `lookup` 逻辑对每一种文件系统都一样，
真正"怎么在这个目录里找 c"由具体文件系统实现。

```c
struct inode_operations {
	struct dentry *(*lookup)(struct inode *, struct dentry *, struct nameidata *);
	int (*create)(struct inode *, struct dentry *, int, struct nameidata *);
};

/* core: fs/namei.c 简化 */
static struct dentry *real_lookup(struct inode *dir, struct dentry *dentry, ...)
{
	return dir->i_op->lookup(dir, dentry, nd);   /* <-- 盲调 */
}
```
为什么是 DI：路径解析算法（处理 `.`/`..`/符号链接/挂载点）是 VFS 通用逻辑，"在这个目录里怎么找一个名字"
才是文件系统特有逻辑，二者通过 `i_op` 解耦。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/02-ops-table/`](c/02-ops-table/)

### 9.1 `vtable_shape_polymorphism.c` — Ops Table 实现多态对象系统

- 场景：`circle`/`rectangle`/`triangle` 三种形状共享 `struct shape_ops` 契约，Core 遍历异构数组统一调用。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o vtable_shape_polymorphism vtable_shape_polymorphism.c -lm`
- 实测输出：
  ```
  === vtable_shape_polymorphism demo ===

  -- Core 遍历异构对象数组，全部走同一套 ops table 调用路径 --
  [core] circle(r=2.00)                   area=12.566 perimeter=12.566 (type=circle)
  [core] rectangle(3.00x4.00)             area=12.000 perimeter=14.000 (type=rectangle)
  [core] equilateral-triangle(side=5.00)  area=10.825 perimeter=15.000 (type=triangle)

  [core] total area of all shapes = 35.392

  -- 销毁阶段：每个对象自己知道怎么释放自己 --
    [circle_ops] destroying circle at 0x582e2b0
    [rect_ops] destroying rectangle at 0x582e2d0
    [triangle_ops] destroying triangle at 0x582e300

  === done ===
  ```
- 为什么是 DI：`print_shape_report()`/`shape_area()` 等 Core 函数从未 `#include` 任何具体形状的实现细节，
  只通过 `s->ops->area(s)` 盲调；新增 `pentagon` 类型不需要改一行 Core 代码。

### 9.2 `mini_vfs_filesystem.c` — 模拟文件系统 `file_operations`

- 场景：`ramfs`/`devnull`/`readonly` 三种"文件系统"共享 `struct mini_file_operations`，
  `mini_vfs_read`/`mini_vfs_write` 是 VFS Core。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o mini_vfs_filesystem mini_vfs_filesystem.c`
- 实测输出：
  ```
    [readonly] write rejected, -EROFS(-30)
  === mini_vfs_filesystem demo ===

  [vfs] open('memo.txt') via fs='ramfs'
    [ramfs] allocated backing buffer for 'memo.txt'
  [core] read 12 bytes from memo.txt: hello ramfs
    [ramfs] freeing backing buffer for 'memo.txt'
  [vfs] closed 'memo.txt'

  [vfs] open('null') via fs='devnull'
  [core] devnull write() returned 18 (accepted but discarded)
  [core] devnull read() returned 0 (always EOF)
  [vfs] closed 'null'

  [vfs] open('etc-hosts') via fs='readonly'
  [core] read 24 bytes from readonly fs: static readonly content
  [core] write to readonly fs returned -30 (expected negative)
  [vfs] closed 'etc-hosts'

  === done ===
  ```
  （首行 `-EROFS` 告警是 stderr 无缓冲提前刷出，对应"对只读文件系统写入被拒绝"这一预期路径。）
- 为什么是 DI：`devnull_fops` 故意不填 `open`/`release`，验证了 Core 必须对"契约里的可选字段"做 NULL 检查——
  这与真实 Kernel `file_operations` 里大量字段可选、VFS 处处判空的写法完全一致。

### 9.3 `strategy_pattern_ops.c` — 策略模式（压缩算法族）

- 场景：`none`/`rle`/`xor-mask` 三种编解码策略，运行期在**同一个 `codec` 变量**上切换 `ops` 指针。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o strategy_pattern_ops strategy_pattern_ops.c`
- 实测输出：
  ```
  === strategy_pattern_ops demo ===

  -- pipeline[A] using algo='none' --
    compress: 28 bytes -> 28 bytes (ratio 1.00)
    decompress: 28 bytes -> 28 bytes, round-trip OK
  -- pipeline[B] using algo='rle' --
    compress: 28 bytes -> 8 bytes (ratio 0.29)
    decompress: 8 bytes -> 28 bytes, round-trip OK
  -- pipeline[C] using algo='rle' --
    compress: 43 bytes -> 86 bytes (ratio 2.00)
    decompress: 86 bytes -> 43 bytes, round-trip OK
  -- pipeline[D] using algo='xor-mask' --
    compress: 43 bytes -> 43 bytes (ratio 1.00)
    decompress: 43 bytes -> 43 bytes, round-trip OK

  [core] final codec stats: bytes_in=43 bytes_out=43 (algo=xor-mask)

  === done ===
  ```
  （示例 C 用 RLE 压缩自然语言文本导致"膨胀"（86 > 43 字节），这是真实的 RLE 特性：
  没有重复字节的数据用 RLE 反而变大，示例特意保留这个"失败"案例来说明"策略选择不当也是一种 Failure”。）
- 为什么是 DI：`pipeline_run()` 从头到尾不知道当前用的是哪种算法，只认 `codec_ops` 契约；
  新增一种算法（比如真正的 zlib）只需要新写一份 `static const struct codec_ops zlib_ops = {...}`。

一次性编译并运行全部三个示例：`cd c/02-ops-table && make run`

---

## 10. 与其它 DI 手法比较

| 对比 | Ops Table | Function Pointer Callback | Notifier Chain | Driver Registration |
|---|---|---|---|---|
| 注入粒度 | 一组相关函数指针（结构体） | 单个函数指针 | 单个函数指针 + 优先级，支持多个 | 整个对象族（ops + 元数据 + 匹配规则） |
| 是否共享实例 | **是**，`static const`，被同类型所有对象共享 | 视场景，通常每个注册项独立 | 每个订阅者独立注册 | 每个驱动独立注册，但可以匹配多个设备 |
| 是否表达"完整实现" | **是**，一份 ops table = 一种完整实现 | 否，只是一个动作 | 否，只是"对某事件的响应" | 是，且带有更多注册期元数据（id_table 等） |
| 何时用 Ops Table | 一个对象需要实现 3 个以上相关操作，且这些操作总是"整体出现"（一种文件系统/一种设备类型） | 只需要注入一个独立动作 | 需要"广播"给不确定数量的订阅者 | 需要"注册一种可被总线发现和匹配的实现" |
| 何时不要用 | 只有 1-2 个操作、且不同对象组合各异时，用散列的 callback 字段反而更灵活 | — | — | — |

一句话：**Ops Table 是"打包版的 Callback”**——当一个对象的多个回调总是同生共死、同属一种实现时，
把它们收进一个 `static const` 结构体，既表达了"这是一整套实现"的语义，又节省了内存、提升了 cache 命中率。
下一篇 [03-context-injection.md](03-context-injection.md) 会讲：**ops table 本身是无状态的（`static const`），
那具体每个对象实例的私有状态放在哪里？**——答案就是 Context Injection。
