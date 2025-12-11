# Virtual Memory and Physical Memory Structures and API Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] Linux内存管理的整体架构是什么？
A: Linux内存管理分为多个层次：
```
+----------------------------------------------------------+
|                    用户空间 (User Space)                  |
+----------------------------------------------------------+
                            |
                            v
+----------------------------------------------------------+
|                  进程地址空间 (mm_struct)                  |
|  +-------+  +-------+  +-------+  +-------+  +-------+   |
|  | stack |  | mmap  |  | heap  |  | bss   |  | text  |   |
|  +-------+  +-------+  +-------+  +-------+  +-------+   |
|              虚拟内存区域 (vm_area_struct)                 |
+----------------------------------------------------------+
                            |
                            v
+----------------------------------------------------------+
|              页表 (Page Tables)                           |
|     PGD -> PUD -> PMD -> PTE -> Physical Page            |
+----------------------------------------------------------+
                            |
                            v
+----------------------------------------------------------+
|              物理内存管理                                  |
|  +------------------+  +------------------+               |
|  | Node (pg_data_t) |  | Node (pg_data_t) |   NUMA节点    |
|  +------------------+  +------------------+               |
|         |                     |                          |
|         v                     v                          |
|  +------+------+------+  +------+------+------+          |
|  | DMA  |Normal|HighMem| | DMA  |Normal|HighMem| Zone    |
|  +------+------+------+  +------+------+------+          |
|              |                                           |
|              v                                           |
|     Buddy System (free_area) -> struct page              |
+----------------------------------------------------------+
```

Q: [Basic] 虚拟地址空间的典型布局是什么？
A: Linux进程的虚拟地址空间布局（x86_64）：
```
用户空间 (0x0 - 0x7fffffffffff, 128TB)
+---------------------------+ 0x7fffffffffff
|         Stack             | <- 向下增长
|            ↓              |
+---------------------------+
|                           |
|      (空闲区域)           |
|                           |
+---------------------------+
|         mmap区域          | <- 动态库、内存映射文件
|            ↑              |
+---------------------------+
|                           |
|      (空闲区域)           |
|                           |
+---------------------------+
|           ↑               |
|         Heap              | <- brk/sbrk扩展
+---------------------------+ <- brk
|          BSS              | <- 未初始化数据
+---------------------------+
|          Data             | <- 已初始化数据
+---------------------------+
|          Text             | <- 代码段(只读)
+---------------------------+ 0x400000 (传统起始地址)
|                           |
+---------------------------+ 0x0 (保护区，不可访问)

内核空间 (0xffff800000000000 - 0xffffffffffffffff)
+---------------------------+ 0xffffffffffffffff
|        模块区             |
+---------------------------+
|        vmalloc区          |
+---------------------------+
|      直接映射区           | <- 线性映射所有物理内存
+---------------------------+ 0xffff880000000000
|                           |
+---------------------------+ 0xffff800000000000

/* 关键宏定义 */
#define TASK_SIZE       (0x00007ffffffff000UL)  // 用户空间大小
#define PAGE_OFFSET     (0xffff880000000000UL)  // 直接映射起始
```

---

## struct mm_struct (进程地址空间)

Q: [Intermediate] struct mm_struct的结构是什么？
A: mm_struct描述进程的整个地址空间：
```c
/* include/linux/mm_types.h */
struct mm_struct {
    /* VMA管理 */
    struct vm_area_struct *mmap;      // VMA链表头
    struct rb_root mm_rb;             // VMA红黑树（快速查找）
    struct vm_area_struct *mmap_cache; // 最近find_vma的结果

    /* 地址空间布局 */
    unsigned long (*get_unmapped_area)(...);  // 查找空闲区域
    unsigned long mmap_base;          // mmap区域基地址
    unsigned long task_size;          // 用户空间大小
    unsigned long free_area_cache;    // 空闲区域缓存

    /* 页表 */
    pgd_t *pgd;                       // 页全局目录

    /* 引用计数 */
    atomic_t mm_users;                // 用户数（线程共享）
    atomic_t mm_count;                // 引用数（包括内核引用）
    int map_count;                    // VMA数量

    /* 锁 */
    spinlock_t page_table_lock;       // 页表锁
    struct rw_semaphore mmap_sem;     // mmap信号量

    /* 内存统计 */
    unsigned long total_vm;           // 总映射页数
    unsigned long locked_vm;          // 锁定页数
    unsigned long shared_vm;          // 共享页数
    unsigned long exec_vm;            // 可执行页数
    unsigned long stack_vm;           // 栈页数

    /* 段地址 */
    unsigned long start_code, end_code;   // 代码段
    unsigned long start_data, end_data;   // 数据段
    unsigned long start_brk, brk;         // 堆
    unsigned long start_stack;            // 栈起始
    unsigned long arg_start, arg_end;     // 命令行参数
    unsigned long env_start, env_end;     // 环境变量

    /* RSS统计 */
    struct mm_rss_stat rss_stat;      // 内存使用统计
};

/* mm_users vs mm_count */
// mm_users: 共享此mm的用户空间上下文数
//           线程共享mm时增加，所有线程退出时归零
// mm_count: 总引用数，mm_users算作1
//           内核临时引用时增加
//           归零时mm_struct被释放
```

Q: [Intermediate] 如何获取和操作mm_struct？
A: mm_struct的获取和引用管理：
```c
/* 获取当前进程的mm */
struct mm_struct *mm = current->mm;

/* 获取并增加引用 */
struct mm_struct *get_task_mm(struct task_struct *task);
void mmput(struct mm_struct *mm);  // 释放引用

/* 内核线程的mm */
// 内核线程没有用户地址空间
task->mm == NULL      // 没有mm
task->active_mm       // 借用的mm（懒惰TLB）

/* 切换mm（上下文切换时）*/
void switch_mm(struct mm_struct *prev, struct mm_struct *next,
               struct task_struct *tsk);

/* 示例：访问另一个进程的mm */
struct task_struct *task = find_task_by_pid(pid);
struct mm_struct *mm = get_task_mm(task);
if (mm) {
    down_read(&mm->mmap_sem);  // 获取读锁
    
    /* 遍历VMA */
    struct vm_area_struct *vma;
    for (vma = mm->mmap; vma; vma = vma->vm_next) {
        pr_info("VMA: %lx-%lx\n", vma->vm_start, vma->vm_end);
    }
    
    up_read(&mm->mmap_sem);
    mmput(mm);  // 释放引用
}
```

