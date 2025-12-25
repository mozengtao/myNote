# Advanced Systems Structures in C — Object Pools, Free Lists, Refcounting, Bitmaps

## 1. Object Pools

### Definition & Design Principles

```
+------------------------------------------------------------------+
|  OBJECT POOL: PRE-ALLOCATED REUSABLE OBJECTS                     |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  malloc/free for small, frequent allocations is:           │
    │  - Slow (syscall overhead)                                  │
    │  - Fragmentation-prone                                      │
    │  - Unpredictable latency                                    │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: OBJECT POOL
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Allocate many objects ONCE at startup                  │
    │  2. Hand out objects from pool (O(1))                      │
    │  3. Return to pool instead of freeing (O(1))               │
    │  4. All objects same size → no fragmentation               │
    └─────────────────────────────────────────────────────────────┘

    VISUAL:
    Pool (at startup):
    ┌───────────────────────────────────────────────────────────┐
    │ [obj1] [obj2] [obj3] [obj4] [obj5] [obj6] [obj7] [obj8]  │
    │   ↑                                                       │
    │  free_list                                                │
    └───────────────────────────────────────────────────────────┘

    After allocating 3 objects:
    ┌───────────────────────────────────────────────────────────┐
    │ [USED] [USED] [USED] [obj4] [obj5] [obj6] [obj7] [obj8]  │
    │                        ↑                                  │
    │                     free_list                             │
    └───────────────────────────────────────────────────────────┘
```

**中文解释：**
- **对象池**：预先分配一批相同大小的对象，重复使用
- 解决问题：malloc/free 慢、碎片化、延迟不可预测
- 所有对象大小相同 → 无碎片

### Complete Example: Object Pool

```c
/*
 * Object Pool Implementation
 * Pre-allocate fixed-size objects for O(1) alloc/free
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* Pool object must contain link for free list */
struct pool_object {
    union {
        struct pool_object *next;  /* When free: link to next free */
        char data[1];              /* When in use: user data starts here */
    };
};

struct object_pool {
    void *memory;                  /* Raw memory block */
    struct pool_object *free_list; /* Head of free list */
    size_t object_size;            /* Size of each object */
    size_t capacity;               /* Total objects */
    size_t allocated;              /* Currently allocated */
};

int pool_init(struct object_pool *pool, size_t object_size, size_t capacity)
{
    /* Ensure objects can hold the free list pointer */
    if (object_size < sizeof(struct pool_object *))
        object_size = sizeof(struct pool_object *);
    
    /* Align to pointer size */
    object_size = (object_size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
    
    pool->memory = malloc(object_size * capacity);
    if (!pool->memory)
        return -1;
    
    pool->object_size = object_size;
    pool->capacity = capacity;
    pool->allocated = 0;
    
    /* Build free list */
    pool->free_list = NULL;
    char *ptr = (char *)pool->memory;
    
    for (size_t i = 0; i < capacity; i++) {
        struct pool_object *obj = (struct pool_object *)(ptr + i * object_size);
        obj->next = pool->free_list;
        pool->free_list = obj;
    }
    
    return 0;
}

void *pool_alloc(struct object_pool *pool)
{
    if (!pool->free_list)
        return NULL;  /* Pool exhausted */
    
    struct pool_object *obj = pool->free_list;
    pool->free_list = obj->next;
    pool->allocated++;
    
    return obj;
}

void pool_free(struct object_pool *pool, void *ptr)
{
    if (!ptr)
        return;
    
    struct pool_object *obj = (struct pool_object *)ptr;
    obj->next = pool->free_list;
    pool->free_list = obj;
    pool->allocated--;
}

void pool_destroy(struct object_pool *pool)
{
    free(pool->memory);
    pool->memory = NULL;
    pool->free_list = NULL;
}

/* Example usage */
struct my_object {
    int id;
    char name[32];
    double value;
};

int main(void)
{
    printf("=== Object Pool Demo ===\n\n");
    
    struct object_pool pool;
    pool_init(&pool, sizeof(struct my_object), 100);
    
    printf("Pool: capacity=%zu, object_size=%zu\n",
           pool.capacity, pool.object_size);
    
    /* Allocate some objects */
    struct my_object *objs[10];
    for (int i = 0; i < 10; i++) {
        objs[i] = pool_alloc(&pool);
        objs[i]->id = i;
        snprintf(objs[i]->name, sizeof(objs[i]->name), "Object_%d", i);
        objs[i]->value = i * 1.5;
    }
    
    printf("After allocating 10: allocated=%zu\n", pool.allocated);
    
    /* Free some */
    pool_free(&pool, objs[3]);
    pool_free(&pool, objs[7]);
    printf("After freeing 2: allocated=%zu\n", pool.allocated);
    
    /* Reallocate */
    struct my_object *new_obj = pool_alloc(&pool);
    printf("Reallocated object at %p\n", (void *)new_obj);
    
    pool_destroy(&pool);
    return 0;
}
```

