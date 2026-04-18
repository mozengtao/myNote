# Windows WSL2 + Docker + Kafka 学习指南

# 一、 环境准备 (WSL2 & Docker Desktop)

## 1. 配置 WSL2 引擎

1. **安装/运行 Docker Desktop**：确保右下角托盘出现鲸鱼图标，说明 Docker 已正常启动。

2. **开启 WSL2 后端**：
        

    - 进入 Docker Desktop 的 `Settings -> General`（设置 -> 通用），勾选 `Use the Docker Desktop-based engine`（使用基于 Docker Desktop 的引擎）。

3. **启用发行版集成**：
        

    - 进入 `Settings -> Resources -> WSL Integration`（设置 -> 资源 -> WSL 集成）。

    - 开启 `Enable integration with my default WSL distro`（启用与默认 WSL 发行版的集成）。

    - 在下方发行版列表中，将你的 WSL 发行版（如 `Ubuntu`）的开关拨至 **On** 状态。

4. **验证配置**：打开 WSL2 终端（如 Ubuntu 终端），输入命令 `docker compose version`，若能正常输出版本号，无报错信息，说明配置成功。

# 二、 搭建 Kafka 实验环境 (KRaft 模式)

采用现代的 KRaft 模式搭建 Kafka，无需额外安装 Zookeeper，配置更简洁、部署更高效。

## 1. 创建配置文件

在 WSL2 终端中依次执行以下命令，创建工作目录并编辑 Docker Compose 配置文件：

```bash
mkdir kafka-lab && cd kafka-lab
vim docker-compose.yml
```

## 2. 写入配置内容

在打开的 `docker-compose.yml` 文件中，写入以下 YAML 配置（直接复制粘贴即可）：

```yaml
services:
  kafka:
    image: bitnami/kafka:3.7
    container_name: kafka-lab
    ports:
      - "9092:9092"
    environment:
      - KAFKA_CFG_NODE_ID=1
      - KAFKA_CFG_PROCESS_ROLES=controller,broker
      - KAFKA_CFG_LISTENERS=PLAINTEXT://:9092,CONTROLLER://:9093
      - KAFKA_CFG_LISTENER_SECURITY_PROTOCOL_MAP=CONTROLLER:PLAINTEXT,PLAINTEXT:PLAINTEXT
      - KAFKA_CFG_CONTROLLER_QUORUM_VOTERS=1@127.0.0.1:9093
      - KAFKA_CFG_CONTROLLER_LISTENER_NAMES=CONTROLLER
      - KAFKA_CFG_ADVERTISED_LISTENERS=PLAINTEXT://localhost:9092
      - ALLOW_PLAINTEXT_LISTENER=yes
```

## 3. 启动容器

在 WSL2 终端（当前处于 kafka-lab 目录下）执行以下命令，启动 Kafka 容器：

```bash
# 若遇到网络超时，请在 Docker Desktop 配置镜像加速器或代理
docker compose up -d
```

Docker Desktop 配置镜像加速器

```json
{
  "builder": {
    "gc": {
      "defaultKeepStorage": "20GB",
      "enabled": true
    }
  },
  "experimental": false,
  "registry-mirrors": [
    "https://docker.hlmirror.com",
    "https://docker.1panel.live",
    "https://dockerpull.com",
    "https://docker.anyhub.us.kg"
  ]
}
```

执行完成后，可通过 `docker ps` 命令查看容器是否正常运行。

# 三、 Kafka 核心概念与实操命令

通过执行 Kafka 容器内的脚本，可直观学习 Kafka 核心概念及运行机制，所有命令均在 WSL2 终端执行。

## 1. 主题 (Topic) 与 分区 (Partition)

**核心概念**：Topic 是 Kafka 中消息的分类单位，用于区分不同类型的消息；Partition 是 Topic 的物理并行单位，将一个 Topic 拆分到多个 Partition 中，可实现负载均衡，提升消息读写性能。

**实操操作**：创建一个包含 3 个分区、1 个副本的 Topic（名称为 learn-kafka），命令如下：

```bash
docker exec -it kafka-lab kafka-topics.sh --create --topic learn-kafka --partitions 3 --replication-factor 1 --bootstrap-server localhost:9092
```

## 2. 生产者 (Producer) 与 消息追加

**核心概念**：Producer 是消息的发送者，负责将消息发送到指定 Topic；消息以“追加写入”（Append-only）的方式写入 Partition，顺序写磁盘的特性让 Kafka 拥有极高的读写性能。

