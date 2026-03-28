# Apache Kafka 系统化学习走读（架构 → 抽象 → 内部机制 → 可观测）

面向读者：有 Linux/网络/分布式系统背景，但没学过 Kafka；目标是能在生产环境 **架构级理解 + 读懂内部数据流 + 用工具排障**。

配套：命令与排障手册见 `kafka-debugging-toolkit.md`。

---

## 1. 高层架构（必须从鸟瞰开始）

### 1.1 Kafka 在系统里是什么

Kafka 的核心不是“队列”，而是一个 **分布式、可持久化、可重放（replay）的追加日志系统（distributed commit log）**：

- 上游（Producer）只做一件事：把记录 **追加** 到某个日志里
- 下游（Consumer）只做一件事：从日志里 **按 offset 拉取** 记录
- Kafka 提供：分区（并行）、复制（容错）、偏移（可重放）、消费组（扩展消费并行度）

### 1.2 鸟瞰图：Producer / Broker / Topic / Partition / Consumer / Consumer Group / KRaft(ZK)

```text
                       (Kafka cluster)
                +--------------------------------+
                |   Broker 1      Broker 2       |
                |  +---------+   +---------+     |
Producer(s)     |  | Topic T |   | Topic T |     |     Consumer Group G
 +---------+    |  | P0 LDR  |<->| P0 FOL  |     |     +----------------+
 | app A   |--->|  | P1 FOL  |<->| P1 LDR  |<----|-----| Consumer C1    |
 +---------+    |  +---------+   +---------+     |     | (owns P0)      |
 +---------+    |        Broker 3                |     | Consumer C2    |
 | app B   |--->|      +---------+               |     | (owns P1)      |
 +---------+    |      | Topic T |               |     +----------------+
                |      | P0 FOL  |               |
                |      | P1 FOL  |               |
                |      +---------+               |
                +--------------------------------+

Metadata plane (modern): KRaft Controller Quorum
 +------------------------------+
 | Controller 1/2/3 (Raft)      |
 | - topic/partition metadata   |
 | - leader election decisions  |
 +------------------------------+

(legacy): ZooKeeper used to hold metadata & controller election (older Kafka)
```

**为什么要这样设计（对比传统 MQ）**

传统 MQ（典型队列）更像“把消息放进队列，消费者取走即消失”。Kafka 选择“日志”模型，带来几件 MQ 很难同时做到的能力：

- **可重放**：消息消费位置（offset）是客户端状态；你可以回放一段历史做重算/补数/调试。
- **高吞吐**：追加写 + 顺序 I/O + 批处理 + OS page cache 友好。
- **水平扩展**：topic 被切成多个 partition；生产/消费都能并行。
- **多下游**：同一份日志可以被多个 consumer group 独立消费（互不影响）。

可以把 Kafka 想成：**分布式的 append-only 文件系统 + 可复制的 commit log + 标准化的读写协议**。

---

## 2. 核心概念（清晰、精确、带误解纠偏）

下面每个概念都用同一模板：**定义 → 类比 → 为什么存在 → 关键性质 → 常见误解 → 可观测/排障抓手**。

### Concept: Topic（主题）

- **定义**：Kafka 中消息的逻辑分类名；是“日志的名字/命名空间”。
- **类比**：一组相关日志文件的目录名（但 Kafka 会把它切分为多个分区日志）。
- **为什么存在**：隔离不同业务流；便于 ACL、保留策略、分区策略配置。
- **关键性质**
  - topic 本身不保证全局顺序；顺序只在 partition 内成立。
  - topic 的保留策略通常是 **按时间/按大小** 删除旧数据（不是“被消费就删除”）。
- **常见误解**
  - “topic = queue”：不完全等价；Kafka 的消费是可重复的。
- **可观测/排障**
  - `kafka-topics.sh --describe` 看 partition 数、leader/replicas/ISR。

### Concept: Partition（分区）

