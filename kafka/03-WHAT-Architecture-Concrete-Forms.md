# WHAT | Architecture and Concrete Forms

## Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                     librdkafka OBJECT HIERARCHY                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

    Application Layer                   librdkafka Internal Layer
    =================                   =========================

    ┌────────────────────┐
    │   rd_kafka_conf_t  │ ─────────────────────────────────────────────────────┐
    │   (Configuration)  │                                                       │
    └─────────┬──────────┘                                                       │
              │ creates                                                          │
              v                                                                  │
    ┌────────────────────┐                                                       │
    │    rd_kafka_t      │  ══════════════════════════════════════════════════╗ │
    │  (Producer/Consumer│                                                    ║ │
    │     Handle)        │      Internal Structures                           ║ │
    │                    │      ────────────────────                           ║ │
    │  ┌──────────────┐  │      ┌──────────────────────────────────────────┐  ║ │
    │  │ rd_kafka_    │  │      │  rd_kafka_broker_t (per broker)          │  ║ │
    │  │ queue_t      │  │      │  ├── Connection state                    │  ║ │
    │  │ (Event Queue)│  │      │  ├── I/O buffers                         │  ║ │
    │  └──────────────┘  │      │  ├── Request queue                       │  ║ │
    │                    │      │  └── Thread handle                        │  ║ │
    │  ┌──────────────┐  │      └──────────────────────────────────────────┘  ║ │
    │  │ rd_kafka_    │  │                                                    ║ │
    │  │ topic_t      │  │      ┌──────────────────────────────────────────┐  ║ │
    │  │ (Topic Ref)  │──┼─────>│  rd_kafka_itopic_t (internal topic)      │  ║ │
    │  └──────────────┘  │      │  ├── Partition list                      │  ║ │
    │                    │      │  ├── Configuration                        │  ║ │
    │                    │      │  └── Metadata                             │  ║ │
    └────────────────────┘      └──────────────────────────────────────────┘  ║ │
                                                                              ║ │
                                ┌──────────────────────────────────────────┐  ║ │
                                │  rd_kafka_toppar_t (topic-partition)     │  ║ │
                                │  ├── Broker assignment                   │  ║ │
                                │  ├── Message queue (producer)            │  ║ │
                                │  ├── Fetch state (consumer)              │  ║ │
                                │  └── Offset tracking                      │  ║ │
                                └──────────────────────────────────────────┘  ║ │
                                                                              ║ │
    ┌────────────────────┐      ┌──────────────────────────────────────────┐  ║ │
    │ rd_kafka_message_t │      │  rd_kafka_msg_t (internal message)       │  ║ │
    │ (Public Message)   │<─────│  ├── Payload + Key                       │  ║ │
    │                    │      │  ├── Headers                              │  ║ │
    │  - payload         │      │  ├── Timestamp                            │  ║ │
    │  - len             │      │  ├── Partition hint                       │  ║ │
    │  - key             │      │  └── Delivery callback                    │  ║ │
    │  - err             │      └──────────────────────────────────────────┘  ║ │
    │  - offset          │                                                    ║ │
    │  - partition       │                                                    ║ │
    └────────────────────┘ ════════════════════════════════════════════════════╝ │


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     PRODUCER MESSAGE LIFECYCLE                                   │
└─────────────────────────────────────────────────────────────────────────────────┘

                        APPLICATION
                            │
                            │ rd_kafka_producev()
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  1. MESSAGE CREATION                                                        │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  rd_kafka_msg_t created with:                                          │ │
    │  │  - Payload copied (if RD_KAFKA_MSG_F_COPY)                             │ │
    │  │  - Key serialized                                                      │ │
    │  │  - Headers attached                                                    │ │
    │  │  - Timestamp set (or use broker time)                                  │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  2. PARTITION SELECTION                                                     │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  if (key != NULL)                                                      │ │
    │  │      partition = partitioner(key) % partition_cnt;                     │ │
    │  │  else if (partition == RD_KAFKA_PARTITION_UA)                          │ │
    │  │      partition = round_robin_counter++ % partition_cnt;                │ │
    │  │  else                                                                   │ │
    │  │      use specified partition;                                          │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  3. ENQUEUE TO PARTITION                                                    │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  rd_kafka_toppar_t *rktp = get_toppar(topic, partition);               │ │
    │  │  enqueue(rktp->rktp_msgq, msg);                                        │ │
    │  │                                                                         │ │
    │  │  Partition Queue (FIFO):                                               │ │
    │  │  ┌─────┬─────┬─────┬─────┬─────┐                                       │ │
    │  │  │msg1 │msg2 │msg3 │msg4 │ NEW │                                       │ │
    │  │  └─────┴─────┴─────┴─────┴─────┘                                       │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            │ (background sender thread)
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  4. BATCH FORMATION                                                         │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  Batch formed when:                                                    │ │
    │  │  - batch.size bytes accumulated, OR                                    │ │
    │  │  - linger.ms elapsed since first message, OR                          │ │
    │  │  - rd_kafka_flush() called                                            │ │
    │  │                                                                         │ │
    │  │  ┌──────────────────────────────────────────┐                          │ │
    │  │  │  ProduceRequest (batch)                  │                          │ │
    │  │  │  ├── topic: "orders"                     │                          │ │
    │  │  │  ├── partition: 0                        │                          │ │
    │  │  │  ├── messages: [msg1, msg2, msg3, ...]   │                          │ │
    │  │  │  └── compression: snappy                 │                          │ │
    │  │  └──────────────────────────────────────────┘                          │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            │ TCP to leader broker
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  5. BROKER PROCESSING                                                       │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  Broker receives ProduceRequest:                                       │ │
    │  │  1. Validate (auth, topic exists, partition valid)                     │ │
    │  │  2. Append to leader's log                                             │ │
    │  │  3. Wait for ISR replication (if acks=all)                            │ │
    │  │  4. Send ProduceResponse with base_offset                              │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  6. DELIVERY REPORT CALLBACK                                                │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  dr_msg_cb(rk, rkmessage, opaque) {                                    │ │
    │  │      if (rkmessage->err)                                               │ │
    │  │          handle_error();                                               │ │
    │  │      else                                                               │ │
    │  │          success(rkmessage->offset, rkmessage->partition);             │ │
    │  │  }                                                                      │ │
    │  │                                                                         │ │
    │  │  Called from: rd_kafka_poll() or rd_kafka_flush()                      │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     CONSUMER MESSAGE LIFECYCLE                                   │
