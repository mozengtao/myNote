# 适配器模式 (Adapter Pattern)

## 定义

适配器模式是一种结构型设计模式，它允许将一个类的接口转换成客户端期望的另一个接口。适配器模式使得原本由于接口不兼容而不能一起工作的类可以协同工作。

## 适用场景

- 需要使用现有类，但其接口不符合需求时
- 希望创建一个可复用的类，与不相关或不可预见的类协同工作
- 需要统一多个类的接口时
- 封装第三方库，提供统一的访问接口
- 新旧系统的对接和兼容
- 不同硬件设备的驱动统一接口

## ASCII 图解

```
+------------------------------------------------------------------------+
|                         ADAPTER PATTERN                                 |
+------------------------------------------------------------------------+
|                                                                         |
|   Without Adapter (Incompatible):                                       |
|                                                                         |
|   +----------+          X          +-------------------+                |
|   |  Client  |=========/===========>|  Legacy System   |                |
|   +----------+    Interface         +-------------------+                |
|                   Mismatch          | + oldMethod()    |                |
|                                     | + legacyAPI()    |                |
|                                     +-------------------+                |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   With Adapter (Compatible):                                            |
|                                                                         |
|   +----------+     +-------------+     +-------------------+            |
|   |  Client  |---->|   Adapter   |---->|  Legacy System   |            |
|   +----------+     +-------------+     +-------------------+            |
|                    | Translates  |     | + oldMethod()    |            |
|   Expected         | interface   |     | + legacyAPI()    |            |
|   Interface:       | calls       |     +-------------------+            |
|   + newMethod()    +-------------+                                      |
|   + modernAPI()         |                                               |
|                         |                                               |
|                    +----+----+                                          |
|                    |         |                                          |
|                    v         v                                          |
|              newMethod() -> oldMethod()                                 |
|              modernAPI() -> legacyAPI()                                 |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Adapter Internal Structure:                                           |
|                                                                         |
|   +---------------------------------------------------------------+    |
|   |                         Adapter                                |    |
|   +---------------------------------------------------------------+    |
|   |  +------------------+     +---------------------------+       |    |
|   |  | Target Interface |     |   Adaptee (Legacy)        |       |    |
|   |  +------------------+     +---------------------------+       |    |
|   |  | + request()      |---->| + specific_request()      |       |    |
|   |  +------------------+     +---------------------------+       |    |
|   |                                                               |    |
|   |  Implementation:                                              |    |
|   |  request() {                                                  |    |
|   |      // Translate and delegate                                |    |
|   |      return adaptee->specific_request();                      |    |
|   |  }                                                            |    |
|   +---------------------------------------------------------------+    |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了适配器模式解决接口不兼容问题的方式。上半部分显示了没有适配器时，客户端与旧系统因接口不匹配而无法直接通信。下半部分展示了适配器如何充当"翻译器"的角色：客户端调用适配器提供的新接口（`newMethod()`），适配器内部将调用转换为旧系统能理解的接口（`oldMethod()`）。这样，客户端可以使用统一的现代接口与任何被适配的系统交互。

## 实现方法

在C语言中实现适配器模式：

1. 定义目标接口（客户端期望的接口）
2. 创建适配器结构体，包含指向被适配对象的指针
3. 在适配器中实现目标接口，内部调用被适配对象的方法
4. 必要时进行数据格式转换

## C语言代码示例

### 场景：统一不同日志系统的接口

```c
// target_logger.h - 目标接口（客户端期望的统一接口）
#ifndef TARGET_LOGGER_H
#define TARGET_LOGGER_H

// 日志级别
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// 统一的日志接口
typedef struct Logger Logger;
typedef struct {
    void (*log)(Logger* self, LogLevel level, const char* message);
    void (*set_level)(Logger* self, LogLevel level);
    void (*destroy)(Logger* self);
} LoggerVTable;

struct Logger {
    const LoggerVTable* vtable;
};

// 通用日志操作
void logger_log(Logger* logger, LogLevel level, const char* message);
void logger_set_level(Logger* logger, LogLevel level);
void logger_destroy(Logger* logger);

// 便捷宏
#define LOG_D(logger, msg) logger_log(logger, LOG_DEBUG, msg)
#define LOG_I(logger, msg) logger_log(logger, LOG_INFO, msg)
#define LOG_W(logger, msg) logger_log(logger, LOG_WARNING, msg)
#define LOG_E(logger, msg) logger_log(logger, LOG_ERROR, msg)

