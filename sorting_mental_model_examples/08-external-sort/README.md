# 08. 外部排序（External Sort）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 8）：
>
> 场景：数据量远超内存容量（数据库、大文件、分布式系统）。
> 步骤：分块排序 -> 落盘临时文件 -> 多路归并。
>
> **心智模式：内存排序解决不了的问题，就把排序过程分阶段完成。**

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-chunk-and-spill-to-disk](example-1-chunk-and-spill-to-disk/) | 分块排序 + 落盘 | 数据切成内存放得下的小块，块内排序后立刻写盘 |
| [example-2-k-way-merge-with-heap](example-2-k-way-merge-with-heap/) | K 路归并 | 用最小堆把多个有序小文件合并成一个整体有序结果 |
| [example-3-full-external-merge-sort-pipeline](example-3-full-external-merge-sort-pipeline/) | 完整流程 | 生成 -> 分块排序 -> 落盘 -> K 路归并 -> 与内存排序结果校验 |

## 心智模式回顾

> **内存排序解决不了的问题，就把排序过程分阶段完成。**

三个示例其实是同一条流水线的三个视角：

```
example-1：流水线的第一阶段（分块 + 落盘）
example-2：流水线的第二阶段（K 路归并），单独拿出来演示堆的作用
example-3：把两个阶段串起来，跑一次完整的 External Merge Sort，并做正确性校验
```

工程实践提示：

```
数据库           排序缓冲区不够时触发 External Merge Sort
Hadoop/Spark     Shuffle 阶段本质上就是"分块排序 + 多路归并"
Unix sort 命令   处理超大文件时自动切换到外部排序模式
```

无论具体系统如何实现，核心思路都没有变过：**先在能装进内存的范围内排序，
再用归并这种"只需要顺序读取"的操作把结果拼接起来**。
