# Stack & Queue

## 1. Technical Specification

### Stack (LIFO)

| Property | Array-based | List-based |
|----------|------------|-----------|
| Push | O(1) amortized | O(1) |
| Pop | O(1) | O(1) |
| Peek | O(1) | O(1) |
| Memory | Contiguous, cache-friendly | Scattered nodes |
| Max size | Dynamic (resize) or fixed | Unbounded |

### Queue (FIFO)

| Property | Circular Array | List-based |
|----------|---------------|-----------|
| Enqueue | O(1) amortized | O(1) |
| Dequeue | O(1) | O(1) |
| Peek | O(1) | O(1) |
| Memory | Contiguous, cache-friendly | Scattered nodes |
| Max size | Dynamic (resize) or fixed | Unbounded |

### Use Cases

**Stack:**
- **Compilers**: Expression evaluation, AST traversal, scope management.
- **OS Kernel**: Call stacks, interrupt context save/restore.
- **Networking**: Protocol header push/pop (MPLS label stack, VLAN tags).
- **Algorithms**: DFS, backtracking, undo/redo.

**Queue:**
- **DPDK**: Packet queues between pipeline stages (`rte_ring` is a queue).
- **OS Kernel**: Scheduler run queues, work queues, IRQ bottom-halves.
- **Networking**: Traffic shaping (token bucket queues), message passing.
- **Algorithms**: BFS, producer-consumer patterns.

### Trade-offs

| Array-based | List-based |
|-------------|-----------|
| Better cache locality | No resize overhead |
| Resize cost (amortized O(1)) | malloc per operation |
| Fixed max with ring variant | True unbounded |
| Predictable memory | Fragmentation risk |

---

## 2. Implementation Strategy

### Array-based Stack

```
stack_t
+------------+
| items -----+---> [ A | B | C | D |   |   |   |   ]
| top = 3    |                   ^top
| capacity=8 |
| free_fn    |
+------------+

push(E): items[++top] = E
pop():   return items[top--]
```

### List-based Queue

```
queue_t
+--------+
| head --+--> [A|*] --> [B|*] --> [C|/] <--+-- tail
| tail --+-----------------------------------------+
| size=3 |
| free_fn|
+--------+

enqueue(D): tail->next = new_node(D); tail = new_node
dequeue():  item = head->data; head = head->next
```

### Circular Array Queue

```
cqueue_t (capacity=8)
+----------------+
| buf[8] --------+---> [   | A | B | C | D |   |   |   ]
| head = 1       |           ^head           ^tail
| tail = 5       |
| capacity = 8   |
| size = 4       |
+----------------+

enqueue(E): buf[tail] = E; tail = (tail+1) % cap
dequeue():  item = buf[head]; head = (head+1) % cap
```

---

## 3. Implementation

