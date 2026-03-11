# Red-Black Tree

## 1. Technical Specification

### Characteristics

| Property | Value |
|----------|-------|
| Memory Layout | Node-based, pointer-linked binary tree |
| Insert | O(log n) |
| Search | O(log n) |
| Delete | O(log n) |
| In-order traversal | O(n) |
| Height guarantee | ≤ 2·log₂(n+1) |
| Memory per node | data + left + right + parent + color (1 bit) |

### Red-Black Properties (Invariants)

1. Every node is either **red** or **black**.
2. The root is **black**.
3. Every leaf (NIL sentinel) is **black**.
4. If a node is **red**, both children are **black** (no two consecutive reds).
5. For each node, all simple paths to descendant leaves have the **same black-height**.

### Use Cases

- **Linux Kernel**: `struct rb_root` / `rb_node` — the primary balanced tree. Used in:
  - CFS scheduler (task timeline)
  - Memory management (VMA interval tree)
  - `epoll` file descriptor management
- **DPDK**: Timer management, sorted flow tables.
- **Databases**: In-memory indexes, B-tree node internal indexes.
- **Networking**: Routing table lookups, interval-based ACLs.
- **C++ STL**: `std::map`, `std::set` are typically red-black trees.

### Trade-offs

| vs. AVL Tree | Red-Black Wins | AVL Wins |
|-------------|---------------|---------|
| Insert/Delete | Fewer rotations (≤2 insert, ≤3 delete) | N/A |
| Search | Slightly taller | Stricter balance → faster |
| Implementation | Simpler delete rebalancing | N/A |
| Kernel usage | Industry standard | Less common |

---

## 2. Implementation Strategy

```
rbtree_t
+--------------+
| root --------+--->  [P=NULL | BLACK | key | left | right]
| nil (sentinel)|       /                         \
| size          |   [RED | key]                [RED | key]
| cmp_fn        |    /      \                   /      \
| key_free      |  [nil]  [nil]             [nil]   [nil]
| val_free      |
+--------------+

All NULL children point to a shared NIL sentinel (black).
Parent pointers enable O(1) rotation and successor finding.
```

- Sentinel NIL node eliminates NULL checks throughout the code.
- Comparator function pointer for generic key ordering.
- Separate key/value destructors for owned data.
- Left-leaning variant not used; standard Cormen (CLRS) algorithm.

---

## 3. Implementation

