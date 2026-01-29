# Linux Kernel v3.2 Memory Management Flashcards

## Physical Memory Fundamentals

Q: What is a page frame in Linux kernel terminology?
A: A page frame is a fixed-size block of physical memory, typically 4KB on most architectures. It's the basic unit of physical memory management. Each page frame is tracked by a `struct page` descriptor.
[Basic]

Q: What is the default page size on x86 and x86-64 architectures?
A: 4KB (4096 bytes), defined by PAGE_SIZE macro. This is determined by PAGE_SHIFT (12 on x86), where PAGE_SIZE = (1 << PAGE_SHIFT).
[Basic]

Q: What data structure represents a physical page frame in Linux kernel v3.2?
A: `struct page` defined in `include/linux/mm_types.h`. It contains flags, mapping pointer, reference counts (_count, _mapcount), and LRU list linkage.
[Basic]

Q: What is a PFN (Page Frame Number)?
A: PFN is the index of a physical page frame. It's calculated as: PFN = physical_address >> PAGE_SHIFT. The kernel uses PFN to index into the mem_map array to find the corresponding struct page.
[Basic]

Q: How do you convert between struct page and PFN?
A: Use `page_to_pfn(page)` to get PFN from struct page, and `pfn_to_page(pfn)` to get struct page from PFN. These macros are architecture-specific.
[Intermediate]

Q: What is the mem_map array?
A: mem_map is a global array of struct page descriptors, one for each physical page frame in the system. It provides O(1) access to page metadata given a PFN.
[Intermediate]

Q: In struct page, what does the `flags` field contain?
A: Atomic flags indicating page state: PG_locked (page is locked), PG_dirty (page modified), PG_lru (on LRU list), PG_active (on active list), PG_slab (used by slab allocator), PG_buddy (free in buddy system), etc.
[Intermediate]

Q: What does the `_count` field in struct page represent?
A: The reference count tracking how many users hold a reference to the page. Incremented by get_page(), decremented by put_page(). When it reaches 0, the page can be freed.
[Intermediate]

Q: What does the `_mapcount` field in struct page represent?
A: The count of page table entries (PTEs) mapping this page. Used by reverse mapping (rmap) to track how many processes have mapped a page. Value of -1 means unmapped.
[Intermediate]

Q: How does the kernel identify if a page is currently managed by the buddy allocator?
A: By checking the PG_buddy flag. When set, the page is free and part of the buddy system. The order is stored in page->private.
[Intermediate]

## Memory Zones

Q: What are memory zones in the Linux kernel?
A: Zones are logical groupings of physical memory with similar characteristics. They help the kernel allocate memory according to hardware constraints (e.g., DMA limitations).
[Basic]

Q: Name the main memory zones in Linux kernel v3.2.
A: ZONE_DMA (0-16MB on x86), ZONE_DMA32 (0-4GB on x86-64), ZONE_NORMAL (directly mapped), ZONE_HIGHMEM (not directly mapped, 32-bit only), ZONE_MOVABLE (for migration/hot-remove).
[Basic]

Q: Why does ZONE_DMA exist?
A: For legacy ISA devices that can only perform DMA to the first 16MB of physical memory. The kernel reserves this zone for such devices.
[Basic]

Q: What is ZONE_HIGHMEM?
A: On 32-bit systems, memory above approximately 896MB that cannot be permanently mapped into kernel address space. Must be temporarily mapped using kmap() before kernel access.
[Basic]

Q: Why doesn't x86-64 have ZONE_HIGHMEM?
A: 64-bit systems have enough virtual address space to directly map all physical memory. The kernel can address any physical page without temporary mappings.
[Basic]

Q: What data structure represents a memory zone?
A: `struct zone` defined in `include/linux/mmzone.h`. Contains watermarks, free_area arrays for buddy allocator, LRU lists, and statistics.
[Intermediate]

Q: What are zone watermarks?
A: WMARK_MIN, WMARK_LOW, WMARK_HIGH - thresholds that control memory reclaim behavior. When free pages fall below these levels, different actions are triggered (kswapd wakeup, direct reclaim, etc.).
[Intermediate]

Q: What is the purpose of the `lowmem_reserve` array in struct zone?
A: It reserves pages in lower zones for allocations that specifically request them. Prevents higher-zone allocations from exhausting memory needed by lower-zone-only devices.
[Advanced]

Q: What is the zone fallback order?
A: MOVABLE => HIGHMEM => NORMAL => DMA32 => DMA. When allocation from preferred zone fails, the kernel tries zones in this order.
[Intermediate]

Q: What does the `struct free_area` contain in a zone?
A: An array of free_list linked lists (one per migrate type) and nr_free count. Each index corresponds to a buddy order (0 to MAX_ORDER-1).
[Intermediate]

## NUMA Architecture

Q: What does NUMA stand for?
A: Non-Uniform Memory Access. An architecture where memory access time depends on the memory location relative to the processor.
[Basic]

Q: What is a NUMA node?
A: A group of CPUs and local memory with uniform access characteristics. Access to local memory is faster than to remote memory on other nodes.
[Basic]

Q: What data structure represents a NUMA node?
A: `pg_data_t` (typedef for `struct pglist_data`) in `include/linux/mmzone.h`. Contains node_zones array, zonelists, and node information.
[Intermediate]

Q: How do you access a node's data structure?
A: Using NODE_DATA(nid) macro, where nid is the node ID. On non-NUMA systems, this returns &contig_page_data (single node).
[Intermediate]

Q: What is a zonelist?
A: A prioritized list of zones for allocation. Each node has zonelists specifying the order to try zones when allocating memory.
[Intermediate]

Q: What are the two zonelists per node in NUMA configuration?
A: node_zonelists[0]: Zonelist with fallback to other nodes. node_zonelists[1]: GFP_THISNODE list (no fallback, allocate from this node only).
[Advanced]

Q: What is the `kswapd` field in pg_data_t?
A: A pointer to the kswapd kernel thread for this node. Each NUMA node has its own kswapd to reclaim memory locally.
[Intermediate]

## Buddy Allocator

Q: What is the buddy allocator?
A: The primary physical page allocator in Linux. It manages free pages by grouping them into power-of-2 sized blocks, enabling efficient allocation and coalescing.
[Basic]

Q: What is MAX_ORDER in the buddy allocator?
A: The maximum order of allocation (default 11). The largest single allocation is 2^(MAX_ORDER-1) pages = 2^10 * 4KB = 4MB.
[Basic]

Q: How does buddy allocation work?
A: 1) Find smallest block >= requested size. 2) If exact match, allocate. 3) If larger, split recursively into buddies until correct size. 4) Return one block, keep other as free buddy.
[Intermediate]

Q: How does buddy freeing work?
A: 1) Check if buddy block is free. 2) If free and same order, coalesce into larger block. 3) Repeat until no more coalescing possible or MAX_ORDER reached.
[Intermediate]

Q: How do you find a page's buddy given its PFN and order?
A: buddy_pfn = page_pfn ^ (1 << order). XOR operation toggles the appropriate bit to find the buddy.
[Intermediate]

Q: What function allocates pages from the buddy allocator?
A: `alloc_pages(gfp_mask, order)` - returns struct page pointer. `__get_free_pages(gfp_mask, order)` - returns kernel virtual address.
[Basic]

Q: What function frees pages to the buddy allocator?
A: `__free_pages(page, order)` - takes struct page pointer. `free_pages(addr, order)` - takes kernel virtual address.
[Basic]

Q: What are the migrate types in the buddy allocator?
A: MIGRATE_UNMOVABLE (kernel allocations), MIGRATE_RECLAIMABLE (can be reclaimed), MIGRATE_MOVABLE (can be moved/compacted), MIGRATE_RESERVE, MIGRATE_ISOLATE.
[Intermediate]

Q: Why are migrate types important?
A: They reduce memory fragmentation by grouping pages with similar mobility. Movable pages grouped together enable compaction and memory hot-remove.
[Intermediate]

Q: What is the per-cpu page cache in the buddy allocator?
A: Each CPU has per_cpu_pages (pcp) lists for quick allocation of single pages without zone lock contention. Hot pages (recently used) and cold pages are maintained.
[Advanced]

Q: What is the difference between hot and cold pages?
A: Hot pages are likely in CPU cache (recently freed), preferred for allocations. Cold pages are unlikely cached, used when cache pollution doesn't matter.
[Intermediate]

## GFP Flags

Q: What does GFP stand for?
A: Get Free Pages - flags that specify allocation constraints and behavior.
[Basic]

Q: What is GFP_KERNEL?
A: The most common allocation flag: (__GFP_WAIT | __GFP_IO | __GFP_FS). Can sleep, perform I/O, and call into filesystem. Used for normal kernel allocations.
[Basic]

Q: What is GFP_ATOMIC?
A: (__GFP_HIGH) - Non-sleeping allocation for interrupt context. Can use emergency reserves but may fail. Cannot block.
[Basic]

Q: What is __GFP_WAIT?
A: Flag indicating the allocator may sleep (block) waiting for memory. Not set for atomic contexts.
[Basic]

Q: What is __GFP_HIGH?
A: Flag to access emergency memory reserves. Used for high-priority allocations that must not fail if possible.
[Intermediate]

Q: What is __GFP_IO?
A: Flag allowing the allocator to start physical I/O (e.g., swapping) to free memory.
[Intermediate]

Q: What is __GFP_FS?
A: Flag allowing the allocator to call into filesystem code. Used with __GFP_IO for full reclaim capability.
[Intermediate]

Q: What is GFP_NOIO?
A: (__GFP_WAIT) - Can sleep but cannot start I/O. Used in block I/O paths to avoid recursion.
[Intermediate]

Q: What is GFP_NOFS?
A: (__GFP_WAIT | __GFP_IO) - Can sleep and do I/O but cannot call filesystem. Used in filesystem code.
[Intermediate]

Q: What is GFP_USER?
A: (__GFP_WAIT | __GFP_IO | __GFP_FS | __GFP_HARDWALL) - For user-space allocations, respects cpuset memory constraints.
[Intermediate]

Q: What is GFP_HIGHUSER?
A: GFP_USER | __GFP_HIGHMEM - User-space allocation that can use high memory. Common for user page allocations.
[Intermediate]

Q: What is __GFP_ZERO?
A: Flag to zero the allocated memory before returning. Equivalent to using get_zeroed_page() or kzalloc().
[Basic]

Q: What is __GFP_DMA?
A: Zone modifier to allocate from ZONE_DMA. For devices with 16MB DMA limitation.
[Basic]

Q: What is __GFP_HIGHMEM?
A: Zone modifier to allow allocation from ZONE_HIGHMEM. Memory may need kmap() for kernel access.
[Basic]

Q: What is __GFP_MOVABLE?
A: Flag indicating pages can be moved by compaction or memory hot-remove. Part of anti-fragmentation mechanism.
[Intermediate]

Q: What is __GFP_NOWARN?
A: Suppresses allocation failure warnings. Used when failure is acceptable and expected in some cases.
[Intermediate]

Q: What is __GFP_REPEAT?
A: Try hard to allocate by retrying after reclaim, but may still fail. Less aggressive than __GFP_NOFAIL.
[Intermediate]

Q: What is __GFP_NOFAIL?
A: Never-fail flag (deprecated). Allocator must retry indefinitely. Dangerous - can cause system hangs.
[Intermediate]

## Slab Allocator

Q: What problem does the slab allocator solve?
A: Efficient allocation of small objects (smaller than a page). Reduces fragmentation and overhead compared to allocating full pages for small objects.
[Basic]

Q: What are the three slab allocator implementations in Linux v3.2?
A: SLAB (original, complex, many queues), SLUB (simplified, default since 2.6.23), SLOB (minimal for embedded systems).
[Basic]

Q: What is a kmem_cache?
A: A cache for allocating objects of a specific size. Created with kmem_cache_create(), allocations via kmem_cache_alloc().
[Basic]

Q: What function allocates memory from the slab allocator?
A: `kmalloc(size, gfp_flags)` for general allocations. `kmem_cache_alloc(cache, gfp_flags)` for specific cache.
[Basic]

Q: What function frees slab-allocated memory?
A: `kfree(ptr)` for kmalloc'd memory. `kmem_cache_free(cache, ptr)` for cache-specific allocations.
[Basic]

Q: What is the maximum kmalloc size?
A: KMALLOC_MAX_SIZE = min(2^25 bytes (32MB), 2^(MAX_ORDER+PAGE_SHIFT-1)). Typically 4MB on most systems.
[Intermediate]

Q: How does SLUB differ from SLAB?
A: SLUB removes per-cpu queues and per-node partial lists of SLAB. Uses page struct directly for metadata. Simpler, lower memory overhead, better debugging.
[Intermediate]

Q: What is a slab page?
A: A page (or compound page) managed by the slab allocator, divided into equal-sized objects. Tracks free objects via freelist.
[Intermediate]

Q: What is the freelist in SLUB?
A: A linked list of free objects within a slab. Each free object contains a pointer to the next free object at a fixed offset.
[Intermediate]

Q: What is SLAB_HWCACHE_ALIGN?
A: Flag to align objects to CPU cache line boundaries. Improves performance by preventing false sharing.
[Intermediate]

Q: What is SLAB_DESTROY_BY_RCU?
A: Flag to delay freeing slab pages by an RCU grace period. Memory location remains valid for RCU readers even after kmem_cache_free().
[Advanced]

Q: What does kmem_cache_create() do?
A: Creates a new slab cache: `kmem_cache_create(name, size, align, flags, ctor)`. Returns pointer to kmem_cache structure.
[Intermediate]

Q: What is a constructor function in slab allocator?
A: Optional function (ctor parameter in kmem_cache_create) called to initialize each object when first allocated to the cache.
[Intermediate]

Q: What is ZERO_SIZE_PTR?
A: Special pointer value (16) returned for zero-size kmalloc requests. Dereferencing causes a fault, but kfree(ZERO_SIZE_PTR) is safe.
[Intermediate]

