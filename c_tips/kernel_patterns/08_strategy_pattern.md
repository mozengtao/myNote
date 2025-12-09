# Strategy Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                     STRATEGY PATTERN                              |
+------------------------------------------------------------------+
|                                                                   |
|    +------------------+           +------------------+            |
|    |    Context       |           |   Strategy       |            |
|    +------------------+   uses    +------------------+            |
|    | - strategy*      |---------->| + algorithm()    |            |
|    | + execute()      |           +--------+---------+            |
|    |   {              |                    ^                      |
|    |   strategy->     |                    |                      |
|    |   algorithm();   |         +----------+----------+           |
|    |   }              |         |          |          |           |
|    +------------------+         v          v          v           |
|                            +--------+ +--------+ +--------+       |
|                            |Concrete| |Concrete| |Concrete|       |
|                            |Strat.A | |Strat.B | |Strat.C |       |
|                            +--------+ +--------+ +--------+       |
|                            |alg() A | |alg() B | |alg() C |       |
|                            +--------+ +--------+ +--------+       |
|                                                                   |
|    Strategy can be changed at runtime via function pointer        |
|    Different algorithms interchangeable through common interface  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 策略模式将不同算法封装为独立策略，通过函数指针动态切换，适配不同业务场景。在Linux内核中，调度器类（CFS、RT、Deadline）、文件系统操作、网络协议处理都是策略模式的典型应用。上下文持有策略的函数指针，运行时可以切换到不同的策略实现。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Scheduler Classes (Strategies)

```c
/* From: include/linux/sched.h */

/**
 * struct sched_class - Scheduler strategy interface
 *
 * Different scheduling algorithms implement this interface.
 * Each task has a pointer to its scheduling class.
 */
struct sched_class {
    const struct sched_class *next;

    /* Strategy operations - different implementations */
    void (*enqueue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*dequeue_task)(struct rq *rq, struct task_struct *p, int flags);
    void (*yield_task)(struct rq *rq);

    void (*check_preempt_curr)(struct rq *rq, struct task_struct *p, int flags);

    struct task_struct *(*pick_next_task)(struct rq *rq);
    void (*put_prev_task)(struct rq *rq, struct task_struct *p);

    void (*set_curr_task)(struct rq *rq);
    void (*task_tick)(struct rq *rq, struct task_struct *p, int queued);
    void (*task_fork)(struct task_struct *p);
    /* ... more operations ... */
};

/* Task structure contains strategy reference */
struct task_struct {
    volatile long state;
    void *stack;
    
    /* STRATEGY: Reference to scheduling algorithm */
    const struct sched_class *sched_class;
    
    struct sched_entity se;      /* For CFS scheduler */
    struct sched_rt_entity rt;   /* For RT scheduler */
    /* ... */
};

/* Concrete Strategy 1: Completely Fair Scheduler */
const struct sched_class fair_sched_class = {
    .next           = &idle_sched_class,
    .enqueue_task   = enqueue_task_fair,
    .dequeue_task   = dequeue_task_fair,
    .yield_task     = yield_task_fair,
    .check_preempt_curr = check_preempt_wakeup,
    .pick_next_task = pick_next_task_fair,
    .put_prev_task  = put_prev_task_fair,
    .set_curr_task  = set_curr_task_fair,
    .task_tick      = task_tick_fair,
    .task_fork      = task_fork_fair,
    /* ... */
};

/* Concrete Strategy 2: Real-Time Scheduler */
const struct sched_class rt_sched_class = {
    .next           = &fair_sched_class,
    .enqueue_task   = enqueue_task_rt,
    .dequeue_task   = dequeue_task_rt,
    .yield_task     = yield_task_rt,
    .check_preempt_curr = check_preempt_curr_rt,
    .pick_next_task = pick_next_task_rt,
    .put_prev_task  = put_prev_task_rt,
    .set_curr_task  = set_curr_task_rt,
    .task_tick      = task_tick_rt,
    /* ... */
};
```

### 2.2 Kernel Example: File Operations Strategy

