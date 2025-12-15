# Linux Kernel Patterns: User-Space Code Examples

Complete, compilable user-space implementations of Linux kernel design patterns.

---

## 1. container_of Pattern

### 1.1 Concept Diagram

```
+============================================================================+
|                    container_of: FROM MEMBER TO CONTAINER                   |
+============================================================================+
|                                                                             |
|   Memory Layout:                                                            |
|   +----------------------------------+                                      |
|   | struct employee                  |                                      |
|   | +------------------------------+ | <- address: 0x1000                  |
|   | | char name[32]        offset 0| |                                      |
|   | +------------------------------+ |                                      |
|   | | int age             offset 32| |                                      |
|   | +------------------------------+ |                                      |
|   | | struct list_node    offset 36| | <- You have: 0x1024                 |
|   | +------------------------------+ |                                      |
|   | | int salary          offset 52| |                                      |
|   | +------------------------------+ |                                      |
|   +----------------------------------+                                      |
|                                                                             |
|   container_of(0x1024, struct employee, node)                              |
|   = 0x1024 - offsetof(struct employee, node)                               |
|   = 0x1024 - 36                                                             |
|   = 0x1000 (pointer to whole struct!)                                      |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 你有成员 `node` 的地址（0x1024），需要整个结构体的地址
- `container_of` 用成员地址减去成员在结构体中的偏移量
- 结果就是容器结构体的起始地址

### 1.2 Complete Example

```c
/*
 * container_of_example.c
 * 
 * Compile: gcc -o container_of_example container_of_example.c
 * Run:     ./container_of_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>  /* For offsetof */
#include <string.h>

/*============================================================================
 * CONTAINER_OF MACRO IMPLEMENTATION
 *============================================================================*/

/**
 * container_of - Get pointer to containing structure from member pointer
 * @ptr:    Pointer to the member
 * @type:   Type of the containing structure
 * @member: Name of the member within the structure
 *
 * This is the same macro used in the Linux kernel!
 */
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})
/* 
 * Key insight:
 * 1. ((type *)0)->member gets the type of member (without accessing memory)
 * 2. offsetof(type, member) calculates byte offset of member
 * 3. Subtract offset from ptr to get container start address
 */

/*============================================================================
 * LIST NODE STRUCTURE (Simplified version of kernel's list_head)
 *============================================================================*/

struct list_node {
    struct list_node *next;
    struct list_node *prev;
};

/*============================================================================
 * OUR DATA STRUCTURE - EMPLOYEE
 *============================================================================*/

struct employee {
    char name[32];           /* Employee name */
    int id;                  /* Employee ID */
    double salary;           /* Salary */
    struct list_node node;   /* Embedded list node - can be on any list */
};

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== container_of Pattern Demo ===\n\n");

    /* Create an employee */
    struct employee emp = {
        .name = "Alice",
        .id = 101,
        .salary = 75000.0,
    };

    /* Print memory layout info */
    printf("Memory Layout:\n");
    printf("  sizeof(struct employee) = %zu bytes\n", sizeof(struct employee));
    printf("  offsetof(name)   = %zu\n", offsetof(struct employee, name));
    printf("  offsetof(id)     = %zu\n", offsetof(struct employee, id));
    printf("  offsetof(salary) = %zu\n", offsetof(struct employee, salary));
    printf("  offsetof(node)   = %zu\n", offsetof(struct employee, node));
    printf("\n");

    /* Address of employee struct */
    printf("Address of emp:       %p\n", (void*)&emp);
    
    /* Address of embedded node (what we'd get from a list) */
    struct list_node *node_ptr = &emp.node;
    printf("Address of emp.node:  %p\n", (void*)node_ptr);
    printf("\n");

    /*=========================================================================
     * THE MAGIC: container_of
     *=========================================================================
     * Scenario: We're walking a list and have a pointer to the list_node,
     * but we need access to the full employee structure!
     */
    
    /* Use container_of to get back to the employee */
    struct employee *recovered = container_of(node_ptr, struct employee, node);
    /*                                         ^         ^                ^
     *                                         |         |                |
     *                                    pointer to   type of        name of
     *                                    the member   container      member
     */

    printf("Recovered employee address: %p\n", (void*)recovered);
    printf("Match: %s\n\n", (&emp == recovered) ? "YES!" : "NO");

    /* Now we can access all employee fields! */
    printf("Recovered employee data:\n");
    printf("  Name:   %s\n", recovered->name);
    printf("  ID:     %d\n", recovered->id);
    printf("  Salary: $%.2f\n", recovered->salary);

    /*=========================================================================
     * PRACTICAL EXAMPLE: Employee list
     *=========================================================================*/
    printf("\n=== Practical Example: Employee List ===\n\n");

    /* Create array of employees */
    struct employee team[3] = {
        { .name = "Bob",     .id = 102, .salary = 65000.0 },
        { .name = "Charlie", .id = 103, .salary = 70000.0 },
        { .name = "Diana",   .id = 104, .salary = 80000.0 },
    };

    /* Link them together using their embedded nodes */
    team[0].node.next = &team[1].node;
    team[1].node.next = &team[2].node;
    team[2].node.next = NULL;
    
    /* Walk the list using only node pointers */
    printf("Walking list via node pointers:\n");
    for (struct list_node *n = &team[0].node; n != NULL; n = n->next) {
        /* We only have 'n', but we need the employee! */
        struct employee *e = container_of(n, struct employee, node);
        printf("  Employee: %-10s ID: %d  Salary: $%.2f\n", 
               e->name, e->id, e->salary);
    }

    return 0;
}

/*
 * EXPECTED OUTPUT:
 * 
 * === container_of Pattern Demo ===
 * 
 * Memory Layout:
 *   sizeof(struct employee) = 64 bytes
 *   offsetof(name)   = 0
 *   offsetof(id)     = 32
 *   offsetof(salary) = 40
 *   offsetof(node)   = 48
 * 
 * Address of emp:       0x7ffd12345678
 * Address of emp.node:  0x7ffd123456a8
 * 
 * Recovered employee address: 0x7ffd12345678
 * Match: YES!
 * 
 * Recovered employee data:
 *   Name:   Alice
 *   ID:     101
 *   Salary: $75000.00
 * 
 * === Practical Example: Employee List ===
 * 
 * Walking list via node pointers:
 *   Employee: Bob        ID: 102  Salary: $65000.00
 *   Employee: Charlie    ID: 103  Salary: $70000.00
 *   Employee: Diana      ID: 104  Salary: $80000.00
 */
```

---

## 2. Reference Counting (kref) Pattern

### 2.1 Concept Diagram

```
+============================================================================+
|                    REFERENCE COUNTING LIFECYCLE                             |
+============================================================================+
|                                                                             |
|   Object Creation:    refcount = 1                                         |
|                                                                             |
|   +--------+                                                                |
|   | Object |     User A: get()  -> refcount = 2                            |
|   | ref: 1 |                                                                |
|   +--------+     User B: get()  -> refcount = 3                            |
|                                                                             |
|   +--------+     +--------+     +--------+                                  |
|   | User A |     | User B |     | Object |                                  |
|   |   ref  |---->|   ref  |---->| ref: 3 |                                  |
|   +--------+     +--------+     +--------+                                  |
|                                                                             |
|   User A: put()  -> refcount = 2  (Object still alive)                     |
|   User B: put()  -> refcount = 1  (Object still alive)                     |
|   Creator: put() -> refcount = 0  (RELEASE CALLBACK CALLED!)               |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 对象创建时 refcount=1（创建者持有引用）
- 每次 get() 增加计数，put() 减少计数
- 当计数归零时，自动调用释放回调函数

