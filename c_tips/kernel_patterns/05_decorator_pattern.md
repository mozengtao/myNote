# Decorator Pattern in Linux Kernel

## 1. Pattern Overview

```
+------------------------------------------------------------------+
|                     DECORATOR PATTERN                             |
+------------------------------------------------------------------+
|                                                                   |
|    +------------------+                                           |
|    |   Component      |<-----------------------------------+      |
|    +------------------+                                    |      |
|    | + operation()    |                                    |      |
|    +--------+---------+                                    |      |
|             ^                                              |      |
|             |                                              |      |
|    +--------+--------+--------+                            |      |
|    |                 |        |                            |      |
|    v                 v        v                            |      |
| +------+      +------------+  +------------+               |      |
| |Concr.|      | Decorator  |  | Decorator  |               |      |
| |Comp. |      |     A      |  |     B      |               |      |
| +------+      +------------+  +------------+               |      |
| |op()  |      | - wrapped  |--|-> wraps Component          |      |
| +------+      | + op()     |  +------------+               |      |
|               |   {        |                               |      |
|               |   pre();   |                               |      |
|               |   wrapped  |                               |      |
|               |   ->op();  |                               |      |
|               |   post();  |                               |      |
|               |   }        |                               |      |
|               +------------+                               |      |
|                                                            |      |
|    Decorators can wrap other decorators (chaining)  -------+      |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 装饰器模式通过嵌套结构体和函数指针，在不修改原对象的情况下动态扩展功能。Linux内核中广泛使用这种模式来添加日志、追踪、缓存、加密等功能层。装饰器可以链式组合，每个装饰器调用被装饰对象的方法前后可以添加额外处理。

---

## 2. Linux Kernel Implementation

### 2.1 Kernel Example: Ftrace Function Decorators

```c
/* From: kernel/trace/trace_functions.c */

/**
 * ftrace_ops - Function tracer operations (Decorator)
 *
 * Wraps function calls to add tracing functionality.
 * Multiple tracers can be stacked (decorator chaining).
 */
struct ftrace_ops {
    ftrace_func_t           func;      /* The decorated function */
    struct ftrace_ops       *next;     /* Next decorator in chain */
    unsigned long           flags;
    struct ftrace_hash      *notrace_hash;
    struct ftrace_hash      *filter_hash;
};

/**
 * function_trace_call - Decorator that adds tracing
 * @ip: Instruction pointer
 * @parent_ip: Caller's instruction pointer
 *
 * This function wraps every traced function call, adding
 * logging functionality without modifying the original code.
 */
static void function_trace_call(unsigned long ip, unsigned long parent_ip)
{
    struct trace_array *tr = func_trace;
    struct trace_array_cpu *data;
    unsigned long flags;
    int cpu;

    /* Pre-decoration: Check if tracing is enabled */
    if (unlikely(!ftrace_function_enabled))
        return;

    local_irq_save(flags);
    cpu = raw_smp_processor_id();
    data = tr->data[cpu];
    
    /* Check recursion (decorator guard) */
    if (likely(atomic_inc_return(&data->disabled) == 1)) {
        /* DECORATION: Record the function trace */
        trace_function(tr, ip, parent_ip, flags, preempt_count());
    }

    atomic_dec(&data->disabled);
    local_irq_restore(flags);
}

/**
 * function_stack_trace_call - Extended decorator with stack trace
 *
 * Adds stack trace recording to function tracing.
 * Demonstrates decorator adding more functionality.
 */
static void function_stack_trace_call(unsigned long ip, unsigned long parent_ip)
{
    struct trace_array *tr = func_trace;
    struct trace_array_cpu *data;
    unsigned long flags;
    int cpu;

    if (unlikely(!ftrace_function_enabled))
        return;

    local_irq_save(flags);
    cpu = raw_smp_processor_id();
    data = tr->data[cpu];
    
    if (likely(atomic_inc_return(&data->disabled) == 1)) {
        /* First decoration: trace the function */
        trace_function(tr, ip, parent_ip, flags, preempt_count());
        
        /* Second decoration: also capture stack trace */
        __trace_stack(tr, flags, 5, preempt_count());
    }

    atomic_dec(&data->disabled);
    local_irq_restore(flags);
}

