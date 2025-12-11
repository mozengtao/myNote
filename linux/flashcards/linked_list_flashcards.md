# Linked List Related Structures and API Flashcards for Linux Kernel Implementation

## 基础概念 (Basic Concepts)

Q: [Basic] Linux内核中有哪些类型的链表实现？
A: Linux内核提供多种链表实现：
```
+------------------------+---------------------------+
|      链表类型          |         用途              |
+------------------------+---------------------------+
| struct list_head       | 双向循环链表（最常用）    |
| struct hlist_head      | 哈希链表（节省空间）      |
| struct llist_head      | 无锁单链表（lock-free）   |
| RCU链表操作            | 读多写少场景              |
+------------------------+---------------------------+

设计特点：
1. 链表头嵌入到数据结构中（侵入式链表）
2. 使用container_of宏获取外部结构
3. 独立于具体数据类型
4. 操作通过内联函数和宏实现
```

Q: [Basic] 什么是侵入式链表(Intrusive Linked List)？
A: 侵入式链表将链表节点嵌入到数据结构中：
```c
/* 非侵入式链表（传统方式）*/
struct node {
    void *data;           // 指向实际数据
    struct node *next;
    struct node *prev;
};

/* 侵入式链表（Linux内核方式）*/
struct my_data {
    int value;
    char name[32];
    struct list_head list;  // 链表节点嵌入结构中
};

/* 优点 */
1. 减少内存分配次数（不需要单独分配节点）
2. 减少指针间接访问
3. 数据结构可以同时在多个链表中
4. 更好的缓存局部性

/* 示例：一个结构在多个链表中 */
struct task_struct {
    struct list_head tasks;     // 全部进程链表
    struct list_head children;  // 子进程链表
    struct list_head sibling;   // 兄弟进程链表
    struct list_head thread_group; // 线程组链表
    /* ... */
};
```

Q: [Basic] container_of宏的原理是什么？
A: container_of通过成员地址计算外部结构地址：
```c
/* 定义（include/linux/kernel.h）*/
#define container_of(ptr, type, member) ({                  \
    const typeof(((type *)0)->member) *__mptr = (ptr);      \
    (type *)((char *)__mptr - offsetof(type, member)); })

/* offsetof定义 */
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/* 原理图示 */
struct my_struct {
    int a;                    // offset 0
    char b;                   // offset 4
    struct list_head list;    // offset 8 (假设)
    int c;                    // offset 24
};

+---+---+---+---+---+---+---+---+...
| a | b |pad|   list_head   | c |
+---+---+---+---+---+---+---+---+...
^           ^
|           |
|           +-- ptr (list_head地址)
|
+-- container_of(ptr, my_struct, list)
    = ptr - offsetof(my_struct, list)
    = ptr - 8
    = 外部结构地址

/* 使用示例 */
struct list_head *pos;
list_for_each(pos, &head) {
    struct my_struct *entry = container_of(pos, struct my_struct, list);
    // 现在可以访问entry->a, entry->b等
}
```

---

## struct list_head (双向循环链表)

Q: [Basic] struct list_head的结构是什么？
A: list_head是双向循环链表的核心结构：
```c
/* include/linux/types.h */
struct list_head {
    struct list_head *next, *prev;
};

/* 特点 */
1. 双向：可以向前向后遍历
2. 循环：最后一个节点的next指向头节点
         头节点的prev指向最后一个节点
3. 空链表：next和prev都指向自己

/* 图示 */
空链表:
+------+
| head |<-+
+------+  |
| next |--+
| prev |--+
+------+

有三个节点的链表:
+------+   +------+   +------+   +------+
| head |-->| node1|-->| node2|-->| node3|--+
+------+   +------+   +------+   +------+  |
^                                          |
|                                          |
+------------------------------------------+

head.prev指向node3，node3.next指向head
```

Q: [Basic] 如何初始化链表头？
A: 有多种初始化方式：
```c
/* 1. 静态初始化（编译时）*/
#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

/* 使用 */
static LIST_HEAD(my_list);  // 声明并初始化

/* 2. 动态初始化（运行时）*/
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/* 使用 */
struct list_head my_list;
INIT_LIST_HEAD(&my_list);

/* 3. 结构体初始化器 */
struct my_device {
    struct list_head list;
    int id;
};

struct my_device dev = {
    .list = LIST_HEAD_INIT(dev.list),
    .id = 0,
};
```