Q: What are the general-purpose kmalloc caches?
A: Pre-created caches for power-of-2 sizes: kmalloc-8, kmalloc-16, ..., kmalloc-8192, etc. kmalloc() selects appropriate cache.
[Intermediate]

Q: What is kzalloc()?
A: Allocates zeroed memory: `kzalloc(size, flags)` is equivalent to `kmalloc(size, flags | __GFP_ZERO)`.
[Basic]

Q: What is kcalloc()?
A: Array allocation: `kcalloc(n, size, flags)` allocates n*size bytes, zeroed. Checks for multiplication overflow.
[Basic]

Q: What is krealloc()?
A: Reallocates memory: `krealloc(ptr, new_size, flags)`. May move data to new location if size increased.
[Basic]

## Virtual Memory - mm_struct

Q: What data structure represents a process's virtual address space?
A: `struct mm_struct` defined in `include/linux/mm_types.h`. Contains VMAs, page tables, memory statistics.
[Basic]

Q: What field in task_struct points to the memory descriptor?
A: `task->mm` for user-space processes. Kernel threads have mm = NULL (they use active_mm for page tables).
[Basic]

Q: What is the `mmap` field in mm_struct?
A: Head of the linked list of vm_area_struct (VMA) structures, sorted by virtual address.
[Intermediate]

Q: What is the `mm_rb` field in mm_struct?
A: Red-black tree root for VMAs. Enables O(log n) lookup of VMA by address, faster than linear list search.
[Intermediate]

Q: What is the `pgd` field in mm_struct?
A: Pointer to the page global directory - the top level of the process's page tables. Loaded into CR3 on x86.
[Intermediate]

Q: What do `mm_users` and `mm_count` track?
A: mm_users: number of threads sharing this mm_struct. mm_count: references to mm_struct (mm_users counts as 1 reference).
[Intermediate]

Q: What is mmap_sem in mm_struct?
A: Read-write semaphore protecting the VMA list and page tables. Held for read during page faults, write during mmap/munmap.
[Intermediate]

Q: What do `start_code`, `end_code`, `start_data`, `end_data` represent?
A: Boundaries of the process's code and data segments in virtual address space.
[Basic]

Q: What are `start_brk` and `brk` in mm_struct?
A: start_brk: beginning of the heap. brk: current end of heap (program break). Region grows via brk() syscall.
[Basic]

Q: What is `start_stack` in mm_struct?
A: Starting address of the user stack (grows downward on most architectures).
[Basic]

Q: What is `total_vm` in mm_struct?
A: Total number of pages mapped in the process's address space (including both resident and swapped).
[Basic]

Q: What is `locked_vm` in mm_struct?
A: Pages locked in memory via mlock() - cannot be swapped out.
[Intermediate]

## Virtual Memory Areas (VMA)

Q: What is a VMA (Virtual Memory Area)?
A: A contiguous region of virtual memory with uniform properties. Represented by `struct vm_area_struct`.
[Basic]

Q: What fields define a VMA's address range?
A: `vm_start`: first virtual address in VMA. `vm_end`: first address after VMA. The range is [vm_start, vm_end).
[Basic]

Q: What does `vm_mm` point to?
A: The mm_struct that this VMA belongs to (the owning address space).
[Basic]

Q: What do `vm_next` and `vm_prev` represent?
A: Linked list pointers connecting VMAs in address order within an mm_struct.
[Basic]

Q: What does `vm_flags` contain?
A: Permission and behavior flags: VM_READ, VM_WRITE, VM_EXEC, VM_SHARED, VM_GROWSDOWN (stack), VM_LOCKED, etc.
[Intermediate]

Q: What is `vm_page_prot`?
A: Page protection bits (pgprot_t) for hardware page tables. Derived from vm_flags but in architecture-specific format.
[Intermediate]

Q: What does `vm_file` point to?
A: For file-backed mappings, the struct file being mapped. NULL for anonymous mappings.
[Basic]

Q: What is `vm_pgoff`?
A: Offset (in PAGE_SIZE units) into the file for file-backed mappings. First page of VMA maps to file offset vm_pgoff * PAGE_SIZE.
[Intermediate]

Q: What is `vm_ops`?
A: Pointer to vm_operations_struct containing VMA-specific callback functions (fault handler, open, close, etc.).
[Intermediate]

Q: What is the difference between file-backed and anonymous VMAs?
A: File-backed: mapped from a file (vm_file != NULL). Anonymous: not backed by file, backed by swap (e.g., heap, stack).
[Basic]

Q: What is `anon_vma`?
A: Pointer to anon_vma structure for reverse mapping of anonymous pages. Links all VMAs that might share anonymous pages.
[Advanced]

Q: What is the VM_SHARED flag?
A: Indicates a shared mapping where modifications are visible to other processes mapping the same file.
[Basic]

Q: What is the VM_PRIVATE flag (absence of VM_SHARED)?
A: Private mapping with copy-on-write semantics. Modifications create private copies of pages.
[Basic]

Q: What is VM_GROWSDOWN?
A: Flag for stack VMAs indicating the region can grow downward (toward lower addresses) on demand.
[Intermediate]

Q: How does find_vma() work?
A: Returns the first VMA with vm_end > addr. Uses mmap_cache (last lookup result) for optimization, then rb-tree search.
[Intermediate]

## Page Tables

Q: How many levels of page tables does x86-64 use?
A: Four levels: PGD (Page Global Directory), PUD (Page Upper Directory), PMD (Page Middle Directory), PTE (Page Table Entry).
[Basic]

Q: How many levels of page tables does 32-bit x86 use (without PAE)?
A: Two levels: PGD and PTE. PMD and PUD are folded (single-entry).
[Basic]

Q: What is stored in a Page Table Entry (PTE)?
A: Physical page frame number, present bit, read/write permission, user/supervisor, accessed/dirty bits, cache control bits.
[Basic]

Q: What is the PRESENT bit in a PTE?
A: Indicates whether the page is in physical memory. If clear, accessing causes a page fault.
[Basic]

Q: What is the DIRTY bit in a PTE?
A: Set by hardware when the page is written. Tells kernel the page needs to be written back before freeing.
[Basic]

Q: What is the ACCESSED bit in a PTE?
A: Set by hardware when the page is read or written. Used by page reclaim to determine page activity.
[Basic]

Q: What is the USER/SUPERVISOR bit?
A: Controls whether user-space can access the page. Clear = kernel only, Set = user accessible.
[Basic]

Q: What is pte_offset_kernel()?
A: Function to get PTE pointer from PMD and virtual address. Used for kernel page table manipulation.
[Intermediate]

Q: What is the page table walk sequence?
A: pgd_offset(mm, addr) → pud_offset(pgd, addr) → pmd_offset(pud, addr) → pte_offset(pmd, addr)
[Intermediate]

Q: What is pgd_offset_k()?
A: Returns PGD entry for kernel address. Uses init_mm.pgd (kernel master page table).
[Intermediate]

Q: What is the TLB?
A: Translation Lookaside Buffer - CPU cache for page table entries. Must be flushed when page tables change.
[Basic]

Q: What is flush_tlb_range()?
A: Flushes TLB entries for a range of virtual addresses in a specific address space.
[Intermediate]

Q: What is a compound page?
A: Multiple contiguous pages treated as a single unit. The first page is "head", others are "tail". Used for huge pages and high-order slab allocations.
[Intermediate]

## Page Fault Handling

Q: What is a page fault?
A: Exception raised when accessing a virtual address with no valid page table mapping or with violated permissions.
[Basic]

Q: What are the types of page faults?
A: Minor (page in memory, just needs PTE update), Major (page not in memory, needs I/O), Invalid (segfault).
[Basic]

Q: What function handles page faults in Linux?
A: `handle_mm_fault()` in mm/memory.c. Called from architecture-specific fault handler.
[Intermediate]

Q: What is a minor page fault?
A: Page is in page cache or can be allocated without I/O. Only page table update needed. Fast.
[Basic]

Q: What is a major page fault?
A: Page must be read from disk (swap or file). Requires blocking I/O. Slow.
[Basic]

Q: What is demand paging?
A: Delaying physical page allocation until first access. Virtual pages start with no physical backing, allocated on fault.
[Basic]

Q: What is copy-on-write (COW)?
A: Optimization where forked processes share read-only pages. On write attempt, fault handler copies the page to make it private.
[Basic]

Q: What is the fault handler function pointer in vm_operations_struct?
A: `fault` - called to handle page faults in a VMA. Responsible for allocating/locating the page.
[Intermediate]

Q: What is a fault info structure (struct vm_fault)?
A: Contains fault information passed to fault handlers: virtual_address, flags, page (output), pgoff, etc.
[Intermediate]

Q: What does FAULT_FLAG_WRITE indicate?
A: The fault was caused by a write operation. Handler may need to break COW sharing.
[Intermediate]

Q: What is do_anonymous_page()?
A: Handles faults on anonymous memory (heap, stack). Allocates a new zeroed page.
[Intermediate]

Q: What is do_swap_page()?
A: Handles faults on swapped-out pages. Reads page from swap and updates page tables.
[Intermediate]

## Memory Reclaim

Q: What is memory reclaim?
A: The process of freeing used pages when memory is low. Includes page cache shrinking, swapping, and slab shrinking.
[Basic]

Q: What is kswapd?
A: Per-node kernel thread that performs background memory reclaim when free pages fall below watermarks.
[Basic]

Q: What are the LRU lists?
A: Lists tracking page activity for reclaim: LRU_INACTIVE_ANON, LRU_ACTIVE_ANON, LRU_INACTIVE_FILE, LRU_ACTIVE_FILE, LRU_UNEVICTABLE.
[Intermediate]

Q: What is the difference between active and inactive LRU lists?
A: Active: recently accessed pages, protected from reclaim. Inactive: candidates for reclaim. Pages move between lists based on access patterns.
[Intermediate]

Q: What is the difference between anon and file LRU lists?
A: Anon lists: anonymous pages (heap, stack) backed by swap. File lists: file-backed pages (page cache).
[Intermediate]

Q: What is direct reclaim?
A: Synchronous reclaim by the allocating process when background reclaim can't keep up. Process blocks while reclaiming.
[Intermediate]

Q: What triggers kswapd wakeup?
A: When zone free pages fall below low_wmark_pages(zone). kswapd tries to restore free pages to high_wmark_pages.
[Intermediate]

Q: What happens when free pages fall below min watermark?
A: Direct reclaim is triggered. Allocator tries to free pages synchronously before allocation can proceed.
[Intermediate]

Q: What is the vm_swappiness parameter?
A: Controls preference between reclaiming file pages vs. anonymous pages (swapping). Default 60. Higher = more swapping.
[Intermediate]

Q: What is shrink_zone()?
A: Core reclaim function that scans LRU lists in a zone and tries to free pages.
[Intermediate]

Q: What is the page reclaim scanning algorithm?
A: Two-handed clock algorithm: pages move inactive→active on access, active→inactive when scanned inactive, freed from inactive tail.
[Advanced]

Q: What is page writeback?
A: Writing dirty pages to backing store (file or swap) before they can be freed.
[Basic]

Q: What is the difference between sync and async reclaim?
A: Async: doesn't wait for I/O, skips dirty pages. Sync: can wait for writeback, used under memory pressure.
[Intermediate]

Q: What is lumpy reclaim?
A: Reclaim strategy for high-order allocations that frees contiguous pages together to create free blocks.
[Advanced]

Q: What is compaction?
A: Moving movable pages to create contiguous free regions. Alternative to lumpy reclaim for high-order allocations.
[Advanced]

## OOM Killer

Q: What is the OOM killer?
A: Out-Of-Memory killer - last resort when system cannot reclaim enough memory. Selects and kills a process to free memory.
[Basic]

Q: When is the OOM killer invoked?
A: When allocation fails after all reclaim attempts, all zones exhausted, and no other option exists.
[Basic]

Q: How does OOM killer select a victim?
A: Scores processes based on memory usage, oom_score_adj setting, and other factors. Highest score is killed.
[Intermediate]

Q: What is oom_score_adj?
A: Per-process tunable (-1000 to 1000) affecting OOM kill probability. -1000 = never kill, positive = more likely to kill.
[Intermediate]

Q: What is the OOM reaper?
A: Kernel thread that asynchronously frees memory from OOM-killed process if it's stuck in D state.
[Advanced]

Q: What signal does OOM killer send?
A: SIGKILL to the selected victim process. Cannot be caught or ignored.
[Basic]

## vmalloc

Q: What is vmalloc()?
A: Allocates virtually contiguous memory that may be physically non-contiguous. Uses separate address range from direct mapping.
[Basic]

Q: When should you use vmalloc() vs kmalloc()?
A: vmalloc for large allocations where physical contiguity isn't needed. kmalloc for small allocations or when physical contiguity required (DMA).
[Intermediate]

Q: Where is vmalloc address space located?
A: In the vmalloc area, typically starting at VMALLOC_START. Separate from linear mapping of physical memory.
[Intermediate]

