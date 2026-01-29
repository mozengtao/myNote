# Linux Kernel v3.2 Memory Management Flashcards

> Comprehensive Anki-style flashcards covering physical memory, virtual memory, allocators, page tables, reclaim, and kernel APIs.

---

## Section 1: Physical Memory Fundamentals

---

Q: What is a page frame in Linux kernel memory management?
A: A page frame is the smallest unit of physical memory managed by the kernel. On most architectures, it is 4KB (4096 bytes). The kernel tracks each page frame using a `struct page` descriptor.
[Basic]

---

Q: What is the primary data structure used to represent a physical page frame in the Linux kernel?
A: `struct page` - defined in `include/linux/mm_types.h`. Each physical page frame in the system has an associated `struct page` descriptor that tracks its state, usage, and metadata.
[Basic]

---

Q: (Cloze) The size of a page frame is defined by the constant _____, which equals _____ bytes on x86.
A: `PAGE_SIZE`, 4096 (or 4KB). This is defined via `PAGE_SHIFT` (typically 12, meaning 2^12 = 4096).
[Basic]

---

Q: What is a PFN (Page Frame Number)?
A: The PFN is the index of a physical page frame in memory. It equals the physical address divided by `PAGE_SIZE`. PFN 0 corresponds to physical address 0, PFN 1 to address 4096 (assuming 4KB pages), etc.
[Basic]

---

Q: How do you convert between `struct page` and PFN?
A: Use `page_to_pfn(page)` to get the PFN from a page descriptor, and `pfn_to_page(pfn)` to get the page descriptor from a PFN. These macros are defined based on the memory model.
[Basic]

---

Q: What does `page_address()` return?
A: `page_address(page)` returns the kernel virtual address corresponding to the given `struct page`. For low memory pages, this is a direct mapping. For highmem pages, it may return NULL if the page isn't currently mapped.
[Intermediate]

---

Q: (Code Interpretation) What does this code accomplish?
```c
struct page *page = alloc_page(GFP_KERNEL);
void *addr = page_address(page);
unsigned long pfn = page_to_pfn(page);
```
A: 1. Allocates a single physical page using the buddy allocator
2. Gets the kernel virtual address of that page
3. Gets the Page Frame Number (physical page index)
These are the three fundamental representations of the same physical memory.
[Intermediate]

---

Q: What are the key fields in `struct page` (v3.2)?
A: Key fields include:
- `flags` - page state flags (locked, dirty, referenced, etc.)
- `_count` - reference count (atomic)
- `_mapcount` - number of page table mappings
- `mapping` - address_space or anon_vma pointer
- `index` - offset within mapping
- `lru` - LRU list linkage for page reclaim
- `private` - used by various subsystems
[Intermediate]

---

Q: What is the difference between `_count` and `_mapcount` in `struct page`?
A: `_count` is the total reference count - how many users hold a reference to this page (kernel, page cache, etc.). `_mapcount` specifically counts how many page table entries (PTEs) map this page. A page can have `_count > 0` but `_mapcount = -1` (unmapped but referenced).
[Intermediate]

---

Q: (Reverse) This field in struct page counts the number of page table entries mapping the page.
A: Q: What is `_mapcount` in struct page?
[Intermediate]

---

Q: What does a `_mapcount` value of -1 mean?
A: The page is not mapped in any process page table. The actual stored value is -1, but the `page_mapcount()` function returns 0 for this case. When the first PTE maps the page, `_mapcount` becomes 0 (meaning 1 mapping).
[Intermediate]

---

Q: (ASCII Diagram) Draw the relationship between physical memory, struct page array, and virtual addresses.
A:
```
Physical Memory          struct page array       Virtual Address
+----------------+       +----------------+      +----------------+
|  Page Frame 0  | <---> | page[0]        | ---> | 0xC0000000     |
+----------------+       +----------------+      +----------------+
|  Page Frame 1  | <---> | page[1]        | ---> | 0xC0001000     |
+----------------+       +----------------+      +----------------+
|  Page Frame 2  | <---> | page[2]        | ---> | 0xC0002000     |
+----------------+       +----------------+      +----------------+
        |                        |                      |
   pfn_to_page()           page_to_pfn()         page_address()
```
[Basic]

---

Q: What are page flags and where are they stored?
A: Page flags are status bits stored in the `flags` field of `struct page`. They indicate the page's state: `PG_locked`, `PG_referenced`, `PG_dirty`, `PG_lru`, `PG_active`, `PG_slab`, `PG_reserved`, etc. Defined in `include/linux/page-flags.h`.
[Basic]

---

Q: What does the `PG_locked` flag indicate?
A: The page is locked for I/O or other exclusive operations. Other processes trying to lock the page will sleep until it's unlocked. Used during page read/write operations from disk.
[Intermediate]

---

Q: What does the `PG_dirty` flag indicate?
A: The page contains modified data that hasn't been written back to its backing store (disk). The writeback mechanism uses this flag to know which pages need to be flushed.
[Basic]

---

Q: What does the `PG_referenced` flag indicate?
A: The page has been accessed recently. Used by the page reclaim algorithm (second-chance/clock algorithm) to determine which pages are actively used and should not be evicted.
[Intermediate]

---

Q: What does the `PG_active` flag indicate?
A: The page is on the active LRU list (recently accessed). Pages without this flag are on the inactive list and are candidates for reclaim. Pages are promoted/demoted between lists based on access patterns.
[Intermediate]

---

Q: (Understanding) Why does the kernel use flags instead of separate boolean fields in struct page?
A: Space efficiency. `struct page` exists for every physical page frame, so even saving a few bytes per page saves megabytes of memory on large systems. Bit flags pack many booleans into a single `unsigned long`, and atomic bit operations allow lock-free updates.
[Intermediate]

---

Q: How do you check and manipulate page flags?
A: Use helper functions like:
- `PageLocked(page)`, `PageDirty(page)` - test flags
- `SetPageDirty(page)`, `ClearPageDirty(page)` - set/clear flags
- `TestSetPageLocked(page)` - atomic test-and-set
These are generated macros defined in `page-flags.h`.
[Intermediate]

---

Q: What is a memory zone in Linux?
A: A memory zone is a contiguous range of physical memory with specific characteristics. The kernel divides physical memory into zones based on hardware constraints (DMA limitations, addressing capabilities). Each zone is managed independently by the buddy allocator.
[Basic]

---

Q: List the memory zones in Linux kernel v3.2 and their purposes.
A:
- `ZONE_DMA` - Memory for legacy ISA DMA (first 16MB on x86)
- `ZONE_DMA32` - Memory addressable by 32-bit DMA devices (first 4GB)
- `ZONE_NORMAL` - Directly mapped memory (up to 896MB on 32-bit)
- `ZONE_HIGHMEM` - Memory above direct mapping (32-bit only)
- `ZONE_MOVABLE` - Movable pages for memory hotplug/compaction
[Basic]

---

Q: (Cloze) On 32-bit x86, ZONE_DMA covers addresses from _____ to _____, while ZONE_NORMAL covers _____ to _____.
A: ZONE_DMA: 0 to 16MB; ZONE_NORMAL: 16MB to 896MB. Memory above 896MB goes to ZONE_HIGHMEM because the kernel direct mapping is limited to ~896MB on 32-bit systems.
[Intermediate]

---

Q: Why does ZONE_HIGHMEM exist only on 32-bit systems?
A: On 32-bit systems, the kernel virtual address space is limited (typically 1GB out of 4GB total). Only ~896MB can be directly mapped. Physical memory above this must be temporarily mapped when accessed (highmem). On 64-bit systems, the huge virtual address space allows direct mapping of all physical memory.
[Intermediate]

---

Q: (ASCII Diagram) Show the memory zone layout on a 32-bit x86 system with 2GB RAM.
A:
```
Physical Memory Layout (32-bit x86, 2GB RAM)
+------------------+ 0x00000000
|    ZONE_DMA      |  0-16MB (ISA DMA)
+------------------+ 0x01000000
|   ZONE_NORMAL    |  16MB-896MB (direct mapped)
+------------------+ 0x38000000
|   ZONE_HIGHMEM   |  896MB-2GB (requires kmap)
+------------------+ 0x80000000

Kernel Virtual Address Space:
+------------------+ 0xC0000000
| Direct Mapping   |  Maps ZONE_DMA + ZONE_NORMAL
| (896MB max)      |
+------------------+ 0xF8000000
| vmalloc, kmap,   |  For HIGHMEM pages
| fixmap, etc.     |
+------------------+ 0xFFFFFFFF
```
[Intermediate]

---

Q: What is `ZONE_MOVABLE` and why was it introduced?
A: `ZONE_MOVABLE` contains only pages that can be migrated (moved to different physical locations). It supports memory hotplug (removing physical memory) and memory compaction (defragmentation). Pages allocated from this zone can be relocated, unlike kernel pages.
[Intermediate]

---

Q: What data structure represents a memory zone?
A: `struct zone` defined in `include/linux/mmzone.h`. It contains the zone's free page lists (buddy allocator), watermarks, LRU lists, statistics, and a lock for synchronization.
[Basic]

---

Q: What are zone watermarks?
A: Watermarks are thresholds that control memory reclaim behavior:
- `WMARK_MIN` - Below this, only critical allocations succeed; triggers direct reclaim
- `WMARK_LOW` - Wakes kswapd to start background reclaim
- `WMARK_HIGH` - kswapd stops; zone has enough free pages
[Intermediate]

---

Q: (Understanding) What happens when free pages in a zone drop below WMARK_LOW?
A: The kswapd daemon is awakened to perform background page reclaim. It will reclaim pages until free memory reaches WMARK_HIGH. This prevents the system from running out of memory during allocation bursts.
[Intermediate]

---

Q: (Understanding) What happens when free pages drop below WMARK_MIN?
A: Direct reclaim is triggered - the allocating process must itself reclaim pages before the allocation can succeed. Only `GFP_ATOMIC` and `__GFP_HIGH` allocations may dip into emergency reserves below WMARK_MIN.
[Intermediate]

---

Q: (Code Interpretation) What do these zone watermark checks indicate?
```c
if (zone_page_state(zone, NR_FREE_PAGES) < zone->watermark[WMARK_LOW])
    wakeup_kswapd(zone);
if (zone_page_state(zone, NR_FREE_PAGES) < zone->watermark[WMARK_MIN])
    return NULL; /* allocation fails, need direct reclaim */
```
A: First check: If free pages fall below low watermark, wake the kswapd background reclaimer. Second check: If free pages are below minimum watermark, normal allocation fails - caller must perform direct reclaim or fail.
[Advanced]

