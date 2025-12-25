# Array (Static and Dynamic) in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE FUNDAMENTAL PROBLEM: INDEXED ACCESS                         |
+------------------------------------------------------------------+

    Given N elements, how do we:
    1. Access ANY element by position in O(1) time?
    2. Store elements contiguously for cache efficiency?
    3. Minimize memory overhead per element?

    ANSWER: ARRAY — the simplest, most fundamental data structure

    ┌─────────────────────────────────────────────────────────────┐
    │  Array = Base Address + (Index × Element Size)              │
    │                                                             │
    │  This formula enables O(1) random access!                   │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 数组解决的核心问题：如何在 O(1) 时间内通过索引访问任意元素
- 关键公式：`地址 = 基址 + 索引 × 元素大小`
- 这是最基础、最高效的数据结构

### Invariants

```
+------------------------------------------------------------------+
|  ARRAY INVARIANTS                                                |
+------------------------------------------------------------------+

    1. CONTIGUOUS MEMORY
       All elements occupy consecutive memory addresses
       No gaps between elements

    2. FIXED ELEMENT SIZE
       sizeof(element) is constant for all elements
       This enables O(1) address calculation

    3. ZERO-BASED INDEXING (in C)
       First element at index 0
       Last element at index (size - 1)

    4. BOUNDS
       Static array: size fixed at compile time
       Dynamic array: capacity can change, but at a cost
```

### Design Philosophy

```
+------------------------------------------------------------------+
|  WHY ARRAYS ARE SHAPED THIS WAY                                  |
+------------------------------------------------------------------+

    HARDWARE ALIGNMENT:
    ┌─────────────────────────────────────────────────────────────┐
    │  • CPU cache lines are typically 64 bytes                   │
    │  • Sequential memory access is FAST (prefetching)           │
    │  • Random access to distant memory is SLOW                  │
    │  • Arrays exploit this: iterate = cache hits!               │
    └─────────────────────────────────────────────────────────────┘

    SIMPLICITY:
    ┌─────────────────────────────────────────────────────────────┐
    │  • No pointers between elements                             │
    │  • No metadata per element                                  │
    │  • Memory = N × sizeof(element)  (exactly)                  │
    │  • Minimal overhead = maximum efficiency                    │
    └─────────────────────────────────────────────────────────────┘
```

---

## 2. Memory Model

### Static Array Memory Layout

```
+------------------------------------------------------------------+
|  STATIC ARRAY MEMORY LAYOUT                                      |
+------------------------------------------------------------------+

    Declaration:
    int arr[5] = {10, 20, 30, 40, 50};

    Memory (assuming sizeof(int) = 4 bytes):

    Address:    0x1000    0x1004    0x1008    0x100C    0x1010
                ┌─────────┬─────────┬─────────┬─────────┬─────────┐
    Value:      │   10    │   20    │   30    │   40    │   50    │
                └─────────┴─────────┴─────────┴─────────┴─────────┘
    Index:         [0]       [1]       [2]       [3]       [4]

    Total size: 5 × 4 = 20 bytes (contiguous, no gaps)

    Address calculation:
    &arr[i] = (char*)arr + i * sizeof(int)
            = 0x1000 + i * 4

    Examples:
    &arr[0] = 0x1000 + 0 * 4 = 0x1000
    &arr[3] = 0x1000 + 3 * 4 = 0x100C
```

**中文解释：**
- 静态数组在内存中连续存储
- 地址计算：基址 + 索引 × 元素大小
- 无额外开销，纯数据存储

### Dynamic Array Memory Layout

```
+------------------------------------------------------------------+
|  DYNAMIC ARRAY MEMORY LAYOUT                                     |
+------------------------------------------------------------------+

    Declaration:
    int *arr = malloc(5 * sizeof(int));

    Stack (pointer variable):         Heap (actual data):
    ┌──────────────┐                  ┌─────┬─────┬─────┬─────┬─────┐
    │ arr = 0x2000 │ ───────────────▶ │ 10  │ 20  │ 30  │ 40  │ 50  │
    └──────────────┘                  └─────┴─────┴─────┴─────┴─────┘
    (8 bytes on x64)                  0x2000                    0x2010

    OWNERSHIP MODEL:
    ┌─────────────────────────────────────────────────────────────┐
    │  • malloc() allocates heap memory                           │
    │  • The CALLER owns this memory                              │
    │  • The CALLER must call free()                              │
    │  • Forgetting free() = MEMORY LEAK                          │
    │  • Double free() = UNDEFINED BEHAVIOR                       │
    └─────────────────────────────────────────────────────────────┘
```

### Growable Dynamic Array (Vector Pattern)

```
+------------------------------------------------------------------+
|  GROWABLE ARRAY PATTERN                                          |
+------------------------------------------------------------------+

    struct vector {
        int *data;      // Pointer to heap array
        size_t size;    // Number of elements in use
        size_t capacity;// Total allocated slots
    };

    Memory layout:

    Stack (struct):                   Heap (data):
    ┌─────────────────────┐           ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ data = 0x3000       │ ────────▶ │ A │ B │ C │ ? │ ? │ ? │ ? │ ? │
    │ size = 3            │           └───┴───┴───┴───┴───┴───┴───┴───┘
    │ capacity = 8        │            used      unused (but allocated)
    └─────────────────────┘

    GROWTH STRATEGY:
    ┌─────────────────────────────────────────────────────────────┐
    │  When size == capacity:                                     │
    │  1. Allocate new array (2× capacity is common)              │
    │  2. Copy all elements to new array                          │
    │  3. Free old array                                          │
    │  4. Update pointer and capacity                             │
    │                                                             │
    │  Amortized O(1) append, but individual append can be O(n)!  │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- 可增长数组需要三个字段：指针、大小、容量
- 当数组满时，分配 2× 空间，复制数据，释放旧数组
- 摊销 O(1) 追加，但单次追加可能 O(n)

### Cache Behavior

```
+------------------------------------------------------------------+
|  CACHE AND LOCALITY                                              |
+------------------------------------------------------------------+

    CPU CACHE ARCHITECTURE (simplified):
    ┌─────────────────────────────────────────────────────────────┐
    │  L1 Cache: ~32KB, ~4 cycles latency                         │
    │  L2 Cache: ~256KB, ~12 cycles latency                       │
    │  L3 Cache: ~8MB, ~40 cycles latency                         │
    │  Main Memory: GB, ~200+ cycles latency                      │
    └─────────────────────────────────────────────────────────────┘

    CACHE LINE (typically 64 bytes):
    ┌────────────────────────────────────────────────────────────────┐
    │  When you access arr[0], CPU loads entire cache line:         │
    │                                                                │
    │  ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬─┐│
    │  │arr[0]│arr[1]│arr[2]│arr[3]│arr[4]│arr[5]│...│arr[15]│    ││
    │  └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴─┘│
    │  ◄─────────────── 64 bytes (16 ints) ─────────────────────────▶ │
    │                                                                │
    │  Accessing arr[1] through arr[15] = CACHE HIT (free!)          │
    └────────────────────────────────────────────────────────────────┘

    ARRAY ITERATION IS CACHE-OPTIMAL:
    ┌─────────────────────────────────────────────────────────────┐
    │  for (int i = 0; i < n; i++)                                │
    │      sum += arr[i];                                         │
    │                                                              │
    │  • Sequential access triggers prefetching                   │
    │  • Each cache line gives 16 ints "for free"                 │
    │  • This is WHY arrays are fast!                             │
    └─────────────────────────────────────────────────────────────┘
