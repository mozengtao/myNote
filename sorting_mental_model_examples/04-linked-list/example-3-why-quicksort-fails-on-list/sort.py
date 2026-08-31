"""
场景：链表 —— 为什么 Quick Sort 不适合链表
心智模型：链表不支持 O(1) 随机访问，Quick Sort 依赖的下标访问
         在链表上必须退化为"从头遍历"，代价是 O(n)。
         本示例统计遍历步数，量化这个代价，并与 Merge Sort 对比。
"""

traverse_steps = 0


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


def get_node_at(head, index):
    """模拟"数组下标访问"：必须从头遍历 index 步才能拿到目标节点，O(n) 代价。"""
    global traverse_steps
    for _ in range(index):
        head = head.next
        traverse_steps += 1
    return head


def partition_by_index(head, low, high):
    pivot = get_node_at(head, high).val
    i = low - 1
    for j in range(low, high):
        nj = get_node_at(head, j)
        if nj.val < pivot:
            i += 1
            ni = get_node_at(head, i)
            ni.val, nj.val = nj.val, ni.val
    ni1 = get_node_at(head, i + 1)
    nhigh = get_node_at(head, high)
    ni1.val, nhigh.val = nhigh.val, ni1.val
    return i + 1


def quicksort_by_index(head, low, high):
    if low < high:
        p = partition_by_index(head, low, high)
        quicksort_by_index(head, low, p - 1)
        quicksort_by_index(head, p + 1, high)


def split_middle(head):
    global traverse_steps
    slow, fast = head, head.next
    while fast and fast.next:
        slow = slow.next
        fast = fast.next.next
        traverse_steps += 2
    mid = slow.next
    slow.next = None
    return mid


def merge(a, b):
    global traverse_steps
    dummy = Node(0)
    tail = dummy
    while a and b:
        traverse_steps += 1
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
    global traverse_steps
    values = [50, 12, 88, 33, 67, 5, 91, 24, 76, 41, 8, 59]
    n = len(values)

    print("=== 链表：为什么 Quick Sort 不适合链表 ===")

    # 方案一：把链表当数组用，靠"下标访问"实现 Quick Sort
    head1 = build_list(values)
    print_list("排序前", head1)
    traverse_steps = 0
    quicksort_by_index(head1, 0, n - 1)
    print_list("方案一 Quick Sort(下标模拟) 结果", head1)
    quicksort_steps = traverse_steps

    # 方案二：Merge Sort，全程只做顺序遍历 + 修改 next 引用
    head2 = build_list(values)
    traverse_steps = 0
    head2 = merge_sort(head2)
    print_list("方案二 Merge Sort 结果", head2)
    mergesort_steps = traverse_steps

    print(f"\n链表长度 n = {n}")
    print(f"方案一 (Quick Sort, 下标模拟) 遍历步数 = {quicksort_steps}")
    print(f"方案二 (Merge Sort, 顺序遍历)   遍历步数 = {mergesort_steps}")
    print(
        "\n说明：Quick Sort 每次'下标访问'在链表上都要从头遍历，退化为 O(n)，"
        "整体复杂度退化到 O(n^2)；Merge Sort 全程只做顺序遍历和修改 next 引用，"
        "复杂度保持 O(n log n)。这就是链表场景下应该选 Merge Sort 的原因。"
    )


if __name__ == "__main__":
    main()
