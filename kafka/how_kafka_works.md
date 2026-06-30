# Kafka 核心概念、工作原理、心智模型与 CLI 最佳实践

> 目标：
>
> - 理解 Kafka 为什么快
> - 建立 Kafka 的整体架构心智模型
> - 掌握 Producer → Broker → Consumer 的完整数据流
> - 理解 Topic / Partition / Offset / Consumer Group 等核心概念
> - 熟悉日常开发与运维最常用 CLI

---

# 一、Kafka 是什么？

一句话：

> Kafka 是一个**分布式、可持久化、高吞吐、可扩展的 Commit Log（提交日志）系统**。

很多人认为 Kafka 是：

```
Message Queue
```

其实更准确应该理解成：

```
                 Kafka

          +----------------+
          | Distributed Log|
          +----------------+

                    │
        ┌───────────┼────────────┐
        │           │            │
        ▼           ▼            ▼

 Message Queue   Event Bus   Streaming Platform
```

所以 Kafka 有三个身份：

- Message Queue
- Event Streaming
- Distributed Log

---

# 二、Kafka整体架构

```
                    Producer

                        │

             append message

                        │

                        ▼

               +----------------+
               |     Broker     |
               +----------------+

        Topic A

        Partition0
        Partition1
        Partition2
        Partition3

               │
               │

        Consumer Group

        Consumer1
        Consumer2
        Consumer3
```

Broker 保存数据。

Producer 写。

Consumer 读。

---

# 三、Kafka 的核心组件

```
Kafka Cluster

        │

        ├──────── Broker1
        │
        ├──────── Broker2
        │
        ├──────── Broker3
        │
        └──────── Broker4
```

Broker：

就是 Kafka Server。

可以理解成：

```
Broker

≈

Storage Server
```

每个 Broker：

- 保存数据
- 接收 Producer
- 服务 Consumer

---

# 四、Topic

Topic 可以理解成：

```
Linux

directory
```

例如：

```
logs

metrics

orders

payment
```

Producer：

```
write -> topic
```

Consumer：

```
read <- topic
```

例如：

```
orders

    OrderCreated

    OrderPaid

    OrderCancelled
```

---

# 五、Partition（最重要）

Topic 不是真正存数据。

真正存数据的是：

```
Topic

    Partition0

    Partition1

    Partition2
```

例如：

```
Topic: orders

+----------------------+

Partition0

Partition1

Partition2

Partition3

+----------------------+
```

为什么？

为了：

```
Parallelism
```

例如：

```
Producer

        │

        ▼

Partition0

Partition1

Partition2

Partition3
```

可以同时写。

Consumer：

```
Consumer1

Consumer2

Consumer3
```

可以同时读。

所以：

Partition 是 Kafka 扩展性的核心。

---

# 六、Partition 本质

Partition 本质就是：

```
Append Only Log
```

例如：

```
Offset

0

1

2

3

4

5

6

7

8

9

10
```

消息：

```
0 hello

1 world

2 kafka

3 rocks
```

永远：

```
append

append

append

append
```

不会插入。

不会修改。

不会覆盖。

所以速度极快。

---

# 七、Offset

Offset：

就是：

```
数组下标
```

例如：

```
Offset

0

1

2

3

4

5
```

Producer：

```
append
```

Consumer：

```
read offset=3
```

继续：

```
offset=4

offset=5
```

所以：

Offset = 消息位置。

---

# 八、为什么 Kafka 快？

原因一：

Append Only

```
Disk

------------>

append

append

append
```

没有随机写。

---

原因二：

Sequential IO

```
Disk

>>>>>>>>>>>>>>>>>>>>>
```

顺序写。

---

原因三：

Page Cache

```
Application

↓

Kernel Page Cache

↓

Disk
```

大量写都先写缓存。

---

原因四：

Zero Copy

```
Disk

↓

Kernel

↓

NIC
```

数据几乎不用复制到用户态。

---

原因五：

Batch

Producer：

```
100 messages

↓

一次发送
```

Consumer：

```
1000 messages

↓

一次读取
```

---

# 九、Replication

Kafka：

不是只有一个副本。

例如：

```
Partition0

Leader

Follower1

Follower2
```

Leader：

负责：

```
Read

Write
```

Follower：

同步 Leader。

如果：

```
Leader Down
```

新的：

```
Follower

↓

Leader
```

---

# 十、Producer 工作原理

```
Application

↓

Producer API

↓

Serializer

↓

Partitioner

↓

Batch

↓

Compress

↓

Network

↓

Broker
```

Producer 并不是：

```
send()

↓

立即发送
```

而是：

```
缓存

↓

批量发送
```

---

# 十一、Partition 如何选择？

默认：

```
key 有：

hash(key)

↓

partition
```

没有 key：

```
Round Robin
```

例如：

```
A

↓

Partition0

B

↓

Partition1

C

↓

Partition2
```

---

# 十二、Consumer 工作原理

