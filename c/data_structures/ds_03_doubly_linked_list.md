# Doubly Linked List in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE PROBLEM: BIDIRECTIONAL TRAVERSAL & O(1) DELETION            |
+------------------------------------------------------------------+

    Singly linked list limitations:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Cannot traverse backwards                               │
    │  2. Deletion requires O(n) to find previous node            │
    │  3. Cannot remove node without scanning from head           │
    └─────────────────────────────────────────────────────────────┘

    DOUBLY LINKED LIST SOLUTION:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Each node has BOTH next AND prev pointers                │
    │  • Can traverse in either direction                         │
    │  • O(1) deletion when you have pointer to node              │
    │  • O(1) insertion before or after any node                  │
    └─────────────────────────────────────────────────────────────┘

    TRADE-OFF:
    ┌─────────────────────────────────────────────────────────────┐
    │  Cost:   2× pointer overhead (16 bytes vs 8 bytes per node) │
    │  Gain:   O(1) deletion, bidirectional traversal             │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 双链表解决单链表无法反向遍历和删除需要 O(n) 的问题
- 每个节点有 next 和 prev 两个指针
- 代价：每节点 16 字节指针开销（64位系统）
- 收益：O(1) 删除、双向遍历

### Linux Kernel Implementation Reference

The Linux kernel's `list.h` is the canonical example of a doubly linked list:

```c
/* From linux/include/linux/types.h */
struct list_head {
    struct list_head *next, *prev;
};
```

This is an **intrusive** doubly linked list — the node is embedded in your data structure rather than containing a pointer to data.

### Invariants

```
+------------------------------------------------------------------+
|  DOUBLY LINKED LIST INVARIANTS                                   |
+------------------------------------------------------------------+

    1. BIDIRECTIONAL LINKAGE
       For any node n: n->next->prev == n
       For any node n: n->prev->next == n
       (Except at boundaries in non-circular lists)

    2. CIRCULAR STRUCTURE (Linux kernel style)
       Empty list: head->next == head && head->prev == head
       head->prev points to last element
       last->next points to head

    3. SYMMETRY
       Insert/delete operations are symmetric
       What you do to next, you mirror for prev

    4. SENTINEL HEAD (Linux pattern)
       List has a dedicated head node (not a data node)
       Simplifies insert/delete at boundaries
```

### Design Philosophy

```
+------------------------------------------------------------------+
|  WHY DOUBLY LINKED LISTS ARE SHAPED THIS WAY                     |
+------------------------------------------------------------------+

    CIRCULAR DESIGN (Linux kernel choice):
    ┌─────────────────────────────────────────────────────────────┐
    │  • No NULL pointers to check (simpler code)                 │
    │  • head->prev gives O(1) access to tail                     │
    │  • Insert/delete code is uniform (no special cases)         │
    │  • Empty list check: head->next == head                     │
    └─────────────────────────────────────────────────────────────┘

    INTRUSIVE DESIGN:
    ┌─────────────────────────────────────────────────────────────┐
    │  • No separate allocation for nodes                         │
    │  • Node is part of the containing structure                 │
    │  • container_of() recovers containing structure             │
    │  • One object can be on multiple lists simultaneously!      │
    └─────────────────────────────────────────────────────────────┘

    SENTINEL HEAD:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Head is never NULL                                       │
    │  • Empty list is valid (head points to itself)              │
    │  • No special-case code for first/last insert               │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Memory Model

### Memory Layout (Circular, Intrusive)

```
+------------------------------------------------------------------+
|  DOUBLY LINKED LIST MEMORY LAYOUT (LINUX KERNEL STYLE)           |
+------------------------------------------------------------------+

    Three-element circular list:

                    ┌───────────────────────────────────────┐
                    │                                       │
                    ▼                                       │
    ┌─────────────────────┐     ┌─────────────────────────┐ │
    │      LIST HEAD      │     │       NODE A            │ │
    │  ┌───────┬───────┐  │     │  ┌─────────────────┐    │ │
    │  │ next  │ prev  │  │     │  │   user data     │    │ │
    │  └───┬───┴───┬───┘  │     │  ├───────┬─────────┤    │ │
    │      │       │      │     │  │ next  │  prev   │    │ │
    └──────┼───────┼──────┘     │  └───┬───┴────┬────┘    │ │
           │       │            └──────┼────────┼─────────┘ │
           │       │                   │        │           │
           │       └───────────────────┼────────┘           │
           │                           │                    │
           ▼                           ▼                    │
    ┌─────────────────────┐     ┌─────────────────────┐     │
    │       NODE B        │     │       NODE C        │     │
    │  ┌─────────────────┐│     │  ┌─────────────────┐│     │
    │  │   user data     ││     │  │   user data     ││     │
    │  ├───────┬─────────┤│     │  ├───────┬─────────┤│     │
    │  │ next  │  prev   ││     │  │ next  │  prev   ││     │
    │  └───┬───┴────┬────┘│     │  └───┬───┴────┬────┘│     │
    └──────┼────────┼─────┘     └──────┼────────┼─────┘     │
           │        │                  │        │           │
           │        └──────────────────┘        │           │
           │                                    │           │
           └────────────────────────────────────┼───────────┘
                                                │
                                                └─────▶ back to head

    KEY OBSERVATIONS:
    • Circular: last->next == head, head->prev == last
    • No NULL pointers in the chain
    • List head is a sentinel (no data)
    • Each node's list_head is EMBEDDED in user structure
