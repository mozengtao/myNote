# Memory Management Flashcards - Linux Kernel Implementation

> **Focus**: Linux Kernel memory management internals, data structures, and APIs
> **Language**: English terms with Chinese explanations
> **Total Cards**: 100+

---

## 1. Memory Architecture Overview (内存架构概述)

---

Q: What are the three main layers of Linux memory management?
A: 
```
+----------------------------------------------------------+
|                    User Space                             |
|    malloc() / mmap() / brk()                             |
+----------------------------------------------------------+
                          |
                          v
+----------------------------------------------------------+
|                Virtual Memory Layer                       |
|    VMA, Page Tables, mm_struct                           |
|    Page Fault Handler                                     |
+----------------------------------------------------------+
                          |
                          v
+----------------------------------------------------------+
|              Physical Memory Allocators                   |
|    Buddy System (页分配器)                                |
|    Slab/Slub/Slob (对象分配器)                            |
+----------------------------------------------------------+
                          |
                          v
+----------------------------------------------------------+
|              Physical Memory (RAM)                        |
|    Zones: DMA, DMA32, Normal, HighMem                    |
|    Nodes (NUMA)                                          |
+----------------------------------------------------------+
```
三层：用户空间接口、虚拟内存管理、物理内存分配。
[Basic]

---

Q: What is the difference between physical address and virtual address?
A: 
| 地址类型 | 说明 | 范围 |
|----------|------|------|
| **物理地址** | RAM芯片上的实际地址 | 取决于物理内存大小 |
| **虚拟地址** | CPU/进程看到的地址 | 32位: 4GB, 64位: 128TB+ |

```
Virtual Address                Physical Address
+------------+                 +------------+
| 0xFFFF...  |   MMU/Page     |  实际RAM   |
|  内核空间   |   Tables       |            |
+------------+ ─────────────> +------------+
| 0x7FFF...  |                 |            |
|  用户空间   |                 |            |
+------------+                 +------------+
| 0x0000...  |
+------------+
```

虚拟地址通过MMU和页表转换为物理地址。
[Basic]

---

Q: What is the kernel virtual address space layout on x86_64?
A: 
```
0xFFFFFFFFFFFFFFFF  +------------------+
                    | 未使用            |
0xFFFFFFFF80000000  +------------------+
                    | 直接映射(物理内存) |  kernel text/data
0xFFFF888000000000  +------------------+  PAGE_OFFSET
                    | 直接映射区域       |  physmem直接映射
                    | (最大64TB)        |
0xFFFF800000000000  +------------------+
                    | 保护空间(空洞)     |
                    |                  |
0x0000800000000000  +------------------+
                    |   用户空间        |  每进程独立
                    |   (128TB)        |
0x0000000000000000  +------------------+
```
内核使用高地址，用户空间使用低地址，中间有空洞防止越界访问。
[Intermediate]

---

## 2. Physical Memory Organization (物理内存组织)

---

Q: What is a page frame and what is `struct page`?
A: **页帧(Page Frame)**: 物理内存的基本管理单位，通常4KB。

**`struct page`**: 内核为每个物理页帧维护的描述符：
```c
// include/linux/mm_types.h
struct page {
    unsigned long flags;        // 页面状态标志 (PG_locked, PG_dirty等)
    
    union {
        struct {                // 用于页面缓存
            struct list_head lru;   // LRU链表
            struct address_space *mapping;
            pgoff_t index;          // 文件偏移
        };
        struct {                // 用于slab
            struct kmem_cache *slab_cache;
            void *freelist;
        };
        struct {                // 用于复合页
            unsigned long compound_head;
            unsigned int compound_order;
        };
    };
    
    atomic_t _refcount;         // 引用计数
    atomic_t _mapcount;         // 映射计数（多少PTE指向此页）
    // ...
};
```
通过`virt_to_page()`和`page_to_virt()`转换。
[Intermediate]

---

Q: What are memory zones and why do they exist?
A: 内存区域(Zone)根据物理地址范围和用途划分：

| Zone | 地址范围(x86) | 用途 |
|------|--------------|------|
| `ZONE_DMA` | 0-16MB | ISA DMA设备 |
| `ZONE_DMA32` | 0-4GB | 32位DMA设备 |
| `ZONE_NORMAL` | 16MB-896MB(32位) / 全部(64位) | 普通内存分配 |
| `ZONE_HIGHMEM` | >896MB (仅32位) | 高端内存，需临时映射 |

```c
struct zone {
    unsigned long watermark[NR_WMARK];  // 水位线 (min/low/high)
    unsigned long nr_reserved_highatomic;
    struct free_area free_area[MAX_ORDER]; // 伙伴系统空闲链表
    spinlock_t lock;
    // ...
};

// 获取区域
struct zone *zone = page_zone(page);
```
[Intermediate]

---

Q: What is NUMA and how is it represented in the kernel?
A: **NUMA (Non-Uniform Memory Access)**: 多CPU系统中，每个CPU有本地内存，访问远程内存较慢。

```
+--------+     内存总线      +--------+
| Node 0 |<---------------->| Node 1 |
|  CPU0  |                  |  CPU1  |
|  CPU1  |                  |  CPU2  |
|  本地内存|                  | 本地内存 |
+--------+                  +--------+
    |                           |
    | 访问本地: ~100ns          | 访问远程: ~300ns
    v                           v
```

内核表示：
```c
// include/linux/mmzone.h
typedef struct pglist_data {
    struct zone node_zones[MAX_NR_ZONES];  // 该节点的所有zone
    struct zonelist node_zonelists[MAX_ZONELISTS]; // zone优先级列表
    int nr_zones;
    struct page *node_mem_map;              // 该节点的page数组
    unsigned long node_start_pfn;           // 起始页帧号
    unsigned long node_spanned_pages;       // 总页数
    int node_id;                            // 节点ID
    // ...
} pg_data_t;

// 获取节点
int nid = page_to_nid(page);
pg_data_t *pgdat = NODE_DATA(nid);
```
[Intermediate]

