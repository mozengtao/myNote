# Iterator Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                     ITERATOR PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    +------------------+         +------------------+              |
|    |    Aggregate     |         |    Iterator      |              |
|    +------------------+  creates +------------------+              |
|    | + iterator()     |-------->| + first()        |              |
|    +--------+---------+         | + next()         |              |
|             |                   | + is_done()      |              |
|             |                   | + current()      |              |
|    +--------v---------+         +--------+---------+              |
|    | Concrete         |                  ^                        |
|    | Aggregate        |                  |                        |
|    +------------------+         +--------+---------+              |
|    | - elements[]     |         | Concrete         |              |
|    | + iterator()     |         | Iterator         |              |
|    +------------------+         +------------------+              |
|                                 | - aggregate      |              |
|                                 | - current_pos    |              |
|                                 +------------------+              |
|                                                                   |
|    Iterator provides sequential access to elements                |
|    without exposing underlying representation                     |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 迭代器模式封装集合遍历逻辑，提供统一遍历接口，无需关注集合内部结构。在Linux内核中，list_for_each、for_each_process等宏是迭代器模式的典型应用。它们提供统一的方式遍历链表、进程、设备等集合，隐藏了底层数据结构的细节。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: List Iterator Macros

```c
/* From: include/linux/list.h */

/**
 * list_for_each - iterate over a list
 * @pos: the &struct list_head to use as a loop cursor.
 * @head: the head for your list.
 *
 * Basic list iterator - traverse all entries.
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_safe - safe against removal
 * @pos: the &struct list_head to use as a loop cursor.
 * @n: another &struct list_head for temporary storage
 * @head: the head for your list.
 *
 * Safe version - allows removing current element during iteration.
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

/**
 * list_for_each_entry - iterate over list of given type
 * @pos: the type * to use as a loop cursor.
 * @head: the head for your list.
 * @member: the name of the list_struct within the struct.
 *
 * Typed iterator - returns the containing structure.
 */
#define list_for_each_entry(pos, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                    \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - safe typed iteration
 * @pos: the type * to use as a loop cursor.
 * @n: another type * for temporary storage
 * @head: the head for your list.
 * @member: the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)          \
    for (pos = list_entry((head)->next, typeof(*pos), member),  \
         n = list_entry(pos->member.next, typeof(*pos), member);\
         &pos->member != (head);                    \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))
```

### 2.2 Kernel Example: Device/Driver Iterators

```c
/* From: drivers/base/bus.c */

/**
 * bus_for_each_dev - device iterator
 * @bus: bus type
 * @start: device to start iterating from
 * @data: data for the callback
 * @fn: function to be called for each device
 *
 * Iterate over @bus's list of devices, and call @fn for each.
 */
int bus_for_each_dev(struct bus_type *bus, struct device *start,
                     void *data, int (*fn)(struct device *, void *))
{
    struct klist_iter i;
    struct device *dev;
    int error = 0;

    if (!bus)
        return -EINVAL;

    /* Initialize iterator */
    klist_iter_init_node(&bus->p->klist_devices, &i,
                         (start ? &start->p->knode_bus : NULL));
    
    /* Iterate until callback returns non-zero or list ends */
    while ((dev = next_device(&i)) && !error)
        error = fn(dev, data);
    
    /* Cleanup iterator */
    klist_iter_exit(&i);
    return error;
}

/**
 * driver_for_each_device - iterate over driver's devices
 * @drv: driver
 * @start: device to start with
 * @data: data for callback
 * @fn: callback function
 */
int driver_for_each_device(struct device_driver *drv, struct device *start,
                           void *data, int (*fn)(struct device *, void *))
{
    struct klist_iter i;
    struct device *dev;
    int error = 0;

    if (!drv)
        return -EINVAL;

    klist_iter_init_node(&drv->p->klist_devices, &i,
                         start ? &start->p->knode_driver : NULL);
    while ((dev = next_device(&i)) && !error)
        error = fn(dev, data);
    klist_iter_exit(&i);
    return error;
}
```

### 2.3 Kernel Example: Process Iterator

