# X-Macro 技术

## 定义

X-Macro是一种C语言预处理器技术，通过定义一个包含数据的宏列表，然后多次展开这个列表来生成不同形式的代码（如枚举定义、字符串表、switch语句等）。这确保了相关代码的一致性，遵循DRY（Don't Repeat Yourself）原则。

## 适用场景

- 错误码定义及其对应的错误消息
- 状态机的状态定义和处理函数
- 命令解析器的命令列表
- 配置项的定义和初始化
- 协议消息类型的定义
- 日志级别的定义
- 需要枚举值与字符串互转的场景

## ASCII 图解

```
+------------------------------------------------------------------------+
|                         X-MACRO TECHNIQUE                               |
+------------------------------------------------------------------------+
|                                                                         |
|   SINGLE SOURCE OF TRUTH (Define once):                                 |
|   +--------------------------------------------------+                  |
|   | #define ERROR_LIST(X)                            |                  |
|   |     X(ERR_OK,       0,   "Success")              |                  |
|   |     X(ERR_NOMEM,    1,   "Out of memory")        |                  |
|   |     X(ERR_IO,       2,   "I/O error")            |                  |
|   |     X(ERR_TIMEOUT,  3,   "Operation timeout")    |                  |
|   |     X(ERR_INVALID,  4,   "Invalid argument")     |                  |
|   +--------------------------------------------------+                  |
|                              |                                          |
|                              | Expand with different X() definitions    |
|                              |                                          |
|         +--------------------+--------------------+                     |
|         |                    |                    |                     |
|         v                    v                    v                     |
|   +-----------+       +-------------+       +---------------+           |
|   | ENUM      |       | STRING      |       | SWITCH        |           |
|   | DEFINITION|       | TABLE       |       | STATEMENT     |           |
|   +-----------+       +-------------+       +---------------+           |
|   | enum {    |       | const char* |       | switch(e) {   |           |
|   |  ERR_OK=0,|       | err_str[]={ |       |  case ERR_OK: |           |
|   |  ERR_NOMEM|       |  "Success", |       |   return "OK";|           |
|   |  ...      |       |  "Out of.." |       |  case ERR_..  |           |
|   | };        |       | };          |       | }             |           |
|   +-----------+       +-------------+       +---------------+           |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   HOW IT WORKS:                                                         |
|                                                                         |
|   Step 1: Define macro that takes X as parameter                        |
|   +---------------------------------------+                             |
|   | #define LIST(X)  X(A) X(B) X(C)       |                             |
|   +---------------------------------------+                             |
|                                                                         |
|   Step 2: Define X to generate specific code                            |
|   +---------------------------------------+                             |
|   | #define X_ENUM(name) name,            |  -> generates: A, B, C,     |
|   | #define X_STR(name) #name,            |  -> generates: "A","B","C", |
|   +---------------------------------------+                             |
|                                                                         |
|   Step 3: Invoke LIST with different X definitions                      |
|   +---------------------------------------+                             |
|   | enum { LIST(X_ENUM) };                |  -> enum { A, B, C, };      |
|   | char* names[] = { LIST(X_STR) };      |  -> {"A", "B", "C"};        |
|   +---------------------------------------+                             |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了X-Macro技术的核心思想。顶部定义了一个包含所有错误码数据的宏列表（ERROR_LIST），每个条目包含名称、数值和描述。通过定义不同的"X"宏，可以从同一个数据源生成枚举定义、字符串表、switch语句等不同形式的代码。这样添加或删除条目时，只需修改一处，所有相关代码都会自动同步更新，消除了维护多处重复代码的风险。

## 实现方法

1. 定义包含数据的宏列表，每项调用一个占位符宏X
2. 使用前`#define`将X定义为需要的代码生成形式
3. 展开列表宏生成代码
4. 使用后`#undef`取消X的定义（避免污染）

## C语言代码示例

### 基础示例：错误码系统