```

**中文解释：**
- **循环结构**：最后一个节点指向头，头指向第一个节点
- **无 NULL**：链中没有空指针，简化代码
- **哨兵头**：头节点不含数据，只用于标记
- **侵入式**：list_head 嵌入在用户结构中

### Intrusive vs Non-Intrusive

```
+------------------------------------------------------------------+
|  INTRUSIVE vs NON-INTRUSIVE COMPARISON                           |
+------------------------------------------------------------------+

    NON-INTRUSIVE (traditional textbook):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct node {                                              │
    │      void *data;          /* Pointer to user data */        │
    │      struct node *next;                                     │
    │      struct node *prev;                                     │
    │  };                                                         │
    │                                                              │
    │  Problem: Separate allocation, extra indirection           │
    │                                                              │
    │  Memory:                                                    │
    │  ┌─────────────┐     ┌──────────────┐                       │
    │  │ node (heap) │────▶│ data (heap)  │                       │
    │  └─────────────┘     └──────────────┘                       │
    │  Two mallocs per element!                                   │
    └─────────────────────────────────────────────────────────────┘

    INTRUSIVE (Linux kernel):
    ┌─────────────────────────────────────────────────────────────┐
    │  struct list_head {                                         │
    │      struct list_head *next, *prev;                         │
    │  };                                                         │
    │                                                              │
    │  struct my_struct {                                         │
    │      int data;                                              │
    │      struct list_head list;  /* EMBEDDED */                 │
    │  };                                                         │
    │                                                              │
    │  Memory:                                                    │
    │  ┌──────────────────────────────┐                           │
    │  │ my_struct (single allocation)│                           │
    │  │  data | list_head (embedded) │                           │
    │  └──────────────────────────────┘                           │
    │  One malloc per element!                                    │
    └─────────────────────────────────────────────────────────────┘
```

### The container_of Mechanism

```c
/* From linux/include/linux/kernel.h */
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);        \
    (type *)( (char *)__mptr - offsetof(type,member) );})
```

```
+------------------------------------------------------------------+
|  HOW container_of WORKS                                          |
+------------------------------------------------------------------+

    struct task_struct {
        int pid;
        char name[16];
        struct list_head tasks;  ← list_head embedded here
    };

    Memory layout:
    ┌────────────────────────────────────────────────────────────┐
    │ offset 0    │ offset 4     │ offset 20                     │
    │  pid (4B)   │ name (16B)   │ tasks (list_head, 16B)        │
    │             │              │  ┌──────┬───────┐             │
    │             │              │  │ next │ prev  │             │
    │             │              │  └──────┴───────┘             │
    └────────────────────────────────────────────────────────────┘
    ▲                            ▲
    │                            │
    task_struct*                 list_head*
    (what we want)               (what we have)

    container_of(&node->tasks, struct task_struct, tasks):
    1. Get address of 'tasks' member: &node->tasks
    2. Get offset of 'tasks' in task_struct: offsetof(...) = 20
    3. Subtract offset: (char*)&node->tasks - 20 = task_struct*
```

**中文解释：**
- **container_of**：从嵌入的成员指针反推包含它的结构指针
- 计算方法：成员地址 - 成员在结构中的偏移量 = 结构起始地址
- 这是 Linux 内核侵入式数据结构的核心机制

### Cache Behavior

```
+------------------------------------------------------------------+
|  CACHE BEHAVIOR: STILL POOR (BUT BETTER THAN SINGLY)             |
+------------------------------------------------------------------+

    SAME PROBLEM AS SINGLY LINKED:
    ┌─────────────────────────────────────────────────────────────┐
    │  Nodes scattered in heap                                    │
    │  Each node access = potential cache miss                    │
    │  ~100% miss rate in worst case                              │
    └─────────────────────────────────────────────────────────────┘

    SLIGHT ADVANTAGE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Reverse traversal possible (can process backwards)         │
    │  Can scan from both ends simultaneously                     │
    │  Intrusive: node and data in same cache line!               │
    └─────────────────────────────────────────────────────────────┘

    MITIGATION STRATEGIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Allocate nodes from same memory pool (locality)         │
    │  2. Keep list short                                         │
    │  3. Use array if cache matters                              │
    │  4. Prefetch next->next during traversal                    │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Typical Application Scenarios

### Where Doubly Linked Lists Are Used

```
+------------------------------------------------------------------+
|  REAL-WORLD DOUBLY LINKED LIST APPLICATIONS                      |
+------------------------------------------------------------------+

    LINUX KERNEL (primary use case):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Task lists (all processes in system)                     │
    │  • Scheduler runqueues                                      │
    │  • Wait queues                                              │
    │  • Timer lists                                              │
    │  • Device driver lists                                      │
    │  • VFS inode/dentry caches                                  │
    │  • Network socket lists                                     │
    │  • Memory management (page lists, LRU)                      │
    └─────────────────────────────────────────────────────────────┘

    USER SPACE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • LRU caches (move accessed item to front)                 │
    │  • Undo/redo buffers                                        │
    │  • Text editor line buffers                                 │
    │  • Music playlist (next/previous)                           │
    │  • Browser history (back/forward)                           │
    └─────────────────────────────────────────────────────────────┘

    WHY KERNEL USES DOUBLY LINKED:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Objects need to be on MULTIPLE lists                     │
    │  • Removal must be O(1) for performance                     │
    │  • Forward and backward traversal both needed               │
    │  • Objects have complex lifetimes                           │
    └─────────────────────────────────────────────────────────────┘
```

### When to Use Doubly Linked List

```
+------------------------------------------------------------------+
|  USE DOUBLY LINKED LIST WHEN:                                    |
+------------------------------------------------------------------+

    ✓ Need O(1) removal of arbitrary nodes
    ✓ Need bidirectional traversal
    ✓ Objects need to be on multiple lists
    ✓ Implementing LRU cache (move to front on access)
    ✓ Need to splice lists together
    ✓ Order matters and changes frequently
```

### When NOT to Use

```
+------------------------------------------------------------------+
|  AVOID DOUBLY LINKED LIST WHEN:                                  |
+------------------------------------------------------------------+

    ✗ Only forward traversal needed (use singly linked)
    ✗ Random access needed (use array)
    ✗ Cache performance critical (use array)
    ✗ Memory very constrained (16B overhead per element)
    ✗ Simple stack/queue operations (singly linked is enough)
```

---

## 4. Complete C Examples

### Example 1: Linux Kernel Style Intrusive List

