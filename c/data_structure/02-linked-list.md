# Linked List (Singly & Doubly)

## 1. Technical Specification

### Characteristics

| Property | Singly Linked | Doubly Linked |
|----------|--------------|--------------|
| Memory per node | `data + next` | `data + next + prev` |
| Insert at head | O(1) | O(1) |
| Insert at tail | O(1) with tail ptr | O(1) |
| Delete (given node) | O(n) — need predecessor | O(1) |
| Search | O(n) | O(n) |
| Reverse traversal | Not possible | O(n) |
| Cache friendliness | Poor (scattered allocs) | Poor |

### Use Cases

- **Linux Kernel**: `list_head` intrusive doubly-linked list is ubiquitous (`include/linux/list.h`).
- **DPDK**: `TAILQ` macros for device lists, `rte_ring` internal chains.
- **Memory allocators**: Free-list management (slab allocators, jemalloc bins).
- **LRU Caches**: Doubly linked + hash map for O(1) eviction.
- **Undo/Redo**: Doubly linked command history.

### Trade-offs

| vs. Dynamic Array | Linked List Wins | Array Wins |
|-------------------|-----------------|------------|
| Insert/Delete at arbitrary position | O(1) given node | O(n) shift |
| Memory usage | Only what's needed | Slack capacity |
| Random access | O(n) | O(1) |
| Cache performance | Poor | Excellent |

---

## 2. Implementation Strategy

### Singly Linked List

```
slist_t
+------------+
| head ------+---> [data|next] ---> [data|next] ---> [data|next] ---> NULL
| tail ------+---> (points to last node)
| size = 3   |
| free_fn    |
+------------+
```

### Doubly Linked List

```
dlist_t
+------------+
| head ------+---> [prev|data|next] <===> [prev|data|next] <===> [prev|data|next]
| tail ------+---> (points to last node)           ^                    ^
| size = 3   |     prev = NULL                     |          next = NULL
| free_fn    |                                     |
+------------+                          bidirectional links
```

- Both use sentinel-free design (head/tail pointers directly).
- Destructor callback for owned data.
- Iterators via simple `node->next` / `node->prev` traversal.

---

## 3. Implementation

