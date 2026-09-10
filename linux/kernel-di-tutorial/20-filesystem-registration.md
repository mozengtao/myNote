# 20. Filesystem Registration（`struct file_system_type` / `register_filesystem`）

> 本篇是本教程的收尾主题，也是一个**综合案例**：它同时复用了 02 篇 Ops Table
> （`file_system_type` 里的 `mount`/`kill_sb` 回调）、05 篇 Notifier Chain 风格的
> "链表式注册表管理"（`register_filesystem`/`unregister_filesystem` 对全局链表的操作）、
> 06 篇 Initcall（文件系统模块几乎总是用 `module_init` 在加载时机注册自己）三者。
> 如果前面 19 篇你都读过了，这一篇不会有任何新的"手法"，而是一次"综合演练"。

---

## 1. Why

VFS（Virtual File System）要同时支持 ext2、proc、tmpfs、NFS…… 数十种完全不同的文件系统实现，
但 `mount` 系统调用的入口只有一个。如果没有 Filesystem Registration 这套机制，VFS 核心代码只能写成：

```c
long do_mount(const char *fstype, ...)
{
	if (strcmp(fstype, "ext2") == 0)
		return ext2_mount(...);
	else if (strcmp(fstype, "proc") == 0)
		return proc_mount(...);
	else if (strcmp(fstype, "tmpfs") == 0)
		return tmpfs_mount(...);
	/* 每加一种文件系统，都要在这里加一个 else if，还要在 VFS 源码里
	 * #include 这个文件系统模块的头文件 —— 完全违反"开闭原则" */
	else
		return -ENODEV;
}
```

这样写的问题与 00 篇 Mental Model 里论证的完全一致：VFS 核心会与每一种文件系统实现
产生编译期依赖，新增一种文件系统需要修改 VFS 核心代码，而且**编译进内核的文件系统模块
无法在运行期动态增减**（`insmod ext4.ko` 之后 VFS 核心却对 ext4 一无所知）。

Filesystem Registration 的设计目标：

> **让每一种文件系统在自己选择的时机（模块加载/内核启动）把自己"登记"进一个全局注册表，
> VFS 核心只需要在 `mount` 时按名字查这个注册表，然后盲调查到的回调——完全不需要知道
> 这个世界上到底存在多少种文件系统。**

---

## 2. Architecture

```
   VFS 核心（do_mount 路径，完全不知道具体有哪些文件系统）
        |
        | 第一步：按名字查找
        v
  +---------------------------+
  | 全局链表 file_systems      |   fs/filesystems.c:32
  | （侵入式单链表，通过        |
  |  file_system_type.next 串联）|
  +---------------------------+
        |  每个节点是
        v
  +----------------------------+
  | struct file_system_type    |   include/linux/fs.h:1859
  |   name                     |   <-- 查找键
  |   fs_flags                 |   <-- FS_REQUIRES_DEV 等标志
  |   mount                    |   <-- Ops Table 风格回调（02篇）
  |   kill_sb                  |   <-- Ops Table 风格回调（02篇）
  |   owner                    |   <-- 模块引用计数（fops_get/fops_put 同类机制）
  |   next                     |   <-- 链表指针
  +----------------------------+
        |
        | 第二步：盲调
        v
  ext2_fs_type.mount() / proc_fs_type.mount() / tmpfs_fs_type.mount() ...
  （具体文件系统的实现，VFS 核心从不 #include 它们）
```

**版本说明（如实核对本仓库真实情况）**：`struct file_system_type` 在很多教程/旧版内核里
用的是 `get_sb` 回调，但**本仓库（`include/linux/fs.h:1862`）已经是更新过的 `mount` 接口**
（`struct dentry *(*mount)(struct file_system_type *, int, const char *, void *)`），
不再有 `get_sb` 字段——这是内核 2.6.39/3.0 前后的一次真实重构（`mount_bdev`/`mount_single`/
`mount_nodev` 等辅助函数取代了旧的 `get_sb` + 手写 superblock 创建逻辑）。本篇后续所有示例
和引用都严格按本仓库这个真实版本，不采用已经过时的 `get_sb` 写法。

---

## 3. Lifecycle

