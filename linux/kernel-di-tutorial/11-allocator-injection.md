# 11. Allocator Injection

> 前面几篇讨论的 DI 手法，注入的时机不外乎两种：**运行期**（02 Ops Table、04/09 驱动注册、
> 05 Notifier Chain 都是"装好指针，运行时盲调"）和**链接期**（06 Initcall，靠链接器收集）。
> 本篇要介绍第三种、也是最"早"的一种注入时机：**编译期**——SLAB 和 SLUB 是两套完全独立、
> 互不知道对方存在的分配器实现，Kconfig 帮你在编译这一刻就二选一，运行期的内核镜像里
> 根本不存在"另一个选项"。

---

## 1. Why

内核里几乎每个子系统都要频繁分配/释放小块内存（`task_struct`、`inode`、网络包描述符、
文件描述符……）。如果每次都直接找伙伴系统（buddy allocator，按页分配），会有两个问题：

1. **粒度不匹配**：`task_struct` 只有几百字节，为它分配一整页（4KB）浪费严重。
2. **初始化开销重复**：同一种对象反复分配/释放时，如果每次都从"一张白纸"开始构造字段，
   构造逻辑要重复执行；如果能"记住"上一次的对象布局，可以省掉部分工作
   （这正是 08 节要讲的 `ctor` 回调价值所在）。

Kernel 用 **slab 分配器**解决这两个问题：预先按"对象类型"划分出一批同尺寸的小块内存
（`kmem_cache`），需要用某种对象类型时直接从对应的 cache 里取，不再向 buddy allocator
频繁申请/释放整页。

但"slab 分配器具体怎么实现"这件事，Kernel 社区在历史上给出了**两套完全不同、各有取舍**
的答案，且本仓库版本还带了第三套：

- **SLAB**（`mm/slab.c`）：维护 per-CPU、per-node 的对象队列，追求"缓存热"（cache-hot）
  对象的高效复用，元数据开销较大，逻辑也更复杂。
- **SLUB**（`mm/slub.c`）：尽量减少每个对象的元数据开销，用更紧凑的布局换取更好的
  cache line 利用率，是本仓库 `init/Kconfig` 里的默认选项。
- **SLOB**（`mm/slob.c`）：面向嵌入式/内存极度受限场景的极简实现（`depends on EXPERT`）。

如果不用某种"契约 + 编译期选择"的设计，唯一的替代方案是：**内核源码里同时保留三份实现，
运行期用 `if (allocator_type == SLAB) ... else if (== SLUB) ...` 来决定走哪一条路**——
这不仅每次分配都多一次分支判断（热路径上的额外开销不可接受），三份实现的所有符号还会
同时出现在最终的 `vmlinux` 里，即使你永远只会用到其中一个。

设计目标：

> **让 SLAB/SLUB/SLOB 三套实现各自完整地实现同一套对外契约
> （`kmem_cache_create`/`kmem_cache_alloc`/`kmem_cache_free` 等函数签名完全相同），
> 通过 Kconfig 的编译期选择，只让其中一套的 `.o` 文件真正参与链接。
> 调用方代码（几乎整个内核）永远只 `#include <linux/slab.h>`，不知道、也不需要知道
> 运行的到底是哪一套实现。**

这是"分层解耦"思路在**时间维度**上的极致体现——04/09 篇的 DI 是"运行期插指针"，
06 篇 Initcall 是"链接期收集函数指针数组"，本篇的 SLAB/SLUB 选择则更进一步：
**连"两套实现同时存在于二进制里、只是没被调用"这件事本身都不允许发生**，
非选中的那一套实现的目标文件根本不会被编译进最终镜像。

---

## 2. Architecture

```
   Kconfig（编译期，人或 make menuconfig 决定）
        |
        |  choice "Choose SLAB allocator"   (init/Kconfig)
        v
   +-----------------------------------------------------+
   |   CONFIG_SLAB=y          或    CONFIG_SLUB=y         |   <-- 二选一，互斥
   +-----------------------------------------------------+
        |                              |
        v                              v
   mm/Makefile:                  mm/Makefile:
   obj-$(CONFIG_SLAB) += slab.o  obj-$(CONFIG_SLUB) += slub.o
        |                              |
        v                              v
   +----------------+           +----------------+
   |   mm/slab.c     |   （只有其中一个真正参与链接，        |   mm/slub.c     |
   |  完整独立实现    |    另一个的 .o 从未出现在 vmlinux 里）  |  完整独立实现    |
   +----------------+                                      +----------------+
        |                                                          |
        +-----------------------+----------------------------------+
                                 |  两者导出完全相同的符号名
                                 v
                    kmem_cache_create() / kmem_cache_alloc() / kmem_cache_free()
                                 |
                                 |  调用方（几乎全部内核子系统）
                                 v
                    #include <linux/slab.h>，只依赖这一份函数签名契约
```

