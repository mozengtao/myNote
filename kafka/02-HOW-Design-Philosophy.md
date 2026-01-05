# HOW | Design Philosophy and Core Ideas

## Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                     KAFKA'S FUNDAMENTAL ABSTRACTION: THE LOG                     │
└─────────────────────────────────────────────────────────────────────────────────┘

    Topic: "orders"
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │  Partition 0 (Leader: Broker 1)                                            │
    │  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐                 │
    │  │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │ 8  │ 9  │....│  ──> append    │
    │  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘                 │
    │                            ^                                               │
    │                            │                                               │
    │                      Consumer A (offset=5)                                 │
    │                                                                             │
    │  Partition 1 (Leader: Broker 2)                                            │
    │  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┐                           │
    │  │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │ 6  │ 7  │....│  ──> append              │
    │  └────┴────┴────┴────┴────┴────┴────┴────┴────┘                           │
    │                 ^                                                          │
    │                 │                                                          │
    │           Consumer B (offset=3)                                            │
    │                                                                             │
    │  Partition 2 (Leader: Broker 3)                                            │
    │  ┌────┬────┬────┬────┬────┬────┬────┐                                     │
    │  │ 0  │ 1  │ 2  │ 3  │ 4  │ 5  │....│  ──> append                        │
    │  └────┴────┴────┴────┴────┴────┴────┘                                     │
    │       ^                                                                    │
    │       │                                                                    │
    │  Consumer C (offset=1)                                                     │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    Key Insight: Each partition is an independent, ordered, append-only log


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     ARCHITECTURAL COMPONENTS OVERVIEW                            │
└─────────────────────────────────────────────────────────────────────────────────┘

                                    ┌───────────────────┐
                                    │    Zookeeper /    │
                                    │    KRaft (new)    │
                                    │   (Coordination)  │
                                    └─────────┬─────────┘
                                              │
              ┌───────────────────────────────┼───────────────────────────────┐
              │                               │                               │
              v                               v                               v
    ┌─────────────────┐           ┌─────────────────┐           ┌─────────────────┐
    │    Broker 1     │           │    Broker 2     │           │    Broker 3     │
    │   (Controller)  │<─────────>│                 │<─────────>│                 │
    │                 │           │                 │           │                 │
    │  ┌───────────┐  │           │  ┌───────────┐  │           │  ┌───────────┐  │
    │  │ Topic A   │  │           │  │ Topic A   │  │           │  │ Topic A   │  │
    │  │ P0 LEADER │  │           │  │ P0 follower│ │           │  │ P1 LEADER │  │
    │  │ P1 follower│ │           │  │ P1 follower│ │           │  │ P0 follower│ │
    │  └───────────┘  │           │  └───────────┘  │           │  └───────────┘  │
    └─────────────────┘           └─────────────────┘           └─────────────────┘
              ^                           ^                           ^
              │                           │                           │
    ┌─────────┴─────────┐       ┌─────────┴─────────┐                │
    │                   │       │                   │                │
    │                   │       │                   │                │
┌───────────┐    ┌───────────┐  │            ┌───────────────────────┘
│ Producer  │    │ Producer  │  │            │
│     1     │    │     2     │  │            │
└───────────┘    └───────────┘  │            │
                                │            │
                    ┌───────────┴────┐  ┌────┴──────────┐
                    │  Consumer      │  │  Consumer     │
                    │  Group A       │  │  Group B      │
                    │  ┌────┬────┐   │  │  ┌────┐       │
                    │  │ C1 │ C2 │   │  │  │ C1 │       │
                    │  └────┴────┘   │  │  └────┘       │
                    └────────────────┘  └───────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     DATA FLOW: PRODUCER TO CONSUMER                              │