- **定义**：topic 的一个物理子日志；每个 partition 是 **单调递增 offset 的追加日志**。
- **类比**：多文件分片（shard）的日志文件；每片内部严格有序。
- **为什么存在**：提供并行度与扩展性（写/读都能按分区并行）。
- **关键性质**
  - **单分区内有序**；跨分区无全局序。
  - partition 的 leader 处理写入；followers 从 leader 拉取复制。
- **常见误解**
  - “增加 consumer 数就能无限并行”：并行度上限是 partition 数（同一 group 内）。
- **可观测/排障**
  - 分区分布/ISR：`kafka-topics.sh --describe`。
  - 消费积压按分区看：`kafka-consumer-groups.sh --describe`。

### Concept: Offset（偏移）

- **定义**：partition 内记录的位置编号；消费者用它作为“读指针”。
- **类比**：文件的字节偏移/行号（更像行号：每条 record 一个 offset）。
- **为什么存在**：解耦存储与消费进度；支持 replay 与多 group 消费。
- **关键性质**
  - offset 由 broker 分配（追加写入时确定）。
  - consumer group 的 offset 通常存储在 Kafka 内部主题 `__consumer_offsets`。
- **常见误解**
  - “offset 是全局唯一”：不是；它只在 **某个 partition** 内有意义。
- **可观测/排障**
  - `CURRENT-OFFSET / LOG-END-OFFSET / LAG`：`kafka-consumer-groups.sh --describe`。
  - “最新 offset”：可用 `kafka-run-class.sh kafka.tools.GetOffsetShell ...`。

### Concept: Broker（代理/节点）

- **定义**：Kafka 集群中的服务器进程；负责存储日志、服务读写请求、参与复制。
- **类比**：分布式文件系统的数据节点 + 日志服务端。
- **为什么存在**：持久化数据、管理复制、对外提供统一协议。
- **关键性质**
  - 一个 broker 承载多个 topic/partition 的 leader 或 follower 副本。
  - broker 主要瓶颈常在：磁盘吞吐/IOPS、网络、page cache、请求线程池。
- **常见误解**
  - “broker = partition leader”：broker 只是宿主；leader 是 partition 级别属性。
- **可观测/排障**
  - broker 日志 `server.log`；磁盘目录健康：`kafka-log-dirs.sh --describe`。

### Concept: Producer（生产者）

- **定义**：向 Kafka 写入记录的客户端；负责分区选择、批处理、重试/幂等等语义。
- **类比**：把日志 append 到分布式日志文件的写入库。
- **为什么存在**：把“吞吐与语义控制”前置到客户端：批处理、压缩、幂等、事务等。
- **关键性质**
  - 写入语义由 `acks`、`retries`、`enable.idempotence`、`transactional.id` 等组合决定。
  - 默认是“至少一次”或“最多一次”（看配置与应用处理）。
- **常见误解**
  - “acks=all 就一定不丢”：仍可能在极端场景（不安全配置/unclean leader）或应用端处理里丢/重。
- **可观测/排障**
  - 失败时看异常类型（超时、NotLeaderForPartition、TimeoutException）。
  - 结合 topic 的 ISR 与 `min.insync.replicas` 判断能否满足 acks=all。

### Concept: Consumer（消费者）

- **定义**：从 Kafka 拉取记录的客户端；负责 fetch、解压、处理、提交 offset。
- **类比**：读取日志文件的 tailer，但读位置可随时跳转（seek）。
- **为什么存在**：把消费进度与处理语义交给应用掌控（replay/回溯/幂等等）。
- **关键性质**
  - Kafka 是 **pull** 模型：consumer 主动 fetch。
  - offset 提交可以自动或手动；决定“至少一次/最多一次”的边界。
- **常见误解**
  - “Kafka 会推给我所以更实时”：Kafka 不推；实时性来自 poll/fetch 周期与批量参数。
- **可观测/排障**
  - lag、分配情况：`kafka-consumer-groups.sh --describe`。
  - 处理慢导致 poll 超时：看 `max.poll.interval.ms` 与 rebalance 频率。

### Concept: Consumer Group（消费组）

