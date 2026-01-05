# WHERE | Source Code Reading Strategy

## Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                     librdkafka SOURCE CODE MAP                                   │
└─────────────────────────────────────────────────────────────────────────────────┘

    src/
    ├── PUBLIC API (Start Here)
    │   ├── rdkafka.h ─────────────────── Main public header
    │   ├── rdkafka_error.h ───────────── Error handling API
    │   └── rdkafka_mock.h ────────────── Mock cluster for testing
    │
    ├── CORE CLIENT
    │   ├── rdkafka.c ─────────────────── Client lifecycle, initialization
    │   ├── rdkafka_conf.c ────────────── Configuration parsing/validation
    │   ├── rdkafka_broker.c ──────────── Broker connection management
    │   └── rdkafka_queue.c ───────────── Event queue implementation
    │
    ├── TOPIC & PARTITION
    │   ├── rdkafka_topic.c ───────────── Topic management
    │   ├── rdkafka_partition.c ───────── Partition state machine
    │   ├── rdkafka_metadata.c ────────── Cluster metadata handling
    │   └── rdkafka_assignment.c ──────── Partition assignment
    │
    ├── PRODUCER PATH
    │   ├── rdkafka_msg.c ─────────────── Message creation & batching
    │   ├── rdkafka_msgset.c ──────────── Message set encoding
    │   └── rdkafka_idempotence.c ─────── Idempotent producer logic
    │
    ├── CONSUMER PATH
    │   ├── rdkafka_cgrp.c ────────────── Consumer group protocol
    │   ├── rdkafka_fetcher.c ─────────── Fetch request/response
    │   ├── rdkafka_offset.c ──────────── Offset management
    │   └── rdkafka_subscription.c ────── Subscription handling
    │
    ├── PROTOCOL
    │   ├── rdkafka_request.c ─────────── Protocol request building
    │   ├── rdkafka_buf.c ─────────────── Protocol buffer management
    │   └── rdkafka_protocol.h ────────── Kafka protocol definitions
    │
    └── UTILITIES
        ├── rdkafka_timer.c ───────────── Timer management
        ├── rdkafka_transport.c ───────── Network I/O
        └── rdkafka_sasl.c ────────────── Authentication


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     READING ORDER: TOP-DOWN APPROACH                             │
└─────────────────────────────────────────────────────────────────────────────────┘

    PHASE 1: PUBLIC API                 PHASE 2: CLIENT ARCHITECTURE
    ═══════════════════                 ════════════════════════════

    ┌─────────────────┐                 ┌─────────────────────────────────────┐
    │   rdkafka.h     │                 │       rdkafka.c                     │
    │                 │                 │                                     │
    │ • Types         │  ────────>      │  rd_kafka_new()                     │
    │ • Functions     │                 │  ├── Configuration validation       │
    │ • Callbacks     │                 │  ├── Thread creation               │
    │ • Enums         │                 │  ├── Broker discovery              │
    │                 │                 │  └── Background tasks              │
    └─────────────────┘                 │                                     │
           │                            │  rd_kafka_destroy()                 │
           │                            │  ├── Flush pending                 │
           │                            │  ├── Thread join                   │
           │                            │  └── Resource cleanup              │
           │                            └─────────────────────────────────────┘
           │
           v
    ┌─────────────────┐                 ┌─────────────────────────────────────┐
    │ rdkafka_conf.c  │                 │       rdkafka_broker.c              │
    │                 │                 │                                     │
    │ • Property      │  ────────>      │  Per-broker state machine:          │
    │   definitions   │                 │                                     │
    │ • Defaults      │                 │  INIT ──> UP ──> UPDATE ──> DOWN    │
    │ • Validation    │                 │    ^                    │           │
    │ • Callbacks     │                 │    └────────────────────┘           │
    │                 │                 │                                     │
    └─────────────────┘                 │  Connection handling:               │
                                        │  • TCP/TLS setup                    │
                                        │  • SASL auth                        │
                                        │  • API version negotiation          │
                                        └─────────────────────────────────────┘


    PHASE 3: PROTOCOL & I/O             PHASE 4: RELIABILITY
    ═══════════════════════             ════════════════════

    ┌─────────────────────────────────────┐
    │       rdkafka_request.c             │
    │                                     │
    │  Request Types:                     │
    │  ┌───────────────────────────────┐  │
    │  │ Produce     │ Fetch          │  │
    │  │ Metadata    │ OffsetCommit   │  │
    │  │ JoinGroup   │ SyncGroup      │  │
    │  │ Heartbeat   │ LeaveGroup     │  │
    │  └───────────────────────────────┘  │
    │                                     │
    │  Request lifecycle:                 │
    │  1. Create request buffer           │
    │  2. Serialize to Kafka protocol     │
    │  3. Send to broker                  │
    │  4. Wait for response               │
    │  5. Parse response                  │
    │  6. Dispatch to handler             │
    └─────────────────────────────────────┘
              │
              v
    ┌─────────────────────────────────────┐
    │       rdkafka_buf.c                 │
    │                                     │
    │  Buffer management:                 │
    │  ┌───────────────────────────────┐  │
    │  │  rd_kafka_buf_t               │  │
    │  │  ├── Write position           │  │
    │  │  ├── Read position            │  │
    │  │  ├── Compression state        │  │
    │  │  └── Response handler         │  │
    │  └───────────────────────────────┘  │
    └─────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     KEY DATA STRUCTURES                                          │
