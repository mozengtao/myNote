# Ring Buffer (Lock-Free SPSC)

## 1. Technical Specification

### Characteristics

| Property | Value |
|----------|-------|
| Memory Layout | Contiguous, power-of-two sized, cache-aligned |
| Enqueue (produce) | O(1), wait-free |
| Dequeue (consume) | O(1), wait-free |
| Concurrency | Single-Producer Single-Consumer (SPSC) without locks |
| Synchronization | C11 `<stdatomic.h>` acquire/release semantics |
| Capacity | Power-of-two minus one (one slot wasted for full/empty distinction) |
| False sharing | Prevented via cache-line padding between head and tail |

### Use Cases

- **DPDK**: `rte_ring` — the core inter-thread packet passing mechanism.
- **Linux Kernel**: `kfifo` — lock-free kernel FIFO.
- **Audio/Video**: Real-time audio pipelines (PortAudio, JACK), frame buffers.
- **Logging**: Lock-free log buffers (high-throughput logging frameworks).
- **IPC**: Shared-memory communication between processes.
- **Networking**: NIC DMA descriptor rings, virtio vrings.

### Trade-offs

| vs. Locked Queue | Ring Buffer Wins | Locked Queue Wins |
|------------------|-----------------|------------------|
| Latency | No lock contention | N/A |
| Throughput | ~100M+ ops/sec | ~10M ops/sec |
| Complexity | Higher (memory ordering) | Simpler (mutex) |
| Flexibility | Fixed capacity, SPSC | MPMC, dynamic |

### Why Power-of-Two?

- **Fast modulo**: `index & (capacity - 1)` replaces expensive `%` division.
- **Alignment**: Natural alignment to cache lines (64 bytes).
- **Overflow safety**: Unsigned wrap-around with mask is always correct.

---

## 2. Implementation Strategy

```
ringbuf_t (capacity = 8, usable = 7)

  Padded to separate cache lines:
  +-- cacheline 0 --+-- cacheline 1 --+-- cacheline 2+ --+
  |  head (atomic)   |  tail (atomic)   |  buf[8] (void*) |
  |  pad[56 bytes]   |  pad[56 bytes]   |  mask, capacity  |
  +-----------------+-----------------+------------------+

  buf[]:  [ A | B | C |   |   |   |   |   ]
            ^head=0          ^tail=3

  Producer writes: buf[tail & mask] = item; atomic_store(tail+1)
  Consumer reads:  item = buf[head & mask]; atomic_store(head+1)

  Empty: head == tail
  Full:  tail - head == capacity (with power-of-two wrap)
```

### Memory Ordering

- **Producer** writes data first, then publishes `tail` with `memory_order_release`.
- **Consumer** reads `tail` with `memory_order_acquire`, then reads data.
- This guarantees the consumer sees the data the producer wrote.
- No CAS needed for SPSC — simple load/store with correct ordering.

---

## 3. Implementation

