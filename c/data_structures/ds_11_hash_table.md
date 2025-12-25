# Hash Table in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE HASH TABLE: O(1) AVERAGE-CASE LOOKUP                        |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Need FAST key-value lookup:                                │
    │  - Symbol tables (name → address)                           │
    │  - Caches (URL → page content)                              │
    │  - Counting (word → frequency)                              │
    │  - Deduplication (seen this before?)                        │
    └─────────────────────────────────────────────────────────────┘

    COMPARISON:
    ┌──────────────────┬─────────────┬─────────────┬──────────────┐
    │ Operation        │ Array       │ BST         │ Hash Table   │
    ├──────────────────┼─────────────┼─────────────┼──────────────┤
    │ Search           │ O(n)        │ O(log n)    │ O(1) avg     │
    │ Insert           │ O(1)        │ O(log n)    │ O(1) avg     │
    │ Delete           │ O(n)        │ O(log n)    │ O(1) avg     │
    │ Ordered iter     │ No          │ Yes         │ No           │
    │ Range query      │ No          │ Yes         │ No           │
    └──────────────────┴─────────────┴─────────────┴──────────────┘

    KEY INSIGHT:
    Use a HASH FUNCTION to compute index directly from key
    
    index = hash(key) % table_size
    
    Direct addressing: O(1) access!
```

**中文解释：**
- **哈希表**：使用哈希函数将键直接映射到数组索引，实现 O(1) 平均查找
- 用途：符号表、缓存、计数、去重
- 代价：无序、无法范围查询

### Hash Function Requirements

```
+------------------------------------------------------------------+
|  GOOD HASH FUNCTION PROPERTIES                                   |
+------------------------------------------------------------------+

    1. DETERMINISTIC
       Same key always produces same hash
       hash("hello") = 12345 (always)

    2. UNIFORM DISTRIBUTION
       Keys spread evenly across table
       Minimizes collisions

    3. FAST TO COMPUTE
       Hash computation should be O(key_length)
       Not a bottleneck

    4. AVALANCHE EFFECT
       Small key change → large hash change
       "hello" and "hellp" should hash very differently

    COMMON HASH FUNCTIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  Integer:    key % table_size (simple but okay)            │
    │  String:     djb2, FNV-1a, MurmurHash                      │
    │  General:    SipHash (security), xxHash (speed)            │
    └─────────────────────────────────────────────────────────────┘
```

### Collision Handling

```
+------------------------------------------------------------------+
|  COLLISION: TWO KEYS HASH TO SAME INDEX                          |
+------------------------------------------------------------------+

    PROBLEM:
    hash("cat") % 10 = 3
    hash("dog") % 10 = 3  ← Collision!

    SOLUTION 1: CHAINING (Separate Chaining)
    ┌─────────────────────────────────────────────────────────────┐
    │  Each bucket holds a linked list of entries                 │
    │                                                              │
    │  [0] → NULL                                                  │
    │  [1] → ("apple", val) → NULL                                │
    │  [2] → NULL                                                  │
    │  [3] → ("cat", val) → ("dog", val) → NULL                   │
    │  [4] → ("egg", val) → NULL                                  │
    │  ...                                                        │
    │                                                              │
    │  + Simple to implement                                      │
    │  + Handles high load gracefully                             │
    │  - Cache unfriendly (linked list traversal)                 │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION 2: OPEN ADDRESSING
    ┌─────────────────────────────────────────────────────────────┐
    │  Find next available slot in the array                     │
    │                                                              │
    │  [0] = empty                                                 │
    │  [1] = ("apple", val)                                       │
    │  [2] = empty                                                 │
    │  [3] = ("cat", val)  ← hash("cat") = 3                     │
    │  [4] = ("dog", val)  ← hash("dog") = 3, slot 3 taken, use 4│
    │  ...                                                        │
    │                                                              │
    │  Probing methods:                                           │
    │  - Linear: try 3, 4, 5, 6, ...                              │
    │  - Quadratic: try 3, 3+1², 3+2², 3+3², ...                  │
    │  - Double hashing: try 3, 3+h₂(key), 3+2·h₂(key), ...      │
    │                                                              │
    │  + Cache friendly (array access)                            │
    │  - Clustering problems                                      │
    │  - Deletion is tricky (tombstones)                          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Memory Model

### Chaining Hash Table Layout