Q: What are disadvantages of vmalloc()?
A: Slower due to page table setup. Not physically contiguous (can't use for DMA). TLB pressure. Requires page table walk for each page.
[Intermediate]

Q: What is vfree()?
A: Frees memory allocated by vmalloc(). Must use vfree(), not kfree(), for vmalloc'd memory.
[Basic]

Q: What is vmap()?
A: Maps an array of pages into contiguous virtual address space. Lower-level function used by vmalloc().
[Intermediate]

Q: What is the vm_struct?
A: Describes a vmalloc'd region: address, size, flags, pages array, etc. Different from vm_area_struct.
[Intermediate]

Q: What is VMALLOC_START and VMALLOC_END?
A: Architecture-specific bounds of the vmalloc address range. vmalloc allocations fall within this range.
[Intermediate]

Q: How does vmalloc handle page allocation?
A: Allocates individual pages (order-0) from buddy allocator, then maps them into contiguous virtual addresses.
[Intermediate]

## High Memory (32-bit specific)

Q: What is high memory?
A: Physical memory that cannot be permanently mapped into kernel address space (32-bit systems with >~896MB RAM).
[Basic]

Q: Why is high memory needed on 32-bit systems?
A: Only ~896MB of the 4GB virtual space is reserved for kernel. Physical memory above this can't be directly mapped.
[Basic]

Q: What is kmap()?
A: Maps a high memory page into kernel address space temporarily. Returns kernel virtual address. May sleep.
[Intermediate]

Q: What is kunmap()?
A: Unmaps a page previously mapped with kmap(). Must be called to free the mapping slot.
[Intermediate]

Q: What is kmap_atomic()?
A: Atomic (non-sleeping) variant of kmap. Uses per-CPU slots. Must be unmapped with kunmap_atomic().
[Intermediate]

Q: Why must kmap_atomic() mappings be short-lived?
A: Uses limited per-CPU slots. Nesting requires careful management. Preemption is disabled while mapped.
[Intermediate]

Q: What is the HIGHMEM allocation flag?
A: __GFP_HIGHMEM allows allocation from ZONE_HIGHMEM. Pages may need kmap() for kernel access.
[Basic]

## Process Address Space Layout

Q: What is the typical address space layout of a Linux process?
A: (low to high): text, data, BSS, heap (grows up), ..., memory mappings, ..., stack (grows down).
[Basic]

Q: What system call creates memory mappings?
A: mmap() - maps files or anonymous memory into address space. munmap() removes mappings.
[Basic]

Q: What is the brk() system call?
A: Changes the program break (end of heap). Used by malloc() implementations to grow heap.
[Basic]

Q: What is ASLR?
A: Address Space Layout Randomization - randomizes locations of stack, heap, mmap area for security.
[Basic]

Q: What is the mmap_base field in mm_struct?
A: Base address of mmap region. On traditional layout, mmap grows down from near stack.
[Intermediate]

Q: What is the difference between legacy and modern mmap layout?
A: Legacy: mmap grows up from fixed base. Modern: mmap grows down from below stack, allowing larger heap.
[Advanced]

Q: What is vDSO?
A: Virtual Dynamic Shared Object - kernel code mapped into user space for fast system calls (e.g., gettimeofday).
[Intermediate]

## Memory Mapping

Q: What is MAP_PRIVATE?
A: mmap flag for private mapping with COW. Modifications don't affect underlying file or other mappings.
[Basic]

Q: What is MAP_SHARED?
A: mmap flag for shared mapping. Modifications are visible to other processes and written to file.
[Basic]

Q: What is MAP_ANONYMOUS?
A: mmap flag for mapping without file backing. Pages are zero-filled on first access.
[Basic]

Q: What is MAP_FIXED?
A: mmap flag to force mapping at exact specified address. Dangerous - can overwrite existing mappings.
[Intermediate]

Q: What is MAP_POPULATE?
A: mmap flag to pre-fault pages. Avoids later page faults but increases mmap() time.
[Intermediate]

Q: What is MREMAP_MAYMOVE?
A: mremap() flag allowing the mapping to move to a new address if expansion in place isn't possible.
[Intermediate]

Q: What is mprotect()?
A: System call to change protection flags (read/write/exec) on existing memory regions.
[Basic]

Q: What is madvise()?
A: System call to advise kernel about expected memory access patterns (sequential, random, willneed, dontneed).
[Intermediate]

Q: What is MADV_DONTNEED?
A: madvise hint that pages aren't needed. For anonymous mappings, pages may be freed immediately.
[Intermediate]

Q: What is MADV_WILLNEED?
A: madvise hint that pages will be needed soon. Triggers readahead for file-backed pages.
[Intermediate]

## Copy-on-Write

Q: How is COW implemented in fork()?
A: Parent and child share same physical pages, marked read-only in both. Write triggers fault, page is copied.
[Basic]

Q: What happens to page reference count in COW?
A: Shared page has _count > 1. On COW fault, new page allocated, _count of both adjusted.
[Intermediate]

Q: How does kernel detect COW situation?
A: Page mapped read-only in VMA with VM_WRITE permission. Write fault to writable region = COW.
[Intermediate]

Q: What is the advantage of COW?
A: Fork is fast (no page copying). Memory saved when child execs quickly. Only modified pages copied.
[Basic]

Q: What is the disadvantage of COW?
A: Write faults add latency. Memory usage hard to predict. Parent modification affects child timing.
[Intermediate]

## Swap

Q: What is swap space?
A: Disk space used to store anonymous pages evicted from RAM. Extends effective memory size.
[Basic]

Q: What are swap partitions vs swap files?
A: Partition: dedicated disk partition for swap. File: regular file on filesystem used for swap.
[Basic]

Q: What is swp_entry_t?
A: Type representing a swap entry: swap area identifier + offset within that area. Stored in PTE when page swapped.
[Intermediate]

Q: What is the swap cache?
A: Cache of swap pages also present in memory. Allows multiple PTEs to reference same swap slot.
[Intermediate]

Q: How does the kernel decide what to swap?
A: Based on LRU age and vm_swappiness parameter. Anonymous inactive pages are swap candidates.
[Intermediate]

Q: What is swap readahead?
A: Reading multiple swap pages when one is needed, anticipating sequential access to swapped data.
[Intermediate]

Q: What is swappiness=0?
A: Setting vm_swappiness=0 avoids swapping almost entirely, preferring to reclaim file cache.
[Intermediate]

## Memory Debugging

Q: What is kmemcheck?
A: Kernel memory checker that detects use of uninitialized memory. Uses page permissions to trap accesses.
[Intermediate]

Q: What is kmemleak?
A: Kernel memory leak detector. Scans memory for unreferenced allocations (potential leaks).
[Intermediate]

Q: What is SLAB_POISON?
A: Debug flag filling freed objects with pattern (0x6b). Helps detect use-after-free bugs.
[Intermediate]

Q: What is SLAB_RED_ZONE?
A: Debug flag adding guard zones around slab objects. Detects buffer overflows/underflows.
[Intermediate]

Q: What is PagePoisoned?
A: Page flag set on freed pages. Accessing poisoned page triggers fault, detecting use-after-free.
[Intermediate]

Q: What does slabinfo show?
A: /proc/slabinfo shows slab cache statistics: name, active/total objects, object size, etc.
[Basic]

## Kernel Memory Layout

Q: What is the kernel's virtual address space layout?
A: (x86-64): direct mapping at PAGE_OFFSET, vmalloc area, vmemmap, modules area, fixmap.
[Intermediate]

Q: What is the direct mapping?
A: Linear mapping of all physical memory into kernel virtual address space. Access via __va/__pa macros.
[Basic]

Q: What is PAGE_OFFSET?
A: Virtual address where physical memory direct mapping begins. 0xC0000000 on 32-bit, varies on 64-bit.
[Intermediate]

Q: What is __pa() macro?
A: Physical address from kernel virtual: __pa(vaddr) = vaddr - PAGE_OFFSET (for direct-mapped memory).
[Intermediate]

Q: What is __va() macro?
A: Virtual address from physical: __va(paddr) = paddr + PAGE_OFFSET. Only valid for lowmem.
[Intermediate]

Q: What is page_to_virt()?
A: Returns kernel virtual address for a struct page in the direct mapping. Not valid for highmem pages.
[Intermediate]

Q: What is the fixmap area?
A: Fixed virtual addresses for compile-time constant mappings. Used for early boot and special mappings.
[Advanced]

Q: What is the vmemmap area?
A: Virtual memmap - sparse memory's array of struct page, mapped on demand rather than requiring contiguous physical memory.
[Advanced]

## Per-CPU Allocations

Q: What are per-CPU variables?
A: Variables with separate copy for each CPU. Avoids cache bouncing and locking. Declared with DEFINE_PER_CPU.
[Intermediate]

Q: How do you access per-CPU variables?
A: get_cpu_var() with put_cpu_var(), or this_cpu_ptr() / __this_cpu_ptr(). Former disables preemption.
[Intermediate]

Q: What is alloc_percpu()?
A: Dynamically allocates per-CPU memory. Returns pointer that must be dereferenced with per_cpu_ptr().
[Intermediate]

Q: What is free_percpu()?
A: Frees memory allocated by alloc_percpu().
[Intermediate]

## Memory Barriers

Q: What is a memory barrier?
A: Instruction preventing CPU/compiler from reordering memory operations across the barrier.
[Basic]

Q: What is mb()?
A: Full memory barrier - orders both reads and writes before and after.
[Intermediate]

Q: What is rmb()?
A: Read memory barrier - ensures reads before the barrier complete before reads after.
[Intermediate]

Q: What is wmb()?
A: Write memory barrier - ensures writes before the barrier complete before writes after.
[Intermediate]

Q: What is smp_mb()?
A: SMP memory barrier - full barrier on SMP, may be no-op on UP.
[Intermediate]

## Huge Pages

Q: What are huge pages?
A: Large page sizes (2MB or 1GB on x86-64) reducing TLB misses and page table overhead.
[Basic]

Q: What is hugetlbfs?
A: Pseudo-filesystem for allocating huge pages. Applications mmap from hugetlbfs to get huge pages.
[Intermediate]

Q: What are transparent huge pages (THP)?
A: Automatic use of huge pages without application changes. Kernel promotes/demotes pages as needed.
[Intermediate]

Q: What is khugepaged?
A: Kernel thread that scans for opportunities to collapse regular pages into transparent huge pages.
[Intermediate]

Q: What is the disadvantage of huge pages?
A: More internal fragmentation. Harder to allocate (need contiguous physical memory). Can waste memory.
[Intermediate]

## Common Misconceptions

Q: Can kmalloc() return physically contiguous memory?
A: Yes, kmalloc() returns physically contiguous memory from the slab allocator (up to KMALLOC_MAX_SIZE).
[Basic]

Q: Does vmalloc() return physically contiguous memory?
A: No, vmalloc() returns virtually contiguous but typically physically non-contiguous memory.
[Basic]

Q: Can GFP_ATOMIC allocations sleep?
A: No, GFP_ATOMIC never sleeps. It may fail if memory cannot be allocated immediately.
[Basic]

Q: Are all kernel memory allocations pageable (swappable)?
A: No, kernel memory is not swappable. Only user-space anonymous memory can be swapped.
[Basic]

Q: Does kfree(NULL) crash?
A: No, kfree(NULL) is a safe no-op, like free(NULL) in userspace.
[Basic]

Q: Can you use kmalloc'd memory for DMA to any device?
A: Not necessarily. For devices with address limitations, use GFP_DMA/GFP_DMA32 or dma_alloc_coherent().
[Intermediate]

Q: Is ZONE_HIGHMEM used on 64-bit systems?
A: No, 64-bit systems have enough virtual address space to directly map all physical memory.
[Basic]

Q: Does fork() immediately double memory usage?
A: No, fork() uses COW. Pages are shared until written, so minimal additional memory until modifications.
[Basic]

## Code Patterns

Q: What is the correct pattern for kmalloc with error checking?
A: 
```c
ptr = kmalloc(size, GFP_KERNEL);
if (!ptr)
    return -ENOMEM;
```
[Basic]

Q: How do you allocate zeroed memory?
A: Use kzalloc(size, flags) or kmalloc(size, flags | __GFP_ZERO).
[Basic]

Q: How do you allocate a struct with slab cache?
A: 
```c
cache = kmem_cache_create("name", sizeof(struct), 0, 0, NULL);
obj = kmem_cache_alloc(cache, GFP_KERNEL);
```
[Intermediate]

Q: How do you properly free slab cache objects?
A:
```c
kmem_cache_free(cache, obj);
/* When cache no longer needed: */
kmem_cache_destroy(cache);
```
[Intermediate]

Q: What is the pattern for handling highmem pages?
A:
```c
page = alloc_page(GFP_HIGHUSER);
addr = kmap(page);
/* use addr */
kunmap(page);
```
[Intermediate]

Q: How do you convert between page and virtual address?
A: page_address(page) for lowmem pages. For highmem, must use kmap()/kunmap().
[Intermediate]

## ASCII Diagrams

Q: Draw the basic memory zone organization.
A:
```
Physical Memory Layout (x86):
+------------------+ 0
|    ZONE_DMA      | 0-16MB
+------------------+ 16MB
|   ZONE_NORMAL    | 16MB-896MB
+------------------+ ~896MB
|   ZONE_HIGHMEM   | >896MB (32-bit only)
+------------------+
```
[Basic]

Q: Draw the buddy allocator free_area structure.
A:
```
zone->free_area[order]:
order 0: [list] -> page -> page -> page  (1 page each)
order 1: [list] -> page -> page          (2 pages each)
order 2: [list] -> page                  (4 pages each)
...
order 10:[list] -> page                  (1024 pages each)
```
[Intermediate]

Q: Draw the page table hierarchy (x86-64).
A:
```
Virtual Address (48 bits used):
[PGD idx|PUD idx|PMD idx|PTE idx|Offset]
   9b      9b      9b      9b     12b

CR3 -> PGD[512] -> PUD[512] -> PMD[512] -> PTE[512] -> Page
```
[Intermediate]

Q: Draw the LRU list organization.
A:
```
LRU Lists per zone:
active_anon   <-> page <-> page <-> page
inactive_anon <-> page <-> page <-> page
active_file   <-> page <-> page <-> page
inactive_file <-> page <-> page <-> page
unevictable   <-> page <-> page
```
[Intermediate]

Q: Draw the vm_area_struct list in mm_struct.
A:
```
mm_struct:
+--------+
|  mmap -+---> vma1 <-> vma2 <-> vma3 -> NULL
| mm_rb  |    [0x400000-0x401000] [0x7ff...]
+--------+     code segment       stack
```
[Intermediate]

Q: Draw the slab/page relationship.
A:
```
kmem_cache:
+-----------+
| object_sz |      Slab (page):
| slabs ----|---> +---+---+---+---+
+-----------+     |obj|obj|obj|obj| <- fixed-size objects
                  +---+---+---+---+
                  freelist chains free objects
```
[Intermediate]

Q: Draw the struct page memory layout concept.
A:
```
struct page (simplified):
+------------------+
| flags            | Page state bits
| mapping          | Address space or anon_vma
| index/freelist   | File offset or slab free
| _mapcount/_count | Reference counts
| lru              | LRU list linkage
| private          | Context-dependent
+------------------+
```
[Intermediate]

## Key APIs Summary

Q: What is get_zeroed_page()?
A: `get_zeroed_page(gfp_mask)` - allocates single zeroed page, returns kernel virtual address.
[Basic]

Q: What is alloc_page()?
A: `alloc_page(gfp_mask)` - macro expanding to alloc_pages(gfp_mask, 0). Returns single struct page.
[Basic]

Q: What is __get_free_pages()?
A: `__get_free_pages(gfp_mask, order)` - allocates 2^order pages, returns kernel virtual address.
[Basic]

Q: What is copy_to_user()?
A: `copy_to_user(to, from, n)` - safely copies n bytes from kernel to user space. Returns bytes NOT copied.
[Basic]

Q: What is copy_from_user()?
A: `copy_from_user(to, from, n)` - safely copies n bytes from user to kernel space. Returns bytes NOT copied.
[Basic]

Q: What is get_user_pages()?
A: Pins user pages in memory and returns array of struct page. Used for direct I/O.
[Advanced]

Q: What is remap_pfn_range()?
A: Maps physical pages into user space VMA. Used by drivers for MMIO or sharing kernel buffers.
[Advanced]

Q: What is vm_insert_page()?
A: Inserts a single kernel page into a user VMA. Alternative to remap_pfn_range for single pages.
[Advanced]

## Reclaim Internals

Q: What is the scan_control structure?
A: Contains parameters for a reclaim scan: nr_scanned, nr_reclaimed, gfp_mask, order, may_unmap, may_swap.
[Advanced]

Q: What is the shrinker interface?
A: Callback mechanism for reclaiming non-page-cache memory (dcache, icache, etc). register_shrinker() adds callback.
[Advanced]

Q: What is shrink_slab()?
A: Calls registered shrinkers to reclaim slab objects (dentries, inodes, etc.) proportionally to page reclaim.
[Advanced]

Q: What is the zone_reclaim_stat?
A: Statistics tracking recent_rotated (pages kept) vs recent_scanned (pages examined) to guide reclaim decisions.
[Advanced]

Q: What is the inactive_ratio?
A: Target ratio of active to inactive pages. Controls how aggressively pages are demoted from active list.
[Advanced]

## Memory Cgroups

Q: What is a memory cgroup (memcg)?
A: Control group that limits and accounts memory usage of a group of processes.
[Intermediate]

Q: What does memory.limit_in_bytes control?
A: Maximum memory a cgroup can use before reclaim/OOM within the cgroup.
[Intermediate]

Q: What is memory.memsw.limit_in_bytes?
A: Combined limit on memory + swap usage for a cgroup.
[Intermediate]

Q: What is the mem_cgroup structure?
A: Kernel structure tracking memory usage, limits, and per-cgroup LRU lists.
[Advanced]

## Kernel Samepage Merging (KSM)

Q: What is KSM?
A: Kernel Samepage Merging - deduplicates identical pages, sharing one copy. Primarily for virtualization.
[Intermediate]

Q: How does KSM work?
A: ksmd scans pages marked MADV_MERGEABLE, finds identical content, replaces with single COW-shared page.
[Intermediate]

Q: What is the performance tradeoff of KSM?
A: Saves memory by deduplication but adds CPU overhead for scanning and COW faults on write.
[Intermediate]

## Memory Hotplug

Q: What is memory hotplug?
A: Adding or removing physical memory while system is running. Requires ZONE_MOVABLE and page migration.
[Advanced]

Q: Why is ZONE_MOVABLE important for hotplug?
A: Contains only movable pages that can be migrated, allowing the memory to be completely freed for removal.
[Advanced]

Q: What is memory offlining?
A: Process of migrating pages off a memory region before physical removal.
[Advanced]

## Final Review Cards

Q: List the main memory allocator APIs and their characteristics.
A: 
- kmalloc: small objects, physically contiguous, fast
- vmalloc: large objects, virtually contiguous, slow
- alloc_pages: page granularity, physical pages
- kmem_cache_alloc: fixed-size objects, fast
[Basic]

Q: What are the key zone watermarks and their effects?
A:
- WMARK_HIGH: kswapd stops reclaiming
- WMARK_LOW: kswapd starts reclaiming
- WMARK_MIN: direct reclaim triggered, allocations may fail
[Intermediate]

Q: Summarize the memory reclaim priority order.
A:
1. Free pages from buddy allocator
2. Page cache (clean pages first)
3. Anonymous pages (requires swap)
4. Slab caches (via shrinkers)
5. OOM killer (last resort)
[Intermediate]

Q: What must you verify before freeing memory?
A:
- Using correct free function (kfree, vfree, free_pages, kmem_cache_free)
- Not double-freeing
- Not freeing stack or static memory
- Pointer is valid (not NULL unless safe)
[Basic]

Q: List key differences between user and kernel address spaces.
A:
- User: per-process, pageable, protected, 0 to TASK_SIZE
- Kernel: global, not pageable, privileged, above TASK_SIZE
- User pages can swap, kernel pages cannot
- User space needs copy_to/from_user for kernel access
[Basic]

 Kernel v3.2 Memory Management - Anki Flashcards

This document contains 200+ Anki-style flashcards covering Linux kernel v3.2 memory management comprehensively.

---

## Section 1: Physical Memory Fundamentals

---

Q: What is a page frame in the Linux kernel?
A: A page frame is the smallest unit of physical memory that the kernel manages. On most architectures, it is 4KB (4096 bytes). Each page frame is represented by a `struct page` in the kernel.
[Basic]

---

Q: What is the purpose of `struct page` in Linux memory management?
A: `struct page` tracks the state of each physical page frame in the system. It contains metadata including reference counts, flags, LRU list linkage, and mapping information. There is one `struct page` for every physical page frame.
[Basic]

---

Q: In `struct page`, what does the `flags` field store?
A: The `flags` field stores atomic flags indicating page state: PG_locked (page is locked), PG_dirty (page has been modified), PG_lru (page is on an LRU list), PG_active (page is on active list), PG_slab (page is used by slab allocator), PG_buddy (page is free in buddy system), and zone/node encoding.
[Intermediate]

---

Q: What is the `_count` field in `struct page`?
A: `_count` is the reference count for the page. When `_count` equals 0, the page is free. When greater than 0, it indicates how many users hold a reference. Use `get_page()` to increment and `put_page()` to decrement.
[Basic]

---

Q: What is the difference between `_count` and `_mapcount` in `struct page`?
A: `_count` is the total reference count (all users including kernel). `_mapcount` specifically counts how many page table entries (PTEs) map this page. A page with `_mapcount >= 0` is mapped; `_mapcount == -1` means unmapped. For buddy system free pages, `_mapcount == PAGE_BUDDY_MAPCOUNT_VALUE (-128)`.
[Intermediate]

---

Q: What does the `mapping` field in `struct page` indicate?
A: If the low bit is clear, `mapping` points to an `address_space` structure (for page cache pages). If the low bit is set, it points to an `anon_vma` structure (for anonymous pages). This dual use is indicated by the `PAGE_MAPPING_ANON` bit.
[Intermediate]

---

Q: {{c1::page_to_pfn()}} converts a `struct page` pointer to a page frame number, while {{c2::pfn_to_page()}} does the reverse conversion.
A: page_to_pfn(), pfn_to_page()
[Basic]

---

Q: What does `page_address()` return?
A: `page_address()` returns the kernel virtual address for a given page. For lowmem pages, this is a direct calculation. For highmem pages, it returns the address if the page is currently kmapped, or NULL if not mapped.
[Basic]

---

Q: The `lru` field in `struct page` serves what purpose?
A: The `lru` field is a `list_head` that links the page into LRU (Least Recently Used) lists for page reclaim. When the page is free in the buddy system, this same field links it into the buddy free list.
[Intermediate]

---

Q: What is stored in the `private` field of `struct page`?
A: The `private` field is multipurpose: for buffer pages, it points to buffer_heads; for swap pages, it stores the swap entry; for free buddy pages, it indicates the order (power of 2) of the free block.
[Intermediate]

---

Q: What are the five memory zones in Linux kernel v3.2 (x86)?
A: 1) ZONE_DMA: <16MB for legacy ISA DMA devices
2) ZONE_DMA32: <4GB for 32-bit DMA devices (x86_64 only)
3) ZONE_NORMAL: Directly mapped kernel memory
4) ZONE_HIGHMEM: Memory above ~896MB (32-bit only), requires kmap
5) ZONE_MOVABLE: Virtual zone for migration
[Basic]

