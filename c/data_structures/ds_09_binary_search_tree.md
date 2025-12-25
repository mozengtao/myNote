# Binary Search Tree (BST) in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE BST: ORDERED SEARCH IN O(log n)                             |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Need fast search, insert, delete with ordering:            │
    │  - Dictionary lookup                                        │
    │  - Range queries (find all keys between A and B)            │
    │  - Predecessor/successor finding                            │
    │  - Dynamic sorted data                                      │
    └─────────────────────────────────────────────────────────────┘

    COMPARISON WITH OTHER STRUCTURES:
    ┌──────────────────┬─────────────────┬────────────────────────┐
    │ Operation        │ Sorted Array    │ BST (balanced)         │
    ├──────────────────┼─────────────────┼────────────────────────┤
    │ Search           │ O(log n)        │ O(log n)               │
    │ Insert           │ O(n) - shift!   │ O(log n)               │
    │ Delete           │ O(n) - shift!   │ O(log n)               │
    │ Min/Max          │ O(1)            │ O(log n)               │
    │ Range query      │ O(log n + k)    │ O(log n + k)           │
    └──────────────────┴─────────────────┴────────────────────────┘

    BST KEY PROPERTY:
    For every node X:
    - All keys in LEFT subtree < X.key
    - All keys in RIGHT subtree > X.key

    VISUAL:
                  ┌────┐
                  │  8 │
                  └────┘
                 /      \
            ┌────┐      ┌────┐
            │  3 │      │ 10 │
            └────┘      └────┘
           /      \          \
       ┌────┐  ┌────┐      ┌────┐
       │  1 │  │  6 │      │ 14 │
       └────┘  └────┘      └────┘
              /     \      /
           ┌────┐ ┌────┐ ┌────┐
           │  4 │ │  7 │ │ 13 │
           └────┘ └────┘ └────┘

    In-order traversal gives: 1, 3, 4, 6, 7, 8, 10, 13, 14 (sorted!)
```

**中文解释：**
- **二叉搜索树**：每个节点的左子树值都小于节点值，右子树值都大于节点值
- 解决问题：快速搜索、插入、删除，同时保持有序性
- 中序遍历自动得到排序结果

### BST Property (Invariant)

```
+------------------------------------------------------------------+
|  BST INVARIANT (MUST ALWAYS HOLD)                                |
+------------------------------------------------------------------+

    FOR EVERY NODE X:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. All keys in X's left subtree  < X.key                   │
    │  2. All keys in X's right subtree > X.key                   │
    │  3. Left and right subtrees are also BSTs                   │
    └─────────────────────────────────────────────────────────────┘

    DUPLICATE HANDLING:
    ┌─────────────────────────────────────────────────────────────┐
    │  Option A: Disallow duplicates                              │
    │  Option B: Go left (key ≤ X.key)                           │
    │  Option C: Go right (key ≥ X.key)                          │
    │  Option D: Store count in node                              │
    └─────────────────────────────────────────────────────────────┘
```

### Degenerate Cases

```
+------------------------------------------------------------------+
|  BST DEGENERATION (THE PROBLEM)                                  |
+------------------------------------------------------------------+

    BALANCED (ideal):          DEGENERATE (worst):
    
          8                         1
         / \                         \
        4   12                        2
       / \  / \                        \
      2  6 10  14                       3
                                         \
    Height: log₂(n)                       4
    Operations: O(log n)                   \
                                            5
                                   
                                   Height: n
                                   Operations: O(n) - no better than list!

    WHEN DEGENERATION HAPPENS:
    ┌─────────────────────────────────────────────────────────────┐
    │  - Inserting sorted data: 1, 2, 3, 4, 5, ...               │
    │  - Inserting reverse sorted data: 5, 4, 3, 2, 1, ...       │
    │  - Unlucky insertion patterns                               │
    └─────────────────────────────────────────────────────────────┘

    SOLUTIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  - Self-balancing trees: AVL, Red-Black, Splay             │
    │  - Randomized BST (treaps)                                  │
    │  - Periodic rebalancing                                     │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Memory Model

