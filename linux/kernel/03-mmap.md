# mmap 内存映射 - 内核执行路径详解

> 🎯 **学习目标**: 深入理解 Linux 内存映射机制，掌握从虚拟内存到物理页面的完整映射过程和零拷贝原理

---

## 🧩 第一部分：宏观架构图

```
User Space
├── Application
│   └── mmap(addr, length, prot, flags, fd, offset)
├── glibc
│   └── syscall(__NR_mmap)
└── Virtual Address Space (VMA)
    ├── [heap] 0x1000000-0x2000000
    ├── [stack] 0x7fff0000-0x80000000
    ├── [mmap] 0x7f8000000000-0x7f8010000000  [Mapped Region]
    └── [text/data] 0x400000-0x600000

════════════════════════════════════════════════════════════════════════════════

Kernel Space - Virtual Memory Management
├── sys_mmap()                               [Syscall Entry]
├── vm_mmap_pgoff()                          [Core mmap Logic]
│   ├── security_mmap_addr()                 [Security Check]
│   ├── get_unmapped_area()                  [Find Virtual Address Range]
│   ├── mmap_region()                        [Create Mapping Region]
│   │   ├── may_expand_vm()                  [Check VMA Limits]
│   │   ├── find_vma_links()                 [Find Insert Position]
│   │   ├── vma_merge()                      [Merge Adjacent VMAs]
│   │   ├── vm_area_alloc()                  [Allocate New VMA]
│   │   ├── file->f_op->mmap()               [Call Filesystem mmap]
│   │   │   └── ext4_file_mmap()             [File Mapping]
│   │   │       └── generic_file_mmap()      [Generic File Mapping]
│   │   ├── vma_link()                       [Link to mm_struct]
│   │   └── Set PTE (Deferred until Page Fault)
│   └── Return Mapped Address

════════════════════════════════════════════════════════════════════════════════

Page Management
├── Page Fault Handler
│   ├── do_page_fault()                      [Page Fault Entry]
│   ├── handle_mm_fault()                    [MM Fault Handler]
│   ├── __handle_mm_fault()                  [Core Fault Logic]
│   │   ├── handle_pte_fault()               [PTE-level Handling]
│   │   ├── do_fault()                       [File Mapping Fault]
│   │   │   ├── __do_fault()                 [Read Page from File]
│   │   │   │   └── vma->vm_ops->fault()     [Call VMA Ops]
│   │   │   │       └── filemap_fault()      [File Mapping Fault]
│   │   │   │           ├── find_get_page()  [Lookup in Page Cache]
│   │   │   │           └── page_cache_read() [Read from Disk]
│   │   │   └── alloc_set_pte()              [Set Page Table Entry]
│   │   └── do_anonymous_page()              [Anonymous Page]
│   └── Update TLB (Translation Lookaside Buffer)
├── Page Reclaim
│   ├── kswapd                               [Kernel Reclaim Thread]
│   ├── shrink_page_list()                   [Reclaim Logic]
│   └── Write Back Dirty Pages
└── Page Cache
    ├── address_space                        [Page Cache Management]
    ├── radix_tree / xarray                  [Page Index Structure]
    └── Unified File Data Cache

════════════════════════════════════════════════════════════════════════════════

Hardware Layer
├── MMU (Memory Management Unit)             [Address Translation]
├── TLB (Translation Lookaside Buffer)       [Translation Cache]
├── Physical Memory (RAM)                    [Actual Storage]
└── Storage Device (SSD/HDD)               [Persistent Data]
```

*图表说明：mmap 在用户态建立虚拟地址映射，内核通过 VMA 管理映射区域；首次访问触发缺页中断，从 Page Cache 或磁盘加载页面并建立页表映射。MMU/TLB 完成虚拟地址到物理内存的转换，实现文件到内存的直接映射和零拷贝访问。*

---

## 🔬 第二部分：内核执行路径

### 2.1 mmap() 系统调用完整路径

