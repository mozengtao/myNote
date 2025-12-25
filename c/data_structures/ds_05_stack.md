# Stack (Array vs List Implementation) in C

## 1. Definition & Design Principles

### What Problem Does This Data Structure Solve?

```
+------------------------------------------------------------------+
|  THE STACK: LAST-IN-FIRST-OUT (LIFO) ACCESS                      |
+------------------------------------------------------------------+

    PROBLEM:
    ┌─────────────────────────────────────────────────────────────┐
    │  Need to track "nested" or "reversible" operations:         │
    │  - Function call return addresses                           │
    │  - Undo operations                                          │
    │  - Expression parsing (matching parentheses)                │
    │  - Depth-first search traversal                             │
    │  - Backtracking algorithms                                  │
    └─────────────────────────────────────────────────────────────┘

    SOLUTION: STACK
    ┌─────────────────────────────────────────────────────────────┐
    │  • Only access the TOP element                              │
    │  • Push: add to top                                         │
    │  • Pop: remove from top                                     │
    │  • Last thing pushed is first thing popped                  │
    └─────────────────────────────────────────────────────────────┘

    VISUAL:
           ┌─────┐
           │  C  │  ← TOP (push/pop here)
           ├─────┤
           │  B  │
           ├─────┤
           │  A  │
           └─────┘
           BOTTOM

    Push order: A, B, C
    Pop order:  C, B, A (reversed!)
```

**中文解释：**
- 栈解决"嵌套"或"可逆"操作的跟踪问题
- LIFO（后进先出）：最后压入的元素最先弹出
- 用途：函数调用、撤销操作、表达式解析、深度优先搜索

### Invariants

```
+------------------------------------------------------------------+
|  STACK INVARIANTS                                                |
+------------------------------------------------------------------+

    1. LIFO ORDER
       pop() always returns the most recently pushed element

    2. TOP TRACKING
       A pointer/index always identifies the current top
       Empty stack: top is invalid or sentinel

    3. SIZE BOUNDS
       Array stack: 0 ≤ size ≤ capacity
       List stack: size ≥ 0, no upper bound

    4. OPERATION RESTRICTIONS
       Can only access top element
       No random access allowed (conceptually)
```

### Two Implementation Strategies

```
+------------------------------------------------------------------+
|  ARRAY-BASED vs LIST-BASED STACK                                 |
+------------------------------------------------------------------+

    ARRAY-BASED:
    ┌─────────────────────────────────────────────────────────────┐
    │  ┌───┬───┬───┬───┬───┬───┬───┬───┐                         │
    │  │ A │ B │ C │ ? │ ? │ ? │ ? │ ? │                         │
    │  └───┴───┴───┴───┴───┴───┴───┴───┘                         │
    │            ▲                                                │
    │           top (index 2)                                     │
    │                                                              │
    │  + Cache-friendly (contiguous memory)                       │
    │  + No allocation per push (until resize)                    │
    │  - Fixed capacity (or requires realloc)                     │
    │  - May waste memory if rarely full                          │
    └─────────────────────────────────────────────────────────────┘

    LIST-BASED:
    ┌─────────────────────────────────────────────────────────────┐
    │  top                                                        │
    │   │                                                         │
    │   ▼                                                         │
    │  ┌───┐   ┌───┐   ┌───┐                                     │
    │  │ C │──▶│ B │──▶│ A │──▶ NULL                             │
    │  └───┘   └───┘   └───┘                                     │
    │                                                              │
    │  + Unlimited capacity (until OOM)                           │
    │  + No wasted memory                                         │
    │  - Allocation per push                                      │
    │  - Cache-unfriendly (scattered nodes)                       │
    │  - Pointer overhead per element                             │
    └─────────────────────────────────────────────────────────────┘
```

**中文解释：**
- **数组栈**：连续内存，缓存友好，固定容量
- **链表栈**：无限容量，每次 push 分配内存，缓存不友好