```c
/* From: include/linux/sched.h */

/**
 * for_each_process - iterate over all processes
 * @p: the &struct task_struct * to use as a loop cursor.
 *
 * Iterates through all processes in the system.
 */
#define for_each_process(p) \
    for (p = &init_task ; (p = next_task(p)) != &init_task ; )

/**
 * for_each_thread - iterate over all threads
 * @p: the task_struct of the main thread
 * @t: thread iterator
 */
#define for_each_thread(p, t) \
    for (t = p; (t = next_thread(t)) != p; )

/* From: kernel/sched/sched.h */

/**
 * for_each_domain - iterate over scheduling domains
 * @sd: pointer to sched_domain
 * @cpu: CPU number
 */
#define for_each_domain(cpu, sd) \
    for (sd = rcu_dereference(cpu_rq(cpu)->sd); sd; sd = sd->parent)
```

### 2.4 Kernel Example: klist Iterator

```c
/* From: lib/klist.c */

/**
 * struct klist_iter - Iterator structure
 *
 * Maintains iteration state for klist traversal.
 */
struct klist_iter {
    struct klist *i_klist;
    struct klist_node *i_cur;
};

/**
 * klist_iter_init_node - Initialize iterator from a specific node
 * @k: klist to iterate
 * @i: iterator structure
 * @n: starting node (or NULL for beginning)
 */
void klist_iter_init_node(struct klist *k, struct klist_iter *i,
                          struct klist_node *n)
{
    i->i_klist = k;
    i->i_cur = n;
    if (n)
        kref_get(&n->n_ref);
}

/**
 * klist_iter_exit - Cleanup iterator
 * @i: iterator to cleanup
 */
void klist_iter_exit(struct klist_iter *i)
{
    if (i->i_cur) {
        klist_put(i->i_cur, false);
        i->i_cur = NULL;
    }
}

/**
 * klist_next - Get next element in iteration
 * @i: iterator
 *
 * Returns next node or NULL if end reached.
 */
struct klist_node *klist_next(struct klist_iter *i)
{
    void (*put)(struct klist_node *) = i->i_klist->put;
    struct klist_node *last = i->i_cur;
    struct klist_node *next;

    spin_lock(&i->i_klist->k_lock);

    if (last) {
        next = to_klist_node(last->n_node.next);
    } else {
        next = to_klist_node(i->i_klist->k_list.next);
    }

    /* Skip dead nodes */
    while (next != to_klist_node(&i->i_klist->k_list) && 
           knode_dead(next))
        next = to_klist_node(next->n_node.next);

    if (next != to_klist_node(&i->i_klist->k_list)) {
        kref_get(&next->n_ref);
        i->i_cur = next;
    } else {
        i->i_cur = NULL;
    }

    spin_unlock(&i->i_klist->k_lock);

    if (last)
        put(last);

    return i->i_cur;
}
```

### 2.5 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL ITERATOR PATTERN                       |
|                   (list_for_each_entry)                           |
+------------------------------------------------------------------+
|                                                                   |
|    Data Structure: Linked List of Devices                         |
|                                                                   |
|    +------+     +------+     +------+     +------+                |
|    | head |<--->| dev1 |<--->| dev2 |<--->| dev3 |                |
|    +------+     +------+     +------+     +------+                |
|                                                                   |
|    Iterator Macro Usage:                                          |
|    +--------------------------------------------------------+     |
|    | struct device *dev;                                    |     |
|    | list_for_each_entry(dev, &device_list, list_member) {  |     |
|    |     printk("Device: %s\n", dev->name);                 |     |
|    | }                                                      |     |
|    +--------------------------------------------------------+     |
|                                                                   |
|    Macro Expansion:                                               |
|    +--------------------------------------------------------+     |
|    | for (dev = list_entry((&device_list)->next,            |     |
|    |                       typeof(*dev), list_member);      |     |
|    |      &dev->list_member != (&device_list);              |     |
|    |      dev = list_entry(dev->list_member.next,           |     |
|    |                       typeof(*dev), list_member))      |     |
|    +--------------------------------------------------------+     |
|                                                                   |
|    Iteration Steps:                                               |
|    1. Start: dev = container_of(head->next)                       |
|    2. Check: &dev->list_member != head                            |
|    3. Body: Execute loop body                                     |
|    4. Next: dev = container_of(dev->list_member.next)             |
|    5. Repeat from step 2                                          |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux内核的list_for_each_entry宏是迭代器模式的典型实现。它通过宏展开为for循环，使用container_of获取包含链表节点的结构体。迭代器隐藏了链表的内部细节（next指针、list_head结构），提供了统一的遍历接口。用户只需关心要遍历的结构体类型和链表成员名。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Uniform Interface** | Same iteration syntax for different data structures |
| **Encapsulation** | Hides internal structure of collection |
| **Single Responsibility** | Iteration logic separate from data structure |
| **Multiple Iterators** | Can have multiple active iterators on same collection |
| **Safe Iteration** | _safe variants allow modification during iteration |
| **Lazy Evaluation** | Elements accessed one at a time |