### 2.2 Complete Example

```c
/*
 * refcount_example.c
 * 
 * Compile: gcc -o refcount_example refcount_example.c -pthread
 * Run:     ./refcount_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>  /* C11 atomics for thread-safety */
#include <pthread.h>

/*============================================================================
 * KREF IMPLEMENTATION (User-space version)
 *============================================================================*/

struct kref {
    atomic_int refcount;  /* Atomic counter for thread safety */
};

/* Initialize reference count to 1 (creator holds first reference) */
static inline void kref_init(struct kref *kref)
{
    atomic_init(&kref->refcount, 1);
}

/* Get current reference count (for debugging) */
static inline int kref_read(struct kref *kref)
{
    return atomic_load(&kref->refcount);
}

/* Acquire a reference (increment count) */
static inline void kref_get(struct kref *kref)
{
    int old = atomic_fetch_add(&kref->refcount, 1);
    /* Warn if count was 0 (use-after-free bug!) */
    if (old == 0) {
        fprintf(stderr, "BUG: kref_get on zero refcount!\n");
        abort();
    }
}

/**
 * kref_put - Release a reference
 * @kref: Reference counter
 * @release: Callback to free the object when count reaches 0
 * 
 * Returns 1 if object was freed, 0 otherwise
 */
static inline int kref_put(struct kref *kref, 
                           void (*release)(struct kref *kref))
{
    /* Decrement and check if we reached zero */
    if (atomic_fetch_sub(&kref->refcount, 1) == 1) {
        /* Count was 1, now 0 -> call release function */
        release(kref);
        return 1;  /* Object was freed */
    }
    return 0;  /* Object still has references */
}

/*============================================================================
 * OUR DATA STRUCTURE - DOCUMENT
 *============================================================================*/

struct document {
    struct kref ref;      /* Reference counter - MUST BE FIRST or use container_of */
    char *title;          /* Document title (dynamically allocated) */
    char *content;        /* Document content */
    int id;               /* Document ID */
};

/* Forward declaration */
static void document_release(struct kref *kref);

/* Create a new document */
struct document *document_create(int id, const char *title, const char *content)
{
    struct document *doc = malloc(sizeof(*doc));
    if (!doc) return NULL;
    
    doc->title = strdup(title);
    doc->content = strdup(content);
    doc->id = id;
    
    kref_init(&doc->ref);  /* refcount = 1 */
    
    printf("[DOC %d] Created '%s' (refcount=%d)\n", 
           doc->id, doc->title, kref_read(&doc->ref));
    return doc;
}

/* Get a reference to document */
struct document *document_get(struct document *doc)
{
    kref_get(&doc->ref);  /* refcount++ */
    printf("[DOC %d] Get reference (refcount=%d)\n", 
           doc->id, kref_read(&doc->ref));
    return doc;
}

/* Release a reference to document */
void document_put(struct document *doc)
{
    printf("[DOC %d] Put reference (refcount=%d -> %d)\n", 
           doc->id, kref_read(&doc->ref), kref_read(&doc->ref) - 1);
    kref_put(&doc->ref, document_release);
}

/* Release callback - called when refcount reaches 0 */
static void document_release(struct kref *kref)
{
    /* Use container_of to get document from kref */
    struct document *doc = (struct document *)kref;  /* kref is first member */
    /* Or: doc = container_of(kref, struct document, ref); */
    
    printf("[DOC %d] RELEASING '%s' - freeing memory!\n", doc->id, doc->title);
    free(doc->title);
    free(doc->content);
    free(doc);
}

/*============================================================================
 * SIMULATED USERS
 *============================================================================*/

struct user_context {
    const char *name;
    struct document *doc;
};

void *user_thread(void *arg)
{
    struct user_context *ctx = arg;
    struct document *doc = ctx->doc;
    
    printf("[%s] Starting to use document %d\n", ctx->name, doc->id);
    
    /* Simulate some work */
    for (int i = 0; i < 3; i++) {
        printf("[%s] Reading: %s\n", ctx->name, doc->title);
        usleep(100000);  /* 100ms */
    }
    
    printf("[%s] Done, releasing reference\n", ctx->name);
    document_put(doc);  /* Release our reference */
    
    return NULL;
}

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== Reference Counting (kref) Pattern Demo ===\n\n");

    /* Create document - creator holds refcount=1 */
    struct document *doc = document_create(42, "Linux Kernel Patterns", 
                                           "This document explains...");
    printf("\n");

    /* Multiple users want to access the document */
    printf("--- Multiple users acquiring references ---\n");
    struct document *user_a_ref = document_get(doc);  /* refcount=2 */
    struct document *user_b_ref = document_get(doc);  /* refcount=3 */
    struct document *user_c_ref = document_get(doc);  /* refcount=4 */
    printf("\n");

    /* Users release in different order */
    printf("--- Users releasing references ---\n");
    document_put(user_b_ref);  /* refcount=3 */
    document_put(user_a_ref);  /* refcount=2 */
    document_put(user_c_ref);  /* refcount=1 */
    printf("\n");

    /* Original creator releases last reference */
    printf("--- Creator releases final reference ---\n");
    document_put(doc);  /* refcount=0 -> document_release() called! */
    printf("\n");

    /*=========================================================================
     * MULTI-THREADED EXAMPLE
     *=========================================================================*/
    printf("=== Multi-threaded Example ===\n\n");

    doc = document_create(99, "Shared Resource", "Data shared between threads");
    
    /* Create contexts for threads - each gets their own reference */
    struct user_context ctx_a = { .name = "ThreadA", .doc = document_get(doc) };
    struct user_context ctx_b = { .name = "ThreadB", .doc = document_get(doc) };
    printf("\n");

    /* Start threads */
    pthread_t thread_a, thread_b;
    pthread_create(&thread_a, NULL, user_thread, &ctx_a);
    pthread_create(&thread_b, NULL, user_thread, &ctx_b);

    /* Creator releases its reference */
    printf("[Main] Releasing creator's reference\n");
    document_put(doc);
    printf("[Main] Creator done, document may still be in use by threads\n\n");

    /* Wait for threads */
    pthread_join(thread_a, NULL);
    pthread_join(thread_b, NULL);

    printf("\nDocument freed after last thread finished!\n");

    return 0;
}

/*
 * EXPECTED OUTPUT:
 * 
 * === Reference Counting (kref) Pattern Demo ===
 * 
 * [DOC 42] Created 'Linux Kernel Patterns' (refcount=1)
 * 
 * --- Multiple users acquiring references ---
 * [DOC 42] Get reference (refcount=2)
 * [DOC 42] Get reference (refcount=3)
 * [DOC 42] Get reference (refcount=4)
 * 
 * --- Users releasing references ---
 * [DOC 42] Put reference (refcount=4 -> 3)
 * [DOC 42] Put reference (refcount=3 -> 2)
 * [DOC 42] Put reference (refcount=2 -> 1)
 * 
 * --- Creator releases final reference ---
 * [DOC 42] Put reference (refcount=1 -> 0)
 * [DOC 42] RELEASING 'Linux Kernel Patterns' - freeing memory!
 */
```

---

## 3. Linked List (list_head) Pattern

### 3.1 Concept Diagram

