/*
 * 场景：双向链表归并排序
 * 心智模型：Merge 时既要修改 next，也要同步维护 prev，
 *          但代价依然只是"重新连接节点"，不搬移数据。
 */
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int val;
    struct Node *prev;
    struct Node *next;
} Node;

static Node *new_node(int val)
{
    Node *node = malloc(sizeof(Node));
    node->val = val;
    node->prev = node->next = NULL;
    return node;
}

static Node *build_list(const int *values, int n)
{
    Node *head = NULL, *tail = NULL;
    for (int i = 0; i < n; i++) {
        Node *node = new_node(values[i]);
        if (!head) {
            head = tail = node;
        } else {
            node->prev = tail;
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

static Node *split_middle(Node *head)
{
    Node *slow = head, *fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    Node *mid = slow->next;
    slow->next = NULL;
    if (mid)
        mid->prev = NULL;
    return mid;
}

/* Move Strategy：合并时同步维护 prev / next，双向链接均不搬移数据 */
static Node *merge(Node *a, Node *b)
{
    Node dummy;
    dummy.val = 0;
    dummy.prev = NULL;
    dummy.next = NULL;
    Node *tail = &dummy;

    while (a && b) {
        Node *chosen = (a->val <= b->val) ? a : b;
        if (chosen == a)
            a = a->next;
        else
            b = b->next;

        chosen->prev = tail;
        tail->next = chosen;
        tail = chosen;
    }
    Node *rest = a ? a : b;
    tail->next = rest;
    if (rest)
        rest->prev = tail;

    Node *new_head = dummy.next;
    if (new_head)
        new_head->prev = NULL;
    return new_head;
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

static void print_forward(const char *label, Node *head)
{
    printf("%s: ", label);
    while (head) {
        printf("%d ", head->val);
        head = head->next;
    }
    printf("\n");
}

static void print_backward(const char *label, Node *head)
{
    Node *tail = head;
    while (tail && tail->next)
        tail = tail->next;

    printf("%s: ", label);
    while (tail) {
        printf("%d ", tail->val);
        tail = tail->prev;
    }
    printf("\n");
}

int main(void)
{
    int values[] = {38, 27, 43, 3, 9, 82, 10};
    int n = sizeof(values) / sizeof(values[0]);

    Node *head = build_list(values, n);

    printf("=== 双向链表归并排序 ===\n");
    print_forward("排序前(正向)", head);

    head = merge_sort(head);

    print_forward("排序后(正向 next)", head);
    print_backward("排序后(反向 prev)", head);
    printf("\n说明：Merge 过程中 next 和 prev 都被重新连接，\n"
           "正向、反向遍历结果互相印证，但节点数据本身从未被拷贝或交换。\n");

    free_list(head);
    return 0;
}
