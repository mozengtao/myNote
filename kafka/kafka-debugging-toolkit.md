# Kafka Debugging Toolkit（命令手册 + 系统化排障方法）

这份文档把 **概念 → 机制 → 可观测行为 → 命令** 串起来，目标是让你在生产环境里能快速收敛问题。

前置：如果你对 topic/partition/offset/ISR/rebalance 还不熟，先看 `kafka-systematic-learning-walkthrough.md`。

---

## 6. Debugging Toolkit（非常实用：每个命令一页心智模型）

> 说明：不同 Kafka 版本脚本路径/输出字段可能略有差异；但核心字段与用法一致。  
> 有鉴权/SSL 的环境通常要加 `--command-config client.properties`（这里用最小示例说明机制）。

建议先固定两个变量：

```bash
export BS="127.0.0.1:9092"
export TOPIC="your-topic"
export GROUP="your-group"
```

### Command: kafka-console-producer.sh

- **Purpose（作用）**：向 topic 写入测试消息；验证“写入链路是否通”。
- **When（何时用）**
  - 怀疑 producer 没写进去：先用 console 直接写，隔离业务代码问题
  - 验证 ACL/认证/网络连通
- **Example（示例）**

```bash
kafka-console-producer.sh \
  --bootstrap-server "$BS" \
  --topic "$TOPIC"
```

在交互里输入几行文本回车即可。

- **Observe（观察什么）**
  - 是否立刻报错（认证失败、topic 不存在、网络超时、NotLeader）
  - 与下游 console consumer / group lag 的联动（写了以后 LEO 是否增长）

---

### Command: kafka-console-consumer.sh

- **Purpose**：从 topic 读取消息；验证“topic 里到底有没有数据、数据长什么样”。
- **When**
  - “下游没数据”：先直接读 topic，判断是生产没写还是消费没读
  - 验证 key/value/header（注意二进制 value 需要 formatter 才能友好展示）
- **Example**

从最新开始读：

```bash
kafka-console-consumer.sh \
  --bootstrap-server "$BS" \
  --topic "$TOPIC" \
  --from-beginning false
```

从最早开始读（小心会读很多）：

```bash
kafka-console-consumer.sh \
  --bootstrap-server "$BS" \
  --topic "$TOPIC" \
  --from-beginning
```

打印 key：

```bash
kafka-console-consumer.sh \
  --bootstrap-server "$BS" \
  --topic "$TOPIC" \
  --from-beginning \
  --property print.key=true \
  --property key.separator=" | "
```

- **Observe**
  - 是否能读到数据（证明 broker 侧存储与网络路径 OK）
  - 数据是否符合预期（是否被业务过滤/解析失败）
  - 如果读不到：要么 topic 没数据，要么你读的分区/起点不对（console 默认会读所有分区）

---

### Command: kafka-consumer-groups.sh

- **Purpose**：查看/管理 consumer group：成员、分配、offset、lag。
- **When**
  - consumer lag 增加
  - 消费者“不工作/不消费”
  - 怀疑 rebalance storm
- **Example**

列出 group：

```bash
kafka-consumer-groups.sh --bootstrap-server "$BS" --list
```

查看某个 group 的消费进度与 lag：

```bash
kafka-consumer-groups.sh \
  --bootstrap-server "$BS" \
  --group "$GROUP" \
  --describe
```

- **Observe**
  - `CURRENT-OFFSET / LOG-END-OFFSET / LAG`（含义见 7.1）
  - 某些分区 lag 特别大：可能是分区热点或单消费者瓶颈
  - 分配是否均衡：同一成员拿太多分区会拖慢

---

### Command: kafka-topics.sh

- **Purpose**：查看/创建/修改 topic；排障时主要用来“看结构与副本健康”。
- **When**
  - topic 不存在/拼写不确定
  - 想看 partition 数、leader、replicas、ISR（健康度）
  - 怀疑 ISR 变小影响 acks=all
- **Example**

列出 topic：

```bash
kafka-topics.sh --bootstrap-server "$BS" --list
```

描述 topic（最重要的排障命令之一）：

```bash
kafka-topics.sh --bootstrap-server "$BS" --topic "$TOPIC" --describe
```

- **Observe**
  - leader 是否存在/是否频繁变化
  - ISR 是否缩小（副本落后）
  - replicas 分布是否过于集中（可能导致单 broker 热点）

---

### Command: kafka-run-class.sh

- **Purpose**：运行 Kafka 自带的“内部工具类”（Java main class），用于更底层的检查/解剖。
- **When**
  - 需要查某个 topic/partition 的最新 offset（比 console 更直接）
  - 需要 dump segment 文件、解析日志格式、定位损坏段
  - 需要用一些不提供独立脚本的工具（本质都靠它启动）
- **Example**

查询某 topic 各分区的最新 offset（LEO）：

```bash
kafka-run-class.sh kafka.tools.GetOffsetShell \
  --bootstrap-server "$BS" \
  --topic "$TOPIC" \
  --time -1
```

（更底层）dump 本地日志段（需要在 broker 机器上，并指向实际 log.dirs 文件）：