```
+============================================================================+
|                    EMBEDDED LINKED LIST DESIGN                              |
+============================================================================+
|                                                                             |
|   Traditional (External Node):         Linux (Embedded Node):              |
|                                                                             |
|   +--------+     +--------+            +------------------+                 |
|   | Node   |     | Node   |            | struct task      |                 |
|   | data*--|---->| Data A |            | +--------------+ |                 |
|   | next*  |     +--------+            | | list_head    | | <-- embedded   |
|   +--------+                           | +--------------+ |                 |
|       |                                | | pid          | |                 |
|       v                                | | name         | |                 |
|   +--------+     +--------+            +------------------+                 |
|   | Node   |     | Data B |                    ^                            |
|   | data*--|---->+--------+                    |                            |
|   | next*  |                           Use container_of() to get task      |
|   +--------+                           from list_head pointer              |
|                                                                             |
|   ADVANTAGE of embedded design:                                             |
|   - One object can be on MULTIPLE lists simultaneously!                    |
|                                                                             |
|   struct task {                                                             |
|       struct list_head run_list;    /* On runqueue */                      |
|       struct list_head wait_list;   /* On waitqueue */                     |
|       struct list_head sibling;     /* In parent's children list */        |
|   };                                                                        |
|                                                                             |
+============================================================================+
```

### 3.2 Complete Example

```c
/*
 * list_example.c
 * 
 * Compile: gcc -o list_example list_example.c
 * Run:     ./list_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/*============================================================================
 * CONTAINER_OF (needed for list traversal)
 *============================================================================*/

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/*============================================================================
 * LIST_HEAD IMPLEMENTATION (Linux kernel style)
 *============================================================================*/

struct list_head {
    struct list_head *next, *prev;
};

/* Initialize a list head (empty list points to itself) */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;  /* Empty list: next points to itself */
    list->prev = list;  /* Empty list: prev points to itself */
}

/* Check if list is empty */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;  /* Empty if pointing to itself */
}

/* Internal: insert new entry between two known entries */
static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;   /* next's prev now points to new */
    new->next = next;   /* new's next points to next */
    new->prev = prev;   /* new's prev points to prev */
    prev->next = new;   /* prev's next now points to new */
}

/* Add to head of list (after head, before first element) */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

/* Add to tail of list (before head, after last element) */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/* Internal: remove entry by connecting prev and next */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/* Remove entry from list */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;  /* Poison pointers to catch bugs */
    entry->prev = NULL;
}

/* Iterate through list */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* Iterate with entry access (using container_of) */
#define list_for_each_entry(pos, head, member)                          \
    for (pos = container_of((head)->next, typeof(*pos), member);        \
         &pos->member != (head);                                         \
         pos = container_of(pos->member.next, typeof(*pos), member))

/* Safe iteration for deletion */
#define list_for_each_entry_safe(pos, n, head, member)                  \
    for (pos = container_of((head)->next, typeof(*pos), member),        \
         n = container_of(pos->member.next, typeof(*pos), member);      \
         &pos->member != (head);                                         \
         pos = n, n = container_of(n->member.next, typeof(*n), member))

/*============================================================================
 * APPLICATION: TASK SCHEDULER SIMULATION
 *============================================================================*/

/* Process states */
enum task_state {
    TASK_RUNNING,
    TASK_WAITING,
    TASK_STOPPED
};

const char *state_names[] = { "RUNNING", "WAITING", "STOPPED" };

/* Task structure with MULTIPLE embedded list nodes */
struct task {
    int pid;
    char name[32];
    int priority;
    enum task_state state;
    
    /* Embedded list nodes - task can be on multiple lists! */
    struct list_head run_list;    /* For runqueue */
    struct list_head all_list;    /* For all tasks list */
};

/* Global lists */
LIST_HEAD(runqueue);      /* Tasks ready to run */
LIST_HEAD(waitqueue);     /* Tasks waiting for I/O */
LIST_HEAD(all_tasks);     /* All tasks in system */

/* Create a new task */
struct task *task_create(int pid, const char *name, int priority)
{
    struct task *t = malloc(sizeof(*t));
    t->pid = pid;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->priority = priority;
    t->state = TASK_RUNNING;
    
    INIT_LIST_HEAD(&t->run_list);  /* Initialize list nodes */
    INIT_LIST_HEAD(&t->all_list);
    
    /* Add to all tasks list */
    list_add_tail(&t->all_list, &all_tasks);
    
    /* Add to runqueue */
    list_add_tail(&t->run_list, &runqueue);
    
    printf("[SCHED] Created task %d '%s' (priority %d)\n", 
           pid, name, priority);
    return t;
}

/* Move task to wait queue */
void task_wait(struct task *t)
{
    if (t->state == TASK_RUNNING) {
        list_del(&t->run_list);            /* Remove from runqueue */
        list_add_tail(&t->run_list, &waitqueue);  /* Add to waitqueue */
        t->state = TASK_WAITING;
        printf("[SCHED] Task %d '%s' -> WAITING\n", t->pid, t->name);
    }
}

/* Wake up task (move back to runqueue) */
void task_wakeup(struct task *t)
{
    if (t->state == TASK_WAITING) {
        list_del(&t->run_list);            /* Remove from waitqueue */
        list_add_tail(&t->run_list, &runqueue);   /* Add to runqueue */
        t->state = TASK_RUNNING;
        printf("[SCHED] Task %d '%s' -> RUNNING\n", t->pid, t->name);
    }
}

/* Print all tasks in a list */
void print_list(const char *name, struct list_head *head)
{
    struct task *t;
    printf("\n%s:\n", name);
    if (list_empty(head)) {
        printf("  (empty)\n");
        return;
    }
    
    /* Use list_for_each_entry to iterate */
    list_for_each_entry(t, head, run_list) {
        printf("  [%d] %-10s priority=%d state=%s\n", 
               t->pid, t->name, t->priority, state_names[t->state]);
    }
}

/* Print all tasks (using all_list) */
void print_all_tasks(void)
{
    struct task *t;
    printf("\nAll Tasks in System:\n");
    list_for_each_entry(t, &all_tasks, all_list) {
        printf("  [%d] %-10s priority=%d state=%s\n", 
               t->pid, t->name, t->priority, state_names[t->state]);
    }
}

/* Destroy all tasks */
void destroy_all_tasks(void)
{
    struct task *t, *tmp;
    
    /* Use _safe version when deleting during iteration */
    list_for_each_entry_safe(t, tmp, &all_tasks, all_list) {
        printf("[SCHED] Destroying task %d '%s'\n", t->pid, t->name);
        list_del(&t->all_list);
        list_del(&t->run_list);
        free(t);
    }
}

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== Linux Linked List (list_head) Pattern Demo ===\n\n");

    /* Create several tasks */
    struct task *t1 = task_create(1, "init",    10);
    struct task *t2 = task_create(2, "kworker", 20);
    struct task *t3 = task_create(3, "bash",    15);
    struct task *t4 = task_create(4, "nginx",   25);
    struct task *t5 = task_create(5, "mysql",   30);

    /* Show initial state */
    print_list("Run Queue", &runqueue);
    print_list("Wait Queue", &waitqueue);
    print_all_tasks();

    /* Simulate some tasks waiting for I/O */
    printf("\n--- Simulating I/O wait ---\n");
    task_wait(t3);  /* bash waits for user input */
    task_wait(t5);  /* mysql waits for disk I/O */

    print_list("Run Queue", &runqueue);
    print_list("Wait Queue", &waitqueue);

    /* Wake up a task */
    printf("\n--- I/O complete, waking up tasks ---\n");
    task_wakeup(t5);  /* mysql I/O complete */

    print_list("Run Queue", &runqueue);
    print_list("Wait Queue", &waitqueue);

    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    destroy_all_tasks();

    printf("\nDone!\n");
    return 0;
}
```