#endif
```

```c
// target_logger.c
#include "target_logger.h"

void logger_log(Logger* logger, LogLevel level, const char* message) {
    if (logger && logger->vtable && logger->vtable->log) {
        logger->vtable->log(logger, level, message);
    }
}

void logger_set_level(Logger* logger, LogLevel level) {
    if (logger && logger->vtable && logger->vtable->set_level) {
        logger->vtable->set_level(logger, level);
    }
}

void logger_destroy(Logger* logger) {
    if (logger && logger->vtable && logger->vtable->destroy) {
        logger->vtable->destroy(logger);
    }
}
```

### 旧日志系统（被适配者）

```c
// legacy_logger.h - 旧的日志系统（第三方或遗留系统）
#ifndef LEGACY_LOGGER_H
#define LEGACY_LOGGER_H

// 旧系统使用不同的日志级别定义
#define LEGACY_TRACE    0
#define LEGACY_NOTICE   1
#define LEGACY_WARN     2
#define LEGACY_FATAL    3

typedef struct {
    int min_priority;
    char prefix[32];
} LegacyLogger;

// 旧系统的接口（不同于目标接口）
LegacyLogger* legacy_logger_create(const char* prefix);
void legacy_logger_write(LegacyLogger* logger, int priority, const char* text);
void legacy_logger_set_min_priority(LegacyLogger* logger, int priority);
void legacy_logger_free(LegacyLogger* logger);

#endif
```

```c
// legacy_logger.c
#include "legacy_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

LegacyLogger* legacy_logger_create(const char* prefix) {
    LegacyLogger* logger = (LegacyLogger*)malloc(sizeof(LegacyLogger));
    if (logger) {
        logger->min_priority = LEGACY_TRACE;
        strncpy(logger->prefix, prefix, sizeof(logger->prefix) - 1);
        logger->prefix[sizeof(logger->prefix) - 1] = '\0';
    }
    return logger;
}

void legacy_logger_write(LegacyLogger* logger, int priority, const char* text) {
    if (!logger || priority < logger->min_priority) return;
    
    const char* priority_names[] = {"TRACE", "NOTICE", "WARN", "FATAL"};
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[LEGACY][%s][%s][%s] %s\n", 
           time_buf, 
           logger->prefix,
           priority_names[priority], 
           text);
}

void legacy_logger_set_min_priority(LegacyLogger* logger, int priority) {
    if (logger) {
        logger->min_priority = priority;
    }
}

void legacy_logger_free(LegacyLogger* logger) {
    free(logger);
}
```

### 另一个日志系统（另一个被适配者）

```c
// file_logger.h - 文件日志系统
#ifndef FILE_LOGGER_H
#define FILE_LOGGER_H

#include <stdio.h>

typedef struct {
    FILE* file;
    int verbosity;  // 0-3, 0=最少信息
} FileLogger;

FileLogger* file_logger_open(const char* filename);
void file_logger_record(FileLogger* logger, int verbosity, const char* msg);
void file_logger_set_verbosity(FileLogger* logger, int verbosity);
void file_logger_close(FileLogger* logger);

#endif
```

```c
// file_logger.c
#include "file_logger.h"
#include <stdlib.h>
#include <time.h>

FileLogger* file_logger_open(const char* filename) {
    FileLogger* logger = (FileLogger*)malloc(sizeof(FileLogger));
    if (logger) {
        logger->file = fopen(filename, "a");
        logger->verbosity = 3;  // 默认记录所有
        if (!logger->file) {
            free(logger);
            return NULL;
        }
    }
    return logger;
}

void file_logger_record(FileLogger* logger, int verbosity, const char* msg) {
    if (!logger || !logger->file || verbosity > logger->verbosity) return;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);
    
    fprintf(logger->file, "[%s][V%d] %s\n", time_buf, verbosity, msg);
    fflush(logger->file);
}

void file_logger_set_verbosity(FileLogger* logger, int verbosity) {
    if (logger) {
        logger->verbosity = verbosity;
    }
}

void file_logger_close(FileLogger* logger) {
    if (logger) {
        if (logger->file) {
            fclose(logger->file);
        }
        free(logger);
    }
}
```

### 适配器实现

```c
// logger_adapters.h
#ifndef LOGGER_ADAPTERS_H
#define LOGGER_ADAPTERS_H

#include "target_logger.h"
#include "legacy_logger.h"
#include "file_logger.h"

