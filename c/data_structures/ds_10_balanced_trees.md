# Balanced Trees: Red-Black Tree & AVL Tree in C

## 1. Definition & Design Principles

### The Problem: BST Degeneration

```
+------------------------------------------------------------------+
|  WHY BALANCED TREES?                                             |
+------------------------------------------------------------------+

    PROBLEM WITH PLAIN BST:
    ┌─────────────────────────────────────────────────────────────┐
    │  Sorted insertion creates a linked list!                    │
    │                                                              │
    │  Insert: 1, 2, 3, 4, 5                                      │
    │                                                              │
    │      1                                                       │
    │       \                                                      │
    │        2        Height = n - 1                               │
    │         \       Operations = O(n)                            │
    │          3      NO BETTER THAN LINKED LIST!                 │
    │           \                                                  │
    │            4                                                 │
    │             \                                                │
    │              5                                               │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: SELF-BALANCING TREES
    ┌─────────────────────────────────────────────────────────────┐
    │  Maintain approximate balance after every insert/delete    │
    │  Guarantee O(log n) height                                  │
    │  Trade-off: more complex insert/delete                      │
    └─────────────────────────────────────────────────────────────┘

    BALANCED RESULT:
              3
             / \
            2   4        Height = O(log n)
           /     \       Operations = O(log n)
          1       5      GUARANTEED!
```

**中文解释：**
- **问题**：普通 BST 在顺序插入时退化为链表，O(n) 操作
- **解决方案**：自平衡树——在每次插入/删除后保持近似平衡
- **代价**：更复杂的插入/删除操作

### Two Main Balanced Tree Strategies

```
+------------------------------------------------------------------+
|  AVL TREE vs RED-BLACK TREE                                      |
+------------------------------------------------------------------+

    AVL TREE (Adelson-Velsky and Landis, 1962):
    ┌─────────────────────────────────────────────────────────────┐
    │  INVARIANT: For every node, heights of left and right      │
    │             subtrees differ by at most 1                    │
    │                                                              │
    │  Balance factor = height(left) - height(right)              │
    │  Valid: -1, 0, +1                                           │
    │                                                              │
    │  STRICTEST BALANCE → best search performance               │
    │  More rotations on insert/delete                            │
    └─────────────────────────────────────────────────────────────┘

    RED-BLACK TREE (Guibas & Sedgewick, 1978):
    ┌─────────────────────────────────────────────────────────────┐
    │  INVARIANTS:                                                │
    │  1. Every node is RED or BLACK                              │
    │  2. Root is BLACK                                           │
    │  3. NULL leaves are BLACK                                   │
    │  4. RED node has BLACK children (no consecutive reds)       │
    │  5. Every path root→NULL has same BLACK count              │
    │                                                              │
    │  RELAXED BALANCE → fewer rotations, simpler amortized      │
    │  Height ≤ 2 log₂(n+1)                                       │
    └─────────────────────────────────────────────────────────────┘

    COMPARISON:
    ┌─────────────────────┬────────────────┬────────────────────────┐
    │ Aspect              │ AVL            │ Red-Black              │
    ├─────────────────────┼────────────────┼────────────────────────┤
    │ Height bound        │ 1.44 log n     │ 2 log n                │
    │ Search speed        │ Faster         │ Slightly slower        │
    │ Insert rotations    │ ≤ 2            │ ≤ 2                    │
    │ Delete rotations    │ O(log n)       │ ≤ 3                    │
    │ Memory overhead     │ +2 bits/node   │ +1 bit/node            │
    │ Implementation      │ Simpler logic  │ More cases             │
    │ Real-world use      │ Databases      │ OS/libraries (Linux)   │
    └─────────────────────┴────────────────┴────────────────────────┘
```

---

## 2. Memory Model

### AVL Tree Node

```
+------------------------------------------------------------------+
|  AVL TREE NODE LAYOUT                                            |
+------------------------------------------------------------------+

    struct avl_node {
        int key;                    /* 4 bytes */
        int height;                 /* 4 bytes (or balance factor) */
        struct avl_node *left;      /* 8 bytes */
        struct avl_node *right;     /* 8 bytes */
    };                              /* 24 bytes (with padding) */

    ALTERNATIVE (save space):
    struct avl_node {
        int key;
        signed char balance;        /* -1, 0, +1 only */
        struct avl_node *left;
        struct avl_node *right;
    };  /* Still 24 bytes due to alignment, but logically smaller */
```

### Red-Black Tree Node