---

## 4. Wait Queue Pattern

### 4.1 Concept Diagram

```
+============================================================================+
|                         WAIT QUEUE MECHANISM                                |
+============================================================================+
|                                                                             |
|   Producer                            Consumer                              |
|   ========                            ========                              |
|                                       wait_event(wq, data_ready)           |
|                                            |                                |
|                                            v                                |
|   +------------------+              +------------------+                    |
|   | Produce data     |              | Check condition  |                    |
|   +------------------+              +------------------+                    |
|          |                                 |                                |
|          v                          [data_ready=false]                      |
|   +------------------+                     |                                |
|   | data_ready = true|                     v                                |
|   +------------------+              +------------------+                    |
|          |                          |     SLEEP        |                    |
|          v                          | (added to queue) |                    |
|   +------------------+              +------------------+                    |
|   | wake_up(&wq)     |------------>        |                                |
|   +------------------+                     v                                |
|                                     +------------------+                    |
|                                     |    WAKE UP       |                    |
|                                     +------------------+                    |
|                                            |                                |
|                                            v                                |
|                                     +------------------+                    |
|                                     | Check condition  |                    |
|                                     +------------------+                    |
|                                            |                                |
|                                     [data_ready=true]                       |
|                                            |                                |
|                                            v                                |
|                                     +------------------+                    |
|                                     | Process data     |                    |
|                                     +------------------+                    |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 消费者调用 `wait_event()`，检查条件，若不满足则睡眠
- 生产者准备好数据后调用 `wake_up()` 唤醒等待者
- 被唤醒后重新检查条件，满足则继续执行

### 4.2 Complete Example

```c
/*
 * waitqueue_example.c - Producer-Consumer with Wait Queue
 * 
 * Compile: gcc -o waitqueue_example waitqueue_example.c -pthread
 * Run:     ./waitqueue_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

/*============================================================================
 * WAIT QUEUE IMPLEMENTATION (User-space simulation)
 *============================================================================*/

struct wait_queue_head {
    pthread_mutex_t lock;
    pthread_cond_t cond;
};

#define DECLARE_WAIT_QUEUE_HEAD(name) \
    struct wait_queue_head name = { \
        .lock = PTHREAD_MUTEX_INITIALIZER, \
        .cond = PTHREAD_COND_INITIALIZER \
    }

static inline void init_waitqueue_head(struct wait_queue_head *wq)
{
    pthread_mutex_init(&wq->lock, NULL);
    pthread_cond_init(&wq->cond, NULL);
}

/**
 * wait_event - Sleep until condition becomes true
 * @wq: Wait queue
 * @condition: Condition to check
 * 
 * The condition is checked under the lock, and we sleep if false.
 * When woken, we re-check the condition (spurious wakeups!)
 */
#define wait_event(wq, condition)                               \
    do {                                                        \
        pthread_mutex_lock(&(wq).lock);                         \
        while (!(condition)) {                                  \
            pthread_cond_wait(&(wq).cond, &(wq).lock);          \
        }                                                       \
        pthread_mutex_unlock(&(wq).lock);                       \
    } while (0)

/**
 * wait_event_timeout - Sleep until condition or timeout
 * @wq: Wait queue
 * @condition: Condition to check
 * @timeout_ms: Timeout in milliseconds
 * @result: Variable to store result (0=timeout, 1=condition met)
 */
#define wait_event_timeout(wq, condition, timeout_ms, result)   \
    do {                                                        \
        struct timespec ts;                                     \
        clock_gettime(CLOCK_REALTIME, &ts);                     \
        ts.tv_sec += (timeout_ms) / 1000;                       \
        ts.tv_nsec += ((timeout_ms) % 1000) * 1000000;          \
        if (ts.tv_nsec >= 1000000000) {                         \
            ts.tv_sec++; ts.tv_nsec -= 1000000000;              \
        }                                                       \
        pthread_mutex_lock(&(wq).lock);                         \
        result = 1;                                             \
        while (!(condition)) {                                  \
            if (pthread_cond_timedwait(&(wq).cond, &(wq).lock, &ts)) { \
                result = 0; break;                              \
            }                                                   \
        }                                                       \
        pthread_mutex_unlock(&(wq).lock);                       \
    } while (0)

/* Wake up one waiter */
static inline void wake_up(struct wait_queue_head *wq)
{
    pthread_mutex_lock(&wq->lock);
    pthread_cond_signal(&wq->cond);  /* Wake one */
    pthread_mutex_unlock(&wq->lock);
}

/* Wake up all waiters */
static inline void wake_up_all(struct wait_queue_head *wq)
{
    pthread_mutex_lock(&wq->lock);
    pthread_cond_broadcast(&wq->cond);  /* Wake all */
    pthread_mutex_unlock(&wq->lock);
}

/*============================================================================
 * APPLICATION: BOUNDED BUFFER (PRODUCER-CONSUMER)
 *============================================================================*/

#define BUFFER_SIZE 5

struct bounded_buffer {
    int data[BUFFER_SIZE];
    int head, tail;
    int count;
    
    struct wait_queue_head not_full;   /* Producers wait here */
    struct wait_queue_head not_empty;  /* Consumers wait here */
    pthread_mutex_t mutex;
    
    bool shutdown;
};

struct bounded_buffer buffer = {
    .head = 0, .tail = 0, .count = 0,
    .not_full = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER },
    .not_empty = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER },
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .shutdown = false
};

/* Check conditions */
bool buffer_not_full(void)  { return buffer.count < BUFFER_SIZE || buffer.shutdown; }
bool buffer_not_empty(void) { return buffer.count > 0 || buffer.shutdown; }

/* Produce an item */
bool produce(int item)
{
    /* Wait until buffer is not full */
    printf("[Producer] Waiting to produce %d...\n", item);
    wait_event(buffer.not_full, buffer_not_full());
    
    if (buffer.shutdown) return false;
    
    pthread_mutex_lock(&buffer.mutex);
    
    buffer.data[buffer.tail] = item;
    buffer.tail = (buffer.tail + 1) % BUFFER_SIZE;
    buffer.count++;
    
    printf("[Producer] Produced %d (count=%d)\n", item, buffer.count);
    
    pthread_mutex_unlock(&buffer.mutex);
    
    /* Wake up consumers */
    wake_up(&buffer.not_empty);
    
    return true;
}

/* Consume an item */
bool consume(int *item)
{
    /* Wait until buffer is not empty */
    printf("[Consumer] Waiting for data...\n");
    wait_event(buffer.not_empty, buffer_not_empty());
    
    if (buffer.shutdown && buffer.count == 0) return false;
    
    pthread_mutex_lock(&buffer.mutex);
    
    *item = buffer.data[buffer.head];
    buffer.head = (buffer.head + 1) % BUFFER_SIZE;
    buffer.count--;
    
    printf("[Consumer] Consumed %d (count=%d)\n", *item, buffer.count);
    
    pthread_mutex_unlock(&buffer.mutex);
    
    /* Wake up producers */
    wake_up(&buffer.not_full);
    
    return true;
}

/*============================================================================
 * PRODUCER AND CONSUMER THREADS
 *============================================================================*/

