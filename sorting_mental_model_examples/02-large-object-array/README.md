# 02. 大对象数组（Large Object Array）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 2）：
>
> 优势：数据不移动，Cache 更友好，减少 memcpy。
>
> **心智模式：对象保持不动，排序的是访问路径。**

当单条 Record 很大（比如几百字节甚至几 KB）时，直接交换整条 Record 的代价会随着
记录数增长而急剧膨胀。工程上的标准做法是建立一份指针/索引数组，只对指针排序。

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-direct-swap-large-struct](example-1-direct-swap-large-struct/) | 直接交换大结构体（反面示例） | 统计"交换次数 x sizeof(Record)"，量化直接交换的代价 |
| [example-2-pointer-index-sort](example-2-pointer-index-sort/) | 建立 `Record* index[N]`，只排序指针 | 原始数组原地不动，搬移量从 MB 级降到 KB 级 |
| [example-3-ordered-view-without-reorder](example-3-ordered-view-without-reorder/) | 同一份数据的多个"有序视图" | 同时维护按不同字段排序的多个索引，数据永远只存一份 |

## 心智模式回顾

> **对象保持不动，排序的是访问路径。**

三个示例形成一条递进的逻辑链：

```
example-1: 直接交换 Record       -> 搬移量 = 交换次数 x sizeof(Record)（很大）
                ↓
example-2: 只排序 Record* index  -> 搬移量 = 交换次数 x sizeof(指针)（很小）
                ↓
example-3: 同一份数据多个视图     -> 数据"零拷贝"，可以同时存在多种排序结果
```

Python 补充说明：Python 的 `list` 本身存的就是对象引用，`sorted()`/`list.sort()`
从来不会拷贝对象本体，天然等价于 C/C++ 里手动维护的"指针索引数组"。
`example-1` 为了真实复现"值类型大结构体"的搬移代价，特意使用了 `ctypes.Structure`。