```c
/**
 * @file ringbuf.c
 * @brief Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
 *
 * Uses C11 atomics with acquire/release semantics.
 * Capacity must be a power of two. One slot is reserved to
 * distinguish full from empty, so usable capacity = size - 1.
 *
 * Cache-line padding prevents false sharing between producer
 * and consumer on separate cores.
 *
 * Standard: C11 (requires <stdatomic.h>)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdalign.h>
#include <errno.h>

#define RINGBUF_CACHELINE  64
#define RINGBUF_MIN_SIZE   4

/**
 * @brief Round up to the next power of two.
 */
static inline size_t
ringbuf_next_pow2(size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#if SIZE_MAX > 0xFFFFFFFFUL
    v |= v >> 32;
#endif
    v++;
    return v;
}

typedef struct ringbuf {
    /*
     * head and tail are on separate cache lines to prevent
     * false sharing between producer (tail) and consumer (head).
     */
    alignas(RINGBUF_CACHELINE) atomic_size_t head;
    char pad_head[RINGBUF_CACHELINE - sizeof(atomic_size_t)];

    alignas(RINGBUF_CACHELINE) atomic_size_t tail;
    char pad_tail[RINGBUF_CACHELINE - sizeof(atomic_size_t)];

    void   **buf;
    size_t   mask;       /* capacity - 1, for fast modulo */
    size_t   capacity;   /* power of two */
} ringbuf_t;

/**
 * @brief Create a ring buffer.
 * @param min_size Minimum number of usable slots. Rounded up to next power of two.
 *                 Actual usable capacity = returned_capacity - 1.
 * @return New ring buffer, or NULL on failure.
 */
static ringbuf_t *
ringbuf_create(size_t min_size)
{
    ringbuf_t *rb;
    size_t cap;

    if (min_size < RINGBUF_MIN_SIZE)
        min_size = RINGBUF_MIN_SIZE;

    /* +1 because one slot is wasted for full/empty detection */
    cap = ringbuf_next_pow2(min_size + 1);

    rb = aligned_alloc(RINGBUF_CACHELINE, sizeof(*rb));
    if (rb == NULL)
        return NULL;
    memset(rb, 0, sizeof(*rb));

    rb->buf = aligned_alloc(RINGBUF_CACHELINE, cap * sizeof(void *));
    if (rb->buf == NULL) {
        free(rb);
        return NULL;
    }
    memset(rb->buf, 0, cap * sizeof(void *));

    rb->capacity = cap;
    rb->mask     = cap - 1;
    atomic_init(&rb->head, 0);
    atomic_init(&rb->tail, 0);

    return rb;
}

/**
 * @brief Destroy the ring buffer.
 *
 * Does NOT free remaining elements — caller must drain first
 * or accept the leak. This is intentional: the ring buffer
 * does not own the data (no destructor callback), matching
 * the pattern of DPDK rte_ring and Linux kfifo.
 */
static void
ringbuf_destroy(ringbuf_t *rb)
{
    if (rb == NULL)
        return;

    free(rb->buf);
    free(rb);
}

/**
 * @brief Enqueue one element (producer side).
 *
 * @param rb   The ring buffer.
 * @param item Pointer to enqueue (must not be NULL for safety).
 * @return 0 on success, -1 if full.
 *
 * Thread safety: Call from the single producer thread only.
 */
static int
ringbuf_enqueue(ringbuf_t *rb, void *item)
{
    size_t head, tail, next_tail;

    if (rb == NULL) {
        errno = EINVAL;
        return -1;
    }

    tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    next_tail = (tail + 1) & rb->mask;

    head = atomic_load_explicit(&rb->head, memory_order_acquire);
    if (next_tail == head)
        return -1;  /* full */

    rb->buf[tail] = item;

    /*
     * Release fence: ensures buf[tail] write is visible
     * before the consumer sees the updated tail.
     */
    atomic_store_explicit(&rb->tail, next_tail, memory_order_release);

    return 0;
}

/**
 * @brief Dequeue one element (consumer side).
 *
 * @param rb  The ring buffer.
 * @param out Receives the dequeued pointer.
 * @return 0 on success, -1 if empty.
 *
 * Thread safety: Call from the single consumer thread only.
 */
static int
ringbuf_dequeue(ringbuf_t *rb, void **out)
{
    size_t head, tail;

    if (rb == NULL || out == NULL) {
        errno = EINVAL;
        return -1;
    }

    head = atomic_load_explicit(&rb->head, memory_order_relaxed);

    /*
     * Acquire fence: ensures we see the data the producer
     * wrote before publishing this tail value.
     */
    tail = atomic_load_explicit(&rb->tail, memory_order_acquire);
    if (head == tail)
        return -1;  /* empty */

    *out = rb->buf[head];

    atomic_store_explicit(&rb->head, (head + 1) & rb->mask,
                          memory_order_release);

    return 0;
}

/**
 * @brief Bulk enqueue up to n elements.
 * @param items Array of pointers to enqueue.
 * @param n     Number of items to enqueue.
 * @return Number of items actually enqueued (0 to n).
 */
static size_t
ringbuf_enqueue_bulk(ringbuf_t *rb, void *const *items, size_t n)
{
    size_t head, tail, free_slots, i;

    if (rb == NULL || items == NULL)
        return 0;

    tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    head = atomic_load_explicit(&rb->head, memory_order_acquire);

    free_slots = (rb->capacity + head - tail - 1) & rb->mask;
    if (n > free_slots)
        n = free_slots;

    for (i = 0; i < n; i++)
        rb->buf[(tail + i) & rb->mask] = items[i];

    atomic_store_explicit(&rb->tail, (tail + n) & rb->mask,
                          memory_order_release);

    return n;
}

/**
 * @brief Bulk dequeue up to n elements.
 * @param out Array to receive dequeued pointers.
 * @param n   Max number of items to dequeue.
 * @return Number of items actually dequeued (0 to n).
 */
static size_t
ringbuf_dequeue_bulk(ringbuf_t *rb, void **out, size_t n)
{
    size_t head, tail, avail, i;

    if (rb == NULL || out == NULL)
        return 0;

    head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    avail = (tail - head) & rb->mask;
    if (n > avail)
        n = avail;

    for (i = 0; i < n; i++)
        out[i] = rb->buf[(head + i) & rb->mask];

    atomic_store_explicit(&rb->head, (head + n) & rb->mask,
                          memory_order_release);

    return n;
}

/** @brief Number of elements currently in the ring. */
static inline size_t
ringbuf_count(const ringbuf_t *rb)
{
    if (rb == NULL)
        return 0;

    size_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);

    return (tail - head) & rb->mask;
}

/** @brief Usable capacity (capacity - 1). */
static inline size_t
ringbuf_capacity(const ringbuf_t *rb)
{
    return rb ? rb->capacity - 1 : 0;
}

static inline int
ringbuf_full(const ringbuf_t *rb)
{
    return ringbuf_count(rb) == ringbuf_capacity(rb);
}

static inline int
ringbuf_empty(const ringbuf_t *rb)
{
    return ringbuf_count(rb) == 0;
}

/*
 * === Example / Self-test ===
 */
#ifdef RINGBUF_TEST
#include <assert.h>
#include <pthread.h>

#define TEST_COUNT  1000000

static ringbuf_t *g_rb;

static void *
producer_thread(void *arg)
{
    size_t i;
    (void)arg;

    for (i = 0; i < TEST_COUNT; i++) {
        while (ringbuf_enqueue(g_rb, (void *)(uintptr_t)(i + 1)) != 0) {
            /* spin until space available */
        }
    }

    return NULL;
}

static void *
consumer_thread(void *arg)
{
    size_t i, expected;
    void  *item;
    (void)arg;

    for (i = 0; i < TEST_COUNT; i++) {
        while (ringbuf_dequeue(g_rb, &item) != 0) {
            /* spin until data available */
        }
        expected = i + 1;
        assert((uintptr_t)item == expected);
    }

    return NULL;
}

int
main(void)
{
    pthread_t prod, cons;

    /* --- Single-threaded basic test --- */
    {
        ringbuf_t *rb;
        void *out;
        int i;

        rb = ringbuf_create(8);
        assert(rb != NULL);
        assert(ringbuf_capacity(rb) >= 8);

        for (i = 1; i <= 7; i++)
            assert(ringbuf_enqueue(rb, (void *)(uintptr_t)i) == 0);

        assert(ringbuf_count(rb) == 7);

        for (i = 1; i <= 7; i++) {
            assert(ringbuf_dequeue(rb, &out) == 0);
            assert((uintptr_t)out == (uintptr_t)i);
        }

        assert(ringbuf_empty(rb));
        assert(ringbuf_dequeue(rb, &out) == -1);

        /* Bulk operations */
        {
            void *items[4] = {
                (void *)10, (void *)20, (void *)30, (void *)40
            };
            void *results[4];

            assert(ringbuf_enqueue_bulk(rb, items, 4) == 4);
            assert(ringbuf_count(rb) == 4);

            assert(ringbuf_dequeue_bulk(rb, results, 4) == 4);
            assert((uintptr_t)results[0] == 10);
            assert((uintptr_t)results[3] == 40);
        }

        ringbuf_destroy(rb);
        printf("ringbuf: single-threaded tests passed\n");
    }

    /* --- Multi-threaded SPSC test --- */
    g_rb = ringbuf_create(1024);
    assert(g_rb != NULL);

    pthread_create(&prod, NULL, producer_thread, NULL);
    pthread_create(&cons, NULL, consumer_thread, NULL);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    assert(ringbuf_empty(g_rb));
    ringbuf_destroy(g_rb);
    printf("ringbuf: SPSC multi-threaded test passed (%d ops)\n", TEST_COUNT);

    return 0;
}
#endif /* RINGBUF_TEST */
```

