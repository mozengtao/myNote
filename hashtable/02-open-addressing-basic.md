# Hash Table Implementation Guide - Part 2: Open Addressing Basics

## 3. Open Addressing - Linear Probing

### 1. How It Works

In open addressing, all entries are stored directly in the bucket array itself - no linked lists, no separate allocations. When a collision occurs, we probe for the next available slot using a deterministic sequence.

Linear probing uses the simplest probe sequence: check slot `h`, then `h+1`, `h+2`, etc., wrapping around.

```
Linear Probing Example:
Initial state (size=8):
+---+---+---+---+---+---+---+---+
| - | - | - | - | - | - | - | - |
+---+---+---+---+---+---+---+---+
  0   1   2   3   4   5   6   7

Insert "apple" -> hash=2:
+---+---+---+---+---+---+---+---+
| - | - | A | - | - | - | - | - |
+---+---+---+---+---+---+---+---+
          ^

Insert "banana" -> hash=2 (collision!), probe to 3:
+---+---+---+---+---+---+---+---+
| - | - | A | B | - | - | - | - |
+---+---+---+---+---+---+---+---+
          ^   ^
          |   +-- banana lands here
          +-- collision at apple

Insert "cherry" -> hash=3 (collision!), probe to 4:
+---+---+---+---+---+---+---+---+
| - | - | A | B | C | - | - | - |
+---+---+---+---+---+---+---+---+
              ^   ^
              |   +-- cherry lands here
              +-- collision

Cluster forms: positions 2,3,4 are contiguous
```

线性探测内存布局说明：
- 所有元素直接存储在连续的桶数组中
- 发生冲突时，检查下一个位置（索引+1）
- 形成聚类（cluster）：连续占用的槽位
- 查找时需要扫描整个聚类直到找到目标或空槽
- 删除复杂：不能简单清空，需要用墓碑标记或重新插入后续元素

**Lookup path:**
1. Compute `idx = hash % size`
2. If slot[idx] matches key, return
3. If slot[idx] is empty, key doesn't exist
4. Otherwise, check idx+1, idx+2, ... (wrapping)

**Insert path:**
1. Probe until empty slot or matching key found
2. Insert at empty slot, or update existing

**Delete path (with tombstones):**
1. Find entry
2. Mark as "deleted" (tombstone), don't clear
3. Tombstones allow probes to continue past deleted entries
4. Tombstones are reused during insert

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) with low load factor |
| Worst-case lookup | O(n) when table nearly full or bad clustering |
| Space overhead | Fixed overhead per slot (key, value, state flag) |
| Cache friendliness | **Excellent** - sequential memory access |
| High load factor behavior | Performance degrades rapidly above 70% |
| Resize complexity | O(n) - must rehash, no tombstones in new table |

---

### 3. Pros and Cons

#### Pros
- **Best cache behavior** - sequential memory reads
- No pointer chasing
- No per-entry allocation
- Simple implementation
- Predictable memory layout

#### Cons
- **Primary clustering** - collisions create runs that grow quadratically
- Tombstones accumulate, degrading performance
- Must resize before table gets too full
- Load factor limited to ~70% for good performance
- Deletion is complex

**Implementation Risks:**
- Forgetting to handle tombstones during lookup
- Infinite loop if table is full and no empty/tombstone found
- Not resizing when load factor gets high

---

### 4. When to Use

**Good fit when:**
- Keys are small (fit in cache line with entry)
- Read-heavy workload
- Load factor can be kept below 0.7
- Deletions are rare
- Cache performance is critical

**Workload patterns:**
- Lookup-dominated
- Insert once, read many times
- Known maximum size

---

### 5. Real-World Use Cases

- **Python dict** (CPython) - uses open addressing with pseudo-random probing
- **Rust HashMap** - uses SwissTable (open addressing variant)
- **Go map** - uses open addressing with buckets
- **Redis dict** - incremental rehashing with open addressing
- **Embedded systems** - when malloc is unavailable or expensive

---

### 6. Complete Userspace C Example