/* Decorator registration */
static struct ftrace_ops trace_ops __read_mostly = {
    .func = function_trace_call,
    .flags = FTRACE_OPS_FL_GLOBAL,
};

static struct ftrace_ops trace_stack_ops __read_mostly = {
    .func = function_stack_trace_call,
    .flags = FTRACE_OPS_FL_GLOBAL,
};
```

### 2.2 Kernel Example: Block I/O Decorators (Tracing)

```c
/* From: include/trace/events/block.h */

/**
 * Block I/O tracing decorates block layer operations.
 * Each trace point wraps an operation with logging.
 */

/* Trace point decorator for block request queue */
TRACE_EVENT(block_rq_insert,
    TP_PROTO(struct request_queue *q, struct request *rq),
    
    TP_ARGS(q, rq),
    
    TP_STRUCT__entry(
        __field(dev_t, dev)
        __field(sector_t, sector)
        __field(unsigned int, nr_sector)
        /* ... more fields ... */
    ),
    
    /* PRE-DECORATION: Capture data before operation */
    TP_fast_assign(
        __entry->dev = rq->rq_disk ? disk_devt(rq->rq_disk) : 0;
        __entry->sector = blk_rq_pos(rq);
        __entry->nr_sector = blk_rq_sectors(rq);
    ),
    
    /* DECORATION: Format and output trace */
    TP_printk("%d,%d %s %llu + %u",
              MAJOR(__entry->dev), MINOR(__entry->dev),
              __entry->rwbs, __entry->sector, __entry->nr_sector)
);
```

### 2.3 Kernel Example: Crypto Cipher Chaining

```c
/* From: crypto/cipher.c */

/**
 * Crypto algorithms can be wrapped/decorated to add features
 * like encryption modes (CBC, CTR) on top of basic ciphers.
 *
 * Example: AES (base) -> CBC mode (decorator) -> CTS (decorator)
 */

struct crypto_tfm {
    u32 crt_flags;
    
    /* The actual algorithm (base component) */
    struct crypto_alg *__crt_alg;
    
    /* Exit function (cleanup decorator chain) */
    void (*exit)(struct crypto_tfm *tfm);
    
    /* Algorithm-specific context follows */
};

/**
 * Cipher modes act as decorators around base ciphers
 */
struct blkcipher_alg {
    int (*setkey)(struct crypto_tfm *tfm, const u8 *key, unsigned int keylen);
    
    /* Encryption/decryption with decorated behavior */
    int (*encrypt)(struct blkcipher_desc *desc, struct scatterlist *dst,
                   struct scatterlist *src, unsigned int nbytes);
    int (*decrypt)(struct blkcipher_desc *desc, struct scatterlist *dst,
                   struct scatterlist *src, unsigned int nbytes);
    
    /* Inner cipher (the decorated component) */
    unsigned int min_keysize;
    unsigned int max_keysize;
    unsigned int ivsize;
};
```

### 2.4 Architecture Diagram

```
+------------------------------------------------------------------+
|               LINUX KERNEL DECORATOR ARCHITECTURE                 |
+------------------------------------------------------------------+
|                                                                   |
|    Function Call Flow with Ftrace Decorators                      |
|                                                                   |
|    +------------------+                                           |
|    | Original Code    |                                           |
|    | func_a() {       |                                           |
|    |   // original    |                                           |
|    |   // code        |                                           |
|    | }                |                                           |
|    +--------+---------+                                           |
|             |                                                     |
|             | (with ftrace enabled)                               |
|             v                                                     |
|    +--------+------------------+                                  |
|    |  Decorator Layer 1        |                                  |
|    |  function_trace_call()    |                                  |
|    +---------------------------+                                  |
|    | - Check if enabled        |                                  |
|    | - Disable interrupts      |                                  |
|    | - Record timestamp        |                                  |
|    | - Call wrapped function --+---> Original func_a()            |
|    | - Record exit time        |                                  |
|    | - Enable interrupts       |                                  |
|    +---------------------------+                                  |
|             |                                                     |
|             | (with stack trace enabled)                          |
|             v                                                     |
|    +--------+------------------+                                  |
|    |  Decorator Layer 2        |                                  |
|    |  function_stack_trace()   |                                  |
|    +---------------------------+                                  |
|    | - All of above PLUS       |                                  |
|    | - Capture stack frames    |                                  |
|    | - Record call chain       |                                  |
|    +---------------------------+                                  |
|                                                                   |
|    Decorators are enabled/disabled at runtime                     |
|    Original code is NOT modified                                  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** Linux内核的ftrace系统是装饰器模式的典型应用。原始函数代码不需要修改，ftrace通过动态代码修补在函数入口处插入跳转到装饰器函数。装饰器可以叠加：基础追踪器记录函数调用，堆栈追踪器在此基础上还记录调用堆栈。装饰器可以在运行时动态启用或禁用。

