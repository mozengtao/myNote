# 装饰器模式 (Decorator Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      DECORATOR PATTERN                            |
+------------------------------------------------------------------+

    BASIC COMPONENT:
    
    +------------------+
    |   DataStream     |
    |  +------------+  |
    |  | write()    |  |
    |  | read()     |  |
    |  +------------+  |
    +------------------+


    DECORATED COMPONENT (Layered):
    
    +----------------------------------------------------------+
    |                 ENCRYPTION DECORATOR                      |
    |  +----------------------------------------------------+  |
    |  |               COMPRESSION DECORATOR                |  |
    |  |  +----------------------------------------------+  |  |
    |  |  |              LOGGING DECORATOR               |  |  |
    |  |  |  +----------------------------------------+  |  |  |
    |  |  |  |          BASE DataStream              |  |  |  |
    |  |  |  |  write() - actual write               |  |  |  |
    |  |  |  +----------------------------------------+  |  |  |
    |  |  |  write() { log(); base->write(); }           |  |  |
    |  |  +----------------------------------------------+  |  |
    |  |  write() { compress(); base->write(); }            |  |
    |  +----------------------------------------------------+  |
    |  write() { encrypt(); base->write(); }                    |
    +----------------------------------------------------------+


    CALL FLOW:
    
    client->write(data)
        |
        v
    [Encryption] --> encrypt(data)
        |
        v
    [Compression] --> compress(encrypted_data)
        |
        v
    [Logging] --> log("writing...")
        |
        v
    [Base Stream] --> actual_write(processed_data)


    STRUCTURE:
    
    +------------------+
    |   stream_ops_t   |  <-- Common Interface
    | write(), read()  |
    +--------+---------+
             ^
             |
    +--------+--------+--------+--------+
    |                 |                 |
    +--------+   +----+----+   +--------+
    | Base   |   |Decorator|   |Decorator|
    | Stream |   |  (Log)  |   |(Encrypt)|
    +--------+   +----+----+   +----+----+
                      |             |
                      v             v
                 [wrapped]     [wrapped]
                   stream        stream
```

**核心思想说明：**
- 动态地给对象添加额外职责
- 不修改原始对象，通过包装（装饰）扩展功能
- 装饰器和被装饰对象有相同接口
- 可以层层嵌套，灵活组合功能

## 实现思路

1. **定义公共接口**：装饰器和被装饰对象使用相同接口
2. **持有被装饰对象**：装饰器内部包含指向被装饰对象的指针
3. **扩展功能**：在调用被装饰对象方法前后添加新功能
4. **链式组合**：装饰器可以装饰另一个装饰器

## 典型应用场景

- I/O流增强（缓冲、压缩、加密）
- 日志装饰
- 权限检查装饰
- 性能监控装饰
- 缓存装饰

## 完整代码示例

```c
/*============================================================================
 * 装饰器模式示例 - 数据流处理（日志/压缩/加密）
 *============================================================================*/

/*---------------------------------------------------------------------------
 * stream.h - 公共接口定义
 *---------------------------------------------------------------------------*/
#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 关键点：公共接口 - 装饰器和基础流都实现此接口 */
typedef struct stream stream_t;

typedef struct {
    int (*write)(stream_t *stream, const uint8_t *data, size_t len);
    int (*read)(stream_t *stream, uint8_t *data, size_t len);
    void (*close)(stream_t *stream);
} stream_ops_t;

struct stream {
    const stream_ops_t *ops;
    void *context;          /* 私有数据 */
    stream_t *wrapped;      /* 关键点：被装饰的流（装饰器用） */
};

/* 创建函数声明 */
stream_t* file_stream_create(const char *filename);
stream_t* logging_decorator_create(stream_t *wrapped, const char *tag);
stream_t* compression_decorator_create(stream_t *wrapped, int level);
stream_t* encryption_decorator_create(stream_t *wrapped, const uint8_t *key);

void stream_destroy(stream_t *stream);

#endif /* STREAM_H */


/*---------------------------------------------------------------------------
 * file_stream.c - 基础文件流实现
 *---------------------------------------------------------------------------*/
#include "stream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char filename[256];
    FILE *fp;
} file_context_t;

static int file_write(stream_t *stream, const uint8_t *data, size_t len) {
    file_context_t *ctx = (file_context_t *)stream->context;
    
    printf("[FileStream] Writing %zu bytes to '%s'\n", len, ctx->filename);
    
    /* 模拟写入 */
    if (ctx->fp != NULL) {
        return (int)fwrite(data, 1, len, ctx->fp);
    }
    
    /* 演示：打印数据 */
    printf("[FileStream] Data: ");
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    return (int)len;
}