```
   allocate
   (每种文件系统的实现代码里定义一个 static struct file_system_type 实例，
    比如 fs/ext2/super.c:1490 的 ext2_fs_type)
        |
        v
   initialize
   (用指定初始化器填好 name/fs_flags/mount/kill_sb/owner 字段——
    这几个字段合起来就是"契约"，02 篇 Ops Table 手法的直接应用)
        |
        v
   register（通常发生在 module_init，见 06 篇 Initcall）
   (register_filesystem(&ext2_fs_type)：
    在 file_systems_lock 保护下，把这个实例追加到全局链表 file_systems 末尾)
        |
        v
   framework owns dispatch（任意次 mount 系统调用）
   (do_mount() -> get_fs_type(name) 查找 -> type->mount(...) 盲调；
    可以被调用任意多次，每次挂载互相独立)
        |
        v
   unregister（通常发生在 module_exit，见 06 篇 Initcall）
   (unregister_filesystem(&ext2_fs_type)：从链表摘除，之后 get_fs_type
    再也找不到这个名字；unregister_filesystem() 内部还会调用
    synchronize_rcu()（19 篇 RCU Callback）确保没有正在进行中的
    查找操作还持有指向这个已摘除节点的引用)
        |
        v
   destroy
   (file_system_type 实例本身通常是 static 数据，随模块卸载一起消失；
    真正被销毁的是它挂载出来的 super_block 实例，由 kill_sb 回调负责)
```

---

## 4. Relationship

```
   模块加载/初始化代码（如 fs/ext2/super.c:1498 init_ext2_fs()）
     |
     | 06篇 Initcall：module_init(init_ext2_fs) 决定"什么时候会被调用"
     v
   register_filesystem(&ext2_fs_type)
     |
     | 05篇 Notifier Chain 风格：把节点挂进全局链表
     v
   file_systems（全局链表，VFS 核心持有）
     |
     | mount 系统调用时按名字遍历查找
     v
   get_fs_type("ext2") 返回匹配的 file_system_type 指针
     |
     | 02篇 Ops Table：盲调其中的 mount 字段
     v
   ext2_mount() —— 具体文件系统的实现
     |
     | 进一步调用 mount_bdev() -> ext2_fill_super()
     v
   struct super_block（挂载出来的具体实例，携带 ext2 私有数据，
                       03篇 Context Injection：s_fs_info 字段）
```

一句话总结这张关系图：**Initcall 决定"谁在什么时候登记"，Notifier Chain 风格的链表
决定"登记去哪、怎么查"，Ops Table 决定"查到之后怎么用"——三种手法在这里严丝合缝地拼在一起。**

---

## 5. Data Flow

对照真实内核路径（`mount -t ext2 /dev/sda1 /mnt`）：

```
用户态: mount(2) 系统调用
  |
  v
do_mount()                                          fs/namespace.c
  |
  v
do_kern_mount(fstype="ext2", ...)                   fs/namespace.c:1958
  |
  +- get_fs_type("ext2")                             fs/filesystems.c:270
  |     |
  |     +- __get_fs_type("ext2", 4)                  fs/filesystems.c:257
  |     |     find_filesystem("ext2", 4)             fs/filesystems.c:46
  |     |     （在 file_systems 链表里按名字线性查找）
  |     |
  |     +- 如果没找到，request_module("ext2") 触发   fs/filesystems.c:278
  |        动态加载对应 .ko，再重试一次查找            （对应 06 篇 Initcall 的
  |                                                    "模块加载时自注册"）
  v
vfs_kern_mount(type, flags, name, data)
  |
  v
mount_fs(type, flags, name, data)                   fs/super.c:1075
  |
  v
root = type->mount(type, flags, name, data);         fs/super.c:1092  <-- 盲调
  |
  v
ext2_mount(fs_type, flags, dev_name, data)           fs/ext2/super.c
  |
  +- mount_bdev(fs_type, flags, dev_name, data, ext2_fill_super)
  |     打开块设备，分配 super_block，调用 ext2_fill_super() 填充
  |
  v
返回代表 ext2 根目录的 dentry，挂载完成
```