```
+------------------------------------------------------------------+
|  RED-BLACK TREE NODE LAYOUT                                      |
+------------------------------------------------------------------+

    struct rb_node {
        int key;                    /* 4 bytes */
        unsigned int color : 1;     /* 1 bit (often stored in ptr) */
        struct rb_node *parent;     /* 8 bytes (needed for RB!) */
        struct rb_node *left;       /* 8 bytes */
        struct rb_node *right;      /* 8 bytes */
    };                              /* 32 bytes */

    LINUX KERNEL OPTIMIZATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  Color bit stored in LOW BIT of parent pointer             │
    │  (Works because pointers are aligned to 4+ bytes)          │
    │                                                              │
    │  struct rb_node {                                           │
    │      unsigned long __rb_parent_color;  /* parent + color */ │
    │      struct rb_node *rb_right;                              │
    │      struct rb_node *rb_left;                               │
    │  };  /* Only 24 bytes on 64-bit! */                         │
    └─────────────────────────────────────────────────────────────┘
```

### Rotation Operations

```
+------------------------------------------------------------------+
|  TREE ROTATIONS (USED BY BOTH AVL AND RB)                        |
+------------------------------------------------------------------+

    LEFT ROTATION (right child becomes parent):
    
        X                       Y
       / \                     / \
      a   Y        ──▶        X   c
         / \                 / \
        b   c               a   b

    RIGHT ROTATION (left child becomes parent):
    
          Y                     X
         / \                   / \
        X   c      ──▶        a   Y
       / \                       / \
      a   b                     b   c

    DOUBLE ROTATIONS:
    Left-Right: rotate left on left child, then right on node
    Right-Left: rotate right on right child, then left on node

    KEY INSIGHT:
    - Rotations maintain BST property
    - Rotations are O(1) - just pointer swaps
    - They change local heights, restoring balance
```

---

## 3. Typical Application Scenarios

### Where Each Is Used

```
+------------------------------------------------------------------+
|  BALANCED TREE APPLICATIONS                                      |
+------------------------------------------------------------------+

    RED-BLACK TREE (more common in practice):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Linux kernel (everywhere!)                               │
    │    - VM area management (vm_area_struct)                    │
    │    - CFS scheduler (fair scheduling)                        │
    │    - I/O schedulers                                         │
    │    - epoll implementation                                   │
    │  • C++ std::map, std::set                                   │
    │  • Java TreeMap, TreeSet                                    │
    │  • Many language runtimes                                   │
    └─────────────────────────────────────────────────────────────┘

    AVL TREE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Databases (read-heavy workloads)                         │
    │  • In-memory indices                                        │
    │  • When search speed is critical                            │
    │  • Academic/educational contexts                            │
    └─────────────────────────────────────────────────────────────┘

    WHEN TO CHOOSE:
    ┌─────────────────────────────────────────────────────────────┐
    │  AVL: Read-heavy, need fastest lookups                     │
    │  RB:  Write-heavy, need balanced insert/delete/search      │
    │                                                              │
    │  In practice: RB-tree wins for general purpose             │
    │  (predictable insert/delete, good enough search)           │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Complete C Examples

### Example 1: AVL Tree Implementation

```c
/*
 * Example 1: Complete AVL Tree
 *
 * Self-balancing BST with height-based balance factor
 * Compile: gcc -Wall -Wextra -o avl avl.c
 */

#include <stdio.h>
#include <stdlib.h>

struct avl_node {
    int key;
    int height;
    struct avl_node *left;
    struct avl_node *right;
};

struct avl_tree {
    struct avl_node *root;
    size_t size;
};

/* Get height (NULL-safe) */
static int height(struct avl_node *node)
{
    return node ? node->height : 0;
}

/* Get balance factor */
static int balance_factor(struct avl_node *node)
{
    return node ? height(node->left) - height(node->right) : 0;
}

/* Update height based on children */
static void update_height(struct avl_node *node)
{
    int lh = height(node->left);
    int rh = height(node->right);
    node->height = 1 + (lh > rh ? lh : rh);
}

/* Create node */
static struct avl_node *node_create(int key)
{
    struct avl_node *node = malloc(sizeof(*node));
    if (node) {
        node->key = key;
        node->height = 1;
        node->left = NULL;
        node->right = NULL;
    }
    return node;
}

/* Right rotation */
static struct avl_node *rotate_right(struct avl_node *y)
{
    struct avl_node *x = y->left;
    struct avl_node *T2 = x->right;
    