└─────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  1. SUBSCRIPTION / ASSIGNMENT                                               │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  Option A: Subscribe (automatic assignment)                            │ │
    │  │  ──────────────────────────────────────────                            │ │
    │  │  rd_kafka_subscribe(rk, topics);                                       │ │
    │  │  // Consumer group coordinator assigns partitions                      │ │
    │  │                                                                         │ │
    │  │  Option B: Assign (manual assignment)                                  │ │
    │  │  ────────────────────────────────────                                  │ │
    │  │  rd_kafka_assign(rk, partitions);                                      │ │
    │  │  // Application specifies exact partitions                             │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  2. JOIN GROUP & REBALANCE (if subscribed)                                  │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │                                                                         │ │
    │  │  Consumer ──> Coordinator: JoinGroupRequest                            │ │
    │  │  Coordinator ──> Consumer: JoinGroupResponse (assignment)              │ │
    │  │  Consumer ──> Coordinator: SyncGroupRequest                            │ │
    │  │  Coordinator ──> Consumer: SyncGroupResponse (partitions)              │ │
    │  │                                                                         │ │
    │  │  Rebalance Callback (if registered):                                   │ │
    │  │  ┌────────────────────────────────────────┐                            │ │
    │  │  │  rebalance_cb(rk, err, partitions) {   │                            │ │
    │  │  │      if (err == ASSIGN)                │                            │ │
    │  │  │          // Prepare for new partitions │                            │ │
    │  │  │      else if (err == REVOKE)           │                            │ │
    │  │  │          // Commit offsets, cleanup    │                            │ │
    │  │  │  }                                     │                            │ │
    │  │  └────────────────────────────────────────┘                            │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  3. FETCH LOOP                                                              │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  For each assigned partition:                                          │ │
    │  │                                                                         │ │
    │  │  ┌────────────────────────────────────────────────────────────┐        │ │
    │  │  │  FetchRequest                                              │        │ │
    │  │  │  ├── partition: P0                                         │        │ │
    │  │  │  ├── fetch_offset: 1000                                    │        │ │
    │  │  │  ├── max_bytes: 1048576                                    │        │ │
    │  │  │  └── max_wait_ms: 500                                      │        │ │
    │  │  └────────────────────────────────────────────────────────────┘        │ │
    │  │                           │                                             │ │
    │  │                           v                                             │ │
    │  │  ┌────────────────────────────────────────────────────────────┐        │ │
    │  │  │  FetchResponse                                             │        │ │
    │  │  │  ├── partition: P0                                         │        │ │
    │  │  │  ├── high_watermark: 1500                                  │        │ │
    │  │  │  └── messages: [{offset:1000}, {offset:1001}, ...]         │        │ │
    │  │  └────────────────────────────────────────────────────────────┘        │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  4. MESSAGE DELIVERY TO APPLICATION                                         │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  rd_kafka_message_t *msg = rd_kafka_consumer_poll(rk, timeout_ms);     │ │
    │  │                                                                         │ │
    │  │  if (msg) {                                                            │ │
    │  │      if (msg->err) {                                                   │ │
    │  │          // Handle error (partition EOF, etc.)                         │ │
    │  │      } else {                                                          │ │
    │  │          // Process message                                            │ │
    │  │          process(msg->payload, msg->len);                              │ │
    │  │      }                                                                  │ │
    │  │      rd_kafka_message_destroy(msg);                                    │ │
    │  │  }                                                                      │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘
                            │
                            v
    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  5. OFFSET COMMIT                                                           │
    │  ┌────────────────────────────────────────────────────────────────────────┐ │
    │  │  Auto-commit (background):                                             │ │
    │  │  ─────────────────────────                                             │ │
    │  │  enable.auto.commit=true + auto.commit.interval.ms                     │ │
    │  │                                                                         │ │
    │  │  Manual commit:                                                        │ │
    │  │  ──────────────                                                        │ │
    │  │  rd_kafka_commit(rk, NULL, async);           // commit current         │ │
    │  │  rd_kafka_commit_message(rk, msg, async);    // commit specific        │ │
    │  │                                                                         │ │
    │  │  Stored in: __consumer_offsets topic                                   │ │
    │  │  ┌────────────────────────────────────────────────────────────┐        │ │
    │  │  │  Key: (group_id, topic, partition)                         │        │ │
    │  │  │  Value: (offset, metadata, timestamp)                      │        │ │
    │  │  └────────────────────────────────────────────────────────────┘        │ │
    │  └────────────────────────────────────────────────────────────────────────┘ │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     CORE ARCHITECTURAL PATTERNS                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

    DISTRIBUTED APPEND-ONLY LOG
    ═══════════════════════════

    Time ────────────────────────────────────────────────────────>

    ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
    │  0  │  1  │  2  │  3  │  4  │  5  │  6  │  7  │  8  │ ... │  ──> append
    └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
      │                                                     │
      │ Segment 0 (00000000000000000000.log)               │
      │                                                     │
      └──────────────── Segment files on disk ─────────────┘

    Properties:
    • Write: O(1) - always append
    • Read by offset: O(1) - index lookup
    • Sequential I/O: Disk-friendly
    • Immutable: No locking needed


    LEADER-FOLLOWER REPLICATION
    ═══════════════════════════

    Partition P0 (replication factor = 3)

    ┌─────────────────────────────────────────────────────────────────────────┐
    │                                                                         │
    │   Broker 1 (Leader)        Broker 2 (Follower)    Broker 3 (Follower)  │
    │   ┌─────────────────┐      ┌─────────────────┐    ┌─────────────────┐  │
    │   │ Log: [0,1,2,3,4]│─────>│ Log: [0,1,2,3,4]│───>│ Log: [0,1,2,3,4]│  │
    │   └────────┬────────┘      └─────────────────┘    └─────────────────┘  │
    │            │                        │                      │           │
    │            │                        │                      │           │
    │        Writes go                Followers             All in ISR       │
    │        here only                fetch from            (In-Sync         │
    │                                 leader                 Replicas)       │
    │                                                                         │
    └─────────────────────────────────────────────────────────────────────────┘

    Write Path:
    1. Producer writes to leader
    2. Leader appends to local log
    3. Followers fetch and replicate
    4. Leader tracks ISR membership
    5. acks=all waits for ISR ack


    ZERO-COPY / PAGE CACHE
    ══════════════════════

    Traditional Copy Path:                Zero-Copy (sendfile):
    ─────────────────────                 ─────────────────────

    ┌──────────────────┐                 ┌──────────────────┐
    │   Disk (file)    │                 │   Disk (file)    │
    └────────┬─────────┘                 └────────┬─────────┘
             │ read()                             │
             v                                    │ sendfile()
    ┌──────────────────┐                         │ (DMA)
    │  Kernel Buffer   │                         │
    └────────┬─────────┘                         │
             │ copy                              │
             v                                    │
    ┌──────────────────┐                         │
    │  User Buffer     │                         │
    └────────┬─────────┘                         │
             │ copy                              │
             v                                    v
    ┌──────────────────┐                 ┌──────────────────┐
    │  Kernel Buffer   │                 │  Socket Buffer   │
    └────────┬─────────┘                 └────────┬─────────┘
             │ send()                             │ send()
             v                                    v
    ┌──────────────────┐                 ┌──────────────────┐
    │    Network       │                 │    Network       │
    └──────────────────┘                 └──────────────────┘

    4 copies, 4 context switches         2 copies, 2 context switches