Compile and test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -pthread -DRINGBUF_TEST -o test_ringbuf ringbuf.c && ./test_ringbuf
```

---

## 4. Memory / ASCII Visualization

### Ring Buffer Physical Layout

```
Memory layout (64-bit system, RINGBUF_CACHELINE = 64):

Offset  Contents                            Cache Line
------  ------------------------------------  ----------
0x000   [  atomic head  |  pad (56 bytes)  ]  Line 0 (consumer-owned)
0x040   [  atomic tail  |  pad (56 bytes)  ]  Line 1 (producer-owned)
0x080   [  buf pointer  |  mask | capacity ]  Line 2 (read-only shared)
0x0C0   buf[0] buf[1] buf[2] ... buf[cap-1]  Lines 3+

False sharing prevented: head and tail never share a cache line.
```

### SPSC Protocol Diagram

```
Producer (Core 0)                  Consumer (Core 1)
=================                  =================

1. Load tail (relaxed)
2. Compute next_tail
3. Load head (acquire) --------->  
4. Check: full?                    
5. Write buf[tail] = item          
6. Store tail (release) --------->  1. Load head (relaxed)
                                    2. Load tail (acquire) <-- sees new tail
                                    3. Check: empty?
                                    4. Read item = buf[head]  <-- sees data
                                    5. Store head (release) ----+
                                                                |
   Load head (acquire) <------------ new head visible  <--------+

   RELEASE on store = "flush all prior writes"
   ACQUIRE on load  = "don't reorder reads before this"