**实操操作**：开启生产者终端，发送测试消息，命令如下：

```bash
docker exec -it kafka-lab kafka-console-producer.sh --topic learn-kafka --bootstrap-server localhost:9092
```

执行命令后，进入消息输入模式，可输入以下测试消息（输入完成后按回车发送）：

```bash
>Hello Kafka
>Learning Partition and Offset
```

## 3. 消费者 (Consumer) 与 偏移量 (Offset)

**核心概念**：Consumer 是消息的接收者，负责从 Topic 中读取消息；Offset（偏移量）是消息在 Partition 内的唯一位置标识，消费者通过记录自身的 Offset，来追踪已读取的消息进度，下次可从指定 Offset 继续读取。

**实操操作**：开启消费者终端，从 Topic 开头读取所有消息（含之前发送的测试消息），命令如下：

```bash
docker exec -it kafka-lab kafka-console-consumer.sh --topic learn-kafka --from-beginning
```

# 四、 Kafka 进阶学习路线（架构视角）

## 1. 整体学习路线（脑图级框架）

```plaintext
Kafka Learning Path
│
├── Phase 1: 基础数据流
│   Producer → Broker → Topic → Partition → Consumer
│
├── Phase 2: 存储与消费机制
│   Log Segment / Offset / Consumer Group / Rebalance
│
├── Phase 3: 高级特性
│   Replication / ISR(In-Sync Replicas) / Leader Election / Acks
│
├── Phase 4: KRaft架构
│   Controller / Metadata Log / Quorum
│
└── Phase 5: 工程实践
    Debug / 性能 / 故障注入
```

## 2. Phase 1：最小闭环（数据流）

**目标**：搞清 Kafka 是怎么“流动”的

### （1）创建 Topic

```bash
docker exec -it kafka-lab kafka-topics.sh \
  --create \
  --topic test-topic \
  --bootstrap-server localhost:9092 \
  --partitions 3 \
  --replication-factor 1
```

**👉 学什么：**

- Topic = 逻辑队列

- Partition = 并行度单位

### （2）查看 Topic

```bash
kafka-topics.sh --list --bootstrap-server localhost:9092
kafka-topics.sh --describe \
  --topic test-topic \
  --bootstrap-server localhost:9092
```

**👉 重点观察：**

- Partition 数

- Leader

- ISR（虽然现在只有1）

### （3）Producer 写数据

```bash
kafka-console-producer.sh \
  --topic test-topic \
  --bootstrap-server localhost:9092
```

输入：

```bash
hello kafka
msg1
msg2
```

### （4）Consumer 读数据

```bash
kafka-console-consumer.sh \
  --topic test-topic \
  --from-beginning \
  --bootstrap-server localhost:9092
```

### （5）Consumer Group（关键）

```bash
kafka-console-consumer.sh \
  --topic test-topic \
  --group g1 \
  --bootstrap-server localhost:9092
```

再开一个窗口，执行相同命令：

```bash
kafka-console-consumer.sh \
  --topic test-topic \
  --group g1 \
  --bootstrap-server localhost:9092
```

**👉 观察：**

- 消息被“分摊”

- 触发 Rebalance

### （6）查看消费组状态

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --describe \
  --group g1
```

**👉 核心理解：**

- offset 是谁维护的？

- lag 是什么？

## 3. Phase 2：存储层（你必须搞懂的底层）

### （1）查看日志目录（容器内）

```bash
docker exec -it kafka-lab ls /bitnami/kafka/data
docker exec -it kafka-lab ls /bitnami/kafka/data/__cluster_metadata-0
```

**👉 你会看到：**

```plaintext
00000000000000000000.log
00000000000000000000.index
```

### （2）关键概念拆解

- Partition = append-only log（追加日志）

log segment 结构：

```plaintext
log segment:
  ├── .log      (数据)
  ├── .index    (offset索引)