```
+------------------------------------------------------------------+
|  CHAINING HASH TABLE MEMORY LAYOUT                               |
+------------------------------------------------------------------+

    struct hash_entry {
        char *key;              /* Key string (allocated) */
        void *value;            /* Value pointer */
        struct hash_entry *next;/* Chain pointer */
    };

    struct hash_table {
        struct hash_entry **buckets;  /* Array of bucket heads */
        size_t size;                  /* Number of buckets */
        size_t count;                 /* Number of entries */
    };

    MEMORY VIEW:
    
    hash_table struct:           Bucket array:
    ┌─────────────────┐          ┌─────┬─────┬─────┬─────┬─────┐
    │ buckets ────────┼─────────▶│ [0] │ [1] │ [2] │ [3] │ [4] │
    │ size = 5        │          └──┼──┴──┼──┴──┼──┴──┼──┴──┼──┘
    │ count = 4       │             │     │     │     │     │
    └─────────────────┘            NULL   │    NULL   │    NULL
                                          ▼           ▼
                                    ┌─────────┐ ┌─────────┐
                                    │"apple"  │ │"cat"    │
                                    │ val     │ │ val     │
                                    │ next────┼▶│ next ───┼─▶ ┌─────────┐
                                    └─────────┘ └─────────┘   │"dog"    │
                                                              │ val     │
                                                              │ next=NULL│
                                                              └─────────┘
```

### Open Addressing Layout

```
+------------------------------------------------------------------+
|  OPEN ADDRESSING HASH TABLE MEMORY LAYOUT                        |
+------------------------------------------------------------------+

    struct hash_entry {
        char *key;       /* NULL = empty, TOMBSTONE = deleted */
        void *value;
    };

    struct hash_table {
        struct hash_entry *entries;  /* Contiguous array */
        size_t capacity;
        size_t count;                /* Actual entries */
        size_t tombstones;           /* Deleted slots */
    };

    MEMORY VIEW (capacity = 8):
    
    ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
    │ (empty) │ "apple" │ (empty) │ "cat"   │ "dog"   │ (tomb)  │ "egg"   │ (empty) │
    │  NULL   │  val    │  NULL   │  val    │  val    │ DELETED │  val    │  NULL   │
    └─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
       [0]       [1]       [2]       [3]       [4]       [5]       [6]       [7]

    STATES:
    - Empty: key = NULL, never used
    - Occupied: key = valid pointer
    - Tombstone: key = TOMBSTONE, was deleted (can't stop probing here!)
```

### Load Factor and Resizing

```
+------------------------------------------------------------------+
|  LOAD FACTOR & PERFORMANCE                                       |
+------------------------------------------------------------------+

    LOAD FACTOR (α) = count / capacity

    CHAINING:
    ┌─────────────────────────────────────────────────────────────┐
    │  Average chain length = α                                   │
    │  α = 0.5: avg 0.5 comparisons per lookup                   │
    │  α = 1.0: avg 1.0 comparisons (still okay!)                │
    │  α = 2.0: avg 2.0 comparisons (getting slow)               │
    │                                                              │
    │  Resize when α > 1.0 (typically)                            │
    └─────────────────────────────────────────────────────────────┘

    OPEN ADDRESSING:
    ┌─────────────────────────────────────────────────────────────┐
    │  Expected probes = 1/(1-α) for unsuccessful search         │
    │  α = 0.5: ~2 probes                                         │
    │  α = 0.7: ~3.3 probes                                       │
    │  α = 0.9: ~10 probes (too high!)                            │
    │  α = 0.99: ~100 probes (disaster!)                          │
    │                                                              │
    │  MUST keep α < 0.7 (typically resize at 0.5-0.75)          │
    └─────────────────────────────────────────────────────────────┘

    RESIZING:
    ┌─────────────────────────────────────────────────────────────┐
    │  When load factor exceeds threshold:                        │
    │  1. Allocate new table (typically 2× size)                 │
    │  2. Rehash ALL entries (indices change!)                    │
    │  3. Free old table                                          │
    │                                                              │
    │  Cost: O(n), but amortized O(1) over many inserts          │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Typical Application Scenarios

### Where Hash Tables Are Used

```
+------------------------------------------------------------------+
|  HASH TABLE APPLICATIONS                                         |
+------------------------------------------------------------------+

    EVERYWHERE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Python dict, set                                         │
    │  • JavaScript Object, Map                                   │
    │  • Go map                                                   │
    │  • Rust HashMap                                             │
    │  • C++ unordered_map, unordered_set                        │
    └─────────────────────────────────────────────────────────────┘

    SPECIFIC USES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Symbol tables (compiler: name → type/address)            │
    │  • Caching (memoization, LRU cache)                         │
    │  • Deduplication (have I seen this before?)                 │
    │  • Counting (word frequency, histogram)                     │
    │  • Database indexing (hash index)                           │
    │  • Network routing tables                                   │
    │  • Object storage (content-addressable)                     │
    └─────────────────────────────────────────────────────────────┘

    LINUX KERNEL:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Page cache (hash of inode + offset)                     │
    │  • Dentry cache (directory entry lookup)                   │
    │  • PID hash (process lookup)                                │
    │  • Network connection tracking                              │
    └─────────────────────────────────────────────────────────────┘
