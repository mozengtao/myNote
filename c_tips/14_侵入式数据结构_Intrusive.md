# 侵入式数据结构 (Intrusive Data Structures)

## 定义

侵入式数据结构是一种将容器节点（如链表节点）嵌入到数据结构内部的技术，而非让容器持有指向数据的指针。Linux内核广泛使用这种技术，通过著名的`container_of`宏从嵌入的节点反推出包含它的结构体指针。

## 适用场景

- 操作系统内核开发
- 高性能数据结构实现
- 内存受限的嵌入式系统
- 需要最小化内存分配次数的场景
- 一个对象需要同时存在于多个容器中
- 游戏引擎中的实体组件系统
- 需要精确控制内存布局的场景

## ASCII 图解

```
+------------------------------------------------------------------------+
|                    INTRUSIVE DATA STRUCTURES                            |
+------------------------------------------------------------------------+
|                                                                         |
|   NON-INTRUSIVE (Traditional):                                          |
|                                                                         |
|   +----------+     +----------+     +----------+                        |
|   | ListNode |     | ListNode |     | ListNode |                        |
|   +----------+     +----------+     +----------+                        |
|   | *data  --|--+  | *data  --|--+  | *data  --|--+                     |
|   | *next    |  |  | *next    |  |  | *next    |  |                     |
|   | *prev    |  |  | *prev    |  |  | *prev    |  |                     |
|   +----------+  |  +----------+  |  +----------+  |                     |
|                 |               |                 |                     |
|                 v               v                 v                     |
|             +------+        +------+         +------+                   |
|             | Data |        | Data |         | Data |                   |
|             +------+        +------+         +------+                   |
|                                                                         |
|   Problem: 2 allocations per item, extra pointer indirection            |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   INTRUSIVE (Embedded):                                                 |
|                                                                         |
|   +------------------+   +------------------+   +------------------+    |
|   |     MyData       |   |     MyData       |   |     MyData       |    |
|   +------------------+   +------------------+   +------------------+    |
|   | int value        |   | int value        |   | int value        |    |
|   | char name[32]    |   | char name[32]    |   | char name[32]    |    |
|   | +------------+   |   | +------------+   |   | +------------+   |    |
|   | | ListNode   |<----->| | ListNode   |<----->| | ListNode   |   |    |
|   | +------------+   |   | +------------+   |   | +------------+   |    |
|   | int flags        |   | int flags        |   | int flags        |    |
|   +------------------+   +------------------+   +------------------+    |
|                                                                         |
|   Advantage: 1 allocation, better cache locality, no indirection        |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   container_of() MACRO MAGIC:                                           |
|                                                                         |
|   +------------------+                                                  |
|   |     MyData       |  <-- We want this (struct pointer)               |
|   +------------------+                                                  |
|   | value (offset 0) |                                                  |
|   | name  (offset 4) |                                                  |
|   +~~~~~~~~~~~~~~~~~~+                                                  |
|   | node  (offset 36)|  <-- We have this (member pointer)               |
|   +~~~~~~~~~~~~~~~~~~+                                                  |
|   | flags (offset 52)|                                                  |
|   +------------------+                                                  |
|                                                                         |
|   container_of(node_ptr, MyData, node):                                 |
|   struct_ptr = (MyData*)((char*)node_ptr - offsetof(MyData, node))      |
|              = (MyData*)((char*)node_ptr - 36)                          |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   ONE OBJECT IN MULTIPLE LISTS:                                         |
|                                                                         |
|   +------------------------+                                            |
|   |       Task             |                                            |
|   +------------------------+                                            |
|   | int priority           |                                            |
|   | +------------------+   |      All Tasks List                        |
|   | | all_tasks_node   |<=======> [Task]<=>[Task]<=>[Task]              |
|   | +------------------+   |                                            |
|   | +------------------+   |      Ready Queue                           |
|   | | ready_queue_node |<=======> [Task]<=>[Task]                       |
|   | +------------------+   |                                            |
|   | +------------------+   |      Priority Queue                        |
|   | | priority_node    |<=======> [Task]<=>[Task]<=>[Task]<=>[Task]     |
|   | +------------------+   |                                            |
|   +------------------------+                                            |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图对比了传统链表和侵入式链表的区别。传统方式需要两次内存分配（节点和数据各一次），且需要通过指针间接访问数据。侵入式链表将节点嵌入数据结构中，只需一次分配，且有更好的缓存局部性。`container_of`宏通过计算成员偏移量，从嵌入的节点指针反推出包含它的结构体指针。底部展示了侵入式设计的独特优势：同一个对象可以同时存在于多个不同的链表中。

## 实现方法

1. 定义通用的链表节点结构（只包含指针）
2. 实现`container_of`宏计算偏移
3. 将节点嵌入到数据结构中
4. 使用通用链表操作函数处理节点
5. 通过`container_of`获取实际数据

## C语言代码示例

### 基础设施

```c
// intrusive_list.h
#ifndef INTRUSIVE_LIST_H
#define INTRUSIVE_LIST_H

