# HOW｜架构策略

## 1. Linux 如何分离虚拟内存与物理内存

```
VIRTUAL VS PHYSICAL MEMORY SEPARATION
+=============================================================================+
|                                                                              |
|  THE TWO-LAYER ABSTRACTION                                                   |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │                         USER SPACE                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │    Process A             Process B             Process C           │   │ |
|  │  │    ┌─────────┐          ┌─────────┐          ┌─────────┐          │   │ |
|  │  │    │ VA Space│          │ VA Space│          │ VA Space│          │   │ |
|  │  │    │ 0-4GB   │          │ 0-4GB   │          │ 0-4GB   │          │   │ |
|  │  │    └────┬────┘          └────┬────┘          └────┬────┘          │   │ |
|  │  │         │                    │                    │                │   │ |
|  │  └─────────┼────────────────────┼────────────────────┼────────────────┘   │ |
|  │            │                    │                    │                    │ |
|  │            │                    │                    │                    │ |
|  │  ══════════╪════════════════════╪════════════════════╪════════════════   │ |
|  │            │                    │                    │                    │ |
|  │            │                    │                    │                    │ |
|  │                         KERNEL SPACE                                      │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │              VIRTUAL MEMORY LAYER (mm/)                            │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  mm_struct         vm_area_struct        Page Tables        │   │   │ |
|  │  │  │  (per-process)     (memory regions)      (translations)     │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  Manages: address spaces, mappings, permissions, faults     │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              │ get_free_page(), __free_page()      │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │              PHYSICAL MEMORY LAYER (mm/)                           │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  struct page       Buddy Allocator     Zone Management      │   │   │ |
|  │  │  │  (page descriptors) (free page mgmt)   (DMA/Normal/High)    │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  Manages: physical pages, memory zones, page states        │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  └──────────────────────────────┼────────────────────────────────────┘   │ |
|  │                                 │                                        │ |
|  │  ═══════════════════════════════╪════════════════════════════════════   │ |
|  │                                 │                                        │ |
|  │                                 ▼                                        │ |
|  │                         HARDWARE (MMU)                                   │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │    Physical RAM          TLB              Page Table Walker       │   │ |
|  │  │    ┌────────────┐     ┌──────┐          ┌───────────────┐        │   │ |
|  │  │    │            │     │Cache │          │ HW Translation│        │   │ |
|  │  │    │  16GB RAM  │     │      │          │               │        │   │ |
|  │  │    │            │     └──────┘          └───────────────┘        │   │ |
|  │  │    └────────────┘                                                 │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**两层抽象**：

1. **虚拟内存层**（mm/）：
   - `mm_struct`：每进程地址空间描述符
   - `vm_area_struct`：内存区域（代码、数据、堆、栈）
   - 页表：虚拟到物理地址转换
   - 管理：地址空间、映射、权限、页错误

2. **物理内存层**（mm/）：
   - `struct page`：每个物理页的描述符
   - Buddy 分配器：空闲页管理
   - 区域管理：DMA、Normal、HighMem
   - 管理：物理页、内存区域、页状态

3. **硬件层（MMU）**：
   - 物理 RAM
   - TLB（缓存最近转换）
   - 页表遍历器（硬件地址转换）

---

```
KEY INSIGHT: INDIRECTION THROUGH PAGE TABLES
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Virtual Address (64-bit, x86-64):                                       │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  63    48 47    39 38    30 29    21 20    12 11         0        │   │ |
|  │  │  ┌──────┬────────┬────────┬────────┬────────┬────────────────┐    │   │ |
|  │  │  │ Sign │  PML4  │  PDP   │   PD   │   PT   │    Offset      │    │   │ |
|  │  │  │extend│ Index  │ Index  │ Index  │ Index  │   (12 bits)    │    │   │ |
|  │  │  └──────┴────┬───┴────┬───┴────┬───┴────┬───┴────────────────┘    │   │ |
|  │  │              │        │        │        │                          │   │ |
|  │  │              │9 bits  │9 bits  │9 bits  │9 bits                    │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  4-Level Page Table Walk:                                                │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  CR3 (Page Table Base)                                             │   │ |
|  │  │        │                                                           │   │ |
|  │  │        ▼                                                           │   │ |
|  │  │  ┌──────────┐                                                      │   │ |
|  │  │  │  PML4    │ ◄─── Index from bits 47-39                           │   │ |
|  │  │  │  Table   │                                                      │   │ |
|  │  │  └────┬─────┘                                                      │   │ |
|  │  │       │                                                            │   │ |
|  │  │       ▼                                                            │   │ |
|  │  │  ┌──────────┐                                                      │   │ |
|  │  │  │   PDP    │ ◄─── Index from bits 38-30                           │   │ |
|  │  │  │  Table   │      (1GB page possible here)                        │   │ |
|  │  │  └────┬─────┘                                                      │   │ |
|  │  │       │                                                            │   │ |
|  │  │       ▼                                                            │   │ |
|  │  │  ┌──────────┐                                                      │   │ |
|  │  │  │   PD     │ ◄─── Index from bits 29-21                           │   │ |
|  │  │  │  Table   │      (2MB huge page possible here)                   │   │ |
|  │  │  └────┬─────┘                                                      │   │ |
|  │  │       │                                                            │   │ |
|  │  │       ▼                                                            │   │ |
|  │  │  ┌──────────┐                                                      │   │ |
|  │  │  │   PT     │ ◄─── Index from bits 20-12                           │   │ |
|  │  │  │  Table   │                                                      │   │ |
|  │  │  └────┬─────┘                                                      │   │ |
|  │  │       │                                                            │   │ |
|  │  │       ▼                                                            │   │ |
|  │  │  Physical Frame + Offset ──► Physical Address                      │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  PAGE TABLE ENTRY (PTE) FLAGS:                                           │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  Bit   Flag         Meaning                                        │   │ |
|  │  │  ───   ────         ───────                                        │   │ |
|  │  │   0    Present      Page is in memory (vs swapped out)             │   │ |
|  │  │   1    R/W          0=Read-only, 1=Read-Write                      │   │ |
|  │  │   2    U/S          0=Kernel-only, 1=User accessible               │   │ |
|  │  │   5    Accessed     Page was read                                  │   │ |
|  │  │   6    Dirty        Page was written                               │   │ |
|  │  │   7    PS           Page Size (1=huge page)                        │   │ |
|  │  │  63    NX           No Execute (security)                          │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**通过页表的间接层**：

