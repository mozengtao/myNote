# Real Project Usage and Engineering Practices

## Overview Diagram

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                     RECOMMENDED PROJECT STRUCTURE                                │
└─────────────────────────────────────────────────────────────────────────────────┘

    my_kafka_project/
    │
    ├── CMakeLists.txt                 # Build configuration
    ├── README.md                      # Project documentation
    │
    ├── include/                       # Public headers
    │   └── my_kafka/
    │       ├── producer.h             # Producer API
    │       ├── consumer.h             # Consumer API
    │       └── common.h               # Shared types
    │
    ├── src/
    │   ├── kafka/                     # Kafka abstraction layer
    │   │   ├── config.c               # Configuration builder
    │   │   ├── config.h
    │   │   ├── producer.c             # Producer wrapper
    │   │   ├── consumer.c             # Consumer wrapper
    │   │   └── error.c                # Error handling
    │   │
    │   ├── producer/                  # Producer-specific code
    │   │   ├── message_builder.c      # Message construction
    │   │   ├── partitioner.c          # Custom partitioning
    │   │   └── retry_handler.c        # Retry logic
    │   │
    │   ├── consumer/                  # Consumer-specific code
    │   │   ├── worker_pool.c          # Worker thread pool
    │   │   ├── offset_manager.c       # Offset commit logic
    │   │   └── rebalance_handler.c    # Rebalance callbacks
    │   │
    │   ├── processing/                # Business logic
    │   │   ├── message_handler.c      # Message processing
    │   │   └── pipeline.c             # Processing pipeline
    │   │
    │   ├── storage/                   # State management
    │   │   ├── checkpoint.c           # Offset checkpointing
    │   │   └── state_store.c          # Local state
    │   │
    │   └── app/                       # Application entry
    │       ├── main_producer.c        # Producer executable
    │       └── main_consumer.c        # Consumer executable
    │
    └── tests/
        ├── unit/                      # Unit tests
        └── integration/               # Integration tests


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     PRODUCER MODULE DESIGN                                       │
└─────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                        PRODUCER WRAPPER                                     │
    │                                                                             │
    │   Application                                                               │
    │       │                                                                     │
    │       │  my_producer_send(producer, event)                                 │
    │       v                                                                     │
    │   ┌───────────────────────────────────────────────────────────┐            │
    │   │                   my_producer_t                            │            │
    │   │                                                            │            │
    │   │   ┌────────────────────────────────────────────────────┐  │            │
    │   │   │  Event Serialization                               │  │            │
    │   │   │  - JSON/Protobuf/Avro encoding                     │  │            │
    │   │   │  - Schema validation                                │  │            │
    │   │   └─────────────────────┬──────────────────────────────┘  │            │
    │   │                         │                                  │            │
    │   │   ┌─────────────────────v──────────────────────────────┐  │            │
    │   │   │  Partition Selection                               │  │            │
    │   │   │  - Extract key from event                          │  │            │
    │   │   │  - Apply partitioning strategy                     │  │            │
    │   │   └─────────────────────┬──────────────────────────────┘  │            │
    │   │                         │                                  │            │
    │   │   ┌─────────────────────v──────────────────────────────┐  │            │
    │   │   │  Backpressure Handling                             │  │            │
    │   │   │  - Check queue depth                                │  │            │
    │   │   │  - Block or return error if full                   │  │            │
    │   │   └─────────────────────┬──────────────────────────────┘  │            │
    │   │                         │                                  │            │
    │   │   ┌─────────────────────v──────────────────────────────┐  │            │
    │   │   │  rd_kafka_producev()                               │  │            │
    │   │   └─────────────────────┬──────────────────────────────┘  │            │
    │   │                         │                                  │            │
    │   │   ┌─────────────────────v──────────────────────────────┐  │            │
    │   │   │  Metrics Collection                                 │  │            │
    │   │   │  - Messages produced                                │  │            │
    │   │   │  - Bytes produced                                   │  │            │
    │   │   │  - Failures                                         │  │            │
    │   │   └────────────────────────────────────────────────────┘  │            │
    │   │                                                            │            │
    │   └────────────────────────────────────────────────────────────┘            │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     CONSUMER WORKER POOL DESIGN                                  │
