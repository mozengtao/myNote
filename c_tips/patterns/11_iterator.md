# 迭代器模式 (Iterator Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      ITERATOR PATTERN                             |
+------------------------------------------------------------------+

    WITHOUT ITERATOR (Exposed Internal Structure):
    
    // Client needs to know internal structure!
    for (int i = 0; i < list->size; i++) {
        process(list->items[i]);
    }
    
    // Different structure = different code!
    node = tree->root;
    while (node != NULL) {
        process(node->data);
        node = node->next;
    }


    WITH ITERATOR (Uniform Access):
    
    +------------------+         +------------------+
    |    Collection    | creates |     Iterator     |
    |   (Aggregate)    |-------->|                  |
    +------------------+         +------------------+
    | create_iterator()|         | has_next()       |
    +------------------+         | next()           |
                                 | current()        |
                                 | reset()          |
                                 +------------------+


    UNIFORM ITERATION:
    
    // Same code works for any collection!
    iterator = collection->create_iterator();
    while (iterator->has_next(iterator)) {
        item = iterator->next(iterator);
        process(item);
    }


    MULTIPLE ITERATORS ON SAME COLLECTION:
    
    +------------------+
    |    Collection    |
    |  [A] [B] [C] [D] |
    +--------+---------+
             |
    +--------+--------+--------+
    |                 |        |
    v                 v        v
    Iterator1      Iterator2   Iterator3
    current: B     current: D  current: A
    
    Each iterator maintains its own position!
```

**核心思想说明：**
- 提供统一的方式遍历不同类型的集合
- 不暴露集合的内部结构
- 每个迭代器独立维护遍历状态
- 支持同一集合的多个并发遍历

## 实现思路

1. **定义迭代器接口**：`has_next()`, `next()`, `reset()`
2. **集合提供创建迭代器方法**：返回迭代器实例
3. **迭代器持有集合引用**：访问集合元素
4. **维护遍历状态**：当前位置等

## 典型应用场景

- 容器/集合遍历
- 文件系统遍历
- 数据库结果集
- 树/图遍历
- 自定义数据结构遍历

## 完整代码示例

```c
/*============================================================================
 * 迭代器模式示例 - 通用集合遍历
 *============================================================================*/

/*---------------------------------------------------------------------------
 * iterator.h - 迭代器接口定义
 *---------------------------------------------------------------------------*/
#ifndef ITERATOR_H
#define ITERATOR_H

#include <stdbool.h>

/* 关键点：迭代器接口 */
typedef struct iterator iterator_t;

typedef struct {
    bool (*has_next)(iterator_t *iter);
    void* (*next)(iterator_t *iter);
    void* (*current)(iterator_t *iter);
    void (*reset)(iterator_t *iter);
    void (*destroy)(iterator_t *iter);
} iterator_ops_t;

struct iterator {
    const iterator_ops_t *ops;
    void *collection;
    void *state;
};

/* 便捷宏 */
#define ITER_HAS_NEXT(iter)  ((iter)->ops->has_next(iter))
#define ITER_NEXT(iter)      ((iter)->ops->next(iter))
#define ITER_CURRENT(iter)   ((iter)->ops->current(iter))
#define ITER_RESET(iter)     ((iter)->ops->reset(iter))
#define ITER_DESTROY(iter)   ((iter)->ops->destroy(iter))

#endif /* ITERATOR_H */


/*---------------------------------------------------------------------------
 * array_list.h - 数组列表定义
 *---------------------------------------------------------------------------*/
#ifndef ARRAY_LIST_H
#define ARRAY_LIST_H

#include "iterator.h"
#include <stddef.h>

typedef struct {
    void **items;
    size_t size;
    size_t capacity;
} array_list_t;

array_list_t* array_list_create(size_t initial_capacity);
void array_list_destroy(array_list_t *list);
bool array_list_add(array_list_t *list, void *item);
void* array_list_get(array_list_t *list, size_t index);
size_t array_list_size(array_list_t *list);

/* 关键点：创建迭代器 */
iterator_t* array_list_create_iterator(array_list_t *list);

#endif /* ARRAY_LIST_H */