---

## 2. Free Lists

### Definition & Design Principles

```
+------------------------------------------------------------------+
|  FREE LIST: EMBEDDED LINKED LIST OF AVAILABLE SLOTS              |
+------------------------------------------------------------------+

    KEY INSIGHT:
    ┌─────────────────────────────────────────────────────────────┐
    │  When an object is FREE, its memory can store the link     │
    │  to the next free object (no extra memory needed!)         │
    └─────────────────────────────────────────────────────────────┘

    MEMORY REUSE:
    ┌───────────────────────────────────────────────────────────┐
    │                                                           │
    │  When ALLOCATED:           When FREE:                     │
    │  ┌───────────────┐         ┌───────────────┐              │
    │  │ user data     │         │ next ─────────┼──▶ (next)    │
    │  │ ...           │         │ (garbage)     │              │
    │  │ ...           │         │ (garbage)     │              │
    │  └───────────────┘         └───────────────┘              │
    │                                                           │
    │  Same memory, different interpretation!                   │
    └───────────────────────────────────────────────────────────┘

    LINUX KERNEL EXAMPLE:
    SLAB allocator uses free lists within each slab
    See: mm/slub.c
```

### Complete Example: Free List Allocator

```c
/*
 * Free List Allocator
 * Simple fixed-size allocator using embedded free list
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* Free slot contains pointer to next free slot */
union slot {
    union slot *next;
    char data[1];
};

struct free_list_allocator {
    char *arena;           /* Memory arena */
    union slot *free_head; /* Head of free list */
    size_t slot_size;      /* Size of each slot */
    size_t total_slots;    /* Total slots in arena */
    size_t free_slots;     /* Available slots */
};

int fla_init(struct free_list_allocator *fla, size_t slot_size, size_t count)
{
    /* Ensure slot can hold pointer */
    if (slot_size < sizeof(union slot *))
        slot_size = sizeof(union slot *);
    
    /* Align */
    slot_size = (slot_size + 7) & ~7;
    
    fla->arena = malloc(slot_size * count);
    if (!fla->arena)
        return -1;
    
    fla->slot_size = slot_size;
    fla->total_slots = count;
    fla->free_slots = count;
    
    /* Initialize free list */
    fla->free_head = (union slot *)fla->arena;
    union slot *current = fla->free_head;
    
    for (size_t i = 1; i < count; i++) {
        current->next = (union slot *)(fla->arena + i * slot_size);
        current = current->next;
    }
    current->next = NULL;
    
    return 0;
}

void *fla_alloc(struct free_list_allocator *fla)
{
    if (!fla->free_head)
        return NULL;
    
    union slot *slot = fla->free_head;
    fla->free_head = slot->next;
    fla->free_slots--;
    
    return slot;
}

void fla_free(struct free_list_allocator *fla, void *ptr)
{
    if (!ptr)
        return;
    
    union slot *slot = (union slot *)ptr;
    slot->next = fla->free_head;
    fla->free_head = slot;
    fla->free_slots++;
}

void fla_destroy(struct free_list_allocator *fla)
{
    free(fla->arena);
}

int main(void)
{
    printf("=== Free List Allocator Demo ===\n\n");
    
    struct free_list_allocator fla;
    fla_init(&fla, 64, 16);  /* 16 slots of 64 bytes */
    
    printf("Initial: %zu free slots\n", fla.free_slots);
    
    /* Allocate and use */
    char *buf1 = fla_alloc(&fla);
    strcpy(buf1, "Hello");
    
    char *buf2 = fla_alloc(&fla);
    strcpy(buf2, "World");
    
    printf("After 2 allocs: %zu free, buf1='%s', buf2='%s'\n",
           fla.free_slots, buf1, buf2);
    
    /* Free first */
    fla_free(&fla, buf1);
    printf("After freeing buf1: %zu free\n", fla.free_slots);
    
    /* Allocate again - should reuse buf1's slot */
    char *buf3 = fla_alloc(&fla);
    printf("buf3 address same as buf1? %s\n",
           buf3 == buf1 ? "YES (reused!)" : "NO");
    
    fla_destroy(&fla);
    return 0;
}
```

