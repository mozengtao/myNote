# Hash Table Implementation Guide - Part 1: Separate Chaining

## 1. Separate Chaining (Linked List Buckets)

### 1. How It Works

Separate chaining resolves collisions by storing multiple entries in the same bucket using a linked list. Each bucket in the hash table array points to the head of a linked list containing all key-value pairs that hash to that bucket index.

```
Memory Layout:
+----------------+
| Hash Table     |
+----------------+
| size: 8        |
| count: 5       |
| buckets[] -----|---> Array of bucket pointers
+----------------+

buckets[]:
+---+---+---+---+---+---+---+---+
| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
+---+---+---+---+---+---+---+---+
  |   |       |
  v   v       v
 [A] [B]     [D]
  |   |
  v   v
 [C] [E]

Each node:
+-------------+
| key   ------|---> "apple"
| value ------|---> 42
| next  ------|---> next node or NULL
+-------------+
```

哈希表内存布局说明：
- 哈希表主结构包含大小(size)、元素计数(count)和桶数组指针(buckets)
- buckets数组的每个元素是一个链表头指针
- 发生哈希冲突时，新节点被插入到对应桶的链表中
- 每个节点包含键指针、值和指向下一个节点的指针

**Lookup Path:**
1. Compute hash of key
2. Index into bucket array: `bucket_idx = hash % size`
3. Traverse linked list at that bucket
4. Compare keys until match or end of list

**Insert Path:**
1. Compute hash and bucket index
2. Check if key exists (traverse list)
3. If exists, update value; if not, prepend or append new node

**Delete Path:**
1. Compute hash and bucket index
2. Traverse list to find node
3. Unlink node from list and free memory

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) with good hash function |
| Worst-case lookup | O(n) if all keys collide |
| Space overhead | sizeof(pointer) per entry + per-node allocation overhead |
| Cache friendliness | Poor - pointer chasing, scattered memory |
| High load factor behavior | Graceful degradation, lists grow longer |
| Resize complexity | O(n) - must rehash all entries |

---

### 3. Pros and Cons

#### Pros
- Simple to implement and understand
- Deletion is straightforward (standard linked list removal)
- Load factor can exceed 1.0 without failure
- No clustering problems
- Works well with any hash function

#### Cons
- Poor cache locality due to pointer chasing
- Memory allocation per node is expensive
- Extra pointer storage per entry
- Memory fragmentation over time
- Worst case O(n) if hash function is bad

**Implementation Risks:**
- Forgetting to free nodes on delete/destroy leads to memory leaks
- Not checking malloc return values
- Hash function returning values larger than bucket count without modulo

---

### 4. When to Use

**Good fit when:**
- Entry count is unpredictable or highly variable
- Deletion is frequent
- Load factor may exceed 1.0
- Implementation simplicity is valued over raw performance
- Keys have variable size (strings)

**Workload patterns:**
- Mixed read/write workloads
- Moderate lookup frequency
- Not latency-critical

**System constraints:**
- Memory fragmentation is acceptable
- Cache misses are tolerable
- Predictable worst-case not required

---

### 5. Real-World Use Cases

- **Linux kernel symbol tables** - The kernel uses chained hash tables for module symbol resolution
- **Compiler symbol tables** - GCC uses chained hashing for identifier lookup
- **DNS caches** - Domain name caching often uses separate chaining
- **Database indexes** - Some database hash indexes use this approach
- **The SELinux hashtab.c** - As shown in the attached file, uses separate chaining for security context lookups

---

### 6. Complete Userspace C Example

