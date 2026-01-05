# Reflection and Knowledge Transfer

## Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                     KAFKA'S STRENGTHS AND LIMITATIONS                            │
└─────────────────────────────────────────────────────────────────────────────────┘

    SWEET SPOT                                    BREAKS DOWN
    ══════════                                    ═══════════

    ┌─────────────────────────────────┐          ┌─────────────────────────────────┐
    │                                 │          │                                 │
    │  High Throughput Streaming      │          │  Ultra-Low Latency (<5ms)       │
    │  ├─ Event sourcing              │          │  ├─ Trading systems             │
    │  ├─ Log aggregation             │          │  ├─ Real-time gaming            │
    │  ├─ Metrics collection          │          │  └─ Robotics control            │
    │  └─ Change data capture         │          │                                 │
    │                                 │          │  Request-Reply Patterns         │
    │  Decoupled Microservices        │          │  ├─ RPC calls                   │
    │  ├─ Async communication         │          │  ├─ API gateways               │
    │  ├─ Event-driven architecture   │          │  └─ Synchronous workflows       │
    │  └─ Service mesh events         │          │                                 │
    │                                 │          │  Small Scale                    │
    │  Data Pipeline                  │          │  ├─ < 1000 msg/sec             │
    │  ├─ ETL processes               │          │  ├─ Single machine sufficient  │
    │  ├─ Stream processing           │          │  └─ Operational overhead       │
    │  └─ Analytics feed              │          │                                 │
    │                                 │          │  Complex Queries                │
    │  Replay & Audit                 │          │  ├─ Random key lookup          │
    │  ├─ Event replay                │          │  ├─ Aggregations               │
    │  ├─ Audit logs                  │          │  └─ Joins across topics        │
    │  └─ State reconstruction        │          │                                 │
    │                                 │          │                                 │
    └─────────────────────────────────┘          └─────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     KAFKA IDEAS TO REUSE IN YOUR DESIGNS                         │
