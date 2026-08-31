# 05. 树结构（Tree）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 5）：
>
> 通常不会整体排序，因为树本身已经有序（中序遍历天然有序）。
>
> **最佳实践：维持平衡，而不是重新排序。**

平衡二叉搜索树（AVL、红黑树、B+ 树等）把"有序"内建到了数据结构本身，
不需要像数组那样在查询前专门执行一次排序。

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-bst-inorder-is-sorted](example-1-bst-inorder-is-sorted/) | BST 中序遍历天然有序 | 从未调用任何排序函数，中序遍历直接得到升序结果 |
| [example-2-simplified-balanced-insert](example-2-simplified-balanced-insert/) | 简化版 AVL 旋转 | 递增序列插入对比：普通 BST 退化成链表 vs AVL 保持 O(log n) 高度 |
| [example-3-topk-via-inorder-range](example-3-topk-via-inorder-range/) | Top-K / 区间查询 | 利用树的有序性剪枝，不需要遍历+排序全部数据 |

## 心智模式回顾

> **维持平衡，而不是重新排序。**

三个示例分别对应树结构在工程中的三种典型价值：

```
example-1：结构有序      -> 遍历即排序，无需额外算法
example-2：插入时维持平衡 -> 避免退化，始终 O(log n)
example-3：查询时利用有序 -> Top-K / 区间查询可以剪枝，不必先排序全量数据
```

工程实践中很少对红黑树/B+ 树做"整体排序"这个操作——因为只要插入/删除时
正确维护平衡，树就永远处于有序状态，随时可以中序遍历或区间查询。
