# Intrusive Linked List (Embedded Node Pattern) in C

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE PROBLEM: NON-INTRUSIVE LIST OVERHEAD                        |
+------------------------------------------------------------------+

    Traditional (Non-Intrusive) Linked List:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct node {                                              │
    │      void *data;    /* Pointer to actual data */            │
    │      struct node *next;                                     │
    │  };                                                         │
    │                                                              │
    │  PROBLEMS:                                                  │
    │  1. Two allocations per element (node + data)               │
    │  2. Extra indirection to access data                        │
    │  3. Object can only be on ONE list                          │
    │  4. Given data, cannot find node in O(1)                    │
    │  5. Worse cache behavior (node and data separate)           │
    └─────────────────────────────────────────────────────────────┘

    INTRUSIVE SOLUTION:
    ┌─────────────────────────────────────────────────────────────┐
    │  Embed the list node INSIDE the data structure              │
    │  No separate allocation, no data pointer, no indirection    │
    │  Object can be on MULTIPLE lists (embed multiple nodes)     │
    │  Given data, list node is already there!                    │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- **非侵入式问题**：每元素两次分配、额外间接访问、只能在一个列表、缓存行为差
- **侵入式解决方案**：将链表节点嵌入到数据结构内部
- 无需单独分配、无数据指针、可在多个列表

### The Core Insight

```
+------------------------------------------------------------------+
|  INTRUSIVE LIST: THE KEY IDEA                                    |
+------------------------------------------------------------------+

    NON-INTRUSIVE:
    
    ┌────────────────┐      ┌────────────────────────┐
    │ List Node      │      │ Your Data              │
    │ ┌────────────┐ │      │ ┌──────────────────┐   │
    │ │ data ────────────────▶│ actual fields    │   │
    │ │ next ──────┼─┼────▶ │ └──────────────────┘   │
    │ └────────────┘ │      └────────────────────────┘
    └────────────────┘       ↑
         malloc #1           malloc #2
    
    INTRUSIVE:
    
    ┌────────────────────────────────────────────┐
    │ Your Data (with embedded node)             │
    │ ┌──────────────────────────────────────┐   │
    │ │ actual fields                        │   │
    │ │ ┌──────────────────────────────────┐ │   │
    │ │ │ list_node (next, prev embedded)  │ │   │
    │ │ └──────────────────────────────────┘ │   │
    │ └──────────────────────────────────────┘   │
    └────────────────────────────────────────────┘
         malloc #1 only!
```

### Linux Kernel's list_head

This pattern is canonically implemented in the Linux kernel:

```c
/* From linux/include/linux/types.h */
struct list_head {
    struct list_head *next, *prev;
};

/* From linux/include/linux/list.h - line 350-351 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)
```

### Design Philosophy

```
+------------------------------------------------------------------+
|  WHY INTRUSIVE LISTS EXIST                                       |
+------------------------------------------------------------------+

    1. ZERO ALLOCATION OVERHEAD
       ┌─────────────────────────────────────────────────────────┐
       │  No separate node allocation                            │
       │  No allocator metadata per node                         │
       │  Single malloc = single object with embedded node       │
       └─────────────────────────────────────────────────────────┘

    2. BETTER CACHE LOCALITY
       ┌─────────────────────────────────────────────────────────┐
       │  Node and data in same cache line                       │
       │  No pointer chasing to get from node to data            │
       │  Accessing node likely brings data into cache too       │
       └─────────────────────────────────────────────────────────┘

    3. MULTIPLE LIST MEMBERSHIP
       ┌─────────────────────────────────────────────────────────┐
       │  Embed multiple list_head members                       │
       │  Same object on "all items" and "active items" lists    │
       │  Essential for kernel's task_struct, inode, etc.        │
       └─────────────────────────────────────────────────────────┘

    4. TYPE-SAFE WITH container_of
       ┌─────────────────────────────────────────────────────────┐
       │  No void* needed                                        │
       │  Compiler knows the containing type                     │
       │  Calculated at compile time (zero runtime cost)         │
       └─────────────────────────────────────────────────────────┘
```

### Invariants

```
+------------------------------------------------------------------+
|  INTRUSIVE LIST INVARIANTS                                       |
+------------------------------------------------------------------+

    1. EMBEDDED NODE IS PART OF OBJECT
       Object owns its list_head member(s)
       When object is freed, list node is freed with it
       Must remove from list BEFORE freeing object!

    2. CONTAINER_OF VALIDITY
       container_of only works when:
       - ptr actually points to the embedded member
       - Member is at known offset in container

    3. LIST MEMBERSHIP TRACKING
       Object must track which lists it's on
       Or: design must ensure exclusive list membership

    4. LIFETIME COUPLING
       Object lifetime ≥ list membership
       Never leave dangling references in list
```

---

## 2. Memory Model

### Memory Layout Comparison