└─────────────────────────────────────────────────────────────────────────────────┘

    PRODUCER SIDE                           BROKER SIDE
    ═════════════                           ═══════════

    ┌─────────────────┐                    ┌─────────────────────────────────┐
    │  Application    │                    │           BROKER                │
    │                 │                    │                                 │
    │  produce(msg)   │                    │  ┌───────────────────────────┐  │
    │       │         │                    │  │     Network Thread        │  │
    │       v         │                    │  │     (Acceptor)            │  │
    │  ┌─────────┐    │                    │  └───────────┬───────────────┘  │
    │  │Serializer│   │                    │              │                  │
    │  └────┬────┘    │                    │              v                  │
    │       │         │                    │  ┌───────────────────────────┐  │
    │       v         │                    │  │     Request Handler       │  │
    │  ┌─────────┐    │   ProduceRequest   │  │     Thread Pool           │  │
    │  │Partitioner│  │ ═══════════════>   │  └───────────┬───────────────┘  │
    │  └────┬────┘    │                    │              │                  │
    │       │         │                    │              v                  │
    │       v         │                    │  ┌───────────────────────────┐  │
    │  ┌─────────┐    │                    │  │     Log Manager           │  │
    │  │ Batcher │    │                    │  │                           │  │
    │  │ (per    │    │                    │  │  append(partition, msgs)  │  │
    │  │partition)│   │                    │  └───────────┬───────────────┘  │
    │  └────┬────┘    │                    │              │                  │
    │       │         │                    │              v                  │
    │       v         │                    │  ┌───────────────────────────┐  │
    │  ┌─────────┐    │                    │  │     Page Cache (OS)       │  │
    │  │ Sender  │    │                    │  │     + Disk Write          │  │
    │  │ Thread  │    │                    │  └───────────────────────────┘  │
    │  └─────────┘    │                    │                                 │
    │                 │                    │  ┌───────────────────────────┐  │
    └─────────────────┘                    │  │     Replication           │  │
                                           │  │     (to followers)        │  │
                                           │  └───────────────────────────┘  │
                                           └─────────────────────────────────┘

    CONSUMER SIDE
    ═════════════

    ┌─────────────────┐                    ┌─────────────────────────────────┐
    │   Application   │                    │           BROKER                │
    │                 │                    │                                 │
    │   poll()        │   FetchRequest     │  ┌───────────────────────────┐  │
    │       │         │ ═══════════════>   │  │     Fetch Handler         │  │
    │       │         │                    │  └───────────┬───────────────┘  │
    │       │         │                    │              │                  │
    │       │         │                    │              v                  │
    │       │         │                    │  ┌───────────────────────────┐  │
    │       │         │                    │  │   Read from Page Cache    │  │
    │       │         │   FetchResponse    │  │   (zero-copy sendfile)    │  │
    │       │         │ <═══════════════   │  └───────────────────────────┘  │
    │       v         │                    │                                 │
    │  ┌─────────┐    │                    └─────────────────────────────────┘
    │  │Deserialize│  │
    │  └────┬────┘    │
    │       │         │
    │       v         │
    │  process(msg)   │
    │       │         │
    │       v         │
    │  commit(offset) │ ───> __consumer_offsets topic
    │                 │
    └─────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     DELIVERY SEMANTICS                                           │