---

## 2. Memory Model

### Array Stack Memory Layout

```
+------------------------------------------------------------------+
|  ARRAY STACK MEMORY LAYOUT                                       |
+------------------------------------------------------------------+

    struct array_stack {
        int *data;      /* Pointer to heap array */
        size_t top;     /* Index of next empty slot */
        size_t capacity;
    };

    Stack (on stack or heap):        Data array (on heap):
    ┌─────────────────────┐          ┌───┬───┬───┬───┬───┬───┐
    │ data ───────────────┼─────────▶│ A │ B │ C │ ? │ ? │ ? │
    │ top = 3             │          └───┴───┴───┴───┴───┴───┘
    │ capacity = 6        │           [0] [1] [2] [3] [4] [5]
    └─────────────────────┘                       ▲
                                                 top
    OPERATIONS:
    push(X): data[top++] = X     (increment after store)
    pop():   return data[--top]  (decrement before load)

    STATE TRANSITIONS:
    ┌─────────────────────────────────────────────────────────────┐
    │  Empty:  top = 0, data[0..capacity-1] unused               │
    │  1 elem: top = 1, data[0] = element                        │
    │  Full:   top = capacity, cannot push without resize        │
    └─────────────────────────────────────────────────────────────┘
```

### List Stack Memory Layout

```
+------------------------------------------------------------------+
|  LIST STACK MEMORY LAYOUT                                        |
+------------------------------------------------------------------+

    struct stack_node {
        int value;
        struct stack_node *next;
    };

    struct list_stack {
        struct stack_node *top;
        size_t size;
    };

    Stack head:                   Nodes (heap, scattered):
    ┌─────────────────┐
    │ top ────────────┼────────┐
    │ size = 3        │        │
    └─────────────────┘        │
                               ▼
                        ┌─────────────┐      ┌─────────────┐
                        │ value = C   │      │ value = B   │
                        │ next ───────┼─────▶│ next ───────┼───┐
                        └─────────────┘      └─────────────┘   │
                                                               ▼
                                                        ┌─────────────┐
                                                        │ value = A   │
                                                        │ next = NULL │
                                                        └─────────────┘

    OPERATIONS:
    push(X): new_node->next = top; top = new_node;
    pop():   old_top = top; top = top->next; return old_top->value;
```

### Cache Behavior

```
+------------------------------------------------------------------+
|  CACHE BEHAVIOR COMPARISON                                       |
+------------------------------------------------------------------+

    ARRAY STACK (excellent):
    ┌─────────────────────────────────────────────────────────────┐
    │  All elements in contiguous memory                          │
    │  Push/pop at end: always same cache line                    │
    │  Prefetching helps during resize-copy                       │
    │  Near-optimal for CPU cache                                 │
    └─────────────────────────────────────────────────────────────┘

    LIST STACK (poor):
    ┌─────────────────────────────────────────────────────────────┐
    │  Each node potentially on different cache line              │
    │  Every pop() = potential cache miss                         │
    │  malloc overhead compounds the problem                      │
    │  However: if nodes from pool, can be better                 │
    └─────────────────────────────────────────────────────────────┘

    PERFORMANCE REALITY:
    ┌─────────────────────────────────────────────────────────────┐
    │  For small stacks: array is 2-10× faster                    │
    │  For large stacks: array can be 10-50× faster               │
    │  Exception: if array keeps resizing, list may win           │
    └─────────────────────────────────────────────────────────────┘
```

### Failure Modes