#include <stddef.h>

// ==================== container_of 宏 ====================

// 从成员指针获取包含它的结构体指针
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// ==================== 侵入式链表节点 ====================

typedef struct ListNode {
    struct ListNode* next;
    struct ListNode* prev;
} ListNode;

// ==================== 链表初始化 ====================

// 静态初始化
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) ListNode name = LIST_HEAD_INIT(name)

// 动态初始化
static inline void list_init(ListNode* head) {
    head->next = head;
    head->prev = head;
}

// ==================== 基本操作 ====================

// 在两个节点之间插入
static inline void __list_add(ListNode* new_node, 
                              ListNode* prev, 
                              ListNode* next) {
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

// 在头部插入
static inline void list_add(ListNode* new_node, ListNode* head) {
    __list_add(new_node, head, head->next);
}

// 在尾部插入
static inline void list_add_tail(ListNode* new_node, ListNode* head) {
    __list_add(new_node, head->prev, head);
}

// 删除节点
static inline void list_del(ListNode* entry) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->next = NULL;
    entry->prev = NULL;
}

// 检查是否为空
static inline int list_empty(const ListNode* head) {
    return head->next == head;
}

// 检查是否是唯一节点
static inline int list_is_singular(const ListNode* head) {
    return !list_empty(head) && (head->next == head->prev);
}

// ==================== 遍历宏 ====================

// 遍历链表
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

// 安全遍历（可在遍历中删除）
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; \
         pos != (head); \
         pos = n, n = pos->next)

// 获取包含节点的结构体
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

// 遍历并获取结构体
#define list_for_each_entry(pos, head, type, member) \
    for (pos = list_entry((head)->next, type, member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, type, member))

// 安全遍历并获取结构体
#define list_for_each_entry_safe(pos, n, head, type, member) \
    for (pos = list_entry((head)->next, type, member), \
         n = list_entry(pos->member.next, type, member); \
         &pos->member != (head); \
         pos = n, n = list_entry(n->member.next, type, member))

// 获取第一个/最后一个元素
#define list_first_entry(head, type, member) \
    list_entry((head)->next, type, member)

#define list_last_entry(head, type, member) \
    list_entry((head)->prev, type, member)

#endif // INTRUSIVE_LIST_H
```

### 实际应用：任务调度器

```c
// task_scheduler.h
#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "intrusive_list.h"
#include <stdint.h>

typedef enum {
    TASK_CREATED,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_TERMINATED
} TaskState;

typedef void (*TaskFunction)(void* arg);

// 任务结构 - 包含多个链表节点用于不同队列
typedef struct Task {
    uint32_t id;
    char name[32];
    int priority;
    TaskState state;
    TaskFunction func;
    void* arg;
    
    // 侵入式链表节点 - 可同时在多个队列中
    ListNode all_tasks_node;     // 所有任务链表
    ListNode ready_queue_node;   // 就绪队列
    ListNode priority_node;      // 优先级队列
} Task;

// 调度器
typedef struct {
    ListNode all_tasks;      // 所有任务
    ListNode ready_queue;    // 就绪队列
    ListNode priority_queues[10]; // 优先级队列 (0-9)
    uint32_t next_id;
    int task_count;
} Scheduler;

// API
void scheduler_init(Scheduler* sched);
Task* scheduler_create_task(Scheduler* sched, const char* name, 
                           int priority, TaskFunction func, void* arg);
void scheduler_destroy_task(Scheduler* sched, Task* task);
void scheduler_set_ready(Scheduler* sched, Task* task);
void scheduler_set_blocked(Scheduler* sched, Task* task);
Task* scheduler_pick_next(Scheduler* sched);
void scheduler_run_one(Scheduler* sched);
void scheduler_print_all(Scheduler* sched);
void scheduler_print_ready(Scheduler* sched);
void scheduler_cleanup(Scheduler* sched);

#endif
```

```c
// task_scheduler.c
#include "task_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void scheduler_init(Scheduler* sched) {
    if (!sched) return;
    
    list_init(&sched->all_tasks);
    list_init(&sched->ready_queue);
    
    for (int i = 0; i < 10; i++) {
        list_init(&sched->priority_queues[i]);
    }
    
    sched->next_id = 1;
    sched->task_count = 0;
    
    printf("[Scheduler] Initialized\n");
}