void *producer_thread(void *arg)
{
    int id = *(int *)arg;
    
    for (int i = 1; i <= 8; i++) {
        int item = id * 100 + i;
        if (!produce(item)) break;
        usleep(100000);  /* 100ms between productions */
    }
    
    printf("[Producer %d] Finished\n", id);
    return NULL;
}

void *consumer_thread(void *arg)
{
    int id = *(int *)arg;
    
    while (1) {
        int item;
        if (!consume(&item)) break;
        usleep(150000);  /* 150ms to "process" item */
    }
    
    printf("[Consumer %d] Finished\n", id);
    return NULL;
}

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== Wait Queue Pattern Demo ===\n");
    printf("=== Producer-Consumer with Bounded Buffer ===\n\n");

    pthread_t producers[2], consumers[2];
    int producer_ids[] = {1, 2};
    int consumer_ids[] = {1, 2};

    /* Start producers and consumers */
    pthread_create(&producers[0], NULL, producer_thread, &producer_ids[0]);
    pthread_create(&producers[1], NULL, producer_thread, &producer_ids[1]);
    pthread_create(&consumers[0], NULL, consumer_thread, &consumer_ids[0]);
    pthread_create(&consumers[1], NULL, consumer_thread, &consumer_ids[1]);

    /* Wait for producers to finish */
    pthread_join(producers[0], NULL);
    pthread_join(producers[1], NULL);

    /* Give consumers time to drain buffer */
    sleep(2);

    /* Signal shutdown */
    printf("\n[Main] Signaling shutdown...\n");
    buffer.shutdown = true;
    wake_up_all(&buffer.not_empty);
    wake_up_all(&buffer.not_full);

    /* Wait for consumers */
    pthread_join(consumers[0], NULL);
    pthread_join(consumers[1], NULL);

    printf("\nDone! All threads finished.\n");
    return 0;
}
```

---

## 5. goto Cleanup Pattern

### 5.1 Concept Diagram

```
+============================================================================+
|                      GOTO CLEANUP PATTERN                                   |
+============================================================================+
|                                                                             |
|   WITHOUT goto (deeply nested):       WITH goto (flat, clear):             |
|                                                                             |
|   if (alloc_a()) {                    a = alloc_a();                        |
|       if (alloc_b()) {                if (!a)                               |
|           if (alloc_c()) {                goto err_a;                       |
|               /* success */                                                 |
|           } else {                    b = alloc_b();                        |
|               free(b);                if (!b)                               |
|               free(a);                    goto err_b;                       |
|               return -1;                                                    |
|           }                           c = alloc_c();                        |
|       } else {                        if (!c)                               |
|           free(a);                        goto err_c;                       |
|           return -1;                                                        |
|       }                               return 0; /* SUCCESS */               |
|   } else {                                                                  |
|       return -1;                      err_c: free(b);                       |
|   }                                   err_b: free(a);                       |
|                                       err_a: return -1;                     |
|                                                                             |
|   The goto version is:                                                      |
|   - Flatter (less nesting)                                                  |
|   - Easier to read                                                          |
|   - Error handling in one place                                             |
|   - Harder to forget cleanup steps                                          |
|                                                                             |
+============================================================================+
```

**中文说明**：
- `goto cleanup` 是内核中唯一推荐使用 goto 的场景
- 资源按获取顺序分配，按逆序释放
- 错误处理集中在函数末尾，清晰易维护

### 5.2 Complete Example

```c
/*
 * goto_cleanup_example.c - Structured Error Handling
 * 
 * Compile: gcc -o goto_cleanup_example goto_cleanup_example.c
 * Run:     ./goto_cleanup_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

/*============================================================================
 * SIMULATED RESOURCE TYPES
 *============================================================================*/

/* Simulated file handle */
struct file_handle {
    char name[64];
    int fd;
};

/* Simulated memory buffer */
struct buffer {
    void *data;
    size_t size;
};

/* Simulated device */
struct device {
    char name[32];
    int irq;
    bool initialized;
};

/*============================================================================
 * SIMULATED RESOURCE OPERATIONS (some may fail!)
 *============================================================================*/

/* Simulate opening a file (may fail) */
struct file_handle *open_file(const char *name, bool should_fail)
{
    printf("  [OPEN] Opening file '%s'...\n", name);
    
    if (should_fail) {
        printf("  [OPEN] FAILED!\n");
        return NULL;
    }
    
    struct file_handle *fh = malloc(sizeof(*fh));
    strcpy(fh->name, name);
    fh->fd = rand() % 1000;
    printf("  [OPEN] Success (fd=%d)\n", fh->fd);
    return fh;
}

void close_file(struct file_handle *fh)
{
    printf("  [CLOSE] Closing file '%s' (fd=%d)\n", fh->name, fh->fd);
    free(fh);
}

/* Simulate allocating buffer (may fail) */
struct buffer *alloc_buffer(size_t size, bool should_fail)
{
    printf("  [ALLOC] Allocating %zu bytes...\n", size);
    
    if (should_fail) {
        printf("  [ALLOC] FAILED!\n");
        return NULL;
    }
    
    struct buffer *buf = malloc(sizeof(*buf));
    buf->data = malloc(size);
    buf->size = size;
    printf("  [ALLOC] Success (ptr=%p)\n", buf->data);
    return buf;
}

void free_buffer(struct buffer *buf)
{
    printf("  [FREE] Freeing buffer (ptr=%p, size=%zu)\n", buf->data, buf->size);
    free(buf->data);
    free(buf);
}

/* Simulate initializing device (may fail) */
int init_device(struct device *dev, const char *name, bool should_fail)
{
    printf("  [INIT] Initializing device '%s'...\n", name);
    
    if (should_fail) {
        printf("  [INIT] FAILED!\n");
        return -1;
    }
    
    strcpy(dev->name, name);
    dev->irq = rand() % 256;
    dev->initialized = true;
    printf("  [INIT] Success (irq=%d)\n", dev->irq);
    return 0;
}

void cleanup_device(struct device *dev)
{
    printf("  [CLEANUP] Cleaning up device '%s'\n", dev->name);
    dev->initialized = false;
}

/*============================================================================
 * COMPLEX INITIALIZATION WITH GOTO CLEANUP
 *============================================================================*/

/**
 * initialize_system - Initialize system with multiple resources
 * 
 * This function demonstrates the goto cleanup pattern.
 * Resources are allocated in order, and cleaned up in reverse order on failure.
 */
