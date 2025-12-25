# Ring Buffer (Circular Buffer) in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE RING BUFFER: FIXED-SIZE STREAMING DATA                      |
+------------------------------------------------------------------+

    PROBLEMS SOLVED:
    ┌─────────────────────────────────────────────────────────────┐
    │  1. BOUNDED MEMORY: Fixed buffer, no dynamic allocation     │
    │  2. CONSTANT TIME: O(1) read and write, always              │
    │  3. STREAMING DATA: Continuous data flow without copying    │
    │  4. PRODUCER-CONSUMER: Natural FIFO with fixed buffer       │
    │  5. LOCK-FREE POTENTIAL: Single-producer single-consumer    │
    └─────────────────────────────────────────────────────────────┘

    KEY INSIGHT: "Wrap around" instead of shifting data
    
    LINEAR BUFFER PROBLEM:
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ X │ X │ X │ A │ B │ C │ ? │ ? │
    └───┴───┴───┴───┴───┴───┴───┴───┘
                ▲           ▲
              read        write
    
    Front space wasted! Eventually must shift or wrap.

    RING BUFFER SOLUTION:
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ D │ E │ ? │ ? │ A │ B │ C │ ? │  ← Wraps around!
    └───┴───┴───┴───┴───┴───┴───┴───┘
          ▲           ▲
        write       read
    
    No wasted space, no shifting, O(1) forever.
```

**中文解释：**
- **环形缓冲区**：固定大小的缓冲区，写入到达末尾时回绕到开头
- 解决问题：有界内存、常数时间、流式数据、生产者-消费者
- 关键思想："回绕"而不是移动数据

### Design Philosophy

```
+------------------------------------------------------------------+
|  RING BUFFER DESIGN PRINCIPLES                                   |
+------------------------------------------------------------------+

    1. CIRCULAR ADDRESSING
       ┌─────────────────────────────────────────────────────────┐
       │  Logically infinite, physically finite                  │
       │  next_index = (index + 1) % capacity                    │
       │  With power-of-2: next = (index + 1) & mask             │
       └─────────────────────────────────────────────────────────┘

    2. PRODUCER-CONSUMER SEPARATION
       ┌─────────────────────────────────────────────────────────┐
       │  Producer only advances write pointer                   │
       │  Consumer only advances read pointer                    │
       │  No data movement, only pointer movement                │
       └─────────────────────────────────────────────────────────┘

    3. FULL VS EMPTY DISAMBIGUATION
       ┌─────────────────────────────────────────────────────────┐
       │  Problem: read == write means full OR empty?            │
       │                                                          │
       │  Solutions:                                              │
       │  a) Keep separate count (simplest)                      │
       │  b) Waste one slot (capacity-1 usable)                  │
       │  c) Use wrap-around counter (lock-free pattern)         │
       └─────────────────────────────────────────────────────────┘
```

### Invariants

```
+------------------------------------------------------------------+
|  RING BUFFER INVARIANTS                                          |
+------------------------------------------------------------------+

    1. BOUNDED SIZE
       0 ≤ count ≤ capacity
       Buffer never grows or shrinks

    2. INDEX BOUNDS
       0 ≤ read < capacity
       0 ≤ write < capacity
       Indices always wrap within bounds

    3. FIFO ORDER
       Data comes out in same order it went in
       No reordering, no random access (conceptually)

    4. SPACE CALCULATION
       used = count (if tracking count)
       used = (write - read + capacity) % capacity (if not)
       free = capacity - used (or capacity - 1 if wasting slot)
```

---

## 2. Memory Model

### Physical Layout

```
+------------------------------------------------------------------+
|  RING BUFFER MEMORY LAYOUT                                       |
+------------------------------------------------------------------+

    struct ring_buffer {
        uint8_t *buffer;    /* Contiguous memory block */
        size_t capacity;    /* Total size in bytes */
        size_t read_idx;    /* Next byte to read */
        size_t write_idx;   /* Next byte to write */
        size_t count;       /* Bytes currently stored */
    };

    PHYSICAL ARRAY (capacity = 8):
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │  ← indices
    └───┴───┴───┴───┴───┴───┴───┴───┘

    LOGICAL VIEW (circular):
    
              ┌───┐
            7 │   │ 0
             ╲│   │╱
          6 ──┤   ├── 1
             ╱│   │╲
            5 │   │ 2
              └───┘
             4   3

    Data written 0→1→2→3→4→5→6→7→0→1→... (wraps)