```c
/*
 * Example 1: Linux Kernel Style Doubly Linked List
 *
 * Simplified implementation matching kernel's list.h
 * Compile: gcc -Wall -Wextra -o kernel_list kernel_list.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * Core list_head structure (same as kernel)
 * ═══════════════════════════════════════════════════════════════ */
struct list_head {
    struct list_head *next, *prev;
};

/* ═══════════════════════════════════════════════════════════════
 * Macros matching kernel's list.h
 * ═══════════════════════════════════════════════════════════════ */

/* Static initialization */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

/* Runtime initialization */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/* Check if list is empty */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/* Internal: insert between two known entries */
static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/* Add after head (for stack-like behavior) */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

/* Add before head (at tail, for queue-like behavior) */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/* Internal: connect prev and next, excluding entry between them */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/* Remove entry from list */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    /* Poison pointers to catch use-after-remove bugs */
    entry->next = (void *)0xDEADBEEF;
    entry->prev = (void *)0xDEADBEEF;
}

/* Remove and reinitialize (can be added to another list) */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/* Move entry to after head */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/* Move entry to before head (tail) */
static inline void list_move_tail(struct list_head *list, 
                                   struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/* ═══════════════════════════════════════════════════════════════
 * container_of macro
 * ═══════════════════════════════════════════════════════════════ */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* Get containing structure from list_head pointer */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/* Get first entry */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/* Iterate over list */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* Iterate and get containing structure */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* Safe iteration (allows removal during iteration) */
#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member), \
         n = list_entry(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* ═══════════════════════════════════════════════════════════════
 * Example: Process list (like kernel's task_struct)
 * ═══════════════════════════════════════════════════════════════ */
struct process {
    int pid;
    char name[32];
    int priority;
    struct list_head list;  /* Embedded list node */
};

struct process *process_create(int pid, const char *name, int priority)
{
    struct process *p = malloc(sizeof(*p));
    if (!p) return NULL;
    
    p->pid = pid;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->name[sizeof(p->name) - 1] = '\0';
    p->priority = priority;
    INIT_LIST_HEAD(&p->list);
    
    return p;
}

void process_print(struct process *p)
{
    printf("  PID=%d, Name=\"%s\", Priority=%d\n",
           p->pid, p->name, p->priority);
}

int main(void)
{
    /* Declare list head */
    LIST_HEAD(process_list);
    
    /* Create processes */
    struct process *init = process_create(1, "init", 0);
    struct process *bash = process_create(100, "bash", 20);
    struct process *vim = process_create(200, "vim", 10);
    struct process *gcc = process_create(300, "gcc", 15);
    
    /* Add to list */
    printf("=== Adding processes ===\n");
    list_add_tail(&init->list, &process_list);
    list_add_tail(&bash->list, &process_list);
    list_add_tail(&vim->list, &process_list);
    list_add_tail(&gcc->list, &process_list);
    
    /* Iterate and print */
    printf("\n=== Process List ===\n");
    struct process *pos;
    list_for_each_entry(pos, &process_list, list) {
        process_print(pos);
    }
    
    /* Move vim to front (highest priority) */
    printf("\n=== Move vim to front ===\n");
    list_move(&vim->list, &process_list);
    list_for_each_entry(pos, &process_list, list) {
        process_print(pos);
    }
    
    /* Remove bash */
    printf("\n=== Remove bash ===\n");
    list_del(&bash->list);
    free(bash);
    list_for_each_entry(pos, &process_list, list) {
        process_print(pos);
    }
    
    /* Safe deletion of all processes */
    printf("\n=== Cleanup (safe iteration) ===\n");
    struct process *tmp;
    list_for_each_entry_safe(pos, tmp, &process_list, list) {
        printf("Freeing PID %d\n", pos->pid);
        list_del(&pos->list);
        free(pos);
    }
    
    printf("List empty: %s\n", list_empty(&process_list) ? "yes" : "no");
    
    return 0;
}
```

**Output:**
```
=== Adding processes ===

=== Process List ===
  PID=1, Name="init", Priority=0
  PID=100, Name="bash", Priority=20
  PID=200, Name="vim", Priority=10
  PID=300, Name="gcc", Priority=15

=== Move vim to front ===
  PID=200, Name="vim", Priority=10
  PID=1, Name="init", Priority=0
  PID=100, Name="bash", Priority=20
  PID=300, Name="gcc", Priority=15

=== Remove bash ===
  PID=200, Name="vim", Priority=10
  PID=1, Name="init", Priority=0
  PID=300, Name="gcc", Priority=15

=== Cleanup (safe iteration) ===
Freeing PID 200
Freeing PID 1
Freeing PID 300
List empty: yes
```

---

### Example 2: LRU Cache Using Doubly Linked List