Task* scheduler_create_task(Scheduler* sched, const char* name,
                           int priority, TaskFunction func, void* arg) {
    if (!sched || priority < 0 || priority > 9) return NULL;
    
    Task* task = (Task*)calloc(1, sizeof(Task));
    if (!task) return NULL;
    
    task->id = sched->next_id++;
    strncpy(task->name, name, sizeof(task->name) - 1);
    task->priority = priority;
    task->state = TASK_CREATED;
    task->func = func;
    task->arg = arg;
    
    // 初始化链表节点
    list_init(&task->all_tasks_node);
    list_init(&task->ready_queue_node);
    list_init(&task->priority_node);
    
    // 加入所有任务链表
    list_add_tail(&task->all_tasks_node, &sched->all_tasks);
    sched->task_count++;
    
    printf("[Scheduler] Created task '%s' (id=%u, priority=%d)\n",
           task->name, task->id, task->priority);
    
    return task;
}

void scheduler_destroy_task(Scheduler* sched, Task* task) {
    if (!sched || !task) return;
    
    printf("[Scheduler] Destroying task '%s'\n", task->name);
    
    // 从所有链表中移除
    if (task->all_tasks_node.next) {
        list_del(&task->all_tasks_node);
    }
    if (task->ready_queue_node.next) {
        list_del(&task->ready_queue_node);
    }
    if (task->priority_node.next) {
        list_del(&task->priority_node);
    }
    
    sched->task_count--;
    free(task);
}

void scheduler_set_ready(Scheduler* sched, Task* task) {
    if (!sched || !task) return;
    
    // 如果已经在就绪队列，先移除
    if (task->ready_queue_node.next && task->ready_queue_node.prev) {
        list_del(&task->ready_queue_node);
        list_init(&task->ready_queue_node);
    }
    if (task->priority_node.next && task->priority_node.prev) {
        list_del(&task->priority_node);
        list_init(&task->priority_node);
    }
    
    task->state = TASK_READY;
    
    // 加入就绪队列和优先级队列
    list_add_tail(&task->ready_queue_node, &sched->ready_queue);
    list_add_tail(&task->priority_node, &sched->priority_queues[task->priority]);
    
    printf("[Scheduler] Task '%s' is now READY\n", task->name);
}

void scheduler_set_blocked(Scheduler* sched, Task* task) {
    if (!sched || !task) return;
    
    // 从就绪队列移除
    if (task->ready_queue_node.next && task->ready_queue_node.prev) {
        list_del(&task->ready_queue_node);
        list_init(&task->ready_queue_node);
    }
    if (task->priority_node.next && task->priority_node.prev) {
        list_del(&task->priority_node);
        list_init(&task->priority_node);
    }
    
    task->state = TASK_BLOCKED;
    printf("[Scheduler] Task '%s' is now BLOCKED\n", task->name);
}

Task* scheduler_pick_next(Scheduler* sched) {
    if (!sched) return NULL;
    
    // 从最高优先级开始查找
    for (int p = 9; p >= 0; p--) {
        if (!list_empty(&sched->priority_queues[p])) {
            Task* task = list_first_entry(&sched->priority_queues[p], 
                                          Task, priority_node);
            return task;
        }
    }
    
    return NULL;
}

void scheduler_run_one(Scheduler* sched) {
    Task* task = scheduler_pick_next(sched);
    if (!task) {
        printf("[Scheduler] No ready tasks\n");
        return;
    }
    
    printf("[Scheduler] Running task '%s' (priority=%d)\n", 
           task->name, task->priority);
    
    // 从就绪队列移除
    list_del(&task->ready_queue_node);
    list_init(&task->ready_queue_node);
    list_del(&task->priority_node);
    list_init(&task->priority_node);
    
    task->state = TASK_RUNNING;
    
    // 执行任务
    if (task->func) {
        task->func(task->arg);
    }
    
    task->state = TASK_TERMINATED;
    printf("[Scheduler] Task '%s' completed\n", task->name);
}

void scheduler_print_all(Scheduler* sched) {
    if (!sched) return;
    
    printf("\n+========== All Tasks (%d) ==========+\n", sched->task_count);
    printf("| %-4s | %-15s | %-8s | %-8s |\n", "ID", "Name", "Priority", "State");
    printf("+------+-----------------+----------+----------+\n");
    
    const char* state_names[] = {
        "CREATED", "READY", "RUNNING", "BLOCKED", "DONE"
    };
    
    ListNode* pos;
    list_for_each(pos, &sched->all_tasks) {
        Task* task = list_entry(pos, Task, all_tasks_node);
        printf("| %-4u | %-15s | %-8d | %-8s |\n",
               task->id, task->name, task->priority, state_names[task->state]);
    }
    printf("+======================================+\n\n");
}

void scheduler_print_ready(Scheduler* sched) {
    if (!sched) return;
    
    printf("\n+==== Ready Queue ====+\n");
    for (int p = 9; p >= 0; p--) {
        if (!list_empty(&sched->priority_queues[p])) {
            printf("Priority %d: ", p);
            ListNode* pos;
            list_for_each(pos, &sched->priority_queues[p]) {
                Task* task = list_entry(pos, Task, priority_node);
                printf("[%s] ", task->name);
            }
            printf("\n");
        }
    }
    printf("+=====================+\n\n");
}