---

Q: Why does ZONE_DMA exist?
A: ZONE_DMA exists because some legacy hardware (especially ISA devices) can only perform DMA to the first 16MB of physical memory. The kernel reserves this zone for such devices when needed.
[Basic]

---

Q: What is ZONE_HIGHMEM and when is it used?
A: ZONE_HIGHMEM contains physical memory that cannot be permanently mapped into the kernel's virtual address space (on 32-bit systems, memory above ~896MB). Pages in this zone must be temporarily mapped using `kmap()` or `kmap_atomic()` before kernel access.
[Intermediate]

---

Q: Why doesn't 64-bit Linux have ZONE_HIGHMEM?
A: On 64-bit systems, the kernel virtual address space is large enough (128TB+) to directly map all physical memory. There's no need for temporary mappings, so ZONE_HIGHMEM doesn't exist on 64-bit architectures.
[Basic]

---

Q: What is the purpose of ZONE_MOVABLE?
A: ZONE_MOVABLE is a virtual zone containing pages that can be migrated. It's used for memory hotplug and huge page allocation. The kernel can migrate pages from ZONE_MOVABLE to defragment memory or remove memory modules.
[Intermediate]

---

Q: What are the three zone watermarks and their purposes?
A: WMARK_MIN: Absolute minimum free pages; triggers direct reclaim
WMARK_LOW: Wake kswapd to start background reclaim
WMARK_HIGH: kswapd stops reclaiming; zone is healthy
[Basic]

---

Q: What happens when free pages fall below WMARK_MIN?
A: When free pages fall below WMARK_MIN, the allocator enters direct reclaim - the allocating process itself must reclaim pages synchronously before allocation can succeed. This is a performance-critical situation.
[Intermediate]

---

Q: How do you access zone watermarks in kernel code?
A: Use macros: `min_wmark_pages(z)`, `low_wmark_pages(z)`, `high_wmark_pages(z)` where `z` is a pointer to `struct zone`. These access `zone->watermark[WMARK_MIN/LOW/HIGH]`.
[Basic]

---

Q: What is the `struct zone` structure?
A: `struct zone` represents a memory zone and contains: watermarks, free_area array for buddy allocator, LRU lists for page reclaim, per-CPU page lists, zone statistics, and the zone's page range information.
[Intermediate]

---

Q: In `struct zone`, what does `free_area[MAX_ORDER]` represent?
A: `free_area[MAX_ORDER]` is an array of free lists for the buddy allocator. Each `free_area[order]` contains lists of free page blocks of size 2^order pages, organized by migration type.
[Intermediate]

---

Q: What is MAX_ORDER in the Linux kernel and what does it default to?
A: MAX_ORDER defaults to 11, meaning the buddy allocator can allocate blocks from 2^0 (1 page) to 2^10 (1024 pages = 4MB). The maximum contiguous allocation is `MAX_ORDER_NR_PAGES = (1 << (MAX_ORDER - 1))`.
[Basic]

---

Q: What is PAGE_ALLOC_COSTLY_ORDER?
A: PAGE_ALLOC_COSTLY_ORDER is 3 (8 pages = 32KB). Allocations of this order or higher are considered "costly" because they're harder to satisfy without reclaim. The kernel may take extra measures for such allocations.
[Intermediate]

---

Q: What is the `lowmem_reserve` array in `struct zone`?
A: `lowmem_reserve` specifies how many pages to reserve in lower zones for higher zone allocations. It prevents lower zones from being depleted by allocations that could use higher zones, ensuring DMA and kernel allocations can succeed.
[Advanced]

---

Q: What is the purpose of zone padding (`ZONE_PADDING`) in `struct zone`?
A: Zone padding ensures that frequently accessed fields (like `lock` and `lru_lock`) fall into separate cache lines. This prevents false sharing between CPUs and improves SMP performance by reducing cache line bouncing.
[Advanced]

---

Q: What does the `pageset` field in `struct zone` contain?
A: `pageset` points to per-CPU page lists (`struct per_cpu_pageset`). These are hot caches of free pages that reduce lock contention on `zone->lock` by allowing CPUs to allocate/free pages without taking the zone lock.
[Intermediate]

---

Q: What is the purpose of `zone->lru_lock`?
A: `zone->lru_lock` protects the zone's LRU lists during page reclaim operations. It's one of the hottest locks in the kernel, so the zone structure is padded to keep it in a separate cache line from `zone->lock`.
[Intermediate]

---

Q: What does `zone->pages_scanned` track?
A: `zone->pages_scanned` counts pages scanned since the last successful reclaim. It helps determine if a zone is thrashing (scanning many pages but reclaiming few) and influences reclaim decisions.
[Advanced]

---

Q: What is `zone->all_unreclaimable` flag?
A: When set, `all_unreclaimable` indicates all pages in the zone are pinned and cannot be reclaimed. The OOM killer checks this flag to determine if memory is truly exhausted.
[Advanced]

---