└─────────────────────────────────────────────────────────────────────────────────┘

    AT-MOST-ONCE                      AT-LEAST-ONCE                  EXACTLY-ONCE
    ═════════════                     ═════════════                  ════════════

    Producer:                         Producer:                      Producer:
    acks=0 (fire & forget)           acks=all + retries             enable.idempotence=true
                                                                     + transactional.id

    ┌─────────┐                      ┌─────────┐                     ┌─────────┐
    │ produce │                      │ produce │                     │ begin   │
    └────┬────┘                      └────┬────┘                     │ txn     │
         │                                │                          └────┬────┘
         │ (no wait)                      │                               │
         v                                v                               v
    ┌─────────┐                      ┌─────────┐                     ┌─────────┐
    │ continue│                      │ wait for│                     │ produce │
    │         │                      │ ack     │                     │ (idempot)│
    └─────────┘                      └────┬────┘                     └────┬────┘
                                          │                               │
                                     timeout?                             │
                                          │                               v
                                     ┌────┴────┐                     ┌─────────┐
                                     │  retry  │                     │ commit  │
                                     └─────────┘                     │ txn     │
                                                                     └─────────┘

    Risk: Message lost             Risk: Duplicates                 Risk: Higher latency
    if broker crashes              on retry                         + complexity


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     CONSUMER GROUPS AND PARTITION ASSIGNMENT                     │
└─────────────────────────────────────────────────────────────────────────────────┘

    Topic: orders (4 partitions)
    Consumer Group: order-processor

    SCENARIO 1: 2 Consumers              SCENARIO 2: 4 Consumers
    ════════════════════════             ════════════════════════

    ┌────┬────┬────┬────┐               ┌────┬────┬────┬────┐
    │ P0 │ P1 │ P2 │ P3 │               │ P0 │ P1 │ P2 │ P3 │
    └──┬─┴──┬─┴──┬─┴──┬─┘               └──┬─┴──┬─┴──┬─┴──┬─┘
       │    │    │    │                    │    │    │    │
       └──┬─┘    └──┬─┘                    │    │    │    │
          │        │                       │    │    │    │
          v        v                       v    v    v    v
       ┌────┐   ┌────┐                  ┌────┐┌────┐┌────┐┌────┐
       │ C1 │   │ C2 │                  │ C1 ││ C2 ││ C3 ││ C4 │
       └────┘   └────┘                  └────┘└────┘└────┘└────┘

    C1: P0, P1                          C1: P0
    C2: P2, P3                          C2: P1
                                        C3: P2
                                        C4: P3

    SCENARIO 3: 6 Consumers (over-provisioned)
    ══════════════════════════════════════════

    ┌────┬────┬────┬────┐
    │ P0 │ P1 │ P2 │ P3 │
    └──┬─┴──┬─┴──┬─┴──┬─┘
       │    │    │    │
       v    v    v    v
    ┌────┐┌────┐┌────┐┌────┐  ┌────┐┌────┐
    │ C1 ││ C2 ││ C3 ││ C4 │  │ C5 ││ C6 │  <- IDLE (no partitions)
    └────┘└────┘└────┘└────┘  └────┘└────┘

    Rule: Max parallelism = number of partitions


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     REBALANCING PROTOCOL                                         │
└─────────────────────────────────────────────────────────────────────────────────┘

    Consumer joins/leaves -> Rebalance triggered

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  1. JoinGroup Request (all consumers)                                       │
    │     ┌────────────────────────────────────────────────────────────┐          │
    │     │                                                            │          │
    │     │  C1 ────> Coordinator: "I want to join group X"           │          │
    │     │  C2 ────> Coordinator: "I want to join group X"           │          │
    │     │  C3 ────> Coordinator: "I want to join group X"           │          │
    │     │                                                            │          │
    │     └────────────────────────────────────────────────────────────┘          │
    │                                                                             │
    │  2. Leader Election (Coordinator picks one consumer as leader)              │
    │     ┌────────────────────────────────────────────────────────────┐          │
    │     │                                                            │          │
    │     │  Coordinator ────> C1: "You are leader, here's member list"│          │
    │     │  Coordinator ────> C2: "Wait for assignment"               │          │
    │     │  Coordinator ────> C3: "Wait for assignment"               │          │
    │     │                                                            │          │
    │     └────────────────────────────────────────────────────────────┘          │
    │                                                                             │
    │  3. SyncGroup (Leader computes and sends assignment)                        │
    │     ┌────────────────────────────────────────────────────────────┐          │
    │     │                                                            │          │
    │     │  C1 (leader) ────> Coordinator: "Here's assignment plan"   │          │
    │     │                                                            │          │
    │     │     Assignment: C1=[P0], C2=[P1], C3=[P2,P3]              │          │
    │     │                                                            │          │
    │     └────────────────────────────────────────────────────────────┘          │
    │                                                                             │
    │  4. Assignment Distribution                                                 │
    │     ┌────────────────────────────────────────────────────────────┐          │
    │     │                                                            │          │
    │     │  Coordinator ────> C1: "Your partitions: [P0]"            │          │
    │     │  Coordinator ────> C2: "Your partitions: [P1]"            │          │
    │     │  Coordinator ────> C3: "Your partitions: [P2,P3]"         │          │
    │     │                                                            │          │
    │     └────────────────────────────────────────────────────────────┘          │
    └─────────────────────────────────────────────────────────────────────────────┘