```c
/*
 * separate_chaining.c
 * Compile: gcc -Wall -Wextra -O2 -o separate_chaining separate_chaining.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define INITIAL_SIZE 16
#define LOAD_FACTOR_THRESHOLD 0.75

/* Node in the collision chain - allocated separately for each entry */
struct ht_node {
    char *key;              /* Owned copy of key string */
    int value;
    struct ht_node *next;   /* Chain pointer for collision resolution */
};

struct hashtable {
    struct ht_node **buckets;  /* Array of chain head pointers */
    size_t size;               /* Number of buckets */
    size_t count;              /* Number of entries */
};

/* DJB2 hash - simple, fast, reasonable distribution for strings */
static uint32_t hash_string(const char *str)
{
    uint32_t hash = 5381;
    int c;
    
    /* Each character shifts hash left and adds character value.
     * The magic number 33 (hash * 33 = hash << 5 + hash) provides
     * good avalanche properties for ASCII strings. */
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    
    return hash;
}

struct hashtable *ht_create(size_t size)
{
    struct hashtable *ht = malloc(sizeof(*ht));
    if (!ht)
        return NULL;
    
    /* Allocate bucket array - all pointers start as NULL (empty chains) */
    ht->buckets = calloc(size, sizeof(struct ht_node *));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    
    ht->size = size;
    ht->count = 0;
    return ht;
}

/* Internal: resize when load factor exceeded */
static int ht_resize(struct hashtable *ht, size_t new_size)
{
    struct ht_node **new_buckets = calloc(new_size, sizeof(struct ht_node *));
    if (!new_buckets)
        return -1;
    
    /* Rehash all existing entries into new bucket array.
     * This is O(n) but amortized over many insertions. */
    for (size_t i = 0; i < ht->size; i++) {
        struct ht_node *node = ht->buckets[i];
        while (node) {
            struct ht_node *next = node->next;
            
            /* Recompute bucket index for new size */
            uint32_t idx = hash_string(node->key) % new_size;
            
            /* Prepend to new chain - order doesn't matter for correctness */
            node->next = new_buckets[idx];
            new_buckets[idx] = node;
            
            node = next;
        }
    }
    
    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->size = new_size;
    return 0;
}

int ht_insert(struct hashtable *ht, const char *key, int value)
{
    /* Check load factor before insertion - resize proactively
     * to maintain O(1) average case */
    if ((double)ht->count / ht->size > LOAD_FACTOR_THRESHOLD) {
        if (ht_resize(ht, ht->size * 2) < 0)
            return -1;
    }
    
    uint32_t idx = hash_string(key) % ht->size;
    
    /* Search chain for existing key - update if found */
    struct ht_node *node = ht->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0) {
            node->value = value;  /* Update existing */
            return 0;
        }
        node = node->next;
    }
    
    /* Key not found - create new node */
    struct ht_node *new_node = malloc(sizeof(*new_node));
    if (!new_node)
        return -1;
    
    /* Duplicate key string - hashtable owns this memory */
    new_node->key = strdup(key);
    if (!new_node->key) {
        free(new_node);
        return -1;
    }
    
    new_node->value = value;
    
    /* Prepend to chain - O(1) insertion, order doesn't affect correctness */
    new_node->next = ht->buckets[idx];
    ht->buckets[idx] = new_node;
    
    ht->count++;
    return 0;
}

int *ht_search(struct hashtable *ht, const char *key)
{
    uint32_t idx = hash_string(key) % ht->size;
    
    /* Linear search through chain - O(chain_length) */
    struct ht_node *node = ht->buckets[idx];
    while (node) {
        if (strcmp(node->key, key) == 0)
            return &node->value;  /* Return pointer to allow modification */
        node = node->next;
    }
    
    return NULL;
}

int ht_delete(struct hashtable *ht, const char *key)
{
    uint32_t idx = hash_string(key) % ht->size;
    
    struct ht_node *node = ht->buckets[idx];
    struct ht_node *prev = NULL;
    
    while (node) {
        if (strcmp(node->key, key) == 0) {
            /* Unlink from chain */
            if (prev)
                prev->next = node->next;
            else
                ht->buckets[idx] = node->next;  /* Was head of chain */
            
            /* Free node and its owned key string */
            free(node->key);
            free(node);
            ht->count--;
            return 0;
        }
        prev = node;
        node = node->next;
    }
    
    return -1;  /* Key not found */
}

void ht_destroy(struct hashtable *ht)
{
    if (!ht)
        return;
    
    /* Free all chains */
    for (size_t i = 0; i < ht->size; i++) {
        struct ht_node *node = ht->buckets[i];
        while (node) {
            struct ht_node *next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
    }
    
    free(ht->buckets);
    free(ht);
}

/* Debug: print table statistics */
void ht_stats(struct hashtable *ht)
{
    size_t max_chain = 0;
    size_t used_buckets = 0;
    
    for (size_t i = 0; i < ht->size; i++) {
        size_t chain_len = 0;
        struct ht_node *node = ht->buckets[i];
        while (node) {
            chain_len++;
            node = node->next;
        }
        if (chain_len > 0)
            used_buckets++;
        if (chain_len > max_chain)
            max_chain = chain_len;
    }
    
    printf("Buckets: %zu, Used: %zu, Entries: %zu, Max chain: %zu, Load: %.2f\n",
           ht->size, used_buckets, ht->count, max_chain,
           (double)ht->count / ht->size);
}

int main(void)
{
    struct hashtable *ht = ht_create(INITIAL_SIZE);
    if (!ht) {
        fprintf(stderr, "Failed to create hash table\n");
        return 1;
    }
    
    /* Insert some entries */
    const char *keys[] = {"apple", "banana", "cherry", "date", "elderberry",
                          "fig", "grape", "honeydew", "kiwi", "lemon"};
    
    for (int i = 0; i < 10; i++) {
        if (ht_insert(ht, keys[i], i * 10) < 0) {
            fprintf(stderr, "Insert failed for %s\n", keys[i]);
        }
    }
    
    printf("After inserting 10 items:\n");
    ht_stats(ht);
    
    /* Lookup test */
    printf("\nLookup tests:\n");
    for (int i = 0; i < 10; i++) {
        int *val = ht_search(ht, keys[i]);
        if (val)
            printf("  %s -> %d\n", keys[i], *val);
        else
            printf("  %s -> NOT FOUND\n", keys[i]);
    }
    
    /* Test non-existent key */
    int *val = ht_search(ht, "mango");
    printf("  mango -> %s\n", val ? "FOUND (error!)" : "NOT FOUND (correct)");
    
    /* Delete test */
    printf("\nDeleting 'cherry' and 'grape'...\n");
    ht_delete(ht, "cherry");
    ht_delete(ht, "grape");
    
    printf("After deletion:\n");
    ht_stats(ht);
    
    /* Verify deletion */
    val = ht_search(ht, "cherry");
    printf("  cherry -> %s\n", val ? "FOUND (error!)" : "NOT FOUND (correct)");
    
    /* Update existing key */
    printf("\nUpdating 'apple' to 999...\n");
    ht_insert(ht, "apple", 999);
    val = ht_search(ht, "apple");
    printf("  apple -> %d (should be 999)\n", val ? *val : -1);
    
    ht_destroy(ht);
    printf("\nHash table destroyed.\n");
    
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why separate chaining is appropriate here:**
- String keys have variable length, making fixed-size open addressing slots awkward
- The example demonstrates dynamic insertion count - we don't know final size upfront
- Deletion is clean and doesn't leave tombstones or require special handling
- Load factor handling is simple - just grow chains

**What would go wrong with other methods:**
- Open addressing would require tombstones for deletion, complicating the code
- Robin Hood hashing would need complex displacement tracking
- Perfect hashing requires knowing all keys in advance - impossible here

---

## 2. Separate Chaining with Intrusive Lists

### 1. How It Works

Intrusive lists embed the list linkage directly in the data structure rather than allocating separate nodes. The `next` pointer lives inside the user's structure, not in a wrapper node. This eliminates one level of indirection and one malloc per entry.

```
Standard Chaining (non-intrusive):
+------------+     +------------+     +------------+
| ht_node    |     | ht_node    |     | user_data  |
| key -------|---> | key -------|---> | actual     |
| value -----|---> | value -----|---> | fields     |
| next ------|---> | next       |     +------------+
+------------+     +------------+
  Two allocations per entry: node + data