```

---

## 1. Core Architectural Patterns

### 1.1 Distributed Append-Only Log

The fundamental pattern Kafka builds upon:

```c
// Conceptual log structure
struct log_segment {
    int64_t base_offset;    // First offset in segment
    FILE    *data_file;     // .log file
    FILE    *index_file;    // .index file (offset -> position)
    FILE    *timeindex;     // .timeindex file (timestamp -> offset)
};

struct partition_log {
    log_segment *segments[];    // Array of segments
    int64_t     active_segment; // Currently writable segment
    int64_t     log_end_offset; // Next offset to assign
};

// Append operation: O(1)
int64_t append(partition_log *log, message *msg) {
    msg->offset = log->log_end_offset++;
    write(log->segments[active]->data_file, msg);
    return msg->offset;
}
```

### 1.2 Leader-Follower Replication

```
Replication States:
├── LEADER: Handles all reads/writes for partition
├── FOLLOWER: Replicates from leader, can become leader
└── ISR (In-Sync Replicas): Followers caught up with leader

High Watermark: Last offset replicated to all ISR members
                (consumers can only read up to this point)
```

### 1.3 Zero-Copy / Page Cache Usage

Kafka leverages OS-level optimizations:

```c
// sendfile() system call (Linux)
// Transfers data from file to socket without user-space copy
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

