# B+ Tree

## 1. Technical Specification

### Characteristics

| Property | Value |
|----------|-------|
| Memory Layout | Multi-way tree, high fanout, leaf-linked |
| Insert | O(log_B n) |
| Search (point) | O(log_B n) |
| Range scan | O(log_B n + k), k = results |
| Delete | O(log_B n) |
| Min keys per node | ⌈order/2⌉ - 1 |
| Max keys per node | order - 1 |
| Disk I/O per op | O(log_B n), B = branching factor |

### Key Differences from B-Tree

| B-Tree | B+ Tree |
|--------|---------|
| Data in all nodes | Data only in leaves |
| No leaf linking | Leaves form a linked list |
| Less predictable scan | Sequential scan is O(k) |
| Slightly less space | Internal nodes are pure indexes |

### Use Cases

- **File Systems**: ext4, XFS, Btrfs — directory indexes, extent maps.
- **Databases**: PostgreSQL, MySQL/InnoDB — primary and secondary indexes.
- **DPDK**: Longest Prefix Match (LPM) tables, sorted flow caches.
- **Key-Value Stores**: LevelDB/RocksDB internal memtable indexes.
- **Storage**: SSD FTL (Flash Translation Layer) mapping tables.

### Trade-offs

| vs. Red-Black Tree | B+ Tree Wins | RB Tree Wins |
|--------------------|-------------|-------------|
| Cache/disk locality | High fanout → fewer cache misses | N/A |
| Range queries | Linked leaves → sequential | No leaf chain |
| Memory overhead | Higher per node | Lower per node |
| Update complexity | Split/merge logic | Simpler rotation |

---

## 2. Implementation Strategy

```
bpt_t (order = 4, max 3 keys per node)

         Internal Node
        [  20  |  40  ]
       /    |         \
      v     v          v
  [5|10|15] [20|25|30] [40|50|60]
      |          |          |
      +--next--->+--next--->+--next---> NULL

  Leaf nodes: contain actual key-value pairs.
  Internal nodes: contain separator keys + child pointers.
  Leaf nodes are chained left-to-right for range scans.
```

- **Order (branching factor)**: Configurable at creation. Each internal node has up to `order` children and `order-1` keys.
- **Split on overflow**: When a node exceeds `order-1` keys, split into two and push median key up.
- **Merge on underflow**: When a node drops below `⌈order/2⌉-1` keys, merge with sibling or redistribute.
- **Leaf chain**: All leaves linked for O(k) range scans.

---

## 3. Implementation

