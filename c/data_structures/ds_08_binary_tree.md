# Binary Tree in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE BINARY TREE: HIERARCHICAL DATA ORGANIZATION                 |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Need to represent hierarchical relationships:              │
    │  - Expression trees (parse trees)                           │
    │  - Decision trees                                           │
    │  - File system structure                                    │
    │  - Organization charts                                      │
    │  - Recursive decomposition                                  │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: BINARY TREE
    ┌─────────────────────────────────────────────────────────────┐
    │  Each node has at most TWO children (left, right)           │
    │  One node is the ROOT (entry point)                         │
    │  Naturally supports recursive operations                    │
    └─────────────────────────────────────────────────────────────┘

    VISUAL:
                    ┌───┐
                    │ A │  ← ROOT
                    └───┘
                   /     \
                ┌───┐   ┌───┐
                │ B │   │ C │
                └───┘   └───┘
               /    \       \
            ┌───┐ ┌───┐   ┌───┐
            │ D │ │ E │   │ F │
            └───┘ └───┘   └───┘

    TERMINOLOGY:
    - Root: A (top node, no parent)
    - Leaf: D, E, F (no children)
    - Internal node: B, C (has children)
    - Height: 2 (longest path from root to leaf)
    - Depth of E: 2 (edges from root)
```

**中文解释：**
- **二叉树**：每个节点最多有两个子节点（左、右）
- 解决层次关系表示问题：表达式树、决策树、文件系统
- 天然支持递归操作

### Types of Binary Trees

```
+------------------------------------------------------------------+
|  BINARY TREE VARIANTS                                            |
+------------------------------------------------------------------+

    FULL BINARY TREE:
    Every node has 0 or 2 children (never 1)
              ┌───┐
              │ A │
              └───┘
             /     \
          ┌───┐   ┌───┐
          │ B │   │ C │
          └───┘   └───┘
         /    \
      ┌───┐ ┌───┐
      │ D │ │ E │
      └───┘ └───┘

    COMPLETE BINARY TREE:
    All levels filled except possibly last, filled left-to-right
              ┌───┐
              │ A │
              └───┘
             /     \
          ┌───┐   ┌───┐
          │ B │   │ C │
          └───┘   └───┘
         /    \   /
      ┌───┐ ┌───┐ ┌───┐
      │ D │ │ E │ │ F │
      └───┘ └───┘ └───┘

    PERFECT BINARY TREE:
    All internal nodes have 2 children, all leaves same depth
              ┌───┐
              │ A │
              └───┘
             /     \
          ┌───┐   ┌───┐
          │ B │   │ C │
          └───┘   └───┘
         /    \   /    \
      ┌───┐ ┌───┐ ┌───┐ ┌───┐
      │ D │ │ E │ │ F │ │ G │
      └───┘ └───┘ └───┘ └───┘
```

### Invariants

```
+------------------------------------------------------------------+
|  BINARY TREE INVARIANTS                                          |
+------------------------------------------------------------------+

    1. AT MOST TWO CHILDREN
       Each node has left and/or right child, or none

    2. SINGLE ROOT
       Exactly one node has no parent

    3. NO CYCLES
       Following parent pointers always reaches root
       Following child pointers eventually reaches NULL

    4. UNIQUE PATH
       Exactly one path from root to any node
```

---

## 2. Memory Model

### Node Structure

```
+------------------------------------------------------------------+
|  BINARY TREE NODE LAYOUT                                         |
+------------------------------------------------------------------+

    struct tree_node {
        int data;                    /*  4 bytes */
        struct tree_node *left;      /*  8 bytes (64-bit) */
        struct tree_node *right;     /*  8 bytes */
    };                               /* 24 bytes total (with padding) */

    MEMORY LAYOUT:
    ┌────────────────────────────────────────┐
    │ data (4) │ pad (4) │ left (8) │ right (8) │
    └────────────────────────────────────────┘
         0         4          8          16

    WITH PARENT POINTER:
    struct tree_node {
        int data;
        struct tree_node *parent;    /* For traversal up */
        struct tree_node *left;
        struct tree_node *right;
    };  /* 32 bytes */