---

## 3. Advantages Analysis

| Advantage | Description |
|-----------|-------------|
| **Open/Closed Principle** | Extend behavior without modifying original code |
| **Runtime Flexibility** | Add/remove decorations dynamically |
| **Single Responsibility** | Each decorator handles one concern |
| **Decorator Chaining** | Multiple decorators can be stacked |
| **Transparent Wrapping** | Client code unchanged |
| **Reusable Decorators** | Same decorator applies to different components |

**中文说明：** 装饰器模式的优势包括：开闭原则（扩展行为而不修改原代码）、运行时灵活性（动态添加/移除装饰）、单一职责（每个装饰器处理一个关注点）、装饰器链（多个装饰器可叠加）、透明包装（客户端代码不变）、可重用（同一装饰器可用于不同组件）。

---

## 4. User-Space Implementation Example

```c
/*
 * Decorator Pattern - User Space Implementation
 * Mimics Linux Kernel's ftrace/block tracing mechanism
 * 
 * Compile: gcc -o decorator decorator.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Component Interface - Base operations
 * Similar to file_operations in kernel
 * ============================================================ */

/* Forward declarations */
struct data_stream;

/* Stream operations - the interface to be decorated */
struct stream_ops {
    int (*read)(struct data_stream *stream, char *buf, int len);
    int (*write)(struct data_stream *stream, const char *buf, int len);
    void (*close)(struct data_stream *stream);
    const char *name;
};

/* Base stream structure */
struct data_stream {
    const struct stream_ops *ops;
    void *private_data;
    struct data_stream *wrapped;  /* For decorator chaining */
};

/* ============================================================
 * Concrete Component - Basic Memory Stream
 * ============================================================ */

struct memory_stream_data {
    char buffer[4096];
    int position;
    int size;
};

static int memory_read(struct data_stream *stream, char *buf, int len)
{
    struct memory_stream_data *data = stream->private_data;
    int available = data->size - data->position;
    int to_read = (len < available) ? len : available;
    
    if (to_read > 0) {
        memcpy(buf, data->buffer + data->position, to_read);
        data->position += to_read;
    }
    
    return to_read;
}

static int memory_write(struct data_stream *stream, const char *buf, int len)
{
    struct memory_stream_data *data = stream->private_data;
    int available = sizeof(data->buffer) - data->size;
    int to_write = (len < available) ? len : available;
    
    if (to_write > 0) {
        memcpy(data->buffer + data->size, buf, to_write);
        data->size += to_write;
    }
    
    return to_write;
}

static void memory_close(struct data_stream *stream)
{
    printf("[MemoryStream] Closing\n");
    free(stream->private_data);
}

static const struct stream_ops memory_ops = {
    .read = memory_read,
    .write = memory_write,
    .close = memory_close,
    .name = "MemoryStream"
};

struct data_stream *create_memory_stream(void)
{
    struct data_stream *stream = malloc(sizeof(struct data_stream));
    struct memory_stream_data *data = malloc(sizeof(struct memory_stream_data));
    
    if (!stream || !data) {
        free(stream);
        free(data);
        return NULL;
    }
    
    data->position = 0;
    data->size = 0;
    memset(data->buffer, 0, sizeof(data->buffer));
    
    stream->ops = &memory_ops;
    stream->private_data = data;
    stream->wrapped = NULL;
    
    printf("[Factory] Created MemoryStream\n");
    return stream;
}

/* ============================================================
 * Decorator 1: Logging Decorator
 * Adds logging to all stream operations
 * ============================================================ */

struct logging_decorator_data {
    FILE *log_file;
    int log_level;
};

static int logging_read(struct data_stream *stream, char *buf, int len)
{
    struct logging_decorator_data *log_data = stream->private_data;
    int result;
    
    /* PRE-DECORATION: Log before operation */
    printf("[LOG] READ request: %d bytes\n", len);
    
    /* Call wrapped stream's read */
    result = stream->wrapped->ops->read(stream->wrapped, buf, len);
    
    /* POST-DECORATION: Log after operation */
    printf("[LOG] READ complete: %d bytes read\n", result);
    
    return result;
}

static int logging_write(struct data_stream *stream, const char *buf, int len)
{
    struct logging_decorator_data *log_data = stream->private_data;
    int result;
    
    /* PRE-DECORATION */
    printf("[LOG] WRITE request: %d bytes, data='%.20s%s'\n", 
           len, buf, (len > 20) ? "..." : "");
    
    /* Call wrapped stream's write */
    result = stream->wrapped->ops->write(stream->wrapped, buf, len);
    
    /* POST-DECORATION */
    printf("[LOG] WRITE complete: %d bytes written\n", result);
    
    return result;
}

static void logging_close(struct data_stream *stream)
{
    printf("[LOG] CLOSE requested\n");
    
    /* Close wrapped stream first */
    stream->wrapped->ops->close(stream->wrapped);
    
    printf("[LOG] CLOSE complete\n");
    free(stream->private_data);
}

static const struct stream_ops logging_ops = {
    .read = logging_read,
    .write = logging_write,
    .close = logging_close,
    .name = "LoggingDecorator"
};

/**
 * wrap_with_logging - Add logging decoration to a stream
 * @stream: The stream to decorate
 *
 * Returns: New stream with logging capabilities
 */
struct data_stream *wrap_with_logging(struct data_stream *stream)
{
    struct data_stream *decorator = malloc(sizeof(struct data_stream));
    struct logging_decorator_data *data = malloc(sizeof(struct logging_decorator_data));
    
    if (!decorator || !data) {
        free(decorator);
        free(data);
        return stream;  /* Return original on failure */
    }
    
    data->log_file = stdout;
    data->log_level = 1;
    
    decorator->ops = &logging_ops;
    decorator->private_data = data;
    decorator->wrapped = stream;  /* Chain to original */
    
    printf("[Factory] Added LoggingDecorator\n");
    return decorator;
}

/* ============================================================
 * Decorator 2: Timing Decorator
 * Measures operation performance
 * ============================================================ */

struct timing_decorator_data {
    unsigned long total_read_time;
    unsigned long total_write_time;
    int read_count;
    int write_count;
};

static unsigned long get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static int timing_read(struct data_stream *stream, char *buf, int len)
{
    struct timing_decorator_data *timing = stream->private_data;
    unsigned long start, end;
    int result;
    
    /* PRE-DECORATION: Record start time */
    start = get_time_us();
    
    /* Call wrapped stream's read */
    result = stream->wrapped->ops->read(stream->wrapped, buf, len);
    
    /* POST-DECORATION: Calculate elapsed time */
    end = get_time_us();
    timing->total_read_time += (end - start);
    timing->read_count++;
    
    printf("[TIMING] READ took %lu us\n", end - start);
    
    return result;
}

static int timing_write(struct data_stream *stream, const char *buf, int len)
{
    struct timing_decorator_data *timing = stream->private_data;
    unsigned long start, end;
    int result;
    
    start = get_time_us();
    
    result = stream->wrapped->ops->write(stream->wrapped, buf, len);
    
    end = get_time_us();
    timing->total_write_time += (end - start);
    timing->write_count++;
    
    printf("[TIMING] WRITE took %lu us\n", end - start);
    
    return result;
}

static void timing_close(struct data_stream *stream)
{
    struct timing_decorator_data *timing = stream->private_data;
    
    /* Print statistics before closing */
    printf("[TIMING] Statistics:\n");
    printf("  Total reads: %d, Total time: %lu us, Avg: %lu us\n",
           timing->read_count, timing->total_read_time,
           timing->read_count ? timing->total_read_time / timing->read_count : 0);
    printf("  Total writes: %d, Total time: %lu us, Avg: %lu us\n",
           timing->write_count, timing->total_write_time,
           timing->write_count ? timing->total_write_time / timing->write_count : 0);
    
    stream->wrapped->ops->close(stream->wrapped);
    free(stream->private_data);
}

static const struct stream_ops timing_ops = {
    .read = timing_read,
    .write = timing_write,
    .close = timing_close,
    .name = "TimingDecorator"
};

struct data_stream *wrap_with_timing(struct data_stream *stream)
{
    struct data_stream *decorator = malloc(sizeof(struct data_stream));
    struct timing_decorator_data *data = malloc(sizeof(struct timing_decorator_data));
    
    if (!decorator || !data) {
        free(decorator);
        free(data);
        return stream;
    }
    
    memset(data, 0, sizeof(*data));
    
    decorator->ops = &timing_ops;
    decorator->private_data = data;
    decorator->wrapped = stream;
    
    printf("[Factory] Added TimingDecorator\n");
    return decorator;
}

/* ============================================================
 * Decorator 3: Encryption Decorator (XOR for demo)
 * Adds encryption/decryption layer
 * ============================================================ */

struct encryption_decorator_data {
    unsigned char key;
};

static void xor_data(char *buf, int len, unsigned char key)
{
    for (int i = 0; i < len; i++) {
        buf[i] ^= key;
    }
}

static int encryption_read(struct data_stream *stream, char *buf, int len)
{
    struct encryption_decorator_data *enc = stream->private_data;
    int result;
    
    /* Read encrypted data */
    result = stream->wrapped->ops->read(stream->wrapped, buf, len);
    
    /* POST-DECORATION: Decrypt the data */
    if (result > 0) {
        printf("[ENCRYPT] Decrypting %d bytes\n", result);
        xor_data(buf, result, enc->key);
    }
    
    return result;
}

static int encryption_write(struct data_stream *stream, const char *buf, int len)
{
    struct encryption_decorator_data *enc = stream->private_data;
    char *encrypted_buf;
    int result;
    
    /* PRE-DECORATION: Encrypt the data */
    encrypted_buf = malloc(len);
    if (!encrypted_buf) return -1;
    
    memcpy(encrypted_buf, buf, len);
    printf("[ENCRYPT] Encrypting %d bytes\n", len);
    xor_data(encrypted_buf, len, enc->key);
    
    /* Write encrypted data */
    result = stream->wrapped->ops->write(stream->wrapped, encrypted_buf, len);
    
    free(encrypted_buf);
    return result;
}

static void encryption_close(struct data_stream *stream)
{
    printf("[ENCRYPT] Closing encryption layer\n");
    stream->wrapped->ops->close(stream->wrapped);
    free(stream->private_data);
}

static const struct stream_ops encryption_ops = {
    .read = encryption_read,
    .write = encryption_write,
    .close = encryption_close,
    .name = "EncryptionDecorator"
};

struct data_stream *wrap_with_encryption(struct data_stream *stream, unsigned char key)
{
    struct data_stream *decorator = malloc(sizeof(struct data_stream));
    struct encryption_decorator_data *data = malloc(sizeof(struct encryption_decorator_data));
    
    if (!decorator || !data) {
        free(decorator);
        free(data);
        return stream;
    }
    
    data->key = key;
    
    decorator->ops = &encryption_ops;
    decorator->private_data = data;
    decorator->wrapped = stream;
    
    printf("[Factory] Added EncryptionDecorator (key=0x%02x)\n", key);
    return decorator;
}

/* ============================================================
 * Helper to destroy decorator chain
 * ============================================================ */

void destroy_stream_chain(struct data_stream *stream)
{
    if (stream) {
        stream->ops->close(stream);
        
        /* Free decorator wrappers (not the wrapped streams, 
         * they're freed by their close functions) */
        struct data_stream *current = stream;
        while (current) {
            struct data_stream *wrapped = current->wrapped;
            free(current);
            current = wrapped;
        }
    }
}

/* ============================================================
 * Main - Demonstrate Decorator Pattern
 * ============================================================ */

int main(void)
{
    struct data_stream *stream;
    char buffer[256];
    int bytes;

    printf("=== Decorator Pattern Demo ===\n\n");

    /* Create base stream */
    printf("--- Creating base stream ---\n");
    stream = create_memory_stream();
    
    /* Add decorators - ORDER MATTERS! */
    /* Outermost decorator is called first */
    printf("\n--- Adding decorators ---\n");
    stream = wrap_with_timing(stream);      /* Inner decorator */
    stream = wrap_with_logging(stream);     /* Middle decorator */
    stream = wrap_with_encryption(stream, 0x42);  /* Outer decorator */
    
    printf("\n--- Decorator chain: Encryption -> Logging -> Timing -> MemoryStream ---\n");

    /* Use the decorated stream */
    printf("\n--- Writing data ---\n");
    bytes = stream->ops->write(stream, "Hello, Decorated World!", 23);
    
    printf("\n--- Resetting read position ---\n");
    /* For memory stream, we need to reset position for reading */
    struct data_stream *base = stream;
    while (base->wrapped) base = base->wrapped;
    ((struct memory_stream_data *)base->private_data)->position = 0;
    
    printf("\n--- Reading data ---\n");
    memset(buffer, 0, sizeof(buffer));
    bytes = stream->ops->read(stream, buffer, sizeof(buffer));
    printf("\n[RESULT] Read data: '%s'\n", buffer);

    /* Test without encryption to show plaintext */
    printf("\n\n--- Creating simple stream (no encryption) ---\n");
    struct data_stream *simple = create_memory_stream();
    simple = wrap_with_logging(simple);
    
    simple->ops->write(simple, "Plain text message", 18);
    base = simple;
    while (base->wrapped) base = base->wrapped;
    ((struct memory_stream_data *)base->private_data)->position = 0;
    
    memset(buffer, 0, sizeof(buffer));
    simple->ops->read(simple, buffer, sizeof(buffer));
    printf("[RESULT] Read data: '%s'\n", buffer);

    /* Cleanup */
    printf("\n--- Closing streams ---\n");
    stream->ops->close(stream);
    simple->ops->close(simple);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

---

## 5. Decorator Chaining Diagram

```
+------------------------------------------------------------------+
|                 DECORATOR CHAINING FLOW                           |
+------------------------------------------------------------------+
|                                                                   |
|    Client calls: stream->ops->write(stream, "data", 4)            |
|                                                                   |
|    +-------------------+                                          |
|    | Encryption        |  (Outermost)                             |
|    | Decorator         |                                          |
|    +-------------------+                                          |
|    | 1. Encrypt data   |                                          |
|    | 2. Call wrapped   +--+                                       |
|    +-------------------+  |                                       |
|                           v                                       |
|         +-------------------+                                     |
|         | Logging           |                                     |
|         | Decorator         |                                     |
|         +-------------------+                                     |
|         | 1. Log "WRITE"    |                                     |
|         | 2. Call wrapped   +--+                                  |
|         | 3. Log "complete" |  |                                  |
|         +-------------------+  |                                  |
|                                v                                  |
|              +-------------------+                                |
|              | Timing            |                                |
|              | Decorator         |                                |
|              +-------------------+                                |
|              | 1. Record start   |                                |
|              | 2. Call wrapped   +--+                             |
|              | 3. Record end     |  |                             |
|              | 4. Calc duration  |  |                             |
|              +-------------------+  |                             |
|                                     v                             |
|                   +-------------------+                           |
|                   | Memory Stream     |  (Base Component)         |
|                   +-------------------+                           |
|                   | - Write to buffer |                           |
|                   | - Return count    |                           |
|                   +-------------------+                           |
|                                                                   |
|    Call Stack (unwinding):                                        |
|    MemoryStream.write() returns -> Timing records time ->         |
|    Logging logs completion -> Encryption returns                  |
|                                                                   |
+------------------------------------------------------------------+
```

**中文说明：** 装饰器链的执行流程：客户端调用最外层装饰器（加密），加密装饰器预处理数据后调用下一层（日志），日志装饰器记录操作后调用下一层（计时），计时装饰器记录开始时间后调用基础组件（内存流），内存流执行实际写操作后返回，然后调用链逐层返回，每个装饰器执行后处理（计时计算耗时，日志记录完成）。

---

## 6. Key Implementation Points

1. **Common Interface**: Decorator implements same interface as component
2. **Wrapped Reference**: Store pointer to wrapped component
3. **Delegation**: Call wrapped component's method in decorator
4. **Pre/Post Processing**: Add behavior before/after delegation
5. **Chain Awareness**: Each decorator should properly propagate calls
6. **Resource Management**: Cleanup must traverse the chain

**中文说明：** 实现装饰器模式的关键点：装饰器实现与组件相同的接口、存储对被包装组件的引用、在装饰器中调用被包装组件的方法、在委托前后添加行为、每个装饰器正确传播调用、资源清理必须遍历整个链。