```
+------------------------------------------------------------------+
|  COMMON FAILURE MODES                                            |
+------------------------------------------------------------------+

    1. STACK OVERFLOW (array)
    ┌─────────────────────────────────────────────────────────────┐
    │  Pushing beyond capacity without resize check               │
    │  Result: buffer overflow, memory corruption                 │
    └─────────────────────────────────────────────────────────────┘

    2. STACK UNDERFLOW (both)
    ┌─────────────────────────────────────────────────────────────┐
    │  Popping from empty stack                                   │
    │  Array: read garbage, potentially negative index            │
    │  List: dereference NULL pointer                             │
    └─────────────────────────────────────────────────────────────┘

    3. MEMORY LEAK (list)
    ┌─────────────────────────────────────────────────────────────┐
    │  Destroying stack without freeing all nodes                 │
    │  Each pop must free the removed node                        │
    └─────────────────────────────────────────────────────────────┘

    4. USE-AFTER-POP (list)
    ┌─────────────────────────────────────────────────────────────┐
    │  Keeping pointer to popped element after free               │
    │  Solution: return value, not pointer, from pop              │
    └─────────────────────────────────────────────────────────────┘
```

---

## 3. Typical Application Scenarios

### Where Stacks Are Used

```
+------------------------------------------------------------------+
|  REAL-WORLD STACK APPLICATIONS                                   |
+------------------------------------------------------------------+

    SYSTEM LEVEL:
    ┌─────────────────────────────────────────────────────────────┐
    │  • CPU call stack (hardware-supported!)                     │
    │  • Interrupt handling (save/restore context)                │
    │  • Virtual machine execution (bytecode interpreter)         │
    │  • Memory allocator (free block stack for LIFO reuse)       │
    └─────────────────────────────────────────────────────────────┘

    ALGORITHMS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Depth-first search (DFS)                                 │
    │  • Backtracking (maze solving, N-queens)                    │
    │  • Expression evaluation (postfix/infix)                    │
    │  • Parentheses matching                                     │
    │  • Undo/redo (command pattern)                              │
    └─────────────────────────────────────────────────────────────┘

    COMPILERS:
    ┌─────────────────────────────────────────────────────────────┐
    │  • Parser (shift-reduce)                                    │
    │  • Symbol table scope management                            │
    │  • Register allocation spilling                             │
    └─────────────────────────────────────────────────────────────┘
```

### When to Choose Array vs List

```
+------------------------------------------------------------------+
|  ARRAY STACK: WHEN TO USE                                        |
+------------------------------------------------------------------+

    ✓ Maximum size is known or bounded
    ✓ Performance is critical
    ✓ Stack is accessed frequently
    ✓ Memory is limited (no per-element overhead)
    ✓ Elements are small (int, pointer, etc.)

+------------------------------------------------------------------+
|  LIST STACK: WHEN TO USE                                         |
+------------------------------------------------------------------+

    ✓ Maximum size unknown and highly variable
    ✓ Push/pop frequency is low
    ✓ Memory for max capacity cannot be pre-allocated
    ✓ Elements are large (struct node overhead is small relative)
    ✓ Using intrusive list (no allocation overhead)
```

---

## 4. Complete C Examples

### Example 1: Array Stack (Fixed Capacity)