Q: [Intermediate] list_add和list_add_tail的区别是什么？
A: 它们在链表不同位置插入节点：
```c
/* list_add - 添加到头部（后进先出，栈）*/
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

/* list_add_tail - 添加到尾部（先进先出，队列）*/
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/* __list_add的实现 */
static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/* 图示 */
初始: head <-> A <-> B <-> (back to head)

list_add(C, &head):     // C添加到head之后
head <-> C <-> A <-> B <-> (back to head)

list_add_tail(D, &head): // D添加到head之前（即末尾）
head <-> C <-> A <-> B <-> D <-> (back to head)

/* 使用场景 */
// 实现栈(LIFO)
list_add(&new_item->list, &stack_head);
entry = list_first_entry(&stack_head, struct item, list);

// 实现队列(FIFO)
list_add_tail(&new_item->list, &queue_head);
entry = list_first_entry(&queue_head, struct item, list);
```

Q: [Intermediate] 如何删除链表节点？
A: 使用list_del或list_del_init：
```c
/* list_del - 从链表中删除节点 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = LIST_POISON1;  // 0x00100100
    entry->prev = LIST_POISON2;  // 0x00200200
}

/* LIST_POISON的作用 */
// 设置为无效地址，帮助调试：
// 1. 检测对已删除节点的访问
// 2. 如果访问会触发页错误
// 3. 值选择避免与合法地址冲突

/* list_del_init - 删除后重新初始化 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);  // 可以安全地重新添加到链表
}

/* 使用场景对比 */
// list_del: 节点即将被释放
list_del(&entry->list);
kfree(entry);

// list_del_init: 节点可能被重新添加到链表
list_del_init(&entry->list);
/* ... 稍后 ... */
list_add(&entry->list, &other_list);
```

Q: [Basic] 如何检查链表状态？
A: 提供多个检查函数：
```c
/* 检查链表是否为空 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/* 更安全的空检查（防止并发修改）*/
static inline int list_empty_careful(const struct list_head *head)
{
    struct list_head *next = head->next;
    return (next == head) && (next == head->prev);
}

/* 检查是否只有一个节点 */
static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->next == head->prev);
}

/* 检查节点是否是最后一个 */
static inline int list_is_last(const struct list_head *list,
                               const struct list_head *head)
{
    return list->next == head;
}

/* 使用示例 */
if (list_empty(&my_list)) {
    pr_info("List is empty\n");
} else if (list_is_singular(&my_list)) {
    pr_info("List has exactly one element\n");
}
```

---

## list_head遍历宏 (Traversal Macros)

Q: [Intermediate] list_for_each系列宏的用法是什么？
A: 提供多种遍历方式：
```c
/* 1. 基本遍历（只获取list_head指针）*/
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

struct list_head *pos;
list_for_each(pos, &head) {
    struct my_struct *entry = container_of(pos, struct my_struct, list);
    /* 处理entry */
}

/* 2. 直接获取外部结构的遍历（最常用）*/
#define list_for_each_entry(pos, head, member)                \
    for (pos = list_entry((head)->next, typeof(*pos), member); \
         &pos->member != (head);                               \
         pos = list_entry(pos->member.next, typeof(*pos), member))

struct my_struct *entry;
list_for_each_entry(entry, &head, list) {
    /* 直接使用entry */
    pr_info("value: %d\n", entry->value);
}

/* 3. 反向遍历 */
#define list_for_each_entry_reverse(pos, head, member)        \
    for (pos = list_entry((head)->prev, typeof(*pos), member); \
         &pos->member != (head);                               \
         pos = list_entry(pos->member.prev, typeof(*pos), member))

list_for_each_entry_reverse(entry, &head, list) {
    /* 从尾到头遍历 */
}
```

