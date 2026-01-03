# WHERE｜源代码地图

## 1. mm/ 目录结构

```
MM/ DIRECTORY STRUCTURE
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  mm/                                                                     │ |
|  │  ├── Makefile                                                            │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  CORE ADDRESS SPACE MANAGEMENT                                        │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── memory.c          ◄── Core page fault handling                     │ |
|  │  │                         handle_mm_fault(), do_page_fault()           │ |
|  │  │                         do_anonymous_page(), do_wp_page()            │ |
|  │  │                                                                       │ |
|  │  ├── mmap.c            ◄── VMA management                               │ |
|  │  │                         mmap(), munmap(), brk()                      │ |
|  │  │                         find_vma(), insert_vm_struct()               │ |
|  │  │                                                                       │ |
|  │  ├── mprotect.c        ◄── Memory protection                            │ |
|  │  │                         mprotect() implementation                    │ |
|  │  │                                                                       │ |
|  │  ├── mremap.c          ◄── Remapping memory                             │ |
|  │  │                         mremap() implementation                      │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  PAGE ALLOCATION                                                      │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── page_alloc.c      ◄── Buddy allocator (core)                       │ |
|  │  │                         __alloc_pages(), __free_pages()              │ |
|  │  │                         Zone management, watermarks                  │ |
|  │  │                                                                       │ |
|  │  ├── vmalloc.c         ◄── Non-contiguous allocation                    │ |
|  │  │                         vmalloc(), vfree()                           │ |
|  │  │                         Kernel virtual address management            │ |
|  │  │                                                                       │ |
|  │  ├── percpu.c          ◄── Per-CPU memory allocation                    │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  SLAB/SLUB ALLOCATORS                                                 │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── slab.c            ◄── SLAB allocator (older)                       │ |
|  │  ├── slub.c            ◄── SLUB allocator (default, modern)             │ |
|  │  ├── slob.c            ◄── SLOB allocator (tiny systems)                │ |
|  │  ├── slab_common.c     ◄── Common slab code                             │ |
|  │  │                         kmem_cache_create(), kmalloc()               │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  PAGE RECLAIM AND SWAP                                                │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── vmscan.c          ◄── Page reclaim (LRU, kswapd)                   │ |
|  │  │                         shrink_lruvec(), try_to_free_pages()         │ |
|  │  │                                                                       │ |
|  │  ├── swap.c            ◄── Swap entry management                        │ |
|  │  ├── swapfile.c        ◄── Swap file/partition handling                 │ |
|  │  ├── swap_state.c      ◄── Swap cache                                   │ |
|  │  │                                                                       │ |
|  │  ├── oom_kill.c        ◄── OOM killer                                   │ |
|  │  │                         out_of_memory(), select_bad_process()        │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  FILE-BACKED MEMORY                                                   │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── filemap.c         ◄── Page cache operations                        │ |
|  │  │                         find_get_page(), add_to_page_cache()         │ |
|  │  │                         filemap_fault()                              │ |
|  │  │                                                                       │ |
|  │  ├── readahead.c       ◄── Read-ahead logic                             │ |
|  │  ├── truncate.c        ◄── File truncation                              │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  REVERSE MAPPING                                                      │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── rmap.c            ◄── Reverse mapping (page→VMA)                   │ |
|  │  │                         page_referenced(), try_to_unmap()            │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  HUGE PAGES                                                           │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── hugetlb.c         ◄── Explicit huge pages                          │ |
|  │  ├── huge_memory.c     ◄── Transparent Huge Pages (THP)                 │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  MEMORY POLICY AND CGROUPS                                            │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── mempolicy.c       ◄── NUMA memory policy                           │ |
|  │  ├── memcontrol.c      ◄── Memory cgroups                               │ |
|  │  │                                                                       │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │  MISC                                                                 │ |
|  │  │  ═══════════════════════════════════════════════════════════════     │ |
|  │  │                                                                       │ |
|  │  ├── init-mm.c         ◄── Initial mm_struct                            │ |
|  │  ├── mlock.c           ◄── Page locking                                 │ |
|  │  ├── migrate.c         ◄── Page migration                               │ |
|  │  ├── compaction.c      ◄── Memory compaction                            │ |
|  │  └── ...                                                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  ARCH-SPECIFIC MM (arch/x86/mm/, arch/arm64/mm/)                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  arch/x86/mm/                                                            │ |
|  │  ├── fault.c           ◄── x86 page fault entry                         │ |
|  │  │                         do_page_fault()                              │ |
|  │  ├── pgtable.c         ◄── Page table operations                        │ |
|  │  ├── tlb.c             ◄── TLB flush operations                         │ |
|  │  ├── init.c            ◄── MM initialization                            │ |
|  │  └── ...                                                                 │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**mm/ 目录结构**：

1. **核心地址空间管理**：
   - `memory.c`：页错误处理核心
   - `mmap.c`：VMA 管理（`mmap()`、`munmap()`）
   - `mprotect.c`、`mremap.c`：内存保护和重映射

2. **页分配**：
   - `page_alloc.c`：伙伴分配器
   - `vmalloc.c`：非连续分配
   - `percpu.c`：per-CPU 内存

3. **SLAB/SLUB 分配器**：
   - `slab.c`、`slub.c`、`slob.c`：三种实现
   - `slab_common.c`：公共代码

4. **页回收和交换**：
   - `vmscan.c`：LRU、kswapd
   - `swap*.c`：交换管理
   - `oom_kill.c`：OOM killer

5. **文件支持内存**：
   - `filemap.c`：页缓存操作
   - `readahead.c`：预读

6. **反向映射**：
   - `rmap.c`：页→VMA 反向映射

7. **巨页**：
   - `hugetlb.c`：显式巨页
   - `huge_memory.c`：透明巨页（THP）

---

## 2. 架构锚点

```
ARCHITECTURAL ANCHORS: MM_STRUCT
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  MM_STRUCT AS THE CENTRAL HUB                                            │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │                         task_struct                                │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              │ ->mm                                 │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │  ┌───────────────────────────────────────────────────────────┐    │   │ |
|  │  │  │                      mm_struct                             │    │   │ |
|  │  │  │                                                            │    │   │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │    │   │ |
|  │  │  │  │  VMA MANAGEMENT                                      │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  │  mmap ──────► VMA ──► VMA ──► VMA                   │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  │  mm_rb (RB-tree for O(log n) lookup)                 │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │    │   │ |
|  │  │  │                         │                                  │    │   │ |
|  │  │  │                         ▼                                  │    │   │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │    │   │ |
|  │  │  │  │  PAGE TABLES                                         │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  │  pgd ──► PGD ──► PUD ──► PMD ──► PTE ──► Physical   │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │    │   │ |
|  │  │  │                         │                                  │    │   │ |
|  │  │  │                         ▼                                  │    │   │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │    │   │ |
|  │  │  │  │  STATISTICS                                          │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  │  total_vm, locked_vm, rss_stat                       │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │    │   │ |
|  │  │  │                         │                                  │    │   │ |
|  │  │  │                         ▼                                  │    │   │ |
|  │  │  │  ┌─────────────────────────────────────────────────────┐   │    │   │ |
|  │  │  │  │  MEMORY LIMITS                                       │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  │  start_brk, brk, start_stack                         │   │    │   │ |
|  │  │  │  │  start_code, end_code, start_data, end_data          │   │    │   │ |
|  │  │  │  │                                                      │   │    │   │ |
|  │  │  │  └─────────────────────────────────────────────────────┘   │    │   │ |
|  │  │  │                                                            │    │   │ |
|  │  │  └───────────────────────────────────────────────────────────┘    │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  LOCATION: include/linux/mm_types.h                                      │ |
|  │                                                                          │ |
|  │  KEY FUNCTIONS OPERATING ON MM_STRUCT:                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Creation/Destruction:                                           │    │ |
|  │  │  • mm_alloc()           Allocate new mm_struct                   │    │ |
|  │  │  • mmput(mm)            Release reference                        │    │ |
|  │  │  • copy_mm()            Duplicate for fork()                     │    │ |
|  │  │  • exit_mm()            Clean up on process exit                 │    │ |
|  │  │                                                                  │    │ |
|  │  │  VMA Operations:                                                 │    │ |
|  │  │  • find_vma(mm, addr)   Find VMA containing address              │    │ |
|  │  │  • insert_vm_struct()   Insert new VMA                           │    │ |
|  │  │  • do_mmap()            Create new mapping                       │    │ |
|  │  │  • do_munmap()          Remove mapping                           │    │ |
|  │  │                                                                  │    │ |
|  │  │  Page Table Operations:                                          │    │ |
|  │  │  • pgd_alloc(mm)        Allocate page table root                 │    │ |
|  │  │  • pgd_free(mm, pgd)    Free page table                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**mm_struct 作为中心枢纽**：