    /* Rotate */
    x->right = y;
    y->left = T2;
    
    /* Update heights */
    update_height(y);
    update_height(x);
    
    return x;  /* New root */
}

/* Left rotation */
static struct avl_node *rotate_left(struct avl_node *x)
{
    struct avl_node *y = x->right;
    struct avl_node *T2 = y->left;
    
    /* Rotate */
    y->left = x;
    x->right = T2;
    
    /* Update heights */
    update_height(x);
    update_height(y);
    
    return y;  /* New root */
}

/* Rebalance node */
static struct avl_node *rebalance(struct avl_node *node)
{
    update_height(node);
    
    int bf = balance_factor(node);
    
    /* Left heavy */
    if (bf > 1) {
        if (balance_factor(node->left) < 0)
            node->left = rotate_left(node->left);  /* Left-Right case */
        return rotate_right(node);
    }
    
    /* Right heavy */
    if (bf < -1) {
        if (balance_factor(node->right) > 0)
            node->right = rotate_right(node->right);  /* Right-Left case */
        return rotate_left(node);
    }
    
    return node;  /* Already balanced */
}

/* Insert recursively */
static struct avl_node *insert_node(struct avl_node *node, int key)
{
    if (!node)
        return node_create(key);
    
    if (key < node->key)
        node->left = insert_node(node->left, key);
    else if (key > node->key)
        node->right = insert_node(node->right, key);
    else
        return node;  /* Duplicate */
    
    return rebalance(node);
}

/* Find minimum */
static struct avl_node *find_min(struct avl_node *node)
{
    while (node && node->left)
        node = node->left;
    return node;
}

/* Delete recursively */
static struct avl_node *delete_node(struct avl_node *node, int key)
{
    if (!node)
        return NULL;
    
    if (key < node->key)
        node->left = delete_node(node->left, key);
    else if (key > node->key)
        node->right = delete_node(node->right, key);
    else {
        /* Found node to delete */
        if (!node->left || !node->right) {
            struct avl_node *child = node->left ? node->left : node->right;
            free(node);
            return child;
        }
        
        /* Two children: replace with successor */
        struct avl_node *succ = find_min(node->right);
        node->key = succ->key;
        node->right = delete_node(node->right, succ->key);
    }
    
    return rebalance(node);
}

/* Public interface */
void avl_init(struct avl_tree *tree)
{
    tree->root = NULL;
    tree->size = 0;
}

void avl_insert(struct avl_tree *tree, int key)
{
    tree->root = insert_node(tree->root, key);
    tree->size++;
}

void avl_delete(struct avl_tree *tree, int key)
{
    tree->root = delete_node(tree->root, key);
    tree->size--;
}

struct avl_node *avl_search(struct avl_tree *tree, int key)
{
    struct avl_node *node = tree->root;
    while (node) {
        if (key < node->key)
            node = node->left;
        else if (key > node->key)
            node = node->right;
        else
            return node;
    }
    return NULL;
}

void avl_inorder(struct avl_node *node)
{
    if (!node) return;
    avl_inorder(node->left);
    printf("%d(h=%d) ", node->key, node->height);
    avl_inorder(node->right);
}

void avl_print_tree(struct avl_node *node, int level, char prefix)
{
    if (!node) return;
    
    for (int i = 0; i < level; i++)
        printf("    ");
    printf("%c── %d (bf=%d)\n", prefix, node->key, balance_factor(node));
    
    avl_print_tree(node->left, level + 1, 'L');
    avl_print_tree(node->right, level + 1, 'R');
}

void avl_destroy_node(struct avl_node *node)
{
    if (!node) return;
    avl_destroy_node(node->left);
    avl_destroy_node(node->right);
    free(node);
}

void avl_destroy(struct avl_tree *tree)
{
    avl_destroy_node(tree->root);
    tree->root = NULL;
    tree->size = 0;
}