```

### When NOT to Use Hash Tables

```
+------------------------------------------------------------------+
|  HASH TABLE LIMITATIONS                                          |
+------------------------------------------------------------------+

    DON'T USE WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ Need ordered iteration (use tree)                       │
    │  ✗ Need range queries (use tree)                           │
    │  ✗ Need predecessor/successor (use tree)                   │
    │  ✗ Keys are already integers 0..n (use array!)             │
    │  ✗ Adversarial input possible (hash DoS)                   │
    │  ✗ Memory is very tight (pointer overhead)                  │
    │  ✗ Worst-case O(n) is unacceptable                         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Complete C Examples

### Example 1: Chaining Hash Table

```c
/*
 * Example 1: Hash Table with Chaining
 *
 * String keys, void* values
 * Compile: gcc -Wall -Wextra -o hash_chain hash_chain.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct hash_entry {
    char *key;
    void *value;
    struct hash_entry *next;
};

struct hash_table {
    struct hash_entry **buckets;
    size_t size;
    size_t count;
};

/* djb2 hash function */
static unsigned long hash_djb2(const char *str)
{
    unsigned long hash = 5381;
    int c;
    
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;  /* hash * 33 + c */
    
    return hash;
}

/* Create hash table */
struct hash_table *hash_create(size_t size)
{
    struct hash_table *ht = malloc(sizeof(*ht));
    if (!ht)
        return NULL;
    
    ht->buckets = calloc(size, sizeof(struct hash_entry *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    
    ht->size = size;
    ht->count = 0;
    
    return ht;
}

/* Get bucket index for key */
static size_t get_index(struct hash_table *ht, const char *key)
{
    return hash_djb2(key) % ht->size;
}

/* Insert key-value (returns old value if exists, NULL otherwise) */
void *hash_put(struct hash_table *ht, const char *key, void *value)
{
    size_t index = get_index(ht, key);
    
    /* Search for existing key */
    struct hash_entry *entry = ht->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            void *old_value = entry->value;
            entry->value = value;
            return old_value;
        }
        entry = entry->next;
    }
    
    /* Insert new entry at head of chain */
    entry = malloc(sizeof(*entry));
    if (!entry)
        return NULL;
    
    entry->key = strdup(key);
    entry->value = value;
    entry->next = ht->buckets[index];
    ht->buckets[index] = entry;
    ht->count++;
    
    return NULL;
}

/* Get value for key */
void *hash_get(struct hash_table *ht, const char *key)
{
    size_t index = get_index(ht, key);
    
    struct hash_entry *entry = ht->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0)
            return entry->value;
        entry = entry->next;
    }
    
    return NULL;
}

/* Check if key exists */
bool hash_contains(struct hash_table *ht, const char *key)
{
    return hash_get(ht, key) != NULL;
}

/* Remove key (returns value if found) */
void *hash_remove(struct hash_table *ht, const char *key)
{
    size_t index = get_index(ht, key);
    
    struct hash_entry **pp = &ht->buckets[index];
    while (*pp) {
        struct hash_entry *entry = *pp;
        if (strcmp(entry->key, key) == 0) {
            *pp = entry->next;
            void *value = entry->value;
            free(entry->key);
            free(entry);
            ht->count--;
            return value;
        }
        pp = &entry->next;
    }
    
    return NULL;
}

/* Print table statistics */
void hash_stats(struct hash_table *ht)
{
    size_t empty = 0, max_chain = 0;
    
    for (size_t i = 0; i < ht->size; i++) {
        size_t chain_len = 0;
        for (struct hash_entry *e = ht->buckets[i]; e; e = e->next)
            chain_len++;
        
        if (chain_len == 0)
            empty++;
        if (chain_len > max_chain)
            max_chain = chain_len;
    }
    
    printf("Hash table stats:\n");
    printf("  Buckets: %zu\n", ht->size);
    printf("  Entries: %zu\n", ht->count);
    printf("  Load factor: %.2f\n", (double)ht->count / ht->size);
    printf("  Empty buckets: %zu (%.1f%%)\n", 
           empty, 100.0 * empty / ht->size);
    printf("  Max chain length: %zu\n", max_chain);
}

/* Destroy table */
void hash_destroy(struct hash_table *ht)
{
    for (size_t i = 0; i < ht->size; i++) {
        struct hash_entry *entry = ht->buckets[i];
        while (entry) {
            struct hash_entry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

int main(void)
{
    printf("=== Hash Table (Chaining) Demo ===\n\n");
    
    struct hash_table *ht = hash_create(16);
    
    /* Insert entries */
    printf("Inserting entries:\n");
    hash_put(ht, "apple", "red fruit");
    hash_put(ht, "banana", "yellow fruit");
    hash_put(ht, "cherry", "small red fruit");
    hash_put(ht, "date", "brown fruit");
    hash_put(ht, "elderberry", "purple berry");
    
    /* Lookup */
    printf("\nLookups:\n");
    printf("  apple: %s\n", (char *)hash_get(ht, "apple"));
    printf("  banana: %s\n", (char *)hash_get(ht, "banana"));
    printf("  fig: %s\n", 
           hash_get(ht, "fig") ? (char *)hash_get(ht, "fig") : "(not found)");
    
    /* Update */
    printf("\nUpdating apple...\n");
    hash_put(ht, "apple", "green or red fruit");
    printf("  apple: %s\n", (char *)hash_get(ht, "apple"));
    
    /* Remove */
    printf("\nRemoving banana...\n");
    hash_remove(ht, "banana");
    printf("  banana exists: %s\n", 
           hash_contains(ht, "banana") ? "yes" : "no");
    
    /* Stats */
    printf("\n");
    hash_stats(ht);
    
    hash_destroy(ht);
    return 0;
}
```

