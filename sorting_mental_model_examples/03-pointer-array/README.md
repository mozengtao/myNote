# 03. 指针 / 引用数组（Pointer Array）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 3）：
>
> 交换成本：8 字节，而不是 512 字节。现代 C++ 大量使用。

指针数组把"排序"和"数据本体"彻底解耦：数组里存的是指针/引用，交换成本恒定为
一个指针的大小（通常 8 字节），与 Record 本身有多大无关。这也让"同一份数据、
多种排序规则"变得非常廉价。

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-vector-of-pointers-multi-key](example-1-vector-of-pointers-multi-key/) | 同一批指针按不同字段反复排序 | 切换 Comparator 即可切换排序结果，不需要复制数据 |
| [example-2-stable-sort-pointers](example-2-stable-sort-pointers/) | 稳定排序指针 | 相同 Key 的记录排序前后相对顺序保持不变 |
| [example-3-multi-level-comparator](example-3-multi-level-comparator/) | 多级比较器（先部门后薪资） | Comparator 可以组合多个 Key，Move Strategy 不受影响 |

## 心智模式回顾

> **交换指针，而不是交换对象；Comparator 与 Move Strategy 相互独立。**

三个示例共同印证的是原文档第三节的核心结论：

```
                Sort
                  │
         ┌────────┴────────┐
         │                 │
    Comparator          Move Strategy
   (怎么比大小)         (怎么移动数据)
```

- `example-1` 说明 Comparator 可以随时替换，Move Strategy（交换指针）始终不变。
- `example-2` 说明 Move Strategy 相同（都是交换指针），但排序算法是否稳定会影响结果。
- `example-3` 说明 Comparator 可以任意复杂（多级比较），复杂度完全不会传导到 Move Strategy 上。

工程实践提示：C 用 `qsort` 时若需要稳定排序必须自己实现（如归并排序）；
C++ 优先用 `std::stable_sort`；Python 的内置排序天然稳定，无需额外处理。
