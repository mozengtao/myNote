# 04. 链表（Linked List）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 4）：
>
> 不适合：Quick Sort、Heap Sort。最佳实践：Merge Sort。
>
> **心智模式：链表排序不是移动数据，而是重新连接节点。**

链表不支持 O(1) 随机访问（`A[i]` 这种操作在链表上必须从头遍历），
因此依赖"任意下标访问"的 Quick Sort / Heap Sort 并不适合链表；
Merge Sort 只需要顺序遍历 + 修改指针，天然契合链表的访问特性。

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-singly-linked-list-merge-sort](example-1-singly-linked-list-merge-sort/) | 单链表归并排序 | 快慢指针找中点 + Merge 时只修改 next |
| [example-2-doubly-linked-list-merge-sort](example-2-doubly-linked-list-merge-sort/) | 双向链表归并排序 | Merge 时同步维护 prev/next，正反遍历互相印证 |
| [example-3-why-quicksort-fails-on-list](example-3-why-quicksort-fails-on-list/) | 为什么 Quick Sort 不适合链表 | 用"遍历步数"量化下标访问在链表上的 O(n) 代价 |

## 心智模式回顾

> **链表排序不是移动数据，而是重新连接节点。**

三个示例的关系：

```
example-1 / example-2：Merge Sort 的正确实现方式
                            ↓
example-3：如果强行用 Quick Sort（依赖下标访问），代价会退化到 O(n^2)
```

工程实践提示：

```
Linux Kernel  list_sort()
Java          LinkedList（内部用归并策略）
C++ STL       list::sort()
```

以上都直接采用 Merge Sort 作为链表排序的标准答案，不需要重新造轮子。