int main(void)
{
    printf("=== AVL Tree Demo ===\n\n");
    
    struct avl_tree tree;
    avl_init(&tree);
    
    /* Insert in sorted order (would degenerate plain BST) */
    printf("Inserting 1-10 in order:\n");
    for (int i = 1; i <= 10; i++) {
        avl_insert(&tree, i);
        printf("After insert %d: height = %d\n", i, height(tree.root));
    }
    
    printf("\nTree structure:\n");
    avl_print_tree(tree.root, 0, 'R');
    
    printf("\nIn-order (sorted): ");
    avl_inorder(tree.root);
    printf("\n");
    
    printf("\nNote: Height is %d, not 9 (plain BST would be 9)!\n",
           height(tree.root));
    printf("For 10 nodes, ideal height = ceil(log2(11)) = 4\n");
    
    /* Delete */
    printf("\nDeleting 5...\n");
    avl_delete(&tree, 5);
    avl_print_tree(tree.root, 0, 'R');
    
    avl_destroy(&tree);
    return 0;
}
```

---

### Example 2: Red-Black Tree (Simplified)

```c
/*
 * Example 2: Red-Black Tree Implementation
 *
 * Production-style RB-tree with color invariants
 * Compile: gcc -Wall -Wextra -o rbtree rbtree.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum { RED, BLACK } color_t;

struct rb_node {
    int key;
    color_t color;
    struct rb_node *parent;
    struct rb_node *left;
    struct rb_node *right;
};

struct rb_tree {
    struct rb_node *root;
    struct rb_node *nil;  /* Sentinel for NULL */
    size_t size;
};

/* Create sentinel (all NULL leaves point here) */
static struct rb_node *create_nil(void)
{
    struct rb_node *nil = malloc(sizeof(*nil));
    if (nil) {
        nil->color = BLACK;
        nil->parent = nil;
        nil->left = nil;
        nil->right = nil;
    }
    return nil;
}

/* Initialize tree */
void rb_init(struct rb_tree *tree)
{
    tree->nil = create_nil();
    tree->root = tree->nil;
    tree->size = 0;
}

/* Create node */
static struct rb_node *node_create(struct rb_tree *tree, int key)
{
    struct rb_node *node = malloc(sizeof(*node));
    if (node) {
        node->key = key;
        node->color = RED;  /* New nodes are RED */
        node->parent = tree->nil;
        node->left = tree->nil;
        node->right = tree->nil;
    }
    return node;
}

/* Left rotation */
static void rotate_left(struct rb_tree *tree, struct rb_node *x)
{
    struct rb_node *y = x->right;
    
    x->right = y->left;
    if (y->left != tree->nil)
        y->left->parent = x;
    
    y->parent = x->parent;
    if (x->parent == tree->nil)
        tree->root = y;
    else if (x == x->parent->left)
        x->parent->left = y;
    else
        x->parent->right = y;
    
    y->left = x;
    x->parent = y;
}

/* Right rotation */
static void rotate_right(struct rb_tree *tree, struct rb_node *y)
{
    struct rb_node *x = y->left;
    
    y->left = x->right;
    if (x->right != tree->nil)
        x->right->parent = y;
    
    x->parent = y->parent;
    if (y->parent == tree->nil)
        tree->root = x;
    else if (y == y->parent->right)
        y->parent->right = x;
    else
        y->parent->left = x;
    
    x->right = y;
    y->parent = x;
}

/* Fix RB properties after insert */
static void insert_fixup(struct rb_tree *tree, struct rb_node *z)
{
    while (z->parent->color == RED) {
        if (z->parent == z->parent->parent->left) {
            struct rb_node *y = z->parent->parent->right;  /* Uncle */
            
            if (y->color == RED) {
                /* Case 1: Uncle is red - recolor */
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    /* Case 2: Uncle is black, z is right child */
                    z = z->parent;
                    rotate_left(tree, z);
                }
                /* Case 3: Uncle is black, z is left child */
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rotate_right(tree, z->parent->parent);
            }
        } else {
            /* Symmetric cases (parent is right child) */
            struct rb_node *y = z->parent->parent->left;
            
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rotate_right(tree, z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rotate_left(tree, z->parent->parent);
            }
        }
    }
    
    tree->root->color = BLACK;  /* Root is always black */
}

/* Insert key */
void rb_insert(struct rb_tree *tree, int key)
{
    struct rb_node *z = node_create(tree, key);
    struct rb_node *y = tree->nil;
    struct rb_node *x = tree->root;
    
    /* BST insert */
    while (x != tree->nil) {
        y = x;
        if (key < x->key)
            x = x->left;
        else
            x = x->right;
    }
    
    z->parent = y;
    if (y == tree->nil)
        tree->root = z;
    else if (key < y->key)
        y->left = z;
    else
        y->right = z;
    
    /* Fix RB properties */
    insert_fixup(tree, z);
    tree->size++;
}

/* Search */
struct rb_node *rb_search(struct rb_tree *tree, int key)
{
    struct rb_node *node = tree->root;
    