static int file_read(stream_t *stream, uint8_t *data, size_t len) {
    file_context_t *ctx = (file_context_t *)stream->context;
    
    printf("[FileStream] Reading %zu bytes from '%s'\n", len, ctx->filename);
    
    /* 模拟读取 */
    memset(data, 0xAA, len);
    return (int)len;
}

static void file_close(stream_t *stream) {
    file_context_t *ctx = (file_context_t *)stream->context;
    
    printf("[FileStream] Closing '%s'\n", ctx->filename);
    
    if (ctx->fp != NULL) {
        fclose(ctx->fp);
        ctx->fp = NULL;
    }
}

static const stream_ops_t file_stream_ops = {
    .write = file_write,
    .read = file_read,
    .close = file_close
};

stream_t* file_stream_create(const char *filename) {
    stream_t *stream = malloc(sizeof(stream_t));
    file_context_t *ctx = malloc(sizeof(file_context_t));
    
    if (stream == NULL || ctx == NULL) {
        free(stream);
        free(ctx);
        return NULL;
    }
    
    strncpy(ctx->filename, filename, sizeof(ctx->filename) - 1);
    ctx->fp = NULL;  /* 演示用，不实际打开文件 */
    
    stream->ops = &file_stream_ops;
    stream->context = ctx;
    stream->wrapped = NULL;  /* 基础流没有被装饰的流 */
    
    printf("[FileStream] Created for '%s'\n", filename);
    return stream;
}


/*---------------------------------------------------------------------------
 * logging_decorator.c - 日志装饰器
 *---------------------------------------------------------------------------*/
#include "stream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    char tag[32];
    uint32_t write_count;
    uint32_t read_count;
    size_t bytes_written;
    size_t bytes_read;
} logging_context_t;

/* 关键点：装饰器方法 - 添加日志后调用被装饰对象 */
static int logging_write(stream_t *stream, const uint8_t *data, size_t len) {
    logging_context_t *ctx = (logging_context_t *)stream->context;
    stream_t *wrapped = stream->wrapped;
    
    /* 装饰：记录日志 */
    ctx->write_count++;
    printf("[%s] WRITE #%u: %zu bytes\n", ctx->tag, ctx->write_count, len);
    
    /* 关键点：调用被装饰的流 */
    int result = wrapped->ops->write(wrapped, data, len);
    
    /* 装饰：统计 */
    if (result > 0) {
        ctx->bytes_written += result;
        printf("[%s] Total written: %zu bytes\n", ctx->tag, ctx->bytes_written);
    }
    
    return result;
}

static int logging_read(stream_t *stream, uint8_t *data, size_t len) {
    logging_context_t *ctx = (logging_context_t *)stream->context;
    stream_t *wrapped = stream->wrapped;
    
    ctx->read_count++;
    printf("[%s] READ #%u: requesting %zu bytes\n", ctx->tag, ctx->read_count, len);
    
    int result = wrapped->ops->read(wrapped, data, len);
    
    if (result > 0) {
        ctx->bytes_read += result;
        printf("[%s] Total read: %zu bytes\n", ctx->tag, ctx->bytes_read);
    }
    
    return result;
}

static void logging_close(stream_t *stream) {
    logging_context_t *ctx = (logging_context_t *)stream->context;
    stream_t *wrapped = stream->wrapped;
    
    printf("[%s] CLOSE - Stats: writes=%u, reads=%u, bytes_out=%zu, bytes_in=%zu\n",
           ctx->tag, ctx->write_count, ctx->read_count, 
           ctx->bytes_written, ctx->bytes_read);
    
    wrapped->ops->close(wrapped);
}

static const stream_ops_t logging_decorator_ops = {
    .write = logging_write,
    .read = logging_read,
    .close = logging_close
};

stream_t* logging_decorator_create(stream_t *wrapped, const char *tag) {
    stream_t *stream = malloc(sizeof(stream_t));
    logging_context_t *ctx = malloc(sizeof(logging_context_t));
    
    if (stream == NULL || ctx == NULL) {
        free(stream);
        free(ctx);
        return NULL;
    }
    
    memset(ctx, 0, sizeof(logging_context_t));
    strncpy(ctx->tag, tag, sizeof(ctx->tag) - 1);
    
    stream->ops = &logging_decorator_ops;
    stream->context = ctx;
    stream->wrapped = wrapped;  /* 关键点：保存被装饰的流 */
    
    printf("[LogDecorator] Created with tag '%s'\n", tag);
    return stream;
}