真实定义（对比 [mm/slab.c:2213](../../mm/slab.c) 与 [mm/slub.c:3872](../../mm/slub.c)，
本仓库真实代码，两份函数签名逐字符相同）：

```c
/* include/linux/slab.h:101，两套实现都必须满足这份签名契约 */
struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
			unsigned long, void (*)(void *));
```

同一份头文件声明，`mm/slab.c` 和 `mm/slub.c` 各自定义了一份完全独立、互不调用彼此
内部函数的实现——这与 02 篇 Ops Table"同一个对象的 ops 字段运行期可能指向不同实现"
不同：**这里没有"字段"，也没有"运行期指向"，符号解析这件事在链接阶段就已经唯一确定。**

---

## 3. Lifecycle

Allocator Injection 实际上包含两层完全不同时间尺度的"生命周期"，本节分别展开：

### 3.1 编译期选择的生命周期（SLAB vs SLUB，一次性决定）

```
   allocate
   (人在 make menuconfig / make defconfig 时面对 choice "Choose SLAB allocator")
        |
        v
   initialize
   (.config 文件里写入 CONFIG_SLUB=y，或 CONFIG_SLAB=y，两者互斥)
        |
        v
   inject（编译期）
   (mm/Makefile 的 obj-$(CONFIG_SLUB) += slub.o 只会把 slub.o 加进编译列表)
        |
        v
   framework owns dispatch（链接期）
   (链接器把 slub.o 里定义的 kmem_cache_create/alloc/free 符号，解析给所有
    调用方——调用方从未见过 mm/slab.c 的存在，即使它就在同一份源码树里)
        |
        v
   [整个内核镜像的运行期：这个选择永远不会改变，没有"运行期切换分配器"这个操作]
        |
        v
   destroy
   (不适用——除非重新配置 + 重新编译 + 重新部署整个内核)
```

### 3.2 单个 `kmem_cache` 的运行期生命周期（`ctor` 注入发生在这一层）

```
   allocate
   (kmem_cache_create("my_obj", sizeof(struct my_obj), align, flags, my_obj_ctor))
        |
        v
   initialize
   (cachep->ctor = my_obj_ctor —— DI 的核心动作，把"对象构造逻辑"
    注入进通用 slab 框架，此后 cachep 的行为携带了这份契约)
        |
        v
   inject（首次构造）
   (kmem_cache_alloc() 第一次把一块原始内存组织成"这种类型的对象"时，
    调用一次 cachep->ctor(objp)——只在真正"新分配"时触发，见 11.1 示例)
        |
        v
   framework owns dispatch（复用）
   (对象被 kmem_cache_free() 归还后再被 kmem_cache_alloc() 取出复用，
    ctor **不会**再被调用——这是本篇 Failure 一节的头号陷阱)
        |
        v
   destroy
   (kmem_cache_destroy(cachep) —— 回收整个 cache 占用的所有 slab 页；
    真实 Kernel 要求此时 cache 内没有任何对象仍在使用中，否则拒绝销毁)
```

---

## 4. Relationship

```
   Kconfig（编译期决策者，运行期不存在对应的运行期对象）
        |
        | 决定链接哪一个 .o
        v
   mm/slab.c  或  mm/slub.c（二者只有一个真正参与链接）
        |
        | 导出相同符号名
        v
   struct kmem_cache（运行期对象，字段布局因后端不同而不同，
                      但对调用方永远不可见——是"不透明指针"）
        |
        | .ctor 字段（03 篇 Context Injection 手法）
        v
   业务代码提供的构造函数（如 my_obj_ctor），在 kmem_cache_create() 时注入
        |
        | 被 slab 框架在"首次构造"时盲调
        v
   新分配出来的对象实例
```