---

## 3. Reference-Counted Objects

### Definition & Design Principles

```
+------------------------------------------------------------------+
|  REFERENCE COUNTING: SHARED OWNERSHIP                            |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Multiple owners need access to same object                 │
    │  Who frees it? When?                                        │
    │  If freed too early → use-after-free                        │
    │  If never freed → memory leak                               │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: REFERENCE COUNTING
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Object contains a counter (refcount)                    │
    │  2. Take reference: increment counter                       │
    │  3. Release reference: decrement counter                    │
    │  4. When counter hits 0: free the object                    │
    └─────────────────────────────────────────────────────────────┘

    VISUAL:
    
    ┌──────────────┐
    │  Object      │
    │  refcount: 3 │◀──── Owner A
    │  data...     │◀──── Owner B
    │              │◀──── Owner C
    └──────────────┘

    Owner B releases:
    ┌──────────────┐
    │  Object      │
    │  refcount: 2 │◀──── Owner A
    │  data...     │
    │              │◀──── Owner C
    └──────────────┘

    All release → refcount: 0 → FREE!
```

**中文解释：**
- **引用计数**：对象包含计数器，跟踪有多少拥有者
- 获取引用：计数器加一
- 释放引用：计数器减一
- 计数器为零：释放对象

### Complete Example: Reference Counted Object

```c
/*
 * Reference Counted Object
 * Automatic cleanup when last reference released
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

/* Reference counted base structure */
struct refcounted {
    atomic_int refcount;
    void (*destroy)(struct refcounted *);
};

static inline void ref_init(struct refcounted *ref, 
                            void (*destroy)(struct refcounted *))
{
    atomic_store(&ref->refcount, 1);
    ref->destroy = destroy;
}

static inline void ref_get(struct refcounted *ref)
{
    atomic_fetch_add(&ref->refcount, 1);
}

static inline void ref_put(struct refcounted *ref)
{
    if (atomic_fetch_sub(&ref->refcount, 1) == 1) {
        /* Was 1, now 0 - destroy */
        if (ref->destroy)
            ref->destroy(ref);
    }
}

static inline int ref_count(struct refcounted *ref)
{
    return atomic_load(&ref->refcount);
}

/* ═══════════════════════════════════════════════════════════════
 * Example: Refcounted buffer
 * ═══════════════════════════════════════════════════════════════ */

struct buffer {
    struct refcounted ref;
    size_t size;
    char data[];  /* Flexible array member */
};

static void buffer_destroy(struct refcounted *ref)
{
    struct buffer *buf = (struct buffer *)ref;
    printf("  [Buffer destroyed, was size %zu]\n", buf->size);
    free(buf);
}

struct buffer *buffer_create(size_t size)
{
    struct buffer *buf = malloc(sizeof(*buf) + size);
    if (!buf)
        return NULL;
    
    ref_init(&buf->ref, buffer_destroy);
    buf->size = size;
    memset(buf->data, 0, size);
    
    return buf;
}

static inline void buffer_get(struct buffer *buf)
{
    ref_get(&buf->ref);
}

static inline void buffer_put(struct buffer *buf)
{
    ref_put(&buf->ref);
}

/* ═══════════════════════════════════════════════════════════════
 * Example: Shared buffer between components
 * ═══════════════════════════════════════════════════════════════ */

struct reader {
    struct buffer *buf;
    int id;
};

struct reader *reader_create(struct buffer *buf, int id)
{
    struct reader *r = malloc(sizeof(*r));
    if (!r)
        return NULL;
    
    r->buf = buf;
    r->id = id;
    buffer_get(buf);  /* Take a reference */
    
    printf("Reader %d created, buffer refcount: %d\n",
           id, ref_count(&buf->ref));
    
    return r;
}

void reader_destroy(struct reader *r)
{
    printf("Reader %d destroyed, ", r->id);
    buffer_put(r->buf);  /* Release reference */
    free(r);
}

int main(void)
{
    printf("=== Reference Counting Demo ===\n\n");
    
    /* Create shared buffer */
    struct buffer *buf = buffer_create(1024);
    strcpy(buf->data, "Shared data here");
    printf("Buffer created, refcount: %d\n\n", ref_count(&buf->ref));
    
    /* Create readers that share the buffer */
    struct reader *r1 = reader_create(buf, 1);
    struct reader *r2 = reader_create(buf, 2);
    struct reader *r3 = reader_create(buf, 3);
    
    /* Original creator releases */
    printf("\nOriginal owner releases:\n");
    buffer_put(buf);
    printf("  Buffer refcount: %d (still alive!)\n", ref_count(&buf->ref));
    
    /* Readers access data */
    printf("\nReaders access shared buffer:\n");
    printf("  Reader 1 sees: '%s'\n", r1->buf->data);
    
    /* Destroy readers one by one */
    printf("\nDestroying readers:\n");
    reader_destroy(r1);
    printf("  Buffer refcount: %d\n", ref_count(&buf->ref));
    
    reader_destroy(r2);
    printf("  Buffer refcount: %d\n", ref_count(&buf->ref));
    
    reader_destroy(r3);
    /* Buffer is now destroyed (refcount hit 0) */
    
    printf("\nNote: Buffer was freed automatically when last reader released!\n");
    
    return 0;
}
```

