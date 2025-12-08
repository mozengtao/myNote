# Decorator Pattern (装饰器模式)

## 1. Core Concept and Use Cases

### Core Concept
Attach **additional responsibilities** to an object dynamically. Decorators provide a flexible alternative to subclassing for extending functionality.

### Typical Use Cases
- I/O stream wrapping (buffering, encryption, compression)
- Logging wrappers
- Authentication/authorization layers
- Caching layers
- Data transformation pipelines

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                     Decorator Pattern                                             |
+--------------------------------------------------------------------------------------------------+

                              +---------------------------+
                              |    Component Interface    |
                              +---------------------------+
                              | + operation()             |
                              +-------------+-------------+
                                            |
                      +---------------------+---------------------+
                      |                                           |
                      v                                           v
         +------------------------+                  +------------------------+
         |   Concrete Component   |                  |   Base Decorator       |
         +------------------------+                  +------------------------+
         | + operation()          |                  | - wrapped: Component   |
         +------------------------+                  +------------------------+
                                                     | + operation() {        |
                                                     |     wrapped.operation()|
                                                     | }                      |
                                                     +------------+-----------+
                                                                  |
                              +-----------------------------------+-----------------------------------+
                              |                                   |                                   |
                              v                                   v                                   v
                 +------------------------+          +------------------------+          +------------------------+
                 |   Decorator A          |          |   Decorator B          |          |   Decorator C          |
                 |   (e.g., Logging)      |          |   (e.g., Encryption)   |          |   (e.g., Compression)  |
                 +------------------------+          +------------------------+          +------------------------+
                 | + operation() {        |          | + operation() {        |          | + operation() {        |
                 |     log("before");     |          |     data = encrypt();  |          |     data = compress(); |
                 |     wrapped.operation()|          |     wrapped.operation()|          |     wrapped.operation()|
                 |     log("after");      |          | }                      |          | }                      |
                 | }                      |          +------------------------+          +------------------------+
                 +------------------------+


    Wrapping Example:
    
    +-------------+     +-------------+     +-------------+     +-------------+
    | Compression | --> | Encryption  | --> |   Logging   | --> |   Base      |
    |  Decorator  |     |  Decorator  |     |  Decorator  |     |  Component  |
    +-------------+     +-------------+     +-------------+     +-------------+
          |                   |                   |                   |
          v                   v                   v                   v
       compress           encrypt              log              actual work
```

**中文说明：**

装饰器模式的核心流程：

1. **组件接口（Component）**：
   - 定义被装饰对象的接口
   - 所有装饰器和具体组件都实现此接口

2. **具体组件（Concrete Component）**：
   - 实际执行工作的对象
   - 不知道装饰器的存在

3. **装饰器（Decorator）**：
   - 包装一个组件
   - 在调用被包装组件前后添加功能
   - 可以层层嵌套

---

## 3. Code Skeleton

```c
/* Component interface */
typedef struct Component {
    int (*read)(struct Component* self, char* buf, int len);
    int (*write)(struct Component* self, const char* buf, int len);
    void (*close)(struct Component* self);
    struct Component* wrapped;  /* Decorated component */
} Component;

/* Create decorator that wraps a component */
Component* create_logging_decorator(Component* wrapped);
Component* create_encryption_decorator(Component* wrapped);
Component* create_compression_decorator(Component* wrapped);
```

**中文说明：**

代码骨架包含：
- `Component`：组件接口，包含操作函数指针和被包装组件
- 各种装饰器创建函数
- 每个装饰器增强特定功能

---

## 4. Complete Example Code

```c
/*
 * Decorator Pattern - Data Stream Example
 * 
 * This example demonstrates a data stream that can be
 * decorated with logging, encryption, and compression.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BUFFER_SIZE 1024

/* ============================================
 * Component Interface (Base Stream)
 * ============================================ */
typedef struct Stream Stream;

struct Stream {
    char name[32];                                          /* Stream name */
    int (*write)(Stream* self, const char* data, int len);  /* Write operation */
    int (*read)(Stream* self, char* buf, int max_len);      /* Read operation */
    void (*close)(Stream* self);                            /* Close and cleanup */
    Stream* wrapped;                                         /* Wrapped stream (for decorators) */
    void* private_data;                                      /* Stream-specific data */
};

/* ============================================
 * Concrete Component: File Stream
 * ============================================ */