└─────────────────────────────────────────────────────────────────────────────────┘

    rd_kafka_t (Main Client Handle)
    ═══════════════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  struct rd_kafka_s {                                                        │
    │      rd_kafka_type_t         type;        // PRODUCER or CONSUMER          │
    │      rd_kafka_conf_t        *conf;        // Configuration                  │
    │                                                                             │
    │      /* Broker management */                                                │
    │      rd_list_t               brokers;     // Known brokers                  │
    │      rd_kafka_broker_t      *internal_broker; // Internal ops broker       │
    │                                                                             │
    │      /* Topic management */                                                 │
    │      rd_list_t               topics;      // rd_kafka_itopic_t list        │
    │      rd_kafka_metadata_t    *metadata;    // Cluster metadata              │
    │                                                                             │
    │      /* Threading */                                                        │
    │      thrd_t                  main_thread; // Background thread             │
    │      mtx_t                   lock;        // Main lock                     │
    │      cnd_t                   cond;        // Condition variable            │
    │                                                                             │
    │      /* Queues */                                                           │
    │      rd_kafka_q_t           *rep_q;       // Reply queue                   │
    │      rd_kafka_q_t           *dr_q;        // Delivery report queue         │
    │                                                                             │
    │      /* Consumer specific */                                                │
    │      rd_kafka_cgrp_t        *cgrp;        // Consumer group handle         │
    │  };                                                                         │
    └─────────────────────────────────────────────────────────────────────────────┘


    rd_kafka_broker_t (Broker Connection)
    ═════════════════════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  struct rd_kafka_broker_s {                                                 │
    │      int32_t                 nodeid;      // Broker ID                      │
    │      char                   *name;        // host:port                      │
    │                                                                             │
    │      /* Connection state */                                                 │
    │      rd_kafka_broker_state_t state;       // INIT, UP, DOWN, etc.          │
    │      int                     sockfd;      // Socket FD                      │
    │      SSL                    *ssl;         // TLS context                    │
    │                                                                             │
    │      /* I/O */                                                              │
    │      rd_kafka_bufq_t         outbufs;     // Outgoing request queue        │
    │      rd_kafka_bufq_t         waitresps;   // Awaiting response             │
    │      thrd_t                  thread;      // Broker I/O thread             │
    │                                                                             │
    │      /* Partition mapping */                                                │
    │      rd_list_t               toppars;     // Partitions led by broker      │
    │  };                                                                         │
    └─────────────────────────────────────────────────────────────────────────────┘


    rd_kafka_toppar_t (Topic-Partition)
    ═══════════════════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  struct rd_kafka_toppar_s {                                                 │
    │      rd_kafka_itopic_t      *topic;       // Parent topic                  │
    │      int32_t                 partition;   // Partition ID                  │
    │                                                                             │
    │      /* Broker assignment */                                                │
    │      rd_kafka_broker_t      *leader;      // Current leader               │
    │      rd_kafka_broker_t      *broker;      // Connection broker            │
    │                                                                             │
    │      /* Producer state */                                                   │
    │      rd_kafka_msgq_t         msgq;        // Pending messages             │
    │      rd_kafka_msgq_t         xmit_msgq;   // In-flight batch              │
    │      int64_t                 acked_msgid; // Last acked                    │
    │                                                                             │
    │      /* Consumer state */                                                   │
    │      int64_t                 next_offset; // Next offset to fetch         │
    │      int64_t                 hi_offset;   // High watermark               │
    │      int64_t                 committed;   // Committed offset             │
    │      rd_kafka_fetch_state_t  fetch_state; // NONE, OFFSET, ACTIVE         │
    │  };                                                                         │
    └─────────────────────────────────────────────────────────────────────────────┘


    rd_kafka_msg_t (Internal Message)
    ═════════════════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │  struct rd_kafka_msg_s {                                                    │
    │      TAILQ_ENTRY(rd_kafka_msg_s) link;    // Queue linkage                 │
    │                                                                             │
    │      /* Payload */                                                          │
    │      void                   *payload;     // Message value                 │
    │      size_t                  len;         // Value length                  │
    │      void                   *key;         // Message key                   │
    │      size_t                  key_len;     // Key length                    │
    │                                                                             │
    │      /* Metadata */                                                         │
    │      int32_t                 partition;   // Target partition             │
    │      int64_t                 offset;      // Assigned offset (after ack)  │
    │      int64_t                 timestamp;   // Message timestamp            │
    │      rd_kafka_headers_t     *headers;     // Optional headers             │
    │                                                                             │
    │      /* Delivery tracking */                                                │
    │      rd_kafka_msg_status_t   status;      // NOT_PERSISTED, PERSISTED     │
    │      void                   *msg_opaque;  // User opaque (callback)       │
    │      uint64_t                msgid;       // Internal sequence            │
    │  };                                                                         │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     HOT PATHS (Performance Critical)                             │