└─────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  IDEA 1: LOG-BASED ARCHITECTURE                                             │
    │  ═══════════════════════════════                                            │
    │                                                                             │
    │  Traditional State:                    Log-Based State:                     │
    │  ┌─────────────────┐                  ┌─────────────────────────────────┐  │
    │  │  Database       │                  │  Event Log                      │  │
    │  │  (current state │                  │  ┌───┬───┬───┬───┬───┬───┐     │  │
    │  │   only)         │                  │  │e1 │e2 │e3 │e4 │e5 │...│     │  │
    │  └─────────────────┘                  │  └───┴───┴───┴───┴───┴───┘     │  │
    │                                       │           │                     │  │
    │                                       │           v                     │  │
    │                                       │  ┌─────────────────────────┐   │  │
    │                                       │  │ Derived View (database) │   │  │
    │                                       │  │ Derived View (cache)    │   │  │
    │                                       │  │ Derived View (search)   │   │  │
    │                                       │  └─────────────────────────┘   │  │
    │                                       └─────────────────────────────────┘  │
    │                                                                             │
    │  Benefits:                                                                  │
    │  • Audit trail built-in                                                    │
    │  • Multiple derived views from same truth                                  │
    │  • Easy to add new consumers                                               │
    │  • Debug by replaying events                                               │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  IDEA 2: PARTITIONED PARALLELISM                                            │
    │  ═══════════════════════════════                                            │
    │                                                                             │
    │  Single Queue (Scaling Limit):        Partitioned (Linear Scale):          │
    │                                                                             │
    │  ┌─────────────────────────┐          ┌───────────┐ ┌───────────┐          │
    │  │   Queue                 │          │  Part 0   │ │  Part 1   │          │
    │  │   ┌───┬───┬───┬───┬───┐│          │  ┌───┐    │ │  ┌───┐    │          │
    │  │   │msg│msg│msg│msg│msg││          │  │msg│    │ │  │msg│    │          │
    │  │   └───┴───┴───┴───┴───┘│          │  └───┘    │ │  └───┘    │          │
    │  │           │            │          │     │     │ │     │     │          │
    │  │           v            │          │     v     │ │     v     │          │
    │  │   ┌───────────────┐    │          │ ┌──────┐  │ │ ┌──────┐  │          │
    │  │   │  Consumer     │    │          │ │ C1   │  │ │ │ C2   │  │          │
    │  │   └───────────────┘    │          │ └──────┘  │ │ └──────┘  │          │
    │  │   (bottleneck)         │          └───────────┘ └───────────┘          │
    │  └─────────────────────────┘                                               │
    │                                       Add partitions = add parallelism     │
    │                                                                             │
    │  Apply in your designs:                                                    │
    │  • Shard by user_id, tenant_id, etc.                                      │
    │  • Each shard independently processable                                    │
    │  • Ordering only needed within shard                                       │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  IDEA 3: IMMUTABLE DATA FLOWS                                               │
    │  ════════════════════════════                                               │
    │                                                                             │
    │  Mutable (Traditional):               Immutable (Kafka-style):             │
    │                                                                             │
    │  ┌─────────────────────┐              ┌─────────────────────────────────┐  │
    │  │  record.status =    │              │  Events:                        │  │
    │  │    "pending"        │              │  ├─ OrderCreated                │  │
    │  │        │            │              │  ├─ OrderPaid                   │  │
    │  │        v            │              │  ├─ OrderShipped                │  │
    │  │  record.status =    │              │  └─ OrderDelivered              │  │
    │  │    "paid"           │              │                                 │  │
    │  │        │            │              │  State = f(events)              │  │
    │  │        v            │              │                                 │  │
    │  │  record.status =    │              │  Current status = last event   │  │
    │  │    "shipped"        │              │  History = all events           │  │
    │  │                     │              │                                 │  │
    │  └─────────────────────┘              └─────────────────────────────────┘  │
    │                                                                             │
    │  Benefits:                                                                  │
    │  • No lost updates (concurrent modifications)                              │
    │  • Audit trail automatic                                                   │
    │  • Time-travel queries possible                                            │
    │  • Simpler concurrency model                                               │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  IDEA 4: EXPLICIT OFFSET-BASED STATE RECOVERY                               │
    │  ════════════════════════════════════════════                               │
    │                                                                             │
    │  Instead of:                          Use:                                  │
    │  ┌─────────────────────┐              ┌─────────────────────────────────┐  │
    │  │  Checkpoint:        │              │  Offset-based:                  │  │
    │  │  "last_processed_   │              │                                 │  │
    │  │   at = 2024-01-15   │              │  committed_offset = 12345       │  │
    │  │   14:30:22"         │              │                                 │  │
    │  │                     │              │  On recovery:                   │  │
    │  │  Problem: What if   │              │  1. Read committed_offset       │  │
    │  │  timestamps don't   │              │  2. Seek to offset + 1          │  │
    │  │  match exactly?     │              │  3. Continue processing         │  │
    │  │                     │              │                                 │  │
    │  └─────────────────────┘              │  Deterministic recovery!        │  │
    │                                       └─────────────────────────────────┘  │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     DESIGNING YOUR OWN STREAMING SYSTEM                          │
└─────────────────────────────────────────────────────────────────────────────────┘

    IDEAS TO KEEP FROM KAFKA                  IDEAS TO SIMPLIFY
    ════════════════════════                  ═══════════════════

    ✓ Append-only log storage                 • Consumer groups (if single consumer)
    ✓ Partition-based parallelism             • Replication (if single node OK)
    ✓ Offset-based consumption                • Exactly-once (if at-least-once OK)
    ✓ Pull-based reading                      • Compaction (if simple retention OK)
    ✓ Batching for throughput                 • Transactions (if no cross-topic atomicity)
    ✓ Sequential I/O optimization
    ✓ Zero-copy where possible

    HISTORICAL TRADE-OFFS (May Not Apply To You)
    ════════════════════════════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │  1. Zookeeper Dependency (Historical)                                       │
    │     ─────────────────────────────────                                       │
    │     Kafka originally used Zookeeper for coordination.                       │
    │     KRaft removes this. Your system: use Raft directly if needed.          │
    │                                                                             │
    │  2. No Built-in Exactly-Once Consumer                                       │
    │     ─────────────────────────────────                                       │
    │     Kafka added this later. If you control producer + consumer,            │
    │     you might not need it (idempotent consumers simpler).                  │
    │                                                                             │
    │  3. JVM-Based (Originally)                                                  │
    │     ────────────────────────                                                │
    │     Kafka brokers are JVM. librdkafka proves C clients work fine.          │
    │     Your system: choose runtime for your needs.                            │
    │                                                                             │
    │  4. Complex Protocol Evolution                                              │
    │     ────────────────────────────                                            │
    │     40+ API versions for compatibility. If greenfield, you can             │
    │     version more aggressively.                                              │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     MINIMUM VIABLE STREAMING SYSTEM                              │
