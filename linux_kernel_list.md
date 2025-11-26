# Linux 内核链表操作深入讲解

基于 Linux 3.2 内核源码分析

---

## 目录

- [概述与设计理念](#概述与设计理念)
- [核心数据结构](#核心数据结构)
- [链表初始化](#链表初始化)
- [链表操作函数](#链表操作函数)
- [链表遍历宏](#链表遍历宏)
- [container_of 宏详解](#container_of-宏详解)
- [哈希链表 (hlist)](#哈希链表-hlist)
- [实际使用示例](#实际使用示例)
- [RCU 安全链表](#rcu-安全链表)
- [最佳实践与注意事项](#最佳实践与注意事项)

---

## 概述与设计理念

### 传统链表 vs Linux 内核链表

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         传统链表实现                                         │
│                                                                              │
│   struct node {                                                              │
│       int data;                                                              │
│       struct node *next;     // 特定于此数据类型                             │
│       struct node *prev;                                                     │
│   };                                                                         │
│                                                                              │
│   问题:                                                                       │
│   - 每种数据类型需要单独实现链表操作                                          │
│   - 代码重复                                                                  │
│   - 不通用                                                                    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                      Linux 内核链表实现                                       │
│                                                                              │
│   struct list_head {                                                         │
│       struct list_head *next, *prev;    // 通用链表节点                      │
│   };                                                                         │
│                                                                              │
│   struct my_struct {                                                         │
│       int data;                                                              │
│       char name[32];                                                         │
│       struct list_head list;    // 嵌入式链表节点                            │
│   };                                                                         │
│                                                                              │
│   优点:                                                                       │
│   - 一套通用的链表操作适用于所有数据类型                                      │
│   - 代码复用                                                                  │
│   - 类型无关                                                                  │
│   - 一个对象可同时在多个链表中                                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 核心设计思想

```
           传统方式: 数据包含链表                    Linux内核: 链表嵌入数据
                 
        ┌─────────────────┐                    ┌─────────────────┐
        │     data        │                    │     data        │
        ├─────────────────┤                    ├─────────────────┤
        │     next ───────┼──►                 │     name        │
        ├─────────────────┤                    ├─────────────────┤
        │     prev ◄──────┼──                  │ list_head       │
        └─────────────────┘                    │   next ─────────┼──►
                                               │   prev ◄────────┼──
                                               └─────────────────┘
                                                      │
                                                      │ 通过 container_of
                                                      │ 从 list_head 获取
                                                      ▼ 包含结构体指针
```

---

## 核心数据结构

### struct list_head

```c
// include/linux/types.h
struct list_head {
    struct list_head *next, *prev;
};
```

### 双向循环链表结构

```
                           头节点 (哨兵)
                              │
                              ▼
                        ┌──────────┐
               ┌────────│   head   │────────┐
               │        │next│prev │        │
               │        └──┬───┬───┘        │
               │           │   │            │
               │     ┌─────┘   └─────┐      │
               │     │               │      │
               │     ▼               ▼      │
               │ ┌──────────┐   ┌──────────┐│
               │ │  node1   │──►│  node2   ││
               │ │next│prev │   │next│prev ││
               │ └──┬───┬───┘   └──┬───┬───┘│
               │    │   │          │   │    │
               │    │   └──────────┼───┘    │
               │    │              │        │
               └────┼──────────────┼────────┘
                    │              │
                    └──────────────┘

特点:
- 双向: 可以从任意节点向前或向后遍历
- 循环: 首尾相连，head->prev 指向最后一个节点
- 对称: 头节点和普通节点结构相同
```

---

## 链表初始化

### 静态初始化

```c
// 方法1: 宏定义
#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

// 使用示例
LIST_HEAD(my_list);  // 定义并初始化一个链表头

// 展开后等价于:
struct list_head my_list = { &my_list, &my_list };
```

### 动态初始化

```c
// 方法2: 函数初始化
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

// 使用示例
struct list_head my_list;
INIT_LIST_HEAD(&my_list);
```

### 初始化后的状态

```
初始化后 (空链表):
                        
        ┌──────────┐
        │   head   │
        │   next ──┼──┐
        │   prev ──┼──┤
        └──────────┘  │
              ▲       │
              └───────┘
              
head->next == head
head->prev == head
```

---

## 链表操作函数

### 1. 添加节点

#### list_add - 头插法

```c
/**
 * list_add - 在头节点之后添加新节点 (栈式操作)
 * @new: 新节点
 * @head: 链表头
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

// 内部实现
static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}
```

**图解:**

```
添加前:                              添加后:
                                   
head ─────────► node1              head ─────► new ─────► node1
  ▲               │                  ▲           │          │
  └───────────────┘                  └───────────┴──────────┘

操作步骤:
1. next->prev = new     (node1->prev = new)
2. new->next = next     (new->next = node1)  
3. new->prev = prev     (new->prev = head)
4. prev->next = new     (head->next = new)
```

#### list_add_tail - 尾插法

```c
/**
 * list_add_tail - 在头节点之前添加新节点 (队列式操作)
 * @new: 新节点
 * @head: 链表头
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}
```

**图解:**

```
添加前:                              添加后:
                                   
head ◄─────────── node1            head ◄──── new ◄──── node1
  │                 ▲                │          ▲          ▲
  └─────────────────┘                └──────────┴──────────┘

新节点添加在 head 和 最后一个节点 之间
```

### 2. 删除节点

```c
/**
 * list_del - 从链表中删除节点
 * @entry: 要删除的节点
 */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = LIST_POISON1;  // 0x00100100
    entry->prev = LIST_POISON2;  // 0x00200200
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}
```

**图解:**

```
删除前:
prev ───────► entry ───────► next
  ▲             │              │
  └─────────────┴──────────────┘

删除后:
prev ─────────────────────► next
  ▲                           │
  └───────────────────────────┘

entry->next = POISON1 (调试用，访问会触发异常)
entry->prev = POISON2
```

#### list_del_init - 删除并重新初始化

```c
static inline void list_del_init(struct list_head *entry)
{
    __list_del_entry(entry);
    INIT_LIST_HEAD(entry);  // 可以安全地再次使用
}
```

### 3. 替换节点

```c
static inline void list_replace(struct list_head *old,
                                struct list_head *new)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}
```

### 4. 移动节点

```c
/**
 * list_move - 从一个链表移到另一个链表头部
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del_entry(list);
    list_add(list, head);
}

/**
 * list_move_tail - 从一个链表移到另一个链表尾部
 */
static inline void list_move_tail(struct list_head *list,
                                  struct list_head *head)
{
    __list_del_entry(list);
    list_add_tail(list, head);
}
```

### 5. 链表状态检查

```c
/**
 * list_empty - 检查链表是否为空
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/**
 * list_is_last - 检查节点是否是最后一个
 */
static inline int list_is_last(const struct list_head *list,
                               const struct list_head *head)
{
    return list->next == head;
}

/**
 * list_is_singular - 检查链表是否只有一个节点
 */
static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->next == head->prev);
}
```

### 6. 链表合并

```c
/**
 * list_splice - 将一个链表合并到另一个链表头部
 * @list: 要合并的链表
 * @head: 目标链表
 */
static inline void list_splice(const struct list_head *list,
                               struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->next);
}

static inline void __list_splice(const struct list_head *list,
                                 struct list_head *prev,
                                 struct list_head *next)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}
```

**图解:**

```
合并前:
head1: ──► A ──► B ──► (head1)
list:  ──► X ──► Y ──► (list)

list_splice(list, head1) 后:
head1: ──► X ──► Y ──► A ──► B ──► (head1)
```

### 7. 链表切分

```c
/**
 * list_cut_position - 将链表从指定位置切分
 * @list: 存放切分出的部分
 * @head: 原链表
 * @entry: 切分点
 */
static inline void list_cut_position(struct list_head *list,
                                     struct list_head *head,
                                     struct list_head *entry);
```

---

## 链表遍历宏

### 1. 基本遍历

```c
/**
 * list_for_each - 遍历链表
 * @pos: 循环游标
 * @head: 链表头
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

// 使用示例
struct list_head *pos;
list_for_each(pos, &my_list) {
    // pos 指向当前 list_head 节点
}
```

### 2. 反向遍历

```c
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)
```

### 3. 安全遍历 (可删除节点)

```c
/**
 * list_for_each_safe - 安全遍历，允许删除当前节点
 * @pos: 循环游标
 * @n: 临时变量，保存下一个节点
 * @head: 链表头
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

// 使用示例 - 删除所有节点
struct list_head *pos, *n;
list_for_each_safe(pos, n, &my_list) {
    list_del(pos);  // 安全删除
    // 释放内存...
}
```

### 4. 获取包含结构体的遍历 ★★★

```c
/**
 * list_for_each_entry - 遍历链表，直接获取包含结构体指针
 * @pos: 包含结构体类型的指针
 * @head: 链表头
 * @member: list_head 在结构体中的成员名
 */
#define list_for_each_entry(pos, head, member)                      \
    for (pos = list_entry((head)->next, typeof(*pos), member);      \
         &pos->member != (head);                                    \
         pos = list_entry(pos->member.next, typeof(*pos), member))

// 使用示例
struct my_struct {
    int value;
    struct list_head list;
};

struct my_struct *entry;
list_for_each_entry(entry, &my_list, list) {
    printk("value = %d\n", entry->value);
}
```

### 5. 安全遍历包含结构体

```c
/**
 * list_for_each_entry_safe - 安全遍历，允许删除
 */
#define list_for_each_entry_safe(pos, n, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member),      \
         n = list_entry(pos->member.next, typeof(*pos), member);    \
         &pos->member != (head);                                    \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

// 使用示例 - 删除所有节点并释放内存
struct my_struct *entry, *tmp;
list_for_each_entry_safe(entry, tmp, &my_list, list) {
    list_del(&entry->list);
    kfree(entry);
}
```

### 6. 其他遍历变体

```c
// 反向遍历获取结构体
list_for_each_entry_reverse(pos, head, member)

// 从当前位置继续遍历
list_for_each_entry_continue(pos, head, member)

// 从当前位置开始遍历
list_for_each_entry_from(pos, head, member)
```

---

## container_of 宏详解

### 定义

```c
// include/linux/kernel.h
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);        \
    (type *)( (char *)__mptr - offsetof(type, member) );})
```

### 工作原理

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       container_of 原理图解                                  │
│                                                                              │
│   struct my_struct {                                                         │
│       int    a;           // offset = 0                                      │
│       char   b[10];       // offset = 4                                      │
│       struct list_head list;  // offset = 16 (假设)                          │
│       long   c;           // offset = 32                                     │
│   };                                                                         │
│                                                                              │
│   内存布局:                                                                   │
│   ┌───────┬──────────┬─────────────┬───────┐                                │
│   │   a   │   b[10]  │    list     │   c   │                                │
│   │(4B)   │  (10B)   │next│prev    │ (8B)  │                                │
│   └───────┴──────────┴──┬──┴───────┴───────┘                                │
│   ▲                     ▲                                                    │
│   │                     │                                                    │
│   结构体起始地址         list 成员地址 (ptr)                                  │
│   (container_of 返回值)                                                       │
│                                                                              │
│   计算公式:                                                                   │
│   结构体地址 = list成员地址 - offsetof(struct my_struct, list)                │
│            = ptr - 16                                                        │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 代码分解

```c
#define container_of(ptr, type, member) ({
    // 1. 类型检查: 确保 ptr 类型正确
    const typeof( ((type *)0)->member ) *__mptr = (ptr);
    
    // 2. 计算偏移并返回容器指针
    (type *)( (char *)__mptr - offsetof(type, member) );
})

// offsetof 宏
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
```

### list_entry 宏

```c
// list_entry 是 container_of 的别名
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

// 使用示例
struct my_struct *obj = list_entry(list_ptr, struct my_struct, list);
```

### list_first_entry 宏

```c
/**
 * list_first_entry - 获取链表第一个元素的包含结构体
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

// 使用示例
struct my_struct *first = list_first_entry(&my_list, struct my_struct, list);
```

---

## 哈希链表 (hlist)

### 为什么需要 hlist?

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      普通链表 vs 哈希链表                                     │
│                                                                              │
│   普通链表头 (list_head):                                                     │
│   ┌──────────┐                                                               │
│   │   next   │ 8 bytes                                                       │
│   │   prev   │ 8 bytes     共 16 bytes                                       │
│   └──────────┘                                                               │
│                                                                              │
│   哈希链表头 (hlist_head):                                                    │
│   ┌──────────┐                                                               │
│   │  first   │ 8 bytes     共 8 bytes (节省一半!)                             │
│   └──────────┘                                                               │
│                                                                              │
│   优势: 哈希表通常有大量桶(bucket)，节省内存很重要                             │
│   代价: 失去 O(1) 访问尾部的能力                                               │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 数据结构

```c
// include/linux/types.h
struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next;
    struct hlist_node **pprev;  // 指向前一个节点的 next 指针
};
```

### hlist 结构示意

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          hlist 结构                                          │
│                                                                              │
│   hlist_head                                                                 │
│   ┌────────┐                                                                 │
│   │ first ─┼──────────────────────────┐                                     │
│   └────────┘                          │                                     │
│       ▲                               ▼                                     │
│       │                          ┌─────────┐     ┌─────────┐                │
│       │                          │ node1   │     │ node2   │                │
│       │                          │ next ───┼────►│ next ───┼──► NULL        │
│       │                          │ pprev ──┼─┐   │ pprev ──┼─┐              │
│       │                          └─────────┘ │   └─────────┘ │              │
│       │                               ▲      │        ▲      │              │
│       └───────────────────────────────┼──────┘        │      │              │
│                                       └───────────────┼──────┘              │
│                                                       │                      │
│   pprev 指向前一个节点的 next 字段地址                  │                      │
│   对于第一个节点，pprev 指向 head->first               │                      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### hlist 操作函数

```c
// 初始化
#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = { .first = NULL }
static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

// 添加到头部
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

// 删除节点
static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    *pprev = next;           // 前一个节点的 next 指向下一个节点
    if (next)
        next->pprev = pprev; // 下一个节点的 pprev 指向前一个节点的 next
}

// 检查是否在链表中
static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pprev;
}
```

### hlist 遍历宏

```c
#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos ; pos = pos->next)

#define hlist_for_each_entry(tpos, pos, head, member)           \
    for (pos = (head)->first;                                   \
         pos &&                                                 \
         ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)

#define hlist_for_each_entry_safe(tpos, pos, n, head, member)   \
    for (pos = (head)->first;                                   \
         pos && ({ n = pos->next; 1; }) &&                      \
         ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = n)
```

---

## 实际使用示例

### 示例1: 进程管理中的链表

```c
// include/linux/sched.h
struct task_struct {
    // ...
    struct list_head tasks;      // 所有进程链表
    struct list_head children;   // 子进程链表
    struct list_head sibling;    // 兄弟进程链表
    struct list_head cg_list;    // cgroup 链表
    // ...
};

// 遍历所有进程
#define for_each_process(p) \
    list_for_each_entry(p, &init_task.tasks, tasks)

// 使用示例 (kernel/cpu.c)
struct task_struct *p;
for_each_process(p) {
    if (task_cpu(p) == cpu && p->state == TASK_RUNNING)
        printk("Task %s is on cpu %d\n", p->comm, cpu);
}
```

### 示例2: 完整的链表使用

```c
#include <linux/list.h>
#include <linux/slab.h>

// 定义数据结构
struct my_device {
    int id;
    char name[32];
    struct list_head list;  // 嵌入链表节点
};

// 定义链表头
static LIST_HEAD(device_list);

// 添加设备
int add_device(int id, const char *name)
{
    struct my_device *dev;
    
    dev = kmalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;
    
    dev->id = id;
    strncpy(dev->name, name, sizeof(dev->name));
    
    // 添加到链表尾部
    list_add_tail(&dev->list, &device_list);
    
    return 0;
}

// 查找设备
struct my_device *find_device(int id)
{
    struct my_device *dev;
    
    list_for_each_entry(dev, &device_list, list) {
        if (dev->id == id)
            return dev;
    }
    return NULL;
}

// 删除设备
void remove_device(int id)
{
    struct my_device *dev, *tmp;
    
    list_for_each_entry_safe(dev, tmp, &device_list, list) {
        if (dev->id == id) {
            list_del(&dev->list);
            kfree(dev);
            return;
        }
    }
}

// 清空所有设备
void cleanup_all_devices(void)
{
    struct my_device *dev, *tmp;
    
    list_for_each_entry_safe(dev, tmp, &device_list, list) {
        list_del(&dev->list);
        kfree(dev);
    }
}

// 打印所有设备
void print_all_devices(void)
{
    struct my_device *dev;
    
    if (list_empty(&device_list)) {
        printk("No devices\n");
        return;
    }
    
    list_for_each_entry(dev, &device_list, list) {
        printk("Device: id=%d, name=%s\n", dev->id, dev->name);
    }
}
```

### 示例3: 内核中的实际使用 (调度器)

```c
// kernel/sched_fair.c
static inline void list_add_leaf_cfs_rq(struct cfs_rq *cfs_rq)
{
    if (!cfs_rq->on_list) {
        if (cfs_rq->tg->parent &&
            cfs_rq->tg->parent->cfs_rq[cpu_of(rq_of(cfs_rq))]->on_list) {
            list_add_rcu(&cfs_rq->leaf_cfs_rq_list,
                &rq_of(cfs_rq)->leaf_cfs_rq_list);
        } else {
            list_add_tail_rcu(&cfs_rq->leaf_cfs_rq_list,
                &rq_of(cfs_rq)->leaf_cfs_rq_list);
        }
        cfs_rq->on_list = 1;
    }
}

// 遍历所有 CFS 运行队列
#define for_each_leaf_cfs_rq(rq, cfs_rq) \
    list_for_each_entry_rcu(cfs_rq, &rq->leaf_cfs_rq_list, leaf_cfs_rq_list)
```

---

## RCU 安全链表

### RCU 链表操作

```c
// include/linux/rculist.h

// RCU 安全的添加
static inline void list_add_rcu(struct list_head *new, struct list_head *head)
{
    __list_add_rcu(new, head, head->next);
}

// RCU 安全的删除
static inline void list_del_rcu(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->prev = LIST_POISON2;
}

// RCU 安全的遍历
#define list_for_each_entry_rcu(pos, head, member) \
    for (pos = list_entry_rcu((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry_rcu(pos->member.next, typeof(*pos), member))
```

### RCU 使用模式

```c
// 读取端 (不需要锁)
rcu_read_lock();
list_for_each_entry_rcu(entry, &my_list, list) {
    // 安全读取
}
rcu_read_unlock();

// 写入端 (需要适当的锁)
spin_lock(&my_lock);
list_add_rcu(&new_entry->list, &my_list);
spin_unlock(&my_lock);

// 删除后需要等待
spin_lock(&my_lock);
list_del_rcu(&old_entry->list);
spin_unlock(&my_lock);
synchronize_rcu();  // 等待所有读者完成
kfree(old_entry);   // 安全释放
```

---

## 最佳实践与注意事项

### 1. 遍历时删除必须使用 _safe 版本

```c
// 错误! 删除后 pos->member.next 无效
list_for_each_entry(pos, head, member) {
    if (should_delete(pos)) {
        list_del(&pos->member);  // pos 被破坏
        kfree(pos);
    }
}

// 正确做法
list_for_each_entry_safe(pos, tmp, head, member) {
    if (should_delete(pos)) {
        list_del(&pos->member);
        kfree(pos);
    }
}
```

### 2. 并发访问需要加锁

```c
// 读写都需要保护
spin_lock(&list_lock);
list_for_each_entry(entry, &my_list, list) {
    // 操作...
}
spin_unlock(&list_lock);

// 或使用 RCU (读多写少场景)
```

### 3. 删除后的节点状态

```c
// list_del 后节点被 poison
list_del(&entry->list);
// entry->next = LIST_POISON1
// entry->prev = LIST_POISON2
// 此时 list_empty(&entry->list) 返回 false!

// 如需重新使用，用 list_del_init
list_del_init(&entry->list);
// 节点恢复到初始状态，可安全重新加入链表
```

### 4. 空链表检查

```c
// 操作前检查
if (list_empty(&my_list))
    return;  // 链表为空

// 获取第一个元素前检查
if (!list_empty(&my_list)) {
    struct my_struct *first = list_first_entry(&my_list, 
                                                struct my_struct, list);
}
```

---

## 关键源码文件

| 文件 | 功能 |
|------|------|
| `include/linux/list.h` | 链表核心实现 |
| `include/linux/rculist.h` | RCU 安全链表 |
| `include/linux/rculist_bl.h` | 位锁 RCU 链表 |
| `include/linux/llist.h` | 无锁单向链表 |
| `include/linux/types.h` | list_head/hlist_head 定义 |
| `include/linux/kernel.h` | container_of 定义 |

---

## 总结

### Linux 内核链表核心要点

1. **嵌入式设计**: `list_head` 嵌入到数据结构中
2. **container_of**: 从成员指针获取容器指针
3. **双向循环**: 高效的插入、删除、遍历
4. **类型无关**: 一套 API 适用于所有数据类型

### API 速查表

| 函数/宏 | 功能 |
|---------|------|
| `LIST_HEAD(name)` | 静态定义并初始化链表 |
| `INIT_LIST_HEAD(head)` | 动态初始化链表 |
| `list_add(new, head)` | 头插法添加 |
| `list_add_tail(new, head)` | 尾插法添加 |
| `list_del(entry)` | 删除节点 |
| `list_del_init(entry)` | 删除并重新初始化 |
| `list_move(list, head)` | 移动到另一链表头 |
| `list_empty(head)` | 检查是否为空 |
| `list_entry(ptr, type, member)` | 获取包含结构体 |
| `list_for_each_entry(pos, head, member)` | 遍历 |
| `list_for_each_entry_safe(pos, n, head, member)` | 安全遍历 |

---

*本文档基于 Linux 3.2 内核源码 `include/linux/list.h` 分析*