// 创建Legacy日志系统的适配器
Logger* create_legacy_logger_adapter(const char* prefix);

// 创建File日志系统的适配器
Logger* create_file_logger_adapter(const char* filename);

// 创建控制台日志（原生实现目标接口）
Logger* create_console_logger(void);

#endif
```

```c
// logger_adapters.c
#include "logger_adapters.h"
#include <stdio.h>
#include <stdlib.h>

// ==================== Legacy Logger Adapter ====================

typedef struct {
    Logger base;
    LegacyLogger* legacy;
    LogLevel current_level;
} LegacyLoggerAdapter;

// 日志级别转换：新接口 -> 旧接口
static int convert_level_to_legacy(LogLevel level) {
    switch (level) {
        case LOG_DEBUG:   return LEGACY_TRACE;
        case LOG_INFO:    return LEGACY_NOTICE;
        case LOG_WARNING: return LEGACY_WARN;
        case LOG_ERROR:   return LEGACY_FATAL;
        default:          return LEGACY_TRACE;
    }
}

static void legacy_adapter_log(Logger* self, LogLevel level, const char* message) {
    LegacyLoggerAdapter* adapter = (LegacyLoggerAdapter*)self;
    if (level >= adapter->current_level) {
        int legacy_level = convert_level_to_legacy(level);
        legacy_logger_write(adapter->legacy, legacy_level, message);
    }
}

static void legacy_adapter_set_level(Logger* self, LogLevel level) {
    LegacyLoggerAdapter* adapter = (LegacyLoggerAdapter*)self;
    adapter->current_level = level;
    legacy_logger_set_min_priority(adapter->legacy, convert_level_to_legacy(level));
}

static void legacy_adapter_destroy(Logger* self) {
    LegacyLoggerAdapter* adapter = (LegacyLoggerAdapter*)self;
    legacy_logger_free(adapter->legacy);
    free(adapter);
}

static const LoggerVTable legacy_adapter_vtable = {
    .log = legacy_adapter_log,
    .set_level = legacy_adapter_set_level,
    .destroy = legacy_adapter_destroy
};

Logger* create_legacy_logger_adapter(const char* prefix) {
    LegacyLoggerAdapter* adapter = (LegacyLoggerAdapter*)malloc(sizeof(LegacyLoggerAdapter));
    if (adapter) {
        adapter->base.vtable = &legacy_adapter_vtable;
        adapter->legacy = legacy_logger_create(prefix);
        adapter->current_level = LOG_DEBUG;
        if (!adapter->legacy) {
            free(adapter);
            return NULL;
        }
    }
    return (Logger*)adapter;
}

// ==================== File Logger Adapter ====================

typedef struct {
    Logger base;
    FileLogger* file_logger;
    LogLevel current_level;
} FileLoggerAdapter;

// 日志级别转换：新接口 -> 文件日志接口
static int convert_level_to_verbosity(LogLevel level) {
    // verbosity: 3=DEBUG, 2=INFO, 1=WARNING, 0=ERROR
    switch (level) {
        case LOG_DEBUG:   return 3;
        case LOG_INFO:    return 2;
        case LOG_WARNING: return 1;
        case LOG_ERROR:   return 0;
        default:          return 3;
    }
}

static void file_adapter_log(Logger* self, LogLevel level, const char* message) {
    FileLoggerAdapter* adapter = (FileLoggerAdapter*)self;
    if (level >= adapter->current_level) {
        int verbosity = convert_level_to_verbosity(level);
        file_logger_record(adapter->file_logger, verbosity, message);
    }
}

static void file_adapter_set_level(Logger* self, LogLevel level) {
    FileLoggerAdapter* adapter = (FileLoggerAdapter*)self;
    adapter->current_level = level;
    file_logger_set_verbosity(adapter->file_logger, convert_level_to_verbosity(level));
}

static void file_adapter_destroy(Logger* self) {
    FileLoggerAdapter* adapter = (FileLoggerAdapter*)self;
    file_logger_close(adapter->file_logger);
    free(adapter);
}

static const LoggerVTable file_adapter_vtable = {
    .log = file_adapter_log,
    .set_level = file_adapter_set_level,
    .destroy = file_adapter_destroy
};

