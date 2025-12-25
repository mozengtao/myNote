# Singly Linked List in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE PROBLEM: DYNAMIC COLLECTIONS WITH O(1) INSERTION            |
+------------------------------------------------------------------+

    Arrays have fundamental limitations:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Fixed size (or costly reallocation)                     │
    │  2. O(n) insertion/deletion in middle                       │
    │  3. Cannot grow without copying                             │
    └─────────────────────────────────────────────────────────────┘

    LINKED LIST SOLUTION:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Each element contains pointer to next element            │
    │  • No contiguous memory required                            │
    │  • O(1) insert/delete IF you have pointer to position       │
    │  • Grows/shrinks naturally without reallocation             │
    └─────────────────────────────────────────────────────────────┘

    TRADE-OFF:
    ┌─────────────────────────────────────────────────────────────┐
    │  Give up: O(1) random access, cache locality                │
    │  Gain:    O(1) insertion/deletion, flexible sizing          │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 链表解决数组的固定大小和中间插入 O(n) 的问题
- 每个元素包含指向下一个元素的指针
- 代价：失去 O(1) 随机访问和缓存局部性
- 收益：O(1) 插入/删除，灵活扩展

### Structure Definition

```c
/* Basic singly linked list node */
struct slist_node {
    void *data;              /* Pointer to actual data */
    struct slist_node *next; /* Pointer to next node */
};

/* List head (optional but recommended) */
struct slist {
    struct slist_node *head; /* First node */
    struct slist_node *tail; /* Last node (optional, for O(1) append) */
    size_t count;            /* Number of nodes (optional) */
};
```

### Invariants

```
+------------------------------------------------------------------+
|  SINGLY LINKED LIST INVARIANTS                                   |
+------------------------------------------------------------------+

    1. CHAIN STRUCTURE
       Each node points to exactly one next node (or NULL)
       Following next pointers eventually reaches NULL

    2. NO CYCLES (for standard list)
       There is no path from a node back to itself
       Traversal always terminates

    3. SINGLE DIRECTION
       Can only traverse forward (head → tail)
       Cannot go backwards without O(n) work

    4. HEAD POINTS TO FIRST
       list->head == NULL means empty list
       list->head->next == NULL means single element

    5. TAIL INVARIANT (if maintained)
       list->tail points to last node
       list->tail->next == NULL always
```

### Design Philosophy

```
+------------------------------------------------------------------+
|  WHY LINKED LISTS ARE SHAPED THIS WAY                            |
+------------------------------------------------------------------+

    MINIMAL NODE STRUCTURE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Only ONE pointer per node (next)                           │
    │  Minimum overhead for list structure                        │
    │  Compare: doubly-linked needs TWO pointers                  │
    └─────────────────────────────────────────────────────────────┘

    DECOUPLED ALLOCATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  Each node allocated independently                          │
    │  No need to find large contiguous block                     │
    │  Natural fragmentation tolerance                            │
    └─────────────────────────────────────────────────────────────┘

    POINTER-BASED NAVIGATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  No index calculation needed                                │
    │  Follows pointer = one memory load                          │
    │  But: pointer chasing = cache misses                        │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Memory Model

### Memory Layout

```
+------------------------------------------------------------------+
|  SINGLY LINKED LIST MEMORY LAYOUT                                |
+------------------------------------------------------------------+

    struct slist list;
    
    Stack (list head):          Heap (scattered nodes):
    ┌──────────────────┐
    │ head = 0x1000    │ ────────────────────────────────────────┐
    │ tail = 0x3000    │ ─────────────────────────────────────┐  │
    │ count = 3        │                                      │  │
    └──────────────────┘                                      │  │
                                                              │  │
    0x1000 (Node A):                                          │  │
    ┌──────────────┬───────────┐                              │  │
    │ data = 0x4000│ next=0x2000│ ────────────────────┐       │  │
    └──────────────┴───────────┘                      │       │  │
                                                      ▼       │  │
    0x2000 (Node B):                                          │  │
    ┌──────────────┬───────────┐                              │  │
    │ data = 0x5000│ next=0x3000│ ──────────────┐             │  │
    └──────────────┴───────────┘                │             │  │
                                                ▼             │  │
    0x3000 (Node C):                            ◀─────────────┘  │
    ┌──────────────┬───────────┐                                 │
    │ data = 0x6000│ next=NULL │ ◀───────────────────────────────┘
    └──────────────┴───────────┘

    KEY OBSERVATION:
    - Nodes are SCATTERED in heap memory
    - Connected ONLY by pointers
    - No guarantee of contiguity