---

Q: What is the relationship between node, zone, and page?
A: 
```
+------------------------------------------------------------------+
|                          System Memory                            |
+------------------------------------------------------------------+
|                                                                  |
|  +------------------------+    +------------------------+        |
|  |        Node 0          |    |        Node 1          |        |
|  +------------------------+    +------------------------+        |
|  |                        |    |                        |        |
|  |  +------------------+  |    |  +------------------+  |        |
|  |  |   ZONE_DMA       |  |    |  |   ZONE_DMA       |  |        |
|  |  +------------------+  |    |  +------------------+  |        |
|  |  |   ZONE_DMA32     |  |    |  |   ZONE_DMA32     |  |        |
|  |  +------------------+  |    |  +------------------+  |        |
|  |  |   ZONE_NORMAL    |  |    |  |   ZONE_NORMAL    |  |        |
|  |  |  +----+----+--+  |  |    |  |  +----+----+--+  |  |        |
|  |  |  |page|page|..|  |  |    |  |  |page|page|..|  |  |        |
|  |  |  +----+----+--+  |  |    |  |  +----+----+--+  |  |        |
|  |  +------------------+  |    |  +------------------+  |        |
|  +------------------------+    +------------------------+        |
|                                                                  |
+------------------------------------------------------------------+

层次关系：
System → Node(s) → Zone(s) → Page(s)
```
[Basic]

---

## 3. Buddy System (伙伴系统)

---

Q: What is the buddy system and how does it work?
A: 伙伴系统是Linux物理页分配器，管理2^n大小的连续页块：

```
Order  Pages   Size
  0      1      4KB
  1      2      8KB
  2      4     16KB
  ...
  10   1024    4MB (MAX_ORDER-1)

分配过程（请求4页 = order 2）：
+----------------------------------+
|          Order 3 (8页)            |  没有order 2，分裂order 3
+----------------------------------+
        |                |
        v                v
+----------------+  +----------------+
|  Order 2 (4页) |  |  Order 2 (4页) |  分裂成两个伙伴
+----------------+  +----------------+
     返回             放入空闲链表

释放过程（归还4页）：
检查伙伴是否也空闲 → 如果是，合并成order 3
```
避免外部碎片，但可能有内部碎片（请求3页得到4页）。
[Intermediate]

---

Q: What are the key functions for page allocation?
A: 
```c
/* 核心分配函数 */
// 分配2^order个连续页，返回第一页的page结构
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

// 分配单页
struct page *alloc_page(gfp_t gfp_mask);
#define alloc_page(gfp) alloc_pages(gfp, 0)

// 分配并返回虚拟地址
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
unsigned long __get_free_page(gfp_t gfp_mask);

// 分配并清零
unsigned long get_zeroed_page(gfp_t gfp_mask);

/* 释放函数 */
void __free_pages(struct page *page, unsigned int order);
void __free_page(struct page *page);
void free_pages(unsigned long addr, unsigned int order);
void free_page(unsigned long addr);

/* 地址转换 */
void *page_address(struct page *page);  // page → 虚拟地址
struct page *virt_to_page(void *addr);  // 虚拟地址 → page
unsigned long virt_to_phys(void *addr); // 虚拟 → 物理
void *phys_to_virt(phys_addr_t addr);   // 物理 → 虚拟
```
[Basic]

---

Q: What are GFP flags and what do they mean?
A: GFP (Get Free Pages) 标志控制内存分配行为：

**Zone修饰符**：
| 标志 | 含义 |
|------|------|
| `__GFP_DMA` | 从ZONE_DMA分配 |
| `__GFP_DMA32` | 从ZONE_DMA32分配 |
| `__GFP_HIGHMEM` | 可使用高端内存 |

**行为修饰符**：
| 标志 | 含义 |
|------|------|
| `__GFP_WAIT` / `__GFP_RECLAIM` | 允许睡眠/回收 |
| `__GFP_IO` | 允许发起I/O |
| `__GFP_FS` | 允许调用文件系统 |
| `__GFP_ZERO` | 清零返回的页 |
| `__GFP_ATOMIC` | 不可睡眠 |
| `__GFP_NOWARN` | 失败时不打印警告 |

**常用组合**：
```c
GFP_KERNEL   // 内核常规分配，可睡眠
GFP_ATOMIC   // 中断上下文，不可睡眠
GFP_USER     // 用户空间分配
GFP_HIGHUSER // 用户空间，可用高端内存
GFP_DMA      // DMA内存
```
[Intermediate]

---

Q: When to use GFP_KERNEL vs GFP_ATOMIC?
A: 
| 标志 | 上下文 | 特点 |
|------|--------|------|
| `GFP_KERNEL` | 进程上下文 | 可睡眠、可回收页面、成功率高 |
| `GFP_ATOMIC` | 中断/原子上下文 | 不可睡眠、从紧急储备分配、可能失败 |

```c
// 进程上下文（可睡眠）
void *ptr = kmalloc(size, GFP_KERNEL);

// 中断处理程序（不可睡眠）
irqreturn_t my_handler(int irq, void *dev_id)
{
    void *ptr = kmalloc(size, GFP_ATOMIC);
    // ...
}

// 持有spinlock时（不可睡眠）
spin_lock(&lock);
ptr = kmalloc(size, GFP_ATOMIC);
spin_unlock(&lock);
```

**规则**：
- 持有spinlock → GFP_ATOMIC
- 中断上下文 → GFP_ATOMIC
- 其他情况 → 优先GFP_KERNEL
[Basic]

---

## 4. Slab Allocator (Slab分配器)

---

Q: What is the slab allocator and why is it needed?
A: Slab分配器用于高效分配小于一页的内核对象：