```c
/**
 * @file linked_list.c
 * @brief Generic singly and doubly linked list implementations.
 *
 * Both variants store void* data with optional destructor.
 * Thread-unsafe; caller must synchronize.
 *
 * Standard: C99 / C11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ======================================================================
 * Part A: Singly Linked List
 * ====================================================================== */

typedef struct snode {
    void         *data;
    struct snode *next;
} snode_t;

typedef struct slist {
    snode_t *head;
    snode_t *tail;
    size_t   size;
    void   (*free_fn)(void *);
} slist_t;

/**
 * @brief Create a new singly linked list.
 * @param free_fn Optional destructor for element data.
 * @return New list, or NULL on allocation failure.
 */
static slist_t *
slist_create(void (*free_fn)(void *))
{
    slist_t *sl;

    sl = calloc(1, sizeof(*sl));
    if (sl == NULL)
        return NULL;

    sl->free_fn = free_fn;
    return sl;
}

/**
 * @brief Destroy the list and free all nodes/data.
 */
static void
slist_destroy(slist_t *sl)
{
    snode_t *cur, *tmp;

    if (sl == NULL)
        return;

    cur = sl->head;
    while (cur != NULL) {
        tmp = cur->next;
        if (sl->free_fn != NULL && cur->data != NULL)
            sl->free_fn(cur->data);
        free(cur);
        cur = tmp;
    }

    free(sl);
}

/**
 * @brief Push an element to the front of the list.
 * @return 0 on success, -1 on failure.
 */
static int
slist_push_front(slist_t *sl, void *data)
{
    snode_t *node;

    if (sl == NULL) {
        errno = EINVAL;
        return -1;
    }

    node = malloc(sizeof(*node));
    if (node == NULL)
        return -1;

    node->data = data;
    node->next = sl->head;
    sl->head = node;

    if (sl->tail == NULL)
        sl->tail = node;

    sl->size++;
    return 0;
}

/**
 * @brief Push an element to the back of the list.
 * @return 0 on success, -1 on failure.
 */
static int
slist_push_back(slist_t *sl, void *data)
{
    snode_t *node;

    if (sl == NULL) {
        errno = EINVAL;
        return -1;
    }

    node = malloc(sizeof(*node));
    if (node == NULL)
        return -1;

    node->data = data;
    node->next = NULL;

    if (sl->tail != NULL)
        sl->tail->next = node;
    else
        sl->head = node;

    sl->tail = node;
    sl->size++;

    return 0;
}

/**
 * @brief Pop the front element.
 * @return Data pointer, or NULL if empty (caller takes ownership).
 */
static void *
slist_pop_front(slist_t *sl)
{
    snode_t *node;
    void    *data;

    if (sl == NULL || sl->head == NULL)
        return NULL;

    node = sl->head;
    data = node->data;
    sl->head = node->next;

    if (sl->head == NULL)
        sl->tail = NULL;

    free(node);
    sl->size--;

    return data;
}

/**
 * @brief Search for an element using a comparator.
 * @param key The search key.
 * @param cmp Returns 0 on match.
 * @return Pointer to matching data, or NULL.
 */
static void *
slist_search(const slist_t *sl, const void *key,
             int (*cmp)(const void *, const void *))
{
    snode_t *cur;

    if (sl == NULL || cmp == NULL)
        return NULL;

    for (cur = sl->head; cur != NULL; cur = cur->next) {
        if (cmp(cur->data, key) == 0)
            return cur->data;
    }

    return NULL;
}

/**
 * @brief Remove first element matching key.
 * @param out If non-NULL, receives the removed data (caller takes ownership).
 * @return 0 if removed, -1 if not found.
 */
static int
slist_remove(slist_t *sl, const void *key,
             int (*cmp)(const void *, const void *), void **out)
{
    snode_t *cur, *prev;

    if (sl == NULL || cmp == NULL) {
        errno = EINVAL;
        return -1;
    }

    prev = NULL;
    for (cur = sl->head; cur != NULL; prev = cur, cur = cur->next) {
        if (cmp(cur->data, key) != 0)
            continue;

        if (prev != NULL)
            prev->next = cur->next;
        else
            sl->head = cur->next;

        if (cur == sl->tail)
            sl->tail = prev;

        if (out != NULL) {
            *out = cur->data;
        } else if (sl->free_fn != NULL && cur->data != NULL) {
            sl->free_fn(cur->data);
        }

        free(cur);
        sl->size--;
        return 0;
    }

    return -1;
}

static inline size_t
slist_size(const slist_t *sl)
{
    return sl ? sl->size : 0;
}

/* ======================================================================
 * Part B: Doubly Linked List
 * ====================================================================== */

typedef struct dnode {
    void         *data;
    struct dnode *prev;
    struct dnode *next;
} dnode_t;

typedef struct dlist {
    dnode_t *head;
    dnode_t *tail;
    size_t   size;
    void   (*free_fn)(void *);
} dlist_t;

/**
 * @brief Create a new doubly linked list.
 */
static dlist_t *
dlist_create(void (*free_fn)(void *))
{
    dlist_t *dl;

    dl = calloc(1, sizeof(*dl));
    if (dl == NULL)
        return NULL;

    dl->free_fn = free_fn;
    return dl;
}

/**
 * @brief Destroy the list and free all nodes/data.
 */
static void
dlist_destroy(dlist_t *dl)
{
    dnode_t *cur, *tmp;

    if (dl == NULL)
        return;

    cur = dl->head;
    while (cur != NULL) {
        tmp = cur->next;
        if (dl->free_fn != NULL && cur->data != NULL)
            dl->free_fn(cur->data);
        free(cur);
        cur = tmp;
    }

    free(dl);
}

/**
 * @brief Internal: unlink a node from the list (does not free).
 */
static void
dlist_unlink(dlist_t *dl, dnode_t *node)
{
    if (node->prev != NULL)
        node->prev->next = node->next;
    else
        dl->head = node->next;

    if (node->next != NULL)
        node->next->prev = node->prev;
    else
        dl->tail = node->prev;

    dl->size--;
}

/**
 * @brief Push to front.
 */
static int
dlist_push_front(dlist_t *dl, void *data)
{
    dnode_t *node;

    if (dl == NULL) {
        errno = EINVAL;
        return -1;
    }

    node = malloc(sizeof(*node));
    if (node == NULL)
        return -1;

    node->data = data;
    node->prev = NULL;
    node->next = dl->head;

    if (dl->head != NULL)
        dl->head->prev = node;
    else
        dl->tail = node;

    dl->head = node;
    dl->size++;

    return 0;
}

/**
 * @brief Push to back.
 */
static int
dlist_push_back(dlist_t *dl, void *data)
{
    dnode_t *node;

    if (dl == NULL) {
        errno = EINVAL;
        return -1;
    }

    node = malloc(sizeof(*node));
    if (node == NULL)
        return -1;

    node->data = data;
    node->next = NULL;
    node->prev = dl->tail;

    if (dl->tail != NULL)
        dl->tail->next = node;
    else
        dl->head = node;

    dl->tail = node;
    dl->size++;

    return 0;
}

/**
 * @brief Pop from front.
 */
static void *
dlist_pop_front(dlist_t *dl)
{
    dnode_t *node;
    void    *data;

    if (dl == NULL || dl->head == NULL)
        return NULL;

    node = dl->head;
    data = node->data;
    dlist_unlink(dl, node);
    free(node);

    return data;
}

/**
 * @brief Pop from back.
 */
static void *
dlist_pop_back(dlist_t *dl)
{
    dnode_t *node;
    void    *data;

    if (dl == NULL || dl->tail == NULL)
        return NULL;

    node = dl->tail;
    data = node->data;
    dlist_unlink(dl, node);
    free(node);

    return data;
}

/**
 * @brief Search for an element.
 */
static void *
dlist_search(const dlist_t *dl, const void *key,
             int (*cmp)(const void *, const void *))
{
    dnode_t *cur;

    if (dl == NULL || cmp == NULL)
        return NULL;

    for (cur = dl->head; cur != NULL; cur = cur->next) {
        if (cmp(cur->data, key) == 0)
            return cur->data;
    }

    return NULL;
}

/**
 * @brief Remove first element matching key.
 */
static int
dlist_remove(dlist_t *dl, const void *key,
             int (*cmp)(const void *, const void *), void **out)
{
    dnode_t *cur;

    if (dl == NULL || cmp == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (cur = dl->head; cur != NULL; cur = cur->next) {
        if (cmp(cur->data, key) != 0)
            continue;

        dlist_unlink(dl, cur);

        if (out != NULL) {
            *out = cur->data;
        } else if (dl->free_fn != NULL && cur->data != NULL) {
            dl->free_fn(cur->data);
        }

        free(cur);
        return 0;
    }

    return -1;
}

/**
 * @brief Move a node to the front (useful for LRU caches).
 */
static void
dlist_move_to_front(dlist_t *dl, dnode_t *node)
{
    if (dl == NULL || node == NULL || node == dl->head)
        return;

    dlist_unlink(dl, node);

    node->prev = NULL;
    node->next = dl->head;

    if (dl->head != NULL)
        dl->head->prev = node;
    else
        dl->tail = node;

    dl->head = node;
    dl->size++;
}

static inline size_t
dlist_size(const dlist_t *dl)
{
    return dl ? dl->size : 0;
}

/*
 * === Example / Self-test ===
 */
#ifdef LLIST_TEST
#include <assert.h>

static int
int_cmp(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

int
main(void)
{
    slist_t *sl;
    dlist_t *dl;
    int *v, key;
    int i;

    /* --- Singly Linked --- */
    sl = slist_create(free);
    assert(sl != NULL);

    for (i = 0; i < 10; i++) {
        v = malloc(sizeof(int));
        *v = i;
        assert(slist_push_back(sl, v) == 0);
    }
    assert(slist_size(sl) == 10);

    key = 5;
    v = slist_search(sl, &key, int_cmp);
    assert(v != NULL && *v == 5);

    assert(slist_remove(sl, &key, int_cmp, NULL) == 0);
    assert(slist_size(sl) == 9);

    v = slist_pop_front(sl);
    assert(v != NULL && *v == 0);
    free(v);

    slist_destroy(sl);
    printf("slist: all tests passed\n");

    /* --- Doubly Linked --- */
    dl = dlist_create(free);
    assert(dl != NULL);

    for (i = 0; i < 10; i++) {
        v = malloc(sizeof(int));
        *v = i;
        assert(dlist_push_back(dl, v) == 0);
    }

    v = dlist_pop_back(dl);
    assert(v != NULL && *v == 9);
    free(v);

    v = dlist_pop_front(dl);
    assert(v != NULL && *v == 0);
    free(v);

    key = 5;
    assert(dlist_remove(dl, &key, int_cmp, NULL) == 0);
    assert(dlist_size(dl) == 7);

    dlist_destroy(dl);
    printf("dlist: all tests passed\n");

    return 0;
}
#endif /* LLIST_TEST */
```