typedef struct {
    char filename[128];
    char buffer[MAX_BUFFER_SIZE];
    int buffer_len;
} FileStreamData;

int file_stream_write(Stream* self, const char* data, int len)
{
    FileStreamData* fs = (FileStreamData*)self->private_data;
    
    /* Simulate writing to file by storing in buffer */
    if (fs->buffer_len + len < MAX_BUFFER_SIZE) {
        memcpy(&fs->buffer[fs->buffer_len], data, len);
        fs->buffer_len += len;
        printf("    [FileStream] Wrote %d bytes to %s\n", len, fs->filename);
        return len;
    }
    return -1;
}

int file_stream_read(Stream* self, char* buf, int max_len)
{
    FileStreamData* fs = (FileStreamData*)self->private_data;
    
    int to_read = (fs->buffer_len < max_len) ? fs->buffer_len : max_len;
    memcpy(buf, fs->buffer, to_read);
    printf("    [FileStream] Read %d bytes from %s\n", to_read, fs->filename);
    return to_read;
}

void file_stream_close(Stream* self)
{
    printf("    [FileStream] Closing stream\n");
    if (self->private_data) {
        free(self->private_data);
    }
    free(self);
}

Stream* create_file_stream(const char* filename)
{
    Stream* stream = (Stream*)malloc(sizeof(Stream));
    FileStreamData* data = (FileStreamData*)malloc(sizeof(FileStreamData));
    
    if (stream == NULL || data == NULL) {
        free(stream);
        free(data);
        return NULL;
    }
    
    strncpy(stream->name, "FileStream", sizeof(stream->name) - 1);
    stream->write = file_stream_write;
    stream->read = file_stream_read;
    stream->close = file_stream_close;
    stream->wrapped = NULL;
    stream->private_data = data;
    
    strncpy(data->filename, filename, sizeof(data->filename) - 1);
    memset(data->buffer, 0, MAX_BUFFER_SIZE);
    data->buffer_len = 0;
    
    printf("[Create] FileStream for %s\n", filename);
    return stream;
}

/* ============================================
 * Decorator 1: Logging Decorator
 * Adds logging before and after operations
 * ============================================ */
typedef struct {
    int read_count;
    int write_count;
    int total_bytes_read;
    int total_bytes_written;
} LoggingData;

int logging_write(Stream* self, const char* data, int len)
{
    LoggingData* log = (LoggingData*)self->private_data;
    
    log->write_count++;
    printf("  [LoggingDecorator] WRITE #%d: %d bytes\n", log->write_count, len);
    
    /* Call wrapped stream's write */
    int result = self->wrapped->write(self->wrapped, data, len);
    
    if (result > 0) {
        log->total_bytes_written += result;
        printf("  [LoggingDecorator] WRITE complete. Total written: %d bytes\n",
               log->total_bytes_written);
    }
    
    return result;
}

int logging_read(Stream* self, char* buf, int max_len)
{
    LoggingData* log = (LoggingData*)self->private_data;
    
    log->read_count++;
    printf("  [LoggingDecorator] READ #%d: requesting %d bytes\n",
           log->read_count, max_len);
    
    /* Call wrapped stream's read */
    int result = self->wrapped->read(self->wrapped, buf, max_len);
    
    if (result > 0) {
        log->total_bytes_read += result;
        printf("  [LoggingDecorator] READ complete. Total read: %d bytes\n",
               log->total_bytes_read);
    }
    
    return result;
}

void logging_close(Stream* self)
{
    LoggingData* log = (LoggingData*)self->private_data;
    
    printf("  [LoggingDecorator] Closing. Stats: %d reads, %d writes\n",
           log->read_count, log->write_count);
    
    /* Close wrapped stream first */
    if (self->wrapped) {
        self->wrapped->close(self->wrapped);
    }
    
    free(self->private_data);
    free(self);
}

/* Create logging decorator - wraps another stream */
Stream* create_logging_decorator(Stream* wrapped)
{
    Stream* stream = (Stream*)malloc(sizeof(Stream));
    LoggingData* data = (LoggingData*)malloc(sizeof(LoggingData));
    
    if (stream == NULL || data == NULL) {
        free(stream);
        free(data);
        return NULL;
    }
    
    strncpy(stream->name, "LoggingDecorator", sizeof(stream->name) - 1);
    stream->write = logging_write;
    stream->read = logging_read;
    stream->close = logging_close;
    stream->wrapped = wrapped;              /* Store the wrapped stream */
    stream->private_data = data;
    
    memset(data, 0, sizeof(LoggingData));
    
    printf("[Create] LoggingDecorator wrapping %s\n", wrapped->name);
    return stream;
}