// Kafka uses this for fetch responses
// Data goes: Disk -> Page Cache -> Socket (no application copy)
```

### 1.4 State Machines for Coordination

```
Consumer Group State Machine:
─────────────────────────────

    ┌──────────────┐
    │    EMPTY     │ <─────────────────────────────┐
    └──────┬───────┘                               │
           │ member joins                          │
           v                                       │
    ┌──────────────┐                               │
    │  PREPARING   │                               │
    │  REBALANCE   │                               │
    └──────┬───────┘                               │
           │ all members joined                    │
           v                                       │
    ┌──────────────┐                               │
    │ COMPLETING   │                               │ timeout /
    │  REBALANCE   │                               │ all leave
    └──────┬───────┘                               │
           │ sync complete                         │
           v                                       │
    ┌──────────────┐   member joins/leaves   ┌────┴────┐
    │    STABLE    │ ───────────────────────>│REBALANCE│
    └──────────────┘                         └─────────┘
```

### 1.5 Protocol-Driven Design

All Kafka communication uses well-defined request/response protocols:

```
Request Types:
├── Produce (API Key: 0)
├── Fetch (API Key: 1)
├── ListOffsets (API Key: 2)
├── Metadata (API Key: 3)
├── OffsetCommit (API Key: 8)
├── OffsetFetch (API Key: 9)
├── FindCoordinator (API Key: 10)
├── JoinGroup (API Key: 11)
├── Heartbeat (API Key: 12)
├── LeaveGroup (API Key: 13)
├── SyncGroup (API Key: 14)
└── ... (40+ request types)
```

---

## 2. Key librdkafka Objects and Their Roles

### 2.1 `rd_kafka_t` - Client Handle

**What it is:** The main handle for producer or consumer instance

**Why it exists:**
- Single entry point for all operations
- Owns all internal resources (threads, connections, queues)
- Manages client lifecycle

```c
// Creation
rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));

// Key responsibilities:
// - Broker connection management
// - Background thread coordination
// - Event/callback dispatching
// - Metadata caching

// Destruction (blocks until cleanup complete)
rd_kafka_destroy(rk);
```

### 2.2 `rd_kafka_conf_t` - Configuration Object

**What it is:** Builder for configuration before client creation

**Why it exists:**
- Separates configuration phase from runtime
- Allows validation before committing resources
- Configuration is immutable after client creation

```c
rd_kafka_conf_t *conf = rd_kafka_conf_new();

// Set properties
rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:9092", NULL, 0);
rd_kafka_conf_set(conf, "acks", "all", NULL, 0);

// Set callbacks
rd_kafka_conf_set_dr_msg_cb(conf, delivery_report_cb);
rd_kafka_conf_set_error_cb(conf, error_cb);

// conf is CONSUMED by rd_kafka_new() - do not use after
rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, ...);
```

### 2.3 `rd_kafka_topic_t` - Topic Handle

**What it is:** Reference to a topic for produce operations

**Why it exists:**
- Caches topic metadata
- Associates topic-specific configuration
- Required for legacy produce API

```c
rd_kafka_topic_t *rkt = rd_kafka_topic_new(rk, "my-topic", topic_conf);

// Used with rd_kafka_produce() (legacy API)
rd_kafka_produce(rkt, partition, flags, payload, len, key, key_len, opaque);

// Note: rd_kafka_producev() doesn't require topic handle
rd_kafka_producev(rk, RD_KAFKA_V_TOPIC("my-topic"), ...);