```c
// 用户态调用
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

// 内核路径展开
SYSCALL_DEFINE6(mmap, unsigned long, addr, unsigned long, len,
                unsigned long, prot, unsigned long, flags,
                unsigned long, fd, unsigned long, off)
├── ksys_mmap_pgoff()
├── vm_mmap_pgoff(file, addr, len, prot, flag, pgoff)
│   ├── security_mmap_addr(addr)                     // 地址安全检查
│   ├── security_mmap_file(file, prot, flag)         // 文件权限检查
│   ├── down_write(&mm->mmap_sem)                    // 获取 mm 写锁
│   ├── do_mmap_pgoff(file, addr, len, prot, flag, pgoff) // 🔥 核心映射
│   │   ├── get_unmapped_area(file, addr, len, pgoff, flags) // 寻找虚拟地址
│   │   │   ├── arch_get_unmapped_area()             // 架构相关地址分配
│   │   │   └── file->f_op->get_unmapped_area()      // 文件特定地址分配
│   │   ├── mmap_region(file, addr, len, vm_flags, pgoff) // 🔥 创建映射区域
│   │   │   ├── may_expand_vm(mm, vm_flags, len >> PAGE_SHIFT) // 检查VM限制
│   │   │   ├── munmap_vma_range(mm, addr, len)      // 清理重叠区域
│   │   │   ├── find_vma_links(mm, addr, len, &prev, &rb_link, &rb_parent)
│   │   │   ├── accountable_mapping(file, vm_flags)  // 检查映射限制
│   │   │   ├── vma_merge()                          // 🔥 尝试合并相邻VMA
│   │   │   │   └── 检查相邻 VMA 是否可合并
│   │   │   ├── vm_area_alloc(mm)                    // 分配新的 VMA
│   │   │   ├── vma_set_file(vma, file)              // 设置文件关联
│   │   │   ├── call_mmap(file, vma)                 // 🔥 调用文件系统 mmap
│   │   │   │   └── file->f_op->mmap(file, vma)      
│   │   │   │       └── ext4_file_mmap()             // ext4 文件映射
│   │   │   │           └── generic_file_mmap()      // 通用文件映射
│   │   │   │               ├── vma->vm_ops = &generic_file_vm_ops
│   │   │   │               └── 设置页面操作函数
│   │   │   ├── vma_link(mm, vma, prev, rb_link, rb_parent) // 链接VMA到mm
│   │   │   │   ├── __vma_link_list()                // 链表链接
│   │   │   │   ├── __vma_link_rb()                  // 红黑树链接
│   │   │   │   └── __vma_link_file()                // 文件映射链接
│   │   │   └── vm_stat_account()                    // 统计更新
│   │   └── 返回映射地址
│   └── up_write(&mm->mmap_sem)                      // 释放 mm 写锁
└── 返回用户态
```

### 2.2 缺页中断处理路径

```c
// 当用户首次访问 mmap 区域时触发
do_page_fault(struct pt_regs *regs, unsigned long address)
├── find_vma(mm, address)                    // 查找对应的 VMA
├── handle_mm_fault(vma, address, flags)     // 🔥 MM 缺页处理
│   ├── __handle_mm_fault(vma, address, flags)
│   │   ├── 遍历页表层次 (PGD → PUD → PMD → PTE)
│   │   ├── handle_pte_fault(vmf)            // PTE 级别处理
│   │   │   ├── do_fault(vmf)                // 🔥 处理映射缺页
│   │   │   │   ├── __do_fault(vmf)          // 从文件加载页面
│   │   │   │   │   ├── vma->vm_ops->fault() // 调用VMA操作
│   │   │   │   │   │   └── filemap_fault()  // 🔥 文件映射缺页
│   │   │   │   │   │       ├── find_get_page(mapping, offset) // 查找页缓存
│   │   │   │   │   │       ├── 如果未找到 → page_cache_read() // 从磁盘读取
│   │   │   │   │   │       │   ├── __page_cache_alloc()      // 分配页面
│   │   │   │   │   │       │   ├── add_to_page_cache_lru()   // 加入页缓存
│   │   │   │   │   │       │   └── mapping->a_ops->readpage() // 读取数据
│   │   │   │   │   │       │       └── ext4_readpage()       // ext4读页
│   │   │   │   │   │       └── 返回页面
│   │   │   │   │   └── 返回页面给 VMM
│   │   │   │   ├── alloc_set_pte(vmf, memcg, page) // 🔥 设置页表项
│   │   │   │   │   ├── maybe_mkwrite()              // 设置写权限
│   │   │   │   │   ├── inc_mm_counter_fast()        // 更新统计
│   │   │   │   │   └── set_pte_at()                 // 设置硬件页表项
│   │   │   │   └── unlock_page(page)
│   │   │   └── do_anonymous_page()          // 匿名页处理 (MAP_ANONYMOUS)
│   │   │       ├── alloc_zeroed_user_highpage_movable() // 分配零页
│   │   │       └── 设置页表项
│   │   └── 更新访问位和脏位
│   └── flush_tlb_page()                     // 刷新 TLB
└── 返回用户态继续执行
```