---

## struct vm_area_struct (虚拟内存区域)

Q: [Intermediate] struct vm_area_struct的结构是什么？
A: VMA描述一个连续的虚拟内存区域：
```c
/* include/linux/mm_types.h */
struct vm_area_struct {
    /* 所属mm和地址范围 */
    struct mm_struct *vm_mm;          // 所属地址空间
    unsigned long vm_start;           // 起始地址(包含)
    unsigned long vm_end;             // 结束地址(不包含)

    /* 链表和红黑树 */
    struct vm_area_struct *vm_next, *vm_prev;  // VMA链表
    struct rb_node vm_rb;             // 红黑树节点

    /* 权限和标志 */
    pgprot_t vm_page_prot;            // 页保护位
    unsigned long vm_flags;           // VMA标志

    /* 文件映射 */
    unsigned long vm_pgoff;           // 文件偏移（页单位）
    struct file *vm_file;             // 映射的文件(可为NULL)
    void *vm_private_data;            // 私有数据

    /* 匿名映射 */
    struct list_head anon_vma_chain;  // anon_vma链表
    struct anon_vma *anon_vma;        // 反向映射

    /* 操作函数 */
    const struct vm_operations_struct *vm_ops;
};

/* vm_flags常用标志 */
#define VM_READ         0x00000001    // 可读
#define VM_WRITE        0x00000002    // 可写
#define VM_EXEC         0x00000004    // 可执行
#define VM_SHARED       0x00000008    // 共享映射
#define VM_MAYREAD      0x00000010    // 可能变为可读
#define VM_MAYWRITE     0x00000020    // 可能变为可写
#define VM_MAYEXEC      0x00000040    // 可能变为可执行
#define VM_GROWSDOWN    0x00000100    // 向下增长(栈)
#define VM_GROWSUP      0x00000200    // 向上增长
#define VM_PFNMAP       0x00000400    // 页帧号映射
#define VM_DENYWRITE    0x00000800    // 禁止写文件
#define VM_LOCKED       0x00002000    // 锁定在内存
#define VM_IO           0x00004000    // I/O空间映射
#define VM_SEQ_READ     0x00008000    // 顺序读取
#define VM_RAND_READ    0x00010000    // 随机读取
#define VM_DONTCOPY     0x00020000    // fork时不复制
#define VM_DONTEXPAND   0x00040000    // 不能扩展
#define VM_HUGETLB      0x00400000    // 大页映射
```

Q: [Intermediate] VMA的查找和操作有哪些API？
A: VMA操作的核心函数：
```c
/* 查找包含addr的VMA */
struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr);
// 返回第一个vm_end > addr的VMA
// 注意：addr可能不在返回的VMA范围内！

/* 精确查找 */
struct vm_area_struct *find_vma_exact(struct mm_struct *mm, 
                                       unsigned long addr, 
                                       unsigned long len);

/* 查找并检查交叉 */
struct vm_area_struct *find_vma_intersection(struct mm_struct *mm,
                                              unsigned long start_addr,
                                              unsigned long end_addr);

/* 使用示例 */
down_read(&mm->mmap_sem);

struct vm_area_struct *vma = find_vma(mm, addr);
if (vma && vma->vm_start <= addr) {
    // addr在vma范围内
    pr_info("Found VMA: %lx-%lx\n", vma->vm_start, vma->vm_end);
} else {
    // addr不在任何VMA中（可能是hole）
}

up_read(&mm->mmap_sem);

/* 插入VMA */
int insert_vm_struct(struct mm_struct *mm, struct vm_area_struct *vma);

/* 合并VMA */
struct vm_area_struct *vma_merge(struct mm_struct *mm, ...);

/* 分割VMA */
int split_vma(struct mm_struct *mm, struct vm_area_struct *vma,
              unsigned long addr, int new_below);
```

Q: [Intermediate] vm_operations_struct的作用是什么？
A: vm_ops定义VMA的操作行为：
```c
/* include/linux/mm.h */
struct vm_operations_struct {
    /* 打开/关闭VMA */
    void (*open)(struct vm_area_struct *area);
    void (*close)(struct vm_area_struct *area);
    
    /* 缺页处理 - 最重要的回调 */
    int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);
    
    /* 页变为可写时调用（COW完成后）*/
    int (*page_mkwrite)(struct vm_area_struct *vma, struct vm_fault *vmf);
    
    /* 直接访问进程内存 */
    int (*access)(struct vm_area_struct *vma, unsigned long addr,
                  void *buf, int len, int write);
    
#ifdef CONFIG_NUMA
    /* NUMA策略 */
    int (*set_policy)(struct vm_area_struct *vma, struct mempolicy *new);
    struct mempolicy *(*get_policy)(struct vm_area_struct *vma,
                                    unsigned long addr);
#endif
};

/* 示例：设备驱动的vm_ops */
static int my_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
    struct page *page;
    
    /* 获取或分配物理页 */
    page = alloc_page(GFP_KERNEL);
    if (!page)
        return VM_FAULT_OOM;
    
    /* 填充页内容 */
    clear_highpage(page);
    
    /* 返回页给VM */
    vmf->page = page;
    return 0;
}

static struct vm_operations_struct my_vm_ops = {
    .fault = my_fault,
};

/* 在mmap中设置vm_ops */
static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
    vma->vm_ops = &my_vm_ops;
    return 0;
}
```

---

## 页表结构 (Page Table Structures)

