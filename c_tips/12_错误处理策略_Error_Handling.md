# 错误处理策略 (Error Handling Strategies)

## 定义

错误处理策略是一套在C语言中检测、报告和恢复错误的系统性方法。由于C语言没有内置的异常机制，程序员需要选择合适的错误处理模式来构建健壮的程序。

## 适用场景

- 系统编程和驱动开发
- 库和API设计
- 需要精细控制错误流程的应用
- 资源管理密集的程序
- 需要详细错误信息的诊断系统
- 跨平台代码开发

## ASCII 图解

```
+------------------------------------------------------------------------+
|                    ERROR HANDLING STRATEGIES                            |
+------------------------------------------------------------------------+
|                                                                         |
|   Strategy 1: RETURN CODES                                              |
|   +--------------------+       +--------------------+                   |
|   |    Function        |       |     Caller         |                   |
|   +--------------------+       +--------------------+                   |
|   | int do_something() |       | int ret = do_...();|                   |
|   | {                  |       | if (ret < 0) {     |                   |
|   |   if (error)       |------>|   handle_error();  |                   |
|   |     return -1;     |       | }                  |                   |
|   |   return 0;        |       +--------------------+                   |
|   | }                  |                                                |
|   +--------------------+   Simple but error-prone (easy to ignore)      |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Strategy 2: OUTPUT PARAMETER                                          |
|   +--------------------+       +--------------------+                   |
|   |    Function        |       |     Caller         |                   |
|   +--------------------+       +--------------------+                   |
|   | Result func(       |       | Error err;         |                   |
|   |   Error* err) {    |       | Result r = func(&e)|                   |
|   |   if (problem) {   |------>| if (err.code) {    |                   |
|   |     err->code=E_X; |       |   log(err.msg);    |                   |
|   |     err->msg=".."  |       | }                  |                   |
|   |   }                |       +--------------------+                   |
|   | }                  |   Carries detailed error info                  |
|   +--------------------+                                                |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Strategy 3: ERRNO PATTERN (Thread-Local)                              |
|   +--------------------+       +--------------------+                   |
|   |    Function        |       |     Caller         |                   |
|   +--------------------+       +--------------------+                   |
|   | int func() {       |       | errno = 0;         |                   |
|   |   if (error) {     |       | int r = func();    |                   |
|   |     errno = ENOENT;|------>| if (r < 0) {       |                   |
|   |     return -1;     |       |   perror("func");  |                   |
|   |   }                |       | }                  |                   |
|   | }                  |       +--------------------+                   |
|   +--------------------+   Standard C pattern, thread-safe              |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Strategy 4: GOTO CLEANUP                                              |
|                                                                         |
|   int complex_func() {                                                  |
|       +------------------+                                              |
|       | alloc resource A |----> if fail, goto cleanup_a                 |
|       +--------+---------+                                              |
|                |                                                        |
|       +--------v---------+                                              |
|       | alloc resource B |----> if fail, goto cleanup_b                 |
|       +--------+---------+                                              |
|                |                                                        |
|       +--------v---------+                                              |
|       | alloc resource C |----> if fail, goto cleanup_c                 |
|       +--------+---------+                                              |
|                |                                                        |
|       +--------v---------+                                              |
|       | do actual work   |                                              |
|       +--------+---------+                                              |
|                |                                                        |
|   cleanup_c:   | free(C)   <----+                                       |
|   cleanup_b:   | free(B)   <----+---- Centralized cleanup               |
|   cleanup_a:   | free(A)   <----+                                       |
|       return result;                                                    |
|   }                                                                     |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了C语言中四种主要的错误处理策略。返回码方式简单直接，但容易被忽略。输出参数方式可以携带详细错误信息。errno模式是标准C库采用的方式，支持线程局部存储。goto cleanup模式适合需要释放多个资源的复杂函数，通过集中清理避免代码重复和资源泄漏。

## 实现方法

1. 定义统一的错误码枚举
2. 创建详细的错误结构体
3. 提供错误信息查询函数
4. 使用宏简化错误检查
5. 选择适合场景的处理策略

## C语言代码示例

### 错误系统基础设施

```c
// error_system.h
#ifndef ERROR_SYSTEM_H
#define ERROR_SYSTEM_H

