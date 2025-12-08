# DI + Service Locator + Strategy Pattern - Complete C Example

## 1. Design Overview

This example demonstrates a **Data Processor System** that combines three design patterns:

- **Dependency Injection (DI)**: External modules inject their processing callbacks
- **Service Locator**: Central registry for processor lookup by name/ID
- **Strategy Pattern**: Format selection (TEXT/JSON/XML) based on capabilities

---

## 2. Architecture Diagram

```
+==================================================================================================+
|                     Data Processor System - Three Patterns Combined                               |
+==================================================================================================+

                              DEPENDENCY INJECTION
                              (External modules inject processors)
    +-------------------+     +-------------------+     +-------------------+
    |   Temperature     |     |   Humidity        |     |   Pressure        |
    |   Processor       |     |   Processor       |     |   Processor       |
    +-------------------+     +-------------------+     +-------------------+
    | name = "temp"     |     | name = "humidity" |     | name = "pressure" |
    | caps = TEXT|JSON  |     | caps = TEXT       |     | caps = TEXT|JSON  |
    | process_cb = A()  |     | process_cb = B()  |     | process_cb = C()  |
    +-------------------+     +-------------------+     +-------------------+
            |                         |                         |
            |  processor_t            |  processor_t            |  processor_t
            |  (Interface)            |  (Interface)            |  (Interface)
            |                         |                         |
            +-------------------------+-------------------------+
                                      |
                                      v  register_processor()
                        +-----------------------------+
                        |   INJECTION POINT           |
                        +-----------------------------+
                                      |
                                      v
+==================================================================================================+
|                              SERVICE LOCATOR (Central Registry)                                   |
|                                                                                                   |
|   processor_registry[MAX_PROCESSORS]                                                             |
|   +--------+--------+--------+--------+--------+                                                 |
|   | ptr[0] | ptr[1] | ptr[2] | ptr[3] | ...    |                                                 |
|   +---+----+---+----+---+----+--------+--------+                                                 |
|       |        |        |                                                                         |
|       v        v        v                                                                         |
|   +-------+ +--------+ +--------+                                                                |
|   | temp  | |humidity| |pressure|   <-- Registered Processors                                   |
|   +-------+ +--------+ +--------+                                                                |
|                                                                                                   |
|   Lookup Methods:                                                                                |
|   - get_processor_by_name(name)  --> O(n) string match                                          |
|   - get_processor_by_id(id)      --> O(1) direct index                                          |
+==================================================================================================+
                                      |
                                      v
+==================================================================================================+
|                              STRATEGY PATTERN (Format Selection)                                  |
|                                                                                                   |
|   Capabilities Bitmask:                                                                          |
|   +--------+--------+--------+                                                                   |
|   |  TEXT  |  JSON  |  XML   |   = 0x01, 0x02, 0x04                                             |
|   +--------+--------+--------+                                                                   |
|                                                                                                   |
|   Strategy Selection at Registration:                                                            |
|   +------------------------------------------------------------------+                           |
|   |  if(caps & CAP_TEXT)  --> format_text_cb = format_text()        |                           |
|   |  else                 --> format_text_cb = not_supported()      |                           |
|   |                                                                  |                           |
|   |  if(caps & CAP_JSON)  --> format_json_cb = format_json()        |                           |
|   |  else                 --> format_json_cb = not_supported()      |                           |
|   +------------------------------------------------------------------+                           |
|                                                                                                   |
|   Two Concrete Strategies:                                                                       |
|   +-----------------------+     +-----------------------+                                        |
|   | Strategy A:           |     | Strategy B:           |                                        |
|   | format_xxx()          |     | not_supported()       |                                        |
|   | - Calls process_cb()  |     | - Returns error msg   |                                        |
|   | - Formats output      |     | - Graceful rejection  |                                        |
|   +-----------------------+     +-----------------------+                                        |
+==================================================================================================+
```

**中文说明：**

这个数据处理器系统展示了三种设计模式的综合应用：

1. **依赖注入（上层）**：
   - 外部模块（温度、湿度、气压处理器）创建 `processor_t` 结构
   - 每个结构包含模块特定的 `process_cb` 回调函数
   - 通过 `register_processor()` 将依赖注入到注册表