rd_kafka_topic_destroy(rkt);
```

### 2.4 `rd_kafka_topic_conf_t` - Topic Configuration

**What it is:** Configuration specific to a topic

**Why it exists:**
- Different topics may need different settings
- Overrides default configuration
- Consumed when creating topic handle

```c
rd_kafka_topic_conf_t *tconf = rd_kafka_topic_conf_new();
rd_kafka_topic_conf_set(tconf, "acks", "1", NULL, 0);

// tconf consumed by topic_new()
rd_kafka_topic_t *rkt = rd_kafka_topic_new(rk, "topic", tconf);
```

### 2.5 `rd_kafka_message_t` - Message Structure

**What it is:** Public-facing message representation

**Why it exists:**
- Clean interface for application code
- Contains all message metadata
- Used in both produce callbacks and consume returns

```c
typedef struct rd_kafka_message_s {
    rd_kafka_resp_err_t err;      // Error code (0 = success)
    rd_kafka_topic_t   *rkt;      // Topic handle
    int32_t             partition; // Partition number
    void               *payload;   // Message payload
    size_t              len;       // Payload length
    void               *key;       // Message key
    size_t              key_len;   // Key length
    int64_t             offset;    // Message offset
    void               *_private;  // Internal use (msg_opaque)
} rd_kafka_message_t;
```

### 2.6 `rd_kafka_queue_t` - Event Queue

**What it is:** Queue for events, messages, and callbacks

**Why it exists:**
- Decouples event generation from processing
- Enables custom event routing
- Allows polling multiple event sources

```c
// Default queues exist automatically
// Custom queues can be created:
rd_kafka_queue_t *queue = rd_kafka_queue_new(rk);

// Forward partition events to custom queue
rd_kafka_consume_start_queue(rkt, partition, offset, queue);

// Poll custom queue
rd_kafka_message_t *msg = rd_kafka_consume_queue(queue, timeout_ms);

rd_kafka_queue_destroy(queue);
```

### 2.7 `rd_kafka_resp_err_t` - Error Codes

**What it is:** Enumeration of all possible error conditions

**Why it exists:**
- Consistent error handling across API
- Maps to Kafka protocol errors
- Includes librdkafka-specific errors

```c
// Common error codes:
RD_KAFKA_RESP_ERR_NO_ERROR           // Success
RD_KAFKA_RESP_ERR__PARTITION_EOF     // End of partition (not error)
RD_KAFKA_RESP_ERR__TIMED_OUT         // Operation timed out
RD_KAFKA_RESP_ERR__QUEUE_FULL        // Internal queue full
RD_KAFKA_RESP_ERR__MSG_TIMED_OUT     // Message delivery timeout
RD_KAFKA_RESP_ERR_UNKNOWN_TOPIC_OR_PART  // Topic doesn't exist

// Convert to string
const char *errstr = rd_kafka_err2str(err);
```

---

## 3. End-to-End Message Lifecycle (C Perspective)

### 3.1 Producer Configuration

```c
rd_kafka_conf_t *conf = rd_kafka_conf_new();

// Essential settings
rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr, sizeof(errstr));

// Reliability settings
rd_kafka_conf_set(conf, "acks", "all", NULL, 0);           // Wait for all ISR
rd_kafka_conf_set(conf, "retries", "5", NULL, 0);          // Retry on failure
rd_kafka_conf_set(conf, "retry.backoff.ms", "100", NULL, 0);

// Performance settings
rd_kafka_conf_set(conf, "linger.ms", "5", NULL, 0);        // Batch delay
rd_kafka_conf_set(conf, "batch.size", "16384", NULL, 0);   // Batch size
rd_kafka_conf_set(conf, "compression.type", "snappy", NULL, 0);

// Callbacks
rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);  // Delivery reports
```

### 3.2 Message Batching

librdkafka batches messages automatically:

```
Configuration          Behavior
─────────────          ────────
linger.ms = 0          Send immediately (no batching delay)
linger.ms = 5          Wait up to 5ms to fill batch
batch.size = 16384     Max batch size in bytes
batch.num.messages     Max messages per batch
```

### 3.3 Partition Selection

```c
// Automatic (round-robin for null key, hash for keyed)
rd_kafka_producev(rk,
    RD_KAFKA_V_TOPIC(topic),
    RD_KAFKA_V_VALUE(payload, len),
    RD_KAFKA_V_KEY(key, key_len),  // Same key -> same partition
    RD_KAFKA_V_END);

// Manual partition selection
rd_kafka_producev(rk,
    RD_KAFKA_V_TOPIC(topic),
    RD_KAFKA_V_PARTITION(3),       // Explicit partition
    RD_KAFKA_V_VALUE(payload, len),
    RD_KAFKA_V_END);