### 2.3 munmap 与内存释放路径

```c
// 用户态调用
int munmap(void *addr, size_t length);

SYSCALL_DEFINE2(munmap, unsigned long, addr, unsigned long, len)
└── vm_munmap(addr, len)
    └── do_munmap()
        ├── find_vma()                    // 查找对应 VMA
        ├── unmap_region()                // 解除页表映射
        │   ├── unmap_vmas()              // 清除 PTE
        │   ├── free_pgd_range()          // 释放空页表
        │   └── tlb_gather_mmu()          // 批量刷新 TLB
        ├── remove_vma()                  // 从 mm 中移除 VMA
        │   ├── vma->vm_ops->close()      // 文件映射: 解除 page cache 关联
        │   └── vm_area_free()            // 释放 VMA 结构
        └── 如果是 MAP_SHARED 脏页 → 标记 writeback
```

### 2.4 共享内存 (MAP_SHARED) 与进程间通信

```c
// 进程 A
void *addr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
strcpy(addr, "hello");  // 写入共享映射
msync(addr, size, MS_SYNC);  // 可选: 强制同步

// 进程 B (同一文件 MAP_SHARED)
void *addr2 = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
printf("%s\n", (char*)addr2);  // 读到 "hello" — 同一物理页

// 内核机制:
// 1. 两个 VMA 指向同一 struct address_space (页缓存)
// 2. 同一物理 page frame 映射到不同进程虚拟地址
// 3. 写时无需 COW (与 MAP_PRIVATE 不同)
```

**应用场景**: 共享库加载、进程间通信 (shm)、数据库 mmap、Redis AOF

### 2.5 关键内核子系统协作

1. **虚拟内存管理 (VMM)**: 管理进程虚拟地址空间
2. **页面分配器**: 分配和释放物理页面  
3. **页缓存**: 缓存文件数据，实现统一缓存
4. **文件系统**: 提供底层数据读取
5. **MMU**: 硬件地址转换支持

---

## 🧱 第三部分：核心数据结构

### 3.1 struct vm_area_struct - 虚拟内存区域

```c
struct vm_area_struct {
    /* VMA 范围 */
    unsigned long vm_start;         // VMA 起始地址  
    unsigned long vm_end;           // VMA 结束地址
    
    /* 链表和树结构 */
    struct vm_area_struct *vm_next; // 按地址排序的链表
    struct vm_area_struct *vm_prev;
    struct rb_node vm_rb;           // 红黑树节点 (快速查找)
    
    /* 内存描述符 */
    struct mm_struct *vm_mm;        // 所属的内存描述符
    
    /* 权限和标志 */
    pgprot_t vm_page_prot;          // 页面保护属性 (硬件相关)
    unsigned long vm_flags;         // VMA 标志 (VM_READ, VM_WRITE...)
    
    /* 红黑树缓存 */
    struct {
        struct rb_node rb;
        unsigned long rb_subtree_last;
    } shared;
    
    /* 匿名 VMA 或文件 VMA */
    union {
        struct {                    // 匿名映射
            struct list_head list;
            void *parent;
            struct vm_area_struct *head;
        } vm_set;
        
        struct raw_prio_tree_node prio_tree_node; // 优先树节点 (文件映射)
    };
    
    /* 操作函数 */
    const struct vm_operations_struct *vm_ops; // VMA 操作函数表
    
    /* 文件映射相关 */
    unsigned long vm_pgoff;         // 文件偏移 (页为单位)
    struct file *vm_file;           // 映射的文件 (如果是文件映射)
    void *vm_private_data;          // 私有数据
    
#ifdef CONFIG_NUMA
    struct mempolicy *vm_policy;    // NUMA 内存策略
#endif
    struct vm_userfaultfd_ctx vm_userfaultfd_ctx; // 用户态缺页处理
};
```

