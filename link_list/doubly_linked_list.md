# 双向链表 (Doubly Linked List)

## 1. 数据结构定义

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                         Doubly Linked List Structure                                │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│   HEAD                                                                   TAIL       │
│     │                                                                      │        │
│     ▼                                                                      ▼        │
│  ┌────────────┐     ┌────────────┐     ┌────────────┐     ┌────────────┐            │
│  │    prev    │◄────│    prev    │◄────│    prev    │◄────│    prev    │            │
│  ├────────────┤     ├────────────┤     ├────────────┤     ├────────────┤            │
│  │    data    │     │    data    │     │    data    │     │    data    │            │
│  ├────────────┤     ├────────────┤     ├────────────┤     ├────────────┤            │
│  │    next    │────►│    next    │────►│    next    │────►│    next    │            │
│  └────────────┘     └────────────┘     └────────────┘     └────────────┘            │
│       │                                                        │                    │
│       ▼                                                        ▼                    │
│     NULL                                                     NULL                   │
│                                                                                     │
│     Node 0             Node 1             Node 2             Node 3                 │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 双向链表的每个节点包含三部分：前驱指针（prev）、数据域（data）、后继指针（next）
- HEAD 指向第一个节点，TAIL 指向最后一个节点（可选）
- 可以从任意节点向前或向后遍历
- 第一个节点的 prev 为 NULL，最后一个节点的 next 为 NULL
- 相比单链表，多占用一个指针的空间，但操作更灵活

---

## 2. 节点结构

```
┌─────────────────────────────────────────────────────────────────┐
│                    Double Node Structure                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│                      ┌───────────────────┐                      │
│                      │       Node        │                      │
│                      ├──────┬──────┬─────┤                      │
│                      │ prev │ data │ next│                      │
│                      │ (ptr)│(any) │(ptr)│                      │
│                      └──┬───┴──────┴──┬──┘                      │
│                         │             │                         │
│           ┌─────────────┘             └─────────────┐           │
│           ▼                                         ▼           │
│      Prev Node                                 Next Node        │
│      or NULL                                   or NULL          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**说明**：
- prev：指向前一个节点的指针
- data：存储节点的实际数据
- next：指向下一个节点的指针

### 核心伪代码

```c
struct DNode {
    struct DNode *prev;  /* 前驱指针 */
    DataType data;       /* 数据域 */
    struct DNode *next;  /* 后继指针 */
};