### Node Structure

```
+------------------------------------------------------------------+
|  BST NODE LAYOUT                                                 |
+------------------------------------------------------------------+

    struct bst_node {
        int key;                    /* 4 bytes */
        struct bst_node *left;      /* 8 bytes */
        struct bst_node *right;     /* 8 bytes */
    };                              /* 24 bytes (with padding) */

    WITH PARENT POINTER (for iterators, easier deletion):
    struct bst_node {
        int key;
        struct bst_node *parent;
        struct bst_node *left;
        struct bst_node *right;
    };  /* 32 bytes */

    WITH VALUE (key-value store):
    struct bst_node {
        int key;
        void *value;                /* Pointer to associated data */
        struct bst_node *left;
        struct bst_node *right;
    };  /* 32 bytes */
```

### Search Path

```
+------------------------------------------------------------------+
|  BST SEARCH PATH                                                 |
+------------------------------------------------------------------+

    Searching for key = 6:
    
              ┌───┐
              │ 8 │  6 < 8, go LEFT
              └───┘
                │
         ┌───┐◀┘
         │ 3 │  6 > 3, go RIGHT
         └───┘
              │
            ┌─┴─┐
            │ 6 │  FOUND!
            └───┘

    Each step eliminates half the remaining tree (on average)
    Path length = O(log n) for balanced tree
```

---

## 3. Typical Application Scenarios

### Where BSTs Are Used

```
+------------------------------------------------------------------+
|  BST APPLICATIONS                                                |
+------------------------------------------------------------------+

    DIRECT USE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Symbol tables (compilers)                                │
    │  • Dictionary implementations                               │
    │  • Priority queues (when need min AND max)                  │
    │  • Event scheduling                                         │
    └─────────────────────────────────────────────────────────────┘

    AS FOUNDATION FOR:
    ┌─────────────────────────────────────────────────────────────┐
    │  • std::map, std::set (C++ - usually Red-Black)            │
    │  • TreeMap, TreeSet (Java - Red-Black)                     │
    │  • Databases (B-trees are generalized BSTs)                │
    │  • File systems (directory indexing)                        │
    └─────────────────────────────────────────────────────────────┘

    LINUX KERNEL:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Memory management (vm_area_struct via RB-tree)          │
    │  • Process scheduling (CFS uses RB-tree)                   │
    │  • I/O scheduling                                           │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Complete C Examples

### Example 1: Basic BST Operations

```c
/*
 * Example 1: Complete BST Implementation
 *
 * Search, insert, delete, traversal
 * Compile: gcc -Wall -Wextra -o bst bst.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct bst_node {
    int key;
    struct bst_node *left;
    struct bst_node *right;
};

struct bst {
    struct bst_node *root;
    size_t size;
};

/* Create node */
struct bst_node *node_create(int key)
{
    struct bst_node *node = malloc(sizeof(*node));
    if (node) {
        node->key = key;
        node->left = NULL;
        node->right = NULL;
    }
    return node;
}

/* Initialize BST */
void bst_init(struct bst *tree)
{
    tree->root = NULL;
    tree->size = 0;
}

/* Search for key */
struct bst_node *bst_search(struct bst *tree, int key)
{
    struct bst_node *node = tree->root;
    
    while (node) {
        if (key < node->key)
            node = node->left;
        else if (key > node->key)
            node = node->right;
        else
            return node;  /* Found */
    }
    
    return NULL;  /* Not found */
}

/* Insert key (returns true if inserted, false if duplicate) */
bool bst_insert(struct bst *tree, int key)
{
    struct bst_node **link = &tree->root;
    
    while (*link) {
        if (key < (*link)->key)
            link = &(*link)->left;
        else if (key > (*link)->key)
            link = &(*link)->right;
        else
            return false;  /* Duplicate */
    }
    
    *link = node_create(key);
    if (!*link)
        return false;
    
    tree->size++;
    return true;
}