#include <stddef.h>

// ==================== 错误码定义 ====================

typedef enum {
    ERR_OK = 0,
    ERR_UNKNOWN = -1,
    ERR_NULL_PTR = -2,
    ERR_INVALID_ARG = -3,
    ERR_OUT_OF_MEMORY = -4,
    ERR_BUFFER_OVERFLOW = -5,
    ERR_FILE_NOT_FOUND = -6,
    ERR_FILE_READ = -7,
    ERR_FILE_WRITE = -8,
    ERR_PERMISSION = -9,
    ERR_TIMEOUT = -10,
    ERR_BUSY = -11,
    ERR_NOT_SUPPORTED = -12,
    ERR_ALREADY_EXISTS = -13,
    ERR_NOT_INITIALIZED = -14,
    ERR_CONNECTION = -15
} ErrorCode;

// ==================== 详细错误结构 ====================

typedef struct {
    ErrorCode code;
    const char* message;
    const char* file;
    int line;
    const char* function;
} ErrorInfo;

// 初始化错误结构
#define ERROR_INFO_INIT { ERR_OK, NULL, NULL, 0, NULL }

// 设置错误信息的宏
#define SET_ERROR(err, c, msg) do {     \
    if (err) {                          \
        (err)->code = (c);              \
        (err)->message = (msg);         \
        (err)->file = __FILE__;         \
        (err)->line = __LINE__;         \
        (err)->function = __func__;     \
    }                                   \
} while(0)

// 清除错误
#define CLEAR_ERROR(err) do {           \
    if (err) {                          \
        (err)->code = ERR_OK;           \
        (err)->message = NULL;          \
    }                                   \
} while(0)

// 检查并返回错误
#define RETURN_IF_ERROR(err) do {       \
    if ((err) && (err)->code != ERR_OK) \
        return (err)->code;             \
} while(0)

// 检查条件，失败则设置错误并返回
#define CHECK_ERROR(cond, err, code, msg, ret) do { \
    if (!(cond)) {                                  \
        SET_ERROR(err, code, msg);                  \
        return ret;                                 \
    }                                               \
} while(0)

// ==================== API声明 ====================

const char* error_code_to_string(ErrorCode code);
const char* error_code_to_description(ErrorCode code);
void error_print(const ErrorInfo* err);
void error_log(const ErrorInfo* err);

#endif // ERROR_SYSTEM_H
```

```c
// error_system.c
#include "error_system.h"
#include <stdio.h>
#include <time.h>

// 错误码到名称的映射
static const struct {
    ErrorCode code;
    const char* name;
    const char* description;
} error_table[] = {
    { ERR_OK,             "ERR_OK",             "Success" },
    { ERR_UNKNOWN,        "ERR_UNKNOWN",        "Unknown error" },
    { ERR_NULL_PTR,       "ERR_NULL_PTR",       "Null pointer" },
    { ERR_INVALID_ARG,    "ERR_INVALID_ARG",    "Invalid argument" },
    { ERR_OUT_OF_MEMORY,  "ERR_OUT_OF_MEMORY",  "Out of memory" },
    { ERR_BUFFER_OVERFLOW,"ERR_BUFFER_OVERFLOW","Buffer overflow" },
    { ERR_FILE_NOT_FOUND, "ERR_FILE_NOT_FOUND", "File not found" },
    { ERR_FILE_READ,      "ERR_FILE_READ",      "File read error" },
    { ERR_FILE_WRITE,     "ERR_FILE_WRITE",     "File write error" },
    { ERR_PERMISSION,     "ERR_PERMISSION",     "Permission denied" },
    { ERR_TIMEOUT,        "ERR_TIMEOUT",        "Operation timeout" },
    { ERR_BUSY,           "ERR_BUSY",           "Resource busy" },
    { ERR_NOT_SUPPORTED,  "ERR_NOT_SUPPORTED",  "Not supported" },
    { ERR_ALREADY_EXISTS, "ERR_ALREADY_EXISTS", "Already exists" },
    { ERR_NOT_INITIALIZED,"ERR_NOT_INITIALIZED","Not initialized" },
    { ERR_CONNECTION,     "ERR_CONNECTION",     "Connection failed" },
};