- **定义**：一组协作消费同一 topic 的消费者实例；保证 **同一 group 内每个 partition 只被一个成员消费**。
- **类比**：把分区当作任务队列的“分片任务”；group 负责把分片分配给工人。
- **为什么存在**：水平扩展消费能力，同时保持每个分区内处理的顺序性。
- **关键性质**
  - 并行度上限 = partition 数（对一个 topic 来说）。
  - 组内成员变化会触发 rebalance（后面单独深挖）。
- **常见误解**
  - “多开 consumer 一定更快”：如果 partition 不够，新增 consumer 只会空转 + 引入 rebalance 成本。
- **可观测/排障**
  - 组状态、成员、分配：`kafka-consumer-groups.sh --describe`。

### Concept: Leader / Follower（分区主从副本）

- **定义**：每个 partition 在多个 broker 上有副本；其中一个是 leader（处理读写），其余是 follower（复制 leader）。
- **类比**：主库写入、从库拉取 binlog 的数据库复制。
- **为什么存在**：容错与高可用；leader 挂了可以切换。
- **关键性质**
  - producer/consumer 的请求都定向到 leader（consumer 可配是否读 follower，但常规读 leader）。
  - follower 通过 fetch 从 leader 复制并追赶。
- **常见误解**
  - “replication factor=3 就能同时读写三个副本”：不是；写入确认策略由 acks/ISR 决定。
- **可观测/排障**
  - leader 所在 broker、replicas 与 ISR：`kafka-topics.sh --describe`。

### Concept: ISR（In-Sync Replicas）

- **定义**：与 leader 保持“足够同步”的副本集合（由 controller 维护），满足一定滞后阈值。
- **类比**：数据库的“同步副本集合”，只有追得上的从库才算“同步”。
- **为什么存在**：给“写入确认”一个可操作的集合；避免等到永远追不上的副本。
- **关键性质**
  - ISR 会随副本落后/恢复而 shrink/expand。
  - 与 `min.insync.replicas` + `acks=all` 共同决定写入能否成功。
- **常见误解**
  - “ISR 越大越好”：ISR 变大意味着更强容错但可能更高写延迟（要等更多副本确认）。
- **可观测/排障**
  - ISR 变小：`kafka-topics.sh --describe` 里 ISR 数减少；broker 日志会有副本落后/加入退出信息。

### Concept: Replication Factor（复制因子）

- **定义**：每个 partition 的副本数（包含 leader + followers）。
- **类比**：数据冗余份数。
- **为什么存在**：容错：允许 broker 故障而不丢数据/不停服务（取决于配置）。
- **关键性质**
  - RF=3 常见；能容忍 1 台 broker 故障（且仍满足多数/ISR 约束取决于 min ISR）。
- **常见误解**
  - “RF=3 就一定容忍 2 台故障”：不一定；可用性与写入确认需要更多约束（min ISR、acks）。
- **可观测/排障**
  - topic describe 中 replicas 列表长度；结合 ISR 判断健康度。

### Concept: Log Segment（日志段）

- **定义**：partition 的物理日志文件被切成多个 segment（段文件）；每段包含数据文件和索引文件。
- **类比**：按大小滚动的日志文件（`app.log`, `app.log.1`...）+ 索引。
- **为什么存在**：便于删除/压缩/查找；避免单文件无限增长。
- **关键性质**
  - 顺序追加写入当前 active segment；达到阈值滚动新 segment。
  - 常见文件：`.log`（数据）、`.index`（offset 索引）、`.timeindex`（时间索引），可能还有 `.txnindex`（事务）。
- **常见误解**
  - “Kafka 会随机写磁盘”：主要是顺序追加写（随机主要来自索引/元数据/flush 以及多分区并发）。
- **可观测/排障**
  - 磁盘占用与分区分布：`kafka-log-dirs.sh --describe`。
  - 深入分析 segment：`kafka-run-class.sh kafka.tools.DumpLogSegments ...`（见配套文档）。

---

## 3. 数据流深挖（端到端机制 → 可观测行为）

### 3.1 Producer → Broker：从“send()”到“落盘/复制确认”

```text
Producer thread(s)
  |
  |  (1) serialize key/value/headers
  v
RecordAccumulator (per-partition batch)
  |
  |  (2) batch + compress (optional)
  v
Sender I/O thread
  |
  |  (3) produce request -> leader broker of target partition
  v
Broker leader append + replicate + ack
```