/* Find minimum in subtree */
struct bst_node *find_min(struct bst_node *node)
{
    while (node && node->left)
        node = node->left;
    return node;
}

/* Find maximum in subtree */
struct bst_node *find_max(struct bst_node *node)
{
    while (node && node->right)
        node = node->right;
    return node;
}

/* Delete key (returns true if deleted) */
bool bst_delete(struct bst *tree, int key)
{
    struct bst_node **link = &tree->root;
    
    /* Find the node */
    while (*link) {
        if (key < (*link)->key)
            link = &(*link)->left;
        else if (key > (*link)->key)
            link = &(*link)->right;
        else
            break;  /* Found */
    }
    
    if (!*link)
        return false;  /* Not found */
    
    struct bst_node *node = *link;
    
    /* Case 1: No children */
    if (!node->left && !node->right) {
        *link = NULL;
        free(node);
    }
    /* Case 2: One child */
    else if (!node->left) {
        *link = node->right;
        free(node);
    }
    else if (!node->right) {
        *link = node->left;
        free(node);
    }
    /* Case 3: Two children - replace with in-order successor */
    else {
        struct bst_node *successor = find_min(node->right);
        node->key = successor->key;
        
        /* Delete successor (has at most one child) */
        struct bst_node **succ_link = &node->right;
        while (*succ_link != successor)
            succ_link = &(*succ_link)->left;
        
        *succ_link = successor->right;
        free(successor);
    }
    
    tree->size--;
    return true;
}

/* In-order traversal (sorted output) */
void bst_inorder(struct bst_node *node)
{
    if (!node)
        return;
    
    bst_inorder(node->left);
    printf("%d ", node->key);
    bst_inorder(node->right);
}

/* Print tree structure */
void bst_print_tree(struct bst_node *node, int level, char prefix)
{
    if (!node)
        return;
    
    for (int i = 0; i < level; i++)
        printf("    ");
    printf("%c── %d\n", prefix, node->key);
    
    bst_print_tree(node->left, level + 1, 'L');
    bst_print_tree(node->right, level + 1, 'R');
}

/* Free entire tree */
void bst_destroy_subtree(struct bst_node *node)
{
    if (!node)
        return;
    
    bst_destroy_subtree(node->left);
    bst_destroy_subtree(node->right);
    free(node);
}

void bst_destroy(struct bst *tree)
{
    bst_destroy_subtree(tree->root);
    tree->root = NULL;
    tree->size = 0;
}

int main(void)
{
    printf("=== Binary Search Tree Demo ===\n\n");
    
    struct bst tree;
    bst_init(&tree);
    
    /* Insert elements */
    int keys[] = {8, 3, 10, 1, 6, 14, 4, 7, 13};
    int n = sizeof(keys) / sizeof(keys[0]);
    
    printf("Inserting: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", keys[i]);
        bst_insert(&tree, keys[i]);
    }
    printf("\n\n");
    
    printf("Tree structure:\n");
    bst_print_tree(tree.root, 0, 'R');
    
    printf("\nIn-order (sorted): ");
    bst_inorder(tree.root);
    printf("\n");
    
    printf("\nMin: %d\n", find_min(tree.root)->key);
    printf("Max: %d\n", find_max(tree.root)->key);
    
    /* Search */
    printf("\nSearching for 6: %s\n", 
           bst_search(&tree, 6) ? "FOUND" : "NOT FOUND");
    printf("Searching for 99: %s\n", 
           bst_search(&tree, 99) ? "FOUND" : "NOT FOUND");
    
    /* Delete */
    printf("\nDeleting 3 (node with two children)...\n");
    bst_delete(&tree, 3);
    printf("Tree after deletion:\n");
    bst_print_tree(tree.root, 0, 'R');
    
    printf("\nIn-order: ");
    bst_inorder(tree.root);
    printf("\n");
    
    bst_destroy(&tree);
    return 0;
}
```

---

### Example 2: BST with Parent Pointers (Iterator Support)

```c
/*
 * Example 2: BST with Parent Pointers and Iterator
 *
 * Enables forward/backward traversal without stack
 * Compile: gcc -Wall -Wextra -o bst_iterator bst_iterator.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct bst_node {
    int key;
    struct bst_node *parent;
    struct bst_node *left;
    struct bst_node *right;
};

struct bst {
    struct bst_node *root;
    size_t size;
};

struct bst_node *node_create(int key, struct bst_node *parent)
{
    struct bst_node *node = malloc(sizeof(*node));
    if (node) {
        node->key = key;
        node->parent = parent;
        node->left = NULL;
        node->right = NULL;
    }
    return node;
}

void bst_init(struct bst *tree)
{
    tree->root = NULL;
    tree->size = 0;
}

bool bst_insert(struct bst *tree, int key)
{
    struct bst_node *parent = NULL;
    struct bst_node **link = &tree->root;
    
    while (*link) {
        parent = *link;
        if (key < (*link)->key)
            link = &(*link)->left;
        else if (key > (*link)->key)
            link = &(*link)->right;
        else
            return false;
    }
    
    *link = node_create(key, parent);
    if (!*link)
        return false;
    
    tree->size++;
    return true;
}

/* Find minimum in subtree */
struct bst_node *tree_min(struct bst_node *node)
{
    if (!node)
        return NULL;
    while (node->left)
        node = node->left;
    return node;
}