└─────────────────────────────────────────────────────────────────────────────────┘

    If you were to build a simple Kafka-like system:

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │  STORAGE LAYER                                                              │
    │  ═════════════                                                              │
    │                                                                             │
    │  struct segment {                                                           │
    │      int fd;                    // File descriptor                         │
    │      int64_t base_offset;       // First offset in segment                 │
    │      int64_t size;              // Current size                            │
    │  };                                                                         │
    │                                                                             │
    │  struct partition {                                                         │
    │      char *topic;                                                          │
    │      int32_t id;                                                           │
    │      segment *segments;         // Array of segments                       │
    │      int64_t next_offset;       // Next offset to assign                   │
    │      pthread_mutex_t lock;      // Write lock                              │
    │  };                                                                         │
    │                                                                             │
    │  // Append (producer)                                                       │
    │  int64_t append(partition *p, void *data, size_t len) {                    │
    │      pthread_mutex_lock(&p->lock);                                         │
    │      segment *s = get_active_segment(p);                                   │
    │      write_record(s, p->next_offset, data, len);                           │
    │      int64_t offset = p->next_offset++;                                    │
    │      pthread_mutex_unlock(&p->lock);                                       │
    │      return offset;                                                         │
    │  }                                                                          │
    │                                                                             │
    │  // Read (consumer)                                                         │
    │  record *read_from(partition *p, int64_t offset, int max_records) {        │
    │      segment *s = find_segment(p, offset);                                 │
    │      return read_records(s, offset, max_records);                          │
    │  }                                                                          │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │  PROTOCOL (Simple Version)                                                  │
    │  ═════════════════════════                                                  │
    │                                                                             │
    │  Request Types:                                                             │
    │  ┌────────────────────────────────────────────────────────────┐            │
    │  │  PRODUCE   │ topic, partition, records[]                   │            │
    │  │  FETCH     │ topic, partition, offset, max_bytes           │            │
    │  │  METADATA  │ topics[]                                      │            │
    │  │  COMMIT    │ group, topic, partition, offset               │            │
    │  └────────────────────────────────────────────────────────────┘            │
    │                                                                             │
    │  Response Format:                                                           │
    │  ┌────────────────────────────────────────────────────────────┐            │
    │  │  error_code │ response_data                                │            │
    │  └────────────────────────────────────────────────────────────┘            │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │  WHAT YOU CAN SKIP (For Simple Use Cases)                                   │
    │  ═════════════════════════════════════════                                  │
    │                                                                             │
    │  ❌ Replication         → Single node, rely on disk/backup                 │
    │  ❌ Consumer groups     → Single consumer per partition                    │
    │  ❌ Transactions        → At-least-once is enough                          │
    │  ❌ Compaction          → Time-based retention only                        │
    │  ❌ Schema registry     → Application handles schema                       │
    │  ❌ ACLs                → Trust internal services                          │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     LEARNING OUTCOME CHECKLIST                                   │