### Linux Kernel Style: kref

```c
/*
 * Linux Kernel Style kref
 * Simplified version of include/linux/kref.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>

struct kref {
    atomic_int refcount;
};

static inline void kref_init(struct kref *kref)
{
    atomic_store(&kref->refcount, 1);
}

static inline void kref_get(struct kref *kref)
{
    atomic_fetch_add(&kref->refcount, 1);
}

static inline int kref_put(struct kref *kref,
                           void (*release)(struct kref *kref))
{
    if (atomic_fetch_sub(&kref->refcount, 1) == 1) {
        release(kref);
        return 1;
    }
    return 0;
}

/* Usage example */
struct device {
    struct kref kref;
    char name[32];
};

#define to_device(k) ((struct device *)((char *)(k) - offsetof(struct device, kref)))

void device_release(struct kref *kref)
{
    struct device *dev = to_device(kref);
    printf("Device '%s' released\n", dev->name);
    free(dev);
}

struct device *device_create(const char *name)
{
    struct device *dev = malloc(sizeof(*dev));
    kref_init(&dev->kref);
    snprintf(dev->name, sizeof(dev->name), "%s", name);
    return dev;
}

void device_get(struct device *dev)
{
    kref_get(&dev->kref);
}

void device_put(struct device *dev)
{
    kref_put(&dev->kref, device_release);
}
```

---

## 4. Bitmaps / Bitsets

### Definition & Design Principles

```
+------------------------------------------------------------------+
|  BITMAP: COMPACT BOOLEAN ARRAY                                   |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Need to track many true/false flags efficiently:           │
    │  - Which pages are free?                                    │
    │  - Which file descriptors are open?                         │
    │  - Which CPUs are online?                                   │
    │  - Which blocks are allocated?                              │
    └─────────────────────────────────────────────────────────────┘

    NAIVE APPROACH:
    bool flags[1000000];  // 1 MB for 1M flags!

    BITMAP APPROACH:
    unsigned long flags[1000000 / 64];  // ~15 KB for 1M flags!
    
    MEMORY SAVINGS: 8× (1 bit vs 1 byte)

    VISUAL (32 flags in one word):
    ┌────────────────────────────────────────────────────────┐
    │ 0 1 1 0 0 1 0 1 │ 1 0 0 0 1 1 0 0 │ ... │ 0 1 0 1 │    │
    │ bit 0           │ bit 8           │     │ bit 28  │    │
    └────────────────────────────────────────────────────────┘
       Flags 0-7          8-15            ...    28-31
```