---

Q: How are zone watermarks calculated?
A: Watermarks are calculated during boot based on zone size and the `min_free_kbytes` sysctl. `WMARK_MIN` = min_free_kbytes for the zone. `WMARK_LOW` = WMARK_MIN + WMARK_MIN/4. `WMARK_HIGH` = WMARK_MIN + WMARK_MIN/2.
[Advanced]

---

Q: (Reverse) This kernel parameter controls the minimum amount of free memory the kernel tries to maintain.
A: Q: What is `min_free_kbytes`?
[Intermediate]

---

Q: What is `mem_map` in the Linux kernel?
A: `mem_map` is the global array of `struct page` descriptors, one for each physical page frame. It allows O(1) translation between PFN and page descriptor: `mem_map[pfn]` gives the struct page for that PFN.
[Intermediate]

---

Q: (Understanding) Why might `pfn_to_page()` be implemented differently on NUMA vs UMA systems?
A: On UMA (single node) systems, there's one contiguous `mem_map` array and `pfn_to_page(pfn)` is simply `mem_map + pfn`. On NUMA, each node has its own `node_mem_map` array, so `pfn_to_page()` must first determine which node owns the PFN, then index into that node's array.
[Advanced]

---

Q: What is the `vmemmap` memory model?
A: A sparse memory model where `struct page` descriptors are allocated on demand and mapped into a contiguous virtual address region (`vmemmap`). It's efficient for systems with large memory holes because it doesn't waste memory on descriptors for non-existent pages.
[Advanced]

---

Q: What is `PAGE_OFFSET` and what does it represent?
A: `PAGE_OFFSET` is the kernel virtual address where physical memory starts being direct-mapped. On 32-bit x86, it's typically 0xC0000000 (3GB). Physical address 0 maps to virtual address `PAGE_OFFSET`, physical address 4096 maps to `PAGE_OFFSET + 4096`, etc.
[Intermediate]

---

Q: How do you convert between physical and kernel virtual addresses?
A: For directly mapped memory:
- `__pa(vaddr)` - virtual to physical: `vaddr - PAGE_OFFSET`
- `__va(paddr)` - physical to virtual: `paddr + PAGE_OFFSET`
- `virt_to_phys()` / `phys_to_virt()` - wrapper functions
These only work for the direct-mapped region, not vmalloc or highmem.
[Intermediate]

---

Q: (Code Interpretation) What's wrong with this code?
```c
void *ptr = vmalloc(PAGE_SIZE);
unsigned long phys = __pa(ptr);  // Get physical address
```
A: This is incorrect! `__pa()` only works for directly-mapped kernel addresses. vmalloc addresses are not directly mapped - they use separate page tables. To get the physical address of vmalloc memory, you need: `page = vmalloc_to_page(ptr); phys = page_to_phys(page);`
[Advanced]

---

Q: What is `struct page`'s `mapping` field used for?
A: The `mapping` field serves dual purposes:
- For page cache pages: Points to the `address_space` structure of the file
- For anonymous pages: Points to `anon_vma` (or has `PAGE_MAPPING_ANON` bit set)
The lowest bit distinguishes between these cases.
[Intermediate]

---

Q: What is `struct page`'s `index` field?
A: For page cache pages, `index` is the page's offset within the file (in pages). For anonymous pages, it's the virtual page number within the VMA. This allows reverse mapping - finding all PTEs that reference a page.
[Intermediate]

---

Q: (Understanding) Why is `struct page` size critical to system memory usage?
A: Every physical page frame needs a `struct page` descriptor. On a 64GB system with 4KB pages, there are 16 million pages. If `struct page` is 64 bytes, that's 1GB just for page descriptors. Keeping `struct page` small is essential for large memory systems.
[Advanced]

---

## Section 2: NUMA and Node Management

---

Q: What is NUMA (Non-Uniform Memory Access)?
A: NUMA is a memory architecture where memory access time depends on the memory location relative to the processor. Each CPU has "local" memory (fast access) and "remote" memory (slower access through interconnect). The kernel must be NUMA-aware for optimal performance.
[Basic]

---

Q: What is a NUMA node?
A: A NUMA node is a collection of CPUs and their local memory. Each node has its own memory controller and local memory. Access to memory on the same node is faster than accessing memory on remote nodes. The kernel tracks nodes separately for allocation decisions.
[Basic]

---