```c
/* From: include/linux/fs.h */

/**
 * struct file_operations - File I/O strategy interface
 *
 * Each filesystem provides its own implementations.
 * File structure contains pointer to operations (strategy).
 */
struct file_operations {
    struct module *owner;
    
    /* Strategy operations for file I/O */
    loff_t (*llseek)(struct file *, loff_t, int);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    int (*mmap)(struct file *, struct vm_area_struct *);
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);
    unsigned int (*poll)(struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    /* ... */
};

/* File structure contains strategy reference */
struct file {
    struct path         f_path;
    struct inode        *f_inode;
    
    /* STRATEGY: Operations specific to this file type */
    const struct file_operations *f_op;
    
    unsigned int        f_flags;
    fmode_t             f_mode;
    loff_t              f_pos;
    /* ... */
};

/* Context uses strategy */
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
    ssize_t ret;
    
    /* Use the strategy (file's operations) */
    if (file->f_op->read)
        ret = file->f_op->read(file, buf, count, pos);
    else
        ret = do_sync_read(file, buf, count, pos);
    
    return ret;
}
```

### 2.3 Kernel Example: Network Protocol Operations

```c
/* From: include/linux/net.h */

/**
 * struct proto_ops - Network protocol strategy interface
 *
 * Different protocols (TCP, UDP, RAW) implement different strategies.
 */
struct proto_ops {
    int family;
    struct module *owner;
    
    /* Strategy operations */
    int (*release)(struct socket *sock);
    int (*bind)(struct socket *sock, struct sockaddr *myaddr, int sockaddr_len);
    int (*connect)(struct socket *sock, struct sockaddr *vaddr, 
                   int sockaddr_len, int flags);
    int (*accept)(struct socket *sock, struct socket *newsock, int flags);
    int (*getname)(struct socket *sock, struct sockaddr *addr, int *len, int peer);
    int (*listen)(struct socket *sock, int backlog);
    int (*shutdown)(struct socket *sock, int flags);
    int (*sendmsg)(struct kiocb *iocb, struct socket *sock, 
                   struct msghdr *m, size_t len);
    int (*recvmsg)(struct kiocb *iocb, struct socket *sock, 
                   struct msghdr *m, size_t len, int flags);
    /* ... */
};

/* Socket structure contains strategy reference */
struct socket {
    socket_state        state;
    short               type;
    unsigned long       flags;
    
    /* STRATEGY: Protocol-specific operations */
    const struct proto_ops *ops;
    
    struct file         *file;
    struct sock         *sk;
    /* ... */
};

/* TCP protocol strategy */
const struct proto_ops inet_stream_ops = {
    .family     = PF_INET,
    .owner      = THIS_MODULE,
    .release    = inet_release,
    .bind       = inet_bind,
    .connect    = inet_stream_connect,
    .accept     = inet_accept,
    .listen     = inet_listen,
    .shutdown   = inet_shutdown,
    .sendmsg    = tcp_sendmsg,
    .recvmsg    = tcp_recvmsg,
    /* ... */
};

/* UDP protocol strategy */
const struct proto_ops inet_dgram_ops = {
    .family     = PF_INET,
    .owner      = THIS_MODULE,
    .release    = inet_release,
    .bind       = inet_bind,
    .connect    = inet_dgram_connect,
    .sendmsg    = udp_sendmsg,
    .recvmsg    = udp_recvmsg,
    /* ... */
};
```