**中文解释：**
- **位图**：用一个比特表示一个布尔值，节省内存
- 用途：内存页管理、文件描述符、CPU 掩码、块分配
- 内存节省：8 倍（1 位 vs 1 字节）

### Complete Example: Bitmap

```c
/*
 * Bitmap Implementation
 * Compact storage for boolean flags
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define BITS_PER_LONG (sizeof(unsigned long) * 8)

struct bitmap {
    unsigned long *bits;
    size_t nbits;
    size_t nwords;
};

/* Calculate number of words needed */
static inline size_t bits_to_words(size_t nbits)
{
    return (nbits + BITS_PER_LONG - 1) / BITS_PER_LONG;
}

/* Allocate bitmap */
int bitmap_alloc(struct bitmap *bm, size_t nbits)
{
    bm->nbits = nbits;
    bm->nwords = bits_to_words(nbits);
    bm->bits = calloc(bm->nwords, sizeof(unsigned long));
    return bm->bits ? 0 : -1;
}

void bitmap_free(struct bitmap *bm)
{
    free(bm->bits);
}

/* Set bit */
static inline void bitmap_set(struct bitmap *bm, size_t bit)
{
    if (bit < bm->nbits)
        bm->bits[bit / BITS_PER_LONG] |= (1UL << (bit % BITS_PER_LONG));
}

/* Clear bit */
static inline void bitmap_clear(struct bitmap *bm, size_t bit)
{
    if (bit < bm->nbits)
        bm->bits[bit / BITS_PER_LONG] &= ~(1UL << (bit % BITS_PER_LONG));
}

/* Test bit */
static inline bool bitmap_test(const struct bitmap *bm, size_t bit)
{
    if (bit >= bm->nbits)
        return false;
    return (bm->bits[bit / BITS_PER_LONG] & (1UL << (bit % BITS_PER_LONG))) != 0;
}

/* Toggle bit */
static inline void bitmap_toggle(struct bitmap *bm, size_t bit)
{
    if (bit < bm->nbits)
        bm->bits[bit / BITS_PER_LONG] ^= (1UL << (bit % BITS_PER_LONG));
}

/* Set all bits */
void bitmap_set_all(struct bitmap *bm)
{
    memset(bm->bits, 0xFF, bm->nwords * sizeof(unsigned long));
}

/* Clear all bits */
void bitmap_clear_all(struct bitmap *bm)
{
    memset(bm->bits, 0, bm->nwords * sizeof(unsigned long));
}

/* Count set bits (popcount) */
size_t bitmap_count(const struct bitmap *bm)
{
    size_t count = 0;
    for (size_t i = 0; i < bm->nwords; i++) {
        unsigned long word = bm->bits[i];
        while (word) {
            count += word & 1;
            word >>= 1;
        }
    }
    return count;
}

/* Find first set bit (returns -1 if none) */
ssize_t bitmap_find_first_set(const struct bitmap *bm)
{
    for (size_t i = 0; i < bm->nwords; i++) {
        if (bm->bits[i]) {
            unsigned long word = bm->bits[i];
            size_t bit = i * BITS_PER_LONG;
            while (!(word & 1)) {
                word >>= 1;
                bit++;
            }
            return (bit < bm->nbits) ? (ssize_t)bit : -1;
        }
    }
    return -1;
}

/* Find first zero bit */
ssize_t bitmap_find_first_zero(const struct bitmap *bm)
{
    for (size_t i = 0; i < bm->nwords; i++) {
        if (bm->bits[i] != ~0UL) {
            unsigned long word = bm->bits[i];
            size_t bit = i * BITS_PER_LONG;
            while (word & 1) {
                word >>= 1;
                bit++;
            }
            return (bit < bm->nbits) ? (ssize_t)bit : -1;
        }
    }
    return -1;
}

/* Print bitmap */
void bitmap_print(const struct bitmap *bm, size_t max_bits)
{
    size_t n = (max_bits < bm->nbits) ? max_bits : bm->nbits;
    printf("[");
    for (size_t i = 0; i < n; i++) {
        printf("%c", bitmap_test(bm, i) ? '1' : '0');
        if ((i + 1) % 8 == 0 && i + 1 < n)
            printf(" ");
    }
    if (n < bm->nbits)
        printf(" ...");
    printf("] (%zu bits)\n", bm->nbits);
}

int main(void)
{
    printf("=== Bitmap Demo ===\n\n");
    
    struct bitmap bm;
    bitmap_alloc(&bm, 64);
    
    printf("Empty bitmap: ");
    bitmap_print(&bm, 32);
    
    /* Set some bits */
    bitmap_set(&bm, 0);
    bitmap_set(&bm, 5);
    bitmap_set(&bm, 10);
    bitmap_set(&bm, 15);
    bitmap_set(&bm, 31);
    
    printf("After setting 0,5,10,15,31: ");
    bitmap_print(&bm, 32);
    
    printf("Bit 5 is %s\n", bitmap_test(&bm, 5) ? "SET" : "CLEAR");
    printf("Bit 6 is %s\n", bitmap_test(&bm, 6) ? "SET" : "CLEAR");
    
    printf("Count of set bits: %zu\n", bitmap_count(&bm));
    printf("First set bit: %zd\n", bitmap_find_first_set(&bm));
    printf("First zero bit: %zd\n", bitmap_find_first_zero(&bm));
    
    /* Toggle */
    printf("\nToggling bit 5:\n");
    bitmap_toggle(&bm, 5);
    printf("After toggle: ");
    bitmap_print(&bm, 32);
    printf("Bit 5 is now %s\n", bitmap_test(&bm, 5) ? "SET" : "CLEAR");
    
    /* Memory usage comparison */
    printf("\nMemory usage comparison for 1M flags:\n");
    printf("  bool array: %zu bytes\n", 1000000 * sizeof(bool));
    printf("  Bitmap: %zu bytes\n", bits_to_words(1000000) * sizeof(unsigned long));
    printf("  Savings: %.1f×\n",
           (double)(1000000 * sizeof(bool)) / (bits_to_words(1000000) * sizeof(unsigned long)));
    
    bitmap_free(&bm);
    return 0;
}
```