└─────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │                     CONSUMER WORKER ARCHITECTURE                            │
    │                                                                             │
    │   ┌───────────────────────────────────────────────────────────────────┐    │
    │   │                    Poll Thread (Single)                           │    │
    │   │                                                                   │    │
    │   │   while (running) {                                              │    │
    │   │       msg = rd_kafka_consumer_poll(rk, 100);                     │    │
    │   │       if (msg && !msg->err)                                      │    │
    │   │           work_queue_push(queue, msg);  // Don't destroy yet     │    │
    │   │   }                                                               │    │
    │   │                                                                   │    │
    │   └───────────────────────────────┬───────────────────────────────────┘    │
    │                                   │                                         │
    │                                   v                                         │
    │   ┌───────────────────────────────────────────────────────────────────┐    │
    │   │                     Work Queue (Thread-Safe)                      │    │
    │   │   ┌─────┬─────┬─────┬─────┬─────┬─────┐                          │    │
    │   │   │ msg │ msg │ msg │ msg │ msg │ msg │                          │    │
    │   │   └─────┴─────┴─────┴─────┴─────┴─────┘                          │    │
    │   └───────────────────────────────┬───────────────────────────────────┘    │
    │                                   │                                         │
    │           ┌───────────────────────┼───────────────────────┐                │
    │           │                       │                       │                │
    │           v                       v                       v                │
    │   ┌───────────────┐       ┌───────────────┐       ┌───────────────┐       │
    │   │  Worker 1     │       │  Worker 2     │       │  Worker N     │       │
    │   │               │       │               │       │               │       │
    │   │  msg = pop()  │       │  msg = pop()  │       │  msg = pop()  │       │
    │   │  process(msg) │       │  process(msg) │       │  process(msg) │       │
    │   │  ack(msg)     │       │  ack(msg)     │       │  ack(msg)     │       │
    │   │  destroy(msg) │       │  destroy(msg) │       │  destroy(msg) │       │
    │   │               │       │               │       │               │       │
    │   └───────────────┘       └───────────────┘       └───────────────┘       │
    │                                                                             │
    │   ┌───────────────────────────────────────────────────────────────────┐    │
    │   │                    Commit Thread (Periodic)                       │    │
    │   │                                                                   │    │
    │   │   while (running) {                                              │    │
    │   │       sleep(commit_interval);                                    │    │
    │   │       offsets = collect_acked_offsets();                         │    │
    │   │       rd_kafka_commit(rk, offsets, async=1);                     │    │
    │   │   }                                                               │    │
    │   │                                                                   │    │
    │   └───────────────────────────────────────────────────────────────────┘    │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    IMPORTANT: Offset Tracking Per Partition
    ─────────────────────────────────────────

    Partition 0:  processed_offset = 105  │  committed_offset = 100
    Partition 1:  processed_offset = 203  │  committed_offset = 200
    Partition 2:  processed_offset = 57   │  committed_offset = 50

    Only commit when ALL messages up to offset X are processed!


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     BACKPRESSURE HANDLING                                        │
└─────────────────────────────────────────────────────────────────────────────────┘

    PRODUCER BACKPRESSURE
    ═════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │   Strategy 1: Block on Queue Full                                          │
    │   ─────────────────────────────────                                         │
    │                                                                             │
    │   while (1) {                                                               │
    │       err = rd_kafka_producev(rk, ...);                                    │
    │       if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {                          │
    │           // Wait for queue to drain                                       │
    │           rd_kafka_poll(rk, 1000);                                         │
    │           continue;  // Retry                                              │
    │       }                                                                     │
    │       break;                                                                │
    │   }                                                                         │
    │                                                                             │
    │   Strategy 2: Proactive Monitoring                                         │
    │   ────────────────────────────────                                          │
    │                                                                             │
    │   int queue_len = rd_kafka_outq_len(rk);                                   │
    │   if (queue_len > high_watermark) {                                        │
    │       // Slow down production                                              │
    │       // - Rate limit incoming requests                                    │
    │       // - Return "try later" to callers                                   │
    │   }                                                                         │
    │                                                                             │
    │   Strategy 3: Callback-Based                                               │
    │   ───────────────────────────                                               │
    │                                                                             │
    │   // Track in-flight count in delivery callback                            │
    │   static atomic_int in_flight = 0;                                         │
    │                                                                             │
    │   void dr_cb(...) {                                                        │
    │       atomic_fetch_sub(&in_flight, 1);                                     │
    │   }                                                                         │
    │                                                                             │
    │   void send(...) {                                                         │
    │       while (atomic_load(&in_flight) > max_in_flight)                      │
    │           rd_kafka_poll(rk, 100);                                          │
    │       rd_kafka_producev(rk, ...);                                          │
    │       atomic_fetch_add(&in_flight, 1);                                     │
    │   }                                                                         │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘


    CONSUMER BACKPRESSURE
    ═════════════════════

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │   Strategy 1: Pause Partitions                                             │
    │   ────────────────────────────                                              │
    │                                                                             │
    │   // Work queue getting full                                               │
    │   if (work_queue_size > threshold) {                                       │
    │       rd_kafka_pause_partitions(rk, assigned_partitions);                  │
    │   }                                                                         │
    │                                                                             │
    │   // Work queue drained                                                    │
    │   if (work_queue_size < low_threshold) {                                   │
    │       rd_kafka_resume_partitions(rk, assigned_partitions);                 │
    │   }                                                                         │
    │                                                                             │
    │   Strategy 2: Bounded Work Queue                                           │
    │   ──────────────────────────────                                            │
    │                                                                             │
    │   // Blocking push (natural backpressure)                                  │
    │   work_queue_push_blocking(queue, msg, timeout);                           │
    │   // If queue full, poll thread blocks, consumer_poll not called          │
    │   // Kafka fetch naturally slows down                                      │
    │                                                                             │
    │   Strategy 3: Rate Limiting                                                │
    │   ─────────────────────────                                                 │
    │                                                                             │
    │   token_bucket_t rate_limiter;                                             │
    │   init_token_bucket(&rate_limiter, 1000);  // 1000 msg/sec                 │
    │                                                                             │
    │   while (running) {                                                        │
    │       msg = rd_kafka_consumer_poll(rk, 100);                               │
    │       if (msg && !msg->err) {                                              │
    │           acquire_token(&rate_limiter);  // May block                      │
    │           process(msg);                                                    │
    │       }                                                                     │
    │   }                                                                         │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     OFFSET COMMIT STRATEGIES                                     │