└─────────────────────────────────────────────────────────────────────────────────┘

    PRODUCE HOT PATH
    ════════════════

    Application Thread                    Broker Thread
    ──────────────────                    ─────────────

    rd_kafka_producev()                          │
         │                                        │
         v                                        │
    ┌─────────────────┐                          │
    │ rd_kafka_msg_new│ ◄── Allocate message     │
    └────────┬────────┘                          │
             │                                    │
             v                                    │
    ┌─────────────────┐                          │
    │ partition_select│ ◄── Hash key or RR       │
    └────────┬────────┘                          │
             │                                    │
             v                                    │
    ┌─────────────────┐                          │
    │ msgq_enqueue    │ ◄── Lock-free queue      │
    └────────┬────────┘                          │
             │                                    │
             │        ┌──────────────────────────┘
             │        │
             │        v
             │   ┌─────────────────┐
             │   │ batch_build     │ ◄── Triggered by timer/size
             │   └────────┬────────┘
             │            │
             │            v
             │   ┌─────────────────┐
             │   │ compress_batch  │ ◄── Optional compression
             │   └────────┬────────┘
             │            │
             │            v
             │   ┌─────────────────┐
             │   │ send_to_broker  │ ◄── TCP send
             │   └────────┬────────┘
             │            │
             │   ┌────────┴────────┐
             │   │ wait_response   │
             │   └────────┬────────┘
             │            │
             v            v
    ┌─────────────────────────────────┐
    │ dr_callback (from rd_kafka_poll)│
    └─────────────────────────────────┘


    FETCH HOT PATH
    ══════════════

    Broker Thread                         Application Thread
    ─────────────                         ──────────────────

         │                                        │
    ┌────┴────────────────┐                      │
    │ send FetchRequest   │                      │
    └─────────────────────┘                      │
         │                                        │
         │  (broker processes)                   │
         │                                        │
    ┌────v────────────────┐                      │
    │ receive FetchResp   │                      │
    └─────────────────────┘                      │
         │                                        │
         v                                        │
    ┌─────────────────────┐                      │
    │ decompress (if any) │                      │
    └─────────────────────┘                      │
         │                                        │
         v                                        │
    ┌─────────────────────┐                      │
    │ parse messages      │                      │
    └─────────────────────┘                      │
         │                                        │
         v                                        │
    ┌─────────────────────┐                      │
    │ enqueue to fetchq   │                      │
    └─────────────────────┘                      │
         │                                        │
         └───────────────────────────────────────┤
                                                 │
                                    ┌────────────v────────────┐
                                    │ rd_kafka_consumer_poll()│
                                    └────────────┬────────────┘
                                                 │
                                    ┌────────────v────────────┐
                                    │ dequeue from fetchq     │
                                    └────────────┬────────────┘
                                                 │
                                    ┌────────────v────────────┐
                                    │ return to application   │
                                    └─────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     WHERE WHY/HOW/WHAT SHOWS IN CODE                             │