static const size_t error_table_size = sizeof(error_table) / sizeof(error_table[0]);

const char* error_code_to_string(ErrorCode code) {
    for (size_t i = 0; i < error_table_size; i++) {
        if (error_table[i].code == code) {
            return error_table[i].name;
        }
    }
    return "ERR_UNKNOWN";
}

const char* error_code_to_description(ErrorCode code) {
    for (size_t i = 0; i < error_table_size; i++) {
        if (error_table[i].code == code) {
            return error_table[i].description;
        }
    }
    return "Unknown error";
}

void error_print(const ErrorInfo* err) {
    if (!err) return;
    
    if (err->code == ERR_OK) {
        printf("[OK] No error\n");
        return;
    }
    
    printf("[ERROR] %s (%d): %s\n",
           error_code_to_string(err->code),
           err->code,
           err->message ? err->message : error_code_to_description(err->code));
    
    if (err->file) {
        printf("        at %s:%d in %s()\n",
               err->file, err->line,
               err->function ? err->function : "unknown");
    }
}

void error_log(const ErrorInfo* err) {
    if (!err || err->code == ERR_OK) return;
    
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t);
    
    fprintf(stderr, "[%s] ERROR %s (%d): %s | %s:%d %s()\n",
            time_buf,
            error_code_to_string(err->code),
            err->code,
            err->message ? err->message : "",
            err->file ? err->file : "?",
            err->line,
            err->function ? err->function : "?");
}
```

### Goto Cleanup 模式

```c
// file_processor.c
#include "error_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 使用goto cleanup模式处理多资源操作
ErrorCode process_file(const char* input_path, 
                       const char* output_path,
                       ErrorInfo* err) {
    ErrorCode result = ERR_UNKNOWN;
    FILE* input = NULL;
    FILE* output = NULL;
    char* buffer = NULL;
    
    // 参数检查
    CHECK_ERROR(input_path != NULL, err, ERR_NULL_PTR, 
                "input_path is NULL", ERR_NULL_PTR);
    CHECK_ERROR(output_path != NULL, err, ERR_NULL_PTR,
                "output_path is NULL", ERR_NULL_PTR);
    
    // 打开输入文件
    input = fopen(input_path, "r");
    if (!input) {
        SET_ERROR(err, ERR_FILE_NOT_FOUND, "Cannot open input file");
        result = ERR_FILE_NOT_FOUND;
        goto cleanup;
    }
    
    // 打开输出文件
    output = fopen(output_path, "w");
    if (!output) {
        SET_ERROR(err, ERR_FILE_WRITE, "Cannot create output file");
        result = ERR_FILE_WRITE;
        goto cleanup;
    }
    
    // 分配缓冲区
    buffer = (char*)malloc(4096);
    if (!buffer) {
        SET_ERROR(err, ERR_OUT_OF_MEMORY, "Cannot allocate buffer");
        result = ERR_OUT_OF_MEMORY;
        goto cleanup;
    }
    
    // 处理文件
    while (fgets(buffer, 4096, input)) {
        // 转换为大写（示例处理）
        for (char* p = buffer; *p; p++) {
            if (*p >= 'a' && *p <= 'z') {
                *p = *p - 'a' + 'A';
            }
        }
        
        if (fputs(buffer, output) == EOF) {
            SET_ERROR(err, ERR_FILE_WRITE, "Write failed");
            result = ERR_FILE_WRITE;
            goto cleanup;
        }
    }
    
    // 检查读取是否有错误
    if (ferror(input)) {
        SET_ERROR(err, ERR_FILE_READ, "Read error");
        result = ERR_FILE_READ;
        goto cleanup;
    }
    
    // 成功
    CLEAR_ERROR(err);
    result = ERR_OK;
    
cleanup:
    // 集中清理（注意：free(NULL)和fclose(NULL)的安全性）
    if (buffer) free(buffer);
    if (output) fclose(output);
    if (input) fclose(input);
    
    return result;
}
```

### 错误回调模式

```c
// error_callback.h
#ifndef ERROR_CALLBACK_H
#define ERROR_CALLBACK_H