#### 分区选择（partition selection）

- **key-based（常见）**：对 key 做哈希 → 落到固定 partition  
  - **用途**：同一 key 的消息保持相对顺序（同分区），也便于按 key 聚合处理。
- **round-robin（无 key 或自定义）**：在可用 partition 间轮询  
  - **用途**：更均匀的吞吐分布，但没有按 key 顺序语义。

**可观测行为**

- key-based 时，同一 key 的消息在 `kafka-console-consumer.sh --property print.key=true` 下会集中到某些分区（但 console 不直接显示分区；需要结合 group describe 或客户端日志）。

#### 批处理（batching）

Kafka 吞吐很大一部分来自客户端批处理：

- `batch.size`：每个分区批次的目标大小
- `linger.ms`：为了凑批允许的等待时间（提高吞吐，增加尾延迟）
- `compression.type`：压缩（lz4/zstd/snappy/gzip）降低网络与磁盘带宽

**可观测行为**

- 吞吐上升但端到端延迟增加：常见是 `linger.ms`/批量变大导致。

#### acks（写入确认语义）

- `acks=0`：不等 broker 响应  
  - **行为**：最低延迟/最高吞吐；失败不可见（丢消息难以发现）。
- `acks=1`：leader 写入本地日志后即响应  
  - **行为**：leader 挂在复制完成前可能丢数据（见容错章节）。
- `acks=all`：等待 ISR 中副本确认（且通常还受 `min.insync.replicas` 约束）  
  - **行为**：更强持久性；在 ISR 缩小或副本落后时可能写入失败（可用性下降）。

**可观测抓手**

- 如果 `acks=all` 频繁超时/失败，第一时间看：
  - ISR 是否缩小（`kafka-topics.sh --describe`）
  - `min.insync.replicas` 是否过高（`kafka-configs.sh --describe`）

#### retry / 幂等 / 去重

- `retries` + `delivery.timeout.ms`：失败重试窗口
- **幂等 producer**（`enable.idempotence=true`）：在重试时避免“同一分区内因重试产生重复写入”  
  - 依赖 producer id + sequence number（分区级序号）机制
- **Exactly-once** 通常需要事务（producer 事务 + consumer 读已提交 + 处理/提交原子性），不是一句配置就完事（后文说明边界）。

---

### 3.2 Broker 内部：追加日志、段文件、索引、I/O 模式

#### 分区在磁盘上的布局（简化）

```text
log.dirs/
  topicT-0/                      (partition 0 directory)
    00000000000000000000.log     (segment data)
    00000000000000000000.index   (offset -> position)
    00000000000000000000.timeindex
    00000000000000123456.log
    00000000000000123456.index
    00000000000000123456.timeindex
    leader-epoch-checkpoint
  topicT-1/
    ...
```

#### 追加写与“看起来像顺序 I/O”的原因

- leader broker 对某个 partition 的写入是 **append** 到 active segment 尾部。
- Linux 下主要依赖 **page cache**：写入先进入缓存，异步 flush 到磁盘。
- 对吞吐最友好的是：
  - 顺序追加写（segment .log）
  - 批量网络发送/接收
  - 读路径多为顺序读/预读（consumer fetch）

**现实世界里的“非理想”**

Kafka 并不是只有一条日志：有很多 partition 同时写入；再加上索引、flush、后台 compaction/清理等，会造成：

- 多 partition 并发时磁盘会出现“多路顺序写交织”，表现为混合 I/O
- 延迟敏感时，你会看到 fsync/flush、磁盘队列深度等影响尾延迟

**可观测抓手**

- 分区/副本在各磁盘目录上的分布与大小：`kafka-log-dirs.sh --describe`（见配套文档）。
- broker 日志里常能看到 replica fetch 落后、ISR 变化、磁盘异常等信号。

---

### 3.3 复制：Leader ↔ Followers（以及为何会影响一致性/数据丢失）

