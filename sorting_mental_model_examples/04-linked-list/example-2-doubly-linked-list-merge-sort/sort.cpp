/*
 * 场景：双向链表归并排序
 * 心智模型：Merge 时既要修改 next，也要同步维护 prev，
 *          但代价依然只是"重新连接节点"，不搬移数据。
 */
#include <iostream>
#include <initializer_list>
#include <string>

struct Node {
    int val;
    Node *prev;
    Node *next;
    explicit Node(int v) : val(v), prev(nullptr), next(nullptr) {}
};

static Node *build_list(std::initializer_list<int> values)
{
    Node *head = nullptr;
    Node *tail = nullptr;
    for (int v : values) {
        Node *node = new Node(v);
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
        delete head;
        head = next;
    }
}

static Node *split_middle(Node *head)
{
    Node *slow = head;
    Node *fast = head->next;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
    }
    Node *mid = slow->next;
    slow->next = nullptr;
    if (mid)
        mid->prev = nullptr;
    return mid;
}

static Node *merge(Node *a, Node *b)
{
    Node dummy(0);
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
        new_head->prev = nullptr;
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

static void print_forward(const std::string &label, Node *head)
{
    std::cout << label << ": ";
    for (Node *p = head; p; p = p->next)
        std::cout << p->val << " ";
    std::cout << "\n";
}

static void print_backward(const std::string &label, Node *head)
{
    Node *tail = head;
    while (tail && tail->next)
        tail = tail->next;

    std::cout << label << ": ";
    for (Node *p = tail; p; p = p->prev)
        std::cout << p->val << " ";
    std::cout << "\n";
}

int main()
{
    Node *head = build_list({38, 27, 43, 3, 9, 82, 10});

    std::cout << "=== 双向链表归并排序 ===\n";
    print_forward("排序前(正向)", head);

    head = merge_sort(head);

    print_forward("排序后(正向 next)", head);
    print_backward("排序后(反向 prev)", head);
    std::cout << "\n说明：Merge 过程中 next 和 prev 都被重新连接，\n"
                 "正向、反向遍历结果互相印证，节点数据本身从未被拷贝或交换。\n"
                 "工程实践中可直接使用 std::list，其 sort() 内部就是 Merge Sort。\n";

    free_list(head);
    return 0;
}