对照本文档 [c/20-filesystem-registration/mount_dispatch_demo.c](c/20-filesystem-registration/mount_dispatch_demo.c)
的真实运行输出（第 9 节完整粘贴）：`mini_do_mount()` 的"查找 -> 盲调"两步与上面这条真实路径
的 `get_fs_type()` -> `type->mount()` 完全一一对应。

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `file_system_type.name`/`fs_flags`/`mount`/`kill_sb` | **注册后不再修改** | 一旦定义好 `static struct file_system_type` 常量，运行期不应该再改这几个字段 |
| `file_system_type.next` | **仅由 Core（`register_filesystem`/`unregister_filesystem`）修改** | 与 18 篇 `tasklet_struct.next`、03 篇 `work_struct` 的链表指针同理——业务代码不应该直接触碰 |
| 全局链表 `file_systems` 本身 | **运行期频繁变化**，随模块加载/卸载增减节点 | 受 `file_systems_lock`（`rwlock_t`）保护，读多（每次 mount 都要查）写少（模块加载/卸载才变） |
| 单个 `file_system_type` 的注册状态（在链表里/不在链表里） | **可变，可逆**（先注册再注销，还可以再注册） | 04 篇 Driver Registration 讲过的"热插拔"特性在这里同样适用——本篇 9.3 示例的核心断言 |
| 由 `mount()` 回调创建出来的 `super_block` 实例 | **每次挂载独立，各自有自己的生命周期** | 与 `file_system_type`（一种文件系统的"类型"，全局唯一）要区分开——一种类型可以同时挂载出任意多个 `super_block` 实例（比如同时挂载两个 ext2 分区） |

---

## 7. Failure

1. **在没有持有正确锁的情况下遍历/修改 `file_systems` 链表**：真实实现用 `rwlock_t
   file_systems_lock`（`fs/filesystems.c:33`）保护，`register_filesystem`/
   `unregister_filesystem` 用写锁，`get_fs_type`/`fs_index` 等查找用读锁——
   如果哪个新增的文件系统模块绕过 `register_filesystem()` 直接手改链表，会破坏并发安全性。
2. **忘记在 `unregister_filesystem()` 之后等待并发查找结束**：真实实现在成功摘链后调用
   `synchronize_rcu()`（`fs/filesystems.c:113`，见 19 篇 RCU Callback）——这保证了
   "已经在链表遍历过程中拿到这个节点指针"的并发查找不会读到一个正在被卸载模块释放的
   `file_system_type` 结构体。如果模块作者以为"锁一放就可以立刻释放内存"，就会引入一个
   典型的 use-after-free。
3. **重复注册同名文件系统**：`register_filesystem()` 里 `find_filesystem` 一旦发现同名项就
   返回 `-EBUSY`（`fs/filesystems.c:80`），这是防止两个模块都叫自己"ext2"导致行为混乱的
   基本防线——本篇 9.1 示例专门验证了这一点。
4. **`fs_flags` 里的 `FS_REQUIRES_DEV` 没有被正确检查**：如果一个要求块设备的文件系统
   （如 ext2）被以"无设备"方式挂载，`mount()` 回调内部（或调用它之前的 VFS 通用逻辑）
   必须能识别并拒绝，否则回调内部访问一个不存在的设备指针会直接崩溃——本篇 9.2 示例
   演示了这个检查点。
5. **误以为链表插入是"插入头部"**：这是一个容易凭直觉猜错的细节——真实
   `register_filesystem()`（`fs/filesystems.c:69`）实际上是**追加到链表末尾**
   （`find_filesystem` 返回的是"匹配项或链表末尾空槎"的地址），本篇 9.1 示例专门用
   实测输出验证了这一点，提醒"不要凭直觉假设内核数据结构的具体行为，永远去读源码"。
6. **调试方法**：
   - 用户态：本篇三个示例分别用真实的注册/查找/挂载时序输出，直接验证链表操作的正确性；
     可以用 `valgrind` 检测 `multi_fs_module_sim.c` 里模块注销后是否还有残留引用。
   - Kernel：`cat /proc/filesystems`（对应 `fs/filesystems.c:222` 的
     `filesystems_proc_show()`）可以直接看到当前注册表里有哪些文件系统类型，
     以及它们各自的 `nodev` 标志；`lsmod` 配合 `/proc/filesystems` 可以交叉验证
     "某个 `.ko` 是否真的注册成功了它声称支持的文件系统"；`ftrace` 可以跟踪
     `vfs_kern_mount`/`mount_fs` 这类函数的调用，定位挂载失败发生在"查找"还是
     "回调内部"哪一步。