Q: [Intermediate] 何时需要使用safe版本的遍历宏？
A: 当遍历过程中需要删除节点时：
```c
/* 问题：普通遍历中删除节点会导致崩溃 */
list_for_each_entry(entry, &head, list) {
    if (should_remove(entry)) {
        list_del(&entry->list);  // 危险！pos->next已失效
        kfree(entry);
    }
}

/* 解决方案：使用safe版本 */
#define list_for_each_entry_safe(pos, n, head, member)            \
    for (pos = list_entry((head)->next, typeof(*pos), member),    \
         n = list_entry(pos->member.next, typeof(*pos), member);  \
         &pos->member != (head);                                   \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

struct my_struct *entry, *tmp;
list_for_each_entry_safe(entry, tmp, &head, list) {
    if (should_remove(entry)) {
        list_del(&entry->list);  // 安全：tmp保存了下一个节点
        kfree(entry);
    }
}

/* safe版本原理 */
// 使用临时变量n/tmp预先保存下一个节点
// 即使当前节点被删除，仍能继续遍历

/* 其他safe变体 */
list_for_each_safe(pos, n, head)
list_for_each_entry_safe_reverse(pos, n, head, member)
list_for_each_entry_safe_continue(pos, n, head, member)
list_for_each_entry_safe_from(pos, n, head, member)
```

Q: [Intermediate] list_entry和list_first_entry的用法？
A: 用于从list_head获取外部结构：
```c
/* list_entry - container_of的别名 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/* list_first_entry - 获取第一个元素 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/* list_last_entry - 获取最后一个元素 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

/* list_next_entry - 获取下一个元素 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

/* list_prev_entry - 获取前一个元素 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

/* 使用示例 */
if (!list_empty(&my_list)) {
    struct my_struct *first = list_first_entry(&my_list, 
                                                struct my_struct, list);
    struct my_struct *last = list_last_entry(&my_list,
                                              struct my_struct, list);
    pr_info("First: %d, Last: %d\n", first->value, last->value);
}

/* 注意：list_first_entry在空链表上是未定义行为 */
// 使用前务必检查list_empty()
// 或使用list_first_entry_or_null
#define list_first_entry_or_null(ptr, type, member) \
    (!list_empty(ptr) ? list_first_entry(ptr, type, member) : NULL)
```

---

## 链表操作函数 (List Operations)

Q: [Intermediate] 如何移动和拼接链表？
A: 提供多个链表操作函数：
```c
/* list_move - 将节点移到另一个链表头部 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/* list_move_tail - 将节点移到另一个链表尾部 */
static inline void list_move_tail(struct list_head *list,
                                  struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/* list_splice - 将整个链表拼接到另一个链表 */
static inline void list_splice(const struct list_head *list,
                               struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->next);
}

/* list_splice_tail - 拼接到尾部 */
static inline void list_splice_tail(struct list_head *list,
                                    struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head->prev, head);
}

/* list_splice_init - 拼接后重新初始化源链表 */
static inline void list_splice_init(struct list_head *list,
                                    struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head, head->next);
        INIT_LIST_HEAD(list);
    }
}

/* 使用示例：合并两个链表 */
static LIST_HEAD(list1);
static LIST_HEAD(list2);

/* list1: A -> B -> C */
/* list2: X -> Y -> Z */

list_splice(&list2, &list1);
/* list1: X -> Y -> Z -> A -> B -> C */

list_splice_tail(&list2, &list1);
/* list1: A -> B -> C -> X -> Y -> Z */
```

Q: [Intermediate] list_replace的用法是什么？
A: 用于替换链表中的节点：
```c
/* list_replace - 用新节点替换旧节点 */
static inline void list_replace(struct list_head *old,
                                struct list_head *new)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

/* list_replace_init - 替换后初始化旧节点 */
static inline void list_replace_init(struct list_head *old,
                                     struct list_head *new)
{
    list_replace(old, new);
    INIT_LIST_HEAD(old);
}

/* 使用场景：更换数据结构但保持位置 */
struct my_struct old_entry, new_entry;
/* ... 初始化 ... */

/* 用new_entry替换old_entry在链表中的位置 */
list_replace(&old_entry.list, &new_entry.list);

/* 更常见的用法：切分链表 */
static inline void list_cut_position(struct list_head *list,
                                     struct list_head *head,
                                     struct list_head *entry)
/* 将head链表从entry处切分，前半部分移到list */
```

---

## struct hlist_head (哈希链表)