struct DList {
    struct DNode *head;  /* 头指针 */
    struct DNode *tail;  /* 尾指针（可选，提高尾部操作效率） */
    int size;            /* 链表长度（可选） */
};
```

---

## 3. 常用操作

### 3.1 头部插入 (Insert at Head)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           Insert at Head Operation                                  │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  BEFORE:                                                                            │
│                                                                                     │
│   HEAD                                                                              │
│     │                                                                               │
│     ▼                                                                               │
│  ┌──────┐     ┌──────┐     ┌──────┐                                                 │
│  │  A   │◄═══►│  B   │◄═══►│  C   │                                                 │
│  └──────┘     └──────┘     └──────┘                                                 │
│                                                                                     │
│  NEW NODE:  ┌──────┐                                                                │
│             │  X   │                                                                │
│             └──────┘                                                                │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  STEPS:                                                                             │
│                                                                                     │
│  1. new->next = head           (X points to A)                                      │
│  2. new->prev = NULL           (X's prev is NULL)                                   │
│  3. head->prev = new           (A's prev points to X)                               │
│  4. head = new                 (update head pointer)                                │
│                                                                                     │
│             ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                         │
│   NULL ◄────│  X   │◄═══►│  A   │◄═══►│  B   │◄═══►│  C   │                         │
│             └──────┘     └──────┘     └──────┘     └──────┘                         │
│               ▲                                                                     │
│               │                                                                     │
│             HEAD                                                                    │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  AFTER:                                                                             │
│                                                                                     │
│   HEAD                                                                              │
│     │                                                                               │
│     ▼                                                                               │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                                    │
│  │  X   │◄═══►│  A   │◄═══►│  B   │◄═══►│  C   │                                    │
│  └──────┘     └──────┘     └──────┘     └──────┘                                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 创建新节点 X
- 设置 X 的 next 指向原头节点 A
- 设置 X 的 prev 为 NULL
- 设置原头节点 A 的 prev 指向 X
- 更新 HEAD 指向 X
- 时间复杂度：O(1)

### 核心伪代码

```c
void insert_at_head(DList *list, DataType data)
{
    DNode *new_node = malloc(sizeof(DNode));
    new_node->data = data;
    new_node->prev = NULL;
    new_node->next = list->head;

    if (list->head != NULL) {
        list->head->prev = new_node;  /* 原头节点的 prev 指向新节点 */
    } else {
        list->tail = new_node;        /* 空链表，tail 也指向新节点 */
    }

    list->head = new_node;
}
```

---

### 3.2 尾部插入 (Insert at Tail)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                           Insert at Tail Operation                                  │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  BEFORE:                                                                            │
│                                                                                     │
│   HEAD                                                              TAIL            │
│     │                                                                │              │
│     ▼                                                                ▼              │
│  ┌──────┐     ┌──────┐     ┌──────┐                                                 │
│  │  A   │◄═══►│  B   │◄═══►│  C   │────► NULL                                       │
│  └──────┘     └──────┘     └──────┘                                                 │
│                                                                                     │
│  NEW NODE:  ┌──────┐                                                                │
│             │  X   │                                                                │
│             └──────┘                                                                │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  STEPS:                                                                             │
│                                                                                     │
│  1. new->prev = tail           (X's prev points to C)                               │
│  2. new->next = NULL           (X's next is NULL)                                   │
│  3. tail->next = new           (C's next points to X)                               │
│  4. tail = new                 (update tail pointer)                                │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  AFTER:                                                                             │
│                                                                                     │
│   HEAD                                                                   TAIL       │
│     │                                                                      │        │
│     ▼                                                                      ▼        │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                                    │
│  │  A   │◄═══►│  B   │◄═══►│  C   │◄═══►│  X   │────► NULL                          │
│  └──────┘     └──────┘     └──────┘     └──────┘                                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 创建新节点 X
- 设置 X 的 prev 指向原尾节点 C
- 设置 X 的 next 为 NULL
- 设置原尾节点 C 的 next 指向 X
- 更新 TAIL 指向 X
- 时间复杂度：O(1)（有尾指针时）

### 核心伪代码

```c
void insert_at_tail(DList *list, DataType data)
{
    DNode *new_node = malloc(sizeof(DNode));
    new_node->data = data;
    new_node->next = NULL;
    new_node->prev = list->tail;

    if (list->tail != NULL) {
        list->tail->next = new_node;  /* 原尾节点的 next 指向新节点 */
    } else {
        list->head = new_node;        /* 空链表，head 也指向新节点 */
    }

    list->tail = new_node;
}
```

---

### 3.3 中间插入 (Insert at Position)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                        Insert at Position Operation                                 │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  BEFORE:  Insert X after node B                                                     │
│                                                                                     │
│  ┌──────┐     ┌──────┐     ┌──────┐                                                 │
│  │  A   │◄═══►│  B   │◄═══►│  C   │                                                 │
│  └──────┘     └──────┘     └──────┘                                                 │
│                 curr                                                                │
│                                                                                     │
│  NEW NODE:  ┌──────┐                                                                │
│             │  X   │                                                                │
│             └──────┘                                                                │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  STEPS:                                                                             │
│                                                                                     │
│  1. new->prev = curr           (X's prev = B)                                       │
│  2. new->next = curr->next     (X's next = C)                                       │
│  3. curr->next->prev = new     (C's prev = X)                                       │
│  4. curr->next = new           (B's next = X)                                       │
│                                                                                     │
│         ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                             │
│         │  A   │◄═══►│  B   │◄═══►│  X   │◄═══►│  C   │                             │
│         └──────┘     └──────┘     └──────┘     └──────┘                             │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  POINTER CHANGES DETAIL:                                                            │
│                                                                                     │
│                      ┌─────────────────────┐                                        │
│                      │                     │                                        │
│                      │    ┌──────┐         │                                        │
│                      │    │  X   │         │                                        │
│                      │    └──┬───┘         │                                        │
│                      │       │   ▲         │                                        │
│                      ▼       ▼   │         ▼                                        │
│         ┌──────┐     ┌──────┐   │   ┌──────┐                                        │
│         │  B   │─────│ next │   │   │  C   │                                        │
│         │      │◄────│ prev │───┘   │      │                                        │
│         └──────┘     └──────┘       └──────┘                                        │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 找到要插入位置的前驱节点 B
- 设置新节点 X 的 prev 指向 B
- 设置新节点 X 的 next 指向 C
- 设置 C 的 prev 指向 X
- 设置 B 的 next 指向 X
- 注意：必须按正确顺序修改指针，避免丢失引用
- 时间复杂度：O(n)

### 核心伪代码

```c
void insert_after(DList *list, DNode *curr, DataType data)
{
    if (curr == NULL)
        return;

    DNode *new_node = malloc(sizeof(DNode));
    new_node->data = data;

    new_node->prev = curr;           /* 步骤 1 */
    new_node->next = curr->next;     /* 步骤 2 */

    if (curr->next != NULL) {
        curr->next->prev = new_node; /* 步骤 3 */
    } else {
        list->tail = new_node;       /* 插入到尾部 */
    }

    curr->next = new_node;           /* 步骤 4 */
}
```

---

### 3.4 删除节点 (Delete Node)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          Delete Node Operation                                      │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  BEFORE:  Delete node B                                                             │
│                                                                                     │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                                    │
│  │  A   │◄═══►│  B   │◄═══►│  C   │◄═══►│  D   │                                    │
│  └──────┘     └──────┘     └──────┘     └──────┘                                    │
│               target                                                                │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  STEPS:                                                                             │
│                                                                                     │
│  1. target->prev->next = target->next    (A's next = C)                             │
│  2. target->next->prev = target->prev    (C's prev = A)                             │
│  3. free(target)                                                                    │
│                                                                                     │
│  POINTER CHANGES:                                                                   │
│                                                                                     │
│         ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                             │
│         │  A   │     │  B   │     │  C   │◄═══►│  D   │                             │
│         └──┬───┘     └──────┘     └──┬───┘     └──────┘                             │
│            │                         │                                              │
│            │         (deleted)       │                                              │
│            │                         │                                              │
│            └─────────────────────────┘                                              │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  AFTER:                                                                             │
│                                                                                     │
│  ┌──────┐     ┌──────┐     ┌──────┐                                                 │
│  │  A   │◄═══►│  C   │◄═══►│  D   │                                                 │
│  └──────┘     └──────┘     └──────┘                                                 │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 双向链表删除节点不需要遍历找前驱，因为每个节点都有 prev 指针
- 直接修改前驱和后继节点的指针
- 释放被删除节点的内存
- 需要处理边界情况（删除头节点或尾节点）
- 时间复杂度：O(1)（已知节点位置时）

### 核心伪代码

```c
void delete_node(DList *list, DNode *target)
{
    if (target == NULL)
        return;

    /* 处理前驱节点 */
    if (target->prev != NULL) {
        target->prev->next = target->next;
    } else {
        list->head = target->next;  /* 删除头节点 */
    }

    /* 处理后继节点 */
    if (target->next != NULL) {
        target->next->prev = target->prev;
    } else {
        list->tail = target->prev;  /* 删除尾节点 */
    }

    free(target);
}
```

---

### 3.5 双向遍历 (Bidirectional Traversal)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                      Bidirectional Traversal                                        │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  FORWARD TRAVERSAL (from head to tail):                                             │
│                                                                                     │
│   HEAD                                                                              │
│     │                                                                               │
│     ▼                                                                               │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                                    │
│  │  A   │────►│  B   │────►│  C   │────►│  D   │────► NULL                          │
│  └──────┘     └──────┘     └──────┘     └──────┘                                    │
│     1st         2nd         3rd         4th                                         │
│                                                                                     │
│  curr = head;                                                                       │
│  while (curr != NULL) {                                                             │
│      process(curr);                                                                 │
│      curr = curr->next;                                                             │
│  }                                                                                  │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  BACKWARD TRAVERSAL (from tail to head):                                            │
│                                                                                     │
│                                                                   TAIL              │
│                                                                     │               │
│                                                                     ▼               │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                                    │
│  │  A   │◄────│  B   │◄────│  C   │◄────│  D   │                                    │
│  └──────┘     └──────┘     └──────┘     └──────┘                                    │
│     4th         3rd         2nd         1st                                         │
│                                                                                     │
│  curr = tail;                                                                       │
│  while (curr != NULL) {                                                             │
│      process(curr);                                                                 │
│      curr = curr->prev;                                                             │
│  }                                                                                  │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 正向遍历：从 head 开始，沿 next 指针依次访问
- 反向遍历：从 tail 开始，沿 prev 指针依次访问
- 双向遍历是双向链表相比单链表的最大优势
- 时间复杂度：O(n)

### 核心伪代码

```c
/* 正向遍历 */
void traverse_forward(DList *list)
{
    DNode *curr = list->head;
    while (curr != NULL) {
        process(curr->data);
        curr = curr->next;
    }
}