```
+------------------------------------------------------------------+
|                    为什么需要Slab？                                |
+------------------------------------------------------------------+
|                                                                  |
| 问题：伙伴系统最小分配4KB，但内核经常需要分配几十/几百字节的对象    |
|       (如task_struct ~6KB, inode ~400B, dentry ~200B)            |
|                                                                  |
| 解决方案：                                                        |
|                                                                  |
|   +------------+     +------------+     +------------+           |
|   | Buddy      |     | Slab       |     | 内核对象    |           |
|   | (4KB页)    | --> | 分配器     | --> | 小块分配    |           |
|   +------------+     +------------+     +------------+           |
|                                                                  |
| 优势：                                                            |
| 1. 减少内部碎片                                                   |
| 2. 对象缓存复用（避免频繁构造/析构）                               |
| 3. 硬件缓存着色（提高CPU缓存命中率）                               |
| 4. per-CPU缓存（减少锁竞争）                                       |
+------------------------------------------------------------------+
```
[Basic]

---

Q: What are the three slab allocator implementations in Linux?
A: 
| 实现 | 特点 | 使用场景 |
|------|------|----------|
| **SLAB** | 传统实现，功能完整，元数据开销大 | 传统服务器 |
| **SLUB** | 简化设计，性能好，默认选择 | 现代系统（默认） |
| **SLOB** | 极简实现，内存效率高 | 嵌入式系统 |

```
编译配置选择：
CONFIG_SLAB  - 传统SLAB
CONFIG_SLUB  - SLUB (默认)
CONFIG_SLOB  - SLOB

SLUB组织结构：
+-------------------+
|   kmem_cache      |  缓存描述符
+-------------------+
         |
         v
+-------------------+
|   Slab (一个或多个页)|
|   +-----------+    |
|   | object    |    |
|   +-----------+    |
|   | object    |    |
|   +-----------+    |
|   | object    |    |
|   +-----------+    |
|   | freelist  |    |  空闲对象链表
+-------------------+
```
[Intermediate]

---

Q: What is `struct kmem_cache` and how to create one?
A: `kmem_cache`是对象缓存描述符：
```c
struct kmem_cache {
    struct kmem_cache_cpu __percpu *cpu_slab;  // per-CPU缓存
    unsigned int size;           // 对象大小(含对齐)
    unsigned int object_size;    // 实际对象大小
    unsigned int offset;         // 空闲指针偏移
    struct kmem_cache_node *node[MAX_NUMNODES]; // per-node数据
    const char *name;            // 缓存名称
    // ...
};

/* 创建专用缓存 */
struct kmem_cache *my_cache;

// 模块初始化
my_cache = kmem_cache_create(
    "my_objects",        // 名称（/proc/slabinfo中显示）
    sizeof(struct my_obj), // 对象大小
    0,                    // 对齐（0=默认）
    SLAB_HWCACHE_ALIGN,  // 标志
    NULL                  // 构造函数（可选）
);
if (!my_cache)
    return -ENOMEM;

/* 分配对象 */
struct my_obj *obj = kmem_cache_alloc(my_cache, GFP_KERNEL);

/* 释放对象 */
kmem_cache_free(my_cache, obj);

/* 销毁缓存（模块退出时） */
kmem_cache_destroy(my_cache);
```
[Intermediate]

---

Q: What are kmalloc and its variants?
A: `kmalloc`是通用小内存分配接口：
```c
/* 基本分配 */
void *ptr = kmalloc(size, GFP_KERNEL);

/* 分配并清零 */
void *ptr = kzalloc(size, GFP_KERNEL);

/* 分配数组 */
void *ptr = kmalloc_array(n, size, GFP_KERNEL);
void *ptr = kcalloc(n, size, GFP_KERNEL);  // 清零版本

/* 释放 */
kfree(ptr);

/* 重新分配 */
void *new_ptr = krealloc(ptr, new_size, GFP_KERNEL);

/* 复制字符串 */
char *new_str = kstrdup(str, GFP_KERNEL);
char *new_str = kstrndup(str, len, GFP_KERNEL);

/* 复制内存 */
void *copy = kmemdup(src, size, GFP_KERNEL);
```

**kmalloc大小限制**：
- 最大通常为128KB（KMALLOC_MAX_SIZE）
- 使用2的幂次缓存（32, 64, 128, 256...字节）
- 大于限制用`vmalloc()`或`alloc_pages()`
[Basic]

---

Q: What is the difference between kmalloc and vmalloc?
A: 
| 特性 | kmalloc | vmalloc |
|------|---------|---------|
| 物理连续 | 是 | 否 |
| 虚拟连续 | 是 | 是 |
| 最大大小 | ~128KB | 可达GB级别 |
| 性能 | 快 | 慢（需建立页表） |
| 用途 | 小块、DMA、性能敏感 | 大块、非DMA |

```c
// kmalloc: 物理连续，适合DMA
void *buf = kmalloc(4096, GFP_KERNEL);
dma_addr_t dma = virt_to_phys(buf);  // 可以获取物理地址

// vmalloc: 虚拟连续，大内存
void *buf = vmalloc(1024 * 1024);  // 1MB
// 不能直接用于DMA！

// 释放
kfree(buf);
vfree(buf);

/* 相关变体 */
void *vzalloc(size);              // vmalloc + 清零
void *vmalloc_user(size);         // 可映射到用户空间
void *vmalloc_32(size);           // 32位地址范围
```

**选择原则**：
- 小于一页 → kmalloc
- 需要物理连续/DMA → kmalloc或alloc_pages
- 大内存、不需物理连续 → vmalloc
[Intermediate]

---

Q: What is kvmalloc and when to use it?
A: `kvmalloc`自动选择kmalloc或vmalloc：
```c
void *kvmalloc(size_t size, gfp_t flags);
void *kvzalloc(size_t size, gfp_t flags);
void kvfree(void *ptr);

// 实现逻辑：
void *kvmalloc(size_t size, gfp_t flags)
{
    // 1. 先尝试kmalloc
    void *ret = kmalloc(size, flags | __GFP_NOWARN);
    if (ret || size <= PAGE_SIZE)
        return ret;
    
    // 2. kmalloc失败且size > PAGE_SIZE，使用vmalloc
    return vmalloc(size);
}
```