2. **服务定位器（中层）**：
   - `processor_registry[]` 数组作为集中注册表
   - 提供按名称查找 `get_processor_by_name()` 和按 ID 查找 `get_processor_by_id()`
   - 客户端不需要知道处理器的创建细节

3. **策略模式（下层）**：
   - `capabilities` 位掩码声明支持的输出格式
   - 注册时根据能力自动绑定格式化策略
   - 不支持的格式使用 `not_supported()` 策略优雅拒绝

---

## 3. Data Flow Diagram

```
+==================================================================================================+
|                              Complete Data Flow                                                   |
+==================================================================================================+

    CLIENT                         REGISTRY                        PROCESSOR MODULE
       |                              |                                    |
       |  1. process("temp", JSON)    |                                    |
       |----------------------------->|                                    |
       |                              |                                    |
       |                              |  2. get_processor_by_name("temp")  |
       |                              |     [SERVICE LOCATOR]              |
       |                              |                                    |
       |                              |  3. Check: caps & CAP_JSON?        |
       |                              |     [STRATEGY SELECTION]           |
       |                              |                                    |
       |                              |  4. Call format_json_cb()          |
       |                              |     [STRATEGY EXECUTION]           |
       |                              |                                    |
       |                              |--------------------------------->  |
       |                              |  5. Call process_cb()              |
       |                              |     [DEPENDENCY INJECTION]         |
       |                              |                                    |
       |                              |  <---------------------------------|
       |                              |  6. Return raw data                |
       |                              |                                    |
       |                              |  7. Format as JSON                 |
       |                              |                                    |
       |  <---------------------------|                                    |
       |  8. Return: {"temp": 25.5}   |                                    |
       |                              |                                    |
```

**中文说明：**

完整的数据处理流程：

| 步骤 | 涉及模式 | 说明 |
|------|----------|------|
| 1 | - | 客户端请求处理 "temp" 数据，输出格式为 JSON |
| 2 | 服务定位器 | 通过名称在注册表中查找处理器 |
| 3 | 策略模式 | 检查处理器是否支持 JSON 格式 |
| 4 | 策略模式 | 调用预绑定的 JSON 格式化策略 |
| 5 | 依赖注入 | 调用外部模块注入的 `process_cb` 获取原始数据 |
| 6 | 依赖注入 | 外部模块返回处理后的数据 |
| 7 | 策略模式 | 将数据格式化为 JSON |
| 8 | - | 返回最终结果给客户端 |

---

## 4. Core Code Structure

```
+==================================================================================================+
|                              Code Structure Overview                                              |
+==================================================================================================+

    +---------------------------+
    |      processor.h          |   <-- Interface Definition
    +---------------------------+
    | - processor_t struct      |
    | - Capability flags        |
    | - Callback typedefs       |
    +---------------------------+
              |
              v
    +---------------------------+
    |   processor_registry.c    |   <-- Service Locator + Strategy
    +---------------------------+
    | - registry[] array        |
    | - register_processor()    |
    | - get_by_name/id()        |
    | - format_text/json()      |
    | - process_data()          |
    +---------------------------+
              |
              v
    +---------------------------+
    |    temp_processor.c       |   <-- External Module (DI Provider)
    +---------------------------+
    | - temp_process_cb()       |
    | - temp_processor struct   |
    | - init_temp_processor()   |
    +---------------------------+
              |
              v
    +---------------------------+
    |        main.c             |   <-- Client Code
    +---------------------------+
    | - Initialize processors   |
    | - Call process_data()     |
    +---------------------------+
```

**中文说明：**

代码结构分为四个部分：

| 文件 | 职责 | 涉及模式 |
|------|------|----------|
| `processor.h` | 定义接口结构和常量 | 依赖注入接口 |
| `processor_registry.c` | 注册表管理和策略执行 | 服务定位器 + 策略模式 |
| `temp_processor.c` | 具体处理器实现 | 依赖注入提供者 |
| `main.c` | 客户端代码 | 使用者 |

---

## 5. Complete C Code Example

### 5.1 processor.h - Interface Definition