### 2.4 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL STRATEGY PATTERN                       |
|                  (Scheduler Classes)                              |
+------------------------------------------------------------------+
|                                                                   |
|    Task Structure (Context)                                       |
|    +------------------------+                                     |
|    | struct task_struct     |                                     |
|    +------------------------+                                     |
|    | pid, state, stack      |                                     |
|    | ...                    |                                     |
|    | sched_class* ----------+---> Points to strategy              |
|    +------------------------+                                     |
|                                                                   |
|    Strategy Selection (based on task policy)                      |
|                                                                   |
|    +------------------+   +------------------+   +---------------+|
|    | SCHED_FIFO/RR    |   | SCHED_NORMAL     |   | SCHED_IDLE    ||
|    +--------+---------+   +--------+---------+   +-------+-------+|
|             |                      |                     |        |
|             v                      v                     v        |
|    +--------+---------+   +--------+---------+   +-------+-------+|
|    | rt_sched_class   |   | fair_sched_class |   |idle_sched_cls ||
|    +------------------+   +------------------+   +---------------+|
|    | pick_next_task   |   | pick_next_task   |   |pick_next_task ||
|    | enqueue_task     |   | enqueue_task     |   |enqueue_task   ||
|    | dequeue_task     |   | dequeue_task     |   |dequeue_task   ||
|    | task_tick        |   | task_tick        |   |task_tick      ||
|    +------------------+   +------------------+   +---------------+|
|                                                                   |
|    Scheduler picks next task using current strategy:              |
|    next = current->sched_class->pick_next_task(rq);               |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux调度器使用策略模式。每个任务的task_struct包含指向调度类（sched_class）的指针。不同的调度策略（如CFS、RT、Idle）实现相同的接口但有不同的算法。调度器调用current->sched_class->pick_next_task()时，会执行当前任务对应策略的算法。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Algorithm Encapsulation** | Each algorithm in its own module |
| **Runtime Switching** | Change algorithm by changing function pointer |
| **Open/Closed Principle** | Add new strategies without modifying context |
| **Eliminates Conditionals** | Replace if-else chains with polymorphism |
| **Code Reuse** | Share context code among different strategies |
| **Testing** | Test each strategy independently |

**中文说明：** 策略模式的优势包括：算法封装（每个算法在独立模块中）、运行时切换（通过改变函数指针切换算法）、开闭原则（添加新策略无需修改上下文）、消除条件判断（用多态替代if-else链）、代码重用（不同策略共享上下文代码）、可测试性（独立测试每个策略）。

---

## 4. User-Space Implementation Example