```c
/*
 * Example 2: LRU (Least Recently Used) Cache
 *
 * Classic doubly linked list application:
 * - Move accessed item to front (O(1))
 * - Evict from tail when full (O(1))
 *
 * Compile: gcc -Wall -Wextra -o lru_cache lru_cache.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* List implementation (simplified from Example 1) */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list; list->prev = list;
}

static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next) {
    next->prev = new; new->next = next;
    new->prev = prev; prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
    __list_add(new, head, head->next);
}

static inline void __list_del(struct list_head *prev, struct list_head *next) {
    next->prev = prev; prev->next = next;
}

static inline void list_del(struct list_head *entry) {
    __list_del(entry->prev, entry->next);
}

static inline void list_move(struct list_head *list, struct list_head *head) {
    __list_del(list->prev, list->next);
    list_add(list, head);
}

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* ═══════════════════════════════════════════════════════════════
 * LRU Cache Implementation
 * ═══════════════════════════════════════════════════════════════ */

struct cache_entry {
    int key;
    int value;
    struct list_head lru_list;  /* Position in LRU order */
};

struct lru_cache {
    struct list_head lru_head;  /* Most recent at front */
    struct cache_entry **table; /* Hash table for O(1) lookup */
    size_t table_size;
    size_t capacity;
    size_t size;
};

/* Simple hash function */
static size_t hash(int key, size_t table_size)
{
    return (unsigned int)key % table_size;
}

/* Create cache */
struct lru_cache *cache_create(size_t capacity)
{
    struct lru_cache *cache = malloc(sizeof(*cache));
    if (!cache) return NULL;
    
    cache->table_size = capacity * 2;  /* Load factor 0.5 */
    cache->table = calloc(cache->table_size, sizeof(*cache->table));
    if (!cache->table) {
        free(cache);
        return NULL;
    }
    
    INIT_LIST_HEAD(&cache->lru_head);
    cache->capacity = capacity;
    cache->size = 0;
    
    return cache;
}

/* Find entry in hash table */
static struct cache_entry *cache_find(struct lru_cache *cache, int key)
{
    size_t idx = hash(key, cache->table_size);
    
    /* Linear probing */
    for (size_t i = 0; i < cache->table_size; i++) {
        size_t probe = (idx + i) % cache->table_size;
        if (!cache->table[probe])
            return NULL;
        if (cache->table[probe]->key == key)
            return cache->table[probe];
    }
    return NULL;
}

/* Insert into hash table */
static void cache_insert_table(struct lru_cache *cache, struct cache_entry *entry)
{
    size_t idx = hash(entry->key, cache->table_size);
    
    for (size_t i = 0; i < cache->table_size; i++) {
        size_t probe = (idx + i) % cache->table_size;
        if (!cache->table[probe]) {
            cache->table[probe] = entry;
            return;
        }
    }
}

/* Remove from hash table */
static void cache_remove_table(struct lru_cache *cache, int key)
{
    size_t idx = hash(key, cache->table_size);
    
    for (size_t i = 0; i < cache->table_size; i++) {
        size_t probe = (idx + i) % cache->table_size;
        if (cache->table[probe] && cache->table[probe]->key == key) {
            cache->table[probe] = NULL;
            return;
        }
    }
}

/* Get value (returns -1 if not found) */
int cache_get(struct lru_cache *cache, int key)
{
    struct cache_entry *entry = cache_find(cache, key);
    if (!entry)
        return -1;
    
    /* Move to front of LRU list (most recently used) */
    list_move(&entry->lru_list, &cache->lru_head);
    
    return entry->value;
}

/* Put key-value pair */
void cache_put(struct lru_cache *cache, int key, int value)
{
    struct cache_entry *entry = cache_find(cache, key);
    
    if (entry) {
        /* Update existing entry */
        entry->value = value;
        list_move(&entry->lru_list, &cache->lru_head);
        return;
    }
    
    /* Evict if at capacity */
    if (cache->size >= cache->capacity) {
        /* Remove least recently used (at tail) */
        struct list_head *tail = cache->lru_head.prev;
        struct cache_entry *victim = list_entry(tail, struct cache_entry, lru_list);
        
        printf("  [Evicting key=%d]\n", victim->key);
        
        list_del(&victim->lru_list);
        cache_remove_table(cache, victim->key);
        free(victim);
        cache->size--;
    }
    
    /* Create new entry */
    entry = malloc(sizeof(*entry));
    entry->key = key;
    entry->value = value;
    
    /* Add to front of LRU list */
    list_add(&entry->lru_list, &cache->lru_head);
    cache_insert_table(cache, entry);
    cache->size++;
}

/* Print cache state */
void cache_print(struct lru_cache *cache)
{
    printf("Cache (size=%zu/%zu) [MRU -> LRU]: ",
           cache->size, cache->capacity);
    
    struct cache_entry *pos;
    list_for_each_entry(pos, &cache->lru_head, lru_list) {
        printf("(%d:%d) ", pos->key, pos->value);
    }
    printf("\n");
}

/* Destroy cache */
void cache_destroy(struct lru_cache *cache)
{
    struct list_head *pos, *tmp;
    for (pos = cache->lru_head.next; pos != &cache->lru_head; ) {
        tmp = pos->next;
        struct cache_entry *entry = list_entry(pos, struct cache_entry, lru_list);
        free(entry);
        pos = tmp;
    }
    free(cache->table);
    free(cache);
}

int main(void)
{
    printf("=== LRU Cache Demo (capacity=3) ===\n\n");
    
    struct lru_cache *cache = cache_create(3);
    
    printf("Put (1, 100), (2, 200), (3, 300):\n");
    cache_put(cache, 1, 100);
    cache_put(cache, 2, 200);
    cache_put(cache, 3, 300);
    cache_print(cache);
    
    printf("\nGet key=1 (moves to front):\n");
    printf("  Value: %d\n", cache_get(cache, 1));
    cache_print(cache);
    
    printf("\nPut (4, 400) - should evict key=2:\n");
    cache_put(cache, 4, 400);
    cache_print(cache);
    
    printf("\nGet key=2 (should be gone):\n");
    printf("  Value: %d (not found)\n", cache_get(cache, 2));
    
    printf("\nPut (5, 500) - should evict key=3:\n");
    cache_put(cache, 5, 500);
    cache_print(cache);
    
    cache_destroy(cache);
    
    return 0;
}
```

**Output:**
```
=== LRU Cache Demo (capacity=3) ===

Put (1, 100), (2, 200), (3, 300):
Cache (size=3/3) [MRU -> LRU]: (3:300) (2:200) (1:100) 

Get key=1 (moves to front):
  Value: 100
Cache (size=3/3) [MRU -> LRU]: (1:100) (3:300) (2:200) 

Put (4, 400) - should evict key=2:
  [Evicting key=2]
Cache (size=3/3) [MRU -> LRU]: (4:400) (1:100) (3:300) 

Get key=2 (should be gone):
  Value: -1 (not found)

Put (5, 500) - should evict key=3:
  [Evicting key=3]
Cache (size=3/3) [MRU -> LRU]: (5:500) (4:400) (1:100)
```

---

### Example 3: Object on Multiple Lists