void scheduler_cleanup(Scheduler* sched) {
    if (!sched) return;
    
    ListNode *pos, *n;
    list_for_each_safe(pos, n, &sched->all_tasks) {
        Task* task = list_entry(pos, Task, all_tasks_node);
        scheduler_destroy_task(sched, task);
    }
    
    printf("[Scheduler] Cleaned up\n");
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "task_scheduler.h"

// 示例任务函数
void task_print(void* arg) {
    const char* msg = (const char*)arg;
    printf("  >> Task executing: %s\n", msg);
}

void task_compute(void* arg) {
    int* val = (int*)arg;
    printf("  >> Computing: %d * 2 = %d\n", *val, *val * 2);
}

int main() {
    printf("=== Intrusive Data Structure Demo ===\n\n");
    
    // 初始化调度器
    Scheduler sched;
    scheduler_init(&sched);
    
    // 创建各种优先级的任务
    static int nums[] = {10, 20, 30};
    
    Task* t1 = scheduler_create_task(&sched, "Init", 5, task_print, "Initializing...");
    Task* t2 = scheduler_create_task(&sched, "Compute1", 8, task_compute, &nums[0]);
    Task* t3 = scheduler_create_task(&sched, "Compute2", 8, task_compute, &nums[1]);
    Task* t4 = scheduler_create_task(&sched, "Cleanup", 3, task_print, "Cleaning up...");
    Task* t5 = scheduler_create_task(&sched, "HighPri", 9, task_print, "URGENT!");
    Task* t6 = scheduler_create_task(&sched, "LowPri", 1, task_print, "Background work");
    
    // 打印所有任务
    scheduler_print_all(&sched);
    
    // 设置任务为就绪状态
    printf("--- Setting tasks ready ---\n");
    scheduler_set_ready(&sched, t1);
    scheduler_set_ready(&sched, t2);
    scheduler_set_ready(&sched, t3);
    scheduler_set_ready(&sched, t4);
    scheduler_set_ready(&sched, t5);
    scheduler_set_ready(&sched, t6);
    
    // 打印就绪队列
    scheduler_print_ready(&sched);
    
    // 阻塞一个任务
    printf("--- Blocking Compute2 ---\n");
    scheduler_set_blocked(&sched, t3);
    scheduler_print_ready(&sched);
    
    // 运行任务（按优先级）
    printf("--- Running tasks by priority ---\n");
    for (int i = 0; i < 5; i++) {
        scheduler_run_one(&sched);
    }
    
    // 打印最终状态
    scheduler_print_all(&sched);
    
    // 清理
    scheduler_cleanup(&sched);
    
    return 0;
}

/* 输出示例:
=== Intrusive Data Structure Demo ===

[Scheduler] Initialized
[Scheduler] Created task 'Init' (id=1, priority=5)
[Scheduler] Created task 'Compute1' (id=2, priority=8)
[Scheduler] Created task 'Compute2' (id=3, priority=8)
[Scheduler] Created task 'Cleanup' (id=4, priority=3)
[Scheduler] Created task 'HighPri' (id=5, priority=9)
[Scheduler] Created task 'LowPri' (id=6, priority=1)

+========== All Tasks (6) ==========+
| ID   | Name            | Priority | State    |
+------+-----------------+----------+----------+
| 1    | Init            | 5        | CREATED  |
| 2    | Compute1        | 8        | CREATED  |
| 3    | Compute2        | 8        | CREATED  |
| 4    | Cleanup         | 3        | CREATED  |
| 5    | HighPri         | 9        | CREATED  |
| 6    | LowPri          | 1        | CREATED  |
+======================================+

--- Setting tasks ready ---
...

+==== Ready Queue ====+
Priority 9: [HighPri] 
Priority 8: [Compute1] [Compute2] 
Priority 5: [Init] 
Priority 3: [Cleanup] 
Priority 1: [LowPri] 
+=====================+

--- Running tasks by priority ---
[Scheduler] Running task 'HighPri' (priority=9)
  >> Task executing: URGENT!
[Scheduler] Task 'HighPri' completed
...
*/
```

## 优缺点

### 优点
- **单次内存分配**：对象和节点一起分配
- **更好的缓存局部性**：数据紧凑连续
- **无指针间接访问**：直接访问数据
- **支持多重从属**：同一对象可在多个容器中
- **零额外内存开销**：节点嵌入在对象中

### 缺点
- 代码复杂度增加
- 需要手动管理节点生命周期
- 类型安全性较弱（依赖宏）
- 调试困难（指针运算）
- 需要确保节点正确初始化和清理

