# Hash Table Implementation Guide - Part 4: Specialized Methods

## 8. Perfect Hashing

### 1. How It Works

Perfect hashing guarantees **no collisions** by construction. It requires knowing all keys in advance (static key set). The classic FKS (Fredman-Komlós-Szemerédi) scheme uses two levels:

1. **First level:** Hash to buckets using a universal hash function
2. **Second level:** Each bucket has its own perfect hash function sized to avoid collisions

```
Two-Level Perfect Hashing:

Input keys: {"apple", "banana", "cherry", "date"}

Level 1: Primary hash h1 maps to buckets
+--------+--------+--------+--------+
| Bucket0| Bucket1| Bucket2| Bucket3|
+--------+--------+--------+--------+
| apple  | banana |        | cherry |
| date   |        |        |        |
+--------+--------+--------+--------+
  2 keys   1 key    0 keys   1 key

Level 2: Each non-empty bucket gets a secondary table
         sized to n_i^2 to guarantee no collisions

Bucket0 (2 keys): secondary table of size 4
h2_0("apple") = 1, h2_0("date") = 3  -> no collision

+---+-------+---+------+
| - | apple | - | date |
+---+-------+---+------+
  0     1     2    3

Bucket1 (1 key): secondary table of size 1
+--------+
| banana |
+--------+

Bucket3 (1 key): secondary table of size 1
+--------+
| cherry |
+--------+

Lookup: O(1) guaranteed
- Compute h1(key) -> bucket index
- Compute h2_i(key) -> slot in secondary table
- Direct access, no probing ever
```

完美哈希说明：
- 需要预先知道所有键（静态键集）
- 构造阶段找到无冲突的哈希函数
- 两级结构：一级哈希分桶，二级哈希在桶内无冲突
- 二级表大小为桶内键数的平方，确保能找到无冲突的哈希函数
- 总空间O(n)，虽然二级表看似浪费但数学上证明总和是线性的
- 查找是真正的O(1)，没有任何探测或比较
- 不支持动态插入，只适合静态数据

**Space guarantee:** Total space is O(n) because sum of squares of bucket sizes is bounded.

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Lookup | O(1) **guaranteed**, no probing |
| Construction | O(n) expected time |
| Space overhead | O(n), but with constant factor ~2-4x |
| Cache friendliness | Excellent - direct indexing |
| High load factor behavior | N/A - no collisions by design |
| Dynamic updates | **Not supported** |

---

### 3. Pros and Cons

#### Pros
- **True O(1) lookup** - no worst case, no variance
- No key comparisons after hash (minimal perfect)
- Excellent cache behavior
- Predictable performance

#### Cons
- **Static key set required** - no inserts after construction
- Construction is complex
- Space overhead (2-4x)
- Changing any key requires complete rebuild

**Implementation Risks:**
- Must handle construction failures (retry with different hash seeds)
- Secondary table sizing is critical

---

### 4. When to Use

**Good fit when:**
- Key set is known at compile/startup time
- No dynamic updates needed
- Lookup performance is critical
- Key set changes are rare and rebuild is acceptable

---

### 5. Real-World Use Cases

- **gperf** - GNU perfect hash generator for keyword lookup
- **Language keyword tables** - compiler/interpreter reserved words
- **DNS root servers** - static zone files
- **ROM lookup tables** - embedded systems
- **Configuration keys** - known set of config options

---

### 6. Complete Userspace C Example

