# Hash Table Implementation Guide - Part 3: Advanced Open Addressing

## 6. Robin Hood Hashing

### 1. How It Works

Robin Hood hashing is a variation of linear probing that reduces variance in probe lengths. The key insight: when inserting, if the current entry is "richer" (has probed fewer steps) than what we're inserting, we swap them. This "steals from the rich to give to the poor."

```
Probe Length Visualization:

Standard Linear Probing (uneven probe lengths):
Slot:     0   1   2   3   4   5   6   7
Entry:    A   B   C   D   -   E   -   -
Probes:   0   1   2   3       0        <- D probed 3 times!

Robin Hood (equalized probe lengths):
Slot:     0   1   2   3   4   5   6   7
Entry:    A   B   C   D   -   E   -   -
Probes:   0   1   1   1       0        <- maximum reduced

Insertion Example - "steal from the rich":
Current state (numbers show probe distance):
+-------+-------+-------+-------+-------+
| A(0)  | B(1)  | C(2)  | -     | -     |
+-------+-------+-------+-------+-------+
    0       1       2       3       4

Insert D, hash(D)=1, so D wants slot 1
At slot 1: B has probe_dist=1, D has probe_dist=0
D(0) < B(1), so D continues probing

At slot 2: C has probe_dist=2, D has probe_dist=1  
D(1) < C(2), so D continues probing

At slot 3: empty
D(2) inserts here

Final:
+-------+-------+-------+-------+-------+
| A(0)  | B(1)  | C(2)  | D(2)  | -     |
+-------+-------+-------+-------+-------+

Now insert E, hash(E)=1
At slot 1: B(1), E(0) -> E continues
At slot 2: C(2), E(1) -> E continues  
At slot 3: D(2), E(2) -> E continues (tie goes to incumbent)
At slot 4: empty -> E(3) inserts

Hmm, E has probe_dist=3, seems unfair...

ROBIN HOOD VARIANT:
Insert E, hash(E)=1
At slot 1: B(1), E(0) -> E continues
At slot 2: C(2), E(1) -> E continues
At slot 3: D(2), E(2) -> tie, E continues
At slot 4: empty -> E inserts

If instead D had probe_dist=1 when E arrived with probe_dist=2:
E "robs" D, takes D's slot, and D continues looking
```

Robin Hood哈希说明：
- 基于线性探测，但在插入时比较"探测距离"（probe distance）
- 探测距离 = 当前位置 - 理想位置（哈希值对应的槽位）
- 如果待插入元素的探测距离大于当前槽位元素的探测距离，交换它们
- "劫富济贫"：让探测距离大的元素优先占用好位置
- 结果：最大探测距离被均摊，查找时间更稳定
- 删除时可以用"后移法"代替墓碑，避免墓碑累积

**Key properties:**
- Lookup can terminate early: if we've probed farther than the entry at current slot would have, key doesn't exist
- Variance in lookup times is dramatically reduced
- Deletion can be done by backward shifting instead of tombstones

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) |
| Worst-case lookup | O(log n) expected (exponentially unlikely to be worse) |
| Space overhead | Need to store probe distance or compute from hash |
| Cache friendliness | Good - still sequential like linear probing |
| High load factor behavior | **Excellent** - can go to 0.9+ |
| Resize complexity | O(n) |

---

### 3. Pros and Cons

#### Pros
- Very low variance in lookup times
- Supports high load factors (0.9+)
- Early termination during unsuccessful lookup
- Can delete without tombstones (backward shift)
- Cache-friendly like linear probing

#### Cons
- More complex insertion logic (swapping)
- Need to track probe distance (or recompute)
- Backward-shift deletion is O(cluster_size)
- Slightly more expensive insertion

**Implementation Risks:**
- Forgetting to update probe distance after swap
- Incorrect early termination condition
- Backward shift deletion bugs

---

### 4. When to Use

**Good fit when:**
- Need predictable lookup times
- Memory is constrained (want high load factor)
- Latency variance matters (real-time, gaming)
- Read-heavy workload

---