/* 反向遍历 */
void traverse_backward(DList *list)
{
    DNode *curr = list->tail;
    while (curr != NULL) {
        process(curr->data);
        curr = curr->prev;
    }
}
```

---

### 3.6 反转链表 (Reverse)

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                          Reverse List Operation                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  BEFORE:                                                                            │
│                                                                                     │
│   HEAD                                                              TAIL            │
│     │                                                                │              │
│     ▼                                                                ▼              │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                                    │
│  │  A   │◄═══►│  B   │◄═══►│  C   │◄═══►│  D   │                                    │
│  └──────┘     └──────┘     └──────┘     └──────┘                                    │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  PROCESS: Swap prev and next for each node                                          │
│                                                                                     │
│  For each node:                                                                     │
│     temp = curr->prev                                                               │
│     curr->prev = curr->next                                                         │
│     curr->next = temp                                                               │
│                                                                                     │
│  Then swap head and tail pointers                                                   │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  AFTER:                                                                             │
│                                                                                     │
│   HEAD                                                              TAIL            │
│     │                                                                │              │
│     ▼                                                                ▼              │
│  ┌──────┐     ┌──────┐     ┌──────┐     ┌──────┐                                    │
│  │  D   │◄═══►│  C   │◄═══►│  B   │◄═══►│  A   │                                    │
│  └──────┘     └──────┘     └──────┘     └──────┘                                    │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 遍历每个节点，交换其 prev 和 next 指针
- 最后交换 head 和 tail 指针
- 时间复杂度：O(n)

### 核心伪代码

```c
void reverse(DList *list)
{
    DNode *curr = list->head;
    DNode *temp;

    /* 遍历所有节点，交换 prev 和 next */
    while (curr != NULL) {
        temp = curr->prev;
        curr->prev = curr->next;
        curr->next = temp;
        curr = curr->prev;  /* 注意：交换后用 prev 前进 */
    }

    /* 交换 head 和 tail */
    temp = list->head;
    list->head = list->tail;
    list->tail = temp;
}
```

---

## 4. 单链表 vs 双向链表 对比

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                    Singly vs Doubly Linked List Comparison                          │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  SINGLY LINKED LIST:                                                                │
│                                                                                     │
│  ┌──────┐     ┌──────┐     ┌──────┐                                                 │
│  │ data │────►│ data │────►│ data │────► NULL                                       │
│  │ next │     │ next │     │ next │                                                 │
│  └──────┘     └──────┘     └──────┘                                                 │
│                                                                                     │
│  Memory per node: data + 1 pointer                                                  │
│  Traversal: Forward only                                                            │
│  Delete known node: O(n) - need to find prev                                        │
│                                                                                     │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  DOUBLY LINKED LIST:                                                                │
│                                                                                     │
│  ┌──────┐     ┌──────┐     ┌──────┐                                                 │
│  │ prev │◄────│ prev │◄────│ prev │                                                 │
│  │ data │     │ data │     │ data │                                                 │
│  │ next │────►│ next │────►│ next │                                                 │
│  └──────┘     └──────┘     └──────┘                                                 │
│                                                                                     │
│  Memory per node: data + 2 pointers                                                 │
│  Traversal: Forward and Backward                                                    │
│  Delete known node: O(1) - have direct access to prev                               │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 单链表每个节点只有一个指针，内存占用少
- 双向链表每个节点有两个指针，但操作更灵活
- 双向链表删除已知节点只需 O(1)，单链表需要 O(n) 找前驱

---

## 5. 时间复杂度总结

| 操作 | 单链表 | 双向链表 | 说明 |
|------|--------|----------|------|
| 头部插入 | O(1) | O(1) | 相同 |
| 尾部插入 | O(n) / O(1)* | O(1) | *单链表有尾指针时 O(1) |
| 中间插入 | O(n) | O(n) | 需要遍历找位置 |
| 删除头节点 | O(1) | O(1) | 相同 |
| 删除尾节点 | O(n) | O(1) | 双向链表有 prev 指针 |
| 删除已知节点 | O(n) | O(1) | 双向链表不需找前驱 |
| 查找 | O(n) | O(n) | 相同 |
| 反向遍历 | O(n²) | O(n) | 单链表需反复从头遍历 |

---

## 6. 应用场景

### 6.1 适用场景

1. **浏览器历史记录**
   - 前进/后退功能需要双向遍历
   - 每个页面是一个节点

2. **LRU 缓存**
   - 最近最少使用缓存淘汰算法
   - 需要快速移动节点到头部
   - 配合哈希表实现 O(1) 访问

3. **文本编辑器**
   - 撤销/重做功能
   - 光标移动需要双向遍历

4. **音乐播放器**
   - 上一曲/下一曲功能
   - 播放列表管理

5. **操作系统**
   - 进程调度双向队列
   - 内存管理空闲块链表

6. **数据库**
   - B+ 树叶子节点链表
   - 支持范围查询的正反向扫描

### 6.2 Linux 内核中的双向链表

```c
/* Linux 内核链表定义 */
struct list_head {
    struct list_head *next, *prev;
};

