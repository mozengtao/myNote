# 回调模式 (Callback Pattern)

## 定义

回调模式是一种设计模式，通过函数指针将一个函数作为参数传递给另一个函数，使得被调用的函数可以在适当的时机"回调"调用者提供的函数。这是实现控制反转（IoC）和事件驱动编程的基础。

## 适用场景

- 异步操作完成后的通知
- 事件处理和事件驱动架构
- 排序/搜索算法的自定义比较函数
- 遍历数据结构时的自定义处理
- 定时器到期回调
- I/O完成通知
- GUI事件处理
- 中间件和插件系统

## ASCII 图解

```
+------------------------------------------------------------------------+
|                        CALLBACK PATTERN                                 |
+------------------------------------------------------------------------+
|                                                                         |
|   Synchronous Callback:                                                 |
|                                                                         |
|   +--------+    register_callback(fn)    +-------------+               |
|   | Client |----------------------------->|   Library   |               |
|   +--------+                              +-------------+               |
|       ^                                          |                      |
|       |            callback(result)              |                      |
|       +------------------------------------------+                      |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Asynchronous Callback:                                                |
|                                                                         |
|   +--------+   start_async(callback)   +-------------+                 |
|   | Client |-------------------------->|   Library   |                 |
|   +--------+                           +------+------+                 |
|       |                                       |                         |
|       | (continues execution)                 | (background work)       |
|       |                                       |                         |
|       v                                       v                         |
|   [Other work]                          [Processing...]                |
|       |                                       |                         |
|       |          callback(result)             |                         |
|       +<--------------------------------------+                         |
|       |                                                                 |
|       v                                                                 |
|   [Handle result]                                                       |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Callback Structure:                                                   |
|                                                                         |
|   +---------------------------+                                         |
|   |     Callback Function     |                                         |
|   +---------------------------+                                         |
|   | typedef void (*Callback)  |                                         |
|   |   (void* context,         |  <-- User data                          |
|   |    Result* result);       |  <-- Operation result                   |
|   +---------------------------+                                         |
|                                                                         |
|   +---------------------------+                                         |
|   |   Callback Registration   |                                         |
|   +---------------------------+                                         |
|   | + callback_fn   ----------|--> Points to user function              |
|   | + user_context  ----------|--> User's private data                  |
|   +---------------------------+                                         |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Callback Chain / Event Handler:                                       |
|                                                                         |
|   [Event Occurs]                                                        |
|         |                                                               |
|         v                                                               |
|   +-----+-----+                                                         |
|   | Handler 1 |---> callback_1(ctx, event)                             |
|   +-----------+                                                         |
|         |                                                               |
|         v                                                               |
|   +-----+-----+                                                         |
|   | Handler 2 |---> callback_2(ctx, event)                             |
|   +-----------+                                                         |
|         |                                                               |
|         v                                                               |
|   +-----+-----+                                                         |
|   | Handler N |---> callback_n(ctx, event)                             |
|   +-----------+                                                         |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了回调模式的多种使用场景。同步回调中，客户端注册回调函数后，库函数在完成操作时立即调用回调。异步回调中，客户端启动异步操作后可以继续执行其他工作，当操作完成时回调会被触发。中间展示了回调函数的典型结构，包括函数指针和用户上下文数据。底部展示了回调链，用于处理需要多个处理器响应同一事件的场景。

## 实现方法

在C语言中实现回调模式：

1. 定义回调函数类型（typedef函数指针）
2. 在结构体中存储回调函数指针和用户数据
3. 在适当时机调用回调函数
4. 传递必要的上下文和结果数据

## C语言代码示例

### 基本回调实现

```c
// callback_basic.h
#ifndef CALLBACK_BASIC_H
#define CALLBACK_BASIC_H

// 基本回调函数类型
typedef void (*SimpleCallback)(int result);

// 带上下文的回调函数类型
typedef void (*ContextCallback)(void* context, int result);

// 回调信息结构
typedef struct {
    ContextCallback callback;
    void* context;
} CallbackInfo;

// 注册和调用回调的函数
void set_callback(CallbackInfo* info, ContextCallback cb, void* ctx);
void invoke_callback(CallbackInfo* info, int result);

#endif
```

```c
// callback_basic.c
#include "callback_basic.h"
#include <stdio.h>

void set_callback(CallbackInfo* info, ContextCallback cb, void* ctx) {
    if (info) {
        info->callback = cb;
        info->context = ctx;
        printf("[Callback] Registered\n");
    }
}

