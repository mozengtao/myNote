# 单链表 (Singly Linked List)

## 1. 数据结构定义

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Singly Linked List Structure                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐        │
│  │   data   │     │   data   │     │   data   │     │   data   │        │
│  ├──────────┤     ├──────────┤     ├──────────┤     ├──────────┤        │
│  │   next ──┼────►│   next ──┼────►│   next ──┼────►│   next ──┼──► NULL│
│  └──────────┘     └──────────┘     └──────────┘     └──────────┘        │
│     Node 0           Node 1           Node 2           Node 3           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 单链表由一系列节点组成，每个节点包含两部分：数据域（data）和指针域（next）
- HEAD 是头指针，指向链表的第一个节点
- 每个节点的 next 指针指向下一个节点
- 最后一个节点的 next 指针为 NULL，表示链表结束
- 只能从头到尾单向遍历，无法反向访问

---

## 2. 节点结构

```
┌─────────────────────────────────────────┐
│           Single Node Structure         │
├─────────────────────────────────────────┤
│                                         │
│        ┌─────────────────────┐          │
│        │      Node           │          │
│        ├──────────┬──────────┤          │
│        │   data   │  next    │          │
│        │  (any)   │ (ptr)    │          │
│        └──────────┴────┬─────┘          │
│                        │                │
│                        ▼                │
│                   Next Node             │
│                   or NULL               │
│                                         │
└─────────────────────────────────────────┘
```

**说明**：
- data：存储节点的实际数据，可以是任意类型
- next：指向下一个节点的指针，类型为节点指针

### 核心伪代码

```c
struct Node {
    DataType data;      /* 数据域 */
    struct Node *next;  /* 指针域，指向下一个节点 */
};

struct List {
    struct Node *head;  /* 头指针 */
    int size;           /* 链表长度（可选） */
};
```

---

## 3. 常用操作

### 3.1 头部插入 (Insert at Head)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Insert at Head Operation                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  BEFORE:                                                                │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐                                     │
│  │  A   │────►│  B   │────►│  C   │────► NULL                           │
│  └──────┘     └──────┘     └──────┘                                     │
│                                                                         │
│  NEW NODE:  ┌──────┐                                                    │
│             │  X   │                                                    │
│             └──────┘                                                    │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  AFTER:                                                                 │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                        │
│  │  X   │────►│  A   │────►│  B   │────►│  C   │────► NULL              │
│  └──────┘     └──────┘     └──────┘     └──────┘                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 创建新节点 X
- 将新节点的 next 指向原来的头节点 A
- 更新 HEAD 指针指向新节点 X
- 时间复杂度：O(1)

### 核心伪代码

```c
void insert_at_head(List *list, DataType data)
{
    Node *new_node = malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = list->head;  /* 新节点指向原头节点 */
    list->head = new_node;        /* 更新头指针 */
}
```

---

### 3.2 尾部插入 (Insert at Tail)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Insert at Tail Operation                           │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  BEFORE:                                                                │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐                                     │
│  │  A   │────►│  B   │────►│  C   │────► NULL                           │
│  └──────┘     └──────┘     └──────┘                                     │
│                               ▲                                         │
│                               │                                         │
│                             tail                                        │
│                                                                         │
│  NEW NODE:  ┌──────┐                                                    │
│             │  X   │────► NULL                                          │
│             └──────┘                                                    │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  AFTER:                                                                 │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                        │
│  │  A   │────►│  B   │────►│  C   │────►│  X   │────► NULL              │
│  └──────┘     └──────┘     └──────┘     └──────┘                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 创建新节点 X，其 next 指向 NULL
- 遍历链表找到最后一个节点 C（其 next 为 NULL）
- 将 C 的 next 指向新节点 X
- 时间复杂度：O(n)，需要遍历整个链表

### 核心伪代码