/*---------------------------------------------------------------------------
 * array_list.c - 数组列表实现
 *---------------------------------------------------------------------------*/
#include "array_list.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

array_list_t* array_list_create(size_t initial_capacity) {
    array_list_t *list = malloc(sizeof(array_list_t));
    if (list == NULL) return NULL;
    
    list->items = malloc(sizeof(void*) * initial_capacity);
    if (list->items == NULL) {
        free(list);
        return NULL;
    }
    
    list->size = 0;
    list->capacity = initial_capacity;
    
    return list;
}

void array_list_destroy(array_list_t *list) {
    if (list != NULL) {
        free(list->items);
        free(list);
    }
}

bool array_list_add(array_list_t *list, void *item) {
    if (list->size >= list->capacity) {
        size_t new_capacity = list->capacity * 2;
        void **new_items = realloc(list->items, sizeof(void*) * new_capacity);
        if (new_items == NULL) return false;
        
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    list->items[list->size++] = item;
    return true;
}

void* array_list_get(array_list_t *list, size_t index) {
    if (index >= list->size) return NULL;
    return list->items[index];
}

size_t array_list_size(array_list_t *list) {
    return list->size;
}

/* 数组列表迭代器状态 */
typedef struct {
    size_t current_index;
} array_list_iter_state_t;

static bool array_list_iter_has_next(iterator_t *iter) {
    array_list_t *list = (array_list_t *)iter->collection;
    array_list_iter_state_t *state = (array_list_iter_state_t *)iter->state;
    return state->current_index < list->size;
}

static void* array_list_iter_next(iterator_t *iter) {
    array_list_t *list = (array_list_t *)iter->collection;
    array_list_iter_state_t *state = (array_list_iter_state_t *)iter->state;
    
    if (state->current_index >= list->size) {
        return NULL;
    }
    
    return list->items[state->current_index++];
}

static void* array_list_iter_current(iterator_t *iter) {
    array_list_t *list = (array_list_t *)iter->collection;
    array_list_iter_state_t *state = (array_list_iter_state_t *)iter->state;
    
    if (state->current_index == 0 || state->current_index > list->size) {
        return NULL;
    }
    
    return list->items[state->current_index - 1];
}

static void array_list_iter_reset(iterator_t *iter) {
    array_list_iter_state_t *state = (array_list_iter_state_t *)iter->state;
    state->current_index = 0;
}

static void array_list_iter_destroy(iterator_t *iter) {
    free(iter->state);
    free(iter);
}

static const iterator_ops_t array_list_iter_ops = {
    .has_next = array_list_iter_has_next,
    .next = array_list_iter_next,
    .current = array_list_iter_current,
    .reset = array_list_iter_reset,
    .destroy = array_list_iter_destroy
};

/* 关键点：创建数组列表迭代器 */
iterator_t* array_list_create_iterator(array_list_t *list) {
    iterator_t *iter = malloc(sizeof(iterator_t));
    array_list_iter_state_t *state = malloc(sizeof(array_list_iter_state_t));
    
    if (iter == NULL || state == NULL) {
        free(iter);
        free(state);
        return NULL;
    }
    
    state->current_index = 0;
    
    iter->ops = &array_list_iter_ops;
    iter->collection = list;
    iter->state = state;
    
    return iter;
}


/*---------------------------------------------------------------------------
 * linked_list.h - 链表定义
 *---------------------------------------------------------------------------*/
#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include "iterator.h"

typedef struct linked_node {
    void *data;
    struct linked_node *next;
} linked_node_t;

typedef struct {
    linked_node_t *head;
    linked_node_t *tail;
    size_t size;
} linked_list_t;

linked_list_t* linked_list_create(void);
void linked_list_destroy(linked_list_t *list);
bool linked_list_add(linked_list_t *list, void *item);
size_t linked_list_size(linked_list_t *list);

/* 关键点：创建迭代器 */
iterator_t* linked_list_create_iterator(linked_list_t *list);

#endif /* LINKED_LIST_H */


/*---------------------------------------------------------------------------
 * linked_list.c - 链表实现
 *---------------------------------------------------------------------------*/
#include "linked_list.h"
#include <stdlib.h>
#include <stdio.h>

linked_list_t* linked_list_create(void) {
    linked_list_t *list = malloc(sizeof(linked_list_t));
    if (list == NULL) return NULL;
    
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    
    return list;
}

void linked_list_destroy(linked_list_t *list) {
    if (list == NULL) return;
    
    linked_node_t *current = list->head;
    while (current != NULL) {
        linked_node_t *next = current->next;
        free(current);
        current = next;
    }
    
    free(list);
}

bool linked_list_add(linked_list_t *list, void *item) {
    linked_node_t *node = malloc(sizeof(linked_node_t));
    if (node == NULL) return false;
    
    node->data = item;
    node->next = NULL;
    
    if (list->tail == NULL) {
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    
    list->size++;
    return true;
}

size_t linked_list_size(linked_list_t *list) {
    return list->size;
}

/* 链表迭代器状态 */
typedef struct {
    linked_node_t *current_node;
    linked_node_t *prev_node;
} linked_list_iter_state_t;

static bool linked_list_iter_has_next(iterator_t *iter) {
    linked_list_iter_state_t *state = (linked_list_iter_state_t *)iter->state;
    return state->current_node != NULL;
}

static void* linked_list_iter_next(iterator_t *iter) {
    linked_list_iter_state_t *state = (linked_list_iter_state_t *)iter->state;
    
    if (state->current_node == NULL) {
        return NULL;
    }
    
    void *data = state->current_node->data;
    state->prev_node = state->current_node;
    state->current_node = state->current_node->next;
    
    return data;
}

static void* linked_list_iter_current(iterator_t *iter) {
    linked_list_iter_state_t *state = (linked_list_iter_state_t *)iter->state;
    return (state->prev_node != NULL) ? state->prev_node->data : NULL;
}

static void linked_list_iter_reset(iterator_t *iter) {
    linked_list_t *list = (linked_list_t *)iter->collection;
    linked_list_iter_state_t *state = (linked_list_iter_state_t *)iter->state;
    state->current_node = list->head;
    state->prev_node = NULL;
}

static void linked_list_iter_destroy(iterator_t *iter) {
    free(iter->state);
    free(iter);
}

static const iterator_ops_t linked_list_iter_ops = {
    .has_next = linked_list_iter_has_next,
    .next = linked_list_iter_next,
    .current = linked_list_iter_current,
    .reset = linked_list_iter_reset,
    .destroy = linked_list_iter_destroy
};

iterator_t* linked_list_create_iterator(linked_list_t *list) {
    iterator_t *iter = malloc(sizeof(iterator_t));
    linked_list_iter_state_t *state = malloc(sizeof(linked_list_iter_state_t));
    
    if (iter == NULL || state == NULL) {
        free(iter);
        free(state);
        return NULL;
    }
    
    state->current_node = list->head;
    state->prev_node = NULL;
    
    iter->ops = &linked_list_iter_ops;
    iter->collection = list;
    iter->state = state;
    
    return iter;
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "array_list.h"
#include "linked_list.h"
#include <stdio.h>

/* 关键点：统一的处理函数，不关心集合类型 */
void print_all_items(iterator_t *iter, const char *collection_name) {
    printf("\n[%s] Contents:\n", collection_name);
    
    int index = 0;
    while (ITER_HAS_NEXT(iter)) {
        const char *item = (const char *)ITER_NEXT(iter);
        printf("  [%d] %s\n", index++, item);
    }
    
    printf("  Total: %d items\n", index);
}

/* 关键点：演示多个迭代器并行 */
void compare_iteration(iterator_t *iter1, iterator_t *iter2) {
    printf("\n[Parallel Iteration]\n");
    
    while (ITER_HAS_NEXT(iter1) && ITER_HAS_NEXT(iter2)) {
        const char *item1 = (const char *)ITER_NEXT(iter1);
        const char *item2 = (const char *)ITER_NEXT(iter2);
        printf("  ArrayList: %-10s  |  LinkedList: %s\n", item1, item2);
    }
}

int main(void) {
    printf("=== Iterator Pattern Demo ===\n");
    
    /* 创建数组列表 */
    array_list_t *arr_list = array_list_create(4);
    array_list_add(arr_list, "Apple");
    array_list_add(arr_list, "Banana");
    array_list_add(arr_list, "Cherry");
    array_list_add(arr_list, "Date");
    array_list_add(arr_list, "Elderberry");
    
    /* 创建链表 */
    linked_list_t *lnk_list = linked_list_create();
    linked_list_add(lnk_list, "Alpha");
    linked_list_add(lnk_list, "Beta");
    linked_list_add(lnk_list, "Gamma");
    linked_list_add(lnk_list, "Delta");
    linked_list_add(lnk_list, "Epsilon");
    
    /* 关键点：使用相同的方式遍历不同类型的集合 */
    printf("\n========== UNIFORM ITERATION ==========");
    
    iterator_t *arr_iter = array_list_create_iterator(arr_list);
    iterator_t *lnk_iter = linked_list_create_iterator(lnk_list);
    
    print_all_items(arr_iter, "ArrayList");
    print_all_items(lnk_iter, "LinkedList");
    
    /* 关键点：重置迭代器 */
    printf("\n========== RESET AND RE-ITERATE ==========");
    ITER_RESET(arr_iter);
    printf("\n[ArrayList] First 3 items after reset:\n");
    for (int i = 0; i < 3 && ITER_HAS_NEXT(arr_iter); i++) {
        printf("  %s\n", (const char *)ITER_NEXT(arr_iter));
    }
    
    /* 关键点：多个迭代器独立工作 */
    printf("\n========== MULTIPLE ITERATORS ==========");
    
    iterator_t *arr_iter2 = array_list_create_iterator(arr_list);
    
    printf("\nIterator 1 and Iterator 2 on same ArrayList:\n");
    printf("  Iter1 next: %s\n", (const char *)ITER_NEXT(arr_iter));  /* 继续之前位置 */
    printf("  Iter2 next: %s\n", (const char *)ITER_NEXT(arr_iter2)); /* 从头开始 */
    printf("  Iter1 next: %s\n", (const char *)ITER_NEXT(arr_iter));
    printf("  Iter2 next: %s\n", (const char *)ITER_NEXT(arr_iter2));
    
    /* 并行遍历 */
    printf("\n========== PARALLEL ITERATION ==========");
    ITER_RESET(arr_iter);
    ITER_RESET(lnk_iter);
    compare_iteration(arr_iter, lnk_iter);
    
    /* 清理 */
    printf("\n========== CLEANUP ==========\n");
    ITER_DESTROY(arr_iter);
    ITER_DESTROY(arr_iter2);
    ITER_DESTROY(lnk_iter);
    array_list_destroy(arr_list);
    linked_list_destroy(lnk_list);
    
    printf("Done!\n");
    return 0;
}
```

## 运行输出示例

```
=== Iterator Pattern Demo ===

========== UNIFORM ITERATION ==========
[ArrayList] Contents:
  [0] Apple
  [1] Banana
  [2] Cherry
  [3] Date
  [4] Elderberry
  Total: 5 items

[LinkedList] Contents:
  [0] Alpha
  [1] Beta
  [2] Gamma
  [3] Delta
  [4] Epsilon
  Total: 5 items

========== RESET AND RE-ITERATE ==========
[ArrayList] First 3 items after reset:
  Apple
  Banana
  Cherry

========== MULTIPLE ITERATORS ==========
Iterator 1 and Iterator 2 on same ArrayList:
  Iter1 next: Date
  Iter2 next: Apple
  Iter1 next: Elderberry
  Iter2 next: Banana

========== PARALLEL ITERATION ==========
[Parallel Iteration]
  ArrayList: Apple       |  LinkedList: Alpha
  ArrayList: Banana      |  LinkedList: Beta
  ArrayList: Cherry      |  LinkedList: Gamma
  ArrayList: Date        |  LinkedList: Delta
  ArrayList: Elderberry  |  LinkedList: Epsilon

========== CLEANUP ==========
Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **统一接口** | 不同集合使用相同遍历方式 |
| **封装内部** | 不暴露集合内部结构 |
| **多迭代器** | 支持同一集合多个并发遍历 |
| **简化客户端** | 遍历代码简洁一致 |
| **易于扩展** | 新增集合类型只需实现迭代器 |

