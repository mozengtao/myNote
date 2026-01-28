# Case 2: hlist for Hash Tables

## Subsystem Background

```
+=============================================================================+
|                    LINUX KERNEL HASH LIST (hlist)                            |
+=============================================================================+

    WHY HLIST EXISTS:
    =================

    Regular list_head:                    hlist (hash list):
    ==================                    ==================

    struct list_head {                    struct hlist_head {
        struct list_head *next;               struct hlist_node *first;
        struct list_head *prev;           };  /* Only 8 bytes! */
    };  /* 16 bytes */
                                          struct hlist_node {
                                              struct hlist_node *next;
                                              struct hlist_node **pprev;
                                          };

    PROBLEM: Hash tables have MANY buckets, most empty.
             16 bytes per bucket wastes memory.

    SOLUTION: hlist_head is only 8 bytes (single pointer).
              NULL means empty bucket - very common case.


    HASH TABLE STRUCTURE:
    =====================

    +--------+--------+--------+--------+--------+--------+
    | bucket | bucket | bucket | bucket | bucket | bucket |  <- hlist_head array
    |   0    |   1    |   2    |   3    |   4    |   5    |     (8 bytes each)
    +---+----+--------+---+----+--------+--------+---+----+
        |              |                          |
        v              v                          v
    +-------+      +-------+                  +-------+
    | node  |      | node  |                  | node  |  <- hlist_node
    +---+---+      +---+---+                  +-------+
        |              |
        v              v
    +-------+      +-------+
    | node  |      | node  |
    +-------+      +---+---+
                       |
                       v
                   +-------+
                   | node  |
                   +-------+
```

**中文说明：**

为什么存在hlist：普通`list_head`是16字节，而哈希表有很多桶，大多数是空的，这会浪费内存。hlist的解决方案：`hlist_head`只有8字节（一个指针），NULL表示空桶。哈希表结构：hlist_head数组作为桶，每个非空桶指向一个hlist_node链表。

---

## The pprev Trick

```
    WHY pprev IS A DOUBLE POINTER:
    ==============================

    struct hlist_node {
        struct hlist_node *next;
        struct hlist_node **pprev;  /* <-- Points to POINTER to this node */
    };

    ALLOWS DELETION WITHOUT KNOWING THE HEAD:

    Before deletion of B:
    
    hlist_head         A              B              C
    +-------+      +-------+      +-------+      +-------+
    | first-+----->| next--+----->| next--+----->| next  |->NULL
    +-------+      | pprev |      | pprev |      | pprev |
                   +---|---+      +---|---+      +---|---+
                       |              |              |
                       v              v              v
                   &head.first    &A.next        &B.next

    To delete B:
    1. *(B->pprev) = B->next    // A.next now points to C
    2. if (B->next)
           B->next->pprev = B->pprev  // C.pprev now points to &A.next

    NO NEED TO KNOW if B's predecessor is a hlist_head or hlist_node!
    pprev always points to the pointer that points to this node.
```

**中文说明：**

pprev为什么是双重指针：pprev指向"指向本节点的指针"，这允许在不知道头节点的情况下删除节点。删除时，通过pprev可以直接修改前驱的next指针，无论前驱是hlist_head还是hlist_node。

---

## Minimal C Code Simulation

