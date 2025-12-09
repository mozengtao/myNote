# Command Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                      COMMAND PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    +----------+       +----------+       +----------+             |
|    |  Client  |------>| Invoker  |       | Receiver |             |
|    +----------+       +----+-----+       +----+-----+             |
|                            |                  ^                   |
|                            | stores           | executes on       |
|                            v                  |                   |
|                       +----+-----+            |                   |
|                       | Command  |------------+                   |
|                       +----------+                                |
|                       |+execute()|                                |
|                       +----+-----+                                |
|                            ^                                      |
|                   +--------+--------+                             |
|                   |        |        |                             |
|              +----+---+ +--+----+ +-+------+                      |
|              |CmdA    | |CmdB   | |CmdC    |                      |
|              |execute | |execute| |execute |                      |
|              +--------+ +-------+ +--------+                      |
|                                                                   |
|    Commands encapsulate requests as objects                       |
|    Can be queued, logged, undone                                  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 命令模式将请求封装为对象，解耦请求者与执行者，支持请求的存储、队列化和撤销。在Linux内核中，工作队列(workqueue)是命令模式的典型应用。工作项(work_struct)封装了要执行的操作，可以被排队、延迟执行，并由工作线程异步处理。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Work Queue

```c
/* From: include/linux/workqueue.h */

/**
 * struct work_struct - Command object
 *
 * Encapsulates a deferred function call.
 * The 'func' field is the command to execute.
 */
struct work_struct {
    atomic_long_t data;
    struct list_head entry;
    
    /* COMMAND: The function to execute */
    work_func_t func;
    
#ifdef CONFIG_LOCKDEP
    struct lockdep_map lockdep_map;
#endif
};

/* Command function type */
typedef void (*work_func_t)(struct work_struct *work);

/**
 * struct delayed_work - Delayed command
 *
 * Command that executes after a delay.
 */
struct delayed_work {
    struct work_struct work;
    struct timer_list timer;
    int cpu;
};

/* Initialize a work item (command) */
#define INIT_WORK(_work, _func)                 \
    do {                                        \
        (_work)->data = (atomic_long_t)         \
            WORK_DATA_INIT();                   \
        INIT_LIST_HEAD(&(_work)->entry);        \
        (_work)->func = (_func);                \
    } while (0)
```

### 2.2 Kernel Example: Work Queue Operations

```c
/* From: kernel/workqueue.c */

/**
 * queue_work - Queue a command for execution
 * @wq: Workqueue to use
 * @work: Work item (command) to queue
 *
 * Adds the command to the queue for later execution.
 */
int queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
    int ret;

    ret = queue_work_on(get_cpu(), wq, work);
    put_cpu();

    return ret;
}

/**
 * queue_work_on - Queue work on specific CPU
 * @cpu: CPU to execute work on
 * @wq: Workqueue
 * @work: Work item (command)
 *
 * The command is stored and will be executed by a worker thread.
 */
int queue_work_on(int cpu, struct workqueue_struct *wq, 
                  struct work_struct *work)
{
    int ret = 0;

    /* Check if work is already pending */
    if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
        __queue_work(cpu, wq, work);
        ret = 1;
    }
    return ret;
}

/**
 * queue_delayed_work - Queue command with delay
 * @wq: Workqueue
 * @dwork: Delayed work
 * @delay: Delay in jiffies
 *
 * Schedules command execution after specified delay.
 */
int queue_delayed_work(struct workqueue_struct *wq,
                       struct delayed_work *dwork, unsigned long delay)
{
    if (delay == 0)
        return queue_work(wq, &dwork->work);

    return queue_delayed_work_on(-1, wq, dwork, delay);
}

/**
 * flush_workqueue - Wait for all commands to complete
 * @wq: Workqueue
 *
 * Blocks until all queued commands have been executed.
 */
void flush_workqueue(struct workqueue_struct *wq)
{
    /* Wait for completion of all work items */
    /* ... implementation ... */
}
```

### 2.3 Kernel Example: Block I/O Request Queue