---

### Example 2: Open Addressing (Linear Probing)

```c
/*
 * Example 2: Hash Table with Open Addressing
 *
 * Linear probing, tombstone deletion
 * Compile: gcc -Wall -Wextra -o hash_open hash_open.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TOMBSTONE ((char *)-1)
#define LOAD_FACTOR_MAX 0.7

struct hash_entry {
    char *key;    /* NULL = empty, TOMBSTONE = deleted */
    int value;
};

struct hash_table {
    struct hash_entry *entries;
    size_t capacity;
    size_t count;
    size_t tombstones;
};

static unsigned long hash_djb2(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

struct hash_table *hash_create(size_t capacity)
{
    struct hash_table *ht = malloc(sizeof(*ht));
    if (!ht)
        return NULL;
    
    ht->entries = calloc(capacity, sizeof(struct hash_entry));
    if (!ht->entries) {
        free(ht);
        return NULL;
    }
    
    ht->capacity = capacity;
    ht->count = 0;
    ht->tombstones = 0;
    
    return ht;
}

/* Find slot for key (for insert or lookup) */
static size_t find_slot(struct hash_table *ht, const char *key)
{
    size_t index = hash_djb2(key) % ht->capacity;
    size_t first_tombstone = (size_t)-1;
    
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t slot = (index + i) % ht->capacity;
        struct hash_entry *e = &ht->entries[slot];
        
        if (e->key == NULL) {
            /* Empty slot: use first tombstone if found, else this slot */
            return (first_tombstone != (size_t)-1) ? first_tombstone : slot;
        }
        
        if (e->key == TOMBSTONE) {
            if (first_tombstone == (size_t)-1)
                first_tombstone = slot;
            continue;
        }
        
        if (strcmp(e->key, key) == 0)
            return slot;  /* Found existing */
    }
    
    return (first_tombstone != (size_t)-1) ? first_tombstone : (size_t)-1;
}

/* Resize table */
static bool hash_resize(struct hash_table *ht, size_t new_capacity)
{
    struct hash_entry *old_entries = ht->entries;
    size_t old_capacity = ht->capacity;
    
    ht->entries = calloc(new_capacity, sizeof(struct hash_entry));
    if (!ht->entries) {
        ht->entries = old_entries;
        return false;
    }
    
    ht->capacity = new_capacity;
    ht->count = 0;
    ht->tombstones = 0;
    
    /* Rehash all entries */
    for (size_t i = 0; i < old_capacity; i++) {
        struct hash_entry *e = &old_entries[i];
        if (e->key && e->key != TOMBSTONE) {
            size_t slot = find_slot(ht, e->key);
            ht->entries[slot] = *e;
            ht->count++;
        }
    }
    
    free(old_entries);
    return true;
}

/* Insert key-value */
bool hash_put(struct hash_table *ht, const char *key, int value)
{
    /* Check load factor */
    if ((double)(ht->count + ht->tombstones) / ht->capacity > LOAD_FACTOR_MAX) {
        if (!hash_resize(ht, ht->capacity * 2))
            return false;
    }
    
    size_t slot = find_slot(ht, key);
    if (slot == (size_t)-1)
        return false;
    
    struct hash_entry *e = &ht->entries[slot];
    
    if (e->key && e->key != TOMBSTONE) {
        /* Update existing */
        e->value = value;
    } else {
        /* New entry */
        if (e->key == TOMBSTONE)
            ht->tombstones--;
        e->key = strdup(key);
        e->value = value;
        ht->count++;
    }
    
    return true;
}

/* Get value (returns -1 if not found) */
int hash_get(struct hash_table *ht, const char *key)
{
    size_t index = hash_djb2(key) % ht->capacity;
    
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t slot = (index + i) % ht->capacity;
        struct hash_entry *e = &ht->entries[slot];
        
        if (e->key == NULL)
            return -1;  /* Empty = not found */
        
        if (e->key != TOMBSTONE && strcmp(e->key, key) == 0)
            return e->value;
    }
    
    return -1;
}

/* Remove key */
bool hash_remove(struct hash_table *ht, const char *key)
{
    size_t index = hash_djb2(key) % ht->capacity;
    
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t slot = (index + i) % ht->capacity;
        struct hash_entry *e = &ht->entries[slot];
        
        if (e->key == NULL)
            return false;
        
        if (e->key != TOMBSTONE && strcmp(e->key, key) == 0) {
            free(e->key);
            e->key = TOMBSTONE;  /* Can't set to NULL - would break probe chain */
            ht->count--;
            ht->tombstones++;
            return true;
        }
    }
    
    return false;
}

void hash_print(struct hash_table *ht)
{
    printf("Table (capacity=%zu, count=%zu, tombstones=%zu):\n",
           ht->capacity, ht->count, ht->tombstones);
    
    for (size_t i = 0; i < ht->capacity; i++) {
        struct hash_entry *e = &ht->entries[i];
        if (e->key == NULL)
            printf("  [%zu] (empty)\n", i);
        else if (e->key == TOMBSTONE)
            printf("  [%zu] (tombstone)\n", i);
        else
            printf("  [%zu] %s = %d\n", i, e->key, e->value);
    }
}

void hash_destroy(struct hash_table *ht)
{
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->entries[i].key && ht->entries[i].key != TOMBSTONE)
            free(ht->entries[i].key);
    }
    free(ht->entries);
    free(ht);
}

int main(void)
{
    printf("=== Hash Table (Open Addressing) Demo ===\n\n");
    
    struct hash_table *ht = hash_create(8);
    
    /* Insert */
    hash_put(ht, "alice", 25);
    hash_put(ht, "bob", 30);
    hash_put(ht, "carol", 35);
    hash_put(ht, "dave", 28);
    hash_put(ht, "eve", 22);
    
    printf("After insertions:\n");
    hash_print(ht);
    
    /* Lookup */
    printf("\nLookups:\n");
    printf("  alice: %d\n", hash_get(ht, "alice"));
    printf("  bob: %d\n", hash_get(ht, "bob"));
    printf("  zack: %d (not found)\n", hash_get(ht, "zack"));
    
    /* Remove */
    printf("\nRemoving bob...\n");
    hash_remove(ht, "bob");
    hash_print(ht);
    
    /* Search after delete */
    printf("\nLookup carol (after bob deleted): %d\n", hash_get(ht, "carol"));
    
    /* Insert more to trigger resize */
    printf("\nInserting more entries (will resize)...\n");
    hash_put(ht, "frank", 40);
    hash_put(ht, "grace", 45);
    hash_print(ht);
    
    hash_destroy(ht);
    return 0;
}
```