```c
/*******************************************************************************
 * processor.h - Data Processor Interface (Dependency Injection Interface)
 ******************************************************************************/
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdint.h>

/* ============================================================================
 * Capability Flags (Strategy Pattern - Strategy Selection)
 * ============================================================================ */
#define CAP_TEXT    0x01    /* Support TEXT output format */
#define CAP_JSON    0x02    /* Support JSON output format */
#define CAP_XML     0x04    /* Support XML output format */

/* Output format enumeration */
typedef enum {
    FORMAT_TEXT = 0,
    FORMAT_JSON,
    FORMAT_XML,
    FORMAT_MAX
} output_format_t;

/* ============================================================================
 * Callback Type Definitions
 * ============================================================================ */

/* Process callback - INJECTED BY EXTERNAL MODULE (Core DI Point)
 * This is the callback that external modules provide to do actual data processing
 * @param data_out: Output buffer for processed data
 * @param size: Buffer size
 * @return: Length of data written, -1 on error
 */
typedef int (*process_cb_t)(char *data_out, int size);

/* Format callback - AUTO-WIRED BY REGISTRY (Strategy Pattern)
 * Selected based on capabilities at registration time
 * @param proc: Pointer to processor structure
 * @param output: Output buffer
 * @param size: Buffer size
 * @return: Formatted string, NULL on error
 */
struct processor;
typedef const char* (*format_cb_t)(struct processor *proc, char *output, int size);

/* ============================================================================
 * Processor Structure (Dependency Injection Interface)
 * ============================================================================ */
typedef struct processor {
    /* --- Fields set by external module (Dependency Injection) --- */
    const char *name;           /* Processor name for lookup */
    uint8_t capabilities;       /* Bitmask: CAP_TEXT | CAP_JSON | CAP_XML */
    process_cb_t process_cb;    /* <<< CORE INJECTION POINT: Data processing callback */

    /* --- Fields set by registry (Auto-wired) --- */
    int id;                     /* Auto-assigned unique ID */
    format_cb_t format_text_cb; /* Strategy slot: TEXT formatter */
    format_cb_t format_json_cb; /* Strategy slot: JSON formatter */
    format_cb_t format_xml_cb;  /* Strategy slot: XML formatter */
} processor_t;

/* ============================================================================
 * Registry API (Service Locator Interface)
 * ============================================================================ */

/* Register a processor (Injection Point) */
void register_processor(processor_t *proc);

/* Lookup by name - O(n) */
processor_t* get_processor_by_name(const char *name);

/* Lookup by ID - O(1) */
processor_t* get_processor_by_id(int id);

/* Process data with specified format */
const char* process_data(const char *name, output_format_t format,
                         char *output, int size);

#endif /* PROCESSOR_H */
```

**中文说明：**

`processor.h` 定义了依赖注入的接口：

| 组件 | 说明 |
|------|------|
| `CAP_*` 宏 | 策略选择的能力标志位 |
| `process_cb_t` | **核心注入点** - 外部模块提供的数据处理回调 |
| `format_cb_t` | 策略回调类型 - 由 Registry 自动绑定 |
| `processor_t` | 接口结构体，包含注入字段和自动绑定字段 |

---

### 5.2 processor_registry.c - Service Locator + Strategy Implementation