```text
Partition P0, RF=3

          produce (writes)                 follower fetch
Producer ---------------> Broker 1 (Leader) --------------------+
                             |                                  |
                             | replicate over network           |
                             v                                  v
                        Broker 2 (Follower)                 Broker 3 (Follower)

ISR = {Broker1, Broker2, Broker3}  (healthy)
ISR = {Broker1, Broker2}           (Broker3 fell behind)
```

关键点（把“理论”落到“可观测”）：

- leader 有两个重要位置：
  - **LEO**（log end offset）：日志末尾（下一条写入位置）
  - **HW**（high watermark）：已提交（committed）的位置（简化理解：足够安全可对外可见）
- follower 通过 fetch 把自己的 LEO 追到 leader；是否仍在 ISR 取决于落后程度。
- `acks=all` 在语义上是在等待“满足 ISR / min ISR 的确认”，因此 ISR 缩小会直接让写入可用性变化。

---

### 3.4 Consumer：为什么是 Pull，不是 Push

Kafka 选择 pull 的核心原因：**可扩展性与背压控制在客户端**。

- consumer 自己决定：
  - 拉取多大（`fetch.max.bytes`/`max.partition.fetch.bytes`）
  - 拉取频率（poll 周期）
  - 处理速度与并发策略
- broker 不需要维护每个 consumer 的“推送窗口”和复杂的流控状态；能服务更多 consumer。

**可观测行为**

- consumer 处理慢时，lag 增长（可见于 `kafka-consumer-groups.sh --describe`）。
- consumer 长时间不 poll/心跳，会被认为失联，从而触发 rebalance（后文）。

#### offset 管理（提交与语义边界）

决定语义的最关键边界是：**“我什么时候提交 offset？”**

- **先提交再处理**：可能 **最多一次**（处理失败会丢）
- **先处理再提交**：典型 **至少一次**（处理成功但提交失败/重平衡可能重复）
- **Exactly-once**：需要更强的端到端原子性（事务 + 下游幂等/事务性写入），不是单靠 Kafka consumer 一侧 offset 提交能保证。

---

## 4. Consumer Group & Rebalance（关键、必须掌握）

### 4.1 分配发生在什么地方

consumer group 有一个协调点（coordinator，逻辑上是 broker 上的一段功能），负责：

- 维护组成员关系
- 触发 rebalance
- 存储/读取 group 元数据与 committed offsets（在内部主题里）

你在生产环境里看到的“组内分配/重平衡”，本质是在协调器的协议下，consumer 之间完成 **成员加入 → 选举组内 leader → 计算分配 → 下发分配 → 恢复消费**。

### 4.2 分区是如何分配给消费者的（assignment）

抽象成一句话：

> **在同一个 group 内，每个 partition 同一时刻只会被分配给一个 consumer 成员；协调器负责确保这一点。**

常见分配策略（不同 client/版本默认可能不同）：

- **range**：按分区序号分段分配  
  - 优点：简单；缺点：多 topic 时容易不均衡。
- **roundrobin**：轮询分配  
  - 优点：较均匀；缺点：成员变化时搬迁多。
- **sticky / cooperative-sticky**：尽量保持上次分配不变（减少搬迁）  
  - 工程上通常优先：**减少 rebalance 搬迁成本与停顿**。

**可观测/排障抓手**

- 看是否均衡、是否频繁变化：`kafka-consumer-groups.sh --describe`（配套文档给具体命令与字段解释）。

### 4.3 什么会触发 rebalance（trigger）

把触发条件分三类记：

- **成员变化**
  - 新 consumer 加入（扩容、重启后重新加入）
  - consumer 离开（正常关闭）或被判定失联（崩溃、卡住、网络抖动）
- **订阅变化**
  - consumer 订阅的 topic 列表变化（正则订阅匹配到新 topic）
  - topic 分区数变化（扩分区）
- **心跳/轮询约束被破坏**
  - **太久没 poll**（处理太慢、GC stop-the-world、外部依赖阻塞）→ 触发踢出
  - **太久没心跳**（网络抖动、线程饥饿）→ 触发失联

需要记住两条“时间约束”的意义（不必死背默认值）：