Q: ASCII diagram: Draw the relationship between zones and struct page array.
A:
```
Physical Memory Layout:
+------------------+------------------+------------------+
|    ZONE_DMA      |   ZONE_NORMAL    |  ZONE_HIGHMEM    |
|    (0-16MB)      |  (16MB-896MB)    |   (896MB+)       |
+------------------+------------------+------------------+
        |                  |                  |
        v                  v                  v
+--------+--------+--------+--------+--------+--------+
|struct  |struct  |struct  |struct  |struct  |struct  |
|page[0] |page[1] |page[n] |page[n+1|page[m] |page[m+1|
+--------+--------+--------+--------+--------+--------+
         mem_map[] array (one entry per page frame)
```
[Basic]

---

Q: What function checks if a zone has enough free pages?
A: `zone_watermark_ok(zone, order, mark, classzone_idx, alloc_flags)` returns true if the zone has enough free pages at the specified watermark level for an allocation of the given order.
[Intermediate]

---

## Section 2: NUMA and Node Management

---

Q: What is `struct pglist_data` (pg_data_t)?
A: `struct pglist_data` represents a NUMA node's memory. It contains: `node_zones[]` array of zones, `node_zonelists[]` for allocation fallback, `node_mem_map` pointing to the node's page array, and the kswapd task pointer for the node.
[Intermediate]

---

Q: On a UMA (non-NUMA) system, how many `pglist_data` structures exist?
A: On a UMA system, there is exactly one `pglist_data` structure called `contig_page_data`. All memory belongs to node 0, accessed via `NODE_DATA(0)`.
[Basic]

---

Q: What is the purpose of `node_zonelists` in `pglist_data`?
A: `node_zonelists` contains ordered lists of zones to try during allocation. The first zonelist includes fallback to other nodes; the second (with GFP_THISNODE) restricts allocation to the local node only.
[Intermediate]

---

Q: What does `node_start_pfn` represent in `pglist_data`?
A: `node_start_pfn` is the page frame number of the first page in this NUMA node. Combined with `node_spanned_pages`, it defines the physical address range of the node.
[Basic]

---

Q: What is the difference between `node_present_pages` and `node_spanned_pages`?
A: `node_spanned_pages` is the total size including memory holes. `node_present_pages` is actual usable memory excluding holes. The difference accounts for firmware-reserved regions or memory holes.
[Intermediate]

---

Q: How do you access a node's pglist_data structure?
A: Use `NODE_DATA(nid)` macro where `nid` is the node ID. On UMA systems, this always returns `&contig_page_data`. On NUMA, it returns the appropriate node's structure.
[Basic]

---

Q: What NUMA memory policies are available in Linux?
A: MPOL_DEFAULT: Use system default allocation
MPOL_BIND: Allocate only from specified nodes
MPOL_INTERLEAVE: Round-robin across nodes
MPOL_PREFERRED: Prefer specific node, fallback to others
[Intermediate]

---

Q: Where is the NUMA policy for a VMA stored?
A: The NUMA policy is stored in `vm_area_struct->vm_policy` (when CONFIG_NUMA is enabled). This allows different memory regions in a process to have different NUMA allocation policies.
[Intermediate]

---

Q: What is the purpose of `kswapd` field in `pglist_data`?
A: Each NUMA node has its own kswapd kernel thread, stored in `pglist_data->kswapd`. This allows per-node memory reclaim, reducing cross-node memory pressure and improving NUMA locality.
[Intermediate]

---

Q: What is `kswapd_wait` in `pglist_data`?
A: `kswapd_wait` is a wait queue where kswapd sleeps when memory is sufficient. When zone watermarks are breached, `wakeup_kswapd()` wakes the appropriate node's kswapd through this wait queue.
[Advanced]

---

Q: What does `classzone_idx` in `pglist_data` indicate?
A: `classzone_idx` stores the highest zone type that kswapd should try to reclaim for. It's set when kswapd is woken and determines which zones kswapd will balance.
[Advanced]

---

Q: What is a zonelist in Linux memory allocation?
A: A zonelist is an ordered list of zones to try during allocation. It starts with the preferred zone and falls back to other zones (potentially on other NUMA nodes) if the preferred zone cannot satisfy the request.
[Basic]

---

Q: What is the `zonelist_cache` structure used for?
A: `zonelist_cache` accelerates zone scanning by caching which zones are full (`fullzones` bitmap) and zone-to-node mappings (`z_to_n[]`). This avoids repeated checks of empty zones during allocation.
[Advanced]

---

Q: How does `for_each_zone_zonelist()` work?
A: It iterates through a zonelist, visiting each zone at or below a specified zone index. Used during allocation to try zones in priority order until one can satisfy the request.
[Intermediate]

---

Q: What is the NUMA zonelist order?
A: Zonelist order determines fallback preference: "zone" order tries all zones of same type before moving to next type; "node" order tries all zones on same node first. Controlled via `/proc/sys/vm/numa_zonelist_order`.
[Advanced]

---

Q: What function builds all zonelists in the system?
A: `build_all_zonelists()` constructs zonelists for all nodes. Called during boot and when NUMA topology changes. Takes `zonelists_mutex` to protect against concurrent modifications.
[Intermediate]

---

Q: What is the `zoneref` structure?
A: `struct zoneref` contains a pointer to a zone and its zone index, stored in zonelists. This avoids repeatedly dereferencing the zone to get its index during allocation scans.
[Intermediate]

---

Q: How many zonelists does each node have in NUMA configuration?
A: Each NUMA node has MAX_ZONELISTS (2) zonelists:
[0]: Normal zonelist with fallback to other nodes
[1]: GFP_THISNODE zonelist restricting allocation to local node
[Intermediate]

---

Q: What is NUMA_HIT vs NUMA_MISS in zone statistics?
A: NUMA_HIT: Page allocated from the intended (requested) node
NUMA_MISS: Page allocated from a different node than requested
These counters help analyze NUMA memory locality.
[Intermediate]

---

Q: What does `local_memory_node()` return?
A: On systems with memoryless nodes, `local_memory_node(nid)` returns the nearest node with memory to node `nid`. On normal systems, it simply returns `nid`.
[Advanced]

---

## Section 3: Buddy Allocator

---

Q: What is the buddy system algorithm?
A: The buddy allocator manages physical pages by maintaining free lists of power-of-2 sized blocks (orders 0 to MAX_ORDER-1). When allocating, it finds the smallest sufficient block; when freeing, it merges adjacent "buddy" blocks to reduce fragmentation.
[Basic]

---

Q: How do you find a page's buddy in the buddy system?
A: The buddy's page frame number is: `buddy_pfn = page_pfn ^ (1 << order)`. XORing with the block size toggles the bit that distinguishes buddies. Buddies together form the next larger order block.
[Basic]

---

Q: ASCII diagram: Show buddy splitting for a 4-page allocation from an order-3 block.
A:
```
Order 3 (8 pages):  [0 1 2 3 4 5 6 7]
                           |
                    Split to Order 2
                           v
Order 2 (4 pages):  [0 1 2 3] [4 5 6 7]
                       |          |
                   Allocate    Return to
                    this       free list
                       v
                  [0 1 2 3] returned to user
```
[Basic]

---

Q: What is the `free_area` structure in the buddy allocator?
A: `struct free_area` contains:
- `free_list[MIGRATE_TYPES]`: Array of free page lists by migration type
- `nr_free`: Count of free blocks at this order
[Intermediate]

---

Q: What are the migration types in the buddy allocator?
A: MIGRATE_UNMOVABLE (0): Kernel pages that can't move
MIGRATE_RECLAIMABLE (1): Can be reclaimed (page cache)
MIGRATE_MOVABLE (2): User pages that can be migrated
MIGRATE_RESERVE (3): Emergency reserves
MIGRATE_ISOLATE (4): Isolated for memory hotplug
[Intermediate]

---

Q: Why does the buddy allocator separate pages by migration type?
A: Grouping pages by migration type reduces fragmentation. Unmovable pages clustered together prevent them from fragmenting movable regions. This allows larger contiguous allocations and enables memory compaction.
[Intermediate]

---

Q: What is the main allocation function in the buddy system?
A: `__alloc_pages_nodemask(gfp_mask, order, zonelist, nodemask)` is the core page allocator. It tries `get_page_from_freelist()` first, then `__alloc_pages_slowpath()` if that fails.
[Intermediate]

---

Q: What does `get_page_from_freelist()` do?
A: `get_page_from_freelist()` is the fast path allocator. It iterates through the zonelist, checking watermarks and trying to allocate from each zone until successful or all zones exhausted.
[Intermediate]

---

Q: What function removes a page block from the buddy free list?
A: `__rmqueue(zone, order, migratetype)` removes a block from the free list. It calls `__rmqueue_smallest()` to find the smallest suitable order, then `expand()` to split if necessary.
[Intermediate]

---

Q: What does the `expand()` function do in page allocation?
A: `expand()` splits a larger free block to satisfy a smaller allocation. It takes a block of order `high`, splits it repeatedly, and returns smaller buddies to the free list until reaching the requested order `low`.
[Intermediate]

---

Q: Code: How does buddy merging work when freeing a page?
A:
```c
static inline void __free_one_page(struct page *page,
                struct zone *zone, unsigned int order,
                int migratetype)
{
    unsigned long page_idx;
    unsigned long buddy_idx;
    struct page *buddy;

    page_idx = page_to_pfn(page) & ((1 << MAX_ORDER) - 1);

    while (order < MAX_ORDER-1) {
        buddy_idx = __find_buddy_index(page_idx, order);
        buddy = page + (buddy_idx - page_idx);
        if (!page_is_buddy(page, buddy, order))
            break;
        /* Remove buddy from free list */
        list_del(&buddy->lru);
        zone->free_area[order].nr_free--;
        /* Merge with buddy */
        combined_idx = buddy_idx & page_idx;
        page = page + (combined_idx - page_idx);
        page_idx = combined_idx;
        order++;
    }
    /* Add merged block to free list */
    list_add(&page->lru, &zone->free_area[order].free_list[migratetype]);
}
```
[Advanced]

---

Q: What does `page_is_buddy()` check?
A: `page_is_buddy()` verifies a potential buddy is mergeable:
1. Page is in the same zone
2. Page has PG_buddy flag set (is free)
3. Page's order (in page->private) matches
4. Page frame number is valid
[Intermediate]

---

Q: What GFP flags control which zones can be used?
A: __GFP_DMA: Allocate from ZONE_DMA only
__GFP_DMA32: Allocate from ZONE_DMA32 or lower
__GFP_HIGHMEM: Can use ZONE_HIGHMEM
(No flag): ZONE_NORMAL or lower
[Basic]

---

Q: What does GFP_KERNEL mean?
A: GFP_KERNEL = (__GFP_WAIT | __GFP_IO | __GFP_FS). It's the standard flag for kernel allocations: can sleep, can do I/O, can enter filesystem for reclaim. Used when not in atomic context.
[Basic]

---

Q: What is GFP_ATOMIC used for?
A: GFP_ATOMIC = (__GFP_HIGH). Cannot sleep or do I/O. Used in interrupt handlers, spinlock-held code, and other atomic contexts. May access emergency reserves (__GFP_HIGH).
[Basic]

---

Q: What does __GFP_WAIT flag indicate?
A: __GFP_WAIT indicates the allocator can sleep waiting for memory. Without this flag, allocation must complete immediately or fail. Required for direct reclaim.
[Basic]

---

Q: What is __GFP_ZERO?
A: __GFP_ZERO requests that the allocated pages be zeroed before returning. Equivalent to `alloc_pages()` followed by `memset(0)`, but may be optimized.
[Basic]

---

Q: What does __GFP_NOWARN do?
A: __GFP_NOWARN suppresses the kernel warning message that would normally be printed when an allocation fails. Used when the caller handles failure gracefully.
[Basic]

---

Q: What is __GFP_REPEAT?
A: __GFP_REPEAT tells the allocator to retry harder before failing. It will retry reclaim multiple times. Less aggressive than __GFP_NOFAIL but more persistent than default.
[Intermediate]

---

Q: What does __GFP_NOFAIL mean?
A: __GFP_NOFAIL tells the allocator to never fail - keep retrying forever. DANGEROUS: can cause hangs. Only for allocations where failure would be worse than waiting indefinitely.
[Intermediate]

---

Q: What is `alloc_pages()` vs `__get_free_pages()`?
A: `alloc_pages(gfp, order)` returns a `struct page *` pointer.
`__get_free_pages(gfp, order)` returns a kernel virtual address (unsigned long). The latter calls the former and converts the result.
[Basic]

---

Q: What is `alloc_page()` (singular)?
A: `alloc_page(gfp)` is a wrapper that calls `alloc_pages(gfp, 0)` - it allocates a single page (order 0). Returns `struct page *`.
[Basic]

---

Q: What is the difference between `free_pages()` and `__free_pages()`?
A: `free_pages(addr, order)` takes a virtual address.
`__free_pages(page, order)` takes a `struct page *` pointer.
Both free 2^order pages back to the buddy system.
[Basic]

---

Q: What is `__alloc_pages_slowpath()`?
A: `__alloc_pages_slowpath()` is the slow allocation path when fast path fails. It tries: waking kswapd, direct reclaim, compaction, and ultimately the OOM killer if nothing else works.
[Advanced]

---

Q: What order allocation triggers memory compaction?
A: Allocations of order >= PAGE_ALLOC_COSTLY_ORDER (3) may trigger compaction when direct reclaim alone isn't sufficient. Compaction migrates pages to create contiguous free blocks.
[Advanced]

---

Q: What is the per-CPU page cache (PCP) in the allocator?
A: Each CPU has a `per_cpu_pages` cache of free pages. Single-page allocations/frees use this cache to avoid zone lock contention. Pages are refilled/drained in batches from the buddy allocator.
[Intermediate]

---

Q: What fields are in `struct per_cpu_pages`?
A: `count`: Number of pages currently cached
`high`: High watermark; trigger drain when exceeded
`batch`: Number of pages to move in/out of buddy system
`lists[MIGRATE_PCPTYPES]`: Separate lists by migration type
[Intermediate]

---