`struct kmem_cache` 本身是一个天然的"不透明指针"（opaque pointer）——调用方拿到的
`struct kmem_cache *` 从不解引用其内部字段，这与 09 篇 `struct bus_type` 被调用方
（PCI/USB 层）直接读写字段的方式不同：**分配器这一层的封装比总线框架更彻底**，
连"内部字段长什么样"这个信息本身都因编译期选择而不同（SLAB 版本的 `kmem_cache`
比 SLUB 版本多出 per-CPU 队列相关的字段），调用方完全不需要关心。

---

## 5. Data Flow

以 `kmem_cache_ctor_demo.c` 为例：

```
main()
  |
  +- kmem_cache_create("task_obj", sizeof(struct task_obj), task_obj_ctor)
  |     |
  |     v
  |  cachep->ctor = task_obj_ctor   // DI 注入完成，尚未分配任何对象
  |
  +- kmem_cache_alloc(task_cache)  x 3   // 池是空的，3 次都触发新构造
  |     |
  |     v
  |  每次都命中 "pool_used[slot]==0" 分支 -> 调用 ctor(objp) -> 打印"constructed"
  |
  +- kmem_cache_free(task_cache, t1)      // 归还，但 pool_used[0] 仍为 1
  |
  +- kmem_cache_alloc(task_cache)         // 复用 t1 的内存
        |
        v
     pool_used[0]==1，跳过 ctor 调用，直接返回旧内存
     （t4->run_count 仍是归还前设置的 5，证明 ctor 没有被重新调用）
```

对照真实 Kernel（本仓库真实代码，`mm/slab.c` `cache_init_objs()`）：

```
kmem_cache_alloc(cachep, GFP_KERNEL)
  |
  v
cache_alloc_refill() -> 若当前 slab 页里的对象都已构造过，直接从
  slab->free 链表取一个 bufctl，不会再调用 ctor
  |
  v
（只有当需要分配一整块新的 slab 页时才会走到）
cache_grow() -> cache_init_objs(cachep, slabp)
  |
  v
for (i = 0; i < cachep->num; i++) {
    if (cachep->ctor && !(cachep->flags & SLAB_POISON))
        cachep->ctor(objp + obj_offset(cachep));   /* <-- 盲调，仅在新 slab 页初始化时 */
}
```

这印证了 11.1 示例复刻的核心语义：**`ctor` 只在"内存第一次被组织成这种类型的对象"
时调用，而不是在每一次 `kmem_cache_alloc()` 时调用**——这是本篇 Failure 一节要重点
强调的、也是真实内核开发者最容易踩的坑。

---

## 6. State

| 状态 | 可变性 | 说明 |
|---|---|---|
| `CONFIG_SLAB`/`CONFIG_SLUB` 的选择 | **编译期一次性决定，运行期镜像里永久固定** | 与本系列其它所有手法都不同——没有"运行期"这个状态维度 |
| `kmem_cache->ctor` 字段 | **`kmem_cache_create()` 时确定，此后不变** | 一个 cache 只能有一个 ctor，与 02 篇"ops 指针运行期可切换"不同，这里更接近"创建时定死" |
| 单个对象是否"已构造过"（`pool_used[]`/真实 Kernel 的 slab bufctl 状态） | **运行期动态**，随分配/归还变化 | 这是本篇唯一"运行期活跃"的状态，决定 `ctor` 会不会被再次调用 |
| 对象内容（业务字段） | **完全由调用方管理，slab 框架不感知** | 归还后再取出时可能是"脏数据"（11.1 示例的 `buffer_obj` 场景），这是分配器语义留给调用方的责任边界 |
| slab/slub 内部账本（空闲链表、per-CPU 队列、bitmap 等） | **运行期高度动态**，但对调用方完全不可见 | 11.2 示例用两种不同的账本结构（freelist vs bitmap）具体演示了这种"内部状态因后端而异"的现象 |

---

## 7. Failure

1. **误以为 `ctor` 等价于"每次分配都会重新初始化"**：这是本篇最容易踩的坑——
   `ctor` 只在对象**第一次**被组织出来时调用一次，之后反复 `alloc`/`free` 循环里
   **不会**再被调用（见 11.1 示例的 `buffer_obj` 场景：复用旧内存时 `data[0]` 残留
   上一次使用者写入的 `0xFF`，而不是被重新清零）。真实内核对这个语义的正确应对方式是：
   需要"每次分配都保证内容干净"时用 `kzalloc()`（内部会额外做一次 `memset`），
   而不是依赖 `ctor`；`ctor` 更适合用来初始化那些"整个对象生命周期内都不变"的字段
   （比如内嵌的锁、链表头）。