---

### Example 3: Word Frequency Counter

```c
/*
 * Example 3: Word Frequency Counter
 *
 * Real-world use case: counting word occurrences
 * Compile: gcc -Wall -Wextra -o wordcount wordcount.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TABLE_SIZE 1024

struct word_entry {
    char *word;
    int count;
    struct word_entry *next;
};

struct word_counter {
    struct word_entry **buckets;
    size_t total_words;
    size_t unique_words;
};

static unsigned long hash_word(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + tolower(c);
    return hash;
}

struct word_counter *counter_create(void)
{
    struct word_counter *wc = malloc(sizeof(*wc));
    if (!wc)
        return NULL;
    
    wc->buckets = calloc(TABLE_SIZE, sizeof(struct word_entry *));
    if (!wc->buckets) {
        free(wc);
        return NULL;
    }
    
    wc->total_words = 0;
    wc->unique_words = 0;
    return wc;
}

void counter_add(struct word_counter *wc, const char *word)
{
    size_t index = hash_word(word) % TABLE_SIZE;
    
    /* Search for existing word */
    for (struct word_entry *e = wc->buckets[index]; e; e = e->next) {
        if (strcasecmp(e->word, word) == 0) {
            e->count++;
            wc->total_words++;
            return;
        }
    }
    
    /* New word */
    struct word_entry *entry = malloc(sizeof(*entry));
    entry->word = strdup(word);
    entry->count = 1;
    entry->next = wc->buckets[index];
    wc->buckets[index] = entry;
    
    wc->total_words++;
    wc->unique_words++;
}

/* Get top N words */
void counter_top_n(struct word_counter *wc, int n)
{
    /* Collect all entries */
    struct word_entry **all = malloc(wc->unique_words * sizeof(struct word_entry *));
    size_t idx = 0;
    
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        for (struct word_entry *e = wc->buckets[i]; e; e = e->next)
            all[idx++] = e;
    }
    
    /* Simple selection of top N */
    for (int i = 0; i < n && i < (int)wc->unique_words; i++) {
        size_t max_idx = i;
        for (size_t j = i + 1; j < wc->unique_words; j++) {
            if (all[j]->count > all[max_idx]->count)
                max_idx = j;
        }
        struct word_entry *tmp = all[i];
        all[i] = all[max_idx];
        all[max_idx] = tmp;
        
        printf("  %2d. \"%s\" - %d\n", i + 1, all[i]->word, all[i]->count);
    }
    
    free(all);
}

void counter_destroy(struct word_counter *wc)
{
    for (size_t i = 0; i < TABLE_SIZE; i++) {
        struct word_entry *e = wc->buckets[i];
        while (e) {
            struct word_entry *next = e->next;
            free(e->word);
            free(e);
            e = next;
        }
    }
    free(wc->buckets);
    free(wc);
}

int main(void)
{
    printf("=== Word Frequency Counter Demo ===\n\n");
    
    struct word_counter *wc = counter_create();
    
    /* Sample text */
    const char *text = 
        "The quick brown fox jumps over the lazy dog. "
        "The dog was not amused. The fox laughed and ran away. "
        "The quick fox was very quick indeed. Quick quick quick!";
    
    printf("Text:\n%s\n\n", text);
    
    /* Parse and count words */
    char *copy = strdup(text);
    char *token = strtok(copy, " .,!?");
    while (token) {
        counter_add(wc, token);
        token = strtok(NULL, " .,!?");
    }
    free(copy);
    
    printf("Statistics:\n");
    printf("  Total words: %zu\n", wc->total_words);
    printf("  Unique words: %zu\n", wc->unique_words);
    
    printf("\nTop 5 words:\n");
    counter_top_n(wc, 5);
    
    counter_destroy(wc);
    return 0;
}
```