    while (node != tree->nil) {
        if (key < node->key)
            node = node->left;
        else if (key > node->key)
            node = node->right;
        else
            return node;
    }
    
    return NULL;
}

/* In-order traversal */
void rb_inorder_node(struct rb_tree *tree, struct rb_node *node)
{
    if (node == tree->nil)
        return;
    
    rb_inorder_node(tree, node->left);
    printf("%d%c ", node->key, node->color == RED ? 'R' : 'B');
    rb_inorder_node(tree, node->right);
}

void rb_inorder(struct rb_tree *tree)
{
    rb_inorder_node(tree, tree->root);
}

/* Print tree structure */
void rb_print_node(struct rb_tree *tree, struct rb_node *node, 
                   int level, char prefix)
{
    if (node == tree->nil)
        return;
    
    for (int i = 0; i < level; i++)
        printf("    ");
    printf("%c── %d(%c)\n", prefix, node->key, 
           node->color == RED ? 'R' : 'B');
    
    rb_print_node(tree, node->left, level + 1, 'L');
    rb_print_node(tree, node->right, level + 1, 'R');
}

void rb_print(struct rb_tree *tree)
{
    rb_print_node(tree, tree->root, 0, 'R');
}

/* Calculate black height */
int rb_black_height(struct rb_tree *tree, struct rb_node *node)
{
    if (node == tree->nil)
        return 0;
    
    int bh = rb_black_height(tree, node->left);
    if (node->color == BLACK)
        bh++;
    return bh;
}

/* Verify RB properties */
bool rb_verify_node(struct rb_tree *tree, struct rb_node *node, 
                    int black_count, int *path_bh)
{
    if (node == tree->nil) {
        if (*path_bh == -1)
            *path_bh = black_count;
        return black_count == *path_bh;
    }
    
    /* Check no consecutive reds */
    if (node->color == RED && node->parent->color == RED)
        return false;
    
    if (node->color == BLACK)
        black_count++;
    
    return rb_verify_node(tree, node->left, black_count, path_bh) &&
           rb_verify_node(tree, node->right, black_count, path_bh);
}

bool rb_verify(struct rb_tree *tree)
{
    if (tree->root == tree->nil)
        return true;
    
    /* Root must be black */
    if (tree->root->color != BLACK)
        return false;
    
    int path_bh = -1;
    return rb_verify_node(tree, tree->root, 0, &path_bh);
}

void rb_destroy_node(struct rb_tree *tree, struct rb_node *node)
{
    if (node == tree->nil)
        return;
    rb_destroy_node(tree, node->left);
    rb_destroy_node(tree, node->right);
    free(node);
}

void rb_destroy(struct rb_tree *tree)
{
    rb_destroy_node(tree, tree->root);
    free(tree->nil);
    tree->root = NULL;
    tree->nil = NULL;
    tree->size = 0;
}

int main(void)
{
    printf("=== Red-Black Tree Demo ===\n\n");
    
    struct rb_tree tree;
    rb_init(&tree);
    
    /* Insert in sorted order */
    printf("Inserting 1-10 in order:\n");
    for (int i = 1; i <= 10; i++) {
        rb_insert(&tree, i);
    }
    
    printf("\nTree structure (R=Red, B=Black):\n");
    rb_print(&tree);
    
    printf("\nIn-order: ");
    rb_inorder(&tree);
    printf("\n");
    
    printf("\nBlack height: %d\n", rb_black_height(&tree, tree.root));
    printf("RB properties valid: %s\n", rb_verify(&tree) ? "YES" : "NO");
    
    /* Search */
    printf("\nSearching for 5: %s\n",
           rb_search(&tree, 5) ? "FOUND" : "NOT FOUND");
    printf("Searching for 99: %s\n",
           rb_search(&tree, 99) ? "FOUND" : "NOT FOUND");
    
    rb_destroy(&tree);
    return 0;
}
```

---

### Example 3: Linux Kernel RB-Tree Usage Pattern

```c
/*
 * Example 3: Linux Kernel-Style RB-Tree Usage
 *
 * Demonstrates intrusive RB-tree pattern
 * Compile: gcc -Wall -Wextra -o rb_linux rb_linux.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

/* ═══════════════════════════════════════════════════════════════
 * Simplified Linux rb_node (from include/linux/rbtree.h)
 * ═══════════════════════════════════════════════════════════════ */

struct rb_node {
    unsigned long __rb_parent_color;  /* Parent pointer + color bit */
    struct rb_node *rb_right;
    struct rb_node *rb_left;
};