```
+------------------------------------------------------------------+
|  NON-INTRUSIVE VS INTRUSIVE MEMORY LAYOUT                        |
+------------------------------------------------------------------+

    NON-INTRUSIVE (traditional):
    
    List nodes (scattered):         Data objects (also scattered):
    ┌──────────────┐                ┌──────────────────┐
    │ node1        │                │ task A           │
    │  data ───────────────────────▶│  pid: 100        │
    │  next ───────┼──┐             │  name: "init"    │
    └──────────────┘  │             └──────────────────┘
                      │
                      ▼
    ┌──────────────┐                ┌──────────────────┐
    │ node2        │                │ task B           │
    │  data ───────────────────────▶│  pid: 200        │
    │  next ───────┼──┐             │  name: "bash"    │
    └──────────────┘  │             └──────────────────┘
                      ▼

    Allocations: 2N (N nodes + N data objects)
    Indirections: 2 (node → data → field)

    ──────────────────────────────────────────────────────────────

    INTRUSIVE:
    
    ┌────────────────────────────────────────────────────┐
    │ task A                                             │
    │   pid: 100                                         │
    │   name: "init"                                     │
    │   ┌────────────────────────────────────────────┐  │
    │   │ list (embedded)                            │  │
    │   │   next ─────────────────────────┐          │  │
    │   │   prev ◀───────────────────┐    │          │  │
    │   └────────────────────────────│────│──────────┘  │
    └────────────────────────────────│────│─────────────┘
                                     │    │
    ┌────────────────────────────────│────│─────────────┐
    │ task B                         │    │             │
    │   pid: 200                     │    │             │
    │   name: "bash"                 │    │             │
    │   ┌────────────────────────────│────│──────────┐  │
    │   │ list (embedded)            │    │          │  │
    │   │   next ◀───────────────────┘    │          │  │
    │   │   prev ─────────────────────────┘          │  │
    │   └────────────────────────────────────────────┘  │
    └───────────────────────────────────────────────────┘

    Allocations: N (just the data objects)
    Indirections: 1 (node → container via arithmetic)
```

**中文解释：**
- **非侵入式**：2N 次分配，2 层间接访问
- **侵入式**：N 次分配，1 层间接访问（通过算术而非指针）
- 内存更紧凑，缓存更友好

### The container_of Mechanism

```c
/* Standard container_of implementation */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) ); })
```

```
+------------------------------------------------------------------+
|  HOW container_of WORKS                                          |
+------------------------------------------------------------------+

    Given:
    struct task {
        int pid;           /* offset 0, size 4 */
        char name[16];     /* offset 4, size 16 */
        struct list_head list;  /* offset 20, size 16 */
    };

    Memory layout of one task object at address 0x1000:
    
    0x1000 ┌─────────────────────────┐  ◀── task* (what we want)
           │ pid (4 bytes)           │
    0x1004 ├─────────────────────────┤
           │ name (16 bytes)         │
           │                         │
    0x1014 ├─────────────────────────┤  ◀── &task.list (what we have)
           │ list.next (8 bytes)     │
    0x101C │ list.prev (8 bytes)     │
    0x1024 └─────────────────────────┘

    container_of(&task.list, struct task, list):
    
    Step 1: Get offset of 'list' in 'struct task'
            offsetof(struct task, list) = 0x14 (20 decimal)
    
    Step 2: Subtract offset from pointer
            (char*)0x1014 - 0x14 = 0x1000
    
    Step 3: Cast to container type
            (struct task *)0x1000
    
    Result: Pointer to the containing task structure!
```

### Multiple List Membership

```
+------------------------------------------------------------------+
|  OBJECT ON MULTIPLE LISTS                                        |
+------------------------------------------------------------------+

    struct task {
        int pid;
        int state;
        struct list_head all_tasks;    /* All tasks in system */
        struct list_head runqueue;     /* Tasks ready to run */
        struct list_head wait_queue;   /* Tasks waiting for I/O */
        struct list_head children;     /* Child processes */
    };

    Memory layout:
    ┌────────────────────────────────────────────────────────────┐
    │ task (single allocation)                                   │
    │  ┌─────────┬─────────┬──────────┬──────────┬──────────┐   │
    │  │  pid    │  state  │all_tasks │runqueue  │wait_queue│   │
    │  │         │         │ next/prev│ next/prev│ next/prev│   │
    │  └─────────┴─────────┴──────────┴──────────┴──────────┘   │
    └────────────────────────────────────────────────────────────┘
              │                  │           │           │
              │                  ▼           ▼           ▼
              │          ┌──────────┐ ┌──────────┐ ┌──────────┐
              │          │ All      │ │ Ready    │ │ Waiting  │
              │          │ Tasks    │ │ Queue    │ │ Queue    │
              │          │ List     │ │          │ │          │
              │          └──────────┘ └──────────┘ └──────────┘
              │
              └─────── Each list_head links to its own list!

    CRITICAL: Each list_head is independent.
    Object can be on all_tasks AND runqueue simultaneously.
    Removing from runqueue doesn't affect all_tasks membership.
```

**中文解释：**
- 一个对象可以同时在多个列表中
- 每个 list_head 成员独立连接到不同的列表
- 从一个列表移除不影响其他列表的成员资格
- 这是内核设计的关键：task_struct、inode 等都使用此模式

### Ownership and Lifetime