```

### Tree Memory Layout

```
+------------------------------------------------------------------+
|  TREE IN MEMORY                                                  |
+------------------------------------------------------------------+

    Nodes scattered across heap:
    
    Address 0x1000:              Address 0x2000:
    ┌─────────────────────┐      ┌─────────────────────┐
    │ data = 10           │      │ data = 5            │
    │ left  = 0x2000 ─────┼─────▶│ left  = 0x3000      │
    │ right = 0x4000      │      │ right = NULL        │
    └─────────────────────┘      └─────────────────────┘
            │                           │
            │                           ▼
            │                    Address 0x3000:
            │                    ┌─────────────────────┐
            │                    │ data = 3            │
            │                    │ left  = NULL        │
            │                    │ right = NULL        │
            │                    └─────────────────────┘
            ▼
    Address 0x4000:
    ┌─────────────────────┐
    │ data = 15           │
    │ left  = NULL        │
    │ right = 0x5000      │
    └─────────────────────┘

    Logical tree:
              10
             /  \
            5    15
           /       \
          3        (20)
```

### Cache Behavior

```
+------------------------------------------------------------------+
|  BINARY TREE CACHE BEHAVIOR                                      |
+------------------------------------------------------------------+

    PROBLEM: POOR LOCALITY
    ┌─────────────────────────────────────────────────────────────┐
    │  • Nodes allocated at random heap addresses                 │
    │  • Traversing tree = jumping to random memory               │
    │  • Each node access = potential cache miss                  │
    │  • Worst case: O(n) cache misses for n nodes               │
    └─────────────────────────────────────────────────────────────┘

    MITIGATION STRATEGIES:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. Node pools: allocate nodes from contiguous pool         │
    │  2. Array representation: for complete trees                │
    │  3. B-trees: more children per node, shallower tree         │
    │  4. Van Emde Boas layout: cache-optimal arrangement         │
    └─────────────────────────────────────────────────────────────┘
```

### Array Representation (Complete Binary Tree)

```
+------------------------------------------------------------------+
|  ARRAY REPRESENTATION FOR COMPLETE BINARY TREE                   |
+------------------------------------------------------------------+

    Tree:
              1
             / \
            2   3
           / \ / \
          4  5 6  7

    Array (0-indexed):
    ┌───┬───┬───┬───┬───┬───┬───┐
    │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │
    └───┴───┴───┴───┴───┴───┴───┘
     [0] [1] [2] [3] [4] [5] [6]

    INDEX FORMULAS:
    ┌─────────────────────────────────────────────────────────────┐
    │  Parent of i:      (i - 1) / 2                              │
    │  Left child of i:  2 * i + 1                                │
    │  Right child of i: 2 * i + 2                                │
    └─────────────────────────────────────────────────────────────┘

    ADVANTAGES:
    - No pointer overhead (saves 16 bytes per node)
    - Cache-friendly (contiguous memory)
    - Used in heaps (priority queues)

    DISADVANTAGES:
    - Only works for complete trees
    - Insertions may require reallocation
```

---

## 3. Typical Application Scenarios

### Where Binary Trees Are Used

```
+------------------------------------------------------------------+
|  BINARY TREE APPLICATIONS                                        |
+------------------------------------------------------------------+

    EXPRESSION TREES:
    ┌─────────────────────────────────────────────────────────────┐
    │  Representing mathematical expressions:                     │
    │       +                                                     │
    │      / \         represents: (3 + 4) * 5                    │
    │     *   5                                                   │
    │    / \                                                      │
    │   3   4                                                     │
    └─────────────────────────────────────────────────────────────┘

    DECISION TREES:
    ┌─────────────────────────────────────────────────────────────┐
    │  Binary decisions at each node:                             │
    │     age > 30?                                               │
    │      /    \                                                 │
    │    yes    no                                                │
    │    /        \                                               │
    │  employed?   student?                                       │
    └─────────────────────────────────────────────────────────────┘

    HUFFMAN CODING:
    ┌─────────────────────────────────────────────────────────────┐
    │  Data compression using variable-length codes               │
    │  Left = 0, Right = 1                                        │
    │  Characters at leaves                                       │
    └─────────────────────────────────────────────────────────────┘

    HEAP (Priority Queue):
    ┌─────────────────────────────────────────────────────────────┐
    │  Complete binary tree with heap property                    │
    │  Parent ≥ children (max-heap) or ≤ (min-heap)              │
    │  O(log n) insert, O(log n) extract-min/max                  │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Complete C Examples

