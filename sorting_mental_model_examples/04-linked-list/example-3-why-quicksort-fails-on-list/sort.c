/*
 * 场景：链表 —— 为什么 Quick Sort 不适合链表
 * 心智模型：链表不支持 O(1) 随机访问，Quick Sort 依赖的下标访问
 *          在链表上必须退化为"从头遍历"，代价是 O(n)。
 *          本示例统计遍历步数，量化这个代价，并与 Merge Sort 对比。
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node *next;
} Node;

static long g_traverse_steps = 0;

static Node *build_list(const int *values, int n)
{
    Node *head = NULL, *tail = NULL;
    for (int i = 0; i < n; i++) {
        Node *node = malloc(sizeof(Node));
        node->val = values[i];
        node->next = NULL;
        if (!head)
            head = tail = node;
        else {
            tail->next = node;
            tail = node;
        }
    }
    return head;
}

static void free_list(Node *head)
{
    while (head) {
        Node *next = head->next;
        free(head);
        head = next;
    }
}

/* 模拟"数组下标访问"：必须从头遍历 index 步才能拿到目标节点 —— O(n) 代价 */
static Node *get_node_at(Node *head, int index)
{
    for (int i = 0; i < index; i++) {
        head = head->next;
        g_traverse_steps++;
    }
    return head;
}

/* 用"下标访问"模拟经典数组 Quick Sort（仅用于演示代价，不推荐这样写） */
static int partition_by_index(Node *head, int low, int high)
{
    int pivot = get_node_at(head, high)->val;
    int i = low - 1;
    for (int j = low; j < high; j++) {
        Node *nj = get_node_at(head, j);
        if (nj->val < pivot) {
            i++;
            Node *ni = get_node_at(head, i);
            int tmp = ni->val;
            ni->val = nj->val;
            nj->val = tmp;
        }
    }
    Node *ni1 = get_node_at(head, i + 1);
    Node *nhigh = get_node_at(head, high);
    int tmp = ni1->val;
    ni1->val = nhigh->val;
    nhigh->val = tmp;
    return i + 1;
}

static void quicksort_by_index(Node *head, int low, int high)
{
    if (low < high) {
        int p = partition_by_index(head, low, high);
        quicksort_by_index(head, low, p - 1);
        quicksort_by_index(head, p + 1, high);
    }
}

/* Merge Sort：只用 next 指针做顺序遍历，不需要"下标访问" */
static Node *split_middle(Node *head)
{
    Node *slow = head, *fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        g_traverse_steps += 2;
    }
    Node *mid = slow->next;
    slow->next = NULL;
    return mid;
}

static Node *merge(Node *a, Node *b)
{
    Node dummy;
    dummy.next = NULL;
    Node *tail = &dummy;
    while (a && b) {
        g_traverse_steps++;
        if (a->val <= b->val) {
            tail->next = a;
            a = a->next;
        } else {
            tail->next = b;
            b = b->next;
        }
        tail = tail->next;
    }
    tail->next = a ? a : b;
    return dummy.next;
}

static Node *merge_sort(Node *head)
{
    if (!head || !head->next)
        return head;
    Node *mid = split_middle(head);
    Node *left = merge_sort(head);
    Node *right = merge_sort(mid);
    return merge(left, right);
}

static void print_list(const char *label, Node *head)
{
    printf("%s: ", label);
    while (head) {
        printf("%d ", head->val);
        head = head->next;
    }
    printf("\n");
}

int main(void)
{
    int values[] = {50, 12, 88, 33, 67, 5, 91, 24, 76, 41, 8, 59};
    int n = sizeof(values) / sizeof(values[0]);

    printf("=== 链表：为什么 Quick Sort 不适合链表 ===\n");

    /* 方案一：把链表当数组用，靠"下标访问"实现 Quick Sort */
    Node *head1 = build_list(values, n);
    print_list("排序前", head1);
    g_traverse_steps = 0;
    quicksort_by_index(head1, 0, n - 1);
    print_list("方案一 Quick Sort(下标模拟) 结果", head1);
    long quicksort_steps = g_traverse_steps;
    free_list(head1);

    /* 方案二：Merge Sort，全程只做顺序遍历 + 修改 next 指针 */
    Node *head2 = build_list(values, n);
    g_traverse_steps = 0;
    head2 = merge_sort(head2);
    print_list("方案二 Merge Sort 结果", head2);
    long mergesort_steps = g_traverse_steps;
    free_list(head2);

    printf("\n链表长度 n = %d\n", n);
    printf("方案一 (Quick Sort, 下标模拟) 遍历步数 = %ld\n", quicksort_steps);
    printf("方案二 (Merge Sort, 顺序遍历)   遍历步数 = %ld\n", mergesort_steps);
    printf("\n说明：Quick Sort 每次\"下标访问\"在链表上都要从头遍历，退化为 O(n)，\n"
           "整体复杂度退化到 O(n^2)；Merge Sort 全程只做顺序遍历和修改 next 指针，\n"
           "复杂度保持 O(n log n)。这就是链表场景下应该选 Merge Sort 的原因。\n");
    return 0;
}