### 5. Real-World Use Cases

- **Rust's hashbrown** (SwissTable) - inspired by Robin Hood principles
- **Redis** - uses variants for internal tables
- **Game engines** - where lookup latency spikes are unacceptable
- **Financial systems** - low-latency trading

---

### 6. Complete Userspace C Example

```c
/*
 * robin_hood.c
 * Compile: gcc -Wall -Wextra -O2 -o robin_hood robin_hood.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INITIAL_SIZE 16
#define MAX_LOAD_FACTOR 0.9  /* Robin Hood handles high load well */

struct ht_entry {
    char *key;
    int value;
    uint32_t hash;      /* Cache hash to avoid recomputation */
    int probe_dist;     /* Distance from ideal slot: -1 means empty */
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

struct hashtable *ht_create(size_t size)
{
    struct hashtable *ht = malloc(sizeof(*ht));
    if (!ht) return NULL;
    
    ht->entries = malloc(size * sizeof(struct ht_entry));
    if (!ht->entries) {
        free(ht);
        return NULL;
    }
    
    /* Initialize all slots as empty (probe_dist = -1) */
    for (size_t i = 0; i < size; i++) {
        ht->entries[i].key = NULL;
        ht->entries[i].probe_dist = -1;
    }
    
    ht->size = size;
    ht->count = 0;
    return ht;
}

/* Internal insertion - used by both insert and resize */
static void ht_insert_internal(struct hashtable *ht, char *key, int value, 
                               uint32_t hash, int probe_dist)
{
    size_t idx = hash % ht->size;
    
    while (1) {
        /* Empty slot - insert here */
        if (ht->entries[idx].probe_dist < 0) {
            ht->entries[idx].key = key;
            ht->entries[idx].value = value;
            ht->entries[idx].hash = hash;
            ht->entries[idx].probe_dist = probe_dist;
            return;
        }
        
        /*
         * Robin Hood: if our probe distance exceeds the incumbent's,
         * we "steal" their slot. The incumbent becomes the new item
         * to insert, continuing from here.
         * 
         * This equalizes probe distances across all entries.
         */
        if (probe_dist > ht->entries[idx].probe_dist) {
            /* Swap our item with incumbent */
            char *tmp_key = ht->entries[idx].key;
            int tmp_value = ht->entries[idx].value;
            uint32_t tmp_hash = ht->entries[idx].hash;
            int tmp_dist = ht->entries[idx].probe_dist;
            
            ht->entries[idx].key = key;
            ht->entries[idx].value = value;
            ht->entries[idx].hash = hash;
            ht->entries[idx].probe_dist = probe_dist;
            
            /* Continue inserting the displaced entry */
            key = tmp_key;
            value = tmp_value;
            hash = tmp_hash;
            probe_dist = tmp_dist;
        }
        
        /* Continue probing */
        idx = (idx + 1) % ht->size;
        probe_dist++;
    }
}

static int ht_resize(struct hashtable *ht, size_t new_size)
{
    struct ht_entry *old = ht->entries;
    size_t old_size = ht->size;
    
    ht->entries = malloc(new_size * sizeof(struct ht_entry));
    if (!ht->entries) {
        ht->entries = old;
        return -1;
    }
    
    for (size_t i = 0; i < new_size; i++) {
        ht->entries[i].key = NULL;
        ht->entries[i].probe_dist = -1;
    }
    
    ht->size = new_size;
    ht->count = 0;
    
    /* Rehash all entries */
    for (size_t i = 0; i < old_size; i++) {
        if (old[i].probe_dist >= 0) {
            ht_insert_internal(ht, old[i].key, old[i].value, old[i].hash, 0);
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
    
    uint32_t hash = hash_string(key);
    size_t idx = hash % ht->size;
    int probe_dist = 0;
    
    /* Check if key already exists */
    while (ht->entries[idx].probe_dist >= 0) {
        if (ht->entries[idx].hash == hash &&
            strcmp(ht->entries[idx].key, key) == 0) {
            ht->entries[idx].value = value;  /* Update existing */
            return 0;
        }
        
        /*
         * Early termination for lookup: if current entry has smaller
         * probe distance than we would have here, key can't exist further.
         */
        if (probe_dist > ht->entries[idx].probe_dist)
            break;
        
        idx = (idx + 1) % ht->size;
        probe_dist++;
    }
    
    /* Key doesn't exist - insert new entry */
    char *key_copy = strdup(key);
    if (!key_copy) return -1;
    
    ht_insert_internal(ht, key_copy, value, hash, probe_dist);
    ht->count++;
    return 0;
}

int *ht_search(struct hashtable *ht, const char *key)
{
    uint32_t hash = hash_string(key);
    size_t idx = hash % ht->size;
    int probe_dist = 0;
    
    while (ht->entries[idx].probe_dist >= 0) {
        /*
         * Robin Hood early termination: if we've probed farther than
         * the entry at this slot, the key we're looking for would have
         * "stolen" this slot during insertion. Since it didn't, the key
         * doesn't exist.
         */
        if (probe_dist > ht->entries[idx].probe_dist)
            return NULL;
        
        if (ht->entries[idx].hash == hash &&
            strcmp(ht->entries[idx].key, key) == 0) {
            return &ht->entries[idx].value;
        }
        
        idx = (idx + 1) % ht->size;
        probe_dist++;
    }
    
    return NULL;
}

/*
 * Robin Hood deletion uses backward shift instead of tombstones.
 * After removing an entry, we shift subsequent entries back to
 * maintain the Robin Hood invariant.
 */
int ht_delete(struct hashtable *ht, const char *key)
{
    uint32_t hash = hash_string(key);
    size_t idx = hash % ht->size;
    int probe_dist = 0;
    
    /* Find the entry */
    while (ht->entries[idx].probe_dist >= 0) {
        if (probe_dist > ht->entries[idx].probe_dist)
            return -1;  /* Not found */
        
        if (ht->entries[idx].hash == hash &&
            strcmp(ht->entries[idx].key, key) == 0) {
            /* Found - delete and backward shift */
            free(ht->entries[idx].key);
            ht->entries[idx].key = NULL;
            ht->entries[idx].probe_dist = -1;
            ht->count--;
            
            /*
             * Backward shift: move subsequent entries back if they
             * benefit from it (probe_dist > 0 means they're displaced).
             */
            size_t empty = idx;
            size_t scan = (idx + 1) % ht->size;
            
            while (ht->entries[scan].probe_dist > 0) {
                /* Move entry back one slot, reducing its probe distance */
                ht->entries[empty] = ht->entries[scan];
                ht->entries[empty].probe_dist--;
                
                ht->entries[scan].key = NULL;
                ht->entries[scan].probe_dist = -1;
                
                empty = scan;
                scan = (scan + 1) % ht->size;
            }
            
            return 0;
        }
        
        idx = (idx + 1) % ht->size;
        probe_dist++;
    }
    
    return -1;  /* Not found */
}

void ht_destroy(struct hashtable *ht)
{
    if (!ht) return;
    
    for (size_t i = 0; i < ht->size; i++) {
        if (ht->entries[i].probe_dist >= 0)
            free(ht->entries[i].key);
    }
    
    free(ht->entries);
    free(ht);
}

void ht_stats(struct hashtable *ht)
{
    int max_probe = 0;
    int total_probe = 0;
    
    for (size_t i = 0; i < ht->size; i++) {
        if (ht->entries[i].probe_dist >= 0) {
            total_probe += ht->entries[i].probe_dist;
            if (ht->entries[i].probe_dist > max_probe)
                max_probe = ht->entries[i].probe_dist;
        }
    }
    
    printf("Size: %zu, Count: %zu, Load: %.2f\n",
           ht->size, ht->count, (double)ht->count / ht->size);
    printf("Max probe distance: %d, Avg: %.2f\n",
           max_probe, ht->count ? (double)total_probe / ht->count : 0.0);
}

int main(void)
{
    struct hashtable *ht = ht_create(INITIAL_SIZE);
    if (!ht) {
        fprintf(stderr, "Failed to create hash table\n");
        return 1;
    }
    
    /* Insert many entries to demonstrate high load factor */
    char buf[32];
    for (int i = 0; i < 50; i++) {
        snprintf(buf, sizeof(buf), "key%d", i);
        ht_insert(ht, buf, i * 10);
    }
    
    printf("After inserting 50 items:\n");
    ht_stats(ht);
    
    /* Lookup tests */
    printf("\nLookup samples:\n");
    for (int i = 0; i < 50; i += 10) {
        snprintf(buf, sizeof(buf), "key%d", i);
        int *val = ht_search(ht, buf);
        printf("  %s -> %d\n", buf, val ? *val : -1);
    }
    
    /* Test early termination on missing key */
    int *missing = ht_search(ht, "nonexistent");
    printf("\n'nonexistent' -> %s\n", missing ? "found (error)" : "not found (ok)");
    
    /* Delete some entries and verify backward shift works */
    printf("\nDeleting key10, key20, key30...\n");
    ht_delete(ht, "key10");
    ht_delete(ht, "key20");
    ht_delete(ht, "key30");
    
    printf("After deletions:\n");
    ht_stats(ht);
    
    /* Verify adjacent keys still findable */
    int *v11 = ht_search(ht, "key11");
    int *v21 = ht_search(ht, "key21");
    printf("key11 -> %d, key21 -> %d\n", v11 ? *v11 : -1, v21 ? *v21 : -1);
    
    /* Verify deleted keys are gone */
    int *v10 = ht_search(ht, "key10");
    printf("key10 (deleted) -> %s\n", v10 ? "found (error)" : "not found (ok)");
    
    ht_destroy(ht);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why Robin Hood here:**
- Demonstrates the "stealing" swap during insertion
- Shows probe distance tracking
- Illustrates backward-shift deletion (no tombstones!)
- Achieves high load factor (0.9) with good stats

**Key advantage:** The max probe distance stays low even at high load. This makes lookup times predictable, which is why game engines and low-latency systems prefer it.

---

## 7. Cuckoo Hashing

### 1. How It Works

Cuckoo hashing uses multiple hash tables (typically 2) with separate hash functions. Each key has exactly one slot in each table. On collision, we "kick out" the incumbent like a cuckoo bird kicks eggs from a nest.

```
Two Tables, Two Hash Functions:

Table 1 (h1):          Table 2 (h2):
+---+---+---+---+      +---+---+---+---+
| A |   | B |   |      |   | B | A |   |
+---+---+---+---+      +---+---+---+---+
  0   1   2   3          0   1   2   3

Entry A: h1(A)=0, h2(A)=2 -> stored at T1[0] or T2[2]
Entry B: h1(B)=2, h2(B)=1 -> stored at T1[2] or T2[1]

Lookup: Check T1[h1(k)] and T2[h2(k)] - at most 2 lookups!

Insert C where h1(C)=0, h2(C)=1:
1. Try T1[0] - occupied by A
2. Kick A out, put C in T1[0]
3. A needs new home: try T2[h2(A)]=T2[2] - empty!
4. Insert A at T2[2]

After insert C:
Table 1:               Table 2:
+---+---+---+---+      +---+---+---+---+
| C |   | B |   |      |   | B | A |   |
+---+---+---+---+      +---+---+---+---+

Eviction chain example:
Insert D where h1(D)=0, h2(D)=2:
1. T1[0] has C -> kick C, insert D at T1[0]
2. C goes to T2[h2(C)]=T2[1] which has B -> kick B
3. B goes to T1[h1(B)]=T1[2] -> already there? Try T2[h2(B)]=T2[1] 
   but we just put C there -> cycle detected!