### Example 1: Basic Binary Tree Operations

```c
/*
 * Example 1: Basic Binary Tree with Traversals
 *
 * Demonstrates tree creation and three traversal orders
 * Compile: gcc -Wall -Wextra -o btree_basic btree_basic.c
 */

#include <stdio.h>
#include <stdlib.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

/* Create a new node */
struct tree_node *node_create(int data)
{
    struct tree_node *node = malloc(sizeof(*node));
    if (!node)
        return NULL;
    
    node->data = data;
    node->left = NULL;
    node->right = NULL;
    
    return node;
}

/* In-order traversal: Left, Root, Right */
void inorder(struct tree_node *node)
{
    if (!node)
        return;
    
    inorder(node->left);
    printf("%d ", node->data);
    inorder(node->right);
}

/* Pre-order traversal: Root, Left, Right */
void preorder(struct tree_node *node)
{
    if (!node)
        return;
    
    printf("%d ", node->data);
    preorder(node->left);
    preorder(node->right);
}

/* Post-order traversal: Left, Right, Root */
void postorder(struct tree_node *node)
{
    if (!node)
        return;
    
    postorder(node->left);
    postorder(node->right);
    printf("%d ", node->data);
}

/* Calculate tree height */
int tree_height(struct tree_node *node)
{
    if (!node)
        return -1;  /* Empty tree has height -1 */
    
    int left_h = tree_height(node->left);
    int right_h = tree_height(node->right);
    
    return 1 + (left_h > right_h ? left_h : right_h);
}

/* Count nodes */
int tree_size(struct tree_node *node)
{
    if (!node)
        return 0;
    
    return 1 + tree_size(node->left) + tree_size(node->right);
}

/* Free entire tree (post-order) */
void tree_destroy(struct tree_node *node)
{
    if (!node)
        return;
    
    tree_destroy(node->left);
    tree_destroy(node->right);
    free(node);
}

/* Print tree structure (rotated 90 degrees) */
void tree_print(struct tree_node *node, int level)
{
    if (!node)
        return;
    
    tree_print(node->right, level + 1);
    
    for (int i = 0; i < level; i++)
        printf("    ");
    printf("%d\n", node->data);
    
    tree_print(node->left, level + 1);
}

int main(void)
{
    printf("=== Basic Binary Tree Demo ===\n\n");
    
    /*
     * Build this tree:
     *         1
     *        / \
     *       2   3
     *      / \   \
     *     4   5   6
     */
    struct tree_node *root = node_create(1);
    root->left = node_create(2);
    root->right = node_create(3);
    root->left->left = node_create(4);
    root->left->right = node_create(5);
    root->right->right = node_create(6);
    
    printf("Tree structure:\n");
    tree_print(root, 0);
    
    printf("\nTraversals:\n");
    printf("  In-order (L,R,N):   ");
    inorder(root);
    printf("\n");
    
    printf("  Pre-order (N,L,R):  ");
    preorder(root);
    printf("\n");
    
    printf("  Post-order (L,R,N): ");
    postorder(root);
    printf("\n");
    
    printf("\nTree properties:\n");
    printf("  Height: %d\n", tree_height(root));
    printf("  Size:   %d nodes\n", tree_size(root));
    
    tree_destroy(root);
    return 0;
}
```

---

### Example 2: Iterative Traversal with Explicit Stack