```

---

## 1. Kafka's Fundamental Design Philosophy

### 1.1 Logs as the Primary Abstraction

The log is Kafka's core data structure:

```c
// Conceptually, a partition is:
struct partition_log {
    int64_t  base_offset;      // Starting offset
    segment  segments[];       // On-disk segments
    int64_t  high_watermark;   // Last replicated offset
    int64_t  log_end_offset;   // Next offset to write
};
```

**Why logs?**
- **Simple**: Only append, never modify
- **Fast**: Sequential I/O
- **Replayable**: Any offset accessible
- **Scalable**: Partitions parallelize logs

### 1.2 Immutable Data and Append-Only Writes

```
Traditional DB:          Kafka Log:
┌───────────────┐        ┌───────────────────────────────┐
│ id │ value    │        │ offset │ timestamp │ value   │
├────┼──────────┤        ├────────┼───────────┼─────────┤
│ 1  │ "new"    │        │ 0      │ t0        │ "old"   │
│ 2  │ "..."    │        │ 1      │ t1        │ "new"   │
└────┴──────────┘        │ 2      │ t2        │ "newer" │
                         └────────┴───────────┴─────────┘
(overwrites)             (appends all versions)
```

**Benefits:**
- No locking for concurrent reads
- Replication is just copying bytes
- Time-travel via offset
- Compaction reclaims space (optional)

### 1.3 Partitioning as the Unit of Parallelism

```
Topic with 4 partitions = 4 independent logs

┌─────────────────────────────────────────────────────────────┐
│  Producer decides partition:                                │
│                                                             │
│  if (key != NULL)                                          │
│      partition = hash(key) % num_partitions;               │
│  else                                                       │
│      partition = round_robin() % num_partitions;           │
│                                                             │
│  Same key → Same partition → Ordered processing            │
└─────────────────────────────────────────────────────────────┘
```

### 1.4 Shifting Complexity from Brokers to Clients

| Traditional Queue | Kafka |
|-------------------|-------|
| Broker tracks message state | Client tracks offset |
| Broker manages redelivery | Client handles retries |
| Broker does routing | Client selects partition |
| Broker ensures ordering | Client uses same partition for ordering |

**Why?**
- Brokers stay simple and fast
- Clients can be smart (batching, compression)
- Horizontal broker scaling easier

---

## 2. Core Architectural Components

### 2.1 Producer

**Responsibilities:**
- Serialize messages
- Select partition (by key hash or round-robin)
- Batch messages for efficiency
- Retry transient failures
- Handle delivery acknowledgments

**Does NOT:**
- Store messages persistently
- Guarantee delivery without proper config
- Know consumer state

### 2.2 Consumer

**Responsibilities:**
- Subscribe to topics
- Fetch messages in batches
- Track consumed offset
- Commit offsets (manually or auto)
- Handle partition rebalancing

**Does NOT:**
- Delete messages from broker
- Coordinate directly with other consumers
- Know producer state

### 2.3 Broker

**Responsibilities:**
- Accept produce requests
- Store messages in logs
- Serve fetch requests
- Replicate to followers
- Manage segment files

**Does NOT:**
- Route messages to consumers (pull model)
- Track consumer offsets directly (stored in topic)
- Transform messages

### 2.4 Topic

**Definition:** Named stream of records
- Logical grouping
- Configuration boundary (retention, compaction)
- Access control unit

### 2.5 Partition

**Definition:** Ordered, immutable sequence of records
- Unit of parallelism
- Unit of replication
- Each has one leader

### 2.6 Segment

**Definition:** Physical file containing portion of partition

```
partition-0/
├── 00000000000000000000.log    # Messages 0-999
├── 00000000000000000000.index  # Offset index
├── 00000000000000001000.log    # Messages 1000-1999
├── 00000000000000001000.index
└── ...
```

### 2.7 Replica (Leader / Follower)

```
Partition P0 (replication factor = 3)