```c
/* From: include/linux/blkdev.h */

/**
 * struct request - Block I/O command
 *
 * Encapsulates a block I/O operation (read/write).
 */
struct request {
    struct list_head queuelist;
    
    /* Command parameters */
    sector_t __sector;          /* Start sector */
    unsigned int __data_len;    /* Data length */
    
    /* Command type (read/write/etc) */
    unsigned int cmd_flags;
    
    /* Completion callback */
    rq_end_io_fn *end_io;
    void *end_io_data;
    
    /* Bio chain (actual data) */
    struct bio *bio;
    struct bio *biotail;
    
    /* ... */
};

/**
 * blk_queue_bio - Queue a block I/O command
 * @q: Request queue
 * @bio: Bio to queue
 *
 * Creates a request (command) and adds it to the queue.
 */
void blk_queue_bio(struct request_queue *q, struct bio *bio)
{
    struct request *req;
    
    /* Create request (command) from bio */
    req = get_request(q, bio_data_dir(bio), bio, GFP_NOIO);
    
    /* Initialize request */
    init_request_from_bio(req, bio);
    
    /* Add to queue */
    add_request(q, req);
    
    /* Notify device to process commands */
    __blk_run_queue(q);
}
```

### 2.4 Kernel Example: Command Queue in Driver

```c
/* From: drivers/misc/ibmasm/command.c */

/**
 * struct command - Command object for service processor
 */
struct command {
    struct list_head queue_node;    /* Queue linkage */
    void *buffer;                   /* Command data */
    size_t buffer_size;
    int status;                     /* Command result */
    struct completion wait;         /* Completion synchronization */
    atomic_t ref_count;
};

/**
 * enqueue_command - Add command to queue
 * @sp: Service processor
 * @cmd: Command to queue
 */
static void enqueue_command(struct service_processor *sp, struct command *cmd)
{
    list_add_tail(&cmd->queue_node, &sp->command_queue);
}

/**
 * dequeue_command - Remove command from queue
 * @sp: Service processor
 *
 * Returns next command to execute.
 */
static struct command *dequeue_command(struct service_processor *sp)
{
    struct command *cmd;
    struct list_head *next;

    if (list_empty(&sp->command_queue))
        return NULL;

    next = sp->command_queue.next;
    list_del_init(next);
    cmd = list_entry(next, struct command, queue_node);

    return cmd;
}

/**
 * ibmasm_exec_command - Execute a command
 * @sp: Service processor
 * @cmd: Command to execute
 *
 * If no command is running, execute immediately.
 * Otherwise, queue for later execution.
 */
void ibmasm_exec_command(struct service_processor *sp, struct command *cmd)
{
    unsigned long flags;

    spin_lock_irqsave(&sp->lock, flags);

    if (!sp->current_command) {
        /* Execute immediately */
        sp->current_command = cmd;
        command_get(sp->current_command);
        spin_unlock_irqrestore(&sp->lock, flags);
        do_exec_command(sp);
    } else {
        /* Queue for later */
        enqueue_command(sp, cmd);
        spin_unlock_irqrestore(&sp->lock, flags);
    }
}
```

### 2.5 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL COMMAND PATTERN                        |
|                     (Work Queue)                                  |
+------------------------------------------------------------------+
|                                                                   |
|    Client (Driver/Subsystem)                                      |
|    +---------------------------+                                  |
|    | schedule_work(&my_work);  |                                  |
|    | queue_delayed_work(wq,    |                                  |
|    |   &delayed_work, HZ);     |                                  |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  | (submits command)                              |
|                  v                                                |
|    +-------------+-------------+                                  |
|    |    Work Queue (Invoker)   |                                  |
|    +---------------------------+                                  |
|    | Command Queue:            |                                  |
|    |  [work1] -> [work2] -> [work3]                               |
|    +-------------+-------------+                                  |
|                  |                                                |
|                  | (dequeues and executes)                        |
|                  v                                                |
|    +-------------+-------------+                                  |
|    |   Worker Thread           |                                  |
|    +---------------------------+                                  |
|    | while(1) {                |                                  |
|    |   work = dequeue();       |                                  |
|    |   work->func(work);       |  <-- Execute command             |
|    | }                         |                                  |
|    +---------------------------+                                  |
|                                                                   |
|    work_struct (Command)        Receiver                          |
|    +------------------+        +------------------+               |
|    | func = my_work_fn|------->| Actual operation |               |
|    | data = context   |        | (cleanup, I/O,   |               |
|    +------------------+        |  notification)   |               |
|                                +------------------+               |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux内核的工作队列是命令模式的典型实现。驱动或子系统通过schedule_work()提交工作项（命令），工作项被放入队列。工作线程从队列中取出工作项并执行其func回调函数。这种设计允许延迟执行、异步处理，并将请求提交与实际执行解耦。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Decoupling** | Separates requester from executor |
| **Queuing** | Commands can be stored in a queue |
| **Deferred Execution** | Execute commands later or asynchronously |
| **Undo/Redo** | Commands can be reversed if they store state |
| **Logging** | Commands can be logged for debugging |
| **Batching** | Multiple commands can be processed together |