```
+------------------------------------------------------------------+
|  LIFETIME RULES FOR INTRUSIVE LISTS                              |
+------------------------------------------------------------------+

    RULE 1: REMOVE BEFORE FREE
    ┌─────────────────────────────────────────────────────────────┐
    │  /* CORRECT */                                              │
    │  list_del(&task->runqueue);    /* Remove from list first */ │
    │  list_del(&task->all_tasks);   /* Remove from all lists */  │
    │  kfree(task);                  /* NOW safe to free */       │
    │                                                              │
    │  /* WRONG - leaves dangling pointers in list! */            │
    │  kfree(task);                  /* List now corrupt! */      │
    └─────────────────────────────────────────────────────────────┘

    RULE 2: LIST HEAD OUTLIVES MEMBERS
    ┌─────────────────────────────────────────────────────────────┐
    │  The list head (sentinel) must outlive all list members.    │
    │  Typically: head is static or in long-lived parent object.  │
    └─────────────────────────────────────────────────────────────┘

    RULE 3: ITERATION SAFETY
    ┌─────────────────────────────────────────────────────────────┐
    │  During iteration, if you might remove elements:            │
    │  - Use list_for_each_entry_safe()                           │
    │  - Save 'next' before potentially removing 'current'        │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Typical Application Scenarios

### Where Intrusive Lists Dominate

```
+------------------------------------------------------------------+
|  INTRUSIVE LIST APPLICATIONS                                     |
+------------------------------------------------------------------+

    LINUX KERNEL (PRIMARY USE CASE):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Task management (task_struct.tasks)                      │
    │  • Process relationships (children, siblings)               │
    │  • Wait queues                                              │
    │  • Timer lists                                              │
    │  • Inode caches                                             │
    │  • Page frame management                                    │
    │  • Device driver lists                                      │
    │  • Network socket lists                                     │
    │  • Virtually ALL kernel linked lists                        │
    └─────────────────────────────────────────────────────────────┘

    SYSTEM LIBRARIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • glibc malloc (free block lists)                          │
    │  • jemalloc/tcmalloc                                        │
    │  • BSD queue.h (TAILQ, LIST, SLIST)                         │
    └─────────────────────────────────────────────────────────────┘

    DATABASE ENGINES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Buffer pool management                                   │
    │  • Transaction lists                                        │
    │  • Lock wait queues                                         │
    └─────────────────────────────────────────────────────────────┘

    GAME ENGINES:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Entity management                                        │
    │  • Spatial partitioning                                     │
    │  • Object pools                                             │
    └─────────────────────────────────────────────────────────────┘
```

### When to Use Intrusive Lists

```
+------------------------------------------------------------------+
|  USE INTRUSIVE LISTS WHEN:                                       |
+------------------------------------------------------------------+

    ✓ Objects already exist (not created for the list)
    ✓ Objects need to be on multiple lists
    ✓ Allocation overhead is a concern
    ✓ Cache performance matters
    ✓ You control the data structure definition
    ✓ Building system software (OS, database, runtime)
    ✓ Memory is constrained
```

### When NOT to Use

```
+------------------------------------------------------------------+
|  AVOID INTRUSIVE LISTS WHEN:                                     |
+------------------------------------------------------------------+

    ✗ You don't control the struct definition
    ✗ Data comes from external source (files, network)
    ✗ Objects are primitives (int, char, etc.)
    ✗ Simplicity more important than performance
    ✗ Teaching beginners (container_of is confusing)
    ✗ Generic data structures needed (use void* lists)
```

---

## 4. Complete C Examples

### Example 1: Complete Intrusive List Implementation

```c
/*
 * Example 1: Complete Intrusive Doubly Linked List
 *
 * Full implementation matching Linux kernel's list.h pattern
 * Compile: gcc -Wall -Wextra -o intrusive_complete intrusive_complete.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * Core list_head structure
 * ═══════════════════════════════════════════════════════════════ */
struct list_head {
    struct list_head *next, *prev;
};

/* ═══════════════════════════════════════════════════════════════
 * Fundamental macros
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

/* ═══════════════════════════════════════════════════════════════
 * container_of and list_entry
 * ═══════════════════════════════════════════════════════════════ */

#define container_of(ptr, type, member) ({              \
    const typeof(((type *)0)->member) *__mptr = (ptr);  \
    (type *)((char *)__mptr - offsetof(type, member)); })

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

/* ═══════════════════════════════════════════════════════════════
 * List predicates
 * ═══════════════════════════════════════════════════════════════ */

static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->next == head->prev);
}

static inline int list_is_last(const struct list_head *list,
                               const struct list_head *head)
{
    return list->next == head;
}

/* ═══════════════════════════════════════════════════════════════
 * List modification operations
 * ═══════════════════════════════════════════════════════════════ */

/* Internal: insert between two known consecutive entries */
static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/* Add after head (at front of list) */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

/* Add before head (at end of list) */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/* Internal: connect prev and next */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/* Delete entry (doesn't free, doesn't reinitialize) */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    /* Poison pointers to catch use-after-delete */
    entry->next = (struct list_head *)0xDEADBEEF;
    entry->prev = (struct list_head *)0xDEADBEEF;
}