```c
/*
 * Example 3: Object on Multiple Lists Simultaneously
 *
 * Key advantage of intrusive lists: one object can be on many lists!
 * Example: File can be in both "all files" and "open files" lists
 *
 * Compile: gcc -Wall -Wextra -o multi_list multi_list.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* List implementation */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list; list->prev = list;
}
static inline void __list_add(struct list_head *new, 
                              struct list_head *prev, 
                              struct list_head *next) {
    next->prev = new; new->next = next;
    new->prev = prev; prev->next = new;
}
static inline void list_add_tail(struct list_head *new, struct list_head *head) {
    __list_add(new, head->prev, head);
}
static inline void list_del(struct list_head *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = entry->prev = NULL;
}
static inline int list_empty(struct list_head *head) {
    return head->next == head;
}

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#define list_entry(ptr, type, member) container_of(ptr, type, member)
#define list_for_each_entry(pos, head, member) \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* ═══════════════════════════════════════════════════════════════
 * File structure with MULTIPLE list nodes
 * ═══════════════════════════════════════════════════════════════ */
struct file {
    char name[32];
    int fd;
    int is_open;
    
    /* This file can be on MULTIPLE lists simultaneously! */
    struct list_head all_files;   /* Link for "all files" list */
    struct list_head open_files;  /* Link for "open files" list */
    struct list_head dirty_files; /* Link for "dirty files" list */
};

/* Global lists */
LIST_HEAD(all_files_list);
LIST_HEAD(open_files_list);
LIST_HEAD(dirty_files_list);

struct file *file_create(const char *name, int fd)
{
    struct file *f = malloc(sizeof(*f));
    strncpy(f->name, name, sizeof(f->name) - 1);
    f->fd = fd;
    f->is_open = 0;
    
    /* Initialize all list nodes */
    INIT_LIST_HEAD(&f->all_files);
    INIT_LIST_HEAD(&f->open_files);
    INIT_LIST_HEAD(&f->dirty_files);
    
    /* Add to "all files" list */
    list_add_tail(&f->all_files, &all_files_list);
    
    return f;
}

void file_open(struct file *f)
{
    if (!f->is_open) {
        f->is_open = 1;
        list_add_tail(&f->open_files, &open_files_list);
        printf("Opened: %s\n", f->name);
    }
}

void file_close(struct file *f)
{
    if (f->is_open) {
        f->is_open = 0;
        list_del(&f->open_files);
        INIT_LIST_HEAD(&f->open_files);  /* Reinitialize for reuse */
        printf("Closed: %s\n", f->name);
    }
}

void file_mark_dirty(struct file *f)
{
    /* Only add if not already dirty */
    if (list_empty(&f->dirty_files)) {
        list_add_tail(&f->dirty_files, &dirty_files_list);
        printf("Marked dirty: %s\n", f->name);
    }
}

void file_sync(struct file *f)
{
    if (!list_empty(&f->dirty_files)) {
        list_del(&f->dirty_files);
        INIT_LIST_HEAD(&f->dirty_files);
        printf("Synced: %s\n", f->name);
    }
}

void print_all_files(void)
{
    printf("All files: ");
    struct file *f;
    list_for_each_entry(f, &all_files_list, all_files) {
        printf("%s ", f->name);
    }
    printf("\n");
}

void print_open_files(void)
{
    printf("Open files: ");
    struct file *f;
    list_for_each_entry(f, &open_files_list, open_files) {
        printf("%s ", f->name);
    }
    printf("\n");
}

void print_dirty_files(void)
{
    printf("Dirty files: ");
    struct file *f;
    list_for_each_entry(f, &dirty_files_list, dirty_files) {
        printf("%s ", f->name);
    }
    printf("\n");
}

int main(void)
{
    printf("=== Multiple Lists Demo ===\n\n");
    
    /* Create files */
    struct file *f1 = file_create("config.txt", 1);
    struct file *f2 = file_create("data.bin", 2);
    struct file *f3 = file_create("log.txt", 3);
    struct file *f4 = file_create("temp.tmp", 4);
    
    print_all_files();
    printf("\n");
    
    /* Open some files */
    printf("--- Opening files ---\n");
    file_open(f1);
    file_open(f2);
    file_open(f4);
    print_open_files();
    printf("\n");
    
    /* Mark some dirty */
    printf("--- Marking dirty ---\n");
    file_mark_dirty(f1);
    file_mark_dirty(f4);
    print_dirty_files();
    printf("\n");
    
    /* Show all lists */
    printf("--- Current state ---\n");
    print_all_files();
    print_open_files();
    print_dirty_files();
    printf("\n");
    
    /* Sync dirty files */
    printf("--- Syncing ---\n");
    file_sync(f1);
    print_dirty_files();
    printf("\n");
    
    /* Close a file */
    printf("--- Closing f2 ---\n");
    file_close(f2);
    print_open_files();
    
    /* Note: f2 is still in all_files, just not in open_files! */
    printf("\n--- f2 still in all files ---\n");
    print_all_files();
    
    /* Cleanup */
    free(f1); free(f2); free(f3); free(f4);
    
    return 0;
}
```

---

### Example 4: Non-Intrusive Doubly Linked List

