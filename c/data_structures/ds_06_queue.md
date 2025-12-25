# Queue in C — Memory Model First

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE QUEUE: FIRST-IN-FIRST-OUT (FIFO) ACCESS                     |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Need to process items in the ORDER they arrived:           │
    │  - Task scheduling (first task submitted runs first)        │
    │  - Network packet processing                                │
    │  - Event handling (process events chronologically)          │
    │  - Print job spooling                                       │
    │  - Breadth-first search (BFS)                               │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: QUEUE
    ┌─────────────────────────────────────────────────────────────┐
    │  • Enqueue: add to BACK (tail)                              │
    │  • Dequeue: remove from FRONT (head)                        │
    │  • First thing in is first thing out                        │
    └─────────────────────────────────────────────────────────────┘

    VISUAL:
    
    Enqueue here (back)           Dequeue here (front)
           │                              │
           ▼                              ▼
    ┌─────────────────────────────────────────┐
    │  D  │  C  │  B  │  A  │                 │
    └─────────────────────────────────────────┘
          ◀──────────────────
            Direction of flow

    Enqueue order: A, B, C, D
    Dequeue order: A, B, C, D (same order!)
```

**中文解释：**
- 队列解决按到达顺序处理项目的问题
- FIFO（先进先出）：最先入队的元素最先出队
- 用途：任务调度、网络包处理、事件处理、BFS

### Implementation Strategies

```
+------------------------------------------------------------------+
|  QUEUE IMPLEMENTATIONS                                           |
+------------------------------------------------------------------+

    1. ARRAY QUEUE (naive - wasteful):
       ┌───┬───┬───┬───┬───┬───┬───┬───┐
       │ X │ X │ A │ B │ C │ ? │ ? │ ? │
       └───┴───┴───┴───┴───┴───┴───┴───┘
             ▲           ▲
           head        tail
       
       Problem: Elements shift left, wasting space at front

    2. CIRCULAR ARRAY (ring buffer) - BEST:
       ┌───┬───┬───┬───┬───┬───┬───┬───┐
       │ D │ E │ ? │ ? │ A │ B │ C │ ? │
       └───┴───┴───┴───┴───┴───┴───┴───┘
             ▲           ▲
           tail        head
       
       Wraps around! No wasted space, O(1) all operations

    3. LINKED LIST QUEUE:
       
       head                              tail
        │                                  │
        ▼                                  ▼
       ┌───┐   ┌───┐   ┌───┐   ┌───┐
       │ A │──▶│ B │──▶│ C │──▶│ D │──▶ NULL
       └───┘   └───┘   └───┘   └───┘
       
       O(1) enqueue/dequeue, but allocation overhead
```

### Invariants

```
+------------------------------------------------------------------+
|  QUEUE INVARIANTS                                                |
+------------------------------------------------------------------+

    1. FIFO ORDER
       dequeue() returns oldest element (first enqueued)

    2. HEAD/TAIL TRACKING
       head: next element to dequeue
       tail: where next element will be enqueued

    3. CIRCULAR (for ring buffer)
       Indices wrap around: (index + 1) % capacity
       Full vs empty distinguished by count or "waste one slot"

    4. SIZE BOUNDS
       0 ≤ size ≤ capacity
       Enqueue fails if full (or triggers resize)