int initialize_system(bool fail_at_step)
{
    struct file_handle *config_file = NULL;
    struct file_handle *log_file = NULL;
    struct buffer *io_buffer = NULL;
    struct buffer *cache = NULL;
    struct device device;
    int ret = 0;

    printf("\n=== Starting System Initialization ===\n");

    /*=========================================================================
     * STEP 1: Open config file
     *=========================================================================*/
    config_file = open_file("/etc/myapp.conf", fail_at_step == 1);
    if (!config_file) {
        ret = -ENOENT;
        goto err_config;  /* Nothing to clean up yet */
    }

    /*=========================================================================
     * STEP 2: Open log file
     *=========================================================================*/
    log_file = open_file("/var/log/myapp.log", fail_at_step == 2);
    if (!log_file) {
        ret = -ENOENT;
        goto err_log;     /* Must clean up config_file */
    }

    /*=========================================================================
     * STEP 3: Allocate I/O buffer
     *=========================================================================*/
    io_buffer = alloc_buffer(4096, fail_at_step == 3);
    if (!io_buffer) {
        ret = -ENOMEM;
        goto err_io;      /* Must clean up log_file and config_file */
    }

    /*=========================================================================
     * STEP 4: Allocate cache
     *=========================================================================*/
    cache = alloc_buffer(65536, fail_at_step == 4);
    if (!cache) {
        ret = -ENOMEM;
        goto err_cache;   /* Must clean up io_buffer, log_file, config_file */
    }

    /*=========================================================================
     * STEP 5: Initialize device
     *=========================================================================*/
    ret = init_device(&device, "mydevice0", fail_at_step == 5);
    if (ret) {
        goto err_device;  /* Must clean up cache, io_buffer, log_file, config_file */
    }

    /*=========================================================================
     * SUCCESS!
     *=========================================================================*/
    printf("\n=== System Initialization SUCCESSFUL! ===\n");
    
    /* In real code, we'd store these resources somewhere */
    /* For demo, clean up immediately */
    printf("\n=== Cleaning up after successful init ===\n");
    cleanup_device(&device);
    free_buffer(cache);
    free_buffer(io_buffer);
    close_file(log_file);
    close_file(config_file);
    
    return 0;

    /*=========================================================================
     * ERROR CLEANUP - In reverse order of allocation!
     *=========================================================================*/
err_device:
    free_buffer(cache);
err_cache:
    free_buffer(io_buffer);
err_io:
    close_file(log_file);
err_log:
    close_file(config_file);
err_config:
    printf("\n=== System Initialization FAILED at step! ===\n");
    return ret;
}

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== goto Cleanup Pattern Demo ===\n");
    printf("This shows how to use goto for structured error handling.\n");

    /* Test 1: All steps succeed */
    printf("\n\n========== TEST 1: All steps succeed ==========");
    initialize_system(0);

    /* Test 2: Fail at step 2 (log file) */
    printf("\n\n========== TEST 2: Fail at step 2 ==========");
    initialize_system(2);

    /* Test 3: Fail at step 4 (cache allocation) */
    printf("\n\n========== TEST 3: Fail at step 4 ==========");
    initialize_system(4);

    /* Test 4: Fail at step 5 (device init) */
    printf("\n\n========== TEST 4: Fail at step 5 ==========");
    initialize_system(5);

    printf("\n\nDone!\n");
    return 0;
}

/*
 * KEY OBSERVATIONS:
 * 
 * 1. Resources allocated in order: config -> log -> io -> cache -> device
 * 2. On failure, cleanup happens in REVERSE order
 * 3. Each error label cleans up everything allocated BEFORE that point
 * 4. Single exit point for errors, clear flow
 * 5. Fall-through behavior: err_device -> err_cache -> err_io -> etc.
 */
```

---

## 6. Workqueue (Deferred Work) Pattern

### 6.1 Concept Diagram

```
+============================================================================+
|                      WORKQUEUE PATTERN                                      |
+============================================================================+
|                                                                             |
|   PROBLEM: IRQ handlers must be fast, cannot:                              |
|   - Sleep / block                                                           |
|   - Allocate memory (GFP_KERNEL)                                           |
|   - Take mutexes                                                            |
|                                                                             |
|   SOLUTION: Schedule work to run later in process context                  |
|                                                                             |
|   +------------------+         +------------------------+                   |
|   | Interrupt/Signal |         | Worker Thread          |                   |
|   | Handler          |         | (process context)      |                   |
|   +------------------+         +------------------------+                   |
|   |                  |         |                        |                   |
|   | - Quick handling |         | - Slow processing      |                   |
|   | - Queue work     |-------->| - Can sleep            |                   |
|   |                  |         | - Can use mutex        |                   |
|   +------------------+         +------------------------+                   |
|                                                                             |
|   Timeline:                                                                 |
|   ================================================================          |
|   IRQ -> [quick handler] -> schedule_work() -> ... -> [worker runs]       |
|                ^                                           ^                |
|                |                                           |                |
|          microseconds                               can take as long       |
|                                                     as needed              |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 中断处理必须快速，不能睡眠或做耗时操作
- 将耗时工作推迟到工作队列，由工作线程在进程上下文中执行
- 工作线程可以睡眠、使用互斥锁、分配内存等

### 6.2 Complete Example

```c
/*
 * workqueue_example.c - Deferred Work Pattern
 * 
 * Compile: gcc -o workqueue_example workqueue_example.c -pthread
 * Run:     ./workqueue_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <stddef.h>

/*============================================================================
 * CONTAINER_OF
 *============================================================================*/

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/*============================================================================
 * WORK STRUCTURE
 *============================================================================*/

struct work_struct {
    void (*func)(struct work_struct *work);  /* Work function */
    struct work_struct *next;                 /* For queue linking */
};

/* Initialize work with function */
#define INIT_WORK(work, fn) do { (work)->func = (fn); (work)->next = NULL; } while(0)

/*============================================================================
 * WORKQUEUE IMPLEMENTATION
 *============================================================================*/

struct workqueue {
    char name[32];
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct work_struct *head;
    struct work_struct *tail;
    bool shutdown;
};

/* Worker thread function */
void *worker_thread(void *arg)
{
    struct workqueue *wq = arg;
    
    printf("[%s] Worker thread started\n", wq->name);
    
    while (1) {
        struct work_struct *work = NULL;
        
        pthread_mutex_lock(&wq->lock);
        
        /* Wait for work */
        while (!wq->head && !wq->shutdown) {
            pthread_cond_wait(&wq->cond, &wq->lock);
        }
        
        if (wq->shutdown && !wq->head) {
            pthread_mutex_unlock(&wq->lock);
            break;
        }
        
        /* Dequeue work */
        work = wq->head;
        wq->head = work->next;
        if (!wq->head) wq->tail = NULL;
        
        pthread_mutex_unlock(&wq->lock);
        
        /* Execute work function (in process context!) */
        if (work->func) {
            printf("[%s] Executing work...\n", wq->name);
            work->func(work);
        }
    }
    
    printf("[%s] Worker thread exiting\n", wq->name);
    return NULL;
}

/* Create workqueue */
struct workqueue *create_workqueue(const char *name)
{
    struct workqueue *wq = malloc(sizeof(*wq));
    strncpy(wq->name, name, sizeof(wq->name) - 1);
    pthread_mutex_init(&wq->lock, NULL);
    pthread_cond_init(&wq->cond, NULL);
    wq->head = wq->tail = NULL;
    wq->shutdown = false;
    pthread_create(&wq->thread, NULL, worker_thread, wq);
    return wq;
}

/* Destroy workqueue */
void destroy_workqueue(struct workqueue *wq)
{
    pthread_mutex_lock(&wq->lock);
    wq->shutdown = true;
    pthread_cond_signal(&wq->cond);
    pthread_mutex_unlock(&wq->lock);
    
    pthread_join(wq->thread, NULL);
    
    pthread_mutex_destroy(&wq->lock);
    pthread_cond_destroy(&wq->cond);
    free(wq);
}

/* Schedule work on queue */
bool queue_work(struct workqueue *wq, struct work_struct *work)
{
    pthread_mutex_lock(&wq->lock);
    
    work->next = NULL;
    if (wq->tail) {
        wq->tail->next = work;
    } else {
        wq->head = work;
    }
    wq->tail = work;
    
    pthread_cond_signal(&wq->cond);
    pthread_mutex_unlock(&wq->lock);
    
    printf("[Queue] Work scheduled\n");
    return true;
}

/*============================================================================
 * APPLICATION: DEVICE DRIVER SIMULATION
 *============================================================================*/

struct device_data {
    struct work_struct work;     /* Embedded work structure */
    char payload[128];           /* Data from "interrupt" */
    int sequence;
};

/* Slow processing function - runs in worker thread (can sleep!) */
void process_device_data(struct work_struct *work)
{
    struct device_data *data = container_of(work, struct device_data, work);
    
    printf("[Worker] Processing packet #%d: '%s'\n", data->sequence, data->payload);
    
    /* Simulate slow processing - WE CAN SLEEP HERE! */
    printf("[Worker] Doing slow processing (sleeping 500ms)...\n");
    usleep(500000);
    
    /* Simulate file I/O (would block in real driver) */
    printf("[Worker] Writing to log file...\n");
    usleep(100000);
    
    printf("[Worker] Packet #%d processing complete\n", data->sequence);
    
    /* In real code, might signal completion or free data */
    free(data);
}

/* Simulated IRQ handler - must be fast! */
void irq_handler(struct workqueue *wq, const char *received_data, int seq)
{
    printf("\n[IRQ] Interrupt received! (packet #%d)\n", seq);
    
    /* Quick handling: allocate and copy data */
    struct device_data *data = malloc(sizeof(*data));
    strcpy(data->payload, received_data);
    data->sequence = seq;
    INIT_WORK(&data->work, process_device_data);
    
    /* Defer slow processing to workqueue */
    printf("[IRQ] Scheduling work for later processing...\n");
    queue_work(wq, &data->work);
    
    printf("[IRQ] Handler returning quickly!\n");
}

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== Workqueue (Deferred Work) Pattern Demo ===\n\n");

    /* Create workqueue (like create_singlethread_workqueue in kernel) */
    struct workqueue *wq = create_workqueue("mydevice_wq");

    /* Simulate receiving "interrupts" */
    printf("=== Simulating device interrupts ===\n");
    
    irq_handler(wq, "Hello from network", 1);
    usleep(50000);
    irq_handler(wq, "Another packet", 2);
    usleep(50000);
    irq_handler(wq, "Third message", 3);

    /* Wait for all work to complete */
    printf("\n[Main] Waiting for work to complete...\n");
    sleep(3);

    /* Cleanup */
    printf("\n[Main] Destroying workqueue...\n");
    destroy_workqueue(wq);

    printf("\nDone!\n");
    return 0;
}
```