#include "error_system.h"

// 错误回调函数类型
typedef void (*ErrorCallback)(const ErrorInfo* err, void* user_data);

// 全局错误处理器
typedef struct {
    ErrorCallback on_error;
    ErrorCallback on_warning;
    void* user_data;
    int stop_on_error;
} ErrorHandler;

void error_handler_init(ErrorHandler* handler);
void error_handler_set_callback(ErrorHandler* handler, 
                                ErrorCallback on_error,
                                ErrorCallback on_warning,
                                void* user_data);
void error_handler_report(ErrorHandler* handler, const ErrorInfo* err);
void error_handler_warn(ErrorHandler* handler, const ErrorInfo* err);

#endif
```

```c
// error_callback.c
#include "error_callback.h"
#include <stdio.h>

// 默认错误处理
static void default_error_handler(const ErrorInfo* err, void* user_data) {
    (void)user_data;
    fprintf(stderr, "[DEFAULT ERROR] ");
    error_print(err);
}

static void default_warning_handler(const ErrorInfo* err, void* user_data) {
    (void)user_data;
    fprintf(stderr, "[WARNING] %s\n", 
            err->message ? err->message : "Unknown warning");
}

void error_handler_init(ErrorHandler* handler) {
    if (handler) {
        handler->on_error = default_error_handler;
        handler->on_warning = default_warning_handler;
        handler->user_data = NULL;
        handler->stop_on_error = 0;
    }
}

void error_handler_set_callback(ErrorHandler* handler,
                                ErrorCallback on_error,
                                ErrorCallback on_warning,
                                void* user_data) {
    if (handler) {
        if (on_error) handler->on_error = on_error;
        if (on_warning) handler->on_warning = on_warning;
        handler->user_data = user_data;
    }
}

void error_handler_report(ErrorHandler* handler, const ErrorInfo* err) {
    if (handler && err && err->code != ERR_OK && handler->on_error) {
        handler->on_error(err, handler->user_data);
    }
}