```c
/*
 * Example 2: Iterative Traversal Using Explicit Stack
 *
 * Avoids recursion (useful when stack depth is a concern)
 * Compile: gcc -Wall -Wextra -o btree_iterative btree_iterative.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

/* Stack for iterative traversal */
#define STACK_MAX 100

struct node_stack {
    struct tree_node *items[STACK_MAX];
    int top;
};

void stack_init(struct node_stack *s) { s->top = -1; }
bool stack_empty(struct node_stack *s) { return s->top == -1; }
void stack_push(struct node_stack *s, struct tree_node *n) { s->items[++s->top] = n; }
struct tree_node *stack_pop(struct node_stack *s) { return s->items[s->top--]; }
struct tree_node *stack_peek(struct node_stack *s) { return s->items[s->top]; }

/* Create node */
struct tree_node *node_create(int data)
{
    struct tree_node *n = malloc(sizeof(*n));
    if (n) {
        n->data = data;
        n->left = n->right = NULL;
    }
    return n;
}

/* Iterative in-order traversal */
void inorder_iterative(struct tree_node *root)
{
    struct node_stack stack;
    stack_init(&stack);
    
    struct tree_node *current = root;
    
    printf("In-order (iterative): ");
    
    while (current || !stack_empty(&stack)) {
        /* Go to leftmost node */
        while (current) {
            stack_push(&stack, current);
            current = current->left;
        }
        
        /* Process current, then go right */
        current = stack_pop(&stack);
        printf("%d ", current->data);
        current = current->right;
    }
    
    printf("\n");
}

/* Iterative pre-order traversal */
void preorder_iterative(struct tree_node *root)
{
    if (!root)
        return;
    
    struct node_stack stack;
    stack_init(&stack);
    stack_push(&stack, root);
    
    printf("Pre-order (iterative): ");
    
    while (!stack_empty(&stack)) {
        struct tree_node *node = stack_pop(&stack);
        printf("%d ", node->data);
        
        /* Push right first (processed last) */
        if (node->right)
            stack_push(&stack, node->right);
        if (node->left)
            stack_push(&stack, node->left);
    }
    
    printf("\n");
}

/* Iterative post-order traversal (using two stacks) */
void postorder_iterative(struct tree_node *root)
{
    if (!root)
        return;
    
    struct node_stack stack1, stack2;
    stack_init(&stack1);
    stack_init(&stack2);
    
    stack_push(&stack1, root);
    
    while (!stack_empty(&stack1)) {
        struct tree_node *node = stack_pop(&stack1);
        stack_push(&stack2, node);
        
        if (node->left)
            stack_push(&stack1, node->left);
        if (node->right)
            stack_push(&stack1, node->right);
    }
    
    printf("Post-order (iterative): ");
    while (!stack_empty(&stack2)) {
        printf("%d ", stack_pop(&stack2)->data);
    }
    printf("\n");
}

/* Level-order traversal (BFS using queue) */
void levelorder(struct tree_node *root)
{
    if (!root)
        return;
    
    /* Simple array queue */
    struct tree_node *queue[STACK_MAX];
    int front = 0, rear = 0;
    
    queue[rear++] = root;
    
    printf("Level-order (BFS): ");
    
    while (front < rear) {
        struct tree_node *node = queue[front++];
        printf("%d ", node->data);
        
        if (node->left)
            queue[rear++] = node->left;
        if (node->right)
            queue[rear++] = node->right;
    }
    
    printf("\n");
}

void tree_destroy(struct tree_node *node)
{
    if (!node) return;
    tree_destroy(node->left);
    tree_destroy(node->right);
    free(node);
}

int main(void)
{
    printf("=== Iterative Tree Traversals ===\n\n");
    
    /*
     *         1
     *        / \
     *       2   3
     *      / \   \
     *     4   5   6
     */
    struct tree_node *root = node_create(1);
    root->left = node_create(2);
    root->right = node_create(3);
    root->left->left = node_create(4);
    root->left->right = node_create(5);
    root->right->right = node_create(6);
    
    inorder_iterative(root);
    preorder_iterative(root);
    postorder_iterative(root);
    levelorder(root);
    
    tree_destroy(root);
    return 0;
}
```

---

### Example 3: Expression Tree Evaluator

