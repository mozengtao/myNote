/*
C 语言无 “类” 和 “接口继承”，但通过「函数指针结构体（统一迭代接口）+ 聚合对象适配」可完美实现迭代器模式。以下是内核风格的通用范式（数组 + 链表迭代器，统一遍历接口）

代码说明
接口统一：数组迭代器和链表迭代器均实现Iterator接口，上层traverse函数无需区分聚合对象类型；
解耦核心：遍历逻辑（traverse）与聚合对象内部结构（数组下标 / 链表节点）完全分离，新增哈希表迭代器时，仅需实现Iterator接口，无需修改traverse；
安全性：迭代器内部封装了边界判断（如array_has_next判断下标是否越界），上层无需关心；
灵活性：支持重置迭代器、多次遍历，且可同时创建多个迭代器遍历同一聚合对象
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------- 1. 定义统一迭代器接口（Iterator） ----------------------
// 迭代器元素类型（简化为字符串）
typedef char* IteratorData;

// 统一迭代器接口：所有聚合对象的迭代器需实现此接口
typedef struct {
    // 判断是否还有下一个元素
    int (*has_next)(void *iterator);
    // 获取下一个元素
    IteratorData (*next)(void *iterator);
    // 重置迭代器到起始位置
    void (*reset)(void *iterator);
} Iterator;

// ---------------------- 2. 定义聚合对象1：数组（ConcreteAggregate） ----------------------
// 数组迭代器结构体（ConcreteIterator）
typedef struct {
    Iterator base;          // 继承统一迭代器接口
    char **data;            // 指向数组数据
    int size;               // 数组长度
    int current_index;      // 当前迭代下标
} ArrayIterator;

// 数组迭代器-判断是否有下一个元素
static int array_has_next(void *iterator) {
    ArrayIterator *it = (ArrayIterator*)iterator;
    return it->current_index < it->size;
}

// 数组迭代器-获取下一个元素
static IteratorData array_next(void *iterator) {
    ArrayIterator *it = (ArrayIterator*)iterator;
    if (!array_has_next(it)) return NULL;
    return it->data[it->current_index++];
}

// 数组迭代器-重置
static void array_reset(void *iterator) {
    ArrayIterator *it = (ArrayIterator*)iterator;
    it->current_index = 0;
}

// 初始化数组迭代器
static void array_iterator_init(ArrayIterator *it, char **data, int size) {
    it->base.has_next = array_has_next;
    it->base.next = array_next;
    it->base.reset = array_reset;
    it->data = data;
    it->size = size;
    it->current_index = 0;
}

// ---------------------- 3. 定义聚合对象2：单向链表（ConcreteAggregate） ----------------------
// 链表节点结构体
typedef struct ListNode {
    char *data;
    struct ListNode *next;
} ListNode;

// 链表迭代器结构体（ConcreteIterator）
typedef struct {
    Iterator base;          // 继承统一迭代器接口
    ListNode *head;         // 链表头节点
    ListNode *current_node; // 当前迭代节点
} ListIterator;

// 链表迭代器-判断是否有下一个元素
static int list_has_next(void *iterator) {
    ListIterator *it = (ListIterator*)iterator;
    return it->current_node != NULL;
}

// 链表迭代器-获取下一个元素
static IteratorData list_next(void *iterator) {
    ListIterator *it = (ListIterator*)iterator;
    if (!list_has_next(it)) return NULL;
    char *data = it->current_node->data;
    it->current_node = it->current_node->next;
    return data;
}

// 链表迭代器-重置
static void list_reset(void *iterator) {
    ListIterator *it = (ListIterator*)iterator;
    it->current_node = it->head;
}

// 初始化链表迭代器
static void list_iterator_init(ListIterator *it, ListNode *head) {
    it->base.has_next = list_has_next;
    it->base.next = list_next;
    it->base.reset = list_reset;
    it->head = head;
    it->current_node = head;
}

// ---------------------- 4. 上层通用遍历逻辑：无需关心聚合对象类型 ----------------------
// 通用遍历函数：适配任意实现Iterator接口的迭代器
static void traverse(Iterator *it) {
    printf("\n=== 开始遍历元素 ===\n");
    while (it->has_next(it)) {
        IteratorData data = it->next(it);
        printf("遍历到元素：%s\n", data);
    }
    printf("=== 遍历结束 ===\n");
}

// 辅助函数：创建测试链表
static ListNode* create_test_list() {
    ListNode *n1 = malloc(sizeof(ListNode));
    ListNode *n2 = malloc(sizeof(ListNode));
    ListNode *n3 = malloc(sizeof(ListNode));
    n1->data = "链表元素1"; n1->next = n2;
    n2->data = "链表元素2"; n2->next = n3;
    n3->data = "链表元素3"; n3->next = NULL;
    return n1;
}

// 主函数测试
int main() {
    // 测试1：数组迭代器
    char *array_data[] = {"数组元素1", "数组元素2", "数组元素3"};
    ArrayIterator array_it;
    array_iterator_init(&array_it, array_data, 3);
    traverse(&array_it.base); // 传入统一迭代器接口

    // 测试2：链表迭代器
    ListNode *list_head = create_test_list();
    ListIterator list_it;
    list_iterator_init(&list_it, list_head);
    traverse(&list_it.base); // 传入统一迭代器接口（上层逻辑完全复用）

    // 重置链表迭代器并再次遍历
    list_it.base.reset(&list_it);
    printf("\n=== 重置后再次遍历链表 ===\n");
    traverse(&list_it.base);

    // 释放链表内存
    ListNode *tmp = list_head;
    while (tmp) {
        ListNode *next = tmp->next;
        free(tmp);
        tmp = next;
    }

    return 0;
}