```c
/*******************************************************************************
 * processor_registry.c - Service Locator + Strategy Pattern Implementation
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "processor.h"

/* ============================================================================
 * Service Locator - Registry Storage
 * ============================================================================ */
#define MAX_PROCESSORS 16

static processor_t *registry[MAX_PROCESSORS];   /* Service registry array */
static int num_registered = 0;                  /* Number of registered processors */

/* ============================================================================
 * Strategy Pattern - Concrete Strategies
 * ============================================================================ */

/* Strategy A: Actual formatting implementation
 * Calls the injected process_cb to get data, then formats output */
static const char* format_text_impl(processor_t *proc, char *output, int size)
{
    char raw_data[256];
    int len;

    /* Call INJECTED callback to get raw data (Dependency Injection usage) */
    len = proc->process_cb(raw_data, sizeof(raw_data));
    if (len < 0) {
        snprintf(output, size, "Error: Failed to get data from %s", proc->name);
        return output;
    }

    /* Format as plain text */
    snprintf(output, size, "[%s] %s", proc->name, raw_data);
    return output;
}

static const char* format_json_impl(processor_t *proc, char *output, int size)
{
    char raw_data[256];
    int len;

    /* Call INJECTED callback to get raw data (Dependency Injection usage) */
    len = proc->process_cb(raw_data, sizeof(raw_data));
    if (len < 0) {
        snprintf(output, size, "{\"error\": \"Failed to get data from %s\"}", proc->name);
        return output;
    }

    /* Format as JSON */
    snprintf(output, size, "{\"%s\": %s}", proc->name, raw_data);
    return output;
}

static const char* format_xml_impl(processor_t *proc, char *output, int size)
{
    char raw_data[256];
    int len;

    /* Call INJECTED callback to get raw data (Dependency Injection usage) */
    len = proc->process_cb(raw_data, sizeof(raw_data));
    if (len < 0) {
        snprintf(output, size, "<error>Failed to get data from %s</error>", proc->name);
        return output;
    }

    /* Format as XML */
    snprintf(output, size, "<%s>%s</%s>", proc->name, raw_data, proc->name);
    return output;
}

/* Strategy B: Not supported - graceful rejection */
static const char* format_not_supported(processor_t *proc, char *output, int size)
{
    snprintf(output, size, "Error: Format not supported by processor '%s'", proc->name);
    return output;
}

/* ============================================================================
 * Service Locator - Registration (Injection Point)
 * ============================================================================ */
void register_processor(processor_t *proc)
{
    if (num_registered >= MAX_PROCESSORS) {
        printf("Error: Registry full, cannot register '%s'\n", proc->name);
        return;
    }

    /* Step 1: Store injected dependency in registry (Service Locator) */
    registry[num_registered] = proc;

    /* Step 2: Auto-wire format callbacks based on capabilities (Strategy Pattern) */
    /* TEXT format strategy selection */
    if (proc->capabilities & CAP_TEXT) {
        proc->format_text_cb = format_text_impl;     /* Strategy A: actual impl */
    } else {
        proc->format_text_cb = format_not_supported; /* Strategy B: not supported */
    }

    /* JSON format strategy selection */
    if (proc->capabilities & CAP_JSON) {
        proc->format_json_cb = format_json_impl;     /* Strategy A: actual impl */
    } else {
        proc->format_json_cb = format_not_supported; /* Strategy B: not supported */
    }

    /* XML format strategy selection */
    if (proc->capabilities & CAP_XML) {
        proc->format_xml_cb = format_xml_impl;       /* Strategy A: actual impl */
    } else {
        proc->format_xml_cb = format_not_supported;  /* Strategy B: not supported */
    }

    /* Step 3: Assign unique ID */
    proc->id = num_registered;
    num_registered++;

    printf("Registered processor: '%s' (id=%d, caps=0x%02x)\n",
           proc->name, proc->id, proc->capabilities);
}

/* ============================================================================
 * Service Locator - Lookup Methods
 * ============================================================================ */

/* Lookup by name - O(n) linear search */
processor_t* get_processor_by_name(const char *name)
{
    int i;

    for (i = 0; i < num_registered; i++) {
        if (strcmp(registry[i]->name, name) == 0) {
            return registry[i];  /* Found: return pointer to processor */
        }
    }
    return NULL;  /* Not found */
}

/* Lookup by ID - O(1) direct index access */
processor_t* get_processor_by_id(int id)
{
    if (id >= 0 && id < num_registered) {
        return registry[id];  /* Direct array access */
    }
    return NULL;  /* Invalid ID */
}

/* ============================================================================
 * Client API - Process Data
 * ============================================================================ */
const char* process_data(const char *name, output_format_t format,
                         char *output, int size)
{
    processor_t *proc;

    /* Step 1: Service Locator - find processor by name */
    proc = get_processor_by_name(name);
    if (proc == NULL) {
        snprintf(output, size, "Error: Processor '%s' not found", name);
        return output;
    }

    /* Step 2: Strategy Pattern - dispatch to appropriate format callback */
    switch (format) {
    case FORMAT_TEXT:
        return proc->format_text_cb(proc, output, size);  /* Call text strategy */
    case FORMAT_JSON:
        return proc->format_json_cb(proc, output, size);  /* Call JSON strategy */
    case FORMAT_XML:
        return proc->format_xml_cb(proc, output, size);   /* Call XML strategy */
    default:
        snprintf(output, size, "Error: Unknown format %d", format);
        return output;
    }
}
```