Q: [Intermediate] hlist_head和list_head有什么区别？
A: hlist设计用于哈希表，节省内存：
```c
/* 结构定义 */
struct hlist_head {
    struct hlist_node *first;  // 只有一个指针！
};

struct hlist_node {
    struct hlist_node *next;
    struct hlist_node **pprev;  // 指向指针的指针
};

/* 对比 */
+-----------------+------------------+------------------+
|     特性        |   list_head      |    hlist_head    |
+-----------------+------------------+------------------+
| 头节点大小      | 2个指针(16字节)  | 1个指针(8字节)   |
| 链表类型        | 双向循环         | 单向非循环       |
| 遍历方向        | 双向             | 仅向前           |
| 典型用途        | 通用链表         | 哈希表桶         |
+-----------------+------------------+------------------+

/* 为什么哈希表用hlist？*/
// 哈希表可能有很多桶，大多数桶是空的
// list_head: 1000个桶 = 16KB内存（全是头节点）
// hlist_head: 1000个桶 = 8KB内存（节省50%）

/* pprev的设计巧妙之处 */
// pprev指向"前一个节点的next指针"
// 对于第一个节点：pprev指向head->first
// 这样删除节点时不需要知道head

struct hlist_head head;
struct hlist_node A, B;

head.first -> A -> B -> NULL
              |    |
              |    +-- pprev指向&A.next
              +-- pprev指向&head.first
```

Q: [Intermediate] hlist的操作函数有哪些？
A: hlist提供类似list的操作：
```c
/* 初始化 */
#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = HLIST_HEAD_INIT

static inline void INIT_HLIST_HEAD(struct hlist_head *h)
{
    h->first = NULL;
}

static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

/* 检查状态 */
static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pprev;  // 不在任何链表中
}

static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

/* 删除节点 */
static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    *pprev = next;  // 前节点的next指向后节点
    if (next)
        next->pprev = pprev;  // 后节点的pprev指向前节点的next
}

/* 添加节点 */
static inline void hlist_add_head(struct hlist_node *n,
                                  struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

static inline void hlist_add_before(struct hlist_node *n,
                                    struct hlist_node *next);
static inline void hlist_add_after(struct hlist_node *n,
                                   struct hlist_node *prev);
```

Q: [Intermediate] hlist的遍历宏如何使用？
A: hlist遍历需要两个游标：
```c
/* 基本遍历 */
#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos ; pos = pos->next)

/* 安全遍历（可删除）*/
#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
         pos = n)

/* 遍历获取外部结构（注意：需要两个游标）*/
#define hlist_for_each_entry(tpos, pos, head, member)             \
    for (pos = (head)->first;                                      \
         pos &&                                                    \
         ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)

/* 使用示例 */
struct my_hash_entry {
    int key;
    int value;
    struct hlist_node node;
};

struct hlist_head hash_table[HASH_SIZE];
struct my_hash_entry *entry;
struct hlist_node *pos;

/* 遍历一个桶 */
int bucket = hash(key) % HASH_SIZE;
hlist_for_each_entry(entry, pos, &hash_table[bucket], node) {
    if (entry->key == key) {
        return entry->value;
    }
}

/* 安全删除 */
struct hlist_node *tmp;
hlist_for_each_entry_safe(entry, pos, tmp, &hash_table[bucket], node) {
    if (should_delete(entry)) {
        hlist_del(&entry->node);
        kfree(entry);
    }
}
```

---

## struct llist_head (无锁链表)

Q: [Advanced] 什么是无锁链表(Lock-free List)？
A: llist提供无锁的单链表操作：
```c
/* 结构定义（include/linux/llist.h）*/
struct llist_head {
    struct llist_node *first;
};

struct llist_node {
    struct llist_node *next;
};

/* 特点 */
1. 单向链表（只有next指针）
2. 只在头部操作
3. 使用原子操作(cmpxchg)实现无锁
4. 适合生产者-消费者模式

/* 设计限制 */
- 只能在头部添加
- 删除时删除所有节点
- 没有普通的删除单个节点操作
- 遍历只能在删除后进行
```

