# 树形数据结构详解 (Tree Data Structures)

## 目录

1. [二叉树 (Binary Tree)](#1-二叉树-binary-tree)
2. [二叉搜索树 (Binary Search Tree)](#2-二叉搜索树-binary-search-tree)
3. [AVL 树 (AVL Tree)](#3-avl-树-avl-tree)
4. [红黑树 (Red-Black Tree)](#4-红黑树-red-black-tree)
5. [B 树 (B-Tree)](#5-b-树-b-tree)
6. [B+ 树 (B+ Tree)](#6-b-树-b-tree-1)
7. [堆 (Heap)](#7-堆-heap)
8. [字典树 (Trie)](#8-字典树-trie)
9. [树形结构对比总结](#9-树形结构对比总结)

---

## 1. 二叉树 (Binary Tree)

### 1.1 基本结构

```
                              ┌───────────────────────────────────────────┐
                              │           Binary Tree Structure           │
                              └───────────────────────────────────────────┘

                                              ┌───┐
                                              │ A │                          Level 0 (Root)
                                              └─┬─┘
                                        ┌──────┴──────┐
                                        ▼             ▼
                                      ┌───┐         ┌───┐
                                      │ B │         │ C │                    Level 1
                                      └─┬─┘         └─┬─┘
                                    ┌───┴───┐     ┌───┴───┐
                                    ▼       ▼     ▼       ▼
                                  ┌───┐   ┌───┐ ┌───┐   ┌───┐
                                  │ D │   │ E │ │ F │   │ G │                Level 2
                                  └───┘   └───┘ └───┘   └───┘


                              ┌─────────────────────────────────────────────┐
                              │              Node Structure                 │
                              │                                             │
                              │           ┌─────────────────┐               │
                              │           │      Node       │               │
                              │           ├─────────────────┤               │
                              │           │      data       │               │
                              │           ├────────┬────────┤               │
                              │           │  left  │ right  │               │
                              │           └────┬───┴───┬────┘               │
                              │                │       │                    │
                              │                ▼       ▼                    │
                              │           Left Child  Right Child           │
                              │            or NULL     or NULL              │
                              └─────────────────────────────────────────────┘
```

**说明**：
- 二叉树是每个节点最多有两个子节点的树结构
- 每个节点包含：数据域、左子指针、右子指针
- Root（根节点）是树的顶端，没有父节点
- Leaf（叶子节点）没有子节点
- Level（层级）从根节点的 0 开始计数
- Height（高度）是从根到最深叶子的路径长度

### 1.2 节点定义

```c
typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

typedef struct BinaryTree {
    TreeNode *root;
} BinaryTree;
```

### 1.3 四种遍历方式

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                    Tree Traversal Methods                       │
                    └─────────────────────────────────────────────────────────────────┘

                                              ┌───┐
                                              │ 1 │
                                              └─┬─┘
                                        ┌──────┴──────┐
                                        ▼             ▼
                                      ┌───┐         ┌───┐
                                      │ 2 │         │ 3 │
                                      └─┬─┘         └─┬─┘
                                    ┌───┴───┐     ┌───┴───┐
                                    ▼       ▼     ▼       ▼
                                  ┌───┐   ┌───┐ ┌───┐   ┌───┐
                                  │ 4 │   │ 5 │ │ 6 │   │ 7 │
                                  └───┘   └───┘ └───┘   └───┘


    ┌──────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                  │
    │  PRE-ORDER (Root-Left-Right):     1 -> 2 -> 4 -> 5 -> 3 -> 6 -> 7               │
    │                                                                                  │
    │  IN-ORDER (Left-Root-Right):      4 -> 2 -> 5 -> 1 -> 6 -> 3 -> 7               │
    │                                                                                  │
    │  POST-ORDER (Left-Right-Root):    4 -> 5 -> 2 -> 6 -> 7 -> 3 -> 1               │
    │                                                                                  │
    │  LEVEL-ORDER (BFS):               1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7               │
    │                                                                                  │
    └──────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **前序遍历 (Pre-order)**：根 → 左 → 右，用于复制树、序列化
- **中序遍历 (In-order)**：左 → 根 → 右，BST 中得到有序序列
- **后序遍历 (Post-order)**：左 → 右 → 根，用于删除树、计算目录大小
- **层序遍历 (Level-order)**：逐层从左到右，用于 BFS、打印树结构

### 1.4 遍历伪代码

```c
/* 前序遍历 - 递归 */
void preorder(TreeNode *node)
{
    if (node == NULL)
        return;
    process(node->data);      /* 访问根 */
    preorder(node->left);     /* 遍历左子树 */
    preorder(node->right);    /* 遍历右子树 */
}

/* 中序遍历 - 递归 */
void inorder(TreeNode *node)
{
    if (node == NULL)
        return;
    inorder(node->left);      /* 遍历左子树 */
    process(node->data);      /* 访问根 */
    inorder(node->right);     /* 遍历右子树 */
}

/* 后序遍历 - 递归 */
void postorder(TreeNode *node)
{
    if (node == NULL)
        return;
    postorder(node->left);    /* 遍历左子树 */
    postorder(node->right);   /* 遍历右子树 */
    process(node->data);      /* 访问根 */
}

/* 层序遍历 - 使用队列 */
void levelorder(TreeNode *root)
{
    Queue *q = queue_create();
    queue_enqueue(q, root);

    while (!queue_empty(q)) {
        TreeNode *node = queue_dequeue(q);
        process(node->data);

        if (node->left)
            queue_enqueue(q, node->left);
        if (node->right)
            queue_enqueue(q, node->right);
    }
}
```

### 1.5 应用场景

| 应用 | 说明 |
|------|------|
| 表达式树 | 编译器中表示算术表达式 |
| 决策树 | 机器学习分类算法 |
| 文件系统 | 目录结构表示 |
| XML/HTML 解析 | DOM 树 |

---

## 2. 二叉搜索树 (Binary Search Tree)

### 2.1 BST 性质

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │              Binary Search Tree Property                        │
                    └─────────────────────────────────────────────────────────────────┘

                                              ┌────┐
                                              │ 50 │
                                              └──┬─┘
                                        ┌───────┴───────┐
                                        ▼               ▼
                                      ┌────┐         ┌────┐
                                      │ 30 │         │ 70 │
                                      └──┬─┘         └──┬─┘
                                    ┌────┴────┐     ┌────┴────┐
                                    ▼         ▼     ▼         ▼
                                  ┌────┐   ┌────┐ ┌────┐   ┌────┐
                                  │ 20 │   │ 40 │ │ 60 │   │ 80 │
                                  └────┘   └────┘ └────┘   └────┘


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                                                                 │
                    │   BST PROPERTY:                                                 │
                    │                                                                 │
                    │   For every node N:                                             │
                    │     - All values in LEFT subtree  < N.value                     │
                    │     - All values in RIGHT subtree > N.value                     │
                    │                                                                 │
                    │                     ┌─────┐                                     │
                    │                     │  N  │                                     │
                    │                     └──┬──┘                                     │
                    │              ┌─────────┴─────────┐                              │
                    │              ▼                   ▼                              │
                    │        ┌───────────┐      ┌───────────┐                         │
                    │        │  < N.val  │      │  > N.val  │                         │
                    │        │  (LEFT)   │      │  (RIGHT)  │                         │
                    │        └───────────┘      └───────────┘                         │
                    │                                                                 │
                    └─────────────────────────────────────────────────────────────────┘
```

**说明**：
- BST 是一种特殊的二叉树，满足左小右大的性质
- 中序遍历 BST 得到升序序列
- 查找、插入、删除的平均时间复杂度为 O(log n)
- 最坏情况（退化为链表）时间复杂度为 O(n)

### 2.2 查找操作

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   BST Search: Find 40                           │
                    └─────────────────────────────────────────────────────────────────┘

                                              ┌────┐
                                        ┌─────│ 50 │  40 < 50, go LEFT
                                        │     └────┘
                                        ▼
                                      ┌────┐
                                      │ 30 │───┐    40 > 30, go RIGHT
                                      └────┘   │
                                               ▼
                                            ┌────┐
                                            │ 40 │  FOUND!
                                            └────┘

                    Search Path: 50 -> 30 -> 40  (3 comparisons)
```

**说明**：
- 从根节点开始，比较目标值与当前节点值
- 小于则走左子树，大于则走右子树
- 相等则找到，到达 NULL 则不存在

### 2.3 插入操作

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   BST Insert: Add 35                            │
                    └─────────────────────────────────────────────────────────────────┘

                    BEFORE:                              AFTER:

                          ┌────┐                              ┌────┐
                          │ 50 │                              │ 50 │
                          └──┬─┘                              └──┬─┘
                      ┌──────┴──────┐                    ┌──────┴──────┐
                      ▼             ▼                    ▼             ▼
                    ┌────┐       ┌────┐                ┌────┐       ┌────┐
                    │ 30 │       │ 70 │                │ 30 │       │ 70 │
                    └──┬─┘       └────┘                └──┬─┘       └────┘
                   ┌───┴───┐                          ┌───┴───┐
                   ▼       ▼                          ▼       ▼
                 ┌────┐ ┌────┐                      ┌────┐ ┌────┐
                 │ 20 │ │ 40 │                      │ 20 │ │ 40 │
                 └────┘ └────┘                      └────┘ └──┬─┘
                                                             │
                                                             ▼
                                                          ┌────┐
                                                          │ 35 │  NEW
                                                          └────┘

                    Insert Path: 50 -> 30 -> 40 -> (left of 40)
```

**说明**：
- 插入操作类似查找，找到合适的空位置
- 新节点总是作为叶子节点插入
- 保持 BST 性质不变

### 2.4 删除操作

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   BST Delete: Three Cases                       │
                    └─────────────────────────────────────────────────────────────────┘


    CASE 1: Delete leaf node (no children)

                          ┌────┐                              ┌────┐
                          │ 50 │                              │ 50 │
                          └──┬─┘                              └──┬─┘
                      ┌──────┴──────┐                    ┌──────┴──────┐
                      ▼             ▼          =>        ▼             ▼
                    ┌────┐       ┌────┐                ┌────┐       ┌────┐
                    │ 30 │       │ 70 │                │ 30 │       │ 70 │
                    └──┬─┘       └────┘                └────┘       └────┘
                       │
                       ▼
                     ┌────┐
                     │ 20 │  DELETE
                     └────┘

                    Simply remove the node.


    CASE 2: Delete node with one child

                          ┌────┐                              ┌────┐
                          │ 50 │                              │ 50 │
                          └──┬─┘                              └──┬─┘
                      ┌──────┴──────┐                    ┌──────┴──────┐
                      ▼             ▼          =>        ▼             ▼
                    ┌────┐       ┌────┐                ┌────┐       ┌────┐
                    │ 30 │DELETE │ 70 │                │ 20 │       │ 70 │
                    └──┬─┘       └────┘                └────┘       └────┘
                       │
                       ▼
                     ┌────┐
                     │ 20 │
                     └────┘

                    Replace with child.


    CASE 3: Delete node with two children

                          ┌────┐                              ┌────┐
                          │ 50 │ DELETE                       │ 60 │
                          └──┬─┘                              └──┬─┘
                      ┌──────┴──────┐                    ┌──────┴──────┐
                      ▼             ▼          =>        ▼             ▼
                    ┌────┐       ┌────┐                ┌────┐       ┌────┐
                    │ 30 │       │ 70 │                │ 30 │       │ 70 │
                    └────┘       └──┬─┘                └────┘       └────┘
                                   │
                                   ▼
                                 ┌────┐
                                 │ 60 │ In-order Successor
                                 └────┘

                    Replace with in-order successor (or predecessor),
                    then delete the successor.
```

**说明**：
- **情况 1**：删除叶子节点，直接删除
- **情况 2**：删除只有一个子节点的节点，用子节点替代
- **情况 3**：删除有两个子节点的节点，用中序后继（或前驱）替代，再删除后继

### 2.5 核心伪代码

```c
/* 查找 */
TreeNode *bst_search(TreeNode *root, int key)
{
    if (root == NULL || root->data == key)
        return root;

    if (key < root->data)
        return bst_search(root->left, key);
    else
        return bst_search(root->right, key);
}

/* 插入 */
TreeNode *bst_insert(TreeNode *root, int key)
{
    if (root == NULL) {
        TreeNode *node = malloc(sizeof(TreeNode));
        node->data = key;
        node->left = node->right = NULL;
        return node;
    }

    if (key < root->data)
        root->left = bst_insert(root->left, key);
    else if (key > root->data)
        root->right = bst_insert(root->right, key);

    return root;
}

/* 找最小值节点 */
TreeNode *bst_min(TreeNode *root)
{
    while (root->left != NULL)
        root = root->left;
    return root;
}

/* 删除 */
TreeNode *bst_delete(TreeNode *root, int key)
{
    if (root == NULL)
        return NULL;

    if (key < root->data) {
        root->left = bst_delete(root->left, key);
    } else if (key > root->data) {
        root->right = bst_delete(root->right, key);
    } else {
        /* 找到要删除的节点 */
        if (root->left == NULL) {
            TreeNode *temp = root->right;
            free(root);
            return temp;
        } else if (root->right == NULL) {
            TreeNode *temp = root->left;
            free(root);
            return temp;
        }
        /* 有两个子节点，找中序后继 */
        TreeNode *succ = bst_min(root->right);
        root->data = succ->data;
        root->right = bst_delete(root->right, succ->data);
    }
    return root;
}
```

### 2.6 时间复杂度

| 操作 | 平均 | 最坏 |
|------|------|------|
| 查找 | O(log n) | O(n) |
| 插入 | O(log n) | O(n) |
| 删除 | O(log n) | O(n) |

### 2.7 应用场景

| 应用 | 说明 |
|------|------|
| 有序集合 | 动态维护有序数据 |
| 符号表 | 编译器变量查找 |
| 数据库索引 | 简单索引实现 |

---

## 3. AVL 树 (AVL Tree)

### 3.1 AVL 性质

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   AVL Tree Balance Property                     │
                    └─────────────────────────────────────────────────────────────────┘

                    BALANCE FACTOR = height(left) - height(right)

                    Valid AVL Tree:                 Invalid (needs rotation):

                          ┌────┐                              ┌────┐
                          │ 50 │ bf=0                         │ 50 │ bf=-2
                          └──┬─┘                              └──┬─┘
                      ┌──────┴──────┐                            └──────┐
                      ▼             ▼                                   ▼
                    ┌────┐       ┌────┐                              ┌────┐
                    │ 30 │bf=0   │ 70 │bf=0                          │ 70 │ bf=-1
                    └────┘       └────┘                              └──┬─┘
                                                                        └──┐
                                                                           ▼
                                                                        ┌────┐
                                                                        │ 80 │
                                                                        └────┘

                    ┌─────────────────────────────────────────────────────────────────┐
                    │                                                                 │
                    │   AVL INVARIANT:                                                │
                    │                                                                 │
                    │   For every node: |balance_factor| <= 1                         │
                    │                                                                 │
                    │   balance_factor in {-1, 0, 1}                                  │
                    │                                                                 │
                    └─────────────────────────────────────────────────────────────────┘
```

**说明**：
- AVL 树是自平衡的二叉搜索树
- 每个节点的平衡因子（左子树高度 - 右子树高度）必须在 {-1, 0, 1} 范围内
- 通过旋转操作维持平衡
- 保证所有操作的时间复杂度为 O(log n)

### 3.2 四种旋转操作

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   AVL Rotations                                 │
                    └─────────────────────────────────────────────────────────────────┘


    1. LEFT-LEFT Case (LL) - Single Right Rotation

              z                                      y
             / \                                   /   \
            y   T4      Right Rotate (z)          x      z
           / \          - Loss - - - - ->        /  \    /  \
          x   T3                                T1  T2  T3  T4
         / \
        T1   T2

                          ┌───┐                              ┌───┐
                          │ z │                              │ y │
                          └─┬─┘                              └─┬─┘
                      ┌────┴────┐                        ┌────┴────┐
                      ▼         ▼         =>             ▼         ▼
                    ┌───┐     [T4]                     ┌───┐     ┌───┐
                    │ y │                             │ x │     │ z │
                    └─┬─┘                             └─┬─┘     └─┬─┘
                ┌────┴────┐                         ┌──┴──┐   ┌──┴──┐
                ▼         ▼                         ▼     ▼   ▼     ▼
              ┌───┐     [T3]                      [T1]  [T2] [T3]  [T4]
              │ x │
              └─┬─┘
           ┌───┴───┐
           ▼       ▼
         [T1]    [T2]


    2. RIGHT-RIGHT Case (RR) - Single Left Rotation

            z                                        y
           / \                                     /   \
          T1   y         Left Rotate (z)          z      x
              / \        - - - - - - - ->        / \    / \
             T2   x                             T1  T2 T3  T4
                 / \
                T3  T4

                          ┌───┐                              ┌───┐
                          │ z │                              │ y │
                          └─┬─┘                              └─┬─┘
                      ┌────┴────┐                        ┌────┴────┐
                      ▼         ▼         =>             ▼         ▼
                    [T1]      ┌───┐                    ┌───┐     ┌───┐
                              │ y │                    │ z │     │ x │
                              └─┬─┘                    └─┬─┘     └─┬─┘
                          ┌────┴────┐                ┌──┴──┐   ┌──┴──┐
                          ▼         ▼                ▼     ▼   ▼     ▼
                        [T2]      ┌───┐            [T1]  [T2] [T3]  [T4]
                                  │ x │
                                  └─┬─┘
                               ┌───┴───┐
                               ▼       ▼
                             [T3]    [T4]


    3. LEFT-RIGHT Case (LR) - Double Rotation (Left then Right)

              z                               z                           x
             / \                            /   \                        /   \
            y   T4   Left Rotate (y)       x    T4   Right Rotate(z)    y      z
           / \       - - - - - - - - ->   /  \       - - - - - - - ->  / \    / \
          T1   x                         y    T3                      T1  T2 T3  T4
              / \                       / \
            T2   T3                    T1   T2


    4. RIGHT-LEFT Case (RL) - Double Rotation (Right then Left)

            z                            z                              x
           / \                          / \                            /   \
          T1   y    Right Rotate (y)   T1   x     Left Rotate(z)      z      y
              / \   - - - - - - - ->      /  \    - - - - - - - ->   / \    / \
             x   T4                      T2   y                     T1  T2  T3  T4
            / \                              /  \
          T2   T3                           T3   T4
```

**说明**：
- **LL 情况**：左子树的左子树插入导致不平衡，单次右旋
- **RR 情况**：右子树的右子树插入导致不平衡，单次左旋
- **LR 情况**：左子树的右子树插入导致不平衡，先左旋后右旋
- **RL 情况**：右子树的左子树插入导致不平衡，先右旋后左旋

### 3.3 核心伪代码

```c
typedef struct AVLNode {
    int data;
    int height;
    struct AVLNode *left;
    struct AVLNode *right;
} AVLNode;

int height(AVLNode *node)
{
    return node ? node->height : 0;
}

int balance_factor(AVLNode *node)
{
    return node ? height(node->left) - height(node->right) : 0;
}

void update_height(AVLNode *node)
{
    int hl = height(node->left);
    int hr = height(node->right);
    node->height = (hl > hr ? hl : hr) + 1;
}

/* 右旋 */
AVLNode *rotate_right(AVLNode *z)
{
    AVLNode *y = z->left;
    AVLNode *T3 = y->right;

    y->right = z;
    z->left = T3;

    update_height(z);
    update_height(y);

    return y;
}

/* 左旋 */
AVLNode *rotate_left(AVLNode *z)
{
    AVLNode *y = z->right;
    AVLNode *T2 = y->left;

    y->left = z;
    z->right = T2;

    update_height(z);
    update_height(y);

    return y;
}

/* 插入并平衡 */
AVLNode *avl_insert(AVLNode *node, int key)
{
    /* 标准 BST 插入 */
    if (node == NULL)
        return create_node(key);

    if (key < node->data)
        node->left = avl_insert(node->left, key);
    else if (key > node->data)
        node->right = avl_insert(node->right, key);
    else
        return node;

    /* 更新高度 */
    update_height(node);

    /* 获取平衡因子 */
    int bf = balance_factor(node);

    /* LL Case */
    if (bf > 1 && key < node->left->data)
        return rotate_right(node);

    /* RR Case */
    if (bf < -1 && key > node->right->data)
        return rotate_left(node);

    /* LR Case */
    if (bf > 1 && key > node->left->data) {
        node->left = rotate_left(node->left);
        return rotate_right(node);
    }

    /* RL Case */
    if (bf < -1 && key < node->right->data) {
        node->right = rotate_right(node->right);
        return rotate_left(node);
    }

    return node;
}
```

### 3.4 时间复杂度

| 操作 | 时间复杂度 |
|------|-----------|
| 查找 | O(log n) |
| 插入 | O(log n) |
| 删除 | O(log n) |
| 旋转 | O(1) |

### 3.5 应用场景

| 应用 | 说明 |
|------|------|
| 数据库索引 | 需要频繁查找的场景 |
| 内存管理 | 内核内存分配器 |
| 实时系统 | 需要严格 O(log n) 保证 |

---

## 4. 红黑树 (Red-Black Tree)

### 4.1 红黑树性质

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Red-Black Tree Properties                     │
                    └─────────────────────────────────────────────────────────────────┘

                                              ┌────┐
                                              │ 13 │ BLACK
                                              └──┬─┘
                                        ┌───────┴───────┐
                                        ▼               ▼
                                    ┌──────┐        ┌──────┐
                                    │  8   │ RED    │  17  │ RED
                                    └──┬───┘        └──┬───┘
                                  ┌────┴────┐      ┌───┴────┐
                                  ▼         ▼      ▼        ▼
                              ┌────┐     ┌────┐ ┌────┐   ┌────┐
                              │ 1  │BLK  │ 11 │ │ 15 │   │ 25 │ BLACK
                              └──┬─┘     └────┘ └────┘   └──┬─┘
                                 │        BLK    BLK        │
                                 ▼                          ▼
                              ┌────┐                     ┌────┐
                              │ 6  │ RED                 │ 22 │ RED
                              └────┘                     └──┬─┘
                                                            │
                                                            ▼
                                                         ┌────┐
                                                         │ 27 │ RED
                                                         └────┘


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                                                                 │
                    │   RED-BLACK TREE RULES:                                         │
                    │                                                                 │
                    │   1. Every node is either RED or BLACK                          │
                    │   2. Root is always BLACK                                       │
                    │   3. Every leaf (NIL) is BLACK                                  │
                    │   4. If a node is RED, both children must be BLACK              │
                    │      (No two consecutive RED nodes)                             │
                    │   5. Every path from root to leaf has same number of            │
                    │      BLACK nodes (Black Height)                                 │
                    │                                                                 │
                    └─────────────────────────────────────────────────────────────────┘
```

**说明**：
- 红黑树是一种弱平衡的二叉搜索树
- 通过颜色规则保证最长路径不超过最短路径的两倍
- 相比 AVL 树，插入删除时旋转次数更少
- 广泛用于标准库实现（如 C++ STL map/set, Java TreeMap）

### 4.2 红黑树 vs AVL 树

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │              Red-Black Tree vs AVL Tree                         │
                    └─────────────────────────────────────────────────────────────────┘

                    ┌─────────────────────────────────────────────────────────────────┐
                    │                                                                 │
                    │   AVL TREE:                                                     │
                    │   - Strictly balanced (|bf| <= 1)                               │
                    │   - Height: ~1.44 * log2(n)                                     │
                    │   - More rotations on insert/delete                             │
                    │   - Better for read-heavy workloads                             │
                    │                                                                 │
                    │   RED-BLACK TREE:                                               │
                    │   - Loosely balanced                                            │
                    │   - Height: ~2 * log2(n)                                        │
                    │   - Fewer rotations on insert/delete                            │
                    │   - Better for write-heavy workloads                            │
                    │                                                                 │
                    │   ┌──────────────┬─────────────┬─────────────┐                  │
                    │   │   Operation  │   AVL Tree  │  RB Tree    │                  │
                    │   ├──────────────┼─────────────┼─────────────┤                  │
                    │   │   Search     │  Faster     │  Slower     │                  │
                    │   │   Insert     │  Slower     │  Faster     │                  │
                    │   │   Delete     │  Slower     │  Faster     │                  │
                    │   │   Rotations  │  More       │  Fewer      │                  │
                    │   └──────────────┴─────────────┴─────────────┘                  │
                    │                                                                 │
                    └─────────────────────────────────────────────────────────────────┘
```

**说明**：
- AVL 树更严格平衡，查找更快，但维护成本高
- 红黑树平衡要求宽松，插入删除更快
- 选择依据：读多写少用 AVL，写多读少用红黑树

### 4.3 核心伪代码

```c
typedef enum { RED, BLACK } Color;

typedef struct RBNode {
    int data;
    Color color;
    struct RBNode *left;
    struct RBNode *right;
    struct RBNode *parent;
} RBNode;

/* 左旋 */
void rb_rotate_left(RBTree *tree, RBNode *x)
{
    RBNode *y = x->right;
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

/* 插入修复 */
void rb_insert_fixup(RBTree *tree, RBNode *z)
{
    while (z->parent->color == RED) {
        if (z->parent == z->parent->parent->left) {
            RBNode *y = z->parent->parent->right;  /* uncle */
            if (y->color == RED) {
                /* Case 1: Uncle is RED */
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    /* Case 2: Uncle is BLACK, z is right child */
                    z = z->parent;
                    rb_rotate_left(tree, z);
                }
                /* Case 3: Uncle is BLACK, z is left child */
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                rb_rotate_right(tree, z->parent->parent);
            }
        } else {
            /* Symmetric cases */
            /* ... */
        }
    }
    tree->root->color = BLACK;
}
```

### 4.4 应用场景

| 应用 | 说明 |
|------|------|
| C++ STL | map, set, multimap, multiset |
| Java | TreeMap, TreeSet |
| Linux 内核 | CFS 调度器、内存管理 |
| 数据库 | 内存索引 |

---

## 5. B 树 (B-Tree)

### 5.1 B 树结构

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   B-Tree Structure (Order 3)                    │
                    └─────────────────────────────────────────────────────────────────┘

                                        ┌─────────────────┐
                                        │   30  │   70    │                Root
                                        └────┬──┴──┬──────┘
                                     ┌───────┘     └───────┐
                                     ▼                     ▼
                           ┌─────────────────┐   ┌─────────────────┐
                           │  10  │   20     │   │  50  │   60     │    Internal
                           └──┬───┴───┬──────┘   └──┬───┴───┬──────┘
                      ┌───────┘       └───┐   ┌────┘       └────┐
                      ▼                   ▼   ▼                 ▼
                ┌──────────┐        ┌──────────┐          ┌──────────┐
                │ 1│ 5│ 8  │        │ 15│ 18   │          │ 55│ 58   │  Leaf
                └──────────┘        └──────────┘          └──────────┘


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                                                                 │
                    │   B-TREE PROPERTIES (Order m):                                  │
                    │                                                                 │
                    │   1. Every node has at most m children                          │
                    │   2. Every non-leaf node (except root) has at least m/2 children│
                    │   3. Root has at least 2 children if not a leaf                 │
                    │   4. All leaves appear at the same level                        │
                    │   5. A node with k children contains k-1 keys                   │
                    │                                                                 │
                    │   Node Structure:                                               │
                    │   ┌────┬────┬────┬────┬────┬────┬────┐                          │
                    │   │ P0 │ K1 │ P1 │ K2 │ P2 │ K3 │ P3 │                          │
                    │   └──┬─┴────┴──┬─┴────┴──┬─┴────┴──┬─┘                          │
                    │      │         │         │         │                            │
                    │      ▼         ▼         ▼         ▼                            │
                    │   <K1       K1-K2     K2-K3      >K3                             │
                    │                                                                 │
                    └─────────────────────────────────────────────────────────────────┘
```

**说明**：
- B 树是一种多路平衡搜索树，专为磁盘存储设计
- 每个节点可以有多个键和子节点
- 所有叶子节点在同一层，保证平衡
- 减少磁盘 I/O 次数，因为每个节点对应一个磁盘块

### 5.2 B 树查找

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   B-Tree Search: Find 55                        │
                    └─────────────────────────────────────────────────────────────────┘

                                        ┌─────────────────┐
                    Step 1:             │   30  │   70    │   55 > 30, 55 < 70
                                        └────┬──┴──┬──────┘   Go to middle child
                                             └─────┼──────────────┐
                                                   │              │
                                                   ▼              │
                                         ┌─────────────────┐      │
                    Step 2:              │  50  │   60     │      │  55 > 50, 55 < 60
                                         └──┬───┴───┬──────┘      │  Go to middle child
                                            └───────┼─────────────┘
                                                    │
                                                    ▼
                                              ┌──────────┐
                    Step 3:                   │ 55│ 58   │   FOUND 55!
                                              └──────────┘

                    Disk I/O: 3 reads (one per level)
```

**说明**：
- 在每个节点内进行顺序或二分查找
- 找到键则返回，否则沿相应子指针向下
- 到达叶子仍未找到则不存在

### 5.3 B 树插入

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   B-Tree Insert with Split                      │
                    └─────────────────────────────────────────────────────────────────┘

    Insert 25 into a full node (Order 3, max 2 keys per node):

    BEFORE:
                                        ┌─────────────────┐
                                        │   30  │   70    │
                                        └────┬──┴──┬──────┘
                                     ┌───────┘     │
                                     ▼             │
                           ┌─────────────────┐     │
                           │  10  │   20     │     │   Need to insert 25 here
                           └─────────────────┘     │   But node is FULL!
                                                   │
                                                   ▼
                                         ┌─────────────────┐
                                         │  50  │   60     │
                                         └─────────────────┘

    SPLIT PROCESS:

    1. Insert 25 into sorted position: [10, 20, 25]
    2. Split: [10] | 20 | [25]
    3. Promote middle key (20) to parent

    AFTER:
                                        ┌─────────────────────────┐
                                        │   20  │   30  │   70    │
                                        └──┬────┴──┬────┴──┬──────┘
                                   ┌───────┘       │       └───────┐
                                   ▼               ▼               ▼
                             ┌──────────┐   ┌──────────┐   ┌─────────────────┐
                             │    10    │   │    25    │   │  50  │   60     │
                             └──────────┘   └──────────┘   └─────────────────┘
```

**说明**：
- 找到合适的叶子节点插入
- 如果节点已满，则分裂：中间键上升到父节点
- 分裂可能向上传播，直到根节点（树高度增加）

### 5.4 核心伪代码

```c
#define ORDER 3
#define MAX_KEYS (ORDER - 1)
#define MIN_KEYS (ORDER / 2)

typedef struct BTreeNode {
    int n;                          /* 当前键数量 */
    int keys[MAX_KEYS];             /* 键数组 */
    struct BTreeNode *children[ORDER];  /* 子节点指针 */
    int is_leaf;                    /* 是否叶子 */
} BTreeNode;

/* 查找 */
BTreeNode *btree_search(BTreeNode *node, int key, int *idx)
{
    int i = 0;
    while (i < node->n && key > node->keys[i])
        i++;

    if (i < node->n && key == node->keys[i]) {
        *idx = i;
        return node;
    }

    if (node->is_leaf)
        return NULL;

    return btree_search(node->children[i], key, idx);
}

/* 分裂子节点 */
void btree_split_child(BTreeNode *parent, int i)
{
    BTreeNode *full = parent->children[i];
    BTreeNode *new = create_node(full->is_leaf);

    new->n = MIN_KEYS;

    /* 复制后半部分键到新节点 */
    for (int j = 0; j < MIN_KEYS; j++)
        new->keys[j] = full->keys[j + MIN_KEYS + 1];

    /* 如果不是叶子，复制子指针 */
    if (!full->is_leaf) {
        for (int j = 0; j <= MIN_KEYS; j++)
            new->children[j] = full->children[j + MIN_KEYS + 1];
    }

    full->n = MIN_KEYS;

    /* 在父节点中插入中间键 */
    for (int j = parent->n; j > i; j--)
        parent->children[j + 1] = parent->children[j];
    parent->children[i + 1] = new;

    for (int j = parent->n - 1; j >= i; j--)
        parent->keys[j + 1] = parent->keys[j];
    parent->keys[i] = full->keys[MIN_KEYS];

    parent->n++;
}
```

### 5.5 时间复杂度

| 操作 | 时间复杂度 | 磁盘 I/O |
|------|-----------|----------|
| 查找 | O(log n) | O(log_m n) |
| 插入 | O(log n) | O(log_m n) |
| 删除 | O(log n) | O(log_m n) |

### 5.6 应用场景

| 应用 | 说明 |
|------|------|
| 数据库索引 | MySQL, PostgreSQL |
| 文件系统 | NTFS, HFS+, ext4 |
| 键值存储 | LevelDB, RocksDB |

---

## 6. B+ 树 (B+ Tree)

### 6.1 B+ 树结构

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   B+ Tree Structure                             │
                    └─────────────────────────────────────────────────────────────────┘

                                        ┌─────────────────┐
                                        │   30  │   70    │          Internal Nodes
                                        └────┬──┴──┬──────┘          (Only Keys)
                                     ┌───────┘     └───────┐
                                     ▼                     ▼
                           ┌─────────────────┐   ┌─────────────────┐
                           │  10  │   20     │   │  50  │   60     │
                           └──┬───┴───┬──────┘   └──┬───┴───┬──────┘
                      ┌───────┘       └───┐   ┌────┘       └────┐
                      ▼                   ▼   ▼                 ▼
                ┌──────────┐        ┌──────────┐          ┌──────────┐
                │ 5│10│15  │───────►│ 20│25│30 │─────────►│ 50│55│60 │──►...
                │ D│ D│ D  │        │ D │ D│ D │          │ D │ D│ D │
                └──────────┘        └──────────┘          └──────────┘
                                                                        Leaf Nodes
                                                                        (Keys + Data)
                                                                        Linked List


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                                                                 │
                    │   B+ TREE vs B-TREE:                                            │
                    │                                                                 │
                    │   B-TREE:                                                       │
                    │   - Data stored in ALL nodes                                    │
                    │   - No leaf node linking                                        │
                    │                                                                 │
                    │   B+ TREE:                                                      │
                    │   - Data stored ONLY in leaf nodes                              │
                    │   - Internal nodes only contain keys (index)                    │
                    │   - Leaf nodes linked for range queries                         │
                    │   - All keys appear in leaf level                               │
                    │                                                                 │
                    └─────────────────────────────────────────────────────────────────┘
```

**说明**：
- B+ 树是 B 树的变种，数据只存储在叶子节点
- 内部节点只存储键（索引），可以容纳更多键
- 叶子节点通过链表连接，支持高效范围查询
- 所有查找都必须到达叶子节点

### 6.2 B+ 树范围查询

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │              B+ Tree Range Query: Find 20-55                    │
                    └─────────────────────────────────────────────────────────────────┘

    Step 1: Find starting point (20)

                                        ┌─────────────────┐
                                        │   30  │   70    │
                                        └────┬──┴─────────┘
                                     ┌───────┘
                                     ▼
                           ┌─────────────────┐
                           │  10  │   20     │
                           └───────────┬─────┘
                                       │
                                       ▼
                                ┌──────────┐
                                │ 20│25│30 │  START HERE
                                └──────────┘

    Step 2: Follow leaf links until end of range

                ┌──────────┐        ┌──────────┐          ┌──────────┐
                │ 20│25│30 │───────►│ 35│40│45 │─────────►│ 50│55│60 │
                └──────────┘        └──────────┘          └──────────┘
                    START ──────────────────────────────────► STOP at 55

    Result: 20, 25, 30, 35, 40, 45, 50, 55
```

**说明**：
- 先通过树结构找到起始键
- 然后沿叶子链表顺序扫描直到结束键
- 范围查询效率远高于 B 树

### 6.3 核心伪代码

```c
typedef struct BPlusNode {
    int n;                              /* 键数量 */
    int keys[ORDER];                    /* 键数组 */
    int is_leaf;
    union {
        struct BPlusNode *children[ORDER + 1];  /* 内部节点：子指针 */
        struct {
            void *data[ORDER];          /* 叶子节点：数据指针 */
            struct BPlusNode *next;     /* 叶子链表指针 */
        };
    };
} BPlusNode;

/* 范围查询 */
void bplus_range_query(BPlusNode *root, int start, int end)
{
    /* 找到起始叶子节点 */
    BPlusNode *leaf = find_leaf(root, start);

    /* 沿叶子链表遍历 */
    while (leaf != NULL) {
        for (int i = 0; i < leaf->n; i++) {
            if (leaf->keys[i] > end)
                return;
            if (leaf->keys[i] >= start)
                process(leaf->keys[i], leaf->data[i]);
        }
        leaf = leaf->next;
    }
}

/* 查找叶子节点 */
BPlusNode *find_leaf(BPlusNode *node, int key)
{
    while (!node->is_leaf) {
        int i = 0;
        while (i < node->n && key >= node->keys[i])
            i++;
        node = node->children[i];
    }
    return node;
}
```

### 6.4 应用场景

| 应用 | 说明 |
|------|------|
| MySQL InnoDB | 主键索引和二级索引 |
| PostgreSQL | 默认索引类型 |
| 文件系统 | NTFS, ReiserFS |
| 数据库范围查询 | ORDER BY, BETWEEN |

---

## 7. 堆 (Heap)

### 7.1 堆结构

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Heap Structure                                │
                    └─────────────────────────────────────────────────────────────────┘

    MAX-HEAP (parent >= children):        MIN-HEAP (parent <= children):

              ┌────┐                                ┌────┐
              │ 90 │                                │ 10 │
              └──┬─┘                                └──┬─┘
          ┌─────┴─────┐                        ┌─────┴─────┐
          ▼           ▼                        ▼           ▼
        ┌────┐     ┌────┐                    ┌────┐     ┌────┐
        │ 80 │     │ 70 │                    │ 20 │     │ 30 │
        └──┬─┘     └──┬─┘                    └──┬─┘     └──┬─┘
       ┌───┴───┐  ┌───┴───┐                ┌───┴───┐  ┌───┴───┐
       ▼       ▼  ▼       ▼                ▼       ▼  ▼       ▼
     ┌────┐ ┌────┐┌────┐ ┌────┐          ┌────┐ ┌────┐┌────┐ ┌────┐
     │ 60 │ │ 50 ││ 40 │ │ 30 │          │ 50 │ │ 60 ││ 70 │ │ 80 │
     └────┘ └────┘└────┘ └────┘          └────┘ └────┘└────┘ └────┘


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Array Representation                          │
                    └─────────────────────────────────────────────────────────────────┘

                              ┌────┐
                              │ 90 │ index 0
                              └──┬─┘
                          ┌─────┴─────┐
                          ▼           ▼
                        ┌────┐     ┌────┐
                        │ 80 │     │ 70 │ index 1, 2
                        └──┬─┘     └──┬─┘
                       ┌───┴───┐  ┌───┴───┐
                       ▼       ▼  ▼       ▼
                     ┌────┐ ┌────┐┌────┐ ┌────┐
                     │ 60 │ │ 50 ││ 40 │ │ 30 │ index 3, 4, 5, 6
                     └────┘ └────┘└────┘ └────┘

    Array: [90, 80, 70, 60, 50, 40, 30]
            0   1   2   3   4   5   6

    Parent(i) = (i - 1) / 2
    Left(i)   = 2 * i + 1
    Right(i)  = 2 * i + 2
```

**说明**：
- 堆是一种完全二叉树，满足堆性质
- 最大堆：父节点 >= 子节点，根是最大值
- 最小堆：父节点 <= 子节点，根是最小值
- 通常用数组实现，通过索引计算父子关系
- 堆不是有序的，只保证根是极值

### 7.2 堆操作

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Heap Insert (Sift Up)                         │
                    └─────────────────────────────────────────────────────────────────┘

    Insert 85 into Max-Heap:

    Step 1: Add at end           Step 2: Sift up

              ┌────┐                       ┌────┐
              │ 90 │                       │ 90 │
              └──┬─┘                       └──┬─┘
          ┌─────┴─────┐                ┌─────┴─────┐
          ▼           ▼                ▼           ▼
        ┌────┐     ┌────┐            ┌────┐     ┌────┐
        │ 80 │     │ 70 │            │ 85 │     │ 70 │   80 < 85, swap
        └──┬─┘     └────┘            └──┬─┘     └────┘
       ┌───┴───┐                    ┌───┴───┐
       ▼       ▼                    ▼       ▼
     ┌────┐ ┌────┐                ┌────┐ ┌────┐
     │ 60 │ │ 85 │ NEW            │ 60 │ │ 80 │
     └────┘ └────┘                └────┘ └────┘


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Heap Extract Max (Sift Down)                  │
                    └─────────────────────────────────────────────────────────────────┘

    Extract max from Max-Heap:

    Step 1: Replace root with last    Step 2: Sift down

              ┌────┐                       ┌────┐
              │ 30 │ (was 90)              │ 80 │
              └──┬─┘                       └──┬─┘
          ┌─────┴─────┐                ┌─────┴─────┐
          ▼           ▼                ▼           ▼
        ┌────┐     ┌────┐            ┌────┐     ┌────┐
        │ 80 │     │ 70 │            │ 60 │     │ 70 │
        └──┬─┘     └────┘            └──┬─┘     └────┘
       ┌───┘                        ┌───┴───┐
       ▼                            ▼       ▼
     ┌────┐                       ┌────┐ ┌────┐
     │ 60 │                       │ 30 │ │ 50 │
     └────┘                       └────┘ └────┘

    Returned: 90
```

**说明**：
- **插入 (Sift Up)**：添加到末尾，向上调整直到满足堆性质
- **提取极值 (Sift Down)**：取出根，用最后元素替换，向下调整
- 两种操作时间复杂度都是 O(log n)

### 7.3 核心伪代码

```c
typedef struct Heap {
    int *arr;
    int size;
    int capacity;
} Heap;

void swap(int *a, int *b)
{
    int t = *a;
    *a = *b;
    *b = t;
}

/* 向上调整（插入后） */
void sift_up(Heap *h, int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->arr[i] <= h->arr[parent])
            break;
        swap(&h->arr[i], &h->arr[parent]);
        i = parent;
    }
}

/* 向下调整（提取后） */
void sift_down(Heap *h, int i)
{
    while (1) {
        int largest = i;
        int left = 2 * i + 1;
        int right = 2 * i + 2;

        if (left < h->size && h->arr[left] > h->arr[largest])
            largest = left;
        if (right < h->size && h->arr[right] > h->arr[largest])
            largest = right;

        if (largest == i)
            break;

        swap(&h->arr[i], &h->arr[largest]);
        i = largest;
    }
}

/* 插入 */
void heap_insert(Heap *h, int val)
{
    h->arr[h->size] = val;
    sift_up(h, h->size);
    h->size++;
}

/* 提取最大值 */
int heap_extract_max(Heap *h)
{
    int max = h->arr[0];
    h->arr[0] = h->arr[--h->size];
    sift_down(h, 0);
    return max;
}

/* 建堆 - O(n) */
void heapify(Heap *h)
{
    for (int i = h->size / 2 - 1; i >= 0; i--)
        sift_down(h, i);
}

/* 堆排序 */
void heap_sort(int *arr, int n)
{
    Heap h = {arr, n, n};
    heapify(&h);

    for (int i = n - 1; i > 0; i--) {
        swap(&arr[0], &arr[i]);
        h.size--;
        sift_down(&h, 0);
    }
}
```

### 7.4 时间复杂度

| 操作 | 时间复杂度 |
|------|-----------|
| 插入 | O(log n) |
| 提取极值 | O(log n) |
| 查看极值 | O(1) |
| 建堆 | O(n) |
| 堆排序 | O(n log n) |

### 7.5 应用场景

| 应用 | 说明 |
|------|------|
| 优先队列 | 任务调度、事件驱动 |
| 堆排序 | 原地排序算法 |
| Top K 问题 | 找最大/最小的 K 个元素 |
| 图算法 | Dijkstra, Prim 算法 |
| 中位数维护 | 双堆实现 |

---

## 8. 字典树 (Trie)

### 8.1 Trie 结构

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Trie Structure                                │
                    └─────────────────────────────────────────────────────────────────┘

    Words: "cat", "car", "card", "care", "dog", "do"

                                        ┌─────┐
                                        │ ROOT│
                                        └──┬──┘
                               ┌───────────┼───────────┐
                               ▼           │           ▼
                            ┌─────┐        │        ┌─────┐
                            │  c  │        │        │  d  │
                            └──┬──┘        │        └──┬──┘
                               │           │           │
                               ▼           │           ▼
                            ┌─────┐        │        ┌─────┐
                            │  a  │        │        │  o  │*
                            └──┬──┘        │        └──┬──┘
                          ┌────┴────┐      │           │
                          ▼         ▼      │           ▼
                       ┌─────┐   ┌─────┐   │        ┌─────┐
                       │  t  │*  │  r  │*  │        │  g  │*
                       └─────┘   └──┬──┘   │        └─────┘
                                 ┌──┴──┐   │
                                 ▼     ▼   │
                              ┌─────┐┌─────┐
                              │  d  │││  e  │*
                              └─────┘└─────┘
                                 *

                    * = End of word marker


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Node Structure                                │
                    └─────────────────────────────────────────────────────────────────┘

                              ┌────────────────────────────────────┐
                              │           TrieNode                 │
                              ├────────────────────────────────────┤
                              │  is_end: bool                      │
                              │  children[26]: TrieNode*           │
                              │                                    │
                              │  [a][b][c][d]...[x][y][z]          │
                              │   │     │                          │
                              │   ▼     ▼                          │
                              │  ...   ...                         │
                              └────────────────────────────────────┘
```

**说明**：
- Trie（前缀树/字典树）是一种多叉树，用于存储字符串集合
- 每个节点代表一个字符，从根到某节点的路径表示一个前缀
- `is_end` 标记表示该节点是否为某个单词的结尾
- 空间换时间，共享公共前缀节省空间

### 8.2 Trie 操作

```
                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Trie Insert: "care"                           │
                    └─────────────────────────────────────────────────────────────────┘

    Existing: "car"

                    BEFORE:                         AFTER:

                    ┌─────┐                         ┌─────┐
                    │ ROOT│                         │ ROOT│
                    └──┬──┘                         └──┬──┘
                       │                               │
                       ▼                               ▼
                    ┌─────┐                         ┌─────┐
                    │  c  │                         │  c  │
                    └──┬──┘                         └──┬──┘
                       │                               │
                       ▼                               ▼
                    ┌─────┐                         ┌─────┐
                    │  a  │                         │  a  │
                    └──┬──┘                         └──┬──┘
                       │                               │
                       ▼                               ▼
                    ┌─────┐                         ┌─────┐
                    │  r  │*                        │  r  │*
                    └─────┘                         └──┬──┘
                                                      │
                                                      ▼
                                                   ┌─────┐
                                                   │  e  │* NEW
                                                   └─────┘


                    ┌─────────────────────────────────────────────────────────────────┐
                    │                   Trie Search: "car"                            │
                    └─────────────────────────────────────────────────────────────────┘

                    ┌─────┐
                    │ ROOT│  Start here
                    └──┬──┘
                       │ 'c'
                       ▼
                    ┌─────┐
                    │  c  │  Match 'c'
                    └──┬──┘
                       │ 'a'
                       ▼
                    ┌─────┐
                    │  a  │  Match 'a'
                    └──┬──┘
                       │ 'r'
                       ▼
                    ┌─────┐
                    │  r  │* Match 'r', is_end=true, FOUND!
                    └─────┘
```

**说明**：
- **插入**：沿路径创建缺失节点，最后标记 `is_end`
- **查找**：沿路径走，检查最后节点的 `is_end`
- **前缀查找**：沿路径走，只要路径存在就返回 true

### 8.3 核心伪代码

```c
#define ALPHABET_SIZE 26

typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    int is_end;
} TrieNode;

TrieNode *trie_create_node(void)
{
    TrieNode *node = malloc(sizeof(TrieNode));
    node->is_end = 0;
    for (int i = 0; i < ALPHABET_SIZE; i++)
        node->children[i] = NULL;
    return node;
}

/* 插入单词 */
void trie_insert(TrieNode *root, const char *word)
{
    TrieNode *curr = root;

    while (*word) {
        int idx = *word - 'a';
        if (curr->children[idx] == NULL)
            curr->children[idx] = trie_create_node();
        curr = curr->children[idx];
        word++;
    }

    curr->is_end = 1;
}

/* 查找单词 */
int trie_search(TrieNode *root, const char *word)
{
    TrieNode *curr = root;

    while (*word) {
        int idx = *word - 'a';
        if (curr->children[idx] == NULL)
            return 0;
        curr = curr->children[idx];
        word++;
    }

    return curr->is_end;
}

/* 查找前缀 */
int trie_starts_with(TrieNode *root, const char *prefix)
{
    TrieNode *curr = root;

    while (*prefix) {
        int idx = *prefix - 'a';
        if (curr->children[idx] == NULL)
            return 0;
        curr = curr->children[idx];
        prefix++;
    }

    return 1;
}

/* 自动补全 - 返回所有以 prefix 开头的单词 */
void trie_autocomplete(TrieNode *node, char *prefix, int len)
{
    if (node->is_end)
        printf("%s\n", prefix);

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            prefix[len] = 'a' + i;
            prefix[len + 1] = '\0';
            trie_autocomplete(node->children[i], prefix, len + 1);
        }
    }
}
```

### 8.4 时间复杂度

| 操作 | 时间复杂度 |
|------|-----------|
| 插入 | O(m) |
| 查找 | O(m) |
| 前缀查找 | O(m) |
| 删除 | O(m) |

其中 m 是字符串长度

### 8.5 应用场景

| 应用 | 说明 |
|------|------|
| 自动补全 | 搜索引擎、IDE |
| 拼写检查 | 文字处理软件 |
| IP 路由 | 最长前缀匹配 |
| 词频统计 | 文本分析 |
| T9 输入法 | 手机键盘预测 |

---

## 9. 树形结构对比总结

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                           Tree Data Structures Comparison                              │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│  ┌──────────────┬────────────┬────────────┬────────────┬─────────────────────────────┐ │
│  │   Structure  │   Search   │   Insert   │   Delete   │   Best Use Case             │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │ Binary Tree  │    O(n)    │    O(n)    │    O(n)    │ Expression trees, DOM       │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │     BST      │ O(log n)*  │ O(log n)*  │ O(log n)*  │ Simple ordered data         │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │   AVL Tree   │  O(log n)  │  O(log n)  │  O(log n)  │ Read-heavy workloads        │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │  Red-Black   │  O(log n)  │  O(log n)  │  O(log n)  │ Write-heavy, STL maps       │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │   B-Tree     │ O(log_m n) │ O(log_m n) │ O(log_m n) │ Database, file systems      │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │   B+ Tree    │ O(log_m n) │ O(log_m n) │ O(log_m n) │ Range queries, DB indexes   │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │    Heap      │    O(n)    │  O(log n)  │  O(log n)  │ Priority queues, sorting    │ │
│  ├──────────────┼────────────┼────────────┼────────────┼─────────────────────────────┤ │
│  │    Trie      │    O(m)    │    O(m)    │    O(m)    │ String prefix operations    │ │
│  └──────────────┴────────────┴────────────┴────────────┴─────────────────────────────┘ │
│                                                                                        │
│  * BST worst case is O(n) when degenerated to linked list                              │
│  m = string length for Trie, branching factor for B-Tree                               │
│                                                                                        │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              Selection Guide                                           │
├────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                        │
│   Need ordered data?                                                                   │
│       │                                                                                │
│       ├── YES ─► Need guaranteed O(log n)?                                             │
│       │              │                                                                 │
│       │              ├── YES ─► Read-heavy? ─► AVL Tree                                │
│       │              │              │                                                  │
│       │              │              └── Write-heavy? ─► Red-Black Tree                 │
│       │              │                                                                 │
│       │              └── NO ─► BST (simple cases)                                      │
│       │                                                                                │
│       └── NO ─► Need priority access?                                                  │
│                    │                                                                   │
│                    ├── YES ─► Heap                                                     │
│                    │                                                                   │
│                    └── NO ─► Need string prefix?                                       │
│                                  │                                                     │
│                                  ├── YES ─► Trie                                       │
│                                  │                                                     │
│                                  └── NO ─► Disk storage?                               │
│                                                │                                       │
│                                                ├── YES ─► Range queries? ─► B+ Tree    │
│                                                │              │                        │
│                                                │              └── Point queries? ─► B-Tree│
│                                                │                                       │
│                                                └── NO ─► Binary Tree                   │
│                                                                                        │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 选择树结构时需要考虑：数据特征、操作频率、存储介质
- 内存操作优先考虑 AVL/红黑树
- 磁盘操作优先考虑 B/B+ 树
- 字符串操作优先考虑 Trie
- 优先级操作优先考虑 Heap

---

## 10. 完整代码示例：二叉搜索树

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct TreeNode {
    int data;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;

/* 创建节点 */
TreeNode *create_node(int data)
{
    TreeNode *node = malloc(sizeof(TreeNode));
    node->data = data;
    node->left = node->right = NULL;
    return node;
}

/* 插入 */
TreeNode *bst_insert(TreeNode *root, int data)
{
    if (root == NULL)
        return create_node(data);

    if (data < root->data)
        root->left = bst_insert(root->left, data);
    else if (data > root->data)
        root->right = bst_insert(root->right, data);

    return root;
}

/* 查找 */
TreeNode *bst_search(TreeNode *root, int data)
{
    if (root == NULL || root->data == data)
        return root;

    if (data < root->data)
        return bst_search(root->left, data);

    return bst_search(root->right, data);
}

/* 找最小值 */
TreeNode *bst_min(TreeNode *root)
{
    while (root && root->left)
        root = root->left;
    return root;
}

/* 删除 */
TreeNode *bst_delete(TreeNode *root, int data)
{
    if (root == NULL)
        return NULL;

    if (data < root->data) {
        root->left = bst_delete(root->left, data);
    } else if (data > root->data) {
        root->right = bst_delete(root->right, data);
    } else {
        if (root->left == NULL) {
            TreeNode *temp = root->right;
            free(root);
            return temp;
        }
        if (root->right == NULL) {
            TreeNode *temp = root->left;
            free(root);
            return temp;
        }
        TreeNode *succ = bst_min(root->right);
        root->data = succ->data;
        root->right = bst_delete(root->right, succ->data);
    }
    return root;
}

/* 中序遍历 */
void inorder(TreeNode *root)
{
    if (root == NULL)
        return;
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
}

/* 打印树结构 */
void print_tree(TreeNode *root, int level)
{
    if (root == NULL)
        return;

    print_tree(root->right, level + 1);

    for (int i = 0; i < level; i++)
        printf("    ");
    printf("%d\n", root->data);

    print_tree(root->left, level + 1);
}

/* 释放树 */
void free_tree(TreeNode *root)
{
    if (root == NULL)
        return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(void)
{
    TreeNode *root = NULL;

    /* 插入测试 */
    int values[] = {50, 30, 70, 20, 40, 60, 80};
    int n = sizeof(values) / sizeof(values[0]);

    for (int i = 0; i < n; i++)
        root = bst_insert(root, values[i]);

    printf("BST Structure:\n");
    print_tree(root, 0);

    printf("\nInorder traversal: ");
    inorder(root);
    printf("\n");

    /* 查找测试 */
    int key = 40;
    TreeNode *found = bst_search(root, key);
    printf("\nSearch %d: %s\n", key, found ? "Found" : "Not found");

    /* 删除测试 */
    printf("\nDelete 30:\n");
    root = bst_delete(root, 30);
    print_tree(root, 0);

    printf("\nInorder after deletion: ");
    inorder(root);
    printf("\n");

    free_tree(root);
    return 0;
}
```

### 编译运行

```bash
gcc -o bst_demo bst_demo.c
./bst_demo
```

### 输出

```
BST Structure:
        80
    70
        60
50
        40
    30
        20

Inorder traversal: 20 30 40 50 60 70 80

Search 40: Found

Delete 30:
        80
    70
        60
50
    40
        20

Inorder after deletion: 20 40 50 60 70 80
```