**中文说明：**

`processor_registry.c` 实现了服务定位器和策略模式：

| 函数 | 模式 | 说明 |
|------|------|------|
| `registry[]` | 服务定位器 | 全局注册表数组，存储所有处理器指针 |
| `format_*_impl()` | 策略 A | 实际的格式化实现，调用注入的 `process_cb` |
| `format_not_supported()` | 策略 B | 优雅拒绝不支持的格式 |
| `register_processor()` | DI + 策略 | 存储注入的依赖，并自动绑定策略 |
| `get_processor_by_*()` | 服务定位器 | 按名称或 ID 查找处理器 |
| `process_data()` | 综合应用 | 定位 → 选择策略 → 执行 |

---

### 5.3 temp_processor.c - External Module (Dependency Injection Provider)

```c
/*******************************************************************************
 * temp_processor.c - Temperature Processor Module (DI Provider)
 *
 * This external module provides the concrete implementation of data processing.
 * It injects its callback into the registry.
 ******************************************************************************/
#include <stdio.h>
#include "processor.h"

/* ============================================================================
 * Injected Callback - Actual Data Processing Logic
 * ============================================================================ */

/* This is the callback that will be INJECTED into the registry.
 * The registry doesn't know how this function works - it just calls it.
 * This is the core of Dependency Injection. */
static int temp_process_callback(char *data_out, int size)
{
    /* Simulate reading temperature sensor */
    float temperature = 25.5;

    /* Return raw numeric data as string */
    return snprintf(data_out, size, "%.1f", temperature);
}

/* ============================================================================
 * Processor Definition - Dependency Injection Interface Implementation
 * ============================================================================ */

/* Define the processor structure with injected dependencies.
 * This structure will be passed to register_processor(). */
static processor_t temp_processor = {
    .name = "temperature",               /* Unique name for Service Locator lookup */
    .capabilities = CAP_TEXT | CAP_JSON, /* Supports TEXT and JSON (Strategy selection) */
    .process_cb = temp_process_callback, /* <<< INJECTED DEPENDENCY */
    /* Other fields will be auto-wired by registry */
};

/* ============================================================================
 * Module Initialization - Injection Point
 * ============================================================================ */

/* Call this function to register the temperature processor.
 * This is the actual injection point where the dependency is injected. */
void init_temp_processor(void)
{
    register_processor(&temp_processor);  /* Inject dependency into registry */
}
```

**中文说明：**

`temp_processor.c` 是依赖注入的提供者：

| 组件 | 说明 |
|------|------|
| `temp_process_callback()` | **被注入的依赖** - 实际的温度数据处理逻辑 |
| `temp_processor` | 处理器结构体，设置了名称、能力和回调 |
| `init_temp_processor()` | 初始化函数，调用 `register_processor()` 注入依赖 |

**关键点**：Registry 不知道 `temp_process_callback()` 的内部实现，只知道它符合 `process_cb_t` 接口。

---

### 5.4 humidity_processor.c - Another External Module

```c
/*******************************************************************************
 * humidity_processor.c - Humidity Processor Module (DI Provider)
 ******************************************************************************/
#include <stdio.h>
#include "processor.h"

/* Injected callback - processes humidity data */
static int humidity_process_callback(char *data_out, int size)
{
    /* Simulate reading humidity sensor */
    int humidity = 65;

    return snprintf(data_out, size, "%d", humidity);
}

/* Processor definition - only supports TEXT format */
static processor_t humidity_processor = {
    .name = "humidity",
    .capabilities = CAP_TEXT,           /* Only TEXT supported (no JSON, no XML) */
    .process_cb = humidity_process_callback,
};

void init_humidity_processor(void)
{
    register_processor(&humidity_processor);
}
```

**中文说明：**

`humidity_processor.c` 展示了一个只支持 TEXT 格式的处理器：

- `capabilities = CAP_TEXT` 表示只支持纯文本输出
- 当请求 JSON 或 XML 格式时，会触发 `format_not_supported()` 策略

---

### 5.5 main.c - Client Code

