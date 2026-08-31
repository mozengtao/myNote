"""
场景：二叉搜索树（BST）—— 中序遍历天然有序
心智模型：树本身已经有序，不需要"整体排序"，中序遍历即可得到有序序列。
"""


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


def inorder(root, result):
    """中序遍历：左 -> 根 -> 右，对 BST 而言天然按升序访问。"""
    if root is None:
        return
    inorder(root.left, result)
    result.append(root.val)
    inorder(root.right, result)


def main():
    values = [50, 30, 70, 20, 40, 60, 80, 35, 65]

    root = None
    for v in values:
        root = insert(root, v)

    print("=== BST：中序遍历天然有序 ===")
    print(f"插入顺序: {values}")

    result = []
    inorder(root, result)

    print(f"中序遍历结果: {result}")
    print(
        "\n说明：从未对数据做过'排序'这个动作，BST 的结构本身就保证了"
        "中序遍历得到的序列一定是升序的，这就是'树天然有序'。"
    )


if __name__ == "__main__":
    main()