```c
void insert_at_tail(List *list, DataType data)
{
    Node *new_node = malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = NULL;

    if (list->head == NULL) {
        list->head = new_node;  /* 空链表，直接作为头节点 */
        return;
    }

    Node *curr = list->head;
    while (curr->next != NULL) {  /* 遍历到最后一个节点 */
        curr = curr->next;
    }
    curr->next = new_node;        /* 尾节点指向新节点 */
}
```

---

### 3.3 中间插入 (Insert at Position)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   Insert at Position Operation                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  BEFORE:  Insert X after node B (position 1)                            │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐                                     │
│  │  A   │────►│  B   │────►│  C   │────► NULL                           │
│  └──────┘     └──────┘     └──────┘                                     │
│               pos=1                                                     │
│                                                                         │
│  NEW NODE:  ┌──────┐                                                    │
│             │  X   │                                                    │
│             └──────┘                                                    │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  STEP 1: Find node at position                                          │
│                                                                         │
│  ┌──────┐     ┌──────┐ ─ ─ ─ ─ ─ ─►┌──────┐                             │
│  │  A   │────►│  B   │             │  C   │────► NULL                   │
│  └──────┘     └──────┘             └──────┘                             │
│                  │        ┌──────┐    ▲                                 │
│                  │        │  X   │    │                                 │
│                  │        └──┬───┘    │                                 │
│                  │           │        │                                 │
│                  └───────────┼────────┘                                 │
│                              │                                          │
│  STEP 2: X->next = B->next   │                                          │
│  STEP 3: B->next = X ────────┘                                          │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  AFTER:                                                                 │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                        │
│  │  A   │────►│  B   │────►│  X   │────►│  C   │────► NULL              │
│  └──────┘     └──────┘     └──────┘     └──────┘                        │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 首先遍历找到指定位置的节点 B
- 创建新节点 X
- 将 X 的 next 指向 B 的 next（即 C）
- 将 B 的 next 指向 X
- 注意：必须先执行步骤 2，再执行步骤 3，否则会丢失对 C 的引用
- 时间复杂度：O(n)

### 核心伪代码

```c
void insert_after_position(List *list, int pos, DataType data)
{
    Node *curr = list->head;
    int i;

    /* 遍历到指定位置 */
    for (i = 0; i < pos && curr != NULL; i++) {
        curr = curr->next;
    }

    if (curr == NULL)
        return;  /* 位置无效 */

    Node *new_node = malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = curr->next;  /* 先连接后继 */
    curr->next = new_node;        /* 再连接前驱 */
}
```

---

### 3.4 删除节点 (Delete Node)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                       Delete Node Operation                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  BEFORE:  Delete node B                                                 │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                        │
│  │  A   │────►│  B   │────►│  C   │────►│  D   │────► NULL              │
│  └──────┘     └──────┘     └──────┘     └──────┘                        │
│    prev        target                                                   │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  STEP 1: Find prev node (A) and target node (B)                         │
│  STEP 2: prev->next = target->next                                      │
│                                                                         │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                        │
│  │  A   │─┐   │  B   │     │  C   │────►│  D   │────► NULL              │
│  └──────┘ │   └──────┘     └──────┘     └──────┘                        │
│           │                    ▲                                        │
│           └────────────────────┘                                        │
│                                                                         │
│  STEP 3: free(target)                                                   │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  AFTER:                                                                 │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐                                     │
│  │  A   │────►│  C   │────►│  D   │────► NULL                           │
│  └──────┘     └──────┘     └──────┘                                     │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 找到要删除节点 B 的前驱节点 A
- 将 A 的 next 指向 B 的 next（即 C）
- 释放节点 B 的内存
- 关键：必须保存前驱节点的引用，因为单链表无法回溯
- 时间复杂度：O(n)

### 核心伪代码

```c
void delete_node(List *list, DataType data)
{
    Node *prev = NULL;
    Node *curr = list->head;

    /* 遍历查找目标节点 */
    while (curr != NULL && curr->data != data) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL)
        return;  /* 未找到 */

    if (prev == NULL) {
        /* 删除头节点 */
        list->head = curr->next;
    } else {
        /* 删除中间或尾节点 */
        prev->next = curr->next;
    }

    free(curr);
}
```