```c
/*
 * Strategy Pattern - User Space Implementation
 * Mimics Linux Kernel's scheduler class design
 * 
 * Compile: gcc -o strategy strategy.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ============================================================
 * Strategy Interface - Compression Algorithm Operations
 * Similar to sched_class in kernel
 * ============================================================ */

/* Forward declarations */
struct compressor;
struct data_buffer;

/* Strategy interface */
struct compression_ops {
    const char *name;
    int (*compress)(struct compressor *c, struct data_buffer *in, 
                    struct data_buffer *out);
    int (*decompress)(struct compressor *c, struct data_buffer *in, 
                      struct data_buffer *out);
    int (*get_ratio)(struct compressor *c);
    void (*get_stats)(struct compressor *c, char *buf, int len);
};

/* ============================================================
 * Context - Compressor
 * Similar to task_struct in kernel
 * ============================================================ */

struct data_buffer {
    char *data;
    int size;
    int capacity;
};

/* Context structure */
struct compressor {
    const char *name;
    
    /* STRATEGY: Reference to compression algorithm */
    const struct compression_ops *ops;
    
    /* Statistics */
    int total_compressed;
    int total_original;
    int compression_count;
    
    /* Strategy-specific data */
    void *private_data;
};

/* ============================================================
 * Concrete Strategy 1: Run-Length Encoding (RLE)
 * ============================================================ */

static int rle_compress(struct compressor *c, struct data_buffer *in, 
                        struct data_buffer *out)
{
    int i, count;
    int out_pos = 0;
    char current;
    
    printf("[RLE] Compressing %d bytes\n", in->size);
    
    i = 0;
    while (i < in->size && out_pos < out->capacity - 2) {
        current = in->data[i];
        count = 1;
        
        /* Count consecutive characters */
        while (i + count < in->size && 
               in->data[i + count] == current && 
               count < 255) {
            count++;
        }
        
        /* Write count and character */
        out->data[out_pos++] = count;
        out->data[out_pos++] = current;
        i += count;
    }
    
    out->size = out_pos;
    
    /* Update statistics */
    c->total_original += in->size;
    c->total_compressed += out->size;
    c->compression_count++;
    
    printf("[RLE] Compressed: %d -> %d bytes (%.1f%%)\n", 
           in->size, out->size, (float)out->size / in->size * 100);
    
    return out->size;
}

static int rle_decompress(struct compressor *c, struct data_buffer *in,
                          struct data_buffer *out)
{
    int i, j;
    int out_pos = 0;
    int count;
    char ch;
    
    printf("[RLE] Decompressing %d bytes\n", in->size);
    
    for (i = 0; i < in->size - 1 && out_pos < out->capacity; i += 2) {
        count = (unsigned char)in->data[i];
        ch = in->data[i + 1];
        
        for (j = 0; j < count && out_pos < out->capacity; j++) {
            out->data[out_pos++] = ch;
        }
    }
    
    out->size = out_pos;
    return out->size;
}

static int rle_get_ratio(struct compressor *c)
{
    if (c->total_original == 0) return 100;
    return (c->total_compressed * 100) / c->total_original;
}

static void rle_get_stats(struct compressor *c, char *buf, int len)
{
    snprintf(buf, len, "RLE Stats: %d compressions, ratio=%d%%",
             c->compression_count, rle_get_ratio(c));
}

static const struct compression_ops rle_ops = {
    .name = "RLE",
    .compress = rle_compress,
    .decompress = rle_decompress,
    .get_ratio = rle_get_ratio,
    .get_stats = rle_get_stats
};

/* ============================================================
 * Concrete Strategy 2: Simple Dictionary Encoding
 * ============================================================ */

struct dict_private {
    char dict[256][16];  /* Dictionary entries */
    int dict_size;
};

static int dict_compress(struct compressor *c, struct data_buffer *in,
                         struct data_buffer *out)
{
    struct dict_private *priv = c->private_data;
    int i, j, out_pos = 0;
    int best_match, best_len;
    
    printf("[DICT] Compressing %d bytes\n", in->size);
    
    /* Simple word-based dictionary compression */
    i = 0;
    while (i < in->size && out_pos < out->capacity - 1) {
        best_match = -1;
        best_len = 0;
        
        /* Find longest match in dictionary */
        for (j = 0; j < priv->dict_size; j++) {
            int len = strlen(priv->dict[j]);
            if (len > best_len && i + len <= in->size &&
                strncmp(&in->data[i], priv->dict[j], len) == 0) {
                best_match = j;
                best_len = len;
            }
        }
        
        if (best_match >= 0 && best_len > 1) {
            /* Output dictionary reference */
            out->data[out_pos++] = 0;  /* Marker */
            out->data[out_pos++] = best_match;
            i += best_len;
        } else {
            /* Output literal */
            out->data[out_pos++] = in->data[i++];
        }
    }
    
    out->size = out_pos;
    
    c->total_original += in->size;
    c->total_compressed += out->size;
    c->compression_count++;
    
    printf("[DICT] Compressed: %d -> %d bytes (%.1f%%)\n",
           in->size, out->size, (float)out->size / in->size * 100);
    
    return out->size;
}

static int dict_decompress(struct compressor *c, struct data_buffer *in,
                           struct data_buffer *out)
{
    struct dict_private *priv = c->private_data;
    int i, out_pos = 0;
    
    printf("[DICT] Decompressing %d bytes\n", in->size);
    
    for (i = 0; i < in->size && out_pos < out->capacity; i++) {
        if (in->data[i] == 0 && i + 1 < in->size) {
            /* Dictionary reference */
            int idx = (unsigned char)in->data[++i];
            if (idx < priv->dict_size) {
                int len = strlen(priv->dict[idx]);
                if (out_pos + len <= out->capacity) {
                    memcpy(&out->data[out_pos], priv->dict[idx], len);
                    out_pos += len;
                }
            }
        } else {
            /* Literal */
            out->data[out_pos++] = in->data[i];
        }
    }
    
    out->size = out_pos;
    return out->size;
}

static int dict_get_ratio(struct compressor *c)
{
    if (c->total_original == 0) return 100;
    return (c->total_compressed * 100) / c->total_original;
}

static void dict_get_stats(struct compressor *c, char *buf, int len)
{
    struct dict_private *priv = c->private_data;
    snprintf(buf, len, "DICT Stats: %d compressions, dict_size=%d, ratio=%d%%",
             c->compression_count, priv->dict_size, dict_get_ratio(c));
}

static const struct compression_ops dict_ops = {
    .name = "Dictionary",
    .compress = dict_compress,
    .decompress = dict_decompress,
    .get_ratio = dict_get_ratio,
    .get_stats = dict_get_stats
};

/* ============================================================
 * Concrete Strategy 3: No Compression (Passthrough)
 * ============================================================ */

static int none_compress(struct compressor *c, struct data_buffer *in,
                         struct data_buffer *out)
{
    printf("[NONE] Passthrough %d bytes\n", in->size);
    
    int to_copy = (in->size < out->capacity) ? in->size : out->capacity;
    memcpy(out->data, in->data, to_copy);
    out->size = to_copy;
    
    c->total_original += in->size;
    c->total_compressed += out->size;
    c->compression_count++;
    
    return out->size;
}

static int none_decompress(struct compressor *c, struct data_buffer *in,
                           struct data_buffer *out)
{
    int to_copy = (in->size < out->capacity) ? in->size : out->capacity;
    memcpy(out->data, in->data, to_copy);
    out->size = to_copy;
    return out->size;
}

static int none_get_ratio(struct compressor *c)
{
    return 100;  /* No compression */
}

static void none_get_stats(struct compressor *c, char *buf, int len)
{
    snprintf(buf, len, "NONE Stats: %d operations, no compression",
             c->compression_count);
}

static const struct compression_ops none_ops = {
    .name = "None",
    .compress = none_compress,
    .decompress = none_decompress,
    .get_ratio = none_get_ratio,
    .get_stats = none_get_stats
};

/* ============================================================
 * Context Operations
 * ============================================================ */

/* Create compressor with specified strategy */
struct compressor *create_compressor(const struct compression_ops *ops,
                                     void *private_data)
{
    struct compressor *c = malloc(sizeof(struct compressor));
    if (!c) return NULL;
    
    c->name = ops->name;
    c->ops = ops;
    c->total_compressed = 0;
    c->total_original = 0;
    c->compression_count = 0;
    c->private_data = private_data;
    
    printf("[Factory] Created compressor with %s strategy\n", ops->name);
    return c;
}

/* Change strategy at runtime */
void set_compression_strategy(struct compressor *c, 
                              const struct compression_ops *ops,
                              void *private_data)
{
    printf("[Strategy] Switching from %s to %s\n", c->ops->name, ops->name);
    c->ops = ops;
    c->private_data = private_data;
}

/* Use strategy to compress */
int compress_data(struct compressor *c, struct data_buffer *in,
                  struct data_buffer *out)
{
    return c->ops->compress(c, in, out);
}

/* Use strategy to decompress */
int decompress_data(struct compressor *c, struct data_buffer *in,
                    struct data_buffer *out)
{
    return c->ops->decompress(c, in, out);
}

/* Get statistics using strategy */
void print_stats(struct compressor *c)
{
    char buf[256];
    c->ops->get_stats(c, buf, sizeof(buf));
    printf("[Stats] %s\n", buf);
}

/* ============================================================
 * Helper Functions
 * ============================================================ */

struct data_buffer *create_buffer(int capacity)
{
    struct data_buffer *buf = malloc(sizeof(struct data_buffer));
    if (!buf) return NULL;
    
    buf->data = malloc(capacity);
    buf->size = 0;
    buf->capacity = capacity;
    return buf;
}

void destroy_buffer(struct data_buffer *buf)
{
    if (buf) {
        free(buf->data);
        free(buf);
    }
}

/* ============================================================
 * Main - Demonstrate Strategy Pattern
 * ============================================================ */

int main(void)
{
    struct compressor *comp;
    struct data_buffer *input;
    struct data_buffer *compressed;
    struct data_buffer *decompressed;
    struct dict_private dict_data;

    printf("=== Strategy Pattern Demo (Compression) ===\n\n");

    /* Initialize dictionary for dict strategy */
    strcpy(dict_data.dict[0], "the ");
    strcpy(dict_data.dict[1], "and ");
    strcpy(dict_data.dict[2], "is ");
    strcpy(dict_data.dict[3], "to ");
    strcpy(dict_data.dict[4], "compression");
    dict_data.dict_size = 5;

    /* Create buffers */
    input = create_buffer(1024);
    compressed = create_buffer(2048);
    decompressed = create_buffer(1024);

    /* Test data with repetition (good for RLE) */
    const char *test_data1 = "AAAAAABBBBCCCCCCCCDDEE";
    /* Test data with common words (good for dictionary) */
    const char *test_data2 = "the compression is the best and the result is amazing";

    /* Test 1: RLE Strategy */
    printf("--- Test 1: RLE Compression ---\n");
    comp = create_compressor(&rle_ops, NULL);
    
    strcpy(input->data, test_data1);
    input->size = strlen(test_data1);
    
    compress_data(comp, input, compressed);
    decompress_data(comp, compressed, decompressed);
    decompressed->data[decompressed->size] = '\0';
    
    printf("Original:     '%s'\n", test_data1);
    printf("Decompressed: '%s'\n", decompressed->data);
    printf("Match: %s\n\n", strcmp(test_data1, decompressed->data) == 0 ? "YES" : "NO");
    print_stats(comp);

    /* Test 2: Switch to Dictionary Strategy at runtime */
    printf("\n--- Test 2: Dictionary Compression ---\n");
    set_compression_strategy(comp, &dict_ops, &dict_data);
    
    strcpy(input->data, test_data2);
    input->size = strlen(test_data2);
    
    compress_data(comp, input, compressed);
    decompress_data(comp, compressed, decompressed);
    decompressed->data[decompressed->size] = '\0';
    
    printf("Original:     '%s'\n", test_data2);
    printf("Decompressed: '%s'\n", decompressed->data);
    print_stats(comp);

    /* Test 3: Switch to No Compression */
    printf("\n--- Test 3: No Compression (Passthrough) ---\n");
    set_compression_strategy(comp, &none_ops, NULL);
    
    compress_data(comp, input, compressed);
    print_stats(comp);

    /* Cleanup */
    free(comp);
    destroy_buffer(input);
    destroy_buffer(compressed);
    destroy_buffer(decompressed);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Strategy Selection Flow

```
+------------------------------------------------------------------+
|                   STRATEGY SELECTION FLOW                         |
+------------------------------------------------------------------+
|                                                                   |
|    Context (Compressor)                                           |
|    +---------------------------+                                  |
|    | ops = &rle_ops           |  (Initial strategy)               |
|    +-------------+-------------+                                  |
|                  |                                                |
|    compress_data(comp, in, out)                                   |
|                  |                                                |
|                  v                                                |
|    +-------------+-------------+                                  |
|    | comp->ops->compress()    |  (Calls RLE algorithm)            |
|    | = rle_compress()         |                                   |
|    +---------------------------+                                  |
|                                                                   |
|    Runtime Strategy Change:                                       |
|    set_compression_strategy(comp, &dict_ops, data)                |
|                                                                   |
|    +---------------------------+                                  |
|    | ops = &dict_ops          |  (New strategy)                   |
|    +-------------+-------------+                                  |
|                  |                                                |
|    compress_data(comp, in, out)                                   |
|                  |                                                |
|                  v                                                |
|    +-------------+-------------+                                  |
|    | comp->ops->compress()    |  (Calls Dict algorithm)           |
|    | = dict_compress()        |                                   |
|    +---------------------------+                                  |
|                                                                   |
|    Same context, same interface, different behavior               |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 策略选择流程：上下文（压缩器）持有指向策略操作的指针。调用compress_data()时，通过comp->ops->compress()调用当前策略的算法。在运行时调用set_compression_strategy()可以切换到不同策略，之后相同的compress_data()调用会执行新策略的算法。接口不变，行为改变。

---

## 6. Key Implementation Points

1. **Function Pointer Table**: Strategy is a struct of function pointers
2. **Context Reference**: Context holds pointer to current strategy
3. **Runtime Switching**: Strategy can be changed by reassigning pointer
4. **Common Interface**: All strategies implement same function signatures
5. **Strategy-Specific Data**: Use private_data for algorithm parameters
6. **Stateless vs Stateful**: Strategies can maintain state in context

**中文说明：** 实现策略模式的关键点：策略是函数指针结构体、上下文持有指向当前策略的指针、通过重新赋值指针实现运行时切换、所有策略实现相同的函数签名、使用private_data存储算法参数、策略可以在上下文中维护状态。

