# 09. 批量缓冲区（Bulk Buffer）

> 原文引用（[`sorting_mental_model.md`](../../sorting_mental_model.md) 第四节 9，通用化改写）：
>
> 原文以 DPDK `rte_mbuf` 为例，强调网络数据包处理"只排指针，绝不搬运整个包"。
> 这里改用消息队列、音视频帧处理等更常见的工程场景，传达同样的心智模式。
>
> **心智模式：只搬运指针/引用，不搬运大对象或缓冲区本体。**

高吞吐场景（消息队列、音视频流、网络缓冲区）里，每条数据可能有几百字节到几 MB，
一旦需要按某种顺序重新处理这些数据（优先级、时间戳、序列号……），
绝不能直接搬移数据本体，只能搬移一个廉价的引用/指针。

## 示例列表

| 示例 | 场景 | 演示的要点 |
|---|---|---|
| [example-1-message-queue-batch-priority-sort](example-1-message-queue-batch-priority-sort/) | 消息队列批量出队按优先级排序 | 稳定排序保留同优先级的到达顺序（FIFO），只移动指针 |
| [example-2-media-frame-buffer-reorder](example-2-media-frame-buffer-reorder/) | 音视频帧按时间戳重排 | 网络抖动导致乱序到达，重建播放顺序不拷贝帧数据 |
| [example-3-copy-vs-pointer-cost-illustration](example-3-copy-vs-pointer-cost-illustration/) | 直接搬移 vs 只排指针成本对比 | 用相同交换次数量化两种方案的实际数据搬移量差距 |

## 心智模式回顾

> **只搬运指针/引用，不搬运大对象或缓冲区本体。**

这正是原文档里 DPDK `rte_mbuf` 场景想表达的核心思想——收发包时绝不能直接拷贝
整个数据包，而是在描述符（指针/索引）层面完成排序和调度。消息队列、音视频处理
等场景面对的是同样的工程约束：

```
example-1：优先级调度场景 -> 排消息指针，还需要稳定排序保序
example-2：网络乱序重排场景 -> 排帧指针，重建时间顺序
example-3：直接量化两种做法的搬移量差距，用数字说明为什么要选指针排序
```

这与 [02-large-object-array](../02-large-object-array/) 是同一个心智模型，
只是把"大对象数组"换成了更贴近网络/流处理工程场景的"批量缓冲区"。