```c
/*
 * perfect_hashing.c
 * Simplified minimal perfect hashing for a small static key set.
 * 
 * Compile: gcc -Wall -Wextra -O2 -o perfect_hashing perfect_hashing.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * For simplicity, we implement a single-level perfect hash using
 * the "hash, displace, and compress" (CHD) approach simplified.
 * 
 * In production, use gperf or a CHD library.
 */

#define MAX_KEYS 16
#define TABLE_SIZE 32  /* Larger than key count for easier construction */
#define MAX_ATTEMPTS 100

struct perfect_hash {
    const char **keys;     /* Original key array */
    int *values;           /* Values corresponding to keys */
    size_t num_keys;
    
    /* Hash table: maps hash -> key_index */
    int *table;            /* -1 means empty */
    size_t table_size;
    
    /* Hash parameters found during construction */
    uint32_t seed;
};

static uint32_t hash_with_seed(const char *str, uint32_t seed)
{
    uint32_t hash = seed;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c;
    return hash;
}

/*
 * Try to construct a perfect hash with given seed.
 * Returns 0 on success (no collisions), -1 on collision.
 */
static int try_construct(struct perfect_hash *ph, uint32_t seed)
{
    /* Reset table */
    for (size_t i = 0; i < ph->table_size; i++)
        ph->table[i] = -1;
    
    /* Try to place each key */
    for (size_t i = 0; i < ph->num_keys; i++) {
        uint32_t h = hash_with_seed(ph->keys[i], seed) % ph->table_size;
        
        if (ph->table[h] != -1) {
            /* Collision - this seed doesn't work */
            return -1;
        }
        
        ph->table[h] = (int)i;  /* Store index to key */
    }
    
    /* No collisions - success */
    return 0;
}

struct perfect_hash *ph_create(const char **keys, int *values, size_t num_keys)
{
    if (num_keys > MAX_KEYS) {
        fprintf(stderr, "Too many keys for simple perfect hash\n");
        return NULL;
    }
    
    struct perfect_hash *ph = malloc(sizeof(*ph));
    if (!ph) return NULL;
    
    ph->keys = keys;
    ph->values = values;
    ph->num_keys = num_keys;
    ph->table_size = TABLE_SIZE;
    
    ph->table = malloc(TABLE_SIZE * sizeof(int));
    if (!ph->table) {
        free(ph);
        return NULL;
    }
    
    /*
     * Construction: try different seeds until we find one
     * that produces no collisions. For small key sets with
     * larger tables, this typically succeeds quickly.
     */
    for (uint32_t seed = 0; seed < MAX_ATTEMPTS; seed++) {
        if (try_construct(ph, seed) == 0) {
            ph->seed = seed;
            printf("Perfect hash constructed with seed %u\n", seed);
            return ph;
        }
    }
    
    /* Failed to find perfect hash - table too small or unlucky */
    fprintf(stderr, "Failed to construct perfect hash\n");
    free(ph->table);
    free(ph);
    return NULL;
}

/*
 * Lookup is TRUE O(1): one hash, one array access.
 * No probing, no loops, no key comparisons (minimal perfect).
 * 
 * For non-minimal perfect hash, we'd need to verify the key matches.
 */
int ph_lookup(struct perfect_hash *ph, const char *key, int *value)
{
    uint32_t h = hash_with_seed(key, ph->seed) % ph->table_size;
    
    int idx = ph->table[h];
    if (idx < 0) {
        return -1;  /* Empty slot - key not in original set */
    }
    
    /*
     * For robustness against keys not in original set,
     * verify the key matches. A minimal perfect hash
     * would skip this if we guarantee input is from key set.
     */
    if (strcmp(ph->keys[idx], key) != 0) {
        return -1;  /* Hash collision with non-member key */
    }
    
    *value = ph->values[idx];
    return 0;
}

void ph_destroy(struct perfect_hash *ph)
{
    if (!ph) return;
    free(ph->table);
    free(ph);
}

int main(void)
{
    /* Static key set - known at compile time */
    const char *keywords[] = {
        "if", "else", "while", "for", "return",
        "int", "char", "void", "struct", "typedef"
    };
    
    /* Token IDs for each keyword */
    int token_ids[] = {
        100, 101, 102, 103, 104,
        200, 201, 202, 203, 204
    };
    
    size_t num_keywords = sizeof(keywords) / sizeof(keywords[0]);
    
    printf("Building perfect hash for %zu keywords...\n\n", num_keywords);
    
    struct perfect_hash *ph = ph_create(keywords, token_ids, num_keywords);
    if (!ph) {
        return 1;
    }
    
    /* Lookup all keywords - each is O(1) guaranteed */
    printf("Keyword lookups:\n");
    for (size_t i = 0; i < num_keywords; i++) {
        int token;
        if (ph_lookup(ph, keywords[i], &token) == 0) {
            printf("  '%s' -> token %d (hash slot %u)\n", 
                   keywords[i], token,
                   hash_with_seed(keywords[i], ph->seed) % ph->table_size);
        } else {
            printf("  '%s' -> NOT FOUND (error!)\n", keywords[i]);
        }
    }
    
    /* Test non-member keys */
    printf("\nNon-member key tests:\n");
    const char *non_members[] = {"goto", "break", "main", "printf"};
    for (int i = 0; i < 4; i++) {
        int token;
        if (ph_lookup(ph, non_members[i], &token) == 0) {
            printf("  '%s' -> found (error - shouldn't exist!)\n", non_members[i]);
        } else {
            printf("  '%s' -> not found (correct)\n", non_members[i]);
        }
    }
    
    /* Show table utilization */
    printf("\nTable statistics:\n");
    size_t used = 0;
    for (size_t i = 0; i < ph->table_size; i++) {
        if (ph->table[i] >= 0) used++;
    }
    printf("  Table size: %zu, Used: %zu, Utilization: %.1f%%\n",
           ph->table_size, used, 100.0 * used / ph->table_size);
    
    ph_destroy(ph);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why perfect hashing here:**
- Keyword lookup is a classic use case
- Keys are known at compile time
- Lookup must be fast (lexer hot path)
- No dynamic updates needed

**What would go wrong with dynamic methods:**
- Robin Hood: unnecessary complexity for static data
- Chaining: probe overhead on every lookup
- Any method with collisions: wasted comparisons

---

## 9. Hopscotch Hashing

### 1. How It Works

Hopscotch hashing is a hybrid that combines the cache-friendliness of linear probing with bounded probe sequences like cuckoo hashing. Each bucket has a "neighborhood" of H slots where its entries must reside. A bitmap tracks which slots in the neighborhood contain entries for that bucket.

```
Hopscotch with neighborhood size H=4:

Each bucket owns a neighborhood of 4 consecutive slots.
Bucket 2's neighborhood: slots 2, 3, 4, 5

Bitmap indicates which neighborhood slots are "ours":
+-------+-------+-------+-------+-------+-------+-------+-------+
| Slot0 | Slot1 | Slot2 | Slot3 | Slot4 | Slot5 | Slot6 | Slot7 |
+-------+-------+-------+-------+-------+-------+-------+-------+
| A     | B     | C     | D     | E     | F     | -     | -     |
+-------+-------+-------+-------+-------+-------+-------+-------+

Bucket 0: bitmap=1000 (A is in slot 0)
Bucket 1: bitmap=1000 (B is in slot 1)  
Bucket 2: bitmap=1010 (C in slot 2, E in slot 4, both hash to bucket 2)
Bucket 3: bitmap=1001 (D in slot 3, F in slot 5, but wait...)

Wait - E and F can't both be there. Let me redo:

Bucket 2's entries: hash(C)=2, hash(E)=2
C is at slot 2 (offset 0), E is at slot 4 (offset 2)
Bitmap for bucket 2: 1010 (bits 0 and 2 set)

Lookup for key K where hash(K)=2:
1. Check bucket 2's bitmap: 1010
2. Bit 0 set -> check slot 2+0=2
3. Bit 2 set -> check slot 2+2=4
4. Only 2 slots to check, bounded by H

Insert displacement:
If slot 2 is full and we need to insert there:
1. Find empty slot in neighborhood (slots 2-5)
2. If no empty slot, find empty slot further out
3. "Hop" entries toward the empty slot until it's in range
```

Hopscotch哈希说明：
- 每个桶有一个固定大小的"邻域"（neighborhood），通常H=32
- 哈希到某桶的所有键必须存储在该桶的邻域内
- 位图记录邻域中哪些槽属于该桶
- 查找时只需检查位图中标记的槽位，最多H次
- 插入时若邻域满，将远处空槽"跳跃"移近
- 结合了线性探测的缓存友好性和有界探测长度
- 支持并发友好的设计（原论文重点）

**Key property:** Lookup checks at most H slots, and they're all in a contiguous neighborhood (cache-friendly).

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) |
| Worst-case lookup | O(H) where H is neighborhood size |
| Space overhead | H bits per bucket for bitmap |
| Cache friendliness | **Excellent** - neighborhood is contiguous |
| High load factor behavior | Good up to 0.9 with right H |
| Resize complexity | O(n) |

---

### 3. Pros and Cons

#### Pros
- **Bounded worst-case probe length**
- Cache-friendly (contiguous memory)
- Good for concurrent implementations
- High load factor support

#### Cons
- Displacement ("hopping") can cascade
- More complex than linear probing
- Bitmap overhead per bucket
- H must be tuned for workload

**Implementation Risks:**
- Hopscotch displacement can fail if table too full
- Must handle neighborhood exhaustion

---

### 4. When to Use

**Good fit when:**
- Need bounded lookup time
- Cache behavior matters
- Concurrent access is planned
- Load factor 0.8-0.9 desired

---

### 5. Real-World Use Cases

- **Concurrent hash tables** - original motivation
- **Java ConcurrentHashMap** - influenced by hopscotch ideas
- **Database indexes** - bounded probe time important
- **Real-time systems** - predictable latency

---

### 6. Complete Userspace C Example

```c
/*
 * hopscotch_hashing.c
 * Compile: gcc -Wall -Wextra -O2 -o hopscotch_hashing hopscotch_hashing.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Neighborhood size - typically 32 for 32-bit bitmap */
#define HOP_RANGE 8  /* Smaller for demo visibility */
#define INITIAL_SIZE 32
#define MAX_LOAD_FACTOR 0.9

struct hop_entry {
    char *key;
    int value;
};

struct hop_bucket {
    uint32_t bitmap;          /* Bits indicate which neighborhood slots are ours */
    struct hop_entry entry;   /* Entry stored in this slot */
};

struct hopscotch_table {
    struct hop_bucket *buckets;
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

struct hopscotch_table *hop_create(size_t size)
{
    struct hopscotch_table *ht = malloc(sizeof(*ht));
    if (!ht) return NULL;
    