**中文说明：** 命令模式的优势包括：解耦（分离请求者和执行者）、队列化（命令可以存储在队列中）、延迟执行（稍后或异步执行命令）、撤销/重做（命令存储状态可以反转）、日志记录（命令可以被记录用于调试）、批处理（多个命令可以一起处理）。

---

## 4. User-Space Implementation Example

```c
/*
 * Command Pattern - User Space Implementation
 * Mimics Linux Kernel's work queue mechanism
 * 
 * Compile: gcc -o command command.c -pthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

/* ============================================================
 * Command Interface
 * Similar to work_struct in kernel
 * ============================================================ */

/* Forward declarations */
struct command;
struct command_queue;

/* Command function type */
typedef void (*command_func_t)(struct command *cmd);

/* Command status */
enum command_status {
    CMD_PENDING,
    CMD_EXECUTING,
    CMD_COMPLETED,
    CMD_FAILED
};

/* Command structure */
struct command {
    const char *name;
    command_func_t execute;      /* The operation to perform */
    command_func_t undo;         /* Optional undo operation */
    void *data;                  /* Command-specific data */
    enum command_status status;
    struct command *next;        /* Queue linkage */
    
    /* For delayed commands */
    unsigned long delay_ms;
    unsigned long submit_time;
};

/* ============================================================
 * Command Queue (Invoker)
 * Similar to workqueue_struct in kernel
 * ============================================================ */

struct command_queue {
    const char *name;
    struct command *head;
    struct command *tail;
    int count;
    
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_t worker_thread;
    bool running;
    
    /* Statistics */
    int commands_executed;
    int commands_failed;
};

/* ============================================================
 * Command Queue Operations
 * ============================================================ */

/* Worker thread function */
void *worker_thread_func(void *arg)
{
    struct command_queue *queue = arg;
    struct command *cmd;
    
    printf("[Worker] Started for queue '%s'\n", queue->name);
    
    while (queue->running) {
        pthread_mutex_lock(&queue->lock);
        
        /* Wait for commands */
        while (queue->head == NULL && queue->running) {
            pthread_cond_wait(&queue->cond, &queue->lock);
        }
        
        if (!queue->running) {
            pthread_mutex_unlock(&queue->lock);
            break;
        }
        
        /* Dequeue command */
        cmd = queue->head;
        queue->head = cmd->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        queue->count--;
        
        pthread_mutex_unlock(&queue->lock);
        
        /* Check for delayed execution */
        if (cmd->delay_ms > 0) {
            unsigned long elapsed = 
                (unsigned long)time(NULL) * 1000 - cmd->submit_time;
            if (elapsed < cmd->delay_ms) {
                usleep((cmd->delay_ms - elapsed) * 1000);
            }
        }
        
        /* Execute command */
        printf("[Worker] Executing command '%s'\n", cmd->name);
        cmd->status = CMD_EXECUTING;
        
        if (cmd->execute) {
            cmd->execute(cmd);
            cmd->status = CMD_COMPLETED;
            queue->commands_executed++;
        } else {
            cmd->status = CMD_FAILED;
            queue->commands_failed++;
        }
        
        printf("[Worker] Completed command '%s'\n", cmd->name);
    }
    
    printf("[Worker] Stopped\n");
    return NULL;
}

/* Create command queue */
struct command_queue *create_command_queue(const char *name)
{
    struct command_queue *queue = malloc(sizeof(struct command_queue));
    if (!queue) return NULL;
    
    queue->name = name;
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->running = true;
    queue->commands_executed = 0;
    queue->commands_failed = 0;
    
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->cond, NULL);
    pthread_create(&queue->worker_thread, NULL, worker_thread_func, queue);
    
    printf("[Queue] Created command queue '%s'\n", name);
    return queue;
}

/* Destroy command queue */
void destroy_command_queue(struct command_queue *queue)
{
    printf("[Queue] Destroying queue '%s'\n", queue->name);
    
    pthread_mutex_lock(&queue->lock);
    queue->running = false;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
    
    pthread_join(queue->worker_thread, NULL);
    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->cond);
    
    /* Free remaining commands */
    struct command *cmd = queue->head;
    while (cmd) {
        struct command *next = cmd->next;
        free(cmd);
        cmd = next;
    }
    
    free(queue);
}

/**
 * queue_command - Add command to queue
 * @queue: Command queue
 * @cmd: Command to queue
 */
int queue_command(struct command_queue *queue, struct command *cmd)
{
    pthread_mutex_lock(&queue->lock);
    
    cmd->status = CMD_PENDING;
    cmd->next = NULL;
    cmd->submit_time = (unsigned long)time(NULL) * 1000;
    
    /* Add to tail of queue */
    if (queue->tail == NULL) {
        queue->head = queue->tail = cmd;
    } else {
        queue->tail->next = cmd;
        queue->tail = cmd;
    }
    queue->count++;
    
    printf("[Queue] Queued command '%s' (queue size: %d)\n", 
           cmd->name, queue->count);
    
    /* Wake up worker */
    pthread_cond_signal(&queue->cond);
    
    pthread_mutex_unlock(&queue->lock);
    return 0;
}

/**
 * queue_delayed_command - Queue command with delay
 * @queue: Command queue
 * @cmd: Command
 * @delay_ms: Delay in milliseconds
 */
int queue_delayed_command(struct command_queue *queue, 
                          struct command *cmd, 
                          unsigned long delay_ms)
{
    cmd->delay_ms = delay_ms;
    printf("[Queue] Queuing delayed command '%s' (delay: %lu ms)\n",
           cmd->name, delay_ms);
    return queue_command(queue, cmd);
}

/* Wait for all commands to complete */
void flush_command_queue(struct command_queue *queue)
{
    printf("[Queue] Flushing queue...\n");
    while (1) {
        pthread_mutex_lock(&queue->lock);
        int count = queue->count;
        pthread_mutex_unlock(&queue->lock);
        
        if (count == 0) break;
        usleep(10000);  /* 10ms */
    }
    printf("[Queue] Queue flushed\n");
}

/* ============================================================
 * Concrete Commands
 * ============================================================ */

/* Command 1: Print message */
struct print_cmd_data {
    char message[256];
    int repeat_count;
};

void print_execute(struct command *cmd)
{
    struct print_cmd_data *data = cmd->data;
    
    for (int i = 0; i < data->repeat_count; i++) {
        printf("  [PrintCmd] %s (iteration %d)\n", data->message, i + 1);
    }
}

struct command *create_print_command(const char *message, int repeat)
{
    struct command *cmd = malloc(sizeof(struct command));
    struct print_cmd_data *data = malloc(sizeof(struct print_cmd_data));
    
    strncpy(data->message, message, sizeof(data->message) - 1);
    data->repeat_count = repeat;
    
    cmd->name = "PrintCommand";
    cmd->execute = print_execute;
    cmd->undo = NULL;
    cmd->data = data;
    cmd->delay_ms = 0;
    
    return cmd;
}

/* Command 2: Calculate */
struct calc_cmd_data {
    int a, b;
    char operation;
    int result;
};

void calc_execute(struct command *cmd)
{
    struct calc_cmd_data *data = cmd->data;
    
    switch (data->operation) {
    case '+':
        data->result = data->a + data->b;
        break;
    case '-':
        data->result = data->a - data->b;
        break;
    case '*':
        data->result = data->a * data->b;
        break;
    case '/':
        data->result = (data->b != 0) ? data->a / data->b : 0;
        break;
    default:
        data->result = 0;
    }
    
    printf("  [CalcCmd] %d %c %d = %d\n", 
           data->a, data->operation, data->b, data->result);
}

struct command *create_calc_command(int a, int b, char op)
{
    struct command *cmd = malloc(sizeof(struct command));
    struct calc_cmd_data *data = malloc(sizeof(struct calc_cmd_data));
    
    data->a = a;
    data->b = b;
    data->operation = op;
    data->result = 0;
    
    cmd->name = "CalcCommand";
    cmd->execute = calc_execute;
    cmd->undo = NULL;
    cmd->data = data;
    cmd->delay_ms = 0;
    
    return cmd;
}

/* Command 3: File operation (with undo) */
struct file_cmd_data {
    char filename[256];
    char content[1024];
    char backup[1024];
    bool has_backup;
};

void file_execute(struct command *cmd)
{
    struct file_cmd_data *data = cmd->data;
    
    printf("  [FileCmd] Writing to file '%s': %s\n", 
           data->filename, data->content);
    
    /* Simulate backup for undo */
    strcpy(data->backup, "[previous content]");
    data->has_backup = true;
}

void file_undo(struct command *cmd)
{
    struct file_cmd_data *data = cmd->data;
    
    if (data->has_backup) {
        printf("  [FileCmd] UNDO: Restoring '%s' to: %s\n",
               data->filename, data->backup);
        data->has_backup = false;
    }
}

struct command *create_file_command(const char *filename, const char *content)
{
    struct command *cmd = malloc(sizeof(struct command));
    struct file_cmd_data *data = malloc(sizeof(struct file_cmd_data));
    
    strncpy(data->filename, filename, sizeof(data->filename) - 1);
    strncpy(data->content, content, sizeof(data->content) - 1);
    data->has_backup = false;
    
    cmd->name = "FileCommand";
    cmd->execute = file_execute;
    cmd->undo = file_undo;
    cmd->data = data;
    cmd->delay_ms = 0;
    
    return cmd;
}

/* ============================================================
 * Command History (for undo support)
 * ============================================================ */

struct command_history {
    struct command **commands;
    int capacity;
    int count;
};

struct command_history *create_history(int capacity)
{
    struct command_history *hist = malloc(sizeof(struct command_history));
    hist->commands = malloc(sizeof(struct command *) * capacity);
    hist->capacity = capacity;
    hist->count = 0;
    return hist;
}

void history_push(struct command_history *hist, struct command *cmd)
{
    if (hist->count < hist->capacity) {
        hist->commands[hist->count++] = cmd;
    }
}

struct command *history_pop(struct command_history *hist)
{
    if (hist->count > 0) {
        return hist->commands[--hist->count];
    }
    return NULL;
}

void undo_last(struct command_history *hist)
{
    struct command *cmd = history_pop(hist);
    if (cmd && cmd->undo) {
        printf("[History] Undoing command '%s'\n", cmd->name);
        cmd->undo(cmd);
    }
}

/* ============================================================
 * Main - Demonstrate Command Pattern
 * ============================================================ */

int main(void)
{
    struct command_queue *queue;
    struct command_history *history;

    printf("=== Command Pattern Demo (Work Queue) ===\n\n");

    /* Create command queue */
    queue = create_command_queue("MainWorkQueue");
    history = create_history(100);

    /* Queue various commands */
    printf("\n--- Queuing Commands ---\n");
    
    struct command *cmd1 = create_print_command("Hello, Command Pattern!", 2);
    struct command *cmd2 = create_calc_command(10, 5, '+');
    struct command *cmd3 = create_calc_command(10, 5, '*');
    struct command *cmd4 = create_file_command("/tmp/test.txt", "New content");
    
    queue_command(queue, cmd1);
    queue_command(queue, cmd2);
    queue_command(queue, cmd3);
    queue_command(queue, cmd4);
    
    /* Queue a delayed command */
    printf("\n--- Queuing Delayed Command ---\n");
    struct command *delayed_cmd = create_print_command("Delayed message!", 1);
    queue_delayed_command(queue, delayed_cmd, 1000);  /* 1 second delay */

    /* Save file command to history for undo */
    history_push(history, cmd4);

    /* Wait for all commands to complete */
    printf("\n--- Waiting for Commands ---\n");
    flush_command_queue(queue);

    /* Demonstrate undo */
    printf("\n--- Demonstrating Undo ---\n");
    undo_last(history);

    /* Print statistics */
    printf("\n--- Statistics ---\n");
    printf("Commands executed: %d\n", queue->commands_executed);
    printf("Commands failed: %d\n", queue->commands_failed);

    /* Cleanup */
    printf("\n--- Cleanup ---\n");
    destroy_command_queue(queue);
    free(history->commands);
    free(history);

    /* Free command data (in real code, would be done after execution) */
    free(cmd1->data); free(cmd1);
    free(cmd2->data); free(cmd2);
    free(cmd3->data); free(cmd3);
    free(cmd4->data); free(cmd4);
    free(delayed_cmd->data); free(delayed_cmd);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Command Execution Flow

```
+------------------------------------------------------------------+
|                   COMMAND EXECUTION FLOW                          |
+------------------------------------------------------------------+
|                                                                   |
|    Client                                                         |
|    +----------------------+                                       |
|    | cmd = create_cmd();  |                                       |
|    | queue_command(q,cmd);|                                       |
|    +----------+-----------+                                       |
|               |                                                   |
|               v                                                   |
|    +----------+-----------+                                       |
|    | Command Queue        |                                       |
|    +----------------------+                                       |
|    | [cmd1]->[cmd2]->[cmd3]                                       |
|    +----------+-----------+                                       |
|               |                                                   |
|               | (Worker thread dequeues)                          |
|               v                                                   |
|    +----------+-----------+                                       |
|    | Worker Thread        |                                       |
|    +----------------------+                                       |
|    | cmd = dequeue();     |                                       |
|    | cmd->execute(cmd);   |                                       |
|    +----------+-----------+                                       |
|               |                                                   |
|               | (Calls command function)                          |
|               v                                                   |
|    +----------+-----------+                                       |
|    | Command Execution    |                                       |
|    +----------------------+                                       |
|    | - Access cmd->data   |                                       |
|    | - Perform operation  |                                       |
|    | - Update status      |                                       |
|    +----------------------+                                       |
|                                                                   |
|    Optional: Save to history for undo                             |
|    +----------------------+                                       |
|    | Command History      |                                       |
|    | [cmd1, cmd2, cmd3]   |                                       |
|    +----------------------+                                       |
|             |                                                     |
|             | undo_last()                                         |
|             v                                                     |
|    +--------+-------------+                                       |
|    | cmd->undo(cmd);      |                                       |
|    +----------------------+                                       |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 命令执行流程：客户端创建命令对象并将其加入队列。工作线程从队列中取出命令，调用其execute函数执行实际操作。命令可以保存到历史记录中，支持撤销操作。这种设计将命令的创建、排队和执行完全解耦。

---

## 6. Key Implementation Points

1. **Command Encapsulation**: Wrap operation in struct with function pointer
2. **Queue Management**: FIFO queue with mutex for thread safety
3. **Worker Thread**: Separate thread processes commands asynchronously
4. **Delayed Execution**: Store delay and check before execution
5. **Undo Support**: Optional undo function and command history
6. **Command Data**: Each command carries its own parameters

**中文说明：** 实现命令模式的关键点：命令封装（将操作包装在带函数指针的结构体中）、队列管理（带互斥锁的FIFO队列保证线程安全）、工作线程（独立线程异步处理命令）、延迟执行（存储延迟时间并在执行前检查）、撤销支持（可选的撤销函数和命令历史）、命令数据（每个命令携带自己的参数）。