**使用场景**：
- 不确定大小时
- 可能需要大内存但通常是小内存
- 不需要物理连续
[Intermediate]

---

## 5. Virtual Memory (虚拟内存)

---

Q: What is `struct mm_struct` and what does it represent?
A: `mm_struct`描述进程的完整地址空间：
```c
struct mm_struct {
    struct vm_area_struct *mmap;      // VMA链表
    struct rb_root mm_rb;              // VMA红黑树（快速查找）
    
    pgd_t *pgd;                        // 页全局目录（顶级页表）
    
    atomic_t mm_users;                 // 使用者计数（线程）
    atomic_t mm_count;                 // 引用计数
    
    unsigned long start_code, end_code;   // 代码段
    unsigned long start_data, end_data;   // 数据段
    unsigned long start_brk, brk;         // 堆
    unsigned long start_stack;            // 栈
    unsigned long arg_start, arg_end;     // 参数
    unsigned long env_start, env_end;     // 环境变量
    
    unsigned long total_vm;            // 总虚拟内存页数
    unsigned long locked_vm;           // 锁定页数
    unsigned long pinned_vm;           // 固定页数
    
    spinlock_t page_table_lock;
    struct rw_semaphore mmap_sem;      // 保护VMA操作
    // ...
};

// 获取当前进程的mm
struct mm_struct *mm = current->mm;
```
[Intermediate]

---

Q: What is `struct vm_area_struct` (VMA)?
A: VMA描述进程地址空间中的一个连续区域：
```c
struct vm_area_struct {
    unsigned long vm_start;           // 起始虚拟地址
    unsigned long vm_end;             // 结束虚拟地址
    
    struct vm_area_struct *vm_next;   // 链表（按地址排序）
    struct rb_node vm_rb;             // 红黑树节点
    
    pgprot_t vm_page_prot;            // 页保护属性
    unsigned long vm_flags;           // 标志 (VM_READ等)
    
    struct mm_struct *vm_mm;          // 所属mm_struct
    
    struct file *vm_file;             // 映射的文件（mmap）
    unsigned long vm_pgoff;           // 文件偏移（页为单位）
    
    const struct vm_operations_struct *vm_ops;  // VMA操作
    void *vm_private_data;            // 私有数据
};

// VMA标志
#define VM_READ         0x00000001
#define VM_WRITE        0x00000002
#define VM_EXEC         0x00000004
#define VM_SHARED       0x00000008    // 共享映射
#define VM_GROWSDOWN    0x00000100    // 栈：向下增长
#define VM_LOCKED       0x00002000    // 锁定在内存
#define VM_IO           0x00004000    // 设备I/O映射
```

```
进程地址空间：
+------------------+ 0xFFFFFFFF
|  Kernel Space    |
+------------------+ 0xC0000000 (32位)
|  Stack [VMA]     | ↓ grows down
+------------------+
|       ...        |
+------------------+
|  mmap [VMA]      | 文件映射/匿名映射
+------------------+
|       ...        |
+------------------+
|  Heap [VMA]      | ↑ grows up (brk)
+------------------+
|  BSS [VMA]       | 未初始化数据
+------------------+
|  Data [VMA]      | 已初始化数据
+------------------+
|  Text [VMA]      | 代码段
+------------------+ 0x08048000
```
[Intermediate]

---

Q: What are page tables and how is virtual address translated?
A: 页表将虚拟地址转换为物理地址：

**x86_64四级页表**：
```
虚拟地址 (48位有效):
+-------+-------+-------+-------+------------+
| PGD   | PUD   | PMD   | PTE   |   Offset   |
| 9 bits| 9 bits| 9 bits| 9 bits|  12 bits   |
+-------+-------+-------+-------+------------+
   |       |       |       |          |
   v       v       v       v          v
+-----+ +-----+ +-----+ +-----+  +--------+
| PGD |→| PUD |→| PMD |→| PTE |→|Physical|
|Table| |Table| |Table| |Table|  |  Page  |
+-----+ +-----+ +-----+ +-----+  +--------+
   ↓       ↓       ↓       ↓
  512    512     512     512 entries per table
entries entries entries entries

PTE内容：
+------------------+-----+-+-+-+-+-+-+-+
|  Physical PFN    |     |D|A|U|W|P|   |
|  (page frame #)  | ... |i|c|s|r|r|   |
+------------------+-----+-+-+-+-+-+-+-+
                         |D|A|U|W|P|
                         ir c e ri re
                         ty se r/ t s
                            d  s  e e
                               u   n
                               p   t
```

内核函数：
```c
pgd_t *pgd = pgd_offset(mm, addr);
pud_t *pud = pud_offset(pgd, addr);
pmd_t *pmd = pmd_offset(pud, addr);
pte_t *pte = pte_offset_map(pmd, addr);

// 获取物理页帧
unsigned long pfn = pte_pfn(*pte);
struct page *page = pfn_to_page(pfn);
```
[Advanced]

---

Q: What is a page fault and what types are there?
A: 页错误是CPU访问虚拟地址时MMU无法完成转换触发的异常：

| 类型 | 原因 | 处理 |
|------|------|------|
| **Minor Fault** | 页在内存但无PTE | 建立页表映射 |
| **Major Fault** | 页不在内存（需从磁盘加载） | 读取swap/文件 |
| **Invalid Fault** | 非法访问（越界/权限错误） | SIGSEGV |

```c
// 页错误处理流程
do_page_fault()
    |
    +---> handle_mm_fault()
              |
              +---> handle_pte_fault()
                        |
                        +---> 匿名页: do_anonymous_page()
                        |              分配新页并清零
                        |
                        +---> 文件映射: do_fault()
                        |              读取文件内容
                        |
                        +---> 写时复制: do_wp_page()
                        |              复制页面
                        |
                        +---> swap: do_swap_page()
                                     从swap读取

// 统计
cat /proc/vmstat | grep pgfault
cat /proc/[pid]/stat  # 第10/12列：minflt/majflt
```
[Intermediate]

---