```c
/**
 * @file rbtree.c
 * @brief Generic red-black tree implementation (CLRS algorithm).
 *
 * Balanced BST with O(log n) insert/search/delete.
 * Uses void* for generic keys and values.
 * Sentinel NIL node eliminates NULL-check complexity.
 *
 * Standard: C99 / C11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef enum { RB_BLACK = 0, RB_RED = 1 } rb_color_t;

typedef struct rb_node {
    void           *key;
    void           *value;
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
    rb_color_t      color;
} rb_node_t;

typedef struct rbtree {
    rb_node_t *root;
    rb_node_t *nil;       /* shared sentinel */
    size_t     size;
    int      (*cmp_fn)(const void *, const void *);
    void     (*key_free)(void *);
    void     (*val_free)(void *);
} rbtree_t;

/* --- Internal helpers --- */

static rb_node_t *
rb_node_create(rbtree_t *t, void *key, void *value)
{
    rb_node_t *n;

    n = malloc(sizeof(*n));
    if (n == NULL)
        return NULL;

    n->key    = key;
    n->value  = value;
    n->parent = t->nil;
    n->left   = t->nil;
    n->right  = t->nil;
    n->color  = RB_RED;

    return n;
}

static void
rb_node_free(rbtree_t *t, rb_node_t *n)
{
    if (n == NULL || n == t->nil)
        return;

    if (t->key_free != NULL && n->key != NULL)
        t->key_free(n->key);
    if (t->val_free != NULL && n->value != NULL)
        t->val_free(n->value);
    free(n);
}

/**
 * @brief Left rotation around node x.
 *
 *     x              y
 *    / \            / \
 *   a   y    =>   x   c
 *      / \       / \
 *     b   c     a   b
 */
static void
rb_rotate_left(rbtree_t *t, rb_node_t *x)
{
    rb_node_t *y;

    y = x->right;
    x->right = y->left;

    if (y->left != t->nil)
        y->left->parent = x;

    y->parent = x->parent;

    if (x->parent == t->nil)
        t->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;

    y->left   = x;
    x->parent = y;
}

/**
 * @brief Right rotation around node y.
 *
 *       y            x
 *      / \          / \
 *     x   c  =>   a   y
 *    / \              / \
 *   a   b            b   c
 */
static void
rb_rotate_right(rbtree_t *t, rb_node_t *y)
{
    rb_node_t *x;

    x = y->left;
    y->left = x->right;

    if (x->right != t->nil)
        x->right->parent = y;

    x->parent = y->parent;

    if (y->parent == t->nil)
        t->root = x;
    else if (y == y->parent->left)
        y->parent->left = x;
    else
        y->parent->right = x;

    x->right  = y;
    y->parent = x;
}

/**
 * @brief Fix red-black properties after insertion.
 */
static void
rb_insert_fixup(rbtree_t *t, rb_node_t *z)
{
    rb_node_t *y;

    while (z->parent->color == RB_RED) {
        if (z->parent == z->parent->parent->left) {
            y = z->parent->parent->right;
            if (y->color == RB_RED) {
                /* Case 1: uncle is red */
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    /* Case 2: uncle is black, z is right child */
                    z = z->parent;
                    rb_rotate_left(t, z);
                }
                /* Case 3: uncle is black, z is left child */
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_rotate_right(t, z->parent->parent);
            }
        } else {
            /* Mirror: parent is right child */
            y = z->parent->parent->left;
            if (y->color == RB_RED) {
                z->parent->color = RB_BLACK;
                y->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rb_rotate_right(t, z);
                }
                z->parent->color = RB_BLACK;
                z->parent->parent->color = RB_RED;
                rb_rotate_left(t, z->parent->parent);
            }
        }
    }

    t->root->color = RB_BLACK;
}

/**
 * @brief Transplant subtree u with subtree v.
 */
static void
rb_transplant(rbtree_t *t, rb_node_t *u, rb_node_t *v)
{
    if (u->parent == t->nil)
        t->root = v;
    else if (u == u->parent->left)
        u->parent->left = v;
    else
        u->parent->right = v;

    v->parent = u->parent;
}

static rb_node_t *
rb_minimum(const rbtree_t *t, rb_node_t *n)
{
    while (n->left != t->nil)
        n = n->left;
    return n;
}

/**
 * @brief Fix red-black properties after deletion.
 */
static void
rb_delete_fixup(rbtree_t *t, rb_node_t *x)
{
    rb_node_t *w;

    while (x != t->root && x->color == RB_BLACK) {
        if (x == x->parent->left) {
            w = x->parent->right;
            if (w->color == RB_RED) {
                /* Case 1 */
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rb_rotate_left(t, x->parent);
                w = x->parent->right;
            }
            if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
                /* Case 2 */
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->right->color == RB_BLACK) {
                    /* Case 3 */
                    w->left->color = RB_BLACK;
                    w->color = RB_RED;
                    rb_rotate_right(t, w);
                    w = x->parent->right;
                }
                /* Case 4 */
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->right->color = RB_BLACK;
                rb_rotate_left(t, x->parent);
                x = t->root;
            }
        } else {
            /* Mirror */
            w = x->parent->left;
            if (w->color == RB_RED) {
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rb_rotate_right(t, x->parent);
                w = x->parent->left;
            }
            if (w->right->color == RB_BLACK && w->left->color == RB_BLACK) {
                w->color = RB_RED;
                x = x->parent;
            } else {
                if (w->left->color == RB_BLACK) {
                    w->right->color = RB_BLACK;
                    w->color = RB_RED;
                    rb_rotate_left(t, w);
                    w = x->parent->left;
                }
                w->color = x->parent->color;
                x->parent->color = RB_BLACK;
                w->left->color = RB_BLACK;
                rb_rotate_right(t, x->parent);
                x = t->root;
            }
        }
    }

    x->color = RB_BLACK;
}

/* --- Public API --- */

/**
 * @brief Create a new red-black tree.
 * @param cmp_fn    Key comparator: <0 if a<b, 0 if a==b, >0 if a>b (required).
 * @param key_free  Optional key destructor.
 * @param val_free  Optional value destructor.
 */
static rbtree_t *
rbtree_create(int (*cmp_fn)(const void *, const void *),
              void (*key_free)(void *),
              void (*val_free)(void *))
{
    rbtree_t *t;

    if (cmp_fn == NULL) {
        errno = EINVAL;
        return NULL;
    }

    t = calloc(1, sizeof(*t));
    if (t == NULL)
        return NULL;

    t->nil = calloc(1, sizeof(rb_node_t));
    if (t->nil == NULL) {
        free(t);
        return NULL;
    }
    t->nil->color  = RB_BLACK;
    t->nil->parent = t->nil;
    t->nil->left   = t->nil;
    t->nil->right  = t->nil;

    t->root     = t->nil;
    t->cmp_fn   = cmp_fn;
    t->key_free = key_free;
    t->val_free = val_free;

    return t;
}

/**
 * @brief Recursively free all nodes.
 */
static void
rbtree_destroy_subtree(rbtree_t *t, rb_node_t *n)
{
    if (n == t->nil)
        return;

    rbtree_destroy_subtree(t, n->left);
    rbtree_destroy_subtree(t, n->right);
    rb_node_free(t, n);
}

static void
rbtree_destroy(rbtree_t *t)
{
    if (t == NULL)
        return;

    rbtree_destroy_subtree(t, t->root);
    free(t->nil);
    free(t);
}

/**
 * @brief Insert a key-value pair.
 * @return 0 on success, -1 on failure, 1 if key already exists (value updated).
 */
static int
rbtree_insert(rbtree_t *t, void *key, void *value)
{
    rb_node_t *z, *y, *x;
    int rc;

    if (t == NULL || key == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* BST walk to find insertion point */
    y = t->nil;
    x = t->root;

    while (x != t->nil) {
        y = x;
        rc = t->cmp_fn(key, x->key);
        if (rc == 0) {
            /* Key exists — update value */
            if (t->val_free != NULL && x->value != NULL)
                t->val_free(x->value);
            if (t->key_free != NULL && key != NULL)
                t->key_free(key);
            x->value = value;
            return 1;
        }
        x = (rc < 0) ? x->left : x->right;
    }

    z = rb_node_create(t, key, value);
    if (z == NULL)
        return -1;

    z->parent = y;

    if (y == t->nil)
        t->root = z;
    else if (t->cmp_fn(z->key, y->key) < 0)
        y->left = z;
    else
        y->right = z;

    rb_insert_fixup(t, z);
    t->size++;

    return 0;
}

/**
 * @brief Search for a key.
 * @return Pointer to value, or NULL if not found.
 */
static void *
rbtree_search(const rbtree_t *t, const void *key)
{
    rb_node_t *x;
    int rc;

    if (t == NULL || key == NULL)
        return NULL;

    x = t->root;
    while (x != t->nil) {
        rc = t->cmp_fn(key, x->key);
        if (rc == 0)
            return x->value;
        x = (rc < 0) ? x->left : x->right;
    }

    return NULL;
}

/**
 * @brief Remove a key from the tree.
 * @param out_val If non-NULL, receives value (caller owns it).
 * @return 0 on success, -1 if not found.
 */
static int
rbtree_remove(rbtree_t *t, const void *key, void **out_val)
{
    rb_node_t  *z, *y, *x;
    rb_color_t  y_orig_color;
    int         rc;

    if (t == NULL || key == NULL) {
        errno = EINVAL;
        return -1;
    }

    /* Find node */
    z = t->root;
    while (z != t->nil) {
        rc = t->cmp_fn(key, z->key);
        if (rc == 0)
            break;
        z = (rc < 0) ? z->left : z->right;
    }

    if (z == t->nil)
        return -1;

    /* Save value before freeing key */
    if (out_val != NULL)
        *out_val = z->value;
    else if (t->val_free != NULL && z->value != NULL)
        t->val_free(z->value);

    if (t->key_free != NULL && z->key != NULL)
        t->key_free(z->key);

    y = z;
    y_orig_color = y->color;

    if (z->left == t->nil) {
        x = z->right;
        rb_transplant(t, z, z->right);
    } else if (z->right == t->nil) {
        x = z->left;
        rb_transplant(t, z, z->left);
    } else {
        y = rb_minimum(t, z->right);
        y_orig_color = y->color;
        x = y->right;

        if (y->parent == z) {
            x->parent = y;
        } else {
            rb_transplant(t, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        rb_transplant(t, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    free(z);
    t->size--;

    if (y_orig_color == RB_BLACK)
        rb_delete_fixup(t, x);

    return 0;
}

/**
 * @brief In-order traversal with callback.
 */
static void
rbtree_inorder_walk(const rbtree_t *t, rb_node_t *n,
                    void (*cb)(const void *key, void *value, void *ctx),
                    void *ctx)
{
    if (n == t->nil)
        return;

    rbtree_inorder_walk(t, n->left, cb, ctx);
    cb(n->key, n->value, ctx);
    rbtree_inorder_walk(t, n->right, cb, ctx);
}

static void
rbtree_foreach(const rbtree_t *t,
               void (*cb)(const void *key, void *value, void *ctx),
               void *ctx)
{
    if (t == NULL || cb == NULL)
        return;
    rbtree_inorder_walk(t, t->root, cb, ctx);
}

static inline size_t
rbtree_size(const rbtree_t *t)
{
    return t ? t->size : 0;
}

/*
 * === Example / Self-test ===
 */
#ifdef RBTREE_TEST
#include <assert.h>

static int
int_ptr_cmp(const void *a, const void *b)
{
    int va = *(const int *)a;
    int vb = *(const int *)b;

    if (va < vb) return -1;
    if (va > vb) return  1;
    return 0;
}

static void
print_cb(const void *key, void *value, void *ctx)
{
    (void)value;
    int *count = ctx;
    (*count)++;
    /* keys should appear in sorted order */
}

int
main(void)
{
    rbtree_t *t;
    int *k, *v;
    int i, key, count;

    t = rbtree_create(int_ptr_cmp, free, free);
    assert(t != NULL);

    /* Insert 500 elements */
    for (i = 0; i < 500; i++) {
        k = malloc(sizeof(int));
        v = malloc(sizeof(int));
        *k = i;
        *v = i * 10;
        assert(rbtree_insert(t, k, v) == 0);
    }
    assert(rbtree_size(t) == 500);

    /* Search */
    key = 42;
    v = rbtree_search(t, &key);
    assert(v != NULL && *v == 420);

    /* Remove */
    key = 0;
    assert(rbtree_remove(t, &key, NULL) == 0);
    assert(rbtree_size(t) == 499);
    assert(rbtree_search(t, &key) == NULL);

    /* In-order traversal */
    count = 0;
    rbtree_foreach(t, print_cb, &count);
    assert(count == 499);

    /* Delete half the tree */
    for (i = 100; i < 300; i++) {
        key = i;
        assert(rbtree_remove(t, &key, NULL) == 0);
    }
    assert(rbtree_size(t) == 299);

    rbtree_destroy(t);
    printf("rbtree: all tests passed\n");

    return 0;
}
#endif /* RBTREE_TEST */
```

