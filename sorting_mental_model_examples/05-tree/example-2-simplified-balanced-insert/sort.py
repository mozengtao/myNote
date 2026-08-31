"""
场景：二叉搜索树 —— 简化版 AVL 旋转，插入时维持平衡而不是重新排序
心智模型：树的最佳实践是"维持平衡"，而不是退化后再重新整体排序。
"""


class Node:
    __slots__ = ("val", "height", "left", "right")

    def __init__(self, val):
        self.val = val
        self.height = 1
        self.left = None
        self.right = None


def height_of(n):
    return n.height if n else 0


def update_height(n):
    n.height = 1 + max(height_of(n.left), height_of(n.right))


def balance_factor(n):
    return height_of(n.left) - height_of(n.right) if n else 0


def rotate_right(y):
    x = y.left
    t2 = x.right
    x.right = y
    y.left = t2
    update_height(y)
    update_height(x)
    return x


def rotate_left(x):
    y = x.right
    t2 = y.left
    y.left = x
    x.right = t2
    update_height(x)
    update_height(y)
    return y


def avl_insert(root, val):
    """简化版 AVL 插入：插入后自底向上检查平衡因子，超出 [-1,1] 就旋转纠正。"""
    if root is None:
        return Node(val)

    if val < root.val:
        root.left = avl_insert(root.left, val)
    else:
        root.right = avl_insert(root.right, val)

    update_height(root)
    balance = balance_factor(root)

    if balance > 1 and val < root.left.val:
        return rotate_right(root)
    if balance < -1 and val >= root.right.val:
        return rotate_left(root)
    if balance > 1 and val >= root.left.val:
        root.left = rotate_left(root.left)
        return rotate_right(root)
    if balance < -1 and val < root.right.val:
        root.right = rotate_right(root.right)
        return rotate_left(root)
    return root


def bst_insert(root, val):
    """普通 BST 插入：不做任何平衡处理，最坏情况会退化成链表。"""
    if root is None:
        return Node(val)
    if val < root.val:
        root.left = bst_insert(root.left, val)
    else:
        root.right = bst_insert(root.right, val)
    root.height = 1 + max(height_of(root.left), height_of(root.right))
    return root


def inorder(root, result):
    if root is None:
        return
    inorder(root.left, result)
    result.append(root.val)
    inorder(root.right, result)


def main():
    n = 15
    values = list(range(1, n + 1))  # 递增序列：普通 BST 的最坏情况

    bst_root = None
    avl_root = None
    for v in values:
        bst_root = bst_insert(bst_root, v)
        avl_root = avl_insert(avl_root, v)

    result = []
    inorder(avl_root, result)

    print("=== 二叉搜索树：简化版 AVL 旋转，维持平衡而不是重新排序 ===")
    print(f"按递增顺序插入 1..{n}（普通 BST 的最坏情况）")
    print(f"普通 BST 高度 = {height_of(bst_root)}（最坏情况下退化成链表，高度 = n）")
    print(f"AVL   树高度  = {height_of(avl_root)}（始终维持在 O(log n) 量级）")

    print(f"AVL 中序遍历结果: {result}")
    print(
        "\n说明：面对同样的递增插入序列，AVL 通过旋转始终把树高度控制在"
        "O(log n)，而普通 BST 会退化成一条链表（高度 = n）。这正是"
        "'维持平衡，而不是重新排序'这一最佳实践的量化体现。"
    )


if __name__ == "__main__":
    main()