Q: What is Copy-on-Write (COW)?
A: 写时复制是fork()的优化技术：
```
fork()前：
Parent Process
+------------+
|   Page A   |  R/W
+------------+

fork()后（未写入）：
Parent Process        Child Process
+------------+        +------------+
|   Page A   | R-only |   Page A   | R-only
+------------+        +------------+
      |                     |
      +----------+----------+
                 |
           [共享同一物理页]

写入时（COW触发）：
Parent writes to Page A:
1. 触发缺页异常（页面只读）
2. 分配新页，复制内容
3. 更新父进程页表指向新页
4. 新页设为R/W

Parent Process        Child Process
+------------+        +------------+
|   Page A'  | R/W    |   Page A   | R/W (如果只有一个引用)
+------------+        +------------+
      |                     |
   [新页]              [原页]
```

内核实现：
```c
// 处理COW
static int do_wp_page(struct vm_fault *vmf)
{
    struct page *old_page = vmf->page;
    
    // 检查是否可以复用（只有一个映射）
    if (page_mapcount(old_page) == 1) {
        // 直接设为可写
        pte = pte_mkwrite(pte);
        return 0;
    }
    
    // 需要复制
    new_page = alloc_page(GFP_KERNEL);
    copy_user_highpage(new_page, old_page, addr, vma);
    
    // 更新页表
    set_pte_at(mm, addr, vmf->pte, mk_pte(new_page, vma->vm_page_prot));
    
    return 0;
}
```
[Intermediate]

---

## 6. Memory Mapping (内存映射)

---

Q: What is mmap and what are its types?
A: mmap将文件或匿名内存映射到进程地址空间：

| 类型 | 创建方式 | 用途 |
|------|----------|------|
| **文件映射** | `mmap(fd, ...)` | 读写文件、共享库 |
| **匿名映射** | `mmap(MAP_ANONYMOUS)` | malloc大块分配 |
| **私有映射** | `MAP_PRIVATE` | 写时复制 |
| **共享映射** | `MAP_SHARED` | 进程间共享、文件写回 |

```c
// 用户空间
void *addr = mmap(
    NULL,           // 让内核选择地址
    length,         // 映射长度
    PROT_READ | PROT_WRITE,  // 权限
    MAP_SHARED,     // 标志
    fd,             // 文件描述符
    0               // 文件偏移
);

// 内核实现路径
sys_mmap() → do_mmap() → mmap_region() → 创建VMA
                              |
                              v
                      vma->vm_file = file
                      vma->vm_ops = file->f_op->mmap提供
```

组合效果：
| 类型 | 写操作 | 可见性 |
|------|--------|--------|
| 私有文件映射 | COW，不写回文件 | 仅本进程 |
| 共享文件映射 | 写回文件 | 所有映射者可见 |
| 私有匿名映射 | COW | 仅本进程 |
| 共享匿名映射 | 直接修改 | 相关进程可见 |
[Intermediate]

---

Q: How to implement mmap in a device driver?
A: 
```c
static int mydev_mmap(struct file *filp, struct vm_area_struct *vma)
{
    struct mydev_data *dev = filp->private_data;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    
    // 1. 检查请求大小
    if (size > BUFFER_SIZE)
        return -EINVAL;
    
    // 2. 检查权限
    if (vma->vm_flags & VM_WRITE && !(filp->f_mode & FMODE_WRITE))
        return -EACCES;
    
    // 3. 设置页面属性
    // 对于设备内存，禁用缓存
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    
    // 方法A：映射一次性分配的物理内存
    pfn = virt_to_phys(dev->buffer) >> PAGE_SHIFT;
    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot))
        return -EAGAIN;
    
    // 方法B：使用fault处理程序按需映射
    // vma->vm_ops = &mydev_vm_ops;
    // vma->vm_private_data = dev;
    
    // 4. 设置标志
    vma->vm_flags |= VM_IO;  // 标记为I/O内存
    
    return 0;
}

// fault处理程序（按需分配）
static int mydev_fault(struct vm_fault *vmf)
{
    struct vm_area_struct *vma = vmf->vma;
    struct mydev_data *dev = vma->vm_private_data;
    struct page *page;
    
    unsigned long offset = vmf->address - vma->vm_start;
    
    // 获取或分配页面
    page = virt_to_page(dev->buffer + offset);
    get_page(page);
    
    vmf->page = page;
    return 0;
}

static const struct vm_operations_struct mydev_vm_ops = {
    .fault = mydev_fault,
};
```
[Advanced]

---

## 7. Page Cache and Writeback (页面缓存与回写)

---

Q: What is the page cache?
A: 页面缓存是内核用于缓存文件数据的内存区域：
```
+-------------------+
|   User Process    |
|   read()/write()  |
+--------+----------+
         |
         v
+--------+----------+     Cache Miss
|    Page Cache     | ←---------------→ Disk
|  (struct page)    |     Cache Hit
+-------------------+       ↑
                            |
              writeback (pdflush/kworker)

// 查看页面缓存使用
cat /proc/meminfo | grep -E "Cached|Buffers"
echo 3 > /proc/sys/vm/drop_caches  // 清除缓存
```

核心数据结构：
```c
struct address_space {
    struct inode *host;              // 所属inode
    struct radix_tree_root page_tree; // 页面基数树
    spinlock_t tree_lock;
    unsigned long nrpages;           // 缓存页数
    const struct address_space_operations *a_ops;
    // ...
};

// 查找页面
struct page *find_get_page(struct address_space *mapping, pgoff_t offset);

// 添加页面
int add_to_page_cache_lru(struct page *page, struct address_space *mapping,
                          pgoff_t offset, gfp_t gfp_mask);
```
[Intermediate]

---