2. **在 `ctor` 里分配同一个 cache 的对象**：`mm/slab.c` 源码注释明确警告
   "Constructors are not allowed to allocate memory from the same cache which
   they are a constructor for. Otherwise, deadlock."——`ctor` 运行在
   `cache_grow()` 路径上，此时该 cache 的锁可能已经持有，递归申请同一个 cache
   会直接死锁。
3. **假设 SLAB 和 SLUB 的性能特征/内部行为一致**：两者在"对象复用顺序"上可能完全不同
   （见 11.2 示例：SLAB 风格的空闲链表是 LIFO——最近归还的最先被复用；
   SLUB 风格的 bitmap 扫描是"从头找第一个空闲槽"——复用顺序可能是不同的）。
   如果代码依赖了"分配出来的内存地址在某种特定顺序下复用"这种未文档化的行为
   （这本身就是一个不该依赖的实现细节），换一种编译期选择后行为会悄悄改变，
   这类 bug 极难在单一配置下的测试中发现。
4. **忘记编译期选择是"全局唯一"的**：不能指望在同一个内核镜像里"某些子系统用 SLAB，
   某些子系统用 SLUB"——`obj-$(CONFIG_SLAB)`/`obj-$(CONFIG_SLUB)` 是整个内核范围的
   互斥开关，这与 09 篇"不同总线类型可以同时共存、互不干扰"是本质区别：
   09 篇的多态发生在**运行期、按实例**（不同的 `bus_type` 实例），
   本篇的选择发生在**编译期、全局**（只有一种分配器实现存在）。
5. **`kmem_cache_destroy()` 时仍有对象在使用**：真实 Kernel 的 `kmem_cache_destroy()`
   会检查 cache 是否"干净"（没有已分配但未归还的对象），如果调用方在还有对象存活时
   就销毁 cache，后续任何对这些"僵尸对象"的访问都是未定义行为——这与 04 篇驱动卸载时
   的 refcount 保护是同一类问题在不同层面的复现。
6. **调试方法**：
   - 用户态：像 11.1/11.2 示例那样统计 `ctor_calls`/`alloc_count`/`reused_from_freelist`
     等计数器，用"预期值 vs 实际打印值"的方式验证"何时构造、何时复用"的边界。
   - Kernel：`/proc/slabinfo`（`cat /proc/slabinfo`）可以看到当前系统里每个 `kmem_cache`
     的对象大小、活跃对象数、总分配数；`slabtop` 命令提供实时排序视图；
     `CONFIG_SLUB_DEBUG`（编译时开启）可以让 SLUB 在对象头尾插入 poison 字节，
     检测"释放后写入"（use-after-free）；`echo <cache_name> > /sys/kernel/slab/<cache>/validate`
     可以主动触发一次 slab 完整性校验（仅 SLUB 支持，取决于内核版本）。

---

## 8. Kernel Core Pseudocode（5 个）

### Example 1 — Kconfig 里的编译期二选一

真实定义：[init/Kconfig:1227](../../init/Kconfig)

```
choice
	prompt "Choose SLAB allocator"
	default SLUB
	help
	   This option allows to select a slab allocator.

config SLAB
	bool "SLAB"
config SLUB
	bool "SLUB (Unqueued Allocator)"
config SLOB
	depends on EXPERT
	bool "SLOB (Simple Allocator)"
endchoice
```
为什么是 DI：这是全系列教程里**唯一一处不依赖任何 C 语言机制（函数指针/结构体字段）
的注入点**——它是纯粹的构建系统层面的选择，Kconfig 的 `choice`/`endchoice` 保证了
"互斥性"（不可能同时选中 SLAB 和 SLUB），这份互斥性直接反映到 [mm/Makefile:34](../../mm/Makefile)
`obj-$(CONFIG_SLOB) += slob.o` / `obj-$(CONFIG_SLAB) += slab.o` / `obj-$(CONFIG_SLUB) += slub.o`
这三行——只有被选中的那一行才会让对应的 `.o` 出现在最终的链接命令里。

### Example 2 — `kmem_cache_create()`：两套实现，一份签名

真实定义：[mm/slab.c:2213](../../mm/slab.c)、[mm/slub.c:3872](../../mm/slub.c)