```c
/*
 * CONTAINER_OF WITH HLIST SIMULATION
 * Demonstrates hash table with container_of for type recovery
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ==========================================================
 * KERNEL-STYLE DEFINITIONS
 * ========================================================== */

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/* Hash list head - only one pointer */
struct hlist_head {
    struct hlist_node *first;
};

/* Hash list node */
struct hlist_node {
    struct hlist_node *next;
    struct hlist_node **pprev;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT

static inline void INIT_HLIST_HEAD(struct hlist_head *h)
{
    h->first = NULL;
}

static inline void INIT_HLIST_NODE(struct hlist_node *n)
{
    n->next = NULL;
    n->pprev = NULL;
}

static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

/* Add node at the beginning of the list */
static inline void hlist_add_head(struct hlist_node *n, 
                                   struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

/* Delete node from list */
static inline void hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

/* Get container from hlist_node */
#define hlist_entry(ptr, type, member) \
    container_of(ptr, type, member)

/* Iterate over hlist */
#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos; pos = pos->next)

/* Iterate getting container */
#define hlist_for_each_entry(pos, head, member)                         \
    for (pos = (head)->first ?                                          \
            hlist_entry((head)->first, typeof(*pos), member) : NULL;    \
         pos;                                                           \
         pos = pos->member.next ?                                       \
            hlist_entry(pos->member.next, typeof(*pos), member) : NULL)

/* ==========================================================
 * HASH TABLE IMPLEMENTATION
 * ========================================================== */

#define HASH_SIZE 8

/* Simple hash function */
static inline unsigned int hash_string(const char *str)
{
    unsigned int hash = 0;
    while (*str)
        hash = hash * 31 + *str++;
    return hash % HASH_SIZE;
}

/* ==========================================================
 * USER STRUCTURE
 * ========================================================== */

struct cache_entry {
    char key[32];
    char value[64];
    struct hlist_node hash_node;  /* EMBEDDED for hash table */
};

/* Global hash table */
static struct hlist_head cache_table[HASH_SIZE];

/* Initialize hash table */
void cache_init(void)
{
    for (int i = 0; i < HASH_SIZE; i++)
        INIT_HLIST_HEAD(&cache_table[i]);
}

/* Add entry to cache */
struct cache_entry *cache_add(const char *key, const char *value)
{
    unsigned int hash = hash_string(key);
    struct cache_entry *entry = malloc(sizeof(*entry));
    
    if (!entry) return NULL;
    
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    strncpy(entry->value, value, sizeof(entry->value) - 1);
    INIT_HLIST_NODE(&entry->hash_node);
    
    hlist_add_head(&entry->hash_node, &cache_table[hash]);
    
    printf("[ADD] '%s' -> '%s' (bucket %u)\n", key, value, hash);
    return entry;
}

/* Find entry in cache - demonstrates container_of */
struct cache_entry *cache_find(const char *key)
{
    unsigned int hash = hash_string(key);
    struct hlist_node *pos;
    
    printf("[FIND] Looking for '%s' in bucket %u\n", key, hash);
    
    hlist_for_each(pos, &cache_table[hash]) {
        /*
         * pos points to hash_node inside some cache_entry.
         * Use container_of to get the cache_entry.
         */
        struct cache_entry *entry = 
            container_of(pos, struct cache_entry, hash_node);
        
        printf("  [CHECK] hash_node at %p -> entry '%s' at %p\n",
               (void *)pos, entry->key, (void *)entry);
        
        if (strcmp(entry->key, key) == 0) {
            printf("  [FOUND] '%s' = '%s'\n", key, entry->value);
            return entry;
        }
    }
    
    printf("  [NOT FOUND]\n");
    return NULL;
}

/* Delete entry from cache */
void cache_delete(const char *key)
{
    struct cache_entry *entry = cache_find(key);
    if (entry) {
        printf("[DELETE] Removing '%s'\n", entry->key);
        hlist_del(&entry->hash_node);
        free(entry);
    }
}

/* Print cache contents */
void cache_print(void)
{
    printf("\n[CACHE CONTENTS]\n");
    
    for (int i = 0; i < HASH_SIZE; i++) {
        printf("  Bucket %d: ", i);
        
        if (hlist_empty(&cache_table[i])) {
            printf("(empty)\n");
            continue;
        }
        
        struct cache_entry *entry;
        hlist_for_each_entry(entry, &cache_table[i], hash_node) {
            printf("'%s'->'%s' ", entry->key, entry->value);
        }
        printf("\n");
    }
    printf("\n");
}

/* ==========================================================
 * DEMONSTRATION
 * ========================================================== */

int main(void)
{
    printf("=================================================\n");
    printf("CONTAINER_OF WITH HLIST DEMONSTRATION\n");
    printf("=================================================\n\n");
    
    cache_init();
    
    /* Add some entries */
    printf("--- Adding entries ---\n");
    cache_add("apple", "fruit");
    cache_add("carrot", "vegetable");
    cache_add("banana", "fruit");
    cache_add("broccoli", "vegetable");
    cache_add("cherry", "fruit");
    cache_add("asparagus", "vegetable");
    
    cache_print();
    
    /* Look up entries */
    printf("--- Looking up entries ---\n");
    cache_find("banana");
    cache_find("carrot");
    cache_find("grape");  /* Not found */
    
    /* Demonstrate structure */
    printf("\n--- Memory structure demonstration ---\n");
    struct cache_entry *e = cache_find("apple");
    if (e) {
        printf("cache_entry at:     %p\n", (void *)e);
        printf("  .key at:          %p\n", (void *)e->key);
        printf("  .value at:        %p\n", (void *)e->value);
        printf("  .hash_node at:    %p\n", (void *)&e->hash_node);
        printf("offsetof(hash_node) = %zu\n", 
               offsetof(struct cache_entry, hash_node));
        printf("\ncontainer_of(&e->hash_node) calculation:\n");
        printf("  %p - %zu = %p\n",
               (void *)&e->hash_node,
               offsetof(struct cache_entry, hash_node),
               (void *)container_of(&e->hash_node, struct cache_entry, hash_node));
    }
    
    /* Delete some entries */
    printf("\n--- Deleting entry ---\n");
    cache_delete("banana");
    
    cache_print();
    
    printf("=================================================\n");
    printf("KEY INSIGHTS:\n");
    printf("- hlist_head is only 8 bytes (saves memory in hash tables)\n");
    printf("- hlist_node uses pprev for O(1) deletion\n");
    printf("- container_of recovers cache_entry from hash_node\n");
    printf("- Same pattern as list_head, optimized for hash tables\n");
    printf("=================================================\n");
    
    return 0;
}
```