└─────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │   AT-LEAST-ONCE (Recommended Default)                                      │
    │   ═══════════════════════════════════                                       │
    │                                                                             │
    │   while (running) {                                                        │
    │       msg = poll();                                                        │
    │       process(msg);          // Process first                              │
    │       commit(msg->offset+1); // Then commit                                │
    │       destroy(msg);                                                        │
    │   }                                                                         │
    │                                                                             │
    │   On crash: Message may be re-delivered                                    │
    │   Requirement: Processing must be idempotent                               │
    │                                                                             │
    │   ┌─────────────────────────────────────────────────────────────────────┐  │
    │   │  Timeline:                                                           │  │
    │   │                                                                      │  │
    │   │  poll(offset=5) ──> process() ──> CRASH                             │  │
    │   │                                      │                               │  │
    │   │  restart ──> poll(offset=5) ──> process() ──> commit(6)             │  │
    │   │              (same message!)        (must be idempotent)             │  │
    │   │                                                                      │  │
    │   └─────────────────────────────────────────────────────────────────────┘  │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │   AT-MOST-ONCE (When Duplicates Are Worse Than Loss)                       │
    │   ═══════════════════════════════════════════════════                       │
    │                                                                             │
    │   while (running) {                                                        │
    │       msg = poll();                                                        │
    │       commit(msg->offset+1); // Commit first                               │
    │       process(msg);          // Then process                               │
    │       destroy(msg);                                                        │
    │   }                                                                         │
    │                                                                             │
    │   On crash: Message may be lost                                            │
    │   Use case: Metrics, logs where loss is acceptable                         │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────┐
    │                                                                             │
    │   EXACTLY-ONCE (With External Store)                                       │
    │   ═══════════════════════════════════                                       │
    │                                                                             │
    │   // Store offset WITH processing result in same transaction               │
    │                                                                             │
    │   while (running) {                                                        │
    │       msg = poll();                                                        │
    │       BEGIN_TRANSACTION(db);                                               │
    │           process_and_store(msg, db);                                      │
    │           store_offset(msg->partition, msg->offset+1, db);                 │
    │       COMMIT_TRANSACTION(db);                                              │
    │       destroy(msg);                                                        │
    │   }                                                                         │
    │                                                                             │
    │   On restart: Read offset from db, seek consumer to that position          │
    │                                                                             │
    │   // Do NOT use Kafka offset storage in this pattern                       │
    │   rd_kafka_conf_set(conf, "enable.auto.commit", "false");                  │
    │   // Seek on startup:                                                      │
    │   offset = read_offset_from_db(partition);                                 │
    │   rd_kafka_seek(topic, partition, offset, timeout);                        │
    │                                                                             │
    └─────────────────────────────────────────────────────────────────────────────┘


