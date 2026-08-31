"""
场景：双向链表归并排序
心智模型：Merge 时既要修改 next，也要同步维护 prev，
         但代价依然只是"重新连接节点"，不搬移数据。
"""


class Node:
    __slots__ = ("val", "prev", "next")

    def __init__(self, val):
        self.val = val
        self.prev = None
        self.next = None


def build_list(values):
    head = tail = None
    for v in values:
        node = Node(v)
        if head is None:
            head = tail = node
        else:
            node.prev = tail
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
    if mid:
        mid.prev = None
    return mid


def merge(a, b):
    dummy = Node(0)
    tail = dummy
    while a and b:
        chosen = a if a.val <= b.val else b
        if chosen is a:
            a = a.next
        else:
            b = b.next
        chosen.prev = tail
        tail.next = chosen
        tail = chosen

    rest = a if a else b
    tail.next = rest
    if rest:
        rest.prev = tail

    new_head = dummy.next
    if new_head:
        new_head.prev = None
    return new_head


def merge_sort(head):
    if head is None or head.next is None:
        return head
    mid = split_middle(head)
    left = merge_sort(head)
    right = merge_sort(mid)
    return merge(left, right)


def print_forward(label, head):
    values = []
    while head:
        values.append(head.val)
        head = head.next
    print(f"{label}: {' '.join(map(str, values))}")


def print_backward(label, head):
    tail = head
    while tail and tail.next:
        tail = tail.next
    values = []
    while tail:
        values.append(tail.val)
        tail = tail.prev
    print(f"{label}: {' '.join(map(str, values))}")


def main():
    head = build_list([38, 27, 43, 3, 9, 82, 10])

    print("=== 双向链表归并排序 ===")
    print_forward("排序前(正向)", head)

    head = merge_sort(head)

    print_forward("排序后(正向 next)", head)
    print_backward("排序后(反向 prev)", head)
    print(
        "\n说明：Merge 过程中 next 和 prev 都被重新连接，"
        "节点数据本身从未被拷贝或交换。"
    )


if __name__ == "__main__":
    main()