### Linux Kernel Bitmap Operations

```c
/*
 * Common Linux Kernel Bitmap Patterns
 * From include/linux/bitmap.h
 */

/* Iterate over set bits */
#define for_each_set_bit(bit, addr, size) \
    for ((bit) = find_first_bit((addr), (size));        \
         (bit) < (size);                                 \
         (bit) = find_next_bit((addr), (size), (bit) + 1))

/* CPU mask example */
void show_online_cpus(void)
{
    int cpu;
    /* In kernel: for_each_online_cpu(cpu) */
    for_each_set_bit(cpu, cpu_online_mask, nr_cpu_ids) {
        printk("CPU %d is online\n", cpu);
    }
}

/* Page allocation bitmap */
struct zone {
    unsigned long *pageblock_flags;  /* Bitmap of page states */
    unsigned long free_pages;
};

/* File descriptor bitmap */
struct fd_set {
    unsigned long fds_bits[FD_SETSIZE / BITS_PER_LONG];
};
```

---

## 5. Combining Patterns: Slab Allocator Concept

```c
/*
 * Combining All Patterns: Mini Slab Allocator
 * Demonstrates how kernel allocators use these building blocks
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SLAB_SIZE 4096
#define OBJECTS_PER_SLAB 64

struct slab {
    void *memory;                    /* Object pool memory */
    unsigned long free_bitmap;       /* Which slots are free (bitmap) */
    struct slab *next;               /* Free list of slabs */
    unsigned int free_count;         /* Number of free objects */
};

struct kmem_cache {
    const char *name;
    size_t object_size;
    struct slab *slabs_partial;      /* Slabs with some free */
    struct slab *slabs_full;         /* Slabs completely used */
    struct slab *slabs_free;         /* Slabs completely free */
    unsigned int active_objs;
    unsigned int total_objs;
};

/* Create a slab */
struct slab *slab_create(size_t obj_size)
{
    struct slab *slab = malloc(sizeof(*slab));
    slab->memory = aligned_alloc(64, OBJECTS_PER_SLAB * obj_size);
    slab->free_bitmap = ~0UL;  /* All bits set = all free */
    slab->next = NULL;
    slab->free_count = OBJECTS_PER_SLAB;
    return slab;
}

/* Find first free slot in bitmap */
int find_free_slot(unsigned long bitmap)
{
    for (int i = 0; i < 64; i++) {
        if (bitmap & (1UL << i))
            return i;
    }
    return -1;
}

/* Allocate object from cache */
void *kmem_cache_alloc(struct kmem_cache *cache)
{
    struct slab *slab = cache->slabs_partial;
    
    /* No partial slab, try free slab */
    if (!slab) {
        slab = cache->slabs_free;
        if (slab) {
            cache->slabs_free = slab->next;
            slab->next = cache->slabs_partial;
            cache->slabs_partial = slab;
        } else {
            /* Create new slab */
            slab = slab_create(cache->object_size);
            slab->next = cache->slabs_partial;
            cache->slabs_partial = slab;
            cache->total_objs += OBJECTS_PER_SLAB;
        }
    }
    
    /* Find free slot using bitmap */
    int slot = find_free_slot(slab->free_bitmap);
    if (slot < 0)
        return NULL;
    
    /* Mark slot as used */
    slab->free_bitmap &= ~(1UL << slot);
    slab->free_count--;
    cache->active_objs++;
    
    /* Move to full list if needed */
    if (slab->free_count == 0) {
        cache->slabs_partial = slab->next;
        slab->next = cache->slabs_full;
        cache->slabs_full = slab;
    }
    
    return (char *)slab->memory + slot * cache->object_size;
}

void kmem_cache_stats(struct kmem_cache *cache)
{
    printf("Cache '%s': %u/%u objects active\n",
           cache->name, cache->active_objs, cache->total_objs);
}

int main(void)
{
    printf("=== Mini Slab Allocator Demo ===\n\n");
    
    struct kmem_cache cache = {
        .name = "my_objects",
        .object_size = 128,
        .slabs_partial = NULL,
        .slabs_full = NULL,
        .slabs_free = NULL,
        .active_objs = 0,
        .total_objs = 0
    };
    
    printf("Allocating 100 objects:\n");
    void *objs[100];
    for (int i = 0; i < 100; i++) {
        objs[i] = kmem_cache_alloc(&cache);
    }
    kmem_cache_stats(&cache);
    
    printf("\nThis demo shows how kernel allocators combine:\n");
    printf("  - Object pools (pre-allocated slabs)\n");
    printf("  - Free lists (slab linked lists)\n");
    printf("  - Bitmaps (tracking free slots within slab)\n");
    
    return 0;
}
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|  ADVANCED STRUCTURES: KEY TAKEAWAYS                              |
+------------------------------------------------------------------+

    OBJECT POOLS:
    - Pre-allocate fixed-size objects
    - O(1) alloc/free, no fragmentation
    - Used: game engines, embedded, high-frequency trading

    FREE LISTS:
    - Embed links in free slots (zero extra memory)
    - Foundation of many allocators
    - Used: slab/slub, custom allocators

    REFERENCE COUNTING:
    - Shared ownership with automatic cleanup
    - Atomic operations for thread safety
    - Used: Linux kref, file systems, network buffers

    BITMAPS:
    - 8× memory savings vs bool arrays
    - Fast set operations with bitwise ops
    - Used: page tables, FD sets, CPU masks

    LINUX KERNEL EXAMPLES:
    - mm/slub.c (slab allocator)
    - include/linux/kref.h (reference counting)
    - include/linux/bitmap.h (bitmap operations)
    - include/linux/cpumask.h (CPU bitmaps)
```

**中文总结：**
- **对象池**：预分配固定大小对象，O(1) 分配/释放
- **自由链表**：在空闲槽位嵌入链接，零额外内存
- **引用计数**：共享所有权，自动清理
- **位图**：8 倍内存节省，快速集合操作