```

**👉 Kafka 本质就是：**高性能顺序写日志系统（像网络协议栈里的 ring buffer）

## 4. Phase 3：引入“复杂度”（当前环境需要升级）

**当前配置痛点**：单节点 + 无副本

**👉 问题：**

- 没有 replication（副本）

- 没有 leader failover（主节点故障转移）

- ISR 无实际意义

### （1）升级 docker-compose（关键）

**新架构**：3 broker + KRaft quorum（集群模式）

- Broker 1 (controller + broker)：主节点+控制器

- Broker 2 (broker)：从节点

- Broker 3 (broker)：从节点

**改造 docker-compose.yml（核心片段）**：替换原有的单节点配置，写入以下内容：

```yaml
version: '3'
services:

  kafka1:
    image: bitnami/kafka:3.7
    container_name: kafka1
    ports:
      - "9092:9092"
    environment:
      - KAFKA_CFG_NODE_ID=1
      - KAFKA_CFG_PROCESS_ROLES=controller,broker
      - KAFKA_CFG_LISTENERS=PLAINTEXT://:9092,CONTROLLER://:9093
      - KAFKA_CFG_CONTROLLER_QUORUM_VOTERS=1@kafka1:9093
      - KAFKA_CFG_ADVERTISED_LISTENERS=PLAINTEXT://localhost:9092
      - ALLOW_PLAINTEXT_LISTENER=yes

  kafka2:
    image: bitnami/kafka:3.7
    container_name: kafka2
    ports:
      - "9094:9092"
    environment:
      - KAFKA_CFG_NODE_ID=2
      - KAFKA_CFG_PROCESS_ROLES=broker
      - KAFKA_CFG_LISTENERS=PLAINTEXT://:9092
      - KAFKA_CFG_ADVERTISED_LISTENERS=PLAINTEXT://localhost:9094
      - ALLOW_PLAINTEXT_LISTENER=yes

  kafka3:
    image: bitnami/kafka:3.7
    container_name: kafka3
    ports:
      - "9095:9092"
    environment:
      - KAFKA_CFG_NODE_ID=3
      - KAFKA_CFG_PROCESS_ROLES=broker
      - KAFKA_CFG_LISTENERS=PLAINTEXT://:9092
      - KAFKA_CFG_ADVERTISED_LISTENERS=PLAINTEXT://localhost:9095
      - ALLOW_PLAINTEXT_LISTENER=yes
```

修改完成后，执行 `docker compose down` 停止原有单节点容器，再执行 `docker compose up -d` 启动集群。

### （2）Phase 3 核心实验（必须做）

1. **创建带副本的 Topic**`kafka-topics.sh \
  --create \
  --topic replica-topic \
  --partitions 3 \
  --replication-factor 3 \
  --bootstrap-server localhost:9092`

2. **查看 ISR / Leader**`kafka-topics.sh --describe \
  --topic replica-topic \
  --bootstrap-server localhost:9092`**👉 你会看到：**`Leader: 1
Replicas: 1,2,3
ISR: 1,2,3`

3. **故障注入（关键）**`docker stop kafka1`再执行查看命令：`kafka-topics.sh --describe --topic replica-topic --bootstrap-server localhost:9092`**👉 你会看到：**

    - Leader 切换

    - ISR 变化

## 5. Phase 4：深入理解（结合 Linux / 网络背景）

结合 Linux / 网络背景，以下几个点必须打透：

1. **Producer ACK 机制**`kafka-console-producer.sh \
  --topic replica-topic \
  --bootstrap-server localhost:9092 \
  --producer-property acks=all`**👉 对应：**

    - acks=0   → UDP 风格（无需确认，效率高，可能丢数）

    - acks=1   → leader确认（仅主节点确认，可能丢数）

    - acks=all → 类似 quorum commit（所有副本确认，不丢数）

2. **批处理 + 吞吐**`--producer-property batch.size=16384
--producer-property linger.ms=10`**👉 类比：**

    - Nagle算法（合并小数据包，减少网络开销）

    - TCP batching（TCP 批处理，提升传输效率）

3. **Consumer Offset 控制**`--from-beginning
--max-messages 10`**👉 理解：**

    - offset = 应用层 ACK（记录消费进度）

    - Kafka ≠ queue，而是 log replay（日志重放，可回溯消费）

## 6. Phase 5：Debug Playbook（工程级）

系统调试方法，可按以下步骤操作：

1. **查看 broker config（ broker 配置）**`kafka-configs.sh \
  --bootstrap-server localhost:9092 \
  --entity-type brokers \
  --describe`

2. **查看 metadata（元数据）**`kafka-metadata-shell.sh`

3. **查看 controller 状态（控制器状态）**`grep "Controller" logs/server.log`

4. **网络抓包（强烈推荐，结合网络背景）**`tcpdump -i any port 9092`**👉 你会看到：**Kafka protocol（类似自定义 RPC，自定义通信协议）

# 五、 Kafka 核心概念深化（Lag 与 ISR 详解）

## 一、Lag 是什么（消费视角）

### 1. 一句话定义

Lag = 生产进度 - 消费进度

### 2. 用结构图理解

Partition Log:

```plaintext
offset:   0   1   2   3   4   5   6   7
          ────────────────────────────