```c
/* mm/slab.c 版本（简化）：关注对齐、colour、slabp_cache 等 SLAB 特有概念 */
struct kmem_cache *
kmem_cache_create(const char *name, size_t size, size_t align,
		  unsigned long flags, void (*ctor)(void *))
{
	struct kmem_cache *cachep = __kmem_cache_create(name, size, align, flags);
	cachep->ctor = ctor;         /* <-- 注入点，两套实现在这里做的事情高度一致 */
	cachep->name = name;
	setup_cpu_cache(cachep, gfp); /* SLAB 特有：搭建 per-CPU array_cache */
	return cachep;
}

/* mm/slub.c 版本（简化）：关注 find_mergeable（尝试合并到已有 cache）等 SLUB 特有概念 */
struct kmem_cache *
kmem_cache_create(const char *name, size_t size, size_t align,
		  unsigned long flags, void (*ctor)(void *))
{
	struct kmem_cache *s = find_mergeable(size, align, flags, name, ctor);
	if (s) {
		s->refcount++;        /* SLUB 特有：多个同尺寸 cache 可以合并共享 */
		return s;
	}
	s = kmalloc(sizeof(*s), GFP_KERNEL);
	s->ctor = ctor;            /* <-- 同样的注入点 */
	return s;
}
```
两份实现内部的字段（`slabp_cache`/`colour` vs `refcount`/`find_mergeable`）
完全不同，但 `cachep->ctor = ctor` 这一步、以及函数签名，逐字符一致——
这正是"契约稳定，实现自由"的编译期体现。

### Example 3 — `cache_init_objs()`：ctor 只在"新构造"时被调用

真实定义：[mm/slab.c:2744](../../mm/slab.c)（`cache_init_objs()`）

```c
static void cache_init_objs(struct kmem_cache *cachep, struct slab *slabp)
{
	int i;
	for (i = 0; i < cachep->num; i++) {
		void *objp = index_to_obj(cachep, slabp, i);
		if (cachep->ctor)
			cachep->ctor(objp);   /* <-- 盲调，仅在这个 slab 页被首次初始化时执行 */
		slab_bufctl(slabp)[i] = i + 1;
	}
}
```
这个函数只在 `cache_grow()`（需要向 cache 里添加一整页新的 slab 空间）时被调用——
日常的 `kmem_cache_alloc()`/`kmem_cache_free()` 循环走的是 `cache_alloc_refill()`/
`free_block()` 路径，完全不经过这里，`ctor` 自然也不会被再次触发。

### Example 4 — `kmem_cache_alloc()`：两套实现导出相同符号

真实定义：[mm/slab.c:3696](../../mm/slab.c)、[mm/slub.c:2326](../../mm/slub.c)

```c
/* mm/slab.c */
void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	void *ret = __cache_alloc(cachep, flags, ...);
	/* SLAB 特有：优先从 per-CPU array_cache 里取，miss 才去 slab 页里挖 */
	return ret;
}

/* mm/slub.c */
void *kmem_cache_alloc(struct kmem_cache *s, gfp_t gfpflags)
{
	void *ret = slab_alloc(s, gfpflags, NUMA_NO_NODE, _RET_IP_);
	/* SLUB 特有：优先从 per-CPU freelist 单链表头部摘一个，天然 cache line 友好 */
	return ret;
}
```
调用方（几乎整个内核，比如 `kernel/fork.c` 里 `task_struct` 的分配）永远只写
`kmem_cache_alloc(task_struct_cachep, GFP_KERNEL)` 这一行代码，链接到哪一套实现
完全由 `.config` 决定，`kernel/fork.c` 本身不需要为"支持 SLUB"或"支持 SLAB"
做任何区分。

### Example 5 — `find_mergeable()`：SLUB 特有的"cache 合并"优化（无 SLAB 对应物）

真实定义：[mm/slub.c](../../mm/slub.c) `find_mergeable()`（`kmem_cache_create()` 内部调用）

```c
/* mm/slub.c 简化：多个尺寸相近、flags 相同的 cache 可以合并共用同一个 kmem_cache */
static struct kmem_cache *find_mergeable(size_t size, size_t align,
					 unsigned long flags, const char *name,
					 void (*ctor)(void *))
{
	struct kmem_cache *s;
	list_for_each_entry(s, &slab_caches, list) {
		if (ctor)                      continue;  /* 有 ctor 的 cache 不参与合并 */
		if (s->size - size >= sizeof(void *)) continue;
		return s;                       /* 找到可以合并复用的现有 cache */
	}
	return NULL;
}
```
这是"编译期选择不同实现，行为可以天差地别"的最佳例证：**SLAB 完全没有"cache 合并"
这个概念**，SLUB 为了减少 cache 数量（进而减少元数据总开销）额外做了这一层优化——
调用方即使换到 SLAB 编译，代码也完全不需要修改，只是"隐性"地失去了这个特定的内存节省效果，
这正是"契约稳定但具体实现的能力边界可以不同"这一设计哲学的直接体现。

