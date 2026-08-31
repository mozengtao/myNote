# 06. 堆（Heap）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 6）：
>
> 适用于：Top K、Scheduler、Timer、事件驱动。不是一次性排序，而是动态维护有序状态。
>
> **心智模式：堆不是为了得到完整有序序列，而是为了快速获取当前最值。**

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-top-k-priority-queue](example-1-top-k-priority-queue/) | 数据流 Top-K | 只维护 K 个候选元素，不存储/排序全部数据 |
| [example-2-heapsort-vs-library-heap](example-2-heapsort-vs-library-heap/) | 手写堆排序 vs 标准库堆 | 堆排序也能整体排序，但工程上很少这样用 |
| [example-3-event-timer-min-heap](example-3-event-timer-min-heap/) | 定时器/事件队列 | 新事件随时加入，堆持续维护"当前最早触发"的能力 |

## 心智模式回顾

> **堆不是为了得到完整有序序列，而是为了快速获取当前最值。**

三个示例呈现了堆在工程中的完整光谱：

```
example-1：只关心"最大/最小的 K 个"，天然适合用堆过滤海量数据流
example-2：堆也能做"整体排序"，但通常不如 Introsort/Timsort 快，
           工程上很少真的用堆排序去排序数据
example-3：堆最大的价值——支持"边使用边更新"的动态最值维护
```

工程实践提示：

```
C            没有内置堆，需要自行实现（如 example 中的数组二叉堆）
C++          std::priority_queue / std::make_heap+std::sort_heap
Python       heapq 模块
```

真正决定要不要用堆的问题是：**这批数据只需要排一次，还是需要一边使用一边持续加入新数据？**
前者优先用语言自带的排序（Introsort/Timsort），后者才是堆真正大展身手的地方。