---

## 7. Per-CPU Variables Pattern

### 7.1 Concept Diagram

```
+============================================================================+
|                      PER-CPU VARIABLES                                      |
+============================================================================+
|                                                                             |
|   PROBLEM: Shared variable accessed by multiple CPUs                       |
|                                                                             |
|   CPU 0        CPU 1        CPU 2        CPU 3                             |
|     |            |            |            |                                |
|     +------------+------------+------------+                                |
|                       |                                                     |
|                       v                                                     |
|              +----------------+                                             |
|              | counter = 100  |  <-- Lock contention!                      |
|              +----------------+      Cache line bouncing!                   |
|                                                                             |
|   SOLUTION: Each CPU has its own copy                                      |
|                                                                             |
|   CPU 0        CPU 1        CPU 2        CPU 3                             |
|     |            |            |            |                                |
|     v            v            v            v                                |
|   +----+      +----+       +----+       +----+                             |
|   | 25 |      | 30 |       | 20 |       | 25 |  <-- No locking needed!    |
|   +----+      +----+       +----+       +----+                             |
|                                                                             |
|   Total = 25 + 30 + 20 + 25 = 100                                          |
|                                                                             |
|   BENEFITS:                                                                 |
|   - No lock contention                                                      |
|   - No cache line bouncing                                                  |
|   - Each CPU works on local cache line                                     |
|                                                                             |
+============================================================================+
```

**中文说明**：
- 共享变量被多 CPU 访问时，锁竞争和缓存行颠簸严重影响性能
- 每 CPU 变量让每个 CPU 有自己的副本，完全消除竞争
- 需要全局值时，遍历所有 CPU 的副本求和

### 7.2 Complete Example

```c
/*
 * percpu_example.c - Per-CPU Variables Pattern
 * 
 * Compile: gcc -o percpu_example percpu_example.c -pthread
 * Run:     ./percpu_example
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

#define NUM_CPUS 4           /* Simulated number of CPUs */
#define NUM_THREADS 4        /* One thread per "CPU" */
#define ITERATIONS 1000000   /* Operations per thread */

/*============================================================================
 * PER-CPU VARIABLE IMPLEMENTATION
 *============================================================================*/

/* Per-CPU variable macro - creates array with one slot per CPU */
#define DEFINE_PER_CPU(type, name) \
    type __percpu_##name[NUM_CPUS]

/* Access per-CPU variable for given CPU */
#define per_cpu(name, cpu) \
    (__percpu_##name[cpu])

/* Access per-CPU variable for current CPU */
#define this_cpu(name) \
    per_cpu(name, get_cpu_id())

/* Get current CPU (in user-space, we simulate with thread ID) */
__thread int __current_cpu = -1;

int get_cpu_id(void)
{
    return __current_cpu;
}

void set_cpu_id(int cpu)
{
    __current_cpu = cpu;
}

/*============================================================================
 * STATISTICS STRUCTURE
 *============================================================================*/

struct stats {
    unsigned long packets;
    unsigned long bytes;
    unsigned long errors;
};

/* Define per-CPU stats */
DEFINE_PER_CPU(struct stats, cpu_stats);

/* Global counter for comparison */
struct stats global_stats;
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

/*============================================================================
 * FUNCTIONS
 *============================================================================*/

/* Update per-CPU stats (NO LOCKING NEEDED!) */
void update_percpu_stats(int packets, int bytes, int errors)
{
    int cpu = get_cpu_id();
    per_cpu(cpu_stats, cpu).packets += packets;
    per_cpu(cpu_stats, cpu).bytes += bytes;
    per_cpu(cpu_stats, cpu).errors += errors;
}

/* Update global stats (NEEDS LOCKING!) */
void update_global_stats(int packets, int bytes, int errors)
{
    pthread_mutex_lock(&global_lock);
    global_stats.packets += packets;
    global_stats.bytes += bytes;
    global_stats.errors += errors;
    pthread_mutex_unlock(&global_lock);
}

/* Get total stats (sum all per-CPU) */
struct stats get_total_percpu_stats(void)
{
    struct stats total = {0};
    for (int cpu = 0; cpu < NUM_CPUS; cpu++) {
        total.packets += per_cpu(cpu_stats, cpu).packets;
        total.bytes += per_cpu(cpu_stats, cpu).bytes;
        total.errors += per_cpu(cpu_stats, cpu).errors;
    }
    return total;
}

/*============================================================================
 * TEST THREADS
 *============================================================================*/

void *percpu_thread(void *arg)
{
    int cpu = *(int *)arg;
    set_cpu_id(cpu);
    
    /* Simulate processing packets on this CPU */
    for (int i = 0; i < ITERATIONS; i++) {
        update_percpu_stats(1, 64, 0);  /* No locking! */
    }
    
    return NULL;
}

void *global_thread(void *arg)
{
    (void)arg;
    
    /* Simulate processing packets with global counter */
    for (int i = 0; i < ITERATIONS; i++) {
        update_global_stats(1, 64, 0);  /* Needs locking! */
    }
    
    return NULL;
}

/*============================================================================
 * BENCHMARKING
 *============================================================================*/

double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== Per-CPU Variables Pattern Demo ===\n\n");
    printf("Testing with %d threads, %d iterations each\n\n", 
           NUM_THREADS, ITERATIONS);

    pthread_t threads[NUM_THREADS];
    int cpu_ids[NUM_THREADS];
    double start, end;

    /*=========================================================================
     * TEST 1: Per-CPU counters (no locking)
     *=========================================================================*/
    printf("=== Test 1: Per-CPU counters (no locking) ===\n");
    
    /* Initialize per-CPU stats */
    for (int i = 0; i < NUM_CPUS; i++) {
        per_cpu(cpu_stats, i).packets = 0;
        per_cpu(cpu_stats, i).bytes = 0;
        per_cpu(cpu_stats, i).errors = 0;
    }
    
    start = get_time_ms();
    
    /* Start threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        cpu_ids[i] = i;
        pthread_create(&threads[i], NULL, percpu_thread, &cpu_ids[i]);
    }
    
    /* Wait for completion */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    end = get_time_ms();
    
    struct stats total = get_total_percpu_stats();
    printf("  Time: %.2f ms\n", end - start);
    printf("  Total packets: %lu\n", total.packets);
    printf("  Total bytes: %lu\n", total.bytes);
    
    /* Show per-CPU breakdown */
    printf("  Per-CPU breakdown:\n");
    for (int i = 0; i < NUM_CPUS; i++) {
        printf("    CPU %d: %lu packets\n", i, per_cpu(cpu_stats, i).packets);
    }
    printf("\n");

    /*=========================================================================
     * TEST 2: Global counter (with locking)
     *=========================================================================*/
    printf("=== Test 2: Global counter (with locking) ===\n");
    
    global_stats.packets = 0;
    global_stats.bytes = 0;
    global_stats.errors = 0;
    
    start = get_time_ms();
    
    /* Start threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, global_thread, NULL);
    }
    
    /* Wait for completion */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    end = get_time_ms();
    
    printf("  Time: %.2f ms\n", end - start);
    printf("  Total packets: %lu\n", global_stats.packets);
    printf("  Total bytes: %lu\n", global_stats.bytes);
    printf("\n");

    /*=========================================================================
     * COMPARISON
     *=========================================================================*/
    printf("=== Conclusion ===\n");
    printf("Per-CPU variables avoid lock contention and are much faster\n");
    printf("for frequently updated per-CPU data like statistics.\n");

    return 0;
}
```