void invoke_callback(CallbackInfo* info, int result) {
    if (info && info->callback) {
        printf("[Callback] Invoking with result: %d\n", result);
        info->callback(info->context, result);
    }
}
```

### 排序回调示例

```c
// sort_callback.h
#ifndef SORT_CALLBACK_H
#define SORT_CALLBACK_H

#include <stddef.h>

// 比较函数回调类型
// 返回: <0 if a<b, 0 if a==b, >0 if a>b
typedef int (*CompareCallback)(const void* a, const void* b);

// 通用排序函数
void generic_sort(void* array, size_t count, size_t elem_size,
                  CompareCallback compare);

// 预定义比较函数
int compare_int_asc(const void* a, const void* b);
int compare_int_desc(const void* a, const void* b);
int compare_string(const void* a, const void* b);

#endif
```

```c
// sort_callback.c
#include "sort_callback.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 简单冒泡排序（使用回调比较）
void generic_sort(void* array, size_t count, size_t elem_size,
                  CompareCallback compare) {
    char* arr = (char*)array;
    char* temp = (char*)malloc(elem_size);
    
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = 0; j < count - i - 1; j++) {
            void* elem_a = arr + j * elem_size;
            void* elem_b = arr + (j + 1) * elem_size;
            
            // 使用回调函数比较
            if (compare(elem_a, elem_b) > 0) {
                memcpy(temp, elem_a, elem_size);
                memcpy(elem_a, elem_b, elem_size);
                memcpy(elem_b, temp, elem_size);
            }
        }
    }
    
    free(temp);
}

int compare_int_asc(const void* a, const void* b) {
    return (*(int*)a - *(int*)b);
}

int compare_int_desc(const void* a, const void* b) {
    return (*(int*)b - *(int*)a);
}

int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}
```

### 事件系统回调

```c
// event_system.h
#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#define MAX_HANDLERS 16

// 事件类型
typedef enum {
    EVENT_NONE = 0,
    EVENT_BUTTON_CLICK,
    EVENT_KEY_PRESS,
    EVENT_TIMER,
    EVENT_DATA_READY
} EventType;

// 事件数据
typedef struct {
    EventType type;
    int code;
    void* data;
    const char* source;
} Event;

// 事件处理回调
typedef void (*EventHandler)(void* context, Event* event);

// 处理器注册信息
typedef struct {
    EventType type;
    EventHandler handler;
    void* context;
    const char* name;
} HandlerInfo;

// 事件系统
typedef struct {
    HandlerInfo handlers[MAX_HANDLERS];
    int handler_count;
} EventSystem;

// 事件系统API
void event_system_init(EventSystem* es);
int event_system_register(EventSystem* es, EventType type,
                         EventHandler handler, void* context,
                         const char* name);
void event_system_unregister(EventSystem* es, const char* name);
void event_system_dispatch(EventSystem* es, Event* event);
void event_system_cleanup(EventSystem* es);

#endif
```

```c
// event_system.c
#include "event_system.h"
#include <stdio.h>
#include <string.h>

void event_system_init(EventSystem* es) {
    if (es) {
        memset(es->handlers, 0, sizeof(es->handlers));
        es->handler_count = 0;
        printf("[EventSystem] Initialized\n");
    }
}

int event_system_register(EventSystem* es, EventType type,
                         EventHandler handler, void* context,
                         const char* name) {
    if (!es || es->handler_count >= MAX_HANDLERS) {
        return -1;
    }
    
    HandlerInfo* info = &es->handlers[es->handler_count++];
    info->type = type;
    info->handler = handler;
    info->context = context;
    info->name = name;
    
    printf("[EventSystem] Registered handler '%s' for event type %d\n",
           name, type);
    return 0;
}

void event_system_unregister(EventSystem* es, const char* name) {
    if (!es) return;
    
    for (int i = 0; i < es->handler_count; i++) {
        if (strcmp(es->handlers[i].name, name) == 0) {
            // 移动后面的元素
            for (int j = i; j < es->handler_count - 1; j++) {
                es->handlers[j] = es->handlers[j + 1];
            }
            es->handler_count--;
            printf("[EventSystem] Unregistered handler '%s'\n", name);
            return;
        }
    }
}