```c
/**
 * @file stack_queue.c
 * @brief Generic stack (array-based) and queue (circular array + list-based).
 *
 * All structures use void* for generic data storage.
 * Thread-unsafe; caller must synchronize.
 *
 * Standard: C99 / C11
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ======================================================================
 * Part A: Array-based Stack
 * ====================================================================== */

#define STACK_INIT_CAP  16

typedef struct stack {
    void   **items;
    size_t   top;       /* index of next free slot (0 = empty) */
    size_t   capacity;
    void   (*free_fn)(void *);
} stack_t;

/**
 * @brief Create a new stack.
 * @param free_fn Optional destructor for elements.
 */
static stack_t *
stack_create(void (*free_fn)(void *))
{
    stack_t *s;

    s = calloc(1, sizeof(*s));
    if (s == NULL)
        return NULL;

    s->items = calloc(STACK_INIT_CAP, sizeof(void *));
    if (s->items == NULL) {
        free(s);
        return NULL;
    }

    s->capacity = STACK_INIT_CAP;
    s->free_fn  = free_fn;
    return s;
}

static void
stack_destroy(stack_t *s)
{
    size_t i;

    if (s == NULL)
        return;

    if (s->free_fn != NULL) {
        for (i = 0; i < s->top; i++) {
            if (s->items[i] != NULL)
                s->free_fn(s->items[i]);
        }
    }

    free(s->items);
    free(s);
}

/**
 * @brief Push an element onto the stack.
 * @return 0 on success, -1 on failure.
 */
static int
stack_push(stack_t *s, void *item)
{
    void **tmp;

    if (s == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (s->top >= s->capacity) {
        tmp = realloc(s->items, s->capacity * 2 * sizeof(void *));
        if (tmp == NULL)
            return -1;
        s->items    = tmp;
        s->capacity *= 2;
    }

    s->items[s->top++] = item;
    return 0;
}

/**
 * @brief Pop the top element.
 * @return Element pointer, or NULL if empty (caller takes ownership).
 */
static void *
stack_pop(stack_t *s)
{
    if (s == NULL || s->top == 0)
        return NULL;

    return s->items[--s->top];
}

/**
 * @brief Peek at the top element without removing it.
 */
static void *
stack_peek(const stack_t *s)
{
    if (s == NULL || s->top == 0)
        return NULL;

    return s->items[s->top - 1];
}

static inline size_t
stack_size(const stack_t *s)
{
    return s ? s->top : 0;
}

static inline int
stack_empty(const stack_t *s)
{
    return s == NULL || s->top == 0;
}

/* ======================================================================
 * Part B: Circular Array Queue
 * ====================================================================== */

#define CQUEUE_INIT_CAP  16

typedef struct cqueue {
    void   **buf;
    size_t   head;
    size_t   tail;
    size_t   size;
    size_t   capacity;
    void   (*free_fn)(void *);
} cqueue_t;

/**
 * @brief Create a circular array queue.
 */
static cqueue_t *
cqueue_create(void (*free_fn)(void *))
{
    cqueue_t *q;

    q = calloc(1, sizeof(*q));
    if (q == NULL)
        return NULL;

    q->buf = calloc(CQUEUE_INIT_CAP, sizeof(void *));
    if (q->buf == NULL) {
        free(q);
        return NULL;
    }

    q->capacity = CQUEUE_INIT_CAP;
    q->free_fn  = free_fn;
    return q;
}

static void
cqueue_destroy(cqueue_t *q)
{
    size_t i;

    if (q == NULL)
        return;

    if (q->free_fn != NULL) {
        for (i = 0; i < q->size; i++) {
            size_t idx = (q->head + i) % q->capacity;
            if (q->buf[idx] != NULL)
                q->free_fn(q->buf[idx]);
        }
    }

    free(q->buf);
    free(q);
}

/**
 * @brief Resize the circular buffer (double capacity).
 */
static int
cqueue_resize(cqueue_t *q)
{
    void  **new_buf;
    size_t  new_cap, i;

    new_cap = q->capacity * 2;
    new_buf = calloc(new_cap, sizeof(void *));
    if (new_buf == NULL)
        return -1;

    /* Linearize the circular buffer */
    for (i = 0; i < q->size; i++)
        new_buf[i] = q->buf[(q->head + i) % q->capacity];

    free(q->buf);
    q->buf      = new_buf;
    q->head     = 0;
    q->tail     = q->size;
    q->capacity = new_cap;

    return 0;
}

/**
 * @brief Enqueue an element at the back.
 * @return 0 on success, -1 on failure.
 */
static int
cqueue_enqueue(cqueue_t *q, void *item)
{
    if (q == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (q->size >= q->capacity) {
        if (cqueue_resize(q) != 0)
            return -1;
    }

    q->buf[q->tail] = item;
    q->tail = (q->tail + 1) % q->capacity;
    q->size++;

    return 0;
}

/**
 * @brief Dequeue the front element.
 * @return Element pointer, or NULL if empty.
 */
static void *
cqueue_dequeue(cqueue_t *q)
{
    void *item;

    if (q == NULL || q->size == 0)
        return NULL;

    item = q->buf[q->head];
    q->buf[q->head] = NULL;
    q->head = (q->head + 1) % q->capacity;
    q->size--;

    return item;
}

/**
 * @brief Peek at the front element.
 */
static void *
cqueue_peek(const cqueue_t *q)
{
    if (q == NULL || q->size == 0)
        return NULL;

    return q->buf[q->head];
}

static inline size_t
cqueue_size(const cqueue_t *q)
{
    return q ? q->size : 0;
}

static inline int
cqueue_empty(const cqueue_t *q)
{
    return q == NULL || q->size == 0;
}

/* ======================================================================
 * Part C: List-based Queue
 * ====================================================================== */

typedef struct qnode {
    void         *data;
    struct qnode *next;
} qnode_t;

typedef struct lqueue {
    qnode_t *head;
    qnode_t *tail;
    size_t   size;
    void   (*free_fn)(void *);
} lqueue_t;

static lqueue_t *
lqueue_create(void (*free_fn)(void *))
{
    lqueue_t *q;

    q = calloc(1, sizeof(*q));
    if (q == NULL)
        return NULL;

    q->free_fn = free_fn;
    return q;
}

static void
lqueue_destroy(lqueue_t *q)
{
    qnode_t *cur, *tmp;

    if (q == NULL)
        return;

    cur = q->head;
    while (cur != NULL) {
        tmp = cur->next;
        if (q->free_fn != NULL && cur->data != NULL)
            q->free_fn(cur->data);
        free(cur);
        cur = tmp;
    }

    free(q);
}

static int
lqueue_enqueue(lqueue_t *q, void *data)
{
    qnode_t *node;

    if (q == NULL) {
        errno = EINVAL;
        return -1;
    }

    node = malloc(sizeof(*node));
    if (node == NULL)
        return -1;

    node->data = data;
    node->next = NULL;

    if (q->tail != NULL)
        q->tail->next = node;
    else
        q->head = node;

    q->tail = node;
    q->size++;

    return 0;
}

static void *
lqueue_dequeue(lqueue_t *q)
{
    qnode_t *node;
    void    *data;

    if (q == NULL || q->head == NULL)
        return NULL;

    node = q->head;
    data = node->data;
    q->head = node->next;

    if (q->head == NULL)
        q->tail = NULL;

    free(node);
    q->size--;

    return data;
}

static void *
lqueue_peek(const lqueue_t *q)
{
    if (q == NULL || q->head == NULL)
        return NULL;

    return q->head->data;
}

static inline size_t
lqueue_size(const lqueue_t *q)
{
    return q ? q->size : 0;
}

/*
 * === Example / Self-test ===
 */
#ifdef SQ_TEST
#include <assert.h>

int
main(void)
{
    stack_t  *s;
    cqueue_t *cq;
    lqueue_t *lq;
    int      *v;
    int       i;

    /* --- Stack --- */
    s = stack_create(free);
    assert(s != NULL);

    for (i = 0; i < 100; i++) {
        v = malloc(sizeof(int));
        *v = i;
        assert(stack_push(s, v) == 0);
    }
    assert(stack_size(s) == 100);

    v = stack_peek(s);
    assert(*v == 99);

    v = stack_pop(s);
    assert(*v == 99);
    free(v);

    v = stack_pop(s);
    assert(*v == 98);
    free(v);

    stack_destroy(s);
    printf("stack: all tests passed\n");

    /* --- Circular Queue --- */
    cq = cqueue_create(free);
    assert(cq != NULL);

    for (i = 0; i < 100; i++) {
        v = malloc(sizeof(int));
        *v = i;
        assert(cqueue_enqueue(cq, v) == 0);
    }

    /* FIFO order */
    v = cqueue_dequeue(cq);
    assert(*v == 0);
    free(v);

    v = cqueue_dequeue(cq);
    assert(*v == 1);
    free(v);

    assert(cqueue_size(cq) == 98);
    cqueue_destroy(cq);
    printf("cqueue: all tests passed\n");

    /* --- List Queue --- */
    lq = lqueue_create(free);
    assert(lq != NULL);

    for (i = 0; i < 50; i++) {
        v = malloc(sizeof(int));
        *v = i;
        assert(lqueue_enqueue(lq, v) == 0);
    }

    v = lqueue_dequeue(lq);
    assert(*v == 0);
    free(v);

    v = lqueue_peek(lq);
    assert(*v == 1);

    lqueue_destroy(lq);
    printf("lqueue: all tests passed\n");

    return 0;
}
#endif /* SQ_TEST */
```

