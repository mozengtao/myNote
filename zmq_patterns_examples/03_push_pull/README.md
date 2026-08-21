# PUSH / PULL —— 流水线（Pipeline）

## 心智模型

```
┌──────────┐
│ Producer │
└────┬─────┘
     │ Round-Robin 自动分发
 ┌───┼───┐
 ▼   ▼   ▼
Worker1 Worker2 Worker3
```

就像**工厂流水线**：Producer 把任务扔进传送带，ZMQ 自动把任务轮流（Round-Robin）分给空闲的 Worker，谁处理得快，谁就多分到一些（因为 ZMQ 内部按"谁的发送队列有空位"公平分发，不是死板地按顺序 1-2-3-1-2-3）。

## 关键特性

- **有队列、会阻塞、不会丢消息**（与 PUB/SUB 的"丢消息优先"形成鲜明对比）：如果所有 Worker 都忙、队列都满了（达到 HWM），`PUSH.send()` 会阻塞，而不是丢弃。
- PUSH 端不知道、也不关心是哪个 Worker 处理了任务。
- PULL 端只管收，不知道任务是谁发的。
- 单向：PUSH 只能发，PULL 只能收，不能反过来回复（如果需要"处理完通知回去"，通常再接一段 PUSH/PULL 反向管道，或者用 [SINK 汇聚阶段](example2_three_stage_pipeline/)）。

## 什么场景用 PUSH/PULL

- 任务分发 + 水平扩展的计算集群（多个 worker 抢任务）。
- 日志/事件的多生产者单消费者汇聚。
- 多阶段流水线处理（阶段间用 PUSH/PULL 串起来）。

## 什么场景不该用

- 需要"一对一应答"（谁发的任务，处理完要回给谁）-> 用 REQ/REP 或 DEALER/ROUTER。
- 需要广播给所有下游 -> 用 PUB/SUB（PUSH/PULL 是"分摊"，不是"广播"，每条消息只会被一个 Worker 收到）。

## 3 个示例

| 示例 | 场景 | 学到什么 |
|---|---|---|
| [example1_task_distribution](example1_task_distribution/) | 单 Producer + 多 Worker | 最基础的负载均衡分发 |
| [example2_three_stage_pipeline](example2_three_stage_pipeline/) | Producer → Workers → Sink | 经典三级流水线，多阶段串联 |
| [example3_log_aggregation](example3_log_aggregation/) | 多 Producer → 单 Collector | 多对一日志聚合（fan-in） |

每个示例目录下都有独立的 `README.md` 说明如何运行。
