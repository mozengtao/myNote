# Linux 内存管理框架深入讲解

基于 Linux 3.2 内核源码分析

---

## 目录

- [内存管理架构概述](#内存管理架构概述)
- [物理内存管理](#物理内存管理)
- [虚拟内存管理](#虚拟内存管理)
- [内存分配器](#内存分配器)
- [页面回收与交换](#页面回收与交换)
- [关键源码文件](#关键源码文件)

---

## 内存管理架构概述

### 整体架构图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            用户空间 (User Space)                             │
│                                                                              │
│    malloc()    mmap()    brk()    munmap()    mlock()    mprotect()         │
└───────────────────────────────────┬──────────────────────────────────────────┘
                                    │ 系统调用
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           内核内存管理子系统                                  │
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                    虚拟内存管理 (Virtual Memory)                        │ │
│  │                                                                         │ │
│  │   mm_struct ──► vm_area_struct ──► vm_area_struct ──► ...              │ │
│  │       │              │                    │                             │ │
│  │       │         代码段               堆区               栈区             │ │
│  │       ▼                                                                 │ │
│  │   pgd (页全局目录)                                                      │ │
│  │       │                                                                 │ │
│  │       └──► pud ──► pmd ──► pte ──► 物理页                              │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                    │                                         │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                    物理内存管理 (Physical Memory)                       │ │
│  │                                                                         │ │
│  │   ┌──────────────────────────────────────────────────────────────────┐ │ │
│  │   │                     Buddy System (伙伴系统)                       │ │ │
│  │   │   zone_dma ─► zone_normal ─► zone_highmem                        │ │ │
│  │   │       │             │              │                              │ │ │
│  │   │   free_area[0] ... free_area[10]  (2^0 到 2^10 页)               │ │ │
│  │   └──────────────────────────────────────────────────────────────────┘ │ │
│  │                            │                                           │ │
│  │                            ▼                                           │ │
│  │   ┌──────────────────────────────────────────────────────────────────┐ │ │
│  │   │                     Slab/Slub 分配器                              │ │ │
│  │   │   kmem_cache ──► slabs ──► 对象 (固定大小的内核对象)              │ │ │
│  │   └──────────────────────────────────────────────────────────────────┘ │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
│                                    │                                         │
│                                    ▼                                         │
│  ┌────────────────────────────────────────────────────────────────────────┐ │
│  │                      页面回收 (Page Reclaim)                            │ │
│  │                                                                         │ │
│  │   kswapd ──► LRU 链表扫描 ──► 回收/交换                                │ │
│  │                                                                         │ │
│  │   OOM Killer ──► 内存不足时终止进程                                     │ │
│  └────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              物理内存 (RAM)                                  │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 内存分层模型

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              NUMA 架构                                       │
│                                                                              │
│    ┌─────────────────────┐         ┌─────────────────────┐                  │
│    │      Node 0         │         │      Node 1         │                  │
│    │                     │  互联   │                     │                  │
│    │  CPU0  CPU1  内存   │◄──────►│  CPU2  CPU3  内存   │                  │
│    └──────────┬──────────┘         └──────────┬──────────┘                  │
│               │                               │                              │
│               ▼                               ▼                              │
│    ┌─────────────────────┐         ┌─────────────────────┐                  │
│    │     pg_data_t       │         │     pg_data_t       │                  │
│    │                     │         │                     │                  │
│    │  ┌───────────────┐  │         │  ┌───────────────┐  │                  │
│    │  │   Zone DMA    │  │         │  │   Zone DMA    │  │                  │
│    │  │   (0-16MB)    │  │         │  │   (0-16MB)    │  │                  │
│    │  ├───────────────┤  │         │  ├───────────────┤  │                  │
│    │  │  Zone Normal  │  │         │  │  Zone Normal  │  │                  │
│    │  │  (16MB-896MB) │  │         │  │  (16MB-896MB) │  │                  │
│    │  ├───────────────┤  │         │  ├───────────────┤  │                  │
│    │  │  Zone HighMem │  │         │  │  Zone HighMem │  │                  │
│    │  │  (>896MB)     │  │         │  │  (>896MB)     │  │                  │
│    │  └───────────────┘  │         │  └───────────────┘  │                  │
│    └─────────────────────┘         └─────────────────────┘                  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 物理内存管理

### 核心数据结构

#### 1. struct page - 页描述符

```c
// include/linux/mm_types.h
struct page {
    unsigned long flags;            // 页面状态标志 (PG_locked, PG_dirty 等)
    
    atomic_t _count;                // 引用计数
    
    union {
        atomic_t _mapcount;         // 页表映射计数
        struct {                    // SLUB 使用
            u16 inuse;
            u16 objects;
        };
    };
    
    union {
        struct {
            unsigned long private;  // 私有数据
            struct address_space *mapping; // 所属地址空间
        };
        struct kmem_cache *slab;    // SLUB: 所属缓存
        struct page *first_page;    // 复合页: 首页
    };
    
    union {
        pgoff_t index;              // 在文件中的偏移
        void *freelist;             // SLUB: 空闲对象链表
    };
    
    struct list_head lru;           // LRU 链表
    // ...
};
```

#### 2. struct zone - 内存区域

```c
// include/linux/mmzone.h
struct zone {
    unsigned long watermark[NR_WMARK];  // 水位线 (min, low, high)
    
    unsigned long       lowmem_reserve[MAX_NR_ZONES];
    
    struct per_cpu_pageset __percpu *pageset; // 每 CPU 页面缓存
    
    spinlock_t          lock;
    
    struct free_area    free_area[MAX_ORDER]; // 伙伴系统空闲链表
    
    unsigned long       spanned_pages;  // 总页数
    unsigned long       present_pages;  // 实际可用页数
    
    const char          *name;          // 区域名称
    
    // LRU 链表
    struct zone_lru {
        struct list_head list;
        unsigned long nr_saved_scan;
    } lru[NR_LRU_LISTS];
    
    struct zone_reclaim_stat reclaim_stat;
    
    unsigned long       pages_scanned;  // 扫描过的页数
    unsigned long       flags;
    
    // ...
};
```

#### 3. 伙伴系统 (Buddy System)

```c
// include/linux/mmzone.h
struct free_area {
    struct list_head    free_list[MIGRATE_TYPES]; // 不同迁移类型的空闲链表
    unsigned long       nr_free;                  // 空闲块数
};

// 伙伴系统阶数 (order)
// order 0: 2^0 = 1 页 (4KB)
// order 1: 2^1 = 2 页 (8KB)
// order 2: 2^2 = 4 页 (16KB)
// ...
// order 10: 2^10 = 1024 页 (4MB)
```

### 伙伴系统工作原理

```
                    分配 order=2 (4页)
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Buddy System                                          │
│                                                                              │
│  free_area[0]  ──► [1页] [1页] [1页] ...                                    │
│  free_area[1]  ──► [2页] [2页] ...                                          │
│  free_area[2]  ──► [4页] ◄── 从这里分配                                      │
│  free_area[3]  ──► [8页] ...                                                │
│  free_area[4]  ──► [16页] ...                                               │
│  ...                                                                         │
│  free_area[10] ──► [1024页] ...                                             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘

                    释放时的合并
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│   释放块 A (order=2)     相邻块 B (order=2)                                  │
│       │                        │                                             │
│       └────────┬───────────────┘                                             │
│                │                                                             │
│                ▼                                                             │
│   检查 "伙伴" (地址相邻且同 order) 是否空闲                                   │
│                │                                                             │
│       ┌────────┴────────┐                                                    │
│       ▼                 ▼                                                    │
│    空闲              被占用                                                   │
│       │                 │                                                    │
│       ▼                 ▼                                                    │
│   合并为 order=3   直接加入 free_area[2]                                     │
│   继续向上合并                                                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 页面分配 API

```c
// 分配单页
struct page *alloc_page(gfp_t gfp_mask);

// 分配 2^order 页
struct page *alloc_pages(gfp_t gfp_mask, unsigned int order);

// 分配并返回虚拟地址
unsigned long __get_free_page(gfp_t gfp_mask);
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order);
unsigned long get_zeroed_page(gfp_t gfp_mask);

// 释放页面
void __free_page(struct page *page);
void __free_pages(struct page *page, unsigned int order);
void free_page(unsigned long addr);
void free_pages(unsigned long addr, unsigned int order);

// GFP 标志 (Get Free Pages)
#define GFP_KERNEL      // 常规内核分配，可睡眠
#define GFP_ATOMIC      // 原子分配，不可睡眠 (中断上下文)
#define GFP_USER        // 用户空间分配
#define GFP_HIGHUSER    // 优先高端内存
#define GFP_DMA         // DMA 区域
```

---

## 虚拟内存管理

### 进程地址空间

```c
// include/linux/mm_types.h
struct mm_struct {
    struct vm_area_struct *mmap;        // VMA 链表
    struct rb_root mm_rb;               // VMA 红黑树
    
    struct vm_area_struct *mmap_cache;  // 最近访问的 VMA (缓存)
    
    unsigned long (*get_unmapped_area)(struct file *filp,
                    unsigned long addr, unsigned long len,
                    unsigned long pgoff, unsigned long flags);
    
    pgd_t *pgd;                         // 页全局目录
    
    atomic_t mm_users;                  // 用户引用计数
    atomic_t mm_count;                  // 内核引用计数
    
    int map_count;                      // VMA 数量
    
    spinlock_t page_table_lock;
    struct rw_semaphore mmap_sem;       // mmap 信号量
    
    struct list_head mmlist;            // 所有 mm_struct 链表
    
    unsigned long start_code, end_code;   // 代码段
    unsigned long start_data, end_data;   // 数据段
    unsigned long start_brk, brk;         // 堆
    unsigned long start_stack;            // 栈起始
    unsigned long arg_start, arg_end;     // 参数
    unsigned long env_start, env_end;     // 环境变量
    
    unsigned long total_vm;             // 总页数
    unsigned long locked_vm;            // 锁定页数
    unsigned long shared_vm;            // 共享页数
    unsigned long exec_vm;              // 可执行页数
    unsigned long stack_vm;             // 栈页数
    unsigned long reserved_vm;          // 保留页数
    // ...
};
```

### VMA (Virtual Memory Area)

```c
// include/linux/mm_types.h
struct vm_area_struct {
    struct mm_struct *vm_mm;            // 所属 mm_struct
    
    unsigned long vm_start;             // 起始地址
    unsigned long vm_end;               // 结束地址
    
    struct vm_area_struct *vm_next;     // 链表下一个
    struct vm_area_struct *vm_prev;     // 链表上一个
    
    pgprot_t vm_page_prot;              // 访问权限
    unsigned long vm_flags;             // 标志 (VM_READ, VM_WRITE, VM_EXEC)
    
    struct rb_node vm_rb;               // 红黑树节点
    
    struct anon_vma *anon_vma;          // 匿名映射
    
    const struct vm_operations_struct *vm_ops; // VMA 操作
    
    unsigned long vm_pgoff;             // 文件偏移 (以页为单位)
    struct file *vm_file;               // 映射的文件
    void *vm_private_data;              // 私有数据
};
```

### 进程地址空间布局

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         进程虚拟地址空间 (32位)                               │
│                                                                              │
│  高地址                                                                      │
│  0xFFFFFFFF ┌─────────────────────────────────────────────────────────────┐ │
│             │                     内核空间                                │ │
│             │                    (1GB)                                   │ │
│  0xC0000000 ├─────────────────────────────────────────────────────────────┤ │
│             │                     栈 (向下增长)                           │ │
│             │                        ↓                                   │ │
│             │                       ...                                  │ │
│             │                        ↑                                   │ │
│             │                     mmap 区域                              │ │
│             │               (文件映射/共享库)                             │ │
│             │                        ↑                                   │ │
│             │                       ...                                  │ │
│             │                        ↑                                   │ │
│             │                     堆 (向上增长)                           │ │
│             │                        ↑                                   │ │
│             ├─────────────────────────────────────────────────────────────┤ │
│             │                     BSS 段                                 │ │
│             ├─────────────────────────────────────────────────────────────┤ │
│             │                     数据段                                  │ │
│             ├─────────────────────────────────────────────────────────────┤ │
│             │                     代码段                                  │ │
│  0x08048000 └─────────────────────────────────────────────────────────────┘ │
│             │                     保留区域                                │ │
│  0x00000000 └─────────────────────────────────────────────────────────────┘ │
│  低地址                                                                      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 页表结构 (x86_64)

```
                    4级页表 (x86_64)
                          
     虚拟地址 (48位有效)
  ┌─────────────────────────────────────────────────────────────────┐
  │ 63-48 (符号扩展) │ 47-39 │ 38-30 │ 29-21 │ 20-12 │ 11-0  │
  │     (未使用)     │  PGD  │  PUD  │  PMD  │  PTE  │ Offset│
  └────────────────────┬───────┬───────┬───────┬───────┬───────┘
                       │       │       │       │       │
                       │       │       │       │       │
      CR3 ─────────────┘       │       │       │       │
        │                      │       │       │       │
        ▼                      │       │       │       │
    ┌───────┐                  │       │       │       │
    │  PGD  │ 页全局目录        │       │       │       │
    │ Entry │──────────────────┘       │       │       │
    └───────┘                          │       │       │
        │                              │       │       │
        ▼                              │       │       │
    ┌───────┐                          │       │       │
    │  PUD  │ 页上层目录                │       │       │
    │ Entry │──────────────────────────┘       │       │
    └───────┘                                  │       │
        │                                      │       │
        ▼                                      │       │
    ┌───────┐                                  │       │
    │  PMD  │ 页中间目录                        │       │
    │ Entry │──────────────────────────────────┘       │
    └───────┘                                          │
        │                                              │
        ▼                                              │
    ┌───────┐                                          │
    │  PTE  │ 页表项                                   │
    │ Entry │──────────────────────────────────────────┘
    └───────┘
        │
        ▼
    ┌───────────────┐
    │   物理页面     │ + 页内偏移 = 物理地址
    │    (4KB)      │
    └───────────────┘
```

### 缺页异常处理

```
访问虚拟地址
      │
      ▼
 MMU 查找页表
      │
      ├── 找到有效 PTE ──► 正常访问
      │
      └── 缺页异常 (Page Fault)
              │
              ▼
      do_page_fault()
              │
              ▼
      查找 VMA (find_vma)
              │
              ├── 地址不在任何 VMA ──► SIGSEGV (段错误)
              │
              └── 地址在 VMA 中
                      │
                      ▼
              检查权限 (vm_flags)
                      │
                      ├── 权限不足 ──► SIGSEGV
                      │
                      └── 权限正确
                              │
                              ▼
                      handle_mm_fault()
                              │
                      ┌───────┴───────┐
                      │               │
                      ▼               ▼
                  匿名页           文件映射页
                      │               │
                      ▼               ▼
              do_anonymous_page()  do_read_fault()
                      │               │
                      ├── 分配新页    ├── 从页缓存读取
                      │               │   或触发磁盘I/O
                      ├── 清零        │
                      │               │
                      └───────────────┴───────────────┐
                                                      │
                                                      ▼
                                              建立页表映射
                                                      │
                                                      ▼
                                               返回继续执行
```

---

## 内存分配器

### Slab/Slub 分配器

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Slab 分配器架构                                       │
│                                                                              │
│   kmem_cache ("task_struct")         kmem_cache ("inode_cache")             │
│          │                                   │                               │
│          ▼                                   ▼                               │
│   ┌─────────────┐                     ┌─────────────┐                       │
│   │ kmem_cache  │                     │ kmem_cache  │                       │
│   │             │                     │             │                       │
│   │ size = 1024 │                     │ size = 512  │                       │
│   │ name = ...  │                     │ name = ...  │                       │
│   └──────┬──────┘                     └──────┬──────┘                       │
│          │                                   │                               │
│          ▼                                   ▼                               │
│   ┌─────────────────────────────┐     ┌─────────────────────────────┐       │
│   │         Slab               │     │         Slab               │       │
│   │  ┌───┬───┬───┬───┬───┐    │     │  ┌───┬───┬───┬───┐        │       │
│   │  │obj│obj│obj│obj│...│    │     │  │obj│obj│obj│...│        │       │
│   │  └───┴───┴───┴───┴───┘    │     │  └───┴───┴───┴───┘        │       │
│   │      (连续物理页)          │     │      (连续物理页)          │       │
│   └─────────────────────────────┘     └─────────────────────────────┘       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘

                              对象分配流程
                                   │
                                   ▼
                            kmem_cache_alloc()
                                   │
               ┌───────────────────┴───────────────────┐
               │                                       │
               ▼                                       ▼
         CPU 本地缓存有对象?                     从 slab 分配
               │                                       │
               ├── 是 ──► 直接返回                     ├── 部分空闲 slab
               │                                       │
               └── 否 ──► 重新填充本地缓存             ├── 完全空闲 slab
                                                       │
                                                       └── 新分配 slab
                                                           (从伙伴系统)
```

### 内存分配 API

```c
// Slab 分配器
void *kmalloc(size_t size, gfp_t flags);        // 分配连续物理内存
void kfree(const void *objp);                    // 释放

void *kzalloc(size_t size, gfp_t flags);        // 分配并清零

void *kmem_cache_alloc(struct kmem_cache *cachep, gfp_t flags);
void kmem_cache_free(struct kmem_cache *cachep, void *objp);

// vmalloc - 虚拟连续，物理不连续
void *vmalloc(unsigned long size);
void vfree(const void *addr);

// 创建专用缓存
struct kmem_cache *kmem_cache_create(const char *name, size_t size,
                                      size_t align, unsigned long flags,
                                      void (*ctor)(void *));
```

### kmalloc 大小类

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         kmalloc 缓存大小                                     │
│                                                                              │
│   kmalloc-96      kmalloc-128     kmalloc-192     kmalloc-256               │
│   kmalloc-512     kmalloc-1024    kmalloc-2048    kmalloc-4096              │
│   kmalloc-8192    ...                                                        │
│                                                                              │
│   请求 100 字节 ──► 分配 kmalloc-128 ──► 实际占用 128 字节                   │
│   请求 1000 字节 ──► 分配 kmalloc-1024 ──► 实际占用 1024 字节                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 页面回收与交换

### LRU 链表

```c
// 5个 LRU 链表
enum lru_list {
    LRU_INACTIVE_ANON = 0,  // 不活跃匿名页
    LRU_ACTIVE_ANON   = 1,  // 活跃匿名页
    LRU_INACTIVE_FILE = 2,  // 不活跃文件页
    LRU_ACTIVE_FILE   = 3,  // 活跃文件页
    LRU_UNEVICTABLE   = 4,  // 不可回收页
    NR_LRU_LISTS
};
```

### 页面回收流程

```
                    内存不足触发
                         │
                         ▼
                    kswapd 唤醒
                    或直接回收
                         │
                         ▼
              shrink_zone() / shrink_lruvec()
                         │
           ┌─────────────┴─────────────┐
           │                           │
           ▼                           ▼
    shrink_active_list()        shrink_inactive_list()
    (活跃 → 不活跃)             (尝试回收)
           │                           │
           │                           ▼
           │                   isolate_lru_pages()
           │                   (隔离候选页面)
           │                           │
           │                           ▼
           │                   shrink_page_list()
           │                           │
           │              ┌────────────┴────────────┐
           │              │                         │
           │              ▼                         ▼
           │         文件页                     匿名页
           │              │                         │
           │              ▼                         ▼
           │         是否脏页?                  有交换空间?
           │              │                         │
           │     ┌────────┴────────┐       ┌───────┴───────┐
           │     ▼                 ▼       ▼               ▼
           │   干净              脏页     有               无
           │     │                 │       │               │
           │     ▼                 ▼       ▼               ▼
           │  直接回收          写回    swap out       无法回收
           │                   后回收                   (保留)
           │                      │       │
           └──────────────────────┴───────┘
                                  │
                                  ▼
                           free_page()
                           返回伙伴系统
```

### OOM Killer

```c
// mm/oom_kill.c
static void select_bad_process(...)
{
    // 遍历所有进程
    for_each_process(p) {
        // 计算 OOM 分数
        points = oom_badness(p, ...);
        
        // 选择分数最高的进程
        if (points > chosen_points) {
            chosen = p;
            chosen_points = points;
        }
    }
}

// OOM 分数计算因素:
// 1. 进程使用的内存量 (主要因素)
// 2. oom_score_adj 调整值 (-1000 到 1000)
// 3. root 进程分数略低
```

---

## 关键源码文件

### 物理内存管理

| 文件 | 功能 |
|------|------|
| `mm/page_alloc.c` | 伙伴系统，页面分配 |
| `mm/bootmem.c` | 引导期内存分配 |
| `mm/memblock.c` | memblock 分配器 |
| `mm/mmzone.c` | zone 管理 |
| `mm/sparse.c` | 稀疏内存模型 |

### 虚拟内存管理

| 文件 | 功能 |
|------|------|
| `mm/memory.c` | 页表管理，缺页处理 |
| `mm/mmap.c` | VMA 管理，mmap |
| `mm/mremap.c` | 重新映射 |
| `mm/mprotect.c` | 保护属性修改 |
| `mm/mlock.c` | 内存锁定 |
| `mm/rmap.c` | 反向映射 |
| `mm/fremap.c` | 文件重映射 |

### 内存分配器

| 文件 | 功能 |
|------|------|
| `mm/slab.c` | SLAB 分配器 |
| `mm/slub.c` | SLUB 分配器 (默认) |
| `mm/slob.c` | SLOB 分配器 (嵌入式) |
| `mm/vmalloc.c` | vmalloc 分配 |
| `mm/percpu.c` | Per-CPU 分配 |

### 页面回收

| 文件 | 功能 |
|------|------|
| `mm/vmscan.c` | 页面扫描和回收 |
| `mm/swap.c` | LRU 管理 |
| `mm/swapfile.c` | 交换文件管理 |
| `mm/swap_state.c` | 交换缓存 |
| `mm/oom_kill.c` | OOM Killer |

### 页缓存

| 文件 | 功能 |
|------|------|
| `mm/filemap.c` | 文件页缓存 |
| `mm/page-writeback.c` | 脏页写回 |
| `mm/readahead.c` | 预读 |
| `mm/truncate.c` | 页面截断 |
| `mm/shmem.c` | tmpfs/共享内存 |

---

## 总结

### 内存管理核心机制

1. **物理内存**: 伙伴系统管理页面，Slab 管理对象
2. **虚拟内存**: 页表映射，VMA 描述地址空间
3. **缺页处理**: 按需分配，延迟加载
4. **页面回收**: LRU 算法，kswapd 后台回收
5. **交换机制**: 将不活跃页面换出到磁盘

### 设计亮点

1. **分层设计**: Zone → Buddy → Slab
2. **延迟分配**: 写时复制 (COW)，按需调页
3. **高效缓存**: Per-CPU 缓存，减少锁竞争
4. **智能回收**: 区分文件页/匿名页，活跃/不活跃

---

*本文档基于 Linux 3.2 内核源码分析*