void event_system_dispatch(EventSystem* es, Event* event) {
    if (!es || !event) return;
    
    printf("[EventSystem] Dispatching event type %d from '%s'\n",
           event->type, event->source ? event->source : "unknown");
    
    int handled = 0;
    for (int i = 0; i < es->handler_count; i++) {
        HandlerInfo* info = &es->handlers[i];
        if (info->type == event->type && info->handler) {
            printf("  -> Calling handler '%s'\n", info->name);
            info->handler(info->context, event);
            handled++;
        }
    }
    
    if (handled == 0) {
        printf("  -> No handlers for this event type\n");
    }
}

void event_system_cleanup(EventSystem* es) {
    if (es) {
        es->handler_count = 0;
        printf("[EventSystem] Cleaned up\n");
    }
}
```

### 异步操作回调

```c
// async_operation.h
#ifndef ASYNC_OPERATION_H
#define ASYNC_OPERATION_H

// 操作状态
typedef enum {
    OP_PENDING,
    OP_SUCCESS,
    OP_FAILED,
    OP_CANCELLED
} OperationStatus;

// 异步操作结果
typedef struct {
    OperationStatus status;
    int error_code;
    void* result_data;
    size_t result_size;
} AsyncResult;

// 完成回调
typedef void (*CompletionCallback)(void* context, AsyncResult* result);

// 异步操作句柄
typedef struct {
    CompletionCallback on_complete;
    void* user_context;
    int operation_id;
    OperationStatus status;
} AsyncOperation;

// 模拟异步操作
AsyncOperation* start_async_read(const char* filename,
                                 CompletionCallback callback,
                                 void* context);
AsyncOperation* start_async_compute(int value,
                                    CompletionCallback callback,
                                    void* context);
void simulate_completion(AsyncOperation* op);
void async_op_destroy(AsyncOperation* op);

#endif
```

```c
// async_operation.c
#include "async_operation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_operation_id = 0;

AsyncOperation* start_async_read(const char* filename,
                                 CompletionCallback callback,
                                 void* context) {
    AsyncOperation* op = (AsyncOperation*)malloc(sizeof(AsyncOperation));
    if (op) {
        op->on_complete = callback;
        op->user_context = context;
        op->operation_id = ++g_operation_id;
        op->status = OP_PENDING;
        
        printf("[Async] Started read operation #%d for '%s'\n",
               op->operation_id, filename);
    }
    return op;
}

AsyncOperation* start_async_compute(int value,
                                    CompletionCallback callback,
                                    void* context) {
    AsyncOperation* op = (AsyncOperation*)malloc(sizeof(AsyncOperation));
    if (op) {
        op->on_complete = callback;
        op->user_context = context;
        op->operation_id = ++g_operation_id;
        op->status = OP_PENDING;
        
        printf("[Async] Started compute operation #%d with value %d\n",
               op->operation_id, value);
    }
    return op;
}

void simulate_completion(AsyncOperation* op) {
    if (!op) return;
    
    printf("[Async] Operation #%d completing...\n", op->operation_id);
    
    // 模拟操作结果
    AsyncResult result;
    result.status = OP_SUCCESS;
    result.error_code = 0;
    
    // 创建示例结果数据
    int* data = (int*)malloc(sizeof(int));
    *data = op->operation_id * 100;
    result.result_data = data;
    result.result_size = sizeof(int);
    
    op->status = OP_SUCCESS;
    
    // 调用完成回调
    if (op->on_complete) {
        op->on_complete(op->user_context, &result);
    }
    
    free(data);
}