```c
/*
 * linear_probing.c
 * Compile: gcc -Wall -Wextra -O2 -o linear_probing linear_probing.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INITIAL_SIZE 16
#define MAX_LOAD_FACTOR 0.7

/* Slot states - crucial for correct tombstone handling */
typedef enum {
    SLOT_EMPTY,      /* Never used - terminates probe */
    SLOT_OCCUPIED,   /* Contains valid entry */
    SLOT_DELETED     /* Tombstone - probe continues past, can reuse for insert */
} slot_state;

struct ht_entry {
    char *key;       /* NULL if empty/deleted, else owned string */
    int value;
    slot_state state;
};

struct hashtable {
    struct ht_entry *entries;
    size_t size;     /* Total slots */
    size_t count;    /* Occupied slots (not counting tombstones) */
    size_t tombs;    /* Tombstone count - for resize decisions */
};

static uint32_t hash_string(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

struct hashtable *ht_create(size_t size)
{
    struct hashtable *ht = malloc(sizeof(*ht));
    if (!ht)
        return NULL;
    
    ht->entries = calloc(size, sizeof(struct ht_entry));
    if (!ht->entries) {
        free(ht);
        return NULL;
    }
    
    /* calloc zeroes memory, but be explicit about state */
    for (size_t i = 0; i < size; i++)
        ht->entries[i].state = SLOT_EMPTY;
    
    ht->size = size;
    ht->count = 0;
    ht->tombs = 0;
    return ht;
}

static int ht_resize(struct hashtable *ht, size_t new_size)
{
    struct ht_entry *old_entries = ht->entries;
    size_t old_size = ht->size;
    
    ht->entries = calloc(new_size, sizeof(struct ht_entry));
    if (!ht->entries) {
        ht->entries = old_entries;
        return -1;
    }
    
    for (size_t i = 0; i < new_size; i++)
        ht->entries[i].state = SLOT_EMPTY;
    
    ht->size = new_size;
    ht->count = 0;
    ht->tombs = 0;  /* Resize eliminates all tombstones */
    
    /* Rehash all valid entries */
    for (size_t i = 0; i < old_size; i++) {
        if (old_entries[i].state == SLOT_OCCUPIED) {
            /* Re-insert: find new slot with linear probe */
            uint32_t idx = hash_string(old_entries[i].key) % new_size;
            
            while (ht->entries[idx].state == SLOT_OCCUPIED)
                idx = (idx + 1) % new_size;
            
            ht->entries[idx].key = old_entries[i].key;  /* Transfer ownership */
            ht->entries[idx].value = old_entries[i].value;
            ht->entries[idx].state = SLOT_OCCUPIED;
            ht->count++;
        }
    }
    
    free(old_entries);
    return 0;
}

int ht_insert(struct hashtable *ht, const char *key, int value)
{
    /* Resize if load factor too high (count + tombstones affect probing) */
    if ((double)(ht->count + ht->tombs) / ht->size > MAX_LOAD_FACTOR) {
        if (ht_resize(ht, ht->size * 2) < 0)
            return -1;
    }
    
    uint32_t idx = hash_string(key) % ht->size;
    size_t first_tomb = SIZE_MAX;  /* Track first tombstone for reuse */
    
    /* 
     * Linear probe: check consecutive slots.
     * Cache-friendly because we access sequential memory addresses.
     */
    while (ht->entries[idx].state != SLOT_EMPTY) {
        if (ht->entries[idx].state == SLOT_DELETED) {
            /* Remember first tombstone - we might insert here */
            if (first_tomb == SIZE_MAX)
                first_tomb = idx;
        } else if (strcmp(ht->entries[idx].key, key) == 0) {
            /* Key exists - update value */
            ht->entries[idx].value = value;
            return 0;
        }
        
        idx = (idx + 1) % ht->size;
    }
    
    /* Key not found - insert at first tombstone or empty slot */
    size_t insert_idx = (first_tomb != SIZE_MAX) ? first_tomb : idx;
    
    if (ht->entries[insert_idx].state == SLOT_DELETED)
        ht->tombs--;  /* Reusing tombstone */
    
    ht->entries[insert_idx].key = strdup(key);
    if (!ht->entries[insert_idx].key)
        return -1;
    
    ht->entries[insert_idx].value = value;
    ht->entries[insert_idx].state = SLOT_OCCUPIED;
    ht->count++;
    
    return 0;
}

int *ht_search(struct hashtable *ht, const char *key)
{
    uint32_t idx = hash_string(key) % ht->size;
    
    /*
     * Probe until EMPTY (not found) or match.
     * DELETED slots don't terminate search - the key might be after them.
     */
    while (ht->entries[idx].state != SLOT_EMPTY) {
        if (ht->entries[idx].state == SLOT_OCCUPIED &&
            strcmp(ht->entries[idx].key, key) == 0) {
            return &ht->entries[idx].value;
        }
        idx = (idx + 1) % ht->size;
    }
    
    return NULL;
}

int ht_delete(struct hashtable *ht, const char *key)
{
    uint32_t idx = hash_string(key) % ht->size;
    
    while (ht->entries[idx].state != SLOT_EMPTY) {
        if (ht->entries[idx].state == SLOT_OCCUPIED &&
            strcmp(ht->entries[idx].key, key) == 0) {
            /* 
             * Mark as tombstone instead of empty.
             * Empty would break probe sequences for other keys
             * that collided past this slot.
             */
            free(ht->entries[idx].key);
            ht->entries[idx].key = NULL;
            ht->entries[idx].state = SLOT_DELETED;
            ht->count--;
            ht->tombs++;
            return 0;
        }
        idx = (idx + 1) % ht->size;
    }
    
    return -1;  /* Key not found */
}

void ht_destroy(struct hashtable *ht)
{
    if (!ht)
        return;
    
    for (size_t i = 0; i < ht->size; i++) {
        if (ht->entries[i].state == SLOT_OCCUPIED)
            free(ht->entries[i].key);
    }
    
    free(ht->entries);
    free(ht);
}

void ht_stats(struct hashtable *ht)
{
    printf("Size: %zu, Count: %zu, Tombstones: %zu, Load: %.2f\n",
           ht->size, ht->count, ht->tombs,
           (double)(ht->count + ht->tombs) / ht->size);
    
    /* Count cluster lengths */
    size_t max_run = 0, current_run = 0;
    for (size_t i = 0; i < ht->size; i++) {
        if (ht->entries[i].state != SLOT_EMPTY) {
            current_run++;
            if (current_run > max_run)
                max_run = current_run;
        } else {
            current_run = 0;
        }
    }
    printf("Max cluster length: %zu\n", max_run);
}

int main(void)
{
    struct hashtable *ht = ht_create(INITIAL_SIZE);
    if (!ht) {
        fprintf(stderr, "Failed to create hash table\n");
        return 1;
    }
    
    /* Insert entries */
    const char *keys[] = {"apple", "banana", "cherry", "date", "elderberry",
                          "fig", "grape", "honeydew", "kiwi", "lemon"};
    
    for (int i = 0; i < 10; i++) {
        if (ht_insert(ht, keys[i], i * 10) < 0) {
            fprintf(stderr, "Insert failed\n");
            return 1;
        }
    }
    
    printf("After inserting 10 items:\n");
    ht_stats(ht);
    
    /* Lookup test */
    printf("\nLookup tests:\n");
    for (int i = 0; i < 10; i++) {
        int *val = ht_search(ht, keys[i]);
        printf("  %s -> %d\n", keys[i], val ? *val : -1);
    }
    
    /* Delete some entries */
    printf("\nDeleting 'banana', 'date', 'fig'...\n");
    ht_delete(ht, "banana");
    ht_delete(ht, "date");
    ht_delete(ht, "fig");
    
    printf("After deletions:\n");
    ht_stats(ht);
    
    /* Verify lookups still work (tombstones don't break search) */
    printf("\nVerifying remaining entries:\n");
    for (int i = 0; i < 10; i++) {
        int *val = ht_search(ht, keys[i]);
        if (val)
            printf("  %s -> %d (found)\n", keys[i], *val);
    }
    
    /* Insert more to test tombstone reuse */
    printf("\nInserting 'mango', 'nectarine'...\n");
    ht_insert(ht, "mango", 500);
    ht_insert(ht, "nectarine", 600);
    
    printf("After new insertions:\n");
    ht_stats(ht);
    
    ht_destroy(ht);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why linear probing is appropriate here:**
- Simple string key comparison
- Demonstrates clustering behavior with stats
- Shows tombstone handling clearly
- Cache-friendly sequential access pattern

**What would go wrong with separate chaining:**
- More memory allocations
- Pointer chasing on every lookup
- Worse cache performance for small entries

---

## 4. Open Addressing - Quadratic Probing

### 1. How It Works

Quadratic probing uses a non-linear probe sequence to reduce primary clustering. Instead of checking `h, h+1, h+2, ...`, it checks `h, h+1, h+4, h+9, ...` (i.e., `h + i^2`).

```
Quadratic vs Linear Probing:
Linear:    h, h+1, h+2, h+3, h+4, h+5...
Quadratic: h, h+1, h+4, h+9, h+16, h+25...