```

---

## 2. Memory Model

### Ring Buffer (Circular Queue) Memory Layout

```
+------------------------------------------------------------------+
|  RING BUFFER MEMORY LAYOUT                                       |
+------------------------------------------------------------------+

    struct ring_queue {
        int *data;
        size_t head;      /* Index of front element */
        size_t tail;      /* Index of next empty slot */
        size_t capacity;
        size_t count;     /* Current number of elements */
    };

    Example state (capacity=8, 4 elements: A, B, C, D):

    Logical view:          Physical array:
    ┌───────────┐          ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ A (front) │          │ ? │ ? │ A │ B │ C │ D │ ? │ ? │
    │ B         │          └───┴───┴───┴───┴───┴───┴───┴───┘
    │ C         │           [0] [1] [2] [3] [4] [5] [6] [7]
    │ D (back)  │                   ▲               ▲
    └───────────┘                 head=2         tail=6
                                                count=4

    After enqueue(E), enqueue(F), enqueue(G):
    ┌───┬───┬───┬───┬───┬───┬───┬───┐
    │ G │ ? │ A │ B │ C │ D │ E │ F │  ← G wrapped to index 0!
    └───┴───┴───┴───┴───┴───┴───┴───┘
      ▲       ▲
    tail=1  head=2
    count=7

    INDEX ARITHMETIC:
    next_index = (current + 1) % capacity
    enqueue: data[tail] = value; tail = (tail + 1) % capacity; count++
    dequeue: value = data[head]; head = (head + 1) % capacity; count--
```

**中文解释：**
- **环形缓冲区**：头尾指针循环使用数组空间
- 当 tail 到达数组末尾时，回绕到开头
- 无需移动元素，所有操作 O(1)
- 用 count 或"浪费一个槽位"区分满和空

### Linked List Queue Memory Layout

```
+------------------------------------------------------------------+
|  LINKED LIST QUEUE MEMORY LAYOUT                                 |
+------------------------------------------------------------------+

    struct queue_node {
        int value;
        struct queue_node *next;
    };

    struct list_queue {
        struct queue_node *head;  /* Dequeue from here */
        struct queue_node *tail;  /* Enqueue here */
        size_t count;
    };

    Memory layout (elements A, B, C):
    
    Queue struct:                 Nodes (on heap):
    ┌────────────────┐
    │ head ──────────┼───────────▶┌──────────┐
    │ tail ──────────┼───────┐    │ value: A │
    │ count: 3       │       │    │ next ────┼────▶┌──────────┐
    └────────────────┘       │    └──────────┘     │ value: B │
                             │                     │ next ────┼────▶┌──────────┐
                             │                     └──────────┘     │ value: C │
                             └─────────────────────────────────────▶│ next:NULL│
                                                                    └──────────┘
                                                                         ▲
                                                                        tail

    OPERATIONS:
    enqueue: tail->next = new_node; tail = new_node; (or if empty: head = tail = new_node)
    dequeue: value = head->value; head = head->next; free(old_head);
```

### Cache Behavior

```
+------------------------------------------------------------------+
|  CACHE BEHAVIOR COMPARISON                                       |
+------------------------------------------------------------------+

    RING BUFFER (excellent):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Contiguous memory                                        │
    │  • Sequential access pattern (head moves forward)           │
    │  • Prefetching effective                                    │
    │  • Only 1 cache miss per ~16 dequeues (for int)            │
    └─────────────────────────────────────────────────────────────┘

    LINKED LIST (poor):
    ┌─────────────────────────────────────────────────────────────┐
    │  • Nodes scattered in heap                                  │
    │  • Every dequeue = potential cache miss                     │
    │  • malloc/free overhead adds latency                        │
    │  • ~100% miss rate for large queues                        │
    └─────────────────────────────────────────────────────────────┘

    REAL PERFORMANCE:
    ┌─────────────────────────────────────────────────────────────┐
    │  Ring buffer: 5-50× faster than linked list                │
    │  Exception: if nodes from memory pool, gap narrows         │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Typical Application Scenarios

### Where Queues Are Used