```c
/*
 * Example 1: Fixed-Capacity Array Stack
 *
 * Simplest implementation, no dynamic allocation for elements
 * Compile: gcc -Wall -Wextra -o array_stack array_stack.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define STACK_CAPACITY 16

struct array_stack {
    int data[STACK_CAPACITY];  /* Fixed-size array */
    size_t top;                /* Index of next empty slot */
};

/* Initialize stack */
void stack_init(struct array_stack *s)
{
    s->top = 0;
}

/* Check if empty */
bool stack_empty(const struct array_stack *s)
{
    return s->top == 0;
}

/* Check if full */
bool stack_full(const struct array_stack *s)
{
    return s->top == STACK_CAPACITY;
}

/* Get current size */
size_t stack_size(const struct array_stack *s)
{
    return s->top;
}

/* Push element (returns false if full) */
bool stack_push(struct array_stack *s, int value)
{
    if (stack_full(s))
        return false;
    
    s->data[s->top++] = value;
    return true;
}

/* Pop element (undefined behavior if empty - use assert in debug) */
int stack_pop(struct array_stack *s)
{
    assert(!stack_empty(s));  /* Fail fast in debug builds */
    return s->data[--s->top];
}

/* Peek at top without removing */
int stack_peek(const struct array_stack *s)
{
    assert(!stack_empty(s));
    return s->data[s->top - 1];
}

/* Print stack contents */
void stack_print(const struct array_stack *s)
{
    printf("Stack [size=%zu]: BOTTOM -> ", s->top);
    for (size_t i = 0; i < s->top; i++) {
        printf("%d ", s->data[i]);
    }
    printf("<- TOP\n");
}

int main(void)
{
    struct array_stack s;
    stack_init(&s);
    
    printf("=== Fixed Array Stack Demo ===\n\n");
    
    /* Push elements */
    for (int i = 1; i <= 5; i++) {
        stack_push(&s, i * 10);
        printf("Pushed %d: ", i * 10);
        stack_print(&s);
    }
    
    /* Peek */
    printf("\nPeek: %d\n", stack_peek(&s));
    
    /* Pop all */
    printf("\nPopping all:\n");
    while (!stack_empty(&s)) {
        printf("Popped: %d, ", stack_pop(&s));
        stack_print(&s);
    }
    
    /* Test overflow protection */
    printf("\n=== Testing capacity limit ===\n");
    for (int i = 0; i < STACK_CAPACITY + 2; i++) {
        if (stack_push(&s, i)) {
            printf("Pushed %d\n", i);
        } else {
            printf("FAILED to push %d (stack full!)\n", i);
        }
    }
    
    return 0;
}
```

---

### Example 2: Dynamic Array Stack (Auto-Resize)

```c
/*
 * Example 2: Dynamic Array Stack with Auto-Resize
 *
 * Grows automatically when full (like std::vector)
 * Compile: gcc -Wall -Wextra -o dynamic_stack dynamic_stack.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

struct dyn_stack {
    int *data;
    size_t top;
    size_t capacity;
};

#define INITIAL_CAPACITY 4
#define GROWTH_FACTOR 2

/* Initialize with initial capacity */
int dyn_stack_init(struct dyn_stack *s)
{
    s->data = malloc(INITIAL_CAPACITY * sizeof(int));
    if (!s->data)
        return -1;
    
    s->top = 0;
    s->capacity = INITIAL_CAPACITY;
    return 0;
}

/* Free resources */
void dyn_stack_destroy(struct dyn_stack *s)
{
    free(s->data);
    s->data = NULL;
    s->top = 0;
    s->capacity = 0;
}

bool dyn_stack_empty(const struct dyn_stack *s)
{
    return s->top == 0;
}

/* Internal: grow stack capacity */
static int dyn_stack_grow(struct dyn_stack *s)
{
    size_t new_capacity = s->capacity * GROWTH_FACTOR;
    int *new_data = realloc(s->data, new_capacity * sizeof(int));
    
    if (!new_data)
        return -1;
    
    s->data = new_data;
    s->capacity = new_capacity;
    return 0;
}

/* Push with auto-grow */
int dyn_stack_push(struct dyn_stack *s, int value)
{
    if (s->top >= s->capacity) {
        if (dyn_stack_grow(s) < 0)
            return -1;
    }
    
    s->data[s->top++] = value;
    return 0;
}

int dyn_stack_pop(struct dyn_stack *s)
{
    if (dyn_stack_empty(s)) {
        fprintf(stderr, "Error: pop from empty stack\n");
        exit(1);
    }
    return s->data[--s->top];
}

int dyn_stack_peek(const struct dyn_stack *s)
{
    if (dyn_stack_empty(s)) {
        fprintf(stderr, "Error: peek empty stack\n");
        exit(1);
    }
    return s->data[s->top - 1];
}

void dyn_stack_print(const struct dyn_stack *s)
{
    printf("Stack [size=%zu, cap=%zu]: ", s->top, s->capacity);
    for (size_t i = 0; i < s->top; i++)
        printf("%d ", s->data[i]);
    printf("\n");
}

int main(void)
{
    struct dyn_stack s;
    
    if (dyn_stack_init(&s) < 0) {
        fprintf(stderr, "Failed to init stack\n");
        return 1;
    }
    
    printf("=== Dynamic Array Stack Demo ===\n\n");
    
    /* Push many elements to trigger growth */
    printf("Pushing 20 elements (will trigger resizes):\n");
    for (int i = 0; i < 20; i++) {
        dyn_stack_push(&s, i);
        if (i < 5 || i >= 15)  /* Print first and last few */
            dyn_stack_print(&s);
        else if (i == 5)
            printf("... (growing) ...\n");
    }
    
    printf("\nFinal state:\n");
    dyn_stack_print(&s);
    
    printf("\nPopping 5 elements:\n");
    for (int i = 0; i < 5; i++) {
        printf("Popped: %d\n", dyn_stack_pop(&s));
    }
    dyn_stack_print(&s);
    
    dyn_stack_destroy(&s);
    return 0;
}
```