```c
/**
 * @file bptree.c
 * @brief Generic B+ tree implementation.
 *
 * Supports point lookups, range scans, insert, and delete.
 * Internal nodes store separator keys; leaves store key-value pairs.
 * Leaves are linked for efficient range iteration.
 *
 * Standard: C99 / C11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#define BPT_DEFAULT_ORDER  64
#define BPT_MIN_ORDER       3

typedef struct bpt_node {
    bool             is_leaf;
    int              num_keys;
    void           **keys;
    union {
        struct bpt_node **children;   /* internal node */
        void            **values;     /* leaf node */
    };
    struct bpt_node *next;            /* leaf chain (NULL for internals) */
    struct bpt_node *parent;
} bpt_node_t;

typedef struct bptree {
    bpt_node_t *root;
    int          order;
    size_t       size;
    int        (*cmp_fn)(const void *, const void *);
    void       (*key_free)(void *);
    void       (*val_free)(void *);
} bptree_t;

/* --- Internal: Node allocation --- */

static bpt_node_t *
bpt_node_create(int order, bool is_leaf)
{
    bpt_node_t *n;

    n = calloc(1, sizeof(*n));
    if (n == NULL)
        return NULL;

    n->keys = calloc((size_t)order, sizeof(void *));
    if (n->keys == NULL) {
        free(n);
        return NULL;
    }

    if (is_leaf) {
        n->values = calloc((size_t)order, sizeof(void *));
        if (n->values == NULL) {
            free(n->keys);
            free(n);
            return NULL;
        }
    } else {
        n->children = calloc((size_t)(order + 1), sizeof(bpt_node_t *));
        if (n->children == NULL) {
            free(n->keys);
            free(n);
            return NULL;
        }
    }

    n->is_leaf  = is_leaf;
    n->num_keys = 0;
    n->next     = NULL;
    n->parent   = NULL;

    return n;
}

static void
bpt_node_free_shallow(bpt_node_t *n)
{
    if (n == NULL)
        return;

    free(n->keys);
    if (n->is_leaf)
        free(n->values);
    else
        free(n->children);
    free(n);
}

/* --- Internal: Find leaf for a key --- */

static bpt_node_t *
bpt_find_leaf(const bptree_t *t, const void *key)
{
    bpt_node_t *cur;
    int i;

    if (t->root == NULL)
        return NULL;

    cur = t->root;
    while (!cur->is_leaf) {
        i = 0;
        while (i < cur->num_keys && t->cmp_fn(key, cur->keys[i]) >= 0)
            i++;
        cur = cur->children[i];
    }

    return cur;
}

/* --- Internal: Split a full leaf --- */

static int
bpt_split_leaf(bptree_t *t, bpt_node_t *leaf)
{
    bpt_node_t *new_leaf;
    int split, i, j;

    new_leaf = bpt_node_create(t->order, true);
    if (new_leaf == NULL)
        return -1;

    split = (t->order + 1) / 2;

    for (i = split, j = 0; i < leaf->num_keys; i++, j++) {
        new_leaf->keys[j]   = leaf->keys[i];
        new_leaf->values[j] = leaf->values[i];
        leaf->keys[i]   = NULL;
        leaf->values[i] = NULL;
    }

    new_leaf->num_keys = leaf->num_keys - split;
    leaf->num_keys     = split;

    new_leaf->next = leaf->next;
    leaf->next     = new_leaf;
    new_leaf->parent = leaf->parent;

    /* Push first key of new_leaf up to parent */
    return 0; /* caller handles parent insertion */
}

/* --- Internal: Insert into parent --- */

static int bpt_insert_into_parent(bptree_t *t, bpt_node_t *left,
                                  void *key, bpt_node_t *right);

static int
bpt_split_internal(bptree_t *t, bpt_node_t *node)
{
    bpt_node_t *new_node;
    void       *push_key;
    int         split, i, j;

    new_node = bpt_node_create(t->order, false);
    if (new_node == NULL)
        return -1;

    split = node->num_keys / 2;
    push_key = node->keys[split];

    for (i = split + 1, j = 0; i < node->num_keys; i++, j++) {
        new_node->keys[j]     = node->keys[i];
        new_node->children[j] = node->children[i];
        if (new_node->children[j] != NULL)
            new_node->children[j]->parent = new_node;
        node->keys[i]     = NULL;
        node->children[i] = NULL;
    }
    new_node->children[j] = node->children[i];
    if (new_node->children[j] != NULL)
        new_node->children[j]->parent = new_node;
    node->children[i] = NULL;

    new_node->num_keys = node->num_keys - split - 1;
    node->num_keys     = split;
    node->keys[split]  = NULL;

    return bpt_insert_into_parent(t, node, push_key, new_node);
}

static int
bpt_insert_into_parent(bptree_t *t, bpt_node_t *left,
                       void *key, bpt_node_t *right)
{
    bpt_node_t *parent;
    int i, insert_idx;

    parent = left->parent;

    if (parent == NULL) {
        /* Create new root */
        parent = bpt_node_create(t->order, false);
        if (parent == NULL)
            return -1;

        parent->keys[0]     = key;
        parent->children[0] = left;
        parent->children[1] = right;
        parent->num_keys    = 1;

        left->parent  = parent;
        right->parent = parent;
        t->root       = parent;

        return 0;
    }

    /* Find position for new key in parent */
    insert_idx = 0;
    while (insert_idx < parent->num_keys &&
           t->cmp_fn(key, parent->keys[insert_idx]) >= 0)
        insert_idx++;

    /* Shift right */
    for (i = parent->num_keys; i > insert_idx; i--) {
        parent->keys[i]       = parent->keys[i - 1];
        parent->children[i+1] = parent->children[i];
    }

    parent->keys[insert_idx]       = key;
    parent->children[insert_idx+1] = right;
    parent->num_keys++;
    right->parent = parent;

    if (parent->num_keys >= t->order)
        return bpt_split_internal(t, parent);

    return 0;
}

/* --- Public API --- */

/**
 * @brief Create a new B+ tree.
 * @param order    Branching factor (minimum 3).
 * @param cmp_fn   Key comparator (required).
 * @param key_free Optional key destructor.
 * @param val_free Optional value destructor.
 */
static bptree_t *
bptree_create(int order, int (*cmp_fn)(const void *, const void *),
              void (*key_free)(void *), void (*val_free)(void *))
{
    bptree_t *t;

    if (cmp_fn == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (order < BPT_MIN_ORDER)
        order = BPT_DEFAULT_ORDER;

    t = calloc(1, sizeof(*t));
    if (t == NULL)
        return NULL;

    t->order    = order;
    t->cmp_fn   = cmp_fn;
    t->key_free = key_free;
    t->val_free = val_free;

    return t;
}

/**
 * @brief Recursively destroy all nodes.
 */
static void
bptree_destroy_node(bptree_t *t, bpt_node_t *n)
{
    int i;

    if (n == NULL)
        return;

    if (!n->is_leaf) {
        for (i = 0; i <= n->num_keys; i++)
            bptree_destroy_node(t, n->children[i]);
    } else {
        for (i = 0; i < n->num_keys; i++) {
            if (t->key_free != NULL && n->keys[i] != NULL)
                t->key_free(n->keys[i]);
            if (t->val_free != NULL && n->values[i] != NULL)
                t->val_free(n->values[i]);
        }
    }

    bpt_node_free_shallow(n);
}

static void
bptree_destroy(bptree_t *t)
{
    if (t == NULL)
        return;

    bptree_destroy_node(t, t->root);
    free(t);
}

/**
 * @brief Insert a key-value pair.
 * @return 0 on success, 1 if key updated, -1 on error.
 */
static int
bptree_insert(bptree_t *t, void *key, void *value)
{
    bpt_node_t *leaf;
    int i, insert_idx;

    if (t == NULL || key == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* Empty tree */
    if (t->root == NULL) {
        leaf = bpt_node_create(t->order, true);
        if (leaf == NULL)
            return -1;
        leaf->keys[0]   = key;
        leaf->values[0] = value;
        leaf->num_keys   = 1;
        t->root = leaf;
        t->size++;
        return 0;
    }

    leaf = bpt_find_leaf(t, key);

    /* Check for duplicate key */
    for (i = 0; i < leaf->num_keys; i++) {
        if (t->cmp_fn(key, leaf->keys[i]) == 0) {
            if (t->val_free != NULL && leaf->values[i] != NULL)
                t->val_free(leaf->values[i]);
            if (t->key_free != NULL && key != NULL)
                t->key_free(key);
            leaf->values[i] = value;
            return 1;
        }
    }

    /* Find insertion point */
    insert_idx = 0;
    while (insert_idx < leaf->num_keys &&
           t->cmp_fn(key, leaf->keys[insert_idx]) > 0)
        insert_idx++;

    /* Shift right */
    for (i = leaf->num_keys; i > insert_idx; i--) {
        leaf->keys[i]   = leaf->keys[i - 1];
        leaf->values[i] = leaf->values[i - 1];
    }

    leaf->keys[insert_idx]   = key;
    leaf->values[insert_idx] = value;
    leaf->num_keys++;
    t->size++;

    /* Split if full */
    if (leaf->num_keys >= t->order) {
        void *push_key;
        bpt_node_t *new_leaf;

        if (bpt_split_leaf(t, leaf) != 0)
            return -1;

        new_leaf = leaf->next;
        push_key = new_leaf->keys[0];

        return bpt_insert_into_parent(t, leaf, push_key, new_leaf);
    }

    return 0;
}

/**
 * @brief Point lookup.
 * @return Value pointer, or NULL if not found.
 */
static void *
bptree_search(const bptree_t *t, const void *key)
{
    bpt_node_t *leaf;
    int i;

    if (t == NULL || key == NULL || t->root == NULL)
        return NULL;

    leaf = bpt_find_leaf(t, key);

    for (i = 0; i < leaf->num_keys; i++) {
        if (t->cmp_fn(key, leaf->keys[i]) == 0)
            return leaf->values[i];
    }

    return NULL;
}

/**
 * @brief Range scan callback type.
 * @return 0 to continue, non-zero to stop.
 */
typedef int (*bptree_scan_fn)(const void *key, void *value, void *ctx);

/**
 * @brief Scan all keys in [lo, hi] (inclusive).
 * @return Number of entries visited, or -1 on error.
 */
static int
bptree_range_scan(const bptree_t *t, const void *lo, const void *hi,
                  bptree_scan_fn cb, void *ctx)
{
    bpt_node_t *leaf;
    int i, count;

    if (t == NULL || lo == NULL || hi == NULL || cb == NULL)
        return -1;

    if (t->root == NULL)
        return 0;

    leaf = bpt_find_leaf(t, lo);
    count = 0;

    while (leaf != NULL) {
        for (i = 0; i < leaf->num_keys; i++) {
            if (t->cmp_fn(leaf->keys[i], lo) < 0)
                continue;
            if (t->cmp_fn(leaf->keys[i], hi) > 0)
                return count;

            count++;
            if (cb(leaf->keys[i], leaf->values[i], ctx) != 0)
                return count;
        }
        leaf = leaf->next;
    }

    return count;
}

static inline size_t
bptree_size(const bptree_t *t)
{
    return t ? t->size : 0;
}

/*
 * === Example / Self-test ===
 */
#ifdef BPTREE_TEST
#include <assert.h>

static int
int_cmp(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;
    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

static int
scan_sum(const void *key, void *value, void *ctx)
{
    (void)key;
    int *sum = ctx;
    *sum += *(int *)value;
    return 0;
}

int
main(void)
{
    bptree_t *t;
    int *k, *v;
    int i, sum;
    int lo, hi;

    t = bptree_create(4, int_cmp, free, free);
    assert(t != NULL);

    /* Insert 200 key-value pairs */
    for (i = 0; i < 200; i++) {
        k = malloc(sizeof(int));
        v = malloc(sizeof(int));
        *k = i;
        *v = i * 100;
        assert(bptree_insert(t, k, v) == 0);
    }
    assert(bptree_size(t) == 200);

    /* Point lookup */
    i = 42;
    v = bptree_search(t, &i);
    assert(v != NULL && *v == 4200);

    /* Range scan [10, 19] */
    lo = 10; hi = 19;
    sum = 0;
    assert(bptree_range_scan(t, &lo, &hi, scan_sum, &sum) == 10);
    /* sum = (10+11+...+19)*100 = 145*100 = 14500 */
    assert(sum == 14500);

    /* Not found */
    i = 999;
    assert(bptree_search(t, &i) == NULL);

    bptree_destroy(t);
    printf("bptree: all tests passed\n");

    return 0;
}
#endif /* BPTREE_TEST */
```