```
+------------------------------------------------------------------+
|  REAL-WORLD QUEUE APPLICATIONS                                   |
+------------------------------------------------------------------+

    OPERATING SYSTEMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Task/process ready queues                                │
    │  • I/O request queues                                       │
    │  • Network packet buffers                                   │
    │  • Interrupt request queues                                 │
    │  • Device driver ring buffers                               │
    └─────────────────────────────────────────────────────────────┘

    APPLICATIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Message queues (producer-consumer)                       │
    │  • Event loops (GUI, game)                                  │
    │  • Print spooler                                            │
    │  • Web server request handling                              │
    │  • Audio/video streaming buffers                            │
    └─────────────────────────────────────────────────────────────┘

    ALGORITHMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Breadth-first search (BFS)                               │
    │  • Level-order tree traversal                               │
    │  • Topological sort (Kahn's algorithm)                      │
    │  • Sliding window problems                                  │
    └─────────────────────────────────────────────────────────────┘
```

---

## 4. Complete C Examples

### Example 1: Ring Buffer Queue (Power-of-2 Optimization)

```c
/*
 * Example 1: Optimized Ring Buffer Queue
 *
 * Uses power-of-2 capacity for fast modulo (bitwise AND)
 * Compile: gcc -Wall -Wextra -o ring_queue ring_queue.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

struct ring_queue {
    int *data;
    size_t head;       /* Index of front element */
    size_t tail;       /* Index of next empty slot */
    size_t mask;       /* capacity - 1 (for fast modulo) */
    size_t count;
};

/* Ensure capacity is power of 2 */
static size_t next_power_of_2(size_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

int ring_queue_init(struct ring_queue *q, size_t capacity)
{
    capacity = next_power_of_2(capacity);
    
    q->data = malloc(capacity * sizeof(int));
    if (!q->data)
        return -1;
    
    q->head = 0;
    q->tail = 0;
    q->mask = capacity - 1;  /* e.g., 7 for capacity 8 */
    q->count = 0;
    
    return 0;
}

void ring_queue_destroy(struct ring_queue *q)
{
    free(q->data);
    q->data = NULL;
}

bool ring_queue_empty(const struct ring_queue *q)
{
    return q->count == 0;
}

bool ring_queue_full(const struct ring_queue *q)
{
    return q->count > q->mask;  /* count > capacity - 1 */
}

size_t ring_queue_capacity(const struct ring_queue *q)
{
    return q->mask + 1;
}

/* Enqueue (returns false if full) */
bool ring_queue_enqueue(struct ring_queue *q, int value)
{
    if (ring_queue_full(q))
        return false;
    
    q->data[q->tail] = value;
    q->tail = (q->tail + 1) & q->mask;  /* Fast modulo! */
    q->count++;
    
    return true;
}

/* Dequeue (returns false if empty) */
bool ring_queue_dequeue(struct ring_queue *q, int *value)
{
    if (ring_queue_empty(q))
        return false;
    
    *value = q->data[q->head];
    q->head = (q->head + 1) & q->mask;  /* Fast modulo! */
    q->count--;
    
    return true;
}

/* Peek at front without removing */
bool ring_queue_peek(const struct ring_queue *q, int *value)
{
    if (ring_queue_empty(q))
        return false;
    
    *value = q->data[q->head];
    return true;
}

void ring_queue_print(const struct ring_queue *q)
{
    printf("Queue [count=%zu, cap=%zu]: FRONT -> ",
           q->count, ring_queue_capacity(q));
    
    size_t idx = q->head;
    for (size_t i = 0; i < q->count; i++) {
        printf("%d ", q->data[idx]);
        idx = (idx + 1) & q->mask;
    }
    printf("<- BACK\n");
}

int main(void)
{
    struct ring_queue q;
    
    if (ring_queue_init(&q, 5) < 0) {  /* Will be rounded to 8 */
        fprintf(stderr, "Failed to create queue\n");
        return 1;
    }
    
    printf("=== Ring Buffer Queue Demo ===\n\n");
    printf("Requested capacity 5, actual: %zu (power of 2)\n\n",
           ring_queue_capacity(&q));
    
    /* Enqueue elements */
    printf("Enqueuing 1-6:\n");
    for (int i = 1; i <= 6; i++) {
        if (ring_queue_enqueue(&q, i * 10)) {
            printf("  Enqueued %d: ", i * 10);
            ring_queue_print(&q);
        }
    }
    
    /* Dequeue some */
    printf("\nDequeuing 3 elements:\n");
    for (int i = 0; i < 3; i++) {
        int val;
        if (ring_queue_dequeue(&q, &val)) {
            printf("  Dequeued %d: ", val);
            ring_queue_print(&q);
        }
    }
    
    /* Enqueue more (wraps around) */
    printf("\nEnqueuing 70, 80, 90 (wraps around):\n");
    ring_queue_enqueue(&q, 70);
    ring_queue_enqueue(&q, 80);
    ring_queue_enqueue(&q, 90);
    ring_queue_print(&q);
    
    /* Show internal state */
    printf("\nInternal state: head=%zu, tail=%zu, mask=%zu\n",
           q.head, q.tail, q.mask);
    
    ring_queue_destroy(&q);
    return 0;
}
```