---

## 9. User Space Examples（3 个，均已实测编译运行）

代码目录：[`c/11-allocator-injection/`](c/11-allocator-injection/)

### 9.1 `kmem_cache_ctor_demo.c` — ctor 只在首次构造时被调用

- 场景：完整复刻 `kmem_cache_create()`/`kmem_cache_alloc()`/`kmem_cache_free()` 的
  ctor 语义，用 `task_obj`（魔数+运行计数）和 `buffer_obj`（清零验证）两种对象类型，
  验证"首次分配触发 ctor，归还后复用不再触发 ctor，脏数据会残留"这一核心语义。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o kmem_cache_ctor_demo kmem_cache_ctor_demo.c`
- 实测输出：
  ```
  [slab] kmem_cache_create('task_obj', size=24) ctor=yes
  [slab] kmem_cache_create('buffer_obj', size=32) ctor=yes

  === kmem_cache_ctor_demo ===

  -- 第一次分配 3 个 task_obj：应该触发 3 次 ctor --
        [ctor] task_obj constructed at 0x27d28450 (magic set)
        [ctor] task_obj constructed at 0x27d28470 (magic set)
        [ctor] task_obj constructed at 0x27d28490 (magic set)

  -- 归还 t1，再次分配一个新对象：期望复用 t1 的内存，且 ctor 不再被调用 --
    t4 == t1(旧地址复用)? yes ; t4->run_count（未被 ctor 重置，应保留旧值 5）= 5
    ctor_calls so far (预期仍是 3，因为复用没有触发新的构造) = 3

  -- buffer_obj：验证 ctor 只在第一次构造时清零，复用时不会重新清零 --
        [ctor] buffer_obj constructed at 0x27d284b0 (zeroed)
    b1->data[0] (首次构造，ctor 清零后应为 0) = 0
    b2->data[0] (复用旧内存，ctor 未被重新调用，应残留脏数据 255) = 255
    ctor_calls so far (预期仍是 1，复用没有触发新的构造) = 1
        [ctor] buffer_obj constructed at 0x27d284e0 (zeroed)
    b3->data[0] (全新 slot，ctor 清零后应为 0) = 0
    ctor_calls so far (预期变为 2，新 slot 触发了新的构造) = 2

  === done ===
  [slab] kmem_cache_destroy('task_obj'), ctor was called 3 time(s) total
  [slab] kmem_cache_destroy('buffer_obj'), ctor was called 2 time(s) total
  ```
  （`t4 == t1` 且 `run_count` 保留旧值 5，证明复用没有重新构造；`b2->data[0]=255`
  精确复现了"归还后复用、ctor 不会清零"这个真实陷阱，`b3` 因为落在全新 slot 上才
  重新触发 ctor 并清零，两个 cache 最终的 `ctor_calls` 分别精确停在 3 和 2。）
- 为什么是 DI：`kmem_cache_alloc()`（Core）不知道 `task_obj_ctor`/`buffer_obj_ctor`
  具体做什么，只在"这块内存第一次变成对象"这一个精确时机盲调 `cachep->ctor`；
  新增一种对象类型完全不需要修改分配框架代码。

### 9.2 `pluggable_allocator_backend.c` — SLAB 风格 vs SLUB 风格后端

- 场景：用一个统一的 `struct allocator_backend` 契约，实现两种内部行为完全不同的
  分配器后端（LIFO 空闲链表 vs bitmap 线性扫描），启动时选定一种（模拟 Kconfig 的
  编译期二选一），验证调用方代码（`generic_alloc`/`generic_free`）里不存在任何
  区分后端的分支判断，同时两种后端的"复用顺序"确实不同。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o pluggable_allocator_backend pluggable_allocator_backend.c`