```

**中文解释：**
- 链表节点分散在堆内存中
- 仅通过指针连接
- 没有连续性保证
- 每个节点独立分配，独立释放

### Ownership Model

```
+------------------------------------------------------------------+
|  OWNERSHIP AND LIFETIME                                          |
+------------------------------------------------------------------+

    WHO OWNS WHAT?
    
    ┌─────────────────────────────────────────────────────────────┐
    │  1. LIST owns NODES                                         │
    │     - List functions allocate nodes                         │
    │     - List functions free nodes                             │
    │                                                              │
    │  2. NODES may or may not own DATA                           │
    │     Option A: List copies data (list owns copies)           │
    │     Option B: List stores pointers (caller owns data)       │
    │     Option C: List takes ownership (transfers to list)      │
    └─────────────────────────────────────────────────────────────┘

    COMMON PATTERNS:

    Pattern A: Copy semantics (simple, safe)
    ┌─────────────────────────────────────────────────────────────┐
    │  void slist_add(struct slist *list, const void *data,       │
    │                 size_t data_size)                           │
    │  {                                                          │
    │      node->data = malloc(data_size);                        │
    │      memcpy(node->data, data, data_size);                   │
    │  }                                                          │
    │                                                              │
    │  /* List frees both node AND data */                        │
    │  void slist_remove(struct slist *list, ...)                 │
    │  {                                                          │
    │      free(node->data);                                      │
    │      free(node);                                            │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    Pattern B: Reference semantics (efficient, careful)
    ┌─────────────────────────────────────────────────────────────┐
    │  void slist_add_ref(struct slist *list, void *data)         │
    │  {                                                          │
    │      node->data = data;  /* Just store pointer */           │
    │  }                                                          │
    │                                                              │
    │  /* List frees ONLY node, caller frees data */              │
    │  void slist_remove_ref(struct slist *list, ...)             │
    │  {                                                          │
    │      void *data = node->data;                               │
    │      free(node);                                            │
    │      return data;  /* Caller must free */                   │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

### Cache Behavior

```
+------------------------------------------------------------------+
|  CACHE BEHAVIOR: WHY LINKED LISTS ARE SLOW                       |
+------------------------------------------------------------------+

    ARRAY TRAVERSAL:
    ┌─────────────────────────────────────────────────────────────┐
    │  Cache line: [elem0][elem1][elem2]...[elem15]               │
    │  Access elem0 → miss, loads 16 elements                     │
    │  Access elem1-15 → HIT HIT HIT... (free!)                   │
    │  Miss rate: ~6%                                             │
    └─────────────────────────────────────────────────────────────┘

    LINKED LIST TRAVERSAL:
    ┌─────────────────────────────────────────────────────────────┐
    │  Cache line 1: [node_A][garbage...]                         │
    │  Cache line 2: [garbage...][node_B][garbage...]             │
    │  Cache line 3: [garbage...][node_C]                         │
    │                                                              │
    │  Access node_A → miss                                       │
    │  Access node_B → miss (different cache line!)               │
    │  Access node_C → miss                                       │
    │  Miss rate: ~100% (worst case)                              │
    └─────────────────────────────────────────────────────────────┘

    REAL IMPACT:
    ┌─────────────────────────────────────────────────────────────┐
    │  L1 cache hit:  ~4 cycles                                   │
    │  Main memory:   ~200 cycles                                 │
    │                                                              │
    │  Linked list traversal can be 50× slower than array!        │
    │  This is THE major cost of linked lists                     │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 数组遍历：一次缓存加载，16 个元素命中，缓存缺失率约 6%
- 链表遍历：每个节点可能导致缓存缺失，缺失率接近 100%
- 主内存访问比 L1 缓存慢约 50 倍
- 这是链表的主要性能代价

### Failure Modes

```
+------------------------------------------------------------------+
|  COMMON FAILURE MODES                                            |
+------------------------------------------------------------------+

    1. MEMORY LEAK (forgetting to free nodes)
    ┌─────────────────────────────────────────────────────────────┐
    │  void bad_clear(struct slist *list) {                       │
    │      list->head = NULL;  /* LEAKED all nodes! */            │
    │  }                                                          │
    │                                                              │
    │  CORRECT:                                                   │
    │  void good_clear(struct slist *list) {                      │
    │      while (list->head) {                                   │
    │          struct slist_node *tmp = list->head;               │
    │          list->head = tmp->next;                            │
    │          free(tmp);                                         │
    │      }                                                      │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    2. USE-AFTER-FREE (dangling pointers)
    ┌─────────────────────────────────────────────────────────────┐
    │  struct slist_node *node = list->head;                      │
    │  slist_remove(list, node);                                  │
    │  printf("%d\n", node->data);  /* CRASH! node is freed */    │
    └─────────────────────────────────────────────────────────────┘

    3. LOSING THE HEAD
    ┌─────────────────────────────────────────────────────────────┐
    │  struct slist_node *p = list->head;                         │
    │  p = p->next;  /* OK, p is a copy */                        │
    │  list->head = list->head->next;  /* OOPS! Lost first node */ │
    └─────────────────────────────────────────────────────────────┘

    4. INFINITE LOOP (cycle in list)
    ┌─────────────────────────────────────────────────────────────┐
    │  Accidentally: last->next = some_earlier_node;              │
    │  while (p != NULL) {                                        │
    │      p = p->next;  /* Never ends! */                        │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    5. NULL POINTER DEREFERENCE
    ┌─────────────────────────────────────────────────────────────┐
    │  /* Forgot to check for empty list */                       │
    │  return list->head->data;  /* CRASH if head is NULL */      │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Typical Application Scenarios

### Where Singly Linked Lists Are Used

```
+------------------------------------------------------------------+
|  REAL-WORLD APPLICATIONS                                         |
+------------------------------------------------------------------+

    KERNEL SPACE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Free lists in memory allocators (slab, buddy)            │
    │  • Network packet queues (sk_buff chains)                   │
    │  • Hash table bucket chains                                 │
    │  • Work queues and deferred processing                      │
    │  • Device driver interrupt chains                           │
    └─────────────────────────────────────────────────────────────┘

    USER SPACE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Symbol tables in compilers                               │
    │  • Undo/redo stacks (can use singly linked)                 │
    │  • Memory pools and free lists                              │
    │  • Graph adjacency lists                                    │
    │  • Polynomial representation                                │
    └─────────────────────────────────────────────────────────────┘

    EMBEDDED SYSTEMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Event queues                                             │
    │  • Timer chains                                             │
    │  • Message passing systems                                  │
    │  • Buffer management                                        │
    └─────────────────────────────────────────────────────────────┘
```

### When to Use Singly Linked List

```
+------------------------------------------------------------------+
|  USE SINGLY LINKED LIST WHEN:                                    |
+------------------------------------------------------------------+

    ✓ You only need forward traversal
    ✓ Frequent insertions at head (O(1))
    ✓ Implementing a stack (LIFO)
    ✓ Elements are large (pointer overhead is small relative)
    ✓ Size is highly unpredictable
    ✓ Memory is fragmented (no large contiguous block available)
    ✓ You need to splice lists together
```

### When NOT to Use

```
+------------------------------------------------------------------+
|  AVOID SINGLY LINKED LIST WHEN:                                  |
+------------------------------------------------------------------+

    ✗ You need random access by index
    ✗ You need to traverse backwards
    ✗ Elements are small (int, char) — overhead dominates
    ✗ Cache performance is critical
    ✗ You need frequent deletions (need doubly-linked for O(1))
    ✗ You need to search frequently (O(n) every time)
    ✗ List is small and fixed-size (just use array)
```

---

## 4. Complete C Examples

### Example 1: Minimal Implementation

```c
/*
 * Example 1: Minimal Singly Linked List
 *
 * Demonstrates: basic structure, create, traverse, destroy
 * Compile: gcc -Wall -Wextra -o slist_minimal slist_minimal.c
 */

#include <stdio.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════
 * Node structure: stores integer data
 * ═══════════════════════════════════════════════════════════════ */
struct node {
    int data;
    struct node *next;
};

/* ═══════════════════════════════════════════════════════════════
 * Create a new node
 * Returns NULL on allocation failure
 * ═══════════════════════════════════════════════════════════════ */
struct node *node_create(int data)
{
    struct node *n = malloc(sizeof(*n));
    if (!n)
        return NULL;
    
    n->data = data;
    n->next = NULL;
    return n;
}

/* ═══════════════════════════════════════════════════════════════
 * Insert at head (O(1))
 * Returns new head
 * ═══════════════════════════════════════════════════════════════ */
struct node *list_prepend(struct node *head, int data)
{
    struct node *new_node = node_create(data);
    if (!new_node)
        return head;  /* Return unchanged on failure */
    
    new_node->next = head;
    return new_node;  /* New node is now head */
}

/* ═══════════════════════════════════════════════════════════════
 * Insert at tail (O(n) without tail pointer)
 * ═══════════════════════════════════════════════════════════════ */
struct node *list_append(struct node *head, int data)
{
    struct node *new_node = node_create(data);
    if (!new_node)
        return head;
    
    /* Empty list case */
    if (!head)
        return new_node;
    
    /* Find the last node */
    struct node *p = head;
    while (p->next)
        p = p->next;
    
    p->next = new_node;
    return head;
}

/* ═══════════════════════════════════════════════════════════════
 * Print all elements
 * ═══════════════════════════════════════════════════════════════ */
void list_print(struct node *head)
{
    printf("List: ");
    for (struct node *p = head; p != NULL; p = p->next) {
        printf("%d -> ", p->data);
    }
    printf("NULL\n");
}

/* ═══════════════════════════════════════════════════════════════
 * Count elements
 * ═══════════════════════════════════════════════════════════════ */
size_t list_count(struct node *head)
{
    size_t count = 0;
    for (struct node *p = head; p != NULL; p = p->next)
        count++;
    return count;
}

/* ═══════════════════════════════════════════════════════════════
 * Free all nodes (CRITICAL: prevent memory leaks!)
 * ═══════════════════════════════════════════════════════════════ */
void list_destroy(struct node *head)
{
    struct node *p = head;
    while (p) {
        struct node *next = p->next;  /* Save next BEFORE freeing */
        free(p);
        p = next;
    }
}

int main(void)
{
    struct node *list = NULL;  /* Empty list */
    
    /* Build list: prepend is O(1) */
    printf("=== Building list with prepend ===\n");
    list = list_prepend(list, 30);
    list = list_prepend(list, 20);
    list = list_prepend(list, 10);
    list_print(list);
    printf("Count: %zu\n\n", list_count(list));
    
    /* Append is O(n) */
    printf("=== Appending element ===\n");
    list = list_append(list, 40);
    list_print(list);
    
    /* Cleanup */
    printf("\n=== Destroying list ===\n");
    list_destroy(list);
    printf("Done.\n");
    
    return 0;
}
```

**Output:**
```
=== Building list with prepend ===
List: 10 -> 20 -> 30 -> NULL
Count: 3

=== Appending element ===
List: 10 -> 20 -> 30 -> 40 -> NULL

=== Destroying list ===
Done.
```

---

### Example 2: Full-Featured List with Head Structure

```c
/*
 * Example 2: Complete Singly Linked List Implementation
 *
 * Features: head/tail pointers, count, insert, delete, search
 * Compile: gcc -Wall -Wextra -o slist_complete slist_complete.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * Data type (example: store strings)
 * ═══════════════════════════════════════════════════════════════ */
typedef char *string_t;

/* ═══════════════════════════════════════════════════════════════
 * Node structure
 * ═══════════════════════════════════════════════════════════════ */
struct slist_node {
    string_t data;           /* Owned string */
    struct slist_node *next;
};

/* ═══════════════════════════════════════════════════════════════
 * List head structure
 * ═══════════════════════════════════════════════════════════════ */
struct slist {
    struct slist_node *head;
    struct slist_node *tail;
    size_t count;
};

/* ═══════════════════════════════════════════════════════════════
 * Initialize list
 * ═══════════════════════════════════════════════════════════════ */
void slist_init(struct slist *list)
{
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Check if empty
 * ═══════════════════════════════════════════════════════════════ */
bool slist_empty(struct slist *list)
{
    return list->head == NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * Create node with copied string
 * ═══════════════════════════════════════════════════════════════ */
static struct slist_node *node_create(const char *str)
{
    struct slist_node *node = malloc(sizeof(*node));
    if (!node)
        return NULL;
    
    node->data = strdup(str);  /* Copy string */
    if (!node->data) {
        free(node);
        return NULL;
    }
    
    node->next = NULL;
    return node;
}

/* ═══════════════════════════════════════════════════════════════
 * Insert at front (O(1))
 * ═══════════════════════════════════════════════════════════════ */
bool slist_push_front(struct slist *list, const char *str)
{
    struct slist_node *node = node_create(str);
    if (!node)
        return false;
    
    node->next = list->head;
    list->head = node;
    
    /* First element? Update tail too */
    if (!list->tail)
        list->tail = node;
    
    list->count++;
    return true;
}

/* ═══════════════════════════════════════════════════════════════
 * Insert at back (O(1) with tail pointer!)
 * ═══════════════════════════════════════════════════════════════ */
bool slist_push_back(struct slist *list, const char *str)
{
    struct slist_node *node = node_create(str);
    if (!node)
        return false;
    
    if (list->tail) {
        list->tail->next = node;
        list->tail = node;
    } else {
        /* Empty list */
        list->head = node;
        list->tail = node;
    }
    
    list->count++;
    return true;
}

/* ═══════════════════════════════════════════════════════════════
 * Remove from front (O(1))
 * Returns removed string (caller must free) or NULL if empty
 * ═══════════════════════════════════════════════════════════════ */
char *slist_pop_front(struct slist *list)
{
    if (slist_empty(list))
        return NULL;
    
    struct slist_node *node = list->head;
    char *data = node->data;
    
    list->head = node->next;
    if (!list->head)
        list->tail = NULL;  /* List is now empty */
    
    free(node);
    list->count--;
    
    return data;
}

/* ═══════════════════════════════════════════════════════════════
 * Find node containing string
 * Returns pointer to node or NULL
 * ═══════════════════════════════════════════════════════════════ */
struct slist_node *slist_find(struct slist *list, const char *str)
{
    for (struct slist_node *p = list->head; p; p = p->next) {
        if (strcmp(p->data, str) == 0)
            return p;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * Remove specific value (O(n))
 * Returns true if found and removed
 * ═══════════════════════════════════════════════════════════════ */
bool slist_remove(struct slist *list, const char *str)
{
    struct slist_node *prev = NULL;
    struct slist_node *curr = list->head;
    
    while (curr) {
        if (strcmp(curr->data, str) == 0) {
            /* Found it! Unlink */
            if (prev)
                prev->next = curr->next;
            else
                list->head = curr->next;
            
            /* Update tail if removing last element */
            if (curr == list->tail)
                list->tail = prev;
            
            /* Free node and data */
            free(curr->data);
            free(curr);
            list->count--;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════
 * Print list
 * ═══════════════════════════════════════════════════════════════ */
void slist_print(struct slist *list)
{
    printf("List [%zu items]: ", list->count);
    for (struct slist_node *p = list->head; p; p = p->next) {
        printf("\"%s\"", p->data);
        if (p->next)
            printf(" -> ");
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════
 * Free all nodes and data
 * ═══════════════════════════════════════════════════════════════ */
void slist_destroy(struct slist *list)
{
    struct slist_node *p = list->head;
    while (p) {
        struct slist_node *next = p->next;
        free(p->data);  /* Free owned string */
        free(p);        /* Free node */
        p = next;
    }
    slist_init(list);  /* Reset to empty state */
}

/* ═══════════════════════════════════════════════════════════════
 * Demonstration
 * ═══════════════════════════════════════════════════════════════ */
int main(void)
{
    struct slist list;
    slist_init(&list);
    
    /* Insert elements */
    printf("=== Building list ===\n");
    slist_push_back(&list, "Alice");
    slist_push_back(&list, "Bob");
    slist_push_back(&list, "Charlie");
    slist_push_front(&list, "Zoe");  /* Insert at front */
    slist_print(&list);
    
    /* Search */
    printf("\n=== Searching ===\n");
    printf("Find 'Bob': %s\n", 
           slist_find(&list, "Bob") ? "Found" : "Not found");
    printf("Find 'Dave': %s\n",
           slist_find(&list, "Dave") ? "Found" : "Not found");
    
    /* Remove */
    printf("\n=== Removing 'Bob' ===\n");
    slist_remove(&list, "Bob");
    slist_print(&list);
    
    /* Pop front */
    printf("\n=== Pop front ===\n");
    char *popped = slist_pop_front(&list);
    printf("Popped: \"%s\"\n", popped);
    free(popped);  /* Caller owns popped data */
    slist_print(&list);
    
    /* Cleanup */
    printf("\n=== Cleanup ===\n");
    slist_destroy(&list);
    printf("Destroyed. Count: %zu\n", list.count);
    
    return 0;
}
```

---

### Example 3: Stack Implementation Using Singly Linked List

```c
/*
 * Example 3: Stack Using Singly Linked List
 *
 * Perfect use case: LIFO operations only need head access
 * Compile: gcc -Wall -Wextra -o stack_slist stack_slist.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

/* ═══════════════════════════════════════════════════════════════
 * Stack node
 * ═══════════════════════════════════════════════════════════════ */
struct stack_node {
    int value;
    struct stack_node *next;
};

/* ═══════════════════════════════════════════════════════════════
 * Stack structure (just needs head pointer)
 * ═══════════════════════════════════════════════════════════════ */
struct stack {
    struct stack_node *top;
    size_t size;
};

/* ═══════════════════════════════════════════════════════════════
 * Initialize empty stack
 * ═══════════════════════════════════════════════════════════════ */
void stack_init(struct stack *s)
{
    s->top = NULL;
    s->size = 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Check if empty
 * ═══════════════════════════════════════════════════════════════ */
bool stack_empty(struct stack *s)
{
    return s->top == NULL;
}

/* ═══════════════════════════════════════════════════════════════
 * Push: O(1) — just prepend to head
 * ═══════════════════════════════════════════════════════════════ */
bool stack_push(struct stack *s, int value)
{
    struct stack_node *node = malloc(sizeof(*node));
    if (!node)
        return false;
    
    node->value = value;
    node->next = s->top;  /* Point to old top */
    s->top = node;        /* New node is top */
    s->size++;
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════
 * Pop: O(1) — just remove head
 * ═══════════════════════════════════════════════════════════════ */
int stack_pop(struct stack *s)
{
    assert(!stack_empty(s));  /* Fail-fast on programmer error */
    
    struct stack_node *node = s->top;
    int value = node->value;
    
    s->top = node->next;
    free(node);
    s->size--;
    
    return value;
}

/* ═══════════════════════════════════════════════════════════════
 * Peek: O(1) — view without removing
 * ═══════════════════════════════════════════════════════════════ */
int stack_peek(struct stack *s)
{
    assert(!stack_empty(s));
    return s->top->value;
}

/* ═══════════════════════════════════════════════════════════════
 * Destroy stack
 * ═══════════════════════════════════════════════════════════════ */
void stack_destroy(struct stack *s)
{
    while (!stack_empty(s)) {
        struct stack_node *node = s->top;
        s->top = node->next;
        free(node);
    }
    s->size = 0;
}

/* ═══════════════════════════════════════════════════════════════
 * Print stack (top to bottom)
 * ═══════════════════════════════════════════════════════════════ */
void stack_print(struct stack *s)
{
    printf("Stack (size=%zu): TOP -> ", s->size);
    for (struct stack_node *p = s->top; p; p = p->next) {
        printf("[%d] -> ", p->value);
    }
    printf("BOTTOM\n");
}

/* ═══════════════════════════════════════════════════════════════
 * Real-world example: Balanced parentheses checker
 * ═══════════════════════════════════════════════════════════════ */
bool check_balanced(const char *expr)
{
    struct stack s;
    stack_init(&s);
    
    for (const char *p = expr; *p; p++) {
        switch (*p) {
        case '(':
        case '[':
        case '{':
            stack_push(&s, *p);
            break;
            
        case ')':
            if (stack_empty(&s) || stack_pop(&s) != '(')
                goto unbalanced;
            break;
            
        case ']':
            if (stack_empty(&s) || stack_pop(&s) != '[')
                goto unbalanced;
            break;
            
        case '}':
            if (stack_empty(&s) || stack_pop(&s) != '{')
                goto unbalanced;
            break;
        }
    }
    
    bool result = stack_empty(&s);
    stack_destroy(&s);
    return result;

unbalanced:
    stack_destroy(&s);
    return false;
}

int main(void)
{
    struct stack s;
    stack_init(&s);
    
    /* Basic operations */
    printf("=== Stack Operations ===\n");
    stack_push(&s, 10);
    stack_push(&s, 20);
    stack_push(&s, 30);
    stack_print(&s);
    
    printf("Peek: %d\n", stack_peek(&s));
    printf("Pop: %d\n", stack_pop(&s));
    stack_print(&s);
    
    stack_destroy(&s);
    
    /* Real-world: balanced parentheses */
    printf("\n=== Balanced Parentheses ===\n");
    const char *tests[] = {
        "(a + b)",
        "((a + b) * c)",
        "(a + b]",
        "{[()]}",
        "((())"
    };
    
    for (int i = 0; i < 5; i++) {
        printf("\"%s\" -> %s\n", tests[i],
               check_balanced(tests[i]) ? "Balanced" : "Unbalanced");
    }
    
    return 0;
}
```

---

### Example 4: Intrusive Singly Linked List (Linux Kernel Style)

```c
/*
 * Example 4: Intrusive Singly Linked List
 *
 * Key insight: Embed the list node INSIDE your data structure
 * No separate allocation for nodes!
 * This is how the Linux kernel does it.
 *
 * Compile: gcc -Wall -Wextra -o intrusive_slist intrusive_slist.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>  /* offsetof */
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * Intrusive list node — embedded in your structure
 * ═══════════════════════════════════════════════════════════════ */
struct slist_node {
    struct slist_node *next;
};

/* ═══════════════════════════════════════════════════════════════
 * List head
 * ═══════════════════════════════════════════════════════════════ */
struct slist_head {
    struct slist_node *first;
};

/* ═══════════════════════════════════════════════════════════════
 * container_of: recover containing structure from embedded member
 *
 * This is THE KEY MECHANISM of intrusive lists!
 * ═══════════════════════════════════════════════════════════════ */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* ═══════════════════════════════════════════════════════════════
 * List operations
 * ═══════════════════════════════════════════════════════════════ */
#define SLIST_HEAD_INIT { .first = NULL }

static inline void slist_init(struct slist_head *head)
{
    head->first = NULL;
}

static inline int slist_empty(struct slist_head *head)
{
    return head->first == NULL;
}

/* Add to front */
static inline void slist_add(struct slist_node *node, struct slist_head *head)
{
    node->next = head->first;
    head->first = node;
}

/* Remove from front */
static inline struct slist_node *slist_pop(struct slist_head *head)
{
    struct slist_node *node = head->first;
    if (node)
        head->first = node->next;
    return node;
}

/* Iteration macros */
#define slist_for_each(pos, head) \
    for (pos = (head)->first; pos != NULL; pos = pos->next)

#define slist_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define slist_for_each_entry(pos, head, member) \
    for (pos = slist_entry((head)->first, typeof(*pos), member); \
         &pos->member != NULL; \
         pos = slist_entry(pos->member.next, typeof(*pos), member))

/* ═══════════════════════════════════════════════════════════════
 * Example: Task structure with embedded list node
 * ═══════════════════════════════════════════════════════════════ */
struct task {
    int id;
    int priority;
    char name[32];
    struct slist_node node;  /* EMBEDDED list node */
};

/* ═══════════════════════════════════════════════════════════════
 * Create task (allocates task, node is embedded)
 * ═══════════════════════════════════════════════════════════════ */
struct task *task_create(int id, int priority, const char *name)
{
    struct task *t = malloc(sizeof(*t));
    if (!t)
        return NULL;
    
    t->id = id;
    t->priority = priority;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';
    
    /* Node is initialized when added to list */
    return t;
}

/* ═══════════════════════════════════════════════════════════════
 * Print task
 * ═══════════════════════════════════════════════════════════════ */
void task_print(struct task *t)
{
    printf("  Task[id=%d, pri=%d, name=\"%s\"]\n",
           t->id, t->priority, t->name);
}

int main(void)
{
    struct slist_head ready_queue = SLIST_HEAD_INIT;
    
    /* Create tasks */
    struct task *t1 = task_create(1, 10, "init");
    struct task *t2 = task_create(2, 5, "scheduler");
    struct task *t3 = task_create(3, 15, "worker");
    
    /* Add to list (via embedded node) */
    printf("=== Adding tasks ===\n");
    slist_add(&t1->node, &ready_queue);
    slist_add(&t2->node, &ready_queue);
    slist_add(&t3->node, &ready_queue);
    
    /* Iterate and print */
    printf("\n=== Ready Queue ===\n");
    struct slist_node *pos;
    slist_for_each(pos, &ready_queue) {
        /* Use container_of to get task from node */
        struct task *t = container_of(pos, struct task, node);
        task_print(t);
    }
    
    /* Pop and process */
    printf("\n=== Processing tasks ===\n");
    while (!slist_empty(&ready_queue)) {
        struct slist_node *node = slist_pop(&ready_queue);
        struct task *t = container_of(node, struct task, node);
        printf("Running: %s\n", t->name);
        free(t);  /* Free the task (includes the node!) */
    }
    
    printf("\n=== Memory layout demo ===\n");
    struct task demo;
    printf("sizeof(struct task) = %zu\n", sizeof(struct task));
    printf("offset of 'node' = %zu\n", offsetof(struct task, node));
    printf("No separate node allocation!\n");
    
    return 0;
}
```

**中文解释：**
- **侵入式链表**：将链表节点嵌入到数据结构中
- **container_of**：从嵌入的节点反推出包含它的结构
- **优势**：无需单独分配节点，一次 malloc 即可
- **这是 Linux 内核的标准做法**

---

### Example 5: Common Misuse and Bugs

```c
/*
 * Example 5: Singly Linked List Bugs and Anti-Patterns
 *
 * Demonstrates common mistakes and how to avoid them
 * Compile: gcc -Wall -Wextra -fsanitize=address -o slist_bugs slist_bugs.c
 */

#include <stdio.h>
#include <stdlib.h>

struct node {
    int data;
    struct node *next;
};

/* ═══════════════════════════════════════════════════════════════
 * BUG 1: Memory leak when clearing list
 * ═══════════════════════════════════════════════════════════════ */
void bug_memory_leak(void)
{
    printf("=== BUG 1: Memory Leak ===\n");
    
    /* Create list */
    struct node *head = malloc(sizeof(*head));
    head->data = 1;
    head->next = malloc(sizeof(*head));
    head->next->data = 2;
    head->next->next = NULL;
    
    /* WRONG: Just setting head to NULL leaks all nodes! */
    /* head = NULL; */
    
    /* CORRECT: Free each node */
    struct node *p = head;
    while (p) {
        struct node *next = p->next;
        free(p);
        p = next;
    }
    head = NULL;
    
    printf("Properly freed list\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 2: Use after free
 * ═══════════════════════════════════════════════════════════════ */
void bug_use_after_free(void)
{
    printf("=== BUG 2: Use After Free ===\n");
    
    struct node *head = malloc(sizeof(*head));
    head->data = 42;
    head->next = NULL;
    
    struct node *saved = head;  /* Save pointer */
    
    free(head);
    head = NULL;
    
    /* WRONG: saved still points to freed memory! */
    /* printf("Data: %d\n", saved->data);  // CRASH or garbage */
    
    printf("Avoided use-after-free by not accessing 'saved'\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 3: Losing nodes during iteration
 * ═══════════════════════════════════════════════════════════════ */
void bug_lost_nodes(void)
{
    printf("=== BUG 3: Lost Nodes During Removal ===\n");
    
    /* Create: 1 -> 2 -> 3 */
    struct node n1 = {1, NULL};
    struct node n2 = {2, NULL};
    struct node n3 = {3, NULL};
    n1.next = &n2;
    n2.next = &n3;
    
    struct node *head = &n1;
    
    /* Task: Remove node with data == 2 */
    
    /* WRONG approach (loses n3): */
    /*
    for (struct node *p = head; p; p = p->next) {
        if (p->data == 2) {
            // Oops! We need the PREVIOUS node to unlink!
            // And we're about to lose access to p->next
        }
    }
    */
    
    /* CORRECT: Track previous node */
    struct node *prev = NULL;
    struct node *curr = head;
    while (curr) {
        if (curr->data == 2) {
            if (prev)
                prev->next = curr->next;  /* Unlink */
            else
                head = curr->next;  /* Removing head */
            printf("Removed node with data=2\n");
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    
    /* Print remaining */
    printf("Remaining: ");
    for (struct node *p = head; p; p = p->next)
        printf("%d ", p->data);
    printf("\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 4: Infinite loop from cycle
 * ═══════════════════════════════════════════════════════════════ */
void bug_cycle(void)
{
    printf("=== BUG 4: Cycle Detection ===\n");
    
    /* Create: 1 -> 2 -> 3 -> (back to 2) */
    struct node n1 = {1, NULL};
    struct node n2 = {2, NULL};
    struct node n3 = {3, NULL};
    n1.next = &n2;
    n2.next = &n3;
    n3.next = &n2;  /* CYCLE! */
    
    /* WRONG: This would loop forever */
    /*
    for (struct node *p = &n1; p; p = p->next)
        printf("%d ", p->data);
    */
    
    /* CORRECT: Floyd's cycle detection (tortoise and hare) */
    struct node *slow = &n1;
    struct node *fast = &n1;
    int has_cycle = 0;
    
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) {
            has_cycle = 1;
            break;
        }
    }
    
    printf("Cycle detected: %s\n\n", has_cycle ? "YES" : "NO");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 5: Not handling empty list
 * ═══════════════════════════════════════════════════════════════ */
void bug_empty_list(void)
{
    printf("=== BUG 5: Empty List Handling ===\n");
    
    struct node *head = NULL;  /* Empty list */
    
    /* WRONG: Crashes on empty list */
    /* printf("First: %d\n", head->data); */
    
    /* CORRECT: Always check for NULL */
    if (head) {
        printf("First: %d\n", head->data);
    } else {
        printf("List is empty\n");
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 6: Incorrect insert at position
 * ═══════════════════════════════════════════════════════════════ */
void bug_insert_position(void)
{
    printf("=== BUG 6: Insert at Position ===\n");
    
    /* Create: 1 -> 3 */
    struct node *head = malloc(sizeof(*head));
    head->data = 1;
    head->next = malloc(sizeof(*head));
    head->next->data = 3;
    head->next->next = NULL;
    
    /* Insert 2 between 1 and 3 */
    struct node *new_node = malloc(sizeof(*new_node));
    new_node->data = 2;
    
    /* WRONG: This doesn't insert, it replaces! */
    /* new_node->next = head->next;
       head->next = new_node;  // Wait, this is actually correct... */
    
    /* The bug is more subtle: forgetting the order of operations */
    /* 
       head->next = new_node;      // WRONG ORDER! Lost pointer to node 3
       new_node->next = head->next; // Now points to itself!
    */
    
    /* CORRECT: Save next pointer FIRST */
    new_node->next = head->next;  /* Point to 3 */
    head->next = new_node;        /* 1 now points to 2 */
    
    printf("List after insert: ");
    for (struct node *p = head; p; p = p->next)
        printf("%d ", p->data);
    printf("\n\n");
    
    /* Cleanup */
    struct node *p = head;
    while (p) {
        struct node *next = p->next;
        free(p);
        p = next;
    }
}

int main(void)
{
    bug_memory_leak();
    bug_use_after_free();
    bug_lost_nodes();
    bug_cycle();
    bug_empty_list();
    bug_insert_position();
    
    printf("=== Key Lessons ===\n");
    printf("1. Always free all nodes before losing head pointer\n");
    printf("2. Don't use pointers after free()\n");
    printf("3. Track previous node for removal\n");
    printf("4. Use cycle detection if cycles are possible\n");
    printf("5. Always check for NULL/empty list\n");
    printf("6. Order of pointer updates matters!\n");
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

### Time Complexity

```
+------------------------------------------------------------------+
|  SINGLY LINKED LIST OPERATION COMPLEXITY                         |
+------------------------------------------------------------------+

    ┌─────────────────────────┬───────────────┬──────────────────────┐
    │ Operation               │ Complexity    │ Notes                │
    ├─────────────────────────┼───────────────┼──────────────────────┤
    │ Access by index         │ O(n)          │ Must traverse        │
    │ Insert at head          │ O(1)          │ Just update pointer  │
    │ Insert at tail (w/tail) │ O(1)          │ With tail pointer    │
    │ Insert at tail (no tail)│ O(n)          │ Must find end        │
    │ Insert after node       │ O(1)          │ If you have pointer  │
    │ Delete head             │ O(1)          │ Just update pointer  │
    │ Delete specific node    │ O(n)          │ Need previous node   │
    │ Search                  │ O(n)          │ Linear scan          │
    │ Length (no count)       │ O(n)          │ Must count           │
    │ Length (with count)     │ O(1)          │ Just return count    │
    └─────────────────────────┴───────────────┴──────────────────────┘
```

### Memory Overhead

```
+------------------------------------------------------------------+
|  MEMORY OVERHEAD ANALYSIS                                        |
+------------------------------------------------------------------+

    NON-INTRUSIVE LIST (separate nodes):
    ┌─────────────────────────────────────────────────────────────┐
    │  Per element overhead:                                      │
    │  - struct node: 8 bytes (next pointer on 64-bit)            │
    │  - malloc overhead: ~16 bytes (allocator metadata)          │
    │  - data pointer: 8 bytes (if storing pointers)              │
    │                                                              │
    │  Total: ~32 bytes overhead per element!                     │
    │  For int (4 bytes): 800% overhead!                          │
    └─────────────────────────────────────────────────────────────┘

    INTRUSIVE LIST (embedded nodes):
    ┌─────────────────────────────────────────────────────────────┐
    │  Per element overhead:                                      │
    │  - next pointer: 8 bytes only                               │
    │  - No separate allocation                                   │
    │                                                              │
    │  Much more efficient!                                       │
    └─────────────────────────────────────────────────────────────┘

    COMPARISON WITH ARRAY:
    ┌─────────────────────────────────────────────────────────────┐
    │  Array of 1000 ints:      4,000 bytes                       │
    │  Linked list (1000 ints): ~32,000 bytes (non-intrusive)     │
    │                            ~12,000 bytes (intrusive)        │
    │                                                              │
    │  8× worse (non-intrusive) or 3× worse (intrusive)          │
    └─────────────────────────────────────────────────────────────┘
```

### Comparison Table

```
+------------------------------------------------------------------+
|  SINGLY LINKED LIST VS ALTERNATIVES                              |
+------------------------------------------------------------------+

    ┌───────────────────┬────────────────┬────────────────┬────────────────┐
    │ Feature           │ Singly Linked  │ Array          │ Doubly Linked  │
    ├───────────────────┼────────────────┼────────────────┼────────────────┤
    │ Random access     │ O(n) ✗         │ O(1) ✓         │ O(n) ✗         │
    │ Insert at front   │ O(1) ✓         │ O(n) ✗         │ O(1) ✓         │
    │ Insert at back    │ O(1)* ✓        │ O(1) amort ✓   │ O(1) ✓         │
    │ Delete any node   │ O(n) ✗         │ O(n) ✗         │ O(1) ✓         │
    │ Memory overhead   │ 8B/elem        │ 0              │ 16B/elem       │
    │ Cache locality    │ Poor ✗         │ Excellent ✓    │ Poor ✗         │
    │ Backwards traverse│ Impossible ✗   │ Easy ✓         │ Easy ✓         │
    └───────────────────┴────────────────┴────────────────┴────────────────┘
    * With tail pointer
```

---

## 6. Design & Engineering Takeaways

### Rules of Thumb

```
+------------------------------------------------------------------+
|  SINGLY LINKED LIST RULES OF THUMB                               |
+------------------------------------------------------------------+

    1. USE FOR STACK OPERATIONS
       Push/pop at head is O(1) and natural fit

    2. MAINTAIN TAIL POINTER IF APPENDING
       Without tail: O(n) append
       With tail: O(1) append

    3. PREFER INTRUSIVE LISTS
       Embedded nodes = fewer allocations, better cache
       This is standard practice in kernel code

    4. CONSIDER DOUBLY-LINKED FOR DELETION
       Singly-linked delete is O(n) (need previous)
       Doubly-linked delete is O(1)

    5. CHECK FOR NULL EVERYWHERE
       Empty list = NULL head
       End of list = NULL next
       Every access needs NULL check

    6. ITERATE WITH CARE
       Save next pointer before modifying/freeing current node
```

### When to Choose Singly Linked List

```
+------------------------------------------------------------------+
|  DECISION CRITERIA                                               |
+------------------------------------------------------------------+

    CHOOSE SINGLY LINKED LIST WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Only need forward traversal                              │
    │  ✓ Mostly insert/delete at head (stack pattern)             │
    │  ✓ Memory overhead per element must be minimal              │
    │  ✓ Elements are large (pointer overhead is small relative)  │
    │  ✓ Implementing hash table buckets                          │
    │  ✓ Free lists in memory allocators                          │
    └─────────────────────────────────────────────────────────────┘

    CHOOSE DOUBLY LINKED INSTEAD WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Need backwards traversal                                 │
    │  ✓ Frequent deletion of arbitrary nodes                     │
    │  ✓ Need to move nodes between lists efficiently             │
    └─────────────────────────────────────────────────────────────┘

    CHOOSE ARRAY INSTEAD WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Need random access                                       │
    │  ✓ Size is relatively stable                                │
    │  ✓ Cache performance is critical                            │
    │  ✓ Elements are small (int, char, etc.)                     │
    └─────────────────────────────────────────────────────────────┘
```

### Professional Mindset

```
+------------------------------------------------------------------+
|  HOW EXPERTS THINK ABOUT LINKED LISTS                            |
+------------------------------------------------------------------+

    1. "WHAT'S THE ACCESS PATTERN?"
       - Sequential only? → List might work
       - Random access needed? → Use array
       - Insert-heavy? → List wins

    2. "WHAT'S THE REAL COST?"
       - Pointer chasing = cache misses
       - Cache miss = 50× slower than hit
       - Small lists: array usually wins anyway!

    3. "INTRUSIVE FIRST"
       - Default to intrusive lists
       - Separate node allocation = last resort
       - Study Linux kernel's list_head

    4. "OWNERSHIP MUST BE CLEAR"
       - Who allocates nodes?
       - Who frees them?
       - Does the list own the data or just reference it?

    5. "TEST THE EDGE CASES"
       - Empty list
       - Single element
       - Delete head/tail
       - All elements same value
```

---

## Summary

```
+------------------------------------------------------------------+
|  SINGLY LINKED LIST: KEY TAKEAWAYS                               |
+------------------------------------------------------------------+

    CORE INSIGHT:
    Singly linked lists trade random access for dynamic sizing and
    O(1) insertion at known positions. The cost is cache locality.
    
    BEST USE CASES:
    - Stack implementation (LIFO)
    - Hash table bucket chains
    - Free lists in allocators
    - When elements are large (overhead is proportionally small)
    
    AVOID WHEN:
    - Need random access
    - Need backwards traversal
    - Elements are small (overhead dominates)
    - Cache performance matters
    
    KEY IMPLEMENTATION POINTS:
    1. Use intrusive lists when possible
    2. Maintain tail pointer for O(1) append
    3. Always check for NULL
    4. Save next before freeing current
    5. Track previous for deletion
    6. Clear ownership model (who frees what)
```

**中文总结：**
- 单链表用随机访问换取动态大小和已知位置的 O(1) 插入
- 最佳用途：栈、哈希表桶链、内存分配器的空闲链表
- 避免场景：需要随机访问、反向遍历、小元素、缓存敏感
- 关键点：优先用侵入式链表、维护尾指针、始终检查 NULL、释放前保存 next