---

### Example 2: Linked List Queue

```c
/*
 * Example 2: Linked List Queue
 *
 * Unbounded capacity, O(1) operations
 * Compile: gcc -Wall -Wextra -o list_queue list_queue.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct queue_node {
    int value;
    struct queue_node *next;
};

struct list_queue {
    struct queue_node *head;  /* Dequeue from here */
    struct queue_node *tail;  /* Enqueue here */
    size_t count;
};

void list_queue_init(struct list_queue *q)
{
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
}

bool list_queue_empty(const struct list_queue *q)
{
    return q->head == NULL;
}

size_t list_queue_size(const struct list_queue *q)
{
    return q->count;
}

/* Enqueue at tail */
int list_queue_enqueue(struct list_queue *q, int value)
{
    struct queue_node *node = malloc(sizeof(*node));
    if (!node)
        return -1;
    
    node->value = value;
    node->next = NULL;
    
    if (q->tail) {
        q->tail->next = node;  /* Link after current tail */
    } else {
        q->head = node;        /* First element */
    }
    
    q->tail = node;
    q->count++;
    
    return 0;
}

/* Dequeue from head */
int list_queue_dequeue(struct list_queue *q, int *value)
{
    if (list_queue_empty(q))
        return -1;
    
    struct queue_node *node = q->head;
    *value = node->value;
    
    q->head = node->next;
    if (!q->head) {
        q->tail = NULL;  /* Queue is now empty */
    }
    
    free(node);
    q->count--;
    
    return 0;
}

/* Peek at front */
int list_queue_peek(const struct list_queue *q, int *value)
{
    if (list_queue_empty(q))
        return -1;
    
    *value = q->head->value;
    return 0;
}

/* Destroy queue */
void list_queue_destroy(struct list_queue *q)
{
    while (q->head) {
        struct queue_node *node = q->head;
        q->head = node->next;
        free(node);
    }
    q->tail = NULL;
    q->count = 0;
}

void list_queue_print(const struct list_queue *q)
{
    printf("Queue [size=%zu]: FRONT -> ", q->count);
    for (struct queue_node *n = q->head; n; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");
}

int main(void)
{
    struct list_queue q;
    list_queue_init(&q);
    
    printf("=== Linked List Queue Demo ===\n\n");
    
    /* Enqueue */
    for (int i = 1; i <= 5; i++) {
        list_queue_enqueue(&q, i * 10);
        printf("Enqueued %d: ", i * 10);
        list_queue_print(&q);
    }
    
    /* Peek */
    int val;
    list_queue_peek(&q, &val);
    printf("\nPeek: %d\n", val);
    
    /* Dequeue all */
    printf("\nDequeuing all (FIFO order):\n");
    while (!list_queue_empty(&q)) {
        list_queue_dequeue(&q, &val);
        printf("Dequeued: %d\n", val);
    }
    
    list_queue_destroy(&q);
    return 0;
}
```

---

### Example 3: BFS Using Queue