```

### State Examples

```
+------------------------------------------------------------------+
|  RING BUFFER STATES                                              |
+------------------------------------------------------------------+

    EMPTY (count = 0):
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ ? │ ? │ ? │ ? │ ? │ ? │ ? │ ? │
    └───┴───┴───┴───┴───┴───┴───┴───┘
      ▲
     R/W (read == write, count == 0)

    PARTIALLY FULL (count = 3):
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ ? │ ? │ A │ B │ C │ ? │ ? │ ? │
    └───┴───┴───┴───┴───┴───┴───┴───┘
              ▲           ▲
              R           W

    WRAPPED (count = 5):
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ E │ ? │ ? │ ? │ ? │ A │ B │ C │ D at [0] already read
    └───┴───┴───┴───┴───┴───┴───┴───┘
          ▲               ▲
          W               R

    FULL (count = capacity):
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ H │ A │ B │ C │ D │ E │ F │ G │
    └───┴───┴───┴───┴───┴───┴───┴───┘
          ▲
         R/W (read == write, but count == capacity)
```

**中文解释：**
- **空**：read == write 且 count == 0
- **满**：read == write 且 count == capacity
- **回绕**：写指针在读指针"之前"（物理位置），数据跨越数组边界

### Power-of-2 Optimization

```
+------------------------------------------------------------------+
|  POWER-OF-2 OPTIMIZATION                                         |
+------------------------------------------------------------------+

    WHY POWER OF 2?
    ┌─────────────────────────────────────────────────────────────┐
    │  index % capacity  → SLOW (division)                        │
    │  index & (capacity - 1) → FAST (bitwise AND)               │
    │                                                              │
    │  Example: capacity = 8 (0b1000), mask = 7 (0b0111)          │
    │  index = 10 → 10 & 7 = 0b1010 & 0b0111 = 0b0010 = 2        │
    │  Same as: 10 % 8 = 2                                        │
    └─────────────────────────────────────────────────────────────┘

    IMPLEMENTATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  struct ring_buffer {                                       │
    │      uint8_t *buffer;                                       │
    │      size_t mask;      /* capacity - 1 */                   │
    │      size_t read_idx;                                       │
    │      size_t write_idx;                                      │
    │  };                                                         │
    │                                                              │
    │  /* Wrap index */                                           │
    │  new_idx = (idx + n) & rb->mask;  /* Instead of % */        │
    └─────────────────────────────────────────────────────────────┘

    PERFORMANCE GAIN:
    ┌─────────────────────────────────────────────────────────────┐
    │  Division: ~20-80 cycles (varies by CPU)                    │
    │  Bitwise AND: 1 cycle                                       │
    │  Up to 80× faster per index calculation!                    │
    └─────────────────────────────────────────────────────────────┘
```

### Memory Ordering for Lock-Free

```
+------------------------------------------------------------------+
|  LOCK-FREE SINGLE-PRODUCER SINGLE-CONSUMER                       |
+------------------------------------------------------------------+

    KEY INSIGHT:
    ┌─────────────────────────────────────────────────────────────┐
    │  If only ONE producer and ONE consumer:                     │
    │  - Producer only writes to write_idx                        │
    │  - Consumer only writes to read_idx                         │
    │  - No mutex needed! (with proper memory ordering)           │
    └─────────────────────────────────────────────────────────────┘

    MEMORY BARRIERS:
    ┌─────────────────────────────────────────────────────────────┐
    │  Producer:                                                  │
    │    1. Write data to buffer[write_idx]                       │
    │    2. WRITE BARRIER (ensure data visible before index)      │
    │    3. Update write_idx                                      │
    │                                                              │
    │  Consumer:                                                  │
    │    1. Read write_idx                                        │
    │    2. READ BARRIER (ensure fresh data read)                 │
    │    3. Read data from buffer[read_idx]                       │
    │    4. Update read_idx                                       │
    └─────────────────────────────────────────────────────────────┘

    LINUX KERNEL:
    ┌─────────────────────────────────────────────────────────────┐
    │  Uses smp_store_release() / smp_load_acquire()              │
    │  See: include/linux/kfifo.h                                 │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Typical Application Scenarios