- 实测输出（分别以默认参数和 `slub` 参数运行两次）：
  ```
  === pluggable_allocator_backend demo ===

  [config] selected backend = SLAB-style (等价于 CONFIG_SLAB / CONFIG_SLUB 二选一)

  -- 分配 5 个对象 --
    generic_alloc() -> 0x4042e0
    generic_alloc() -> 0x404320
    generic_alloc() -> 0x404360
    generic_alloc() -> 0x4043a0
    generic_alloc() -> 0x4043e0

  -- 归还其中 2 个，再分配 2 个新的（验证复用/分配策略的差异）--
    new alloc -> 0x4043a0, 0x404320

  -- 后端内部统计（不同后端记录的指标完全不同，调用方从不关心）--
    [slab-style] alloc=7 free=2 reused_from_freelist=2 pool_used=5/8

  === done ===
  调用方代码（generic_alloc/generic_free）从头到尾没有出现任何
  if (backend == SLAB) ... else ... 的分支判断。
  ```
  ```
  ===== 以 'slub' 参数运行 =====
  [config] selected backend = SLUB-style (等价于 CONFIG_SLAB / CONFIG_SLUB 二选一)
  ...
  -- 归还其中 2 个，再分配 2 个新的（验证复用/分配策略的差异）--
    new alloc -> 0x4040e0, 0x404160

  -- 后端内部统计（不同后端记录的指标完全不同，调用方从不关心）--
    [slub-style] alloc=7 free=2 bitmap_scans=21 currently_used=5/8
  ```
  （SLAB-style 释放顺序为 `objs[1]`、`objs[3]`，复用时按 LIFO 链表先取出最近归还的
  `objs[3]`（`0x4043a0`）再取出 `objs[1]`（`0x404320`）；SLUB-style 复用时按 bitmap
  从头扫描，先命中索引更小的 `objs[1]`（`0x4040e0`）再命中 `objs[3]`（`0x404160`）——
  两种后端的"复用顺序"确实相反，但 `generic_alloc`/`generic_free` 这两个调用方函数
  的源码完全没有变化，只是链接期绑定的 `active_backend` 不同。）
- 为什么是 DI：`generic_alloc()`/`generic_free()`（Core）严格只通过
  `active_backend->alloc`/`active_backend->free` 这一层间接调用，从未直接引用
  `slab_style_alloc`/`slub_style_alloc` 的符号名；这与真实 Kernel "调用方只
  `#include <linux/slab.h>`，从不 `#include "mm/slab_internal.h"`" 的封装原则完全一致。

### 9.3 `custom_arena_allocator.c` — 通用容器的可插拔节点分配器

- 场景：一个 `generic_list` 单链表容器，节点分配策略被抽成 `struct node_allocator`
  契约，分别接入"直接转发 libc malloc/free"和"线性 bump-pointer arena，free 只记账，
  reset 才批量回收"两种截然不同的分配策略，验证容器代码完全复用、行为差异全部来自
  分配器实现。
- 编译：`gcc -Wall -Wextra -O2 -std=gnu11 -o custom_arena_allocator custom_arena_allocator.c`
- 实测输出：
  ```
  === custom_arena_allocator demo ===

  -- 场景 1：容器使用 malloc-backed 分配器 --
    [3,2,1] (nr_nodes=3)
    [2,1] (nr_nodes=2)
    malloc_stats: alloc_calls=3 free_calls=1

  -- 场景 2：另一个容器实例使用 arena 分配器（同一份 generic_list 代码）--
    [40,30,20,10] (nr_nodes=4)
    [20,10] (nr_nodes=2)
    arena_state: alloc_calls=4 free_calls_ignored=2 offset=64/4096 bytes

  -- arena 特有操作：批量重置，一次性回收所有节点占用的内存 --
    [arena] reset: all 4 previous alloc(s) reclaimed in O(1)
    arena_state after reset: offset=0 (回到 0，可以重新线性分配)

  -- 验证：list1 (malloc) 与 list2 (arena) 使用同一份 list_push_front/list_pop_front 代码 --
    list1.allocator->name = malloc-backed
    list2.allocator->name = arena

  === done ===
  ```
  （`arena_state.free_calls_ignored=2` 精确对应两次 `list_pop_front()` 调用了
  `arena_free()`，但 `offset` 始终维持在 `64`（4 个节点 x 16 字节，未因 free 而减少）——
  证明 arena 的 free 确实"什么都不做"，直到显式 `arena_reset()` 才一次性回收；
  两个容器实例用的是同一份 `list_push_front`/`list_pop_front` 二进制代码。）