Q: [Advanced] llist的操作函数有哪些？
A: llist操作都是原子的：
```c
/* 初始化 */
#define LLIST_HEAD_INIT(name) { NULL }
#define LLIST_HEAD(name) struct llist_head name = LLIST_HEAD_INIT(name)

static inline void init_llist_head(struct llist_head *list)
{
    list->first = NULL;
}

/* 检查是否为空 */
static inline bool llist_empty(const struct llist_head *head)
{
    return ACCESS_ONCE(head->first) == NULL;
}

/* 添加节点（原子操作）*/
static inline bool llist_add(struct llist_node *new, struct llist_head *head)
{
    struct llist_node *entry, *old_entry;

    entry = head->first;
    for (;;) {
        old_entry = entry;
        new->next = entry;
        entry = cmpxchg(&head->first, old_entry, new);
        if (entry == old_entry)
            break;
    }

    return old_entry == NULL;  // 返回链表之前是否为空
}

/* 删除所有节点（原子操作）*/
static inline struct llist_node *llist_del_all(struct llist_head *head)
{
    return xchg(&head->first, NULL);
}

/* 删除第一个节点 */
extern struct llist_node *llist_del_first(struct llist_head *head);

/* 遍历宏 - 只能在删除后遍历 */
#define llist_for_each(pos, node) \
    for ((pos) = (node); pos; (pos) = (pos)->next)

#define llist_for_each_entry(pos, node, member)                 \
    for ((pos) = llist_entry((node), typeof(*(pos)), member);   \
         &(pos)->member != NULL;                                 \
         (pos) = llist_entry((pos)->member.next, typeof(*(pos)), member))
```

Q: [Advanced] llist的典型使用场景是什么？
A: llist适合无锁的生产者-消费者模式：
```c
/* 场景：中断处理延迟处理 */
struct work_item {
    struct llist_node node;
    void (*func)(struct work_item *);
    void *data;
};

static LLIST_HEAD(work_queue);

/* 生产者（可以在中断上下文）*/
void queue_work(struct work_item *item)
{
    /* 无锁添加，多个生产者可以并发 */
    if (llist_add(&item->node, &work_queue)) {
        /* 链表从空变非空，唤醒处理线程 */
        wake_up_process(worker_thread);
    }
}

/* 消费者（单一消费者）*/
void process_work(void)
{
    struct llist_node *list;
    struct work_item *item;

    /* 原子获取所有待处理项 */
    list = llist_del_all(&work_queue);
    
    /* 需要反转顺序（llist是LIFO）*/
    list = llist_reverse_order(list);
    
    /* 遍历处理 */
    llist_for_each_entry(item, list, node) {
        item->func(item);
    }
}

/* 反转链表顺序（lib/llist.c）*/
struct llist_node *llist_reverse_order(struct llist_node *head)
{
    struct llist_node *new_head = NULL;
    
    while (head) {
        struct llist_node *tmp = head;
        head = head->next;
        tmp->next = new_head;
        new_head = tmp;
    }
    return new_head;
}
```

---

## RCU链表操作 (RCU List Operations)

Q: [Advanced] 什么是RCU链表？
A: RCU链表允许读者无锁并发访问：
```c
/* RCU(Read-Copy-Update)特点 */
1. 读者不需要任何锁
2. 写者需要等待所有读者完成
3. 延迟释放被删除的元素

/* RCU链表 vs 普通链表 */
+----------------+------------------+-------------------+
|    操作        |    普通链表      |    RCU链表        |
+----------------+------------------+-------------------+
| 读遍历         | 需要锁           | rcu_read_lock     |
| 添加           | 需要锁           | 需要锁+发布语义   |
| 删除           | 需要锁           | 需要锁+延迟释放   |
| 读者阻塞       | 可能             | 永不              |
+----------------+------------------+-------------------+

/* 基本使用模式 */
读者：
    rcu_read_lock();
    list_for_each_entry_rcu(pos, head, member) {
        /* 访问pos */
    }
    rcu_read_unlock();

写者（添加）：
    spin_lock(&list_lock);
    list_add_rcu(&new->list, &head);
    spin_unlock(&list_lock);

写者（删除）：
    spin_lock(&list_lock);
    list_del_rcu(&entry->list);
    spin_unlock(&list_lock);
    synchronize_rcu();  // 或call_rcu
    kfree(entry);
```