### Where Ring Buffers Are Used

```
+------------------------------------------------------------------+
|  REAL-WORLD RING BUFFER APPLICATIONS                             |
+------------------------------------------------------------------+

    KERNEL SPACE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Network packet buffers (NAPI, virtio)                    │
    │  • TTY/serial line buffers                                  │
    │  • Logging buffers (printk, trace)                          │
    │  • DMA transfer buffers                                     │
    │  • kfifo (kernel FIFO primitive)                            │
    │  • perf event buffers                                       │
    └─────────────────────────────────────────────────────────────┘

    USER SPACE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Audio/video streaming (jitter buffers)                   │
    │  • Network socket buffers                                   │
    │  • Logging frameworks                                       │
    │  • Message passing (ZeroMQ, Disruptor)                      │
    │  • Sensor data collection                                   │
    │  • Real-time signal processing                              │
    └─────────────────────────────────────────────────────────────┘

    EMBEDDED SYSTEMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • UART receive/transmit buffers                            │
    │  • ADC sample buffers                                       │
    │  • CAN message queues                                       │
    │  • Interrupt-to-main communication                          │
    └─────────────────────────────────────────────────────────────┘
```

### When to Use Ring Buffer

```
+------------------------------------------------------------------+
|  USE RING BUFFER WHEN:                                           |
+------------------------------------------------------------------+

    ✓ Fixed memory budget (no dynamic allocation)
    ✓ Streaming data (continuous read/write)
    ✓ Producer-consumer pattern
    ✓ Need O(1) guaranteed operations
    ✓ Single-producer single-consumer (can be lock-free)
    ✓ Overwrite-oldest-on-full is acceptable
    ✓ Real-time systems (predictable timing)

+------------------------------------------------------------------+
|  DON'T USE RING BUFFER WHEN:                                     |
+------------------------------------------------------------------+

    ✗ Need to preserve all data (no overwriting)
    ✗ Variable-length messages (fragmentation issues)
    ✗ Random access to elements
    ✗ Need to remove items from middle
    ✗ Multiple consumers competing for same data
```

---

## 4. Complete C Examples

### Example 1: Basic Ring Buffer

```c
/*
 * Example 1: Basic Ring Buffer
 *
 * Simple byte-oriented ring buffer with count tracking
 * Compile: gcc -Wall -Wextra -o ring_basic ring_basic.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct ring_buffer {
    unsigned char *buffer;
    size_t capacity;
    size_t read_idx;
    size_t write_idx;
    size_t count;
};

int ring_init(struct ring_buffer *rb, size_t capacity)
{
    rb->buffer = malloc(capacity);
    if (!rb->buffer)
        return -1;
    
    rb->capacity = capacity;
    rb->read_idx = 0;
    rb->write_idx = 0;
    rb->count = 0;
    
    return 0;
}

void ring_destroy(struct ring_buffer *rb)
{
    free(rb->buffer);
    rb->buffer = NULL;
}

bool ring_empty(const struct ring_buffer *rb)
{
    return rb->count == 0;
}

bool ring_full(const struct ring_buffer *rb)
{
    return rb->count == rb->capacity;
}

size_t ring_space_used(const struct ring_buffer *rb)
{
    return rb->count;
}

size_t ring_space_free(const struct ring_buffer *rb)
{
    return rb->capacity - rb->count;
}

/* Write single byte */
bool ring_write_byte(struct ring_buffer *rb, unsigned char byte)
{
    if (ring_full(rb))
        return false;
    
    rb->buffer[rb->write_idx] = byte;
    rb->write_idx = (rb->write_idx + 1) % rb->capacity;
    rb->count++;
    
    return true;
}

/* Read single byte */
bool ring_read_byte(struct ring_buffer *rb, unsigned char *byte)
{
    if (ring_empty(rb))
        return false;
    
    *byte = rb->buffer[rb->read_idx];
    rb->read_idx = (rb->read_idx + 1) % rb->capacity;
    rb->count--;
    
    return true;
}

/* Write multiple bytes */
size_t ring_write(struct ring_buffer *rb, const void *data, size_t len)
{
    const unsigned char *src = data;
    size_t written = 0;
    
    while (written < len && ring_write_byte(rb, src[written]))
        written++;
    
    return written;
}

/* Read multiple bytes */
size_t ring_read(struct ring_buffer *rb, void *data, size_t len)
{
    unsigned char *dst = data;
    size_t read_count = 0;
    
    while (read_count < len && ring_read_byte(rb, &dst[read_count]))
        read_count++;
    
    return read_count;
}

/* Print buffer state (for debugging) */
void ring_print_state(const struct ring_buffer *rb)
{
    printf("Ring [cap=%zu, count=%zu, R=%zu, W=%zu]: ",
           rb->capacity, rb->count, rb->read_idx, rb->write_idx);
    
    for (size_t i = 0; i < rb->capacity; i++) {
        if (i == rb->read_idx && i == rb->write_idx)
            printf("[R/W:");
        else if (i == rb->read_idx)
            printf("[R:");
        else if (i == rb->write_idx)
            printf("[W:");
        else
            printf("[");
        
        /* Show content if in use range */
        size_t pos_from_read = (i + rb->capacity - rb->read_idx) % rb->capacity;
        if (pos_from_read < rb->count)
            printf("%c]", rb->buffer[i]);
        else
            printf(".]");
    }
    printf("\n");
}

int main(void)
{
    printf("=== Basic Ring Buffer Demo ===\n\n");
    
    struct ring_buffer rb;
    ring_init(&rb, 8);
    
    /* Write some data */
    printf("Writing 'Hello':\n");
    ring_write(&rb, "Hello", 5);
    ring_print_state(&rb);
    
    /* Read some */
    char buf[10];
    size_t n = ring_read(&rb, buf, 3);
    buf[n] = '\0';
    printf("\nRead %zu bytes: '%s'\n", n, buf);
    ring_print_state(&rb);
    
    /* Write more (wraps around) */
    printf("\nWriting '!!World' (wraps):\n");
    ring_write(&rb, "!!World", 7);
    ring_print_state(&rb);
    
    /* Read all remaining */
    n = ring_read(&rb, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    printf("\nRead %zu bytes: '%s'\n", n, buf);
    ring_print_state(&rb);
    
    ring_destroy(&rb);
    return 0;
}
```

