# C 语言中二级指针（指针的指针）典型应用场景及技巧

## 目录

1. [概述](#概述)
2. [场景一：链表节点的原地删除](#场景一链表节点的原地删除)
3. [场景二：函数内修改调用者的指针](#场景二函数内修改调用者的指针)
4. [场景三：动态二维数组](#场景三动态二维数组)
5. [场景四：字符串数组处理](#场景四字符串数组处理)
6. [场景五：树结构的递归操作](#场景五树结构的递归操作)
7. [场景六：哈希表链地址法](#场景六哈希表链地址法)
8. [场景七：通用回调函数上下文传递](#场景七通用回调函数上下文传递)
9. [场景八：输出参数返回多个值](#场景八输出参数返回多个值)
10. [总结](#总结)

---

## 概述

### 什么是二级指针？

二级指针（Pointer to Pointer）是指向指针的指针，即存储另一个指针地址的变量。

```c
int    x  = 10;      /* 普通变量 */
int   *p  = &x;      /* 一级指针：存储 x 的地址 */
int  **pp = &p;      /* 二级指针：存储 p 的地址 */
```

### 内存布局图解

```
地址        变量        值           说明
------------------------------------------------------
0x1000      x          10           普通整数
0x2000      p          0x1000       指向 x 的指针
0x3000      pp         0x2000       指向 p 的指针（二级指针）

访问关系：
  pp   -> 0x2000 (p 的地址)
 *pp   -> 0x1000 (x 的地址，即 p 的值)
**pp   -> 10     (x 的值)
```

### 核心用途

> **当你需要修改一个指针本身（而不仅仅是它指向的内容）时，就需要传递这个指针的地址，即使用二级指针。**

---

## 场景一：链表节点的原地删除

这是二级指针最经典、最优雅的应用场景。

### 数据结构定义

```c
typedef struct Node {
    int value;
    struct Node *next;
} Node;
```

### 传统方法（一级指针）

```c
/**
 * 传统方法：删除链表中值为 value 的第一个节点
 * 需要特殊处理头节点的情况
 */
void remove_node_traditional(Node **head, int value) {
    Node *prev = NULL;
    Node *curr = *head;
    
    while (curr != NULL) {
        if (curr->value == value) {
            if (prev == NULL) {
                /* 特殊情况：删除的是头节点 */
                *head = curr->next;
            } else {
                /* 普通情况：删除中间或尾部节点 */
                prev->next = curr->next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}
```

#### 传统方法图解

```
删除头节点 A 时：
  Before: head -> [A] -> [B] -> [C] -> NULL
                   ^
                  curr (prev = NULL)
  
  特殊处理: *head = curr->next
  
  After:  head -> [B] -> [C] -> NULL

删除中间节点 B 时：
  Before: head -> [A] -> [B] -> [C] -> NULL
                   ^      ^
                  prev   curr
  
  普通处理: prev->next = curr->next
  
  After:  head -> [A] -> [C] -> NULL
```

### 二级指针方法

```c
/**
 * 二级指针方法：删除链表中值为 value 的第一个节点
 * 无需特殊处理头节点，代码统一优雅
 */
void remove_node_elegant(Node **head, int value) {
    Node **pp = head;  /* pp 指向"指向当前节点的那个指针" */
    
    while (*pp != NULL) {
        if ((*pp)->value == value) {
            Node *to_free = *pp;
            *pp = (*pp)->next;  /* 直接修改前一个节点的 next（或 head） */
            free(to_free);
            return;
        }
        pp = &(*pp)->next;  /* pp 移动到下一个节点的 next 指针的地址 */
    }
}
```

#### 二级指针方法图解

```
初始状态：pp 指向 head 指针本身
  
       pp
       |
       v
     +------+
     | head |---> [A] -> [B] -> [C] -> NULL
     +------+

删除头节点 A 时：
  *pp = (*pp)->next  =>  直接让 head 指向 B
  
       pp
       |
       v
     +------+
     | head |---> [B] -> [C] -> NULL
     +------+

---

移动 pp 后：pp 指向 A 的 next 字段
  
     +------+     +---+---+     +---+---+
     | head |---> | A | *-|---> | B | *-|---> [C] -> NULL
     +------+     +---+---+     +---+---+
                        ^
                        |
                       pp

删除中间节点 B 时：
  *pp = (*pp)->next  =>  直接让 A->next 指向 C
  
     +------+     +---+---+
     | head |---> | A | *-|---> [C] -> NULL
     +------+     +---+---+
```

### 对比分析

| 方面 | 传统方法 | 二级指针方法 |
|------|----------|--------------|
| 代码行数 | 较多（需要 if-else 分支） | 较少（统一处理） |
| 逻辑复杂度 | 需要区分头节点和其他节点 | 所有节点处理方式相同 |
| 可读性 | 对初学者更直观 | 需要理解二级指针概念 |
| 出错风险 | 容易遗漏边界情况 | 边界情况自动处理 |
| 性能 | 相同 | 相同 |

### 删除所有匹配节点的完整示例

```c
/**
 * 删除链表中所有值为 value 的节点
 * 返回删除的节点数量
 */
int remove_all_nodes(Node **head, int value) {
    Node **pp = head;
    int count = 0;
    
    while (*pp != NULL) {
        if ((*pp)->value == value) {
            Node *to_free = *pp;
            *pp = (*pp)->next;  /* 不移动 pp，继续检查新的 *pp */
            free(to_free);
            count++;
        } else {
            pp = &(*pp)->next;  /* 只有不删除时才移动 pp */
        }
    }
    return count;
}
```

---

## 场景二：函数内修改调用者的指针

### 问题演示：一级指针无法修改调用者的指针

```c
/**
 * 错误示例：试图在函数内分配内存并返回
 * 问题：p 是局部变量，修改它不影响调用者的指针
 */
void alloc_wrong(int *p) {
    p = malloc(sizeof(int));  /* 只修改了局部变量 p */
    if (p != NULL) {
        *p = 42;
    }
}

int main() {
    int *ptr = NULL;
    alloc_wrong(ptr);
    /* ptr 仍然是 NULL！*/
    printf("%d\n", *ptr);  /* 段错误！ */
    return 0;
}
```

#### 错误原因图解

```
调用前：
  main 栈帧:    ptr = NULL
  
调用 alloc_wrong(ptr)：
  main 栈帧:    ptr = NULL        <-- 没有被修改
  func 栈帧:    p = NULL (ptr 的副本)
  
malloc 后：
  main 栈帧:    ptr = NULL        <-- 仍然是 NULL
  func 栈帧:    p = 0x1000 (新分配的地址)
  堆:           [42] @ 0x1000
  
函数返回后：
  main 栈帧:    ptr = NULL        <-- 依然是 NULL
  堆:           [42] @ 0x1000     <-- 内存泄漏！
```

### 正确方法：使用二级指针

```c
/**
 * 正确示例：使用二级指针修改调用者的指针
 */
void alloc_correct(int **pp) {
    *pp = malloc(sizeof(int));  /* 修改调用者的指针 */
    if (*pp != NULL) {
        **pp = 42;
    }
}

int main() {
    int *ptr = NULL;
    alloc_correct(&ptr);  /* 传递 ptr 的地址 */
    printf("%d\n", *ptr); /* 输出 42 */
    free(ptr);
    return 0;
}
```

#### 正确方法图解

```
调用前：
  main 栈帧:    ptr = NULL (地址 0x2000)
  
调用 alloc_correct(&ptr)：
  main 栈帧:    ptr = NULL @ 0x2000
  func 栈帧:    pp = 0x2000 (指向 ptr)
  
*pp = malloc(...) 后：
  main 栈帧:    ptr = 0x1000      <-- 被修改了！
  func 栈帧:    pp = 0x2000
  堆:           [42] @ 0x1000
  
函数返回后：
  main 栈帧:    ptr = 0x1000      <-- 正确指向堆内存
  堆:           [42] @ 0x1000
```

### 典型应用：链表头插法

```c
/**
 * 在链表头部插入新节点
 * 需要修改 head 指针，因此使用二级指针
 */
int list_insert_head(Node **head, int value) {
    Node *new_node = malloc(sizeof(Node));
    if (new_node == NULL) {
        return -1;  /* 内存分配失败 */
    }
    
    new_node->value = value;
    new_node->next = *head;  /* 新节点指向原来的头 */
    *head = new_node;        /* 更新头指针 */
    return 0;
}

int main() {
    Node *list = NULL;
    
    list_insert_head(&list, 10);  /* list: [10] -> NULL */
    list_insert_head(&list, 20);  /* list: [20] -> [10] -> NULL */
    list_insert_head(&list, 30);  /* list: [30] -> [20] -> [10] -> NULL */
    
    /* 清理... */
    return 0;
}
```

### 对比：返回值方法 vs 二级指针方法

```c
/* 方法 1：通过返回值 */
Node *list_insert_head_v1(Node *head, int value) {
    Node *new_node = malloc(sizeof(Node));
    new_node->value = value;
    new_node->next = head;
    return new_node;  /* 调用者必须接收返回值 */
}
/* 使用：list = list_insert_head_v1(list, 10); */

/* 方法 2：通过二级指针 */
void list_insert_head_v2(Node **head, int value) {
    Node *new_node = malloc(sizeof(Node));
    new_node->value = value;
    new_node->next = *head;
    *head = new_node;  /* 直接修改调用者的指针 */
}
/* 使用：list_insert_head_v2(&list, 10); */
```

| 方面 | 返回值方法 | 二级指针方法 |
|------|------------|--------------|
| 调用方式 | `list = func(list, val)` | `func(&list, val)` |
| 忘记接收返回值 | 编译器可能警告，但不报错 | 不会出现此问题 |
| 返回值用途 | 被占用，无法返回错误码 | 可以返回错误码 |
| 风格偏好 | 函数式风格 | 命令式风格 |

---

## 场景三：动态二维数组

### 传统方法：一维数组模拟二维

```c
/**
 * 使用一维数组模拟二维数组
 * 访问 arr[i][j] 需要手动计算索引：arr[i * cols + j]
 */
int *create_2d_flat(int rows, int cols) {
    return malloc(rows * cols * sizeof(int));
}

void set_value_flat(int *arr, int cols, int i, int j, int value) {
    arr[i * cols + j] = value;  /* 手动计算索引 */
}

int get_value_flat(int *arr, int cols, int i, int j) {
    return arr[i * cols + j];
}
```

### 二级指针方法：真正的动态二维数组

```c
/**
 * 使用二级指针创建动态二维数组
 * 可以像普通二维数组一样使用 arr[i][j] 语法
 */
int **create_2d_array(int rows, int cols) {
    /* 分配行指针数组 */
    int **arr = malloc(rows * sizeof(int *));
    if (arr == NULL) return NULL;
    
    /* 分配每一行的数据 */
    for (int i = 0; i < rows; i++) {
        arr[i] = malloc(cols * sizeof(int));
        if (arr[i] == NULL) {
            /* 分配失败，清理已分配的内存 */
            for (int j = 0; j < i; j++) {
                free(arr[j]);
            }
            free(arr);
            return NULL;
        }
    }
    return arr;
}

void free_2d_array(int **arr, int rows) {
    if (arr == NULL) return;
    for (int i = 0; i < rows; i++) {
        free(arr[i]);
    }
    free(arr);
}
```

#### 内存布局图解

```
create_2d_array(3, 4) 的内存布局：

栈：
  arr (int **) = 0x1000

堆：
  0x1000: +--------+--------+--------+
          | 0x2000 | 0x3000 | 0x4000 |  <- 行指针数组
          +--------+--------+--------+
             |         |         |
             v         v         v
  0x2000: [  ][  ][  ][  ]  <- 第 0 行 (4 个 int)
  0x3000: [  ][  ][  ][  ]  <- 第 1 行 (4 个 int)
  0x4000: [  ][  ][  ][  ]  <- 第 2 行 (4 个 int)

访问 arr[1][2]：
  1. arr[1] = *(arr + 1) = 0x3000
  2. arr[1][2] = *(0x3000 + 2) = 第 1 行第 2 列的值
```

### 使用示例

```c
int main() {
    int rows = 3, cols = 4;
    int **matrix = create_2d_array(rows, cols);
    
    /* 像普通二维数组一样使用 */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = i * cols + j;
        }
    }
    
    /* 打印 */
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%2d ", matrix[i][j]);
        }
        printf("\n");
    }
    /* 输出：
     *  0  1  2  3
     *  4  5  6  7
     *  8  9 10 11
     */
    
    free_2d_array(matrix, rows);
    return 0;
}
```

### 对比分析

| 方面 | 一维数组模拟 | 二级指针方法 |
|------|--------------|--------------|
| 内存连续性 | 完全连续 | 行指针连续，行数据可能不连续 |
| 访问语法 | `arr[i * cols + j]` | `arr[i][j]` |
| 缓存友好性 | 更好 | 稍差 |
| 行大小可变 | 不支持 | 支持（锯齿数组） |
| 内存分配次数 | 1 次 | rows + 1 次 |

### 锯齿数组（每行长度不同）

```c
/**
 * 创建锯齿数组：每行长度可以不同
 */
int **create_jagged_array(int rows, int *col_sizes) {
    int **arr = malloc(rows * sizeof(int *));
    for (int i = 0; i < rows; i++) {
        arr[i] = malloc(col_sizes[i] * sizeof(int));
    }
    return arr;
}

int main() {
    int col_sizes[] = {3, 5, 2, 4};  /* 每行的列数 */
    int **jagged = create_jagged_array(4, col_sizes);
    
    /* 第 0 行有 3 列，第 1 行有 5 列... */
    jagged[0][2] = 10;
    jagged[1][4] = 20;
    
    /* 清理... */
    return 0;
}
```

---

## 场景四：字符串数组处理

### 命令行参数

```c
/**
 * main 函数的标准签名
 * argv 是 char** 类型：指向字符串指针数组
 */
int main(int argc, char **argv) {
    printf("程序名: %s\n", argv[0]);
    
    for (int i = 1; i < argc; i++) {
        printf("参数 %d: %s\n", i, argv[i]);
    }
    
    return 0;
}
```

#### argv 内存布局图解

```
运行 ./program hello world

栈：
  argc = 3
  argv (char **) = 0x1000

堆/数据段：
  0x1000: +--------+--------+--------+--------+
          | 0x2000 | 0x2010 | 0x2020 |  NULL  |
          +--------+--------+--------+--------+
             |         |         |
             v         v         v
  0x2000: "./program\0"
  0x2010: "hello\0"
  0x2020: "world\0"
```

### 字符串数组的创建和处理

```c
/**
 * 创建字符串数组（深拷贝）
 */
char **create_string_array(const char *strings[], int count) {
    char **arr = malloc((count + 1) * sizeof(char *));  /* +1 for NULL terminator */
    
    for (int i = 0; i < count; i++) {
        arr[i] = strdup(strings[i]);  /* 复制字符串 */
    }
    arr[count] = NULL;  /* NULL 终止 */
    
    return arr;
}

/**
 * 释放字符串数组
 */
void free_string_array(char **arr) {
    if (arr == NULL) return;
    
    for (int i = 0; arr[i] != NULL; i++) {
        free(arr[i]);
    }
    free(arr);
}

/**
 * 打印字符串数组
 */
void print_strings(char **strings) {
    for (int i = 0; strings[i] != NULL; i++) {
        printf("[%d] %s\n", i, strings[i]);
    }
}

int main() {
    const char *names[] = {"Alice", "Bob", "Charlie"};
    char **copy = create_string_array(names, 3);
    
    print_strings(copy);
    /* 输出：
     * [0] Alice
     * [1] Bob
     * [2] Charlie
     */
    
    free_string_array(copy);
    return 0;
}
```

### 字符串排序

```c
/**
 * 比较函数：用于 qsort
 * 注意：qsort 传递的是元素的地址，对于 char* 数组，元素是 char*，
 * 所以传递的是 char** 类型
 */
int compare_strings(const void *a, const void *b) {
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;
    return strcmp(str_a, str_b);
}

void sort_strings(char **strings, int count) {
    qsort(strings, count, sizeof(char *), compare_strings);
}
```

---

## 场景五：树结构的递归操作

### 数据结构定义

```c
typedef struct TreeNode {
    int value;
    struct TreeNode *left;
    struct TreeNode *right;
} TreeNode;
```

### 传统方法：返回新根

```c
/**
 * 传统方法：BST 插入，返回新的根节点
 * 调用者必须接收返回值
 */
TreeNode *bst_insert_traditional(TreeNode *root, int value) {
    if (root == NULL) {
        TreeNode *node = malloc(sizeof(TreeNode));
        node->value = value;
        node->left = node->right = NULL;
        return node;
    }
    
    if (value < root->value) {
        root->left = bst_insert_traditional(root->left, value);
    } else {
        root->right = bst_insert_traditional(root->right, value);
    }
    return root;
}

/* 使用：root = bst_insert_traditional(root, 10); */
```

### 二级指针方法：直接修改

```c
/**
 * 二级指针方法：BST 插入
 * 无需返回值，直接修改树结构
 */
void bst_insert(TreeNode **root, int value) {
    if (*root == NULL) {
        *root = malloc(sizeof(TreeNode));
        (*root)->value = value;
        (*root)->left = (*root)->right = NULL;
        return;
    }
    
    if (value < (*root)->value) {
        bst_insert(&(*root)->left, value);
    } else {
        bst_insert(&(*root)->right, value);
    }
}

/* 使用：bst_insert(&root, 10); */
```

#### 递归过程图解

```
插入值 25 到以下树：
        30
       /
      20
     /
    10

调用链：
  bst_insert(&root, 25)
    root 指向 30，25 < 30，递归左子树
  -> bst_insert(&(30->left), 25)
       30->left 指向 20，25 > 20，递归右子树
  -> bst_insert(&(20->right), 25)
       20->right 是 NULL，分配新节点
       *(20->right 的地址) = 新节点

结果：
        30
       /
      20
     /  \
    10   25
```

### 二级指针方法：BST 删除

```c
/**
 * 找到最小节点的指针的地址
 */
TreeNode **find_min(TreeNode **root) {
    while ((*root)->left != NULL) {
        root = &(*root)->left;
    }
    return root;
}

/**
 * BST 删除节点
 */
void bst_delete(TreeNode **root, int value) {
    if (*root == NULL) return;
    
    if (value < (*root)->value) {
        bst_delete(&(*root)->left, value);
    } else if (value > (*root)->value) {
        bst_delete(&(*root)->right, value);
    } else {
        /* 找到要删除的节点 */
        TreeNode *to_free = *root;
        
        if ((*root)->left == NULL) {
            /* 情况 1：无左子树 */
            *root = (*root)->right;
            free(to_free);
        } else if ((*root)->right == NULL) {
            /* 情况 2：无右子树 */
            *root = (*root)->left;
            free(to_free);
        } else {
            /* 情况 3：有两个子节点 */
            /* 找到右子树的最小节点 */
            TreeNode **min = find_min(&(*root)->right);
            (*root)->value = (*min)->value;  /* 复制值 */
            bst_delete(min, (*min)->value);  /* 删除最小节点 */
        }
    }
}
```

### 对比分析

| 方面 | 返回值方法 | 二级指针方法 |
|------|------------|--------------|
| 调用方式 | `root = func(root, val)` | `func(&root, val)` |
| 递归调用 | `root->left = func(root->left, val)` | `func(&root->left, val)` |
| 代码简洁度 | 每层都要赋值 | 直接修改，无需赋值 |
| 返回值 | 被占用 | 可用于返回错误码 |

---

## 场景六：哈希表链地址法

### 数据结构定义

```c
#define HASH_SIZE 256

typedef struct Entry {
    char *key;
    void *value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry *buckets[HASH_SIZE];
    int count;
} HashMap;
```

### 哈希函数

```c
static unsigned int hash(const char *key) {
    unsigned int h = 0;
    while (*key) {
        h = h * 31 + (unsigned char)*key++;
    }
    return h;
}
```

### 插入操作

```c
/**
 * 插入或更新键值对
 * 使用二级指针遍历链表
 */
int hashmap_put(HashMap *map, const char *key, void *value) {
    unsigned int h = hash(key) % HASH_SIZE;
    Entry **pp = &map->buckets[h];  /* 二级指针指向桶头 */
    
    /* 查找是否已存在 */
    while (*pp != NULL) {
        if (strcmp((*pp)->key, key) == 0) {
            (*pp)->value = value;  /* 更新已存在的键 */
            return 0;
        }
        pp = &(*pp)->next;
    }
    
    /* 插入新节点（此时 *pp == NULL） */
    *pp = malloc(sizeof(Entry));
    if (*pp == NULL) return -1;
    
    (*pp)->key = strdup(key);
    (*pp)->value = value;
    (*pp)->next = NULL;
    map->count++;
    
    return 0;
}
```

#### 插入过程图解

```
哈希表插入 "apple" -> 100

假设 hash("apple") % 256 = 5

初始状态：
  buckets[5] = NULL
  pp = &buckets[5]

  +------------+
  | buckets[5] |---> NULL
  +------------+
        ^
        |
       pp

插入后：
  *pp = malloc(...)

  +------------+     +-------+
  | buckets[5] |---> | apple |---> NULL
  +------------+     | 100   |
                     +-------+

再插入 "banana" -> 200（假设也哈希到 5）：

  pp = &buckets[5]
  *pp != NULL, strcmp("apple", "banana") != 0
  pp = &(apple->next)
  *pp == NULL，在此插入

  +------------+     +-------+     +--------+
  | buckets[5] |---> | apple |---> | banana |---> NULL
  +------------+     | 100   |     | 200    |
                     +-------+     +--------+
```

### 删除操作

```c
/**
 * 删除键值对
 * 使用二级指针简化链表删除
 */
int hashmap_remove(HashMap *map, const char *key) {
    unsigned int h = hash(key) % HASH_SIZE;
    Entry **pp = &map->buckets[h];
    
    while (*pp != NULL) {
        if (strcmp((*pp)->key, key) == 0) {
            Entry *to_free = *pp;
            *pp = (*pp)->next;  /* 从链表中移除 */
            free(to_free->key);
            free(to_free);
            map->count--;
            return 0;  /* 删除成功 */
        }
        pp = &(*pp)->next;
    }
    
    return -1;  /* 未找到 */
}
```

### 查找操作

```c
/**
 * 查找键对应的值
 */
void *hashmap_get(HashMap *map, const char *key) {
    unsigned int h = hash(key) % HASH_SIZE;
    
    for (Entry *e = map->buckets[h]; e != NULL; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            return e->value;
        }
    }
    
    return NULL;  /* 未找到 */
}
```

### 完整使用示例

```c
int main() {
    HashMap map = {0};
    
    int v1 = 100, v2 = 200, v3 = 300;
    
    hashmap_put(&map, "apple", &v1);
    hashmap_put(&map, "banana", &v2);
    hashmap_put(&map, "cherry", &v3);
    
    printf("apple: %d\n", *(int *)hashmap_get(&map, "apple"));   /* 100 */
    printf("banana: %d\n", *(int *)hashmap_get(&map, "banana")); /* 200 */
    
    hashmap_remove(&map, "banana");
    printf("banana: %p\n", hashmap_get(&map, "banana"));  /* (nil) */
    
    return 0;
}
```

---

## 场景七：通用回调函数上下文传递

### 使用 void* 和二级指针实现通用性

```c
/**
 * 通用链表节点
 */
typedef struct GenericNode {
    void *data;
    struct GenericNode *next;
} GenericNode;

/**
 * 访问者函数类型
 * data: 节点数据
 * context: 用户上下文（可通过二级指针修改）
 */
typedef void (*VisitorFunc)(void *data, void *context);

/**
 * 遍历链表，对每个节点调用访问者函数
 */
void list_foreach(GenericNode *head, VisitorFunc visitor, void *context) {
    for (GenericNode *n = head; n != NULL; n = n->next) {
        visitor(n->data, context);
    }
}
```

### 应用示例：计算总和

```c
/**
 * 访问者：累加整数值
 */
void sum_visitor(void *data, void *context) {
    int value = *(int *)data;
    int *sum = (int *)context;
    *sum += value;
}

int calculate_sum(GenericNode *list) {
    int total = 0;
    list_foreach(list, sum_visitor, &total);
    return total;
}
```

### 应用示例：过滤并收集

```c
/**
 * 过滤上下文
 */
typedef struct {
    int threshold;
    int *results;
    int count;
    int capacity;
} FilterContext;

/**
 * 访问者：收集大于阈值的值
 */
void filter_visitor(void *data, void *context) {
    int value = *(int *)data;
    FilterContext *ctx = (FilterContext *)context;
    
    if (value > ctx->threshold && ctx->count < ctx->capacity) {
        ctx->results[ctx->count++] = value;
    }
}

int *filter_greater_than(GenericNode *list, int threshold, int *out_count) {
    FilterContext ctx = {
        .threshold = threshold,
        .results = malloc(100 * sizeof(int)),
        .count = 0,
        .capacity = 100
    };
    
    list_foreach(list, filter_visitor, &ctx);
    
    *out_count = ctx.count;
    return ctx.results;
}
```

---

## 场景八：输出参数返回多个值

### 基本示例：除法运算

```c
/**
 * 同时返回商和余数
 * 使用指针作为输出参数
 */
void divmod(int a, int b, int *quotient, int *remainder) {
    *quotient = a / b;
    *remainder = a % b;
}

int main() {
    int q, r;
    divmod(17, 5, &q, &r);
    printf("17 / 5 = %d 余 %d\n", q, r);  /* 17 / 5 = 3 余 2 */
    return 0;
}
```

### 解析函数示例

```c
/**
 * 解析 "key=value" 格式的字符串
 * 通过二级指针返回分配的字符串
 */
int parse_key_value(const char *str, char **out_key, char **out_value) {
    const char *eq = strchr(str, '=');
    if (eq == NULL) {
        return -1;  /* 格式错误 */
    }
    
    /* 分配并复制 key */
    size_t key_len = eq - str;
    *out_key = malloc(key_len + 1);
    if (*out_key == NULL) return -1;
    strncpy(*out_key, str, key_len);
    (*out_key)[key_len] = '\0';
    
    /* 分配并复制 value */
    *out_value = strdup(eq + 1);
    if (*out_value == NULL) {
        free(*out_key);
        *out_key = NULL;
        return -1;
    }
    
    return 0;
}

int main() {
    char *key, *value;
    
    if (parse_key_value("name=Alice", &key, &value) == 0) {
        printf("key: %s, value: %s\n", key, value);
        free(key);
        free(value);
    }
    
    return 0;
}
```

### 查找并返回指针

```c
/**
 * 在链表中查找节点，返回指向该节点指针的地址
 * 这样调用者可以直接用于删除操作
 */
Node **list_find(Node **head, int value) {
    Node **pp = head;
    
    while (*pp != NULL) {
        if ((*pp)->value == value) {
            return pp;  /* 返回指向目标节点的指针的地址 */
        }
        pp = &(*pp)->next;
    }
    
    return pp;  /* 返回指向 NULL 的指针的地址 */
}

/**
 * 使用示例：查找并删除
 */
void find_and_remove(Node **head, int value) {
    Node **pp = list_find(head, value);
    
    if (*pp != NULL) {
        Node *to_free = *pp;
        *pp = (*pp)->next;
        free(to_free);
    }
}
```

---

## 总结

### 核心思想

> **二级指针的本质是：当你需要修改一个指针变量本身（而不是它指向的数据）时，必须传递该指针的地址。**

这与普通变量的道理相同：
- 修改 `int` 变量的值 → 传递 `int *`
- 修改 `int *` 变量的值 → 传递 `int **`

### 技巧总结

| 技巧 | 说明 | 示例 |
|------|------|------|
| 链表遍历 | `pp` 始终指向"指向当前节点的指针" | `pp = &head; pp = &(*pp)->next;` |
| 统一删除 | 无需区分头节点和中间节点 | `*pp = (*pp)->next;` |
| 函数输出 | 通过参数返回分配的内存 | `func(int **out)` + `*out = malloc(...)` |
| 递归修改 | 避免返回值，直接修改树结构 | `bst_insert(&root->left, val)` |

### 常见模式

#### 模式 1：链表遍历删除

```c
for (Node **pp = &head; *pp != NULL; ) {
    if (should_delete(*pp)) {
        Node *tmp = *pp;
        *pp = (*pp)->next;
        free(tmp);
    } else {
        pp = &(*pp)->next;
    }
}
```

#### 模式 2：函数输出参数

```c
int create_object(Object **out) {
    *out = malloc(sizeof(Object));
    if (*out == NULL) return -1;
    /* 初始化 *out ... */
    return 0;
}

/* 调用 */
Object *obj;
if (create_object(&obj) == 0) {
    /* 使用 obj */
}
```

#### 模式 3：查找并返回可修改的位置

```c
Node **find_node(Node **head, int value) {
    Node **pp = head;
    while (*pp != NULL && (*pp)->value != value) {
        pp = &(*pp)->next;
    }
    return pp;  /* 可直接用于插入或删除 */
}
```

#### 模式 4：递归树操作

```c
void tree_op(TreeNode **node, int value) {
    if (*node == NULL) {
        *node = create_node(value);
        return;
    }
    if (value < (*node)->value) {
        tree_op(&(*node)->left, value);
    } else {
        tree_op(&(*node)->right, value);
    }
}
```

### 应用场景速查表

| 场景 | 为什么用二级指针 | 关键代码 |
|------|------------------|----------|
| 链表删除 | 统一处理头节点和中间节点 | `*pp = (*pp)->next` |
| 函数修改指针 | 一级指针是值传递 | `*pp = malloc(...)` |
| 动态二维数组 | 行数运行时确定 | `int **arr` |
| 字符串数组 | 每个字符串长度不同 | `char **argv` |
| 树递归操作 | 避免返回新根 | `func(&root->left, val)` |
| 哈希表链表 | 每个桶是链表头 | `Entry **pp = &buckets[h]` |
| 输出参数 | 返回多个值/分配的内存 | `parse(&key, &value)` |

### 注意事项

1. **解引用顺序**：`(*pp)->next` 先解引用 `pp` 得到节点指针，再访问 `next`
2. **NULL 检查**：使用前总是检查 `*pp != NULL`
3. **内存管理**：删除节点时先保存指针再修改链接
4. **可读性**：复杂操作考虑添加注释或使用临时变量