// Custom partitioner
rd_kafka_topic_conf_set_partitioner_cb(tconf, my_partitioner);

int32_t my_partitioner(const rd_kafka_topic_t *rkt,
                       const void *keydata, size_t keylen,
                       int32_t partition_cnt,
                       void *rkt_opaque, void *msg_opaque) {
    // Return partition number 0 to partition_cnt-1
    return hash(keydata, keylen) % partition_cnt;
}
```

### 3.4 Send Request Creation

```c
// rd_kafka_producev returns immediately (async)
rd_kafka_resp_err_t err = rd_kafka_producev(rk,
    RD_KAFKA_V_TOPIC(topic),
    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),  // Copy payload
    RD_KAFKA_V_VALUE(payload, len),
    RD_KAFKA_V_KEY(key, key_len),
    RD_KAFKA_V_TIMESTAMP(timestamp_ms),        // Optional
    RD_KAFKA_V_HEADERS(headers),               // Optional
    RD_KAFKA_V_OPAQUE(my_context),             // For callback
    RD_KAFKA_V_END);

if (err) {
    // Immediate error (e.g., queue full)
    // Message NOT enqueued
}
// else: message enqueued, delivery report will follow
```

### 3.5 Broker Acknowledgment

```c
// Delivery callback (called from rd_kafka_poll)
static void dr_msg_cb(rd_kafka_t *rk,
                      const rd_kafka_message_t *rkmessage,
                      void *opaque) {
    if (rkmessage->err) {
        fprintf(stderr, "Delivery failed: %s\n",
                rd_kafka_err2str(rkmessage->err));
        // Retry logic or error handling
    } else {
        printf("Delivered to partition %d at offset %ld\n",
               rkmessage->partition, rkmessage->offset);
    }

    // Access per-message context
    void *msg_opaque = rkmessage->_private;
}
```

### 3.6 Retries and Error Handling

```c
// Retryable errors (librdkafka handles automatically):
// - RD_KAFKA_RESP_ERR__TRANSPORT
// - RD_KAFKA_RESP_ERR_REQUEST_TIMED_OUT
// - RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE
// - RD_KAFKA_RESP_ERR_NOT_LEADER_FOR_PARTITION

// Non-retryable errors:
// - RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE
// - RD_KAFKA_RESP_ERR_TOPIC_AUTHORIZATION_FAILED

// Handle in delivery callback
if (rd_kafka_error_is_retriable(err)) {
    // librdkafka already retried and gave up
    // Consider re-enqueuing
}
```

### 3.7 Consumer Fetch Loop

```c
// Subscribe to topics
rd_kafka_topic_partition_list_t *topics;
topics = rd_kafka_topic_partition_list_new(1);
rd_kafka_topic_partition_list_add(topics, "my-topic", RD_KAFKA_PARTITION_UA);
rd_kafka_subscribe(rk, topics);
rd_kafka_topic_partition_list_destroy(topics);

// Poll loop
while (running) {
    rd_kafka_message_t *msg = rd_kafka_consumer_poll(rk, 1000);

    if (msg == NULL)
        continue;  // Timeout, no message

    if (msg->err) {
        if (msg->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
            // End of partition (not error)
        } else {
            fprintf(stderr, "Consumer error: %s\n",
                    rd_kafka_message_errstr(msg));
        }
    } else {
        // Process message
        process(msg->payload, msg->len, msg->offset);
    }

    rd_kafka_message_destroy(msg);  // MUST destroy
}
```

### 3.8 Offset Commit and Recovery

```c
// Auto-commit (default, but risky)
rd_kafka_conf_set(conf, "enable.auto.commit", "true", NULL, 0);
rd_kafka_conf_set(conf, "auto.commit.interval.ms", "5000", NULL, 0);

// Manual commit (recommended)
rd_kafka_conf_set(conf, "enable.auto.commit", "false", NULL, 0);

// Sync commit (after processing)
rd_kafka_commit(rk, NULL, 0);  // Commit current positions

// Async commit with callback
rd_kafka_commit(rk, NULL, 1);  // Non-blocking

// Commit specific offset
rd_kafka_topic_partition_list_t *offsets;
offsets = rd_kafka_topic_partition_list_new(1);
rd_kafka_topic_partition_list_add(offsets, topic, partition);
offsets->elems[0].offset = next_offset;  // offset + 1
rd_kafka_commit(rk, offsets, 0);
rd_kafka_topic_partition_list_destroy(offsets);
```

---

## 4. librdkafka API Usage Patterns

### 4.1 Initialization and Teardown

```c
// Producer setup
rd_kafka_conf_t *conf = rd_kafka_conf_new();
// ... configure ...
rd_kafka_t *producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
if (!producer) {
    fprintf(stderr, "Failed: %s\n", errstr);
    exit(1);
}