---

### Example 3: Linked List Stack

```c
/*
 * Example 3: Linked List Stack
 *
 * Unlimited capacity, allocation per push
 * Compile: gcc -Wall -Wextra -o list_stack list_stack.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct stack_node {
    int value;
    struct stack_node *next;
};

struct list_stack {
    struct stack_node *top;
    size_t size;
};

void list_stack_init(struct list_stack *s)
{
    s->top = NULL;
    s->size = 0;
}

bool list_stack_empty(const struct list_stack *s)
{
    return s->top == NULL;
}

size_t list_stack_size(const struct list_stack *s)
{
    return s->size;
}

/* Push: allocate new node, link to top */
int list_stack_push(struct list_stack *s, int value)
{
    struct stack_node *node = malloc(sizeof(*node));
    if (!node)
        return -1;
    
    node->value = value;
    node->next = s->top;  /* Point to old top */
    s->top = node;        /* New node becomes top */
    s->size++;
    
    return 0;
}

/* Pop: remove top, return value, free node */
int list_stack_pop(struct list_stack *s)
{
    if (list_stack_empty(s)) {
        fprintf(stderr, "Error: pop from empty stack\n");
        exit(1);
    }
    
    struct stack_node *old_top = s->top;
    int value = old_top->value;
    
    s->top = old_top->next;
    s->size--;
    
    free(old_top);
    return value;
}

int list_stack_peek(const struct list_stack *s)
{
    if (list_stack_empty(s)) {
        fprintf(stderr, "Error: peek empty stack\n");
        exit(1);
    }
    return s->top->value;
}

/* Destroy: free all remaining nodes */
void list_stack_destroy(struct list_stack *s)
{
    while (!list_stack_empty(s)) {
        struct stack_node *node = s->top;
        s->top = node->next;
        free(node);
    }
    s->size = 0;
}

void list_stack_print(const struct list_stack *s)
{
    printf("Stack [size=%zu]: TOP -> ", s->size);
    for (struct stack_node *n = s->top; n; n = n->next) {
        printf("%d -> ", n->value);
    }
    printf("NULL\n");
}

int main(void)
{
    struct list_stack s;
    list_stack_init(&s);
    
    printf("=== Linked List Stack Demo ===\n\n");
    
    /* Push elements */
    for (int i = 1; i <= 5; i++) {
        list_stack_push(&s, i * 100);
        printf("Pushed %d: ", i * 100);
        list_stack_print(&s);
    }
    
    /* Pop and print */
    printf("\nPopping:\n");
    while (!list_stack_empty(&s)) {
        printf("Popped %d: ", list_stack_pop(&s));
        list_stack_print(&s);
    }
    
    /* Push again and destroy */
    printf("\nPush some and destroy:\n");
    list_stack_push(&s, 1);
    list_stack_push(&s, 2);
    list_stack_push(&s, 3);
    list_stack_print(&s);
    
    list_stack_destroy(&s);
    printf("Destroyed. Size = %zu\n", s.size);
    
    return 0;
}
```