---

### 3.5 查找节点 (Search)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Search Operation                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Search for value "C":                                                  │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                        │
│  │  A   │────►│  B   │────►│  C   │────►│  D   │────► NULL              │
│  └──────┘     └──────┘     └──────┘     └──────┘                        │
│     │            │            │                                         │
│     ▼            ▼            ▼                                         │
│   A!=C         B!=C         C==C                                        │
│  continue     continue      FOUND!                                      │
│                             return                                      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 从头节点开始，逐个比较节点的数据
- 找到匹配的节点则返回
- 遍历到 NULL 仍未找到则返回失败
- 时间复杂度：O(n)

### 核心伪代码

```c
Node *search(List *list, DataType data)
{
    Node *curr = list->head;

    while (curr != NULL) {
        if (curr->data == data)
            return curr;  /* 找到 */
        curr = curr->next;
    }

    return NULL;  /* 未找到 */
}
```

---

### 3.6 反转链表 (Reverse)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Reverse List Operation                             │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  BEFORE:                                                                │
│                                                                         │
│   HEAD                                                                  │
│     │                                                                   │
│     ▼                                                                   │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                        │
│  │  A   │────►│  B   │────►│  C   │────►│  D   │────► NULL              │
│  └──────┘     └──────┘     └──────┘     └──────┘                        │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  PROCESS: Three pointers - prev, curr, next                             │
│                                                                         │
│  Step 1:  prev=NULL, curr=A                                             │
│           NULL ◄──── A ────► B ────► C ────► D ────► NULL               │
│                                                                         │
│  Step 2:  prev=A, curr=B                                                │
│           NULL ◄──── A ◄──── B ────► C ────► D ────► NULL               │
│                                                                         │
│  Step 3:  prev=B, curr=C                                                │
│           NULL ◄──── A ◄──── B ◄──── C ────► D ────► NULL               │
│                                                                         │
│  Step 4:  prev=C, curr=D                                                │
│           NULL ◄──── A ◄──── B ◄──── C ◄──── D      curr=NULL           │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  AFTER:                                                                 │
│                                                                         │
│                                             HEAD                        │
│                                               │                         │
│                                               ▼                         │
│       NULL ◄────┌──────┐◄────┌──────┐◄────┌──────┐◄────┌──────┐         │
│                 │  A   │     │  B   │     │  C   │     │  D   │         │
│                 └──────┘     └──────┘     └──────┘     └──────┘         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 使用三个指针：prev（前驱）、curr（当前）、next（后继）
- 遍历链表，逐个反转每个节点的 next 指针方向
- 最后将 HEAD 指向原来的尾节点
- 时间复杂度：O(n)，空间复杂度：O(1)

### 核心伪代码

```c
void reverse(List *list)
{
    Node *prev = NULL;
    Node *curr = list->head;
    Node *next = NULL;

    while (curr != NULL) {
        next = curr->next;   /* 保存下一个节点 */
        curr->next = prev;   /* 反转指针方向 */
        prev = curr;         /* prev 前进 */
        curr = next;         /* curr 前进 */
    }

    list->head = prev;       /* 更新头指针 */
}
```

---

## 4. 时间复杂度总结

| 操作 | 时间复杂度 | 说明 |
|------|-----------|------|
| 头部插入 | O(1) | 直接修改头指针 |
| 尾部插入 | O(n) | 需要遍历到尾部 |
| 中间插入 | O(n) | 需要遍历到指定位置 |
| 删除节点 | O(n) | 需要找到前驱节点 |
| 查找 | O(n) | 顺序遍历 |
| 反转 | O(n) | 遍历一次 |
| 获取长度 | O(n) | 需要遍历计数 |

---

## 5. 应用场景

### 5.1 适用场景