---

## 8. Kernel Core Pseudocode（5 个，均标注本仓库真实文件/行号）

### Example 1 — `struct file_system_type` 本身

真实定义：[include/linux/fs.h:1859](../../include/linux/fs.h)

```c
struct file_system_type {
	const char *name;
	int fs_flags;
	struct dentry *(*mount) (struct file_system_type *, int,
		       const char *, void *);
	void (*kill_sb) (struct super_block *);
	struct module *owner;
	struct file_system_type * next;
	struct list_head fs_supers;
	/* ... lock_class_key 字段用于 lockdep，与 DI 主题无关，省略 */
};
```
为什么是 DI：`mount`/`kill_sb` 是标准的 Ops Table 契约字段（02篇），`next` 是侵入式链表指针
（与 18 篇 `tasklet_struct.next` 同一设计），`owner` 用于模块引用计数，防止模块被卸载后
仍有代码引用它的 `mount` 函数指针指向的、已经被释放的代码段。

### Example 2 — `register_filesystem()`：注册进全局链表

真实定义：[fs/filesystems.c:69](../../fs/filesystems.c)，
查找辅助函数：[fs/filesystems.c:46](../../fs/filesystems.c) `find_filesystem()`

```c
static struct file_system_type **find_filesystem(const char *name, unsigned len)
{
	struct file_system_type **p;
	for (p=&file_systems; *p; p=&(*p)->next)
		if (strlen((*p)->name) == len &&
		    strncmp((*p)->name, name, len) == 0)
			break;
	return p;
}

int register_filesystem(struct file_system_type * fs)
{
	int res = 0;
	struct file_system_type ** p;

	BUG_ON(strchr(fs->name, '.'));
	if (fs->next)
		return -EBUSY;
	INIT_LIST_HEAD(&fs->fs_supers);
	write_lock(&file_systems_lock);
	p = find_filesystem(fs->name, strlen(fs->name));
	if (*p)
		res = -EBUSY;
	else
		*p = fs;             /* 追加到链表末尾，见本篇 Failure 第5点 */
	write_unlock(&file_systems_lock);
	return res;
}
```
为什么是 DI：`register_filesystem()`（Core 的一部分）从未 `#include` `ext2_fs_type`/
`proc_fs_type` 的定义，只操作契约类型 `struct file_system_type *`——任何模块只要填好这个
结构体，调用这一个函数就能"接入"VFS，完全不需要 VFS 核心配合修改一行代码。

### Example 3 — `unregister_filesystem()` + RCU（19篇）延迟安全性

真实定义：[fs/filesystems.c:102](../../fs/filesystems.c)

```c
int unregister_filesystem(struct file_system_type * fs)
{
	struct file_system_type ** tmp;

	write_lock(&file_systems_lock);
	tmp = &file_systems;
	while (*tmp) {
		if (fs == *tmp) {
			*tmp = fs->next;
			fs->next = NULL;
			write_unlock(&file_systems_lock);
			synchronize_rcu();   /* 见 19 篇 RCU Callback */
			return 0;
		}
		tmp = &(*tmp)->next;
	}
	write_unlock(&file_systems_lock);
	return -EINVAL;
}
```
为什么是 DI：这一行 `synchronize_rcu()` 正是 19 篇讲过的"延迟是为了安全性证明"在这里的
具体应用——摘链之后立刻返回是不够的，必须等到"所有可能已经拿到旧 `fs` 指针的并发查找"
都结束之后，调用者才能安全地释放/重用这个 `file_system_type` 所在的内存（比如 `rmmod`
接下来会卸载整个模块代码段）。

### Example 4 — `mount_fs()`：VFS 核心对 `mount` 回调的盲调

真实定义：[fs/super.c:1075](../../fs/super.c)