void async_op_destroy(AsyncOperation* op) {
    if (op) {
        printf("[Async] Operation #%d destroyed\n", op->operation_id);
        free(op);
    }
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include <string.h>
#include "event_system.h"
#include "sort_callback.h"
#include "async_operation.h"

// ========== 事件处理回调 ==========

typedef struct {
    char name[32];
    int click_count;
} ButtonContext;

void button_click_handler(void* context, Event* event) {
    ButtonContext* btn = (ButtonContext*)context;
    btn->click_count++;
    printf("    [Button '%s'] Clicked! (count: %d)\n",
           btn->name, btn->click_count);
}

void logger_handler(void* context, Event* event) {
    (void)context;
    printf("    [Logger] Event received: type=%d, code=%d\n",
           event->type, event->code);
}

// ========== 异步操作回调 ==========

void read_complete_handler(void* context, AsyncResult* result) {
    const char* filename = (const char*)context;
    printf("    [ReadCallback] File '%s' read complete!\n", filename);
    printf("    [ReadCallback] Status: %s, Data: %d\n",
           result->status == OP_SUCCESS ? "SUCCESS" : "FAILED",
           result->result_data ? *(int*)result->result_data : -1);
}

void compute_complete_handler(void* context, AsyncResult* result) {
    int original = *(int*)context;
    printf("    [ComputeCallback] Computation for %d complete!\n", original);
    printf("    [ComputeCallback] Result: %d\n",
           result->result_data ? *(int*)result->result_data : -1);
}

// ========== Main ==========

int main() {
    printf("=== Callback Pattern Demo ===\n\n");
    
    // ========== 排序回调示例 ==========
    printf("--- Sort with Callbacks ---\n\n");
    
    int numbers[] = {64, 34, 25, 12, 22, 11, 90};
    int count = sizeof(numbers) / sizeof(numbers[0]);
    
    printf("Before (ascending): ");
    for (int i = 0; i < count; i++) printf("%d ", numbers[i]);
    printf("\n");
    
    generic_sort(numbers, count, sizeof(int), compare_int_asc);
    
    printf("After (ascending):  ");
    for (int i = 0; i < count; i++) printf("%d ", numbers[i]);
    printf("\n\n");
    
    // ========== 事件系统回调示例 ==========
    printf("--- Event System with Callbacks ---\n\n");
    
    EventSystem es;
    event_system_init(&es);
    
    ButtonContext btn1 = { .name = "Submit", .click_count = 0 };
    ButtonContext btn2 = { .name = "Cancel", .click_count = 0 };
    
    event_system_register(&es, EVENT_BUTTON_CLICK, 
                         button_click_handler, &btn1, "SubmitHandler");
    event_system_register(&es, EVENT_BUTTON_CLICK,
                         button_click_handler, &btn2, "CancelHandler");
    event_system_register(&es, EVENT_BUTTON_CLICK,
                         logger_handler, NULL, "ClickLogger");
    
    // 触发事件
    printf("\n");
    Event click_event = {
        .type = EVENT_BUTTON_CLICK,
        .code = 1,
        .data = NULL,
        .source = "UI"
    };
    event_system_dispatch(&es, &click_event);
    
    printf("\n");
    event_system_dispatch(&es, &click_event);
    
    event_system_cleanup(&es);
    
    // ========== 异步操作回调示例 ==========
    printf("\n--- Async Operations with Callbacks ---\n\n");
    
    static const char* filename = "data.txt";
    static int compute_value = 42;
    
    AsyncOperation* read_op = start_async_read(filename,
        read_complete_handler, (void*)filename);
    
    AsyncOperation* compute_op = start_async_compute(compute_value,
        compute_complete_handler, &compute_value);
    
    printf("\n[Main] Doing other work while async ops run...\n\n");
    
    // 模拟异步完成
    simulate_completion(read_op);
    printf("\n");
    simulate_completion(compute_op);
    
    async_op_destroy(read_op);
    async_op_destroy(compute_op);
    
    return 0;
}

/* 输出示例:
=== Callback Pattern Demo ===

--- Sort with Callbacks ---

Before (ascending): 64 34 25 12 22 11 90 
After (ascending):  11 12 22 25 34 64 90 

--- Event System with Callbacks ---

[EventSystem] Initialized
[EventSystem] Registered handler 'SubmitHandler' for event type 1
[EventSystem] Registered handler 'CancelHandler' for event type 1
[EventSystem] Registered handler 'ClickLogger' for event type 1

[EventSystem] Dispatching event type 1 from 'UI'
  -> Calling handler 'SubmitHandler'
    [Button 'Submit'] Clicked! (count: 1)
  -> Calling handler 'CancelHandler'
    [Button 'Cancel'] Clicked! (count: 1)
  -> Calling handler 'ClickLogger'
    [Logger] Event received: type=1, code=1

--- Async Operations with Callbacks ---

[Async] Started read operation #1 for 'data.txt'
[Async] Started compute operation #2 with value 42

[Main] Doing other work while async ops run...

[Async] Operation #1 completing...
    [ReadCallback] File 'data.txt' read complete!
    [ReadCallback] Status: SUCCESS, Data: 100

[Async] Operation #2 completing...
    [ComputeCallback] Computation for 42 complete!
    [ComputeCallback] Result: 200
*/
```

## 优缺点

### 优点
- 实现代码解耦
- 支持事件驱动编程
- 允许算法/行为的自定义
- 异步操作的基础
- C语言原生支持

### 缺点
- 回调地狱（嵌套过深）
- 错误处理复杂
- 调试困难（调用栈不清晰）
- 可能导致内存管理问题

