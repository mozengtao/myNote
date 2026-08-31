"""
场景：单链表归并排序
心智模型：链表排序不是移动数据，而是重新连接节点（修改 next 引用）。
"""


class Node:
    __slots__ = ("val", "next")

    def __init__(self, val):
        self.val = val
        self.next = None


def build_list(values):
    head = tail = None
    for v in values:
        node = Node(v)
        if head is None:
            head = tail = node
        else:
            tail.next = node
            tail = node
    return head


def split_middle(head):
    slow, fast = head, head.next
    while fast and fast.next:
        slow = slow.next
        fast = fast.next.next
    mid = slow.next
    slow.next = None
    return mid


def merge(a, b):
    dummy = Node(0)
    tail = dummy
    while a and b:
        if a.val <= b.val:
            tail.next = a
            a = a.next
        else:
            tail.next = b
            b = b.next
        tail = tail.next
    tail.next = a if a else b
    return dummy.next


def merge_sort(head):
    if head is None or head.next is None:
        return head
    mid = split_middle(head)
    left = merge_sort(head)
    right = merge_sort(mid)
    return merge(left, right)


def print_list(label, head):
    values = []
    while head:
        values.append(head.val)
        head = head.next
    print(f"{label}: {' '.join(map(str, values))}")


def main():
    head = build_list([38, 27, 43, 3, 9, 82, 10])

    print("=== 单链表归并排序 ===")
    print_list("排序前", head)

    head = merge_sort(head)

    print_list("排序后", head)
    print(
        "\n说明：排序过程没有创建任何新的数据节点，只是重新连接了 next 引用，"
        "这与 C/C++ 版本的思路完全一致。"
    )


if __name__ == "__main__":
    main()