┌─────────────────────────────────────────────────────────────────────────────────┐
│                     COMMON PITFALLS                                              │
└─────────────────────────────────────────────────────────────────────────────────┘

    PITFALL 1: Incorrect Offset Commits
    ════════════════════════════════════

    ❌ WRONG: Commit processed offset (will skip next message)
    ─────────────────────────────────────────────────────────
    process(msg);
    rd_kafka_commit_message(rk, msg, 0);  // Commits msg->offset
    // Next poll will start at msg->offset+1, CORRECT
    // BUT: commit_message commits the MESSAGE's offset, not offset+1

    ✓ CORRECT: Commit next offset to read
    ──────────────────────────────────────
    process(msg);
    // rd_kafka_commit_message internally does offset+1
    rd_kafka_commit_message(rk, msg, 0);  // Actually correct!

    ❌ WRONG: Manual offset without +1
    ───────────────────────────────────
    rd_kafka_topic_partition_list_t *offsets = ...;
    offsets->elems[0].offset = msg->offset;  // WRONG!
    rd_kafka_commit(rk, offsets, 0);
    // Will re-read same message on restart!

    ✓ CORRECT: Manual offset with +1
    ─────────────────────────────────
    offsets->elems[0].offset = msg->offset + 1;  // CORRECT
    rd_kafka_commit(rk, offsets, 0);


    PITFALL 2: Blocking in Poll Loop
    ═════════════════════════════════

    ❌ WRONG: Long-running processing in poll thread
    ─────────────────────────────────────────────────
    while (running) {
        msg = rd_kafka_consumer_poll(rk, 100);
        if (msg) {
            http_request(external_api);  // BLOCKS FOR SECONDS
            db_write(msg);               // BLOCKS
            // During this time:
            // - No heartbeats sent (may be kicked from group)
            // - No other messages fetched
        }
    }

    ✓ CORRECT: Separate poll from processing
    ─────────────────────────────────────────
    // Poll thread
    while (running) {
        msg = rd_kafka_consumer_poll(rk, 100);
        if (msg)
            work_queue_push(queue, msg);
    }

    // Worker threads (separate)
    while (running) {
        msg = work_queue_pop(queue);
        http_request(external_api);  // OK to block here
        db_write(msg);
        ack_message(msg);
    }


    PITFALL 3: Misconfigured Retries and Timeouts
    ══════════════════════════════════════════════

    ❌ WRONG: Default timeouts with slow consumers
    ───────────────────────────────────────────────
    // session.timeout.ms = 45000 (default)
    // max.poll.interval.ms = 300000 (default)

    // If process() takes > 5 minutes:
    while (running) {
        msg = poll();
        slow_process(msg);  // Takes 6 minutes
        // Consumer kicked from group!
    }

    ✓ CORRECT: Tune timeouts for workload
    ──────────────────────────────────────
    rd_kafka_conf_set(conf, "max.poll.interval.ms", "600000");  // 10 min
    // OR use worker pool (better)


    PITFALL 4: Over/Under-Partitioning
    ═══════════════════════════════════

    ❌ WRONG: 100 partitions, 2 consumers
    ──────────────────────────────────────
    // Each consumer handles 50 partitions
    // - More connections to brokers
    // - More memory for partition state
    // - Longer rebalances

    ❌ WRONG: 2 partitions, 10 consumers
    ──────────────────────────────────────
    // 8 consumers idle
    // Can't scale processing

    ✓ CORRECT: Partitions >= expected max consumers
    ─────────────────────────────────────────────────
    // Start with partitions = 2-3x current consumers
    // Plan for future scaling


    PITFALL 5: Ignoring Rebalance Callbacks
    ═══════════════════════════════════════

    ❌ WRONG: No rebalance handling
    ────────────────────────────────
    rd_kafka_subscribe(rk, topics);
    while (running) {
        msg = poll();
        // Processing...
        // On rebalance: committed offset may not match processed offset
        // Data loss or duplicates!
    }

    ✓ CORRECT: Commit on revoke
    ────────────────────────────
    void rebalance_cb(rd_kafka_t *rk, rd_kafka_resp_err_t err,
                      rd_kafka_topic_partition_list_t *parts, void *opaque) {
        if (err == RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS) {
            // Commit processed offsets before losing partitions
            rd_kafka_commit(rk, NULL, 0);  // Sync commit
        }
        rd_kafka_assign(rk, err == RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS
                            ? parts : NULL);
    }

    rd_kafka_conf_set_rebalance_cb(conf, rebalance_cb);