// Consumer setup
rd_kafka_conf_t *cconf = rd_kafka_conf_new();
// ... configure ...
rd_kafka_t *consumer = rd_kafka_new(RD_KAFKA_CONSUMER, cconf, errstr, sizeof(errstr));
rd_kafka_poll_set_consumer(consumer);  // Enable consumer_poll

// Producer teardown
rd_kafka_flush(producer, 10000);  // Wait for delivery
rd_kafka_destroy(producer);       // Clean up

// Consumer teardown
rd_kafka_consumer_close(consumer);  // Commit offsets, leave group
rd_kafka_destroy(consumer);
```

### 4.2 Producer vs. Consumer APIs

```c
// PRODUCER APIs
rd_kafka_producev(rk, ...);           // Enqueue message (preferred)
rd_kafka_produce(rkt, ...);           // Legacy, requires topic handle
rd_kafka_poll(rk, timeout);           // Service callbacks
rd_kafka_flush(rk, timeout);          // Wait for all deliveries

// CONSUMER APIs
rd_kafka_subscribe(rk, topics);       // Subscribe to topics
rd_kafka_assign(rk, partitions);      // Manual assignment
rd_kafka_consumer_poll(rk, timeout);  // Fetch next message
rd_kafka_commit(rk, offsets, async);  // Commit offsets
rd_kafka_consumer_close(rk);          // Clean shutdown
```

### 4.3 Poll-Based Event Handling

```c
// Producer: must poll to trigger delivery reports
while (running) {
    rd_kafka_producev(rk, ...);

    // Poll to service delivery callbacks
    rd_kafka_poll(rk, 0);  // Non-blocking
}
rd_kafka_flush(rk, 10000);  // Final flush

// Consumer: poll returns messages directly
while (running) {
    rd_kafka_message_t *msg = rd_kafka_consumer_poll(rk, 1000);
    if (msg) {
        // Process...
        rd_kafka_message_destroy(msg);
    }
}
```

### 4.4 Callback-Driven Error and Delivery Reporting

```c
// Delivery report callback
static void dr_cb(rd_kafka_t *rk, const rd_kafka_message_t *msg, void *opaque) {
    if (msg->err)
        handle_delivery_failure(msg);
    else
        handle_delivery_success(msg);
}

// Error callback (global errors)
static void error_cb(rd_kafka_t *rk, int err, const char *reason, void *opaque) {
    fprintf(stderr, "Kafka error: %s: %s\n",
            rd_kafka_err2str(err), reason);
}

// Log callback (debug output)
static void log_cb(const rd_kafka_t *rk, int level,
                   const char *fac, const char *buf) {
    fprintf(stderr, "RDKAFKA-%i-%s: %s\n", level, fac, buf);
}

// Rebalance callback (consumer)
static void rebalance_cb(rd_kafka_t *rk, rd_kafka_resp_err_t err,
                         rd_kafka_topic_partition_list_t *partitions,
                         void *opaque) {
    switch (err) {
    case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
        rd_kafka_assign(rk, partitions);
        break;
    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
        rd_kafka_commit(rk, NULL, 0);  // Commit before revoke
        rd_kafka_assign(rk, NULL);
        break;
    }
}
```

### 4.5 Thread Safety Guarantees

```c
/*
 * Thread Safety Rules:
 *
 * SAFE from any thread:
 * - rd_kafka_producev() / rd_kafka_produce()
 * - rd_kafka_poll()
 * - rd_kafka_flush()
 * - rd_kafka_metadata()
 *
 * SAFE but use dedicated consumer thread:
 * - rd_kafka_consumer_poll()
 * - rd_kafka_commit()
 * - rd_kafka_subscribe() / rd_kafka_assign()
 *
 * NOT thread-safe:
 * - rd_kafka_conf_*() during rd_kafka_new()
 * - rd_kafka_topic_conf_*() during topic creation
 *
 * Callbacks:
 * - Called from librdkafka internal threads
 * - Must be thread-safe
 * - Should be fast (don't block)
 */
```

---

## 5. Costs and Boundaries

### 5.1 Latency vs. Throughput Trade-offs

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      LATENCY vs THROUGHPUT KNOBS                        │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  LOW LATENCY                          HIGH THROUGHPUT                   │
│  ──────────                           ───────────────                   │
│  linger.ms = 0                        linger.ms = 100                   │
│  batch.size = small                   batch.size = large                │
│  compression.type = none              compression.type = lz4/zstd       │
│  acks = 1                             acks = all                        │
│                                                                         │
│  Trade-off: More network round trips  Trade-off: Added latency          │
│             Lower efficiency                     Better efficiency      │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Memory Usage and Buffering

```c
// Key memory configurations

