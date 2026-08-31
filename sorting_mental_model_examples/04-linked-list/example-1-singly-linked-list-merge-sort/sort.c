/*
 * 场景：单链表归并排序
 * 心智模型：链表排序不是移动数据，而是重新连接节点（修改 next 指针）。
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node *next;
} Node;

static Node *new_node(int val)
{
    Node *node = malloc(sizeof(Node));
    node->val = val;
    node->next = NULL;
    return node;
}

static Node *build_list(const int *values, int n)
{
    Node *head = NULL, *tail = NULL;
    for (int i = 0; i < n; i++) {
        Node *node = new_node(values[i]);
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

/* 快慢指针找中点，把链表切成两段 */
static Node *split_middle(Node *head)
{
    Node *slow = head, *fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    Node *mid = slow->next;
    slow->next = NULL;
    return mid;
}

/* Move Strategy：合并两个有序链表时，只修改 next 指针，不搬移节点数据 */
static Node *merge(Node *a, Node *b)
{
    Node dummy;
    Node *tail = &dummy;
    dummy.next = NULL;
    while (a && b) {
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
    int values[] = {38, 27, 43, 3, 9, 82, 10};
    int n = sizeof(values) / sizeof(values[0]);

    Node *head = build_list(values, n);

    printf("=== 单链表归并排序 ===\n");
    print_list("排序前", head);

    head = merge_sort(head);

    print_list("排序后", head);
    printf("\n说明：整个排序过程只创建过 %d 个节点，没有再额外分配或拷贝节点，\n"
           "排序效果完全通过重新连接 next 指针实现。\n",
           n);

    free_list(head);
    return 0;
}