struct rb_root {
    struct rb_node *rb_node;
};

#define RB_RED   0
#define RB_BLACK 1

#define rb_parent(r)   ((struct rb_node *)((r)->__rb_parent_color & ~3))
#define rb_color(r)    ((r)->__rb_parent_color & 1)
#define rb_is_red(r)   (!rb_color(r))
#define rb_is_black(r) rb_color(r)

static inline void rb_set_parent_color(struct rb_node *rb,
                                       struct rb_node *p, int color)
{
    rb->__rb_parent_color = (unsigned long)p | color;
}

#define RB_ROOT_INIT { NULL }

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* ═══════════════════════════════════════════════════════════════
 * Example: Process scheduling (simplified CFS-style)
 * ═══════════════════════════════════════════════════════════════ */

struct task_struct {
    int pid;
    unsigned long vruntime;     /* Virtual runtime for scheduling */
    struct rb_node run_node;    /* Embedded RB node */
};

struct cfs_rq {
    struct rb_root tasks_timeline;
    struct rb_node *rb_leftmost;  /* Cache leftmost for O(1) min */
    unsigned int nr_running;
};

/* Initialize CFS runqueue */
void cfs_rq_init(struct cfs_rq *cfs)
{
    cfs->tasks_timeline.rb_node = NULL;
    cfs->rb_leftmost = NULL;
    cfs->nr_running = 0;
}

/* Simple insert (no rebalancing - just for demonstration) */
void enqueue_task(struct cfs_rq *cfs, struct task_struct *task)
{
    struct rb_node **link = &cfs->tasks_timeline.rb_node;
    struct rb_node *parent = NULL;
    int leftmost = 1;
    
    /* Find insertion point */
    while (*link) {
        parent = *link;
        struct task_struct *entry = container_of(parent, struct task_struct, run_node);
        
        if (task->vruntime < entry->vruntime) {
            link = &parent->rb_left;
        } else {
            link = &parent->rb_right;
            leftmost = 0;
        }
    }
    
    /* Insert (simplified - no rebalancing) */
    rb_set_parent_color(&task->run_node, parent, RB_RED);
    task->run_node.rb_left = NULL;
    task->run_node.rb_right = NULL;
    *link = &task->run_node;
    
    if (leftmost)
        cfs->rb_leftmost = &task->run_node;
    
    cfs->nr_running++;
}

/* Get task with minimum vruntime */
struct task_struct *pick_next_task(struct cfs_rq *cfs)
{
    if (!cfs->rb_leftmost)
        return NULL;
    
    return container_of(cfs->rb_leftmost, struct task_struct, run_node);
}

/* Create task */
struct task_struct *create_task(int pid, unsigned long vruntime)
{
    struct task_struct *task = malloc(sizeof(*task));
    if (task) {
        task->pid = pid;
        task->vruntime = vruntime;
    }
    return task;
}

/* Print tasks in vruntime order */
void print_inorder(struct rb_node *node, int level)
{
    if (!node)
        return;
    
    print_inorder(node->rb_left, level + 1);
    
    struct task_struct *task = container_of(node, struct task_struct, run_node);
    for (int i = 0; i < level; i++)
        printf("  ");
    printf("PID %d (vruntime=%lu)\n", task->pid, task->vruntime);
    
    print_inorder(node->rb_right, level + 1);
}

int main(void)
{
    printf("=== Linux Kernel RB-Tree Pattern ===\n\n");
    printf("Simulating CFS Scheduler Task Queue\n\n");
    
    struct cfs_rq cfs;
    cfs_rq_init(&cfs);
    
    /* Create tasks with different vruntimes */
    struct task_struct *tasks[5];
    unsigned long vruntimes[] = {100, 50, 150, 25, 75};
    
    for (int i = 0; i < 5; i++) {
        tasks[i] = create_task(1000 + i, vruntimes[i]);
        enqueue_task(&cfs, tasks[i]);
        printf("Enqueued PID %d with vruntime %lu\n",
               tasks[i]->pid, tasks[i]->vruntime);
    }
    
    printf("\nTask tree (in-order = sorted by vruntime):\n");
    print_inorder(cfs.tasks_timeline.rb_node, 0);
    
    struct task_struct *next = pick_next_task(&cfs);
    printf("\nNext task to run: PID %d (lowest vruntime = %lu)\n",
           next->pid, next->vruntime);
    
    printf("\n--- Key Insight ---\n");
    printf("The Linux CFS scheduler uses an RB-tree to order tasks\n");
    printf("by virtual runtime. The leftmost node (cached!) is the\n");
    printf("next task to run - O(1) lookup, O(log n) insert/remove.\n");
    
    /* Cleanup */
    for (int i = 0; i < 5; i++)
        free(tasks[i]);
    
    return 0;
}
```

---

### Example 4: Performance Comparison

```c
/*
 * Example 4: AVL vs RB-Tree Performance Comparison
 *
 * Measures insert/search times for both trees
 * Compile: gcc -Wall -Wextra -O2 -o tree_perf tree_perf.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * Simple BST (baseline)
 * ═══════════════════════════════════════════════════════════════ */
