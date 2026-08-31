# 01. 连续数组（Array）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 1）：
>
> 特点：连续内存、Cache 友好、O(1) 随机访问。
> 最适合：Quick Sort → Introsort → Insertion Sort（小数组优化）。
> 原因：**CPU Cache 命中率最高。**

数据连续存放在内存里，随机访问是 O(1)，因此排序算法可以放心地直接交换元素本身，
不需要引入指针/索引这一层间接访问。这是所有排序策略里最基础、成本最低的一种。

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-int-array](example-1-int-array/) | 整型数组排序 | 最基础的 Comparator + 交换值 的 Move Strategy；工程上应直接调用标准库排序 |
| [example-2-employee-struct-array](example-2-employee-struct-array/) | 按薪资排序 Employee 结构体数组 | 排序比较的是 Key（salary），但 Record 足够小时仍可直接整体交换 |
| [example-3-small-array-insertion-optimization](example-3-small-array-insertion-optimization/) | Quick Sort + Insertion Sort 混合 | 子数组小于阈值时切换 Insertion Sort，这是 Introsort/Timsort 等工程实现的共同优化手段 |

## 心智模式回顾

> **连续内存 + 小对象 → 直接排序，Cache 命中率最高。**

工程实践中不需要手写这些算法，直接使用语言/标准库提供的排序即可：

```
C++  std::sort
C    glibc qsort
Go   sort.Slice
Rust slice::sort_unstable
```

本分类的示例手写算法只是为了看清楚 Comparator 与 Move Strategy 各自在做什么；
真正写业务代码时，永远优先选择标准库排序。
