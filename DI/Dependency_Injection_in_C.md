# 依赖注入 (Dependency Injection) - C语言实现详解

## 目录

1. [什么是依赖注入](#1-什么是依赖注入)
2. [依赖注入前后对比](#2-依赖注入前后对比)
3. [C语言实现依赖注入](#3-c语言实现依赖注入)
4. [完整示例：日志系统](#4-完整示例日志系统)
5. [如何识别并重构](#5-如何识别并重构)
6. [核心思想与实现步骤](#6-核心思想与实现步骤)

---

## 1. 什么是依赖注入

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         What is Dependency Injection?                                   │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    WITHOUT Dependency Injection              WITH Dependency Injection
    (Hard-coded dependency)                   (Injected dependency)

    ┌─────────────────────────┐               ┌─────────────────────────┐
    │      OrderService       │               │      OrderService       │
    │                         │               │                         │
    │  ┌───────────────────┐  │               │  ┌───────────────────┐  │
    │  │ void process() {  │  │               │  │ Logger *logger;   │  │
    │  │   FileLogger log; │  │               │  │                   │  │
    │  │   log.write(...); │  │               │  │ void process() {  │  │
    │  │ }                 │  │               │  │   logger->log();  │  │
    │  └───────────────────┘  │               │  │ }                 │  │
    │                         │               │  └───────────────────┘  │
    │  Creates its own        │               │                         │
    │  dependency internally  │               │  Receives dependency    │
    │                         │               │  from outside           │
    └───────────┬─────────────┘               └───────────┬─────────────┘
                │                                         │
                │ Tight coupling                          │ Loose coupling
                │                                         │
                ▼                                         ▼
    ┌─────────────────────────┐               ┌─────────────────────────┐
    │      FileLogger         │               │    Logger Interface     │
    │      (concrete)         │               │    (abstraction)        │
    │                         │               │           │             │
    │  - Hard to test         │               │     ┌─────┴─────┐       │
    │  - Hard to change       │               │     ▼           ▼       │
    │  - Hard to reuse        │               │ FileLogger  MockLogger  │
    │                         │               │                         │
    └─────────────────────────┘               │  - Easy to test         │
                                              │  - Easy to change       │
                                              │  - Easy to reuse        │
                                              └─────────────────────────┘
```

**说明**：
- **没有依赖注入**：模块内部创建依赖对象，形成紧耦合
- **使用依赖注入**：依赖从外部传入，模块只依赖抽象接口
- 核心思想：**"不要自己创建依赖，让别人给你"**

### 依赖注入的三种方式

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Three Types of Dependency Injection                             │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    1. Constructor Injection (构造函数注入)
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   OrderService* order_service_create(Logger *logger, Database *db) {                │
    │       OrderService *svc = malloc(sizeof(OrderService));                             │
    │       svc->logger = logger;    // Inject at creation time                           │
    │       svc->db = db;                                                                 │
    │       return svc;                                                                   │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    2. Setter Injection (Setter 注入)
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   void order_service_set_logger(OrderService *svc, Logger *logger) {                │
    │       svc->logger = logger;    // Inject after creation                             │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    3. Interface Injection (接口注入 / 方法参数注入)
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   void order_service_process(OrderService *svc, Logger *logger) {                   │
    │       logger->log("Processing order...");  // Inject per method call                │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **构造函数注入**：创建对象时传入依赖，最常用
- **Setter 注入**：创建后通过 setter 方法设置依赖
- **接口/方法参数注入**：每次调用方法时传入依赖

---

## 2. 依赖注入前后对比

### 2.1 重构前：紧耦合代码

```c
/* ============================================================================
 * 重构前：OrderService 直接依赖具体实现
 * 问题：紧耦合，难以测试，难以更换实现
 * ============================================================================ */

/* order_service.c */
#include <stdio.h>
#include <mysql/mysql.h>  /* 直接依赖 MySQL */

/* 硬编码的日志实现 */
static void log_to_file(const char *msg) {
    FILE *f = fopen("/var/log/order.log", "a");
    if (f) {
        fprintf(f, "[LOG] %s\n", msg);
        fclose(f);
    }
}

/* 硬编码的数据库实现 */
static int save_to_mysql(int order_id, double amount) {
    MYSQL *conn = mysql_init(NULL);
    mysql_real_connect(conn, "localhost", "root", "password", "orders", 0, NULL, 0);
    
    char query[256];
    snprintf(query, sizeof(query), 
             "INSERT INTO orders VALUES (%d, %f)", order_id, amount);
    mysql_query(conn, query);
    mysql_close(conn);
    return 0;
}

/* OrderService - 紧耦合 */
typedef struct {
    int service_id;
} OrderService;

int order_service_create_order(OrderService *svc, int order_id, double amount) {
    /* 问题 1：直接调用具体的日志实现 */
    log_to_file("Creating order...");
    
    /* 问题 2：直接调用具体的数据库实现 */
    save_to_mysql(order_id, amount);
    
    log_to_file("Order created successfully");
    return 0;
}

/* 
 * 问题总结：
 * 1. 无法在不连接数据库的情况下测试
 * 2. 无法更换日志实现（如改为 syslog）
 * 3. 无法更换数据库（如改为 PostgreSQL）
 * 4. 单元测试需要真实的文件系统和数据库
 */
```

### 2.2 重构后：依赖注入

```c
/* ============================================================================
 * 重构后：通过依赖注入解耦
 * 优点：松耦合，易测试，易更换实现
 * ============================================================================ */

/* ========== 1. 定义抽象接口 ========== */

/* logger.h - 日志接口 */
typedef struct Logger {
    void (*log)(struct Logger *self, const char *msg);
    void (*destroy)(struct Logger *self);
    void *private_data;
} Logger;

/* database.h - 数据库接口 */
typedef struct Database {
    int (*save_order)(struct Database *self, int order_id, double amount);
    int (*find_order)(struct Database *self, int order_id, double *amount);
    void (*destroy)(struct Database *self);
    void *private_data;
} Database;

/* ========== 2. OrderService 只依赖接口 ========== */

/* order_service.h */
typedef struct {
    int service_id;
    Logger *logger;      /* 依赖抽象接口，不是具体实现 */
    Database *database;  /* 依赖抽象接口，不是具体实现 */
} OrderService;

/* 构造函数注入 */
OrderService* order_service_create(Logger *logger, Database *database) {
    OrderService *svc = malloc(sizeof(OrderService));
    svc->logger = logger;      /* 注入日志依赖 */
    svc->database = database;  /* 注入数据库依赖 */
    return svc;
}

int order_service_create_order(OrderService *svc, int order_id, double amount) {
    /* 通过接口调用，不关心具体实现 */
    svc->logger->log(svc->logger, "Creating order...");
    
    int result = svc->database->save_order(svc->database, order_id, amount);
    
    if (result == 0) {
        svc->logger->log(svc->logger, "Order created successfully");
    }
    return result;
}

/* ========== 3. 具体实现（可替换）========== */

/* file_logger.c - 文件日志实现 */
static void file_logger_log(Logger *self, const char *msg) {
    FILE *f = (FILE*)self->private_data;
    fprintf(f, "[FILE] %s\n", msg);
}

Logger* file_logger_create(const char *path) {
    Logger *logger = malloc(sizeof(Logger));
    logger->log = file_logger_log;
    logger->private_data = fopen(path, "a");
    return logger;
}

/* console_logger.c - 控制台日志实现 */
static void console_logger_log(Logger *self, const char *msg) {
    printf("[CONSOLE] %s\n", msg);
}

Logger* console_logger_create(void) {
    Logger *logger = malloc(sizeof(Logger));
    logger->log = console_logger_log;
    logger->private_data = NULL;
    return logger;
}

/* mock_logger.c - 测试用 Mock 实现 */
typedef struct {
    char messages[10][256];
    int count;
} MockLoggerData;

static void mock_logger_log(Logger *self, const char *msg) {
    MockLoggerData *data = (MockLoggerData*)self->private_data;
    strncpy(data->messages[data->count++], msg, 255);
}

Logger* mock_logger_create(void) {
    Logger *logger = malloc(sizeof(Logger));
    MockLoggerData *data = malloc(sizeof(MockLoggerData));
    data->count = 0;
    logger->log = mock_logger_log;
    logger->private_data = data;
    return logger;
}

/* ========== 4. 使用示例 ========== */

/* 生产环境 */
void production_main(void) {
    Logger *logger = file_logger_create("/var/log/order.log");
    Database *db = mysql_database_create("localhost", "root", "password");
    
    OrderService *svc = order_service_create(logger, db);  /* 注入依赖 */
    order_service_create_order(svc, 1001, 99.99);
}

/* 测试环境 */
void test_create_order(void) {
    Logger *logger = mock_logger_create();        /* 使用 Mock */
    Database *db = mock_database_create();        /* 使用 Mock */
    
    OrderService *svc = order_service_create(logger, db);
    order_service_create_order(svc, 1001, 99.99);
    
    /* 验证日志被调用 */
    MockLoggerData *data = (MockLoggerData*)logger->private_data;
    assert(data->count == 2);
    assert(strcmp(data->messages[0], "Creating order...") == 0);
}
```

### 2.3 对比图

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Before vs After Dependency Injection                            │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    BEFORE (Tight Coupling)                   AFTER (Loose Coupling)

    ┌─────────────────────────┐               ┌─────────────────────────┐
    │     OrderService        │               │     OrderService        │
    │                         │               │                         │
    │  log_to_file()  ────────┼──┐            │  Logger *logger ────────┼──► <<interface>>
    │                         │  │            │                         │         Logger
    │  save_to_mysql() ───────┼──┼─┐          │  Database *db ──────────┼──► <<interface>>
    │                         │  │ │          │                         │        Database
    └─────────────────────────┘  │ │          └─────────────────────────┘
                                 │ │                    ▲
                                 │ │                    │ Depends on
                                 ▼ ▼                    │ abstraction
    ┌─────────────────────────────────────┐   ┌────────────────────────────────────────┐
    │  Concrete Implementations           │   │              Implementations           │
    │                                     │   │                                        │
    │  ┌─────────────┐  ┌─────────────┐   │   │  ┌─────────────┐  ┌─────────────┐      │
    │  │ FileLogger  │  │   MySQL     │   │   │  │ FileLogger  │  │ MockLogger  │      │
    │  │ (hardcoded) │  │ (hardcoded) │   │   │  └─────────────┘  └─────────────┘      │
    │  └─────────────┘  └─────────────┘   │   │  ┌─────────────┐  ┌─────────────┐      │
    │                                     │   │  │   MySQL     │  │ MockDatabase│      │
    └─────────────────────────────────────┘   │  └─────────────┘  └─────────────┘      │
                                              └────────────────────────────────────────┘

    Problems:                                 Benefits:
    - Cannot test without DB                  - Test with mocks
    - Cannot change logger                    - Swap implementations
    - Cannot reuse in other contexts          - Reuse anywhere
```

**说明**：
- **重构前**：OrderService 直接创建和使用具体实现，形成紧耦合
- **重构后**：OrderService 只依赖抽象接口，具体实现从外部注入
- **好处**：可以用 Mock 对象测试，可以轻松更换实现

---

## 3. C语言实现依赖注入

### 3.1 核心技术：函数指针 + 结构体

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         C Language: Interface via Function Pointers                     │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   In OOP languages:                     In C:                                       │
    │                                                                                     │
    │   interface Logger {                    typedef struct Logger {                     │
    │       void log(String msg);                 void (*log)(struct Logger*, char*);     │
    │   }                                         void *private_data;                     │
    │                                         } Logger;                                   │
    │                                                                                     │
    │   class FileLogger                      void file_log(Logger *self, char *msg) {    │
    │       implements Logger {                   FILE *f = self->private_data;           │
    │       void log(String msg) {                fprintf(f, "%s\n", msg);                │
    │           // write to file              }                                           │
    │       }                                                                             │
    │   }                                     Logger* file_logger_create() {              │
    │                                             Logger *l = malloc(sizeof(Logger));     │
    │                                             l->log = file_log;                      │
    │                                             l->private_data = fopen(...);           │
    │                                             return l;                               │
    │                                         }                                           │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘


    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                         Memory Layout                                               │
    │                                                                                     │
    │   Logger struct:                                                                    │
    │   ┌─────────────────────────────────────────────────────────────────────────────┐   │
    │   │  log (function pointer)  │  private_data (void pointer)                     │   │
    │   │        8 bytes           │           8 bytes                                │   │
    │   └────────────┬─────────────┴──────────────┬───────────────────────────────────┘   │
    │                │                            │                                       │
    │                ▼                            ▼                                       │
    │   ┌─────────────────────────┐    ┌─────────────────────────────────────────────┐    │
    │   │  file_log() function    │    │  FILE* or implementation-specific data      │    │
    │   │  (in code segment)      │    │  (in heap)                                  │    │
    │   └─────────────────────────┘    └─────────────────────────────────────────────┘    │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- C语言通过**函数指针**模拟接口的多态行为
- `private_data` 存储实现特定的数据（类似于 OOP 中的成员变量）
- 不同的实现设置不同的函数指针，调用者无需关心具体实现

### 3.2 接口定义模式

```c
/* ============================================================================
 * C语言接口定义的标准模式
 * ============================================================================ */

/* 接口结构体模板 */
typedef struct InterfaceName {
    /* 方法 1：返回值 (*方法名)(参数列表) */
    int (*method1)(struct InterfaceName *self, int arg1);
    
    /* 方法 2 */
    void (*method2)(struct InterfaceName *self, const char *arg);
    
    /* 析构函数（可选但推荐） */
    void (*destroy)(struct InterfaceName *self);
    
    /* 私有数据指针 - 存储实现特定的数据 */
    void *private_data;
    
} InterfaceName;

/* 使用示例 */
void use_interface(InterfaceName *iface) {
    /* 通过函数指针调用，实现多态 */
    int result = iface->method1(iface, 42);
    iface->method2(iface, "hello");
}
```

---

## 4. 完整示例：日志系统

### 4.1 接口定义

```c
/* ============================================================================
 * 文件: logger.h
 * 说明: 日志接口定义
 * ============================================================================ */

#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

/* 日志接口 */
typedef struct Logger {
    /* 写日志 */
    void (*log)(struct Logger *self, LogLevel level, const char *msg);
    
    /* 设置日志级别 */
    void (*set_level)(struct Logger *self, LogLevel level);
    
    /* 刷新缓冲区 */
    void (*flush)(struct Logger *self);
    
    /* 销毁 */
    void (*destroy)(struct Logger *self);
    
    /* 私有数据 */
    void *private_data;
    
} Logger;

/* 便捷宏 */
#define LOG_DEBUG(logger, msg) (logger)->log((logger), LOG_DEBUG, (msg))
#define LOG_INFO(logger, msg)  (logger)->log((logger), LOG_INFO, (msg))
#define LOG_WARN(logger, msg)  (logger)->log((logger), LOG_WARN, (msg))
#define LOG_ERROR(logger, msg) (logger)->log((logger), LOG_ERROR, (msg))

#endif /* LOGGER_H */
```

### 4.2 文件日志实现

```c
/* ============================================================================
 * 文件: file_logger.c
 * 说明: 文件日志实现
 * ============================================================================ */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    FILE *file;
    LogLevel min_level;
} FileLoggerData;

static const char* level_name(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO";
        case LOG_WARN:  return "WARN";
        case LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static void file_logger_log(Logger *self, LogLevel level, const char *msg) {
    FileLoggerData *data = (FileLoggerData*)self->private_data;
    
    if (level < data->min_level) return;
    
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    
    fprintf(data->file, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec,
            level_name(level), msg);
}

static void file_logger_set_level(Logger *self, LogLevel level) {
    FileLoggerData *data = (FileLoggerData*)self->private_data;
    data->min_level = level;
}

static void file_logger_flush(Logger *self) {
    FileLoggerData *data = (FileLoggerData*)self->private_data;
    fflush(data->file);
}

static void file_logger_destroy(Logger *self) {
    FileLoggerData *data = (FileLoggerData*)self->private_data;
    if (data->file && data->file != stdout && data->file != stderr) {
        fclose(data->file);
    }
    free(data);
    free(self);
}

/* 工厂函数 */
Logger* file_logger_create(const char *path) {
    Logger *logger = (Logger*)malloc(sizeof(Logger));
    FileLoggerData *data = (FileLoggerData*)malloc(sizeof(FileLoggerData));
    
    data->file = fopen(path, "a");
    data->min_level = LOG_DEBUG;
    
    logger->log = file_logger_log;
    logger->set_level = file_logger_set_level;
    logger->flush = file_logger_flush;
    logger->destroy = file_logger_destroy;
    logger->private_data = data;
    
    return logger;
}
```

### 4.3 控制台日志实现

```c
/* ============================================================================
 * 文件: console_logger.c
 * 说明: 控制台日志实现（带颜色）
 * ============================================================================ */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    LogLevel min_level;
} ConsoleLoggerData;

/* ANSI 颜色代码 */
#define COLOR_RESET  "\033[0m"
#define COLOR_DEBUG  "\033[36m"  /* Cyan */
#define COLOR_INFO   "\033[32m"  /* Green */
#define COLOR_WARN   "\033[33m"  /* Yellow */
#define COLOR_ERROR  "\033[31m"  /* Red */

static void console_logger_log(Logger *self, LogLevel level, const char *msg) {
    ConsoleLoggerData *data = (ConsoleLoggerData*)self->private_data;
    
    if (level < data->min_level) return;
    
    const char *color = COLOR_RESET;
    const char *name = "UNKNOWN";
    
    switch (level) {
        case LOG_DEBUG: color = COLOR_DEBUG; name = "DEBUG"; break;
        case LOG_INFO:  color = COLOR_INFO;  name = "INFO";  break;
        case LOG_WARN:  color = COLOR_WARN;  name = "WARN";  break;
        case LOG_ERROR: color = COLOR_ERROR; name = "ERROR"; break;
    }
    
    printf("%s[%s]%s %s\n", color, name, COLOR_RESET, msg);
}

static void console_logger_set_level(Logger *self, LogLevel level) {
    ConsoleLoggerData *data = (ConsoleLoggerData*)self->private_data;
    data->min_level = level;
}

static void console_logger_flush(Logger *self) {
    (void)self;
    fflush(stdout);
}

static void console_logger_destroy(Logger *self) {
    free(self->private_data);
    free(self);
}

Logger* console_logger_create(void) {
    Logger *logger = (Logger*)malloc(sizeof(Logger));
    ConsoleLoggerData *data = (ConsoleLoggerData*)malloc(sizeof(ConsoleLoggerData));
    
    data->min_level = LOG_DEBUG;
    
    logger->log = console_logger_log;
    logger->set_level = console_logger_set_level;
    logger->flush = console_logger_flush;
    logger->destroy = console_logger_destroy;
    logger->private_data = data;
    
    return logger;
}
```

### 4.4 Mock 日志实现（用于测试）

```c
/* ============================================================================
 * 文件: mock_logger.c
 * 说明: Mock 日志实现（用于单元测试）
 * ============================================================================ */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOG_ENTRIES 100
#define MAX_MSG_LEN 256

typedef struct {
    struct {
        LogLevel level;
        char message[MAX_MSG_LEN];
    } entries[MAX_LOG_ENTRIES];
    int count;
    LogLevel min_level;
} MockLoggerData;

static void mock_logger_log(Logger *self, LogLevel level, const char *msg) {
    MockLoggerData *data = (MockLoggerData*)self->private_data;
    
    if (level < data->min_level) return;
    if (data->count >= MAX_LOG_ENTRIES) return;
    
    data->entries[data->count].level = level;
    strncpy(data->entries[data->count].message, msg, MAX_MSG_LEN - 1);
    data->count++;
}

static void mock_logger_set_level(Logger *self, LogLevel level) {
    MockLoggerData *data = (MockLoggerData*)self->private_data;
    data->min_level = level;
}

static void mock_logger_flush(Logger *self) {
    (void)self; /* No-op for mock */
}

static void mock_logger_destroy(Logger *self) {
    free(self->private_data);
    free(self);
}

/* 工厂函数 */
Logger* mock_logger_create(void) {
    Logger *logger = (Logger*)malloc(sizeof(Logger));
    MockLoggerData *data = (MockLoggerData*)malloc(sizeof(MockLoggerData));
    
    data->count = 0;
    data->min_level = LOG_DEBUG;
    
    logger->log = mock_logger_log;
    logger->set_level = mock_logger_set_level;
    logger->flush = mock_logger_flush;
    logger->destroy = mock_logger_destroy;
    logger->private_data = data;
    
    return logger;
}

/* 测试辅助函数 */
int mock_logger_get_count(Logger *logger) {
    MockLoggerData *data = (MockLoggerData*)logger->private_data;
    return data->count;
}

const char* mock_logger_get_message(Logger *logger, int index) {
    MockLoggerData *data = (MockLoggerData*)logger->private_data;
    if (index < 0 || index >= data->count) return NULL;
    return data->entries[index].message;
}

LogLevel mock_logger_get_level(Logger *logger, int index) {
    MockLoggerData *data = (MockLoggerData*)logger->private_data;
    if (index < 0 || index >= data->count) return LOG_DEBUG;
    return data->entries[index].level;
}
```

### 4.5 使用依赖注入的业务代码

```c
/* ============================================================================
 * 文件: order_processor.c
 * 说明: 使用依赖注入的订单处理器
 * ============================================================================ */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int id;
    double amount;
} Order;

typedef struct {
    Logger *logger;  /* 注入的依赖 */
} OrderProcessor;

/* 构造函数注入 */
OrderProcessor* order_processor_create(Logger *logger) {
    OrderProcessor *proc = (OrderProcessor*)malloc(sizeof(OrderProcessor));
    proc->logger = logger;  /* 保存注入的依赖 */
    return proc;
}

void order_processor_destroy(OrderProcessor *proc) {
    /* 注意：不销毁 logger，因为它是外部注入的 */
    free(proc);
}

int order_processor_process(OrderProcessor *proc, Order *order) {
    char msg[256];
    
    /* 使用注入的 logger，不关心具体实现 */
    snprintf(msg, sizeof(msg), "Processing order #%d", order->id);
    proc->logger->log(proc->logger, LOG_INFO, msg);
    
    /* 业务逻辑 */
    if (order->amount <= 0) {
        proc->logger->log(proc->logger, LOG_ERROR, "Invalid order amount");
        return -1;
    }
    
    if (order->amount > 10000) {
        proc->logger->log(proc->logger, LOG_WARN, "Large order, needs approval");
    }
    
    snprintf(msg, sizeof(msg), "Order #%d processed, amount: %.2f", 
             order->id, order->amount);
    proc->logger->log(proc->logger, LOG_INFO, msg);
    
    return 0;
}

/* ========== 生产环境使用 ========== */
void production_example(void) {
    /* 创建文件日志 */
    Logger *logger = file_logger_create("/var/log/orders.log");
    
    /* 注入到处理器 */
    OrderProcessor *proc = order_processor_create(logger);
    
    Order order = { .id = 1001, .amount = 99.99 };
    order_processor_process(proc, &order);
    
    /* 清理 */
    order_processor_destroy(proc);
    logger->destroy(logger);
}

/* ========== 单元测试 ========== */
void test_order_processor(void) {
    /* 使用 Mock 日志 */
    Logger *logger = mock_logger_create();
    OrderProcessor *proc = order_processor_create(logger);
    
    /* 测试正常订单 */
    Order order = { .id = 1001, .amount = 99.99 };
    int result = order_processor_process(proc, &order);
    
    /* 验证结果 */
    assert(result == 0);
    assert(mock_logger_get_count(logger) == 2);
    assert(strstr(mock_logger_get_message(logger, 0), "Processing order") != NULL);
    
    /* 测试无效订单 */
    Order invalid = { .id = 1002, .amount = -10 };
    result = order_processor_process(proc, &invalid);
    
    assert(result == -1);
    assert(mock_logger_get_level(logger, 3) == LOG_ERROR);
    
    printf("All tests passed!\n");
    
    order_processor_destroy(proc);
    logger->destroy(logger);
}
```

---

## 5. 如何识别并重构

### 5.1 识别需要依赖注入的代码

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Code Smells: When to Use Dependency Injection                  │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Smell 1: Direct instantiation of dependencies                                    │
    │                                                                                     │
    │   void process() {                                                                  │
    │       FileLogger *log = file_logger_create();  // <-- SMELL!                        │
    │       log->write("...");                                                            │
    │   }                                                                                 │
    │                                                                                     │
    │   Problem: Cannot replace FileLogger with another implementation                    │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Smell 2: Global variables for services                                           │
    │                                                                                     │
    │   static Database *g_database;  // <-- SMELL!                                       │
    │                                                                                     │
    │   void save_order(Order *order) {                                                   │
    │       g_database->save(order);                                                      │
    │   }                                                                                 │
    │                                                                                     │
    │   Problem: Hidden dependency, hard to test, not thread-safe                         │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Smell 3: Hardcoded external system calls                                         │
    │                                                                                     │
    │   void notify_user(int user_id) {                                                   │
    │       send_email("smtp.example.com", user_id, "...");  // <-- SMELL!                │
    │       send_sms("+1234567890", "...");                  // <-- SMELL!                │
    │   }                                                                                 │
    │                                                                                     │
    │   Problem: Cannot test without real email/SMS, cannot mock                          │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Smell 4: Conditional logic to switch implementations                             │
    │                                                                                     │
    │   void log_message(const char *msg) {                                               │
    │       if (use_file) {                      // <-- SMELL!                            │
    │           write_to_file(msg);                                                       │
    │       } else if (use_syslog) {                                                      │
    │           write_to_syslog(msg);                                                     │
    │       } else {                                                                      │
    │           write_to_console(msg);                                                    │
    │       }                                                                             │
    │   }                                                                                 │
    │                                                                                     │
    │   Problem: Violates Open/Closed principle, grows with each new implementation       │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **直接实例化**：函数内部创建具体类型的对象
- **全局变量**：使用全局变量存储服务实例
- **硬编码调用**：直接调用外部系统（数据库、网络等）
- **条件分支切换**：用 if-else 切换不同实现

### 5.2 重构步骤

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Refactoring Steps for Dependency Injection                      │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Step 1: Identify the dependency
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   void order_service_process(OrderService *svc) {                                   │
    │       FileLogger *log = file_logger_create();  // <-- This is the dependency        │
    │       log->write("Processing...");                                                  │
    │       // ...                                                                        │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 2: Extract interface
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   typedef struct Logger {                                                           │
    │       void (*write)(struct Logger *self, const char *msg);                          │
    │       void (*destroy)(struct Logger *self);                                         │
    │       void *private_data;                                                           │
    │   } Logger;                                                                         │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 3: Add dependency to struct
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   typedef struct OrderService {                                                     │
    │       int id;                                                                       │
    │       Logger *logger;   // <-- Add dependency field                                 │
    │   } OrderService;                                                                   │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 4: Inject via constructor
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   OrderService* order_service_create(Logger *logger) {                              │
    │       OrderService *svc = malloc(sizeof(OrderService));                             │
    │       svc->logger = logger;  // <-- Inject dependency                               │
    │       return svc;                                                                   │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 5: Use interface in business logic
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   void order_service_process(OrderService *svc) {                                   │
    │       svc->logger->write(svc->logger, "Processing...");  // Use interface           │
    │       // ...                                                                        │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 6: Create concrete implementations
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Logger* file_logger_create(const char *path);    // Production                    │
    │   Logger* console_logger_create(void);             // Development                   │
    │   Logger* mock_logger_create(void);                // Testing                       │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
1. **识别依赖**：找到函数内部创建的具体对象
2. **提取接口**：定义包含函数指针的结构体
3. **添加依赖字段**：在主结构体中添加接口指针
4. **构造函数注入**：通过创建函数传入依赖
5. **使用接口**：业务逻辑只调用接口方法
6. **实现接口**：创建不同的具体实现

---

## 6. 核心思想与实现步骤

### 6.1 核心思想

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Core Principles of Dependency Injection                         │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Principle 1: Depend on Abstractions, Not Concretions                              │
    │                                                                                     │
    │   ┌───────────────────────────────────────────────────────────────────────────┐     │
    │   │                                                                           │     │
    │   │   BAD:  OrderService --> FileLogger (concrete)                            │     │
    │   │                                                                           │     │
    │   │   GOOD: OrderService --> Logger (interface) <-- FileLogger                │     │
    │   │                                             <-- ConsoleLogger             │     │
    │   │                                             <-- MockLogger                │     │
    │   │                                                                           │     │
    │   └───────────────────────────────────────────────────────────────────────────┘     │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Principle 2: Inversion of Control (IoC)                                           │
    │                                                                                     │
    │   ┌───────────────────────────────────────────────────────────────────────────┐     │
    │   │                                                                           │     │
    │   │   Traditional: Module creates its dependencies                            │     │
    │   │                                                                           │     │
    │   │       OrderService                                                        │     │
    │   │           │                                                               │     │
    │   │           │ creates                                                       │     │
    │   │           ▼                                                               │     │
    │   │       FileLogger                                                          │     │
    │   │                                                                           │     │
    │   │   IoC: Dependencies are provided to module                                │     │
    │   │                                                                           │     │
    │   │       Main/Container                                                      │     │
    │   │           │                                                               │     │
    │   │           │ creates                                                       │     │
    │   │           ▼                                                               │     │
    │   │       FileLogger ──────► OrderService                                     │     │
    │   │                  injects                                                  │     │
    │   │                                                                           │     │
    │   └───────────────────────────────────────────────────────────────────────────┘     │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘

    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   Principle 3: Single Responsibility for Object Creation                            │
    │                                                                                     │
    │   ┌───────────────────────────────────────────────────────────────────────────┐     │
    │   │                                                                           │     │
    │   │   "A class should not be responsible for creating its dependencies"       │     │
    │   │                                                                           │     │
    │   │   Separate concerns:                                                      │     │
    │   │   - Business logic: OrderService                                          │     │
    │   │   - Object creation: Factory or Main function                             │     │
    │   │                                                                           │     │
    │   └───────────────────────────────────────────────────────────────────────────┘     │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
- **依赖抽象**：模块只依赖接口，不依赖具体实现
- **控制反转**：对象不再自己创建依赖，而是由外部提供
- **单一职责**：业务逻辑和对象创建分离

### 6.2 C语言实现步骤

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                         Implementation Steps in C                                       │
└─────────────────────────────────────────────────────────────────────────────────────────┘

    Step 1: Define Interface (Header File)
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   /* logger.h */                                                                    │
    │   typedef struct Logger {                                                           │
    │       void (*log)(struct Logger *self, const char *msg);                            │
    │       void (*destroy)(struct Logger *self);                                         │
    │       void *private_data;                                                           │
    │   } Logger;                                                                         │
    │                                                                                     │
    │   /* Factory function declarations */                                               │
    │   Logger* file_logger_create(const char *path);                                     │
    │   Logger* console_logger_create(void);                                              │
    │   Logger* mock_logger_create(void);                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 2: Implement Concrete Types
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   /* file_logger.c */                                                               │
    │   typedef struct {                                                                  │
    │       FILE *file;                                                                   │
    │   } FileLoggerData;                                                                 │
    │                                                                                     │
    │   static void file_log(Logger *self, const char *msg) {                             │
    │       FileLoggerData *data = self->private_data;                                    │
    │       fprintf(data->file, "%s\n", msg);                                             │
    │   }                                                                                 │
    │                                                                                     │
    │   Logger* file_logger_create(const char *path) {                                    │
    │       Logger *l = malloc(sizeof(Logger));                                           │
    │       FileLoggerData *d = malloc(sizeof(FileLoggerData));                           │
    │       d->file = fopen(path, "a");                                                   │
    │       l->log = file_log;                                                            │
    │       l->private_data = d;                                                          │
    │       return l;                                                                     │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 3: Add Dependency Field to Consumer
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   typedef struct OrderService {                                                     │
    │       Logger *logger;     /* Dependency field */                                    │
    │       Database *db;       /* Another dependency */                                  │
    │   } OrderService;                                                                   │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 4: Constructor Injection
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   OrderService* order_service_create(Logger *logger, Database *db) {                │
    │       OrderService *svc = malloc(sizeof(OrderService));                             │
    │       svc->logger = logger;    /* Inject */                                         │
    │       svc->db = db;            /* Inject */                                         │
    │       return svc;                                                                   │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 5: Use Interface in Business Logic
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   void order_service_process(OrderService *svc, Order *order) {                     │
    │       /* Use interface, not concrete type */                                        │
    │       svc->logger->log(svc->logger, "Processing order...");                         │
    │       svc->db->save(svc->db, order);                                                │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
    Step 6: Composition Root (Main Function)
    ┌─────────────────────────────────────────────────────────────────────────────────────┐
    │                                                                                     │
    │   int main() {                                                                      │
    │       /* Create all dependencies */                                                 │
    │       Logger *logger = file_logger_create("/var/log/app.log");                      │
    │       Database *db = mysql_database_create("localhost", "app");                     │
    │                                                                                     │
    │       /* Inject dependencies */                                                     │
    │       OrderService *svc = order_service_create(logger, db);                         │
    │                                                                                     │
    │       /* Use service */                                                             │
    │       Order order = { .id = 1, .amount = 99.99 };                                   │
    │       order_service_process(svc, &order);                                           │
    │                                                                                     │
    │       /* Cleanup */                                                                 │
    │       order_service_destroy(svc);                                                   │
    │       logger->destroy(logger);                                                      │
    │       db->destroy(db);                                                              │
    │                                                                                     │
    │       return 0;                                                                     │
    │   }                                                                                 │
    │                                                                                     │
    └─────────────────────────────────────────────────────────────────────────────────────┘
```

**说明**：
1. **定义接口**：使用函数指针结构体定义抽象接口
2. **实现接口**：为每种实现创建工厂函数
3. **添加依赖字段**：在消费者结构体中添加接口指针
4. **构造函数注入**：通过创建函数传入依赖
5. **使用接口**：业务逻辑只调用接口方法
6. **组合根**：在 main 函数中创建和组装所有对象

### 6.3 总结

| 方面 | 说明 |
|------|------|
| **核心思想** | 依赖抽象，控制反转，对象创建与使用分离 |
| **C语言实现** | 函数指针 + 结构体 模拟接口多态 |
| **注入方式** | 构造函数注入（最常用）、Setter 注入、参数注入 |
| **识别时机** | 直接实例化、全局变量、硬编码调用、条件分支切换 |
| **好处** | 松耦合、易测试、易更换实现、易复用 |

---

## 附录：快速参考

```c
/* ============================================================================
 * 依赖注入快速参考模板
 * ============================================================================ */

/* 1. 接口定义模板 */
typedef struct InterfaceName {
    ReturnType (*method)(struct InterfaceName *self, Args...);
    void (*destroy)(struct InterfaceName *self);
    void *private_data;
} InterfaceName;

/* 2. 实现模板 */
typedef struct {
    /* 实现特定的数据 */
} ImplData;

static ReturnType impl_method(InterfaceName *self, Args...) {
    ImplData *data = (ImplData*)self->private_data;
    /* 实现逻辑 */
}

InterfaceName* impl_create(Args...) {
    InterfaceName *iface = malloc(sizeof(InterfaceName));
    ImplData *data = malloc(sizeof(ImplData));
    iface->method = impl_method;
    iface->private_data = data;
    return iface;
}

/* 3. 消费者模板 */
typedef struct Consumer {
    InterfaceName *dependency;
} Consumer;

Consumer* consumer_create(InterfaceName *dependency) {
    Consumer *c = malloc(sizeof(Consumer));
    c->dependency = dependency;  /* 注入 */
    return c;
}

void consumer_do_work(Consumer *c) {
    c->dependency->method(c->dependency, args);  /* 使用接口 */
}

/* 4. 组合根模板 */
int main() {
    InterfaceName *dep = impl_create(args);
    Consumer *c = consumer_create(dep);
    consumer_do_work(c);
    consumer_destroy(c);
    dep->destroy(dep);
    return 0;
}
```