Q: [Intermediate] Linux的4级页表结构是什么？
A: x86_64使用4级页表：
```
虚拟地址 (48位有效)
+--------+--------+--------+--------+--------+
| PGD    | PUD    | PMD    | PTE    | Offset |
| [47:39]| [38:30]| [29:21]| [20:12]| [11:0] |
| 9 bits | 9 bits | 9 bits | 9 bits | 12 bits|
+--------+--------+--------+--------+--------+
    |        |        |        |        |
    v        v        v        v        v
+------+  +------+  +------+  +------+  +------+
|PGD   |->|PUD   |->|PMD   |->|PTE   |->|物理页|
|表项  |  |表项  |  |表项  |  |表项  |  |      |
+------+  +------+  +------+  +------+  +------+
512项     512项     512项     512项     4KB

/* 页表结构定义(x86_64) */
typedef unsigned long   pteval_t;
typedef unsigned long   pmdval_t;
typedef unsigned long   pudval_t;
typedef unsigned long   pgdval_t;

typedef struct { pteval_t pte; } pte_t;
typedef struct { pmdval_t pmd; } pmd_t;
typedef struct { pudval_t pud; } pud_t;
typedef struct { pgdval_t pgd; } pgd_t;

/* 页表项标志位 */
#define _PAGE_PRESENT   (1UL << 0)   // 页存在
#define _PAGE_RW        (1UL << 1)   // 可写
#define _PAGE_USER      (1UL << 2)   // 用户可访问
#define _PAGE_PWT       (1UL << 3)   // 页级写穿
#define _PAGE_PCD       (1UL << 4)   // 页级缓存禁用
#define _PAGE_ACCESSED  (1UL << 5)   // 已访问
#define _PAGE_DIRTY     (1UL << 6)   // 脏页
#define _PAGE_PSE       (1UL << 7)   // 大页(2MB/1GB)
#define _PAGE_GLOBAL    (1UL << 8)   // 全局页
#define _PAGE_NX        (1UL << 63)  // 不可执行
```

Q: [Intermediate] 页表遍历的API有哪些？
A: 内核提供页表遍历宏和函数：
```c
/* 从虚拟地址获取各级页表索引 */
pgd_index(addr)   // PGD索引
pud_index(addr)   // PUD索引
pmd_index(addr)   // PMD索引
pte_index(addr)   // PTE索引

/* 获取页表项 */
pgd_t *pgd = pgd_offset(mm, addr);        // 从mm获取PGD
pud_t *pud = pud_offset(pgd, addr);       // 从PGD获取PUD
pmd_t *pmd = pmd_offset(pud, addr);       // 从PUD获取PMD
pte_t *pte = pte_offset_map(pmd, addr);   // 从PMD获取PTE

/* 检查页表项状态 */
pgd_none(*pgd)    // PGD项为空
pgd_bad(*pgd)     // PGD项无效
pgd_present(*pgd) // PGD项存在

pte_present(*pte) // 页存在
pte_write(*pte)   // 可写
pte_exec(*pte)    // 可执行
pte_dirty(*pte)   // 脏页
pte_young(*pte)   // 最近访问

/* 页表项到页帧号/物理地址 */
unsigned long pfn = pte_pfn(*pte);
struct page *page = pte_page(*pte);

/* 完整的页表遍历示例 */
int walk_page_table(struct mm_struct *mm, unsigned long addr)
{
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    pgd = pgd_offset(mm, addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return -EFAULT;

    pud = pud_offset(pgd, addr);
    if (pud_none(*pud) || pud_bad(*pud))
        return -EFAULT;

    pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return -EFAULT;

    pte = pte_offset_map(pmd, addr);
    if (!pte_present(*pte)) {
        pte_unmap(pte);
        return -EFAULT;
    }

    pr_info("PFN: %lx, Present: %d, Write: %d\n",
            pte_pfn(*pte), pte_present(*pte), pte_write(*pte));

    pte_unmap(pte);
    return 0;
}
```

Q: [Advanced] 如何修改页表项？
A: 页表项修改需要注意TLB刷新：
```c
/* 设置页表项 */
void set_pte(pte_t *ptep, pte_t pte);
void set_pte_at(struct mm_struct *mm, unsigned long addr,
                pte_t *ptep, pte_t pte);

/* 修改页表项属性 */
pte_t pte_mkwrite(pte_t pte);     // 设置可写
pte_t pte_wrprotect(pte_t pte);   // 取消可写
pte_t pte_mkdirty(pte_t pte);     // 设置脏
pte_t pte_mkclean(pte_t pte);     // 清除脏
pte_t pte_mkyoung(pte_t pte);     // 设置访问位
pte_t pte_mkold(pte_t pte);       // 清除访问位

/* 分配页表 */
pte_t *pte_alloc_map(struct mm_struct *mm, struct vm_area_struct *vma,
                     pmd_t *pmd, unsigned long address);
int pud_alloc(struct mm_struct *mm, pgd_t *pgd, unsigned long address);
int pmd_alloc(struct mm_struct *mm, pud_t *pud, unsigned long address);

/* TLB刷新 - 非常重要！ */
void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr);
void flush_tlb_range(struct vm_area_struct *vma,
                     unsigned long start, unsigned long end);
void flush_tlb_mm(struct mm_struct *mm);
void flush_tlb_all(void);          // 刷新所有CPU的TLB

/* 修改页表的完整示例 */
void set_page_readonly(struct mm_struct *mm, unsigned long addr)
{
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte, old_pte, new_pte;

    spin_lock(&mm->page_table_lock);

    pgd = pgd_offset(mm, addr);
    pud = pud_offset(pgd, addr);
    pmd = pmd_offset(pud, addr);
    pte = pte_offset_map(pmd, addr);

    if (pte_present(*pte)) {
        old_pte = *pte;
        new_pte = pte_wrprotect(old_pte);  // 移除写权限
        set_pte_at(mm, addr, pte, new_pte);
        flush_tlb_page(find_vma(mm, addr), addr);  // 刷新TLB
    }

    pte_unmap(pte);
    spin_unlock(&mm->page_table_lock);
}
```

---

## struct page (物理页)