Example with size=16, h=3:
Linear sequence:    3, 4, 5, 6, 7, 8, 9, 10...
Quadratic sequence: 3, 4, 7, 12, 3, 12, 7, 4...  (wraps around)

                    Linear Probing:
+---+---+---+---+---+---+---+---+
| - | - | - | X | X | X | X | - |  <- cluster grows contiguously
+---+---+---+---+---+---+---+---+
              ^^^^^^^^^^^^^^^^^
              Primary cluster

                    Quadratic Probing:
+---+---+---+---+---+---+---+---+
| - | - | - | X | X | - | - | X |  <- entries spread out
+---+---+---+---+---+---+---+---+
              ^   ^           ^
              Collisions jump apart
```

二次探测说明：
- 探测序列：h, h+1, h+4, h+9, h+16... (h + i²)
- 避免了线性探测的主聚类问题
- 但可能产生次级聚类：相同哈希值的键使用相同探测序列
- 表大小必须为素数或2的幂（配合特定探测公式）以保证覆盖所有槽位

**Important constraint:** Table size must be prime OR power of 2 with specific probe formula to guarantee all slots are visited.

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) |
| Worst-case lookup | O(n) |
| Space overhead | Same as linear probing |
| Cache friendliness | Worse than linear - jumps around memory |
| High load factor behavior | Better than linear, still degrades |
| Resize complexity | O(n) |

---

### 3. Pros and Cons

#### Pros
- No primary clustering
- Better distribution than linear probing
- Same memory layout as linear

#### Cons
- **Secondary clustering** - keys with same hash follow same probe sequence
- Cache unfriendly - non-sequential access
- Must ensure probe sequence covers all slots
- Slightly more complex calculation

---

### 4. When to Use

**Good fit when:**
- Primary clustering from linear probing is causing issues
- Load factor between 0.5 and 0.7
- Not extremely cache-sensitive

---

### 5. Real-World Use Cases

- **Java's IdentityHashMap** - uses a variation
- **Educational implementations** - common in textbooks
- **Less common in production** - Robin Hood often preferred

---

### 6. Complete Userspace C Example

```c
/*
 * quadratic_probing.c
 * Compile: gcc -Wall -Wextra -O2 -o quadratic_probing quadratic_probing.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INITIAL_SIZE 17  /* Prime number for better coverage */