```c
/*
 * Example 3: Expression Tree Evaluator
 *
 * Parse and evaluate arithmetic expressions
 * Compile: gcc -Wall -Wextra -o expr_tree expr_tree.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

enum node_type {
    NODE_NUMBER,
    NODE_OPERATOR
};

struct expr_node {
    enum node_type type;
    union {
        int number;
        char operator;
    } value;
    struct expr_node *left;
    struct expr_node *right;
};

/* Create number node */
struct expr_node *node_number(int num)
{
    struct expr_node *node = malloc(sizeof(*node));
    if (node) {
        node->type = NODE_NUMBER;
        node->value.number = num;
        node->left = node->right = NULL;
    }
    return node;
}

/* Create operator node */
struct expr_node *node_operator(char op, struct expr_node *left, struct expr_node *right)
{
    struct expr_node *node = malloc(sizeof(*node));
    if (node) {
        node->type = NODE_OPERATOR;
        node->value.operator = op;
        node->left = left;
        node->right = right;
    }
    return node;
}

/* Evaluate expression tree */
int evaluate(struct expr_node *node)
{
    if (!node)
        return 0;
    
    if (node->type == NODE_NUMBER)
        return node->value.number;
    
    int left_val = evaluate(node->left);
    int right_val = evaluate(node->right);
    
    switch (node->value.operator) {
    case '+': return left_val + right_val;
    case '-': return left_val - right_val;
    case '*': return left_val * right_val;
    case '/': return right_val ? left_val / right_val : 0;
    default:  return 0;
    }
}

/* Print expression (infix with parentheses) */
void print_infix(struct expr_node *node)
{
    if (!node)
        return;
    
    if (node->type == NODE_NUMBER) {
        printf("%d", node->value.number);
        return;
    }
    
    printf("(");
    print_infix(node->left);
    printf(" %c ", node->value.operator);
    print_infix(node->right);
    printf(")");
}

/* Print expression (postfix / RPN) */
void print_postfix(struct expr_node *node)
{
    if (!node)
        return;
    
    if (node->type == NODE_NUMBER) {
        printf("%d ", node->value.number);
        return;
    }
    
    print_postfix(node->left);
    print_postfix(node->right);
    printf("%c ", node->value.operator);
}

void tree_destroy(struct expr_node *node)
{
    if (!node) return;
    tree_destroy(node->left);
    tree_destroy(node->right);
    free(node);
}

int main(void)
{
    printf("=== Expression Tree Evaluator ===\n\n");
    
    /*
     * Build expression: (3 + 4) * (10 - 5)
     *
     *          *
     *         / \
     *        +   -
     *       / \ / \
     *      3  4 10 5
     */
    struct expr_node *plus = node_operator('+',
                                           node_number(3),
                                           node_number(4));
    
    struct expr_node *minus = node_operator('-',
                                            node_number(10),
                                            node_number(5));
    
    struct expr_node *root = node_operator('*', plus, minus);
    
    printf("Expression (infix):   ");
    print_infix(root);
    printf("\n");
    
    printf("Expression (postfix): ");
    print_postfix(root);
    printf("\n");
    
    printf("Result: %d\n", evaluate(root));
    printf("Expected: (3 + 4) * (10 - 5) = 7 * 5 = 35\n");
    
    tree_destroy(root);
    
    /* Build another: 2 + 3 * 4 (respecting precedence) */
    printf("\n--- Another expression ---\n");
    
    struct expr_node *mult = node_operator('*',
                                           node_number(3),
                                           node_number(4));
    root = node_operator('+', node_number(2), mult);
    
    printf("Expression: ");
    print_infix(root);
    printf("\n");
    printf("Result: %d\n", evaluate(root));
    printf("Expected: 2 + (3 * 4) = 2 + 12 = 14\n");
    
    tree_destroy(root);
    return 0;
}
```

---

### Example 4: Binary Heap (Priority Queue)