```bash
kafka-run-class.sh kafka.tools.DumpLogSegments \
  --files /var/lib/kafka/data/"$TOPIC"-0/00000000000000000000.log \
  --print-data-log
```

- **Observe**
  - `GetOffsetShell` 输出能直接告诉你“写入有没有在增长”
  - `DumpLogSegments` 能让你确认“磁盘上的 segment 里到底是什么”（用于极端/底层排障）

---

### Command: kafka-configs.sh

- **Purpose**：查看/修改动态配置（broker/topic/client）；排障时主要用于“确认关键阈值是否导致行为变化”。
- **When**
  - acks=all 写入失败：检查 `min.insync.replicas`
  - 怀疑 topic 保留策略导致数据被清理：检查 retention 配置
- **Example**

查看 topic 配置：

```bash
kafka-configs.sh \
  --bootstrap-server "$BS" \
  --entity-type topics \
  --entity-name "$TOPIC" \
  --describe
```

- **Observe**
  - `min.insync.replicas`
  - `cleanup.policy`（delete/compact）
  - `retention.ms` / `retention.bytes`
  - `segment.bytes` / `segment.ms`

---

### Command: kafka-log-dirs.sh

- **Purpose**：从 broker 角度查看各日志目录上的分区副本大小与状态；常用于磁盘相关与副本异常排障。
- **When**
  - broker 磁盘满/热点目录
  - partition 副本异常、疑似卡复制
  - 想知道数据在各 broker 的落盘分布
- **Example**

```bash
kafka-log-dirs.sh \
  --bootstrap-server "$BS" \
  --describe
```

（按 broker 过滤，视版本支持参数而定）

- **Observe**
  - 每个 broker 的 log dir 是否报错/离线
  - 某些分区副本是否异常大/异常小（数据倾斜、清理异常）

---

## 7. Command Deep Dive（重要）

### 7.1 kafka-consumer-groups.sh 深挖

#### 参数：--bootstrap-server

- **作用**：指定要连接的集群入口；脚本会通过它发现集群元数据。

#### 参数：--group

- **作用**：指定要查询/操作的 consumer group id。

#### 参数：--describe

输出通常包含以下字段（核心三元组 + 分区归属）：

- **CURRENT-OFFSET**
  - 定义：该 group 在该 partition 的“已提交 offset”
  - 解释：consumer 恢复后会从这里继续读（取决于 reset 策略）
- **LOG-END-OFFSET**
  - 定义：该 partition 的日志末尾 offset（下一条将写入的位置）
  - 解释：近似代表“现在 topic 写到了哪里”
- **LAG**
  - 定义：`LOG-END-OFFSET - CURRENT-OFFSET`
  - 解释：落后多少条（或近似条数，严格含义随版本/语义略有差异，但足够排障）

#### 如何解释 lag（把它当差分信号）

- **lag 持续增长**：写入速率 > 消费速率（消费跟不上）
- **lag 稳定但很大**：消费速率 ≈ 写入速率，但历史欠账很大（需要加速追赶或扩分区）
- **lag 锯齿**：常见是 rebalance/处理抖动（追一段、停一段）
- **某些分区 lag 特别大**：热点 key 导致写入倾斜，或某消费者实例负载异常

下一步动作通常是：

- 看 **分区分配是否均衡**（同一成员拿太多分区会拖慢）
- 看 **rebalance 是否频繁**（组状态不稳定会造成停顿）
- 把 lag 拆成“写入在增长吗？”（GetOffsetShell）与“消费提交在动吗？”（CURRENT-OFFSET 是否变化）

---

### 7.2 kafka-run-class.sh 深挖：它到底做了什么，为什么排障离不开它

把它理解为一个通用启动器：

- **它做的事（本质）**
  - 设置 Kafka 的 classpath（包含 Kafka 自身 jar、依赖、配置路径）
  - 解析 JVM 参数（如 `KAFKA_HEAP_OPTS`、`KAFKA_JVM_PERFORMANCE_OPTS`）
  - 然后执行：`java <MainClass> <args...>`
- **为什么重要**
  - Kafka 有大量内部诊断工具以“Java class”的形式存在（不是每个都封装成独立 `.sh`）
  - 在需要更低层证据时（offset、segment、日志格式、损坏分析），你会用它直接运行工具类
- **排障中的典型用法**
  - **查最新 offset**：`kafka.tools.GetOffsetShell`
  - **dump segment**：`kafka.tools.DumpLogSegments`
  - **检查/迁移元数据**（不同版本有不同工具；现代 KRaft 也有专门脚本，但底层仍是 class）

工程建议：

- 生产排障优先用高层脚本（topics/consumer-groups/log-dirs）
- 只有当需要“磁盘上真实证据”时再用 DumpLogSegments（它是“法医工具”）

---

## 8. 系统化 Debugging Methodology（最重要：一步步收敛）

核心原则：**先用只读命令回答三问，再进入假设验证**。