Compile and test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -DLLIST_TEST -o test_llist linked_list.c && ./test_llist
```

---

## 4. Memory / ASCII Visualization

### Singly Linked List — Push/Pop

```
slist_push_front(sl, "C"):

  head                         tail
   |                            |
   v                            v
 +---+---+    +---+---+    +---+---+
 | C | *-+--->| A | *-+--->| B | / |
 +---+---+    +---+---+    +---+---+

slist_pop_front(sl):  returns "C", head moves to "A"

  head              tail
   |                 |
   v                 v
 +---+---+    +---+---+
 | A | *-+--->| B | / |
 +---+---+    +---+---+
```

### Doubly Linked List — Bidirectional Links

```
  head                                          tail
   |                                             |
   v                                             v
 +---+---+---+    +---+---+---+    +---+---+---+
 | / | A | *-+--->| * | B | *-+--->| * | C | / |
 +---+---+---+    +-+-+---+---+    +-+-+---+---+
                   |                |
                   +<--- prev ------+

 /  = NULL pointer
 *  = valid pointer
```

### LRU Cache Pattern (Doubly Linked + Hash Map)

```
 Hash Map                Doubly Linked List (MRU <---> LRU)
 +-------+
 | key_A-+--+           head                          tail
 | key_B-+-+|            |                             |
 | key_C-++ |            v                             v
 +-------+| |  +---+---+---+  +---+---+---+  +---+---+---+
          | +->| / | A | *-+->| * | B | *-+->| * | C | / |
          |    +---+---+---+  +---+---+---+  +---+---+---+
          |                   ^
          +-------------------+

 On access(B): dlist_move_to_front(dl, node_B)
 On evict:     dlist_pop_back(dl) -> removes C
```