```c
/*
 * Example 4: Binary Heap (Min-Heap as Priority Queue)
 *
 * Complete binary tree stored in array
 * Compile: gcc -Wall -Wextra -o heap heap.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct min_heap {
    int *data;
    size_t size;
    size_t capacity;
};

int heap_init(struct min_heap *h, size_t capacity)
{
    h->data = malloc(capacity * sizeof(int));
    if (!h->data)
        return -1;
    
    h->size = 0;
    h->capacity = capacity;
    return 0;
}

void heap_destroy(struct min_heap *h)
{
    free(h->data);
}

static inline size_t parent(size_t i) { return (i - 1) / 2; }
static inline size_t left_child(size_t i) { return 2 * i + 1; }
static inline size_t right_child(size_t i) { return 2 * i + 2; }

static void swap(int *a, int *b)
{
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Bubble up after insertion */
static void heapify_up(struct min_heap *h, size_t i)
{
    while (i > 0 && h->data[parent(i)] > h->data[i]) {
        swap(&h->data[parent(i)], &h->data[i]);
        i = parent(i);
    }
}

/* Bubble down after extraction */
static void heapify_down(struct min_heap *h, size_t i)
{
    size_t smallest = i;
    size_t left = left_child(i);
    size_t right = right_child(i);
    
    if (left < h->size && h->data[left] < h->data[smallest])
        smallest = left;
    
    if (right < h->size && h->data[right] < h->data[smallest])
        smallest = right;
    
    if (smallest != i) {
        swap(&h->data[i], &h->data[smallest]);
        heapify_down(h, smallest);
    }
}

/* Insert element */
bool heap_insert(struct min_heap *h, int value)
{
    if (h->size >= h->capacity)
        return false;
    
    h->data[h->size] = value;
    heapify_up(h, h->size);
    h->size++;
    
    return true;
}

/* Extract minimum */
bool heap_extract_min(struct min_heap *h, int *value)
{
    if (h->size == 0)
        return false;
    
    *value = h->data[0];
    h->data[0] = h->data[h->size - 1];
    h->size--;
    
    if (h->size > 0)
        heapify_down(h, 0);
    
    return true;
}

/* Peek minimum without removing */
bool heap_peek(const struct min_heap *h, int *value)
{
    if (h->size == 0)
        return false;
    
    *value = h->data[0];
    return true;
}

/* Print heap as array */
void heap_print(const struct min_heap *h)
{
    printf("Heap [%zu]: ", h->size);
    for (size_t i = 0; i < h->size; i++)
        printf("%d ", h->data[i]);
    printf("\n");
}

/* Print heap as tree */
void heap_print_tree(const struct min_heap *h)
{
    if (h->size == 0) {
        printf("(empty)\n");
        return;
    }
    
    size_t level_size = 1;
    size_t i = 0;
    
    while (i < h->size) {
        for (size_t j = 0; j < level_size && i < h->size; j++, i++) {
            printf("%d ", h->data[i]);
        }
        printf("\n");
        level_size *= 2;
    }
}

int main(void)
{
    printf("=== Min-Heap Priority Queue Demo ===\n\n");
    
    struct min_heap h;
    heap_init(&h, 16);
    
    /* Insert elements */
    int values[] = {15, 10, 20, 17, 8, 25, 12, 5, 30};
    int n = sizeof(values) / sizeof(values[0]);
    
    printf("Inserting: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", values[i]);
        heap_insert(&h, values[i]);
    }
    printf("\n\n");
    
    printf("Heap as array: ");
    heap_print(&h);
    
    printf("\nHeap as tree:\n");
    heap_print_tree(&h);
    
    /* Extract all in sorted order */
    printf("\nExtracting in priority order:\n");
    int val;
    while (heap_extract_min(&h, &val)) {
        printf("  Extracted: %d\n", val);
    }
    
    heap_destroy(&h);
    return 0;
}
```

---

### Example 5: Threaded Binary Tree (Morris Traversal)