---

### Example 2: Power-of-2 Optimized Ring Buffer

```c
/*
 * Example 2: Power-of-2 Optimized Ring Buffer
 *
 * Uses bitwise AND for fast index wrapping
 * Compile: gcc -Wall -Wextra -O2 -o ring_optimized ring_optimized.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct fast_ring {
    uint8_t *buffer;
    size_t mask;       /* capacity - 1 */
    size_t read_idx;
    size_t write_idx;
};

/* Ensure power of 2 */
static size_t roundup_pow2(size_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if SIZE_MAX > 0xFFFFFFFF
    n |= n >> 32;
#endif
    return n + 1;
}

int fast_ring_init(struct fast_ring *rb, size_t min_capacity)
{
    size_t capacity = roundup_pow2(min_capacity);
    
    rb->buffer = malloc(capacity);
    if (!rb->buffer)
        return -1;
    
    rb->mask = capacity - 1;
    rb->read_idx = 0;
    rb->write_idx = 0;
    
    printf("Requested %zu, allocated %zu (mask=0x%zx)\n",
           min_capacity, capacity, rb->mask);
    
    return 0;
}

void fast_ring_destroy(struct fast_ring *rb)
{
    free(rb->buffer);
}

/* Fast index wrap using bitwise AND */
static inline size_t wrap_index(struct fast_ring *rb, size_t idx)
{
    return idx & rb->mask;  /* Same as idx % capacity, but FAST */
}

static inline size_t fast_ring_capacity(struct fast_ring *rb)
{
    return rb->mask + 1;
}

static inline size_t fast_ring_count(struct fast_ring *rb)
{
    return (rb->write_idx - rb->read_idx) & rb->mask;
}

static inline bool fast_ring_empty(struct fast_ring *rb)
{
    return rb->read_idx == rb->write_idx;
}

static inline bool fast_ring_full(struct fast_ring *rb)
{
    return fast_ring_count(rb) == rb->mask;  /* capacity - 1 usable */
}

bool fast_ring_put(struct fast_ring *rb, uint8_t byte)
{
    if (fast_ring_full(rb))
        return false;
    
    rb->buffer[wrap_index(rb, rb->write_idx)] = byte;
    rb->write_idx++;  /* Let it overflow naturally! */
    
    return true;
}

bool fast_ring_get(struct fast_ring *rb, uint8_t *byte)
{
    if (fast_ring_empty(rb))
        return false;
    
    *byte = rb->buffer[wrap_index(rb, rb->read_idx)];
    rb->read_idx++;
    
    return true;
}

/* Bulk write - efficient for memcpy opportunities */
size_t fast_ring_write(struct fast_ring *rb, const uint8_t *data, size_t len)
{
    size_t written = 0;
    while (written < len && fast_ring_put(rb, data[written]))
        written++;
    return written;
}

/* Bulk read */
size_t fast_ring_read(struct fast_ring *rb, uint8_t *data, size_t len)
{
    size_t n = 0;
    while (n < len && fast_ring_get(rb, &data[n]))
        n++;
    return n;
}

int main(void)
{
    printf("=== Power-of-2 Optimized Ring Buffer ===\n\n");
    
    struct fast_ring rb;
    fast_ring_init(&rb, 10);  /* Will be rounded to 16 */
    
    printf("\nCapacity: %zu, Mask: 0x%zx\n",
           fast_ring_capacity(&rb), rb.mask);
    
    /* Fill buffer */
    printf("\nFilling buffer:\n");
    for (int i = 0; i < 20; i++) {
        if (fast_ring_put(&rb, 'A' + i)) {
            printf("  Put '%c': count=%zu\n", 'A' + i, fast_ring_count(&rb));
        } else {
            printf("  Put '%c': FULL (count=%zu)\n", 'A' + i, fast_ring_count(&rb));
        }
    }
    
    /* Read some */
    printf("\nReading 5 bytes:\n  ");
    uint8_t byte;
    for (int i = 0; i < 5; i++) {
        if (fast_ring_get(&rb, &byte))
            printf("%c ", byte);
    }
    printf("\n");
    
    /* Write more */
    printf("\nWriting 'XYZ':\n");
    const char *msg = "XYZ";
    size_t n = fast_ring_write(&rb, (const uint8_t *)msg, 3);
    printf("  Wrote %zu bytes, count=%zu\n", n, fast_ring_count(&rb));
    
    /* Read all */
    printf("\nReading all:\n  ");
    while (fast_ring_get(&rb, &byte))
        printf("%c", byte);
    printf("\n");
    
    fast_ring_destroy(&rb);
    return 0;
}
```