Logger* create_file_logger_adapter(const char* filename) {
    FileLoggerAdapter* adapter = (FileLoggerAdapter*)malloc(sizeof(FileLoggerAdapter));
    if (adapter) {
        adapter->base.vtable = &file_adapter_vtable;
        adapter->file_logger = file_logger_open(filename);
        adapter->current_level = LOG_DEBUG;
        if (!adapter->file_logger) {
            free(adapter);
            return NULL;
        }
    }
    return (Logger*)adapter;
}

// ==================== Console Logger (Native) ====================

typedef struct {
    Logger base;
    LogLevel min_level;
} ConsoleLogger;

static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG:   return "DEBUG";
        case LOG_INFO:    return "INFO ";
        case LOG_WARNING: return "WARN ";
        case LOG_ERROR:   return "ERROR";
        default:          return "?????";
    }
}

static void console_log(Logger* self, LogLevel level, const char* message) {
    ConsoleLogger* logger = (ConsoleLogger*)self;
    if (level >= logger->min_level) {
        printf("[CONSOLE][%s] %s\n", level_to_string(level), message);
    }
}

static void console_set_level(Logger* self, LogLevel level) {
    ConsoleLogger* logger = (ConsoleLogger*)self;
    logger->min_level = level;
}

static void console_destroy(Logger* self) {
    free(self);
}

static const LoggerVTable console_vtable = {
    .log = console_log,
    .set_level = console_set_level,
    .destroy = console_destroy
};

Logger* create_console_logger(void) {
    ConsoleLogger* logger = (ConsoleLogger*)malloc(sizeof(ConsoleLogger));
    if (logger) {
        logger->base.vtable = &console_vtable;
        logger->min_level = LOG_DEBUG;
    }
    return (Logger*)logger;
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "logger_adapters.h"

void test_application(Logger* logger) {
    LOG_D(logger, "Application starting...");
    LOG_I(logger, "Initializing modules");
    LOG_W(logger, "Configuration file not found, using defaults");
    LOG_E(logger, "Failed to connect to database");
}

int main() {
    printf("=== Adapter Pattern Demo ===\n\n");
    
    // 创建不同的日志实现，但使用统一接口
    Logger* loggers[3];
    loggers[0] = create_console_logger();
    loggers[1] = create_legacy_logger_adapter("MyApp");
    loggers[2] = create_file_logger_adapter("app.log");
    
    const char* logger_names[] = {"Console", "Legacy", "File"};
    
    for (int i = 0; i < 3; i++) {
        if (loggers[i]) {
            printf("\n--- Testing %s Logger ---\n", logger_names[i]);
            test_application(loggers[i]);
            
            printf("\n--- Setting level to WARNING ---\n");
            logger_set_level(loggers[i], LOG_WARNING);
            test_application(loggers[i]);
        }
    }
    
    // 清理
    printf("\n--- Cleanup ---\n");
    for (int i = 0; i < 3; i++) {
        logger_destroy(loggers[i]);
    }
    
    return 0;
}

/* 输出示例:
=== Adapter Pattern Demo ===

--- Testing Console Logger ---
[CONSOLE][DEBUG] Application starting...
[CONSOLE][INFO ] Initializing modules
[CONSOLE][WARN ] Configuration file not found, using defaults
[CONSOLE][ERROR] Failed to connect to database

--- Setting level to WARNING ---
[CONSOLE][WARN ] Configuration file not found, using defaults
[CONSOLE][ERROR] Failed to connect to database

--- Testing Legacy Logger ---
[LEGACY][2025-12-02 10:30:45][MyApp][TRACE] Application starting...
[LEGACY][2025-12-02 10:30:45][MyApp][NOTICE] Initializing modules
[LEGACY][2025-12-02 10:30:45][MyApp][WARN] Configuration file not found, using defaults
[LEGACY][2025-12-02 10:30:45][MyApp][FATAL] Failed to connect to database

--- Setting level to WARNING ---
[LEGACY][2025-12-02 10:30:45][MyApp][WARN] Configuration file not found, using defaults
[LEGACY][2025-12-02 10:30:45][MyApp][FATAL] Failed to connect to database

... (File logger writes to app.log)

--- Cleanup ---
*/
```

## 优缺点

### 优点
- 让不兼容的接口可以协同工作
- 提高类的复用性
- 将接口转换逻辑集中在适配器中
- 符合开闭原则（对扩展开放，对修改关闭）
- 符合单一职责原则（适配逻辑与业务逻辑分离）

### 缺点
- 增加了系统的复杂性
- 过多使用适配器会使系统凌乱
- 适配器需要了解被适配者的实现细节
- 可能存在性能开销（额外的函数调用层）