/* Delete and reinitialize (can be added to another list) */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/* Move entry to front of list */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/* Move entry to end of list */
static inline void list_move_tail(struct list_head *list,
                                   struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/* Replace old entry with new one */
static inline void list_replace(struct list_head *old,
                                struct list_head *new)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

/* ═══════════════════════════════════════════════════════════════
 * Iteration macros
 * ═══════════════════════════════════════════════════════════════ */

/* Iterate over list of given type */
#define list_for_each_entry(pos, head, member)                  \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                                \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* Iterate backwards */
#define list_for_each_entry_reverse(pos, head, member)          \
    for (pos = list_entry((head)->prev, typeof(*pos), member);  \
         &pos->member != (head);                                \
         pos = list_entry(pos->member.prev, typeof(*pos), member))

/* Safe iteration (allows removal during iteration) */
#define list_for_each_entry_safe(pos, n, head, member)          \
    for (pos = list_entry((head)->next, typeof(*pos), member),  \
         n = list_entry(pos->member.next, typeof(*pos), member);\
         &pos->member != (head);                                \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* ═══════════════════════════════════════════════════════════════
 * Example: Event system with multiple lists
 * ═══════════════════════════════════════════════════════════════ */

struct event {
    int id;
    int priority;
    char description[64];
    
    struct list_head all_events;      /* Global event list */
    struct list_head pending_events;  /* Events waiting to be processed */
};

/* Global lists */
LIST_HEAD(g_all_events);
LIST_HEAD(g_pending);

static int next_event_id = 1;

struct event *event_create(int priority, const char *desc)
{
    struct event *e = malloc(sizeof(*e));
    if (!e) return NULL;
    
    e->id = next_event_id++;
    e->priority = priority;
    strncpy(e->description, desc, sizeof(e->description) - 1);
    e->description[sizeof(e->description) - 1] = '\0';
    
    /* Initialize embedded list nodes */
    INIT_LIST_HEAD(&e->all_events);
    INIT_LIST_HEAD(&e->pending_events);
    
    /* Add to global list */
    list_add_tail(&e->all_events, &g_all_events);
    
    /* Also add to pending (new events are pending) */
    list_add_tail(&e->pending_events, &g_pending);
    
    printf("Created event #%d: %s (priority=%d)\n",
           e->id, e->description, e->priority);
    
    return e;
}

void event_process(struct event *e)
{
    printf("Processing event #%d: %s\n", e->id, e->description);
    
    /* Remove from pending, but keep in all_events */
    list_del_init(&e->pending_events);
}

void event_destroy(struct event *e)
{
    printf("Destroying event #%d\n", e->id);
    
    /* MUST remove from ALL lists before freeing! */
    if (!list_empty(&e->all_events))
        list_del(&e->all_events);
    if (!list_empty(&e->pending_events))
        list_del(&e->pending_events);
    
    free(e);
}

void print_all_events(void)
{
    printf("All events: ");
    struct event *e;
    list_for_each_entry(e, &g_all_events, all_events) {
        printf("#%d ", e->id);
    }
    printf("\n");
}

void print_pending_events(void)
{
    printf("Pending events: ");
    struct event *e;
    list_for_each_entry(e, &g_pending, pending_events) {
        printf("#%d ", e->id);
    }
    printf("\n");
}

int main(void)
{
    printf("=== Intrusive List Demo ===\n\n");
    
    /* Create events */
    struct event *e1 = event_create(1, "System startup");
    struct event *e2 = event_create(2, "User login");
    struct event *e3 = event_create(1, "Network ready");
    struct event *e4 = event_create(3, "Database connected");
    
    printf("\n--- Initial state ---\n");
    print_all_events();
    print_pending_events();
    
    /* Process some events */
    printf("\n--- Processing e2 ---\n");
    event_process(e2);
    print_all_events();   /* Still there! */
    print_pending_events(); /* Gone */
    
    /* Destroy e3 (removes from both lists) */
    printf("\n--- Destroying e3 ---\n");
    event_destroy(e3);
    print_all_events();
    print_pending_events();
    
    /* Cleanup all remaining events (safe iteration) */
    printf("\n--- Cleanup ---\n");
    struct event *tmp;
    list_for_each_entry_safe(e1, tmp, &g_all_events, all_events) {
        event_destroy(e1);
    }
    
    printf("\nAll events: %s\n", list_empty(&g_all_events) ? "empty" : "NOT empty");
    printf("Pending events: %s\n", list_empty(&g_pending) ? "empty" : "NOT empty");
    
    return 0;
}
```

---

### Example 2: Singly Linked Intrusive List

```c
/*
 * Example 2: Singly Linked Intrusive List
 *
 * Lower overhead when backwards traversal not needed
 * Compile: gcc -Wall -Wextra -o intrusive_slist intrusive_slist.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* ═══════════════════════════════════════════════════════════════
 * Singly linked intrusive node
 * ═══════════════════════════════════════════════════════════════ */
struct slist_node {
    struct slist_node *next;
};

struct slist_head {
    struct slist_node *first;
};

#define SLIST_HEAD_INIT { .first = NULL }
#define SLIST_HEAD(name) struct slist_head name = SLIST_HEAD_INIT

static inline void slist_init(struct slist_head *head)
{
    head->first = NULL;
}

static inline int slist_empty(struct slist_head *head)
{
    return head->first == NULL;
}

/* Add at head (stack push) */
static inline void slist_add(struct slist_node *new, struct slist_head *head)
{
    new->next = head->first;
    head->first = new;
}

/* Remove from head (stack pop) - returns removed node */
static inline struct slist_node *slist_pop(struct slist_head *head)
{
    struct slist_node *first = head->first;
    if (first)
        head->first = first->next;
    return first;
}

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define slist_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define slist_for_each(pos, head) \
    for (pos = (head)->first; pos != NULL; pos = pos->next)

#define slist_for_each_entry(pos, head, member) \
    for (pos = slist_entry((head)->first, typeof(*pos), member); \
         &pos->member != NULL; \
         pos = slist_entry(pos->member.next, typeof(*pos), member))

/* ═══════════════════════════════════════════════════════════════
 * Example: Free list (memory pool pattern)
 * ═══════════════════════════════════════════════════════════════ */

#define POOL_SIZE 10

struct block {
    char data[64];           /* User data area */
    struct slist_node free;  /* Link when on free list */
};

struct pool {
    struct block blocks[POOL_SIZE];
    struct slist_head free_list;
    int allocated;
};

void pool_init(struct pool *p)
{
    slist_init(&p->free_list);
    p->allocated = 0;
    
    /* Add all blocks to free list */
    for (int i = 0; i < POOL_SIZE; i++) {
        slist_add(&p->blocks[i].free, &p->free_list);
    }
}

struct block *pool_alloc(struct pool *p)
{
    struct slist_node *node = slist_pop(&p->free_list);
    if (!node) {
        printf("Pool exhausted!\n");
        return NULL;
    }
    
    p->allocated++;
    struct block *b = slist_entry(node, struct block, free);
    printf("Allocated block %ld (total: %d)\n",
           b - p->blocks, p->allocated);
    return b;
}

void pool_free(struct pool *p, struct block *b)
{
    printf("Freed block %ld\n", b - p->blocks);
    slist_add(&b->free, &p->free_list);
    p->allocated--;
}

int main(void)
{
    printf("=== Intrusive Singly Linked List (Free List) ===\n\n");
    
    struct pool pool;
    pool_init(&pool);
    
    /* Allocate some blocks */
    printf("--- Allocating ---\n");
    struct block *b1 = pool_alloc(&pool);
    struct block *b2 = pool_alloc(&pool);
    struct block *b3 = pool_alloc(&pool);
    
    /* Free in different order */
    printf("\n--- Freeing ---\n");
    pool_free(&pool, b2);
    pool_free(&pool, b1);
    
    /* Allocate again (LIFO - gets last freed first) */
    printf("\n--- Reallocating ---\n");
    struct block *b4 = pool_alloc(&pool);
    struct block *b5 = pool_alloc(&pool);
    
    /* Should get b1 and b2's slots */
    printf("b4 is block %ld (was b1)\n", b4 - pool.blocks);
    printf("b5 is block %ld (was b2)\n", b5 - pool.blocks);
    
    printf("\nFree list efficient: O(1) alloc and free!\n");
    
    return 0;
}
```

---

### Example 3: Hash Table with Intrusive Buckets

```c
/*
 * Example 3: Hash Table Using Intrusive List Buckets
 *
 * Linux kernel's hlist pattern for hash tables
 * Compile: gcc -Wall -Wextra -o hash_intrusive hash_intrusive.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * Hash list (hlist) - optimized for hash buckets
 * Uses pointer-to-pointer for pprev to save space in head
 * ═══════════════════════════════════════════════════════════════ */

struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next;
    struct hlist_node **pprev;  /* Pointer to previous node's next pointer */
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT

static inline void hlist_init(struct hlist_head *h)
{
    h->first = NULL;
}

static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

static inline void hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define hlist_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos; pos = pos->next)

#define hlist_for_each_entry(pos, head, member)                      \
    for (pos = hlist_entry((head)->first, typeof(*pos), member);     \
         pos && &pos->member;                                        \
         pos = pos->member.next ?                                    \
             hlist_entry(pos->member.next, typeof(*pos), member) : NULL)

/* ═══════════════════════════════════════════════════════════════
 * Example: Symbol table (like in a compiler)
 * ═══════════════════════════════════════════════════════════════ */

#define HASH_BITS 4
#define HASH_SIZE (1 << HASH_BITS)

struct symbol {
    char name[32];
    int type;
    int value;
    struct hlist_node hash_node;  /* Hash bucket link */
};

struct symbol_table {
    struct hlist_head buckets[HASH_SIZE];
    int count;
};

/* Simple hash function */
static unsigned int hash_string(const char *s)
{
    unsigned int hash = 0;
    while (*s) {
        hash = hash * 31 + *s++;
    }
    return hash & (HASH_SIZE - 1);
}

void symbol_table_init(struct symbol_table *st)
{
    for (int i = 0; i < HASH_SIZE; i++)
        hlist_init(&st->buckets[i]);
    st->count = 0;
}

struct symbol *symbol_create(const char *name, int type, int value)
{
    struct symbol *sym = malloc(sizeof(*sym));
    if (!sym) return NULL;
    
    strncpy(sym->name, name, sizeof(sym->name) - 1);
    sym->name[sizeof(sym->name) - 1] = '\0';
    sym->type = type;
    sym->value = value;
    
    return sym;
}

void symbol_table_insert(struct symbol_table *st, struct symbol *sym)
{
    unsigned int bucket = hash_string(sym->name);
    hlist_add_head(&sym->hash_node, &st->buckets[bucket]);
    st->count++;
    printf("Inserted '%s' into bucket %u\n", sym->name, bucket);
}

struct symbol *symbol_table_lookup(struct symbol_table *st, const char *name)
{
    unsigned int bucket = hash_string(name);
    struct hlist_node *pos;
    
    hlist_for_each(pos, &st->buckets[bucket]) {
        struct symbol *sym = hlist_entry(pos, struct symbol, hash_node);
        if (strcmp(sym->name, name) == 0)
            return sym;
    }
    return NULL;
}

void symbol_table_remove(struct symbol_table *st, struct symbol *sym)
{
    hlist_del(&sym->hash_node);
    st->count--;
    free(sym);
}

void symbol_table_print(struct symbol_table *st)
{
    printf("Symbol table (%d symbols):\n", st->count);
    for (int i = 0; i < HASH_SIZE; i++) {
        if (!hlist_empty(&st->buckets[i])) {
            printf("  Bucket %d:", i);
            struct hlist_node *pos;
            hlist_for_each(pos, &st->buckets[i]) {
                struct symbol *sym = hlist_entry(pos, struct symbol, hash_node);
                printf(" %s", sym->name);
            }
            printf("\n");
        }
    }
}

int main(void)
{
    printf("=== Hash Table with Intrusive Lists ===\n\n");
    
    struct symbol_table st;
    symbol_table_init(&st);
    
    /* Insert symbols */
    symbol_table_insert(&st, symbol_create("main", 1, 0x1000));
    symbol_table_insert(&st, symbol_create("printf", 2, 0x2000));
    symbol_table_insert(&st, symbol_create("malloc", 2, 0x3000));
    symbol_table_insert(&st, symbol_create("x", 0, 42));
    symbol_table_insert(&st, symbol_create("y", 0, 100));
    symbol_table_insert(&st, symbol_create("counter", 0, 0));
    
    printf("\n");
    symbol_table_print(&st);
    
    /* Lookup */
    printf("\n--- Lookups ---\n");
    struct symbol *sym = symbol_table_lookup(&st, "printf");
    if (sym)
        printf("Found '%s': type=%d, value=%d\n", sym->name, sym->type, sym->value);
    
    sym = symbol_table_lookup(&st, "unknown");
    printf("Lookup 'unknown': %s\n", sym ? "found" : "not found");
    
    /* Remove */
    printf("\n--- Removing 'x' ---\n");
    sym = symbol_table_lookup(&st, "x");
    if (sym)
        symbol_table_remove(&st, sym);
    symbol_table_print(&st);
    
    return 0;
}
```

---

### Example 4: Priority Queue with Sorted Insert

```c
/*
 * Example 4: Priority Queue Using Intrusive Sorted List
 *
 * Maintains sorted order for priority scheduling
 * Compile: gcc -Wall -Wextra -o pqueue_intrusive pqueue_intrusive.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* Standard list_head */
struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

static inline int list_empty(const struct list_head *head) {
    return head->next == head;
}

static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add_tail(struct list_head *new, struct list_head *head) {
    __list_add(new, head->prev, head);
}

static inline void list_del(struct list_head *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

#define list_for_each_entry(pos, head, member)                  \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                                \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/* ═══════════════════════════════════════════════════════════════
 * Priority Queue Implementation
 * ═══════════════════════════════════════════════════════════════ */

struct pq_item {
    int priority;  /* Lower = higher priority */
    char task[32];
    struct list_head pq_node;
};

struct priority_queue {
    struct list_head head;  /* Sorted by priority, low first */
    int count;
};

void pqueue_init(struct priority_queue *pq)
{
    INIT_LIST_HEAD(&pq->head);
    pq->count = 0;
}

/* Insert in sorted position (O(n)) */
void pqueue_insert(struct priority_queue *pq, struct pq_item *item)
{
    struct pq_item *pos;
    
    INIT_LIST_HEAD(&item->pq_node);
    
    /* Find insertion point (first item with lower priority) */
    list_for_each_entry(pos, &pq->head, pq_node) {
        if (pos->priority > item->priority) {
            /* Insert before pos */
            __list_add(&item->pq_node, pos->pq_node.prev, &pos->pq_node);
            pq->count++;
            return;
        }
    }
    
    /* Insert at end (lowest priority) */
    list_add_tail(&item->pq_node, &pq->head);
    pq->count++;
}

/* Remove highest priority item (O(1)) */
struct pq_item *pqueue_pop(struct priority_queue *pq)
{
    if (list_empty(&pq->head))
        return NULL;
    
    struct pq_item *item = list_first_entry(&pq->head, struct pq_item, pq_node);
    list_del(&item->pq_node);
    pq->count--;
    
    return item;
}

struct pq_item *pq_item_create(int priority, const char *task)
{
    struct pq_item *item = malloc(sizeof(*item));
    if (!item) return NULL;
    
    item->priority = priority;
    strncpy(item->task, task, sizeof(item->task) - 1);
    item->task[sizeof(item->task) - 1] = '\0';
    INIT_LIST_HEAD(&item->pq_node);
    
    return item;
}

void pqueue_print(struct priority_queue *pq)
{
    printf("Priority Queue (%d items): ", pq->count);
    struct pq_item *pos;
    list_for_each_entry(pos, &pq->head, pq_node) {
        printf("[%d:%s] ", pos->priority, pos->task);
    }
    printf("\n");
}

int main(void)
{
    printf("=== Priority Queue with Intrusive List ===\n\n");
    
    struct priority_queue pq;
    pqueue_init(&pq);
    
    /* Insert tasks (order doesn't matter - sorted on insert) */
    printf("--- Inserting tasks ---\n");
    pqueue_insert(&pq, pq_item_create(5, "Low priority task"));
    pqueue_print(&pq);
    
    pqueue_insert(&pq, pq_item_create(1, "URGENT!"));
    pqueue_print(&pq);
    
    pqueue_insert(&pq, pq_item_create(3, "Normal task"));
    pqueue_print(&pq);
    
    pqueue_insert(&pq, pq_item_create(2, "High priority"));
    pqueue_print(&pq);
    
    pqueue_insert(&pq, pq_item_create(1, "Also urgent"));
    pqueue_print(&pq);
    
    /* Process in priority order */
    printf("\n--- Processing tasks ---\n");
    struct pq_item *item;
    while ((item = pqueue_pop(&pq)) != NULL) {
        printf("Processing [priority=%d]: %s\n", item->priority, item->task);
        free(item);
    }
    
    return 0;
}
```

---

### Example 5: Common Mistakes with Intrusive Lists

```c
/*
 * Example 5: Intrusive List Mistakes and Anti-Patterns
 *
 * Compile: gcc -Wall -Wextra -fsanitize=address -o intrusive_bugs intrusive_bugs.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

struct list_head {
    struct list_head *next, *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
    new->next = head->next;
    new->prev = head;
    head->next->prev = new;
    head->next = new;
}

static inline void list_del(struct list_head *entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
}

static inline int list_empty(struct list_head *head) {
    return head->next == head;
}

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* ═══════════════════════════════════════════════════════════════
 * BUG 1: Freeing object without removing from list
 * ═══════════════════════════════════════════════════════════════ */
struct item {
    int value;
    struct list_head link;
};

void bug_free_without_remove(void)
{
    printf("=== BUG 1: Free Without Remove ===\n");
    
    struct list_head head;
    INIT_LIST_HEAD(&head);
    
    struct item *a = malloc(sizeof(*a));
    struct item *b = malloc(sizeof(*a));
    a->value = 1;
    b->value = 2;
    
    list_add(&a->link, &head);
    list_add(&b->link, &head);
    
    /* WRONG: Freeing 'a' without removing from list */
    /* free(a);  // Now head.next points to freed memory! */
    /* Traversing list would crash */
    
    /* CORRECT: Remove before freeing */
    list_del(&a->link);
    free(a);
    
    list_del(&b->link);
    free(b);
    
    printf("Always list_del() before free()!\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 2: Using container_of on wrong pointer type
 * ═══════════════════════════════════════════════════════════════ */
struct task {
    int id;
    struct list_head run_list;
    struct list_head wait_list;
};

void bug_wrong_member(void)
{
    printf("=== BUG 2: Wrong Member in container_of ===\n");
    
    struct task t;
    t.id = 42;
    INIT_LIST_HEAD(&t.run_list);
    INIT_LIST_HEAD(&t.wait_list);
    
    struct list_head *ptr = &t.wait_list;
    
    /* WRONG: Using wrong member name */
    /* struct task *wrong = container_of(ptr, struct task, run_list); */
    /* wrong->id would access garbage! */
    
    /* CORRECT: Use the actual member */
    struct task *correct = container_of(ptr, struct task, wait_list);
    printf("Correct id: %d\n\n", correct->id);
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 3: Using uninitialized list_head
 * ═══════════════════════════════════════════════════════════════ */
void bug_uninitialized(void)
{
    printf("=== BUG 3: Uninitialized list_head ===\n");
    
    struct list_head head;  /* UNINITIALIZED! */
    
    /* WRONG: Adding to uninitialized list */
    /* struct item i;
       list_add(&i.link, &head);  // Writes to garbage addresses */
    
    /* CORRECT: Always initialize */
    INIT_LIST_HEAD(&head);
    
    printf("Always INIT_LIST_HEAD() before use!\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 4: Adding same node to multiple lists (same member)
 * ═══════════════════════════════════════════════════════════════ */
void bug_double_add(void)
{
    printf("=== BUG 4: Same Node on Multiple Lists (Same Member) ===\n");
    
    struct list_head list1, list2;
    INIT_LIST_HEAD(&list1);
    INIT_LIST_HEAD(&list2);
    
    struct item a;
    a.value = 1;
    INIT_LIST_HEAD(&a.link);
    
    list_add(&a.link, &list1);
    
    /* WRONG: Adding same node (same member) to another list */
    /* list_add(&a.link, &list2); */
    /* This corrupts list1! */
    
    /* CORRECT: Use separate list_head members for each list */
    printf("For multiple lists, use multiple list_head members!\n");
    printf("struct item { struct list_head link1, link2; };\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 5: Iterating after modification without safe version
 * ═══════════════════════════════════════════════════════════════ */
void bug_unsafe_iteration(void)
{
    printf("=== BUG 5: Unsafe Iteration During Modification ===\n");
    
    struct list_head head;
    INIT_LIST_HEAD(&head);
    
    struct item items[3];
    for (int i = 0; i < 3; i++) {
        items[i].value = i;
        list_add(&items[i].link, &head);
    }
    
    /* WRONG: Modifying during iteration */
    /*
    struct list_head *pos;
    for (pos = head.next; pos != &head; pos = pos->next) {
        struct item *it = container_of(pos, struct item, link);
        if (it->value == 1) {
            list_del(pos);  // Now pos->next is invalid!
        }
    }
    */
    
    /* CORRECT: Save next before potential deletion */
    struct list_head *pos, *tmp;
    for (pos = head.next, tmp = pos->next; pos != &head; pos = tmp, tmp = pos->next) {
        struct item *it = container_of(pos, struct item, link);
        if (it->value == 1) {
            list_del(pos);
            printf("Safely removed item with value 1\n");
        }
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 6: Assuming list_head is at offset 0
 * ═══════════════════════════════════════════════════════════════ */
struct wrong_assumption {
    struct list_head link;  /* At offset 0 */
    int data;
};

struct correct_struct {
    int data;
    struct list_head link;  /* NOT at offset 0 */
};

void bug_offset_assumption(void)
{
    printf("=== BUG 6: Offset Assumption ===\n");
    
    struct correct_struct obj;
    obj.data = 42;
    
    /* WRONG: Casting directly */
    /* struct correct_struct *wrong = (struct correct_struct *)&obj.link; */
    /* wrong->data would be garbage! */
    
    /* CORRECT: Always use container_of */
    struct list_head *ptr = &obj.link;
    struct correct_struct *correct = container_of(ptr, struct correct_struct, link);
    printf("Correct data: %d\n", correct->data);
    printf("container_of handles any offset!\n\n");
}

int main(void)
{
    bug_free_without_remove();
    bug_wrong_member();
    bug_uninitialized();
    bug_double_add();
    bug_unsafe_iteration();
    bug_offset_assumption();
    
    printf("=== Summary of Intrusive List Rules ===\n");
    printf("1. Always list_del() before free()\n");
    printf("2. Use correct member name in container_of\n");
    printf("3. Always INIT_LIST_HEAD() before use\n");
    printf("4. One list_head per list membership\n");
    printf("5. Use safe iteration when modifying\n");
    printf("6. Never cast directly - use container_of\n");
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

### Memory Overhead Comparison

```
+------------------------------------------------------------------+
|  MEMORY OVERHEAD: INTRUSIVE VS NON-INTRUSIVE                     |
+------------------------------------------------------------------+

    Example: 1000 objects, each 100 bytes, doubly linked

    NON-INTRUSIVE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Per object:                                                │
    │  - Data object: 100 bytes                                   │
    │  - List node: 24 bytes (next, prev, data ptr)               │
    │  - malloc metadata: ~32 bytes × 2 allocations               │
    │                                                              │
    │  Total per object: ~188 bytes (88% overhead!)               │
    │  Total for 1000: ~188 KB                                    │
    └─────────────────────────────────────────────────────────────┘

    INTRUSIVE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Per object:                                                │
    │  - Data object + embedded node: 116 bytes (100 + 16)        │
    │  - malloc metadata: ~32 bytes × 1 allocation                │
    │                                                              │
    │  Total per object: ~148 bytes (48% overhead)                │
    │  Total for 1000: ~148 KB                                    │
    │                                                              │
    │  SAVES: 40 KB (21% reduction!)                              │
    └─────────────────────────────────────────────────────────────┘
```

### Performance Comparison

```
+------------------------------------------------------------------+
|  PERFORMANCE CHARACTERISTICS                                     |
+------------------------------------------------------------------+

    ┌───────────────────────┬─────────────────┬──────────────────┐
    │ Operation             │ Non-Intrusive   │ Intrusive        │
    ├───────────────────────┼─────────────────┼──────────────────┤
    │ Access data from node │ 1 pointer deref │ Arithmetic only  │
    │ Memory allocations    │ 2 per element   │ 1 per element    │
    │ Cache behavior        │ Poor (separate) │ Better (together)│
    │ Add to list           │ Alloc + link    │ Link only        │
    │ Remove from list      │ Unlink + free   │ Unlink only      │
    │ Object on N lists     │ N allocations   │ N embedded nodes │
    └───────────────────────┴─────────────────┴──────────────────┘
```

---

## 6. Design & Engineering Takeaways

### When to Use Intrusive Lists

```
+------------------------------------------------------------------+
|  DECISION FRAMEWORK                                              |
+------------------------------------------------------------------+

    USE INTRUSIVE WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ You control the struct definition                       │
    │  ✓ Objects already exist (not created for list)             │
    │  ✓ Objects need multiple list memberships                   │
    │  ✓ Performance/memory critical                              │
    │  ✓ Building system software                                 │
    └─────────────────────────────────────────────────────────────┘

    USE NON-INTRUSIVE WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ You don't control the struct                            │
    │  ✓ Storing primitives (int, char, etc.)                     │
    │  ✓ Simplicity more important than performance               │
    │  ✓ Generic data structure library needed                    │
    └─────────────────────────────────────────────────────────────┘
```

### Key Implementation Rules

```
+------------------------------------------------------------------+
|  INTRUSIVE LIST IMPLEMENTATION RULES                             |
+------------------------------------------------------------------+

    1. ALWAYS USE container_of
       Never cast list_head* directly to container type

    2. REMOVE BEFORE FREE
       list_del() must precede free()

    3. INITIALIZE BEFORE USE
       INIT_LIST_HEAD() for every list head and node

    4. ONE NODE PER LIST
       Each list membership needs its own list_head member

    5. SAFE ITERATION FOR MODIFICATION
       Use list_for_each_entry_safe() when removing

    6. DOCUMENT LIST OWNERSHIP
       Comment which list each list_head member belongs to
```

---

## Summary

```
+------------------------------------------------------------------+
|  INTRUSIVE LINKED LIST: KEY TAKEAWAYS                            |
+------------------------------------------------------------------+

    CORE INSIGHT:
    Embed the list node inside your data structure.
    Use container_of to recover the container from the node.

    ADVANTAGES:
    - Single allocation per object
    - Better cache locality
    - Multiple list membership
    - No void* casting needed
    - Compile-time type safety

    DISADVANTAGES:
    - More complex API (container_of)
    - Requires modifying struct definition
    - Careful lifetime management
    - Easy to make mistakes

    KEY PATTERN:
    struct my_object {
        int data;
        struct list_head list;  // Embedded node
    };

    struct my_object *obj = container_of(node, struct my_object, list);

    USED BY:
    - Linux kernel (exclusively)
    - BSD queue.h
    - High-performance systems
    - Memory allocators
    - Game engines
```

**中文总结：**
- **核心思想**：将链表节点嵌入数据结构，用 container_of 恢复容器
- **优势**：单次分配、更好缓存、多列表成员资格、类型安全
- **劣势**：API 复杂、需要修改结构定义、生命周期管理复杂
- **用户**：Linux 内核、BSD、高性能系统、内存分配器、游戏引擎