Q: How does the PCP allocation work?
A: For order-0 allocations:
1. Check local CPU's PCP list for requested migratetype
2. If empty, refill `batch` pages from buddy system
3. Return page from PCP list
This avoids zone->lock for most single-page allocations.
[Intermediate]

---

Q: What is MIGRATE_PCPTYPES?
A: MIGRATE_PCPTYPES = 3, the number of migration types tracked in per-CPU page lists. Only UNMOVABLE, RECLAIMABLE, and MOVABLE are cached per-CPU; RESERVE and ISOLATE are not.
[Intermediate]

---

Q: What happens during `__alloc_pages_direct_reclaim()`?
A: Direct reclaim is synchronous reclaim by the allocating process:
1. Call `try_to_free_pages()` to reclaim memory
2. Check if enough pages were freed
3. Retry allocation from zones
4. Return success or continue to other recovery methods
[Advanced]

---

## Section 4: Virtual Memory

---

Q: What is `struct mm_struct`?
A: `struct mm_struct` represents a process's complete address space. It contains: VMA list/tree, page table root (pgd), memory region boundaries (code, data, stack, brk), and various memory statistics.
[Basic]

---

Q: What is the `mmap` field in `mm_struct`?
A: `mmap` is the head of a linked list of all VMAs (Virtual Memory Areas) in the address space, sorted by virtual address. Used for sequential traversal of the address space.
[Basic]

---

Q: What is `mm_rb` in `mm_struct`?
A: `mm_rb` is the root of a red-black tree containing all VMAs, indexed by virtual address. Enables O(log n) VMA lookup by address, faster than the linear linked list.
[Intermediate]

---

Q: What is the purpose of `mmap_cache` in `mm_struct`?
A: `mmap_cache` caches the result of the last `find_vma()` call. Since consecutive memory accesses often hit the same VMA, this provides O(1) lookup for the common case.
[Intermediate]

---

Q: What does `mm_struct->pgd` point to?
A: `pgd` points to the Page Global Directory - the top level of the process's page tables. On context switch, this is loaded into the CPU's page table base register (CR3 on x86).
[Basic]

---

Q: What are the key memory boundaries stored in `mm_struct`?
A: `start_code, end_code`: Text segment
`start_data, end_data`: Data segment
`start_brk, brk`: Heap boundaries
`start_stack`: Stack start
`arg_start, arg_end, env_start, env_end`: Arguments and environment
[Intermediate]

---

Q: What does `mm_struct->total_vm` represent?
A: `total_vm` counts total pages mapped in the address space (in page units). Includes all VMAs regardless of whether physical pages are allocated (demand paging).
[Basic]

---

Q: What is `mm_users` vs `mm_count` in `mm_struct`?
A: `mm_users`: Count of processes sharing this mm (threads)
`mm_count`: Reference count including kernel references
When `mm_users` reaches 0, address space is torn down.
When `mm_count` reaches 0, `mm_struct` is freed.
[Intermediate]

---

Q: What is a Virtual Memory Area (VMA)?
A: A VMA (`struct vm_area_struct`) represents a contiguous region of virtual address space with uniform properties (permissions, backing). A process's address space is composed of multiple non-overlapping VMAs.
[Basic]

---

Q: What are the key fields in `struct vm_area_struct`?
A: `vm_start, vm_end`: Virtual address range [start, end)
`vm_mm`: Owning mm_struct
`vm_flags`: Permission and property flags
`vm_page_prot`: Page protection bits for PTEs
`vm_ops`: Operations (fault handler, etc.)
`vm_file`: Backing file (if file-mapped)
[Intermediate]

---

Q: What do VMA flags VM_READ, VM_WRITE, VM_EXEC indicate?
A: VM_READ: Region is readable
VM_WRITE: Region is writable
VM_EXEC: Region is executable
These determine page-level protections and are checked on access faults.
[Basic]

---

Q: What is the difference between VM_SHARED and VM_PRIVATE?
A: VM_SHARED: Changes are visible to other processes mapping the same object (shared memory, shared file mappings)
VM_PRIVATE: Changes are private via copy-on-write; modifications don't affect the original or other mappers
[Basic]

---

Q: What does VM_GROWSDOWN indicate?
A: VM_GROWSDOWN indicates a stack region that grows toward lower addresses. The kernel can automatically expand such VMAs downward on a fault below the current boundary (stack growth).
[Intermediate]

---

Q: What is VM_DENYWRITE?
A: VM_DENYWRITE indicates writes to the mapped file should be denied while the mapping exists. Used for executable mappings to prevent modification of running code.
[Intermediate]

---

Q: What function finds the VMA containing a given address?
A: `find_vma(mm, addr)` returns the first VMA with `vm_end > addr`, or NULL if none exists. Note: it may return a VMA that doesn't contain addr (if addr is in a hole).
[Basic]

---

Q: How do you check if an address is actually inside the returned VMA?
A: After `vma = find_vma(mm, addr)`, check: `if (vma && vma->vm_start <= addr)` - then addr is inside the VMA. If `vma->vm_start > addr`, the address is in a gap before this VMA.
[Intermediate]

---

Q: What does `vma_merge()` do?
A: `vma_merge()` tries to merge a new VMA with adjacent existing VMAs if they have compatible properties (same flags, file, offset). Reduces VMA count and memory overhead.
[Intermediate]

---

Q: What is `split_vma()` used for?
A: `split_vma()` divides a VMA into two at a specified address. Used by `mprotect()` when changing permissions on part of a VMA, or `munmap()` when unmapping the middle of a VMA.
[Intermediate]

---

Q: ASCII diagram: Show the four-level page table hierarchy.
A:
```
Virtual Address (48-bit on x86_64):
+-------+-------+-------+-------+------------------+
| PGD   | PUD   | PMD   | PTE   |    Offset        |
| 9 bits| 9 bits| 9 bits| 9 bits|    12 bits       |
+-------+-------+-------+-------+------------------+
    |       |       |       |           |
    v       v       v       v           |
  +---+   +---+   +---+   +---+         |
  |PGD|-->|PUD|-->|PMD|-->|PTE|--+      |
  +---+   +---+   +---+   +---+  |      |
                                v      v
                           +---------+----+
                           |Physical |Off |
                           |Page Addr|    |
                           +---------+----+
```
[Basic]

---

Q: What are PGD, PUD, PMD, and PTE?
A: PGD: Page Global Directory (top level)
PUD: Page Upper Directory
PMD: Page Middle Directory
PTE: Page Table Entry (bottom level, points to physical page)
Each level is indexed by bits from the virtual address.
[Basic]

---

Q: How do you get the PGD entry for a virtual address?
A: `pgd = pgd_offset(mm, addr)` returns a pointer to the PGD entry for address `addr` in address space `mm`. For kernel addresses, use `pgd_offset_k(addr)`.
[Basic]

---

Q: What function allocates a PUD if it doesn't exist?
A: `pud_alloc(mm, pgd, addr)` returns the PUD entry, allocating a new PUD page if the PGD entry was empty. Used when creating new page table entries.
[Intermediate]

---

Q: What is the call sequence to walk page tables?
A:
```c
pgd = pgd_offset(mm, addr);
pud = pud_offset(pgd, addr);
pmd = pmd_offset(pud, addr);
pte = pte_offset_map(pmd, addr);
// Use pte...
pte_unmap(pte);
```
[Intermediate]

---

Q: What does `pte_present()` check?
A: `pte_present(pte)` returns true if the PTE maps a valid physical page currently in memory. Returns false for not-present pages (unmapped, swapped out, or demand-zero).
[Basic]

---

Q: What does `pte_none()` indicate?
A: `pte_none(pte)` returns true if the PTE is completely empty (zero). This means the virtual address has never been accessed or has been fully unmapped.
[Basic]

---

Q: What is the difference between `pte_offset_map()` and `pte_offset_kernel()`?
A: `pte_offset_map()` may need to kmap the page table page on highmem systems; must be balanced with `pte_unmap()`.
`pte_offset_kernel()` is for kernel page tables that are always mapped.
[Intermediate]

---

Q: What does `mk_pte()` do?
A: `mk_pte(page, prot)` creates a PTE value from a `struct page` and protection bits. It combines the physical page frame number with the specified page protection flags.
[Intermediate]

---

Q: What is `set_pte_at()` used for?
A: `set_pte_at(mm, addr, ptep, pte)` sets a PTE value at the given location. It handles architecture-specific requirements and updates the page table correctly.
[Intermediate]

---

Q: What does `pte_mkwrite()` do?
A: `pte_mkwrite(pte)` returns a new PTE value with the write permission bit set. Used when making a page writable, such as after copy-on-write.
[Intermediate]

---

Q: What functions flush the TLB?
A: `flush_tlb_page(vma, addr)`: Flush single page entry
`flush_tlb_range(vma, start, end)`: Flush address range
`flush_tlb_mm(mm)`: Flush entire address space
`flush_tlb_all()`: Flush all TLBs on all CPUs
[Intermediate]

---

Q: Why is TLB flushing necessary after page table changes?
A: The TLB caches virtual-to-physical translations. After modifying page tables, stale TLB entries could cause incorrect translations. Flushing ensures the CPU uses the updated page tables.
[Basic]

---

Q: What is `struct mmu_gather`?
A: `struct mmu_gather` batches TLB flush operations during munmap. Instead of flushing after each PTE change, it collects pages and performs a single batched flush at the end for efficiency.
[Advanced]

---

Q: What are the main steps in `tlb_finish_mmu()`?
A: 1. Flush the TLB for all modified entries (batched)
2. Free all collected page table pages
3. Free all collected user pages
This completes the batched unmapping operation.
[Advanced]

---

Q: What is vm_page_prot in a VMA?
A: `vm_page_prot` is the architecture-specific page protection value derived from vm_flags. It's the value used directly in PTEs for pages in this VMA.
[Intermediate]

---

Q: What is `vm_ops` in a VMA?
A: `vm_ops` points to a `vm_operations_struct` containing function pointers for VMA operations: fault() for page fault handling, open()/close() for VMA creation/destruction, and others.
[Intermediate]

---

Q: What is the fault handler in vm_operations_struct?
A: `int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf)` handles page faults in the VMA. It allocates/finds the page to map and returns a status code (VM_FAULT_NOPAGE, VM_FAULT_MAJOR, etc.).
[Intermediate]

---

---

## Section 5: Page Fault Handling

---

Q: What triggers a page fault?
A: A page fault occurs when:
1. Accessing an address with no PTE (not present)
2. Accessing with wrong permissions (write to read-only)
3. Accessing supervisor page from user mode
The CPU raises an exception that the kernel handles.
[Basic]

---

Q: What is the entry point for page fault handling on x86?
A: `do_page_fault()` in `arch/x86/mm/fault.c` is the page fault exception handler. It determines the fault type, finds the VMA, and calls `handle_mm_fault()` for valid user-space faults.
[Intermediate]

---

Q: What function does the main page fault work?
A: `handle_mm_fault(mm, vma, addr, flags)` handles page faults after initial validation. It walks/allocates page tables and calls `handle_pte_fault()` to resolve the actual fault.
[Intermediate]

---

Q: What are the three main types of page faults?
A: 1. Anonymous fault: First access to anonymous memory (zero page needed)
2. File fault: First access to file-mapped page (read from disk)
3. Swap fault: Access to page that was swapped out (read from swap)
[Basic]

---

Q: What is `do_anonymous_page()` responsible for?
A: `do_anonymous_page()` handles faults on anonymous VMAs where no page exists. For reads, it may map the zero page; for writes, it allocates a new zeroed page and installs the PTE.
[Intermediate]

---

Q: What is the "zero page" optimization?
A: For read faults on anonymous memory, instead of allocating a new zeroed page, the kernel maps the shared zero page read-only. A real page is only allocated on first write (minor fault optimization).
[Intermediate]

---

Q: What does `do_fault()` handle?
A: `do_fault()` handles file-backed and shared memory faults. It calls `__do_fault()` which invokes the VMA's fault handler to get the page from the page cache or file system.
[Intermediate]

---

Q: What is `do_swap_page()` responsible for?
A: `do_swap_page()` handles faults on pages that were swapped out. It reads the swap entry from the PTE, retrieves the page from swap cache or disk, and re-establishes the mapping.
[Intermediate]

---

Q: What is a major vs minor page fault?
A: Minor fault: Page was in memory (page cache, zero page), only PTE setup needed - fast
Major fault: Page required disk I/O (file read, swap in) - slow
The distinction affects performance accounting.
[Basic]

---

Q: What is copy-on-write (COW)?
A: COW is a lazy copying optimization. When a process forks, parent and child share pages marked read-only. On first write, a fault occurs, the page is copied, and the writer gets a private copy.
[Basic]

---

Q: How does the kernel implement COW?
A: 1. During fork, parent and child PTEs point to same page, marked read-only
2. Page's mapcount incremented for child's mapping
3. On write fault, `do_wp_page()` allocates new page, copies content
4. New page mapped writable in faulting process only
[Intermediate]

---

Q: What function handles write protection faults (COW)?
A: `do_wp_page()` handles write faults on read-only pages. It checks if COW is needed (page shared), allocates a new page if so, copies content, and updates the PTE to point to the private copy.
[Intermediate]

---

Q: Code: Basic page fault handling flow.
A:
```c
/* Simplified flow */
int handle_mm_fault(mm, vma, addr, flags)
{
    pte = walk_page_tables(mm, addr);  /* May allocate tables */
    
    if (pte_none(*pte))
        return handle_pte_fault(mm, vma, addr, pte, ...);
    if (!pte_present(*pte))
        return do_swap_page(mm, vma, addr, pte, ...);
    if (write_fault && !pte_write(*pte))
        return do_wp_page(mm, vma, addr, pte, ...);
    /* Should not reach here */
    return VM_FAULT_SIGBUS;
}
```
[Intermediate]

---

Q: What is `struct vm_fault`?
A: `struct vm_fault` passes fault information to fault handlers: virtual address, PTE location, fault flags (read/write), and outputs (page to map, VM_FAULT_* return code).
[Intermediate]