**作用**: 描述进程虚拟地址空间中的一个连续区域  
**生命周期**: mmap 时创建 → munmap 时销毁  
**关键字段**:
- `vm_start/vm_end`: 定义虚拟地址范围
- `vm_ops`: 定义该区域的操作方法
- `vm_file`: 如果是文件映射，指向对应文件

### 3.2 struct mm_struct - 内存描述符

```c
struct mm_struct {
    struct vm_area_struct *mmap;        // VMA 链表头
    struct rb_root mm_rb;               // VMA 红黑树根
    u32 vmacache_seqnum;               // VMA 缓存序列号
    
#ifdef CONFIG_MMU
    unsigned long (*get_unmapped_area) (struct file *filp,
                unsigned long addr, unsigned long len,
                unsigned long pgoff, unsigned long flags);
#endif
    unsigned long mmap_base;            // mmap 区域基址
    unsigned long mmap_legacy_base;     // 传统 mmap 基址
    unsigned long task_size;            // 用户态地址空间大小
    unsigned long highest_vm_end;       // 最高 VMA 结束地址
    
    pgd_t * pgd;                       // 页全局目录 (顶级页表)
    
    /**
     * @mm_users: The number of users including userspace.
     * 用户态引用计数，包括用户空间进程
     */
    atomic_t mm_users;
    
    /**
     * @mm_count: The number of references to &struct mm_struct
     * 内核引用计数
     */  
    atomic_t mm_count;
    
#ifdef CONFIG_MMU
    atomic_long_t pgtables_bytes;      // 页表占用内存
#endif
    
    int map_count;                     // VMA 数量
    
    spinlock_t page_table_lock;        // 页表锁
    struct rw_semaphore mmap_sem;      // mmap 信号量 (保护 VMA)
    
    struct list_head mmlist;           // 全局 mm 链表
    
    /* 特殊区域 */
    unsigned long hiwater_rss;         // RSS 高水位
    unsigned long hiwater_vm;          // 虚拟内存高水位
    unsigned long total_vm;            // 总虚拟内存页数
    unsigned long locked_vm;           // 锁定内存页数
    unsigned long pinned_vm;           // 固定内存页数
    unsigned long data_vm;             // 数据段页数
    unsigned long exec_vm;             // 可执行段页数
    unsigned long stack_vm;            // 栈段页数
    
    /* 代码和数据段 */
    unsigned long start_code, end_code, start_data, end_data;
    unsigned long start_brk, brk, start_stack;
    unsigned long arg_start, arg_end, env_start, env_end;
    
    unsigned long saved_auxv[AT_VECTOR_SIZE]; // 辅助向量
    
    struct mm_rss_stat rss_stat;       // RSS 统计
    
    struct linux_binfmt *binfmt;       // 二进制格式
    
    /* Architecture-specific MM context */
    mm_context_t context;              // 架构相关上下文
    
    unsigned long flags;               // MM 标志
    
    struct core_state *core_state;     // 核心转储状态
#ifdef CONFIG_AIO  
    spinlock_t                  ioctx_lock;
    struct kioctx_table __rcu   *ioctx_table;
#endif
#ifdef CONFIG_MEMCG
    struct task_struct __rcu *owner;   // 所有者任务
#endif
    struct user_namespace *user_ns;    // 用户命名空间
    
    /* store ref to file /proc/<pid>/exe symlink points to */
    struct file __rcu *exe_file;       // 可执行文件
    
#ifdef CONFIG_MMU_NOTIFIER  
    struct mmu_notifier_mm *mmu_notifier_mm;
#endif
#if defined(CONFIG_TRANSPARENT_HUGEPAGE) && !USE_SPLIT_PMD_PTLOCKS
    pgtable_t pmd_huge_pte;           // 巨型页 PMD
#endif
#ifdef CONFIG_NUMA_BALANCING
    unsigned long numa_next_scan;      // NUMA 下次扫描时间
    unsigned long numa_scan_offset;    // NUMA 扫描偏移
    int numa_scan_seq;                // NUMA 扫描序列
#endif
#if defined(CONFIG_NUMA_BALANCING) || defined(CONFIG_COMPACTION)
    int tlb_flush_pending;            // TLB 刷新待处理
#endif
    struct uprobes_state uprobes_state; // 用户态探针状态
#ifdef CONFIG_HUGETLB_PAGE  
    atomic_long_t hugetlb_usage;      // 巨型页使用量
#endif
    struct work_struct async_put_work; // 异步释放工作
};
```