---

## hlist vs list_head

```
+=============================================================================+
|                    HLIST vs LIST_HEAD                                        |
+=============================================================================+

    ASPECT              list_head               hlist
    ======              =========               =====

    Head size           16 bytes                8 bytes
                        (next + prev)           (first only)

    Node size           16 bytes                16 bytes
                        (next + prev)           (next + pprev)

    Empty indicator     head == head            first == NULL

    Best for            Lists iterated          Hash buckets
                        both ways               (many empty)

    container_of        list_entry()            hlist_entry()
                        (same as container_of)  (same as container_of)


    MEMORY COMPARISON (1024 bucket hash table):

    Using list_head:    1024 * 16 = 16,384 bytes for heads
    Using hlist_head:   1024 * 8  =  8,192 bytes for heads
    
    SAVINGS: 8 KB just for empty bucket overhead!
```

**中文说明：**

hlist与list_head的对比：hlist_head只有8字节（一个指针），而list_head是16字节。对于有1024个桶的哈希表，使用hlist可以节省8KB内存。hlist适合哈希表（很多空桶），list_head适合需要双向遍历的链表。两者都使用container_of模式。

---

## Real Kernel Examples

### Process ID Hash (kernel/pid.c)

```c
/* PID hash table for fast process lookup */
static struct hlist_head *pid_hash;

struct pid {
    atomic_t count;
    unsigned int level;
    struct hlist_node pid_chain;  /* EMBEDDED for hash */
};

/* Lookup by pid number */
struct pid *find_pid_ns(int nr, struct pid_namespace *ns)
{
    struct hlist_node *elem;
    struct upid *pnr;
    
    hlist_for_each_entry(pnr, elem, 
            &pid_hash[pid_hashfn(nr, ns)], pid_chain) {
        /* container_of used internally by hlist_for_each_entry */
        if (pnr->nr == nr && pnr->ns == ns)
            return container_of(pnr, struct pid, numbers[pnr->ns->level]);
    }
    return NULL;
}
```

### Inode Hash (fs/inode.c)

```c
/* Inode hash for fast lookup */
static struct hlist_head *inode_hashtable;

struct inode {
    struct hlist_node i_hash;  /* EMBEDDED for hash table */
    /* ... */
};
```

---

## Key Takeaways

1. **hlist saves memory**: 8-byte head vs 16-byte list_head
2. **pprev enables O(1) deletion**: Without knowing predecessor type
3. **container_of works the same**: hlist_entry() = container_of()
4. **Ideal for hash tables**: Many empty buckets, one-way traversal
5. **Same embedding pattern**: Node embedded in user structure