data:     A   B   C   D   E   F   G   H
                                 ↑
                          Log End Offset (LEO)

Consumer Group g1:
                     ↑
               committed offset = 4
```

### 3. 计算方式

Lag = LEO - Consumer Offset
    = 8 - 5
    = 3

**👉 表示：**

还有 3 条消息没消费

### 4. 你用命令看到的 Lag

```bash
kafka-consumer-groups.sh \
  --bootstrap-server localhost:9092 \
  --describe \
  --group g1
```

输出类似：

```plaintext
PARTITION  CURRENT-OFFSET  LOG-END-OFFSET  LAG
0          5               8               3
```

### 5. 本质理解（结合你背景）

你可以把 Lag 理解成：

Lag ≈ 网络中的“接收窗口 backlog”

或者更贴近一点：

|Kafka|类比 TCP|
|---|---|
|offset|seq|
|lag|未ACK数据|
|consumer|接收端|
**👉 Kafka 本质就是：**

应用层的“可靠日志流控机制”

### 6. Lag 过大的原因

你在实际系统中要重点关注：

- consumer 太慢

- partition 太少（并行度不足）

- rebalance 频繁

- 下游处理阻塞

## 二、ISR 是什么（副本视角）

### 1. 一句话定义

ISR = In-Sync Replicas（和 leader 保持同步的副本集合）

### 2. 架构图

Partition 0:

```plaintext
Leader (Broker 1)
             │
     ┌───────┼────────┐
     ↓       ↓        ↓
Follower2  Follower3  Follower4

ISR = {1,2,3}
```

### 3. ISR 判定条件（关键）

一个 follower 要进入 ISR，必须：

- 与 leader 的 LEO 差距 < 阈值

- 且在时间窗口内持续同步

Kafka 参数：

replica.lag.time.max.ms

### 4. 动态变化

正常情况：
ISR = [1,2,3]

follower 落后：
ISR = [1,2]

**👉 被踢出 ISR**

### 5. 为什么 ISR 很重要？

Producer acks=all 时：
必须 ISR 中所有副本写成功才返回

**👉 所以：**

- ISR 越小 → 可用性高（更容易成功）

- ISR 越大 → 一致性强

### 6. 故障演示（你可以做）

```bash
docker stop kafka2
```

再看：

`kafka-topics.sh --describe ...`

你会看到：

```plaintext
ISR: 1,3
```

### 7. 本质理解（非常重要）

ISR ≈

一个“强一致副本集合”

结合分布式系统理解：

|概念|类比|
|---|---|
|ISR|Raft quorum（弱化版）|
|Leader|Primary|
|Follower|Replica|
|ISR shrinking|副本失效|
## 三、Lag vs ISR（对比理解）

|维度|Lag|ISR|
|---|---|---|
|视角|Consumer|Broker 副本|
|本质|消费积压|副本同步状态|
|影响|延迟|一致性 / 可用性|
|变化原因|消费慢|follower 落后/宕机|
|命令查看|consumer-groups|topics --describe|
## 四、你必须掌握的核心认知

### 1. Kafka 有两条“进度线”

1. Producer → Log End Offset（写入进度）

2. Consumer → Committed Offset（消费进度）

**👉 Lag 就是它们的差**

### 2. Kafka 有两种“同步”

1. 消费同步（consumer lag）

2. 副本同步（ISR）

### 3. 一个关键 insight（非常重要）

Kafka 的复杂度，本质来自两个“异步系统”：

- 生产 → Broker → 消费   （数据流）

- Leader → Follower      （复制流）

Lag 和 ISR 就是这两个系统的“健康指标”。

## 五、给你的实战建议（别跳过）

你下一步可以这样练：

### 实验 1（Lag）

```bash
# 快速生产
seq 1 10000 | kafka-console-producer.sh --topic replica-topic --bootstrap-server localhost:9092

# 慢速消费
kafka-console-consumer.sh --topic replica-topic --bootstrap-server localhost:9092 --group g2
```

观察 Lag 变化

### 实验 2（ISR）

```bash
docker stop kafka2
```

观察：

- ISR变化

- 是否影响写入

### 实验 3（组合）

```bash
kafka-console-producer.sh --topic replica-topic --bootstrap-server localhost:9092 --producer-property acks=all
# 另一个窗口执行
docker kill kafka2
```

**👉 你会看到：**

- 写入延迟增加

- 或失败