```c
// error_codes.h
#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// ==================== X-Macro数据定义 ====================
// 格式: X(枚举名, 数值, 描述字符串)

#define ERROR_CODE_LIST(X)                                          \
    X(ERR_OK,              0,    "Success")                         \
    X(ERR_UNKNOWN,         1,    "Unknown error")                   \
    X(ERR_NULL_POINTER,    2,    "Null pointer")                    \
    X(ERR_OUT_OF_MEMORY,   3,    "Out of memory")                   \
    X(ERR_INVALID_PARAM,   4,    "Invalid parameter")               \
    X(ERR_FILE_NOT_FOUND,  5,    "File not found")                  \
    X(ERR_PERMISSION,      6,    "Permission denied")               \
    X(ERR_TIMEOUT,         7,    "Operation timeout")               \
    X(ERR_OVERFLOW,        8,    "Buffer overflow")                 \
    X(ERR_NOT_SUPPORTED,   9,    "Operation not supported")         \
    X(ERR_BUSY,           10,    "Resource busy")                   \
    X(ERR_CONNECTION,     11,    "Connection failed")

// ==================== 生成枚举定义 ====================
#define X_ENUM(name, value, desc) name = value,

typedef enum {
    ERROR_CODE_LIST(X_ENUM)
    ERR_COUNT  // 自动计算错误码数量
} ErrorCode;

#undef X_ENUM

// ==================== API声明 ====================
const char* error_get_name(ErrorCode code);
const char* error_get_description(ErrorCode code);
ErrorCode error_from_name(const char* name);
void error_print_all(void);

#endif // ERROR_CODES_H
```

```c
// error_codes.c
#include "error_codes.h"
#include <stdio.h>
#include <string.h>

// ==================== 生成名称字符串表 ====================
#define X_NAME(name, value, desc) [name] = #name,

static const char* error_names[] = {
    ERROR_CODE_LIST(X_NAME)
};

#undef X_NAME

// ==================== 生成描述字符串表 ====================
#define X_DESC(name, value, desc) [name] = desc,

static const char* error_descriptions[] = {
    ERROR_CODE_LIST(X_DESC)
};

#undef X_DESC

// ==================== API实现 ====================

const char* error_get_name(ErrorCode code) {
    if (code >= 0 && code < ERR_COUNT) {
        return error_names[code];
    }
    return "ERR_INVALID";
}

const char* error_get_description(ErrorCode code) {
    if (code >= 0 && code < ERR_COUNT) {
        return error_descriptions[code];
    }
    return "Invalid error code";
}

ErrorCode error_from_name(const char* name) {
    // 生成switch语句进行查找
    #define X_CASE(ename, value, desc) \
        if (strcmp(name, #ename) == 0) return ename;
    
    ERROR_CODE_LIST(X_CASE)
    
    #undef X_CASE
    
    return ERR_UNKNOWN;
}

void error_print_all(void) {
    printf("\n+----+----------------------+---------------------------+\n");
    printf("| %-2s | %-20s | %-25s |\n", "ID", "Name", "Description");
    printf("+----+----------------------+---------------------------+\n");
    
    #define X_PRINT(name, value, desc) \
        printf("| %-2d | %-20s | %-25s |\n", value, #name, desc);
    
    ERROR_CODE_LIST(X_PRINT)
    
    #undef X_PRINT
    
    printf("+----+----------------------+---------------------------+\n\n");
}
```

### 高级示例：命令解析器

```c
// command_parser.h
#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

// ==================== 命令列表定义 ====================
// 格式: X(命令名, 处理函数, 帮助信息)

#define COMMAND_LIST(X)                                             \
    X(help,    cmd_help,    "Show this help message")               \
    X(version, cmd_version, "Show version information")             \
    X(status,  cmd_status,  "Show current status")                  \
    X(start,   cmd_start,   "Start the service")                    \
    X(stop,    cmd_stop,    "Stop the service")                     \
    X(restart, cmd_restart, "Restart the service")                  \
    X(config,  cmd_config,  "Show/edit configuration")              \
    X(list,    cmd_list,    "List all items")                       \
    X(clear,   cmd_clear,   "Clear the screen")                     \
    X(exit,    cmd_exit,    "Exit the program")

// 命令处理函数类型
typedef int (*CommandHandler)(int argc, char* argv[]);

// 命令结构
typedef struct {
    const char* name;
    CommandHandler handler;
    const char* help;
} Command;

// API
int command_execute(const char* name, int argc, char* argv[]);
void command_print_help(void);
const Command* command_find(const char* name);

#endif
```