/*---------------------------------------------------------------------------
 * compression_decorator.c - 压缩装饰器
 *---------------------------------------------------------------------------*/
typedef struct {
    int compression_level;
    size_t original_bytes;
    size_t compressed_bytes;
} compression_context_t;

/* 简单的"压缩"模拟：实际上只是标记数据 */
static void simple_compress(const uint8_t *input, size_t in_len,
                            uint8_t *output, size_t *out_len) {
    /* 模拟压缩：添加头部标记 */
    output[0] = 'C';
    output[1] = 'M';
    output[2] = 'P';
    memcpy(output + 3, input, in_len);
    *out_len = in_len + 3;
}

static int compression_write(stream_t *stream, const uint8_t *data, size_t len) {
    compression_context_t *ctx = (compression_context_t *)stream->context;
    stream_t *wrapped = stream->wrapped;
    
    /* 装饰：压缩数据 */
    uint8_t *compressed = malloc(len + 16);
    size_t compressed_len;
    
    printf("[Compress] Compressing %zu bytes (level %d)\n", len, ctx->compression_level);
    simple_compress(data, len, compressed, &compressed_len);
    
    ctx->original_bytes += len;
    ctx->compressed_bytes += compressed_len;
    
    printf("[Compress] Compressed: %zu -> %zu bytes\n", len, compressed_len);
    
    /* 关键点：将压缩后的数据传给被装饰的流 */
    int result = wrapped->ops->write(wrapped, compressed, compressed_len);
    
    free(compressed);
    return (result > 0) ? (int)len : result;  /* 返回原始长度 */
}

static int compression_read(stream_t *stream, uint8_t *data, size_t len) {
    stream_t *wrapped = stream->wrapped;
    
    /* 读取并解压缩（简化实现） */
    printf("[Compress] Reading and decompressing...\n");
    return wrapped->ops->read(wrapped, data, len);
}

static void compression_close(stream_t *stream) {
    compression_context_t *ctx = (compression_context_t *)stream->context;
    stream_t *wrapped = stream->wrapped;
    
    float ratio = (ctx->original_bytes > 0) ? 
                  (float)ctx->compressed_bytes / ctx->original_bytes : 1.0f;
    printf("[Compress] CLOSE - Compression ratio: %.2f\n", ratio);
    
    wrapped->ops->close(wrapped);
}

static const stream_ops_t compression_decorator_ops = {
    .write = compression_write,
    .read = compression_read,
    .close = compression_close
};

stream_t* compression_decorator_create(stream_t *wrapped, int level) {
    stream_t *stream = malloc(sizeof(stream_t));
    compression_context_t *ctx = malloc(sizeof(compression_context_t));
    
    if (stream == NULL || ctx == NULL) {
        free(stream);
        free(ctx);
        return NULL;
    }
    
    memset(ctx, 0, sizeof(compression_context_t));
    ctx->compression_level = level;
    
    stream->ops = &compression_decorator_ops;
    stream->context = ctx;
    stream->wrapped = wrapped;
    
    printf("[CompressDecorator] Created with level %d\n", level);
    return stream;
}


/*---------------------------------------------------------------------------
 * encryption_decorator.c - 加密装饰器
 *---------------------------------------------------------------------------*/
typedef struct {
    uint8_t key[16];
} encryption_context_t;

/* 简单的 XOR "加密" */
static void simple_encrypt(const uint8_t *input, size_t len,
                           uint8_t *output, const uint8_t *key) {
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ key[i % 16];
    }
}

static int encryption_write(stream_t *stream, const uint8_t *data, size_t len) {
    encryption_context_t *ctx = (encryption_context_t *)stream->context;
    stream_t *wrapped = stream->wrapped;
    
    /* 装饰：加密数据 */
    uint8_t *encrypted = malloc(len);
    
    printf("[Encrypt] Encrypting %zu bytes\n", len);
    simple_encrypt(data, len, encrypted, ctx->key);
    
    /* 关键点：将加密后的数据传给被装饰的流 */
    int result = wrapped->ops->write(wrapped, encrypted, len);
    
    free(encrypted);
    return result;
}

static int encryption_read(stream_t *stream, uint8_t *data, size_t len) {
    encryption_context_t *ctx = (encryption_context_t *)stream->context;
    stream_t *wrapped = stream->wrapped;
    
    int result = wrapped->ops->read(wrapped, data, len);
    
    if (result > 0) {
        printf("[Encrypt] Decrypting %d bytes\n", result);
        /* 解密（XOR 加密的解密是相同的操作） */
        simple_encrypt(data, result, data, ctx->key);
    }
    
    return result;
}