---

### Example 4: Real-World — Expression Evaluation

```c
/*
 * Example 4: Postfix Expression Evaluator Using Stack
 *
 * Real-world use case: calculator, compiler code generation
 * Compile: gcc -Wall -Wextra -o postfix_eval postfix_eval.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_STACK 256

struct eval_stack {
    int data[MAX_STACK];
    int top;
};

void stack_init(struct eval_stack *s) { s->top = 0; }
bool stack_empty(struct eval_stack *s) { return s->top == 0; }

void stack_push(struct eval_stack *s, int val)
{
    if (s->top >= MAX_STACK) {
        fprintf(stderr, "Stack overflow!\n");
        exit(1);
    }
    s->data[s->top++] = val;
}

int stack_pop(struct eval_stack *s)
{
    if (stack_empty(s)) {
        fprintf(stderr, "Stack underflow!\n");
        exit(1);
    }
    return s->data[--s->top];
}

/*
 * Evaluate postfix expression
 * Tokens: single digits and operators +, -, *, /
 * Example: "23+" means 2 + 3 = 5
 */
int evaluate_postfix(const char *expr)
{
    struct eval_stack s;
    stack_init(&s);
    
    for (const char *p = expr; *p; p++) {
        if (*p == ' ')
            continue;
        
        if (isdigit(*p)) {
            /* Push operand */
            stack_push(&s, *p - '0');
            printf("Push %c -> stack has %d elements\n", *p, s.top);
        } else {
            /* Pop two operands, apply operator, push result */
            int b = stack_pop(&s);  /* Second operand (on top) */
            int a = stack_pop(&s);  /* First operand */
            int result;
            
            switch (*p) {
            case '+': result = a + b; break;
            case '-': result = a - b; break;
            case '*': result = a * b; break;
            case '/': result = a / b; break;
            default:
                fprintf(stderr, "Unknown operator: %c\n", *p);
                exit(1);
            }
            
            stack_push(&s, result);
            printf("Apply %c to %d and %d -> %d\n", *p, a, b, result);
        }
    }
    
    if (s.top != 1) {
        fprintf(stderr, "Invalid expression (stack has %d items)\n", s.top);
        exit(1);
    }
    
    return stack_pop(&s);
}

int main(void)
{
    printf("=== Postfix Expression Evaluator ===\n\n");
    
    /* (2 + 3) * 4 = 20 in postfix: 2 3 + 4 * */
    const char *expr1 = "23+4*";
    printf("Expression: %s\n", expr1);
    printf("Meaning: (2 + 3) * 4\n");
    printf("Result: %d\n\n", evaluate_postfix(expr1));
    
    /* 5 * 6 + 7 = 37 in postfix: 5 6 * 7 + */
    const char *expr2 = "56*7+";
    printf("Expression: %s\n", expr2);
    printf("Meaning: 5 * 6 + 7\n");
    printf("Result: %d\n\n", evaluate_postfix(expr2));
    
    /* (3 + 4) * (5 - 2) = 21 in postfix: 3 4 + 5 2 - * */
    const char *expr3 = "34+52-*";
    printf("Expression: %s\n", expr3);
    printf("Meaning: (3 + 4) * (5 - 2)\n");
    printf("Result: %d\n", evaluate_postfix(expr3));
    
    return 0;
}
```

---

### Example 5: Intrusive Stack (Zero Allocation)