void error_handler_warn(ErrorHandler* handler, const ErrorInfo* err) {
    if (handler && err && handler->on_warning) {
        handler->on_warning(err, handler->user_data);
    }
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include "error_system.h"
#include "error_callback.h"

// 自定义错误回调
typedef struct {
    int error_count;
    int warning_count;
    FILE* log_file;
} AppContext;

void app_error_handler(const ErrorInfo* err, void* user_data) {
    AppContext* ctx = (AppContext*)user_data;
    ctx->error_count++;
    
    printf("[APP ERROR #%d] ", ctx->error_count);
    error_print(err);
    
    if (ctx->log_file) {
        fprintf(ctx->log_file, "ERROR: %s at %s:%d\n",
                err->message ? err->message : "?",
                err->file ? err->file : "?",
                err->line);
    }
}

void app_warning_handler(const ErrorInfo* err, void* user_data) {
    AppContext* ctx = (AppContext*)user_data;
    ctx->warning_count++;
    printf("[APP WARNING #%d] %s\n", 
           ctx->warning_count, err->message ? err->message : "?");
}

// 模拟可能失败的操作
ErrorCode do_risky_operation(int scenario, ErrorInfo* err) {
    switch (scenario) {
        case 1:
            SET_ERROR(err, ERR_FILE_NOT_FOUND, "Config file missing");
            return ERR_FILE_NOT_FOUND;
        case 2:
            SET_ERROR(err, ERR_OUT_OF_MEMORY, "Cannot allocate 1GB buffer");
            return ERR_OUT_OF_MEMORY;
        case 3:
            SET_ERROR(err, ERR_TIMEOUT, "Server did not respond");
            return ERR_TIMEOUT;
        default:
            CLEAR_ERROR(err);
            return ERR_OK;
    }
}

int main() {
    printf("=== Error Handling Demo ===\n\n");
    
    // 初始化应用上下文和错误处理器
    AppContext ctx = { 0, 0, NULL };
    ErrorHandler handler;
    error_handler_init(&handler);
    error_handler_set_callback(&handler, 
                               app_error_handler, 
                               app_warning_handler, 
                               &ctx);
    
    // 测试各种错误场景
    ErrorInfo err = ERROR_INFO_INIT;
    
    printf("--- Testing error scenarios ---\n\n");
    
    for (int i = 0; i <= 4; i++) {
        printf("Scenario %d: ", i);
        ErrorCode code = do_risky_operation(i, &err);
        
        if (code != ERR_OK) {
            error_handler_report(&handler, &err);
        } else {
            printf("Success!\n");
        }
    }
    
    // 测试错误信息查询
    printf("\n--- Error code information ---\n");
    ErrorCode codes[] = {ERR_OK, ERR_NULL_PTR, ERR_OUT_OF_MEMORY, ERR_TIMEOUT};
    for (size_t i = 0; i < sizeof(codes)/sizeof(codes[0]); i++) {
        printf("  %s: %s\n", 
               error_code_to_string(codes[i]),
               error_code_to_description(codes[i]));
    }
    
    // 测试文件处理（带goto cleanup）
    printf("\n--- File processing test ---\n");
    
    // 创建测试文件
    FILE* f = fopen("test_input.txt", "w");
    fprintf(f, "hello world\nthis is a test\n");
    fclose(f);
    
    CLEAR_ERROR(&err);
    ErrorCode result = process_file("test_input.txt", "test_output.txt", &err);
    if (result == ERR_OK) {
        printf("File processed successfully!\n");
    } else {
        error_handler_report(&handler, &err);
    }
    
    // 测试错误情况
    CLEAR_ERROR(&err);
    result = process_file("nonexistent.txt", "output.txt", &err);
    error_handler_report(&handler, &err);
    
    // 总结
    printf("\n--- Summary ---\n");
    printf("Total errors: %d\n", ctx.error_count);
    printf("Total warnings: %d\n", ctx.warning_count);
    
    // 清理
    remove("test_input.txt");
    remove("test_output.txt");
    
    return 0;
}

/* 输出示例:
=== Error Handling Demo ===

--- Testing error scenarios ---

Scenario 0: Success!
Scenario 1: [APP ERROR #1] [ERROR] ERR_FILE_NOT_FOUND (-6): Config file missing
        at main.c:45 in do_risky_operation()
Scenario 2: [APP ERROR #2] [ERROR] ERR_OUT_OF_MEMORY (-4): Cannot allocate 1GB buffer
        at main.c:48 in do_risky_operation()
Scenario 3: [APP ERROR #3] [ERROR] ERR_TIMEOUT (-10): Server did not respond
        at main.c:51 in do_risky_operation()
Scenario 4: Success!

--- Error code information ---
  ERR_OK: Success
  ERR_NULL_PTR: Null pointer
  ERR_OUT_OF_MEMORY: Out of memory
  ERR_TIMEOUT: Operation timeout

--- File processing test ---
File processed successfully!
[APP ERROR #4] [ERROR] ERR_FILE_NOT_FOUND (-6): Cannot open input file
        at file_processor.c:28 in process_file()

--- Summary ---
Total errors: 4
Total warnings: 0
*/
```

## 优缺点

### 优点
- **显式控制**：错误处理流程完全可控
- **无运行时开销**：不像异常需要栈展开
- **跨平台**：纯C实现，任何平台都支持
- **灵活性高**：可根据需求选择不同策略

### 缺点
- 需要手动检查每个可能失败的调用
- 代码冗长，容易遗漏检查
- 错误传播需要层层处理
- 没有强制处理机制（容易忽略错误）