Compile and test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -DRBTREE_TEST -o test_rbtree rbtree.c && ./test_rbtree
```

---

## 4. Memory / ASCII Visualization

### Red-Black Tree Structure

```
          [11:B]                  B = Black, R = Red
         /      \
      [2:R]     [14:B]
      /   \         \
   [1:B] [7:B]    [15:R]
          / \
       [5:R] [8:R]

NIL sentinel (shared, black) is implicit at every leaf.
```

### Left Rotation

```
  rb_rotate_left(t, x):

       x                 y
      / \               / \
     a   y      =>     x   c
        / \           / \
       b   c         a   b

  1. x->right = y->left (b)
  2. y->parent = x->parent
  3. y->left = x
  4. x->parent = y
```

### Insert Fixup Cases

```
Case 1: Uncle is RED         Case 3: Uncle is BLACK, z is left child
  Recolor only                 Single rotation + recolor

      G(B)                       G(R)
     / \                        / \
   P(R) U(R)    =>           P(B) U(B)         Rotate right at G
   /                          /
  Z(R)                      Z(R)

  Then move z = G, continue up.       =>     P(B)
                                            / \
                                          Z(R) G(R)
                                                \
                                               U(B)
```

### Memory Layout of rb_node_t

```
rb_node_t (typical 64-bit system, 40 bytes)
+--------+--------+--------+--------+--------+-------+---+
| key    | value  | parent | left   | right  | color |pad|
| 8 bytes| 8 bytes| 8 bytes| 8 bytes| 8 bytes| 4 B   | 4 |
+--------+--------+--------+--------+--------+-------+---+
  0x00     0x08     0x10     0x18     0x20     0x28   0x2C

NIL sentinel: single shared instance, all fields point to self.
```