```

---

## 1. Using Kafka in Real C Projects

### 1.1 Designing Producer Modules

```c
// producer.h
#ifndef MY_PRODUCER_H
#define MY_PRODUCER_H

#include <rdkafka.h>

typedef struct my_producer my_producer_t;

typedef struct {
    const char *bootstrap_servers;
    const char *topic;
    int acks;                    // 0, 1, or -1 (all)
    int linger_ms;
    int batch_size;
    int max_in_flight;
} my_producer_config_t;

typedef void (*my_delivery_cb)(void *context, int success, int64_t offset);

// Lifecycle
my_producer_t *my_producer_new(const my_producer_config_t *config);
void my_producer_destroy(my_producer_t *producer);

// Operations
int my_producer_send(my_producer_t *producer,
                     const char *key, size_t key_len,
                     const void *value, size_t value_len,
                     my_delivery_cb callback, void *context);

int my_producer_flush(my_producer_t *producer, int timeout_ms);

// Monitoring
int my_producer_queue_len(my_producer_t *producer);

#endif
```

```c
// producer.c
#include "producer.h"
#include <stdlib.h>
#include <string.h>

struct my_producer {
    rd_kafka_t *rk;
    char *topic;
    int max_in_flight;
    atomic_int in_flight;
};

typedef struct {
    my_delivery_cb callback;
    void *context;
    my_producer_t *producer;
} delivery_context_t;

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage,
                      void *opaque) {
    delivery_context_t *ctx = rkmessage->_private;
    
    if (ctx) {
        atomic_fetch_sub(&ctx->producer->in_flight, 1);
        
        if (ctx->callback) {
            ctx->callback(ctx->context,
                         rkmessage->err == 0,
                         rkmessage->offset);
        }
        free(ctx);
    }
}

my_producer_t *my_producer_new(const my_producer_config_t *config) {
    my_producer_t *producer = calloc(1, sizeof(*producer));
    if (!producer) return NULL;
    
    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    char errstr[512];
    
    // Required settings
    rd_kafka_conf_set(conf, "bootstrap.servers",
                      config->bootstrap_servers, errstr, sizeof(errstr));
    
    // Reliability settings
    char acks_str[16];
    snprintf(acks_str, sizeof(acks_str), "%d", config->acks);
    rd_kafka_conf_set(conf, "acks", acks_str, NULL, 0);
    
    // Performance settings
    char linger_str[16];
    snprintf(linger_str, sizeof(linger_str), "%d", config->linger_ms);
    rd_kafka_conf_set(conf, "linger.ms", linger_str, NULL, 0);
    
    // Callback
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);
    
    producer->rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!producer->rk) {
        free(producer);
        return NULL;
    }
    
    producer->topic = strdup(config->topic);
    producer->max_in_flight = config->max_in_flight;
    atomic_init(&producer->in_flight, 0);
    
    return producer;
}

int my_producer_send(my_producer_t *producer,
                     const char *key, size_t key_len,
                     const void *value, size_t value_len,
                     my_delivery_cb callback, void *context) {
    
    // Backpressure: wait if too many in flight
    while (atomic_load(&producer->in_flight) >= producer->max_in_flight) {
        rd_kafka_poll(producer->rk, 100);
    }
    
    delivery_context_t *ctx = malloc(sizeof(*ctx));
    ctx->callback = callback;
    ctx->context = context;
    ctx->producer = producer;
    
    rd_kafka_resp_err_t err;
    
retry:
    err = rd_kafka_producev(
        producer->rk,
        RD_KAFKA_V_TOPIC(producer->topic),
        RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
        RD_KAFKA_V_KEY((void *)key, key_len),
        RD_KAFKA_V_VALUE((void *)value, value_len),
        RD_KAFKA_V_OPAQUE(ctx),
        RD_KAFKA_V_END);
    
    if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
        rd_kafka_poll(producer->rk, 1000);
        goto retry;
    }
    
    if (err) {
        free(ctx);
        return -1;
    }
    
    atomic_fetch_add(&producer->in_flight, 1);
    
    // Service delivery reports
    rd_kafka_poll(producer->rk, 0);
    
    return 0;
}

int my_producer_flush(my_producer_t *producer, int timeout_ms) {
    return rd_kafka_flush(producer->rk, timeout_ms);
}