```

### Circular Index Wrapping

```
capacity = 8, mask = 0x7

Index sequence:  0 -> 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 0 -> 1 -> ...
                 |_______________________________________________|
                                wraps naturally

Physical:        tail=13 -> actual index = 13 & 0x7 = 5
                 head=10 -> actual index = 10 & 0x7 = 2

buf[]:  [   |   | A | B | C | D |   |   ]
               ^head=2           ^tail=6 (logical: 10, 13)

count = (tail - head) & mask = (13 - 10) & 7 = 3 & 7 = 3  (items: A,B,C... wait, 4)

Actually: count = (6 - 2) & 7 = 4. Contains: buf[2],buf[3],buf[4],buf[5]
```

### Bulk Enqueue

```
ringbuf_enqueue_bulk(rb, items[4], 4):

Before:
  buf[]:  [   |   |   |   |   |   |   |   ]
  head=0, tail=0, mask=7

Step 1: Compute free_slots = (cap + head - tail - 1) & mask = 7
Step 2: n=4 <= 7, proceed
Step 3: Write buf[0..3]:
  buf[]:  [ A | B | C | D |   |   |   |   ]
Step 4: atomic_store(tail, 4, release)
  head=0, tail=4

After: Consumer can now see 4 items.
```

### State Transitions

```
EMPTY: head == tail
+---+---+---+---+---+---+---+---+
|   |   |   |   |   |   |   |   |    h=0, t=0
+---+---+---+---+---+---+---+---+
 ^h,t

PARTIAL: head != tail, next_tail != head
+---+---+---+---+---+---+---+---+
| A | B | C |   |   |   |   |   |    h=0, t=3
+---+---+---+---+---+---+---+---+
 ^h          ^t

FULL: (tail + 1) & mask == head
+---+---+---+---+---+---+---+---+
| A | B | C | D | E | F | G |   |    h=0, t=7
+---+---+---+---+---+---+---+---+
 ^h                          ^t
 One slot always empty (sentinel for full detection)
 Usable capacity = 7 (for cap=8)

WRAP-AROUND:
+---+---+---+---+---+---+---+---+
|   |   | C | D | E |   |   |   |    h=2, t=5
+---+---+---+---+---+---+---+---+
         ^h          ^t

+---+---+---+---+---+---+---+---+
| H |   |   |   |   | F | G |   |    h=5, t=1 (wrapped)
+---+---+---+---+---+---+---+---+
  ^t                 ^h
```

### Comparison with DPDK `rte_ring`

```
+-------------------+------------------+------------------+
| Feature           | This ringbuf     | DPDK rte_ring    |
+-------------------+------------------+------------------+
| Concurrency       | SPSC only        | SPSC/MPSC/MPMC   |
| Element type      | void *           | void * (+ custom)|
| Size constraint   | Power-of-two     | Power-of-two     |
| Memory ordering   | acquire/release  | acquire/release  |
| Bulk ops          | Yes              | Yes (burst)      |
| Watermarks        | No               | Yes              |
| NUMA awareness    | No               | Yes              |
+-------------------+------------------+------------------+
```