└─────────────────────────────────────────────────────────────────────────────────┘

    WHY (Log-Based Philosophy)                WHERE IN CODE
    ══════════════════════════                ═════════════

    Append-only writes                        rdkafka_msgset.c: rd_kafka_msgset_writer_*
                                              - Messages always appended to batch
                                              - No modification of existing data

    Partition as parallelism unit             rdkafka_partition.c
                                              - Each partition has own state machine
                                              - Independent queues per partition

    Pull-based consumption                    rdkafka_fetcher.c: rd_kafka_fetch_*
                                              - Consumer initiates fetch requests
                                              - Broker responds with available data


    HOW (Design Decisions)                    WHERE IN CODE
    ══════════════════════                    ═════════════

    Batching for throughput                   rdkafka_msg.c: rd_kafka_msgq_*
                                              - Messages queued per partition
                                              - Timer/size triggers batch send

    Client-side partitioning                  rdkafka_msg.c: rd_kafka_msg_partitioner
                                              - Partitioner callback
                                              - Key hashing logic

    Async delivery reports                    rdkafka_broker.c: rd_kafka_dr_*
                                              - Delivery callbacks queued
                                              - Serviced from poll()


    WHAT (Implementation)                     WHERE IN CODE
    ═════════════════════                     ═════════════

    Protocol encoding                         rdkafka_request.c
                                              - Each API type has builder function
                                              - rd_kafka_*Request() pattern

    Broker state machine                      rdkafka_broker.c: rd_kafka_broker_*
                                              - State transitions
                                              - Reconnection logic

    Consumer group protocol                   rdkafka_cgrp.c
                                              - JoinGroup, SyncGroup handlers
                                              - Rebalance callbacks
```

---

## 1. Recommended Reading Order

### Phase 1: Public API (User View)

**Goal:** Understand what a C developer can and cannot do.

#### `src/rdkafka.h`

This is your starting point. Read to understand:

```c
// Key type definitions
typedef struct rd_kafka_s rd_kafka_t;              // Client handle
typedef struct rd_kafka_conf_s rd_kafka_conf_t;   // Configuration
typedef struct rd_kafka_topic_s rd_kafka_topic_t; // Topic handle
typedef struct rd_kafka_message_s rd_kafka_message_t; // Message

// Client lifecycle
rd_kafka_t *rd_kafka_new(rd_kafka_type_t type, ...);
void rd_kafka_destroy(rd_kafka_t *rk);

// Producer API
rd_kafka_resp_err_t rd_kafka_producev(rd_kafka_t *rk, ...);
int rd_kafka_poll(rd_kafka_t *rk, int timeout_ms);
rd_kafka_resp_err_t rd_kafka_flush(rd_kafka_t *rk, int timeout_ms);

// Consumer API
rd_kafka_resp_err_t rd_kafka_subscribe(rd_kafka_t *rk, ...);
rd_kafka_message_t *rd_kafka_consumer_poll(rd_kafka_t *rk, int timeout_ms);
rd_kafka_resp_err_t rd_kafka_commit(rd_kafka_t *rk, ...);
```

**Key questions to answer:**
- What are the main object types?
- What callbacks can be registered?
- What configuration options exist?
- What error codes are possible?

#### Configuration APIs

Look at configuration property tables in `rdkafka.h` or `CONFIGURATION.md`:

```c
// Property categories to understand:
// - bootstrap.servers, client.id (connection)
// - acks, retries, linger.ms (producer reliability/performance)
// - group.id, auto.offset.reset (consumer behavior)
// - enable.idempotence, transactional.id (exactly-once)
```

---

### Phase 2: Client Architecture

**Goal:** Understand how librdkafka manages state, threads, and I/O.

#### `src/rdkafka.c`

Core client implementation:

```c
// Focus on these functions:
rd_kafka_new()
├── Validate configuration
├── Initialize internal structures
├── Create background thread
├── Start broker discovery
└── Return handle

rd_kafka_destroy()
├── Signal shutdown
├── Flush pending messages (producer)
├── Close consumer (consumer)
├── Join background threads
└── Free resources