Intrusive Chaining:
+----------------+     +----------------+
| user_struct    |     | user_struct    |
| name[]         |     | name[]         |
| id             |     | id             |
| ht_next -------|---> | ht_next        |---> NULL
+----------------+     +----------------+
  One allocation per entry: data includes linkage

Bucket array points directly into user structures:
buckets[]:
+---+---+---+---+
| 0 | 1 | 2 | 3 |
+---+---+---+---+
  |       |
  v       v
[user1]  [user3]
  |
  v
[user2]
```

侵入式链表内存布局说明：
- 链表的next指针直接嵌入在用户数据结构中
- 桶数组直接指向用户结构体，没有额外的包装节点
- 每个元素只需要一次内存分配
- 用户结构体必须包含哈希表所需的链接字段

**Key difference from standard chaining:**
- No `malloc` for each node - linkage is embedded
- User is responsible for memory management of entries
- Hash table doesn't own the entries, just links them

---

### 2. Characteristics

| Property | Value |
|----------|-------|
| Average lookup | O(1) |
| Worst-case lookup | O(n) |
| Space overhead | sizeof(pointer) per entry (embedded, not extra) |
| Cache friendliness | Better than standard - fewer allocations, data locality |
| High load factor behavior | Same as standard chaining |
| Resize complexity | O(n) |

---

### 3. Pros and Cons

#### Pros
- No per-entry malloc overhead
- Better cache locality - node data is contiguous with linkage
- User controls memory allocation strategy (pool, arena, stack)
- Can embed entries in multiple containers simultaneously
- Zero-copy - entries aren't duplicated

#### Cons
- More complex API - user manages entry memory
- Structure must be modified to include linkage
- Entry can only be in one hash table (per embedded link)
- Easy to corrupt table by freeing entry while still linked

**Implementation Risks:**
- Double-free if entry removed from table but not properly tracked
- Use-after-free if entry freed while still in table
- Must ensure entry lifetime exceeds table membership

---

### 4. When to Use

**Good fit when:**
- Memory allocation overhead is a concern
- Entries are already heap-allocated for other purposes
- Need to embed same object in multiple containers
- Using memory pools or arena allocators
- Performance-critical paths where malloc latency matters

**Workload patterns:**
- High-frequency insert/remove
- Batch operations with pooled memory
- Real-time systems avoiding malloc

---

### 5. Real-World Use Cases

- **Linux kernel** - `hlist_head` and `hlist_node` for kernel hash tables
- **FreeBSD** - Uses intrusive lists extensively in network stack
- **Game engines** - Entity component systems often use intrusive containers
- **Memory allocators** - Free lists are typically intrusive
- **Device drivers** - Hardware descriptor rings

---

### 6. Complete Userspace C Example

```c
/*
 * intrusive_chaining.c
 * Compile: gcc -Wall -Wextra -O2 -o intrusive_chaining intrusive_chaining.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* 
 * container_of - get pointer to containing structure from member pointer
 * This macro is the key to intrusive data structures.
 * Given a pointer to a member, compute the address of the enclosing struct.
 */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* Intrusive list node - embedded in user structures */