---

Q: What does VM_FAULT_NOPAGE return value mean?
A: VM_FAULT_NOPAGE indicates the fault handler installed the PTE itself and no further action is needed. Used when the handler does custom PTE manipulation.
[Intermediate]

---

Q: What does VM_FAULT_MAJOR indicate?
A: VM_FAULT_MAJOR indicates the fault required disk I/O (a major fault). Used for accounting - major faults are expensive and tracked separately from minor faults.
[Intermediate]

---

Q: What does VM_FAULT_OOM mean?
A: VM_FAULT_OOM indicates the fault handler couldn't allocate memory. The kernel may invoke the OOM killer or return -ENOMEM to the faulting process.
[Intermediate]

---

Q: What happens if a page fault occurs at an invalid address?
A: If no VMA contains the faulting address, or permissions don't match (user accessing kernel), `do_page_fault()` sends SIGSEGV to the process (segmentation fault).
[Basic]

---

Q: How does the kernel distinguish kernel vs user page faults?
A: The faulting address determines this: addresses below TASK_SIZE are user space; above are kernel space. Additional checks include the execution context (error code on x86).
[Intermediate]

---

Q: What is `filemap_fault()` used for?
A: `filemap_fault()` is the default fault handler for file-mapped VMAs (in `vm_operations_struct`). It looks up or reads the page from the page cache for the mapped file.
[Intermediate]

---

Q: What is demand paging?
A: Demand paging means pages are only allocated when first accessed, not when VMAs are created. `mmap()` creates VMAs but no PTEs; pages are faulted in on first access.
[Basic]

---

Q: How is a page fault converted to a memory allocation?
A: 1. Fault exception → `do_page_fault()` → `handle_mm_fault()`
2. Determine fault type (anon/file/swap)
3. Call appropriate handler to get/allocate page
4. Allocate PTE page tables if needed
5. Install PTE mapping page to address
[Intermediate]

---

Q: What is `can_share_swap_page()`?
A: `can_share_swap_page()` checks if a swapped page can be used directly without copying. If only one process maps it (mapcount == 1), it can be reused; otherwise COW is needed.
[Advanced]

---

Q: What is the role of `anon_vma` in page fault handling?
A: `anon_vma` links all VMAs that might share anonymous pages (due to COW). During page fault, it helps determine if a page is shared and needs copying, and enables reverse mapping for reclaim.
[Advanced]

---

---

## Section 6: Slab Allocators

---

Q: What problem do slab allocators solve?
A: Slab allocators efficiently manage small kernel object allocations (smaller than page size). They reduce fragmentation, cache frequently-used object types, and improve performance through object reuse without re-initialization.
[Basic]

---

Q: What are the three slab allocator implementations in Linux?
A: SLAB: Original, complex with per-CPU and shared caches
SLUB: Simpler, default in modern kernels, better scalability
SLOB: Minimal memory footprint, for embedded systems
[Basic]

---

Q: What is a slab cache (`struct kmem_cache`)?
A: A slab cache manages objects of a specific size/type. It contains: object size, alignment, constructor function, name, and per-CPU/per-node object lists. Each cache serves one type of allocation.
[Intermediate]

---

Q: What does `kmem_cache_create()` do?
A: `kmem_cache_create(name, size, align, flags, ctor)` creates a new slab cache for objects of the given size. Returns a `struct kmem_cache *` used for subsequent allocations.
[Basic]

---

Q: What is a slab?
A: A slab is a contiguous set of pages dedicated to a single cache. It contains multiple objects of the same type plus metadata. Slabs can be full, partial (some objects free), or empty.
[Basic]

---

Q: ASCII diagram: Show slab structure.
A:
```
+---------------------------------------------+
|                   SLAB                      |
+---------------------------------------------+
|  object  |  object  |  object  |  object   |
|   [0]    |   [1]    |   [2]    |   [3]     |
|  (used)  |  (free)  |  (used)  |  (free)   |
+---------------------------------------------+
|                 Metadata                    |
|  freelist: [1] -> [3] -> NULL               |
+---------------------------------------------+

One or more contiguous pages from buddy allocator
```
[Basic]

---

Q: What is `kmem_cache_alloc()` and how does it work?
A: `kmem_cache_alloc(cache, gfp)` allocates one object from the specified cache. It first checks per-CPU caches, then partial slabs, then allocates a new slab if needed.
[Basic]

---

Q: What does `kmem_cache_free()` do?
A: `kmem_cache_free(cache, ptr)` returns an object to its cache. The object is added to the freelist. If the slab becomes empty and there are enough partial slabs, it may be freed to the buddy allocator.
[Basic]

---

Q: What is `kmalloc()` and how does it relate to slab?
A: `kmalloc(size, gfp)` allocates memory of arbitrary size. Internally, it selects an appropriate general-purpose cache (`kmalloc-N` where N is a size class) and calls `kmem_cache_alloc()`.
[Basic]

---

Q: What are the kmalloc size classes?
A: kmalloc uses caches for sizes: 8, 16, 32, 64, 96, 128, 192, 256, 512, 1024, 2048, 4096, 8192 bytes. Allocations are rounded up to the next size class.
[Basic]

---

Q: What does `kfree()` do internally?
A: `kfree(ptr)` determines which cache the object belongs to (from page metadata), then calls `kmem_cache_free()` on that cache. Freeing NULL is safe and does nothing.
[Basic]

---

Q: What is `kzalloc()`?
A: `kzalloc(size, gfp)` is equivalent to `kmalloc(size, gfp | __GFP_ZERO)`. It allocates memory and zeros it. Convenience wrapper for a common operation.
[Basic]

---

Q: What is `kcalloc()`?
A: `kcalloc(n, size, gfp)` allocates an array of n elements of given size, zeroed. It safely handles multiplication overflow, unlike `kzalloc(n * size, gfp)`.
[Basic]

---

Q: What is `krealloc()`?
A: `krealloc(ptr, new_size, gfp)` changes the size of an allocation. May allocate new memory and copy if the new size requires a different cache.
[Intermediate]

---

Q: What is the SLUB freelist?
A: In SLUB, each slab's freelist is a pointer chain through free objects. The first free object's address is stored; each free object points to the next. No separate metadata array needed.
[Intermediate]

---

Q: How does SLUB differ from SLAB?
A: SLUB is simpler:
- No separate slab metadata structure
- Freelist embedded in free objects
- Per-CPU partial slabs instead of array caches
- Better memory utilization
- More scalable on large systems
[Intermediate]

---

Q: What is a per-CPU partial slab in SLUB?
A: SLUB keeps partial slabs per-CPU to reduce lock contention. Each CPU has a list of partial slabs it can allocate from without touching global locks.
[Intermediate]

---

Q: What fields in `struct page` are used for SLUB?
A: For SLUB slab pages:
- `freelist`: Points to first free object
- `inuse`, `objects`: Count statistics
- `frozen`: Indicates per-CPU ownership
- `slab`: Points back to `kmem_cache`
[Intermediate]

---

Q: What is `frozen` bit in SLUB?
A: A frozen slab belongs to a specific CPU's freelist and can only be allocated from by that CPU. Unfrozen slabs are on the node partial list and need locking for access.
[Advanced]

---

Q: What is SLUB's fast path for allocation?
A: Check `c->freelist` (per-CPU freelist):
1. If non-NULL, pop object from freelist
2. Update freelist to next free object
3. Return object
No locks needed for fast path.
[Intermediate]

---

Q: What happens in SLUB slow path allocation?
A: 1. Check per-CPU partial slab list
2. If empty, check node partial list (needs lock)
3. If no partials, allocate new slab from buddy
4. New slab becomes frozen per-CPU slab
[Intermediate]

---

Q: What is object poisoning in slab allocators?
A: Poisoning fills freed objects with a magic pattern (e.g., 0x6b). On allocation, the pattern is verified. This detects use-after-free bugs. Enabled by CONFIG_DEBUG_SLAB or slab_debug boot option.
[Intermediate]

---

Q: What is red zoning in slab allocators?
A: Red zoning adds guard bytes before/after each object. On free, guards are checked for corruption. Detects buffer overflows/underflows. Part of slab debugging options.
[Intermediate]

---

Q: What does `kmem_cache_shrink()` do?
A: `kmem_cache_shrink(cache)` frees all empty slabs in the cache back to the buddy allocator. Called to reclaim memory when the cache has grown large.
[Intermediate]

---

Q: What is `kmem_cache_destroy()` used for?
A: `kmem_cache_destroy(cache)` destroys a cache created by `kmem_cache_create()`. Must only be called when all objects have been freed; otherwise it's a bug.
[Basic]

---

Q: What is SLOB and when is it used?
A: SLOB (Simple List Of Blocks) is a minimal slab allocator for memory-constrained embedded systems. Uses simple first-fit allocation. Smallest memory footprint but least efficient for large systems.
[Basic]

---

Q: What is the constructor function in slab caches?
A: The constructor (ctor parameter to `kmem_cache_create()`) initializes objects when first allocated from buddy system. Not called on every allocation - only when new slabs are populated.
[Intermediate]

---

Q: How is `sizeof` of a slab cache determined?
A: The effective object size is: requested size + alignment padding + optional debug overhead (red zones, track buffers). SLUB packs objects tightly; SLAB may have more overhead.
[Intermediate]

---

Q: What's the difference between `GFP_KERNEL` and `GFP_ATOMIC` for kmalloc?
A: GFP_KERNEL: Can sleep, do I/O, use filesystem - normal kernel allocation
GFP_ATOMIC: Cannot sleep, may access reserves - interrupt/atomic context
GFP_ATOMIC may fail more often due to no reclaim.
[Basic]

---

---

## Section 7: vmalloc and Per-CPU Allocators

---

Q: What is `vmalloc()` used for?
A: `vmalloc(size)` allocates virtually contiguous memory that may be physically discontiguous. Used for large allocations where physical contiguity isn't required, like kernel modules and large buffers.
[Basic]

---

Q: What is the difference between `kmalloc()` and `vmalloc()`?
A: kmalloc: Physically contiguous, fast, limited size
vmalloc: Virtually contiguous, can be large, requires page table setup, slightly slower access due to TLB pressure
[Basic]

---

Q: When should you use vmalloc vs kmalloc?
A: Use kmalloc for small allocations and when physical contiguity is needed (DMA). Use vmalloc for large allocations where physical contiguity isn't required. vmalloc has more overhead per allocation.
[Basic]

---

Q: What is `vfree()` used for?
A: `vfree(addr)` frees memory allocated by `vmalloc()`, `vmalloc_user()`, or `vzalloc()`. Must not be called in interrupt context (can sleep).
[Basic]

---

Q: What is `vzalloc()`?
A: `vzalloc(size)` is vmalloc with zeroing - equivalent to vmalloc followed by memset to zero. Convenience function for common pattern.
[Basic]

---

Q: What is `struct vm_struct` in vmalloc?
A: `struct vm_struct` tracks vmalloc allocations: virtual address range, size, physical pages array, and flags. Not the same as `vm_area_struct` (which is for user space).
[Intermediate]

---

Q: ASCII diagram: Show vmalloc memory layout.
A:
```
Kernel Virtual Address Space:
+------------------+
|   Direct Mapped  |  <- PAGE_OFFSET (lowmem)
|   (kmalloc)      |
+------------------+
|      VMALLOC     |  <- VMALLOC_START
|       Area       |
|                  |
|  +------------+  |
|  | vm_struct  |  |
|  |  addr=0xA  |  |
|  |  pages[]   |--+-----> scattered physical pages
|  +------------+  |
+------------------+  <- VMALLOC_END
|      Modules     |
+------------------+
```
[Intermediate]

---

Q: What does `vmap()` do?
A: `vmap(pages, count, flags, prot)` maps an array of `struct page *` into contiguous kernel virtual address space. Unlike vmalloc, you provide pre-allocated pages.
[Intermediate]

---

Q: What is `vunmap()` used for?
A: `vunmap(addr)` unmaps a region created by `vmap()`. It removes the page table entries but does NOT free the underlying pages - caller must do that separately.
[Intermediate]

---

Q: What does `ioremap()` do?
A: `ioremap(phys_addr, size)` maps physical device memory (MMIO regions) into kernel virtual space. Used for memory-mapped I/O, not for regular RAM.
[Basic]

---

Q: What is the difference between `ioremap()` and `vmalloc()`?
A: ioremap: Maps physical device memory at specified address, doesn't allocate pages
vmalloc: Allocates physical pages AND maps them to virtual addresses
ioremap is for MMIO; vmalloc is for kernel buffers.
[Intermediate]

---

Q: What is `__vmalloc()`?
A: `__vmalloc(size, gfp_mask, prot)` is the lower-level vmalloc with explicit GFP flags and protection. `vmalloc()` is a wrapper calling this with default flags.
[Intermediate]

---

Q: What is `alloc_percpu()` used for?
A: `alloc_percpu(type)` allocates memory with separate copies for each CPU. Each CPU accesses its own copy without locking. Used for per-CPU counters, caches, data.
[Basic]

---

Q: How do you access per-CPU memory?
A: Use `get_cpu_var(var)` / `put_cpu_var(var)` or `per_cpu_ptr(ptr, cpu)`. These handle CPU ID lookup and preemption. Example: `int *myint = per_cpu_ptr(pcpu_ptr, smp_processor_id());`
[Basic]

---

Q: What does `free_percpu()` do?
A: `free_percpu(ptr)` frees memory allocated by `alloc_percpu()`. Frees copies on all CPUs.
[Basic]

---

Q: What is the per-CPU page allocator cache?
A: `struct per_cpu_pageset` caches single pages per-CPU to reduce zone lock contention. When allocating order-0 pages, the allocator first checks this cache before taking zone->lock.
[Intermediate]

---

Q: What is `per_cpu_pages.batch`?
A: `batch` is the number of pages moved between per-CPU cache and buddy system at once. When cache empties, `batch` pages are obtained from buddy. When too full, `batch` pages return.
[Intermediate]