```c
/*
 * Example 5: Intrusive Stack (Linux Kernel Style)
 *
 * No per-push allocation - node is embedded in object
 * Compile: gcc -Wall -Wextra -o intrusive_stack intrusive_stack.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════
 * Intrusive stack node (embedded in your struct)
 * ═══════════════════════════════════════════════════════════════ */
struct slist_node {
    struct slist_node *next;
};

struct slist_head {
    struct slist_node *first;
};

#define SLIST_HEAD_INIT { .first = NULL }

static inline void slist_push(struct slist_node *node, struct slist_head *head)
{
    node->next = head->first;
    head->first = node;
}

static inline struct slist_node *slist_pop(struct slist_head *head)
{
    struct slist_node *node = head->first;
    if (node)
        head->first = node->next;
    return node;
}

static inline int slist_empty(struct slist_head *head)
{
    return head->first == NULL;
}

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/* ═══════════════════════════════════════════════════════════════
 * Example: Work item queue (like kernel workqueue)
 * ═══════════════════════════════════════════════════════════════ */

typedef void (*work_func_t)(void *data);

struct work_item {
    work_func_t func;
    void *data;
    struct slist_node stack_link;  /* Embedded stack node */
};

struct work_stack {
    struct slist_head head;
    int count;
};

void work_stack_init(struct work_stack *ws)
{
    ws->head.first = NULL;
    ws->count = 0;
}

void work_stack_push(struct work_stack *ws, struct work_item *item)
{
    slist_push(&item->stack_link, &ws->head);
    ws->count++;
}

struct work_item *work_stack_pop(struct work_stack *ws)
{
    struct slist_node *node = slist_pop(&ws->head);
    if (!node)
        return NULL;
    
    ws->count--;
    return container_of(node, struct work_item, stack_link);
}

/* Example work functions */
void print_message(void *data)
{
    printf("Message: %s\n", (char *)data);
}

void compute_square(void *data)
{
    int *val = (int *)data;
    printf("Square of %d = %d\n", *val, (*val) * (*val));
}

int main(void)
{
    printf("=== Intrusive Stack Demo ===\n\n");
    
    struct work_stack ws;
    work_stack_init(&ws);
    
    /* Create work items (could be from pool, not malloc) */
    struct work_item items[3];
    int numbers[] = {5, 7, 3};
    
    items[0].func = print_message;
    items[0].data = "Hello World!";
    
    items[1].func = compute_square;
    items[1].data = &numbers[0];
    
    items[2].func = compute_square;
    items[2].data = &numbers[1];
    
    /* Push work items */
    printf("Pushing work items...\n");
    work_stack_push(&ws, &items[0]);
    work_stack_push(&ws, &items[1]);
    work_stack_push(&ws, &items[2]);
    printf("Stack has %d items\n\n", ws.count);
    
    /* Process in LIFO order */
    printf("Processing (LIFO order):\n");
    struct work_item *item;
    while ((item = work_stack_pop(&ws)) != NULL) {
        item->func(item->data);
    }
    
    printf("\nStack has %d items\n", ws.count);
    printf("Note: No malloc/free for push/pop!\n");
    
    return 0;
}
```

---

## 5. Trade-offs & Comparisons

### Time Complexity

```
+------------------------------------------------------------------+
|  STACK OPERATION COMPLEXITY                                      |
+------------------------------------------------------------------+

    ┌─────────────────────┬─────────────────┬───────────────────────┐
    │ Operation           │ Array Stack     │ List Stack            │
    ├─────────────────────┼─────────────────┼───────────────────────┤
    │ Push                │ O(1) amortized  │ O(1)                  │
    │ Pop                 │ O(1)            │ O(1)                  │
    │ Peek                │ O(1)            │ O(1)                  │
    │ isEmpty             │ O(1)            │ O(1)                  │
    │ Size                │ O(1)            │ O(1) if tracked       │
    ├─────────────────────┼─────────────────┼───────────────────────┤
    │ REAL COST:          │                 │                       │
    │ Push (typical)      │ 1 store         │ malloc + 2 stores     │
    │ Pop (typical)       │ 1 load          │ 1 load + free         │
    │ Cache misses        │ Rare            │ Common                │
    └─────────────────────┴─────────────────┴───────────────────────┘
```