```c
/*
 * Example 4: Non-Intrusive Doubly Linked List
 *
 * Traditional textbook style (for comparison)
 * Demonstrates why intrusive is often better
 *
 * Compile: gcc -Wall -Wextra -o nonintrusive_dlist nonintrusive_dlist.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════
 * Non-intrusive node: contains pointer to data
 * ═══════════════════════════════════════════════════════════════ */
struct dlist_node {
    void *data;             /* Pointer to user data */
    struct dlist_node *next;
    struct dlist_node *prev;
};

struct dlist {
    struct dlist_node *head;
    struct dlist_node *tail;
    size_t size;
    void (*free_data)(void *);  /* Callback to free user data */
};

/* Initialize list */
void dlist_init(struct dlist *list, void (*free_func)(void *))
{
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->free_data = free_func;
}

/* Check if empty */
bool dlist_empty(struct dlist *list)
{
    return list->head == NULL;
}

/* Add to front */
bool dlist_push_front(struct dlist *list, void *data)
{
    struct dlist_node *node = malloc(sizeof(*node));
    if (!node) return false;
    
    node->data = data;
    node->prev = NULL;
    node->next = list->head;
    
    if (list->head)
        list->head->prev = node;
    else
        list->tail = node;  /* First element */
    
    list->head = node;
    list->size++;
    return true;
}

/* Add to back */
bool dlist_push_back(struct dlist *list, void *data)
{
    struct dlist_node *node = malloc(sizeof(*node));
    if (!node) return false;
    
    node->data = data;
    node->next = NULL;
    node->prev = list->tail;
    
    if (list->tail)
        list->tail->next = node;
    else
        list->head = node;  /* First element */
    
    list->tail = node;
    list->size++;
    return true;
}

/* Remove from front */
void *dlist_pop_front(struct dlist *list)
{
    if (dlist_empty(list)) return NULL;
    
    struct dlist_node *node = list->head;
    void *data = node->data;
    
    list->head = node->next;
    if (list->head)
        list->head->prev = NULL;
    else
        list->tail = NULL;  /* List is now empty */
    
    free(node);
    list->size--;
    return data;
}

/* Remove from back */
void *dlist_pop_back(struct dlist *list)
{
    if (dlist_empty(list)) return NULL;
    
    struct dlist_node *node = list->tail;
    void *data = node->data;
    
    list->tail = node->prev;
    if (list->tail)
        list->tail->next = NULL;
    else
        list->head = NULL;
    
    free(node);
    list->size--;
    return data;
}

/* Remove specific node (O(1) when you have the node) */
void dlist_remove_node(struct dlist *list, struct dlist_node *node)
{
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    
    if (list->free_data)
        list->free_data(node->data);
    free(node);
    list->size--;
}

/* Find node by data (O(n)) */
struct dlist_node *dlist_find(struct dlist *list, void *data,
                               int (*cmp)(void *, void *))
{
    for (struct dlist_node *n = list->head; n; n = n->next) {
        if (cmp(n->data, data) == 0)
            return n;
    }
    return NULL;
}

/* Iterate with callback */
void dlist_foreach(struct dlist *list, void (*callback)(void *))
{
    for (struct dlist_node *n = list->head; n; n = n->next) {
        callback(n->data);
    }
}

/* Destroy list */
void dlist_destroy(struct dlist *list)
{
    while (!dlist_empty(list)) {
        void *data = dlist_pop_front(list);
        if (list->free_data)
            list->free_data(data);
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Demo with integer data
 * ═══════════════════════════════════════════════════════════════ */
void print_int(void *data)
{
    printf("%d ", *(int *)data);
}

int cmp_int(void *a, void *b)
{
    return *(int *)a - *(int *)b;
}

int main(void)
{
    struct dlist list;
    dlist_init(&list, free);  /* Use free() for data cleanup */
    
    /* Add elements */
    printf("=== Adding elements ===\n");
    for (int i = 1; i <= 5; i++) {
        int *val = malloc(sizeof(int));
        *val = i * 10;
        dlist_push_back(&list, val);
    }
    
    printf("List: ");
    dlist_foreach(&list, print_int);
    printf("\n");
    
    /* Pop front and back */
    printf("\n=== Pop operations ===\n");
    int *front = dlist_pop_front(&list);
    printf("Pop front: %d\n", *front);
    free(front);
    
    int *back = dlist_pop_back(&list);
    printf("Pop back: %d\n", *back);
    free(back);
    
    printf("List: ");
    dlist_foreach(&list, print_int);
    printf("\n");
    
    /* Find and remove */
    printf("\n=== Find and remove 30 ===\n");
    int target = 30;
    struct dlist_node *node = dlist_find(&list, &target, cmp_int);
    if (node) {
        dlist_remove_node(&list, node);  /* O(1) removal! */
        printf("Removed 30\n");
    }
    
    printf("List: ");
    dlist_foreach(&list, print_int);
    printf("\n");
    
    /* Cleanup */
    dlist_destroy(&list);
    
    return 0;
}
```

---

### Example 5: Common Bugs with Doubly Linked Lists