Broker 1          Broker 2          Broker 3
┌─────────┐       ┌─────────┐       ┌─────────┐
│ P0      │       │ P0      │       │ P0      │
│ LEADER  │ ───── │ follower │ ───── │ follower │
│         │  sync │         │  sync │         │
└─────────┘       └─────────┘       └─────────┘
    │
    └── All writes go here
        Reads can go to any ISR member
```

### 2.8 Controller

**Responsibilities:**
- Elect partition leaders
- Manage cluster membership
- Handle broker failures
- Propagate metadata

### 2.9 Zookeeper / KRaft

**Zookeeper (legacy):**
- Stores cluster metadata
- Manages controller election
- Tracks broker liveness

**KRaft (new):**
- Metadata stored in Kafka itself
- No external dependency
- Better scaling

---

## 3. Data Flow Model

### 3.1 Producer → Broker → Disk → Consumer

```
Producer                 Broker                  Consumer
────────                 ──────                  ────────
    │                       │                       │
    │  ProduceRequest       │                       │
    │ ──────────────────>   │                       │
    │  [batch of messages]  │                       │
    │                       │                       │
    │                       │ append to log         │
    │                       │ (page cache)          │
    │                       │                       │
    │  ProduceResponse      │                       │
    │ <──────────────────   │                       │
    │  [acks: offset]       │                       │
    │                       │                       │
    │                       │                       │  FetchRequest
    │                       │ <─────────────────────│  [offset, max_bytes]
    │                       │                       │
    │                       │ read from log         │
    │                       │ (zero-copy sendfile)  │
    │                       │                       │
    │                       │  FetchResponse        │
    │                       │ ─────────────────────>│
    │                       │  [messages]           │
```

### 3.2 Batch-Based Message Production

```c
// librdkafka batching (simplified)
struct rd_kafka_msgbatch {
    rd_kafka_toppar_t  *toppar;      // Target partition
    rd_kafka_msg_t     *msgs;        // Message queue
    size_t              msg_cnt;     // Messages in batch
    size_t              bytes;       // Total size
    int64_t             first_ts;    // First message timestamp
};

// Batch triggers:
// 1. batch.size reached
// 2. linger.ms elapsed
// 3. flush() called
```

### 3.3 Pull-Based Consumption

```
Consumer controls:
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  fetch_min_bytes:  Don't return until at least N bytes available   │
│  fetch_max_wait:   Maximum time to wait                            │
│  max_partition_fetch_bytes: Cap data per partition                 │
│                                                                     │
│  Consumer can:                                                      │
│  - Process at own pace                                             │
│  - Batch fetches for efficiency                                    │
│  - Pause partitions under load                                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.4 Offset Management and Checkpointing

```
Offset Storage Options:
══════════════════════

1. __consumer_offsets topic (default)
   - Kafka manages internally
   - Replicated and durable
   - Committed via protocol

2. External storage
   - Application commits to database
   - Enables exactly-once with external systems
   - Application responsibility

Commit Strategies:
─────────────────

Auto-commit:
┌─────────────────────────────────────────────────────────────┐
│  enable.auto.commit = true                                  │
│  auto.commit.interval.ms = 5000                            │
│                                                             │
│  Risk: Commits offset before processing complete           │
│        → at-most-once semantics                            │
└─────────────────────────────────────────────────────────────┘

Manual commit (sync):
┌─────────────────────────────────────────────────────────────┐
│  while (running) {                                          │
│      msgs = poll();                                        │
│      process(msgs);                                        │
│      commit_sync();  // Blocks until ack                   │
│  }                                                          │
│                                                             │
│  Risk: Slow but safe (at-least-once)                       │
└─────────────────────────────────────────────────────────────┘

Manual commit (async):
┌─────────────────────────────────────────────────────────────┐
│  while (running) {                                          │
│      msgs = poll();                                        │
│      process(msgs);                                        │
│      commit_async(callback);  // Non-blocking              │
│  }                                                          │
│                                                             │
│  Risk: Must handle callback failures                       │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Delivery Semantics

### 4.1 At-Most-Once

```c
// Producer config
rd_kafka_conf_set(conf, "acks", "0", ...);  // No ack wait