Q: [Intermediate] struct page的结构是什么？
A: page结构描述一个物理页框：
```c
/* include/linux/mm_types.h */
struct page {
    /* 第一个双字块 */
    unsigned long flags;              // 页标志（PG_locked等）
    struct address_space *mapping;    // 映射信息
                                      // 低位为0：文件映射
                                      // 低位为1：匿名映射(anon_vma)

    /* 第二个双字块 */
    union {
        pgoff_t index;                // 在mapping中的偏移
        void *freelist;               // SLUB第一个空闲对象
    };
    union {
        unsigned long counters;       // 用于cmpxchg_double
        struct {
            union {
                atomic_t _mapcount;   // 映射计数（多少PTE指向此页）
                struct { /* SLUB使用 */ };
            };
            atomic_t _count;          // 引用计数
        };
    };

    /* 第三个双字块 */
    union {
        struct list_head lru;         // LRU链表（活动/非活动）
        struct {                      // SLUB每CPU部分页
            struct page *next;
            int pages;
            int pobjects;
        };
    };

    /* 复合页 */
    union {
        unsigned long private;        // 由映射使用
        struct kmem_cache *slab;      // SLUB所属缓存
        struct page *first_page;      // 复合页的首页
    };
};

/* 重要字段说明 */
// flags: 页状态标志
// _count: 引用计数，0表示空闲
// _mapcount: -1表示未映射，0表示映射一次，>0表示多次映射
// mapping: 指向address_space或anon_vma
// lru: 用于页面回收的LRU链表
```

Q: [Intermediate] 页标志(page flags)有哪些？
A: 页标志定义在include/linux/page-flags.h：
```c
/* 主要页标志 */
PG_locked       // 页被锁定，I/O进行中
PG_error        // I/O错误
PG_referenced   // 最近被访问（用于LRU）
PG_uptodate     // 页内容有效
PG_dirty        // 页被修改
PG_lru          // 在LRU链表中
PG_active       // 在活动链表中
PG_slab         // 被slab分配器使用
PG_reserved     // 保留页，不能换出
PG_private      // private字段有效
PG_writeback    // 正在写回
PG_head         // 复合页的头页
PG_tail         // 复合页的尾页
PG_swapcache    // 在交换缓存中
PG_mappedtodisk // 有磁盘块映射
PG_reclaim      // 即将被回收
PG_swapbacked   // 由交换空间支持
PG_unevictable  // 不可回收
PG_mlocked      // 被mlock锁定

/* 标志操作宏 */
PageLocked(page)      // 检查是否锁定
SetPageLocked(page)   // 设置锁定
ClearPageLocked(page) // 清除锁定
TestSetPageLocked(page) // 测试并设置（原子）

/* 常见操作 */
void lock_page(struct page *page);    // 锁定页（可能睡眠）
int trylock_page(struct page *page);  // 尝试锁定（不睡眠）
void unlock_page(struct page *page);  // 解锁页

/* 示例 */
struct page *page = alloc_page(GFP_KERNEL);
if (page) {
    lock_page(page);
    
    // 页被锁定，可以安全操作
    clear_highpage(page);
    SetPageUptodate(page);
    
    unlock_page(page);
}
```

Q: [Intermediate] page结构的引用计数如何管理？
A: page使用_count管理生命周期：
```c
/* 获取引用 */
void get_page(struct page *page);
// 增加_count

/* 释放引用 */
void put_page(struct page *page);
// 减少_count，降为0时释放页

/* 检查引用计数 */
int page_count(struct page *page);
// 返回_count值

/* 映射计数 vs 引用计数 */
// _count: 内核对页的引用
// _mapcount: 进程页表对页的映射

/* 关系示例 */
struct page *page = alloc_page(GFP_KERNEL);
// _count = 1, _mapcount = -1 (未映射)

remap_pfn_range(...);  // 映射到用户空间
// _count = 2, _mapcount = 0 (映射一次)

fork();  // 子进程共享页表
// _count = 2, _mapcount = 1 (映射两次)

/* 使用示例 */
struct page *page = find_get_page(mapping, index);
if (page) {
    // page已增加引用
    // ... 使用page ...
    put_page(page);  // 释放引用
}

/* get_page_unless_zero - 安全获取可能为0的页 */
int success = get_page_unless_zero(page);
if (!success) {
    // 页已被释放
}
```

---

## 物理内存组织 (Physical Memory Organization)

Q: [Intermediate] pg_data_t(pglist_data)的结构是什么？
A: pg_data_t描述一个NUMA节点的内存：
```c
/* include/linux/mmzone.h */
typedef struct pglist_data {
    /* zone数组 */
    struct zone node_zones[MAX_NR_ZONES];   // 节点的所有zone
    struct zonelist node_zonelists[MAX_ZONELISTS]; // zone分配顺序

    int nr_zones;                           // zone数量

    /* 节点的page数组 */
    struct page *node_mem_map;              // 节点的page数组

    /* 节点范围 */
    unsigned long node_start_pfn;           // 起始页帧号
    unsigned long node_present_pages;       // 实际页数
    unsigned long node_spanned_pages;       // 跨越页数（含空洞）
    
    int node_id;                            // 节点ID

    /* kswapd */
    wait_queue_head_t kswapd_wait;          // kswapd等待队列
    struct task_struct *kswapd;             // kswapd线程
    int kswapd_max_order;                   // kswapd回收的最大order

} pg_data_t;

/* NUMA节点访问 */
#define NODE_DATA(nid)     (node_data[(nid)])  // 获取节点结构
extern struct pglist_data *node_data[];

/* UMA系统只有一个节点 */
extern struct pglist_data contig_page_data;
#define NODE_DATA(nid)     (&contig_page_data)

/* 节点/zone/页帧号转换 */
int page_to_nid(struct page *page);        // 页所属节点
struct zone *page_zone(struct page *page); // 页所属zone
unsigned long page_to_pfn(struct page *page); // 页帧号
struct page *pfn_to_page(unsigned long pfn);  // 帧号到页
```