```c
/*
 * Example 5: Doubly Linked List Bugs and Anti-Patterns
 *
 * Compile: gcc -Wall -Wextra -fsanitize=address -o dlist_bugs dlist_bugs.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

struct list_head {
    struct list_head *next, *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list; list->prev = list;
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 1: Forgetting to update BOTH pointers
 * ═══════════════════════════════════════════════════════════════ */
void bug_incomplete_insert(void)
{
    printf("=== BUG 1: Incomplete Insert ===\n");
    
    struct list_head head, a, b;
    INIT_LIST_HEAD(&head);
    
    /* WRONG: Only updating next, not prev */
    /*
    head.next = &a;
    a.next = &b;
    b.next = &head;
    // Forgot: a.prev, b.prev, head.prev!
    // Backward traversal will crash or loop incorrectly
    */
    
    /* CORRECT: Update all four pointers for each insert */
    /* Insert a after head */
    a.next = head.next;  /* a.next = head (circular) */
    a.prev = &head;
    head.next->prev = &a;
    head.next = &a;
    
    printf("Correctly inserted with all pointers updated\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 2: Using deleted node
 * ═══════════════════════════════════════════════════════════════ */
void bug_use_after_delete(void)
{
    printf("=== BUG 2: Use After Delete ===\n");
    
    struct list_head head, node;
    INIT_LIST_HEAD(&head);
    INIT_LIST_HEAD(&node);
    
    /* Add node */
    node.next = head.next;
    node.prev = &head;
    head.next->prev = &node;
    head.next = &node;
    
    /* Delete node */
    node.prev->next = node.next;
    node.next->prev = node.prev;
    
    /* WRONG: Trying to traverse from deleted node */
    /* node.next is now invalid/dangling */
    /* struct list_head *should_not_use = node.next; */
    
    /* CORRECT: Poison pointers after delete to catch bugs */
    node.next = (void *)0xDEADBEEF;
    node.prev = (void *)0xDEADBEEF;
    
    printf("Poisoned pointers after delete\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 3: Iterating while modifying
 * ═══════════════════════════════════════════════════════════════ */
struct item {
    int value;
    struct list_head list;
};

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

void bug_modify_during_iteration(void)
{
    printf("=== BUG 3: Modify During Iteration ===\n");
    
    struct list_head head;
    INIT_LIST_HEAD(&head);
    
    /* Create items */
    for (int i = 0; i < 5; i++) {
        struct item *it = malloc(sizeof(*it));
        it->value = i;
        it->list.next = head.next;
        it->list.prev = &head;
        head.next->prev = &it->list;
        head.next = &it->list;
    }
    
    /* WRONG: Deleting during iteration breaks the loop */
    /*
    for (struct list_head *pos = head.next; pos != &head; pos = pos->next) {
        struct item *it = container_of(pos, struct item, list);
        if (it->value == 2) {
            // Delete pos... but then pos->next is garbage!
            pos->prev->next = pos->next;
            pos->next->prev = pos->prev;
            free(it);
        }
    }
    */
    
    /* CORRECT: Save next pointer BEFORE potentially deleting */
    struct list_head *pos, *tmp;
    for (pos = head.next, tmp = pos->next; 
         pos != &head; 
         pos = tmp, tmp = pos->next) {
        struct item *it = container_of(pos, struct item, list);
        if (it->value == 2) {
            pos->prev->next = pos->next;
            pos->next->prev = pos->prev;
            free(it);
            printf("Safely deleted item with value 2\n");
        }
    }
    
    /* Cleanup remaining */
    while (head.next != &head) {
        struct list_head *node = head.next;
        node->prev->next = node->next;
        node->next->prev = node->prev;
        free(container_of(node, struct item, list));
    }
    
    printf("Safe iteration complete\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 4: Not initializing list head
 * ═══════════════════════════════════════════════════════════════ */
void bug_uninitialized_head(void)
{
    printf("=== BUG 4: Uninitialized Head ===\n");
    
    /* WRONG: Uninitialized list head */
    struct list_head head;  /* Contains garbage! */
    /* list_empty(&head) returns garbage result */
    /* Adding to this list will corrupt memory */
    
    /* CORRECT: Always initialize */
    INIT_LIST_HEAD(&head);
    
    /* Or use static initialization */
    /* static struct list_head head2 = {&head2, &head2}; */
    
    printf("Always initialize list heads!\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 5: Circular list treated as NULL-terminated
 * ═══════════════════════════════════════════════════════════════ */
void bug_null_check(void)
{
    printf("=== BUG 5: Wrong Termination Check ===\n");
    
    struct list_head head;
    INIT_LIST_HEAD(&head);
    
    /* WRONG: Checking for NULL in circular list */
    /*
    for (struct list_head *p = head.next; p != NULL; p = p->next) {
        // Never terminates! p will never be NULL.
    }
    */
    
    /* CORRECT: Check against head for circular lists */
    for (struct list_head *p = head.next; p != &head; p = p->next) {
        /* This terminates correctly */
    }
    
    printf("Use 'pos != &head' not 'pos != NULL'\n\n");
}

int main(void)
{
    bug_incomplete_insert();
    bug_use_after_delete();
    bug_modify_during_iteration();
    bug_uninitialized_head();
    bug_null_check();
    
    printf("=== Key Lessons ===\n");
    printf("1. Update BOTH next and prev pointers\n");
    printf("2. Poison pointers after delete to catch bugs\n");
    printf("3. Save 'next' before deleting during iteration\n");
    printf("4. Always initialize list heads\n");
    printf("5. Check against head, not NULL, for circular lists\n");
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

### Time Complexity

```
+------------------------------------------------------------------+
|  DOUBLY LINKED LIST OPERATION COMPLEXITY                         |
+------------------------------------------------------------------+

    ┌─────────────────────────┬───────────────┬──────────────────────┐
    │ Operation               │ Complexity    │ Notes                │
    ├─────────────────────────┼───────────────┼──────────────────────┤
    │ Access by index         │ O(n)          │ Must traverse        │
    │ Insert at head          │ O(1)          │ 4 pointer updates    │
    │ Insert at tail          │ O(1)          │ With tail/circular   │
    │ Insert after node       │ O(1)          │ 4 pointer updates    │
    │ Insert before node      │ O(1)          │ 4 pointer updates    │
    │ Delete any node         │ O(1)          │ KEY ADVANTAGE!       │
    │ Search                  │ O(n)          │ Linear scan          │
    │ Splice lists            │ O(1)          │ Just relink          │
    │ Check if empty          │ O(1)          │ Compare head ptrs    │
    └─────────────────────────┴───────────────┴──────────────────────┘

    vs SINGLY LINKED:
    Delete any node: O(1) vs O(n)  ← Major advantage!
```

### Memory Overhead Comparison

```
+------------------------------------------------------------------+
|  MEMORY OVERHEAD                                                 |
+------------------------------------------------------------------+

    PER ELEMENT OVERHEAD (64-bit system):

    ┌───────────────────┬────────────────┬────────────────────────┐
    │ Structure         │ Overhead       │ For 4-byte int data    │
    ├───────────────────┼────────────────┼────────────────────────┤
    │ Array             │ 0              │ 4 bytes (100%)         │
    │ Singly linked     │ 8 bytes        │ 12 bytes (300%)        │
    │ Doubly linked     │ 16 bytes       │ 20 bytes (500%)        │
    │ Non-intrusive DL  │ 16+8+meta      │ ~40 bytes (1000%)      │
    └───────────────────┴────────────────┴────────────────────────┘

    INTRUSIVE SAVES:
    - No separate node allocation
    - No data pointer (embedded)
    - No malloc metadata per node
