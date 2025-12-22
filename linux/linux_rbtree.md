# Linux Kernel Intrusive Red-Black Trees (v3.2)

## Overview

This document explains **intrusive red-black tree usage** in Linux kernel v3.2, focusing on embedding, ordering invariants, and kernel usage patterns.

---

## rb_tree Design

From `include/linux/rbtree.h`:

```c
struct rb_node {
    unsigned long  rb_parent_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));

struct rb_root {
    struct rb_node *rb_node;
};

#define RB_ROOT (struct rb_root) { NULL, }
```

```
+------------------------------------------------------------------+
|  RED-BLACK TREE PROPERTIES                                       |
+------------------------------------------------------------------+

    BALANCING INVARIANTS:
    +----------------------------------------------------------+
    | 1. Every node is RED or BLACK                             |
    | 2. Root is BLACK                                          |
    | 3. Every leaf (NULL) is BLACK                             |
    | 4. RED node has only BLACK children                       |
    | 5. All paths from node to leaves have same BLACK count    |
    +----------------------------------------------------------+
    
    CONSEQUENCE:
    +----------------------------------------------------------+
    | Longest path ≤ 2 × shortest path                          |
    | Operations: O(log n) guaranteed                           |
    +----------------------------------------------------------+

    TREE STRUCTURE:
    
                       ┌───────────────┐
                       │   Root (B)    │
                       │   key: 50     │
                       └───────┬───────┘
                    ┌──────────┴──────────┐
                    ▼                     ▼
             ┌───────────┐         ┌───────────┐
             │ Node (R)  │         │ Node (R)  │
             │ key: 25   │         │ key: 75   │
             └─────┬─────┘         └─────┬─────┘
              ┌────┴────┐           ┌────┴────┐
              ▼         ▼           ▼         ▼
         ┌───────┐ ┌───────┐  ┌───────┐ ┌───────┐
         │  (B)  │ │  (B)  │  │  (B)  │ │  (B)  │
         │  10   │ │  30   │  │  60   │ │  90   │
         └───────┘ └───────┘  └───────┘ └───────┘
    
    (R) = Red, (B) = Black
```

**中文解释：**
- 红黑树性质：节点是红或黑、根是黑、红节点子节点是黑、所有路径黑节点数相同
- 结果：最长路径 ≤ 2 × 最短路径，操作 O(log n) 保证

---

## Embedding rb_node

```
+------------------------------------------------------------------+
|  INTRUSIVE RB_NODE EMBEDDING                                     |
+------------------------------------------------------------------+

    NON-INTRUSIVE (typical C++ style):
    
    struct rb_node {
        void *data;          /* Pointer to actual data */
        struct rb_node *left, *right;
    };
    
    Problem: Extra allocation, pointer indirection, cache miss

    INTRUSIVE (Linux kernel style):
    
    struct my_object {
        int key;
        char data[100];
        struct rb_node rb;   /* Embedded directly! */
    };
    
    Benefits:
    - No separate allocation for node
    - Data and node are cache-adjacent
    - container_of recovers object from node

    MEMORY LAYOUT:
    
    ┌─────────────────────────────────────────────────────────────┐
    │                    struct my_object                          │
    │  ┌──────────┬─────────────────────────────┬───────────────┐ │
    │  │  int key │     char data[100]          │ struct rb_node│ │
    │  │  (4B)    │     (100B)                  │   (24B)       │ │
    │  └──────────┴─────────────────────────────┴───────────────┘ │
    │                                                 ▲            │
    │                                                 │            │
    │                         Tree operations use this pointer    │
    │                         container_of() recovers whole object│
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 非侵入式：需要额外分配、指针间接、缓存未命中
- 侵入式：直接嵌入节点、数据和节点缓存相邻、用 container_of 恢复对象
- 布局：对象包含 rb_node，树操作使用 rb_node 指针

---

## Tree Operations

```c
/* Lookup (user provides comparison) */
struct my_object *my_search(struct rb_root *root, int key)
{
    struct rb_node *node = root->rb_node;
    
    while (node) {
        struct my_object *obj = rb_entry(node, struct my_object, rb);
        
        if (key < obj->key)
            node = node->rb_left;
        else if (key > obj->key)
            node = node->rb_right;
        else
            return obj;  /* Found */
    }
    return NULL;  /* Not found */
}