/* ============================================
 * Decorator 2: Encryption Decorator
 * Encrypts data on write, decrypts on read
 * ============================================ */
typedef struct {
    char key;  /* Simple XOR key for demo */
} EncryptionData;

/* Simple XOR encryption for demonstration */
static void xor_transform(char* data, int len, char key)
{
    for (int i = 0; i < len; i++) {
        data[i] ^= key;
    }
}

int encryption_write(Stream* self, const char* data, int len)
{
    EncryptionData* enc = (EncryptionData*)self->private_data;
    
    /* Create encrypted copy */
    char* encrypted = (char*)malloc(len);
    if (encrypted == NULL) return -1;
    
    memcpy(encrypted, data, len);
    xor_transform(encrypted, len, enc->key);
    
    printf("  [EncryptionDecorator] Encrypting %d bytes\n", len);
    
    /* Write encrypted data to wrapped stream */
    int result = self->wrapped->write(self->wrapped, encrypted, len);
    
    free(encrypted);
    return result;
}

int encryption_read(Stream* self, char* buf, int max_len)
{
    EncryptionData* enc = (EncryptionData*)self->private_data;
    
    /* Read from wrapped stream */
    int result = self->wrapped->read(self->wrapped, buf, max_len);
    
    if (result > 0) {
        /* Decrypt the data */
        xor_transform(buf, result, enc->key);
        printf("  [EncryptionDecorator] Decrypted %d bytes\n", result);
    }
    
    return result;
}

void encryption_close(Stream* self)
{
    printf("  [EncryptionDecorator] Closing\n");
    
    if (self->wrapped) {
        self->wrapped->close(self->wrapped);
    }
    
    free(self->private_data);
    free(self);
}

Stream* create_encryption_decorator(Stream* wrapped, char key)
{
    Stream* stream = (Stream*)malloc(sizeof(Stream));
    EncryptionData* data = (EncryptionData*)malloc(sizeof(EncryptionData));
    
    if (stream == NULL || data == NULL) {
        free(stream);
        free(data);
        return NULL;
    }
    
    strncpy(stream->name, "EncryptionDecorator", sizeof(stream->name) - 1);
    stream->write = encryption_write;
    stream->read = encryption_read;
    stream->close = encryption_close;
    stream->wrapped = wrapped;
    stream->private_data = data;
    
    data->key = key;
    
    printf("[Create] EncryptionDecorator wrapping %s (key=0x%02X)\n",
           wrapped->name, (unsigned char)key);
    return stream;
}

/* ============================================
 * Decorator 3: Compression Decorator
 * Simulates compression on write, decompression on read
 * ============================================ */
typedef struct {
    int compression_ratio;  /* Simulated ratio */
} CompressionData;

int compression_write(Stream* self, const char* data, int len)
{
    CompressionData* comp = (CompressionData*)self->private_data;
    
    /* Simulate compression by adding header */
    char header[32];
    snprintf(header, sizeof(header), "[COMPRESSED:%d]", len);
    int header_len = strlen(header);
    
    char* compressed = (char*)malloc(header_len + len);
    if (compressed == NULL) return -1;
    
    memcpy(compressed, header, header_len);
    memcpy(compressed + header_len, data, len);
    
    printf("  [CompressionDecorator] Compressing %d bytes\n", len);
    
    int result = self->wrapped->write(self->wrapped, compressed, header_len + len);
    
    free(compressed);
    return (result > 0) ? len : -1;
}

int compression_read(Stream* self, char* buf, int max_len)
{
    char temp[MAX_BUFFER_SIZE];
    
    /* Read from wrapped stream */
    int result = self->wrapped->read(self->wrapped, temp, sizeof(temp));
    
    if (result > 0) {
        /* Skip compression header in simulation */
        char* data_start = strchr(temp, ']');
        if (data_start) {
            data_start++;  /* Skip the ']' */
            int data_len = result - (data_start - temp);
            if (data_len > max_len) data_len = max_len;
            memcpy(buf, data_start, data_len);
            printf("  [CompressionDecorator] Decompressed %d bytes\n", data_len);
            return data_len;
        }
    }
    
    return result;
}