```c
/*
 * Example 5: Morris Traversal (O(1) Space In-Order)
 *
 * Traverses tree without stack or recursion using threading
 * Compile: gcc -Wall -Wextra -o morris morris.c
 */

#include <stdio.h>
#include <stdlib.h>

struct tree_node {
    int data;
    struct tree_node *left;
    struct tree_node *right;
};

struct tree_node *node_create(int data)
{
    struct tree_node *n = malloc(sizeof(*n));
    if (n) {
        n->data = data;
        n->left = n->right = NULL;
    }
    return n;
}

/*
 * Morris In-Order Traversal
 *
 * Key insight: Use right pointers of predecessors to create
 * temporary "threads" back to successors
 *
 * Space: O(1) - no stack needed!
 * Time: O(n) - each edge traversed at most 3 times
 */
void morris_inorder(struct tree_node *root)
{
    struct tree_node *current = root;
    
    printf("Morris in-order: ");
    
    while (current) {
        if (!current->left) {
            /* No left subtree, visit current and go right */
            printf("%d ", current->data);
            current = current->right;
        } else {
            /* Find in-order predecessor (rightmost in left subtree) */
            struct tree_node *predecessor = current->left;
            while (predecessor->right && predecessor->right != current) {
                predecessor = predecessor->right;
            }
            
            if (!predecessor->right) {
                /* Make thread: predecessor.right -> current */
                predecessor->right = current;
                current = current->left;
            } else {
                /* Thread exists, remove it and visit current */
                predecessor->right = NULL;  /* Restore tree */
                printf("%d ", current->data);
                current = current->right;
            }
        }
    }
    
    printf("\n");
}

/* Standard recursive for comparison */
void inorder_recursive(struct tree_node *node)
{
    if (!node) return;
    inorder_recursive(node->left);
    printf("%d ", node->data);
    inorder_recursive(node->right);
}

void tree_destroy(struct tree_node *node)
{
    if (!node) return;
    tree_destroy(node->left);
    tree_destroy(node->right);
    free(node);
}

int main(void)
{
    printf("=== Morris Traversal (O(1) Space) ===\n\n");
    
    /*
     * Build tree:
     *         4
     *        / \
     *       2   6
     *      / \ / \
     *     1  3 5  7
     */
    struct tree_node *root = node_create(4);
    root->left = node_create(2);
    root->right = node_create(6);
    root->left->left = node_create(1);
    root->left->right = node_create(3);
    root->right->left = node_create(5);
    root->right->right = node_create(7);
    
    printf("Recursive in-order:  ");
    inorder_recursive(root);
    printf("\n");
    
    morris_inorder(root);
    
    printf("\nKey insight: Morris traversal uses O(1) extra space\n");
    printf("by temporarily modifying tree structure (threading).\n");
    
    tree_destroy(root);
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

```
+------------------------------------------------------------------+
|  BINARY TREE TRADE-OFFS                                          |
+------------------------------------------------------------------+

    POINTER-BASED vs ARRAY-BASED:
    ┌────────────────────┬────────────────────┬───────────────────┐
    │ Aspect             │ Pointer-based      │ Array-based       │
    ├────────────────────┼────────────────────┼───────────────────┤
    │ Memory overhead    │ 16 bytes/node      │ 0 extra           │
    │ Cache behavior     │ Poor               │ Excellent         │
    │ Insertion          │ O(1) after finding │ May need resize   │
    │ Deletion           │ Complex            │ Complex           │
    │ Flexibility        │ Any shape          │ Complete only     │
    │ Use case           │ General trees      │ Heaps             │
    └────────────────────┴────────────────────┴───────────────────┘

    TRAVERSAL METHODS:
    ┌────────────────────┬─────────┬───────────────────────────────┐
    │ Method             │ Space   │ Notes                         │
    ├────────────────────┼─────────┼───────────────────────────────┤
    │ Recursive          │ O(h)    │ Simple, risk of stack overflow│
    │ Iterative w/ stack │ O(h)    │ Explicit control              │
    │ Morris (threaded)  │ O(1)    │ Modifies tree temporarily     │
    │ Parent pointers    │ O(1)    │ Extra pointer per node        │
    └────────────────────┴─────────┴───────────────────────────────┘
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|  BINARY TREE: KEY TAKEAWAYS                                      |
+------------------------------------------------------------------+

    CORE CONCEPT:
    Hierarchical structure with at most 2 children per node
    Foundation for BSTs, heaps, expression trees

    MEMORY:
    - Pointer-based: 24+ bytes per node, poor cache locality
    - Array-based: cache-friendly, but only for complete trees

    TRAVERSALS:
    - In-order: Left, Node, Right (sorted for BST)
    - Pre-order: Node, Left, Right (copy tree)
    - Post-order: Left, Right, Node (delete tree)
    - Level-order: BFS (breadth-first)

    USE WHEN:
    - Hierarchical data (expressions, decisions)
    - Foundation for search trees (BST, RB-tree)
    - Priority queues (heap)

    AVOID WHEN:
    - Linear data (use array/list)
    - Need fast random access
    - Memory-constrained (high pointer overhead)
```

**中文总结：**
- **核心概念**：每个节点最多两个子节点的层次结构
- **内存**：指针实现缓存不友好，数组实现只适用于完全二叉树
- **遍历**：中序、前序、后序、层序
- **用途**：表达式树、决策树、堆、搜索树基础