```c
/*******************************************************************************
 * main.c - Client Code
 *
 * This demonstrates how the three patterns work together from a client's
 * perspective.
 ******************************************************************************/
#include <stdio.h>
#include "processor.h"

/* External module initialization functions */
extern void init_temp_processor(void);
extern void init_humidity_processor(void);

int main(void)
{
    char output[256];
    const char *result;

    printf("=== Data Processor System Demo ===\n\n");

    /* ========================================================================
     * Phase 1: Registration (Dependency Injection)
     * External modules inject their processors into the registry
     * ======================================================================== */
    printf("--- Phase 1: Registering Processors ---\n");
    init_temp_processor();      /* Inject temperature processor */
    init_humidity_processor();  /* Inject humidity processor */
    printf("\n");

    /* ========================================================================
     * Phase 2: Processing (Service Locator + Strategy)
     * Client requests data processing, unaware of concrete implementations
     * ======================================================================== */
    printf("--- Phase 2: Processing Data ---\n\n");

    /* Test 1: Temperature as TEXT (supported) */
    printf("Request: temperature, TEXT\n");
    result = process_data("temperature", FORMAT_TEXT, output, sizeof(output));
    printf("Result:  %s\n\n", result);

    /* Test 2: Temperature as JSON (supported) */
    printf("Request: temperature, JSON\n");
    result = process_data("temperature", FORMAT_JSON, output, sizeof(output));
    printf("Result:  %s\n\n", result);

    /* Test 3: Temperature as XML (NOT supported - Strategy B kicks in) */
    printf("Request: temperature, XML\n");
    result = process_data("temperature", FORMAT_XML, output, sizeof(output));
    printf("Result:  %s\n\n", result);

    /* Test 4: Humidity as TEXT (supported) */
    printf("Request: humidity, TEXT\n");
    result = process_data("humidity", FORMAT_TEXT, output, sizeof(output));
    printf("Result:  %s\n\n", result);

    /* Test 5: Humidity as JSON (NOT supported - Strategy B kicks in) */
    printf("Request: humidity, JSON\n");
    result = process_data("humidity", FORMAT_JSON, output, sizeof(output));
    printf("Result:  %s\n\n", result);

    /* Test 6: Non-existent processor (Service Locator returns NULL) */
    printf("Request: pressure, TEXT\n");
    result = process_data("pressure", FORMAT_TEXT, output, sizeof(output));
    printf("Result:  %s\n\n", result);

    /* ========================================================================
     * Phase 3: Direct Lookup (Service Locator)
     * Demonstrate direct ID-based O(1) lookup
     * ======================================================================== */
    printf("--- Phase 3: Direct Lookup Demo ---\n\n");

    processor_t *proc;

    /* Lookup by name - O(n) */
    proc = get_processor_by_name("temperature");
    if (proc) {
        printf("Found by name: '%s' (id=%d)\n", proc->name, proc->id);
    }

    /* Lookup by ID - O(1) */
    proc = get_processor_by_id(1);
    if (proc) {
        printf("Found by id=1: '%s'\n", proc->name);
    }

    return 0;
}
```

**中文说明：**

`main.c` 展示了客户端如何使用这个系统：

| 阶段 | 说明 | 涉及模式 |
|------|------|----------|
| Phase 1 | 注册处理器 | 依赖注入 |
| Phase 2 | 处理数据请求 | 服务定位器 + 策略 |
| Phase 3 | 直接查找演示 | 服务定位器 |

---

## 6. Expected Output

```
=== Data Processor System Demo ===

--- Phase 1: Registering Processors ---
Registered processor: 'temperature' (id=0, caps=0x03)
Registered processor: 'humidity' (id=1, caps=0x01)

--- Phase 2: Processing Data ---

Request: temperature, TEXT
Result:  [temperature] 25.5

Request: temperature, JSON
Result:  {"temperature": 25.5}

Request: temperature, XML
Result:  Error: Format not supported by processor 'temperature'

Request: humidity, TEXT
Result:  [humidity] 65

Request: humidity, JSON
Result:  Error: Format not supported by processor 'humidity'

Request: pressure, TEXT
Result:  Error: Processor 'pressure' not found

--- Phase 3: Direct Lookup Demo ---

Found by name: 'temperature' (id=0)
Found by id=1: 'humidity'
```