struct bst_node {
    int key;
    struct bst_node *left, *right;
};

struct bst_node *bst_insert(struct bst_node *node, int key)
{
    if (!node) {
        struct bst_node *n = malloc(sizeof(*n));
        n->key = key;
        n->left = n->right = NULL;
        return n;
    }
    if (key < node->key)
        node->left = bst_insert(node->left, key);
    else
        node->right = bst_insert(node->right, key);
    return node;
}

int bst_height(struct bst_node *node)
{
    if (!node) return 0;
    int lh = bst_height(node->left);
    int rh = bst_height(node->right);
    return 1 + (lh > rh ? lh : rh);
}

void bst_destroy(struct bst_node *node)
{
    if (!node) return;
    bst_destroy(node->left);
    bst_destroy(node->right);
    free(node);
}

/* ═══════════════════════════════════════════════════════════════
 * Timing utilities
 * ═══════════════════════════════════════════════════════════════ */
double time_diff_ms(struct timespec start, struct timespec end)
{
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_nsec - start.tv_nsec) / 1000000.0;
}

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
    printf("=== Tree Performance Comparison ===\n\n");
    
    srand((unsigned)time(NULL));
    
    const int N = 100000;
    int *sorted = malloc(N * sizeof(int));
    int *random = malloc(N * sizeof(int));
    
    for (int i = 0; i < N; i++) {
        sorted[i] = i;
        random[i] = i;
    }
    shuffle(random, N);
    
    struct timespec start, end;
    
    /* Test 1: Sorted insertion (worst case for BST) */
    printf("Inserting %d elements in SORTED order:\n", N);
    
    struct bst_node *bst = NULL;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < N; i++)
        bst = bst_insert(bst, sorted[i]);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    printf("  Plain BST: %.2f ms, height = %d\n",
           time_diff_ms(start, end), bst_height(bst));
    printf("    (This is O(n²) insertion - very slow!)\n");
    bst_destroy(bst);
    
    /* Test 2: Random insertion (average case) */
    printf("\nInserting %d elements in RANDOM order:\n", N);
    
    bst = NULL;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < N; i++)
        bst = bst_insert(bst, random[i]);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    printf("  Plain BST: %.2f ms, height = %d\n",
           time_diff_ms(start, end), bst_height(bst));
    printf("    (Random gives O(log n) average height)\n");
    printf("    (Ideal height for %d nodes: %d)\n", N, 17);  /* log2(100000) ≈ 17 */
    
    bst_destroy(bst);
    
    printf("\n--- Summary ---\n");
    printf("Sorted insertion: BST degenerates to O(n) height\n");
    printf("Random insertion: BST is O(log n) on average\n");
    printf("\nBalanced trees (AVL, RB) guarantee O(log n) in ALL cases!\n");
    printf("That's why the Linux kernel uses RB-trees everywhere.\n");
    
    free(sorted);
    free(random);
    
    return 0;
}
```

---

### Example 5: Conceptual AVL vs RB Visualization

```c
/*
 * Example 5: Visualize Balance Differences
 *
 * Shows how AVL is stricter than RB
 * Compile: gcc -Wall -Wextra -o balance_demo balance_demo.c
 */

#include <stdio.h>