**作用**: 描述进程的完整内存布局  
**生命周期**: 进程创建时分配 → 进程退出时释放  
**关键字段**:
- `mmap/mm_rb`: 管理所有 VMA
- `pgd`: 指向顶级页表
- `mmap_sem`: 保护 VMA 操作的读写信号量

### 3.3 struct vm_operations_struct - VMA 操作表

```c
struct vm_operations_struct {
    void (*open)(struct vm_area_struct * area);
    void (*close)(struct vm_area_struct * area);
    int (*split)(struct vm_area_struct * area, unsigned long addr);
    int (*mremap)(struct vm_area_struct * area);
    
    /* 缺页处理 */
    vm_fault_t (*fault)(struct vm_fault *vmf);      // 🔥 主要缺页处理
    vm_fault_t (*huge_fault)(struct vm_fault *vmf, enum page_entry_size pe_size);
    void (*map_pages)(struct vm_fault *vmf,         // 批量映射页面
            pgoff_t start_pgoff, pgoff_t end_pgoff);
    
    /* 页面换出 */  
    unsigned long (*pagesize)(struct vm_area_struct * area);
    
    /* 访问权限 */
    int (*access)(struct vm_area_struct *vma, unsigned long addr,
                  void *buf, int len, int write);
                  
    /* 名称 (用于 /proc/PID/maps) */
    const char *(*name)(struct vm_area_struct *vma);
    
#ifdef CONFIG_NUMA
    int (*set_policy)(struct vm_area_struct *vma, struct mempolicy *new);
    struct mempolicy *(*get_policy)(struct vm_area_struct *vma,
                                   unsigned long addr);
#endif
    struct page *(*find_special_page)(struct vm_area_struct *vma,
                                     unsigned long addr);
};
```

**作用**: 定义 VMA 的具体操作方法，实现多态  
**关键操作**:
- `fault`: 处理页面缺失，从文件或匿名页分配
- `map_pages`: 批量预映射页面，提高性能

---

## ⚙️ 第四部分：最小可运行实验

### 4.1 基础 mmap 测试