```c
/*
 * Example 3: Breadth-First Search Using Queue
 *
 * Real algorithm demonstrating queue usage
 * Compile: gcc -Wall -Wextra -o bfs bfs.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_NODES 100
#define QUEUE_SIZE 100

/* Simple graph (adjacency list) */
struct graph {
    int adj[MAX_NODES][MAX_NODES];  /* adj[u] contains neighbors */
    int adj_count[MAX_NODES];       /* Number of neighbors */
    int num_nodes;
};

/* Simple array queue for BFS */
struct bfs_queue {
    int data[QUEUE_SIZE];
    int head, tail;
};

void queue_init(struct bfs_queue *q) { q->head = q->tail = 0; }
bool queue_empty(struct bfs_queue *q) { return q->head == q->tail; }
void queue_enqueue(struct bfs_queue *q, int v) { q->data[q->tail++] = v; }
int queue_dequeue(struct bfs_queue *q) { return q->data[q->head++]; }

/* Add edge to graph */
void graph_add_edge(struct graph *g, int u, int v)
{
    g->adj[u][g->adj_count[u]++] = v;
    g->adj[v][g->adj_count[v]++] = u;  /* Undirected */
}

/* BFS traversal */
void bfs(struct graph *g, int start)
{
    bool visited[MAX_NODES] = {false};
    int distance[MAX_NODES];
    int parent[MAX_NODES];
    
    for (int i = 0; i < g->num_nodes; i++) {
        distance[i] = -1;
        parent[i] = -1;
    }
    
    struct bfs_queue q;
    queue_init(&q);
    
    /* Start BFS */
    visited[start] = true;
    distance[start] = 0;
    queue_enqueue(&q, start);
    
    printf("BFS traversal from node %d:\n", start);
    printf("Order: ");
    
    while (!queue_empty(&q)) {
        int u = queue_dequeue(&q);
        printf("%d ", u);
        
        /* Visit all neighbors */
        for (int i = 0; i < g->adj_count[u]; i++) {
            int v = g->adj[u][i];
            if (!visited[v]) {
                visited[v] = true;
                distance[v] = distance[u] + 1;
                parent[v] = u;
                queue_enqueue(&q, v);
            }
        }
    }
    printf("\n\n");
    
    /* Print distances */
    printf("Shortest distances from node %d:\n", start);
    for (int i = 0; i < g->num_nodes; i++) {
        if (distance[i] >= 0) {
            printf("  Node %d: distance = %d\n", i, distance[i]);
        }
    }
}

int main(void)
{
    printf("=== BFS Demo Using Queue ===\n\n");
    
    struct graph g = {0};
    g.num_nodes = 6;
    
    /*
     * Graph structure:
     *     0 --- 1 --- 2
     *     |     |
     *     3 --- 4 --- 5
     */
    graph_add_edge(&g, 0, 1);
    graph_add_edge(&g, 0, 3);
    graph_add_edge(&g, 1, 2);
    graph_add_edge(&g, 1, 4);
    graph_add_edge(&g, 3, 4);
    graph_add_edge(&g, 4, 5);
    
    printf("Graph:\n");
    printf("  0 --- 1 --- 2\n");
    printf("  |     |\n");
    printf("  3 --- 4 --- 5\n\n");
    
    bfs(&g, 0);
    
    return 0;
}
```

---

### Example 4: Producer-Consumer Queue