**虚拟地址结构（x86-64）**：
- 48 位有效地址（64 位中的高 16 位是符号扩展）
- 分为 5 部分：PML4 索引、PDP 索引、PD 索引、PT 索引、页内偏移
- 每级索引 9 位（512 个条目）

**4 级页表遍历**：
- CR3 → PML4 表 → PDP 表 → PD 表 → PT 表 → 物理帧 + 偏移

**页表条目（PTE）标志**：
- Present：页在内存中（vs 被换出）
- R/W：读写权限
- U/S：用户/内核访问权限
- Accessed/Dirty：访问/修改标志
- PS：巨页标志
- NX：不可执行（安全）

---

## 2. 地址空间如何建模

```
HOW ADDRESS SPACES ARE MODELED
+=============================================================================+
|                                                                              |
|  THE MM_STRUCT: PROCESS ADDRESS SPACE DESCRIPTOR                             |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  task_struct (process)                                                   │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  struct mm_struct *mm;  ◄─── Points to address space               │   │ |
|  │  │                                                                    │   │ |
|  │  └────────────────────────────┬─────────────────────────────────────┘   │ |
|  │                               │                                          │ |
|  │                               ▼                                          │ |
|  │  mm_struct                                                               │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  struct vm_area_struct *mmap;    ◄── Linked list of VMAs          │   │ |
|  │  │  struct rb_root mm_rb;           ◄── RB-tree for fast VMA lookup  │   │ |
|  │  │                                                                    │   │ |
|  │  │  pgd_t *pgd;                     ◄── Page table root (CR3)        │   │ |
|  │  │                                                                    │   │ |
|  │  │  atomic_t mm_users;              ◄── Number of users (threads)    │   │ |
|  │  │  atomic_t mm_count;              ◄── Reference count              │   │ |
|  │  │                                                                    │   │ |
|  │  │  unsigned long start_code, end_code;   ◄── Text segment           │   │ |
|  │  │  unsigned long start_data, end_data;   ◄── Data segment           │   │ |
|  │  │  unsigned long start_brk, brk;         ◄── Heap                   │   │ |
|  │  │  unsigned long start_stack;            ◄── Stack                  │   │ |
|  │  │                                                                    │   │ |
|  │  │  struct rw_semaphore mmap_sem;   ◄── Lock for VMA operations      │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  THREADS SHARE MM_STRUCT:                                                │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  Thread 1         Thread 2         Thread 3                        │   │ |
|  │  │  task_struct      task_struct      task_struct                     │   │ |
|  │  │      │                │                │                           │   │ |
|  │  │      │                │                │                           │   │ |
|  │  │      └────────────────┼────────────────┘                           │   │ |
|  │  │                       │                                            │   │ |
|  │  │                       ▼                                            │   │ |
|  │  │                  mm_struct (shared)                                │   │ |
|  │  │                  mm_users = 3                                      │   │ |
|  │  │                                                                    │   │ |
|  │  │  All threads in a process see the same address space              │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**mm_struct：进程地址空间描述符**

- `mmap`：VMA 链表头
- `mm_rb`：红黑树用于快速 VMA 查找
- `pgd`：页表根（对应 CR3 寄存器）
- `mm_users`：用户数（线程数）
- `mm_count`：引用计数
- 段信息：`start_code`/`end_code`、`start_data`/`end_data`、`brk`（堆）、`start_stack`
- `mmap_sem`：VMA 操作锁

**线程共享 mm_struct**：
- 同一进程的所有线程指向同一个 `mm_struct`
- `mm_users` 跟踪线程数

---

```
VM_AREA_STRUCT: MEMORY REGION DESCRIPTOR
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  VIRTUAL ADDRESS SPACE LAYOUT (typical process):                         │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  High Address (0x7FFFFFFFFFFF on x86-64)                           │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  ┌──────────────────┐                                       │   │   │ |
|  │  │  │  │ [stack]          │ VMA 6: VM_READ|VM_WRITE|VM_GROWSDOWN  │   │   │ |
|  │  │  │  │ grows down ↓     │                                       │   │   │ |
|  │  │  │  └──────────────────┘                                       │   │   │ |
|  │  │  │         ...                                                 │   │   │ |
|  │  │  │  ┌──────────────────┐                                       │   │   │ |
|  │  │  │  │ libc.so .text    │ VMA 5: VM_READ|VM_EXEC (shared)       │   │   │ |
|  │  │  │  ├──────────────────┤                                       │   │   │ |
|  │  │  │  │ libc.so .data    │ VMA 4: VM_READ|VM_WRITE (private)     │   │   │ |
|  │  │  │  └──────────────────┘                                       │   │   │ |
|  │  │  │         ...                                                 │   │   │ |
|  │  │  │  ┌──────────────────┐                                       │   │   │ |
|  │  │  │  │ [heap]           │ VMA 3: VM_READ|VM_WRITE               │   │   │ |
|  │  │  │  │ grows up ↑       │                                       │   │   │ |
|  │  │  │  └──────────────────┘                                       │   │   │ |
|  │  │  │  ┌──────────────────┐                                       │   │   │ |
|  │  │  │  │ .bss             │ VMA 2: VM_READ|VM_WRITE               │   │   │ |
|  │  │  │  ├──────────────────┤                                       │   │   │ |
|  │  │  │  │ .data            │ VMA 1: VM_READ|VM_WRITE               │   │   │ |
|  │  │  │  ├──────────────────┤                                       │   │   │ |
|  │  │  │  │ .text            │ VMA 0: VM_READ|VM_EXEC                │   │   │ |
|  │  │  │  └──────────────────┘                                       │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  Low Address (0x0)                                          │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  VM_AREA_STRUCT STRUCTURE:                                               │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  struct vm_area_struct {                                           │   │ |
|  │  │      unsigned long vm_start;     ◄── Start address of region      │   │ |
|  │  │      unsigned long vm_end;       ◄── End address (exclusive)      │   │ |
|  │  │                                                                    │   │ |
|  │  │      struct vm_area_struct *vm_next;  ◄── Next VMA in list        │   │ |
|  │  │      struct rb_node vm_rb;            ◄── RB-tree node            │   │ |
|  │  │                                                                    │   │ |
|  │  │      unsigned long vm_flags;     ◄── Permission and type flags    │   │ |
|  │  │                                                                    │   │ |
|  │  │      struct mm_struct *vm_mm;    ◄── Owning mm_struct             │   │ |
|  │  │                                                                    │   │ |
|  │  │      struct file *vm_file;       ◄── Mapped file (or NULL)        │   │ |
|  │  │      unsigned long vm_pgoff;     ◄── Offset in file (pages)       │   │ |
|  │  │                                                                    │   │ |
|  │  │      const struct vm_operations_struct *vm_ops;  ◄── Ops table!   │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  VMA FLAGS (vm_flags):                                                   │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  VM_READ      Can read from region                                 │   │ |
|  │  │  VM_WRITE     Can write to region                                  │   │ |
|  │  │  VM_EXEC      Can execute from region                              │   │ |
|  │  │  VM_SHARED    Changes are shared (vs private copy)                 │   │ |
|  │  │  VM_GROWSDOWN Stack grows down                                     │   │ |
|  │  │  VM_GROWSUP   Heap grows up                                        │   │ |
|  │  │  VM_IO        Memory-mapped I/O region                             │   │ |
|  │  │  VM_LOCKED    Cannot be swapped out                                │   │ |
|  │  │  VM_HUGETLB   Uses huge pages                                      │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**vm_area_struct：内存区域描述符**

典型进程的虚拟地址空间布局：
- `.text`（代码）：`VM_READ|VM_EXEC`
- `.data`/`.bss`（数据）：`VM_READ|VM_WRITE`
- `[heap]`（堆）：`VM_READ|VM_WRITE`，向上增长
- 共享库（如 libc.so）：`.text` 共享只读，`.data` 私有可写
- `[stack]`（栈）：`VM_READ|VM_WRITE|VM_GROWSDOWN`

**VMA 结构**：
- `vm_start`/`vm_end`：区域起止地址
- `vm_next`/`vm_rb`：链表和红黑树节点
- `vm_flags`：权限和类型标志
- `vm_file`：映射的文件（或 NULL）
- `vm_ops`：操作表（多态）！

---

## 3. 页面生命周期如何管理

```
PAGE LIFECYCLE MANAGEMENT
+=============================================================================+
|                                                                              |
|  PAGE STATES AND TRANSITIONS                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │                        PAGE LIFECYCLE                            │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌─────────┐                                                     │    │ |
|  │  │  │  FREE   │ ◄──────────────────────────────────────────┐        │    │ |
|  │  │  │         │                                             │        │    │ |
|  │  │  └────┬────┘                                             │        │    │ |
|  │  │       │                                                  │        │    │ |
|  │  │       │ alloc_page()                                     │        │    │ |
|  │  │       │ get_free_page()                                  │        │    │ |
|  │  │       ▼                                                  │        │    │ |
|  │  │  ┌─────────┐                                             │        │    │ |
|  │  │  │ALLOCATED│                                             │        │    │ |
|  │  │  │(in use) │                                             │        │    │ |
|  │  │  └────┬────┘                                             │        │    │ |
|  │  │       │                                                  │        │    │ |
|  │  │       ├──────────────────┐                               │        │    │ |
|  │  │       │                  │                               │        │    │ |
|  │  │       ▼                  ▼                               │        │    │ |
|  │  │  ┌─────────┐        ┌─────────┐                          │        │    │ |
|  │  │  │  ANON   │        │  FILE   │                          │        │    │ |
|  │  │  │ MAPPED  │        │ BACKED  │                          │        │    │ |
|  │  │  └────┬────┘        └────┬────┘                          │        │    │ |
|  │  │       │                  │                               │        │    │ |
|  │  │       │                  │ (can be reclaimed             │        │    │ |
|  │  │       │                  │  and re-read from file)       │        │    │ |
|  │  │       │                  │                               │        │    │ |
|  │  │       ├──────────────────┘                               │        │    │ |
|  │  │       │                                                  │        │    │ |
|  │  │       │ Memory pressure                                  │        │    │ |
|  │  │       ▼                                                  │        │    │ |
|  │  │  ┌─────────┐       ┌─────────┐                           │        │    │ |
|  │  │  │  DIRTY  │──────►│ WRITEBACK │──────┐                  │        │    │ |
|  │  │  │         │ (I/O) │         │       │                   │        │    │ |
|  │  │  └─────────┘       └─────────┘       │                   │        │    │ |
|  │  │                                      │                   │        │    │ |
|  │  │                                      ▼                   │        │    │ |
|  │  │                               ┌─────────┐                │        │    │ |
|  │  │                               │  CLEAN  │                │        │    │ |
|  │  │                               │         │────────────────┘        │    │ |
|  │  │                               └────┬────┘                         │    │ |
|  │  │                                    │                              │    │ |
|  │  │                                    │ Reclaim                      │    │ |
|  │  │                                    ▼                              │    │ |
|  │  │                               ┌─────────┐                         │    │ |
|  │  │       ┌───────────────────────│ SWAPPED │                         │    │ |
|  │  │       │                       │  OUT    │                         │    │ |
|  │  │       │                       └────┬────┘                         │    │ |
|  │  │       │                            │                              │    │ |
|  │  │       │ Page fault                 │ Page freed                   │    │ |
|  │  │       │ (swap in)                  │ (no longer needed)           │    │ |
|  │  │       ▼                            │                              │    │ |
|  │  │  ┌─────────┐                       │                              │    │ |
|  │  │  │ALLOCATED│◄──────────────────────┘                              │    │ |
|  │  │  └─────────┘                                                      │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**页面生命周期**：

1. **FREE**（空闲）：在伙伴系统空闲列表中
2. **ALLOCATED**（已分配）：通过 `alloc_page()` 分配
3. **ANON MAPPED**（匿名映射）：进程私有内存（堆、栈）
4. **FILE BACKED**（文件支持）：映射自文件，可回收并重新从文件读取
5. **DIRTY**（脏）：已修改，需要写回
6. **WRITEBACK**（写回中）：正在写入磁盘
7. **CLEAN**（干净）：与磁盘同步
8. **SWAPPED OUT**（已换出）：移到交换空间

---

```
STRUCT PAGE: THE PAGE DESCRIPTOR
+=============================================================================+
|                                                                              |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  struct page (simplified):                                               │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  unsigned long flags;         ◄── Page state flags                │   │ |
|  │  │                                   PG_locked, PG_dirty, PG_active  │   │ |
|  │  │                                   PG_slab, PG_compound, etc.      │   │ |
|  │  │                                                                    │   │ |
|  │  │  atomic_t _count;             ◄── Reference count                 │   │ |
|  │  │                                   0 = free, >0 = in use           │   │ |
|  │  │                                                                    │   │ |
|  │  │  atomic_t _mapcount;          ◄── Number of page table mappings   │   │ |
|  │  │                                   -1 = unmapped, >=0 = mapped     │   │ |
|  │  │                                                                    │   │ |
|  │  │  union {                      ◄── Multipurpose union              │   │ |
|  │  │      struct {                                                      │   │ |
|  │  │          struct address_space *mapping;  /* File or anon_vma */   │   │ |
|  │  │          pgoff_t index;                  /* Offset in file */     │   │ |
|  │  │      };                                                            │   │ |
|  │  │      struct {                                                      │   │ |
|  │  │          struct kmem_cache *slab_cache;  /* For SLAB pages */     │   │ |
|  │  │      };                                                            │   │ |
|  │  │  };                                                                │   │ |
|  │  │                                                                    │   │ |
|  │  │  struct list_head lru;        ◄── LRU list linkage                │   │ |
|  │  │                                   For page reclaim                 │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  PAGE FLAGS (commonly used):                                             │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  PG_locked      Page is locked (I/O in progress)                  │   │ |
|  │  │  PG_dirty       Page has been modified                            │   │ |
|  │  │  PG_lru         Page is on LRU list                               │   │ |
|  │  │  PG_active      Page is on active list (recently used)            │   │ |
|  │  │  PG_slab        Page is used by slab allocator                    │   │ |
|  │  │  PG_private     Page has private data                             │   │ |
|  │  │  PG_writeback   Page is being written back                        │   │ |
|  │  │  PG_swapcache   Page is in swap cache                             │   │ |
|  │  │  PG_unevictable Page cannot be reclaimed                          │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  │  REFERENCE COUNTING:                                                     │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  get_page(page)    Increment _count (take reference)              │   │ |
|  │  │  put_page(page)    Decrement _count (release reference)           │   │ |
|  │  │                    When _count reaches 0 → page is freed          │   │ |
|  │  │                                                                    │   │ |
|  │  │  page_count(page)  Return current reference count                 │   │ |
|  │  │                                                                    │   │ |
|  │  │  Rule: Never access a page without holding a reference!           │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**struct page：页描述符**

- `flags`：页状态标志（`PG_locked`、`PG_dirty`、`PG_active` 等）
- `_count`：引用计数（0=空闲，>0=使用中）
- `_mapcount`：页表映射数量（-1=未映射）
- `mapping`：所属文件或 `anon_vma`
- `index`：文件中的偏移
- `lru`：LRU 列表链接（用于页面回收）

**引用计数**：
- `get_page()`：增加引用
- `put_page()`：减少引用，到 0 时释放
- 规则：永远不要在没有持有引用的情况下访问页面！

---

## 4. 回收和压力如何处理

```
RECLAIM AND MEMORY PRESSURE HANDLING
+=============================================================================+
|                                                                              |
|  MEMORY RECLAIM ARCHITECTURE                                                 |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  WHEN IS RECLAIM TRIGGERED?                                              │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  1. DIRECT RECLAIM (synchronous)                                 │    │ |
|  │  │     Process needs memory → no free pages → reclaim now           │    │ |
|  │  │                                                                  │    │ |
|  │  │     __alloc_pages()                                              │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │     try_to_free_pages()                                          │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │     shrink_lruvec()                                              │    │ |
|  │  │                                                                  │    │ |
|  │  │  2. KSWAPD (asynchronous background)                             │    │ |
|  │  │     Daemon wakes when free memory falls below threshold          │    │ |
|  │  │                                                                  │    │ |
|  │  │     [kswapd0] per-node kernel thread                             │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │     balance_pgdat()                                              │    │ |
|  │  │         │                                                        │    │ |
|  │  │         ▼                                                        │    │ |
|  │  │     shrink_node()                                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  LRU LISTS: IDENTIFYING RECLAIMABLE PAGES                                    |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  Two-list LRU approach:                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  ACTIVE LIST                    INACTIVE LIST                    │    │ |
|  │  │  (recently used, protected)     (candidates for reclaim)         │    │ |
|  │  │                                                                  │    │ |
|  │  │  ┌────────────────────────┐    ┌────────────────────────┐       │    │ |
|  │  │  │ page → page → page    │    │ page → page → page     │       │    │ |
|  │  │  │  ↑                    │    │  ↑                     │       │    │ |
|  │  │  │  │ recently accessed  │    │  │ not accessed recently│       │    │ |
|  │  │  └──┼─────────────────────┘    └──┼──────────────────────┘       │    │ |
|  │  │     │                             │                              │    │ |
|  │  │     │                             │                              │    │ |
|  │  │     │    ┌─────────────────┐      │                              │    │ |
|  │  │     │    │                 │      │                              │    │ |
|  │  │     └────│  Promotion      │◄─────┘ (access)                     │    │ |
|  │  │          │                 │                                     │    │ |
|  │  │          │  Demotion       │────────► (aging)                    │    │ |
|  │  │          │                 │                                     │    │ |
|  │  │          └─────────────────┘                                     │    │ |
|  │  │                                                                  │    │ |
|  │  │  Four actual lists:                                              │    │ |
|  │  │  • Active Anonymous (anon pages, recently used)                  │    │ |
|  │  │  • Inactive Anonymous (anon pages, not recently used)            │    │ |
|  │  │  • Active File (file-backed pages, recently used)                │    │ |
|  │  │  • Inactive File (file-backed pages, not recently used)          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  WHAT CAN BE RECLAIMED?                                                      |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  Page Type           Reclaim Method                              │    │ |
|  │  │  ─────────           ──────────────                              │    │ |
|  │  │  Clean file page     Drop immediately (re-read from file)        │    │ |
|  │  │  Dirty file page     Write to file, then drop                    │    │ |
|  │  │  Anonymous page      Write to swap, then drop                    │    │ |
|  │  │  Slab cache          Shrink caches (dentry, inode)               │    │ |
|  │  │                                                                  │    │ |
|  │  │  Cannot reclaim:                                                 │    │ |
|  │  │  • Kernel code and data                                          │    │ |
|  │  │  • Locked pages (mlock)                                          │    │ |
|  │  │  • Pages with active references                                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  OOM KILLER: LAST RESORT                                                     |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  When reclaim fails completely:                                          │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  out_of_memory()                                                 │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  select_bad_process()   ◄── Score each process                   │    │ |
|  │  │      │                                                           │    │ |
|  │  │      │  Badness score based on:                                  │    │ |
|  │  │      │  • RSS (resident set size)                                │    │ |
|  │  │      │  • oom_score_adj tunable                                  │    │ |
|  │  │      │  • Not killing init, kernel threads                       │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  oom_kill_process()                                              │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  Send SIGKILL to selected process                                │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**何时触发回收**：
1. **直接回收**（同步）：进程需要内存但没有空闲页时立即回收
2. **kswapd**（异步后台）：当空闲内存低于阈值时守护进程唤醒

**LRU 列表：识别可回收页面**：
- 两列表 LRU 方法：活跃列表（最近使用，受保护）和非活跃列表（回收候选）
- 四个实际列表：活跃匿名、非活跃匿名、活跃文件、非活跃文件
- 页面在活跃和非活跃列表之间晋升/降级

**可以回收什么**：
- 干净文件页：直接丢弃（从文件重新读取）
- 脏文件页：写入文件后丢弃
- 匿名页：写入交换后丢弃
- Slab 缓存：收缩缓存

**OOM Killer：最后手段**：
- 当回收完全失败时
- 根据 RSS 和 `oom_score_adj` 计算 badness 分数
- 杀死分数最高的进程

---

## 5. 硬件特定细节如何隔离

```
HARDWARE-SPECIFIC ISOLATION
+=============================================================================+
|                                                                              |
|  LAYERED ABSTRACTION FOR PORTABILITY                                         |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  ┌──────────────────────────────────────────────────────────────────┐   │ |
|  │  │                                                                    │   │ |
|  │  │  GENERIC MM LAYER (arch-independent)                               │   │ |
|  │  │  Location: mm/                                                     │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  • mm_struct, vm_area_struct management                     │   │   │ |
|  │  │  │  • Page fault handling (generic path)                       │   │   │ |
|  │  │  │  • Memory policy, reclaim, compaction                       │   │   │ |
|  │  │  │  • mmap(), brk(), munmap() implementation                   │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              │ Calls arch-specific functions       │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │  ARCH-SPECIFIC MM LAYER                                            │   │ |
|  │  │  Location: arch/*/mm/                                              │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  arch/x86/mm/        arch/arm64/mm/      arch/riscv/mm/     │   │   │ |
|  │  │  │  ─────────────       ──────────────      ──────────────     │   │   │ |
|  │  │  │  • fault.c           • fault.c           • fault.c          │   │   │ |
|  │  │  │  • pgtable.c         • mmu.c             • pgtable.c        │   │   │ |
|  │  │  │  • tlb.c             • cache.S           • tlbflush.c       │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                              │                                     │   │ |
|  │  │                              │ Manipulates hardware                │   │ |
|  │  │                              ▼                                     │   │ |
|  │  │  HARDWARE                                                          │   │ |
|  │  │  ┌────────────────────────────────────────────────────────────┐   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  │  MMU, TLB, Page Table Walker, Cache                         │   │   │ |
|  │  │  │                                                             │   │   │ |
|  │  │  └────────────────────────────────────────────────────────────┘   │   │ |
|  │  │                                                                    │   │ |
|  │  └──────────────────────────────────────────────────────────────────┘   │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
|  KEY ABSTRACTION INTERFACES                                                  |
|  ┌────────────────────────────────────────────────────────────────────────┐ |
|  │                                                                          │ |
|  │  PAGE TABLE OPERATIONS:                                                  │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Generic interfaces (asm-generic/pgtable.h) */                │    │ |
|  │  │                                                                  │    │ |
|  │  │  pte_t *pte_offset_map(pmd, addr)   Get PTE for address          │    │ |
|  │  │  void set_pte(pte_t *ptep, pte_t pte)  Set PTE                   │    │ |
|  │  │  int pte_present(pte_t pte)         Is page present?             │    │ |
|  │  │  int pte_write(pte_t pte)           Is page writable?            │    │ |
|  │  │  int pte_dirty(pte_t pte)           Is page dirty?               │    │ |
|  │  │  pte_t pte_mkwrite(pte_t pte)       Make PTE writable            │    │ |
|  │  │  pte_t pte_mkdirty(pte_t pte)       Mark PTE dirty               │    │ |
|  │  │                                                                  │    │ |
|  │  │  /* Each arch provides implementation */                         │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  TLB OPERATIONS:                                                         │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  flush_tlb_all()            Flush entire TLB (expensive!)        │    │ |
|  │  │  flush_tlb_mm(mm)           Flush TLB for address space          │    │ |
|  │  │  flush_tlb_page(vma, addr)  Flush single page (common)           │    │ |
|  │  │  flush_tlb_range(vma, start, end)  Flush range                   │    │ |
|  │  │                                                                  │    │ |
|  │  │  On SMP: IPI (inter-processor interrupt) to all CPUs            │    │ |
|  │  │         → TLB shootdown                                          │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  │  ADDRESS SPACE SWITCH:                                                   │ |
|  │  ┌─────────────────────────────────────────────────────────────────┐    │ |
|  │  │                                                                  │    │ |
|  │  │  switch_mm(prev_mm, next_mm, task)                               │    │ |
|  │  │      │                                                           │    │ |
|  │  │      │  On x86:                                                  │    │ |
|  │  │      │  • Load next_mm->pgd into CR3 register                    │    │ |
|  │  │      │  • May use ASID/PCID to avoid TLB flush                   │    │ |
|  │  │      │                                                           │    │ |
|  │  │      │  On ARM64:                                                │    │ |
|  │  │      │  • Load next_mm->pgd into TTBR0_EL1                       │    │ |
|  │  │      │  • Use ASID for TLB tagging                               │    │ |
|  │  │      │                                                           │    │ |
|  │  │      ▼                                                           │    │ |
|  │  │  Generic code doesn't need to know the details!                  │    │ |
|  │  │                                                                  │    │ |
|  │  └─────────────────────────────────────────────────────────────────┘    │ |
|  │                                                                          │ |
|  └────────────────────────────────────────────────────────────────────────┘ |
|                                                                              |
+=============================================================================+
```

**中文说明：**

**分层抽象实现可移植性**：

1. **通用 MM 层**（mm/）：架构无关
   - `mm_struct`、`vm_area_struct` 管理
   - 页错误处理（通用路径）
   - 内存策略、回收、压缩
   - `mmap()`、`brk()`、`munmap()` 实现

2. **架构特定 MM 层**（arch/*/mm/）：
   - 每个架构有自己的实现
   - `fault.c`：页错误处理
   - `pgtable.c`：页表操作
   - `tlb.c`：TLB 刷新

**关键抽象接口**：

- **页表操作**：`pte_offset_map()`、`set_pte()`、`pte_present()` 等
- **TLB 操作**：`flush_tlb_all()`、`flush_tlb_page()`、`flush_tlb_range()`
- **地址空间切换**：`switch_mm()` - x86 加载 CR3，ARM64 加载 TTBR0_EL1

通用代码不需要知道细节！