```
Consumer

↓

Fetch Request

↓

Broker

↓

Batch Message

↓

Consumer
```

Consumer：

主动拉（Pull）。

不是 Broker 推（Push）。

所以：

Kafka 是：

```
Pull Model
```

---

# 十三、Consumer Group（最重要）

例如：

```
Topic

Partition0

Partition1

Partition2

Partition3
```

Consumer Group：

```
GroupA

Consumer1

Consumer2
```

分配：

```
Consumer1

P0

P2

Consumer2

P1

P3
```

原则：

```
一个 Partition

同一时间

只能被 Group 内一个 Consumer 消费
```

这样：

不会重复消费。

---

# 十四、多个 Consumer Group

```
Topic

↓

Group A

↓

Group B

↓

Group C
```

每个 Group：

维护自己的 Offset。

例如：

```
GroupA

offset=100

GroupB

offset=55

GroupC

offset=300
```

互不影响。

---

# 十五、Kafka 数据流心智模型

```
Producer

↓

Topic

↓

Partition

↓

Append Log

↓

Offset

↓

Replication

↓

Consumer Group

↓

Consumer
```

记住：

```
Producer

↓

Partition

↓

Offset

↓

Consumer
```

Kafka 就已经理解了一半。

---

# 十六、Kafka 数据生命周期

```
Producer

↓

Serialize

↓

Partition

↓

Batch

↓

Broker

↓

Disk(Log)

↓

Replication

↓

Consumer Fetch

↓

Deserialize

↓

Application
```

---

# 十七、Kafka 的心智模型

## 心智模型一：Kafka 是日志，不是队列

```
RabbitMQ

Message Queue

消费后删除
```

Kafka：

```
Commit Log

消费后仍然存在
```

消息不会因为消费而删除。

---

## 心智模型二：Topic 是目录

```
logs/

metrics/

orders/
```

里面真正的数据：

```
Partition
```

---

## 心智模型三：Partition 就是一根不断增长的日志

```
Offset

0

1

2

3

4

5

6
```

永远：

```
append
```

---

## 心智模型四：Offset 就是数组下标

```
vector

index

↓

Kafka

offset
```

Consumer：

保存：

```
读到哪里了
```

---

## 心智模型五：Consumer 自己决定读哪里

Broker：

不会记录：

```
Consumer

读到哪里
```

而是：

Consumer Group：

维护 Offset。

因此：

```
seek()

↓

重新消费
```

完全可以做到。

---

## 心智模型六：Kafka 是 Pull，不是 Push

```
Consumer

↓

我要100条

↓

Broker

↓

返回100条
```

Consumer 控制消费速度。

---

## 心智模型七：Consumer Group 是负载均衡单元

```
Topic

P0

P1

P2

P3

↓

Group

↓

Consumer1

Consumer2
```

Partition 自动分配。

---

## 心智模型八：Kafka 本质是一个分布式 Append-Only Log

```
Producer

↓

Append

↓

Disk

↓

Consumer

↓

Offset
```

整个 Kafka 都围绕这一个思想展开。

---

# 十八、Kafka 常用 CLI

> 以下命令以较新的 Kafka 版本（支持 KRaft 模式）为例。较旧版本可能需要额外指定 ZooKeeper。

---

## 1. 查看版本

```bash
kafka-topics.sh --version
```

---

## 2. 查看 Broker API 版本

```bash
kafka-broker-api-versions.sh \
    --bootstrap-server localhost:9092
```

---

## 3. 创建 Topic

```bash
kafka-topics.sh \
    --bootstrap-server localhost:9092 \
    --create \
    --topic orders \
    --partitions 3 \
    --replication-factor 2
```

说明：

| 参数 | 作用 |
|------|------|
| --topic | Topic 名称 |
| --partitions | 分区数量 |
| --replication-factor | 副本数量 |
| --bootstrap-server | Kafka 集群入口 |

---

## 4. 查看所有 Topic

```bash
kafka-topics.sh \
    --bootstrap-server localhost:9092 \
    --list
```

---

## 5. 查看 Topic 详情

```bash
kafka-topics.sh \
    --bootstrap-server localhost:9092 \
    --describe \
    --topic orders
```

示例输出：

```
PartitionCount:3
ReplicationFactor:2

Partition:0 Leader:1 Replicas:1,2 ISR:1,2
Partition:1 Leader:2 Replicas:2,3 ISR:2,3
Partition:2 Leader:3 Replicas:3,1 ISR:3,1
```

关注：

- Partition 数量
- Leader
- Replicas
- ISR（同步副本集合）

---

## 6. 删除 Topic

```bash
kafka-topics.sh \
    --bootstrap-server localhost:9092 \
    --delete \
    --topic orders
```

---

## 7. 修改 Partition 数

```bash
kafka-topics.sh \
    --bootstrap-server localhost:9092 \
    --alter \
    --topic orders \
    --partitions 8
```

> Partition 只能增加，不能减少。

---