```c
/*
 * Example 4: Thread-Safe Producer-Consumer Queue (Conceptual)
 *
 * Shows the pattern (actual threading requires pthread)
 * Compile: gcc -Wall -Wextra -o producer_consumer producer_consumer.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define QUEUE_CAPACITY 8

struct work_item {
    int id;
    char data[32];
};

struct work_queue {
    struct work_item items[QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    /* In real code: mutex + condition variables here */
};

void work_queue_init(struct work_queue *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
}

bool work_queue_full(const struct work_queue *q)
{
    return q->count >= QUEUE_CAPACITY;
}

bool work_queue_empty(const struct work_queue *q)
{
    return q->count == 0;
}

/* Producer: add work item */
bool produce(struct work_queue *q, int id, const char *data)
{
    /* In real code: lock mutex here */
    /* while (work_queue_full(q)) cond_wait(...); */
    
    if (work_queue_full(q))
        return false;
    
    struct work_item *item = &q->items[q->tail];
    item->id = id;
    strncpy(item->data, data, sizeof(item->data) - 1);
    item->data[sizeof(item->data) - 1] = '\0';
    
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    
    /* In real code: signal consumer, unlock */
    return true;
}

/* Consumer: get work item */
bool consume(struct work_queue *q, struct work_item *out)
{
    /* In real code: lock mutex here */
    /* while (work_queue_empty(q)) cond_wait(...); */
    
    if (work_queue_empty(q))
        return false;
    
    *out = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    
    /* In real code: signal producer, unlock */
    return true;
}

int main(void)
{
    printf("=== Producer-Consumer Queue Demo ===\n\n");
    
    struct work_queue q;
    work_queue_init(&q);
    
    /* Simulate producer */
    printf("Producer adding work:\n");
    produce(&q, 1, "Process file A");
    produce(&q, 2, "Send email B");
    produce(&q, 3, "Compress data C");
    produce(&q, 4, "Upload to cloud D");
    
    printf("Queue has %zu items\n\n", q.count);
    
    /* Simulate consumer */
    printf("Consumer processing (FIFO order):\n");
    struct work_item item;
    while (consume(&q, &item)) {
        printf("  Processing job #%d: %s\n", item.id, item.data);
    }
    
    printf("\nQueue is now empty: %s\n",
           work_queue_empty(&q) ? "yes" : "no");
    
    return 0;
}
```

---

### Example 5: Double-Ended Queue (Deque)