└─────────────────────────────────────────────────────────────────────────────────┘

    After studying these materials, verify you can:

    ARCHITECTURE
    ────────────
    □ Explain why Kafka uses a log-based design
    □ Describe the role of partitions in parallelism
    □ Explain leader-follower replication
    □ Describe consumer group coordination
    □ Explain the difference between high watermark and log end offset

    MESSAGE LIFECYCLE
    ─────────────────
    □ Trace a message from rd_kafka_producev() to broker acknowledgment
    □ Explain batching and when batches are sent
    □ Describe what happens during a fetch request
    □ Explain offset commit and its implications

    librdkafka USAGE
    ────────────────
    □ Configure a producer for at-least-once delivery
    □ Configure a consumer with manual offset commits
    □ Implement a rebalance callback correctly
    □ Handle delivery report callbacks
    □ Explain thread safety rules

    FAILURE HANDLING
    ────────────────
    □ Describe what happens when a broker fails
    □ Explain retry behavior and configuration
    □ Describe rebalancing scenarios and implications
    □ Explain offset commit failure scenarios

    PERFORMANCE
    ───────────
    □ Tune linger.ms and batch.size for throughput vs latency
    □ Explain compression trade-offs
    □ Describe backpressure handling strategies
    □ Explain when to use transactions

    DESIGN APPLICATION
    ──────────────────
    □ Apply log-based thinking to a new problem
    □ Design partition strategy for a use case
    □ Decide when Kafka is appropriate vs alternatives
    □ Identify ideas from Kafka applicable elsewhere
```

---

## 1. When Does Kafka Break Down?

### 1.1 Ultra-Low Latency Requirements (<10ms)

**Problem:** Kafka introduces latency through:
- Batching (linger.ms)
- Replication (acks=all)
- Network round trips
- Disk I/O (even with page cache)

**Symptoms:**
```
Typical Kafka latency:
- acks=0: ~1-5ms (fire and forget)
- acks=1: ~5-15ms (leader ack)
- acks=all: ~10-30ms (ISR ack)

If you need <5ms consistently: Kafka is wrong tool
```

**Alternatives:**
- Direct TCP/UDP
- Shared memory IPC
- In-process queues
- Specialized low-latency messaging (Aeron, Chronicle Queue)

### 1.2 Strict Real-Time Systems

**Problem:** Kafka provides no latency guarantees.

```
Real-time requirements:
┌──────────────────────────────────────────────────────────────┐
│  Hard real-time:  Miss deadline = system failure             │
│                   (robotics, medical devices)                │
│                   Kafka: ❌ NOT SUITABLE                     │
│                                                              │
│  Soft real-time:  Miss deadline = degraded experience        │
│                   (video streaming, games)                   │
│                   Kafka: ⚠️ MARGINAL (tune carefully)        │
│                                                              │
│  Best effort:     No deadline guarantees needed              │
│                   (analytics, logging)                       │
│                   Kafka: ✓ IDEAL                             │
└──────────────────────────────────────────────────────────────┘
```

### 1.3 Very Small-Scale Deployments

**Problem:** Operational overhead not justified.

```
Break-even analysis:
┌──────────────────────────────────────────────────────────────┐
│  Volume < 1000 msg/sec                                       │
│  ├─ Kafka: 3 brokers + Zookeeper (or KRaft)                 │
│  ├─ Monitoring, alerting, upgrades                          │
│  └─ Team knowledge required                                  │
│                                                              │
│  Alternative:                                                │
│  ├─ Redis Streams (single node)                             │
│  ├─ PostgreSQL NOTIFY/LISTEN                                │
│  ├─ Simple file-based queue                                 │
│  └─ In-process queue                                        │
└──────────────────────────────────────────────────────────────┘
```

### 1.4 Complex Query Requirements

**Problem:** Kafka is not a database.

```
What Kafka can do:                What Kafka cannot do:
├─ Read by offset                 ├─ Query by arbitrary key
├─ Read by timestamp (approx)     ├─ SQL-like queries
├─ Scan partition sequentially    ├─ Joins across topics
└─ Read latest per key (compact)  └─ Aggregations in broker
```

**Pattern:** Use Kafka for transport, database for queries.

---

## 2. Ideas to Reuse Outside Kafka

### 2.1 Log-Based System Design

**Core principle:** Events are the source of truth, state is derived.

```c
// Event-sourced design pattern
typedef struct event {
    uint64_t sequence;      // Like Kafka offset
    uint64_t timestamp;
    char type[32];          // Event type
    size_t payload_len;
    char payload[];         // Event data
} event_t;

// State is computed from events
typedef struct account {
    double balance;         // Derived from deposits/withdrawals
    char status[16];        // Derived from status change events
} account_t;