/* Find maximum in subtree */
struct bst_node *tree_max(struct bst_node *node)
{
    if (!node)
        return NULL;
    while (node->right)
        node = node->right;
    return node;
}

/* Find in-order successor (next larger) */
struct bst_node *tree_successor(struct bst_node *node)
{
    if (!node)
        return NULL;
    
    /* If right subtree exists, successor is minimum there */
    if (node->right)
        return tree_min(node->right);
    
    /* Otherwise, go up until we come from left */
    struct bst_node *parent = node->parent;
    while (parent && node == parent->right) {
        node = parent;
        parent = parent->parent;
    }
    
    return parent;
}

/* Find in-order predecessor (previous smaller) */
struct bst_node *tree_predecessor(struct bst_node *node)
{
    if (!node)
        return NULL;
    
    /* If left subtree exists, predecessor is maximum there */
    if (node->left)
        return tree_max(node->left);
    
    /* Otherwise, go up until we come from right */
    struct bst_node *parent = node->parent;
    while (parent && node == parent->left) {
        node = parent;
        parent = parent->parent;
    }
    
    return parent;
}

/* Iterator structure */
struct bst_iterator {
    struct bst_node *current;
};

void iter_begin(struct bst_iterator *it, struct bst *tree)
{
    it->current = tree_min(tree->root);
}

void iter_end(struct bst_iterator *it, struct bst *tree)
{
    it->current = tree_max(tree->root);
}

bool iter_valid(struct bst_iterator *it)
{
    return it->current != NULL;
}

void iter_next(struct bst_iterator *it)
{
    it->current = tree_successor(it->current);
}

void iter_prev(struct bst_iterator *it)
{
    it->current = tree_predecessor(it->current);
}

int iter_get(struct bst_iterator *it)
{
    return it->current->key;
}

void bst_destroy_subtree(struct bst_node *node)
{
    if (!node) return;
    bst_destroy_subtree(node->left);
    bst_destroy_subtree(node->right);
    free(node);
}

void bst_destroy(struct bst *tree)
{
    bst_destroy_subtree(tree->root);
    tree->root = NULL;
    tree->size = 0;
}