// Background thread main loop
rd_kafka_thread_main()
├── Process timers
├── Handle metadata updates
├── Dispatch callbacks
└── Manage broker connections
```

#### `src/rdkafka_conf.c`

Configuration parsing and validation:

```c
// Property definition structure
struct rd_kafka_conf_prop {
    const char *name;
    rd_kafka_conf_type_t type;  // STRING, INT, BOOL, etc.
    void *default_val;
    int (*validate)(const char *value);
    int (*setter)(rd_kafka_conf_t *conf, const char *value);
};

// Understand property inheritance:
// rd_kafka_conf_t -> default topic conf -> per-topic conf
```

#### `src/rdkafka_broker.c`

Broker connection management:

```c
// Broker states (critical to understand)
typedef enum {
    RD_KAFKA_BROKER_STATE_INIT,           // Not connected
    RD_KAFKA_BROKER_STATE_DOWN,           // Connection failed
    RD_KAFKA_BROKER_STATE_CONNECT,        // Connecting
    RD_KAFKA_BROKER_STATE_AUTH,           // Authenticating
    RD_KAFKA_BROKER_STATE_UP,             // Connected and ready
    RD_KAFKA_BROKER_STATE_UPDATE,         // Updating API versions
    RD_KAFKA_BROKER_STATE_APIVERSION_QUERY // Querying capabilities
} rd_kafka_broker_state_t;

// Per-broker I/O thread
rd_kafka_broker_thread_main()
├── Process connection state
├── Send queued requests
├── Receive responses
├── Dispatch to handlers
└── Handle disconnections
```

#### `src/rdkafka_topic.c`

Topic state management:

```c
// Internal topic structure
struct rd_kafka_itopic_s {
    char *topic;                    // Topic name
    rd_kafka_topic_conf_t *conf;   // Topic configuration
    rd_list_t partitions;           // rd_kafka_toppar_t list
    int partition_cnt;              // Known partition count
    // ...
};

// Key operations:
rd_kafka_topic_new()       // Create topic handle
rd_kafka_topic_metadata()  // Request metadata
rd_kafka_topic_partition_available()  // Check leader
```

---

### Phase 3: Protocol and I/O

**Goal:** Understand how Kafka protocol requests are built and processed.

#### `src/rdkafka_request.c`

Protocol request building:

```c
// Request builder pattern
rd_kafka_buf_t *rd_kafka_ProduceRequest(
    rd_kafka_broker_t *rkb,
    rd_kafka_toppar_t *rktp,
    rd_kafka_msgset_t *mset,
    int16_t acks,
    int32_t timeout
) {
    rd_kafka_buf_t *rkbuf = rd_kafka_buf_new_request(
        rkb, RD_KAFKAP_Produce, ...
    );
    
    // Write request fields in protocol order
    rd_kafka_buf_write_i16(rkbuf, acks);
    rd_kafka_buf_write_i32(rkbuf, timeout);
    // ... topic, partition, message set ...
    
    return rkbuf;
}

// Response handler registration
rkbuf->rkbuf_cb = rd_kafka_handle_ProduceResponse;
```

#### `src/rdkafka_fetcher.c`

Consumer fetch implementation:

```c
// Fetch state machine
typedef enum {
    RD_KAFKA_FETCH_S_NONE,      // Not fetching
    RD_KAFKA_FETCH_S_OFFSET,    // Resolving offset
    RD_KAFKA_FETCH_S_ACTIVE     // Actively fetching
} rd_kafka_fetch_state_t;

// Fetch request building
rd_kafka_FetchRequest()
├── Collect assigned partitions
├── Build per-partition fetch specs
├── Include max_bytes, min_bytes
└── Send to appropriate broker

// Fetch response handling
rd_kafka_handle_FetchResponse()
├── Parse response
├── Decompress if needed
├── Parse message batches
├── Enqueue to partition queue
└── Update fetch offset
```

#### `src/rdkafka_buf.c`

Protocol buffer management:

```c
// Buffer structure (simplified)
struct rd_kafka_buf_s {
    int16_t api_key;              // Request type
    int16_t api_version;          // Protocol version
    int32_t corr_id;              // Correlation ID
    
    char *buf;                    // Data buffer
    size_t len;                   // Data length
    size_t pos;                   // Read/write position
    