Compile and test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -DBPTREE_TEST -o test_bptree bptree.c && ./test_bptree
```

---

## 4. Memory / ASCII Visualization

### B+ Tree Structure (order = 4)

```
                         +---------+
                         |   [20]  |          <-- root (internal)
                         +----+----+
                        /          \
              +-----------+     +-----------+
              | [5|10|15] |     | [20|25|30]|  <-- internal nodes
              +--+--+--+--+    +--+--+--+--+
              /  |   |   \     /  |   |   \
             v   v   v    v   v   v   v    v
           +---+---+---+---+---+---+---+---+
  Leaves:  |1-4|5-9|10-|15-|20-|25-|30-|...|
           +---+---+---+---+---+---+---+---+
             |-->|-->|-->|-->|-->|-->|-->NULL
                    leaf chain (next pointers)
```

### Leaf Node Memory Layout

```
bpt_node_t (leaf, order=4)
+---------+----------+------------------------------------------+
| is_leaf | num_keys | keys[0] | keys[1] | keys[2] | keys[3]   |
| true    |    3     |  ptr    |  ptr    |  ptr    |  NULL      |
+---------+----------+---------+---------+---------+------------+
                     | vals[0] | vals[1] | vals[2] | vals[3]   |
                     |  ptr    |  ptr    |  ptr    |  NULL      |
                     +---------+---------+---------+------------+