int main(void)
{
    printf("=== BST with Iterator Demo ===\n\n");
    
    struct bst tree;
    bst_init(&tree);
    
    int keys[] = {50, 30, 70, 20, 40, 60, 80};
    for (int i = 0; i < 7; i++)
        bst_insert(&tree, keys[i]);
    
    printf("Tree: 50, 30, 70, 20, 40, 60, 80\n\n");
    
    /* Forward iteration */
    printf("Forward iteration (min to max):\n  ");
    struct bst_iterator it;
    for (iter_begin(&it, &tree); iter_valid(&it); iter_next(&it)) {
        printf("%d ", iter_get(&it));
    }
    printf("\n");
    
    /* Backward iteration */
    printf("\nBackward iteration (max to min):\n  ");
    for (iter_end(&it, &tree); iter_valid(&it); iter_prev(&it)) {
        printf("%d ", iter_get(&it));
    }
    printf("\n");
    
    /* Find successor/predecessor of 40 */
    struct bst_node *node = tree.root->left->right;  /* node 40 */
    printf("\nFor node 40:\n");
    printf("  Predecessor: %d\n", tree_predecessor(node)->key);
    printf("  Successor:   %d\n", tree_successor(node)->key);
    
    bst_destroy(&tree);
    return 0;
}
```

---

### Example 3: Range Query

```c
/*
 * Example 3: BST Range Query
 *
 * Find all keys in range [low, high]
 * Compile: gcc -Wall -Wextra -o bst_range bst_range.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct bst_node {
    int key;
    struct bst_node *left;
    struct bst_node *right;
};

struct bst {
    struct bst_node *root;
};

struct bst_node *node_create(int key)
{
    struct bst_node *node = malloc(sizeof(*node));
    if (node) {
        node->key = key;
        node->left = node->right = NULL;
    }
    return node;
}

void bst_insert(struct bst *tree, int key)
{
    struct bst_node **link = &tree->root;
    while (*link) {
        if (key < (*link)->key)
            link = &(*link)->left;
        else
            link = &(*link)->right;
    }
    *link = node_create(key);
}

/* Range query: find all keys in [low, high] */
void range_query(struct bst_node *node, int low, int high, 
                 int *result, int *count)
{
    if (!node)
        return;
    
    /* If node->key > low, there might be results in left subtree */
    if (node->key > low)
        range_query(node->left, low, high, result, count);
    
    /* Include node if in range */
    if (node->key >= low && node->key <= high)
        result[(*count)++] = node->key;
    
    /* If node->key < high, there might be results in right subtree */
    if (node->key < high)
        range_query(node->right, low, high, result, count);
}

/* Count keys in range (without collecting) */
int range_count(struct bst_node *node, int low, int high)
{
    if (!node)
        return 0;
    
    int count = 0;
    
    if (node->key > low)
        count += range_count(node->left, low, high);
    
    if (node->key >= low && node->key <= high)
        count++;
    
    if (node->key < high)
        count += range_count(node->right, low, high);
    
    return count;
}

/* Floor: largest key ≤ given key */
struct bst_node *bst_floor(struct bst_node *node, int key)
{
    if (!node)
        return NULL;
    
    if (key == node->key)
        return node;
    
    if (key < node->key)
        return bst_floor(node->left, key);
    
    /* key > node->key: this node is a candidate, check right */
    struct bst_node *right_floor = bst_floor(node->right, key);
    return right_floor ? right_floor : node;
}

/* Ceiling: smallest key ≥ given key */
struct bst_node *bst_ceiling(struct bst_node *node, int key)
{
    if (!node)
        return NULL;
    
    if (key == node->key)
        return node;
    
    if (key > node->key)
        return bst_ceiling(node->right, key);
    
    struct bst_node *left_ceil = bst_ceiling(node->left, key);
    return left_ceil ? left_ceil : node;
}

void bst_destroy(struct bst_node *node)
{
    if (!node) return;
    bst_destroy(node->left);
    bst_destroy(node->right);
    free(node);
}