Compile and test:

```bash
gcc -std=c11 -Wall -Wextra -O2 -DSQ_TEST -o test_sq stack_queue.c && ./test_sq
```

---

## 4. Memory / ASCII Visualization

### Stack Operations

```
stack_push(s, A):    stack_push(s, B):    stack_push(s, C):
+---+---+---+---+   +---+---+---+---+   +---+---+---+---+
| A |   |   |   |   | A | B |   |   |   | A | B | C |   |
+---+---+---+---+   +---+---+---+---+   +---+---+---+---+
  ^top=1               ^   ^top=2           ^   ^   ^top=3

stack_pop(s):        returns C, top=2
+---+---+---+---+
| A | B |   |   |
+---+---+---+---+
  ^   ^top=2
```

### Circular Queue — Wrap-around

```
Initial: head=0, tail=0, size=0, cap=4
+---+---+---+---+
|   |   |   |   |
+---+---+---+---+
 ^h,t

After enqueue(A), enqueue(B), enqueue(C):
+---+---+---+---+
| A | B | C |   |    h=0, t=3, size=3
+---+---+---+---+
 ^h          ^t

After dequeue() -> A, dequeue() -> B:
+---+---+---+---+
|   |   | C |   |    h=2, t=3, size=1
+---+---+---+---+
         ^h  ^t

After enqueue(D), enqueue(E):
+---+---+---+---+
| E |   | C | D |    h=2, t=1, size=3  (wrapped!)
+---+---+---+---+
  ^t     ^h
```

### List Queue

```
lqueue_enqueue sequence: A -> B -> C

  head                          tail
   |                             |
   v                             v
 +---+---+    +---+---+    +---+---+
 | A | *-+--->| B | *-+--->| C | / |
 +---+---+    +---+---+    +---+---+

lqueue_dequeue():  returns A

  head              tail
   |                 |
   v                 v
 +---+---+    +---+---+
 | B | *-+--->| C | / |
 +---+---+    +---+---+
```

### Comparison Summary

```
+------------------+--------+--------+--------+----------+
| Operation        | Stack  | CQueue | LQueue | Notes    |
+------------------+--------+--------+--------+----------+
| push/enqueue     | O(1)*  | O(1)*  | O(1)   | *amort.  |
| pop/dequeue      | O(1)   | O(1)   | O(1)   |          |
| peek             | O(1)   | O(1)   | O(1)   |          |
| memory per elem  | 8 B    | 8 B    | 16+ B  | 64-bit   |
| cache locality   | good   | good   | poor   |          |
+------------------+--------+--------+--------+----------+
```