Q: [Advanced] RCU链表操作函数有哪些？
A: include/linux/rculist.h提供RCU操作：
```c
/* 添加操作 */
static inline void list_add_rcu(struct list_head *new,
                                struct list_head *head)
{
    __list_add_rcu(new, head, head->next);
}

static inline void __list_add_rcu(struct list_head *new,
                                  struct list_head *prev,
                                  struct list_head *next)
{
    new->next = next;
    new->prev = prev;
    /* 关键：使用内存屏障确保对读者可见 */
    rcu_assign_pointer(list_next_rcu(prev), new);
    next->prev = new;
}

/* 删除操作 */
static inline void list_del_rcu(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->prev = LIST_POISON2;
    /* 注意：next不能设置为POISON，读者可能还在访问 */
}

/* 替换操作 */
static inline void list_replace_rcu(struct list_head *old,
                                    struct list_head *new)
{
    new->next = old->next;
    new->prev = old->prev;
    rcu_assign_pointer(list_next_rcu(new->prev), new);
    new->next->prev = new;
    old->prev = LIST_POISON2;
}

/* 遍历宏 */
#define list_for_each_entry_rcu(pos, head, member) \
    for (pos = list_entry_rcu((head)->next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry_rcu(pos->member.next, typeof(*pos), member))

/* list_entry_rcu使用rcu_dereference */
#define list_entry_rcu(ptr, type, member) \
    container_of(rcu_dereference_raw(ptr), type, member)
```

Q: [Advanced] RCU哈希链表操作如何使用？
A: hlist也有对应的RCU版本：
```c
/* 添加到哈希表 */
static inline void hlist_add_head_rcu(struct hlist_node *n,
                                      struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    n->pprev = &h->first;
    rcu_assign_pointer(hlist_first_rcu(h), n);
    if (first)
        first->pprev = &n->next;
}

/* 从哈希表删除 */
static inline void hlist_del_rcu(struct hlist_node *n)
{
    __hlist_del(n);
    n->pprev = LIST_POISON2;
}

/* 遍历哈希表 */
#define hlist_for_each_entry_rcu(tpos, pos, head, member)         \
    for (pos = rcu_dereference_raw(hlist_first_rcu(head));        \
         pos &&                                                    \
         ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = rcu_dereference_raw(hlist_next_rcu(pos)))

/* 完整示例：RCU哈希表 */
struct hash_entry {
    int key;
    int value;
    struct hlist_node hlist;
    struct rcu_head rcu;
};

DEFINE_SPINLOCK(hash_lock);
struct hlist_head hash_table[HASH_SIZE];

/* 查找（无锁）*/
struct hash_entry *lookup(int key)
{
    struct hash_entry *entry;
    struct hlist_node *pos;
    int bucket = hash(key) % HASH_SIZE;
    
    rcu_read_lock();
    hlist_for_each_entry_rcu(entry, pos, &hash_table[bucket], hlist) {
        if (entry->key == key) {
            rcu_read_unlock();
            return entry;
        }
    }
    rcu_read_unlock();
    return NULL;
}

/* 插入 */
void insert(struct hash_entry *entry)
{
    int bucket = hash(entry->key) % HASH_SIZE;
    
    spin_lock(&hash_lock);
    hlist_add_head_rcu(&entry->hlist, &hash_table[bucket]);
    spin_unlock(&hash_lock);
}

/* 删除 */
static void free_entry(struct rcu_head *rcu)
{
    struct hash_entry *entry = container_of(rcu, struct hash_entry, rcu);
    kfree(entry);
}

void delete(struct hash_entry *entry)
{
    spin_lock(&hash_lock);
    hlist_del_rcu(&entry->hlist);
    spin_unlock(&hash_lock);
    call_rcu(&entry->rcu, free_entry);
}
```

---

## 实际应用示例 (Practical Examples)

