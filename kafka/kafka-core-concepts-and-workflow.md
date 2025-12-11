# Kafka 核心概念与工作原理详解

---

## 目录

1. [Kafka 基础架构](#1-kafka-基础架构)
2. [核心组件详解](#2-核心组件详解)
3. [数据流动过程](#3-数据流动过程)
4. [分布式特性](#4-分布式特性)
5. [Producer 发送流程](#5-producer-发送流程)
6. [Consumer 接收流程](#6-consumer-接收流程)
7. [librdkafka 实现详解](#7-librdkafka-实现详解)
8. [关键 API 说明](#8-关键-api-说明)
9. [消息流转总结](#9-消息流转总结)

---

## 1. Kafka 基础架构

```
+==============================================================================+
|                        KAFKA CLUSTER ARCHITECTURE                            |
+==============================================================================+

                              ┌─────────────────────────────────────────────┐
                              │              KAFKA CLUSTER                   │
                              │  ┌─────────┐ ┌─────────┐ ┌─────────┐        │
                              │  │ Broker  │ │ Broker  │ │ Broker  │        │
                              │  │    1    │ │    2    │ │    3    │        │
                              │  │         │ │         │ │         │        │
                              │  │ ┌─────┐ │ │ ┌─────┐ │ │ ┌─────┐ │        │
    ┌──────────┐              │  │ │ P0  │ │ │ │ P1  │ │ │ │ P2  │ │        │
    │ Producer │──────────────┼─▶│ │     │ │ │ │     │ │ │ │     │ │        │
    │    1     │   write      │  │ └─────┘ │ │ └─────┘ │ │ └─────┘ │        │
    └──────────┘              │  │ ┌─────┐ │ │ ┌─────┐ │ │ ┌─────┐ │        │
                              │  │ │ P3  │ │ │ │ P4  │ │ │ │ P5  │ │        │
    ┌──────────┐              │  │ │     │ │ │ │     │ │ │ │     │ │        │
    │ Producer │──────────────┼─▶│ └─────┘ │ │ └─────┘ │ │ └─────┘ │        │
    │    2     │   write      │  └─────────┘ └─────────┘ └─────────┘        │
    └──────────┘              │        │          │          │              │
                              │        │          │          │              │
                              │        ▼          ▼          ▼              │
                              │  ┌─────────────────────────────────┐        │
                              │  │         ZooKeeper / KRaft       │        │
                              │  │    (Cluster Coordination)       │        │
                              │  └─────────────────────────────────┘        │
                              └──────────────────│──────────────────────────┘
                                                 │
                         ┌───────────────────────┼───────────────────────┐
                         │                       │                       │
                         ▼                       ▼                       ▼
                   ┌──────────┐           ┌──────────┐           ┌──────────┐
                   │ Consumer │           │ Consumer │           │ Consumer │
                   │    1     │           │    2     │           │    3     │
                   └──────────┘           └──────────┘           └──────────┘
                   
                   └─────────────────────────────────────────────────────────┘
                                     Consumer Group
```

### 中文说明

Kafka 是一个分布式消息系统，采用发布-订阅模式。上图展示了 Kafka 的基础架构：

- **Producer（生产者）**：消息的生产方，将消息发送到 Kafka 集群
- **Kafka Cluster（Kafka 集群）**：由多个 Broker 组成，负责存储和转发消息
- **Broker（代理节点）**：Kafka 服务器，每个 Broker 存储一部分分区数据
- **Partition（分区）**：Topic 的物理分片，图中 P0-P5 分布在不同 Broker
- **ZooKeeper/KRaft**：集群协调服务，管理元数据和 Leader 选举
- **Consumer（消费者）**：消息的消费方，从 Kafka 拉取消息
- **Consumer Group（消费者组）**：多个消费者组成的逻辑组，实现负载均衡

---

## 2. 核心组件详解

```
+==============================================================================+
|                         KAFKA CORE COMPONENTS                                |
+==============================================================================+

    ┌────────────────────────────────────────────────────────────────────────┐
    │                              TOPIC                                      │
    │                                                                         │
    │   A logical channel for organizing messages (e.g., "snmp-data")        │
    │                                                                         │
    │   ┌─────────────────────────────────────────────────────────────────┐  │
    │   │                      PARTITIONS                                  │  │
    │   │                                                                  │  │
    │   │  ┌─────────────────────────────────────────────────────────┐   │  │
    │   │  │ Partition 0                                             │   │  │
    │   │  │ ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐               │   │  │
    │   │  │ │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │...│  ──────────▶ │   │  │
    │   │  │ └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘     write     │   │  │
    │   │  │   │                               │                      │   │  │
    │   │  │   │                               │                      │   │  │
    │   │  │ oldest                          newest                   │   │  │
    │   │  │ offset=0                        offset=8                 │   │  │
    │   │  └─────────────────────────────────────────────────────────┘   │  │
    │   │                                                                  │  │
    │   │  ┌─────────────────────────────────────────────────────────┐   │  │
    │   │  │ Partition 1                                              │   │  │
    │   │  │ ┌───┬───┬───┬───┬───┬───┬───┐                           │   │  │
    │   │  │ │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │...│  ──────────────────────▶ │   │  │
    │   │  │ └───┴───┴───┴───┴───┴───┴───┘                           │   │  │
    │   │  └─────────────────────────────────────────────────────────┘   │  │
    │   │                                                                  │  │
    │   │  ┌─────────────────────────────────────────────────────────┐   │  │
    │   │  │ Partition 2                                              │   │  │
    │   │  │ ┌───┬───┬───┬───┬───┐                                   │   │  │
    │   │  │ │ 0 │ 1 │ 2 │ 3 │...│  ────────────────────────────▶   │   │  │
    │   │  │ └───┴───┴───┴───┴───┘                                   │   │  │
    │   │  └─────────────────────────────────────────────────────────┘   │  │
    │   │                                                                  │  │
    │   └─────────────────────────────────────────────────────────────────┘  │
    │                                                                         │
    │   Each box (0,1,2...) is a MESSAGE with unique OFFSET                  │
    │                                                                         │
    └────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────┐
    │                           MESSAGE STRUCTURE                              │
    │                                                                          │
    │    ┌──────────────────────────────────────────────────────────────────┐ │
    │    │                         MESSAGE                                   │ │
    │    │                                                                   │ │
    │    │  ┌──────────┐ ┌──────────┐ ┌─────────┐ ┌─────────┐ ┌──────────┐ │ │
    │    │  │  OFFSET  │ │TIMESTAMP │ │   KEY   │ │  VALUE  │ │ HEADERS  │ │ │
    │    │  │ (int64)  │ │ (int64)  │ │ (bytes) │ │ (bytes) │ │ (k-v[])  │ │ │
    │    │  └──────────┘ └──────────┘ └─────────┘ └─────────┘ └──────────┘ │ │
    │    │                                                                   │ │
    │    │  Offset: 唯一标识分区内消息位置                                     │ │
    │    │  Timestamp: 消息时间戳                                            │ │
    │    │  Key: 用于分区路由（可选）                                         │ │
    │    │  Value: 消息实际内容（payload）                                    │ │
    │    │  Headers: 元数据键值对（可选）                                      │ │
    │    │                                                                   │ │
    │    └──────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────┘
```

### 中文说明

#### Topic（主题）
- 消息的逻辑分类，类似于数据库中的"表"
- 一个 Topic 可以有多个 Partition
- 例如：`snmp-data`、`device-notifications`

#### Partition（分区）
- Topic 的物理分片，实现并行处理
- 每个 Partition 是一个有序、不可变的消息序列
- 消息在 Partition 内按顺序追加写入
- 不同 Partition 之间消息**不保证顺序**，**不会重复**

#### Offset（偏移量）
- 分区内消息的唯一标识
- 从 0 开始递增
- Consumer 通过 Offset 追踪消费进度

#### Message（消息）
- Kafka 传输的基本单元
- 包含：Offset、Timestamp、Key、Value、Headers

---

## 3. 数据流动过程

```
+==============================================================================+
|                         DATA FLOW IN KAFKA                                   |
+==============================================================================+

    PRODUCER SIDE                    KAFKA CLUSTER                 CONSUMER SIDE
    ═════════════                    ═════════════                 ═════════════
    
    ┌──────────────┐                                              
    │  Application │                                              
    │     Data     │                                              
    └──────┬───────┘                                              
           │                                                       
           ▼                                                       
    ┌──────────────┐                                              
    │  Serializer  │   Convert data                               
    │   (encode)   │   to bytes                                   
    └──────┬───────┘                                              
           │                                                       
           ▼                                                       
    ┌──────────────┐                                              
    │  Partitioner │   Determine                                  
    │              │   partition                                  
    └──────┬───────┘                                              
           │                                                       
           │ key hash / round-robin / explicit                    
           │                                                       
           ▼                                                       
    ┌──────────────┐     ┌─────────────────────────────────────────────────────┐
    │   Producer   │     │                   BROKER                             │
    │ Send Buffer  │     │  ┌───────────────────────────────────────────────┐  │
    │              │     │  │              Topic: snmp-data                  │  │
    │  ┌────────┐  │     │  │                                               │  │
    │  │ Batch  │──┼─────┼─▶│  ┌─────────┐  ┌─────────┐  ┌─────────┐       │  │
    │  │   1    │  │write│  │  │  P0     │  │   P1    │  │   P2    │       │  │
    │  └────────┘  │     │  │  │┌─┬─┬─┐  │  │┌─┬─┬─┐  │  │┌─┬─┬─┐  │       │  │
    │  ┌────────┐  │     │  │  ││0│1│2│  │  ││0│1│2│  │  ││0│1│2│  │       │  │
    │  │ Batch  │  │     │  │  │└─┴─┴─┘  │  │└─┴─┴─┘  │  │└─┴─┴─┘  │       │  │
    │  │   2    │  │     │  │  └─────────┘  └─────────┘  └─────────┘       │  │
    │  └────────┘  │     │  │       │            │            │            │  │
    │              │     │  │       │            │            │            │  │
    └──────────────┘     │  │       ▼            ▼            ▼            │  │
                         │  │  ┌────────────────────────────────────┐      │  │
           ▲             │  │  │          LOG STORAGE               │      │  │
           │             │  │  │   (Append-only, Immutable)         │      │  │
     ACK   │             │  │  └────────────────────────────────────┘      │  │
  (acks=   │             │  │                                               │  │
   0/1/all)│             │  └───────────────────────────────────────────────┘  │
           │             │                         │                            │
           │             │                         │                            │
           └─────────────┴─────────────────────────┘                            │
                         │                         │                            │
                         └─────────────────────────┼────────────────────────────┘
                                                   │
                                                   │ pull (fetch)
                                                   │
                                                   ▼
                                            ┌──────────────┐
                                            │   Consumer   │
                                            │ Fetch Buffer │
                                            │              │
                                            │  ┌────────┐  │
                                            │  │Messages│  │
                                            │  │ Batch  │  │
                                            │  └────────┘  │
                                            └──────┬───────┘
                                                   │
                                                   ▼
                                            ┌──────────────┐
                                            │ Deserializer │
                                            │   (decode)   │
                                            └──────┬───────┘
                                                   │
                                                   ▼
                                            ┌──────────────┐
                                            │  Application │
                                            │   Process    │
                                            └──────────────┘
```

### 中文说明

#### Producer 发送流程
1. **应用层数据**：业务数据准备发送
2. **序列化**：将数据转换为字节数组
3. **分区选择**：
   - **Key Hash**：相同 Key 的消息发送到同一分区
   - **Round-Robin**：无 Key 时轮询分配
   - **显式指定**：直接指定分区号
4. **批量发送**：消息先进入发送缓冲区，批量发送提高吞吐量
5. **ACK 确认**：
   - `acks=0`：不等待确认
   - `acks=1`：Leader 写入后确认
   - `acks=all`：所有副本写入后确认

#### Consumer 接收流程
1. **拉取消息**：Consumer 主动从 Broker 拉取（Pull 模式）
2. **批量获取**：一次拉取多条消息
3. **反序列化**：字节数组转换为应用数据
4. **业务处理**：应用层处理消息
5. **提交 Offset**：记录消费进度

---

## 4. 分布式特性

```
+==============================================================================+
|                     DISTRIBUTED FEATURES                                     |
+==============================================================================+

    【Replication - 副本机制】
    ═══════════════════════════════════════════════════════════════════════════
    
    Topic: snmp-data (Partition 0, Replication Factor = 3)
    
        Broker 1                  Broker 2                  Broker 3
        ════════                  ════════                  ════════
        
        ┌─────────────┐          ┌─────────────┐          ┌─────────────┐
        │ Partition 0 │          │ Partition 0 │          │ Partition 0 │
        │   LEADER    │  sync    │  FOLLOWER   │  sync    │  FOLLOWER   │
        │ ┌─┬─┬─┬─┐   │ ──────▶  │ ┌─┬─┬─┬─┐   │ ◀──────  │ ┌─┬─┬─┬─┐   │
        │ │0│1│2│3│   │          │ │0│1│2│3│   │          │ │0│1│2│3│   │
        │ └─┴─┴─┴─┘   │          │ └─┴─┴─┴─┘   │          │ └─┴─┴─┴─┘   │
        └──────┬──────┘          └─────────────┘          └─────────────┘
               │
               │ Producer writes to Leader only
               │ Consumer reads from Leader
               ▼
        ┌─────────────┐
        │  Producer/  │
        │  Consumer   │
        └─────────────┘

    
    【Leader Election - Leader 选举】
    ═══════════════════════════════════════════════════════════════════════════
    
        BEFORE FAILURE:                        AFTER BROKER 1 FAILURE:
        
        Broker 1 ──> [Leader P0]               Broker 1 ──> [FAILED] ✗
                           │                              
                           ▼                              
        Broker 2 ──> [Follower P0]             Broker 2 ──> [Leader P0] ✓ (elected)
                           │                              
                           ▼                              
        Broker 3 ──> [Follower P0]             Broker 3 ──> [Follower P0]
        
                                               ZooKeeper/KRaft detects failure
                                               and promotes Follower to Leader


    【Consumer Group - 消费者组】
    ═══════════════════════════════════════════════════════════════════════════
    
    Topic: snmp-data (6 Partitions)
    
                         Consumer Group A                    Consumer Group B
                    ┌───────────────────────┐           ┌───────────────────────┐
                    │                       │           │                       │
    ┌─────────────┐ │  ┌────────────────┐  │           │  ┌────────────────┐  │
    │ Partition 0 │◀┼──│   Consumer 1   │  │           │  │   Consumer X   │──┼▶ P0
    │ Partition 1 │◀┼──│   (P0, P1)     │  │           │  │   (all 6)      │──┼▶ P1
    └─────────────┘ │  └────────────────┘  │           │  │                │──┼▶ P2
                    │                       │           │  │    (single     │──┼▶ P3
    ┌─────────────┐ │  ┌────────────────┐  │           │  │   consumer     │──┼▶ P4
    │ Partition 2 │◀┼──│   Consumer 2   │  │           │  │   reads all)   │──┼▶ P5
    │ Partition 3 │◀┼──│   (P2, P3)     │  │           │  └────────────────┘  │
    └─────────────┘ │  └────────────────┘  │           │                       │
                    │                       │           │                       │
    ┌─────────────┐ │  ┌────────────────┐  │           └───────────────────────┘
    │ Partition 4 │◀┼──│   Consumer 3   │  │
    │ Partition 5 │◀┼──│   (P4, P5)     │  │           Each Consumer Group
    └─────────────┘ │  └────────────────┘  │           receives ALL messages
                    │                       │           independently
                    └───────────────────────┘
                    
                    Partitions are evenly
                    distributed among
                    consumers in same group
```

### 中文说明

#### Replication（副本机制）
- 每个 Partition 有多个副本（Replica）
- **Leader**：处理所有读写请求
- **Follower**：从 Leader 同步数据，作为备份
- **ISR（In-Sync Replicas）**：与 Leader 保持同步的副本集合

#### Leader Election（Leader 选举）
- 当 Leader 所在 Broker 故障时，ZooKeeper/KRaft 触发选举
- 从 ISR 中选择一个 Follower 提升为新 Leader
- 保证服务的高可用性

#### Consumer Group（消费者组）
- 同一 Group 内的 Consumer 共同消费一个 Topic
- 每个 Partition 只被 Group 内的一个 Consumer 消费
- 不同 Group 独立消费，都能收到全量消息
- Consumer 数量不应超过 Partition 数量

---

## 5. Producer 发送流程

```
+==============================================================================+
|                      PRODUCER SEND WORKFLOW                                  |
+==============================================================================+

                        ┌──────────────────────────────────────────────────────┐
                        │               PRODUCER APPLICATION                    │
                        │                                                       │
                        │  kafka_producer_send(header, payload, len)           │
                        │                                                       │
                        └───────────────────────┬──────────────────────────────┘
                                                │
                        ┌───────────────────────▼──────────────────────────────┐
                        │                  VALIDATION                          │
                        │                                                       │
                        │  1. Check header != NULL                              │
                        │  2. Check payload != NULL                             │
                        │  3. Check kafka_producer_status() == true             │
                        │  4. Check topic is not empty                          │
                        │                                                       │
                        └───────────────────────┬──────────────────────────────┘
                                                │
                        ┌───────────────────────▼──────────────────────────────┐
                        │              PREPARE MESSAGE                          │
                        │                                                       │
                        │  1. Get partition: get_kafka_producer_partition()     │
                        │  2. Create headers: kafka_producer_header_new()       │
                        │     - msg_type, src, dest, req_id                     │
                        │     - msg_sub_type, op, first, last                   │
                        │                                                       │
                        └───────────────────────┬──────────────────────────────┘
                                                │
                        ┌───────────────────────▼──────────────────────────────┐
                        │               rd_kafka_producev()                     │
                        │                                                       │
                        │  ┌─────────────────────────────────────────────────┐ │
                        │  │  rd_kafka_producev(                             │ │
                        │  │      kafka_producer_rk,        // Producer      │ │
                        │  │      RD_KAFKA_V_PARTITION(partition),           │ │
                        │  │      RD_KAFKA_V_HEADERS(hdrs), // Headers       │ │
                        │  │      RD_KAFKA_V_KEY(key, len), // Message Key   │ │
                        │  │      RD_KAFKA_V_VALUE(payload, len),            │ │
                        │  │      RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),  │ │
                        │  │      RD_KAFKA_V_TOPIC(topic),  // Topic name    │ │
                        │  │      RD_KAFKA_V_END);                           │ │
                        │  └─────────────────────────────────────────────────┘ │
                        │                                                       │
                        │  Message is queued in internal buffer (async)         │
                        │                                                       │
                        └───────────────────────┬──────────────────────────────┘
                                                │
                        ┌───────────────────────▼──────────────────────────────┐
                        │                rd_kafka_poll()                        │
                        │                                                       │
                        │  ┌─────────────────────────────────────────────────┐ │
                        │  │  rd_kafka_poll(kafka_producer_rk, 0);           │ │
                        │  │                                                 │ │
                        │  │  - Triggers background I/O                      │ │
                        │  │  - Processes delivery report callbacks          │ │
                        │  │  - Non-blocking (timeout = 0)                   │ │
                        │  └─────────────────────────────────────────────────┘ │
                        │                                                       │
                        └───────────────────────┬──────────────────────────────┘
                                                │
                        ┌───────────────────────▼──────────────────────────────┐
                        │                ERROR HANDLING                        │
                        │                                                       │
                        │  if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL)           │
                        │      rd_kafka_poll(rk, KAFKA_POLL_TIMEOUT_MS);       │
                        │      // Wait and retry                                │
                        │                                                       │
                        │  if (err == RD_KAFKA_RESP_ERR__FATAL)                │
                        │      rd_kafka_dump(...);  // Dump debug info          │
                        │      kafka_producer_restart();  // Restart client     │
                        │                                                       │
                        └──────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────┐
    │                   INTERNAL ASYNC FLOW                                   │
    │                                                                          │
    │   Application          librdkafka Internal              Kafka Broker    │
    │   Thread               Background Thread                                 │
    │   ═══════════          ══════════════════                ════════════   │
    │                                                                          │
    │   rd_kafka_producev()                                                    │
    │         │                                                                │
    │         ▼                                                                │
    │   ┌───────────┐                                                          │
    │   │  Internal │    batch & compress                                      │
    │   │   Queue   │ ─────────────────────▶  ┌─────────────┐                 │
    │   │           │                         │  Network    │                 │
    │   └───────────┘                         │  Send       │─────────────────│
    │                                         │  Buffer     │    TCP/TLS      │
    │   rd_kafka_poll()                       └──────┬──────┘       │         │
    │         │                                      │              │         │
    │         │                                      │              ▼         │
    │         │                              ┌───────▼───────┐  ┌───────────┐ │
    │         │                              │   Delivery    │  │  BROKER   │ │
    │         │  invoke callback             │    Report     │◀─│   ACK     │ │
    │         │◀─────────────────────────────│   Callback    │  └───────────┘ │
    │         ▼                              └───────────────┘                 │
    │   msg_delivered()                                                        │
    │                                                                          │
    └─────────────────────────────────────────────────────────────────────────┘
```

### 中文说明

#### Producer 发送步骤
1. **参数校验**：检查 header、payload 是否为空，Kafka 是否已初始化
2. **消息准备**：
   - 获取目标分区
   - 创建 Kafka Headers（包含消息类型、源、目的等元数据）
3. **异步发送**：`rd_kafka_producev()` 将消息放入内部队列
4. **触发 I/O**：`rd_kafka_poll()` 驱动后台线程发送
5. **错误处理**：
   - **QUEUE_FULL**：等待后重试
   - **FATAL**：重启 Kafka 客户端

#### 异步机制
- `rd_kafka_producev()` 是非阻塞的，消息先进入内部队列
- 后台线程负责批量发送和网络 I/O
- Delivery Report 回调通知发送结果
- `rd_kafka_poll()` 处理回调和触发 I/O

---

## 6. Consumer 接收流程

```
+==============================================================================+
|                      CONSUMER RECEIVE WORKFLOW                               |
+==============================================================================+

    ┌──────────────────────────────────────────────────────────────────────────┐
    │                        CONSUMER INITIALIZATION                           │
    │                                                                          │
    │  ┌────────────────────────────────────────────────────────────────────┐  │
    │  │  rd_kafka_conf_t *conf = rd_kafka_conf_new();                      │  │
    │  │  rd_kafka_conf_set(conf, "group.id", "my-group", ...);             │  │
    │  │  rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", ...);    │  │
    │  │                                                                    │  │
    │  │  rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, ...);      │  │
    │  │  rd_kafka_brokers_add(rk, "broker1:9092,broker2:9092");            │  │
    │  └────────────────────────────────────────────────────────────────────┘  │
    │                                                                          │
    └──────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
    ┌──────────────────────────────────────────────────────────────────────────┐
    │                          SUBSCRIBE TO TOPIC                               │
    │                                                                           │
    │  ┌────────────────────────────────────────────────────────────────────┐  │
    │  │  rd_kafka_topic_partition_list_t *topics;                          │  │
    │  │  topics = rd_kafka_topic_partition_list_new(1);                    │  │
    │  │  rd_kafka_topic_partition_list_add(topics, "snmp-data", -1);       │  │
    │  │                                    // -1 = all partitions          │  │
    │  │  rd_kafka_subscribe(rk, topics);                                   │  │
    │  └────────────────────────────────────────────────────────────────────┘  │
    │                                                                           │
    └──────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
    ┌──────────────────────────────────────────────────────────────────────────┐
    │                         CONSUMER POLL LOOP                               │
    │                                                                          │
    │  while (running) {                                                       │
    │      rd_kafka_message_t *msg;                                            │
    │      msg = rd_kafka_consumer_poll(rk, 1000);  // Wait 1s                 │
    │                                                                          │
    │      if (msg == NULL) continue;  // Timeout, no message                  │
    │                                                                          │
    │      if (msg->err) {                                                     │
    │          // Handle error                                                 │
    │          if (msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF)               │
    │              printf("End of partition\n");                               │
    │          else                                                             │
    │              fprintf(stderr, "Error: %s\n", rd_kafka_message_errstr(msg));│
    │      } else {                                                             │
    │          // Process message                                               │
    │          process_message(msg);                                            │
    │      }                                                                    │
    │                                                                           │
    │      rd_kafka_message_destroy(msg);                                       │
    │  }                                                                        │
    │                                                                           │
    └──────────────────────────────────────────────────────────────────────────┘
                                        │
                                        ▼
    ┌──────────────────────────────────────────────────────────────────────────┐
    │                         MESSAGE PROCESSING                               │
    │                                                                          │
    │  ┌────────────────────────────────────────────────────────────────────┐  │
    │  │  void process_message(rd_kafka_message_t *msg) {                   │  │
    │  │      // Access message fields:                                     │  │
    │  │      char *topic  = rd_kafka_topic_name(msg->rkt);                 │  │
    │  │      int partition = msg->partition;                               │  │
    │  │      int64_t offset = msg->offset;                                 │  │
    │  │      void *payload = msg->payload;                                 │  │
    │  │      size_t len    = msg->len;                                     │  │
    │  │      void *key     = msg->key;                                     │  │
    │  │      size_t key_len = msg->key_len;                                │  │
    │  │                                                                    │  │
    │  │      // Access headers (if any):                                   │  │
    │  │      rd_kafka_headers_t *hdrs;                                     │  │
    │  │      rd_kafka_message_headers(msg, &hdrs);                         │  │
    │  │  }                                                                 │  │
    │  └────────────────────────────────────────────────────────────────────┘  │
    │                                                                          │
    └──────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────┐
    │                     CONSUMER INTERNAL FLOW                               │
    │                                                                          │
    │                                                                          │
    │      Consumer               librdkafka              Kafka Broker        │
    │      ═════════              ══════════              ════════════        │
    │                                                                          │
    │  rd_kafka_subscribe()                                                    │
    │         │                                                                │
    │         ▼                                                                │
    │  ┌─────────────┐                              ┌─────────────┐           │
    │  │ Partition   │◀─────── Rebalance ─────────▶│  Group      │           │
    │  │ Assignment  │                              │ Coordinator │           │
    │  └─────────────┘                              └─────────────┘           │
    │                                                                          │
    │  rd_kafka_consumer_poll()                                                │
    │         │                                                                │
    │         ▼                                                                │
    │  ┌─────────────┐         Fetch Request       ┌─────────────┐           │
    │  │   Fetch     │ ──────────────────────────▶│   Broker    │           │
    │  │   Thread    │                             │  (Leader)   │           │
    │  │             │◀──────────────────────────  │             │           │
    │  └─────────────┘       Fetch Response        └─────────────┘           │
    │         │              (batch of msgs)                                   │
    │         ▼                                                                │
    │  ┌─────────────┐                                                         │
    │  │  Message    │                                                         │
    │  │   Queue     │                                                         │
    │  └──────┬──────┘                                                         │
    │         │                                                                │
    │         ▼                                                                │
    │  return message                                                          │
    │                                                                          │
    └─────────────────────────────────────────────────────────────────────────┘
```

### 中文说明

#### Consumer 初始化
1. 创建配置对象，设置 `group.id`（消费者组 ID）
2. 创建 Consumer 实例（类型为 `RD_KAFKA_CONSUMER`）
3. 添加 Broker 地址

#### 订阅 Topic
- 使用 `rd_kafka_subscribe()` 订阅一个或多个 Topic
- 设置 partition 为 -1 表示订阅所有分区

#### 消息拉取循环
1. **Poll**：`rd_kafka_consumer_poll()` 从 Broker 拉取消息
2. **超时处理**：返回 NULL 表示超时
3. **错误处理**：检查 `msg->err`
4. **消息处理**：访问 payload、key、headers 等
5. **清理**：`rd_kafka_message_destroy()` 释放消息

#### 内部流程
- **Rebalance**：当 Consumer 加入/离开 Group 时重新分配分区
- **Fetch**：后台线程批量拉取消息
- **消息队列**：拉取的消息先放入内部队列

---

## 7. librdkafka 实现详解

```
+==============================================================================+
|                    LIBRDKAFKA PRODUCER IMPLEMENTATION                        |
|                    (Based on cms/src/cms/kafka_producer.c)                   |
+==============================================================================+

    ┌──────────────────────────────────────────────────────────────────────────┐
    │                     INITIALIZATION FLOW                                   │
    │                                                                           │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  1. kafka_serverinfo_save(kafka_servers)                          │   │
    │  │     - Save broker addresses: "10.254.25.106:24240,..."            │   │
    │  └────────────────────────────────┬──────────────────────────────────┘   │
    │                                   │                                       │
    │                                   ▼                                       │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  2. kafka_producer_init()                                         │   │
    │  │                                                                   │   │
    │  │     // Create configuration                                       │   │
    │  │     conf = rd_kafka_conf_new();                                   │   │
    │  │                                                                   │   │
    │  │     // Set callbacks                                              │   │
    │  │     rd_kafka_conf_set_log_cb(conf, logger);                       │   │
    │  │     rd_kafka_conf_set_dr_msg_cb(conf, msg_delivered);             │   │
    │  │                                                                   │   │
    │  │     // TLS configuration (if enabled)                             │   │
    │  │     if (kf_ssl_cfg.enable_tls) {                                  │   │
    │  │         rd_kafka_conf_set(conf, "security.protocol", "SSL", ...); │   │
    │  │         rd_kafka_conf_set_ssl_cert(...);  // Public key           │   │
    │  │         rd_kafka_conf_set_ssl_cert(...);  // Private key          │   │
    │  │     }                                                             │   │
    │  └────────────────────────────────┬──────────────────────────────────┘   │
    │                                   │                                       │
    │                                   ▼                                       │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  3. Create Producer Instance                                      │   │
    │  │                                                                   │   │
    │  │     kafka_producer_rk = rd_kafka_new(                             │   │
    │  │         RD_KAFKA_PRODUCER,    // Type                             │   │
    │  │         conf,                 // Configuration                    │   │
    │  │         errstr,               // Error buffer                     │   │
    │  │         sizeof(errstr)                                            │   │
    │  │     );                                                            │   │
    │  └────────────────────────────────┬──────────────────────────────────┘   │
    │                                   │                                       │
    │                                   ▼                                       │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  4. Add Brokers                                                   │   │
    │  │                                                                   │   │
    │  │     rd_kafka_brokers_add(kafka_producer_rk, kafka_brokers);       │   │
    │  │     // e.g., "10.254.25.106:24240,10.254.25.104:28896"           │   │
    │  └───────────────────────────────────────────────────────────────────┘   │
    │                                                                           │
    └──────────────────────────────────────────────────────────────────────────┘


    ┌──────────────────────────────────────────────────────────────────────────┐
    │                        MESSAGE SEND FLOW                                  │
    │                                                                           │
    │  kafka_producer_send(header, payload, len)                               │
    │  ═══════════════════════════════════════════                             │
    │                                                                           │
    │  Step 1: Validation                                                       │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  if (header == NULL || payload == NULL)                           │   │
    │  │      return -1;                                                   │   │
    │  │  if (!kafka_producer_status())    // Check rk != NULL             │   │
    │  │      return -1;                                                   │   │
    │  │  if (topic == NULL || strlen(topic) == 0)                         │   │
    │  │      return -1;                                                   │   │
    │  └────────────────────────────────┬──────────────────────────────────┘   │
    │                                   │                                       │
    │                                   ▼                                       │
    │  Step 2: Prepare Headers                                                  │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  hdrs = kafka_producer_header_new(header);                        │   │
    │  │                                                                   │   │
    │  │  // Creates header with 8 key-value pairs:                        │   │
    │  │  // msg_type, src, dest, req_id, msg_sub_type, op, first, last   │   │
    │  │                                                                   │   │
    │  │  rd_kafka_headers_t *hdrs = rd_kafka_headers_new(8);              │   │
    │  │  rd_kafka_header_add(hdrs, "msg_type", -1, &val, sizeof(val));    │   │
    │  │  rd_kafka_header_add(hdrs, "src", -1, vmc_name, -1);              │   │
    │  │  // ... more headers                                              │   │
    │  └────────────────────────────────┬──────────────────────────────────┘   │
    │                                   │                                       │
    │                                   ▼                                       │
    │  Step 3: Produce Message                                                  │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  err = rd_kafka_producev(                                         │   │
    │  │      kafka_producer_rk,                                           │   │
    │  │      RD_KAFKA_V_PARTITION(partition),  // Target partition        │   │
    │  │      RD_KAFKA_V_HEADERS(hdrs),         // Message headers         │   │
    │  │      RD_KAFKA_V_KEY(key, strlen(key)), // Routing key             │   │
    │  │      RD_KAFKA_V_VALUE(payload, len),   // Message body            │   │
    │  │      RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY), // Copy data       │   │
    │  │      RD_KAFKA_V_TOPIC(topic),          // Target topic            │   │
    │  │      RD_KAFKA_V_END                    // Terminator              │   │
    │  │  );                                                               │   │
    │  └────────────────────────────────┬──────────────────────────────────┘   │
    │                                   │                                       │
    │                                   ▼                                       │
    │  Step 4: Poll for Events                                                  │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  rd_kafka_poll(kafka_producer_rk, 0);                             │   │
    │  │  // Non-blocking poll to trigger callbacks                        │   │
    │  └────────────────────────────────┬──────────────────────────────────┘   │
    │                                   │                                       │
    │                                   ▼                                       │
    │  Step 5: Error Handling                                                   │
    │  ┌───────────────────────────────────────────────────────────────────┐   │
    │  │  if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {                      │   │
    │  │      // Internal queue full, wait and retry                       │   │
    │  │      rd_kafka_poll(rk, KAFKA_POLL_TIMEOUT_MS);  // 500ms          │   │
    │  │  }                                                                │   │
    │  │  else if (err == RD_KAFKA_RESP_ERR__FATAL) {                      │   │
    │  │      // Fatal error, restart client                               │   │
    │  │      rd_kafka_dump(file, rk);      // Debug dump                  │   │
    │  │      kafka_producer_restart();     // Destroy and reinit          │   │
    │  │  }                                                                │   │
    │  └───────────────────────────────────────────────────────────────────┘   │
    │                                                                           │
    └──────────────────────────────────────────────────────────────────────────┘


    ┌──────────────────────────────────────────────────────────────────────────┐
    │                     DELIVERY REPORT CALLBACK                              │
    │                                                                           │
    │  static void msg_delivered(rd_kafka_t *rk,                               │
    │                            const rd_kafka_message_t *rkmessage,          │
    │                            void *opaque)                                  │
    │  {                                                                        │
    │      if (rkmessage->err) {                                               │
    │          // Message delivery failed                                       │
    │          // Log error with throttling (every 100th error)                │
    │          if ((errcnt % 100) == 1)                                        │
    │              DPL_LOG2(..., rd_kafka_err2str(rkmessage->err), errcnt);    │
    │          return;                                                          │
    │      }                                                                    │
    │                                                                           │
    │      // Message delivered successfully                                    │
    │      // Log: size, offset, partition, payload                            │
    │  }                                                                        │
    │                                                                           │
    │  Timeline:                                                                │
    │  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌────────────┐             │
    │  │ produce │───▶│ queue   │───▶│ send    │───▶│ ACK from   │             │
    │  │         │    │         │    │ to      │    │ broker     │             │
    │  │         │    │         │    │ broker  │    │            │             │
    │  └─────────┘    └─────────┘    └─────────┘    └──────┬─────┘             │
    │                                                      │                    │
    │                                                      ▼                    │
    │                                              ┌──────────────┐             │
    │                                              │msg_delivered │             │
    │                                              │  callback    │             │
    │                                              └──────────────┘             │
    │                                                                           │
    └──────────────────────────────────────────────────────────────────────────┘
```

### 中文说明

#### 初始化流程
1. **保存 Broker 地址**：`kafka_serverinfo_save()` 保存 Broker 列表
2. **创建配置**：`rd_kafka_conf_new()` 创建配置对象
3. **设置回调**：
   - `rd_kafka_conf_set_log_cb()`：日志回调
   - `rd_kafka_conf_set_dr_msg_cb()`：投递报告回调
4. **TLS 配置**（可选）：设置 SSL 证书和私钥
5. **创建实例**：`rd_kafka_new()` 创建 Producer 实例
6. **添加 Broker**：`rd_kafka_brokers_add()` 添加集群地址

#### 消息发送流程
1. **参数校验**：检查 header、payload、topic 有效性
2. **准备 Headers**：创建包含 8 个键值对的元数据
3. **发送消息**：`rd_kafka_producev()` 异步发送
4. **Poll 事件**：`rd_kafka_poll()` 触发回调处理
5. **错误处理**：
   - QUEUE_FULL：等待后重试
   - FATAL：重启客户端

#### Delivery Report 回调
- 消息投递成功或失败时触发
- 用于确认消息是否已被 Broker 接收
- 失败时记录错误日志（节流：每 100 条错误记录 1 次）

---

## 8. 关键 API 说明

```
+==============================================================================+
|                         KEY LIBRDKAFKA APIs                                  |
+==============================================================================+

    ┌─────────────────────────────────────────────────────────────────────────┐
    │  CONFIGURATION APIs                                                      │
    ├─────────────────────────────────────────────────────────────────────────┤
    │                                                                          │
    │  rd_kafka_conf_new()                                                     │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Create new configuration object                               │
    │  Returns:  rd_kafka_conf_t*                                              │
    │  Usage:    conf = rd_kafka_conf_new();                                   │
    │                                                                          │
    │  rd_kafka_conf_set(conf, name, value, errstr, errstr_size)              │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Set configuration property                                    │
    │  Example:  rd_kafka_conf_set(conf, "security.protocol", "SSL", ...);    │
    │  Common:   "bootstrap.servers", "group.id", "acks", "debug"             │
    │                                                                          │
    │  rd_kafka_conf_set_dr_msg_cb(conf, callback)                            │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Set delivery report callback                                  │
    │  Callback: void (*)(rd_kafka_t*, const rd_kafka_message_t*, void*)      │
    │  When:     Called when message delivery succeeds or fails                │
    │                                                                          │
    │  rd_kafka_conf_set_log_cb(conf, callback)                               │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Set logging callback                                          │
    │  Callback: void (*)(const rd_kafka_t*, int level, const char* fac,      │
    │                     const char* buf)                                     │
    │                                                                          │
    └─────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────┐
    │  PRODUCER APIs                                                           │
    ├─────────────────────────────────────────────────────────────────────────┤
    │                                                                          │
    │  rd_kafka_new(type, conf, errstr, errstr_size)                          │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Create Kafka handle (Producer or Consumer)                    │
    │  Type:     RD_KAFKA_PRODUCER or RD_KAFKA_CONSUMER                        │
    │  Returns:  rd_kafka_t* (handle), or NULL on error                        │
    │  Note:     Configuration ownership transferred to handle                 │
    │                                                                          │
    │  rd_kafka_brokers_add(rk, brokerlist)                                   │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Add broker addresses                                          │
    │  Format:   "host1:port1,host2:port2,..."                                │
    │  Returns:  Number of brokers added                                       │
    │                                                                          │
    │  rd_kafka_producev(rk, ...)                                             │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Produce message (variadic API)                                │
    │  Returns:  rd_kafka_resp_err_t (error code)                              │
    │  Params:   RD_KAFKA_V_TOPIC(topic)      - Target topic                   │
    │            RD_KAFKA_V_PARTITION(p)      - Target partition (-1 = any)    │
    │            RD_KAFKA_V_KEY(k, len)       - Message key                    │
    │            RD_KAFKA_V_VALUE(v, len)     - Message value (payload)        │
    │            RD_KAFKA_V_HEADERS(hdrs)     - Message headers                │
    │            RD_KAFKA_V_MSGFLAGS(flags)   - RD_KAFKA_MSG_F_COPY etc.       │
    │            RD_KAFKA_V_END               - Terminator (required)          │
    │                                                                          │
    │  rd_kafka_poll(rk, timeout_ms)                                          │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Poll for events and invoke callbacks                          │
    │  Timeout:  0 = non-blocking, -1 = infinite, >0 = wait ms                │
    │  Returns:  Number of events handled                                      │
    │  When:     Triggers delivery reports, error callbacks                    │
    │                                                                          │
    │  rd_kafka_flush(rk, timeout_ms)                                         │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Wait for all messages to be delivered                         │
    │  Usage:    Before destroying producer, ensure all messages sent          │
    │                                                                          │
    │  rd_kafka_destroy(rk)                                                   │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Destroy Kafka handle and release resources                    │
    │                                                                          │
    └─────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────┐
    │  CONSUMER APIs                                                           │
    ├─────────────────────────────────────────────────────────────────────────┤
    │                                                                          │
    │  rd_kafka_subscribe(rk, topics)                                         │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Subscribe to topics                                           │
    │  Params:   topics = rd_kafka_topic_partition_list_t*                     │
    │  Note:     Triggers rebalance in consumer group                          │
    │                                                                          │
    │  rd_kafka_consumer_poll(rk, timeout_ms)                                 │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Poll for messages                                             │
    │  Returns:  rd_kafka_message_t* (message), or NULL on timeout             │
    │  Note:     Caller must destroy message with rd_kafka_message_destroy()  │
    │                                                                          │
    │  rd_kafka_commit(rk, offsets, async)                                    │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Commit offsets                                                │
    │  Params:   offsets = NULL to commit current, async = 0/1                 │
    │                                                                          │
    │  rd_kafka_consumer_close(rk)                                            │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Close consumer (leave group, commit offsets)                  │
    │                                                                          │
    └─────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────┐
    │  HEADER APIs                                                             │
    ├─────────────────────────────────────────────────────────────────────────┤
    │                                                                          │
    │  rd_kafka_headers_new(initial_count)                                    │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Create new headers list                                       │
    │  Returns:  rd_kafka_headers_t*                                           │
    │                                                                          │
    │  rd_kafka_header_add(hdrs, name, name_size, value, value_size)          │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Add header to list                                            │
    │  Note:     name_size = -1 for strlen, value_size = -1 for strlen         │
    │  Example:  rd_kafka_header_add(hdrs, "msg_type", -1, &type, 4);          │
    │                                                                          │
    │  rd_kafka_message_headers(msg, &hdrs)                                   │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Get headers from message                                      │
    │  Returns:  RD_KAFKA_RESP_ERR_NO_ERROR on success                         │
    │                                                                          │
    │  rd_kafka_header_get(hdrs, idx, &name, &value, &size)                   │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Get header by index                                           │
    │                                                                          │
    │  rd_kafka_header_get_last(hdrs, name, &value, &size)                    │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Get last header with given name                               │
    │                                                                          │
    └─────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────┐
    │  ERROR HANDLING APIs                                                     │
    ├─────────────────────────────────────────────────────────────────────────┤
    │                                                                          │
    │  rd_kafka_err2str(err)                                                  │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Convert error code to string                                  │
    │  Returns:  const char*                                                   │
    │                                                                          │
    │  rd_kafka_err2name(err)                                                 │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Convert error code to name                                    │
    │  Returns:  const char* (e.g., "ERR__QUEUE_FULL")                        │
    │                                                                          │
    │  rd_kafka_fatal_error(rk, errstr, errstr_size)                          │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Get fatal error information                                   │
    │  Returns:  Original error that caused fatal state                        │
    │                                                                          │
    │  rd_kafka_dump(fp, rk)                                                  │
    │  ─────────────────────────────────────────────────────────────────────  │
    │  Purpose:  Dump internal state to file for debugging                     │
    │  Usage:    FILE *fp = fopen("/tmp/kafka_dump", "w");                    │
    │            rd_kafka_dump(fp, rk);                                        │
    │                                                                          │
    └─────────────────────────────────────────────────────────────────────────┘
```

### 中文说明

#### 配置 APIs
- **rd_kafka_conf_new()**：创建配置对象
- **rd_kafka_conf_set()**：设置配置项（如 security.protocol、debug）
- **rd_kafka_conf_set_dr_msg_cb()**：设置投递报告回调
- **rd_kafka_conf_set_log_cb()**：设置日志回调

#### Producer APIs
- **rd_kafka_new()**：创建 Producer/Consumer 实例
- **rd_kafka_brokers_add()**：添加 Broker 地址
- **rd_kafka_producev()**：发送消息（可变参数 API）
- **rd_kafka_poll()**：轮询事件，触发回调
- **rd_kafka_flush()**：等待所有消息发送完成
- **rd_kafka_destroy()**：销毁实例

#### Consumer APIs
- **rd_kafka_subscribe()**：订阅 Topic
- **rd_kafka_consumer_poll()**：拉取消息
- **rd_kafka_commit()**：提交 Offset
- **rd_kafka_consumer_close()**：关闭 Consumer

#### Header APIs
- **rd_kafka_headers_new()**：创建 Headers 列表
- **rd_kafka_header_add()**：添加 Header
- **rd_kafka_message_headers()**：从消息获取 Headers
- **rd_kafka_header_get_last()**：按名称获取 Header

#### 错误处理 APIs
- **rd_kafka_err2str()**：错误码转字符串
- **rd_kafka_fatal_error()**：获取致命错误信息
- **rd_kafka_dump()**：转储内部状态用于调试

---

## 9. 消息流转总结

```
+==============================================================================+
|                      MESSAGE FLOW SUMMARY                                    |
+==============================================================================+

    ┌─────────────────────────────────────────────────────────────────────────┐
    │                     COMPLETE MESSAGE LIFECYCLE                           │
    └─────────────────────────────────────────────────────────────────────────┘

    
    STEP 1: PRODUCER INITIALIZATION
    ═══════════════════════════════════════════════════════════════════════════
    
    ┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
    │  Service        │     │  kafka_producer  │     │  librdkafka      │
    │  Discovery      │────▶│  _init()         │────▶│  rd_kafka_new()  │
    │  (Nomad)        │     │                  │     │                  │
    └──────────────────┘     └──────────────────┘     └──────────────────┘
           │                         │                         │
           │  KAFKA_BROKER_          │  rd_kafka_conf_         │  Connect to
           │  ADDRESS_PORT           │  set_dr_msg_cb()        │  Broker cluster
           ▼                         ▼                         ▼
    ┌──────────────────────────────────────────────────────────────────────┐
    │  kafka_brokers = "10.254.25.106:24240,10.254.25.104:28896"          │
    └──────────────────────────────────────────────────────────────────────┘


    STEP 2: MESSAGE PRODUCTION
    ═══════════════════════════════════════════════════════════════════════════
    
    Application                    librdkafka                    Broker
    ═══════════                    ══════════                    ══════
    
    kafka_producer_send()
         │
         ├──▶ kafka_producer_header_new()
         │         │
         │         ├──▶ msg_type, src, dest
         │         ├──▶ req_id, msg_sub_type
         │         └──▶ op, first, last
         │
         └──▶ rd_kafka_producev()
                   │
                   ▼
              ┌─────────────┐
              │  Internal   │      Background Thread
              │   Queue     │ ─────────────────────────▶  Network I/O
              └─────────────┘                                  │
                                                               │
                                                               ▼
                                                        ┌────────────┐
                                                        │   BROKER   │
                                                        │            │
                                                        │  Topic:    │
                                                        │  snmp-data │
                                                        │            │
                                                        │  Partition │
                                                        │    [0]     │
                                                        └────────────┘


    STEP 3: MESSAGE STORAGE IN BROKER
    ═══════════════════════════════════════════════════════════════════════════
    
                              ┌───────────────────────────────────────────┐
                              │              BROKER                        │
                              │                                            │
                              │  ┌────────────────────────────────────┐   │
                              │  │         LOG SEGMENT                 │   │
                              │  │                                     │   │
    Message ─────────────────▶│  │  ┌───┬───┬───┬───┬───┬───┬─────┐  │   │
    (append-only)             │  │  │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ ... │  │   │
                              │  │  └───┴───┴───┴───┴───┴───┴─────┘  │   │
                              │  │            ▲                       │   │
                              │  │            │ newest                │   │
                              │  │                                     │   │
                              │  │  Index File:                        │   │
                              │  │  offset -> position                │   │
                              │  │                                     │   │
                              │  └────────────────────────────────────┘   │
                              │                                            │
                              │                    │                       │
                              │                    │ Replicate             │
                              │                    ▼                       │
                              │  ┌────────────────────────────────────┐   │
                              │  │       FOLLOWER REPLICA              │   │
                              │  │  ┌───┬───┬───┬───┬───┬───┬─────┐  │   │
                              │  │  │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ ... │  │   │
                              │  │  └───┴───┴───┴───┴───┴───┴─────┘  │   │
                              │  └────────────────────────────────────┘   │
                              │                                            │
                              └───────────────────────────────────────────┘


    STEP 4: DELIVERY CONFIRMATION
    ═══════════════════════════════════════════════════════════════════════════
    
              Broker                     librdkafka                Application
              ══════                     ══════════                ═══════════
              
         ACK (acks=1/all)
                │
                ▼
         ┌────────────┐
         │  Response  │
         │   Queue    │
         └──────┬─────┘
                │
                ▼
         rd_kafka_poll()
                │
                ├──▶ msg_delivered() callback
                │         │
                │         ├──▶ if (err) log_error()
                │         │
                │         └──▶ if (ok) log_success()
                │
                └──▶ Return to application


    STEP 5: CONSUMER CONSUMPTION
    ═══════════════════════════════════════════════════════════════════════════
    
    Consumer Application             librdkafka                    Broker
    ════════════════════             ══════════                    ══════
    
    rd_kafka_subscribe()
         │
         ├──▶ Join Consumer Group
         │
         └──▶ Rebalance (assign partitions)
         
    rd_kafka_consumer_poll()
         │
         ▼
    ┌─────────────┐      Fetch Request      ┌─────────────┐
    │  Consumer   │ ───────────────────────▶│   Broker    │
    │  Buffer     │                          │             │
    │             │◀─────────────────────── │  Topic:     │
    └─────────────┘      Fetch Response      │  snmp-data  │
         │              (batch of msgs)      │             │
         │                                   │  Partition  │
         ▼                                   │  [0]        │
    process_message()                        └─────────────┘
         │
         ▼
    rd_kafka_commit()
         │
         └──▶ Update offset in __consumer_offsets topic


    ┌─────────────────────────────────────────────────────────────────────────┐
    │                         KEY TAKEAWAYS                                    │
    └─────────────────────────────────────────────────────────────────────────┘
    
    ┌────────────────────────────────────────────────────────────────────────┐
    │                                                                         │
    │  1. ASYNC MODEL                                                         │
    │     - rd_kafka_producev() 是异步的，消息先进入内部队列                   │
    │     - rd_kafka_poll() 触发后台 I/O 和回调                               │
    │     - Delivery Report 通知发送结果                                      │
    │                                                                         │
    │  2. BATCHING                                                            │
    │     - Producer 端批量发送提高吞吐量                                      │
    │     - Consumer 端批量拉取减少网络开销                                    │
    │                                                                         │
    │  3. PERSISTENCE                                                         │
    │     - 消息写入磁盘（append-only log）                                   │
    │     - 副本机制保证数据不丢失                                            │
    │                                                                         │
    │  4. PARTITIONING                                                        │
    │     - 同一 Key 的消息路由到同一 Partition                               │
    │     - 分区内消息有序，跨分区无序                                        │
    │                                                                         │
    │  5. CONSUMER GROUPS                                                     │
    │     - 同 Group 内 Consumer 共享消费                                     │
    │     - 不同 Group 独立消费（都收到全量消息）                             │
    │                                                                         │
    │  6. OFFSET MANAGEMENT                                                   │
    │     - Consumer 通过 Offset 追踪消费进度                                 │
    │     - 支持 auto-commit 或 manual commit                                 │
    │                                                                         │
    │  7. ERROR HANDLING                                                      │
    │     - QUEUE_FULL: 内部队列满，等待后重试                                │
    │     - FATAL: 致命错误，需要重启客户端                                   │
    │                                                                         │
    └────────────────────────────────────────────────────────────────────────┘
```

### 中文说明

#### 消息完整生命周期

1. **Producer 初始化**
   - 通过服务发现（Nomad）获取 Broker 地址
   - 创建 Kafka 配置，设置回调
   - 创建 Producer 实例并连接 Broker

2. **消息生产**
   - 应用调用 `kafka_producer_send()`
   - 准备消息 Headers（元数据）
   - `rd_kafka_producev()` 将消息放入内部队列
   - 后台线程异步发送

3. **Broker 存储**
   - 消息追加写入 Log Segment
   - 通过 Index 文件快速定位
   - 副本同步到 Follower

4. **投递确认**
   - Broker 返回 ACK
   - `rd_kafka_poll()` 处理响应
   - 触发 `msg_delivered()` 回调

5. **Consumer 消费**
   - 订阅 Topic，加入 Consumer Group
   - Rebalance 分配分区
   - 拉取消息并处理
   - 提交 Offset

#### 关键要点

| 特性 | 说明 |
|------|------|
| 异步模型 | 消息先进队列，后台发送，回调通知结果 |
| 批量处理 | 批量发送/拉取，提高吞吐量 |
| 持久化 | Append-only Log + 副本，数据不丢失 |
| 分区 | 相同 Key 路由到同一分区，分区内有序 |
| 消费者组 | 同组共享消费，不同组独立消费 |
| Offset | 追踪消费进度，支持自动/手动提交 |
| 错误处理 | QUEUE_FULL 重试，FATAL 重启 |

---

## 附录：代码示例映射

```
+==============================================================================+
|                    CODE EXAMPLE MAPPING                                      |
|                    (cms/src/cms/kafka_producer.c)                            |
+==============================================================================+

    Function                          Purpose                    Line
    ────────────────────────────────────────────────────────────────────────
    kafka_producer_init()             Initialize producer        277-361
    kafka_producer_send()             Send SNMP data             434-514
    kafka_producer_header_new()       Create message headers     184-214
    msg_delivered()                   Delivery report callback   236-263
    kafka_producer_restart()          Restart on fatal error     411-415
    kafka_producer_device_status_send() Send device status       517-546
    kafka_producer_ip_assignment_send() Send IP notifications    548-578
    ────────────────────────────────────────────────────────────────────────

    Service Discovery:
    kafka_server_service_discovery.c  Monitor broker changes     1-244
    ────────────────────────────────────────────────────────────────────────
```

---

*文档创建时间：2024年*
*基于 librdkafka 和 cms/src/cms/kafka_producer.c 实现*