Q: What is the LRU (Least Recently Used) list?
A: LRU列表管理可回收页面：
```c
// 每个zone维护LRU列表
enum lru_list {
    LRU_INACTIVE_ANON,   // 非活动匿名页
    LRU_ACTIVE_ANON,     // 活动匿名页
    LRU_INACTIVE_FILE,   // 非活动文件页
    LRU_ACTIVE_FILE,     // 活动文件页
    LRU_UNEVICTABLE,     // 不可回收页
    NR_LRU_LISTS
};

// 页面在LRU间移动
访问页面 → Active列表
长期未访问 → Inactive列表
回收目标 → Inactive列表尾部

```

```
            访问
              ↓
+-------------------+        +-------------------+
|   Active List     | <----- |  Inactive List    |
|   (活跃页)         |        |  (不活跃页)        |
+-------------------+        +-------------------+
         |                           |
         | 老化                       | 回收
         +-------------------------->|
                                     v
                              swap/丢弃(clean)
```

相关函数：
```c
void mark_page_accessed(struct page *page);  // 标记访问
void activate_page(struct page *page);       // 移到活跃列表
void deactivate_page(struct page *page);     // 移到不活跃列表
```
[Intermediate]

---

Q: What triggers page reclaim?
A: 页面回收在以下情况触发：

```
+------------------------------------------------------------------+
|                    Page Reclaim Triggers                          |
+------------------------------------------------------------------+
|                                                                  |
|  1. 直接回收 (Direct Reclaim)                                     |
|     - 分配内存时内存不足                                           |
|     - 同步执行，可能阻塞                                           |
|     alloc_pages() → __alloc_pages_slowpath() → try_to_free_pages()|
|                                                                  |
|  2. kswapd后台回收                                                |
|     - 内存低于low水位线时唤醒                                      |
|     - 异步执行，尝试恢复到high水位线                               |
|     wakeup_kswapd() → balance_pgdat()                            |
|                                                                  |
|  3. OOM Killer                                                   |
|     - 内存严重不足，回收失败                                       |
|     - 选择并杀死进程释放内存                                       |
|     out_of_memory() → oom_kill_process()                         |
|                                                                  |
+------------------------------------------------------------------+

水位线：
  +---------+
  |         |  high watermark - kswapd停止
  +---------+
  |         |  low watermark  - 唤醒kswapd
  +---------+
  |         |  min watermark  - 直接回收阈值
  +---------+
```

查看水位线：
```bash
cat /proc/zoneinfo | grep -E "min|low|high"
```
[Advanced]

---

## 8. Memory Barriers (内存屏障)

---

Q: What are memory barriers and why are they needed?
A: 内存屏障确保内存操作的顺序性：

**问题**：
1. 编译器可能重排指令
2. CPU可能乱序执行
3. 多核缓存不一致

```c
// 没有屏障，可能出问题
int ready = 0;
int data = 0;

// CPU 0                    // CPU 1
data = 42;                  while (!ready);
ready = 1;                  use(data);  // 可能读到0！

// 使用屏障
data = 42;
smp_wmb();  // 写屏障：确保data先于ready写入
ready = 1;

                            while (!ready);
                            smp_rmb();  // 读屏障：确保ready先于data读取
                            use(data);  // 确保读到42
```
[Intermediate]

---

Q: What are the kernel memory barrier functions?
A: 
```c
/* 编译器屏障 */
barrier();          // 防止编译器重排，不影响CPU

/* 通用内存屏障 */
mb();               // 全屏障：读写都不能跨越
rmb();              // 读屏障：读操作不能跨越
wmb();              // 写屏障：写操作不能跨越

/* SMP内存屏障（多核） */
smp_mb();           // SMP全屏障
smp_rmb();          // SMP读屏障
smp_wmb();          // SMP写屏障

/* 数据依赖屏障 */
smp_read_barrier_depends();  // 数据依赖读屏障

/* 带屏障的原子操作 */
smp_mb__before_atomic();
smp_mb__after_atomic();

/* 使用示例 */
// 生产者
WRITE_ONCE(data, value);   // 防止撕裂写
smp_wmb();                  // 确保data先于flag写入
WRITE_ONCE(flag, 1);

// 消费者
while (!READ_ONCE(flag))    // 防止撕裂读
    cpu_relax();
smp_rmb();                  // 确保flag先于data读取
use(READ_ONCE(data));
```
[Advanced]

---

Q: When to use WRITE_ONCE and READ_ONCE?
A: `WRITE_ONCE`和`READ_ONCE`确保原子性访问，防止编译器优化导致的问题：
```c
// 问题1：编译器可能拆分访问
int x;
x = 0x12345678;  // 可能拆成多次写

// 问题2：编译器可能合并/消除访问
while (flag)     // 可能只读一次
    do_something();

// 问题3：编译器可能自创访问
if (condition)
    x = a;
else
    x = b;
// 可能优化为: x = b; if (condition) x = a;  // 多了一次写

/* 使用 WRITE_ONCE/READ_ONCE */
WRITE_ONCE(x, 0x12345678);  // 保证一次完整写入

while (READ_ONCE(flag))     // 每次循环都读取
    do_something();

// 适用场景：
// 1. 多线程/多核共享变量
// 2. 与中断处理程序共享的变量
// 3. 无锁数据结构
```
[Advanced]

---

## 9. Memory Debugging (内存调试)

---

Q: What is KASAN (Kernel Address Sanitizer)?
A: KASAN是内核内存错误检测工具：
```
检测的错误类型：
+------------------+
| Use-after-free   |  释放后使用
| Out-of-bounds    |  越界访问
| Use-after-scope  |  作用域外使用
| Double-free      |  重复释放
+------------------+

启用方式（编译时）：
CONFIG_KASAN=y
CONFIG_KASAN_GENERIC=y  或 CONFIG_KASAN_SW_TAGS=y

KASAN输出示例：
==================================================================
BUG: KASAN: use-after-free in my_function+0x42/0x100
Read of size 4 at addr ffff888012345678 by task test/1234

Call Trace:
 my_function+0x42/0x100
 test_module_init+0x10/0x30
 ...

Allocated by task 1234:
 kmalloc+0x10/0x20
 my_alloc_function+0x15/0x30
 ...

Freed by task 1234:
 kfree+0x10/0x20
 my_free_function+0x15/0x30
 ...
==================================================================
```
[Intermediate]