```c
// demo_mmap_basic.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

void print_mapping_info(void *addr, size_t size) {
    printf("  映射地址: %p\n", addr);
    printf("  映射大小: %zu 字节 (%zu KB)\n", size, size / 1024);
    
    // 尝试读取 /proc/self/maps 来验证映射
    char maps_cmd[256];
    snprintf(maps_cmd, sizeof(maps_cmd), 
             "grep '%lx' /proc/self/maps | head -1", 
             (unsigned long)addr);
    printf("  映射信息: ");
    fflush(stdout);
    system(maps_cmd);
}

int main() {
    printf("=== mmap 内存映射测试 ===\n");

    // 1. 测试匿名映射
    printf("\n1. 测试匿名映射 (MAP_ANONYMOUS):\n");
    size_t anon_size = 4 * 1024; // 4KB
    void *anon_addr = mmap(NULL, anon_size, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (anon_addr == MAP_FAILED) {
        perror("mmap anonymous");
        exit(1);
    }
    
    print_mapping_info(anon_addr, anon_size);
    
    // 写入数据测试
    strcpy((char*)anon_addr, "Hello, Anonymous mmap!");
    printf("  写入数据: %s\n", (char*)anon_addr);

    // 2. 测试文件映射
    printf("\n2. 测试文件映射:\n");
    
    // 创建测试文件
    const char *filename = "mmap_test_file.txt";
    int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    
    // 写入测试数据
    const char *test_data = "This is test data for mmap file mapping test. "
                           "The file will be mapped to virtual memory directly.";
    size_t data_len = strlen(test_data);
    write(fd, test_data, data_len);
    
    // 获取文件大小
    struct stat st;
    fstat(fd, &st);
    size_t file_size = st.st_size;
    
    printf("  文件大小: %zu 字节\n", file_size);
    
    // 映射文件
    void *file_addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
    
    if (file_addr == MAP_FAILED) {
        perror("mmap file");
        close(fd);
        exit(1);
    }
    
    print_mapping_info(file_addr, file_size);
    
    printf("  文件内容: %.50s...\n", (char*)file_addr);
    
    // 修改映射内容
    strncpy((char*)file_addr, "Modified", 8);
    msync(file_addr, 8, MS_SYNC); // 强制同步到文件
    printf("  已修改文件内容 (通过mmap)\n");

    // 3. 测试只读映射
    printf("\n3. 测试只读文件映射:\n");
    
    void *readonly_addr = mmap(NULL, file_size, PROT_READ,
                              MAP_PRIVATE, fd, 0);
    
    if (readonly_addr == MAP_FAILED) {
        perror("mmap readonly");
    } else {
        print_mapping_info(readonly_addr, file_size);
        printf("  只读内容: %.50s...\n", (char*)readonly_addr);
        
        // 注意：尝试写入会导致 SIGSEGV
        munmap(readonly_addr, file_size);
    }

    // 4. 测试共享映射 vs 私有映射
    printf("\n4. 映射类型对比:\n");
    
    void *shared_addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, 0);
    void *private_addr = mmap(NULL, file_size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE, fd, 0);
    
    if (shared_addr != MAP_FAILED && private_addr != MAP_FAILED) {
        printf("  MAP_SHARED 地址: %p\n", shared_addr);
        printf("  MAP_PRIVATE 地址: %p\n", private_addr);
        
        munmap(shared_addr, file_size);
        munmap(private_addr, file_size);
    }

    // 清理
    munmap(anon_addr, anon_size);
    munmap(file_addr, file_size);
    close(fd);
    unlink(filename);
    
    printf("\n5. 内存映射统计:\n");
    system("cat /proc/self/status | grep -E '^(VmSize|VmRSS|VmPeak):'");

    return 0;
}
```

### 4.2 mmap 性能对比测试

```c
// demo_mmap_performance.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>

double get_time_diff(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) + 
           (end->tv_usec - start->tv_usec) / 1000000.0;
}

void test_read_performance(const char *filename, size_t file_size) {
    struct timeval start, end;
    char *buffer = malloc(file_size);
    
    printf("测试 read() 系统调用性能:\n");
    
    gettimeofday(&start, NULL);
    
    int fd = open(filename, O_RDONLY);
    if (fd != -1) {
        ssize_t bytes_read = read(fd, buffer, file_size);
        close(fd);
        
        gettimeofday(&end, NULL);
        
        printf("  读取字节: %ld\n", bytes_read);
        printf("  耗时: %.6f 秒\n", get_time_diff(&start, &end));
    }
    
    free(buffer);
}

void test_mmap_performance(const char *filename, size_t file_size) {
    struct timeval start, end;
    
    printf("测试 mmap() 性能:\n");
    
    gettimeofday(&start, NULL);
    
    int fd = open(filename, O_RDONLY);
    if (fd != -1) {
        void *addr = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr != MAP_FAILED) {
            // 触发页面加载 (访问每个页面)
            volatile char c;
            for (size_t i = 0; i < file_size; i += 4096) {
                c = ((char*)addr)[i];
            }
            
            gettimeofday(&end, NULL);
            
            printf("  映射大小: %zu 字节\n", file_size);
            printf("  耗时: %.6f 秒\n", get_time_diff(&start, &end));
            
            munmap(addr, file_size);
        }
        close(fd);
    }
}

int main() {
    printf("=== mmap vs read 性能对比测试 ===\n");
    
    const char *filename = "performance_test_file.dat";
    const size_t file_size = 10 * 1024 * 1024; // 10MB
    
    // 创建测试文件
    printf("创建 %zu MB 测试文件...\n", file_size / (1024 * 1024));
    
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("create test file");
        return 1;
    }
    
    // 写入随机数据
    for (size_t i = 0; i < file_size; i += 4096) {
        char page[4096];
        memset(page, i % 256, sizeof(page));
        write(fd, page, sizeof(page));
    }
    close(fd);
    
    printf("\n");
    
    // 清空缓存以获得公平的比较
    printf("清空系统缓存...\n");
    system("sync && echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true");
    
    // 测试 read() 性能
    test_read_performance(filename, file_size);
    
    printf("\n");
    
    // 再次清空缓存
    system("sync && echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true");
    
    // 测试 mmap() 性能  
    test_mmap_performance(filename, file_size);
    
    // 清理
    unlink(filename);
    
    return 0;
}
```