- `session.timeout.ms`：多久没心跳就认为成员死了
- `max.poll.interval.ms`：多久没 poll 就认为这个 consumer “不再活跃”

### 4.4 rebalance 期间发生了什么（mechanism）

把 rebalance 想成一次“停止世界的分区重新分配”（不同策略可以增量，但核心仍是搬迁）：

1. **暂停拉取**：成员停止 fetch（避免两个成员同时处理同一分区）
2. **撤销分区**（revoke）：旧成员放弃自己持有的分区
3. **重新分配**（assign）：计算新成员→分区映射
4. **确定起始 offset**：新拥有者从 committed offset（或 reset 策略）开始
5. **恢复拉取与处理**

这解释了最常见的可观测现象：

> **rebalance 会带来短暂停顿 → lag 暂时上升/消费速率下降 → 处理恢复后再追赶。**

### 4.5 时间线图：成员加入导致 rebalance

```text
Time ------------------------------------------------------------>

Consumer A:   start --------- poll/fetch -------- [rebalance] ---- fetch ---->
Consumer B:                               join -- [rebalance] ---- fetch ---->

Coordinator:                 detect join -> rebalance -> assign -> stable

Partitions:   P0 owned by A --------------------> P0 owned by A (or B)
              P1 owned by A --------------------> P1 owned by B

Observable:   lag stable  ---- spike/flatline --- lag decreases (catch-up)
```

### 4.6 常见问题：rebalance storm 与 lag spike

#### rebalance storm（重平衡风暴）

**机制级原因（常见）**

- consumer 实例频繁重启/弹性伸缩过度
- 处理链路偶发慢（下游 DB/HTTP 变慢）击穿 `max.poll.interval.ms`
- JVM GC / CPU 抢占导致心跳线程饥饿
- 网络抖动导致心跳超时，成员被踢出又重新加入

**可观测行为**

- group 状态频繁不稳定（不同版本输出略不同）
- lag 呈“锯齿”：追一段、停一段、再追

**缓解方向（原则）**

- **减少成员抖动**
  - 用 **static membership**（`group.instance.id`）减少“重启=新成员”的震荡
  - 避免不必要的 autoscaling；优先优化单实例吞吐
- **减少搬迁成本**
  - 用 **cooperative-sticky** 降低全停时间（增量协作重平衡）
- **让 consumer 更“活”**
  - 处理重时：解耦 poll 线程与处理线程（但要严格定义提交 offset 的边界）
  - 调整 `max.poll.records`/批处理，让“单次 poll 到处理完成”的时间更可控

#### lag spike（积压突刺）

**机制级原因**

- producer 突发写入峰值（写入速率上升）
- broker IO/网络抖动（复制或 fetch 变慢）
- consumer rebalance/处理慢/批量参数过小（消费速率下降）

**工程化记忆法**

> lag 是一个差分信号：\(写入速率 - 消费速率\)。  
> 排障就是找“谁变快了/谁变慢了”，再定位到具体瓶颈（网络/磁盘/CPU/外部依赖/重平衡）。

---

## 5. 容错与一致性（leader 选举、ISR 变化、数据丢失边界）

### 5.1 领导者选举是谁做的（KRaft vs ZooKeeper）

**现代 Kafka（KRaft）**

- 有一个 **controller quorum**（通常 3 或 5）
- controller 通过 Raft 复制一条 **metadata log**（topics/partitions/配置等）
- 分区 leader 变更与 ISR 管理由 controller 决策并下发

**旧架构（ZooKeeper）**

- 元数据与 controller 选举依赖 ZooKeeper
- 你需要能读懂两套术语，但新系统一般以 KRaft 为主

**可观测/排障抓手**

- 不论 ZK/KRaft：第一抓手仍是 `kafka-topics.sh --describe` 看 leader/ISR；其次看 broker `server.log` 的选举与副本事件。

### 5.2 ISR shrink/expand：系统在自我保护

ISR shrink（缩小）意味着某些 follower 落后到不再满足“同步”标准：