---

## 8. Completion Pattern

### 8.1 Complete Example

```c
/*
 * completion_example.c - One-shot Synchronization
 * 
 * Compile: gcc -o completion_example completion_example.c -pthread
 * Run:     ./completion_example
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

/*============================================================================
 * COMPLETION IMPLEMENTATION
 *============================================================================*/

struct completion {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool done;
};

#define DECLARE_COMPLETION(name) \
    struct completion name = { \
        .lock = PTHREAD_MUTEX_INITIALIZER, \
        .cond = PTHREAD_COND_INITIALIZER, \
        .done = false \
    }

static inline void init_completion(struct completion *c)
{
    pthread_mutex_init(&c->lock, NULL);
    pthread_cond_init(&c->cond, NULL);
    c->done = false;
}

static inline void reinit_completion(struct completion *c)
{
    c->done = false;
}

/* Wait for completion */
static inline void wait_for_completion(struct completion *c)
{
    pthread_mutex_lock(&c->lock);
    while (!c->done) {
        pthread_cond_wait(&c->cond, &c->lock);
    }
    pthread_mutex_unlock(&c->lock);
}

/* Signal completion */
static inline void complete(struct completion *c)
{
    pthread_mutex_lock(&c->lock);
    c->done = true;
    pthread_cond_signal(&c->cond);
    pthread_mutex_unlock(&c->lock);
}

/* Signal completion to all waiters */
static inline void complete_all(struct completion *c)
{
    pthread_mutex_lock(&c->lock);
    c->done = true;
    pthread_cond_broadcast(&c->cond);
    pthread_mutex_unlock(&c->lock);
}

/*============================================================================
 * APPLICATION: FIRMWARE LOADING
 *============================================================================*/

struct device {
    char name[32];
    struct completion fw_loaded;
    void *firmware;
    size_t fw_size;
};

/* Simulated async firmware load callback */
void *firmware_loader_thread(void *arg)
{
    struct device *dev = arg;
    
    printf("[Loader] Starting firmware download for '%s'...\n", dev->name);
    
    /* Simulate download time */
    sleep(2);
    
    /* Simulate firmware data */
    dev->fw_size = 1024;
    dev->firmware = malloc(dev->fw_size);
    memset(dev->firmware, 0xAB, dev->fw_size);
    
    printf("[Loader] Firmware download complete!\n");
    
    /* Signal completion */
    complete(&dev->fw_loaded);
    
    return NULL;
}

/* Device initialization that waits for firmware */
int init_device(struct device *dev, const char *name)
{
    strcpy(dev->name, name);
    init_completion(&dev->fw_loaded);
    dev->firmware = NULL;
    dev->fw_size = 0;
    
    printf("[Device] Starting async firmware load...\n");
    
    /* Start async firmware load */
    pthread_t loader;
    pthread_create(&loader, NULL, firmware_loader_thread, dev);
    pthread_detach(loader);
    
    printf("[Device] Waiting for firmware to load...\n");
    
    /* Block until firmware is loaded */
    wait_for_completion(&dev->fw_loaded);
    
    printf("[Device] Firmware loaded! Size=%zu bytes\n", dev->fw_size);
    printf("[Device] Device '%s' initialized successfully\n", dev->name);
    
    return 0;
}

/*============================================================================
 * MAIN DEMONSTRATION
 *============================================================================*/

int main(void)
{
    printf("=== Completion Pattern Demo ===\n\n");

    struct device my_device;
    
    printf("Initializing device (will wait for async firmware load)...\n\n");
    
    init_device(&my_device, "eth0");
    
    printf("\nDevice is ready to use!\n");
    
    /* Cleanup */
    free(my_device.firmware);

    return 0;
}
```

---

## Compilation and Running

```bash
# Compile all examples
gcc -o container_of_example container_of_example.c
gcc -o refcount_example refcount_example.c -pthread
gcc -o list_example list_example.c
gcc -o waitqueue_example waitqueue_example.c -pthread
gcc -o goto_cleanup_example goto_cleanup_example.c
gcc -o workqueue_example workqueue_example.c -pthread
gcc -o percpu_example percpu_example.c -pthread
gcc -o completion_example completion_example.c -pthread

# Run examples
./container_of_example
./refcount_example
./list_example
./waitqueue_example
./goto_cleanup_example
./workqueue_example
./percpu_example
./completion_example
```

---

## Summary

| Pattern | Key Insight | User-Space Equivalent |
|---------|-------------|----------------------|
| container_of | 从成员指针反推容器地址 | offsetof + 指针算术 |
| kref | 引用计数管理生命周期 | atomic_int + 回调 |
| list_head | 嵌入式链表，一物多链 | 双向链表 + container_of |
| Wait Queue | 条件等待，避免忙等 | pthread_cond_wait |
| goto cleanup | 结构化错误处理 | 逆序释放资源 |
| Workqueue | 延迟执行到进程上下文 | 线程池 |
| Per-CPU | 每 CPU 副本避免竞争 | 线程局部存储 |
| Completion | 一次性同步 | 条件变量 + 标志 |