### 4.3 编译和运行

```bash
# 编译基础测试
gcc -o demo_mmap_basic demo_mmap_basic.c

# 编译性能测试
gcc -o demo_mmap_performance demo_mmap_performance.c

# 运行测试
./demo_mmap_basic
./demo_mmap_performance
```

### 4.4 触发的内核行为

这些测试会触发：

1. **sys_mmap()** - 创建虚拟内存映射
2. **do_page_fault()** - 首次访问时的缺页中断
3. **filemap_fault()** - 文件映射的页面加载
4. **页缓存操作** - 文件数据的统一缓存
5. **页表管理** - PTE 设置和 TLB 更新

---

## 🔍 第五部分：可观测性 & Debug 方法

### 5.1 使用 strace 观察 mmap 行为

```bash
# 跟踪 mmap 相关系统调用
strace -e trace=mmap,munmap,mprotect,msync ./demo_mmap_basic

# 详细显示内存映射
strace -e trace=mmap -v ./demo_mmap_basic
```

**期望输出**：
```
mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f8a1b000000
mmap(NULL, 92, PROT_READ|PROT_WRITE, MAP_SHARED, 3, 0) = 0x7f8a1afff000
```

### 5.2 观察进程虚拟内存布局

```bash
# 查看进程内存映射
cat /proc/self/maps

# 实时监控特定进程的内存映射
watch -n 1 "cat /proc/\$(pgrep demo_mmap)/maps"

# 查看内存使用统计
cat /proc/self/status | grep -E "^Vm"
```

### 5.3 使用 perf 观察内存事件

```bash
# 记录页面错误和内存分配
sudo perf record -e page-faults,minor-faults,major-faults \
    ./demo_mmap_basic

# 查看事件统计
sudo perf stat -e page-faults,minor-faults,major-faults \
    ./demo_mmap_performance

# 查看调用栈
sudo perf script
```

### 5.4 使用 ftrace 跟踪内存管理

```bash
# 跟踪 mmap 相关内核函数
echo function > /sys/kernel/debug/tracing/current_tracer
echo 'vm_mmap_pgoff do_page_fault handle_mm_fault' > \
    /sys/kernel/debug/tracing/set_ftrace_filter
echo 1 > /sys/kernel/debug/tracing/tracing_on

# 运行测试
./demo_mmap_basic

# 查看跟踪结果  
cat /sys/kernel/debug/tracing/trace

# 清理
echo 0 > /sys/kernel/debug/tracing/tracing_on
```

### 5.5 观察页缓存和内存压力

```bash
# 查看页缓存统计
cat /proc/meminfo | grep -E "(Cached|Buffers|Mapped)"

# 查看 slab 分配器统计 (VMA 对象)
cat /proc/slabinfo | grep vm_area_struct

# 观察内存分配
cat /proc/buddyinfo

# 查看 TLB 统计 (如果支持)
cat /proc/vmstat | grep tlb
```

---

## ⚡ 第六部分：性能与设计权衡

