"""
场景：二叉搜索树 —— 借助中序遍历做 Top-K / 区间查询
心智模型：树已经是有序的，查询"最小的 K 个"或"某个区间内的元素"
         不需要先导出全部数据再排序，只需要有选择地遍历/剪枝。
"""

visited_nodes = 0


class Node:
    __slots__ = ("val", "left", "right")

    def __init__(self, val):
        self.val = val
        self.left = None
        self.right = None


def insert(root, val):
    if root is None:
        return Node(val)
    if val < root.val:
        root.left = insert(root.left, val)
    else:
        root.right = insert(root.right, val)
    return root


def k_smallest(root, k, result):
    """中序遍历本身就是升序，收集到 k 个就可以提前停止。"""
    global visited_nodes
    if root is None or len(result) >= k:
        return
    visited_nodes += 1
    k_smallest(root.left, k, result)
    if len(result) < k:
        result.append(root.val)
    k_smallest(root.right, k, result)


def range_query(root, lo, hi, result):
    """利用 BST 的有序性剪枝，跳过不可能落在区间内的子树。"""
    global visited_nodes
    if root is None:
        return
    visited_nodes += 1
    if root.val > lo:
        range_query(root.left, lo, hi, result)
    if lo <= root.val <= hi:
        result.append(root.val)
    if root.val < hi:
        range_query(root.right, lo, hi, result)


def main():
    global visited_nodes
    values = [50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45, 55, 65, 75, 90]

    root = None
    for v in values:
        root = insert(root, v)

    print("=== BST：借助中序遍历做 Top-K / 区间查询 ===")
    print(f"树中共有 {len(values)} 个节点\n")

    k = 4
    topk = []
    visited_nodes = 0
    k_smallest(root, k, topk)
    print(f"最小的 {k} 个值: {topk} (访问节点数 = {visited_nodes}，总节点数 = {len(values)})")

    lo, hi = 30, 60
    range_result = []
    visited_nodes = 0
    range_query(root, lo, hi, range_result)
    print(
        f"区间 [{lo}, {hi}] 内的值: {range_result} "
        f"(访问节点数 = {visited_nodes}，总节点数 = {len(values)})"
    )

    print(
        "\n说明：两个查询都没有把全部节点取出来再排序，"
        "而是利用 BST 天然有序的特性做剪枝遍历，访问节点数明显少于总节点数。"
    )


if __name__ == "__main__":
    main()