int main(void)
{
    printf("=== BST Range Query Demo ===\n\n");
    
    struct bst tree = {NULL};
    
    int keys[] = {50, 30, 70, 20, 40, 60, 80, 25, 35, 45, 55, 65};
    int n = sizeof(keys) / sizeof(keys[0]);
    
    for (int i = 0; i < n; i++)
        bst_insert(&tree, keys[i]);
    
    printf("BST contains: 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 80\n\n");
    
    /* Range query [35, 60] */
    int result[20];
    int count = 0;
    range_query(tree.root, 35, 60, result, &count);
    
    printf("Range query [35, 60]:\n  Found %d keys: ", count);
    for (int i = 0; i < count; i++)
        printf("%d ", result[i]);
    printf("\n");
    
    /* Count in range */
    printf("\nCount in range [20, 40]: %d\n", 
           range_count(tree.root, 20, 40));
    
    /* Floor and ceiling */
    struct bst_node *f, *c;
    
    f = bst_floor(tree.root, 42);
    c = bst_ceiling(tree.root, 42);
    printf("\nFor key 42:\n");
    printf("  Floor (largest ≤ 42):   %d\n", f ? f->key : -1);
    printf("  Ceiling (smallest ≥ 42): %d\n", c ? c->key : -1);
    
    f = bst_floor(tree.root, 50);
    c = bst_ceiling(tree.root, 50);
    printf("\nFor key 50 (exists):\n");
    printf("  Floor:   %d\n", f ? f->key : -1);
    printf("  Ceiling: %d\n", c ? c->key : -1);
    
    bst_destroy(tree.root);
    return 0;
}
```

---

### Example 4: BST as Key-Value Store

```c
/*
 * Example 4: BST as Key-Value Map
 *
 * String keys, arbitrary values
 * Compile: gcc -Wall -Wextra -o bst_map bst_map.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct map_node {
    char *key;              /* Dynamically allocated */
    void *value;            /* User-provided value */
    struct map_node *left;
    struct map_node *right;
};

struct string_map {
    struct map_node *root;
    size_t size;
};

void map_init(struct string_map *m)
{
    m->root = NULL;
    m->size = 0;
}

/* Insert or update key-value pair */
bool map_put(struct string_map *m, const char *key, void *value)
{
    struct map_node **link = &m->root;
    
    while (*link) {
        int cmp = strcmp(key, (*link)->key);
        if (cmp < 0)
            link = &(*link)->left;
        else if (cmp > 0)
            link = &(*link)->right;
        else {
            /* Key exists, update value */
            (*link)->value = value;
            return true;
        }
    }
    
    /* Insert new node */
    struct map_node *node = malloc(sizeof(*node));
    if (!node)
        return false;
    
    node->key = strdup(key);
    if (!node->key) {
        free(node);
        return false;
    }
    
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    
    *link = node;
    m->size++;
    return true;
}

/* Get value for key */
void *map_get(struct string_map *m, const char *key)
{
    struct map_node *node = m->root;
    
    while (node) {
        int cmp = strcmp(key, node->key);
        if (cmp < 0)
            node = node->left;
        else if (cmp > 0)
            node = node->right;
        else
            return node->value;
    }
    
    return NULL;  /* Not found */
}

/* Check if key exists */
bool map_contains(struct string_map *m, const char *key)
{
    struct map_node *node = m->root;
    
    while (node) {
        int cmp = strcmp(key, node->key);
        if (cmp < 0)
            node = node->left;
        else if (cmp > 0)
            node = node->right;
        else
            return true;
    }
    
    return false;
}

/* In-order traversal callback */
typedef void (*map_visitor)(const char *key, void *value, void *ctx);

void map_foreach_node(struct map_node *node, map_visitor visit, void *ctx)
{
    if (!node)
        return;
    
    map_foreach_node(node->left, visit, ctx);
    visit(node->key, node->value, ctx);
    map_foreach_node(node->right, visit, ctx);
}

void map_foreach(struct string_map *m, map_visitor visit, void *ctx)
{
    map_foreach_node(m->root, visit, ctx);
}

void map_destroy_node(struct map_node *node)
{
    if (!node)
        return;
    
    map_destroy_node(node->left);
    map_destroy_node(node->right);
    free(node->key);
    free(node);
}

void map_destroy(struct string_map *m)
{
    map_destroy_node(m->root);
    m->root = NULL;
    m->size = 0;
}

/* Visitor for printing */
void print_entry(const char *key, void *value, void *ctx)
{
    (void)ctx;
    printf("  %s: %s\n", key, (char *)value);
}