    rd_kafka_resp_cb_t *cb;       // Response callback
    void *opaque;                 // Callback context
};
```

---

### Phase 4: Reliability and Coordination

**Goal:** Understand failure handling and recovery.

#### Retry Logic

Located across multiple files:

```c
// rdkafka_broker.c - Connection retry
rd_kafka_broker_reconnect()
├── Exponential backoff
├── reconnect.backoff.ms
├── reconnect.backoff.max.ms
└── State transition to CONNECT

// rdkafka_msg.c - Produce retry
rd_kafka_msg_retry()
├── Check retriable error
├── Check retry count < retries
├── Re-enqueue to partition queue
└── Update retry timestamp

// rdkafka_request.c - Request timeout
rd_kafka_request_timeout_check()
├── Check request age > request.timeout.ms
├── Fail with RD_KAFKA_RESP_ERR__TIMED_OUT
└── Trigger retry or delivery failure
```

#### `src/rdkafka_cgrp.c`

Consumer group protocol:

```c
// Consumer group states
typedef enum {
    RD_KAFKA_CGRP_STATE_INIT,
    RD_KAFKA_CGRP_STATE_QUERY_COORD,   // Finding coordinator
    RD_KAFKA_CGRP_STATE_WAIT_COORD,    // Waiting for coordinator
    RD_KAFKA_CGRP_STATE_JOIN,          // Joining group
    RD_KAFKA_CGRP_STATE_WAIT_SYNC,     // Waiting for sync
    RD_KAFKA_CGRP_STATE_UP,            // Active member
    RD_KAFKA_CGRP_STATE_WAIT_ASSIGN,   // Waiting assignment
    RD_KAFKA_CGRP_STATE_LEAVE          // Leaving group
} rd_kafka_cgrp_state_t;

// Join protocol
rd_kafka_cgrp_join()
├── Send JoinGroupRequest
├── Receive assignment
├── Apply partition assignment
└── Start fetching
```

#### Offset Storage

```c
// rdkafka_offset.c

// Commit offset to broker
rd_kafka_offset_commit()
├── Build OffsetCommitRequest
├── Send to group coordinator
└── Handle response

// Fetch committed offset
rd_kafka_offset_fetch()
├── Build OffsetFetchRequest
├── Send to coordinator
├── Parse committed offsets
└── Initialize fetch state
```

---

## 2. Architecture-Critical Data Structures

### Long-Lived Client State

| Structure | Lifetime | Purpose |
|-----------|----------|---------|
| `rd_kafka_t` | Application lifetime | Main client handle |
| `rd_kafka_broker_t` | Per broker connection | Connection state |
| `rd_kafka_itopic_t` | Per topic | Topic metadata |
| `rd_kafka_toppar_t` | Per partition | Partition state |
| `rd_kafka_cgrp_t` | Consumer lifetime | Group membership |

### Protocol State Machines

| Structure | States | Transitions |
|-----------|--------|-------------|
| `rd_kafka_broker_state_t` | INIT, DOWN, CONNECT, AUTH, UP | Connection lifecycle |
| `rd_kafka_cgrp_state_t` | INIT, QUERY_COORD, JOIN, UP, LEAVE | Group membership |
| `rd_kafka_fetch_state_t` | NONE, OFFSET, ACTIVE | Fetch progress |
| `rd_kafka_toppar_state_t` | STOPPED, OFFSET_WAIT, ACTIVE | Partition consume |

### Performance / Batching Structures

| Structure | Purpose | Hot Path |
|-----------|---------|----------|
| `rd_kafka_msgq_t` | Pending message queue | Producer |
| `rd_kafka_msgset_t` | Batch encoding buffer | Producer |
| `rd_kafka_bufq_t` | Request queue | I/O |
| `rd_kafka_fetchpos_t` | Fetch position tracking | Consumer |

---

## 3. Hot Paths (Performance-Critical Code)

### Producer Hot Path Files

```
rdkafka_msg.c
├── rd_kafka_msg_new()        - Message allocation
├── rd_kafka_msgq_enq()       - Queue insertion
└── rd_kafka_msgq_concat()    - Batch building

rdkafka_msgset.c
├── rd_kafka_msgset_writer_new()  - Batch buffer
├── rd_kafka_msgset_writer_write() - Message encoding
└── rd_kafka_msgset_writer_finalize() - Compression