```c
// command_parser.c
#include "command_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ==================== 命令处理函数声明 ====================
#define X_DECLARE(name, handler, help) static int handler(int argc, char* argv[]);

COMMAND_LIST(X_DECLARE)

#undef X_DECLARE

// ==================== 命令表生成 ====================
#define X_TABLE(name, handler, help) { #name, handler, help },

static const Command commands[] = {
    COMMAND_LIST(X_TABLE)
    { NULL, NULL, NULL }  // 结束标记
};

#undef X_TABLE

// ==================== 命令处理函数实现 ====================

static int cmd_help(int argc, char* argv[]) {
    (void)argc; (void)argv;
    command_print_help();
    return 0;
}

static int cmd_version(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("Version 1.0.0 (Build 2024-01-01)\n");
    return 0;
}

static int cmd_status(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("Status: Running\n");
    printf("Uptime: 3 days, 4 hours\n");
    printf("Memory: 128 MB used\n");
    return 0;
}

static int cmd_start(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("Starting service...\n");
    printf("Service started successfully.\n");
    return 0;
}

static int cmd_stop(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("Stopping service...\n");
    printf("Service stopped.\n");
    return 0;
}

static int cmd_restart(int argc, char* argv[]) {
    cmd_stop(0, NULL);
    cmd_start(0, NULL);
    return 0;
}

static int cmd_config(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Current configuration:\n");
        printf("  port = 8080\n");
        printf("  host = localhost\n");
    } else {
        printf("Setting %s...\n", argv[1]);
    }
    return 0;
}

static int cmd_list(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("Items:\n");
    printf("  1. Item A\n");
    printf("  2. Item B\n");
    printf("  3. Item C\n");
    return 0;
}

static int cmd_clear(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("\033[2J\033[H");  // ANSI清屏
    return 0;
}

static int cmd_exit(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("Goodbye!\n");
    exit(0);
    return 0;
}

// ==================== API实现 ====================

const Command* command_find(const char* name) {
    for (const Command* cmd = commands; cmd->name != NULL; cmd++) {
        if (strcmp(cmd->name, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

int command_execute(const char* name, int argc, char* argv[]) {
    const Command* cmd = command_find(name);
    if (cmd && cmd->handler) {
        return cmd->handler(argc, argv);
    }
    printf("Unknown command: %s\n");
    printf("Type 'help' for available commands.\n");
    return -1;
}

void command_print_help(void) {
    printf("\nAvailable commands:\n");
    printf("+----------+----------------------------------+\n");
    printf("| %-8s | %-32s |\n", "Command", "Description");
    printf("+----------+----------------------------------+\n");
    
    for (const Command* cmd = commands; cmd->name != NULL; cmd++) {
        printf("| %-8s | %-32s |\n", cmd->name, cmd->help);
    }
    
    printf("+----------+----------------------------------+\n\n");
}
```

### 状态机示例