Q: [Intermediate] 如何实现一个简单的对象池？
A: 使用链表管理空闲对象：
```c
/* 对象池实现 */
struct object {
    int data;
    struct list_head list;
};

struct object_pool {
    spinlock_t lock;
    struct list_head free_list;
    struct list_head used_list;
    int free_count;
    int total_count;
};

/* 初始化对象池 */
int pool_init(struct object_pool *pool, int count)
{
    int i;
    
    spin_lock_init(&pool->lock);
    INIT_LIST_HEAD(&pool->free_list);
    INIT_LIST_HEAD(&pool->used_list);
    pool->free_count = 0;
    pool->total_count = 0;
    
    for (i = 0; i < count; i++) {
        struct object *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
        if (!obj)
            return -ENOMEM;
        
        INIT_LIST_HEAD(&obj->list);
        list_add(&obj->list, &pool->free_list);
        pool->free_count++;
        pool->total_count++;
    }
    
    return 0;
}

/* 分配对象 */
struct object *pool_alloc(struct object_pool *pool)
{
    struct object *obj = NULL;
    
    spin_lock(&pool->lock);
    if (!list_empty(&pool->free_list)) {
        obj = list_first_entry(&pool->free_list, struct object, list);
        list_move(&obj->list, &pool->used_list);
        pool->free_count--;
    }
    spin_unlock(&pool->lock);
    
    return obj;
}

/* 释放对象 */
void pool_free(struct object_pool *pool, struct object *obj)
{
    spin_lock(&pool->lock);
    list_move(&obj->list, &pool->free_list);
    pool->free_count++;
    spin_unlock(&pool->lock);
}

/* 销毁对象池 */
void pool_destroy(struct object_pool *pool)
{
    struct object *obj, *tmp;
    
    list_for_each_entry_safe(obj, tmp, &pool->free_list, list) {
        list_del(&obj->list);
        kfree(obj);
    }
    
    list_for_each_entry_safe(obj, tmp, &pool->used_list, list) {
        list_del(&obj->list);
        kfree(obj);
    }
}
```

Q: [Intermediate] 如何实现LRU缓存？
A: 使用链表实现LRU策略：
```c
/* LRU缓存实现 */
struct cache_entry {
    int key;
    int value;
    struct list_head list;      // LRU链表
    struct hlist_node hlist;    // 哈希表
};

struct lru_cache {
    spinlock_t lock;
    struct list_head lru_list;  // 最近最少使用在尾部
    struct hlist_head *hash;    // 哈希表
    int capacity;
    int count;
};

/* 访问时移到头部 */
void cache_touch(struct lru_cache *cache, struct cache_entry *entry)
{
    list_move(&entry->list, &cache->lru_list);
}

/* 获取值 */
int cache_get(struct lru_cache *cache, int key, int *value)
{
    struct cache_entry *entry;
    struct hlist_node *pos;
    int bucket = key % HASH_SIZE;
    int found = 0;
    
    spin_lock(&cache->lock);
    hlist_for_each_entry(entry, pos, &cache->hash[bucket], hlist) {
        if (entry->key == key) {
            *value = entry->value;
            cache_touch(cache, entry);  // 移到最近使用
            found = 1;
            break;
        }
    }
    spin_unlock(&cache->lock);
    
    return found;
}

/* 设置值 */
void cache_set(struct lru_cache *cache, int key, int value)
{
    struct cache_entry *entry;
    int bucket = key % HASH_SIZE;
    
    spin_lock(&cache->lock);
    
    /* 检查是否已存在 */
    entry = cache_lookup(cache, key);
    if (entry) {
        entry->value = value;
        cache_touch(cache, entry);
        spin_unlock(&cache->lock);
        return;
    }
    
    /* 如果满了，淘汰最久未使用的 */
    if (cache->count >= cache->capacity) {
        entry = list_last_entry(&cache->lru_list, struct cache_entry, list);
        hlist_del(&entry->hlist);
        list_del(&entry->list);
        kfree(entry);
        cache->count--;
    }
    
    /* 添加新条目 */
    entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
    if (entry) {
        entry->key = key;
        entry->value = value;
        hlist_add_head(&entry->hlist, &cache->hash[bucket]);
        list_add(&entry->list, &cache->lru_list);
        cache->count++;
    }
    
    spin_unlock(&cache->lock);
}
```