#define MAX_LOAD_FACTOR 0.5  /* Lower threshold for quadratic probing */

typedef enum { SLOT_EMPTY, SLOT_OCCUPIED, SLOT_DELETED } slot_state;

struct ht_entry {
    char *key;
    int value;
    slot_state state;
};

struct hashtable {
    struct ht_entry *entries;
    size_t size;
    size_t count;
};

static uint32_t hash_string(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/* Find next prime >= n (simple implementation) */
static size_t next_prime(size_t n)
{
    if (n <= 2) return 2;
    if (n % 2 == 0) n++;
    
    while (1) {
        int is_prime = 1;
        for (size_t i = 3; i * i <= n; i += 2) {
            if (n % i == 0) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime) return n;
        n += 2;
    }
}

struct hashtable *ht_create(size_t size)
{
    struct hashtable *ht = malloc(sizeof(*ht));
    if (!ht) return NULL;
    
    /* Use prime size for better quadratic probing coverage */
    size = next_prime(size);
    
    ht->entries = calloc(size, sizeof(struct ht_entry));
    if (!ht->entries) {
        free(ht);
        return NULL;
    }
    
    for (size_t i = 0; i < size; i++)
        ht->entries[i].state = SLOT_EMPTY;
    
    ht->size = size;
    ht->count = 0;
    return ht;
}

/*
 * Quadratic probe: index = (h + i^2) % size
 * With prime table size and load factor < 0.5, 
 * we're guaranteed to find an empty slot in size/2 probes.
 */
static size_t quadratic_probe(size_t h, size_t i, size_t size)
{
    return (h + i * i) % size;
}

static int ht_resize(struct hashtable *ht, size_t new_size)
{
    new_size = next_prime(new_size);
    
    struct ht_entry *old_entries = ht->entries;
    size_t old_size = ht->size;
    
    ht->entries = calloc(new_size, sizeof(struct ht_entry));
    if (!ht->entries) {
        ht->entries = old_entries;
        return -1;
    }
    
    for (size_t j = 0; j < new_size; j++)
        ht->entries[j].state = SLOT_EMPTY;
    
    ht->size = new_size;
    ht->count = 0;
    
    for (size_t j = 0; j < old_size; j++) {
        if (old_entries[j].state == SLOT_OCCUPIED) {
            uint32_t h = hash_string(old_entries[j].key) % new_size;
            size_t i = 0;
            size_t idx;
            
            /* Quadratic probe to find slot */
            while (1) {
                idx = quadratic_probe(h, i, new_size);
                if (ht->entries[idx].state != SLOT_OCCUPIED)
                    break;
                i++;
            }
            
            ht->entries[idx].key = old_entries[j].key;
            ht->entries[idx].value = old_entries[j].value;
            ht->entries[idx].state = SLOT_OCCUPIED;
            ht->count++;
        }
    }
    
    free(old_entries);
    return 0;
}

int ht_insert(struct hashtable *ht, const char *key, int value)
{
    if ((double)ht->count / ht->size > MAX_LOAD_FACTOR) {
        if (ht_resize(ht, ht->size * 2) < 0)
            return -1;
    }
    
    uint32_t h = hash_string(key) % ht->size;
    size_t i = 0;
    size_t first_tomb = SIZE_MAX;
    
    /*
     * Quadratic probe sequence: h, h+1, h+4, h+9, h+16...
     * Spreads collisions more evenly than linear.
     */
    while (1) {
        size_t idx = quadratic_probe(h, i, ht->size);
        
        if (ht->entries[idx].state == SLOT_EMPTY) {
            size_t ins_idx = (first_tomb != SIZE_MAX) ? first_tomb : idx;
            ht->entries[ins_idx].key = strdup(key);
            if (!ht->entries[ins_idx].key) return -1;
            ht->entries[ins_idx].value = value;
            ht->entries[ins_idx].state = SLOT_OCCUPIED;
            ht->count++;
            return 0;
        }
        
        if (ht->entries[idx].state == SLOT_DELETED) {
            if (first_tomb == SIZE_MAX)
                first_tomb = idx;
        } else if (strcmp(ht->entries[idx].key, key) == 0) {
            ht->entries[idx].value = value;
            return 0;
        }
        
        i++;
        
        /* Safety check - shouldn't happen with proper load factor */
        if (i >= ht->size) {
            if (ht_resize(ht, ht->size * 2) < 0)
                return -1;
            return ht_insert(ht, key, value);  /* Retry after resize */
        }
    }
}

int *ht_search(struct hashtable *ht, const char *key)
{
    uint32_t h = hash_string(key) % ht->size;
    size_t i = 0;
    
    while (i < ht->size) {
        size_t idx = quadratic_probe(h, i, ht->size);
        
        if (ht->entries[idx].state == SLOT_EMPTY)
            return NULL;
        
        if (ht->entries[idx].state == SLOT_OCCUPIED &&
            strcmp(ht->entries[idx].key, key) == 0) {
            return &ht->entries[idx].value;
        }
        
        i++;
    }
    
    return NULL;
}

int ht_delete(struct hashtable *ht, const char *key)
{
    uint32_t h = hash_string(key) % ht->size;
    size_t i = 0;
    
    while (i < ht->size) {
        size_t idx = quadratic_probe(h, i, ht->size);
        
        if (ht->entries[idx].state == SLOT_EMPTY)
            return -1;
        
        if (ht->entries[idx].state == SLOT_OCCUPIED &&
            strcmp(ht->entries[idx].key, key) == 0) {
            free(ht->entries[idx].key);
            ht->entries[idx].key = NULL;
            ht->entries[idx].state = SLOT_DELETED;
            ht->count--;
            return 0;
        }
        
        i++;
    }
    
    return -1;
}

void ht_destroy(struct hashtable *ht)
{
    if (!ht) return;
    
    for (size_t i = 0; i < ht->size; i++) {
        if (ht->entries[i].state == SLOT_OCCUPIED)
            free(ht->entries[i].key);
    }
    
    free(ht->entries);
    free(ht);
}

int main(void)
{
    struct hashtable *ht = ht_create(INITIAL_SIZE);
    if (!ht) {
        fprintf(stderr, "Failed to create hash table\n");
        return 1;
    }
    
    printf("Table size (prime): %zu\n", ht->size);
    
    const char *keys[] = {"alpha", "beta", "gamma", "delta", "epsilon",
                          "zeta", "eta", "theta", "iota", "kappa"};
    
    for (int i = 0; i < 10; i++) {
        ht_insert(ht, keys[i], i * 100);
    }
    
    printf("Inserted 10 items, count: %zu, load: %.2f\n",
           ht->count, (double)ht->count / ht->size);
    
    printf("\nLookup tests:\n");
    for (int i = 0; i < 10; i++) {
        int *val = ht_search(ht, keys[i]);
        printf("  %s -> %d\n", keys[i], val ? *val : -1);
    }
    
    printf("\nDeleting 'gamma' and 'theta'...\n");
    ht_delete(ht, "gamma");
    ht_delete(ht, "theta");
    
    int *val = ht_search(ht, "gamma");
    printf("gamma after delete: %s\n", val ? "found (error)" : "not found (ok)");
    
    val = ht_search(ht, "delta");
    printf("delta (should exist): %s\n", val ? "found (ok)" : "not found (error)");
    
    ht_destroy(ht);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why quadratic probing here:**
- Demonstrates the i² probe sequence explicitly
- Prime table size ensures coverage
- Lower load factor threshold shows the trade-off

**Trade-off vs linear:** Better distribution, but cache unfriendly due to jumping.

---

## 5. Open Addressing - Double Hashing

### 1. How It Works

Double hashing uses a second hash function to compute the probe step. Instead of fixed steps (linear) or quadratic growth, each key has its own probe stride determined by `h2(key)`.

```
Probe sequence: h1(k), h1(k)+h2(k), h1(k)+2*h2(k), h1(k)+3*h2(k)...

Example:
Key "apple": h1=5, h2=3 -> sequence: 5, 8, 11, 14, 1, 4, 7...
Key "banana": h1=5, h2=7 -> sequence: 5, 12, 3, 10, 1, 8...

Different keys with same h1 have different probe sequences (if h2 differs).

+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |10 |11 |12 |13 |14 |15 |
+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
              ^       ^           ^       ^
              |       |           |       |
              |       apple(h1=5) |       apple+h2
              banana+h2          banana(+h2 again)
```

双重哈希说明：
- 使用两个哈希函数：h1确定起始位置，h2确定步长
- 探测序列：h1(k) + i * h2(k)，其中i = 0, 1, 2, ...
- 不同键即使h1相同，若h2不同则探测序列不同
- 消除了次级聚类问题
- h2不能返回0（否则无法移动），通常h2返回值与表大小互素

**Critical requirement:** `h2(key)` must never return 0, and should be coprime with table size.

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) - best theoretical among open addressing |
| Worst-case lookup | O(n) |
| Space overhead | Same as linear/quadratic |
| Cache friendliness | Poor - unpredictable access pattern |
| High load factor behavior | Best among basic open addressing |
| Resize complexity | O(n) |

---

### 3. Pros and Cons

#### Pros
- No primary or secondary clustering
- Closest to ideal uniform probing
- Can achieve higher load factors than linear/quadratic

#### Cons
- Two hash computations per probe
- Cache unfriendly - random access pattern
- Must ensure h2 is coprime with table size
- More complex implementation

**Implementation Risks:**
- h2 returning 0 causes infinite loop
- h2 not coprime with size prevents full table coverage

---

### 4. When to Use

**Good fit when:**
- Need high load factors (0.7-0.8)
- Hash computation is cheap
- Clustering is a measured problem
- Memory is constrained (can't afford lower load factor)

---

### 5. Real-World Use Cases

- **Theoretical benchmarks** - often used as baseline
- **Specialized hash tables** - when tuned for specific key distributions
- **Less common in practice** - Robin Hood usually preferred for high load

---

### 6. Complete Userspace C Example

```c
/*
 * double_hashing.c
 * Compile: gcc -Wall -Wextra -O2 -o double_hashing double_hashing.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INITIAL_SIZE 17
#define MAX_LOAD_FACTOR 0.75

typedef enum { SLOT_EMPTY, SLOT_OCCUPIED, SLOT_DELETED } slot_state;

struct ht_entry {
    char *key;
    int value;
    slot_state state;
};

struct hashtable {
    struct ht_entry *entries;
    size_t size;
    size_t count;
};

/* Primary hash function - determines starting position */
static uint32_t hash1(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/* Secondary hash function - determines step size
 * Must never return 0, and ideally coprime with table size.
 * Using a prime-based formula ensures non-zero results. */
static uint32_t hash2(const char *str, size_t table_size)
{
    uint32_t hash = 0;
    int c;
    while ((c = *str++))
        hash = hash * 31 + c;
    
    /* 
     * Return value in range [1, table_size-1].
     * For prime table size, any non-zero step is coprime.
     * The +1 ensures we never return 0.
     */
    return 1 + (hash % (table_size - 1));
}

static size_t next_prime(size_t n)
{
    if (n <= 2) return 2;
    if (n % 2 == 0) n++;
    
    while (1) {
        int is_prime = 1;
        for (size_t i = 3; i * i <= n; i += 2) {
            if (n % i == 0) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime) return n;
        n += 2;
    }
}

struct hashtable *ht_create(size_t size)
{
    struct hashtable *ht = malloc(sizeof(*ht));
    if (!ht) return NULL;
    
    size = next_prime(size);
    
    ht->entries = calloc(size, sizeof(struct ht_entry));
    if (!ht->entries) {
        free(ht);
        return NULL;
    }
    
    for (size_t i = 0; i < size; i++)
        ht->entries[i].state = SLOT_EMPTY;
    
    ht->size = size;
    ht->count = 0;
    return ht;
}

static int ht_resize(struct hashtable *ht, size_t new_size)
{
    new_size = next_prime(new_size);
    
    struct ht_entry *old = ht->entries;
    size_t old_size = ht->size;
    
    ht->entries = calloc(new_size, sizeof(struct ht_entry));
    if (!ht->entries) {
        ht->entries = old;
        return -1;
    }
    
    for (size_t i = 0; i < new_size; i++)
        ht->entries[i].state = SLOT_EMPTY;
    
    ht->size = new_size;
    ht->count = 0;
    
    for (size_t i = 0; i < old_size; i++) {
        if (old[i].state == SLOT_OCCUPIED) {
            uint32_t h1 = hash1(old[i].key) % new_size;
            uint32_t h2 = hash2(old[i].key, new_size);
            size_t idx = h1;
            
            while (ht->entries[idx].state == SLOT_OCCUPIED) {
                idx = (idx + h2) % new_size;
            }
            
            ht->entries[idx].key = old[i].key;
            ht->entries[idx].value = old[i].value;
            ht->entries[idx].state = SLOT_OCCUPIED;
            ht->count++;
        }
    }
    
    free(old);
    return 0;
}

int ht_insert(struct hashtable *ht, const char *key, int value)
{
    if ((double)ht->count / ht->size > MAX_LOAD_FACTOR) {
        if (ht_resize(ht, ht->size * 2) < 0)
            return -1;
    }
    
    uint32_t h1 = hash1(key) % ht->size;
    uint32_t h2 = hash2(key, ht->size);  /* Step size specific to this key */
    size_t idx = h1;
    size_t first_tomb = SIZE_MAX;
    size_t probes = 0;
    
    /*
     * Double hashing probe: idx = (h1 + i * h2) % size
     * Each key has unique step size, eliminating secondary clustering.
     */
    while (probes < ht->size) {
        if (ht->entries[idx].state == SLOT_EMPTY) {
            size_t ins = (first_tomb != SIZE_MAX) ? first_tomb : idx;
            ht->entries[ins].key = strdup(key);
            if (!ht->entries[ins].key) return -1;
            ht->entries[ins].value = value;
            ht->entries[ins].state = SLOT_OCCUPIED;
            ht->count++;
            return 0;
        }
        
        if (ht->entries[idx].state == SLOT_DELETED) {
            if (first_tomb == SIZE_MAX)
                first_tomb = idx;
        } else if (strcmp(ht->entries[idx].key, key) == 0) {
            ht->entries[idx].value = value;
            return 0;
        }
        
        idx = (idx + h2) % ht->size;  /* Step by h2 */
        probes++;
    }
    
    return -1;  /* Table full */
}

int *ht_search(struct hashtable *ht, const char *key)
{
    uint32_t h1 = hash1(key) % ht->size;
    uint32_t h2 = hash2(key, ht->size);
    size_t idx = h1;
    size_t probes = 0;
    
    while (probes < ht->size) {
        if (ht->entries[idx].state == SLOT_EMPTY)
            return NULL;
        
        if (ht->entries[idx].state == SLOT_OCCUPIED &&
            strcmp(ht->entries[idx].key, key) == 0) {
            return &ht->entries[idx].value;
        }
        
        idx = (idx + h2) % ht->size;
        probes++;
    }
    
    return NULL;
}

int ht_delete(struct hashtable *ht, const char *key)
{
    uint32_t h1 = hash1(key) % ht->size;
    uint32_t h2 = hash2(key, ht->size);
    size_t idx = h1;
    size_t probes = 0;
    
    while (probes < ht->size) {
        if (ht->entries[idx].state == SLOT_EMPTY)
            return -1;
        
        if (ht->entries[idx].state == SLOT_OCCUPIED &&
            strcmp(ht->entries[idx].key, key) == 0) {
            free(ht->entries[idx].key);
            ht->entries[idx].key = NULL;
            ht->entries[idx].state = SLOT_DELETED;
            ht->count--;
            return 0;
        }
        
        idx = (idx + h2) % ht->size;
        probes++;
    }
    
    return -1;
}

void ht_destroy(struct hashtable *ht)
{
    if (!ht) return;
    
    for (size_t i = 0; i < ht->size; i++) {
        if (ht->entries[i].state == SLOT_OCCUPIED)
            free(ht->entries[i].key);
    }
    
    free(ht->entries);
    free(ht);
}

int main(void)
{
    struct hashtable *ht = ht_create(INITIAL_SIZE);
    if (!ht) {
        fprintf(stderr, "Failed to create hash table\n");
        return 1;
    }
    
    printf("Double hashing table, size: %zu\n\n", ht->size);
    
    /* Insert keys that would cluster badly with linear probing */
    const char *keys[] = {"aaa", "aab", "aac", "aad", "aae",
                          "bbb", "bbc", "bbd", "bbe", "bbf"};
    
    for (int i = 0; i < 10; i++) {
        ht_insert(ht, keys[i], i);
        printf("Inserted %s: h1=%u, h2=%u\n", 
               keys[i], 
               hash1(keys[i]) % ht->size,
               hash2(keys[i], ht->size));
    }
    
    printf("\nLoad factor: %.2f\n", (double)ht->count / ht->size);
    
    printf("\nLookup tests:\n");
    for (int i = 0; i < 10; i++) {
        int *val = ht_search(ht, keys[i]);
        printf("  %s -> %d\n", keys[i], val ? *val : -1);
    }
    
    /* Test with similar strings that might cluster */
    printf("\nTesting similar keys (would cluster with linear probing):\n");
    int *v1 = ht_search(ht, "aaa");
    int *v2 = ht_search(ht, "aab");
    int *v3 = ht_search(ht, "aac");
    printf("  aaa=%d, aab=%d, aac=%d\n", 
           v1 ? *v1 : -1, v2 ? *v2 : -1, v3 ? *v3 : -1);
    
    ht_destroy(ht);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why double hashing here:**
- Shows the two hash functions clearly
- Demonstrates how step size varies per key
- Similar keys get different probe sequences

**Key insight:** The second hash function makes probe sequences key-dependent, not just position-dependent. This eliminates secondary clustering where keys with the same initial hash collide repeatedly.