void my_producer_destroy(my_producer_t *producer) {
    if (!producer) return;
    
    rd_kafka_flush(producer->rk, 10000);
    rd_kafka_destroy(producer->rk);
    free(producer->topic);
    free(producer);
}
```

### 1.2 Designing Consumer Worker Pools

```c
// consumer_pool.h
#ifndef CONSUMER_POOL_H
#define CONSUMER_POOL_H

#include <rdkafka.h>

typedef struct consumer_pool consumer_pool_t;

typedef void (*message_handler_t)(const void *payload, size_t len,
                                  const void *key, size_t key_len,
                                  int32_t partition, int64_t offset,
                                  void *context);

typedef struct {
    const char *bootstrap_servers;
    const char *group_id;
    const char **topics;
    int topic_count;
    int worker_count;
    int work_queue_size;
    int commit_interval_ms;
    message_handler_t handler;
    void *handler_context;
} consumer_pool_config_t;

consumer_pool_t *consumer_pool_new(const consumer_pool_config_t *config);
int consumer_pool_start(consumer_pool_t *pool);
int consumer_pool_stop(consumer_pool_t *pool);
void consumer_pool_destroy(consumer_pool_t *pool);

#endif
```

```c
// consumer_pool.c (simplified)
#include "consumer_pool.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct work_item {
    rd_kafka_message_t *msg;
    struct work_item *next;
} work_item_t;

typedef struct {
    work_item_t *head;
    work_item_t *tail;
    int count;
    int max_size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int shutdown;
} work_queue_t;

struct consumer_pool {
    rd_kafka_t *rk;
    work_queue_t queue;
    pthread_t poll_thread;
    pthread_t *worker_threads;
    pthread_t commit_thread;
    int worker_count;
    int commit_interval_ms;
    message_handler_t handler;
    void *handler_context;
    volatile int running;
    
    // Offset tracking per partition
    pthread_mutex_t offset_lock;
    // Map: (topic, partition) -> highest processed offset
};