void compression_close(Stream* self)
{
    printf("  [CompressionDecorator] Closing\n");
    
    if (self->wrapped) {
        self->wrapped->close(self->wrapped);
    }
    
    free(self->private_data);
    free(self);
}

Stream* create_compression_decorator(Stream* wrapped)
{
    Stream* stream = (Stream*)malloc(sizeof(Stream));
    CompressionData* data = (CompressionData*)malloc(sizeof(CompressionData));
    
    if (stream == NULL || data == NULL) {
        free(stream);
        free(data);
        return NULL;
    }
    
    strncpy(stream->name, "CompressionDecorator", sizeof(stream->name) - 1);
    stream->write = compression_write;
    stream->read = compression_read;
    stream->close = compression_close;
    stream->wrapped = wrapped;
    stream->private_data = data;
    
    data->compression_ratio = 50;  /* Simulated 50% compression */
    
    printf("[Create] CompressionDecorator wrapping %s\n", wrapped->name);
    return stream;
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    char read_buffer[MAX_BUFFER_SIZE];
    const char* test_data = "Hello, Decorator Pattern!";
    
    printf("=== Decorator Pattern Demo ===\n\n");
    
    /* Example 1: Plain file stream */
    printf("--- Example 1: Plain FileStream ---\n");
    Stream* plain_stream = create_file_stream("plain.txt");
    
    plain_stream->write(plain_stream, test_data, strlen(test_data));
    
    memset(read_buffer, 0, sizeof(read_buffer));
    plain_stream->read(plain_stream, read_buffer, sizeof(read_buffer));
    printf("Read data: \"%s\"\n", read_buffer);
    
    plain_stream->close(plain_stream);
    
    /* Example 2: Decorated stream (Logging + Encryption) */
    printf("\n--- Example 2: Logging + Encryption ---\n");
    Stream* file_stream2 = create_file_stream("encrypted.txt");
    Stream* encrypted_stream = create_encryption_decorator(file_stream2, 0x42);
    Stream* logged_stream = create_logging_decorator(encrypted_stream);
    
    /* Write through decorated stream */
    printf("\nWriting through decorated stream:\n");
    logged_stream->write(logged_stream, test_data, strlen(test_data));
    
    /* Read through decorated stream */
    printf("\nReading through decorated stream:\n");
    memset(read_buffer, 0, sizeof(read_buffer));
    logged_stream->read(logged_stream, read_buffer, sizeof(read_buffer));
    printf("Read data: \"%s\"\n", read_buffer);
    
    printf("\nClosing decorated stream:\n");
    logged_stream->close(logged_stream);
    
    /* Example 3: Multiple decorators (Compression + Encryption + Logging) */
    printf("\n--- Example 3: Compression + Encryption + Logging ---\n");
    Stream* file_stream3 = create_file_stream("full.txt");
    Stream* compressed = create_compression_decorator(file_stream3);
    Stream* encrypted = create_encryption_decorator(compressed, 0x5A);
    Stream* logged = create_logging_decorator(encrypted);
    
    printf("\nWriting through triple-decorated stream:\n");
    logged->write(logged, test_data, strlen(test_data));
    
    printf("\nReading through triple-decorated stream:\n");
    memset(read_buffer, 0, sizeof(read_buffer));
    logged->read(logged, read_buffer, sizeof(read_buffer));
    printf("Read data: \"%s\"\n", read_buffer);
    
    printf("\nClosing triple-decorated stream:\n");
    logged->close(logged);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了一个可装饰的数据流：

1. **基础组件（FileStream）**：
   - 实际执行读写操作
   - 模拟文件存储

2. **装饰器实现**：
   - **LoggingDecorator**：记录读写操作的日志和统计
   - **EncryptionDecorator**：写入时加密，读取时解密
   - **CompressionDecorator**：写入时压缩，读取时解压

3. **装饰器嵌套**：
   - 可以任意组合装饰器
   - 调用顺序：外层装饰器 → 内层装饰器 → 基础组件
   - 关闭时自动级联关闭所有层

4. **优势体现**：
   - 动态添加功能，无需修改基础组件
   - 功能组合灵活，可按需堆叠
   - 符合开闭原则