/* 嵌入到数据结构中使用 */
struct my_struct {
    int data;
    struct list_head list;  /* 嵌入式链表节点 */
};
```

**说明**：Linux 内核使用侵入式（Intrusive）链表设计，链表节点嵌入到数据结构中，
通过 `container_of` 宏获取外层结构体指针。

---

## 7. 完整代码示例

```c
#include <stdio.h>
#include <stdlib.h>

/* 节点定义 */
typedef struct DNode {
    struct DNode *prev;
    int data;
    struct DNode *next;
} DNode;

/* 链表定义 */
typedef struct DList {
    DNode *head;
    DNode *tail;
} DList;

/* 初始化链表 */
DList *dlist_create(void)
{
    DList *list = malloc(sizeof(DList));
    list->head = NULL;
    list->tail = NULL;
    return list;
}

/* 头部插入 */
void dlist_insert_head(DList *list, int data)
{
    DNode *node = malloc(sizeof(DNode));
    node->data = data;
    node->prev = NULL;
    node->next = list->head;

    if (list->head != NULL)
        list->head->prev = node;
    else
        list->tail = node;

    list->head = node;
}

/* 尾部插入 */
void dlist_insert_tail(DList *list, int data)
{
    DNode *node = malloc(sizeof(DNode));
    node->data = data;
    node->next = NULL;
    node->prev = list->tail;

    if (list->tail != NULL)
        list->tail->next = node;
    else
        list->head = node;

    list->tail = node;
}