---

Q: What problem does per-CPU data solve?
A: Per-CPU data eliminates cache line bouncing and locking for frequently-accessed data. Each CPU has its own copy, so no synchronization needed for local access. Critical for scalability.
[Basic]

---

Q: What is `DEFINE_PER_CPU()`?
A: `DEFINE_PER_CPU(type, name)` declares a per-CPU variable at compile time. Each CPU gets its own copy. Access via `this_cpu_ptr()` or `per_cpu()` macros.
[Intermediate]

---

---

## Section 8: Page Reclaim and OOM

---

Q: What is page reclaim?
A: Page reclaim frees physical pages by removing them from use: writing dirty pages to disk, dropping clean page cache, swapping anonymous pages. Keeps free memory above watermarks.
[Basic]

---

Q: What are the LRU lists?
A: LRU_INACTIVE_ANON: Anonymous pages not recently accessed
LRU_ACTIVE_ANON: Recently accessed anonymous pages
LRU_INACTIVE_FILE: File pages not recently accessed
LRU_ACTIVE_FILE: Recently accessed file pages
LRU_UNEVICTABLE: Pages that cannot be reclaimed
[Basic]

---

Q: Why are anonymous and file pages on separate LRU lists?
A: File pages can be reclaimed by discarding (if clean) or writing to file. Anonymous pages must be swapped out. Separate lists allow different reclaim policies and ratios between them.
[Intermediate]

---

Q: What determines if a page is active or inactive?
A: The PG_active flag indicates active/inactive. Pages start inactive; accessing them sets PG_referenced. With both referenced and repeated access, pages move to active. Lack of access demotes to inactive.
[Intermediate]

---

Q: How does the two-list LRU algorithm work?
A: 1. New pages added to inactive list
2. Second access promotes to active list
3. Pages age on active list; old ones demote to inactive
4. Reclaim scans inactive list first
5. Referenced inactive pages get second chance
[Intermediate]

---

Q: What is kswapd?
A: `kswapd` is a per-node kernel thread that performs background page reclaim. It wakes when zone free pages fall below WMARK_LOW and reclaims until WMARK_HIGH is reached.
[Basic]

---

Q: What function does kswapd call to reclaim memory?
A: `kswapd` calls `balance_pgdat()` which repeatedly calls `kswapd_shrink_zone()` to scan and reclaim pages from zones until watermarks are satisfied.
[Intermediate]

---

Q: What is direct reclaim?
A: Direct reclaim occurs when an allocator can't get pages and kswapd isn't fast enough. The allocating process itself calls `try_to_free_pages()` to synchronously reclaim memory.
[Basic]

---

Q: What is `struct scan_control`?
A: `struct scan_control` holds parameters for reclaim: target pages to reclaim, priority level, allowed operations (writepage, unmap, swap), and results. Passed through reclaim functions.
[Intermediate]

---

Q: What is reclaim priority?
A: Priority (0-12) controls aggressiveness. At priority N, scan `lru_size >> N` pages. Lower priority = more aggressive. DEF_PRIORITY is 12, scanning 1/4096 of LRU per pass.
[Intermediate]

---

Q: How does `shrink_lruvec()` work?
A: `shrink_lruvec()` scans a zone's LRU lists:
1. Determine pages to scan per list (based on priority, size ratios)
2. Call `shrink_list()` for each LRU
3. Balance between anonymous and file pages
4. Return number of pages reclaimed
[Advanced]

---

Q: What does `shrink_page_list()` do with each page?
A: For each page in the list:
1. Try to take page lock
2. Check if reclaimable (not pinned, not dirty, etc.)
3. If dirty, start writeback
4. If mapped, try to unmap
5. If clean/written, free the page
[Intermediate]

---

Q: What is `pageout()` in reclaim?
A: `pageout()` initiates writeback for a dirty page. It calls the page's `writepage()` operation to start async I/O. For anon pages, this involves the swap subsystem.
[Intermediate]

---

Q: What is `try_to_unmap()`?
A: `try_to_unmap(page, flags)` removes all mappings to a page using reverse mapping (rmap). Required before a mapped page can be reclaimed. Returns success only if all PTEs unmapped.
[Intermediate]

---

Q: What is reverse mapping (rmap)?
A: Rmap allows finding all PTEs that map a given page. For file pages, walk the address_space's priority tree. For anon pages, walk the anon_vma's list. Essential for reclaim of mapped pages.
[Advanced]

---

Q: What happens if reclaim can't free enough pages?
A: If reclaim fails repeatedly and priority reaches 0, the kernel triggers the OOM killer to terminate processes and free their memory.
[Basic]

---

Q: What is the OOM killer?
A: The OOM (Out-Of-Memory) killer selects and terminates a process when the system is critically low on memory. It's a last resort to prevent complete system hang.
[Basic]

---

Q: What function triggers the OOM killer?
A: `out_of_memory(zonelist, gfp_mask, order, nodemask)` is called from the allocation slow path when all reclaim attempts fail. It may kill a process or panic.
[Intermediate]

---

Q: How does `select_bad_process()` choose an OOM victim?
A: It scores processes with `oom_badness()`:
1. Higher score = more likely to kill
2. Based on memory usage (RSS + swap)
3. Adjusted by `oom_score_adj` (-1000 to 1000)
4. Root processes get slight discount
5. Processes with `oom_score_adj = -1000` are immune
[Intermediate]

---

Q: What does `oom_badness()` calculate?
A: Points = (RSS + PageTables + Swap usage) * penalty
Adjusted by oom_score_adj: 0 = neutral, negative = protect, -1000 = never kill
Higher points = worse (more likely to be killed).
[Intermediate]

---

Q: What is `oom_score_adj`?
A: `oom_score_adj` (range -1000 to 1000) adjusts OOM badness score. Set via `/proc/<pid>/oom_score_adj`. -1000 makes process immune; 1000 makes it preferred victim.
[Basic]

---

Q: What does `oom_kill_process()` do?
A: 1. Send SIGKILL to selected process and all its threads
2. Mark victim's mm with MMF_OOM_REAPED
3. Log the OOM event
4. May also kill children sharing mm
[Intermediate]

---

Q: What is the `panic_on_oom` sysctl?
A: `panic_on_oom` controls behavior on OOM:
0: Kill a process (default)
1: Panic if OOM in all nodes
2: Panic on any OOM
Used when OOM killing is unacceptable (e.g., critical servers).
[Intermediate]

---

Q: What is memory cgroup OOM handling?
A: Memory cgroups can isolate OOM situations. If a cgroup exceeds its limit, only processes in that cgroup are OOM killed, not system-wide. Controlled by `memory.oom_control`.
[Advanced]

---

Q: What is `zone->all_unreclaimable`?
A: Set when a zone has been fully scanned with no reclaimable pages found. Tells allocator/reclaimer this zone is exhausted. Cleared when pages become free.
[Advanced]

---

Q: What is the inactive ratio?
A: `zone->inactive_ratio` controls balance between active/inactive anonymous pages. Adjusted dynamically. Higher ratio means more pages on active list (better caching but less reclaim candidates).
[Advanced]

---

Q: What is the difference between reclaim and compaction?
A: Reclaim frees pages (by dropping cache/swapping).
Compaction moves pages to create contiguous free blocks.
Both fight fragmentation but work differently: reclaim reduces memory use; compaction rearranges without freeing.
[Intermediate]

---

Q: When does compaction happen?
A: Compaction runs:
1. When high-order allocation fails
2. Triggered by kcompactd (background)
3. Direct compaction when direct reclaim insufficient
4. During memory hotplug
[Intermediate]

---

Q: What is page migration?
A: Page migration moves a page from one physical frame to another: allocate new page, copy contents, update all PTEs, free old page. Used by compaction, NUMA balancing, memory hotplug.
[Intermediate]

---

Q: What is the swappiness parameter?
A: `vm.swappiness` (0-100) controls preference for reclaiming anonymous vs file pages. 0 = prefer file pages, 100 = equal treatment. Default is 60.
[Basic]

---

Q: Why might you set swappiness to 0?
A: Setting swappiness=0 minimizes swapping, keeping application memory in RAM. Good for databases with their own caching. Trades more page cache pressure for less swap I/O.
[Intermediate]

---

Q: What is `scan_control->nr_to_reclaim`?
A: Target number of pages to reclaim in this scan. Set by caller based on allocation order. Reclaim continues until this target is reached or priority hits 0.
[Intermediate]

---

Q: What is `scan_control->may_unmap`?
A: Boolean indicating if reclaim is allowed to unmap pages from processes. If false, only unmapped pages can be reclaimed. Direct reclaim may set this false initially.
[Advanced]

---

Q: What does ZONE_RECLAIM_LOCKED flag prevent?
A: ZONE_RECLAIM_LOCKED prevents concurrent zone reclaim operations. Only one reclaimer can scan a zone at a time. Others skip the zone or wait.
[Advanced]

---

## Section 9: Additional Key Concepts

---

Q: What is the `address_space` structure?
A: `address_space` represents all cached pages for a file or device. Contains: page tree (radix tree of cached pages), host inode pointer, and operations for reading/writing pages.
[Intermediate]

---

Q: What is the page cache?
A: The page cache stores recently accessed file data in memory. Reads first check the cache; writes go through the cache. Managed per-file via `address_space`. Reduces disk I/O.
[Basic]

---

Q: What is `find_get_page()`?
A: `find_get_page(mapping, index)` looks up a page in an address_space's page cache by offset. Returns the page with refcount incremented, or NULL if not cached.
[Intermediate]

---

Q: What is the difference between clean and dirty pages?
A: Clean page: Contents match disk; can be simply discarded
Dirty page: Modified since last disk write; must be written before reclaim
Dirty pages require I/O to reclaim.
[Basic]

---

Q: What sets a page dirty?
A: `SetPageDirty(page)` or the architecture's dirty bit in PTE. Modified memory-mapped pages become dirty automatically when written; explicit marking for kernel modifications.
[Basic]

---

Q: What is writeback?
A: Writeback is the process of writing dirty pages to disk. Can be triggered by: pdflush/flusher threads, direct reclaim, sync operations, or memory pressure. Converts dirty pages to clean.
[Basic]

---

Q: What is `balance_dirty_pages()`?
A: Called when a process dirties pages. If dirty page ratio exceeds threshold, process is throttled (sleeps) to let writeback catch up. Prevents memory filling with dirty data.
[Intermediate]

---

Q: What is memory pressure?
A: Memory pressure indicates system-wide memory scarcity. Measured by reclaim activity, watermark breaches, allocation failures. High pressure triggers more aggressive reclaim and may wake OOM killer.
[Basic]

---

Q: What is `min_free_kbytes`?
A: `min_free_kbytes` sysctl sets the minimum number of kilobytes the kernel tries to keep free. Affects zone watermarks. Higher values provide more headroom but reduce usable memory.
[Intermediate]

---

Q: What does `overcommit_memory` control?
A: vm.overcommit_memory sysctl:
0: Heuristic overcommit (default)
1: Always allow (never fail malloc)
2: Strict accounting (commit limit = swap + RAM*ratio)
Controls virtual memory promise policy.
[Intermediate]

---

Q: What is memory overcommit?
A: Overcommit allows allocating more virtual memory than physically available. Works because most programs don't use all allocated memory simultaneously. Can lead to OOM if applications actually use everything.
[Basic]

---

Q: What is a kernel memory leak?
A: A kernel memory leak occurs when memory is allocated but never freed. Over time, this exhausts available memory. Detected via kmemleak tool or observing slab growth.
[Basic]

---

Q: What is kmemleak?
A: Kmemleak is a kernel memory leak detector. It tracks allocations and scans memory for pointers. Memory not referenced from anywhere is reported as a potential leak.
[Intermediate]

---

Q: What is KSM (Kernel Same-page Merging)?
A: KSM scans memory for pages with identical content and merges them copy-on-write. Commonly used with VMs to share common pages (like zero pages). Trades CPU for memory savings.
[Intermediate]

---

Q: What is transparent huge pages (THP)?
A: THP automatically uses 2MB pages instead of 4KB for anonymous memory when possible. Reduces TLB pressure and page table overhead. Managed by khugepaged daemon.
[Intermediate]

---

Q: What is a huge page?
A: Huge pages are larger than standard pages (2MB or 1GB vs 4KB on x86). They reduce TLB misses and page table overhead for large memory regions. Require contiguous physical memory.
[Basic]

---

Q: What is `mlock()` system call?
A: `mlock(addr, len)` locks pages in memory, preventing them from being swapped out or reclaimed. Used for real-time applications, cryptographic keys, or DMA buffers.
[Basic]

---

Q: What is `VM_LOCKED` flag?
A: VM_LOCKED indicates a VMA's pages are mlocked and must stay in memory. Set by `mlock()`. Prevents reclaim of these pages. Requires privileges to lock large amounts.
[Intermediate]

---

Q: What is `madvise()` system call?
A: `madvise(addr, len, advice)` gives hints about memory usage patterns:
MADV_WILLNEED: Read-ahead pages
MADV_DONTNEED: Drop pages (like free)
MADV_SEQUENTIAL/RANDOM: Access pattern hints
[Basic]

---

Q: What does `madvise(MADV_DONTNEED)` do?
A: MADV_DONTNEED tells the kernel to drop the specified pages. They're removed from page tables and cache. On next access, pages are faulted in fresh (zero or re-read).
[Intermediate]

---

Q: What is the mempool subsystem?
A: `mempool` provides guaranteed memory allocation from a reserved pool. Used when allocation failure isn't acceptable. Pool is pre-allocated; `mempool_alloc()` draws from pool if normal allocation fails.
[Intermediate]

---

Q: What is a memory mapping type?
A: Types of mappings:
- Private anonymous: brk, stack (fork creates COW copies)
- Shared anonymous: shmem, POSIX shm
- Private file: Normal mmap (COW for writes)
- Shared file: File-backed shared memory
[Basic]