---

### Example 3: Lock-Free SPSC Ring Buffer

```c
/*
 * Example 3: Lock-Free Single-Producer Single-Consumer Ring Buffer
 *
 * Uses memory barriers for thread safety without locks
 * Compile: gcc -Wall -Wextra -O2 -o ring_spsc ring_spsc.c -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

struct spsc_ring {
    uint8_t *buffer;
    size_t mask;
    
    /* Separate cache lines to avoid false sharing */
    _Alignas(64) atomic_size_t write_idx;
    _Alignas(64) atomic_size_t read_idx;
};

int spsc_init(struct spsc_ring *rb, size_t capacity)
{
    /* Must be power of 2 */
    if ((capacity & (capacity - 1)) != 0) {
        fprintf(stderr, "Capacity must be power of 2\n");
        return -1;
    }
    
    rb->buffer = malloc(capacity);
    if (!rb->buffer)
        return -1;
    
    rb->mask = capacity - 1;
    atomic_store(&rb->write_idx, 0);
    atomic_store(&rb->read_idx, 0);
    
    return 0;
}

void spsc_destroy(struct spsc_ring *rb)
{
    free(rb->buffer);
}

/* Producer: write one byte */
bool spsc_produce(struct spsc_ring *rb, uint8_t byte)
{
    size_t write = atomic_load_explicit(&rb->write_idx, memory_order_relaxed);
    size_t read = atomic_load_explicit(&rb->read_idx, memory_order_acquire);
    
    /* Check if full */
    if (((write + 1) & rb->mask) == (read & rb->mask))
        return false;
    
    /* Write data */
    rb->buffer[write & rb->mask] = byte;
    
    /* Publish: make data visible before incrementing write_idx */
    atomic_store_explicit(&rb->write_idx, write + 1, memory_order_release);
    
    return true;
}

/* Consumer: read one byte */
bool spsc_consume(struct spsc_ring *rb, uint8_t *byte)
{
    size_t read = atomic_load_explicit(&rb->read_idx, memory_order_relaxed);
    size_t write = atomic_load_explicit(&rb->write_idx, memory_order_acquire);
    
    /* Check if empty */
    if (read == write)
        return false;
    
    /* Read data (write_idx already synchronized above) */
    *byte = rb->buffer[read & rb->mask];
    
    /* Publish: make slot available */
    atomic_store_explicit(&rb->read_idx, read + 1, memory_order_release);
    
    return true;
}

size_t spsc_count(struct spsc_ring *rb)
{
    size_t write = atomic_load(&rb->write_idx);
    size_t read = atomic_load(&rb->read_idx);
    return (write - read) & rb->mask;
}

/* ═══════════════════════════════════════════════════════════════
 * Thread example
 * ═══════════════════════════════════════════════════════════════ */

struct spsc_ring g_ring;
atomic_bool g_done = false;

void *producer_thread(void *arg)
{
    (void)arg;
    
    for (int i = 0; i < 100; i++) {
        uint8_t byte = 'A' + (i % 26);
        
        /* Spin until we can produce */
        while (!spsc_produce(&g_ring, byte)) {
            /* Buffer full, wait a bit */
            usleep(100);
        }
        
        if (i % 25 == 0)
            printf("Producer: wrote up to %d\n", i + 1);
    }
    
    atomic_store(&g_done, true);
    printf("Producer: done\n");
    return NULL;
}

void *consumer_thread(void *arg)
{
    (void)arg;
    int count = 0;
    uint8_t byte;
    
    while (!atomic_load(&g_done) || spsc_count(&g_ring) > 0) {
        if (spsc_consume(&g_ring, &byte)) {
            count++;
            if (count % 25 == 0)
                printf("Consumer: read %d (last: '%c')\n", count, byte);
        } else {
            /* Buffer empty, wait a bit */
            usleep(50);
        }
    }
    
    printf("Consumer: done, read %d total\n", count);
    return NULL;
}

int main(void)
{
    printf("=== Lock-Free SPSC Ring Buffer ===\n\n");
    
    if (spsc_init(&g_ring, 16) < 0) {
        fprintf(stderr, "Failed to create ring\n");
        return 1;
    }
    
    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer_thread, NULL);
    pthread_create(&cons, NULL, consumer_thread, NULL);
    
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    
    spsc_destroy(&g_ring);
    printf("\nNo locks used! Only atomic operations.\n");
    
    return 0;
}
```