### 6.1 性能瓶颈分析

1. **缺页中断开销**
   - 首次访问需要缺页处理
   - 每次缺页约 1000-3000 个 CPU 周期
   - 预读可以减少缺页次数

2. **TLB 缓存命中率**
   - TLB 未命中需要页表遍历
   - 大页 (Huge Pages) 可以提高命中率
   - 频繁的内存访问模式影响 TLB 效率

3. **页表开销**
   - 多级页表遍历开销
   - 页表占用额外内存空间
   - 写时复制 (COW) 增加复杂度

### 6.2 mmap 设计权衡

**mmap vs read/write**

| 特性 | mmap | read/write |
|------|------|------------|
| **内存拷贝** | 零拷贝 | 内核 ↔ 用户态拷贝 |
| **首次访问开销** | 缺页中断 | 系统调用 |
| **随机访问** | 高效 (页面粒度) | 需要 lseek |
| **顺序访问** | 可能不如 read | 优化良好 |
| **内存使用** | 可能更多 (页对齐) | 精确控制 |

**私有映射 vs 共享映射**

```c
// 私有映射 (MAP_PRIVATE) - 写时复制
void *private_addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE, fd, 0);

// 共享映射 (MAP_SHARED) - 直接修改文件  
void *shared_addr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, 0);
```

**匿名映射 vs 文件映射**
- **匿名映射**: 适合动态内存分配，类似 malloc
- **文件映射**: 适合文件 I/O，零拷贝访问

### 6.3 优化策略

1. **使用大页**
```c
// 申请 2MB 大页
void *addr = mmap(NULL, 2 * 1024 * 1024, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
```

2. **预读优化**
```c
// 建议内核预读模式
madvise(addr, size, MADV_SEQUENTIAL);  // 顺序访问
madvise(addr, size, MADV_RANDOM);      // 随机访问
madvise(addr, size, MADV_WILLNEED);    // 即将访问
```

3. **内存锁定**
```c
// 锁定内存防止换出
mlock(addr, size);
```

---

## 🔗 第七部分：横向对比

### 7.1 mmap vs malloc

| 特性 | mmap | malloc |
|------|------|--------|
| **分配机制** | 系统调用，VMA 管理 | 用户态堆管理 |
| **最小单位** | 页面 (4KB) | 字节级 |
| **释放方式** | munmap() | free() |
| **适用场景** | 大块内存，文件映射 | 小对象，频繁分配 |

### 7.2 mmap vs sendfile

```c
// mmap 方式传输文件
void *src = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, src_fd, 0);
write(dest_fd, src, file_size);
munmap(src, file_size);

// sendfile 方式传输文件  
sendfile(dest_fd, src_fd, NULL, file_size);
```

**sendfile 优势**: 
- 真正的零拷贝 (内核到内核)
- 无需创建 VMA
- 更适合网络传输

### 7.3 不同 OS 的内存映射

**Linux mmap vs Windows VirtualAlloc**
```c
// Linux
void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

// Windows
LPVOID addr = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, 
                          PAGE_READWRITE);
```

**Linux mmap vs macOS mmap**
- 基本接口相同 (POSIX 标准)
- 实现细节不同 (页表结构，缺页处理)

---

## 🧠 第八部分：一句话本质总结

> **mmap 的本质是将文件或匿名内存通过虚拟内存管理器映射到进程地址空间，实现零拷贝访问和统一的内存模型，通过缺页机制按需加载数据。**

---

## 📌 下一步学习

掌握了 mmap 内存映射后，建议继续学习：
1. **[epoll 多路复用](04-epoll.md)** - 基于文件描述符的高效事件处理
2. **[TCP 收包流程](05-tcp-recv.md)** - 网络数据的内存管理

---

## 🔖 关键要点回顾

- ✅ mmap 通过 VMA 实现虚拟内存到物理页面的映射
- ✅ 缺页中断是 mmap 性能的关键机制
- ✅ 页缓存统一了文件 I/O 和内存访问
- ✅ MAP_SHARED 和 MAP_PRIVATE 有不同的写时行为
- ✅ 理解了零拷贝的实现原理和适用场景