Q: [Intermediate] struct zone的结构是什么？
A: zone描述一个内存区域：
```c
/* include/linux/mmzone.h */
struct zone {
    /* 水位线 */
    unsigned long watermark[NR_WMARK];  // min/low/high水位
    unsigned long lowmem_reserve[MAX_NR_ZONES]; // 保留内存

    /* NUMA */
    int node;                           // 所属节点

    /* per-CPU页缓存 */
    struct per_cpu_pageset __percpu *pageset;

    /* 伙伴系统 */
    spinlock_t lock;                    // zone锁
    struct free_area free_area[MAX_ORDER]; // 空闲区域数组

    /* 统计 */
    unsigned long spanned_pages;        // 跨越页数
    unsigned long present_pages;        // 实际页数
    unsigned long managed_pages;        // 被管理页数

    /* LRU链表 */
    spinlock_t lru_lock;
    struct lruvec lruvec;               // LRU向量

    /* 页面回收 */
    unsigned long pages_scanned;        // 扫描的页数
    unsigned long flags;                // ZONE_RECLAIM_LOCKED等

    /* zone名称 */
    const char *name;
};

/* zone类型 */
enum zone_type {
    ZONE_DMA,           // ISA DMA用，<16MB
    ZONE_DMA32,         // 32位DMA用，<4GB
    ZONE_NORMAL,        // 正常可直接映射内存
    ZONE_HIGHMEM,       // 高端内存(32位系统)
    ZONE_MOVABLE,       // 可移动页（内存热插拔）
    MAX_NR_ZONES
};

/* 水位线 */
enum zone_watermarks {
    WMARK_MIN,          // 最小水位，触发直接回收
    WMARK_LOW,          // 低水位，唤醒kswapd
    WMARK_HIGH,         // 高水位，kswapd停止
    NR_WMARK
};

/* 访问水位线 */
#define min_wmark_pages(z) (z->watermark[WMARK_MIN])
#define low_wmark_pages(z) (z->watermark[WMARK_LOW])
#define high_wmark_pages(z) (z->watermark[WMARK_HIGH])
```

Q: [Intermediate] Buddy System(伙伴系统)的结构是什么？
A: 伙伴系统管理空闲页：
```c
/* 空闲区域结构 */
struct free_area {
    struct list_head free_list[MIGRATE_TYPES]; // 按迁移类型分类
    unsigned long nr_free;                      // 空闲块数
};

/* MAX_ORDER通常为11，管理2^0到2^10个连续页 */
#define MAX_ORDER 11

/* zone->free_area数组 */
free_area[0]  -> 2^0 = 1页的块
free_area[1]  -> 2^1 = 2页的块
free_area[2]  -> 2^2 = 4页的块
...
free_area[10] -> 2^10 = 1024页(4MB)的块

/* 迁移类型 */
enum migratetype {
    MIGRATE_UNMOVABLE,     // 不可移动（内核分配）
    MIGRATE_RECLAIMABLE,   // 可回收（页缓存）
    MIGRATE_MOVABLE,       // 可移动（用户页）
    MIGRATE_PCPTYPES,      // per-CPU分配器使用的类型数
    MIGRATE_RESERVE,       // 保留
    MIGRATE_ISOLATE,       // 隔离
    MIGRATE_TYPES
};

/* 伙伴系统图示 */
order=0 (1页):  [A] [B] [C] [D] [E] [F] [G] [H]
                 |   |   |   |   |   |   |   |
order=1 (2页):  [A-B]   [C-D]   [E-F]   [G-H]
                  |       |       |       |
order=2 (4页):  [A-B-C-D]       [E-F-G-H]
                    |               |
order=3 (8页):  [A-B-C-D-E-F-G-H]

/* 分配时分割 */
请求1页：
1. 检查order=0，空则检查order=1
2. 从order=1取一个2页块
3. 分割：1页返回，1页放入order=0

/* 释放时合并 */
释放1页：
1. 查找伙伴页（地址XOR）
2. 如果伙伴空闲，合并成2页块
3. 递归检查更高order
```

---

## 页面分配 (Page Allocation)

Q: [Intermediate] 页面分配的核心API有哪些？
A: 内核提供多层次的分配API：
```c
/* 1. 分配页面（返回struct page *）*/
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);
struct page *alloc_page(gfp_t gfp_mask);  // order=0

/* 2. 分配页面（返回虚拟地址）*/
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
unsigned long __get_free_page(gfp_t gfp_mask);
unsigned long get_zeroed_page(gfp_t gfp_mask); // 清零

/* 3. 释放页面 */
void __free_pages(struct page *page, unsigned int order);
void __free_page(struct page *page);
void free_pages(unsigned long addr, unsigned int order);
void free_page(unsigned long addr);

/* 4. NUMA感知分配 */
struct page *alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order);
struct page *alloc_pages_exact_node(int nid, gfp_t gfp_mask, unsigned int order);

/* 5. 分配大块连续内存（可能跨多个order）*/
void *alloc_pages_exact(size_t size, gfp_t gfp_mask);
void free_pages_exact(void *virt, size_t size);

/* 页面地址转换 */
void *page_address(struct page *page);     // page到虚拟地址
struct page *virt_to_page(void *addr);     // 虚拟地址到page
unsigned long virt_to_phys(void *addr);    // 虚拟地址到物理地址
void *phys_to_virt(unsigned long addr);    // 物理地址到虚拟地址
```

Q: [Intermediate] GFP标志有哪些？
A: GFP(Get Free Pages)标志控制分配行为：
```c
/* 区域修饰符 - 指定分配的zone */
__GFP_DMA       // 从ZONE_DMA分配
__GFP_DMA32     // 从ZONE_DMA32分配
__GFP_HIGHMEM   // 可以从ZONE_HIGHMEM分配
__GFP_MOVABLE   // 分配可移动页

/* 行为修饰符 */
__GFP_WAIT      // 可以睡眠
__GFP_HIGH      // 高优先级，可用紧急保留
__GFP_IO        // 可以启动I/O
__GFP_FS        // 可以调用文件系统
__GFP_COLD      // 分配冷页（缓存不友好）
__GFP_NOWARN    // 不打印警告
__GFP_REPEAT    // 重试分配
__GFP_NOFAIL    // 无限重试直到成功
__GFP_NORETRY   // 不重试
__GFP_ZERO      // 清零页面
__GFP_COMP      // 复合页

/* 常用组合 */
#define GFP_ATOMIC      (__GFP_HIGH)
// 原子上下文，不能睡眠

#define GFP_KERNEL      (__GFP_WAIT | __GFP_IO | __GFP_FS)
// 普通内核分配，可以睡眠

#define GFP_USER        (__GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HARDWALL)
// 为用户空间分配

#define GFP_HIGHUSER    (GFP_USER | __GFP_HIGHMEM)
// 用户空间，可以用高端内存

#define GFP_DMA         __GFP_DMA
// DMA内存

/* 使用建议 */
// 进程上下文：GFP_KERNEL
// 中断/软中断：GFP_ATOMIC
// 用户页面：GFP_HIGHUSER_MOVABLE
// DMA缓冲：GFP_DMA | GFP_KERNEL
```