/* Insert (user provides comparison and position) */
int my_insert(struct rb_root *root, struct my_object *new)
{
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;
    
    /* Find the right place */
    while (*link) {
        struct my_object *obj = rb_entry(*link, struct my_object, rb);
        parent = *link;
        
        if (new->key < obj->key)
            link = &parent->rb_left;
        else if (new->key > obj->key)
            link = &parent->rb_right;
        else
            return -1;  /* Duplicate key */
    }
    
    /* Add new node and rebalance */
    rb_link_node(&new->rb, parent, link);
    rb_insert_color(&new->rb, root);
    return 0;
}

/* Delete */
void my_delete(struct rb_root *root, struct my_object *obj)
{
    rb_erase(&obj->rb, root);
    /* Object still exists, caller frees it */
}
```

```
+------------------------------------------------------------------+
|  KEY INSIGHT: User Defines Ordering                              |
+------------------------------------------------------------------+

    The kernel's rbtree does NOT know about keys!
    
    - rb_insert_color() only rebalances
    - User code finds insertion point
    - User code defines comparison
    
    WHY:
    +----------------------------------------------------------+
    | - Works with any key type                                 |
    | - Works with compound keys                                |
    | - No virtual function overhead                            |
    | - Inlined comparison = zero-cost abstraction              |
    +----------------------------------------------------------+
```

**中文解释：**
- 内核红黑树不知道键！用户代码提供比较逻辑
- rb_insert_color 只做平衡，用户找插入点
- 优势：支持任意键类型、复合键、无虚函数开销、内联比较

---

## Real Kernel Users

```
+------------------------------------------------------------------+
|  KERNEL RB-TREE USAGE EXAMPLES                                   |
+------------------------------------------------------------------+

    1. SCHEDULER: CFS (Completely Fair Scheduler)
    +----------------------------------------------------------+
    | Key: virtual runtime (vruntime)                           |
    | Purpose: Order tasks by how much CPU they've used         |
    | Why RB-tree: Need leftmost (minimum vruntime) quickly     |
    |                                                           |
    | struct sched_entity {                                     |
    |     struct rb_node run_node;  /* Tree node */             |
    |     u64 vruntime;             /* Key */                   |
    | };                                                        |
    +----------------------------------------------------------+
    
    2. MEMORY MANAGEMENT: VMAs (Virtual Memory Areas)
    +----------------------------------------------------------+
    | Key: Virtual address start                                |
    | Purpose: Find VMA containing an address                   |
    | Why RB-tree: Many VMAs, frequent lookups                  |
    |                                                           |
    | struct vm_area_struct {                                   |
    |     struct rb_node vm_rb;     /* Tree node */             |
    |     unsigned long vm_start;   /* Key */                   |
    |     unsigned long vm_end;                                 |
    | };                                                        |
    +----------------------------------------------------------+
    
    3. NETWORKING: Congestion Control
    +----------------------------------------------------------+
    | Key: Sequence number                                      |
    | Purpose: Track out-of-order packets                       |
    | Why RB-tree: Ordered sequence, gap detection              |
    +----------------------------------------------------------+
    
    4. FILESYSTEMS: Extent Trees
    +----------------------------------------------------------+
    | Key: File offset                                          |
    | Purpose: Map file offset to disk blocks                   |
    | Why RB-tree: Large files have many extents                |
    +----------------------------------------------------------+
    
    5. TIMERS: High-Resolution Timers
    +----------------------------------------------------------+
    | Key: Expiration time                                      |
    | Purpose: Find next timer to fire                          |
    | Why RB-tree: Need minimum (earliest) quickly              |
    +----------------------------------------------------------+