/* 删除节点 */
void dlist_delete(DList *list, DNode *target)
{
    if (target == NULL)
        return;

    if (target->prev != NULL)
        target->prev->next = target->next;
    else
        list->head = target->next;

    if (target->next != NULL)
        target->next->prev = target->prev;
    else
        list->tail = target->prev;

    free(target);
}

/* 查找节点 */
DNode *dlist_search(DList *list, int data)
{
    DNode *curr = list->head;
    while (curr != NULL) {
        if (curr->data == data)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/* 反转链表 */
void dlist_reverse(DList *list)
{
    DNode *curr = list->head;
    DNode *temp;

    while (curr != NULL) {
        temp = curr->prev;
        curr->prev = curr->next;
        curr->next = temp;
        curr = curr->prev;
    }

    temp = list->head;
    list->head = list->tail;
    list->tail = temp;
}

/* 正向打印 */
void dlist_print_forward(DList *list)
{
    DNode *curr = list->head;
    printf("Forward:  NULL <-> ");
    while (curr != NULL) {
        printf("%d <-> ", curr->data);
        curr = curr->next;
    }
    printf("NULL\n");
}

/* 反向打印 */
void dlist_print_backward(DList *list)
{
    DNode *curr = list->tail;
    printf("Backward: NULL <-> ");
    while (curr != NULL) {
        printf("%d <-> ", curr->data);
        curr = curr->prev;
    }
    printf("NULL\n");
}

/* 释放链表 */
void dlist_destroy(DList *list)
{
    DNode *curr = list->head;
    DNode *next;
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
    DList *list = dlist_create();

    /* 测试插入 */
    dlist_insert_head(list, 2);
    dlist_insert_head(list, 1);
    dlist_insert_tail(list, 3);
    dlist_insert_tail(list, 4);

    printf("After insertions:\n");
    dlist_print_forward(list);
    dlist_print_backward(list);

    /* 测试删除 */
    DNode *node = dlist_search(list, 2);
    if (node != NULL)
        dlist_delete(list, node);

    printf("\nAfter deleting 2:\n");
    dlist_print_forward(list);

    /* 测试反转 */
    dlist_reverse(list);
    printf("\nAfter reverse:\n");
    dlist_print_forward(list);
    dlist_print_backward(list);

    dlist_destroy(list);
    return 0;
}
```

### 编译运行

```bash
gcc -o doubly_list doubly_list.c
./doubly_list
```

### 输出

```
After insertions:
Forward:  NULL <-> 1 <-> 2 <-> 3 <-> 4 <-> NULL
Backward: NULL <-> 4 <-> 3 <-> 2 <-> 1 <-> NULL

After deleting 2:
Forward:  NULL <-> 1 <-> 3 <-> 4 <-> NULL

After reverse:
Forward:  NULL <-> 4 <-> 3 <-> 1 <-> NULL
Backward: NULL <-> 1 <-> 3 <-> 4 <-> NULL
```

---

## 8. 设计思想总结

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                      Doubly Linked List Design Philosophy                           │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                     │
│  TRADE-OFF:                                                                         │
│                                                                                     │
│     ┌───────────────────┐         ┌───────────────────┐                             │
│     │   Extra Memory    │ ◄─────► │  More Flexibility │                             │
│     │   (prev pointer)  │         │  (bidirectional)  │                             │
│     └───────────────────┘         └───────────────────┘                             │
│                                                                                     │
│  BENEFITS:                                                                          │
│                                                                                     │
│     1. O(1) deletion of known node                                                  │
│     2. Bidirectional traversal                                                      │
│     3. O(1) tail operations with tail pointer                                       │
│     4. Easier implementation of complex operations                                  │
│                                                                                     │
│  COSTS:                                                                             │
│                                                                                     │
│     1. Extra pointer per node (memory overhead)                                     │
│     2. More pointers to maintain during operations                                  │
│     3. Slightly more complex code                                                   │
│                                                                                     │
│  WHEN TO USE:                                                                       │
│                                                                                     │
│     - Need bidirectional traversal                                                  │
│     - Frequent deletions of known nodes                                             │
│     - Implementing LRU cache, undo/redo, etc.                                       │
│                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- 双向链表是空间换时间的典型例子
- 多一个指针的开销换来操作的灵活性
- 选择数据结构时需要根据具体应用场景权衡