```c
/*
 * Example 5: Double-Ended Queue (Deque)
 *
 * Insert/remove from both ends
 * Useful for sliding window, work stealing
 * Compile: gcc -Wall -Wextra -o deque deque.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct deque {
    int *data;
    size_t head;      /* Points to front element */
    size_t tail;      /* Points past last element */
    size_t capacity;
    size_t count;
};

int deque_init(struct deque *d, size_t capacity)
{
    d->data = malloc(capacity * sizeof(int));
    if (!d->data)
        return -1;
    
    d->head = 0;
    d->tail = 0;
    d->capacity = capacity;
    d->count = 0;
    return 0;
}

void deque_destroy(struct deque *d)
{
    free(d->data);
}

bool deque_empty(const struct deque *d)
{
    return d->count == 0;
}

bool deque_full(const struct deque *d)
{
    return d->count >= d->capacity;
}

/* Add to front */
bool deque_push_front(struct deque *d, int value)
{
    if (deque_full(d))
        return false;
    
    /* Move head backwards (with wrap) */
    d->head = (d->head + d->capacity - 1) % d->capacity;
    d->data[d->head] = value;
    d->count++;
    return true;
}

/* Add to back */
bool deque_push_back(struct deque *d, int value)
{
    if (deque_full(d))
        return false;
    
    d->data[d->tail] = value;
    d->tail = (d->tail + 1) % d->capacity;
    d->count++;
    return true;
}

/* Remove from front */
bool deque_pop_front(struct deque *d, int *value)
{
    if (deque_empty(d))
        return false;
    
    *value = d->data[d->head];
    d->head = (d->head + 1) % d->capacity;
    d->count--;
    return true;
}

/* Remove from back */
bool deque_pop_back(struct deque *d, int *value)
{
    if (deque_empty(d))
        return false;
    
    d->tail = (d->tail + d->capacity - 1) % d->capacity;
    *value = d->data[d->tail];
    d->count--;
    return true;
}

void deque_print(const struct deque *d)
{
    printf("Deque [%zu]: FRONT -> ", d->count);
    size_t idx = d->head;
    for (size_t i = 0; i < d->count; i++) {
        printf("%d ", d->data[idx]);
        idx = (idx + 1) % d->capacity;
    }
    printf("<- BACK\n");
}

int main(void)
{
    printf("=== Double-Ended Queue Demo ===\n\n");
    
    struct deque d;
    deque_init(&d, 8);
    
    /* Build from middle */
    printf("Building deque:\n");
    deque_push_back(&d, 3);    /* [3] */
    deque_push_back(&d, 4);    /* [3, 4] */
    deque_push_front(&d, 2);   /* [2, 3, 4] */
    deque_push_front(&d, 1);   /* [1, 2, 3, 4] */
    deque_push_back(&d, 5);    /* [1, 2, 3, 4, 5] */
    deque_print(&d);
    
    /* Pop from both ends */
    int val;
    printf("\nPop front: ");
    deque_pop_front(&d, &val);
    printf("%d -> ", val);
    deque_print(&d);
    
    printf("Pop back: ");
    deque_pop_back(&d, &val);
    printf("%d -> ", val);
    deque_print(&d);
    
    /* Use as stack (LIFO) */
    printf("\nUsing as stack (push/pop back):\n");
    deque_push_back(&d, 100);
    deque_push_back(&d, 200);
    deque_print(&d);
    
    deque_pop_back(&d, &val);
    printf("Pop back: %d\n", val);
    
    /* Use as queue (FIFO) */
    printf("\nUsing as queue (push back, pop front):\n");
    deque_push_back(&d, 300);
    deque_print(&d);
    
    deque_pop_front(&d, &val);
    printf("Pop front: %d\n", val);
    deque_print(&d);
    
    deque_destroy(&d);
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

```
+------------------------------------------------------------------+
|  QUEUE IMPLEMENTATIONS COMPARISON                                |
+------------------------------------------------------------------+

    ┌───────────────────┬──────────────┬──────────────┬─────────────┐
    │ Feature           │ Ring Buffer  │ Linked List  │ Naive Array │
    ├───────────────────┼──────────────┼──────────────┼─────────────┤
    │ Enqueue           │ O(1)         │ O(1)         │ O(1)        │
    │ Dequeue           │ O(1)         │ O(1)         │ O(n) shift! │
    │ Memory overhead   │ ~0           │ High         │ Wasted space│
    │ Cache behavior    │ Excellent    │ Poor         │ Good        │
    │ Max capacity      │ Fixed        │ Unbounded    │ Fixed       │
    │ Implementation    │ Moderate     │ Simple       │ Simple      │
    └───────────────────┴──────────────┴──────────────┴─────────────┘

    RECOMMENDATION:
    • Use ring buffer for most cases
    • Use linked list only for unbounded, variable-size queues
    • Never use naive array (O(n) dequeue is unacceptable)
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|  QUEUE: KEY TAKEAWAYS                                            |
+------------------------------------------------------------------+

    CORE CONCEPT:
    First-In-First-Out (FIFO) access pattern
    Enqueue at back, dequeue from front

    BEST IMPLEMENTATION:
    Ring buffer (circular array) — O(1) all operations,
    excellent cache behavior, fixed capacity

    USE LINKED LIST WHEN:
    - Unbounded capacity needed
    - Elements are large (pointer overhead is small relative)
    - Using intrusive embedding

    COMMON APPLICATIONS:
    - Task scheduling
    - BFS traversal
    - Producer-consumer patterns
    - Event handling

    KEY INSIGHT:
    Power-of-2 capacity enables fast modulo via bitwise AND
```

**中文总结：**
- **核心概念**：FIFO（先进先出），后端入队，前端出队
- **最佳实现**：环形缓冲区——所有操作 O(1)，缓存友好
- **常见应用**：任务调度、BFS、生产者-消费者、事件处理
- **关键技巧**：2的幂容量可用位与代替取模