// Work queue operations
static void queue_init(work_queue_t *q, int max_size) {
    q->head = q->tail = NULL;
    q->count = 0;
    q->max_size = max_size;
    q->shutdown = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

static void queue_push(work_queue_t *q, rd_kafka_message_t *msg) {
    work_item_t *item = malloc(sizeof(*item));
    item->msg = msg;
    item->next = NULL;
    
    pthread_mutex_lock(&q->lock);
    while (q->count >= q->max_size && !q->shutdown) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    
    if (q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        free(item);
        rd_kafka_message_destroy(msg);
        return;
    }
    
    if (q->tail)
        q->tail->next = item;
    else
        q->head = item;
    q->tail = item;
    q->count++;
    
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

static rd_kafka_message_t *queue_pop(work_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    
    if (q->shutdown && q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }
    
    work_item_t *item = q->head;
    q->head = item->next;
    if (!q->head)
        q->tail = NULL;
    q->count--;
    
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    
    rd_kafka_message_t *msg = item->msg;
    free(item);
    return msg;
}

// Rebalance callback
static void rebalance_cb(rd_kafka_t *rk, rd_kafka_resp_err_t err,
                         rd_kafka_topic_partition_list_t *parts, void *opaque) {
    consumer_pool_t *pool = opaque;
    
    switch (err) {
    case RD_KAFKA_RESP_ERR__ASSIGN_PARTITIONS:
        fprintf(stderr, "Assigned %d partitions\n", parts->cnt);
        rd_kafka_assign(rk, parts);
        break;
        
    case RD_KAFKA_RESP_ERR__REVOKE_PARTITIONS:
        fprintf(stderr, "Revoked %d partitions\n", parts->cnt);
        // Commit current offsets before giving up partitions
        rd_kafka_commit(rk, NULL, 0);
        rd_kafka_assign(rk, NULL);
        break;
        
    default:
        fprintf(stderr, "Rebalance error: %s\n", rd_kafka_err2str(err));
        rd_kafka_assign(rk, NULL);
    }
}

// Poll thread
static void *poll_thread_func(void *arg) {
    consumer_pool_t *pool = arg;
    
    while (pool->running) {
        rd_kafka_message_t *msg = rd_kafka_consumer_poll(pool->rk, 100);
        
        if (!msg)
            continue;
        
        if (msg->err) {
            if (msg->err != RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                fprintf(stderr, "Consumer error: %s\n",
                        rd_kafka_message_errstr(msg));
            }
            rd_kafka_message_destroy(msg);
            continue;
        }
        
        queue_push(&pool->queue, msg);
    }
    
    return NULL;
}

// Worker thread
static void *worker_thread_func(void *arg) {
    consumer_pool_t *pool = arg;
    
    while (pool->running || pool->queue.count > 0) {
        rd_kafka_message_t *msg = queue_pop(&pool->queue);
        if (!msg)
            break;
        
        // Process message
        pool->handler(msg->payload, msg->len,
                     msg->key, msg->key_len,
                     msg->partition, msg->offset,
                     pool->handler_context);
        
        // Track processed offset (for commit thread)
        // ... update offset tracking ...
        
        rd_kafka_message_destroy(msg);
    }
    
    return NULL;
}

// Commit thread
static void *commit_thread_func(void *arg) {
    consumer_pool_t *pool = arg;
    
    while (pool->running) {
        // Sleep for commit interval
        struct timespec ts = {
            .tv_sec = pool->commit_interval_ms / 1000,
            .tv_nsec = (pool->commit_interval_ms % 1000) * 1000000
        };
        nanosleep(&ts, NULL);
        
        // Commit current offsets
        rd_kafka_commit(pool->rk, NULL, 1);  // Async
    }
    
    // Final sync commit
    rd_kafka_commit(pool->rk, NULL, 0);
    
    return NULL;
}

consumer_pool_t *consumer_pool_new(const consumer_pool_config_t *config) {
    consumer_pool_t *pool = calloc(1, sizeof(*pool));
    if (!pool) return NULL;
    
    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    char errstr[512];
    
    rd_kafka_conf_set(conf, "bootstrap.servers",
                      config->bootstrap_servers, errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "group.id", config->group_id, NULL, 0);
    rd_kafka_conf_set(conf, "enable.auto.commit", "false", NULL, 0);
    rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", NULL, 0);
    
    rd_kafka_conf_set_rebalance_cb(conf, rebalance_cb);
    rd_kafka_conf_set_opaque(conf, pool);
    
    pool->rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!pool->rk) {
        free(pool);
        return NULL;
    }
    
    rd_kafka_poll_set_consumer(pool->rk);
    
    // Subscribe
    rd_kafka_topic_partition_list_t *topics =
        rd_kafka_topic_partition_list_new(config->topic_count);
    for (int i = 0; i < config->topic_count; i++) {
        rd_kafka_topic_partition_list_add(topics, config->topics[i],
                                          RD_KAFKA_PARTITION_UA);
    }
    rd_kafka_subscribe(pool->rk, topics);
    rd_kafka_topic_partition_list_destroy(topics);
    
    queue_init(&pool->queue, config->work_queue_size);
    
    pool->worker_count = config->worker_count;
    pool->commit_interval_ms = config->commit_interval_ms;
    pool->handler = config->handler;
    pool->handler_context = config->handler_context;
    pool->worker_threads = calloc(config->worker_count, sizeof(pthread_t));
    
    pthread_mutex_init(&pool->offset_lock, NULL);
    
    return pool;
}

int consumer_pool_start(consumer_pool_t *pool) {
    pool->running = 1;
    
    pthread_create(&pool->poll_thread, NULL, poll_thread_func, pool);
    pthread_create(&pool->commit_thread, NULL, commit_thread_func, pool);
    
    for (int i = 0; i < pool->worker_count; i++) {
        pthread_create(&pool->worker_threads[i], NULL,
                       worker_thread_func, pool);
    }
    
    return 0;
}

int consumer_pool_stop(consumer_pool_t *pool) {
    pool->running = 0;
    
    // Signal queue shutdown
    pthread_mutex_lock(&pool->queue.lock);
    pool->queue.shutdown = 1;
    pthread_cond_broadcast(&pool->queue.not_empty);
    pthread_cond_broadcast(&pool->queue.not_full);
    pthread_mutex_unlock(&pool->queue.lock);
    
    pthread_join(pool->poll_thread, NULL);
    pthread_join(pool->commit_thread, NULL);
    
    for (int i = 0; i < pool->worker_count; i++) {
        pthread_join(pool->worker_threads[i], NULL);
    }
    
    return 0;
}

void consumer_pool_destroy(consumer_pool_t *pool) {
    if (!pool) return;
    
    rd_kafka_consumer_close(pool->rk);
    rd_kafka_destroy(pool->rk);
    
    free(pool->worker_threads);
    pthread_mutex_destroy(&pool->queue.lock);
    pthread_cond_destroy(&pool->queue.not_empty);
    pthread_cond_destroy(&pool->queue.not_full);
    pthread_mutex_destroy(&pool->offset_lock);
    
    free(pool);
}
```

---

## 2. Anti-Patterns to Avoid

### 2.1 Treating Kafka as a Queue Instead of a Log

```c
// ❌ ANTI-PATTERN: Expecting delete-after-read behavior
while (running) {
    msg = poll();
    process(msg);
    // Expecting message to be "gone" from Kafka - WRONG!
    // Message stays until retention period expires
}

// ✓ CORRECT: Embrace log semantics
// - Multiple consumer groups can read same data
// - Use retention policy for cleanup
// - Design for replay scenarios
```

### 2.2 Assuming Exactly-Once Without Understanding Transactions

```c
// ❌ ANTI-PATTERN: Assuming no duplicates with acks=all
rd_kafka_conf_set(conf, "acks", "all", NULL, 0);
// This is at-least-once, not exactly-once!

// ✓ CORRECT: True exactly-once requires transactions
rd_kafka_conf_set(conf, "enable.idempotence", "true", NULL, 0);
rd_kafka_conf_set(conf, "transactional.id", "my-producer-1", NULL, 0);

rd_kafka_init_transactions(rk, timeout);
rd_kafka_begin_transaction(rk);
rd_kafka_producev(rk, ...);
rd_kafka_commit_transaction(rk, timeout);
```

### 2.3 Using Kafka as a Database

```c
// ❌ ANTI-PATTERN: Querying Kafka for specific records
// "Find the order with ID 12345"
// Kafka has no index lookup!

// ✓ CORRECT: Use Kafka for streaming, database for queries
// - Kafka: Publish events
// - Consumer: Build materialized view in database
// - Queries: Hit database, not Kafka
```

### 2.4 Mixing Consumer Group Logic with Business Logic

```c
// ❌ ANTI-PATTERN: Business logic in rebalance callback
void rebalance_cb(rd_kafka_t *rk, rd_kafka_resp_err_t err, ...) {
    if (err == ASSIGN) {
        // Initialize business state HERE - BAD
        load_customer_data();
        connect_to_external_api();
        // These operations can fail/timeout during rebalance!
    }
}

// ✓ CORRECT: Keep rebalance callback minimal
void rebalance_cb(rd_kafka_t *rk, rd_kafka_resp_err_t err, ...) {
    if (err == REVOKE) {
        rd_kafka_commit(rk, NULL, 0);  // Just commit
    }
    rd_kafka_assign(rk, ...);  // Just assign
}

// Do business initialization separately
void on_partition_assigned(int partition) {
    // Called from worker thread, can take time
    load_state_for_partition(partition);
}
```

---

## 中文解释 (Chinese Explanations)

### 1. 在实际 C 项目中使用 Kafka

**生产者模块设计要点：**
- 封装 librdkafka，提供简洁的 API
- 内置背压处理（队列满时阻塞或返回错误）
- 跟踪 in-flight 消息数量
- 提供同步和异步发送选项
- 正确处理投递回调

**消费者工作池设计要点：**
- 单独的轮询线程
- 多个工作线程处理消息
- 线程安全的工作队列
- 有界队列提供自然背压
- 周期性提交偏移量
- 处理再平衡回调

### 2. 背压处理策略

**生产者背压：**
1. 队列满时阻塞
2. 主动监控队列深度
3. 基于回调跟踪 in-flight

**消费者背压：**
1. 暂停分区（pause_partitions）
2. 有界工作队列（自然阻塞）
3. 令牌桶限流

### 3. 偏移量提交策略

**至少一次（推荐默认）：**
- 先处理，后提交
- 崩溃时消息可能重新投递
- 要求处理幂等

**至多一次：**
- 先提交，后处理
- 崩溃时消息可能丢失
- 用于指标、日志

**精确一次（外部存储）：**
- 处理结果和偏移量在同一事务中
- 重启时从数据库读取偏移量
- 不使用 Kafka 偏移量存储

### 4. 常见陷阱

1. **错误的偏移量提交**：忘记 +1
2. **轮询循环中阻塞**：长时间处理导致心跳超时
3. **超时配置错误**：默认超时不适合慢消费者
4. **分区数量不当**：过多或过少都有问题
5. **忽略再平衡回调**：未在撤销时提交偏移量

### 5. 反模式

1. **把 Kafka 当队列**：期望消费后删除
2. **假设精确一次**：不理解事务就假设无重复
3. **把 Kafka 当数据库**：期望按键查询
4. **业务逻辑混入再平衡**：在回调中做重操作