account_t rebuild_account(event_t *events, size_t count) {
    account_t acc = {0};
    for (size_t i = 0; i < count; i++) {
        apply_event(&acc, &events[i]);
    }
    return acc;
}
```

**Benefits:**
- Complete audit trail
- Time-travel debugging
- Multiple views from same events
- Easy to add new derived views

### 2.2 Partitioned Parallelism

**Core principle:** Partition data to enable parallel processing while maintaining order where needed.

```c
// Partition by key
int get_partition(const char *key, int num_partitions) {
    uint32_t hash = murmur3_32(key, strlen(key), 0);
    return hash % num_partitions;
}

// Example: Order processing
// Same customer_id always goes to same partition
// Orders for same customer processed in order
// Different customers processed in parallel

void process_order(order_t *order) {
    int partition = get_partition(order->customer_id, NUM_WORKERS);
    enqueue(&worker_queues[partition], order);
}
```

**When to apply:**
- User data (partition by user_id)
- Tenant data (partition by tenant_id)
- Geographic data (partition by region)
- Time-series data (partition by time bucket)

### 2.3 Immutable Data Flows

**Core principle:** Never mutate, only append.

```c
// Mutable approach (problematic)
void update_order(order_t *order, const char *new_status) {
    strcpy(order->status, new_status);  // History lost!
}

// Immutable approach
typedef struct order_event {
    char order_id[64];
    char event_type[32];  // "created", "paid", "shipped", "delivered"
    uint64_t timestamp;
    char data[];
} order_event_t;

void record_order_event(const char *order_id, const char *event_type,
                        const void *data, size_t len) {
    order_event_t *event = create_event(order_id, event_type, data, len);
    append_to_log(event);  // Never modify existing events
}

// Current status = latest event of type "status_changed"
// Full history = all events for order_id
```

### 2.4 Explicit Offset-Based State Recovery

**Core principle:** Use deterministic position markers for recovery.

```c
typedef struct checkpoint {
    char stream_name[64];
    int64_t offset;         // Position in stream
    uint64_t timestamp;     // When checkpointed
} checkpoint_t;

// On startup
void recover() {
    checkpoint_t cp = load_checkpoint();
    seek_to(stream, cp.offset + 1);
    
    while (running) {
        record_t *rec = read_next(stream);
        process(rec);
        
        // Periodic checkpoint
        if (should_checkpoint()) {
            save_checkpoint(stream, rec->offset);
        }
    }
}

// Recovery is deterministic:
// 1. Load checkpoint (offset N)
// 2. Process from N+1 onwards
// No ambiguity about what's been processed
```

---

## 3. If I Were to Design My Own Streaming System

### 3.1 Ideas I Would Keep

```
MUST HAVE                          RATIONALE
────────                           ─────────

Append-only log                    • Simple, fast, reliable
                                   • Sequential I/O
                                   • Easy replication

Partition-based parallelism        • Scales horizontally
                                   • Maintains order per partition
                                   • Worker assignment clear

Offset-based consumption           • Deterministic replay
                                   • Simple state tracking
                                   • Clear recovery semantics

Pull-based reading                 • Consumer controls pace
                                   • Natural backpressure
                                   • Batch efficiently

Batching for throughput            • Amortizes overhead
                                   • Compression opportunities
                                   • Network efficiency
```

### 3.2 Ideas I Would Simplify

```
SIMPLIFY                           HOW
────────                           ───

Consumer groups                    → Single consumer per partition
                                     (application manages assignment)

Replication                        → Start single-node
                                     (add when needed, or use storage-level)

Exactly-once                       → At-least-once + idempotent consumers
                                     (simpler, covers most cases)

Compaction                         → Time-based retention only
                                     (use separate KV store for latest)

Protocol versioning                → Break compatibility when needed
                                     (internal system, control both ends)