---

Q: What is KMEMLEAK and how to use it?
A: KMEMLEAK检测内核内存泄漏：
```
启用方式：
CONFIG_DEBUG_KMEMLEAK=y

使用方法：
# 触发扫描
echo scan > /sys/kernel/debug/kmemleak

# 查看结果
cat /sys/kernel/debug/kmemleak

# 清除记录
echo clear > /sys/kernel/debug/kmemleak

输出示例：
unreferenced object 0xffff888012345678 (size 128):
  comm "test", pid 1234, jiffies 4294667296
  backtrace:
    kmalloc+0x10/0x20
    my_alloc_function+0x20/0x50
    my_module_init+0x30/0x80
    ...

// 标记误报为非泄漏
kmemleak_not_leak(ptr);

// 标记不跟踪
kmemleak_ignore(ptr);
```
[Intermediate]

---

Q: How to enable slab debugging?
A: 
```
编译选项：
CONFIG_SLUB_DEBUG=y
CONFIG_SLUB_DEBUG_ON=y  # 默认启用

启动参数：
slub_debug=FPUZ         # 启用所有检查
slub_debug=Z,my_cache   # 仅对特定缓存

调试标志：
F - 检测double-free
P - 毒化（使用魔术值填充）
U - 用户跟踪（记录分配调用栈）
Z - 红区（检测越界）

运行时启用/禁用：
echo 1 > /sys/kernel/slab/<cache>/sanity_checks

查看slab信息：
cat /proc/slabinfo
slabtop

调试输出示例：
=============================================================================
BUG my_cache: Redzone overwritten
-----------------------------------------------------------------------------
Object ffff888012345678: 00 00 00 00 00 00 00 00  ........
Redzone ffff888012345698: cc cc cc cc cc cc cc cc  ........
        ^^^^^^^^^^^^^^^^^^^ 被覆盖了
```
[Intermediate]

---

## 10. Memory APIs Summary (内存API总结)

---

Q: Provide a quick reference for kernel memory allocation APIs.
A: 
```c
/*=====================================================
 *                  页分配器
 *=====================================================*/
// 分配连续物理页
struct page *alloc_pages(gfp_t gfp, unsigned int order);
struct page *alloc_page(gfp_t gfp);
unsigned long __get_free_pages(gfp_t gfp, unsigned int order);
unsigned long get_zeroed_page(gfp_t gfp);

// 释放
void __free_pages(struct page *page, unsigned int order);
void free_pages(unsigned long addr, unsigned int order);

/*=====================================================
 *                  Slab分配器
 *=====================================================*/
// 通用分配
void *kmalloc(size_t size, gfp_t flags);
void *kzalloc(size_t size, gfp_t flags);
void *kcalloc(size_t n, size_t size, gfp_t flags);
void kfree(void *ptr);

// 专用缓存
struct kmem_cache *kmem_cache_create(name, size, align, flags, ctor);
void *kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags);
void kmem_cache_free(struct kmem_cache *cache, void *obj);
void kmem_cache_destroy(struct kmem_cache *cache);

/*=====================================================
 *                  vmalloc
 *=====================================================*/
void *vmalloc(unsigned long size);
void *vzalloc(unsigned long size);
void vfree(void *addr);

// 自动选择
void *kvmalloc(size_t size, gfp_t flags);
void kvfree(void *ptr);

/*=====================================================
 *                  Per-CPU分配
 *=====================================================*/
DEFINE_PER_CPU(type, name);
type *alloc_percpu(type);
void free_percpu(void *ptr);
per_cpu(var, cpu);
get_cpu_var(var);
put_cpu_var(var);

/*=====================================================
 *                  高端内存映射
 *=====================================================*/
void *kmap(struct page *page);        // 可能睡眠
void kunmap(struct page *page);
void *kmap_atomic(struct page *page); // 不睡眠
void kunmap_atomic(void *addr);
```
[Basic]

---

Q: How to choose the right memory allocation API?
A: 
```
需要分配内存？
    |
    +---> 大小 > 页面大小？
    |         |
    |         +---> 需要物理连续？
    |         |         |
    |         |         +---> 是：alloc_pages()
    |         |         |
    |         |         +---> 否：vmalloc() / kvmalloc()
    |         |
    |         +---> 大小 <= 几页：kmalloc() 可能成功
    |
    +---> 大小 <= 页面大小
              |
              +---> 频繁分配相同大小？
              |         |
              |         +---> 是：kmem_cache_create()
              |         |
              |         +---> 否：kmalloc()
              |
              +---> 特殊要求？
                        |
                        +---> DMA：GFP_DMA
                        +---> 中断上下文：GFP_ATOMIC
                        +---> 需要清零：kzalloc()
```

| 场景 | 推荐API |
|------|---------|
| 小于一页，通用 | `kmalloc()` |
| 小于一页，频繁 | `kmem_cache_*` |
| 大内存，不需物理连续 | `vmalloc()` |
| 大内存，可能小可能大 | `kvmalloc()` |
| 需要物理连续 | `alloc_pages()` |
| DMA缓冲区 | `dma_alloc_coherent()` |
| Per-CPU数据 | `alloc_percpu()` |
[Basic]

---

## 11. Per-CPU Variables (Per-CPU变量)

---

Q: What are per-CPU variables and why use them?
A: Per-CPU变量为每个CPU维护独立副本，避免锁竞争：
```c
/* 静态定义 */
DEFINE_PER_CPU(int, my_counter);

/* 动态分配 */
int __percpu *counter = alloc_percpu(int);

/* 访问 */
// 需要禁止抢占
int cpu = get_cpu();            // 禁止抢占，获取当前CPU
per_cpu(my_counter, cpu)++;     // 访问指定CPU的变量
put_cpu();                       // 允许抢占

// 或使用this_cpu系列（自动禁止抢占）
this_cpu_inc(my_counter);       // 增加当前CPU的计数器
this_cpu_read(my_counter);      // 读取
this_cpu_write(my_counter, 0);  // 写入

/* 遍历所有CPU */
int cpu, total = 0;
for_each_online_cpu(cpu) {
    total += per_cpu(my_counter, cpu);
}

/* 释放 */
free_percpu(counter);
```