### Memory Comparison

```
+------------------------------------------------------------------+
|  MEMORY USAGE                                                    |
+------------------------------------------------------------------+

    ARRAY STACK (1000 integers, capacity 1024):
    ┌─────────────────────────────────────────────────────────────┐
    │  Data:     1024 × 4 = 4096 bytes                            │
    │  Metadata: ~16 bytes (pointer, top, capacity)               │
    │  Total:    ~4112 bytes                                      │
    │  Wasted:   (1024 - 1000) × 4 = 96 bytes (2.3%)              │
    └─────────────────────────────────────────────────────────────┘

    LIST STACK (1000 integers):
    ┌─────────────────────────────────────────────────────────────┐
    │  Per node: 4 (value) + 8 (next) + ~16 (malloc) = ~28 bytes  │
    │  Total:    1000 × 28 = ~28000 bytes                         │
    │  Overhead: 600% vs array!                                   │
    └─────────────────────────────────────────────────────────────┘
```

---

## 6. Design & Engineering Takeaways

### Rules of Thumb

```
+------------------------------------------------------------------+
|  STACK DESIGN RULES                                              |
+------------------------------------------------------------------+

    1. DEFAULT TO ARRAY STACK
       Simpler, faster, less memory
       Use list only when unbounded growth truly needed

    2. PRE-SIZE ARRAY IF POSSIBLE
       Avoid resizing overhead
       If max depth known, allocate it upfront

    3. CHECK EMPTY BEFORE POP
       Stack underflow is a serious bug
       Use assertions in debug, return error in release

    4. CONSIDER INTRUSIVE FOR SYSTEMS CODE
       No allocation overhead
       Objects can be on multiple stacks

    5. STACK SIZE = CALL DEPTH
       For recursive algorithms: max stack size = max recursion depth
       Watch for stack overflow in deep recursion
```

### Decision Framework

```
+------------------------------------------------------------------+
|  WHICH STACK IMPLEMENTATION?                                     |
+------------------------------------------------------------------+

    ARRAY (fixed or dynamic):
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Performance critical                                    │
    │  ✓ Size bounded or predictable                             │
    │  ✓ Elements are primitives or small structs                │
    │  ✓ Most common choice (90% of cases)                       │
    └─────────────────────────────────────────────────────────────┘

    LINKED LIST:
    ┌─────────────────────────────────────────────────────────────┐
    │  ✓ Unbounded, unpredictable growth                         │
    │  ✓ Elements are already node-like structures               │
    │  ✓ Using intrusive pattern                                 │
    │  ✓ Need to transfer elements between stacks cheaply        │
    └─────────────────────────────────────────────────────────────┘
```

---

## Summary

```
+------------------------------------------------------------------+
|  STACK: KEY TAKEAWAYS                                            |
+------------------------------------------------------------------+

    CORE CONCEPT:
    Last-In-First-Out (LIFO) access pattern
    Only operate on top element

    TWO IMPLEMENTATIONS:
    - Array: fast, cache-friendly, bounded or resizable
    - List: unbounded, allocation overhead, flexible

    COMMON USES:
    - Function call tracking
    - Expression evaluation
    - DFS traversal
    - Undo/redo systems
    - Backtracking algorithms

    BEST PRACTICE:
    Default to array stack unless you need unbounded growth
    or are using intrusive embedding pattern

    PERFORMANCE:
    Array stack is 2-50× faster than list stack
    due to cache locality and no allocation overhead
```

**中文总结：**
- **核心概念**：LIFO（后进先出），只操作栈顶元素
- **两种实现**：数组栈（快速、缓存友好）、链表栈（无界、分配开销）
- **常见用途**：函数调用、表达式求值、DFS、撤销/重做
- **最佳实践**：默认使用数组栈，除非需要无界增长