// Message may be lost if:
// - Network error before broker receives
// - Broker crashes before persisting
// - No retries configured
```

**Use case:** Metrics, logs where loss is acceptable

### 4.2 At-Least-Once

```c
// Producer config
rd_kafka_conf_set(conf, "acks", "all", ...);
rd_kafka_conf_set(conf, "retries", "10", ...);
rd_kafka_conf_set(conf, "retry.backoff.ms", "100", ...);

// Message may be duplicated if:
// - Broker acks but ack lost in network
// - Producer retries → duplicate
```

**Use case:** Critical events where duplicates can be handled

### 4.3 Exactly-Once (Idempotent Producer + Transactions)

```c
// Idempotent producer (dedup at broker)
rd_kafka_conf_set(conf, "enable.idempotence", "true", ...);
// Broker assigns PID and tracks sequence numbers

// Transactional producer (atomic batches)
rd_kafka_conf_set(conf, "transactional.id", "my-txn-id", ...);

rd_kafka_init_transactions(rk, timeout);

rd_kafka_begin_transaction(rk);
rd_kafka_producev(rk, ...);  // Multiple messages
rd_kafka_producev(rk, ...);
rd_kafka_commit_transaction(rk, timeout);
// All or nothing
```

**Trade-offs:**
- Higher latency (transaction overhead)
- Lower throughput
- More complex error handling

---

## 5. Concurrency and Parallelism Model

### 5.1 Partitions as Parallelism Units

```
Max Consumer Parallelism = Number of Partitions

Topic: orders (8 partitions)
──────────────────────────

Consumer Group with 4 consumers:
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ P0 │ P1 │ P2 │ P3 │ P4 │ P5 │ P6 │ P7 │
└─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┴─┬──┘
  │    │    │    │    │    │    │    │
  └──┬─┘    └──┬─┘    └──┬─┘    └──┬─┘
     │         │         │         │
     v         v         v         v
  ┌────┐    ┌────┐    ┌────┐    ┌────┐
  │ C1 │    │ C2 │    │ C3 │    │ C4 │
  │P0,P1│   │P2,P3│   │P4,P5│   │P6,P7│
  └────┘    └────┘    └────┘    └────┘
```

### 5.2 Consumer Groups and Rebalancing

**Triggers for rebalance:**
- Consumer joins group
- Consumer leaves (crash or shutdown)
- Partition count changes
- Consumer heartbeat timeout

**Rebalance protocol:**
1. **Eager (stop-the-world):** All consumers release partitions, then reassign
2. **Cooperative (incremental):** Only affected partitions reassigned

### 5.3 Threading Models in Clients (librdkafka)

```
librdkafka Threading Model:
══════════════════════════

┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION THREAD                          │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  rd_kafka_produce() / rd_kafka_consumer_poll()          │   │
│  └─────────────────────────────────────────────────────────┘   │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                v
┌─────────────────────────────────────────────────────────────────┐
│                     librdkafka INTERNAL                         │
│                                                                 │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │  Main       │  │  Broker     │  │  Background             │ │
│  │  Thread     │  │  Threads    │  │  Thread                 │ │
│  │             │  │  (per conn) │  │                         │ │
│  │  - Events   │  │  - I/O      │  │  - Metadata refresh     │ │
│  │  - Timers   │  │  - Protocol │  │  - Statistics           │ │
│  │  - Callbacks│  │  - Batching │  │  - Log cleanup          │ │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 5.4 Broker-Side Request Handling