int main(void)
{
    printf("=== AVL vs Red-Black Balance Comparison ===\n\n");
    
    printf("SAME DATA: Insert 1, 2, 3, 4, 5, 6, 7 in order\n\n");
    
    printf("PLAIN BST (degenerates):\n");
    printf("    1\n");
    printf("     \\\n");
    printf("      2\n");
    printf("       \\\n");
    printf("        3\n");
    printf("         \\\n");
    printf("          ... (height = 6)\n\n");
    
    printf("AVL TREE (strictly balanced):\n");
    printf("        4\n");
    printf("       / \\\n");
    printf("      2   6\n");
    printf("     / \\ / \\\n");
    printf("    1  3 5  7\n");
    printf("\n");
    printf("    Height = 2 (perfect!)\n");
    printf("    Balance factor: all 0\n\n");
    
    printf("RED-BLACK TREE (relaxed balance):\n");
    printf("        4(B)\n");
    printf("       /    \\\n");
    printf("    2(R)     6(R)\n");
    printf("    /  \\     /  \\\n");
    printf("  1(B) 3(B) 5(B) 7(B)\n");
    printf("\n");
    printf("    Height = 2 (same as AVL here)\n");
    printf("    But can be up to 2× AVL height in general\n\n");
    
    printf("─────────────────────────────────────────────────────\n");
    printf("KEY DIFFERENCES:\n");
    printf("─────────────────────────────────────────────────────\n");
    printf("\n");
    printf("AVL:\n");
    printf("  • Balance factor must be -1, 0, or +1\n");
    printf("  • Height ≤ 1.44 × log₂(n)\n");
    printf("  • More rotations on insert/delete\n");
    printf("  • Better for read-heavy workloads\n");
    printf("\n");
    printf("RED-BLACK:\n");
    printf("  • No red-red parent-child\n");
    printf("  • Same black-height on all paths\n");
    printf("  • Height ≤ 2 × log₂(n)\n");
    printf("  • At most 3 rotations per insert/delete\n");
    printf("  • Better for write-heavy workloads\n");
    printf("\n");
    printf("─────────────────────────────────────────────────────\n");
    printf("LINUX KERNEL CHOICE: Red-Black Tree\n");
    printf("─────────────────────────────────────────────────────\n");
    printf("\n");
    printf("Reasons:\n");
    printf("  1. Bounded rotations (predictable worst case)\n");
    printf("  2. Good enough search (2× height isn't bad)\n");
    printf("  3. Simpler amortized analysis\n");
    printf("  4. Works well for mixed read/write\n");
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

```
+------------------------------------------------------------------+
|  BALANCED TREE COMPARISON                                        |
+------------------------------------------------------------------+

    ┌────────────────────┬───────────────────┬──────────────────────┐
    │ Metric             │ AVL Tree          │ Red-Black Tree       │
    ├────────────────────┼───────────────────┼──────────────────────┤
    │ Height bound       │ 1.44 log n        │ 2 log n              │
    │ Search             │ Slightly faster   │ Good                 │
    │ Insert rotations   │ ≤ 2               │ ≤ 2                  │
    │ Delete rotations   │ O(log n)          │ ≤ 3                  │
    │ Memory/node        │ +height field     │ +color bit           │
    │ Implementation     │ Simpler to reason │ More cases           │
    │ Real-world use     │ Databases         │ OS kernels, libs     │
    └────────────────────┴───────────────────┴──────────────────────┘

    WHEN TO USE AVL:
    - Read-heavy workloads (search much more than insert/delete)
    - Need tightest possible height bound
    - Educational purposes (easier to verify)

    WHEN TO USE RED-BLACK:
    - Write-heavy workloads
    - Need bounded worst-case insert/delete
    - Following established patterns (Linux, STL)
    - General purpose
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|  BALANCED TREES: KEY TAKEAWAYS                                   |
+------------------------------------------------------------------+

    THE PROBLEM:
    Plain BST degenerates to O(n) with sorted input

    THE SOLUTION:
    Self-balancing trees guarantee O(log n) height

    TWO MAIN APPROACHES:
    - AVL: strict balance (±1), faster search, more rotations
    - RB:  relaxed balance, bounded rotations, widely used

    KEY INSIGHT:
    Both use ROTATIONS to restore balance after insert/delete
    Rotations are O(1) and maintain BST property

    IN PRACTICE:
    Red-Black tree dominates (Linux kernel, C++ STL, Java)
    because bounded rotations give predictable performance

    LINUX KERNEL:
    Uses RB-tree for VM areas, scheduler, I/O, epoll, etc.
    See: include/linux/rbtree.h
```

**中文总结：**
- **问题**：普通 BST 在顺序输入时退化为 O(n)
- **解决方案**：自平衡树保证 O(log n) 高度
- **两种方法**：AVL（严格平衡，搜索快）vs 红黑树（宽松平衡，旋转少）
- **实际应用**：红黑树占主导（Linux 内核、C++ STL、Java）
- **关键洞察**：旋转是 O(1) 操作，保持 BST 性质