```

### 3.3 Historical Trade-offs to Reconsider

| Kafka Choice | Historical Reason | Modern Alternative |
|--------------|-------------------|-------------------|
| JVM broker | LinkedIn's stack | Rust/C++ for lower latency |
| Zookeeper | Distributed consensus | Built-in Raft (KRaft) |
| Complex protocol | Backward compatibility | Simpler versioning if greenfield |
| Batch-only API | Performance | Streaming API option |

---

## 4. Final Learning Checklist

### Architecture Understanding

- [ ] Can explain Kafka's log-based design and its advantages
- [ ] Understand partition as unit of parallelism and ordering
- [ ] Know the roles of broker, producer, consumer, coordinator
- [ ] Understand ISR, high watermark, and replication
- [ ] Can describe consumer group protocol (JoinGroup, SyncGroup)

### Message Lifecycle Mastery

- [ ] Can trace producer message from application to broker ACK
- [ ] Understand batching triggers (size, time, flush)
- [ ] Know fetch request/response flow
- [ ] Understand offset semantics and commit implications

### librdkafka Proficiency

- [ ] Can configure producer with appropriate delivery guarantees
- [ ] Can implement consumer with correct offset handling
- [ ] Know when and how to use rebalance callbacks
- [ ] Understand thread safety rules and callback contexts
- [ ] Can tune performance knobs for specific workloads

### Failure Handling Expertise

- [ ] Know what happens during broker failure and recovery
- [ ] Understand retry behavior and when retries stop
- [ ] Can handle rebalancing without data loss/duplication
- [ ] Know offset commit failure scenarios and mitigation

### Performance Optimization Skills

- [ ] Can tune latency vs throughput trade-offs
- [ ] Understand compression options and when to use
- [ ] Can implement backpressure in producer and consumer
- [ ] Know when transactions are needed vs overhead

### Design Application Capability

- [ ] Can apply log-based thinking to new problems
- [ ] Can design partition strategy for specific use cases
- [ ] Can decide when Kafka is appropriate vs alternatives
- [ ] Can extract and apply Kafka ideas in custom systems

---

## 中文解释 (Chinese Explanations)

### 1. Kafka 何时不适用？

**超低延迟需求（<10ms）：**
- Kafka 通过批处理、复制、网络往返引入延迟
- 如果需要持续 <5ms 延迟，Kafka 不合适
- 替代方案：直接 TCP/UDP、共享内存、专用低延迟消息系统

**严格实时系统：**
- 硬实时：错过截止时间=系统故障，Kafka 不适用
- 软实时：错过截止时间=体验下降，需仔细调优
- 尽力而为：无截止时间保证，Kafka 理想

**小规模部署：**
- 流量 <1000 msg/sec 时运维开销不划算
- 替代方案：Redis Streams、PostgreSQL、简单文件队列

**复杂查询需求：**
- Kafka 不是数据库
- 不能按任意键查询、不能 SQL、不能跨主题 Join
- 模式：Kafka 传输，数据库查询

### 2. 可在其他地方重用的 Kafka 思想

**日志设计：**
- 事件是事实来源，状态是派生的
- 好处：审计追踪、时间旅行调试、多视图

**分区并行：**
- 按键分区实现并行处理
- 同一分区内保持顺序
- 应用：用户 ID、租户 ID、地理区域

**不可变数据流：**
- 从不修改，只追加
- 好处：完整历史、无更新丢失、简化并发

**偏移量恢复：**
- 使用确定性位置标记
- 恢复：加载检查点，从偏移量+1 继续
- 无歧义

### 3. 设计自己的流系统

**保留的思想：**
- 追加日志
- 分区并行
- 偏移量消费
- 拉取式读取
- 批处理吞吐

**简化的部分：**
- 消费组 → 每分区单消费者
- 复制 → 单节点起步
- 精确一次 → 至少一次 + 幂等消费者
- 压缩 → 仅基于时间的保留

**重新考虑的历史权衡：**
- JVM Broker → 现代 Rust/C++
- Zookeeper → 内置 Raft
- 复杂协议 → 简单版本控制

### 4. 学习成果检查清单

**架构理解：**
- 解释日志设计及其优势
- 理解分区作为并行和排序单元
- 知道各组件角色
- 理解 ISR、高水位、复制
- 描述消费组协议

**消息生命周期：**
- 追踪生产者消息到 Broker ACK
- 理解批处理触发器
- 知道获取请求/响应流程
- 理解偏移量语义和提交含义

**librdkafka 熟练度：**
- 配置适当投递保证的生产者
- 实现正确偏移量处理的消费者
- 知道何时使用再平衡回调
- 理解线程安全规则
- 调优性能旋钮

**设计应用能力：**
- 将日志思维应用于新问题
- 为特定用例设计分区策略
- 决定何时使用 Kafka
- 提取并应用 Kafka 思想