- 为什么是 DI：`list_push_front()`/`list_pop_front()`（Core）只通过
  `list->allocator->alloc`/`->free` 访问分配逻辑，完全不知道背后是 libc heap
  还是一段裸内存 buffer；这对应真实 Kernel 里"某些高频路径子系统（网络收包、
  块设备 I/O）为了避免频繁 `kmalloc`/`kfree` 的开销，自己维护请求级 arena/pool，
  但底层容器/协议栈代码完全不用为此改动"的真实设计动机。

一次性编译并运行全部三个示例：`cd c/11-allocator-injection && make run`

---

## 10. 与其它 DI 手法比较

DI 手法可以按"注入时机"排成一条完整的谱系，本篇正好补上了这条谱系里此前教程尚未
明确讨论过的一端：

| 注入时机 | 代表手法 | 谁做选择 | 选择能否运行期改变 | 未选中的实现是否存在于最终二进制里 |
|---|---|---|---|---|
| **编译期** | Allocator Injection（本篇，SLAB/SLUB/SLOB） | Kconfig（人/`make menuconfig`） | **否**，需要重新编译整个内核 | **否**，`obj-$(CONFIG_x)` 直接控制是否参与编译 |
| **链接期** | Initcall（06 篇） | 链接器（收集所有 `__initcall` section 里的符号） | 否，`insmod` 之外没有"链接期之后再改" | 是（模块化构建下未编译进内核的 `.ko` 不算，但内建部分的注册函数体确实都在） |
| **运行期** | Ops Table（02 篇）、Driver Registration（04/09 篇）、Scheduler Class（10 篇，但链结构编译期固定） | 代码逻辑（`register_xxx()`调用时机） | **是**，`register`/`unregister` 随时可以发生 | 是，所有已编译的实现都在二进制里，只是有没有被"注册"决定会不会被调用 |

| 对比 | Allocator Injection | Initcall | Ops Table |
|---|---|---|---|
| 决策主体 | Kconfig（编译期，构建系统） | 链接器（收集 section） | 代码（运行期赋值 ops 指针） |
| 决策粒度 | **全局**（整个内核只有一种分配器实现） | 每个 initcall 函数独立决定"属于哪个 level" | 每个对象实例独立决定"指向哪个 ops" |
| 是否支持"多个实现同时存在，运行期选一个" | **否**——这正是本篇与 02/04/09/10 篇最大的区别 | 不适用（都会被调用，不是"选一个"） | 是，02 篇讨论的正是这种运行期多态 |
| 典型收益 | 消除运行期分支判断和未使用代码的体积/attack surface 开销 | 消除"运行期调用注册函数"的开销 | 保留运行期灵活切换的能力 |
| 典型代价 | **灵活性最低**——无法在不重新编译的前提下切换 | 无法在运行期新增/移除（对内建部分而言） | 每次调用多一次指针间接跳转的（极小）开销 |

一句话：**如果说 02 篇 Ops Table 是"运行期用一次指针跳转换取多态"，06 篇 Initcall 是
"链接期用零运行时开销换取自动注册"，那么本篇 Allocator Injection 就是这条"解耦收益
与运行时开销"谱系的另一端极值——用完全牺牲"运行期灵活切换"的能力，换取"连未选中的实现
的代码都不会出现在最终二进制里"这一 zero-cost abstraction 的终极形态。** 三者共同构成
了 `00-mental-model.md` 第 4 节提到的"编译器、链接器、开发者代码三者合力替代运行期容器"
这句话里的完整三重奏：**编译器（本篇的 Kconfig+条件编译）负责最粗粒度、最早期的选择；
链接器（06 篇）负责中等粒度的自动收集；开发者代码（02/04/05/09/10 篇的运行期 register）
负责最细粒度、最晚期、也最灵活的绑定。**

至此，本批次（第二批）三篇全部完成。09 篇补齐了 04 篇背后的总线元框架，
10 篇展示了 Ops Table 与链式结构结合出的"依次尝试"新范式，11 篇把"注入时机"这条
此前隐含在各篇背后的时间轴显式地讲清楚——三者与第一批的 8 种手法一起，
共同印证了 `00-mental-model.md` 开篇的那句话：**Kernel 里所有的 DI，
最终都归结为"Core 持有一个契约类型的指针（或在编译期确定一条唯一的符号解析路径），
在恰当的时机对它做一次盲调"，区别只在于这个"恰当的时机"发生在编译期、链接期，还是运行期。**