Q: [Intermediate] 分配失败时会发生什么？
A: 内存分配有多级回退机制：
```c
/* 分配流程 */
alloc_pages(gfp, order)
    |
    v
__alloc_pages_nodemask()
    |
    +-> get_page_from_freelist()  // 快速路径
    |       |
    |       +-> zone_watermark_ok()?  // 检查水位
    |       |       |
    |       |       YES -> rmqueue()  // 从伙伴系统分配
    |       |       NO  -> 下一个zone
    |       |
    |       +-> 所有zone失败 -> 慢速路径
    |
    +-> __alloc_pages_slowpath()  // 慢速路径
            |
            +-> wake_all_kswapds()    // 唤醒kswapd
            |
            +-> __alloc_pages_direct_compact()  // 内存压缩
            |
            +-> __alloc_pages_direct_reclaim()  // 直接回收
            |
            +-> __alloc_pages_may_oom()  // OOM killer
            |
            +-> 重试或返回NULL

/* 内存回收层次 */
1. kswapd后台回收（低水位触发）
2. 直接回收（同步，可能阻塞）
3. 内存压缩（合并碎片）
4. OOM Killer（杀死进程释放内存）

/* OOM处理 */
static void out_of_memory(struct zonelist *zonelist, gfp_t gfp_mask,
                          int order, nodemask_t *nodemask,
                          bool force_kill)
{
    select_bad_process();  // 选择牺牲进程
    oom_kill_process();    // 杀死进程
}

/* 进程OOM评分 */
// /proc/<pid>/oom_score      - 当前得分
// /proc/<pid>/oom_score_adj  - 调整值(-1000到1000)
// -1000：禁止被OOM杀死
// +1000：优先被杀死
```

---

## 虚拟内存映射 (Virtual Memory Mapping)

Q: [Intermediate] mmap的内核实现流程是什么？
A: mmap创建虚拟地址到文件/匿名的映射：
```c
/* 系统调用入口 */
SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len,
                unsigned long, prot, unsigned long, flags,
                unsigned long, fd, off_t, off)
{
    return sys_mmap_pgoff(addr, len, prot, flags, fd, off >> PAGE_SHIFT);
}

/* 核心流程 */
do_mmap_pgoff()
    |
    +-> get_unmapped_area()      // 查找空闲虚拟地址
    |
    +-> mmap_region()
            |
            +-> find_vma_prepare()   // 查找插入位置
            |
            +-> vma_merge()          // 尝试与相邻VMA合并
            |
            +-> kmem_cache_alloc()   // 分配VMA结构
            |
            +-> vma_link()           // 链接到mm
            |
            +-> file->f_op->mmap()   // 调用文件的mmap（如果是文件映射）

/* 文件映射 vs 匿名映射 */
/* 文件映射 */
void *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
// vma->vm_file = file
// vma->vm_ops = file的vm_ops
// 缺页时从文件读取

/* 匿名映射 */
void *addr = mmap(NULL, size, PROT_READ|PROT_WRITE, 
                  MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
// vma->vm_file = NULL
// 缺页时分配零页

/* 共享 vs 私有 */
MAP_SHARED:  修改写回文件，其他进程可见
MAP_PRIVATE: COW，修改不写回，不可见于其他进程

/* 内核设备驱动的mmap */
static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
    /* 映射设备内存到用户空间 */
    return remap_pfn_range(vma, vma->vm_start,
                          phys_addr >> PAGE_SHIFT,
                          vma->vm_end - vma->vm_start,
                          vma->vm_page_prot);
}
```

Q: [Advanced] remap_pfn_range的作用是什么？
A: remap_pfn_range将物理页映射到用户空间：
```c
/* 函数原型 */
int remap_pfn_range(struct vm_area_struct *vma,
                    unsigned long addr,           // 虚拟地址
                    unsigned long pfn,            // 物理页帧号
                    unsigned long size,           // 大小
                    pgprot_t prot);               // 保护位

/* 用途 */
1. 设备驱动映射设备内存到用户空间
2. 映射DMA缓冲区
3. 映射帧缓冲区

/* 示例：映射设备内存 */
static int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
    unsigned long phys_addr = my_device->phys_addr;
    unsigned long size = vma->vm_end - vma->vm_start;

    /* 设置为不可缓存（对于设备I/O内存）*/
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    /* 设置VM_IO标志 */
    vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;

    if (remap_pfn_range(vma, vma->vm_start,
                        phys_addr >> PAGE_SHIFT,
                        size, vma->vm_page_prot))
        return -EAGAIN;

    return 0;
}

/* 注意事项 */
// 1. 使用VM_IO标志表示I/O内存
// 2. 使用VM_PFNMAP表示直接PFN映射
// 3. 设备内存通常需要禁用缓存
// 4. 确保物理地址有效且可访问
```

---

## 缺页处理 (Page Fault Handling)

Q: [Advanced] 缺页异常的处理流程是什么？
A: 缺页是虚拟内存的核心机制：
```c
/* x86缺页处理入口 */
do_page_fault(struct pt_regs *regs, unsigned long error_code)
    |
    +-> __do_page_fault(regs, error_code, address)
            |
            +-> find_vma(mm, address)   // 查找VMA
            |       |
            |       +-> vma为NULL或address < vm_start
            |       |   -> bad_area()   // 段错误
            |       |
            |       +-> 检查vm_flags权限
            |           -> bad_area_access_error()  // 权限错误
            |
            +-> handle_mm_fault(mm, vma, address, flags)
                    |
                    +-> __handle_mm_fault()
                            |
                            +-> pgd_alloc/pud_alloc/pmd_alloc
                            |   // 分配页表
                            |
                            +-> handle_pte_fault()
                                    |
                                    +-- pte_none() ?
                                    |       |
                                    |       YES -> do_anonymous_page()
                                    |              或 do_fault()
                                    |
                                    +-- pte_present() == 0 ?
                                    |       |
                                    |       YES -> do_swap_page()
                                    |              // 从交换空间读回
                                    |
                                    +-- 写保护错误 ?
                                            |
                                            YES -> do_wp_page()
                                                   // COW处理

/* 错误码含义(x86) */
#define PF_PROT     (1<<0)  // 0=页不存在 1=保护错误
#define PF_WRITE    (1<<1)  // 0=读 1=写
#define PF_USER     (1<<2)  // 0=内核模式 1=用户模式
#define PF_RSVD     (1<<3)  // 保留位错误
#define PF_INSTR    (1<<4)  // 0=数据 1=指令获取
```