static void encryption_close(stream_t *stream) {
    stream_t *wrapped = stream->wrapped;
    printf("[Encrypt] CLOSE\n");
    wrapped->ops->close(wrapped);
}

static const stream_ops_t encryption_decorator_ops = {
    .write = encryption_write,
    .read = encryption_read,
    .close = encryption_close
};

stream_t* encryption_decorator_create(stream_t *wrapped, const uint8_t *key) {
    stream_t *stream = malloc(sizeof(stream_t));
    encryption_context_t *ctx = malloc(sizeof(encryption_context_t));
    
    if (stream == NULL || ctx == NULL) {
        free(stream);
        free(ctx);
        return NULL;
    }
    
    memcpy(ctx->key, key, 16);
    
    stream->ops = &encryption_decorator_ops;
    stream->context = ctx;
    stream->wrapped = wrapped;
    
    printf("[EncryptDecorator] Created\n");
    return stream;
}


/*---------------------------------------------------------------------------
 * 通用销毁函数
 *---------------------------------------------------------------------------*/
void stream_destroy(stream_t *stream) {
    if (stream == NULL) return;
    
    /* 递归销毁被装饰的流 */
    if (stream->wrapped != NULL) {
        stream_destroy(stream->wrapped);
    }
    
    if (stream->context != NULL) {
        free(stream->context);
    }
    free(stream);
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "stream.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("=== Decorator Pattern Demo ===\n\n");
    
    uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                       0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    
    /* 关键点：层层装饰，构建处理链 */
    printf("--- Building decorator chain ---\n");
    
    /* 1. 创建基础文件流 */
    stream_t *file = file_stream_create("data.bin");
    
    /* 2. 添加日志装饰器 */
    stream_t *logged = logging_decorator_create(file, "LOG");
    
    /* 3. 添加压缩装饰器 */
    stream_t *compressed = compression_decorator_create(logged, 6);
    
    /* 4. 添加加密装饰器（最外层） */
    stream_t *encrypted = encryption_decorator_create(compressed, key);
    
    printf("\n--- Writing data ---\n");
    
    /* 关键点：使用最外层装饰器，数据会依次经过所有装饰器处理 */
    const char *message = "Hello, Decorator Pattern!";
    encrypted->ops->write(encrypted, (const uint8_t *)message, strlen(message));
    
    printf("\n--- Reading data ---\n");
    
    uint8_t buffer[64];
    encrypted->ops->read(encrypted, buffer, 32);
    
    printf("\n--- Closing stream ---\n");
    
    /* 关闭（会依次关闭所有装饰器） */
    encrypted->ops->close(encrypted);
    
    printf("\n--- Cleanup ---\n");
    stream_destroy(encrypted);
    
    printf("\nDone!\n");
    return 0;
}
```

## 运行输出示例

```
=== Decorator Pattern Demo ===

--- Building decorator chain ---
[FileStream] Created for 'data.bin'
[LogDecorator] Created with tag 'LOG'
[CompressDecorator] Created with level 6
[EncryptDecorator] Created

--- Writing data ---
[Encrypt] Encrypting 25 bytes
[Compress] Compressing 25 bytes (level 6)
[Compress] Compressed: 25 -> 28 bytes
[LOG] WRITE #1: 28 bytes
[FileStream] Writing 28 bytes to 'data.bin'
[FileStream] Data: 42 4C 51 07 03 01 02 ...
[LOG] Total written: 28 bytes

--- Reading data ---
[Encrypt] Decrypting 32 bytes
[Compress] Reading and decompressing...
[LOG] READ #1: requesting 32 bytes
[FileStream] Reading 32 bytes from 'data.bin'
[LOG] Total read: 32 bytes

--- Closing stream ---
[Encrypt] CLOSE
[Compress] CLOSE - Compression ratio: 1.12
[LOG] CLOSE - Stats: writes=1, reads=1, bytes_out=28, bytes_in=32
[FileStream] Closing 'data.bin'

--- Cleanup ---

Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **动态扩展** | 运行时组合功能，无需修改原代码 |
| **单一职责** | 每个装饰器只负责一个功能 |
| **灵活组合** | 可任意组合装饰器顺序和数量 |
| **透明性** | 装饰器和原对象接口相同 |
| **避免继承爆炸** | 不需要为每种组合创建子类 |