Q: What data structure represents a NUMA node in Linux?
A: `struct pglist_data` (typedef'd as `pg_data_t`) defined in `include/linux/mmzone.h`. Each node has its own `pg_data_t` containing the node's zones, memory map, and statistics. On UMA systems, there's only one node (`contig_page_data`).
[Basic]

---

Q: (Cloze) The array of all NUMA nodes is accessed via _____, and a specific node is retrieved using _____.
A: `node_data[]` array, `NODE_DATA(nid)` macro. Example: `NODE_DATA(0)` returns the `pg_data_t` for node 0.
[Intermediate]

---

Q: What are the key fields in `struct pglist_data`?
A: Key fields include:
- `node_zones[]` - array of zones for this node
- `node_zonelists[]` - fallback zone ordering for allocations
- `nr_zones` - number of populated zones
- `node_mem_map` - array of struct page for this node
- `node_start_pfn` - starting PFN for this node
- `node_spanned_pages` - total pages including holes
- `node_present_pages` - actual usable pages
[Intermediate]

---

Q: (ASCII Diagram) Draw a NUMA system with 2 nodes.
A:
```
NUMA System - 2 Nodes
+------------------+     Interconnect      +------------------+
|     NODE 0       |<=====================>|     NODE 1       |
+------------------+                       +------------------+
| CPU 0  | CPU 1   |                       | CPU 2  | CPU 3   |
+--------+---------+                       +--------+---------+
| Memory Controller|                       | Memory Controller|
+------------------+                       +------------------+
| Local Memory     |                       | Local Memory     |
| (Fast Access)    |                       | (Fast Access)    |
| ZONE_DMA         |                       | ZONE_DMA32       |
| ZONE_DMA32       |                       | ZONE_NORMAL      |
| ZONE_NORMAL      |                       +------------------+
+------------------+

CPU 0 accessing Node 0 memory: ~100ns (local)
CPU 0 accessing Node 1 memory: ~300ns (remote)
```
[Intermediate]

---

Q: What is a zonelist?
A: A zonelist is an ordered list of zones to try when allocating memory. It defines the fallback order - which zones to try if the preferred zone is exhausted. Each node has zonelists that start with local zones, then fall back to remote nodes.
[Intermediate]

---

Q: What is `node_zonelists` and how is it organized?
A: `node_zonelists` in `pg_data_t` is an array of `struct zonelist`. Each zonelist contains zones ordered by preference. The first entries are local zones (same node), followed by zones from other nodes. This ensures local allocation preference while allowing fallback.
[Intermediate]

---

Q: (Understanding) Why does each NUMA node have its own zonelist instead of a global one?
A: To implement NUMA-aware allocation. Each node's zonelist prioritizes its local zones first, then falls back to other nodes. This ensures processes preferentially use local memory for better performance while still succeeding if local memory is exhausted.
[Intermediate]

---

Q: What is the zone fallback order in a zonelist?
A: Typically: ZONE_NORMAL → ZONE_DMA32 → ZONE_DMA (high to low). Within this order, local node zones come first, then remote nodes. For GFP_KERNEL on node 0: Node0_NORMAL → Node0_DMA32 → Node0_DMA → Node1_NORMAL → Node1_DMA32 → ...
[Intermediate]

---

Q: What is `node_mem_map`?
A: `node_mem_map` is the per-node array of `struct page` descriptors. It's the node's portion of the memory map. `node_mem_map[0]` corresponds to the first page of this node (at PFN = `node_start_pfn`).
[Intermediate]

---

Q: What is `node_start_pfn`?
A: The Page Frame Number where this node's physical memory begins. For example, if node 1 starts at physical address 4GB (on a system with 4KB pages), its `node_start_pfn` would be 1048576 (4GB / 4KB).
[Intermediate]

---

Q: What is the difference between `node_spanned_pages` and `node_present_pages`?
A: `node_spanned_pages` is the total range of pages from first to last PFN (may include holes). `node_present_pages` is the actual number of usable physical pages. Holes occur due to reserved regions, firmware, or memory-mapped I/O.
[Intermediate]

---

Q: What is a NUMA memory policy?
A: A policy that controls which NUMA nodes are used for memory allocation. Policies can be set per-process or per-VMA. They determine where pages are allocated to optimize for locality or bandwidth.
[Intermediate]

---

Q: List the NUMA memory policies available in Linux v3.2.
A:
- `MPOL_DEFAULT` - Use the process's default policy (usually local allocation)
- `MPOL_BIND` - Allocate only from specified nodes (fail if exhausted)
- `MPOL_INTERLEAVE` - Round-robin across specified nodes
- `MPOL_PREFERRED` - Prefer specified node, fallback to others
[Intermediate]

---

Q: (Understanding) When would you use `MPOL_INTERLEAVE`?
A: For data structures accessed equally by multiple CPUs/nodes, like shared hash tables or global data. Interleaving spreads the data across nodes, balancing memory bandwidth and avoiding hotspots. Used by kernel during boot for key data structures.
[Intermediate]

---

Q: (Understanding) When would you use `MPOL_BIND`?
A: When you need strict memory locality and would rather fail than use remote memory. Useful for latency-sensitive applications or when memory bandwidth is critical. The allocation fails if the specified nodes are exhausted.
[Intermediate]

---

Q: How do you set NUMA memory policy from userspace?
A: Using system calls:
- `set_mempolicy()` - Set policy for current process
- `mbind()` - Set policy for specific memory region
- `get_mempolicy()` - Query current policy
Or via numactl command: `numactl --interleave=all ./program`
[Intermediate]

---

Q: (Code Interpretation) What does this code do?
```c
struct mempolicy *pol = current->mempolicy;
if (pol && pol->mode == MPOL_BIND)
    nodes = pol->v.nodes;
else
    nodes = node_online_map;
```
A: Checks the current process's NUMA policy. If it's MPOL_BIND (strict binding), allocations must come from the specified nodes. Otherwise, all online nodes are potential allocation targets.
[Advanced]

---

Q: What is `numa_node_id()` and when is it used?
A: `numa_node_id()` returns the NUMA node ID of the current CPU. Used to determine the "local" node for allocations. It's called in the allocation fast path to prefer local memory.
[Intermediate]

---

Q: (Reverse) This function returns the struct page array for a given NUMA node.
A: Q: What does `NODE_DATA(nid)->node_mem_map` return?
[Intermediate]

---

Q: How does the kernel handle NUMA on UMA (single-node) systems?
A: UMA systems are treated as NUMA with a single node (node 0). A global `contig_page_data` serves as the only `pg_data_t`. All NUMA code paths work correctly with this degenerate case, just with one node.
[Intermediate]

---

Q: What is `for_each_online_node()` and how is it used?
A: A macro to iterate over all online NUMA nodes:
```c
int nid;
for_each_online_node(nid) {
    pg_data_t *pgdat = NODE_DATA(nid);
    // process each node
}
```
Useful for initializing or scanning all nodes.
[Intermediate]

---

Q: (Understanding) Why does NUMA-awareness matter for kernel memory allocators?
A: Poor NUMA allocation causes remote memory accesses, which can be 2-3x slower than local accesses. For memory-intensive workloads, this significantly impacts performance. NUMA-aware allocation keeps data near the CPUs that use it.
[Advanced]

---

Q: What is `numa_mem_id()` vs `numa_node_id()`?
A: `numa_node_id()` returns the CPU's node. `numa_mem_id()` returns the node to use for memory allocations (may differ if node has no memory). Usually the same, but differs on memoryless nodes.
[Advanced]

---

## Section 3: Buddy Allocator

---

Q: What is the buddy allocator?
A: The primary physical page allocator in Linux. It manages free pages using a buddy system algorithm that efficiently handles allocation and freeing of contiguous page blocks while minimizing external fragmentation.
[Basic]

---

Q: How does the buddy algorithm work?
A: Memory is divided into blocks of 2^n pages (order-n blocks). When allocating, find the smallest block that fits, splitting larger blocks if needed. When freeing, merge adjacent "buddy" blocks of equal size into larger blocks recursively.
[Basic]

---

Q: What is the "order" of an allocation?
A: The order specifies the size as a power of 2. Order-0 = 1 page (4KB), order-1 = 2 pages (8KB), order-2 = 4 pages (16KB), etc. Maximum order is `MAX_ORDER - 1` (typically 10, meaning 1024 pages = 4MB).
[Basic]

---

Q: (Cloze) The maximum allocation order is _____ - 1, which equals _____ contiguous pages on most systems.
A: `MAX_ORDER` - 1, which equals 1024 pages (4MB with 4KB pages). `MAX_ORDER` is typically 11.
[Basic]

---

Q: What data structure stores free pages in the buddy allocator?
A: `struct free_area` array in each zone: `zone->free_area[MAX_ORDER]`. Each element has a linked list of free blocks of that order and a count of free blocks.
[Intermediate]

---

Q: (ASCII Diagram) Show the buddy allocator free lists structure.
A:
```
zone->free_area[MAX_ORDER]
+--------+    +--------+    +--------+
| free_  |    | free_  |    | free_  |
| area[0]|    | area[1]|    | area[2]| ...  [MAX_ORDER-1]
+--------+    +--------+    +--------+
| nr_free|    | nr_free|    | nr_free|
| =5     |    | =3     |    | =2     |
+---+----+    +---+----+    +---+----+
    |             |             |
    v             v             v
 +------+      +------+      +------+
 |1 page|      |2 page|      |4 page|
 | blk  |      | blk  |      | blk  |
 +--+---+      +--+---+      +--+---+
    |             |             |
    v             v             v
 +------+      +------+      +------+
 |1 page|      |2 page|      |4 page|
 | blk  |      | blk  |      | blk  |
 +------+      +------+      +------+
    ...           ...           ...
```
[Intermediate]

---

Q: What is a "buddy" in the buddy allocator?
A: Two adjacent blocks of equal size (same order) that can be merged into a single block of the next order. They must be aligned - a block at PFN P has its buddy at PFN P XOR (1 << order). Buddies are merged when both are free.
[Intermediate]

---

Q: (Code Interpretation) What does this code calculate?
```c
buddy_pfn = page_pfn ^ (1 << order);
```
A: The PFN of the buddy block. XORing with the block size toggles the bit that distinguishes buddies. Example: for order-2 (4 pages), block at PFN 0 has buddy at PFN 4, block at PFN 4 has buddy at PFN 0.
[Intermediate]

---

Q: What are migration types and why are they used?
A: Migration types classify pages by their ability to be moved:
- `MIGRATE_UNMOVABLE` - Kernel pages that cannot move
- `MIGRATE_MOVABLE` - User pages that can be migrated
- `MIGRATE_RECLAIMABLE` - Page cache that can be reclaimed
This reduces fragmentation by grouping similar pages together.
[Intermediate]

---

Q: List all migration types in kernel v3.2.
A:
- `MIGRATE_UNMOVABLE` - Kernel allocations, cannot be moved
- `MIGRATE_RECLAIMABLE` - Page cache, buffers, reclaimable slabs
- `MIGRATE_MOVABLE` - User pages, can be migrated
- `MIGRATE_PCPTYPES` - Number of types for per-cpu lists
- `MIGRATE_RESERVE` - Emergency reserve
- `MIGRATE_ISOLATE` - For memory hotplug isolation
[Intermediate]

---

Q: (Understanding) How do migration types reduce fragmentation?
A: By grouping pages with similar mobility. If unmovable kernel pages were scattered among movable pages, large contiguous regions could never be created. Grouping unmovable pages together keeps other regions free for large allocations.
[Intermediate]

---

Q: What is `__alloc_pages_nodemask()`?
A: The core page allocation function. All page allocations eventually call this. It takes GFP flags, order, zonelist, and optional nodemask. Returns a struct page pointer or NULL on failure.
[Intermediate]

---

Q: Describe the main allocation path in the buddy allocator.
A:
1. `alloc_pages()` → `__alloc_pages_nodemask()`
2. `get_page_from_freelist()` - Try each zone in zonelist
3. `buffered_rmqueue()` - Get from per-CPU list or zone free lists
4. `__rmqueue()` - Search free_area[] for suitable block
5. `expand()` - Split larger block if needed
[Advanced]

---

Q: What is `get_page_from_freelist()`?
A: Iterates through zones in the zonelist trying to allocate. Checks watermarks, applies zone restrictions (DMA, highmem), and calls `buffered_rmqueue()` for the actual allocation. Returns first successful allocation.
[Intermediate]

---

Q: What is the "slow path" in page allocation?
A: When `get_page_from_freelist()` fails, the allocator enters the slow path which tries:
1. Wake kswapd for background reclaim
2. Direct reclaim (synchronous page freeing)
3. Memory compaction
4. OOM killer (last resort)
[Intermediate]

---

Q: What does `__rmqueue()` do?
A: Removes a block from the buddy free lists. It searches `free_area[]` starting at the requested order, going up until it finds a free block. If a larger block is found, it calls `expand()` to split it down to the needed size.
[Intermediate]

---

Q: (Code Interpretation) Explain this allocation loop:
```c
for (current_order = order; current_order < MAX_ORDER; ++current_order) {
    area = &(zone->free_area[current_order]);
    if (list_empty(&area->free_list[migratetype]))
        continue;
    page = list_entry(area->free_list[migratetype].next, struct page, lru);
    list_del(&page->lru);
    area->nr_free--;
    expand(zone, page, order, current_order, area, migratetype);
    return page;
}
```
A: Search for free blocks starting at requested order, going up. When found, remove block from free list, decrement count, and if larger than needed, call `expand()` to split it. Return the page. This is the core of `__rmqueue()`.
[Advanced]

---

Q: What does `expand()` do?
A: Splits a larger block into smaller buddies. If we got an order-4 block but need order-1, `expand()` splits off the upper halves and adds them to their respective free lists: puts one order-3, one order-2, one order-1 block back, keeps the order-1 we need.
[Intermediate]

---

Q: (ASCII Diagram) Show how expand() splits an order-3 block for order-0 allocation.
A:
```
Order-3 block (8 pages): [0|1|2|3|4|5|6|7]

Split order-3 → order-2:
  Keep: [0|1|2|3]  Return to free_area[2]: [4|5|6|7]

Split order-2 → order-1:
  Keep: [0|1]      Return to free_area[1]: [2|3]

Split order-1 → order-0:
  Keep: [0]        Return to free_area[0]: [1]

Result: Allocate page 0, pages 1,2-3,4-7 added to free lists
```
[Intermediate]

---

Q: What is `__free_one_page()`?
A: Frees a page block and merges with its buddy if possible. It finds the buddy, checks if it's free and of the same order, merges them into a larger block, and repeats up to MAX_ORDER. This is the buddy coalescing algorithm.
[Intermediate]

---

Q: (Code Interpretation) Explain this buddy merging loop:
```c
while (order < MAX_ORDER-1) {
    buddy = __page_find_buddy(page, page_pfn, order);
    if (!page_is_buddy(page, buddy, order))
        break;
    list_del(&buddy->lru);
    zone->free_area[order].nr_free--;
    combined_pfn = page_pfn & ~(1 << order);
    page = page + (combined_pfn - page_pfn);
    page_pfn = combined_pfn;
    order++;
}
```
A: Find buddy at current order. If buddy is also free (same order, not allocated), remove it from free list, combine into larger block. Calculate combined block's PFN (clear the bit that distinguished them), increment order, repeat. Continue until buddy isn't free or max order reached.
[Advanced]

---

Q: What GFP flags are and why are they important?
A: GFP (Get Free Pages) flags specify allocation constraints:
- Where to allocate (zone restrictions)
- Whether we can sleep/reclaim
- How hard to try
They guide the allocator's behavior for different contexts (interrupt, atomic, normal).
[Basic]

---

Q: List common GFP flags and their meanings.
A:
- `GFP_KERNEL` - Normal kernel allocation, can sleep
- `GFP_ATOMIC` - Cannot sleep, use reserves
- `GFP_USER` - User-space allocation
- `GFP_HIGHUSER` - User allocation, prefer highmem
- `GFP_DMA` - Require ZONE_DMA
- `GFP_NOWAIT` - Don't sleep or reclaim
- `__GFP_ZERO` - Zero the allocated pages
[Basic]

---

Q: What is the difference between `GFP_KERNEL` and `GFP_ATOMIC`?
A: `GFP_KERNEL` can sleep - it may trigger reclaim, I/O, or memory compaction. Use in process context. `GFP_ATOMIC` cannot sleep - used in interrupt context or with spinlocks held. Uses emergency reserves but may fail if memory is tight.
[Intermediate]

---

Q: (Understanding) Why can't you use `GFP_KERNEL` in interrupt context?
A: `GFP_KERNEL` may sleep (for reclaim, I/O wait, etc.). Sleeping in interrupt context would deadlock - the interrupt can't be preempted, and the CPU is stuck. Always use `GFP_ATOMIC` in interrupts.
[Intermediate]

---

Q: What does `__GFP_NOWARN` do?
A: Suppresses warning messages when allocation fails. Useful when failure is expected/handled, preventing kernel log spam. Often combined with `__GFP_NORETRY` for allocations that should fail silently.
[Basic]

---

Q: What does `__GFP_ZERO` do?
A: Tells the allocator to zero the allocated pages before returning. Equivalent to calling `memset(page_address(page), 0, size)` after allocation. Used for security-sensitive allocations.
[Basic]

---

Q: What is per-CPU page caching (PCP)?
A: Each CPU maintains hot/cold page lists (`struct per_cpu_pageset`) to avoid zone lock contention. Allocations first check the per-CPU list before falling back to the zone's buddy free lists.
[Intermediate]

---

Q: (Understanding) Why does the buddy allocator use per-CPU caches?
A: Zone free list access requires the zone lock (spinlock). In a multi-CPU system, this creates contention. Per-CPU caches allow most single-page allocations to complete without any locking, dramatically improving scalability.
[Intermediate]

---

Q: What is the difference between hot and cold pages?
A: Hot pages were recently used and likely in CPU cache - better for allocations that will be accessed immediately. Cold pages haven't been accessed recently - better for DMA or pages that will be overwritten. Per-CPU lists track both.
[Intermediate]

---

Q: What is `alloc_pages()` vs `__get_free_pages()`?
A: `alloc_pages()` returns a `struct page *` pointer. `__get_free_pages()` returns the kernel virtual address directly. `__get_free_pages()` internally calls `alloc_pages()` and then `page_address()`.
[Basic]

---

Q: How do you allocate a single page?
A: Several options:
- `alloc_page(gfp)` - Returns struct page *
- `__get_free_page(gfp)` - Returns virtual address
- `get_zeroed_page(gfp)` - Returns zeroed page virtual address
All are wrappers around order-0 buddy allocation.
[Basic]

---

Q: How do you free pages allocated with the buddy allocator?
A: Use the corresponding free function:
- `__free_pages(page, order)` - Free by struct page
- `free_pages(addr, order)` - Free by virtual address
- `__free_page(page)` - Single page by struct page
- `free_page(addr)` - Single page by address
[Basic]

---

Q: (Understanding) What happens if you free pages with the wrong order?
A: Memory corruption. The allocator will try to merge with incorrect buddy, corrupting free lists. May cause later allocations to fail or return wrong addresses. Always track and use the same order for alloc and free.
[Advanced]

---

Q: What is `alloc_pages_node()`?
A: Allocates pages from a specific NUMA node: `alloc_pages_node(nid, gfp, order)`. Useful when you need memory local to a specific node for performance. Falls back to other nodes based on zonelist if specified node is exhausted.
[Intermediate]

---

Q: What is external fragmentation?
A: When free memory exists but is scattered in small non-contiguous chunks, making large contiguous allocations impossible despite having enough total free memory. The buddy system addresses this through merging and migration types.
[Basic]

---

Q: (Understanding) Why does the buddy allocator use power-of-2 sizes?
A: Simplifies buddy finding (just XOR with size), ensures proper alignment, enables efficient splitting and merging, and makes address calculations fast (bit shifts instead of division). The trade-off is some internal fragmentation.
[Intermediate]

---

Q: What is memory compaction?
A: A technique to defragment memory by migrating movable pages to create large contiguous free regions. Triggered when high-order allocations fail. Only works for `MIGRATE_MOVABLE` pages. Implemented in `mm/compaction.c`.
[Intermediate]

---

Q: (Reverse) This allocation flag indicates the caller is in atomic context and cannot sleep.
A: Q: What is `GFP_ATOMIC`?
[Basic]

---

## Section 4: Virtual Memory

---

Q: What is virtual memory?
A: An abstraction that gives each process the illusion of having its own large, contiguous address space. The MMU (Memory Management Unit) translates virtual addresses to physical addresses using page tables. This enables isolation, memory overcommit, and demand paging.
[Basic]

---

Q: What is `struct mm_struct`?
A: The data structure representing a process's entire virtual address space. Contains pointers to page tables (pgd), VMA list/tree, memory statistics, and locks. Each process has one `mm_struct`, accessible via `current->mm`.
[Basic]

---

Q: What are the key fields in `struct mm_struct`?
A: Key fields include:
- `pgd` - Pointer to the page global directory (top-level page table)
- `mmap` - Linked list of VMAs
- `mm_rb` - Red-black tree of VMAs for fast lookup
- `map_count` - Number of VMAs
- `mm_users` - Reference count (users sharing mm)
- `mm_count` - Reference count (including kernel refs)
- `total_vm`, `locked_vm`, `shared_vm` - Memory statistics
[Intermediate]

---

Q: What is a VMA (Virtual Memory Area)?
A: A `struct vm_area_struct` representing a contiguous region of virtual memory with the same properties (permissions, backing). Each memory mapping (heap, stack, mmap'd file, etc.) is represented by one or more VMAs.
[Basic]

---

Q: What are the key fields in `struct vm_area_struct`?
A: Key fields include:
- `vm_start`, `vm_end` - Virtual address range [start, end)
- `vm_flags` - Protection and property flags
- `vm_mm` - Owning mm_struct
- `vm_page_prot` - Page table entry protection bits
- `vm_file` - Backing file (if file-mapped)
- `vm_ops` - VMA operation callbacks
- `vm_pgoff` - Offset in file (in pages)
[Intermediate]

---

Q: (ASCII Diagram) Show the relationship between mm_struct and VMAs.
A:
```
struct mm_struct (current->mm)
+------------------+
| pgd    --------->| Page Tables
| mmap   --------->| VMA list head
| mm_rb  --------->| VMA red-black tree
| map_count = 5    |
+------------------+
        |
        v (mmap linked list)
+----------+    +----------+    +----------+
|   VMA    |--->|   VMA    |--->|   VMA    |--->...
| [heap]   |    | [libc]   |    | [stack]  |
| 0x800000 |    | 0x400000 |    | 0x7fff.. |
| -0x900000|    | -0x480000|    | -0x7fff..|
+----------+    +----------+    +----------+
```
[Intermediate]

---

Q: List common VMA flags (`vm_flags`).
A:
- `VM_READ` - Readable
- `VM_WRITE` - Writable  
- `VM_EXEC` - Executable
- `VM_SHARED` - Shared mapping (vs private/COW)
- `VM_GROWSDOWN` - Stack, grows downward
- `VM_DONTCOPY` - Don't copy on fork
- `VM_LOCKED` - Locked in memory (no swap)
- `VM_IO` - Memory-mapped I/O region
[Basic]

---

Q: What is the difference between `VM_SHARED` and private mappings?
A: `VM_SHARED` mappings share modifications with other processes mapping the same file. Private mappings use copy-on-write - writes create private copies of pages. `mmap()` with `MAP_SHARED` vs `MAP_PRIVATE` sets this flag.
[Intermediate]

---

Q: How are VMAs organized for efficient lookup?
A: VMAs are organized in two structures:
1. Linked list (`vma->vm_next`) - For sequential iteration
2. Red-black tree (`mm->mm_rb`) - For O(log n) lookup by address
Both must be kept in sync when VMAs are added/removed.
[Intermediate]

---

Q: What does `find_vma(mm, addr)` do?
A: Returns the first VMA with `vma->vm_end > addr` (VMA that contains or is after the address). Returns NULL if no such VMA exists. Used to find which VMA handles a given virtual address.
[Intermediate]

---

Q: (Code Interpretation) What does this code do?
```c
struct vm_area_struct *vma = find_vma(mm, addr);
if (vma && vma->vm_start <= addr) {
    // addr is within vma
} else {
    // addr is in a hole or beyond all VMAs
}
```
A: Finds the VMA containing or after `addr`. The additional check `vm_start <= addr` confirms the address is actually inside the VMA (not in a hole before it). This is the standard pattern for checking if an address is mapped.
[Intermediate]

---

Q: What is `find_vma_intersection()`?
A: Returns the first VMA that overlaps with a given address range [start, end). Used to check if a region is already mapped before creating a new mapping.
[Intermediate]

---

Q: What is `vma_merge()`?
A: Attempts to merge a new mapping with adjacent VMAs if they have compatible properties (same flags, same file at adjacent offsets, etc.). Reduces the number of VMAs for memory efficiency.
[Intermediate]

---

Q: When can two adjacent VMAs be merged?
A: VMAs can be merged when they have:
- Identical `vm_flags`
- Same `vm_file` at contiguous file offsets
- Same `vm_ops`
- No special properties preventing merge
This reduces memory overhead and simplifies the VMA tree.
[Intermediate]

---

Q: What is `split_vma()`?
A: Splits a VMA into two at a given address. Used when `munmap()` unmaps part of a VMA, or when `mprotect()` changes permissions for only part of a region. The result is two VMAs with the same properties except for the address range.
[Intermediate]

---

Q: What is a page table?
A: A hierarchical data structure that maps virtual addresses to physical addresses. The CPU's MMU walks the page table on every memory access (with TLB caching). Linux uses a 4-level hierarchy on x86-64.
[Basic]

---

Q: What are the four levels of page tables in x86-64 Linux?
A:
- PGD (Page Global Directory) - Top level, indexed by bits 47-39
- PUD (Page Upper Directory) - Second level, bits 38-30
- PMD (Page Middle Directory) - Third level, bits 29-21
- PTE (Page Table Entry) - Bottom level, bits 20-12
Bits 11-0 are the page offset.
[Intermediate]

---

Q: (ASCII Diagram) Show the 4-level page table walk for virtual address translation.
A:
```
Virtual Address (48 bits used on x86-64):
+--------+--------+--------+--------+------------+
| PGD    | PUD    | PMD    | PTE    | Page       |
| index  | index  | index  | index  | Offset     |
| 9 bits | 9 bits | 9 bits | 9 bits | 12 bits    |
+--------+--------+--------+--------+------------+
    |        |        |        |          |
    v        v        v        v          v
  +---+    +---+    +---+    +---+    +--------+
  |PGD|--->|PUD|--->|PMD|--->|PTE|--->|Physical|
  +---+    +---+    +---+    +---+    | Page   |
  512      512      512      512      | + offs |
  entries  entries  entries  entries  +--------+
```
[Intermediate]

---

Q: How do you walk the page tables to find a PTE?
A: Use the offset macros:
```c
pgd = pgd_offset(mm, addr);
pud = pud_offset(pgd, addr);
pmd = pmd_offset(pud, addr);
pte = pte_offset_map(pmd, addr);
```
Each step extracts the index from the address and looks up the next level.
[Intermediate]

---

Q: (Code Interpretation) Explain this page table walk:
```c
pgd_t *pgd = pgd_offset(mm, address);
if (pgd_none(*pgd) || pgd_bad(*pgd))
    return NULL;
pud_t *pud = pud_offset(pgd, address);
if (pud_none(*pud) || pud_bad(*pud))
    return NULL;
// ... continue to PMD and PTE
```
A: Walks page tables checking each level. `pgd_none()` checks if the entry is empty (no mapping). `pgd_bad()` checks for corruption. If either is true, the virtual address has no valid mapping and we return early. This prevents crashes from accessing invalid table entries.
[Advanced]

---

Q: What is a PTE (Page Table Entry)?
A: The leaf entry in the page table containing:
- Physical page frame number (PFN)
- Permission bits (read, write, execute)
- Status bits (present, dirty, accessed)
- Caching attributes
The MMU uses these to translate addresses and enforce permissions.
[Basic]

---

Q: What bits are in a typical x86 PTE?
A: Key bits include:
- Bit 0: Present (P) - Page is in memory
- Bit 1: Read/Write (R/W) - Writable if set
- Bit 2: User/Supervisor (U/S) - User-accessible if set
- Bit 5: Accessed (A) - Page was read
- Bit 6: Dirty (D) - Page was written
- Bit 63: NX (No Execute) - Non-executable if set
- Bits 12-51: Physical page frame number
[Intermediate]

---

Q: What does `pte_present()` check?
A: Returns true if the PTE's present bit is set, meaning the page is currently in physical memory. A non-present PTE might indicate the page is swapped out, not yet allocated (demand paging), or part of a file mapping not yet faulted in.
[Basic]

---

Q: What does `pte_dirty()` indicate?
A: The page has been written to since it was loaded into memory (or since the dirty bit was cleared). The kernel uses this to know which pages need to be written back to disk before being reclaimed.
[Basic]

---

Q: What is the TLB (Translation Lookaside Buffer)?
A: A CPU cache of recent virtual-to-physical address translations. TLB hits avoid page table walks (hundreds of cycles). TLB misses trigger hardware page table walks. The kernel must flush TLB entries when page tables change.
[Basic]

---

Q: What does `flush_tlb_page()` do?
A: Invalidates the TLB entry for a single page on the current CPU. Used after modifying a single PTE to ensure subsequent accesses use the new mapping. Less expensive than flushing the entire TLB.
[Intermediate]

---

Q: What does `flush_tlb_mm()` do?
A: Flushes all TLB entries associated with a given `mm_struct` (process). Used during process exit or major address space changes. On x86, this often means flushing the entire TLB.
[Intermediate]

---

Q: (Understanding) Why is TLB flushing expensive?
A: TLB flushes invalidate cached translations, forcing subsequent accesses to walk page tables. On multi-CPU systems, IPIs (Inter-Processor Interrupts) may be needed to flush TLBs on other CPUs, adding cross-CPU synchronization overhead.
[Intermediate]

---

Q: What is `mm->pgd` and how is it used?
A: The pointer to the top-level page table (Page Global Directory) for a process. During context switch, this value is loaded into the CR3 register (on x86) to switch to the new process's address space.
[Intermediate]

---

Q: What happens during a context switch for memory management?
A: The kernel loads the new process's page table into the CPU:
1. Save current CR3 (page table pointer)
2. Load new process's `mm->pgd` into CR3
3. This implicitly flushes TLB (non-global entries)
The CPU now uses the new process's address translation.
[Intermediate]

---

Q: What is a huge page?
A: A memory page larger than the standard 4KB. On x86-64, 2MB (PMD-level) and 1GB (PUD-level) huge pages are supported. Fewer TLB entries needed for the same memory, improving performance for large allocations.
[Basic]

---

Q: What is Transparent Huge Pages (THP)?
A: A kernel feature that automatically uses huge pages when beneficial, without application changes. The kernel merges 4KB pages into 2MB huge pages in the background and splits them when needed.
[Intermediate]

---

Q: (Understanding) What is the trade-off with huge pages?
A: Benefits: Fewer TLB entries, reduced page table memory, faster TLB lookups. Drawbacks: More internal fragmentation, harder to allocate (need 2MB contiguous physical memory), longer fault times, more memory wasted if not fully utilized.
[Intermediate]

---

Q: What is `do_mmap()`?
A: The core function for creating new memory mappings. Called by `mmap()` system call. Finds space in the address space, creates a VMA, sets up appropriate properties based on flags, and may pre-fault pages.
[Intermediate]

---

Q: What does `do_munmap()` do?
A: Removes a memory mapping. Splits VMAs if needed, unmaps page table entries, frees any private pages, and removes the VMA from the mm_struct. Called by the `munmap()` system call.
[Intermediate]

---

Q: What is the purpose of `vm_ops` in a VMA?
A: The `vm_operations_struct` provides callbacks for VMA-specific operations:
- `fault` - Handle page faults
- `open`/`close` - VMA duplication/destruction
- `access` - For /proc/pid/mem access
Allows different behavior for file mappings, anonymous memory, device memory, etc.
[Intermediate]

---

Q: (Cloze) The kernel heap is grown using the _____ system call, which adjusts the _____ pointer in mm_struct.
A: `brk` (or `sbrk`), `brk` pointer (program break). The heap extends from the end of data segment to `mm->brk`. `brk()` simply moves this boundary.
[Basic]

---

Q: What is the difference between the kernel and user address space?
A: User space is the lower portion of the virtual address space (0 to TASK_SIZE, ~128TB on x86-64). Kernel space is the upper portion, shared across all processes. User processes can only access user space; kernel can access both.
[Basic]

---

Q: (ASCII Diagram) Show the virtual address space layout on x86-64.
A:
```
0xFFFFFFFFFFFFFFFF +-------------------+
                   | Kernel Space      | (shared by all processes)
                   | - Direct mapping  |
                   | - vmalloc area    |
                   | - kernel code     |
0xFFFF800000000000 +-------------------+
                   |    (hole)         |
0x00007FFFFFFFFFFF +-------------------+
                   | User Space        | (per-process)
                   | - Stack (grows ↓) |
                   | - mmap region     |
                   | - Heap (grows ↑)  |
                   | - BSS, Data       |
                   | - Text (code)     |
0x0000000000000000 +-------------------+
```
[Intermediate]

---

Q: What is ASLR (Address Space Layout Randomization)?
A: A security feature that randomizes the location of stack, heap, mmap region, and shared libraries at process start. Makes exploits harder by preventing attackers from knowing where code/data will be located.
[Basic]

---

Q: What is the `mmap_min_addr` sysctl?
A: Defines the minimum address that can be mapped in user space. Default is typically 4096 or 65536. Prevents NULL pointer dereference exploits by ensuring the zero page region cannot be mapped.
[Intermediate]

---

Q: (Reverse) This data structure contains the vm_start and vm_end fields defining a virtual address range.
A: Q: What is `struct vm_area_struct` (VMA)?
[Basic]

---

Q: What is copy-on-write (COW) for VMAs?
A: An optimization where forked processes share read-only copies of pages until one writes. On write fault, the kernel copies the page, giving the writer a private copy. Saves memory and makes fork() fast.
[Basic]

---

Q: (Understanding) How does copy-on-write interact with `vm_flags`?
A: During fork, private writable VMAs get write protection removed from PTEs but keep `VM_WRITE` in `vm_flags`. On write fault, the kernel sees `VM_WRITE` is allowed but PTE is not writable, recognizing a COW situation and making a copy.
[Advanced]

---

## Section 5: Page Fault Handling

---

Q: What is a page fault?
A: A CPU exception triggered when accessing a virtual address that cannot be resolved by the MMU. Causes include: page not present (demand paging), permission violation (write to read-only), or invalid address. The kernel handles it via the page fault handler.
[Basic]

---

Q: What are the three types of page faults by outcome?
A:
1. **Minor fault** - Page is in memory but not mapped (e.g., in page cache). Just update PTE.
2. **Major fault** - Page must be loaded from disk (swap or file). Expensive I/O involved.
3. **Invalid fault** - Access to invalid address or permission violation. Results in SIGSEGV.
[Basic]

---

Q: What is the entry point for page fault handling on x86?
A: `do_page_fault()` in `arch/x86/mm/fault.c`. It's called by the CPU's page fault exception handler. It determines fault type, finds the VMA, and calls the appropriate handler.
[Intermediate]

---

Q: Describe the high-level page fault handling path.
A:
1. `do_page_fault()` - Architecture entry, get fault address/error code
2. `__do_page_fault()` - Find VMA, check permissions
3. `handle_mm_fault()` - Generic fault handler
4. `handle_pte_fault()` - Handle at PTE level
5. Specific handlers: `do_anonymous_page()`, `do_fault()`, `do_swap_page()`, `do_wp_page()`
[Intermediate]

---

Q: (ASCII Diagram) Show the page fault handling call path.
A:
```
CPU Exception
      |
      v
do_page_fault()         [arch/x86/mm/fault.c]
      |
      v
find_vma() ------------> No VMA? SIGSEGV
      |
      v
handle_mm_fault()       [mm/memory.c]
      |
      v
handle_pte_fault()
      |
      +---> do_anonymous_page()  [new anon page]
      +---> do_fault()           [file-backed page]
      |       +---> do_read_fault()
      |       +---> do_cow_fault()
      |       +---> do_shared_fault()
      +---> do_swap_page()       [swap in]
      +---> do_wp_page()         [copy-on-write]
```
[Intermediate]

---

Q: What information does the CPU provide on a page fault?
A: Two key pieces:
1. **Fault address** - The virtual address that caused the fault (CR2 register on x86)
2. **Error code** - Bits indicating fault type: user/kernel mode, read/write, present bit state, instruction fetch vs data
[Intermediate]

---

Q: What does `handle_mm_fault()` do?
A: The architecture-independent fault handler. Takes the mm_struct, VMA, address, and flags. Walks page tables (allocating missing levels), then calls `handle_pte_fault()` to handle the actual fault at the leaf level.
[Intermediate]

---

Q: What does `handle_pte_fault()` do?
A: Examines the PTE and dispatches to the appropriate handler:
- PTE not present → `do_fault()` (file) or `do_anonymous_page()` (anon)
- PTE present but swap entry → `do_swap_page()`
- Write fault on read-only PTE → `do_wp_page()` (COW)
[Intermediate]

---

Q: What is `do_anonymous_page()`?
A: Handles faults on anonymous VMAs (heap, stack, private mmap) when no page exists. Allocates a new zeroed page, creates a PTE mapping it, and returns. For read faults, may map the shared zero page instead.
[Intermediate]

---

Q: (Code Interpretation) What does this code in do_anonymous_page() do?
```c
if (!(flags & FAULT_FLAG_WRITE)) {
    entry = pte_mkspecial(pfn_pte(my_zero_pfn(address), vma->vm_page_prot));
    ptep_set_at(mm, address, pte, entry);
    return 0;
}
```
A: Optimizes read faults by mapping the special "zero page" instead of allocating a new page. All read-only anonymous pages share this single zeroed page. Only when a write occurs will a real page be allocated (via COW).
[Advanced]

---

Q: What is `do_fault()`?
A: Handles faults for file-backed VMAs. Calls the VMA's `vm_ops->fault()` callback to read the page from the file. This invokes the filesystem's page cache mechanism to load the page content.
[Intermediate]

---

Q: What is `do_swap_page()`?
A: Handles faults where the PTE contains a swap entry (page was swapped out). Reads the page back from swap space, allocates a page frame, copies data from swap, and updates the PTE. This is a major fault (involves I/O).
[Intermediate]

---

Q: What is `do_wp_page()` (write-protect page)?
A: Handles copy-on-write faults. When a write occurs to a shared read-only page, this function allocates a new page, copies the content, and maps the new page as writable. The original shared page is unchanged.
[Intermediate]

---

Q: (Understanding) When does copy-on-write trigger `do_wp_page()`?
A: When:
1. Process writes to a page shared via fork()
2. Multiple processes have the same page mapped (e.g., shared library data)
3. The page was initially mapped read-only due to COW but VMA allows write
The function checks if the page can be reused (single reference) or must be copied.
[Intermediate]

---

Q: (Code Interpretation) What optimization is this checking for in do_wp_page()?
```c
if (page_count(old_page) == 1) {
    reuse_swap_page(old_page);
    pte = pte_mkdirty(pte_mkwrite(pte));
    ptep_set_access_flags(...);
    return 0;
}
```
A: If this process is the only user of the page (count==1), skip copying. Just reuse the existing page by making the PTE writable. This happens after the other fork'd process has exited or already made its own copy.
[Advanced]

---

Q: What is demand paging?
A: Memory is allocated only when first accessed, not when mapped. When `mmap()` creates a mapping, no pages are allocated. The first access causes a page fault, which allocates and maps the page. Saves memory for unused mappings.
[Basic]

---

Q: What is the "zero page" optimization?
A: A single physical page filled with zeros, shared by all processes for read-only anonymous mappings. Reading from a freshly mapped anonymous page returns zeros from this shared page. Only when written does a real page get allocated.
[Intermediate]

---

Q: What is a swap entry in a PTE?
A: When a page is swapped out, the PTE is modified to hold a swap entry instead of a physical address. The swap entry contains the swap device/file and offset. The "present" bit is clear, so access triggers a fault.
[Intermediate]

---

Q: (Cloze) A page fault that requires disk I/O is called a _____ fault, while one resolved entirely in memory is a _____ fault.
A: **major** fault, **minor** fault. Major faults are expensive (milliseconds), minor faults are fast (microseconds).
[Basic]

---

Q: How do you monitor page fault statistics?
A: Several methods:
- `/proc/[pid]/stat` - fields for minor and major faults
- `getrusage()` - returns `ru_minflt` and `ru_majflt`
- `perf stat` - shows page fault events
- `/proc/vmstat` - system-wide fault counters
[Intermediate]

---

Q: What is the FAULT_FLAG_WRITE flag?
A: Passed to fault handlers indicating this is a write fault (vs read). Determines whether copy-on-write is needed, and whether to set the dirty bit. Extracted from the CPU's error code.
[Intermediate]

---

Q: What is prefaulting and when is it used?
A: Allocating and mapping pages before they're accessed, avoiding the fault overhead. Done via `mlock()` (locks pages in memory) or `MAP_POPULATE` flag to `mmap()`. Trades memory for lower latency.
[Intermediate]

---

Q: (Reverse) This fault handler function is called when a process writes to a read-only page that's marked for copy-on-write.
A: Q: What is `do_wp_page()`?
[Intermediate]

---

Q: What is a segmentation fault (SIGSEGV)?
A: Signal sent when a page fault cannot be handled:
- Access to unmapped region (no VMA)
- Permission violation (write to read-only, exec on non-exec)
- Invalid address (kernel space from user mode)
Usually terminates the process.
[Basic]

---

Q: (Understanding) How does the kernel distinguish between a valid fault and SIGSEGV?
A: It checks:
1. Is the address within a VMA? (use `find_vma()`)
2. Does the access type (read/write/exec) match VMA permissions?
3. Was it user mode accessing user space?
If any check fails, it's an invalid access → SIGSEGV.
[Intermediate]

---

Q: What happens when a kernel page fault cannot be handled?
A: The kernel panics or oopses. Unlike user-space faults (SIGSEGV), there's no signal to send. Common causes: NULL pointer dereference in kernel code, use-after-free, corrupted page tables.
[Intermediate]

---

## Section 6: Slab Allocators

---

Q: Why do we need slab allocators if we have the buddy allocator?
A: The buddy allocator works with page-sized allocations (4KB minimum). Kernel objects are often small (64-512 bytes). Slab allocators efficiently manage these small allocations, reducing waste and enabling object caching.
[Basic]

---

Q: What is the slab allocator concept?
A: Pre-allocate pages and subdivide them into fixed-size chunks for specific object types. Objects are cached after free for quick reallocation. Reduces fragmentation and allocation overhead for frequently used kernel objects.
[Basic]

---

Q: What three slab allocator implementations exist in the Linux kernel?
A:
1. **SLAB** - Original implementation, feature-rich, more memory overhead
2. **SLUB** - Default since 2.6.23, simpler, better performance and debugging
3. **SLOB** - Minimal implementation for embedded systems, small memory footprint
[Basic]

---

Q: What is `struct kmem_cache`?
A: The descriptor for a slab cache. Contains object size, alignment, constructor, number of objects per slab, per-CPU caches, and partial/full slab lists. Each object type has its own `kmem_cache`.
[Intermediate]

---

Q: What are the key fields in `struct kmem_cache`?
A: Key fields include:
- `name` - Cache identifier string
- `size` - Object size including metadata
- `object_size` - Actual object size requested
- `align` - Required alignment
- `ctor` - Constructor function (optional)
- `cpu_slab` - Per-CPU slab pointer (SLUB)
- `node[]` - Per-node slab lists
[Intermediate]

---

Q: What is `kmem_cache_create()`?
A: Creates a new slab cache:
```c
struct kmem_cache *cache = kmem_cache_create(
    "my_cache",      // name
    sizeof(struct my_obj),  // size
    0,               // alignment (0 = default)
    SLAB_HWCACHE_ALIGN,  // flags
    my_constructor   // constructor (or NULL)
);
```
Returns a cache pointer used for allocations.
[Intermediate]

---

Q: What is `kmem_cache_alloc()`?
A: Allocates an object from a slab cache:
```c
struct my_obj *obj = kmem_cache_alloc(cache, GFP_KERNEL);
```
Returns a pointer to the object, or NULL on failure. If a constructor was specified, the object is already initialized.
[Basic]

---

Q: What is `kmem_cache_free()`?
A: Returns an object to its slab cache:
```c
kmem_cache_free(cache, obj);
```
The object becomes available for reallocation. If the cache has a destructor (rare), it's not called - objects may be reused without re-initialization.
[Basic]

---

Q: What is `kmem_cache_destroy()`?
A: Destroys a slab cache and frees all its memory. All objects must be freed first. Used when the cache is no longer needed (e.g., module unload).
```c
kmem_cache_destroy(cache);
```
[Basic]

---

Q: What is `kmalloc()`?
A: General-purpose kernel memory allocator for arbitrary sizes:
```c
void *ptr = kmalloc(size, GFP_KERNEL);
```
Uses pre-created size-based caches (32, 64, 128, 256... bytes). Fast but may waste memory if size isn't a power of 2.
[Basic]

---

Q: How does `kmalloc()` work internally?
A: kmalloc uses an array of pre-created caches for different size classes. It rounds up the requested size to the nearest cache and calls `kmem_cache_alloc()` on that cache. Size classes are typically powers of 2.
[Intermediate]

---

Q: (Cloze) kmalloc allocates from pre-created caches named _____ with sizes from _____ to _____ bytes.
A: `kmalloc-*` (e.g., `kmalloc-64`, `kmalloc-128`), from 8 (or 32) bytes up to several megabytes (platform-dependent, often 8MB max).
[Intermediate]

---

Q: What is `kfree()`?
A: Frees memory allocated by `kmalloc()`:
```c
kfree(ptr);
```
Determines which cache the allocation came from and calls `kmem_cache_free()`. Passing NULL is safe (no-op).
[Basic]

---

Q: What is the difference between `kmalloc()` and `vmalloc()`?
A: `kmalloc()` returns physically contiguous memory from slab caches. `vmalloc()` returns virtually contiguous but potentially physically scattered memory. Use `kmalloc()` for small allocations and DMA; `vmalloc()` for large allocations where physical contiguity doesn't matter.
[Intermediate]

---

Q: What is `kzalloc()`?
A: Allocates and zeros memory in one call:
```c
void *ptr = kzalloc(size, GFP_KERNEL);
```
Equivalent to `kmalloc()` followed by `memset(ptr, 0, size)`. More efficient and prevents information leaks.
[Basic]

---

Q: What is `kcalloc()`?
A: Allocates an array of objects:
```c
struct obj *arr = kcalloc(n, sizeof(struct obj), GFP_KERNEL);
```
Like `kzalloc()` but with overflow checking on `n * size` multiplication. Returns zeroed memory.
[Basic]

---

Q: What is `krealloc()`?
A: Resizes a kmalloc allocation:
```c
ptr = krealloc(ptr, new_size, GFP_KERNEL);
```
May move the allocation if the new size requires a different cache. Original data is preserved. Returns NULL on failure (original still valid).
[Intermediate]

---

Q: (ASCII Diagram) Show the relationship between kmalloc caches and slab pages.
A:
```
kmalloc-64 cache                    kmalloc-128 cache
+------------------+                +------------------+
| struct kmem_cache|                | struct kmem_cache|
|   object_size=64 |                |  object_size=128 |
|   per_cpu_slab   |                |   per_cpu_slab   |
+--------+---------+                +--------+---------+
         |                                   |
         v                                   v
   +-----------+                      +-----------+
   | Slab Page |                      | Slab Page |
   +-----------+                      +-----------+
   |obj|obj|obj|                      |obj|  obj  |
   |64b|64b|64b|                      |128b| 128b |
   +-----------+                      +-----------+
   (64 objects/page)                  (32 objects/page)
```
[Intermediate]

---

Q: What slab flags affect cache behavior?
A: Common flags:
- `SLAB_HWCACHE_ALIGN` - Align objects to CPU cache lines
- `SLAB_POISON` - Fill freed objects with poison pattern
- `SLAB_RED_ZONE` - Add red zones to detect buffer overflows
- `SLAB_PANIC` - Panic if cache creation fails
- `SLAB_RECLAIM_ACCOUNT` - Count as reclaimable memory
[Intermediate]

---

Q: What is SLAB_HWCACHE_ALIGN?
A: Aligns object start address to L1 cache line boundary (typically 64 bytes). Improves performance by preventing objects from spanning cache lines, reducing false sharing in SMP systems.
[Intermediate]

---

Q: What is a per-CPU slab cache?
A: Each CPU has its own active slab to allocate from, avoiding lock contention. In SLUB, `cpu_slab` points to the current CPU's active slab. Most allocations complete without any locking.
[Intermediate]

---

Q: (Understanding) How does SLUB improve over classic SLAB?
A: SLUB improvements:
- Simpler design, less code
- No per-CPU array caches (uses per-CPU slabs instead)
- Better CPU cache behavior
- Lower memory overhead
- Built-in debugging features
- Better NUMA awareness
[Intermediate]

---

Q: What is a partial slab in SLUB?
A: A slab page with some objects allocated and some free. Partial slabs are kept in per-node lists. When the per-CPU slab is exhausted, allocation falls back to partial slabs before allocating new pages.
[Intermediate]

---

Q: (Code Interpretation) What does this SLUB allocation fast path do?
```c
object = c->freelist;
if (likely(object)) {
    c->freelist = get_freepointer(s, object);
    return object;
}
```
A: Fast path allocation in SLUB. `c` is the per-CPU cache, `freelist` points to the first free object. If available, update freelist to point to next free object (stored in the object itself via `get_freepointer()`), return the object. No locking needed.
[Advanced]

---

Q: How does SLUB store the freelist?
A: SLUB stores the next-free pointer inside the free object itself (at a configurable offset). This saves memory compared to SLAB's external freelist arrays. The pointer is overwritten when the object is allocated.
[Intermediate]

---

Q: What is slab coloring?
A: Technique to reduce cache conflicts by offsetting object starting addresses within slabs. Different slabs start objects at different offsets, spreading them across cache lines and reducing associativity conflicts.
[Advanced]

---

Q: What is `SLAB_POISON` and when is it useful?
A: Fills freed objects with a pattern (0x6b in SLUB). Helps detect use-after-free bugs - accessing freed memory returns recognizable garbage. Disabled in production for performance but useful for debugging.
[Intermediate]

---

Q: What is `SLAB_RED_ZONE` and how does it work?
A: Adds guard bytes before and after objects. These "red zones" are filled with a known pattern. On free, they're checked for corruption, detecting buffer overflows/underflows.
[Intermediate]

---

Q: How do you view slab cache statistics?
A: Multiple interfaces:
- `/proc/slabinfo` - Basic stats for all caches
- `slabtop` command - Top-like view of slab usage
- `/sys/kernel/slab/` - Detailed per-cache info (SLUB)
- `slabinfo` tool - Detailed analysis
[Intermediate]

---

Q: (Code Interpretation) What does this slabinfo output indicate?
```
# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>
task_struct         196    196    5872       5    8
inode_cache        3072   3072     592      27    4
```
A: Two slab caches:
- `task_struct`: 196 active objects, each 5872 bytes, 5 objects per slab, 8 pages per slab (40KB per slab)
- `inode_cache`: 3072 inodes, 592 bytes each, 27 per slab, 4 pages per slab (16KB per slab)
[Intermediate]

---

Q: What is `kmem_cache_shrink()`?
A: Reclaims memory by freeing empty slabs from a cache. Doesn't free partial or full slabs. Called during memory pressure to return unused pages to the buddy allocator.
[Intermediate]

---

Q: (Reverse) This function allocates zeroed memory of a given size from the slab allocator.
A: Q: What is `kzalloc()`?
[Basic]

---

Q: What is the SLOB allocator designed for?
A: Embedded systems with very limited memory. Uses a simple first-fit algorithm on lists of free chunks. Minimal overhead but slower and more fragmentation-prone than SLAB/SLUB. Saves tens of KB of memory.
[Intermediate]

---

Q: (Understanding) Why shouldn't you call kmalloc() with very large sizes?
A: Large kmalloc allocations require contiguous physical pages from the buddy allocator. Under memory pressure, large contiguous regions are scarce. Use vmalloc() for large allocations where physical contiguity isn't needed.
[Intermediate]

---

## Section 7: vmalloc and Per-CPU Allocators

---

Q: What is `vmalloc()`?
A: Allocates virtually contiguous kernel memory that may be physically discontiguous:
```c
void *ptr = vmalloc(size);
```
Useful for large allocations (>page size) where physical contiguity isn't required. Slower than kmalloc due to page table setup.
[Basic]

---

Q: How does vmalloc work internally?
A: 1. Allocate individual pages from buddy allocator (scattered in physical memory)
2. Find free space in vmalloc address range
3. Create page table entries mapping these pages contiguously in virtual space
4. Return the starting virtual address
[Intermediate]

---

Q: (ASCII Diagram) Show how vmalloc maps scattered physical pages.
A:
```
Physical Memory                Virtual Address Space (vmalloc range)
+--------+                     VMALLOC_START
| Page A |---+                 +------------------+
+--------+   |                 | vmalloc region 1 |
   ...       +---------------->| Page A (virt)    |
+--------+   |                 | Page B (virt)    |
| Page B |---+                 | Page C (virt)    |
+--------+                     +------------------+
   ...                         | vmalloc region 2 |
+--------+                     +------------------+
| Page C |---+                        ...
+--------+   |                 VMALLOC_END
             +---------------->
             
Physically scattered, virtually contiguous
```
[Intermediate]

---

Q: What is `vfree()`?
A: Frees memory allocated by vmalloc:
```c
vfree(ptr);
```
Unmaps the virtual addresses, frees the page table entries, and returns individual pages to the buddy allocator. Must not be called in interrupt context.
[Basic]

---

Q: What is `struct vm_struct`?
A: Descriptor for a vmalloc region. Contains:
- `addr` - Start virtual address
- `size` - Size in bytes
- `pages` - Array of struct page pointers
- `nr_pages` - Number of pages
- `flags` - Allocation flags
Linked in a global list for management.
[Intermediate]

---

Q: What is the vmalloc address range?
A: A dedicated region in kernel virtual address space for vmalloc allocations. On x86-64: `VMALLOC_START` to `VMALLOC_END`, typically several hundred GB. This is separate from the direct-mapped physical memory region.
[Intermediate]

---

Q: (Understanding) Why is vmalloc slower than kmalloc?
A: vmalloc must:
1. Allocate pages individually (multiple buddy allocator calls)
2. Allocate and populate page table entries
3. Potentially flush TLBs
kmalloc just finds a slot in a pre-existing slab - no page table manipulation needed.
[Intermediate]

---

Q: What is `vzalloc()`?
A: Allocates and zeros vmalloc memory:
```c
void *ptr = vzalloc(size);
```
Equivalent to vmalloc followed by memset to zero. Convenient for allocating zeroed buffers.
[Basic]

---

Q: What is `vmap()`?
A: Maps an array of already-allocated pages into contiguous virtual addresses:
```c
void *addr = vmap(pages, nr_pages, flags, prot);
```
Unlike vmalloc, doesn't allocate pages - just creates the mapping. Used when you have pages from another source.
[Intermediate]

---

Q: What is `vunmap()`?
A: Removes a mapping created by vmap:
```c
vunmap(addr);
```
Frees the virtual address space and page tables. Does NOT free the underlying pages (caller's responsibility).
[Intermediate]

---

Q: When should you use vmalloc vs kmalloc?
A: Use vmalloc for:
- Large allocations (>128KB)
- When physical contiguity isn't needed
- Allocating module code/data

Use kmalloc for:
- Small to medium allocations
- DMA buffers (need physical contiguity)
- Performance-critical paths
[Intermediate]

---

Q: What is per-CPU memory?
A: Memory allocated separately for each CPU, accessible via per-CPU variable macros. Eliminates cache line bouncing and lock contention since each CPU accesses its own copy. Used for counters, caches, and CPU-local data.
[Basic]

---

Q: How do you declare a per-CPU variable?
A: Static declaration:
```c
DEFINE_PER_CPU(int, counter);  // one int per CPU
```
Access:
```c
int val = this_cpu_read(counter);      // read
this_cpu_write(counter, val + 1);      // write
this_cpu_inc(counter);                 // atomic increment
```
[Intermediate]

---

Q: What is `alloc_percpu()`?
A: Dynamically allocates per-CPU memory:
```c
struct my_data __percpu *ptr = alloc_percpu(struct my_data);
struct my_data *this = per_cpu_ptr(ptr, smp_processor_id());
```
Returns a special pointer that must be dereferenced with `per_cpu_ptr()`. Freed with `free_percpu()`.
[Intermediate]

---

Q: What is `free_percpu()`?
A: Frees memory allocated with alloc_percpu:
```c
free_percpu(ptr);
```
Releases the per-CPU allocations for all CPUs.
[Basic]

---

Q: (Understanding) Why is per-CPU data more efficient than shared data?
A: Per-CPU data eliminates:
1. Cache line bouncing (no sharing between CPUs)
2. Lock contention (no synchronization needed)
3. False sharing (each CPU has its own cache line)
Results in better scalability on multi-core systems.
[Intermediate]

---

Q: What is `per_cpu_ptr(ptr, cpu)`?
A: Converts a per-CPU pointer to a regular pointer for a specific CPU:
```c
struct my_data *data = per_cpu_ptr(ptr, cpu_id);
```
Used to access another CPU's per-CPU data (e.g., for statistics aggregation).
[Intermediate]

---

Q: What is `this_cpu_ptr()`?
A: Returns pointer to current CPU's per-CPU data:
```c
struct my_data *data = this_cpu_ptr(ptr);
```
Equivalent to `per_cpu_ptr(ptr, smp_processor_id())` but may be optimized. Preemption should be disabled during access.
[Intermediate]

---

Q: (Reverse) This function allocates large, virtually contiguous kernel memory without requiring physical contiguity.
A: Q: What is `vmalloc()`?
[Basic]

---

## Section 8: Page Reclaim and OOM

---

Q: What is page reclaim?
A: The process of freeing physical pages when memory is low. The kernel identifies pages that can be freed or swapped out, releases them, and returns them to the buddy allocator for reuse by new allocations.
[Basic]

---

Q: What types of pages can be reclaimed?
A: Reclaimable pages include:
- **Page cache** - File-backed pages (can be re-read from disk)
- **Anonymous pages** - If swap is available (written to swap)
- **Slab caches** - Marked as reclaimable (dentries, inodes)
- **Clean pages** - No writeback needed

Non-reclaimable: kernel code, page tables, locked pages, mlocked pages.
[Basic]

---

Q: What is the LRU (Least Recently Used) list?
A: Data structure tracking page recency. Pages move to the "active" end when accessed and are evicted from the "inactive" end. The kernel maintains separate LRU lists for different page types.
[Basic]

---

Q: What are the four main LRU lists in Linux?
A:
1. `LRU_INACTIVE_ANON` - Inactive anonymous pages
2. `LRU_ACTIVE_ANON` - Active anonymous pages
3. `LRU_INACTIVE_FILE` - Inactive file-backed pages
4. `LRU_ACTIVE_FILE` - Active file-backed pages

(Plus `LRU_UNEVICTABLE` for locked pages)
[Intermediate]

---

Q: (ASCII Diagram) Show the LRU list organization.
A:
```
                 ACTIVE                    INACTIVE
              (hot pages)               (cold pages)
              
Anonymous:   +-------------+            +-------------+
             | Active Anon |  demote    | Inactive    |  reclaim
             | LRU List    | ---------> | Anon LRU    | --------> free
             +-------------+  promote   +-------------+
                            <---------
                            (accessed)

File-backed: +-------------+            +-------------+
             | Active File |  demote    | Inactive    |  reclaim
             | LRU List    | ---------> | File LRU    | --------> free
             +-------------+  promote   +-------------+
                            <---------
```
[Intermediate]

---

Q: What is kswapd?
A: The kernel swap daemon - a per-node kernel thread that performs background page reclaim. Woken when free memory drops below the low watermark, reclaims pages until reaching the high watermark. Runs asynchronously to avoid blocking allocations.
[Basic]

---

Q: What is direct reclaim?
A: Synchronous page reclaim performed by the allocating process when memory is critically low (below min watermark). The allocation blocks while the process itself reclaims pages. Slower than kswapd but necessary under severe pressure.
[Intermediate]

---

Q: What is the difference between kswapd and direct reclaim?
A:
| Aspect | kswapd | Direct Reclaim |
|--------|--------|----------------|
| Trigger | Below low watermark | Below min watermark |
| Context | Background thread | Allocating process |
| Blocking | Non-blocking | Blocks allocation |
| Aggressiveness | Moderate | Aggressive |
[Intermediate]

---

Q: What is `struct scan_control`?
A: Parameters passed to page reclaim functions:
- `nr_to_reclaim` - Target number of pages
- `gfp_mask` - Allocation flags affecting behavior
- `priority` - How hard to try (0=max effort)
- `may_swap` - Whether to swap anonymous pages
- `may_writepage` - Whether to write dirty pages
[Intermediate]

---

Q: What is reclaim priority?
A: A value (12 down to 0) controlling how aggressively to scan. At priority 12, scan 1/4096 of each LRU list. At priority 0, scan everything. Priority decreases with each reclaim pass that fails to free enough pages.
[Intermediate]

---

Q: What is `shrink_zone()`?
A: Main function for reclaiming pages from a memory zone. Iterates LRU lists, selects pages for eviction, handles writeback, and frees clean pages. Calls `shrink_active_list()` and `shrink_inactive_list()`.
[Intermediate]

---

Q: What is `shrink_inactive_list()`?
A: Scans the inactive LRU list for pages to reclaim. For each page:
1. If clean and unmapped → free immediately
2. If dirty file page → queue for writeback
3. If anonymous and swap available → swap out
4. If recently accessed → promote to active list
[Intermediate]

---

Q: (Understanding) Why are there separate LRU lists for file and anonymous pages?
A: Different reclaim characteristics:
- File pages: Can be discarded and re-read (unless dirty)
- Anonymous pages: Must be swapped (expensive I/O)
Having separate lists lets the kernel tune the balance, preferring to reclaim file pages first (cheaper) unless workload is file-heavy.
[Intermediate]

---

Q: What is the accessed bit and how is it used?
A: A bit in the PTE set by hardware when the page is accessed. The reclaim scanner uses it to implement "second chance" - pages with accessed bit set get a second chance (bit cleared, moved back). This approximates LRU without constant reordering.
[Intermediate]

---

Q: (Code Interpretation) What does this reclaim logic implement?
```c
if (page_referenced(page, 0, mf)) {
    if (page_mapping_inact_ref(page))
        goto keep_locked;
    if (page_is_file_cache(page))
        goto keep_locked;
}
```
A: Second-chance algorithm. `page_referenced()` checks and clears the accessed bit. If the page was recently accessed, keep it (don't reclaim). Special handling for file cache and recently activated pages.
[Advanced]

---

Q: What is `balance_pgdat()`?
A: The main kswapd loop function. Iterates zones from highest to lowest priority, calling reclaim functions until watermarks are satisfied or no more progress can be made. Balances memory across zones.
[Intermediate]

---

Q: What is swappiness?
A: A sysctl parameter (0-100) controlling the kernel's preference for swapping anonymous pages vs. reclaiming page cache. Higher values = more willing to swap. Default is 60. Set to 0 to avoid swapping (unless necessary).
[Intermediate]

---

Q: What is the OOM (Out of Memory) killer?
A: A last-resort mechanism when the system is truly out of memory and reclaim has failed. Selects a process to kill based on a badness score, terminates it with SIGKILL, freeing its memory for the system.
[Basic]

---

Q: What function calculates which process to kill in OOM?
A: `oom_badness()` calculates a score for each process. Higher score = more likely to be killed. Based on:
- RSS (resident memory) - Main factor
- oom_score_adj - Userspace adjustment (-1000 to +1000)
- Not kernel threads or init (score = 0)
[Intermediate]

---

Q: What is `select_bad_process()`?
A: OOM function that iterates all processes, calling `oom_badness()` for each, and returns the process with the highest score. Considers all threads in thread groups. Called when OOM decides to kill.
[Intermediate]

---

Q: What is `oom_kill_process()`?
A: Terminates the selected victim:
1. Print OOM information to kernel log
2. Send SIGKILL to the process and its threads
3. Mark process for memory unreservation
4. Wake up any waiters
[Intermediate]

---

Q: What is `oom_score_adj`?
A: Per-process OOM score adjustment (-1000 to +1000). Set via `/proc/[pid]/oom_score_adj`. -1000 means "never kill" (OOM exempt). +1000 means "always kill first". Used to protect critical processes.
[Intermediate]

---

Q: (Understanding) Why might you set a process's oom_score_adj to -1000?
A: To protect critical processes from OOM killer:
- Database servers (data corruption risk)
- Cluster managers
- Critical system daemons
However, be cautious - if all memory-heavy processes are protected, OOM can't free memory and system may hang.
[Intermediate]

---

Q: What triggers the OOM killer?
A: When:
1. Allocation fails even after direct reclaim
2. No swap space available or swap is full
3. All reclaimable pages have been freed
4. No more progress can be made
The allocator calls `out_of_memory()` which invokes the OOM killer.
[Intermediate]

---

Q: What is memory overcommit?
A: Allowing more virtual memory allocation than physical memory available. The system bets that not all allocated memory will be used simultaneously. If the bet fails, OOM killer is triggered.
[Basic]

---

Q: What are the overcommit modes (`vm.overcommit_memory`)?
A:
- **0 (default)** - Heuristic overcommit: Allow reasonable overcommit
- **1** - Always overcommit: Never fail malloc (dangerous)
- **2** - No overcommit: Limit to swap + (RAM × overcommit_ratio)
[Intermediate]

---

Q: What is `vm.overcommit_ratio`?
A: When `overcommit_memory=2`, this percentage of RAM (default 50%) plus swap is the hard commit limit. Example: 8GB RAM, 4GB swap, ratio=50% → limit = 4GB + 4GB = 8GB virtual memory total.
[Intermediate]

---

Q: (Code Interpretation) What does this OOM log message indicate?
```
Out of memory: Kill process 1234 (myapp) score 850 or sacrifice child
Killed process 1234 (myapp) total-vm:4096000kB, anon-rss:3800000kB
```
A: Process 1234 (myapp) was selected by OOM killer with score 850. It had 4GB virtual memory mapped and 3.8GB anonymous RSS (actual physical memory for heap/stack). OOM killed it to free ~3.8GB.
[Intermediate]

---

Q: What is memory cgroup OOM?
A: Containers/cgroups have memory limits. When a cgroup exceeds its limit, its own OOM killer runs (only considers processes in that cgroup). Prevents one container from affecting the whole system.
[Intermediate]

---

Q: (Reverse) This kernel thread performs background page reclaim when free memory is low.
A: Q: What is kswapd?
[Basic]

---

Q: What is page writeback?
A: Writing dirty pages to their backing store (file or swap). Can be triggered by:
- Periodic writeback (pdflush/writeback threads)
- Memory pressure (reclaim needs the page)
- fsync/sync system calls
Pages must be clean before they can be reclaimed.
[Basic]

---

Q: What is the difference between sync and async writeback?
A: Sync writeback blocks until the page is written (used by fsync). Async writeback queues the write and returns immediately. Reclaim uses async writeback to avoid blocking, but sync is needed for data integrity.
[Intermediate]

---

Q: (Understanding) Why is swapping anonymous pages expensive?
A: Unlike file pages (which can be discarded if clean), anonymous pages have no backing file. They must be written to swap on eviction AND read back from swap on fault. Both operations require disk I/O. File pages can often be simply discarded.
[Intermediate]

---