```

### Failure Modes

```
+------------------------------------------------------------------+
|  COMMON FAILURE MODES                                            |
+------------------------------------------------------------------+

    1. BUFFER OVERFLOW (writing past end)
    ┌─────────────────────────────────────────────────────────────┐
    │  int arr[5];                                                │
    │  arr[5] = 100;  // UNDEFINED BEHAVIOR!                      │
    │                 // May corrupt adjacent memory              │
    │                 // May crash, may silently corrupt data     │
    └─────────────────────────────────────────────────────────────┘

    2. USE AFTER FREE
    ┌─────────────────────────────────────────────────────────────┐
    │  int *arr = malloc(10 * sizeof(int));                       │
    │  free(arr);                                                 │
    │  arr[0] = 42;  // UNDEFINED BEHAVIOR!                       │
    │                // Memory may be reused by another malloc    │
    └─────────────────────────────────────────────────────────────┘

    3. MEMORY LEAK
    ┌─────────────────────────────────────────────────────────────┐
    │  void leak(void) {                                          │
    │      int *arr = malloc(1000 * sizeof(int));                 │
    │      // No free() before returning!                         │
    │      // Memory is lost until program exits                  │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘

    4. DOUBLE FREE
    ┌─────────────────────────────────────────────────────────────┐
    │  int *arr = malloc(10 * sizeof(int));                       │
    │  free(arr);                                                 │
    │  free(arr);  // UNDEFINED BEHAVIOR!                         │
    │              // May corrupt heap metadata                   │
    │              // Security vulnerability!                     │
    └─────────────────────────────────────────────────────────────┘

    5. STACK OVERFLOW (large static arrays)
    ┌─────────────────────────────────────────────────────────────┐
    │  void dangerous(void) {                                     │
    │      int huge[10000000];  // ~40MB on stack!                │
    │                           // Stack typically 1-8MB          │
    │                           // CRASH!                         │
    │  }                                                          │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- **缓冲区溢出**：写入数组边界外，可能破坏相邻内存
- **释放后使用**：访问已释放的内存，导致未定义行为
- **内存泄漏**：忘记 free()，内存无法回收
- **重复释放**：两次 free() 同一指针，破坏堆结构
- **栈溢出**：栈上分配大数组，超出栈限制

---

## 3. Typical Application Scenarios

### Where Arrays Are Used

```
+------------------------------------------------------------------+
|  REAL-WORLD ARRAY APPLICATIONS                                   |
+------------------------------------------------------------------+

    KERNEL SPACE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Page tables (fixed-size arrays indexed by virtual addr)  │
    │  • CPU runqueues (array of task lists per priority)         │
    │  • Interrupt vectors (array of handler pointers)            │
    │  • Per-CPU data arrays                                      │
    │  • Hash table buckets (array of list heads)                 │
    └─────────────────────────────────────────────────────────────┘

    USER SPACE:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Image processing (2D arrays of pixels)                   │
    │  • Audio buffers (arrays of samples)                        │
    │  • String storage (array of chars)                          │
    │  • Command-line arguments (argv[])                          │
    │  • Lookup tables (precomputed values)                       │
    │  • Matrices for scientific computing                        │
    └─────────────────────────────────────────────────────────────┘

    EMBEDDED SYSTEMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Sensor data buffers                                      │
    │  • DMA transfer buffers                                     │
    │  • Configuration tables                                     │
    │  • State machine transition tables                          │
    └─────────────────────────────────────────────────────────────┘
```

### When Array Is a GOOD Choice

```
+------------------------------------------------------------------+
|  USE ARRAYS WHEN:                                                |
+------------------------------------------------------------------+

    ✓ You need O(1) random access by index
    ✓ Size is known at compile time (static) or rarely changes
    ✓ You iterate sequentially (cache-friendly)
    ✓ Memory overhead must be minimal
    ✓ You need predictable memory layout
    ✓ You're implementing other data structures on top
```

### When Array Is a BAD Choice

```
+------------------------------------------------------------------+
|  AVOID ARRAYS WHEN:                                              |
+------------------------------------------------------------------+

    ✗ Frequent insertions/deletions in the middle (O(n) shift)
    ✗ Size varies unpredictably and frequently
    ✗ You need O(1) insertion at arbitrary positions
    ✗ Memory fragmentation is a concern (dynamic arrays)
    ✗ You need to find elements by value (O(n) search)
    ✗ Elements have complex ownership relationships
```

---

## 4. Complete C Examples

### Example 1: Static Array Basics

```c
/* 
 * Example 1: Static Array — Fundamentals
 * 
 * Demonstrates: declaration, initialization, iteration, bounds
 * Compile: gcc -Wall -Wextra -o static_array static_array.c
 */

#include <stdio.h>
#include <stddef.h>  /* for size_t */

/* 
 * CRITICAL: Use a macro to get array size safely
 * Only works for actual arrays, NOT pointers!
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int main(void)
{
    /* Static array: size fixed at compile time */
    int temperatures[7] = {72, 75, 68, 80, 77, 73, 69};
    
    /* Partial initialization: rest are zero */
    int partial[10] = {1, 2, 3};  /* [1,2,3,0,0,0,0,0,0,0] */
    
    /* Zero initialization */
    int zeros[5] = {0};  /* All zeros */
    
    /* ═══════════════════════════════════════════════════════════
     * DEMONSTRATION: Size and memory layout
     * ═══════════════════════════════════════════════════════════ */
    printf("=== Memory Layout ===\n");
    printf("Array size: %zu elements\n", ARRAY_SIZE(temperatures));
    printf("Total bytes: %zu\n", sizeof(temperatures));
    printf("Element size: %zu bytes\n", sizeof(temperatures[0]));
    printf("Base address: %p\n", (void*)temperatures);
    
    /* ═══════════════════════════════════════════════════════════
     * DEMONSTRATION: Address calculation
     * ═══════════════════════════════════════════════════════════ */
    printf("\n=== Address Calculation ===\n");
    for (size_t i = 0; i < ARRAY_SIZE(temperatures); i++) {
        printf("temperatures[%zu] at %p = %d\n", 
               i, (void*)&temperatures[i], temperatures[i]);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * DEMONSTRATION: Iteration patterns
     * ═══════════════════════════════════════════════════════════ */
    printf("\n=== Iteration Patterns ===\n");
    
    /* Pattern 1: Index-based (most common) */
    int sum = 0;
    for (size_t i = 0; i < ARRAY_SIZE(temperatures); i++) {
        sum += temperatures[i];
    }
    printf("Sum (index-based): %d\n", sum);
    
    /* Pattern 2: Pointer-based */
    sum = 0;
    int *end = temperatures + ARRAY_SIZE(temperatures);
    for (int *p = temperatures; p < end; p++) {
        sum += *p;
    }
    printf("Sum (pointer-based): %d\n", sum);
    
    /* ═══════════════════════════════════════════════════════════
     * DEMONSTRATION: Partial and zero init verification
     * ═══════════════════════════════════════════════════════════ */
    printf("\n=== Initialization Verification ===\n");
    printf("partial: ");
    for (size_t i = 0; i < ARRAY_SIZE(partial); i++) {
        printf("%d ", partial[i]);
    }
    printf("\nzeros: ");
    for (size_t i = 0; i < ARRAY_SIZE(zeros); i++) {
        printf("%d ", zeros[i]);
    }
    printf("\n");
    
    return 0;
}
```

**Output:**
```
=== Memory Layout ===
Array size: 7 elements
Total bytes: 28
Element size: 4 bytes
Base address: 0x7ffd12345678

=== Address Calculation ===
temperatures[0] at 0x7ffd12345678 = 72
temperatures[1] at 0x7ffd1234567c = 75
...

=== Iteration Patterns ===
Sum (index-based): 514
Sum (pointer-based): 514

=== Initialization Verification ===
partial: 1 2 3 0 0 0 0 0 0 0
zeros: 0 0 0 0 0 0 0 0 0 0
```

---

### Example 2: Dynamic Array with malloc/realloc

```c
/*
 * Example 2: Dynamic Array — Heap Allocation
 *
 * Demonstrates: malloc, realloc, free, error handling
 * Compile: gcc -Wall -Wextra -o dynamic_array dynamic_array.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Dynamic array with explicit capacity management
 */
struct dyn_array {
    int *data;       /* Pointer to heap-allocated storage */
    size_t size;     /* Number of elements currently stored */
    size_t capacity; /* Total allocated slots */
};

/*
 * Initialize empty dynamic array
 * Returns 0 on success, -1 on failure
 */
int dyn_array_init(struct dyn_array *arr, size_t initial_capacity)
{
    if (initial_capacity == 0)
        initial_capacity = 4;  /* Sensible default */
    
    arr->data = malloc(initial_capacity * sizeof(int));
    if (!arr->data)
        return -1;  /* Allocation failed */
    
    arr->size = 0;
    arr->capacity = initial_capacity;
    return 0;
}

/*
 * Free all resources
 */
void dyn_array_destroy(struct dyn_array *arr)
{
    free(arr->data);
    arr->data = NULL;  /* Prevent use-after-free bugs */
    arr->size = 0;
    arr->capacity = 0;
}

/*
 * Grow array capacity (internal helper)
 * Returns 0 on success, -1 on failure
 */
static int dyn_array_grow(struct dyn_array *arr)
{
    size_t new_capacity = arr->capacity * 2;
    if (new_capacity < arr->capacity)  /* Overflow check */
        return -1;
    
    int *new_data = realloc(arr->data, new_capacity * sizeof(int));
    if (!new_data)
        return -1;  /* realloc failed, original data preserved! */
    
    arr->data = new_data;
    arr->capacity = new_capacity;
    return 0;
}

/*
 * Append element to end
 * Returns 0 on success, -1 on failure
 */
int dyn_array_push(struct dyn_array *arr, int value)
{
    if (arr->size >= arr->capacity) {
        if (dyn_array_grow(arr) < 0)
            return -1;
    }
    
    arr->data[arr->size++] = value;
    return 0;
}

/*
 * Remove and return last element
 * Returns the value (caller must ensure array is non-empty)
 */
int dyn_array_pop(struct dyn_array *arr)
{
    if (arr->size == 0) {
        fprintf(stderr, "ERROR: pop from empty array\n");
        abort();  /* Fail loudly in debug */
    }
    return arr->data[--arr->size];
}

/*
 * Get element at index (with bounds check)
 * Returns pointer to element, or NULL if out of bounds
 */
int *dyn_array_get(struct dyn_array *arr, size_t index)
{
    if (index >= arr->size)
        return NULL;
    return &arr->data[index];
}

int main(void)
{
    struct dyn_array arr;
    
    /* ═══════════════════════════════════════════════════════════
     * Initialize with small capacity to demonstrate growth
     * ═══════════════════════════════════════════════════════════ */
    if (dyn_array_init(&arr, 2) < 0) {
        fprintf(stderr, "Failed to initialize array\n");
        return 1;
    }
    
    printf("Initial: size=%zu, capacity=%zu\n", arr.size, arr.capacity);
    
    /* ═══════════════════════════════════════════════════════════
     * Push elements, observe automatic growth
     * ═══════════════════════════════════════════════════════════ */
    for (int i = 0; i < 10; i++) {
        if (dyn_array_push(&arr, i * 10) < 0) {
            fprintf(stderr, "Push failed\n");
            dyn_array_destroy(&arr);
            return 1;
        }
        printf("After push(%d): size=%zu, capacity=%zu\n",
               i * 10, arr.size, arr.capacity);
    }
    
    /* ═══════════════════════════════════════════════════════════
     * Access elements
     * ═══════════════════════════════════════════════════════════ */
    printf("\nElements: ");
    for (size_t i = 0; i < arr.size; i++) {
        int *val = dyn_array_get(&arr, i);
        if (val)
            printf("%d ", *val);
    }
    printf("\n");
    
    /* ═══════════════════════════════════════════════════════════
     * Pop elements
     * ═══════════════════════════════════════════════════════════ */
    printf("\nPopping: ");
    while (arr.size > 0) {
        printf("%d ", dyn_array_pop(&arr));
    }
    printf("\n");
    
    /* ═══════════════════════════════════════════════════════════
     * CRITICAL: Always cleanup!
     * ═══════════════════════════════════════════════════════════ */
    dyn_array_destroy(&arr);
    
    return 0;
}
```

**Output:**
```
Initial: size=0, capacity=2
After push(0): size=1, capacity=2
After push(10): size=2, capacity=2
After push(20): size=3, capacity=4    ← Grew!
After push(30): size=4, capacity=4
After push(40): size=5, capacity=8    ← Grew!
After push(50): size=6, capacity=8
After push(60): size=7, capacity=8
After push(70): size=8, capacity=8
After push(80): size=9, capacity=16   ← Grew!
After push(90): size=10, capacity=16

Elements: 0 10 20 30 40 50 60 70 80 90

Popping: 90 80 70 60 50 40 30 20 10 0
```

---

### Example 3: Real-World Use Case — Ring Buffer

```c
/*
 * Example 3: Ring Buffer (Circular Buffer)
 *
 * Real-world use: Audio streaming, network packet buffers,
 * producer-consumer queues, logging systems
 *
 * Compile: gcc -Wall -Wextra -o ring_buffer ring_buffer.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/*
 * Ring buffer structure
 * Uses "waste one slot" strategy to distinguish full from empty
 */
struct ring_buffer {
    unsigned char *buffer;  /* Storage */
    size_t capacity;        /* Total slots (power of 2 for efficiency) */
    size_t head;            /* Write position */
    size_t tail;            /* Read position */
};

/*
 * Check if n is power of 2
 */
static inline bool is_power_of_2(size_t n)
{
    return n && !(n & (n - 1));
}

/*
 * Initialize ring buffer
 * capacity MUST be power of 2 for efficient modulo
 */
int ring_buffer_init(struct ring_buffer *rb, size_t capacity)
{
    if (!is_power_of_2(capacity)) {
        fprintf(stderr, "Capacity must be power of 2\n");
        return -1;
    }
    
    rb->buffer = malloc(capacity);
    if (!rb->buffer)
        return -1;
    
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    return 0;
}

void ring_buffer_destroy(struct ring_buffer *rb)
{
    free(rb->buffer);
    rb->buffer = NULL;
}

/*
 * Fast modulo for power-of-2 capacity
 * This is WHY we require power of 2!
 */
static inline size_t ring_mask(struct ring_buffer *rb, size_t index)
{
    return index & (rb->capacity - 1);  /* Equivalent to index % capacity */
}

/*
 * Check if buffer is empty
 */
bool ring_buffer_empty(struct ring_buffer *rb)
{
    return rb->head == rb->tail;
}

/*
 * Check if buffer is full
 * (One slot is always empty to distinguish full from empty)
 */
bool ring_buffer_full(struct ring_buffer *rb)
{
    return ring_mask(rb, rb->head + 1) == rb->tail;
}

/*
 * Get number of bytes currently stored
 */
size_t ring_buffer_size(struct ring_buffer *rb)
{
    return ring_mask(rb, rb->head - rb->tail);
}

/*
 * Write one byte
 * Returns true on success, false if full
 */
bool ring_buffer_write(struct ring_buffer *rb, unsigned char byte)
{
    if (ring_buffer_full(rb))
        return false;
    
    rb->buffer[rb->head] = byte;
    rb->head = ring_mask(rb, rb->head + 1);
    return true;
}

/*
 * Read one byte
 * Returns true on success, false if empty
 */
bool ring_buffer_read(struct ring_buffer *rb, unsigned char *byte)
{
    if (ring_buffer_empty(rb))
        return false;
    
    *byte = rb->buffer[rb->tail];
    rb->tail = ring_mask(rb, rb->tail + 1);
    return true;
}

/*
 * Write multiple bytes (for bulk operations)
 * Returns number of bytes written
 */
size_t ring_buffer_write_bulk(struct ring_buffer *rb, 
                               const unsigned char *data, 
                               size_t len)
{
    size_t written = 0;
    while (written < len && ring_buffer_write(rb, data[written]))
        written++;
    return written;
}

/*
 * Demonstration
 */
int main(void)
{
    struct ring_buffer rb;
    
    /* Initialize with capacity 8 (power of 2) */
    if (ring_buffer_init(&rb, 8) < 0) {
        fprintf(stderr, "Failed to create ring buffer\n");
        return 1;
    }
    
    printf("=== Ring Buffer Demo ===\n");
    printf("Capacity: %zu (usable: %zu)\n", rb.capacity, rb.capacity - 1);
    
    /* ═══════════════════════════════════════════════════════════
     * Write data
     * ═══════════════════════════════════════════════════════════ */
    const char *msg = "Hello!";
    size_t written = ring_buffer_write_bulk(&rb, 
                                             (const unsigned char*)msg,
                                             strlen(msg));
    printf("Wrote %zu bytes: \"%s\"\n", written, msg);
    printf("Buffer size: %zu\n", ring_buffer_size(&rb));
    
    /* ═══════════════════════════════════════════════════════════
     * Read some data
     * ═══════════════════════════════════════════════════════════ */
    printf("\nReading 3 bytes: ");
    for (int i = 0; i < 3; i++) {
        unsigned char c;
        if (ring_buffer_read(&rb, &c))
            printf("'%c' ", c);
    }
    printf("\nBuffer size after read: %zu\n", ring_buffer_size(&rb));
    
    /* ═══════════════════════════════════════════════════════════
     * Write more (wrapping around)
     * ═══════════════════════════════════════════════════════════ */
    const char *more = "World";
    written = ring_buffer_write_bulk(&rb,
                                      (const unsigned char*)more,
                                      strlen(more));
    printf("\nWrote %zu bytes: \"%s\"\n", written, more);
    printf("Buffer size: %zu\n", ring_buffer_size(&rb));
    
    /* ═══════════════════════════════════════════════════════════
     * Read all remaining
     * ═══════════════════════════════════════════════════════════ */
    printf("\nReading all: ");
    unsigned char c;
    while (ring_buffer_read(&rb, &c)) {
        printf("'%c' ", c);
    }
    printf("\n");
    
    ring_buffer_destroy(&rb);
    return 0;
}
```

**Output:**
```
=== Ring Buffer Demo ===
Capacity: 8 (usable: 7)
Wrote 6 bytes: "Hello!"
Buffer size: 6

Reading 3 bytes: 'H' 'e' 'l' 
Buffer size after read: 3

Wrote 4 bytes: "World"    ← Only 4 fit (capacity - 1 - 3 = 4)
Buffer size: 7

Reading all: 'l' 'o' '!' 'W' 'o' 'r' 'l'
```

---

### Example 4: 2D Array Patterns

```c
/*
 * Example 4: 2D Arrays — Multiple Allocation Patterns
 *
 * Demonstrates: contiguous vs pointer-to-pointer, row-major order
 * Compile: gcc -Wall -Wextra -o array_2d array_2d.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ═══════════════════════════════════════════════════════════════
 * PATTERN 1: Static 2D Array (True Contiguous)
 * ═══════════════════════════════════════════════════════════════
 *
 * Memory layout (3x4 matrix):
 *
 *   [0][0] [0][1] [0][2] [0][3] [1][0] [1][1] [1][2] [1][3] [2][0] [2][1] [2][2] [2][3]
 *   ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
 *   │  0   │  1   │  2   │  3   │  4   │  5   │  6   │  7   │  8   │  9   │  10  │  11  │
 *   └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
 *   ◄─────────── row 0 ──────────────▶◄─────────── row 1 ──────────────▶◄─── row 2 ────▶
 *
 * This is ROW-MAJOR order (C default)
 */
void demo_static_2d(void)
{
    printf("=== Pattern 1: Static 2D Array ===\n");
    
    int matrix[3][4] = {
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {8, 9, 10, 11}
    };
    
    /* Show memory layout */
    printf("Memory layout (contiguous):\n");
    int *flat = (int*)matrix;  /* Treat as 1D array */
    for (int i = 0; i < 12; i++) {
        printf("%2d ", flat[i]);
    }
    printf("\n\n");
    
    /* Address calculation: &matrix[r][c] = base + (r * cols + c) * sizeof(int) */
    printf("Address calculation demo:\n");
    printf("&matrix[0][0] = %p\n", (void*)&matrix[0][0]);
    printf("&matrix[1][2] = %p (base + (1*4+2)*4 = base + 24)\n", 
           (void*)&matrix[1][2]);
    printf("Difference: %td bytes\n\n", 
           (char*)&matrix[1][2] - (char*)&matrix[0][0]);
}

/*
 * ═══════════════════════════════════════════════════════════════
 * PATTERN 2: Pointer-to-Pointer (Jagged Array)
 * ═══════════════════════════════════════════════════════════════
 *
 * Memory layout:
 *
 *   rows (int**)           Each row (int*)
 *   ┌──────────┐
 *   │ rows[0]  │ ──────▶ ┌───┬───┬───┬───┐
 *   ├──────────┤         │ 0 │ 1 │ 2 │ 3 │
 *   │ rows[1]  │ ──────▶ └───┴───┴───┴───┘
 *   ├──────────┤         ┌───┬───┬───┬───┐
 *   │ rows[2]  │ ──────▶ │ 4 │ 5 │ 6 │ 7 │
 *   └──────────┘         └───┴───┴───┴───┘
 *                        ┌───┬───┬───┬───┐
 *                        │ 8 │ 9 │10 │11 │
 *                        └───┴───┴───┴───┘
 *
 * DISADVANTAGES:
 * - Multiple allocations (fragmentation)
 * - Extra pointer indirection (slower)
 * - Cache unfriendly (rows may be scattered)
 * - More complex allocation/deallocation
 *
 * ADVANTAGES:
 * - Rows can have different lengths (jagged)
 * - Can swap rows by swapping pointers
 */
int **alloc_2d_jagged(int rows, int cols)
{
    int **matrix = malloc(rows * sizeof(int*));
    if (!matrix)
        return NULL;
    
    for (int r = 0; r < rows; r++) {
        matrix[r] = malloc(cols * sizeof(int));
        if (!matrix[r]) {
            /* Cleanup on failure */
            for (int i = 0; i < r; i++)
                free(matrix[i]);
            free(matrix);
            return NULL;
        }
    }
    return matrix;
}

void free_2d_jagged(int **matrix, int rows)
{
    if (!matrix)
        return;
    for (int r = 0; r < rows; r++)
        free(matrix[r]);
    free(matrix);
}

void demo_jagged_2d(void)
{
    printf("=== Pattern 2: Pointer-to-Pointer (Jagged) ===\n");
    
    int rows = 3, cols = 4;
    int **matrix = alloc_2d_jagged(rows, cols);
    if (!matrix) {
        printf("Allocation failed\n");
        return;
    }
    
    /* Initialize */
    int val = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            matrix[r][c] = val++;
    
    /* Show that rows may not be contiguous */
    printf("Row addresses (may not be contiguous):\n");
    for (int r = 0; r < rows; r++) {
        printf("  matrix[%d] at %p: ", r, (void*)matrix[r]);
        for (int c = 0; c < cols; c++)
            printf("%2d ", matrix[r][c]);
        printf("\n");
    }
    printf("\n");
    
    free_2d_jagged(matrix, rows);
}

/*
 * ═══════════════════════════════════════════════════════════════
 * PATTERN 3: Single Allocation with Index Mapping (Best!)
 * ═══════════════════════════════════════════════════════════════
 *
 * Memory layout:
 *
 *   ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
 *   │ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │ 9 │10 │11 │
 *   └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
 *
 * Access: matrix[r * cols + c]
 *
 * ADVANTAGES:
 * - Single allocation (no fragmentation)
 * - Fully contiguous (cache-optimal)
 * - Simple allocation/deallocation
 * - Best for matrices!
 */
struct matrix {
    int *data;
    int rows;
    int cols;
};

int matrix_init(struct matrix *m, int rows, int cols)
{
    m->data = malloc(rows * cols * sizeof(int));
    if (!m->data)
        return -1;
    m->rows = rows;
    m->cols = cols;
    return 0;
}

void matrix_destroy(struct matrix *m)
{
    free(m->data);
    m->data = NULL;
}

/* Access macro (inline for performance) */
#define MATRIX_AT(m, r, c) ((m)->data[(r) * (m)->cols + (c)])

void demo_contiguous_2d(void)
{
    printf("=== Pattern 3: Single Contiguous Allocation ===\n");
    
    struct matrix m;
    if (matrix_init(&m, 3, 4) < 0) {
        printf("Allocation failed\n");
        return;
    }
    
    /* Initialize */
    int val = 0;
    for (int r = 0; r < m.rows; r++)
        for (int c = 0; c < m.cols; c++)
            MATRIX_AT(&m, r, c) = val++;
    
    /* Display */
    printf("Matrix contents:\n");
    for (int r = 0; r < m.rows; r++) {
        printf("  ");
        for (int c = 0; c < m.cols; c++) {
            printf("%2d ", MATRIX_AT(&m, r, c));
        }
        printf("\n");
    }
    
    /* Show memory is truly contiguous */
    printf("\nContiguous memory: ");
    for (int i = 0; i < m.rows * m.cols; i++) {
        printf("%d ", m.data[i]);
    }
    printf("\n\n");
    
    matrix_destroy(&m);
}

int main(void)
{
    demo_static_2d();
    demo_jagged_2d();
    demo_contiguous_2d();
    
    printf("=== Recommendation ===\n");
    printf("For matrices: Use Pattern 3 (single contiguous allocation)\n");
    printf("For jagged data: Use Pattern 2 (pointer-to-pointer)\n");
    printf("For compile-time known size: Use Pattern 1 (static)\n");
    
    return 0;
}
```

---

### Example 5: Array Misuse Examples (Anti-Patterns)

```c
/*
 * Example 5: Array Misuse — What NOT to Do
 *
 * Demonstrates: common bugs and undefined behavior
 * Compile: gcc -Wall -Wextra -fsanitize=address -o array_bugs array_bugs.c
 *          (AddressSanitizer will catch many of these!)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * BUG 1: Buffer Overflow (Write)
 * ═══════════════════════════════════════════════════════════════ */
void bug_buffer_overflow(void)
{
    printf("=== BUG 1: Buffer Overflow ===\n");
    
    int arr[5];
    
    /* WRONG: Writing past the end */
    for (int i = 0; i <= 5; i++) {  /* Should be i < 5 */
        arr[i] = i;  /* arr[5] is OUT OF BOUNDS! */
    }
    
    /*
     * What happens:
     * - May corrupt adjacent stack variables
     * - May corrupt return address (security vulnerability!)
     * - May appear to "work" (silent corruption)
     * - Undefined behavior means anything can happen
     */
    printf("This might crash, or silently corrupt memory!\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 2: Using sizeof on Pointer Instead of Array
 * ═══════════════════════════════════════════════════════════════ */
void process_array(int *arr)  /* arr is a POINTER, not array! */
{
    /* WRONG: This gives pointer size, not array size! */
    size_t wrong_size = sizeof(arr) / sizeof(arr[0]);
    printf("Wrong size (pointer): %zu (expected 10)\n", wrong_size);
    
    /* 
     * On 64-bit: sizeof(int*) = 8, sizeof(int) = 4
     * Result: 8/4 = 2 (WRONG!)
     */
}

void bug_sizeof_decay(void)
{
    printf("=== BUG 2: Array-to-Pointer Decay ===\n");
    
    int arr[10];
    
    /* Correct: sizeof works on actual array */
    size_t correct_size = sizeof(arr) / sizeof(arr[0]);
    printf("Correct size (array): %zu\n", correct_size);
    
    /* But when passed to function, array decays to pointer */
    process_array(arr);
    
    printf("\nFIX: Always pass size as separate parameter!\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 3: Returning Pointer to Local Array
 * ═══════════════════════════════════════════════════════════════ */
int *bug_return_local(void)
{
    int local_arr[5] = {1, 2, 3, 4, 5};
    
    /* WRONG: Returning pointer to stack memory! */
    return local_arr;
    
    /*
     * What happens:
     * - local_arr lives on stack
     * - When function returns, stack frame is destroyed
     * - Caller gets pointer to invalid (freed) memory
     * - Reading/writing = undefined behavior
     */
}

void demo_return_local(void)
{
    printf("=== BUG 3: Returning Local Array ===\n");
    
    int *ptr = bug_return_local();
    
    /* This is undefined behavior! */
    printf("ptr[0] = %d (garbage or crash!)\n", ptr[0]);
    
    printf("\nFIX: Use malloc() or pass buffer as parameter!\n\n");
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 4: Memory Leak in Resize
 * ═══════════════════════════════════════════════════════════════ */
void bug_realloc_leak(void)
{
    printf("=== BUG 4: realloc Memory Leak ===\n");
    
    int *arr = malloc(10 * sizeof(int));
    if (!arr) return;
    
    /* WRONG: Direct assignment loses old pointer if realloc fails */
    arr = realloc(arr, 1000000000 * sizeof(int));  /* Likely to fail */
    
    if (!arr) {
        printf("realloc failed, and we LOST the original pointer!\n");
        printf("Memory leak! Cannot free original allocation.\n");
    }
    
    printf("\nFIX: Use temporary pointer:\n");
    printf("  int *new_arr = realloc(arr, new_size);\n");
    printf("  if (new_arr) arr = new_arr;\n\n");
    
    /* Note: arr is either NULL or huge allocation, either way we try to free */
    free(arr);
}

/* ═══════════════════════════════════════════════════════════════
 * BUG 5: Off-by-One in String Operations
 * ═══════════════════════════════════════════════════════════════ */
void bug_string_overflow(void)
{
    printf("=== BUG 5: String Buffer Overflow ===\n");
    
    char buf[10];
    const char *long_string = "This string is way too long!";
    
    /* WRONG: strcpy doesn't check bounds */
    /* strcpy(buf, long_string);  // BUFFER OVERFLOW! */
    
    /* Still wrong: strncpy doesn't guarantee null termination */
    strncpy(buf, long_string, sizeof(buf));
    /* buf may not be null-terminated! */
    
    printf("strncpy result (may not be null-terminated): ");
    /* This could read past buf looking for null! */
    /* printf("%s\n", buf); */
    
    /* CORRECT: Manually ensure null termination */
    strncpy(buf, long_string, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';  /* Always terminate */
    printf("Safe result: %s\n\n", buf);
}

/* ═══════════════════════════════════════════════════════════════
 * CORRECT PATTERNS
 * ═══════════════════════════════════════════════════════════════ */
void correct_patterns(void)
{
    printf("=== CORRECT PATTERNS ===\n\n");
    
    /* 1. Always pass size with array */
    printf("1. void process(int *arr, size_t n);\n");
    
    /* 2. Use ARRAY_SIZE macro only on actual arrays */
    printf("2. #define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))\n");
    
    /* 3. For strings, use snprintf */
    char buf[10];
    snprintf(buf, sizeof(buf), "%s", "Safe truncation!");
    printf("3. snprintf for strings: \"%s\"\n", buf);
    
    /* 4. Check allocation and handle NULL */
    printf("4. Always check malloc/realloc return value\n");
    
    /* 5. Set pointer to NULL after free */
    int *ptr = malloc(sizeof(int));
    free(ptr);
    ptr = NULL;  /* Prevents double-free bugs */
    printf("5. Set ptr = NULL after free()\n");
}

int main(void)
{
    /* Uncomment each to see the bug */
    /* bug_buffer_overflow(); */
    bug_sizeof_decay();
    /* demo_return_local(); */  /* Undefined behavior */
    bug_realloc_leak();
    bug_string_overflow();
    correct_patterns();
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

### Time Complexity

```
+------------------------------------------------------------------+
|  ARRAY OPERATION COMPLEXITY                                      |
+------------------------------------------------------------------+

    ┌─────────────────────────┬───────────────┬──────────────────────┐
    │ Operation               │ Complexity    │ Real Cost            │
    ├─────────────────────────┼───────────────┼──────────────────────┤
    │ Access by index         │ O(1)          │ 1 memory load        │
    │ Search (unsorted)       │ O(n)          │ n comparisons        │
    │ Search (sorted, binary) │ O(log n)      │ log n cache misses   │
    │ Insert at end           │ O(1) amortized│ Copy on resize       │
    │ Insert at beginning     │ O(n)          │ Shift n elements     │
    │ Insert in middle        │ O(n)          │ Shift n/2 elements   │
    │ Delete at end           │ O(1)          │ Just decrement size  │
    │ Delete at beginning     │ O(n)          │ Shift n elements     │
    │ Delete in middle        │ O(n)          │ Shift n/2 elements   │
    └─────────────────────────┴───────────────┴──────────────────────┘

    REAL-WORLD CONSIDERATIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • O(1) access is truly fast (single pointer arithmetic)   │
    │  • O(n) shift is cache-friendly (memmove is optimized)     │
    │  • Growing array copies all elements (expensive!)          │
    │  • For small n, O(n) may beat O(log n) due to cache        │
    └─────────────────────────────────────────────────────────────┘
```

### Memory Overhead

```
+------------------------------------------------------------------+
|  MEMORY OVERHEAD COMPARISON                                      |
+------------------------------------------------------------------+

    STATIC ARRAY:
    ┌─────────────────────────────────────────────────────────────┐
    │  Overhead: ZERO                                             │
    │  Memory = n × sizeof(element)                               │
    │  No pointers, no metadata, pure data                        │
    └─────────────────────────────────────────────────────────────┘

    DYNAMIC ARRAY (simple):
    ┌─────────────────────────────────────────────────────────────┐
    │  Overhead: ~16 bytes (malloc metadata) + unused capacity    │
    │  Wasted space: (capacity - size) × sizeof(element)          │
    │  With 2× growth: up to 50% waste                            │
    └─────────────────────────────────────────────────────────────┘

    LINKED LIST (for comparison):
    ┌─────────────────────────────────────────────────────────────┐
    │  Overhead: 8-16 bytes PER ELEMENT (pointers)                │
    │  For int list: 4 bytes data + 16 bytes pointers = 400% overhead!
    └─────────────────────────────────────────────────────────────┘
```

### Cache Friendliness

```
+------------------------------------------------------------------+
|  CACHE BEHAVIOR                                                  |
+------------------------------------------------------------------+

    ARRAY ITERATION:
    ┌─────────────────────────────────────────────────────────────┐
    │  Cache line: 64 bytes = 16 ints                             │
    │  Access arr[0]: miss, loads arr[0..15]                      │
    │  Access arr[1..15]: HIT HIT HIT... (15 free accesses!)      │
    │  Access arr[16]: miss, loads arr[16..31]                    │
    │  ... repeat ...                                             │
    │                                                              │
    │  Miss rate: 1/16 = 6.25%                                    │
    │  This is OPTIMAL for sequential access                      │
    └─────────────────────────────────────────────────────────────┘

    RANDOM ACCESS (within cache):
    ┌─────────────────────────────────────────────────────────────┐
    │  Small array (< L1 cache = 32KB):                           │
    │  After warmup, all accesses are cache hits                  │
    │  Random access ≈ sequential access in speed                 │
    └─────────────────────────────────────────────────────────────┘

    RANDOM ACCESS (large array):
    ┌─────────────────────────────────────────────────────────────┐
    │  Large array (>> cache):                                    │
    │  Random access = cache miss every time                      │
    │  ~50× slower than sequential access!                        │
    └─────────────────────────────────────────────────────────────┘
```

### Comparison with Alternatives

```
+------------------------------------------------------------------+
|  ARRAY VS OTHER STRUCTURES                                       |
+------------------------------------------------------------------+

    ┌───────────────────┬────────────────────┬─────────────────────┐
    │ Feature           │ Array              │ Linked List         │
    ├───────────────────┼────────────────────┼─────────────────────┤
    │ Random access     │ O(1) ✓             │ O(n) ✗              │
    │ Insert middle     │ O(n) ✗             │ O(1)* ✓             │
    │ Memory overhead   │ Zero ✓             │ High ✗              │
    │ Cache behavior    │ Excellent ✓        │ Poor ✗              │
    │ Size flexibility  │ Fixed/costly ✗     │ Dynamic ✓           │
    │ Memory layout     │ Contiguous ✓       │ Scattered ✗         │
    └───────────────────┴────────────────────┴─────────────────────┘
    * If you already have a pointer to the insertion point

    ┌───────────────────┬────────────────────┬─────────────────────┐
    │ Feature           │ Array              │ Hash Table          │
    ├───────────────────┼────────────────────┼─────────────────────┤
    │ Access by index   │ O(1) ✓             │ N/A                 │
    │ Access by key     │ O(n) ✗             │ O(1) ✓              │
    │ Ordered iteration │ Yes ✓              │ No ✗                │
    │ Memory overhead   │ Zero ✓             │ Moderate ✗          │
    │ Implementation    │ Simple ✓           │ Complex ✗           │
    └───────────────────┴────────────────────┴─────────────────────┘
```

---

## 6. Design & Engineering Takeaways

### Rules of Thumb

```
+------------------------------------------------------------------+
|  ARRAY RULES OF THUMB                                            |
+------------------------------------------------------------------+

    1. DEFAULT TO ARRAY
       Unless you have a specific reason for another structure,
       start with an array. It's the most cache-friendly choice.

    2. SIZE AT COMPILE TIME? → STATIC ARRAY
       int buffer[1024];  // On stack or as static/global
       Advantages: No malloc, no free, no NULL checks

    3. SIZE AT RUNTIME? → DYNAMIC ARRAY
       int *arr = malloc(n * sizeof(*arr));
       Always check return value, always free()

    4. NEED TO GROW? → PRE-ALLOCATE WITH MARGIN
       Allocate 2× expected size initially
       Avoid frequent reallocations

    5. VERY LARGE ARRAY? → HEAP, NOT STACK
       Stack is typically 1-8 MB
       Large arrays must be heap-allocated

    6. PASS SIZE SEPARATELY
       void process(int *arr, size_t n);
       Never rely on sizeof(arr) in called function
```

### Selection Guidelines

```
+------------------------------------------------------------------+
|  WHEN TO CHOOSE ARRAY                                            |
+------------------------------------------------------------------+

    USE ARRAY WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ You need O(1) random access                              │
    │  ✓ Size is known or grows slowly                            │
    │  ✓ You iterate more than you insert/delete                  │
    │  ✓ Memory efficiency is important                           │
    │  ✓ Cache performance matters                                │
    │  ✓ You want simple, debuggable code                         │
    └─────────────────────────────────────────────────────────────┘

    AVOID ARRAY WHEN:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✗ Frequent insertions/deletions in middle                  │
    │  ✗ Size changes dramatically and unpredictably              │
    │  ✗ You need O(1) insert/delete at arbitrary positions       │
    │  ✗ Elements have complex ownership (use linked list)        │
    │  ✗ You need key-based lookup (use hash table)               │
    │  ✗ You need ordered insertion (use tree or sorted array)    │
    └─────────────────────────────────────────────────────────────┘
```

### How Arrays Influence Architecture

```
+------------------------------------------------------------------+
|  ARCHITECTURAL CONSIDERATIONS                                    |
+------------------------------------------------------------------+

    1. BATCH PROCESSING
       Arrays encourage batch operations:
       - Process all elements in one pass (cache-friendly)
       - Avoid random access patterns
       - Use vectorization opportunities (SIMD)

    2. INDEX-BASED REFERENCES
       Instead of pointers between objects, use indices:
       - Indices survive reallocation (pointers don't!)
       - Indices are serializable (pointers aren't)
       - Indices are smaller (32-bit vs 64-bit pointer)

    3. STRUCT OF ARRAYS (SoA) vs ARRAY OF STRUCTS (AoS)
       
       AoS (traditional):
       struct Point { float x, y, z; };
       struct Point points[1000];
       
       SoA (cache-optimized for single-field access):
       struct Points {
           float x[1000];
           float y[1000];
           float z[1000];
       };
       
       Use SoA when you often access only one field at a time

    4. MEMORY POOLS
       Pre-allocate array of objects, manage allocation manually:
       - Avoids malloc overhead
       - Enables bulk operations
       - Better cache locality
```

### How Professionals Think About Arrays

```
+------------------------------------------------------------------+
|  PROFESSIONAL MINDSET                                            |
+------------------------------------------------------------------+

    1. "MEMORY IS THE BOTTLENECK"
       - Think about cache lines, not just Big-O
       - Contiguous memory wins in real benchmarks
       - Profile before optimizing

    2. "SIMPLICITY WINS"
       - Array is simplest data structure
       - Fewer bugs than complex structures
       - Easier to debug and reason about

    3. "KNOW YOUR DATA"
       - How large will this array be?
       - How often will it grow?
       - What access patterns dominate?
       
    4. "DEFENSIVE CODING"
       - Always check bounds (debug builds)
       - Always check malloc return
       - Always free what you allocate
       - Use tools: valgrind, AddressSanitizer

    5. "MEASURE, DON'T ASSUME"
       - O(n) array might beat O(1) hash for small n
       - Cache effects can dominate complexity
       - Real performance = measurement + understanding
```

---

## Summary

```
+------------------------------------------------------------------+
|  ARRAY: THE FOUNDATION OF SYSTEMS PROGRAMMING                    |
+------------------------------------------------------------------+

    CORE INSIGHT:
    Arrays are the most fundamental data structure because they
    directly map to how memory and CPUs work:
    
    - Contiguous memory = cache efficiency
    - Index arithmetic = O(1) access
    - No metadata = minimal overhead
    
    WHEN TO USE:
    - Almost always as the first choice
    - When you need indexed access
    - When iteration dominates
    - When memory efficiency matters
    
    WHEN TO AVOID:
    - Frequent insertion/deletion in middle
    - Need key-based lookup
    - Dynamic resizing is performance-critical
    
    KEY RULES:
    1. Static when size is compile-time known
    2. Dynamic when runtime-determined
    3. Always pass size as parameter
    4. Always check allocations
    5. Always free what you allocate
    6. Profile before optimizing
```

**中文总结：**
- 数组是最基础的数据结构，直接映射到内存和 CPU 工作方式
- 连续内存 = 缓存效率，索引运算 = O(1) 访问
- 几乎所有情况下首选数组
- 关键规则：静态数组用于编译时已知大小，动态数组用于运行时确定大小
- 始终单独传递大小参数，始终检查分配，始终释放内存