---

### Example 4: Robin Hood Hashing

```c
/*
 * Example 4: Robin Hood Hashing
 *
 * Open addressing with "steal from the rich" optimization
 * Reduces probe variance for more consistent performance
 * Compile: gcc -Wall -Wextra -o robin_hood robin_hood.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define TOMBSTONE ((char *)-1)
#define MAX_LOAD 0.9

struct rh_entry {
    char *key;
    int value;
    size_t psl;  /* Probe Sequence Length (distance from ideal slot) */
};

struct rh_table {
    struct rh_entry *entries;
    size_t capacity;
    size_t count;
};

static unsigned long hash_str(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

struct rh_table *rh_create(size_t capacity)
{
    struct rh_table *ht = malloc(sizeof(*ht));
    ht->entries = calloc(capacity, sizeof(struct rh_entry));
    ht->capacity = capacity;
    ht->count = 0;
    return ht;
}

/*
 * Robin Hood insertion:
 * If new entry has traveled further (higher PSL) than existing entry,
 * swap them and continue inserting the displaced entry.
 * 
 * This evens out probe distances across all entries.
 */
bool rh_put(struct rh_table *ht, const char *key, int value)
{
    if ((double)ht->count / ht->capacity > MAX_LOAD) {
        printf("  (would resize here)\n");
        return false;
    }
    
    size_t index = hash_str(key) % ht->capacity;
    char *insert_key = strdup(key);
    int insert_value = value;
    size_t psl = 0;
    
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t slot = (index + i) % ht->capacity;
        struct rh_entry *e = &ht->entries[slot];
        
        if (e->key == NULL || e->key == TOMBSTONE) {
            /* Empty slot - insert here */
            e->key = insert_key;
            e->value = insert_value;
            e->psl = psl;
            ht->count++;
            return true;
        }
        
        if (strcmp(e->key, insert_key) == 0) {
            /* Update existing */
            free(insert_key);
            e->value = value;
            return true;
        }
        
        /* Robin Hood: if we've traveled further, swap */
        if (psl > e->psl) {
            char *tmp_key = e->key;
            int tmp_value = e->value;
            size_t tmp_psl = e->psl;
            
            e->key = insert_key;
            e->value = insert_value;
            e->psl = psl;
            
            insert_key = tmp_key;
            insert_value = tmp_value;
            psl = tmp_psl;
        }
        
        psl++;
    }
    
    free(insert_key);
    return false;
}

int rh_get(struct rh_table *ht, const char *key)
{
    size_t index = hash_str(key) % ht->capacity;
    
    for (size_t i = 0; i < ht->capacity; i++) {
        size_t slot = (index + i) % ht->capacity;
        struct rh_entry *e = &ht->entries[slot];
        
        if (e->key == NULL)
            return -1;
        
        /* Robin Hood: if PSL < i, key can't be further */
        if (e->key != TOMBSTONE && e->psl < i)
            return -1;
        
        if (e->key != TOMBSTONE && strcmp(e->key, key) == 0)
            return e->value;
    }
    
    return -1;
}

void rh_print(struct rh_table *ht)
{
    printf("Robin Hood table (cap=%zu, count=%zu):\n", 
           ht->capacity, ht->count);
    
    size_t max_psl = 0, total_psl = 0;
    for (size_t i = 0; i < ht->capacity; i++) {
        struct rh_entry *e = &ht->entries[i];
        if (e->key && e->key != TOMBSTONE) {
            printf("  [%zu] %s=%d (psl=%zu)\n", i, e->key, e->value, e->psl);
            total_psl += e->psl;
            if (e->psl > max_psl)
                max_psl = e->psl;
        }
    }
    printf("Max PSL: %zu, Avg PSL: %.2f\n", 
           max_psl, (double)total_psl / ht->count);
}

void rh_destroy(struct rh_table *ht)
{
    for (size_t i = 0; i < ht->capacity; i++) {
        if (ht->entries[i].key && ht->entries[i].key != TOMBSTONE)
            free(ht->entries[i].key);
    }
    free(ht->entries);
    free(ht);
}

int main(void)
{
    printf("=== Robin Hood Hashing Demo ===\n\n");
    
    struct rh_table *ht = rh_create(16);
    
    /* Insert entries */
    const char *names[] = {"alice", "bob", "carol", "dave", "eve",
                           "frank", "grace", "henry", "iris", "jack"};
    
    for (int i = 0; i < 10; i++) {
        rh_put(ht, names[i], (i + 1) * 10);
    }
    
    rh_print(ht);
    
    printf("\nRobin Hood benefit: PSL is more uniform!\n");
    printf("Worst-case lookup is bounded by max PSL, not table size.\n");
    
    rh_destroy(ht);
    return 0;
}
```