**中文说明：** 迭代器模式的优势包括：统一接口（不同数据结构使用相同遍历语法）、封装性（隐藏集合内部结构）、单一职责（遍历逻辑与数据结构分离）、多迭代器（同一集合可有多个活动迭代器）、安全遍历（_safe变体允许遍历中修改）、延迟求值（一次访问一个元素）。

---

## 4. User-Space Implementation Example

```c
/*
 * Iterator Pattern - User Space Implementation
 * Mimics Linux Kernel's list_for_each and klist_iter mechanism
 * 
 * Compile: gcc -o iterator iterator.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================
 * List Infrastructure (similar to kernel's list.h)
 * ============================================================ */

/* List head structure */
struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

/* Initialize list head */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/* Add entry to list */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    head->next->prev = new;
    new->next = head->next;
    new->prev = head;
    head->next = new;
}

/* Add entry to tail */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    head->prev->next = new;
    new->next = head;
    new->prev = head->prev;
    head->prev = new;
}

/* Remove entry from list */
static inline void list_del(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = NULL;
    entry->prev = NULL;
}

/* Check if list is empty */
static inline bool list_empty(const struct list_head *head)
{
    return head->next == head;
}

/* Get containing structure */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/* ============================================================
 * Iterator Macros (like kernel's list_for_each)
 * ============================================================ */

/**
 * list_for_each - iterate over a list
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_safe - iterate safe against removal
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

/**
 * list_for_each_entry - iterate over list of given type
 */
#define list_for_each_entry(pos, head, member)                  \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                                \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - safe typed iteration
 */
#define list_for_each_entry_safe(pos, n, head, member)          \
    for (pos = list_entry((head)->next, typeof(*pos), member),  \
         n = list_entry(pos->member.next, typeof(*pos), member);\
         &pos->member != (head);                                \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* ============================================================
 * Object-Oriented Iterator (like kernel's klist_iter)
 * ============================================================ */

/* Forward declarations */
struct collection;

/* Iterator structure */
struct iterator {
    struct collection *collection;
    struct list_head *current;
    bool started;
};

/* Collection structure */
struct collection {
    const char *name;
    struct list_head items;
    int count;
};

/* Iterator operations */
void iterator_init(struct iterator *iter, struct collection *coll)
{
    iter->collection = coll;
    iter->current = &coll->items;
    iter->started = false;
}

bool iterator_has_next(struct iterator *iter)
{
    if (!iter->started) {
        return !list_empty(&iter->collection->items);
    }
    return iter->current->next != &iter->collection->items;
}

void *iterator_next(struct iterator *iter, size_t offset)
{
    if (!iter->started) {
        iter->current = iter->collection->items.next;
        iter->started = true;
    } else {
        iter->current = iter->current->next;
    }
    
    if (iter->current == &iter->collection->items) {
        return NULL;
    }
    
    /* Return the containing structure */
    return (char *)iter->current - offset;
}

void iterator_reset(struct iterator *iter)
{
    iter->current = &iter->collection->items;
    iter->started = false;
}

/* ============================================================
 * Example Data Structures
 * ============================================================ */

/* Employee structure */
struct employee {
    int id;
    char name[64];
    int salary;
    struct list_head list;  /* List linkage */
};

/* Product structure */
struct product {
    int sku;
    char name[64];
    double price;
    int quantity;
    struct list_head list;
};

/* ============================================================
 * Collection Operations
 * ============================================================ */

struct collection *create_collection(const char *name)
{
    struct collection *coll = malloc(sizeof(struct collection));
    if (!coll) return NULL;
    
    coll->name = name;
    INIT_LIST_HEAD(&coll->items);
    coll->count = 0;
    
    printf("[Collection] Created '%s'\n", name);
    return coll;
}

void collection_add(struct collection *coll, struct list_head *item)
{
    list_add_tail(item, &coll->items);
    coll->count++;
}

/* ============================================================
 * Callback-Based Iterator (like kernel's bus_for_each_dev)
 * ============================================================ */

/* Callback type */
typedef int (*for_each_fn)(void *item, void *data);

/**
 * for_each_employee - iterate over employees with callback
 * @coll: collection
 * @data: user data for callback
 * @fn: callback function
 *
 * Similar to kernel's bus_for_each_dev()
 */
int for_each_employee(struct collection *coll, void *data, for_each_fn fn)
{
    struct employee *emp;
    int ret = 0;
    
    list_for_each_entry(emp, &coll->items, list) {
        ret = fn(emp, data);
        if (ret != 0) break;  /* Stop iteration if callback returns non-zero */
    }
    
    return ret;
}

/* ============================================================
 * Filter Iterator
 * ============================================================ */

struct filter_iterator {
    struct iterator base;
    bool (*filter)(void *item);
    size_t item_offset;
};

void filter_iterator_init(struct filter_iterator *fiter, 
                          struct collection *coll,
                          bool (*filter)(void *),
                          size_t offset)
{
    iterator_init(&fiter->base, coll);
    fiter->filter = filter;
    fiter->item_offset = offset;
}

void *filter_iterator_next(struct filter_iterator *fiter)
{
    void *item;
    
    while ((item = iterator_next(&fiter->base, fiter->item_offset)) != NULL) {
        if (fiter->filter(item)) {
            return item;
        }
    }
    
    return NULL;
}

/* ============================================================
 * Example Callbacks and Filters
 * ============================================================ */

/* Print employee callback */
int print_employee_cb(void *item, void *data)
{
    struct employee *emp = item;
    printf("  Employee #%d: %s ($%d)\n", emp->id, emp->name, emp->salary);
    return 0;
}

/* Sum salaries callback */
int sum_salary_cb(void *item, void *data)
{
    struct employee *emp = item;
    int *total = data;
    *total += emp->salary;
    return 0;
}

/* Find by ID callback */
int find_by_id_cb(void *item, void *data)
{
    struct employee *emp = item;
    int *target_id = data;
    return (emp->id == *target_id) ? 1 : 0;  /* Return 1 to stop */
}

/* Filter: high earners */
bool high_earner_filter(void *item)
{
    struct employee *emp = item;
    return emp->salary > 70000;
}

/* ============================================================
 * Main - Demonstrate Iterator Pattern
 * ============================================================ */

int main(void)
{
    struct collection *employees;
    struct employee *emp, *tmp;
    struct iterator iter;
    struct filter_iterator fiter;
    int total_salary = 0;
    int search_id;

    printf("=== Iterator Pattern Demo ===\n\n");

    /* Create collection */
    employees = create_collection("Employees");

    /* Add employees */
    printf("--- Adding Employees ---\n");
    
    struct employee emp1 = { .id = 1, .name = "Alice", .salary = 80000 };
    struct employee emp2 = { .id = 2, .name = "Bob", .salary = 65000 };
    struct employee emp3 = { .id = 3, .name = "Charlie", .salary = 90000 };
    struct employee emp4 = { .id = 4, .name = "Diana", .salary = 55000 };
    struct employee emp5 = { .id = 5, .name = "Eve", .salary = 75000 };
    
    collection_add(employees, &emp1.list);
    collection_add(employees, &emp2.list);
    collection_add(employees, &emp3.list);
    collection_add(employees, &emp4.list);
    collection_add(employees, &emp5.list);
    
    printf("Added %d employees\n\n", employees->count);

    /* Method 1: Macro-based iteration (like list_for_each_entry) */
    printf("--- Method 1: Macro Iterator ---\n");
    list_for_each_entry(emp, &employees->items, list) {
        printf("  [%d] %s: $%d\n", emp->id, emp->name, emp->salary);
    }

    /* Method 2: Object-oriented iterator */
    printf("\n--- Method 2: Object Iterator ---\n");
    iterator_init(&iter, employees);
    while (iterator_has_next(&iter)) {
        emp = iterator_next(&iter, offsetof(struct employee, list));
        if (emp) {
            printf("  [%d] %s: $%d\n", emp->id, emp->name, emp->salary);
        }
    }

    /* Method 3: Callback-based iteration */
    printf("\n--- Method 3: Callback Iterator ---\n");
    for_each_employee(employees, NULL, print_employee_cb);

    /* Calculate total salary using callback */
    printf("\n--- Sum Salaries (callback) ---\n");
    for_each_employee(employees, &total_salary, sum_salary_cb);
    printf("  Total salary: $%d\n", total_salary);

    /* Find employee by ID */
    printf("\n--- Find by ID (callback with early exit) ---\n");
    search_id = 3;
    int found = 0;
    list_for_each_entry(emp, &employees->items, list) {
        if (emp->id == search_id) {
            printf("  Found: %s (ID=%d)\n", emp->name, emp->id);
            found = 1;
            break;
        }
    }
    if (!found) printf("  Not found\n");

    /* Method 4: Filter iterator */
    printf("\n--- Method 4: Filter Iterator (salary > $70000) ---\n");
    filter_iterator_init(&fiter, employees, high_earner_filter, 
                         offsetof(struct employee, list));
    while ((emp = filter_iterator_next(&fiter)) != NULL) {
        printf("  High earner: %s ($%d)\n", emp->name, emp->salary);
    }

    /* Safe iteration with removal */
    printf("\n--- Safe Iteration with Removal ---\n");
    printf("Removing employees with salary < $60000:\n");
    list_for_each_entry_safe(emp, tmp, &employees->items, list) {
        if (emp->salary < 60000) {
            printf("  Removing: %s\n", emp->name);
            list_del(&emp->list);
            employees->count--;
        }
    }

    /* Verify removal */
    printf("\nRemaining employees:\n");
    list_for_each_entry(emp, &employees->items, list) {
        printf("  [%d] %s: $%d\n", emp->id, emp->name, emp->salary);
    }

    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    free(employees);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Iterator Comparison

```
+------------------------------------------------------------------+
|                   ITERATOR TYPES COMPARISON                       |
+------------------------------------------------------------------+
|                                                                   |
|  1. Macro-Based Iterator (list_for_each_entry)                    |
|     +----------------------------------------------------------+  |
|     | Pros: Zero overhead, compile-time expansion              |  |
|     | Cons: Not object-oriented, limited flexibility           |  |
|     | Use:  Simple iteration over known types                  |  |
|     +----------------------------------------------------------+  |
|                                                                   |
|  2. Object-Based Iterator (klist_iter)                            |
|     +----------------------------------------------------------+  |
|     | Pros: State encapsulation, multiple iterators            |  |
|     | Cons: Runtime overhead, memory for iterator struct       |  |
|     | Use:  Complex iteration, concurrent access               |  |
|     +----------------------------------------------------------+  |
|                                                                   |
|  3. Callback-Based Iterator (bus_for_each_dev)                    |
|     +----------------------------------------------------------+  |
|     | Pros: Flexible, can pass context data                    |  |
|     | Cons: Function call overhead, early exit via return code |  |
|     | Use:  Operations on each element, searching              |  |
|     +----------------------------------------------------------+  |
|                                                                   |
|  4. Filter Iterator (custom)                                      |
|     +----------------------------------------------------------+  |
|     | Pros: Selective iteration, composable                    |  |
|     | Cons: Additional overhead per element                    |  |
|     | Use:  When only subset of elements needed                |  |
|     +----------------------------------------------------------+  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 四种迭代器类型对比：1) 宏迭代器：零开销但不面向对象；2) 对象迭代器：状态封装但有运行时开销；3) 回调迭代器：灵活但有函数调用开销；4) 过滤迭代器：选择性遍历但每元素有额外开销。选择哪种取决于具体需求。

---

## 6. Key Implementation Points

1. **Container Macro**: Use container_of/list_entry to get containing struct
2. **Safe Variants**: Store next pointer before body for safe deletion
3. **State Encapsulation**: Object iterator stores current position
4. **Callback Pattern**: Pass function pointer for operation on each element
5. **Early Termination**: Return non-zero from callback to stop iteration
6. **Filter Support**: Wrap iterator with filter condition

**中文说明：** 实现迭代器模式的关键点：使用container_of/list_entry获取包含结构体、安全变体在循环体前保存next指针、对象迭代器封装当前位置状态、回调模式传递函数指针处理每个元素、回调返回非零可提前终止、用过滤条件包装迭代器实现过滤。

