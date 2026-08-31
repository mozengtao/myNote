# 工程排序最佳实践示例库

本目录是 [`sorting_mental_model.md`](../sorting_mental_model.md) 的配套实践项目。

> 核心心智模型：
>
> **排序算法只是工具，数据组织形式（Data Organization）才决定采用哪种排序策略。**
> 真正要问的不是"用什么排序算法"，而是"数据是怎么组织的？移动数据的成本是多少？如何建立有序视图？"

每一种数据组织形式，都对应一套典型的 **Comparator（比较器）+ Move Strategy（移动策略）** 组合。本目录为 10 种数据组织形式各选取了 3 个最具工程代表性的示例场景，每个场景都提供 **C / C++ / Python** 三种语言的完整可运行实现，方便对照学习同一个心智模式在不同语言中的落地方式。

## 目录导航

| 分类 | 数据组织形式 | 核心心智模式 |
|---|---|---|
| [01-array](01-array/README.md) | 连续数组 | 连续内存 + 小对象 → 直接排序，Cache 命中率最高 |
| [02-large-object-array](02-large-object-array/README.md) | 大对象数组 | 对象保持不动，排序的是访问路径 |
| [03-pointer-array](03-pointer-array/README.md) | 指针 / 引用数组 | 交换指针成本远低于交换对象本身 |
| [04-linked-list](04-linked-list/README.md) | 链表 | 链表排序不是移动数据，而是重新连接节点 |
| [05-tree](05-tree/README.md) | 树结构 | 维持平衡，而不是重新排序 |
| [06-heap](06-heap/README.md) | 堆 | 堆不是为了得到完整有序序列，而是为了快速获取当前最值 |
| [07-database-records](07-database-records/README.md) | 数据库记录 | 排序索引，而不是排序数据 |
| [08-external-sort](08-external-sort/README.md) | 外部排序 | 内存排序解决不了的问题，就把排序过程分阶段完成 |
| [09-bulk-buffer](09-bulk-buffer/README.md) | 批量缓冲区（通用化） | 只搬运指针/引用，不搬运大对象本体 |
| [10-task-scheduler](10-task-scheduler/README.md) | 任务调度（通用化） | 动态维护最值/优先级，而不是整体重排 |

关于第 9、10 类的说明：原文档使用 DPDK `rte_mbuf` 与 Linux CFS Scheduler 作为示例，但这两者依赖特定内核/硬件环境，脱离该环境难以做到真正"可运行"。这里改用消息队列、音视频帧处理、通用任务调度器等更常见的工程场景来传达完全相同的心智模式。

## 如何阅读每个示例

每个 `example-N-*/` 目录下都有三个文件：`sort.c`、`sort.cpp`、`sort.py`。它们描述的是**同一个场景**、**同一种输入输出**，只是分别用 C、C++、Python 实现，便于对比：

- 同样的问题，comparator 长什么样？
- 同样的问题，move strategy（交换对象 / 交换指针 / 修改链接 / 修改索引）长什么样？
- 语言特性（例如 Python 的一切皆引用、C++ 的智能指针/STL、C 的手动内存管理）如何影响实现方式？

## 编译与运行

```bash
# C
gcc -Wall -O2 -o sort_c sort.c && ./sort_c

# C++
g++ -Wall -std=c++17 -O2 -o sort_cpp sort.cpp && ./sort_cpp

# Python
python3 sort.py
```

## 最终总结

完整的最佳实践表格、设计原则与心智模型总结见 [`BEST_PRACTICES.md`](BEST_PRACTICES.md)。