---

### Example 4: Overwrite-On-Full Ring Buffer

```c
/*
 * Example 4: Overwrite-On-Full Ring Buffer
 *
 * For logging/tracing where newest data is more important
 * Compile: gcc -Wall -Wextra -o ring_overwrite ring_overwrite.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

struct log_entry {
    time_t timestamp;
    int level;
    char message[64];
};

struct log_ring {
    struct log_entry *entries;
    size_t capacity;
    size_t write_idx;   /* Next write position */
    size_t count;       /* Valid entries (up to capacity) */
};

int log_ring_init(struct log_ring *lr, size_t capacity)
{
    lr->entries = malloc(capacity * sizeof(struct log_entry));
    if (!lr->entries)
        return -1;
    
    lr->capacity = capacity;
    lr->write_idx = 0;
    lr->count = 0;
    
    return 0;
}

void log_ring_destroy(struct log_ring *lr)
{
    free(lr->entries);
}

/* Write entry, overwriting oldest if full */
void log_ring_write(struct log_ring *lr, int level, const char *msg)
{
    struct log_entry *entry = &lr->entries[lr->write_idx];
    
    entry->timestamp = time(NULL);
    entry->level = level;
    strncpy(entry->message, msg, sizeof(entry->message) - 1);
    entry->message[sizeof(entry->message) - 1] = '\0';
    
    lr->write_idx = (lr->write_idx + 1) % lr->capacity;
    
    if (lr->count < lr->capacity)
        lr->count++;
    /* If count == capacity, oldest entry was just overwritten */
}

/* Get read index for iteration */
size_t log_ring_read_start(const struct log_ring *lr)
{
    if (lr->count < lr->capacity) {
        return 0;  /* Not wrapped yet */
    } else {
        return lr->write_idx;  /* Oldest is where we'll write next */
    }
}

/* Iterate through entries (oldest to newest) */
void log_ring_print_all(const struct log_ring *lr)
{
    if (lr->count == 0) {
        printf("(empty log)\n");
        return;
    }
    
    size_t read_idx = log_ring_read_start(lr);
    
    printf("Log entries (oldest first):\n");
    for (size_t i = 0; i < lr->count; i++) {
        const struct log_entry *e = &lr->entries[read_idx];
        char time_buf[32];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&e->timestamp));
        
        printf("  [%s] L%d: %s\n", time_buf, e->level, e->message);
        
        read_idx = (read_idx + 1) % lr->capacity;
    }
}

int main(void)
{
    printf("=== Overwrite-On-Full Ring Buffer (Logging) ===\n\n");
    
    struct log_ring log;
    log_ring_init(&log, 5);  /* Small buffer for demo */
    
    printf("Buffer capacity: 5 entries\n\n");
    
    /* Write 8 entries (will overwrite oldest 3) */
    log_ring_write(&log, 1, "System starting");
    log_ring_write(&log, 1, "Loading config");
    log_ring_write(&log, 2, "Warning: low memory");
    log_ring_write(&log, 1, "Network connected");
    log_ring_write(&log, 3, "Error: disk full");
    
    printf("After 5 writes:\n");
    log_ring_print_all(&log);
    
    printf("\nWriting 3 more (oldest 3 will be overwritten):\n");
    log_ring_write(&log, 1, "Disk space cleared");
    log_ring_write(&log, 1, "Backup started");
    log_ring_write(&log, 1, "Backup completed");
    
    log_ring_print_all(&log);
    
    printf("\nNote: First 3 entries ('System starting', 'Loading config',\n");
    printf("      'Warning: low memory') were overwritten.\n");
    
    log_ring_destroy(&log);
    return 0;
}
```