```

**中文解释：**
- CFS 调度器：按虚拟运行时间排序任务
- VMA 内存管理：按虚拟地址查找内存区域
- 网络拥塞控制：跟踪乱序包
- 文件系统范围树：映射文件偏移到磁盘块
- 高精度定时器：按过期时间排序

---

## Correctness Guarantees

```
+------------------------------------------------------------------+
|  RB-TREE INVARIANTS                                              |
+------------------------------------------------------------------+

    USER MUST GUARANTEE:
    +----------------------------------------------------------+
    | 1. Key comparison is consistent (trichotomy)              |
    |    a < b OR a = b OR a > b (exactly one)                  |
    |                                                           |
    | 2. Key doesn't change while node is in tree               |
    |    (Would violate ordering!)                              |
    |                                                           |
    | 3. rb_node initialized before insert                      |
    |    rb_link_node() + rb_insert_color()                     |
    |                                                           |
    | 4. rb_erase() before freeing object                       |
    |    (Otherwise dangling pointers in tree)                  |
    +----------------------------------------------------------+
    
    KERNEL GUARANTEES:
    +----------------------------------------------------------+
    | 1. Tree remains balanced after insert/delete              |
    |    O(log n) operations guaranteed                         |
    |                                                           |
    | 2. rb_first/rb_last/rb_next/rb_prev work correctly        |
    |    Ordered iteration is O(n)                              |
    |                                                           |
    | 3. Color bits don't corrupt parent pointer                |
    |    (Uses alignment trick in rb_parent_color)              |
    +----------------------------------------------------------+
```

**中文解释：**
- 用户保证：比较一致性、节点在树中时键不变、插入前初始化、释放前删除
- 内核保证：插入/删除后保持平衡、迭代正确、颜色位不破坏父指针

---

## User-Space Ordered Map

```c
/* User-space intrusive RB-tree inspired by Linux kernel */

#include <stddef.h>

struct rb_node {
    unsigned long parent_color;
    struct rb_node *left;
    struct rb_node *right;
};

struct rb_root {
    struct rb_node *node;
};

#define RB_RED   0
#define RB_BLACK 1

#define rb_parent(r)  ((struct rb_node *)((r)->parent_color & ~3))
#define rb_color(r)   ((r)->parent_color & 1)
#define rb_is_red(r)  (!rb_color(r))
#define rb_is_black(r) rb_color(r)

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

/* Example: Ordered integer map */
struct int_map_entry {
    int key;
    int value;
    struct rb_node rb;
};

struct int_map_entry *int_map_find(struct rb_root *root, int key)
{
    struct rb_node *node = root->node;
    
    while (node) {
        struct int_map_entry *e = rb_entry(node, 
                                           struct int_map_entry, rb);
        
        if (key < e->key)
            node = node->left;
        else if (key > e->key)
            node = node->right;
        else
            return e;
    }
    return NULL;
}

int int_map_insert(struct rb_root *root, struct int_map_entry *new)
{
    struct rb_node **link = &root->node;
    struct rb_node *parent = NULL;
    
    while (*link) {
        struct int_map_entry *e = rb_entry(*link, 
                                           struct int_map_entry, rb);
        parent = *link;
        
        if (new->key < e->key)
            link = &parent->left;
        else if (new->key > e->key)
            link = &parent->right;
        else
            return -1;  /* Duplicate */
    }
    
    /* Link node */
    new->rb.parent_color = (unsigned long)parent;
    new->rb.left = new->rb.right = NULL;
    *link = &new->rb;
    
    /* Rebalance (simplified - real impl needs rotations) */
    /* rb_insert_color(&new->rb, root); */
    
    return 0;
}

/* In-order iteration */
struct rb_node *rb_first(struct rb_root *root)
{
    struct rb_node *n = root->node;
    if (!n) return NULL;
    while (n->left) n = n->left;
    return n;
}

struct rb_node *rb_next(struct rb_node *node)
{
    if (node->right) {
        node = node->right;
        while (node->left) node = node->left;
        return node;
    }
    
    struct rb_node *parent;
    while ((parent = rb_parent(node)) && node == parent->right)
        node = parent;
    return parent;
}

/* Usage example */
void print_map(struct rb_root *root)
{
    struct rb_node *node;
    for (node = rb_first(root); node; node = rb_next(node)) {
        struct int_map_entry *e = rb_entry(node, 
                                           struct int_map_entry, rb);
        printf("%d: %d\n", e->key, e->value);
    }
}
```

**中文解释：**
- 用户态侵入式红黑树：模拟内核实现
- 嵌入 rb_node 到对象中
- 用户提供比较逻辑
- 支持有序迭代：rb_first + rb_next