```c
// state_machine_xmacro.c
#include <stdio.h>

// ==================== 状态定义 ====================
#define STATE_LIST(X)                   \
    X(STATE_IDLE,      "Idle")          \
    X(STATE_STARTING,  "Starting")      \
    X(STATE_RUNNING,   "Running")       \
    X(STATE_PAUSED,    "Paused")        \
    X(STATE_STOPPING,  "Stopping")      \
    X(STATE_ERROR,     "Error")

// 生成枚举
#define X_ENUM(name, desc) name,
typedef enum { STATE_LIST(X_ENUM) STATE_COUNT } State;
#undef X_ENUM

// 生成字符串表
#define X_STRING(name, desc) [name] = desc,
static const char* state_names[] = { STATE_LIST(X_STRING) };
#undef X_STRING

// ==================== 事件定义 ====================
#define EVENT_LIST(X)                   \
    X(EVENT_START,    "Start")          \
    X(EVENT_PAUSE,    "Pause")          \
    X(EVENT_RESUME,   "Resume")         \
    X(EVENT_STOP,     "Stop")           \
    X(EVENT_ERROR,    "Error")          \
    X(EVENT_RESET,    "Reset")

#define X_ENUM(name, desc) name,
typedef enum { EVENT_LIST(X_ENUM) EVENT_COUNT } Event;
#undef X_ENUM

#define X_STRING(name, desc) [name] = desc,
static const char* event_names[] = { EVENT_LIST(X_STRING) };
#undef X_STRING

// ==================== 状态转换表 ====================
// 使用X-Macro定义转换规则

#define TRANSITION_LIST(X)                              \
    X(STATE_IDLE,     EVENT_START,  STATE_STARTING)     \
    X(STATE_STARTING, EVENT_START,  STATE_RUNNING)      \
    X(STATE_RUNNING,  EVENT_PAUSE,  STATE_PAUSED)       \
    X(STATE_RUNNING,  EVENT_STOP,   STATE_STOPPING)     \
    X(STATE_PAUSED,   EVENT_RESUME, STATE_RUNNING)      \
    X(STATE_PAUSED,   EVENT_STOP,   STATE_STOPPING)     \
    X(STATE_STOPPING, EVENT_STOP,   STATE_IDLE)         \
    X(STATE_ERROR,    EVENT_RESET,  STATE_IDLE)

typedef struct {
    State from;
    Event event;
    State to;
} Transition;

#define X_TRANSITION(from, event, to) { from, event, to },
static const Transition transitions[] = {
    TRANSITION_LIST(X_TRANSITION)
    { -1, -1, -1 }  // 结束标记
};
#undef X_TRANSITION

// ==================== 状态机实现 ====================

typedef struct {
    State current;
} StateMachine;

void sm_init(StateMachine* sm) {
    sm->current = STATE_IDLE;
    printf("[SM] Initialized to %s\n", state_names[sm->current]);
}

int sm_handle_event(StateMachine* sm, Event event) {
    printf("[SM] Event: %s in state %s\n", 
           event_names[event], state_names[sm->current]);
    
    for (const Transition* t = transitions; t->from != -1; t++) {
        if (t->from == sm->current && t->event == event) {
            printf("[SM] Transition: %s -> %s\n",
                   state_names[sm->current], state_names[t->to]);
            sm->current = t->to;
            return 0;
        }
    }
    
    printf("[SM] No valid transition found!\n");
    return -1;
}

// ==================== 使用示例 ====================

int main() {
    printf("=== X-Macro State Machine Demo ===\n\n");
    
    StateMachine sm;
    sm_init(&sm);
    
    printf("\n--- State transitions ---\n");
    sm_handle_event(&sm, EVENT_START);   // IDLE -> STARTING
    sm_handle_event(&sm, EVENT_START);   // STARTING -> RUNNING
    sm_handle_event(&sm, EVENT_PAUSE);   // RUNNING -> PAUSED
    sm_handle_event(&sm, EVENT_RESUME);  // PAUSED -> RUNNING
    sm_handle_event(&sm, EVENT_STOP);    // RUNNING -> STOPPING
    sm_handle_event(&sm, EVENT_STOP);    // STOPPING -> IDLE
    
    printf("\n--- Invalid transition ---\n");
    sm_handle_event(&sm, EVENT_PAUSE);   // IDLE -> ? (invalid)
    
    return 0;
}

/* 输出:
=== X-Macro State Machine Demo ===

[SM] Initialized to Idle

--- State transitions ---
[SM] Event: Start in state Idle
[SM] Transition: Idle -> Starting
[SM] Event: Start in state Starting
[SM] Transition: Starting -> Running
[SM] Event: Pause in state Running
[SM] Transition: Running -> Paused
[SM] Event: Resume in state Paused
[SM] Transition: Paused -> Running
[SM] Event: Stop in state Running
[SM] Transition: Running -> Stopping
[SM] Event: Stop in state Stopping
[SM] Transition: Stopping -> Idle

--- Invalid transition ---
[SM] Event: Pause in state Idle
[SM] No valid transition found!
*/
```

## 优缺点

### 优点
- **单一数据源**：所有相关代码从一个定义生成，保持一致性
- **易于维护**：添加/删除/修改条目只需改一处
- **减少错误**：避免多处代码不同步的风险
- **编译时生成**：零运行时开销
- **灵活性强**：可生成各种形式的代码

### 缺点
- 代码可读性降低，对新手不友好
- 调试困难（宏展开后的代码不易追踪）
- IDE支持有限（自动补全、跳转等可能失效）
- 复杂的宏定义容易出错
- 预处理器错误信息难以理解