---

### Example 5: Variable-Length Message Ring Buffer

```c
/*
 * Example 5: Variable-Length Message Ring Buffer
 *
 * Stores messages with length prefix
 * Compile: gcc -Wall -Wextra -o ring_varlen ring_varlen.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

struct msg_ring {
    uint8_t *buffer;
    size_t capacity;
    size_t read_idx;
    size_t write_idx;
    size_t used;
};

int msg_ring_init(struct msg_ring *mr, size_t capacity)
{
    mr->buffer = malloc(capacity);
    if (!mr->buffer)
        return -1;
    
    mr->capacity = capacity;
    mr->read_idx = 0;
    mr->write_idx = 0;
    mr->used = 0;
    
    return 0;
}

void msg_ring_destroy(struct msg_ring *mr)
{
    free(mr->buffer);
}

size_t msg_ring_free_space(const struct msg_ring *mr)
{
    return mr->capacity - mr->used;
}

/* Write single byte with wrap */
static void write_byte(struct msg_ring *mr, uint8_t b)
{
    mr->buffer[mr->write_idx] = b;
    mr->write_idx = (mr->write_idx + 1) % mr->capacity;
    mr->used++;
}

/* Read single byte with wrap */
static uint8_t read_byte(struct msg_ring *mr)
{
    uint8_t b = mr->buffer[mr->read_idx];
    mr->read_idx = (mr->read_idx + 1) % mr->capacity;
    mr->used--;
    return b;
}

/* Write message: [2-byte length][data] */
bool msg_ring_write(struct msg_ring *mr, const void *data, uint16_t len)
{
    /* Need 2 bytes for length + data bytes */
    if (msg_ring_free_space(mr) < (size_t)(len + 2))
        return false;
    
    /* Write length (big-endian) */
    write_byte(mr, (len >> 8) & 0xFF);
    write_byte(mr, len & 0xFF);
    
    /* Write data */
    const uint8_t *src = data;
    for (uint16_t i = 0; i < len; i++) {
        write_byte(mr, src[i]);
    }
    
    return true;
}

/* Read message: returns length, fills buffer */
int msg_ring_read(struct msg_ring *mr, void *buf, size_t buf_size)
{
    if (mr->used < 2)
        return -1;  /* Not enough for length header */
    
    /* Peek length without consuming */
    uint16_t len = ((uint16_t)mr->buffer[mr->read_idx] << 8) |
                    mr->buffer[(mr->read_idx + 1) % mr->capacity];
    
    if (mr->used < (size_t)(len + 2))
        return -1;  /* Incomplete message */
    
    if (len > buf_size)
        return -2;  /* Buffer too small */
    
    /* Consume length bytes */
    read_byte(mr);
    read_byte(mr);
    
    /* Read data */
    uint8_t *dst = buf;
    for (uint16_t i = 0; i < len; i++) {
        dst[i] = read_byte(mr);
    }
    
    return len;
}

/* Check if message available */
bool msg_ring_has_message(const struct msg_ring *mr)
{
    if (mr->used < 2)
        return false;
    
    uint16_t len = ((uint16_t)mr->buffer[mr->read_idx] << 8) |
                    mr->buffer[(mr->read_idx + 1) % mr->capacity];
    
    return mr->used >= (size_t)(len + 2);
}

int main(void)
{
    printf("=== Variable-Length Message Ring Buffer ===\n\n");
    
    struct msg_ring mr;
    msg_ring_init(&mr, 64);
    
    /* Write some messages */
    printf("Writing messages:\n");
    msg_ring_write(&mr, "Hello", 5);
    printf("  Wrote 'Hello' (5 bytes)\n");
    
    msg_ring_write(&mr, "World!", 6);
    printf("  Wrote 'World!' (6 bytes)\n");
    
    msg_ring_write(&mr, "This is a longer message", 24);
    printf("  Wrote 'This is a longer message' (24 bytes)\n");
    
    printf("\nBuffer used: %zu / %zu bytes\n\n", mr.used, mr.capacity);
    
    /* Read messages back */
    printf("Reading messages:\n");
    char buf[64];
    int len;
    
    while ((len = msg_ring_read(&mr, buf, sizeof(buf))) > 0) {
        buf[len] = '\0';
        printf("  Read (%d bytes): '%s'\n", len, buf);
    }
    
    printf("\nBuffer used after reading: %zu bytes\n", mr.used);
    
    msg_ring_destroy(&mr);
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

```
+------------------------------------------------------------------+
|  RING BUFFER DESIGN DECISIONS                                    |
+------------------------------------------------------------------+

    FULL/EMPTY DETECTION:
    ┌──────────────────────┬──────────────────────────────────────┐
    │ Method               │ Trade-off                            │
    ├──────────────────────┼──────────────────────────────────────┤
    │ Count variable       │ Simple, requires atomic for SPSC     │
    │ Waste one slot       │ Lock-free friendly, loses 1 slot     │
    │ Wrap counter         │ Complex, true lock-free possible     │
    └──────────────────────┴──────────────────────────────────────┘

    CAPACITY:
    ┌──────────────────────┬──────────────────────────────────────┐
    │ Arbitrary            │ Requires division (slow modulo)      │
    │ Power of 2           │ Fast bitwise AND, wastes some memory │
    └──────────────────────┴──────────────────────────────────────┘

    ELEMENT SIZE:
    ┌──────────────────────┬──────────────────────────────────────┐
    │ Fixed (byte/struct)  │ Simple, predictable                  │
    │ Variable length      │ Complex, fragmentation possible      │
    └──────────────────────┴──────────────────────────────────────┘
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|  RING BUFFER: KEY TAKEAWAYS                                      |
+------------------------------------------------------------------+

    CORE CONCEPT:
    Fixed-size array with wrapping read/write pointers
    O(1) operations, no data movement, no allocation

    KEY OPTIMIZATIONS:
    - Power-of-2 capacity for fast modulo (bitwise AND)
    - Cache-line alignment for lock-free SPSC
    - Separate read/write indices for parallelism

    BEST FOR:
    - Streaming data (audio, network, logs)
    - Producer-consumer patterns
    - Real-time systems (bounded, predictable)
    - Lock-free SPSC communication

    WATCH OUT FOR:
    - Full vs empty ambiguity (count or waste slot)
    - Variable-length data complexity
    - Memory ordering in lock-free code

    LINUX KERNEL EXAMPLE:
    include/linux/kfifo.h - production-quality ring buffer
```

**中文总结：**
- **核心概念**：固定大小数组，读写指针回绕，O(1) 操作
- **关键优化**：2的幂容量（快速取模）、缓存行对齐（无锁 SPSC）
- **最佳用途**：流式数据、生产者-消费者、实时系统
- **注意事项**：满/空区分、变长数据复杂性、无锁代码的内存序