Q: [Advanced] 内核中链表的真实使用示例？
A: task_struct中的进程链表使用：
```c
/* include/linux/sched.h */
struct task_struct {
    /* ... */
    
    /* 进程链表 */
    struct list_head tasks;       // 系统中所有进程
    
    /* 子进程/兄弟进程 */
    struct list_head children;    // 子进程链表
    struct list_head sibling;     // 兄弟进程链表
    
    /* 线程组 */
    struct list_head thread_group; // 同一线程组
    struct list_head thread_node;  // 信号处理器链表
    
    /* ... */
};

/* 遍历所有进程 */
struct task_struct *task;

rcu_read_lock();
for_each_process(task) {  // 宏展开为list_for_each_entry
    printk("%s[%d]\n", task->comm, task->pid);
}
rcu_read_unlock();

/* for_each_process定义 */
#define for_each_process(p) \
    for (p = &init_task ; (p = next_task(p)) != &init_task ; )

#define next_task(p) \
    list_entry_rcu((p)->tasks.next, struct task_struct, tasks)

/* 遍历线程组 */
struct task_struct *t;

rcu_read_lock();
for_each_thread(task, t) {
    /* t遍历task所在线程组的所有线程 */
}
rcu_read_unlock();

/* 遍历子进程 */
struct task_struct *child;
list_for_each_entry(child, &task->children, sibling) {
    /* child是task的子进程 */
}
```

---

## 调试技巧 (Debugging Tips)

Q: [Intermediate] 如何调试链表问题？
A: 常见问题和调试方法：
```c
/* 1. 常见问题 */
- 使用未初始化的链表头
- 在遍历中删除节点但不用safe版本
- 多次删除同一节点
- 删除后访问节点
- 并发访问未加锁

/* 2. 启用链表调试 */
CONFIG_DEBUG_LIST=y

// 启用后会检查：
// - list_add时prev/next是否有效
// - list_del时是否已被删除
// - 指针是否被破坏

/* 3. LIST_POISON检测 */
// 访问已删除节点会触发页错误
// LIST_POISON1 = 0x00100100
// LIST_POISON2 = 0x00200200

/* 4. 添加检查代码 */
static inline void my_list_add(struct list_head *new,
                               struct list_head *head)
{
    BUG_ON(new == NULL);
    BUG_ON(head == NULL);
    BUG_ON(new->next != LIST_POISON1 && new->prev != LIST_POISON2);
    
    list_add(new, head);
}

/* 5. 打印链表内容 */
void dump_list(struct list_head *head, const char *name)
{
    struct my_struct *entry;
    int count = 0;
    
    pr_info("List %s contents:\n", name);
    list_for_each_entry(entry, head, list) {
        pr_info("  [%d] value=%d\n", count++, entry->value);
        if (count > 1000) {
            pr_err("  ... possible loop!\n");
            break;
        }
    }
    pr_info("Total: %d entries\n", count);
}

/* 6. 检查链表完整性 */
bool verify_list(struct list_head *head)
{
    struct list_head *pos, *prev = head;
    int count = 0;
    
    list_for_each(pos, head) {
        if (pos->prev != prev) {
            pr_err("Broken prev link at %d\n", count);
            return false;
        }
        if (prev->next != pos) {
            pr_err("Broken next link at %d\n", count);
            return false;
        }
        prev = pos;
        if (++count > 10000) {
            pr_err("Possible infinite loop\n");
            return false;
        }
    }
    
    return true;
}
```

Q: [Intermediate] 链表使用的最佳实践是什么？
A: 遵循以下最佳实践：
```c
/* 1. 始终初始化链表头 */
static LIST_HEAD(my_list);  // 静态
INIT_LIST_HEAD(&my_list);   // 动态

/* 2. 初始化嵌入的链表节点 */
struct my_struct *obj = kmalloc(...);
INIT_LIST_HEAD(&obj->list);

/* 3. 删除后可能重用时用list_del_init */
list_del_init(&entry->list);

/* 4. 遍历中可能删除节点时用safe版本 */
list_for_each_entry_safe(entry, tmp, &head, list) {
    if (condition)
        list_del(&entry->list);
}

/* 5. 多线程访问时正确加锁 */
spin_lock(&my_lock);
list_add(&entry->list, &head);
spin_unlock(&my_lock);

/* 6. 读多写少场景使用RCU */
rcu_read_lock();
list_for_each_entry_rcu(entry, &head, list) {
    /* 读取 */
}
rcu_read_unlock();

/* 7. 检查空链表后再获取第一个元素 */
if (!list_empty(&my_list)) {
    entry = list_first_entry(&my_list, struct my_struct, list);
}
// 或使用
entry = list_first_entry_or_null(&my_list, struct my_struct, list);

/* 8. 使用合适的链表类型 */
// 通用链表：list_head
// 哈希表：hlist_head
// 无锁场景：llist_head
// 读多写少：RCU链表
```