```c
struct dentry *
mount_fs(struct file_system_type *type, int flags, const char *name, void *data)
{
	struct dentry *root;
	struct super_block *sb;

	root = type->mount(type, flags, name, data);   /* fs/super.c:1092 <-- 盲调 */
	if (IS_ERR(root))
		return root;
	sb = root->d_sb;
	BUG_ON(!sb);
	...
	return root;
}
```
为什么是 DI：`mount_fs()` 拿到 `type` 之后，唯一知道的事情是"它有一个叫 `mount` 的函数指针字段，
调用它会返回一个 `dentry`"——`type` 具体是 ext2 还是 proc、内部怎么解析超级块，
对 `mount_fs()` 完全不可见，这是 02 篇 Ops Table 的标准应用。

### Example 5 — `init_ext2_fs()`：一个真实文件系统模块的完整注册时序

真实定义：[fs/ext2/super.c:1490-1526](../../fs/ext2/super.c)

```c
static struct file_system_type ext2_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "ext2",
	.mount		= ext2_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_ext2_fs(void)
{
	int err = init_ext2_xattr();
	if (err) return err;
	err = init_inodecache();
	if (err) goto out1;
	err = register_filesystem(&ext2_fs_type);
	if (err) goto out;
	return 0;
out:  destroy_inodecache();
out1: exit_ext2_xattr();
	return err;
}

static void __exit exit_ext2_fs(void)
{
	unregister_filesystem(&ext2_fs_type);
	destroy_inodecache();
	exit_ext2_xattr();
}

module_init(init_ext2_fs)
module_exit(exit_ext2_fs)
```
为什么是 DI：这段代码是本篇"综合案例"定位的最佳注脚——`module_init`/`module_exit`
（06篇 Initcall）决定了 `register_filesystem`/`unregister_filesystem`（本篇）
"什么时候会被调用"，而 `ext2_fs_type` 里的 `mount`/`kill_sb`（02篇 Ops Table）
决定了"被调用之后 VFS 该怎么用它"——三层机制的组合，一行都不多余。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/20-filesystem-registration/`](c/20-filesystem-registration/)

### 9.1 `file_system_type_registry.c` — 全局链表的插入/查找/摘除

- 场景：完整复刻 `fs/filesystems.c` 里 `find_filesystem`/`register_filesystem`/
  `unregister_filesystem` 的真实逻辑，重点验证"新注册的文件系统是被追加到链表末尾，
  而不是插入头部"这个容易被凭直觉猜错的细节。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o file_system_type_registry file_system_type_registry.c`
- 实测输出：
  ```
  === file_system_type_registry ===

  -- 依次注册 ext2 / proc / tmpfs --
    [register_filesystem] 'ext2' 注册成功，追加到链表末尾
    [register_filesystem] 'proc' 注册成功，追加到链表末尾
    [register_filesystem] 'tmpfs' 注册成功，追加到链表末尾

    ---- /proc/filesystems 风格输出（按链表顺序） ----
    	ext2
    nodev	proc
    nodev	tmpfs
    --------------------------------------------------

  -- 重复注册 'ext2'，验证 -EBUSY --
    [register_filesystem] 名字 'ext2' 已存在，拒绝(-EBUSY)

  -- 按名字查找 'proc' --
    找到: name='proc'

  -- 注销 'proc'（链表中间节点），验证链表结构仍然完整 --
    [unregister_filesystem] 'proc' 已从链表摘除
    ---- /proc/filesystems 风格输出（按链表顺序） ----
    	ext2
    nodev	tmpfs
    --------------------------------------------------

  -- 再注册一个新的 'proc2'，验证它被追加到当前末尾（而不是头部）--
    [register_filesystem] 'proc2' 注册成功，追加到链表末尾
    ---- /proc/filesystems 风格输出（按链表顺序） ----
    	ext2
    nodev	tmpfs
    nodev	proc2
    --------------------------------------------------

  === done ===
  ```
- 为什么是 DI：`find_filesystem`/`mini_register_filesystem`（Core）从未 `#include`
  `ext2_fs_type`/`proc_fs_type` 的具体实现，只操作 `struct mini_file_system_type *`
  这一层契约；输出直观验证了"追加到末尾"这个真实内核行为，而不是想象中的"插入头部"。

### 9.2 `mount_dispatch_demo.c` — "名字查找 + 回调分发"两步流程