4. Need to resize or use more hash functions
```

布谷鸟哈希说明：
- 使用多个哈希表（通常2个）和多个哈希函数
- 每个键在每个表中只有一个可能的位置
- 查找是O(1)最坏情况：只需检查2个位置
- 插入时若位置被占，"踢出"当前元素到其备用位置
- 被踢出的元素继续这个过程，形成"踢出链"
- 如果踢出链成环，必须扩容或使用更多哈希函数
- 空间效率约50%（每个表半满）

**Key property:** Lookup is always O(1) worst-case - check at most k positions for k tables.

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) |
| Worst-case lookup | **O(1)** - just k table lookups |
| Space overhead | ~50% utilization typical |
| Cache friendliness | Good - few predictable memory accesses |
| High load factor behavior | Fails at ~50%, must resize |
| Resize complexity | O(n), may need multiple attempts with new hash functions |

---

### 3. Pros and Cons

#### Pros
- **Guaranteed O(1) worst-case lookup**
- No probe sequences to traverse
- Predictable latency
- Simple lookup code

#### Cons
- ~50% space efficiency (can't exceed ~50% load)
- Insert can be slow (long eviction chains)
- Insert can fail, requiring resize
- Need multiple hash functions
- Risk of cycles during insertion

**Implementation Risks:**
- Infinite loop if cycle not detected during eviction
- Must handle resize failures properly
- Need good hash function independence

---

### 4. When to Use

**Good fit when:**
- Need guaranteed O(1) lookup (hard real-time)
- Memory is not extremely constrained
- Can tolerate occasional slow inserts
- Data is relatively static after initial load

---

### 5. Real-World Use Cases

- **Network switches** - hardware CAM tables for MAC address lookup
- **FPGA implementations** - predictable timing
- **Hardware packet filtering** - fixed latency lookups
- **Bloom filter variants** - cuckoo filters
- **Real-time systems** - where worst-case matters

---

### 6. Complete Userspace C Example

```c
/*
 * cuckoo_hashing.c
 * Compile: gcc -Wall -Wextra -O2 -o cuckoo_hashing cuckoo_hashing.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define TABLE_SIZE 16
#define MAX_KICKS 500  /* Max evictions before declaring failure */