struct iht_node {
    struct iht_node *next;
};

struct iht_head {
    struct iht_node *first;
};

struct intrusive_hashtable {
    struct iht_head *buckets;
    size_t size;
    size_t count;
};

/* User-defined structure with embedded hash node */
struct user_entry {
    char name[32];           /* Key */
    int id;                  /* Value */
    struct iht_node ht_link; /* Embedded hash table linkage */
};

static uint32_t hash_string(const char *str)
{
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

struct intrusive_hashtable *iht_create(size_t size)
{
    struct intrusive_hashtable *ht = malloc(sizeof(*ht));
    if (!ht)
        return NULL;
    
    ht->buckets = calloc(size, sizeof(struct iht_head));
    if (!ht->buckets) {
        free(ht);
        return NULL;
    }
    
    ht->size = size;
    ht->count = 0;
    return ht;
}

/*
 * Insert entry into table.
 * Note: caller owns the memory and must not free while entry is in table.
 * The hash table does NOT duplicate the entry.
 */
void iht_insert(struct intrusive_hashtable *ht, struct user_entry *entry)
{
    uint32_t idx = hash_string(entry->name) % ht->size;
    
    /* Prepend to bucket chain - entry's embedded node links into chain */
    entry->ht_link.next = ht->buckets[idx].first;
    ht->buckets[idx].first = &entry->ht_link;
    
    ht->count++;
}

/*
 * Search by key.
 * Returns pointer to user_entry, not the node.
 * Uses container_of to recover enclosing structure from embedded node.
 */
struct user_entry *iht_search(struct intrusive_hashtable *ht, const char *name)
{
    uint32_t idx = hash_string(name) % ht->size;
    
    struct iht_node *node = ht->buckets[idx].first;
    while (node) {
        /* Recover user_entry from embedded ht_link member */
        struct user_entry *entry = container_of(node, struct user_entry, ht_link);
        
        if (strcmp(entry->name, name) == 0)
            return entry;
        
        node = node->next;
    }
    
    return NULL;
}

/*
 * Remove entry from table.
 * Does NOT free the entry - caller retains ownership.
 * Entry can be reused or freed after removal.
 */
int iht_remove(struct intrusive_hashtable *ht, struct user_entry *entry)
{
    uint32_t idx = hash_string(entry->name) % ht->size;
    
    struct iht_node **pp = &ht->buckets[idx].first;
    
    /* Walk chain, keeping pointer to 'next' field that points to current */
    while (*pp) {
        if (*pp == &entry->ht_link) {
            /* Unlink: make previous 'next' skip over this node */
            *pp = entry->ht_link.next;
            entry->ht_link.next = NULL;  /* Clean up removed node */
            ht->count--;
            return 0;
        }
        pp = &(*pp)->next;
    }
    
    return -1;  /* Entry not found in table */
}

void iht_destroy(struct intrusive_hashtable *ht)
{
    /* 
     * Note: We do NOT free entries - they belong to caller.
     * Caller must remove/free entries before destroying table,
     * or track them separately for cleanup.
     */
    free(ht->buckets);
    free(ht);
}

/* Iterate all entries - useful for cleanup */
void iht_foreach(struct intrusive_hashtable *ht,
                 void (*callback)(struct user_entry *entry, void *ctx),
                 void *ctx)
{
    for (size_t i = 0; i < ht->size; i++) {
        struct iht_node *node = ht->buckets[i].first;
        while (node) {
            struct iht_node *next = node->next;  /* Save before callback */
            struct user_entry *entry = container_of(node, struct user_entry, ht_link);
            callback(entry, ctx);
            node = next;
        }
    }
}

/* Allocation helpers for demo - in real code these might use pools */
struct user_entry *user_entry_create(const char *name, int id)
{
    struct user_entry *entry = malloc(sizeof(*entry));
    if (!entry)
        return NULL;
    
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    entry->id = id;
    entry->ht_link.next = NULL;
    
    return entry;
}

void user_entry_destroy(struct user_entry *entry)
{
    free(entry);
}

/* Callback for cleanup iteration */
static void free_entry_cb(struct user_entry *entry, void *ctx)
{
    (void)ctx;
    user_entry_destroy(entry);
}

int main(void)
{
    struct intrusive_hashtable *ht = iht_create(16);
    if (!ht) {
        fprintf(stderr, "Failed to create hash table\n");
        return 1;
    }
    
    /* Create and insert entries */
    const char *names[] = {"alice", "bob", "charlie", "diana", "eve"};
    struct user_entry *entries[5];
    
    for (int i = 0; i < 5; i++) {
        entries[i] = user_entry_create(names[i], i * 100);
        if (!entries[i]) {
            fprintf(stderr, "Failed to create entry\n");
            return 1;
        }
        iht_insert(ht, entries[i]);
    }
    
    printf("Inserted 5 entries.\n\n");
    
    /* Lookup test */
    printf("Lookup tests:\n");
    for (int i = 0; i < 5; i++) {
        struct user_entry *found = iht_search(ht, names[i]);
        if (found)
            printf("  %s -> id=%d\n", found->name, found->id);
        else
            printf("  %s -> NOT FOUND\n", names[i]);
    }
    
    struct user_entry *notfound = iht_search(ht, "frank");
    printf("  frank -> %s\n", notfound ? "FOUND" : "NOT FOUND (correct)");
    
    /* Remove test - we still have the pointer, can free it ourselves */
    printf("\nRemoving 'charlie'...\n");
    struct user_entry *charlie = iht_search(ht, "charlie");
    if (charlie) {
        iht_remove(ht, charlie);
        user_entry_destroy(charlie);  /* We own it, we free it */
        entries[2] = NULL;            /* Clear our reference */
    }
    
    /* Verify removal */
    charlie = iht_search(ht, "charlie");
    printf("  charlie after removal -> %s\n", 
           charlie ? "FOUND (error!)" : "NOT FOUND (correct)");
    
    /* Demonstrate that entry can exist outside table */
    printf("\nCreating standalone entry (not in table)...\n");
    struct user_entry *standalone = user_entry_create("zoe", 999);
    printf("  standalone entry: %s, id=%d\n", standalone->name, standalone->id);
    
    /* Add to table */
    printf("  Adding to table...\n");
    iht_insert(ht, standalone);
    
    struct user_entry *found = iht_search(ht, "zoe");
    printf("  zoe in table -> %s, id=%d\n", 
           found ? found->name : "NOT FOUND", 
           found ? found->id : -1);
    
    /* Cleanup: iterate and free all remaining entries */
    printf("\nCleaning up...\n");
    iht_foreach(ht, free_entry_cb, NULL);
    iht_destroy(ht);
    
    printf("Done.\n");
    return 0;
}
```

---

### 7. Why This Example Fits This Method

**Why intrusive chaining is appropriate here:**
- User entries (`user_entry`) are naturally heap-allocated
- We want to avoid double-allocation (node wrapper + data)
- The `container_of` pattern demonstrates how to recover the full structure
- Memory management is explicit - caller controls lifetime

**What would go wrong with other methods:**
- Standard chaining would require copying user data into nodes
- Open addressing would require fixed-size slots, can't handle variable structures
- Any method that "owns" entries would fight with external ownership patterns

**Key insight:** Intrusive containers are about _linking existing objects_, not _storing copies_. This is essential when objects have identity and lifecycle managed elsewhere.