- Q1：消息有没有写进 Kafka（topic/partition 的 LEO 在增长吗）？
- Q2：目标 consumer group 有没有在消费（CURRENT-OFFSET 在动吗）？
- Q3：消费组为什么不动（rebalance/分配/外部依赖/容量瓶颈）？

### Scenario 1: Consumer Lag Increasing（积压持续增加）

1. **Check consumer group（先量化问题）**
   - `kafka-consumer-groups.sh --bootstrap-server "$BS" --group "$GROUP" --describe`
   - 观察：lag 是所有分区都涨，还是少数分区涨（热点）
2. **Check partition distribution（看是否“并行度不足/不均衡”）**
   - `kafka-topics.sh --bootstrap-server "$BS" --topic "$TOPIC" --describe`
   - 观察：partition 数是否足够；leader/ISR 是否健康
3. **Check broker load / replica health（看是否 broker 侧变慢）**
   - `kafka-log-dirs.sh --bootstrap-server "$BS" --describe`
   - 结合 broker `server.log`：是否有副本落后、ISR 缩小、磁盘异常
4. **Check network / fetch size / consumer pacing（看是否 consumer 侧吞吐被参数限制）**
   - 方向性检查：consumer 处理是否慢、poll 是否卡住、是否频繁 rebalance
   - 若 lag 锯齿明显，优先怀疑 rebalance storm（见主文档 4.6）

### Scenario 2: Messages Not Consumed（消息没被消费）

1. **Check topic existence**
   - `kafka-topics.sh --bootstrap-server "$BS" --topic "$TOPIC" --describe`
2. **Check offsets（有没有数据/有没有在增长）**
   - `kafka-run-class.sh kafka.tools.GetOffsetShell --bootstrap-server "$BS" --topic "$TOPIC" --time -1`
   - 如果 LEO 不增长：问题在 producer 或上游
3. **Check group id（是不是看错 group / 用错环境）**
   - `kafka-consumer-groups.sh --bootstrap-server "$BS" --list | grep -F "$GROUP"`
   - `kafka-consumer-groups.sh --bootstrap-server "$BS" --group "$GROUP" --describe`
4. **Check commit behavior（应用是否提交了 offset）**
   - 如果 CURRENT-OFFSET 一直不变：可能没提交、或一直在 rebalance、或卡在处理逻辑
   - 用 console consumer 直接读验证“数据确实存在且可读”

### Scenario 3: Producer Sending but No Data（producer 说发送成功但看不到数据）

1. **Check acks / retries / idempotence（先确认语义是否允许“假成功”）**
   - acks=0 可能导致“应用侧无错误但实际丢”
2. **Check leader availability（leader 是否可用/是否频繁切换）**
   - `kafka-topics.sh --bootstrap-server "$BS" --topic "$TOPIC" --describe`
3. **Check ISR（acks=all 是否被 min ISR 卡住）**
   - `kafka-topics.sh --describe` 看 ISR
   - `kafka-configs.sh --describe` 看 `min.insync.replicas`
4. **Check broker logs（server.log）**
   - 观察：NotLeader、RequestTimedOut、replica fetch 落后、磁盘错误

### Scenario 4: Broker Issues（broker/磁盘/副本问题）

1. **用 kafka-log-dirs.sh 快速定位磁盘与副本异常**
   - `kafka-log-dirs.sh --bootstrap-server "$BS" --describe`
2. **看 broker 日志**
   - `server.log`：controller、ISR 变动、磁盘 I/O 错误、请求超时
3. **把问题收敛到“数据面还是控制面”**
   - 数据面：复制落后、磁盘满、网络丢包
   - 控制面：leader 频繁切换、controller 不稳定（KRaft quorum 问题）

---

## 9. Observability Mapping（内部概念 → 可观测信号/命令）

| Concept | Where to Observe（命令/位置） | What it tells you（告诉你什么） |
|--------|-------------------------------|--------------------------------|
| Topic | `kafka-topics.sh --describe` | 分区数、副本、leader、ISR 健康度 |
| Partition | `kafka-topics.sh --describe` | 每个分区的 leader/replicas/ISR |
| Offset（commit） | `kafka-consumer-groups.sh --describe` | group 消费到哪里（CURRENT-OFFSET） |
| Offset（LEO） | `kafka-run-class.sh kafka.tools.GetOffsetShell` | topic 是否在增长（写入是否发生） |
| Lag | `kafka-consumer-groups.sh --describe` | 消费是否跟得上（差分信号） |
| ISR | `kafka-topics.sh --describe` | 副本是否同步、acks=all 是否可靠 |
| 磁盘使用/副本大小 | `kafka-log-dirs.sh --describe` | 数据分布、磁盘热点、异常副本 |
| broker 异常 | broker `server.log` | 选举、超时、I/O 错误、副本状态变化 |

---

## 附：把“理论→工具”连起来的一句话

- **写入是否发生**：看 LEO 是否增长（GetOffsetShell）
- **消费是否发生**：看 CURRENT-OFFSET 是否增长（consumer-groups describe）
- **为什么不稳定**：看 leader/ISR（topics describe）+ 看 rebalance/处理抖动（group 状态/lag 锯齿）