struct entry {
    char *key;
    int value;
    int occupied;
};

struct cuckoo_table {
    struct entry *table1;
    struct entry *table2;
    size_t size;
    size_t count;
    uint32_t seed1;  /* Seeds for hash function variation */
    uint32_t seed2;
};

/* Hash function with seed for independence */
static uint32_t hash_with_seed(const char *str, uint32_t seed)
{
    uint32_t hash = seed;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) ^ c;
    return hash;
}

static uint32_t hash1(struct cuckoo_table *ct, const char *key)
{
    return hash_with_seed(key, ct->seed1) % ct->size;
}

static uint32_t hash2(struct cuckoo_table *ct, const char *key)
{
    return hash_with_seed(key, ct->seed2) % ct->size;
}

struct cuckoo_table *ct_create(size_t size)
{
    struct cuckoo_table *ct = malloc(sizeof(*ct));
    if (!ct) return NULL;
    
    ct->table1 = calloc(size, sizeof(struct entry));
    ct->table2 = calloc(size, sizeof(struct entry));
    
    if (!ct->table1 || !ct->table2) {
        free(ct->table1);
        free(ct->table2);
        free(ct);
        return NULL;
    }
    
    ct->size = size;
    ct->count = 0;
    
    /* Use random seeds for hash function independence */
    ct->seed1 = 5381;
    ct->seed2 = 63689;
    
    return ct;
}

/* Forward declaration for rehashing */
static int ct_rehash(struct cuckoo_table *ct);