- 场景：完整模拟 `do_kern_mount()` -> `get_fs_type()` -> `mount_fs()` -> `type->mount()`
  这条真实路径，覆盖成功挂载、缺少必要参数、文件系统未注册三种场景。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o mount_dispatch_demo mount_dispatch_demo.c`
- 实测输出：
  ```
  === mount_dispatch_demo ===

  -- 挂载 1: mount -t ext2 /dev/sda1 /mnt --
    [do_mount] 请求挂载 fstype='ext2' dev_name='/dev/sda1'
    [do_mount] 命中注册表: file_system_type{name='ext2', fs_flags=0x1}
    [do_mount] 分发到 type->mount() 回调...
    [do_mount] 挂载成功: root_content="ext2 root inode from /dev/sda1 (block_size=4096)"

  -- 挂载 2: mount -t proc proc /proc --
    [do_mount] 请求挂载 fstype='proc' dev_name='(none)'
    [do_mount] 命中注册表: file_system_type{name='proc', fs_flags=0x0}
    [do_mount] 分发到 type->mount() 回调...
    [do_mount] 挂载成功: root_content="procfs pseudo-root (pid entries generated on the fly)"

  -- 挂载 3: mount -t tmpfs -o size=64m tmpfs /tmp --
    [do_mount] 请求挂载 fstype='tmpfs' dev_name='(none)'
    [do_mount] 命中注册表: file_system_type{name='tmpfs', fs_flags=0x0}
    [do_mount] 分发到 type->mount() 回调...
    [do_mount] 挂载成功: root_content="tmpfs in-memory root (size=64m)"

  -- 挂载 4: mount -t ext2 (忘了指定设备) --
    [do_mount] 请求挂载 fstype='ext2' dev_name='(none)'
    [do_mount] 命中注册表: file_system_type{name='ext2', fs_flags=0x1}
    [do_mount] 'ext2' 要求块设备但 dev_name 为空，返回 -EINVAL

  -- 挂载 5: mount -t btrfs /dev/sdb1 /data（未注册的文件系统）--
    [do_mount] 请求挂载 fstype='btrfs' dev_name='/dev/sdb1'
    [do_mount] 未找到文件系统类型 'btrfs'，返回 -ENODEV

  === done ===
  ```
- 为什么是 DI：`mini_do_mount()`（Core）只做两件事——按字符串名字查找、拿到结果后盲调
  `type->mount()`——ext2/proc/tmpfs 三种"文件系统"的挂载细节（是否需要设备、
  怎么构造根目录内容）完全由各自的回调实现决定，`mini_do_mount()` 一行都不需要为
  "新增一种文件系统"而修改。

### 9.3 `multi_fs_module_sim.c` — 多模块动态注册/注销的完整时间线

- 场景：3 个"模块"（`proc.ko` 常驻、`ext4.ko` 延迟加载后又卸载、`btrfs.ko` 更晚才加载）
  各自在独立线程里模拟自己的生命周期，同时一个"用户挂载操作"线程持续尝试挂载，
  完全不知道此刻哪些模块处于加载状态。
- 编译：`gcc -Wall -Wextra -O2 -pthread -std=gnu11 -o multi_fs_module_sim multi_fs_module_sim.c`
- 实测输出：
  ```
  === multi_fs_module_sim ===

  -- 启动 3 个独立的"模块"生命周期线程 + 1 个用户挂载操作线程 --

    [t=  0ms] proc.ko: module_init -> register_filesystem('proc')
    [t= 50ms] 用户线程尝试挂载:
      [mount] mount -t proc                -> 成功 (ret=0)
    [t=100ms] ext4.ko: insmod -> module_init -> register_filesystem('ext4')
    [t=150ms] 用户线程尝试挂载:
      [mount] mount -t ext4   /dev/sda1    -> 成功 (ret=0)
    [t=200ms] 用户线程尝试挂载:
      [mount] mount -t btrfs  /dev/sdb1    -> 失败: -ENODEV（未注册或已卸载）
    [t=300ms] btrfs.ko: insmod -> module_init -> register_filesystem('btrfs')
    [t=320ms] 用户线程尝试挂载:
      [mount] mount -t btrfs  /dev/sdb1    -> 成功 (ret=0)
    [t=350ms] ext4.ko: rmmod -> module_exit -> unregister_filesystem('ext4')
    [t=400ms] 用户线程尝试挂载:
      [mount] mount -t ext4   /dev/sda1    -> 失败: -ENODEV（未注册或已卸载）
    [t=450ms] 用户线程尝试挂载:
      [mount] mount -t proc                -> 成功 (ret=0)

  -- 最终注册表状态 --
    当前仍注册的文件系统: proc btrfs 

  === done: 注册表核心代码全程没有 #include 任何一个具体文件系统的实现 ===
  ```
  （各 `[t=Nms]` 行的相对顺序由 `usleep` 精确控制，因此在不同机器上重复运行结果稳定一致：
  `t=200ms` 时 btrfs 还未加载所以挂载失败，`t=320ms` 之后成功；`t=400ms` 时 ext4 已被
  卸载所以挂载失败，`t=450ms` 时常驻的 proc 依然可以挂载。）
- 为什么是 DI：`mini_try_mount()`/`mini_register_filesystem()`/`mini_unregister_filesystem()`
  （注册表 Core）从头到尾不知道 `proc_mount_cb`/`ext4_mount_cb`/`btrfs_mount_cb` 这三个
  具体回调的存在，也不知道它们何时会出现或消失——这正是本篇 Why 一节论证的核心收益：
  文件系统的"存在与否"变成了一个纯粹的运行期状态，VFS 核心代码不需要因此做任何改动。

一次性编译并运行全部三个示例：`cd c/20-filesystem-registration && make run`

---

## 10. 与其它 DI 手法比较

本篇作为综合案例，第 10 节不再是"与单一手法的横向对比表"，而是明确列出它复用了哪几个前面篇章的模式：

| 复用的手法 | 在本篇里的具体体现 | 对应章节 |
|---|---|---|
| Ops Table | `file_system_type.mount`/`kill_sb` 打包成一份"完整实现"，VFS 核心盲调 | [02-ops-table.md](02-ops-table.md) |
| Context Injection | `mount()` 回调内部通过 `sb->s_fs_info` 携带具体文件系统的私有数据（ext2 的 `ext2_sb_info` 等） | [03-context-injection.md](03-context-injection.md) |
| Notifier Chain 风格的注册表 | `file_systems` 全局链表的插入（追加末尾）/查找/摘除，与 05 篇 notifier chain 的链表管理手法同构 | [05-notifier-chain.md](05-notifier-chain.md) |
| Initcall | `module_init(init_ext2_fs)` 决定"什么时候调用 `register_filesystem`" | [06-initcall.md](06-initcall.md) |
| Driver Registration 的"热插拔"思想 | 文件系统模块可以 `insmod`/`rmmod`，注册表随之增减，与 04 篇总线-驱动匹配的热插拔特性同构 | [04-driver-registration.md](04-driver-registration.md) |
| RCU Callback | `unregister_filesystem()` 摘链后调用 `synchronize_rcu()`，确保安全释放 | [19-rcu-callback.md](19-rcu-callback.md) |

再补充一张和 18/19 篇同类主题的对比，帮助定位本篇在"注册类"手法里的位置：

| 对比 | Driver Registration（04篇） | Filesystem Registration（本篇） |
|---|---|---|
| 匹配方式 | 总线按 ID/VID-PID 等硬件信息匹配设备与驱动 | VFS 按**字符串名字**匹配 `mount` 请求与 `file_system_type` |
| 一个类型可以产生多少个运行期实例 | 一个驱动可以绑定多个匹配的设备 | 一个 `file_system_type` 可以同时挂载出多个 `super_block` 实例 |
| 注册表数据结构 | 通常是链表 + 匹配函数（`bus_type` 内部） | 简单的单链表 `file_systems`（`fs/filesystems.c:32`） |
| 是否支持热插拔 | 是（PCI/USB 设备可以随时插拔） | 是（文件系统模块可以随时 `insmod`/`rmmod`） |

一句话总结全篇（也总结本教程第二批 Group D 三个主题的内在联系）：
**17/18 篇（softirq/tasklet）讲的是"回调在什么执行环境下被触发"的谱系，19 篇（RCU）讲的是
"回调何时被安全地触发"，本篇（文件系统注册）则展示了"回调如何被组织成一个可动态增减的注册表，
供一个完全通用的核心代码按需查找并分发"——三者合在一起，覆盖了 Kernel DI 在
"时机"、"安全性"、"可发现性"这三个维度上最具代表性的设计模式。**