## 8. 生产消息

```bash
kafka-console-producer.sh \
    --bootstrap-server localhost:9092 \
    --topic orders
```

输入：

```
hello
world
Kafka
```

---

## 9. 指定 Key 生产消息

```bash
kafka-console-producer.sh \
    --bootstrap-server localhost:9092 \
    --topic orders \
    --property parse.key=true \
    --property key.separator=:
```

输入：

```
user1:pay
user1:refund
user2:create
```

相同 Key 默认会落到同一个 Partition，保证顺序性。

---

## 10. 消费消息

```bash
kafka-console-consumer.sh \
    --bootstrap-server localhost:9092 \
    --topic orders
```

---

## 11. 从头开始消费

```bash
kafka-console-consumer.sh \
    --bootstrap-server localhost:9092 \
    --topic orders \
    --from-beginning
```

---

## 12. 显示 Offset、Partition、Key

```bash
kafka-console-consumer.sh \
    --bootstrap-server localhost:9092 \
    --topic orders \
    --from-beginning \
    --property print.offset=true \
    --property print.partition=true \
    --property print.key=true
```

示例输出：

```
Partition:0 Offset:15 Key:user1 Value:pay
Partition:1 Offset: 8 Key:user2 Value:create
```

---

## 13. 查看 Consumer Group

```bash
kafka-consumer-groups.sh \
    --bootstrap-server localhost:9092 \
    --list
```

---

## 14. 查看 Consumer Group 状态

```bash
kafka-consumer-groups.sh \
    --bootstrap-server localhost:9092 \
    --describe \
    --group payment-service
```

关键字段：

| 字段 | 含义 |
|------|------|
| CURRENT-OFFSET | 当前消费位置 |
| LOG-END-OFFSET | 分区最新消息位置 |
| LAG | 积压消息数 |
| CONSUMER-ID | 消费者实例 |
| HOST | 消费者主机 |
| CLIENT-ID | 客户端 ID |

---

## 15. 重置 Consumer Group Offset

查看可重置范围（仅预览）：

```bash
kafka-consumer-groups.sh \
    --bootstrap-server localhost:9092 \
    --group payment-service \
    --reset-offsets \
    --to-earliest \
    --topic orders
```

执行重置：

```bash
kafka-consumer-groups.sh \
    --bootstrap-server localhost:9092 \
    --group payment-service \
    --reset-offsets \
    --to-earliest \
    --topic orders \
    --execute
```

常见选项：

- `--to-earliest`
- `--to-latest`
- `--shift-by`
- `--to-offset`
- `--to-datetime`

---

## 16. 查看日志目录

```bash
kafka-log-dirs.sh \
    --bootstrap-server localhost:9092 \
    --describe
```

可查看：

- Broker 日志目录
- Partition 分布
- 磁盘使用情况

---

## 17. 查看集群元数据

```bash
kafka-metadata-shell.sh
```

（主要用于 KRaft 模式下查看元数据）

---

## 18. 查看 Producer 性能

```bash
kafka-producer-perf-test.sh \
    --topic orders \
    --num-records 1000000 \
    --record-size 1024 \
    --throughput -1 \
    --producer-props bootstrap.servers=localhost:9092
```

适用于压测 Producer 吞吐和延迟。

---

## 19. 查看 Consumer 性能

```bash
kafka-consumer-perf-test.sh \
    --bootstrap-server localhost:9092 \
    --topic orders \
    --messages 1000000
```

用于评估 Consumer 吞吐能力。

---

# 十九、CLI 心智模型

Kafka CLI 大致可分为四类：

```
                    Kafka CLI
                         │
     ┌───────────────────┼───────────────────┐
     │                   │                   │
     ▼                   ▼                   ▼
  Topic 管理         数据读写          消费管理
(kafka-topics)   (producer/consumer) (consumer-groups)
                         │
                         ▼
                  集群与诊断工具
         (broker-api、log-dirs、metadata、perf-test)
```

对应生命周期如下：

```
创建 Topic
     │
     ▼
Producer 写入
     │
     ▼
Broker 保存
     │
     ▼
Consumer Group 消费
     │
     ▼
查看 Lag / Offset
     │
     ▼
性能分析与故障排查
```

---

# 二十、总结

## Kafka 架构心智模型

```
              Producer
                  │
                  ▼
              Topic(逻辑)
                  │
      ┌───────────┴───────────┐
      ▼           ▼           ▼
 Partition0  Partition1  Partition2
      │           │           │
      └───────────┬───────────┘
                  ▼
           Append-Only Log
                  │
             Offset 索引
                  │
          Replication 副本
                  │
                  ▼
         Consumer Group
                  │
          Consumer 实例
```

## 一句话记住 Kafka

> **Kafka 的本质是一个分布式、可复制、可持久化的 Append-Only Commit Log；Producer 负责顺序追加消息，Broker 负责存储和复制，Consumer Group 通过维护 Offset 以 Pull 模式并行消费数据。**