- 可能原因：follower 磁盘慢/网络慢/CPU 忙/GC，或 leader 写入太猛
- 直接影响：`acks=all` 变得更脆弱  
  - 如果 ISR 数 < `min.insync.replicas`，写入会失败（这是保护你不丢数据的“硬刹车”）

ISR expand（扩大）意味着落后副本追上并重新加入：

- 影响：写入可用性恢复、容错能力增强

### 5.3 broker 挂了会怎样（数据面）

假设 partition P0 的 leader 在 Broker 1，RF=3：

- Broker 1 死亡
  - controller 从 ISR 中选择一个 follower 提升为 leader
  - producer/consumer 可能短暂报 `NotLeaderForPartition`/`LeaderNotAvailable`，刷新元数据后恢复
- 如果 ISR 内没有可用副本
  - 该 partition 暂不可用（取决于配置是否允许不干净选举）

**可观测/排障抓手**

- `kafka-topics.sh --describe`：leader/ISR 的变化是最直观的健康信号
- `kafka-log-dirs.sh --describe`：可用来确认某些 broker/磁盘目录的副本状态与大小

### 5.4 数据丢失/重复会在什么条件下发生（精确边界）

把根因分成三类最不容易乱：

#### A) Producer 写入语义导致的丢/重

- **acks=0**：丢消息不可见（最危险）
- **acks=1**：leader 写了就回；若 leader 在复制前宕机，可能丢
- **重试但非幂等**：超时/断连导致 producer 认为失败而重试，可能产生重复写入

#### B) 复制与 leader 切换导致的丢（尤其是不安全选举）

最典型“真的丢数据”来自 **unclean leader election**（不推荐）：

- 当 ISR 全不可用时，从“落后副本”选 leader → 日志截断 → 丢掉之前已写入但未复制的数据

生产系统通常希望（方向）：

- `acks=all`
- `min.insync.replicas` 合理（例如 RF=3 时常设为 2）
- 禁用 unclean leader election（偏持久性）

#### C) Consumer 提交 offset 的边界导致的重/丢

- **至少一次（At-least-once）**：处理完再提交 offset  
  - 可能重复：处理成功但提交前 rebalance/崩溃，重启后会从旧 offset 再处理
- **最多一次（At-most-once）**：先提交 offset 再处理  
  - 可能丢：提交后处理失败，消息不会再被读到

#### Exactly-once（精确一次）的真实含义

Kafka 语境下的“Exactly-once”通常指：

- 幂等 + 事务性 producer（`transactional.id`）
- consumer `isolation.level=read_committed`（只读已提交事务）
- 处理结果写入下游也要配合幂等/事务（否则只能保证 Kafka 内部流转，不保证外部系统）

结论（工程化记忆法）：

> **Exactly-once 不是一个按钮，是一个端到端协议与工程约束集合。**

---

## 10. Mental Model Summary（压缩心智模型）

Kafka 是：

- 一个分布式 **append-only 日志系统**
- **Topic** 是日志集合名字
- **Partition** 是并行与顺序的基本单元（单分区有序）
- **Offset** 是读指针（消费者状态），让 replay 成为常态能力
- **复制（RF/Leader/Follower/ISR）** 提供容错与持久性
- **Consumer Group** 把分区分配给一组消费者，实现水平扩展
- **Pull 模型** 让背压与吞吐控制在客户端

如果你只记三条，用于生产排障：

- **数据在哪里**：在 partition 的追加日志里（段文件 + 索引）
- **谁在读**：consumer group 的 committed offset 决定从哪读
- **为什么写不进去/读不动**：看 leader/ISR（写入语义）+ 看 rebalance/处理耗时（消费语义）

---

## 下一步：系统化练习建议（从理论走到可观测）

- **练习 1（只看结构）**：对一个真实 topic：
  - 看 partition 数与 leader/ISR
  - 看某个 consumer group 的分配与 lag
- **练习 2（验证数据流）**：用 console producer 写入，用 console consumer 读取，观察 offset/lag 变化
- **练习 3（故障注入思维）**：故意让 consumer 慢处理/重启，观察 rebalance 与 lag 的“锯齿”

具体命令与逐步排障流程见 `kafka-debugging-toolkit.md`。