    ht->buckets = calloc(size, sizeof(struct hop_bucket));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    
    ht->size = size;
    ht->count = 0;
    return ht;
}

/*
 * Find an empty slot and "hop" it into the target bucket's neighborhood.
 * Returns the final slot index, or SIZE_MAX on failure.
 */
static size_t hop_find_and_displace(struct hopscotch_table *ht, size_t bucket_idx)
{
    /* First, find any empty slot */
    size_t empty = bucket_idx;
    while (empty < ht->size && ht->buckets[empty].entry.key != NULL) {
        empty++;
    }
    
    if (empty >= ht->size) {
        /* Try wrapping */
        empty = 0;
        while (empty < bucket_idx && ht->buckets[empty].entry.key != NULL) {
            empty++;
        }
        if (empty >= bucket_idx)
            return SIZE_MAX;  /* Table full */
    }
    
    /*
     * Now we have an empty slot at 'empty'. If it's within HOP_RANGE
     * of bucket_idx, we're done. Otherwise, we need to "hop" it closer.
     */
    while (1) {
        /* Check if empty is in bucket_idx's neighborhood */
        size_t dist = (empty >= bucket_idx) ? 
                      (empty - bucket_idx) : 
                      (ht->size - bucket_idx + empty);
        
        if (dist < HOP_RANGE)
            return empty;  /* Success - slot is in range */
        
        /*
         * Empty slot too far. Find a bucket H that:
         * 1. Has an entry in a slot between H and empty
         * 2. That slot is within HOP_RANGE of H
         * Move that entry to 'empty', making its old slot the new 'empty'.
         */
        int found = 0;
        for (size_t offset = HOP_RANGE - 1; offset > 0; offset--) {
            size_t candidate = (empty >= offset) ? 
                               (empty - offset) : 
                               (ht->size - offset + empty);
            
            /* Check if candidate bucket has any entry we can move */
            uint32_t bitmap = ht->buckets[candidate].bitmap;
            
            for (size_t bit = 0; bit < offset && bit < HOP_RANGE; bit++) {
                if (bitmap & (1u << bit)) {
                    /* Found an entry at candidate+bit, move it to empty */
                    size_t src = (candidate + bit) % ht->size;
                    
                    /* Move entry */
                    ht->buckets[empty].entry = ht->buckets[src].entry;
                    ht->buckets[src].entry.key = NULL;
                    
                    /* Update bitmaps */
                    ht->buckets[candidate].bitmap &= ~(1u << bit);
                    ht->buckets[candidate].bitmap |= (1u << offset);
                    
                    /* New empty slot is src */
                    empty = src;
                    found = 1;
                    break;
                }
            }
            
            if (found) break;
        }
        
        if (!found)
            return SIZE_MAX;  /* Can't displace - need resize */
    }
}

int hop_insert(struct hopscotch_table *ht, const char *key, int value)
{
    if ((double)ht->count / ht->size > MAX_LOAD_FACTOR) {
        /* Would need resize - not implemented for brevity */
        fprintf(stderr, "Table too full, resize needed\n");
        return -1;
    }
    
    uint32_t h = hash_string(key) % ht->size;
    
    /* Check if key exists in neighborhood */
    uint32_t bitmap = ht->buckets[h].bitmap;
    for (size_t i = 0; i < HOP_RANGE && bitmap; i++) {
        if (bitmap & (1u << i)) {
            size_t slot = (h + i) % ht->size;
            if (ht->buckets[slot].entry.key &&
                strcmp(ht->buckets[slot].entry.key, key) == 0) {
                /* Update existing */
                ht->buckets[slot].entry.value = value;
                return 0;
            }
        }
        bitmap &= ~(1u << i);
    }
    
    /* Key doesn't exist - find slot in neighborhood */
    size_t slot = SIZE_MAX;
    
    /* First try: find empty slot directly in neighborhood */
    for (size_t i = 0; i < HOP_RANGE; i++) {
        size_t s = (h + i) % ht->size;
        if (ht->buckets[s].entry.key == NULL) {
            slot = s;
            break;
        }
    }
    
    /* Second try: displace to bring empty slot into neighborhood */
    if (slot == SIZE_MAX) {
        slot = hop_find_and_displace(ht, h);
        if (slot == SIZE_MAX)
            return -1;  /* Failed */
    }
    
    /* Insert at slot */
    char *key_copy = strdup(key);
    if (!key_copy) return -1;
    
    ht->buckets[slot].entry.key = key_copy;
    ht->buckets[slot].entry.value = value;
    
    /* Update bitmap */
    size_t offset = (slot >= h) ? (slot - h) : (ht->size - h + slot);
    ht->buckets[h].bitmap |= (1u << offset);
    
    ht->count++;
    return 0;
}

/*
 * Lookup: check only slots indicated by bitmap.
 * Bounded to HOP_RANGE comparisons max.
 */
int *hop_search(struct hopscotch_table *ht, const char *key)
{
    uint32_t h = hash_string(key) % ht->size;
    uint32_t bitmap = ht->buckets[h].bitmap;
    
    /*
     * Only check slots where bitmap bit is set.
     * This bounds our search to at most HOP_RANGE slots,
     * all within a contiguous memory region.
     */
    for (size_t i = 0; i < HOP_RANGE && bitmap; i++) {
        if (bitmap & (1u << i)) {
            size_t slot = (h + i) % ht->size;
            if (ht->buckets[slot].entry.key &&
                strcmp(ht->buckets[slot].entry.key, key) == 0) {
                return &ht->buckets[slot].entry.value;
            }
        }
    }
    
    return NULL;
}

int hop_delete(struct hopscotch_table *ht, const char *key)
{
    uint32_t h = hash_string(key) % ht->size;
    uint32_t bitmap = ht->buckets[h].bitmap;
    
    for (size_t i = 0; i < HOP_RANGE && bitmap; i++) {
        if (bitmap & (1u << i)) {
            size_t slot = (h + i) % ht->size;
            if (ht->buckets[slot].entry.key &&
                strcmp(ht->buckets[slot].entry.key, key) == 0) {
                /* Found - delete */
                free(ht->buckets[slot].entry.key);
                ht->buckets[slot].entry.key = NULL;
                ht->buckets[h].bitmap &= ~(1u << i);
                ht->count--;
                return 0;
            }
        }
    }
    
    return -1;  /* Not found */
}

void hop_destroy(struct hopscotch_table *ht)
{
    if (!ht) return;
    
    for (size_t i = 0; i < ht->size; i++) {
        if (ht->buckets[i].entry.key)
            free(ht->buckets[i].entry.key);
    }
    
    free(ht->buckets);
    free(ht);
}

void hop_stats(struct hopscotch_table *ht)
{
    printf("Size: %zu, Count: %zu, Load: %.2f\n",
           ht->size, ht->count, (double)ht->count / ht->size);
    
    /* Analyze bitmap utilization */
    size_t max_bits = 0;
    size_t total_bits = 0;
    for (size_t i = 0; i < ht->size; i++) {
        size_t bits = __builtin_popcount(ht->buckets[i].bitmap);
        total_bits += bits;
        if (bits > max_bits) max_bits = bits;
    }
    
    printf("Max entries per neighborhood: %zu, Avg: %.2f\n",
           max_bits, (double)total_bits / ht->size);
}

int main(void)
{
    struct hopscotch_table *ht = hop_create(INITIAL_SIZE);
    if (!ht) {
        fprintf(stderr, "Failed to create hopscotch table\n");
        return 1;
    }
    
    printf("Hopscotch hashing demo (neighborhood=%d)\n\n", HOP_RANGE);
    
    /* Insert entries */
    char buf[32];
    for (int i = 0; i < 20; i++) {
        snprintf(buf, sizeof(buf), "key%d", i);
        if (hop_insert(ht, buf, i * 10) < 0) {
            fprintf(stderr, "Insert failed for %s\n", buf);
        }
    }
    
    printf("After inserting 20 items:\n");
    hop_stats(ht);
    
    /* Lookup tests */
    printf("\nLookup tests:\n");
    for (int i = 0; i < 20; i += 5) {
        snprintf(buf, sizeof(buf), "key%d", i);
        int *val = hop_search(ht, buf);
        printf("  %s -> %d\n", buf, val ? *val : -1);
    }
    
    int *missing = hop_search(ht, "nonexistent");
    printf("  'nonexistent' -> %s\n", missing ? "found (error)" : "not found (ok)");
    
    /* Delete test */
    printf("\nDeleting key5, key10, key15...\n");
    hop_delete(ht, "key5");
    hop_delete(ht, "key10");
    hop_delete(ht, "key15");
    
    printf("After deletions:\n");
    hop_stats(ht);
    
    /* Verify deletions */
    int *v5 = hop_search(ht, "key5");
    int *v6 = hop_search(ht, "key6");
    printf("key5 -> %s, key6 -> %d\n",
           v5 ? "found (error)" : "not found (ok)",
           v6 ? *v6 : -1);
    
    hop_destroy(ht);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why hopscotch here:**
- Demonstrates neighborhood concept with bitmap
- Shows bounded lookup (scan only bitmap bits)
- Illustrates displacement logic
- Cache-friendly contiguous access

**Key insight:** Hopscotch gets the best of both worlds: linear probing's cache behavior with cuckoo-like bounded search. The bitmap makes lookup efficient - only check slots that matter.

---

## 10. Hash Trie / HAMT (Hash Array Mapped Trie)

### 1. How It Works

A Hash Array Mapped Trie (HAMT) uses the bits of a hash value to navigate a tree structure. Each level of the tree consumes a few bits of the hash to index into a sparse array. This gives O(log n) lookup with excellent memory efficiency through bitmap-indexed sparse arrays.

```
HAMT Structure (5 bits per level, 32-bit hash):

Hash of "apple" = 0b 01101 00011 10010 01010 11100 00001
                     |     |     |     |     |     |
                     L1    L2    L3    L4    L5    L6

Level 1: Use bits 01101 (13) to index
Level 2: Use bits 00011 (3) to index
... and so on

Each node has a 32-bit bitmap indicating which children exist,
and a compact array of only the present children.

Example node with 3 children (indices 3, 7, 15):
Bitmap: 00000000 00000000 10000000 10001000
        ^                 ^        ^   ^
        bit 31            bit 15   bit 7 bit 3

Children array: [child_3, child_7, child_15]  <- compact, no wasted space

To find child at index 7:
1. Check if bit 7 is set in bitmap: yes
2. Count bits below bit 7: popcount(bitmap & 0b01111111) = 1
3. child_7 is at children[1]

        Root
         |
    +----+----+----+
    |    |    |    |
   [3]  [7]  [15]  ...
         |
    +----+----+
    |    |    
   [1]  [9]   
    |
   "apple" -> 42
```

HAMT哈希数组映射字典树说明：
- 把哈希值分成多段，每段作为一层的索引
- 每个节点用位图指示哪些子节点存在
- 子节点数组是紧凑的，只存储存在的子节点
- 使用popcount计算子节点在数组中的位置
- 内存效率高：稀疏数组不浪费空间
- 结构共享：适合持久化数据结构（函数式编程）
- 查找时间O(log n)，但常数因子小（每层5-6位）
- 适合大型动态集合

**Key properties:**
- O(log n) lookup but with small constant (32-bit hash -> max 7 levels with 5 bits/level)
- Memory efficient - only stores present children
- Supports structural sharing for persistent/immutable implementations

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(log n) but very shallow |
| Worst-case lookup | O(hash_bits / bits_per_level) |
| Space overhead | Minimal - bitmap compression |
| Cache friendliness | Moderate - tree traversal |
| High load factor behavior | N/A - grows as needed |
| Resize complexity | None - grows incrementally |

---

### 3. Pros and Cons

#### Pros
- **No resizing needed** - grows incrementally
- Memory efficient through bitmap compression
- Supports structural sharing (immutable/persistent)
- Good for very large collections
- Prefix operations possible

#### Cons
- More complex implementation
- Tree traversal has pointer overhead
- Not as cache-friendly as flat tables
- Requires popcount operation

**Implementation Risks:**
- Bitmap manipulation errors
- Memory leaks in tree structure
- Incorrect popcount usage

---

### 4. When to Use

**Good fit when:**
- Data set is very large
- Don't want to resize ever
- Need persistent/immutable semantics
- Memory efficiency matters
- Incremental growth is preferred

---

### 5. Real-World Use Cases

- **Clojure's persistent hash map** - HAMT-based
- **Scala's immutable HashMap** - HAMT
- **Haskell's unordered-containers** - HAMT
- **IPFS** - content-addressed storage uses HAMTs
- **Git internals** - tree objects similar structure

---

### 6. Complete Userspace C Example

```c
/*
 * hamt.c - Simplified Hash Array Mapped Trie
 * Compile: gcc -Wall -Wextra -O2 -o hamt hamt.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* 
 * Use 5 bits per level -> 32 possible children per node.
 * 32-bit hash -> at most 7 levels (32/5 = 6.4).
 */
#define BITS_PER_LEVEL 5
#define CHILDREN_PER_NODE (1 << BITS_PER_LEVEL)  /* 32 */
#define MASK ((1 << BITS_PER_LEVEL) - 1)         /* 0x1F */

/* Count set bits - use builtin for efficiency */
#define popcount(x) __builtin_popcount(x)

typedef enum {
    NODE_EMPTY,
    NODE_LEAF,     /* Contains key-value pair */
    NODE_INTERNAL  /* Contains children */
} node_type;

struct hamt_node {
    node_type type;
    union {
        struct {
            char *key;
            int value;
            uint32_t hash;  /* Cached for collision handling */
        } leaf;
        struct {
            uint32_t bitmap;          /* Which children exist */
            struct hamt_node **children;  /* Compact array */
        } internal;
    } data;
};

struct hamt {
    struct hamt_node *root;
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

/* Extract the relevant bits for a given level */
static uint32_t get_index(uint32_t hash, int level)
{
    return (hash >> (level * BITS_PER_LEVEL)) & MASK;
}

/* Get position in compact array from bitmap */
static int get_position(uint32_t bitmap, uint32_t index)
{
    /* Count bits set below 'index' */
    return popcount(bitmap & ((1u << index) - 1));
}

static struct hamt_node *node_create_leaf(const char *key, int value, uint32_t hash)
{
    struct hamt_node *node = malloc(sizeof(*node));
    if (!node) return NULL;
    
    node->type = NODE_LEAF;
    node->data.leaf.key = strdup(key);
    if (!node->data.leaf.key) {
        free(node);
        return NULL;
    }
    node->data.leaf.value = value;
    node->data.leaf.hash = hash;
    
    return node;
}

static struct hamt_node *node_create_internal(void)
{
    struct hamt_node *node = malloc(sizeof(*node));
    if (!node) return NULL;
    
    node->type = NODE_INTERNAL;
    node->data.internal.bitmap = 0;
    node->data.internal.children = NULL;
    
    return node;
}

static void node_destroy(struct hamt_node *node)
{
    if (!node) return;
    
    if (node->type == NODE_LEAF) {
        free(node->data.leaf.key);
    } else if (node->type == NODE_INTERNAL) {
        int num_children = popcount(node->data.internal.bitmap);
        for (int i = 0; i < num_children; i++) {
            node_destroy(node->data.internal.children[i]);
        }
        free(node->data.internal.children);
    }
    
    free(node);
}

struct hamt *hamt_create(void)
{
    struct hamt *h = malloc(sizeof(*h));
    if (!h) return NULL;
    
    h->root = node_create_internal();
    if (!h->root) {
        free(h);
        return NULL;
    }
    
    h->count = 0;
    return h;
}

/*
 * Insert at a specific node, handling the tree structure.
 * Returns new/modified node, or NULL on failure.
 */
static struct hamt_node *node_insert(struct hamt_node *node, 
                                     const char *key, int value,
                                     uint32_t hash, int level)
{
    if (!node || node->type == NODE_EMPTY) {
        /* Create leaf here */
        return node_create_leaf(key, value, hash);
    }
    
    if (node->type == NODE_LEAF) {
        /* Collision at leaf level */
        if (node->data.leaf.hash == hash &&
            strcmp(node->data.leaf.key, key) == 0) {
            /* Same key - update value */
            node->data.leaf.value = value;
            return node;
        }
        
        /* Different key - need to expand to internal node */
        struct hamt_node *new_internal = node_create_internal();
        if (!new_internal) return NULL;
        
        /* Reinsert old leaf and new key */
        uint32_t old_idx = get_index(node->data.leaf.hash, level);
        uint32_t new_idx = get_index(hash, level);
        
        if (old_idx == new_idx) {
            /* Same index at this level - recurse deeper */
            struct hamt_node *child = node_insert(node, key, value, hash, level + 1);
            if (!child) {
                free(new_internal);
                return NULL;
            }
            
            new_internal->data.internal.bitmap = (1u << old_idx);
            new_internal->data.internal.children = malloc(sizeof(struct hamt_node *));
            if (!new_internal->data.internal.children) {
                free(new_internal);
                return NULL;
            }
            new_internal->data.internal.children[0] = child;
        } else {
            /* Different indices - both fit at this level */
            new_internal->data.internal.bitmap = (1u << old_idx) | (1u << new_idx);
            new_internal->data.internal.children = malloc(2 * sizeof(struct hamt_node *));
            if (!new_internal->data.internal.children) {
                free(new_internal);
                return NULL;
            }
            
            struct hamt_node *new_leaf = node_create_leaf(key, value, hash);
            if (!new_leaf) {
                free(new_internal->data.internal.children);
                free(new_internal);
                return NULL;
            }
            
            /* Insert in order */
            if (old_idx < new_idx) {
                new_internal->data.internal.children[0] = node;
                new_internal->data.internal.children[1] = new_leaf;
            } else {
                new_internal->data.internal.children[0] = new_leaf;
                new_internal->data.internal.children[1] = node;
            }
        }
        
        return new_internal;
    }
    
    /* Internal node */
    uint32_t idx = get_index(hash, level);
    uint32_t bit = (1u << idx);
    int pos = get_position(node->data.internal.bitmap, idx);
    
    if (node->data.internal.bitmap & bit) {
        /* Child exists - recurse */
        struct hamt_node *new_child = node_insert(
            node->data.internal.children[pos], key, value, hash, level + 1);
        if (!new_child) return NULL;
        node->data.internal.children[pos] = new_child;
    } else {
        /* No child - create leaf */
        struct hamt_node *new_leaf = node_create_leaf(key, value, hash);
        if (!new_leaf) return NULL;
        
        int num_children = popcount(node->data.internal.bitmap);
        struct hamt_node **new_children = realloc(
            node->data.internal.children,
            (num_children + 1) * sizeof(struct hamt_node *));
        if (!new_children) {
            node_destroy(new_leaf);
            return NULL;
        }
        
        /* Insert at correct position */
        for (int i = num_children; i > pos; i--) {
            new_children[i] = new_children[i - 1];
        }
        new_children[pos] = new_leaf;
        
        node->data.internal.children = new_children;
        node->data.internal.bitmap |= bit;
    }
    
    return node;
}

int hamt_insert(struct hamt *h, const char *key, int value)
{
    uint32_t hash = hash_string(key);
    
    struct hamt_node *new_root = node_insert(h->root, key, value, hash, 0);
    if (!new_root) return -1;
    
    h->root = new_root;
    h->count++;
    return 0;
}

int *hamt_search(struct hamt *h, const char *key)
{
    uint32_t hash = hash_string(key);
    struct hamt_node *node = h->root;
    int level = 0;
    
    /*
     * Navigate down the trie using hash bits.
     * Each level consumes BITS_PER_LEVEL bits.
     */
    while (node && node->type == NODE_INTERNAL) {
        uint32_t idx = get_index(hash, level);
        uint32_t bit = (1u << idx);
        
        if (!(node->data.internal.bitmap & bit)) {
            return NULL;  /* No child at this index */
        }
        
        int pos = get_position(node->data.internal.bitmap, idx);
        node = node->data.internal.children[pos];
        level++;
    }
    
    if (node && node->type == NODE_LEAF) {
        if (strcmp(node->data.leaf.key, key) == 0) {
            return &node->data.leaf.value;
        }
    }
    
    return NULL;
}

void hamt_destroy(struct hamt *h)
{
    if (!h) return;
    node_destroy(h->root);
    free(h);
}

/* Debug: count nodes at each level */
static void count_levels(struct hamt_node *node, int level, int *counts, int max_level)
{
    if (!node || level >= max_level) return;
    
    counts[level]++;
    
    if (node->type == NODE_INTERNAL) {
        int num_children = popcount(node->data.internal.bitmap);
        for (int i = 0; i < num_children; i++) {
            count_levels(node->data.internal.children[i], level + 1, counts, max_level);
        }
    }
}

void hamt_stats(struct hamt *h)
{
    printf("HAMT entries: %zu\n", h->count);
    
    int level_counts[10] = {0};
    count_levels(h->root, 0, level_counts, 10);
    
    printf("Nodes per level: ");
    for (int i = 0; i < 10 && level_counts[i] > 0; i++) {
        printf("L%d=%d ", i, level_counts[i]);
    }
    printf("\n");
}

int main(void)
{
    struct hamt *h = hamt_create();
    if (!h) {
        fprintf(stderr, "Failed to create HAMT\n");
        return 1;
    }
    
    printf("HAMT demo (%d bits per level)\n\n", BITS_PER_LEVEL);
    
    /* Insert entries */
    const char *keys[] = {"apple", "banana", "cherry", "date", "elderberry",
                          "fig", "grape", "honeydew", "kiwi", "lemon",
                          "mango", "nectarine", "orange", "papaya", "quince"};
    
    for (int i = 0; i < 15; i++) {
        if (hamt_insert(h, keys[i], i * 10) < 0) {
            fprintf(stderr, "Insert failed for %s\n", keys[i]);
        }
    }
    
    printf("After inserting 15 items:\n");
    hamt_stats(h);
    
    /* Lookup tests */
    printf("\nLookup tests:\n");
    for (int i = 0; i < 15; i++) {
        int *val = hamt_search(h, keys[i]);
        printf("  %s -> %d\n", keys[i], val ? *val : -1);
    }
    
    int *missing = hamt_search(h, "raspberry");
    printf("  'raspberry' -> %s\n", missing ? "found (error)" : "not found (ok)");
    
    /* Insert many more to show growth */
    printf("\nInserting 100 more entries...\n");
    char buf[32];
    for (int i = 0; i < 100; i++) {
        snprintf(buf, sizeof(buf), "item%03d", i);
        hamt_insert(h, buf, i);
    }
    
    printf("After inserting 115 total:\n");
    hamt_stats(h);
    
    /* Verify some lookups */
    int *v50 = hamt_search(h, "item050");
    int *v99 = hamt_search(h, "item099");
    printf("item050 -> %d, item099 -> %d\n", v50 ? *v50 : -1, v99 ? *v99 : -1);
    
    hamt_destroy(h);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why HAMT here:**
- Demonstrates bitmap-indexed sparse arrays
- Shows incremental growth without resizing
- Illustrates level-by-level hash consumption
- Handles hash collisions via deeper levels

**Key insight:** HAMT is the go-to choice for functional programming languages because it naturally supports structural sharing - unchanged parts of the tree can be shared between versions after updates. In C, it's useful when you need a very large collection that grows incrementally.