---

### Example 5: Hash Function Comparison

```c
/*
 * Example 5: Hash Function Quality Comparison
 *
 * Demonstrates different hash functions and their distribution
 * Compile: gcc -Wall -Wextra -O2 -o hash_compare hash_compare.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TABLE_SIZE 256
#define NUM_WORDS 10000

/* Hash 1: Simple modulo (terrible for strings) */
unsigned long hash_naive(const char *str)
{
    unsigned long hash = 0;
    while (*str)
        hash += *str++;
    return hash;
}

/* Hash 2: djb2 (classic, good general purpose) */
unsigned long hash_djb2(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/* Hash 3: FNV-1a (good distribution, fast) */
unsigned long hash_fnv1a(const char *str)
{
    unsigned long hash = 2166136261u;
    while (*str) {
        hash ^= (unsigned char)*str++;
        hash *= 16777619u;
    }
    return hash;
}

/* Hash 4: SDBM (used in gawk) */
unsigned long hash_sdbm(const char *str)
{
    unsigned long hash = 0;
    int c;
    while ((c = *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}

/* Generate random word */
void random_word(char *buf, int maxlen)
{
    int len = 3 + rand() % (maxlen - 3);
    for (int i = 0; i < len; i++)
        buf[i] = 'a' + rand() % 26;
    buf[len] = '\0';
}

/* Measure distribution quality */
void test_hash(const char *name, unsigned long (*hash_fn)(const char *),
               char **words, int num_words)
{
    int buckets[TABLE_SIZE] = {0};
    
    for (int i = 0; i < num_words; i++) {
        size_t index = hash_fn(words[i]) % TABLE_SIZE;
        buckets[index]++;
    }
    
    /* Calculate statistics */
    int min_bucket = num_words, max_bucket = 0;
    double expected = (double)num_words / TABLE_SIZE;
    double variance = 0;
    int empty = 0;
    
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (buckets[i] < min_bucket)
            min_bucket = buckets[i];
        if (buckets[i] > max_bucket)
            max_bucket = buckets[i];
        if (buckets[i] == 0)
            empty++;
        
        double diff = buckets[i] - expected;
        variance += diff * diff;
    }
    
    variance /= TABLE_SIZE;
    double std_dev = sqrt(variance);
    
    printf("%s:\n", name);
    printf("  Expected per bucket: %.1f\n", expected);
    printf("  Min/Max bucket: %d / %d\n", min_bucket, max_bucket);
    printf("  Empty buckets: %d (%.1f%%)\n", empty, 100.0 * empty / TABLE_SIZE);
    printf("  Std deviation: %.2f (lower is better)\n", std_dev);
    printf("\n");
}

/* For sqrt */
double sqrt(double x)
{
    double guess = x / 2;
    for (int i = 0; i < 20; i++)
        guess = (guess + x / guess) / 2;
    return guess;
}

int main(void)
{
    printf("=== Hash Function Comparison ===\n\n");
    
    srand((unsigned)time(NULL));
    
    /* Generate random words */
    char **words = malloc(NUM_WORDS * sizeof(char *));
    for (int i = 0; i < NUM_WORDS; i++) {
        words[i] = malloc(16);
        random_word(words[i], 15);
    }
    
    printf("Testing with %d random words, %d buckets:\n\n", 
           NUM_WORDS, TABLE_SIZE);
    
    test_hash("Naive (sum of chars)", hash_naive, words, NUM_WORDS);
    test_hash("djb2", hash_djb2, words, NUM_WORDS);
    test_hash("FNV-1a", hash_fnv1a, words, NUM_WORDS);
    test_hash("SDBM", hash_sdbm, words, NUM_WORDS);
    
    printf("─────────────────────────────────────────────\n");
    printf("CONCLUSION:\n");
    printf("  Naive hash is terrible (clustering!)\n");
    printf("  djb2, FNV-1a, SDBM all good for strings\n");
    printf("  For security: use SipHash\n");
    printf("  For speed: use xxHash or MurmurHash3\n");
    
    for (int i = 0; i < NUM_WORDS; i++)
        free(words[i]);
    free(words);
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

```
+------------------------------------------------------------------+
|  HASH TABLE COMPARISON                                           |
+------------------------------------------------------------------+

    CHAINING vs OPEN ADDRESSING:
    ┌─────────────────────┬────────────────────┬───────────────────┐
    │ Aspect              │ Chaining           │ Open Addressing   │
    ├─────────────────────┼────────────────────┼───────────────────┤
    │ Cache behavior      │ Poor               │ Good              │
    │ Memory overhead     │ Pointer per entry  │ Minimal           │
    │ Load factor         │ Can exceed 1.0     │ Must be < 1.0     │
    │ Deletion            │ Simple             │ Tombstones needed │
    │ Implementation      │ Simpler            │ More complex      │
    │ Clustering          │ No                 │ Yes               │
    └─────────────────────┴────────────────────┴───────────────────┘

    HASH TABLE vs TREE:
    ┌─────────────────────┬────────────────────┬───────────────────┐
    │ Aspect              │ Hash Table         │ Balanced Tree     │
    ├─────────────────────┼────────────────────┼───────────────────┤
    │ Search              │ O(1) average       │ O(log n)          │
    │ Worst case          │ O(n)               │ O(log n)          │
    │ Ordered iteration   │ No                 │ Yes               │
    │ Range queries       │ No                 │ Yes               │
    │ Memory              │ Lower              │ Higher            │
    │ Cache behavior      │ Variable           │ Poor              │
    └─────────────────────┴────────────────────┴───────────────────┘
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|  HASH TABLE: KEY TAKEAWAYS                                       |
+------------------------------------------------------------------+

    CORE CONCEPT:
    hash(key) → index for O(1) average lookup

    TWO COLLISION STRATEGIES:
    - Chaining: linked lists at each bucket
    - Open addressing: probe for next slot

    KEY DESIGN CHOICES:
    - Hash function: djb2, FNV-1a for strings
    - Load factor: resize before too full
    - Chaining vs open addressing

    WHEN TO USE:
    ✓ Need O(1) lookup
    ✓ Don't need ordering
    ✓ Don't need range queries
    ✓ Keys are hashable

    WHEN NOT TO USE:
    ✗ Need ordered iteration (use tree)
    ✗ Adversarial input possible (hash DoS)
    ✗ Worst-case O(n) unacceptable

    MODERN VARIANTS:
    - Robin Hood hashing (even probe distribution)
    - Cuckoo hashing (O(1) worst-case lookup)
    - Swiss table (SIMD-accelerated)
```

**中文总结：**
- **核心概念**：hash(key) → 索引，实现 O(1) 平均查找
- **两种冲突策略**：链式（链表）、开放寻址（探测）
- **关键设计**：哈希函数选择、负载因子控制
- **适用场景**：需要快速查找、不需要排序或范围查询
- **现代变体**：Robin Hood、Cuckoo、Swiss Table