int ct_insert(struct cuckoo_table *ct, const char *key, int value)
{
    /* Check if already exists */
    uint32_t h1 = hash1(ct, key);
    uint32_t h2 = hash2(ct, key);
    
    if (ct->table1[h1].occupied && strcmp(ct->table1[h1].key, key) == 0) {
        ct->table1[h1].value = value;
        return 0;
    }
    if (ct->table2[h2].occupied && strcmp(ct->table2[h2].key, key) == 0) {
        ct->table2[h2].value = value;
        return 0;
    }
    
    /* Try to insert in table1 first */
    char *key_copy = strdup(key);
    if (!key_copy) return -1;
    
    char *cur_key = key_copy;
    int cur_value = value;
    int use_table1 = 1;
    
    /*
     * Cuckoo insertion: keep kicking out incumbents until
     * we find an empty slot or hit max kicks (cycle).
     */
    for (int kicks = 0; kicks < MAX_KICKS; kicks++) {
        uint32_t idx;
        struct entry *table;
        
        if (use_table1) {
            idx = hash1(ct, cur_key);
            table = ct->table1;
        } else {
            idx = hash2(ct, cur_key);
            table = ct->table2;
        }
        
        if (!table[idx].occupied) {
            /* Empty slot found - insert and done */
            table[idx].key = cur_key;
            table[idx].value = cur_value;
            table[idx].occupied = 1;
            ct->count++;
            return 0;
        }
        
        /*
         * Slot occupied - "cuckoo" the incumbent out.
         * Swap our entry with theirs, then try to place
         * the evicted entry in its alternate location.
         */
        char *evicted_key = table[idx].key;
        int evicted_value = table[idx].value;
        
        table[idx].key = cur_key;
        table[idx].value = cur_value;
        
        cur_key = evicted_key;
        cur_value = evicted_value;
        
        /* Evicted entry goes to its OTHER table */
        use_table1 = !use_table1;
    }
    
    /*
     * Too many kicks - we're in a cycle.
     * Need to rehash with new hash functions.
     */
    printf("Max kicks exceeded, rehashing...\n");
    
    /* Put back the entry we're holding */
    ct->count++;  /* Account for entry we'll add after rehash */
    
    if (ct_rehash(ct) < 0) {
        free(cur_key);
        ct->count--;
        return -1;
    }
    
    /* After rehash, recursively insert the displaced entry */
    int ret = ct_insert(ct, cur_key, cur_value);
    free(cur_key);
    ct->count--;  /* Adjust since recursive call will increment */
    return ret;
}

static int ct_rehash(struct cuckoo_table *ct)
{
    struct entry *old1 = ct->table1;
    struct entry *old2 = ct->table2;
    size_t old_size = ct->size;
    size_t old_count = ct->count;
    
    /* Double size and change hash seeds */
    ct->size *= 2;
    ct->seed1 = ct->seed1 * 31 + 17;
    ct->seed2 = ct->seed2 * 37 + 23;
    ct->count = 0;
    
    ct->table1 = calloc(ct->size, sizeof(struct entry));
    ct->table2 = calloc(ct->size, sizeof(struct entry));
    
    if (!ct->table1 || !ct->table2) {
        free(ct->table1);
        free(ct->table2);
        ct->table1 = old1;
        ct->table2 = old2;
        ct->size = old_size;
        ct->count = old_count;
        return -1;
    }
    
    /* Reinsert all entries */
    for (size_t i = 0; i < old_size; i++) {
        if (old1[i].occupied) {
            if (ct_insert(ct, old1[i].key, old1[i].value) < 0) {
                /* Rehash failed - restore old state */
                /* In production, would need more robust recovery */
                return -1;
            }
            free(old1[i].key);
        }
        if (old2[i].occupied) {
            if (ct_insert(ct, old2[i].key, old2[i].value) < 0) {
                return -1;
            }
            free(old2[i].key);
        }
    }
    
    free(old1);
    free(old2);
    return 0;
}

/*
 * Lookup is O(1) worst-case: check exactly 2 locations.
 * This is the key advantage of cuckoo hashing.
 */
int *ct_search(struct cuckoo_table *ct, const char *key)
{
    uint32_t h1 = hash1(ct, key);
    uint32_t h2 = hash2(ct, key);
    
    /* Check table1 */
    if (ct->table1[h1].occupied && strcmp(ct->table1[h1].key, key) == 0) {
        return &ct->table1[h1].value;
    }
    
    /* Check table2 */
    if (ct->table2[h2].occupied && strcmp(ct->table2[h2].key, key) == 0) {
        return &ct->table2[h2].value;
    }
    
    return NULL;  /* Not in either location */
}