Q: [Advanced] do_anonymous_page处理匿名页缺页？
A: 匿名页缺页分配新页：
```c
static int do_anonymous_page(struct mm_struct *mm,
                             struct vm_area_struct *vma,
                             unsigned long address, pte_t *page_table,
                             pmd_t *pmd, unsigned int flags)
{
    struct page *page;
    pte_t entry;

    /* 只读访问：映射零页（共享） */
    if (!(flags & FAULT_FLAG_WRITE)) {
        entry = pte_mkspecial(pfn_pte(my_zero_pfn(address),
                              vma->vm_page_prot));
        goto setpte;
    }

    /* 分配物理页 */
    page = alloc_zeroed_user_highpage_movable(vma, address);
    if (!page)
        return VM_FAULT_OOM;

    /* 准备匿名反向映射 */
    if (anon_vma_prepare(vma))
        goto oom;

    /* 建立PTE映射 */
    entry = mk_pte(page, vma->vm_page_prot);
    if (vma->vm_flags & VM_WRITE)
        entry = pte_mkwrite(pte_mkdirty(entry));

    /* 设置页表项 */
    set_pte_at(mm, address, page_table, entry);

    /* 添加到反向映射 */
    page_add_new_anon_rmap(page, vma, address);

    /* 更新RSS统计 */
    inc_mm_counter(mm, MM_ANONPAGES);

setpte:
    update_mmu_cache(vma, address, page_table);
    return 0;

oom:
    page_cache_release(page);
    return VM_FAULT_OOM;
}
```

Q: [Advanced] COW(写时复制)是如何实现的？
A: do_wp_page处理写保护错误：
```c
static int do_wp_page(struct mm_struct *mm,
                      struct vm_area_struct *vma, unsigned long address,
                      pte_t *page_table, pmd_t *pmd, spinlock_t *ptl,
                      pte_t orig_pte)
{
    struct page *old_page, *new_page;
    pte_t entry;

    old_page = vm_normal_page(vma, address, orig_pte);

    /* 检查是否是唯一映射 */
    if (page_mapcount(old_page) == 1 && PageAnon(old_page)) {
        /* 唯一映射，直接修改权限 */
        reuse:
        entry = pte_mkyoung(orig_pte);
        entry = pte_mkdirty(entry);
        entry = pte_mkwrite(entry);
        set_pte_at(mm, address, page_table, entry);
        return VM_FAULT_WRITE;
    }

    /* 需要复制页面 */
    new_page = alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma, address);
    if (!new_page)
        return VM_FAULT_OOM;

    /* 复制页内容 */
    cow_user_page(new_page, old_page, address, vma);

    /* 建立新的映射 */
    entry = mk_pte(new_page, vma->vm_page_prot);
    entry = pte_mkwrite(pte_mkdirty(entry));

    /* 原子替换PTE */
    set_pte_at_notify(mm, address, page_table, entry);

    /* 更新反向映射 */
    page_add_new_anon_rmap(new_page, vma, address);

    /* 减少旧页引用 */
    page_remove_rmap(old_page);
    put_page(old_page);

    return VM_FAULT_WRITE;
}

/* COW流程图 */
fork()前:
进程A  --(PTE: RW)--> 物理页P (mapcount=1)

fork()后:
进程A  --(PTE: RO)--> 物理页P (mapcount=2)
进程B  --(PTE: RO)--/

进程A写入触发COW:
进程A  --(PTE: RW)--> 新物理页P' (mapcount=1)
进程B  --(PTE: RO)--> 物理页P   (mapcount=1)
```

---

## vmalloc区域 (vmalloc Area)

Q: [Intermediate] vmalloc和kmalloc有什么区别？
A: vmalloc分配虚拟连续的内存：
```c
/* kmalloc vs vmalloc */
+------------------+-------------------+-------------------+
|      特性        |     kmalloc       |     vmalloc       |
+------------------+-------------------+-------------------+
| 物理连续性       | 物理连续          | 物理不连续        |
| 虚拟连续性       | 虚拟连续          | 虚拟连续          |
| 分配大小         | 较小（<128KB）    | 较大（可到MB级）  |
| 效率             | 高                | 低（需修改页表）  |
| 适用场景         | 小块、DMA         | 大块、非DMA       |
| 地址范围         | 低端内存          | vmalloc区         |
+------------------+-------------------+-------------------+

/* vmalloc API */
void *vmalloc(unsigned long size);         // 分配
void *vzalloc(unsigned long size);         // 分配并清零
void *vmalloc_user(unsigned long size);    // 可映射到用户空间
void *vmalloc_node(unsigned long size, int node);  // NUMA感知
void vfree(const void *addr);              // 释放

/* vmalloc实现原理 */
vmalloc()
    |
    +-> __vmalloc_node_range()
            |
            +-> __get_vm_area_node()  // 在vmalloc区找空间
            |       |
            |       +-> 在vmap_area红黑树中查找
            |
            +-> __vmalloc_area_node()
                    |
                    +-> for each page:
                    |       alloc_page()  // 分配物理页
                    |
                    +-> map_vm_area()     // 建立页表映射

/* vmalloc区域结构 */
struct vm_struct {
    struct vm_struct *next;       // 链表
    void *addr;                   // 虚拟地址
    unsigned long size;           // 大小
    unsigned long flags;          // 标志
    struct page **pages;          // 物理页数组
    unsigned int nr_pages;        // 页数
    phys_addr_t phys_addr;        // 物理地址（ioremap）
    const void *caller;           // 调用者
};

/* 使用示例 */
void *buf = vmalloc(1024 * 1024);  // 分配1MB
if (buf) {
    memset(buf, 0, 1024 * 1024);
    /* 使用buf */
    vfree(buf);
}
```