优势：
- 无锁访问（每CPU独立）
- 缓存友好（数据局部性）
- 适合统计计数器、缓存等
[Intermediate]

---

## 12. NUMA Awareness (NUMA感知)

---

Q: How to write NUMA-aware code?
A: 
```c
/* 获取节点信息 */
int node = numa_node_id();           // 当前CPU所在节点
int node = page_to_nid(page);        // 页面所在节点
int node = cpu_to_node(cpu);         // CPU所在节点

/* NUMA感知内存分配 */
// 指定节点分配
struct page *page = alloc_pages_node(node, gfp, order);
void *ptr = kmalloc_node(size, gfp, node);
void *ptr = vmalloc_node(size, node);

// 分配在当前节点
void *ptr = kmalloc(size, GFP_KERNEL);  // 默认当前节点优先

// kmem_cache在指定节点分配
void *obj = kmem_cache_alloc_node(cache, gfp, node);

/* 内存策略（用户空间影响） */
// 可通过mbind()/set_mempolicy()设置
// 内核中使用numactl或/proc/<pid>/numa_maps查看

/* 最佳实践 */
// 1. 数据靠近使用它的CPU
// 2. 使用per-CPU数据
// 3. 大内存分配时考虑交织(interleave)分布
// 4. 使用cpuset限制进程到特定节点
```
[Advanced]

---

## 13. Memory Hotplug (内存热插拔)

---

Q: What is memory hotplug and how does it work?
A: 内存热插拔允许运行时添加/移除物理内存：
```
添加内存：
1. 硬件检测到新内存
2. ACPI通知内核
3. 内核初始化页帧
4. 添加到zone和伙伴系统

移除内存：
1. 迁移内存中的页面到其他区域
2. 隔离内存块
3. 通知硬件可以移除

相关配置：
CONFIG_MEMORY_HOTPLUG=y
CONFIG_MEMORY_HOTREMOVE=y

查看状态：
ls /sys/devices/system/memory/
cat /sys/devices/system/memory/memory0/state

操作：
# 下线内存块
echo offline > /sys/devices/system/memory/memory1/state

# 上线内存块
echo online > /sys/devices/system/memory/memory1/state
```
[Advanced]

---

## 14. Out of Memory (OOM)

---

Q: What is OOM Killer and how does it work?
A: OOM Killer在内存严重不足时选择并杀死进程：
```
触发条件：
1. 直接回收失败
2. kswapd回收失败
3. 无法从任何zone分配内存

选择算法（oom_badness）：
score = (process_pages * 1000) / total_pages
+ 调整因子(oom_score_adj)

高分进程更可能被杀死

调整OOM分数：
# 查看进程OOM分数
cat /proc/<pid>/oom_score

# 调整（-1000到1000，-1000=禁止杀死）
echo -1000 > /proc/<pid>/oom_score_adj

# 查看调整值
cat /proc/<pid>/oom_score_adj
```

内核日志：
```
Out of memory: Kill process 1234 (my_app) score 500 or sacrifice child
Killed process 1234 (my_app) total-vm:1234567kB, anon-rss:987654kB, 
file-rss:1234kB
```

保护关键进程：
```bash
# 禁止OOM杀死
echo -1000 > /proc/$(pidof sshd)/oom_score_adj

# 或系统级别禁用（危险！可能导致系统挂起）
sysctl vm.oom-kill = 0
```
[Intermediate]

---

## 15. Common Mistakes (常见错误)

---

Q: What are common memory management mistakes in kernel code?
A: 
| 错误 | 后果 | 正确做法 |
|------|------|----------|
| 忘记检查返回值 | NULL解引用崩溃 | 始终检查返回值 |
| 错误的GFP标志 | 死锁/分配失败 | 中断用GFP_ATOMIC |
| 内存泄漏 | 系统内存耗尽 | 使用devm_*，成对释放 |
| Double free | 内存损坏 | 释放后置NULL |
| 越界访问 | 数据损坏 | 使用KASAN检测 |
| 使用已释放内存 | 未定义行为 | 使用KASAN检测 |
| vmalloc用于DMA | DMA失败 | 使用kmalloc或dma_alloc |
| 大kmalloc分配 | 分配失败 | 使用vmalloc/kvmalloc |
| 未对齐访问 | 性能差/异常 | 使用对齐的分配 |
[Basic]

---

Q: How to avoid common memory errors?
A: 
```c
/* 1. 始终检查分配结果 */
ptr = kmalloc(size, GFP_KERNEL);
if (!ptr)
    return -ENOMEM;

/* 2. 使用正确的GFP标志 */
// 进程上下文
ptr = kmalloc(size, GFP_KERNEL);
// 中断上下文
ptr = kmalloc(size, GFP_ATOMIC);
// 用户空间请求
ptr = kmalloc(size, GFP_USER);

/* 3. 成对的分配和释放 */
ptr = kmalloc(size, GFP_KERNEL);
// ... 使用 ...
kfree(ptr);
ptr = NULL;  // 防止double free

/* 4. 使用devm_*系列（设备驱动） */
ptr = devm_kzalloc(&pdev->dev, size, GFP_KERNEL);
// 无需手动释放

/* 5. 大内存使用vmalloc */
if (size > PAGE_SIZE * 8)
    ptr = vmalloc(size);  // 或 kvmalloc

/* 6. 启用调试选项 */
// CONFIG_KASAN=y
// CONFIG_KMEMLEAK=y
// CONFIG_SLUB_DEBUG=y

/* 7. 初始化分配的内存 */
ptr = kzalloc(size, GFP_KERNEL);  // 自动清零
// 或
memset(ptr, 0, size);
```
[Basic]

---

*Total: 100+ cards covering Linux kernel memory management implementation*

