# 07. 数据库记录（Database Records）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 7）：
>
> 真正排序：RowID、Tuple Pointer、Page Offset。不是整行数据。
>
> **心智模式：排序索引，而不是排序数据。**

一行数据库记录可能有几 KB（甚至更大），直接搬移整行代价极高。
数据库内部普遍的做法是：排序某种"轻量标识符"（RowID、指针、`(PageID, Offset)`），
最后再按需读取真正的行数据。

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-order-by-sorts-rowid-not-row](example-1-order-by-sorts-rowid-not-row/) | `ORDER BY salary` | 排序的是 RowID（整数下标），原表数据原地不动 |
| [example-2-page-tuple-offset-sort](example-2-page-tuple-offset-sort/) | 模拟分页存储 | 用 `(PageID, Offset)` 这种二级地址代替 RowID，更贴近真实存储引擎 |
| [example-3-composite-index-multi-field](example-3-composite-index-multi-field/) | 复合索引 `ORDER BY dept, salary DESC` | 多列组合成一个 Comparator，仍然只排序 RowID |

## 心智模式回顾

> **排序索引，而不是排序数据。**

三个示例的递进关系：

```
example-1：RowID 是最简单的间接层（一维下标）
                ↓
example-2：(PageID, Offset) 是更贴近磁盘存储的二级间接层
                ↓
example-3：无论间接层长什么样，Comparator 都可以任意组合多个列
```

这与 [02-large-object-array](../02-large-object-array/) 和 [03-pointer-array](../03-pointer-array/)
是同一个心智模型在不同场景下的复用：**数据本体不动，只搬移一个廉价的"引用"**。
数据库的特殊之处在于，这个"引用"往往还要能定位到磁盘上的具体页面和偏移量。