int main(void)
{
    printf("=== BST String Map Demo ===\n\n");
    
    struct string_map phonebook;
    map_init(&phonebook);
    
    /* Insert entries */
    map_put(&phonebook, "Alice", "555-1234");
    map_put(&phonebook, "Bob", "555-5678");
    map_put(&phonebook, "Charlie", "555-9012");
    map_put(&phonebook, "Diana", "555-3456");
    map_put(&phonebook, "Eve", "555-7890");
    
    printf("Phonebook entries (sorted by name):\n");
    map_foreach(&phonebook, print_entry, NULL);
    
    /* Lookup */
    printf("\nLooking up 'Charlie': %s\n", 
           (char *)map_get(&phonebook, "Charlie"));
    printf("Looking up 'Frank': %s\n",
           map_get(&phonebook, "Frank") ? 
           (char *)map_get(&phonebook, "Frank") : "(not found)");
    
    /* Update */
    printf("\nUpdating Alice's number...\n");
    map_put(&phonebook, "Alice", "555-0000");
    printf("Alice's new number: %s\n",
           (char *)map_get(&phonebook, "Alice"));
    
    printf("\nMap size: %zu\n", phonebook.size);
    
    map_destroy(&phonebook);
    return 0;
}
```

---

### Example 5: Demonstrating Degenerate BST

```c
/*
 * Example 5: BST Degeneration Demo
 *
 * Shows how sorted insertion creates a linear tree
 * Compile: gcc -Wall -Wextra -o bst_degenerate bst_degenerate.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct bst_node {
    int key;
    struct bst_node *left;
    struct bst_node *right;
};

struct bst_node *node_create(int key)
{
    struct bst_node *node = malloc(sizeof(*node));
    if (node) {
        node->key = key;
        node->left = node->right = NULL;
    }
    return node;
}

void bst_insert(struct bst_node **root, int key)
{
    struct bst_node **link = root;
    while (*link) {
        if (key < (*link)->key)
            link = &(*link)->left;
        else
            link = &(*link)->right;
    }
    *link = node_create(key);
}

int bst_height(struct bst_node *node)
{
    if (!node)
        return -1;
    
    int lh = bst_height(node->left);
    int rh = bst_height(node->right);
    
    return 1 + (lh > rh ? lh : rh);
}

int bst_count(struct bst_node *node)
{
    if (!node)
        return 0;
    return 1 + bst_count(node->left) + bst_count(node->right);
}

/* Count steps to find a key */
int search_steps(struct bst_node *node, int key)
{
    int steps = 0;
    while (node) {
        steps++;
        if (key == node->key)
            return steps;
        if (key < node->key)
            node = node->left;
        else
            node = node->right;
    }
    return steps;
}

void bst_destroy(struct bst_node *node)
{
    if (!node) return;
    bst_destroy(node->left);
    bst_destroy(node->right);
    free(node);
}