连接所有内存管理组件：
- **VMA 管理**：`mmap` 链表和 `mm_rb` 红黑树
- **页表**：`pgd` → 多级页表 → 物理页
- **统计**：`total_vm`、`locked_vm`、`rss_stat`
- **内存限制**：堆、栈、代码、数据的边界

**位置**：`include/linux/mm_types.h`

**关键函数**：
- 创建/销毁：`mm_alloc()`、`mmput()`、`copy_mm()`、`exit_mm()`
- VMA 操作：`find_vma()`、`insert_vm_struct()`、`do_mmap()`、`do_munmap()`
- 页表操作：`pgd_alloc()`、`pgd_free()`

---

## 3. 控制中心

```
CONTROL HUBS: DO_PAGE_FAULT() AND FRIENDS
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PAGE FAULT CONTROL FLOW                                                 │ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  ENTRY (arch-specific):                                            │   │ |
|  │  │                                                                    │   │ |
|  │  │  arch/x86/mm/fault.c:                                              │   │ |
|  │  │  do_page_fault(regs, error_code)                                   │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── address = read_cr2()  /* Get fault address */             │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── Check for kernel fault:                                   │   │ |
|  │  │      │   • vmalloc area → vmalloc_fault()                          │   │ |
|  │  │      │   • oops area → kernel_oops()                               │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── Check for special cases:                                  │   │ |
|  │  │      │   • vsyscall emulation                                      │   │ |
|  │  │      │   • reserved bits violation                                 │   │ |
|  │  │      │                                                             │   │ |
|  │  │      └── __do_page_fault(regs, error_code, address)                │   │ |
|  │  │              │                                                     │   │ |
|  │  │              ▼                                                     │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  GENERIC HANDLING (mm/memory.c):                                         │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  handle_mm_fault(mm, vma, address, flags)                          │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── Check VMA permissions                                     │   │ |
|  │  │      │                                                             │   │ |
|  │  │      ├── __handle_mm_fault()                                       │   │ |
|  │  │      │       │                                                     │   │ |
|  │  │      │       ├── Allocate page table levels:                       │   │ |
|  │  │      │       │   pud_alloc() → pmd_alloc() → pte_alloc()          │   │ |
|  │  │      │       │                                                     │   │ |
|  │  │      │       └── handle_pte_fault()                                │   │ |
|  │  │      │               │                                             │   │ |
|  │  │      │               ├── !pte_present:                             │   │ |
|  │  │      │               │   ├── pte_none: do_fault() or              │   │ |
|  │  │      │               │   │             do_anonymous_page()         │   │ |
|  │  │      │               │   └── swapped: do_swap_page()               │   │ |
|  │  │      │               │                                             │   │ |
|  │  │      │               ├── pte_present && write_fault:               │   │ |
|  │  │      │               │   └── do_wp_page() (COW)                    │   │ |
|  │  │      │               │                                             │   │ |
|  │  │      │               └── pte_present && read_fault:                │   │ |
|  │  │      │                   └── Usually just update accessed bit      │   │ |
|  │  │      │                                                             │   │ |
|  │  │      └── Return fault result                                       │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  FAULT HANDLERS:                                                         │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  do_anonymous_page()  (mm/memory.c)                                │   │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐  │   │ |
|  │  │  │  • Allocate new zero-filled page                             │  │   │ |
|  │  │  │  • Set up PTE with correct permissions                       │  │   │ |
|  │  │  │  • Add page to LRU                                           │  │   │ |
|  │  │  └─────────────────────────────────────────────────────────────┘  │   │ |
|  │  │                                                                    │   │ |
|  │  │  do_fault() → __do_fault()  (mm/memory.c)                          │   │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐  │   │ |
|  │  │  │  • Call vma->vm_ops->fault(vma, vmf)                         │  │   │ |
|  │  │  │  • For files: filemap_fault() reads from disk                │  │   │ |
|  │  │  │  • Page is now in page cache                                 │  │   │ |
|  │  │  └─────────────────────────────────────────────────────────────┘  │   │ |
|  │  │                                                                    │   │ |
|  │  │  do_swap_page()  (mm/memory.c)                                     │   │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐  │   │ |
|  │  │  │  • Look up swap entry from PTE                               │  │   │ |
|  │  │  │  • Check swap cache first                                    │  │   │ |
|  │  │  │  • If not cached: read from swap device                      │  │   │ |
|  │  │  │  • Set up PTE pointing to page                               │  │   │ |
|  │  │  └─────────────────────────────────────────────────────────────┘  │   │ |
|  │  │                                                                    │   │ |
|  │  │  do_wp_page()  (mm/memory.c)                                       │   │ |
|  │  │  ┌─────────────────────────────────────────────────────────────┐  │   │ |
|  │  │  │  • Check if page is shared                                   │  │   │ |
|  │  │  │  • If only one reference: just make writable                 │  │   │ |
|  │  │  │  • If shared: allocate new page, copy contents               │  │   │ |
|  │  │  │  • Update PTE to point to new page                           │  │   │ |
|  │  │  └─────────────────────────────────────────────────────────────┘  │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**页错误控制流**：

1. **入口（架构特定）**：
   - `arch/x86/mm/fault.c`: `do_page_fault()`
   - 读取 CR2 获取错误地址
   - 检查内核错误、特殊情况

2. **通用处理**（`mm/memory.c`）：
   - `handle_mm_fault()`：检查 VMA 权限
   - `__handle_mm_fault()`：分配页表级别
   - `handle_pte_fault()`：根据情况分发

3. **错误处理器**：
   - `do_anonymous_page()`：分配零填充页
   - `do_fault()` → `vma->vm_ops->fault()`：从文件读取
   - `do_swap_page()`：从交换读取
   - `do_wp_page()`：写时复制

---

## 4. 在代码中验证抽象

```
VALIDATING ABSTRACTIONS IN CODE
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  HOW TO VERIFY ARCHITECTURAL INTENT:                                     │ |
|  │                                                                          │ |
|  │  1. TRACE VMA LOOKUP                                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* mm/mmap.c */                                                 │    │ |
|  │  │  struct vm_area_struct *find_vma(struct mm_struct *mm,           │    │ |
|  │  │                                  unsigned long addr)             │    │ |
|  │  │  {                                                               │    │ |
|  │  │      struct vm_area_struct *vma;                                 │    │ |
|  │  │                                                                  │    │ |
|  │  │      /* Check cache first - often hits */                        │    │ |
|  │  │      vma = mm->mmap_cache;                                       │    │ |
|  │  │      if (vma && vma->vm_end > addr && vma->vm_start <= addr)     │    │ |
|  │  │          return vma;                                             │    │ |
|  │  │                                                                  │    │ |
|  │  │      /* Fall back to RB-tree search */                           │    │ |
|  │  │      vma = find_vma_rb(mm, addr);                                │    │ |
|  │  │                                                                  │    │ |
|  │  │      if (vma)                                                    │    │ |
|  │  │          mm->mmap_cache = vma;  /* Update cache */               │    │ |
|  │  │                                                                  │    │ |
|  │  │      return vma;                                                 │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  INSIGHT: Shows cache + RB-tree optimization                     │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  2. TRACE PAGE ALLOCATION                                                │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* mm/page_alloc.c */                                           │    │ |
|  │  │  struct page *__alloc_pages_nodemask(gfp_t gfp_mask,             │    │ |
|  │  │                                      unsigned int order, ...)    │    │ |
|  │  │  {                                                               │    │ |
|  │  │      /* Try fast path first - per-CPU lists */                   │    │ |
|  │  │      page = get_page_from_freelist(gfp_mask, order, ...);        │    │ |
|  │  │      if (page)                                                   │    │ |
|  │  │          return page;                                            │    │ |
|  │  │                                                                  │    │ |
|  │  │      /* Slow path - try harder */                                │    │ |
|  │  │      page = __alloc_pages_slowpath(gfp_mask, order, ...);        │    │ |
|  │  │          /* May trigger: */                                      │    │ |
|  │  │          /* - Direct reclaim */                                  │    │ |
|  │  │          /* - Compaction */                                      │    │ |
|  │  │          /* - OOM killer */                                      │    │ |
|  │  │                                                                  │    │ |
|  │  │      return page;                                                │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  INSIGHT: Shows fast path/slow path separation                   │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  3. TRACE VMA OPS-TABLE DISPATCH                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* mm/memory.c: do_fault() */                                   │    │ |
|  │  │  static int __do_fault(struct vm_area_struct *vma,               │    │ |
|  │  │                        struct vm_fault *vmf)                     │    │ |
|  │  │  {                                                               │    │ |
|  │  │      /* Polymorphic dispatch! */                                 │    │ |
|  │  │      ret = vma->vm_ops->fault(vma, vmf);                         │    │ |
|  │  │      ...                                                         │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Different implementations: */                                │    │ |
|  │  │  /* mm/filemap.c: filemap_fault() for regular files */           │    │ |
|  │  │  /* mm/shmem.c: shmem_fault() for tmpfs */                       │    │ |
|  │  │  /* drivers: device-specific fault handlers */                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  INSIGHT: Shows ops-table polymorphism                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  4. TRACE REFERENCE COUNTING                                             │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* include/linux/mm.h */                                        │    │ |
|  │  │  static inline void get_page(struct page *page)                  │    │ |
|  │  │  {                                                               │    │ |
|  │  │      VM_BUG_ON(atomic_read(&page->_count) <= 0);                 │    │ |
|  │  │      atomic_inc(&page->_count);                                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  static inline void put_page(struct page *page)                  │    │ |
|  │  │  {                                                               │    │ |
|  │  │      if (put_page_testzero(page))                                │    │ |
|  │  │          __put_page(page);  /* Actually free */                  │    │ |
|  │  │  }                                                               │    │ |
|  │  │                                                                  │    │ |
|  │  │  INSIGHT: Shows explicit lifecycle management                    │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**在代码中验证架构意图**：

1. **追踪 VMA 查找**：
   - `find_vma()` 显示缓存 + 红黑树优化
   - 先检查 `mmap_cache`，未命中则搜索红黑树

2. **追踪页分配**：
   - `__alloc_pages_nodemask()` 显示快速路径/慢路径分离
   - 快速路径：per-CPU 列表
   - 慢路径：直接回收、压缩、OOM killer

3. **追踪 VMA 操作表分发**：
   - `vma->vm_ops->fault()` 显示多态分发
   - 不同实现：`filemap_fault()`（普通文件）、`shmem_fault()`（tmpfs）

4. **追踪引用计数**：
   - `get_page()`/`put_page()` 显示显式生命周期管理

---

## 5. 阅读顺序

```
RECOMMENDED READING ORDER
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PHASE 1: UNDERSTAND DATA STRUCTURES (理解数据结构)                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. include/linux/mm_types.h                                     │    │ |
|  │  │     • struct mm_struct                                           │    │ |
|  │  │     • struct vm_area_struct                                      │    │ |
|  │  │     • struct page                                                │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. include/linux/mm.h                                           │    │ |
|  │  │     • Page flags (PG_*)                                          │    │ |
|  │  │     • VMA flags (VM_*)                                           │    │ |
|  │  │     • get_page(), put_page()                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  3. include/asm-generic/pgtable.h                                │    │ |
|  │  │     • Page table entry operations                                │    │ |
|  │  │     • pte_present(), pte_write(), etc.                           │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 2: UNDERSTAND PAGE FAULT PATH (理解页错误路径)                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  4. arch/x86/mm/fault.c                                          │    │ |
|  │  │     • do_page_fault() - entry point                              │    │ |
|  │  │     • Error code decoding                                        │    │ |
|  │  │                                                                  │    │ |
|  │  │  5. mm/memory.c                                                  │    │ |
|  │  │     • handle_mm_fault() - main handler                           │    │ |
|  │  │     • handle_pte_fault() - PTE-level handling                    │    │ |
|  │  │     • do_anonymous_page() - anonymous page allocation            │    │ |
|  │  │     • do_wp_page() - copy-on-write                               │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 3: UNDERSTAND VMA MANAGEMENT (理解 VMA 管理)                      │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  6. mm/mmap.c                                                    │    │ |
|  │  │     • do_mmap() - create mapping                                 │    │ |
|  │  │     • do_munmap() - remove mapping                               │    │ |
|  │  │     • find_vma() - lookup                                        │    │ |
|  │  │     • insert_vm_struct() - insert VMA                            │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 4: UNDERSTAND PAGE ALLOCATION (理解页分配)                        │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  7. mm/page_alloc.c                                              │    │ |
|  │  │     • __alloc_pages_nodemask() - page allocator entry            │    │ |
|  │  │     • get_page_from_freelist() - fast path                       │    │ |
|  │  │     • __free_pages() - free pages                                │    │ |
|  │  │     • Zone watermarks and balancing                              │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 5: UNDERSTAND RECLAIM (理解回收)                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  8. mm/vmscan.c                                                  │    │ |
|  │  │     • shrink_lruvec() - LRU scanning                             │    │ |
|  │  │     • try_to_free_pages() - direct reclaim                       │    │ |
|  │  │     • kswapd() - background reclaim                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  9. mm/oom_kill.c                                                │    │ |
|  │  │     • out_of_memory() - OOM handling                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 6: UNDERSTAND SLAB (理解 SLAB)                                    │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  10. mm/slub.c (or mm/slab.c)                                    │    │ |
|  │  │      • kmem_cache_create() - cache creation                      │    │ |
|  │  │      • kmem_cache_alloc() - object allocation                    │    │ |
|  │  │      • Per-CPU freelist optimization                             │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  PHASE 7: ADVANCED TOPICS (高级主题)                                     │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  11. mm/rmap.c - Reverse mapping                                 │    │ |
|  │  │  12. mm/huge_memory.c - Transparent huge pages                   │    │ |
|  │  │  13. mm/mempolicy.c - NUMA policy                                │    │ |
|  │  │  14. mm/compaction.c - Memory compaction                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TIPS FOR READING:                                                       │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  • Use cscope/ctags for navigation                               │    │ |
|  │  │  • Use ftrace to observe actual execution                        │    │ |
|  │  │  • Read /proc/meminfo while tracing code                         │    │ |
|  │  │  • Use KASAN to catch memory bugs                                │    │ |
|  │  │  • Focus on data structure transitions, not just code flow       │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**推荐阅读顺序**：

**阶段 1：理解数据结构**
1. `include/linux/mm_types.h`：核心结构
2. `include/linux/mm.h`：页标志、VMA 标志
3. `include/asm-generic/pgtable.h`：页表操作

**阶段 2：理解页错误路径**
4. `arch/x86/mm/fault.c`：入口点
5. `mm/memory.c`：主处理器

**阶段 3：理解 VMA 管理**
6. `mm/mmap.c`：`do_mmap()`、`find_vma()`

**阶段 4：理解页分配**
7. `mm/page_alloc.c`：伙伴分配器

**阶段 5：理解回收**
8. `mm/vmscan.c`：LRU、kswapd
9. `mm/oom_kill.c`：OOM 处理

**阶段 6：理解 SLAB**
10. `mm/slub.c`：对象缓存

**阶段 7：高级主题**
11-14：反向映射、巨页、NUMA 策略、压缩

**阅读技巧**：使用 cscope/ctags 导航，使用 ftrace 观察执行，关注数据结构转换