int ct_delete(struct cuckoo_table *ct, const char *key)
{
    uint32_t h1 = hash1(ct, key);
    uint32_t h2 = hash2(ct, key);
    
    if (ct->table1[h1].occupied && strcmp(ct->table1[h1].key, key) == 0) {
        free(ct->table1[h1].key);
        ct->table1[h1].occupied = 0;
        ct->count--;
        return 0;
    }
    
    if (ct->table2[h2].occupied && strcmp(ct->table2[h2].key, key) == 0) {
        free(ct->table2[h2].key);
        ct->table2[h2].occupied = 0;
        ct->count--;
        return 0;
    }
    
    return -1;  /* Not found */
}

void ct_destroy(struct cuckoo_table *ct)
{
    if (!ct) return;
    
    for (size_t i = 0; i < ct->size; i++) {
        if (ct->table1[i].occupied)
            free(ct->table1[i].key);
        if (ct->table2[i].occupied)
            free(ct->table2[i].key);
    }
    
    free(ct->table1);
    free(ct->table2);
    free(ct);
}

void ct_stats(struct cuckoo_table *ct)
{
    size_t t1_used = 0, t2_used = 0;
    
    for (size_t i = 0; i < ct->size; i++) {
        if (ct->table1[i].occupied) t1_used++;
        if (ct->table2[i].occupied) t2_used++;
    }
    
    printf("Table size: %zu, Count: %zu\n", ct->size, ct->count);
    printf("Table1: %zu/%zu, Table2: %zu/%zu\n", 
           t1_used, ct->size, t2_used, ct->size);
    printf("Load factor: %.2f\n", (double)ct->count / (2 * ct->size));
}

int main(void)
{
    struct cuckoo_table *ct = ct_create(TABLE_SIZE);
    if (!ct) {
        fprintf(stderr, "Failed to create cuckoo table\n");
        return 1;
    }
    
    printf("Cuckoo hashing demo\n\n");
    
    /* Insert entries */
    const char *keys[] = {"apple", "banana", "cherry", "date", "elderberry",
                          "fig", "grape", "honeydew", "kiwi", "lemon",
                          "mango", "nectarine", "orange", "papaya", "quince"};
    
    for (int i = 0; i < 15; i++) {
        printf("Inserting %s...\n", keys[i]);
        if (ct_insert(ct, keys[i], i * 10) < 0) {
            fprintf(stderr, "Insert failed for %s\n", keys[i]);
        }
    }
    
    printf("\nTable stats:\n");
    ct_stats(ct);
    
    /* Lookup - demonstrate O(1) worst case */
    printf("\nLookup tests (each is exactly 2 table accesses max):\n");
    for (int i = 0; i < 15; i++) {
        int *val = ct_search(ct, keys[i]);
        printf("  %s -> %d (h1=%u, h2=%u)\n", 
               keys[i], val ? *val : -1,
               hash1(ct, keys[i]), hash2(ct, keys[i]));
    }
    
    int *missing = ct_search(ct, "raspberry");
    printf("  raspberry -> %s\n", missing ? "found (error)" : "not found (ok)");
    
    /* Delete test */
    printf("\nDeleting 'grape' and 'mango'...\n");
    ct_delete(ct, "grape");
    ct_delete(ct, "mango");
    
    int *v1 = ct_search(ct, "grape");
    int *v2 = ct_search(ct, "mango");
    printf("grape -> %s, mango -> %s\n",
           v1 ? "found (error)" : "not found (ok)",
           v2 ? "found (error)" : "not found (ok)");
    
    ct_destroy(ct);
    printf("\nDone.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why Cuckoo hashing here:**
- Shows the two-table structure clearly
- Demonstrates the eviction ("cuckoo") process
- Highlights the O(1) worst-case lookup guarantee
- Shows rehash handling when cycles occur

**Key insight:** Cuckoo hashing trades space (only ~50% load factor) for guaranteed lookup time. When you absolutely need O(1) worst-case, this is the method. When space matters more than worst-case, use Robin Hood.