```
Broker Network Architecture:
═══════════════════════════

                    ┌───────────────────────────┐
                    │     Acceptor Thread       │
                    │   (accept connections)    │
                    └───────────────┬───────────┘
                                    │
                    ┌───────────────┴───────────┐
                    │                           │
           ┌────────▼────────┐        ┌────────▼────────┐
           │  Network Thread │        │  Network Thread │
           │   (selector)    │        │   (selector)    │
           └────────┬────────┘        └────────┬────────┘
                    │                          │
                    v                          v
           ┌─────────────────────────────────────────────┐
           │            Request Queue                    │
           └─────────────────────────────────────────────┘
                              │
           ┌──────────────────┼──────────────────┐
           │                  │                  │
           v                  v                  v
    ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │ I/O Thread 1 │  │ I/O Thread 2 │  │ I/O Thread N │
    │  (handler)   │  │  (handler)   │  │  (handler)   │
    └──────────────┘  └──────────────┘  └──────────────┘
           │                  │                  │
           v                  v                  v
    ┌───────────────────────────────────────────────────┐
    │              Log Manager (writes)                 │
    │              Fetch Manager (reads)                │
    └───────────────────────────────────────────────────┘
```

---

## 中文解释 (Chinese Explanations)

### 1. Kafka 的基本设计哲学

**日志作为主要抽象：**
- 日志是 Kafka 的核心数据结构
- 只追加，从不修改
- 顺序 I/O 性能极高
- 任何偏移量都可访问
- 分区将日志并行化

**不可变数据和追加写入：**
- 传统数据库覆盖旧值
- Kafka 追加所有版本
- 无需锁定即可并发读取
- 复制就是简单的字节复制
- 通过偏移量实现"时间旅行"

**分区作为并行单元：**
- 每个主题有多个分区
- 相同键哈希到相同分区
- 相同分区内消息有序
- 消费者并行度 = 分区数

**复杂性从 Broker 转移到客户端：**
- Broker 追踪消息状态 → 客户端追踪偏移量
- Broker 管理重试 → 客户端处理重试
- Broker 路由消息 → 客户端选择分区

### 2. 核心架构组件

**生产者职责：**
- 序列化消息
- 选择分区
- 批量发送
- 重试瞬时故障
- 处理投递确认

**消费者职责：**
- 订阅主题
- 批量获取消息
- 跟踪消费偏移量
- 提交偏移量
- 处理分区再平衡

**Broker 职责：**
- 接受生产请求
- 将消息存储在日志中
- 服务获取请求
- 复制到 follower
- 管理段文件

**分区：**
- 有序、不可变的记录序列
- 并行单元
- 复制单元
- 每个分区有一个 leader

### 3. 数据流模型

**生产者 → Broker → 磁盘 → 消费者：**
- 生产者发送 ProduceRequest（消息批次）
- Broker 追加到日志（页面缓存）
- 消费者发送 FetchRequest（偏移量）
- Broker 从日志读取（零拷贝）

**批量消息生产：**
- batch.size 达到触发发送
- linger.ms 超时触发发送
- flush() 调用触发发送

**拉取式消费：**
- 消费者控制速率
- 可以批量获取提高效率
- 可以在负载下暂停分区

**偏移量管理：**
- 自动提交：简单但可能丢数据
- 手动同步提交：慢但安全
- 手动异步提交：需处理回调失败

### 4. 投递语义

**至多一次（At-Most-Once）：**
- acks=0，不等待确认
- 消息可能丢失
- 适用于指标、日志

**至少一次（At-Least-Once）：**
- acks=all + 重试
- 消息可能重复
- 适用于关键事件

**精确一次（Exactly-Once）：**
- 幂等生产者 + 事务
- 延迟更高
- 复杂度更高
- 适用于金融等场景

### 5. 并发和并行模型

**分区作为并行单元：**
- 最大消费者并行度 = 分区数
- 超过分区数的消费者会闲置

**消费组再平衡触发条件：**
- 消费者加入组
- 消费者离开（崩溃或关闭）
- 分区数变化
- 心跳超时

**librdkafka 线程模型：**
- 主线程：事件、定时器、回调
- Broker 线程：I/O、协议、批处理
- 后台线程：元数据刷新、统计