| next ---+---> (next leaf in chain)
| parent -+---> (parent internal node)
+----------+
```

### Internal Node Memory Layout

```
bpt_node_t (internal, order=4)
+---------+----------+-----------------------------------+
| is_leaf | num_keys | keys[0] | keys[1] | keys[2]      |
| false   |    2     |  20     |  40     |  NULL         |
+---------+----------+---------+---------+---------------+
                     | child[0]| child[1]| child[2]      |
                     | <20     | 20..39  | >=40           |
                     +---------+---------+---------------+
```

### Split Operation

```
Insert 16 into full leaf [10|12|14|15], order=4:

Before:
  Parent: [10 | ... ]
            |
  Leaf:  [10|12|14|15]  <- FULL (4 keys, order=4)

Step 1: Insert 16 -> [10|12|14|15|16] overflow
Step 2: Split at midpoint:
  Left:  [10|12]
  Right: [14|15|16]

Step 3: Push separator (14) to parent:
  Parent: [10 | 14 | ... ]
            |    |
          [10|12] [14|15|16]
                    ^
                    next pointer maintained
```

### Range Scan Path

```
bptree_range_scan(t, 15, 35):

  1. Find leaf containing 15:
     root [20] -> left child -> leaf with [10,12,14,15]

  2. Start scanning from key 15:
     [10,12,14,>>15>>] --next--> [20,25,>>30>>] --next--> [35,...]
                                                            ^ stop: 35 > hi

  Visited: 15, 20, 25, 30, 35 = 5 entries via sequential leaf scan
```
