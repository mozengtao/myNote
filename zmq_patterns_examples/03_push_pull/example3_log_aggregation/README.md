# 示例 3：多生产者单收集者日志聚合（Many-to-One）

3 个独立的服务实例（模拟微服务节点）各自产生日志，都往同一个 Collector 推送。Collector 只需要 `bind` 一个 PULL socket，就能自动接收所有节点的日志——这是 PUSH/PULL **多对一（fan-in）** 的典型用法。

## 场景

分布式系统里最常见的需求：N 个服务节点的日志需要汇总到一处（无论是打印、写文件还是转发给日志系统），且节点数量可能随时增减。

## 心智模型

```
LogProducer(serviceA) ──┐
LogProducer(serviceB) ──┼──▶ Collector (PULL, bind 一个端口)
LogProducer(serviceC) ──┘
```

跟 [示例1](../example1_task_distribution/) 的"一对多"正好相反：这里是**多个 PUSH 连接同一个 PULL**，ZMQ 自动把它们的消息合并到 Collector 的同一个接收队列里，顺序是**到达顺序**（谁先到先被收到），不是按 producer 轮流。

## 编译 & 运行（C）

固定配置：3 个 producer，每个发送 10 条日志，Collector 共等待 30 条。

```bash
make
./collector                     # 终端1：先起收集端
./log_producer serviceA &       # 终端2/3/4：起三个"服务节点"
./log_producer serviceB &
./log_producer serviceC &
```

## 运行（Python）

```bash
python3 collector.py
python3 log_producer.py serviceA &
python3 log_producer.py serviceB &
python3 log_producer.py serviceC &
```

## 预期输出（Collector，示例）

```
[Collector] Listening on tcp://*:5566, waiting for 30 logs total
[Collector] serviceA: log line 0
[Collector] serviceB: log line 0
[Collector] serviceA: log line 1
[Collector] serviceC: log line 0
...
[Collector] All 30 logs received.
[Collector] Summary: serviceA=10, serviceB=10, serviceC=10
```

## 关键点

- Collector **无需事先知道有多少个 producer**，也不用给每个 producer 分配单独的连接——bind 一个端口就能接受任意多个 PUSH 连接进来。
- 和 [示例2](../example2_three_stage_pipeline/) 的 Sink 阶段本质上是同一种用法，这里单独拎出来强调"多对一"这个场景本身（脱离流水线上下文）就非常常见和实用。
- 如果日志速率远大于 Collector 处理速率，PUSH 端会阻塞（不是丢弃），这跟 [PUB/SUB 慢订阅者丢消息](../../02_pub_sub/example3_slow_subscriber_hwm/) 的行为形成对比——务必根据"能不能丢"来选择 pattern。
