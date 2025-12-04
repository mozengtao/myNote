# 防御式编程 (Defensive Programming)

## 定义

防御式编程是一种编程实践，通过在代码中添加多层检查和保护措施，使程序在面对无效输入、意外情况或编程错误时能够安全地失败或正常工作，而不是产生未定义行为或崩溃。

## 适用场景

- 处理外部输入（用户输入、文件、网络数据）
- API边界的参数验证
- 安全关键系统
- 多线程和并发代码
- 内存操作和缓冲区处理
- 长期运行的服务程序
- 需要高可靠性的嵌入式系统

## ASCII 图解

```
+------------------------------------------------------------------------+
|                      DEFENSIVE PROGRAMMING                              |
+------------------------------------------------------------------------+
|                                                                         |
|   DEFENSE LAYERS:                                                       |
|                                                                         |
|   +--------------------+                                                |
|   | External Input     | <-- Untrusted data from outside               |
|   +--------+-----------+                                                |
|            |                                                            |
|            v                                                            |
|   +--------+-----------+                                                |
|   | INPUT VALIDATION   | Layer 1: Validate all inputs                  |
|   | - NULL checks      |          - Type checking                       |
|   | - Range checking   |          - Format validation                   |
|   +--------+-----------+                                                |
|            |                                                            |
|            v                                                            |
|   +--------+-----------+                                                |
|   | BOUNDS CHECKING    | Layer 2: Prevent buffer overflows             |
|   | - Array bounds     |          - Safe string operations              |
|   | - Size limits      |          - Memory boundaries                   |
|   +--------+-----------+                                                |
|            |                                                            |
|            v                                                            |
|   +--------+-----------+                                                |
|   | STATE VALIDATION   | Layer 3: Verify object state                  |
|   | - Magic numbers    |          - Initialization check                |
|   | - Invariants       |          - Valid transitions                   |
|   +--------+-----------+                                                |
|            |                                                            |
|            v                                                            |
|   +--------+-----------+                                                |
|   | ERROR HANDLING     | Layer 4: Graceful failure                     |
|   | - Return codes     |          - Logging                             |
|   | - Recovery         |          - Cleanup                             |
|   +--------+-----------+                                                |
|            |                                                            |
|            v                                                            |
|   +--------------------+                                                |
|   | Safe Operation     | Trusted internal processing                   |
|   +--------------------+                                                |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了防御式编程的多层防护策略。外部数据首先经过输入验证（检查NULL、范围、格式）。然后进行边界检查（防止缓冲区溢出）。接着验证对象状态（通过魔数和不变式）。最后是错误处理层，确保失败时能安全清理。只有通过所有层的数据才能进入核心处理逻辑。

## 实现方法

1. 所有外部输入都要验证
2. 使用安全的字符串和内存操作函数
3. 检查整数运算溢出
4. 使用魔数验证结构体有效性
5. 记录详细日志便于调试

## C语言代码示例

### 安全操作函数

```c
// safe_ops.h
#ifndef SAFE_OPS_H
#define SAFE_OPS_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

// ==================== 安全字符串操作 ====================

// 安全复制，保证null终止
static inline size_t safe_strcpy(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return 0;
    if (!src) { dest[0] = '\0'; return 0; }
    
    size_t i;
    for (i = 0; i < dest_size - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return i;
}

// 安全拼接
static inline size_t safe_strcat(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return 0;
    size_t dest_len = strnlen(dest, dest_size);
    if (dest_len >= dest_size - 1) return dest_len;
    return dest_len + safe_strcpy(dest + dest_len, dest_size - dest_len, src);
}

// ==================== 安全整数运算 ====================

// 检测加法溢出
static inline int safe_add_int(int a, int b, int* result) {
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        return -1;  // 溢出
    }
    *result = a + b;
    return 0;
}

// 检测乘法溢出
static inline int safe_mul_int(int a, int b, int* result) {
    if (a > 0 && b > 0 && a > INT_MAX / b) return -1;
    if (a > 0 && b < 0 && b < INT_MIN / a) return -1;
    if (a < 0 && b > 0 && a < INT_MIN / b) return -1;
    if (a < 0 && b < 0 && a < INT_MAX / b) return -1;
    *result = a * b;
    return 0;
}

// 安全数组索引
#define SAFE_INDEX(arr, idx, arr_size, default_val) \
    (((idx) >= 0 && (size_t)(idx) < (arr_size)) ? (arr)[idx] : (default_val))

// ==================== 参数验证宏 ====================

#define VALIDATE_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "[ERROR] NULL pointer: %s at %s:%d\n", \
                #ptr, __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#define VALIDATE_RANGE(val, min, max) do { \
    if ((val) < (min) || (val) > (max)) { \
        fprintf(stderr, "[ERROR] %s=%d out of range [%d,%d] at %s:%d\n", \
                #val, (int)(val), (int)(min), (int)(max), __FILE__, __LINE__); \
        return -1; \
    } \
} while(0)

#endif // SAFE_OPS_H
```

### 带魔数验证的安全结构

```c
// safe_buffer.h
#ifndef SAFE_BUFFER_H
#define SAFE_BUFFER_H