Q: [Intermediate] ioremap的作用是什么？
A: ioremap将设备物理内存映射到内核虚拟地址：
```c
/* ioremap变体 */
void __iomem *ioremap(phys_addr_t phys_addr, size_t size);
void __iomem *ioremap_nocache(phys_addr_t phys_addr, size_t size);
void __iomem *ioremap_wc(phys_addr_t phys_addr, size_t size);  // Write-combining
void __iomem *ioremap_cache(phys_addr_t phys_addr, size_t size);
void iounmap(volatile void __iomem *addr);

/* I/O内存访问 */
// 使用专用函数访问ioremap的内存
u8 readb(const volatile void __iomem *addr);
u16 readw(const volatile void __iomem *addr);
u32 readl(const volatile void __iomem *addr);
u64 readq(const volatile void __iomem *addr);

void writeb(u8 value, volatile void __iomem *addr);
void writew(u16 value, volatile void __iomem *addr);
void writel(u32 value, volatile void __iomem *addr);
void writeq(u64 value, volatile void __iomem *addr);

/* 带内存屏障版本 */
u32 readl_relaxed(const volatile void __iomem *addr);
void writel_relaxed(u32 value, volatile void __iomem *addr);

/* 使用示例：设备驱动 */
static int my_probe(struct pci_dev *pdev, ...)
{
    void __iomem *regs;
    resource_size_t base = pci_resource_start(pdev, 0);
    resource_size_t size = pci_resource_len(pdev, 0);

    /* 映射BAR0 */
    regs = ioremap(base, size);
    if (!regs)
        return -ENOMEM;

    /* 读取设备寄存器 */
    u32 status = readl(regs + STATUS_REG);
    
    /* 写入设备寄存器 */
    writel(0x1, regs + CONTROL_REG);

    /* 卸载时 */
    iounmap(regs);
    return 0;
}
```

---

## 内存调试 (Memory Debugging)

Q: [Intermediate] 如何调试内存问题？
A: Linux提供多种内存调试工具：
```bash
# 1. 查看进程内存映射
$ cat /proc/<pid>/maps
$ cat /proc/<pid>/smaps  # 详细信息

# 2. 查看系统内存状态
$ cat /proc/meminfo
$ cat /proc/buddyinfo    # 伙伴系统状态
$ cat /proc/pagetypeinfo # 页类型分布
$ cat /proc/slabinfo     # slab分配器状态
$ cat /proc/vmstat       # VM统计

# 3. 内核配置选项
CONFIG_DEBUG_PAGEALLOC=y      # 页分配调试
CONFIG_DEBUG_VM=y             # VM调试
CONFIG_DEBUG_SLAB=y           # slab调试
CONFIG_SLUB_DEBUG=y           # SLUB调试
CONFIG_PAGE_OWNER=y           # 页所有者跟踪
CONFIG_PAGE_POISONING=y       # 页毒化
CONFIG_KASAN=y                # 地址清理器

# 4. 运行时SLUB调试
$ echo 1 > /sys/kernel/slab/<cache>/trace
$ cat /sys/kernel/slab/<cache>/alloc_calls
$ cat /sys/kernel/slab/<cache>/free_calls

# 5. kmemleak - 内存泄漏检测
CONFIG_DEBUG_KMEMLEAK=y
$ echo scan > /sys/kernel/debug/kmemleak
$ cat /sys/kernel/debug/kmemleak

# 6. KASAN示例输出
==================================================================
BUG: KASAN: use-after-free in test_function+0x42/0x50
Read of size 4 at addr ffff888012345678 by task test/1234

Call Trace:
 dump_stack+0x...
 print_address_description+0x...
 kasan_report+0x...
 test_function+0x42/0x50
```

Q: [Intermediate] 查看/proc/meminfo的关键字段？
A: /proc/meminfo提供详细的内存统计：
```bash
$ cat /proc/meminfo
MemTotal:       16384000 kB   # 总物理内存
MemFree:         8192000 kB   # 空闲内存
MemAvailable:   12000000 kB   # 可用内存（包括可回收）
Buffers:          512000 kB   # 块设备缓冲
Cached:          4096000 kB   # 页缓存
SwapCached:        10000 kB   # 交换缓存
Active:          4000000 kB   # 活动内存（最近使用）
Inactive:        3000000 kB   # 非活动内存
Active(anon):    2000000 kB   # 活动匿名页
Inactive(anon):  1000000 kB   # 非活动匿名页
Active(file):    2000000 kB   # 活动文件页
Inactive(file):  2000000 kB   # 非活动文件页
Unevictable:           0 kB   # 不可回收内存
Mlocked:               0 kB   # mlock锁定内存
SwapTotal:       8192000 kB   # 交换空间总量
SwapFree:        8192000 kB   # 空闲交换空间
Dirty:              1000 kB   # 脏页
Writeback:             0 kB   # 正在写回
AnonPages:       3000000 kB   # 匿名页
Mapped:           500000 kB   # 已映射页
Shmem:            100000 kB   # 共享内存
Slab:             500000 kB   # slab分配器
SReclaimable:     400000 kB   # 可回收slab
SUnreclaim:       100000 kB   # 不可回收slab
KernelStack:       20000 kB   # 内核栈
PageTables:        50000 kB   # 页表
NFS_Unstable:          0 kB   # NFS不稳定页
Bounce:                0 kB   # 回弹缓冲
WritebackTmp:          0 kB   # 临时写回
CommitLimit:    16384000 kB   # 提交限制
Committed_AS:   10000000 kB   # 已提交地址空间
VmallocTotal:   34359738367 kB # vmalloc总空间
VmallocUsed:      100000 kB   # vmalloc已用
VmallocChunk:   34359600000 kB # vmalloc最大连续块
HugePages_Total:       0      # 大页总数
HugePages_Free:        0      # 空闲大页
HugePages_Rsvd:        0      # 保留大页
HugePages_Surp:        0      # 超额大页
Hugepagesize:       2048 kB   # 大页大小
```