**中文说明：**

运行结果展示了三种模式的协作效果：

| 测试 | 结果 | 体现的模式 |
|------|------|----------|
| temperature, TEXT | 成功输出 | 策略 A 执行 |
| temperature, JSON | 成功输出 | 策略 A 执行 |
| temperature, XML | 不支持错误 | 策略 B 优雅拒绝 |
| humidity, TEXT | 成功输出 | 策略 A 执行 |
| humidity, JSON | 不支持错误 | 策略 B 优雅拒绝 |
| pressure, TEXT | 未找到错误 | 服务定位器返回 NULL |

---

## 7. Compilation and Execution

```bash
# Compile all source files
gcc -c processor_registry.c -o processor_registry.o
gcc -c temp_processor.c -o temp_processor.o
gcc -c humidity_processor.c -o humidity_processor.o
gcc -c main.c -o main.o

# Link to create executable
gcc processor_registry.o temp_processor.o humidity_processor.o main.o -o demo

# Run
./demo
```

---

## 8. Pattern Summary

```
+==================================================================================================+
|                              Three Patterns Summary                                               |
+==================================================================================================+

    +----------------------------+     +----------------------------+     +----------------------------+
    |   DEPENDENCY INJECTION     |     |     SERVICE LOCATOR        |     |     STRATEGY PATTERN       |
    +----------------------------+     +----------------------------+     +----------------------------+
    |                            |     |                            |     |                            |
    |  WHO provides the logic?   |     |  HOW to find the service?  |     |  WHICH algorithm to use?   |
    |                            |     |                            |     |                            |
    +----------------------------+     +----------------------------+     +----------------------------+
    |                            |     |                            |     |                            |
    |  - External modules        |     |  - Central registry        |     |  - Capability bitmask      |
    |  - process_cb callback     |     |  - get_by_name()           |     |  - Auto-wire at register   |
    |  - Injected at register    |     |  - get_by_id()             |     |  - Strategy A/B selection  |
    |                            |     |                            |     |                            |
    +----------------------------+     +----------------------------+     +----------------------------+
    |                            |     |                            |     |                            |
    |  temp_processor.c          |     |  processor_registry.c      |     |  processor_registry.c      |
    |  humidity_processor.c      |     |    registry[] array        |     |    format_*_impl()         |
    |                            |     |                            |     |    format_not_supported()  |
    +----------------------------+     +----------------------------+     +----------------------------+


    Working Together:
    
    +--------------------+          +--------------------+          +--------------------+
    |                    |  inject  |                    |  lookup  |                    |
    | External Module    |--------->| Registry           |<---------| Client             |
    | (DI Provider)      |          | (Service Locator)  |          |                    |
    +--------------------+          +--------------------+          +--------------------+
                                           |
                                           | select strategy
                                           v
                                    +--------------------+
                                    | Strategy Callback  |
                                    | (format_*_cb)      |
                                    +--------------------+
                                           |
                                           | call injected callback
                                           v
                                    +--------------------+
                                    | process_cb()       |
                                    | (from DI Provider) |
                                    +--------------------+
```

**中文说明：**

### 三种模式的核心问题

| 模式 | 解决的问题 | 核心代码 |
|------|------------|----------|
| **依赖注入** | 谁提供处理逻辑？ | `process_cb` 由外部模块注入 |
| **服务定位器** | 如何找到服务？ | `registry[]` + `get_by_name/id()` |
| **策略模式** | 用哪种算法？ | `capabilities` → `format_*_cb` |

### 模式协作流程

1. **启动时**：外部模块将 `process_cb` 注入到 Registry（依赖注入）
2. **注册时**：Registry 根据 capabilities 自动绑定格式策略（策略模式）
3. **运行时**：客户端通过名称查找处理器（服务定位器）
4. **执行时**：调用预选择的策略，策略内部调用注入的回调（综合应用）

### 设计优势

- **解耦**：Registry 不依赖任何具体处理器实现
- **扩展**：新增处理器只需实现 `process_cb` 并注册
- **灵活**：通过 capabilities 声明支持的功能
- **健壮**：不支持的功能优雅拒绝而非崩溃
- **可测**：可注入 mock 回调进行单元测试