#include "safe_ops.h"
#include <stdlib.h>

#define BUFFER_MAGIC 0xDEADBEEF
#define BUFFER_FREED 0xFEEDFACE

typedef struct {
    uint32_t magic;      // 魔数验证
    char* data;
    size_t size;
    size_t capacity;
    uint32_t checksum;   // 完整性校验
} SafeBuffer;

// 计算校验和
static inline uint32_t buffer_checksum(SafeBuffer* buf) {
    return (uint32_t)(buf->magic ^ buf->size ^ buf->capacity ^ (uintptr_t)buf->data);
}

// 验证缓冲区有效性
static inline int buffer_is_valid(SafeBuffer* buf) {
    if (!buf) return 0;
    if (buf->magic != BUFFER_MAGIC) return 0;
    if (buf->checksum != buffer_checksum(buf)) return 0;
    if (buf->size > buf->capacity) return 0;
    return 1;
}

// 创建缓冲区
static inline SafeBuffer* buffer_create(size_t capacity) {
    if (capacity == 0 || capacity > SIZE_MAX / 2) return NULL;
    
    SafeBuffer* buf = (SafeBuffer*)calloc(1, sizeof(SafeBuffer));
    if (!buf) return NULL;
    
    buf->data = (char*)malloc(capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    
    buf->magic = BUFFER_MAGIC;
    buf->size = 0;
    buf->capacity = capacity;
    buf->checksum = buffer_checksum(buf);
    
    return buf;
}

// 安全写入
static inline int buffer_write(SafeBuffer* buf, const void* data, size_t len) {
    if (!buffer_is_valid(buf)) {
        fprintf(stderr, "[ERROR] Invalid buffer\n");
        return -1;
    }
    if (!data || len == 0) return 0;
    
    // 边界检查
    if (len > buf->capacity - buf->size) {
        fprintf(stderr, "[ERROR] Buffer overflow prevented\n");
        return -1;
    }
    
    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->checksum = buffer_checksum(buf);
    
    return (int)len;
}

// 安全销毁
static inline void buffer_destroy(SafeBuffer* buf) {
    if (!buf) return;
    
    if (buf->magic == BUFFER_FREED) {
        fprintf(stderr, "[ERROR] Double free detected!\n");
        return;
    }
    
    if (buf->magic == BUFFER_MAGIC && buf->data) {
        memset(buf->data, 0, buf->capacity);  // 清除敏感数据
        free(buf->data);
    }
    
    buf->magic = BUFFER_FREED;  // 标记已释放
    free(buf);
}

#endif // SAFE_BUFFER_H
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "safe_ops.h"
#include "safe_buffer.h"

// 带防御的函数示例
int process_data(const char* input, char* output, size_t output_size) {
    // 层1: 输入验证
    VALIDATE_NOT_NULL(input);
    VALIDATE_NOT_NULL(output);
    VALIDATE_RANGE(output_size, 1, 10240);
    
    // 层2: 安全操作
    size_t copied = safe_strcpy(output, output_size, input);
    
    printf("Processed %zu characters\n", copied);
    return 0;
}

int main() {
    printf("=== Defensive Programming Demo ===\n\n");
    
    // 安全字符串操作
    printf("--- Safe String Operations ---\n");
    char small_buf[10];
    safe_strcpy(small_buf, sizeof(small_buf), "Hello, World!");
    printf("Truncated: '%s'\n", small_buf);
    
    // 安全整数运算
    printf("\n--- Safe Integer Operations ---\n");
    int result;
    if (safe_add_int(INT_MAX, 1, &result) != 0) {
        printf("Overflow detected in addition!\n");
    }
    if (safe_mul_int(100000, 100000, &result) != 0) {
        printf("Overflow detected in multiplication!\n");
    }
    
    // 安全缓冲区
    printf("\n--- Safe Buffer ---\n");
    SafeBuffer* buf = buffer_create(100);
    if (buf) {
        buffer_write(buf, "Hello", 5);
        buffer_write(buf, " World", 6);
        printf("Buffer size: %zu/%zu\n", buf->size, buf->capacity);
        
        // 尝试溢出
        char overflow[200];
        memset(overflow, 'A', sizeof(overflow));
        if (buffer_write(buf, overflow, sizeof(overflow)) < 0) {
            printf("Overflow prevented!\n");
        }
        
        buffer_destroy(buf);
        
        // 双重释放检测
        buffer_destroy(buf);  // 会检测到错误
    }
    
    // 参数验证
    printf("\n--- Parameter Validation ---\n");
    char output[50];
    process_data("Test input", output, sizeof(output));
    process_data(NULL, output, sizeof(output));  // 会报错
    
    printf("\n=== Demo complete ===\n");
    return 0;
}
```

## 优缺点

### 优点
- **提高健壮性**：程序能处理意外情况
- **安全性**：防止缓冲区溢出等漏洞
- **易于调试**：详细的错误信息
- **防止数据损坏**：通过校验和检测
- **检测编程错误**：如双重释放

### 缺点
- 代码量增加
- 性能开销（检查需要时间）
- 可能掩盖真正的bug
- 需要权衡检查的粒度