```

### Comparison Table

```
+------------------------------------------------------------------+
|  DOUBLY LINKED vs ALTERNATIVES                                   |
+------------------------------------------------------------------+

    ┌───────────────────┬────────────────┬────────────────┬────────────────┐
    │ Feature           │ Doubly Linked  │ Singly Linked  │ Array          │
    ├───────────────────┼────────────────┼────────────────┼────────────────┤
    │ Random access     │ O(n) ✗         │ O(n) ✗         │ O(1) ✓         │
    │ Insert at front   │ O(1) ✓         │ O(1) ✓         │ O(n) ✗         │
    │ Insert at back    │ O(1) ✓         │ O(1)* ✓        │ O(1) amort ✓   │
    │ Delete any node   │ O(1) ✓✓        │ O(n) ✗         │ O(n) ✗         │
    │ Backwards traverse│ O(1) ✓         │ O(n) ✗         │ O(1) ✓         │
    │ Memory overhead   │ 16B/elem ✗     │ 8B/elem ○      │ 0 ✓            │
    │ Cache locality    │ Poor ✗         │ Poor ✗         │ Excellent ✓    │
    │ Multiple lists    │ Yes ✓✓         │ Yes ✓          │ No ✗           │
    │ Splice O(1)       │ Yes ✓          │ Yes ✓          │ No ✗           │
    └───────────────────┴────────────────┴────────────────┴────────────────┘
    * With tail pointer
```

---

## 6. Design & Engineering Takeaways

### Rules of Thumb

```
+------------------------------------------------------------------+
|  DOUBLY LINKED LIST RULES OF THUMB                               |
+------------------------------------------------------------------+

    1. USE FOR O(1) ARBITRARY DELETION
       If you have a pointer to a node and need to remove it fast,
       doubly linked is the only choice.

    2. USE CIRCULAR DESIGN
       No NULL checks, simpler code, O(1) tail access.
       Linux kernel uses this exclusively.

    3. USE INTRUSIVE DESIGN
       Embed list_head in your structure.
       Saves allocations, enables multiple lists.

    4. ALWAYS UPDATE ALL FOUR POINTERS
       Insert: new->next, new->prev, neighbor->next, neighbor->prev
       Miss one = corrupt list.

    5. USE SAFE ITERATION FOR DELETION
       Save next pointer before modifying.
       Use list_for_each_entry_safe().

    6. POISON DELETED NODES
       Set next/prev to invalid address after delete.
       Helps catch use-after-free bugs.
```

### Linux Kernel Patterns

```
+------------------------------------------------------------------+
|  HOW LINUX KERNEL USES LISTS                                     |
+------------------------------------------------------------------+

    KEY PATTERNS:

    1. SENTINEL HEAD
       ┌─────────────────────────────────────────────────────────┐
       │  LIST_HEAD(my_list);  /* Static, self-referential */    │
       │  /* Empty list: head.next == head.prev == &head */      │
       └─────────────────────────────────────────────────────────┘

    2. MULTIPLE LISTS PER OBJECT
       ┌─────────────────────────────────────────────────────────┐
       │  struct task_struct {                                   │
       │      struct list_head tasks;      /* All tasks list */  │
       │      struct list_head sibling;    /* Sibling list */    │
       │      struct list_head children;   /* Children list */   │
       │  };                                                     │
       └─────────────────────────────────────────────────────────┘

    3. LIST SPLICING
       ┌─────────────────────────────────────────────────────────┐
       │  list_splice(&src, &dst);  /* Move entire list O(1) */  │
       └─────────────────────────────────────────────────────────┘

    4. EMBEDDED IN HASH BUCKETS
       ┌─────────────────────────────────────────────────────────┐
       │  struct hlist_head table[HASH_SIZE];                    │
       │  /* Each bucket is a list for collision chaining */     │
       └─────────────────────────────────────────────────────────┘
```

### Selection Guidelines

```
+------------------------------------------------------------------+
|  WHEN TO CHOOSE DOUBLY LINKED LIST                               |
+------------------------------------------------------------------+

    CHOOSE DOUBLY LINKED WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Need O(1) removal of arbitrary nodes                    │
    │  ✓ Need bidirectional traversal                            │
    │  ✓ Implementing LRU cache                                  │
    │  ✓ Objects belong to multiple lists                        │
    │  ✓ Need to splice lists together                           │
    │  ✓ Order matters and changes frequently                    │
    └─────────────────────────────────────────────────────────────┘

    CHOOSE SINGLY LINKED WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Only forward traversal needed                           │
    │  ✓ Memory is very constrained                              │
    │  ✓ Implementing stack (LIFO only)                          │
    │  ✓ Simpler code is priority                                │
    └─────────────────────────────────────────────────────────────┘

    CHOOSE ARRAY WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Need random access                                      │
    │  ✓ Cache performance critical                              │
    │  ✓ Size relatively stable                                  │
    │  ✓ Elements are small                                      │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  DOUBLY LINKED LIST: KEY TAKEAWAYS                               |
+------------------------------------------------------------------+

    CORE VALUE:
    O(1) deletion of arbitrary nodes — the defining advantage
    over singly linked lists.

    KERNEL PATTERN:
    - Circular (no NULL checks)
    - Intrusive (embed list_head)
    - Sentinel head (empty = head points to self)
    - container_of() for type recovery

    BEST USE CASES:
    - LRU caches (move to front, evict from back)
    - Objects on multiple lists simultaneously
    - Scheduler queues (frequent reordering)
    - Any case needing O(1) removal

    AVOID WHEN:
    - Only forward traversal needed (use singly linked)
    - Random access needed (use array)
    - Cache performance critical (use array)

    KEY IMPLEMENTATION POINTS:
    1. Always update all four pointers on insert
    2. Use safe iteration when deleting
    3. Poison deleted nodes
    4. Initialize before use
    5. Check against head, not NULL
```

**中文总结：**
- **核心价值**：O(1) 删除任意节点——相比单链表的决定性优势
- **内核模式**：循环、侵入式、哨兵头、container_of()
- **最佳用途**：LRU 缓存、多列表成员、调度队列
- **关键点**：更新全部四个指针、安全迭代、毒化已删除节点