1. **栈的实现**
   - 使用头部插入和删除，O(1) 时间复杂度
   - 后进先出（LIFO）特性

2. **队列的实现**（配合尾指针）
   - 尾部插入，头部删除
   - 先进先出（FIFO）特性

3. **多项式表示**
   - 每个节点存储系数和指数
   - 方便进行多项式运算

4. **稀疏矩阵**
   - 只存储非零元素
   - 节省存储空间

5. **内存管理**
   - 空闲块链表
   - 动态内存分配

6. **操作系统**
   - 进程调度队列
   - 文件系统目录结构

### 5.2 不适用场景

1. **频繁随机访问** - 数组更合适
2. **需要双向遍历** - 双向链表更合适
3. **需要快速查找** - 哈希表或树更合适

---

## 6. 完整代码示例

```c
#include <stdio.h>
#include <stdlib.h>

/* 节点定义 */
typedef struct Node {
    int data;
    struct Node *next;
} Node;

/* 链表定义 */
typedef struct List {
    Node *head;
} List;

/* 初始化链表 */
List *list_create(void)
{
    List *list = malloc(sizeof(List));
    list->head = NULL;
    return list;
}

/* 头部插入 */
void list_insert_head(List *list, int data)
{
    Node *node = malloc(sizeof(Node));
    node->data = data;
    node->next = list->head;
    list->head = node;
}

/* 尾部插入 */
void list_insert_tail(List *list, int data)
{
    Node *node = malloc(sizeof(Node));
    node->data = data;
    node->next = NULL;

    if (list->head == NULL) {
        list->head = node;
        return;
    }

    Node *curr = list->head;
    while (curr->next != NULL)
        curr = curr->next;
    curr->next = node;
}

/* 删除节点 */
void list_delete(List *list, int data)
{
    Node *prev = NULL;
    Node *curr = list->head;

    while (curr != NULL && curr->data != data) {
        prev = curr;
        curr = curr->next;
    }

    if (curr == NULL)
        return;

    if (prev == NULL)
        list->head = curr->next;
    else
        prev->next = curr->next;

    free(curr);
}

/* 查找节点 */
Node *list_search(List *list, int data)
{
    Node *curr = list->head;
    while (curr != NULL) {
        if (curr->data == data)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/* 反转链表 */
void list_reverse(List *list)
{
    Node *prev = NULL;
    Node *curr = list->head;
    Node *next;

    while (curr != NULL) {
        next = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next;
    }
    list->head = prev;
}

/* 打印链表 */
void list_print(List *list)
{
    Node *curr = list->head;
    printf("List: ");
    while (curr != NULL) {
        printf("%d -> ", curr->data);
        curr = curr->next;
    }
    printf("NULL\n");
}

/* 释放链表 */
void list_destroy(List *list)
{
    Node *curr = list->head;
    Node *next;
    while (curr != NULL) {
        next = curr->next;
        free(curr);
        curr = next;
    }
    free(list);
}

/* 测试 */
int main(void)
{
    List *list = list_create();

    list_insert_head(list, 3);
    list_insert_head(list, 2);
    list_insert_head(list, 1);
    list_print(list);  /* List: 1 -> 2 -> 3 -> NULL */

    list_insert_tail(list, 4);
    list_print(list);  /* List: 1 -> 2 -> 3 -> 4 -> NULL */

    list_delete(list, 2);
    list_print(list);  /* List: 1 -> 3 -> 4 -> NULL */

    list_reverse(list);
    list_print(list);  /* List: 4 -> 3 -> 1 -> NULL */

    list_destroy(list);
    return 0;
}
```

### 编译运行

```bash
gcc -o singly_list singly_list.c
./singly_list
```

### 输出

```
List: 1 -> 2 -> 3 -> NULL
List: 1 -> 2 -> 3 -> 4 -> NULL
List: 1 -> 3 -> 4 -> NULL
List: 4 -> 3 -> 1 -> NULL
```