// Producer internal queue size
"queue.buffering.max.messages" = 100000    // Max queued messages
"queue.buffering.max.kbytes"   = 1048576   // Max queued KB

// Consumer fetch buffer
"queued.min.messages"        = 100000      // Min messages to fetch ahead
"queued.max.messages.kbytes" = 65536       // Max prefetch KB

// Message size limits
"message.max.bytes"          = 1000000     // Max message size

// Memory calculation:
// Producer: queue.buffering.max.kbytes + message copies in flight
// Consumer: queued.max.messages.kbytes * assigned_partitions
```

### 5.3 Broker vs. Client Responsibilities

```
┌───────────────────────────┬────────────────────────────────────────────┐
│      BROKER OWNS          │           CLIENT OWNS                      │
├───────────────────────────┼────────────────────────────────────────────┤
│ Message storage           │ Serialization/Deserialization              │
│ Replication               │ Partition selection (producer)             │
│ Segment management        │ Offset tracking (consumer)                 │
│ Leader election           │ Batching decisions                         │
│ Access control            │ Retry logic                                │
│ Quota enforcement         │ Consumer group coordination*               │
│ Metadata distribution     │ Error handling                             │
│                           │                                            │
│ *Coordinator runs on      │ *Client drives rebalance protocol          │
│  broker but client drives │                                            │
└───────────────────────────┴────────────────────────────────────────────┘
```

### 5.4 When Kafka is the Wrong Tool

```
❌ DON'T USE KAFKA FOR:

1. Request-Reply Patterns
   - Kafka is async by design
   - Use: HTTP, gRPC, message queues with reply-to

2. Very Low Latency (<10ms end-to-end)
   - Batching and replication add latency
   - Use: Direct network calls, shared memory

3. Small Scale (< 1000 msg/sec)
   - Operational overhead not justified
   - Use: Simple queues, direct calls

4. Database Replacement
   - No random access by key (except compacted topics)
   - No complex queries
   - Use: Actual database

5. Binary Large Objects (BLOBs)
   - 1MB default message limit
   - Inefficient for large payloads
   - Use: Object storage + reference in Kafka

6. Strong Consistency Requirements
   - Kafka is eventually consistent
   - Use: Consensus systems (Raft, Paxos)
```

---

## 中文解释 (Chinese Explanations)

### 1. 核心架构模式

**分布式追加日志：**
- 写入 O(1)：始终追加
- 按偏移量读取 O(1)：索引查找
- 顺序 I/O：对磁盘友好
- 不可变：无需锁定

**Leader-Follower 复制：**
- Leader 处理分区的所有读写
- Follower 从 Leader 复制
- ISR（同步副本集）：跟上 Leader 的 Follower
- High Watermark：已复制到所有 ISR 的最后偏移量

**零拷贝/页面缓存：**
- 传统方式：4次拷贝，4次上下文切换
- sendfile：2次拷贝，2次上下文切换
- 数据路径：磁盘 → 页面缓存 → Socket

### 2. 关键 librdkafka 对象

**rd_kafka_t：** 主客户端句柄
- 生产者或消费者实例
- 拥有所有内部资源
- 管理客户端生命周期

**rd_kafka_conf_t：** 配置对象
- 客户端创建前的配置构建器
- 被 rd_kafka_new() 消费
- 创建后配置不可变

**rd_kafka_message_t：** 消息结构
- 公共消息表示
- 包含所有消息元数据
- 用于生产回调和消费返回

### 3. 消息生命周期

**生产者流程：**
1. 消息创建（复制有效载荷、键、头）
2. 分区选择（哈希键或轮询）
3. 入队到分区队列
4. 批次形成（batch.size 或 linger.ms）
5. 发送到 Leader Broker
6. 等待确认（根据 acks 设置）
7. 投递报告回调

**消费者流程：**
1. 订阅/分配
2. 加入组和再平衡
3. 获取循环（FetchRequest/FetchResponse）
4. 消息投递到应用
5. 偏移量提交

### 4. API 使用模式

**线程安全规则：**
- rd_kafka_producev()：任何线程安全
- rd_kafka_consumer_poll()：使用专用消费者线程
- 回调：从内部线程调用，必须线程安全

**轮询机制：**
- 生产者必须 poll 触发投递报告
- 消费者 poll 直接返回消息

### 5. 成本和边界

**延迟 vs 吞吐量：**
- linger.ms=0：低延迟，低吞吐
- linger.ms=100：高吞吐，高延迟

**何时不使用 Kafka：**
- 请求-回复模式
- 极低延迟（<10ms）
- 小规模（<1000 msg/s）
- 数据库替代
- 大型二进制对象
- 强一致性需求
