/*
 * 场景：单链表归并排序
 * 心智模型：链表排序不是移动数据，而是重新连接节点（修改 next 指针）。
 */
#include <iostream>
#include <initializer_list>
#include <string>

struct Node {
    int val;
    Node *next;
    explicit Node(int v) : val(v), next(nullptr) {}
};

static Node *build_list(std::initializer_list<int> values)
{
    Node *head = nullptr;
    Node *tail = nullptr;
    for (int v : values) {
        Node *node = new Node(v);
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
    return mid;
}

// Move Strategy：合并时只修改 next 指针，不搬移节点数据
static Node *merge(Node *a, Node *b)
{
    Node dummy(0);
    Node *tail = &dummy;
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

static void print_list(const std::string &label, Node *head)
{
    std::cout << label << ": ";
    for (Node *p = head; p; p = p->next)
        std::cout << p->val << " ";
    std::cout << "\n";
}

int main()
{
    Node *head = build_list({38, 27, 43, 3, 9, 82, 10});

    std::cout << "=== 单链表归并排序 ===\n";
    print_list("排序前", head);

    head = merge_sort(head);

    print_list("排序后", head);
    std::cout << "\n说明：整个排序过程没有额外分配或拷贝任何节点，\n"
                 "排序效果完全通过重新连接 next 指针实现。\n"
                 "工程实践中若使用 std::list，可直接调用其内置 sort()（同样是 Merge Sort）。\n";

    free_list(head);
    return 0;
}