/* Shuffle array (Fisher-Yates) */
void shuffle(int *arr, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

int main(void)
{
    printf("=== BST Degeneration Demo ===\n\n");
    
    srand((unsigned)time(NULL));
    
    const int N = 1000;
    int *sorted = malloc(N * sizeof(int));
    int *shuffled = malloc(N * sizeof(int));
    
    for (int i = 0; i < N; i++) {
        sorted[i] = i;
        shuffled[i] = i;
    }
    shuffle(shuffled, N);
    
    /* Build tree from sorted data (WORST CASE) */
    struct bst_node *degenerate = NULL;
    clock_t start = clock();
    for (int i = 0; i < N; i++)
        bst_insert(&degenerate, sorted[i]);
    clock_t end = clock();
    
    printf("SORTED INSERTION (worst case):\n");
    printf("  Nodes: %d\n", bst_count(degenerate));
    printf("  Height: %d (expected: %d for linear)\n", 
           bst_height(degenerate), N - 1);
    printf("  Insert time: %.4f sec\n", 
           (double)(end - start) / CLOCKS_PER_SEC);
    printf("  Steps to find 500: %d\n", search_steps(degenerate, 500));
    printf("  Steps to find 999: %d\n", search_steps(degenerate, 999));
    
    /* Build tree from shuffled data (AVERAGE CASE) */
    struct bst_node *balanced = NULL;
    start = clock();
    for (int i = 0; i < N; i++)
        bst_insert(&balanced, shuffled[i]);
    end = clock();
    
    printf("\nSHUFFLED INSERTION (average case):\n");
    printf("  Nodes: %d\n", bst_count(balanced));
    printf("  Height: %d (ideal: ~%d for balanced)\n", 
           bst_height(balanced), (int)(1.44 * 10));  /* log2(1000) ≈ 10 */
    printf("  Insert time: %.4f sec\n", 
           (double)(end - start) / CLOCKS_PER_SEC);
    printf("  Steps to find 500: %d\n", search_steps(balanced, 500));
    printf("  Steps to find 999: %d\n", search_steps(balanced, 999));
    
    printf("\n");
    printf("LESSON: Sorted insertion creates O(n) operations!\n");
    printf("SOLUTION: Use self-balancing trees (AVL, Red-Black)\n");
    
    bst_destroy(degenerate);
    bst_destroy(balanced);
    free(sorted);
    free(shuffled);
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

```
+------------------------------------------------------------------+
|  BST COMPLEXITY                                                  |
+------------------------------------------------------------------+

    ┌────────────────┬─────────────────┬────────────────────────────┐
    │ Operation      │ Average (rand)  │ Worst (sorted input)       │
    ├────────────────┼─────────────────┼────────────────────────────┤
    │ Search         │ O(log n)        │ O(n)                       │
    │ Insert         │ O(log n)        │ O(n)                       │
    │ Delete         │ O(log n)        │ O(n)                       │
    │ Min/Max        │ O(log n)        │ O(n)                       │
    │ Successor      │ O(log n)        │ O(n)                       │
    │ Range query    │ O(log n + k)    │ O(n + k)                   │
    └────────────────┴─────────────────┴────────────────────────────┘

    BST vs ALTERNATIVES:
    ┌────────────────┬─────────────────┬─────────────────────────────┐
    │ Structure      │ Pros            │ Cons                        │
    ├────────────────┼─────────────────┼─────────────────────────────┤
    │ BST            │ Simple          │ Can degenerate              │
    │ AVL            │ Strictly balanced│ Complex rotations          │
    │ Red-Black      │ Good balance    │ Complex, more common        │
    │ Hash table     │ O(1) average    │ No ordering, no range query │
    │ Skip list      │ Simpler balance │ Probabilistic               │
    └────────────────┴─────────────────┴─────────────────────────────┘
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|  BST: KEY TAKEAWAYS                                              |
+------------------------------------------------------------------+

    CORE PROPERTY:
    left < node < right (recursively)
    In-order traversal = sorted output

    TIME COMPLEXITY:
    O(log n) average, O(n) worst (degenerate)
    Height determines performance

    KEY OPERATIONS:
    - Search: compare, go left or right
    - Insert: search to leaf, add there
    - Delete: 3 cases (leaf, one child, two children)
    - Range query: prune branches outside range

    WHEN TO USE BST:
    ✓ Need ordered traversal
    ✓ Need range queries
    ✓ Need predecessor/successor
    ✓ Random insertion order

    WHEN TO AVOID:
    ✗ Sorted input (use balanced tree)
    ✗ Only need search (use hash table)
    ✗ Memory-constrained (24+ bytes/node)

    REAL WORLD:
    Almost always use balanced variants (RB-tree, AVL)
    Linux kernel uses RB-tree exclusively
```

**中文总结：**
- **核心性质**：左 < 节点 < 右，中序遍历得到排序结果
- **时间复杂度**：平均 O(log n)，最坏 O(n)（退化为链表）
- **关键操作**：搜索、插入、删除、范围查询
- **使用场景**：需要有序遍历、范围查询、前驱/后继
- **实际应用**：几乎总是使用平衡变体（红黑树、AVL树）