rdkafka_broker.c
├── rd_kafka_broker_send()    - Send request
└── rd_kafka_broker_op_serve() - Process responses
```

### Consumer Hot Path Files

```
rdkafka_fetcher.c
├── rd_kafka_fetch_decide()   - Fetch planning
├── rd_kafka_FetchRequest()   - Build request
└── rd_kafka_handle_FetchResponse() - Parse response

rdkafka_partition.c
├── rd_kafka_toppar_fetch_decide()  - Per-partition fetch
└── rd_kafka_toppar_enq_msg()       - Enqueue fetched message

rdkafka_queue.c
├── rd_kafka_q_enq()          - Event enqueue
└── rd_kafka_q_pop()          - Event dequeue (poll)
```

---

## 4. Validating WHY / HOW / WHAT in Code

### Log-Based Philosophy in Client Design

**Where to look:**

```c
// rdkafka_msg.c - Messages are immutable after creation
rd_kafka_msg_new()
// Key: payload is copied (if flag set), never modified

// rdkafka_partition.c - Offset is the only cursor
rktp->rktp_next_offset  // Consumer position
rktp->rktp_hi_offset    // High watermark
// Key: No message IDs, only offsets
```

### Performance Optimizations Affecting Readability

**Batching optimization:**

```c
// rdkafka_msgq.h - Lock-free message queue
// Uses atomic operations for producer thread to enqueue
// while broker thread batches

// rdkafka_buf.c - Buffer pooling
// Reuses buffers to avoid allocation in hot path
```

**Zero-copy where possible:**

```c
// RD_KAFKA_MSG_F_FREE flag
// If set, librdkafka takes ownership of payload
// Avoids copy from application to internal buffer
```

### Thin Abstraction Boundaries

**Direct protocol access:**

```c
// rdkafka_request.c
// Request builders map almost directly to Kafka protocol
// Minimal abstraction layer

// rdkafka_protocol.h
// Protocol constants match Kafka specification exactly
#define RD_KAFKAP_Produce           0
#define RD_KAFKAP_Fetch             1
// ...
```

---

## 中文解释 (Chinese Explanations)

### 1. 源码阅读顺序

**第一阶段：公共 API**
- 从 `rdkafka.h` 开始
- 理解主要类型定义
- 了解可用的回调函数
- 掌握配置选项

**第二阶段：客户端架构**
- `rdkafka.c`：客户端生命周期
- `rdkafka_conf.c`：配置解析和验证
- `rdkafka_broker.c`：Broker 连接管理
- `rdkafka_topic.c`：主题状态管理

**第三阶段：协议和 I/O**
- `rdkafka_request.c`：协议请求构建
- `rdkafka_fetcher.c`：消费者获取实现
- `rdkafka_buf.c`：协议缓冲区管理

**第四阶段：可靠性和协调**
- 重试逻辑
- 消费组协议
- 偏移量存储

### 2. 关键数据结构

**长期客户端状态：**
- `rd_kafka_t`：主客户端句柄
- `rd_kafka_broker_t`：每个 Broker 连接
- `rd_kafka_toppar_t`：每个分区状态

**协议状态机：**
- Broker 状态：INIT → CONNECT → AUTH → UP
- 消费组状态：INIT → JOIN → UP → LEAVE
- 获取状态：NONE → OFFSET → ACTIVE

**性能/批处理结构：**
- `rd_kafka_msgq_t`：待发送消息队列
- `rd_kafka_msgset_t`：批次编码缓冲区
- `rd_kafka_bufq_t`：请求队列

### 3. 热路径

**生产者热路径：**
- 消息分配（`rd_kafka_msg_new`）
- 队列插入（`rd_kafka_msgq_enq`）
- 批次构建（`rd_kafka_msgset_writer_*`）
- 发送请求（`rd_kafka_broker_send`）

**消费者热路径：**
- 获取规划（`rd_kafka_fetch_decide`）
- 构建请求（`rd_kafka_FetchRequest`）
- 解析响应（`rd_kafka_handle_FetchResponse`）
- 事件出队（`rd_kafka_q_pop`）

### 4. 设计哲学在代码中的体现

**日志设计哲学：**
- 消息创建后不可变
- 偏移量是唯一游标
- 无消息 ID，只有偏移量

**性能优化：**
- 无锁消息队列
- 缓冲区池化
- 零拷贝选项

**薄抽象层：**
- 请求构建器直接映射 Kafka 协议
- 协议常量与 Kafka 规范完全匹配
