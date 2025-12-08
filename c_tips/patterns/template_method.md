# Template Method Pattern (模板方法模式)

## 1. Core Concept and Use Cases

### Core Concept
Define the **skeleton of an algorithm** in a function, deferring some steps to "subclasses" (via function pointers in C). The template method lets subclasses redefine certain steps without changing the algorithm's structure.

### Typical Use Cases
- Framework lifecycle methods
- Data processing pipelines
- Report generation
- Game loop implementations
- Protocol handling templates

---

## 2. Architecture Diagram

```
+--------------------------------------------------------------------------------------------------+
|                                  Template Method Pattern                                          |
+--------------------------------------------------------------------------------------------------+

                              +---------------------------+
                              |    Abstract Template      |
                              +---------------------------+
                              | + template_method() {     |  <-- Fixed algorithm skeleton
                              |     step1();              |
                              |     step2();              |
                              |     hook();               |
                              |     step3();              |
                              | }                         |
                              +---------------------------+
                              | # step1() = 0            |  <-- Abstract (must implement)
                              | # step2() = 0            |
                              | # hook() { }             |  <-- Optional hook
                              | # step3() = 0            |
                              +-------------+-------------+
                                            |
                      +---------------------+---------------------+
                      |                                           |
                      v                                           v
         +------------------------+                  +------------------------+
         |  Concrete Template A   |                  |  Concrete Template B   |
         +------------------------+                  +------------------------+
         | # step1() { ... }      |                  | # step1() { ... }      |
         | # step2() { ... }      |                  | # step2() { ... }      |
         | # hook() { ... }       |                  | # step3() { ... }      |
         | # step3() { ... }      |                  |   (no hook override)   |
         +------------------------+                  +------------------------+


    Template Method Execution:
    
    template_method()
          |
          +---> step1()     [Implemented by concrete class]
          |
          +---> step2()     [Implemented by concrete class]
          |
          +---> hook()      [Optional, may be overridden]
          |
          +---> step3()     [Implemented by concrete class]
          |
          v
       Complete
```

**中文说明：**

模板方法模式的核心流程：

1. **模板方法（Template Method）**：
   - 定义算法的固定骨架
   - 调用一系列步骤方法
   - 步骤的执行顺序固定

2. **抽象步骤（Abstract Steps）**：
   - 由子类/具体实现提供
   - 通过函数指针实现多态

3. **钩子方法（Hooks）**：
   - 可选的扩展点
   - 有默认实现，可被覆盖

---

## 3. Code Skeleton

```c
/* Template structure with function pointers */
typedef struct Template {
    /* Abstract methods (must be implemented) */
    void (*step1)(struct Template* self);
    void (*step2)(struct Template* self);
    void (*step3)(struct Template* self);
    
    /* Hook method (optional, has default) */
    void (*hook)(struct Template* self);
    
    void* context;
} Template;

/* Template method - the fixed algorithm */
void template_execute(Template* t);

/* Factory functions for concrete implementations */
Template* create_concrete_a(void);
Template* create_concrete_b(void);
```

**中文说明：**

代码骨架包含：
- `Template`：模板结构，包含步骤函数指针
- `template_execute()`：模板方法，定义算法骨架
- 工厂函数创建具体实现

---

## 4. Complete Example Code

```c
/*
 * Template Method Pattern - Data Processing Pipeline Example
 * 
 * This example demonstrates a data processing pipeline where
 * the overall structure is fixed but individual steps vary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DATA_SIZE 1024

/* ============================================
 * Data Processing Template
 * ============================================ */
typedef struct DataProcessor DataProcessor;

/* Step function types */
typedef int  (*read_fn)(DataProcessor* self, char* buffer, int max_size);
typedef int  (*validate_fn)(DataProcessor* self, const char* data, int size);
typedef int  (*transform_fn)(DataProcessor* self, char* data, int size);
typedef int  (*write_fn)(DataProcessor* self, const char* data, int size);
typedef void (*hook_fn)(DataProcessor* self, const char* phase);

struct DataProcessor {
    char name[32];
    
    /* Abstract methods - must be implemented */
    read_fn      read_data;
    validate_fn  validate_data;
    transform_fn transform_data;
    write_fn     write_data;
    
    /* Hook method - optional, has default */
    hook_fn      on_phase_complete;
    
    /* Context for processor-specific data */
    void* context;
    
    /* Statistics */
    int bytes_read;
    int bytes_written;
    int errors;
};

/* ============================================
 * Default Hook Implementation
 * ============================================ */
void default_hook(DataProcessor* self, const char* phase)
{
    /* Default: do nothing */
    (void)self;
    (void)phase;
}

/* ============================================
 * Template Method - Fixed Algorithm Skeleton
 * This is the core of the Template Method pattern
 * ============================================ */
int data_processor_execute(DataProcessor* dp)
{
    char buffer[MAX_DATA_SIZE];
    int size;
    int result;
    
    printf("\n[%s] Starting data processing pipeline\n", dp->name);
    printf("================================================\n");
    
    /* Step 1: Read data (abstract - implemented by concrete class) */
    printf("[Step 1] Reading data...\n");
    size = dp->read_data(dp, buffer, MAX_DATA_SIZE);
    if (size <= 0) {
        printf("[Step 1] ERROR: Failed to read data\n");
        dp->errors++;
        return -1;
    }
    dp->bytes_read = size;
    printf("[Step 1] Read %d bytes\n", size);
    dp->on_phase_complete(dp, "read");  /* Hook */
    
    /* Step 2: Validate data (abstract) */
    printf("[Step 2] Validating data...\n");
    result = dp->validate_data(dp, buffer, size);
    if (result != 0) {
        printf("[Step 2] ERROR: Validation failed\n");
        dp->errors++;
        return -1;
    }
    printf("[Step 2] Validation passed\n");
    dp->on_phase_complete(dp, "validate");  /* Hook */
    
    /* Step 3: Transform data (abstract) */
    printf("[Step 3] Transforming data...\n");
    size = dp->transform_data(dp, buffer, size);
    if (size <= 0) {
        printf("[Step 3] ERROR: Transform failed\n");
        dp->errors++;
        return -1;
    }
    printf("[Step 3] Transformed to %d bytes\n", size);
    dp->on_phase_complete(dp, "transform");  /* Hook */
    
    /* Step 4: Write data (abstract) */
    printf("[Step 4] Writing data...\n");
    result = dp->write_data(dp, buffer, size);
    if (result <= 0) {
        printf("[Step 4] ERROR: Write failed\n");
        dp->errors++;
        return -1;
    }
    dp->bytes_written = result;
    printf("[Step 4] Wrote %d bytes\n", result);
    dp->on_phase_complete(dp, "write");  /* Hook */
    
    printf("================================================\n");
    printf("[%s] Pipeline complete! Read=%d, Written=%d\n",
           dp->name, dp->bytes_read, dp->bytes_written);
    
    return 0;
}

/* ============================================
 * Concrete Implementation 1: CSV Processor
 * ============================================ */
typedef struct {
    char input_file[64];
    char output_file[64];
    char delimiter;
} CsvContext;

int csv_read(DataProcessor* self, char* buffer, int max_size)
{
    CsvContext* ctx = (CsvContext*)self->context;
    /* Simulate reading CSV file */
    const char* csv_data = "name,age,city\nAlice,30,NYC\nBob,25,LA\nCharlie,35,Chicago";
    int len = strlen(csv_data);
    if (len > max_size) len = max_size;
    memcpy(buffer, csv_data, len);
    buffer[len] = '\0';
    printf("    Read from %s: %s\n", ctx->input_file, buffer);
    return len;
}

int csv_validate(DataProcessor* self, const char* data, int size)
{
    CsvContext* ctx = (CsvContext*)self->context;
    /* Check for delimiter */
    if (strchr(data, ctx->delimiter) == NULL) {
        printf("    Invalid CSV: delimiter '%c' not found\n", ctx->delimiter);
        return -1;
    }
    printf("    CSV format validated (delimiter='%c')\n", ctx->delimiter);
    return 0;
}

int csv_transform(DataProcessor* self, char* data, int size)
{
    CsvContext* ctx = (CsvContext*)self->context;
    /* Transform: replace delimiter with pipe */
    printf("    Transforming: '%c' -> '|'\n", ctx->delimiter);
    for (int i = 0; i < size; i++) {
        if (data[i] == ctx->delimiter) {
            data[i] = '|';
        }
    }
    printf("    Result: %s\n", data);
    return size;
}

int csv_write(DataProcessor* self, const char* data, int size)
{
    CsvContext* ctx = (CsvContext*)self->context;
    printf("    Writing to %s: %s\n", ctx->output_file, data);
    return size;
}

void csv_hook(DataProcessor* self, const char* phase)
{
    printf("    [CSV Hook] Phase '%s' completed\n", phase);
}

DataProcessor* create_csv_processor(const char* input, const char* output)
{
    DataProcessor* dp = (DataProcessor*)malloc(sizeof(DataProcessor));
    CsvContext* ctx = (CsvContext*)malloc(sizeof(CsvContext));
    
    strncpy(dp->name, "CsvProcessor", sizeof(dp->name) - 1);
    
    /* Set abstract methods */
    dp->read_data = csv_read;
    dp->validate_data = csv_validate;
    dp->transform_data = csv_transform;
    dp->write_data = csv_write;
    
    /* Override hook */
    dp->on_phase_complete = csv_hook;
    
    /* Setup context */
    strncpy(ctx->input_file, input, sizeof(ctx->input_file) - 1);
    strncpy(ctx->output_file, output, sizeof(ctx->output_file) - 1);
    ctx->delimiter = ',';
    dp->context = ctx;
    
    dp->bytes_read = 0;
    dp->bytes_written = 0;
    dp->errors = 0;
    
    return dp;
}

/* ============================================
 * Concrete Implementation 2: JSON Processor
 * ============================================ */
typedef struct {
    int indent_spaces;
    int minify;
} JsonContext;

int json_read(DataProcessor* self, char* buffer, int max_size)
{
    /* Simulate reading JSON */
    const char* json_data = "{\"users\":[{\"name\":\"Alice\"},{\"name\":\"Bob\"}]}";
    int len = strlen(json_data);
    if (len > max_size) len = max_size;
    memcpy(buffer, json_data, len);
    buffer[len] = '\0';
    printf("    Read JSON: %s\n", buffer);
    return len;
}

int json_validate(DataProcessor* self, const char* data, int size)
{
    /* Simple validation: check for balanced braces */
    int braces = 0;
    int brackets = 0;
    for (int i = 0; i < size; i++) {
        if (data[i] == '{') braces++;
        if (data[i] == '}') braces--;
        if (data[i] == '[') brackets++;
        if (data[i] == ']') brackets--;
    }
    if (braces != 0 || brackets != 0) {
        printf("    Invalid JSON: unbalanced braces/brackets\n");
        return -1;
    }
    printf("    JSON structure validated\n");
    return 0;
}

int json_transform(DataProcessor* self, char* data, int size)
{
    JsonContext* ctx = (JsonContext*)self->context;
    
    if (ctx->minify) {
        /* Remove whitespace for minification */
        int write_idx = 0;
        for (int i = 0; i < size; i++) {
            if (data[i] != ' ' && data[i] != '\n' && data[i] != '\t') {
                data[write_idx++] = data[i];
            }
        }
        data[write_idx] = '\0';
        printf("    Minified JSON: %s\n", data);
        return write_idx;
    }
    
    printf("    JSON unchanged (minify=false)\n");
    return size;
}

int json_write(DataProcessor* self, const char* data, int size)
{
    printf("    Writing JSON output: %s\n", data);
    return size;
}

/* JSON processor uses default hook (no override) */

DataProcessor* create_json_processor(int minify)
{
    DataProcessor* dp = (DataProcessor*)malloc(sizeof(DataProcessor));
    JsonContext* ctx = (JsonContext*)malloc(sizeof(JsonContext));
    
    strncpy(dp->name, "JsonProcessor", sizeof(dp->name) - 1);
    
    dp->read_data = json_read;
    dp->validate_data = json_validate;
    dp->transform_data = json_transform;
    dp->write_data = json_write;
    
    /* Use default hook (no logging) */
    dp->on_phase_complete = default_hook;
    
    ctx->indent_spaces = 2;
    ctx->minify = minify;
    dp->context = ctx;
    
    dp->bytes_read = 0;
    dp->bytes_written = 0;
    dp->errors = 0;
    
    return dp;
}

/* ============================================
 * Concrete Implementation 3: Text Processor
 * ============================================ */
int text_read(DataProcessor* self, char* buffer, int max_size)
{
    const char* text = "Hello World! This is a TEST string.";
    int len = strlen(text);
    memcpy(buffer, text, len);
    buffer[len] = '\0';
    printf("    Read text: %s\n", buffer);
    return len;
}

int text_validate(DataProcessor* self, const char* data, int size)
{
    if (size == 0) {
        printf("    Invalid: empty text\n");
        return -1;
    }
    printf("    Text validated (length=%d)\n", size);
    return 0;
}

int text_transform(DataProcessor* self, char* data, int size)
{
    /* Transform to uppercase */
    printf("    Transforming to uppercase...\n");
    for (int i = 0; i < size; i++) {
        if (data[i] >= 'a' && data[i] <= 'z') {
            data[i] = data[i] - 'a' + 'A';
        }
    }
    printf("    Result: %s\n", data);
    return size;
}

int text_write(DataProcessor* self, const char* data, int size)
{
    printf("    Output: %s\n", data);
    return size;
}

void text_hook(DataProcessor* self, const char* phase)
{
    printf("    [TEXT] >>> Phase '%s' done <<<\n", phase);
}

DataProcessor* create_text_processor(void)
{
    DataProcessor* dp = (DataProcessor*)malloc(sizeof(DataProcessor));
    
    strncpy(dp->name, "TextProcessor", sizeof(dp->name) - 1);
    
    dp->read_data = text_read;
    dp->validate_data = text_validate;
    dp->transform_data = text_transform;
    dp->write_data = text_write;
    dp->on_phase_complete = text_hook;
    
    dp->context = NULL;
    dp->bytes_read = 0;
    dp->bytes_written = 0;
    dp->errors = 0;
    
    return dp;
}

/* ============================================
 * Cleanup
 * ============================================ */
void processor_destroy(DataProcessor* dp)
{
    if (dp->context) free(dp->context);
    free(dp);
}

/* ============================================
 * Main Function - Demonstration
 * ============================================ */
int main(void)
{
    printf("=== Template Method Pattern Demo ===\n");
    
    /* Create different processors */
    DataProcessor* csv = create_csv_processor("input.csv", "output.csv");
    DataProcessor* json = create_json_processor(1);  /* minify=true */
    DataProcessor* text = create_text_processor();
    
    /* Execute same template with different implementations */
    printf("\n########################################");
    printf("\n#       CSV PROCESSOR                  #");
    printf("\n########################################");
    data_processor_execute(csv);
    
    printf("\n########################################");
    printf("\n#       JSON PROCESSOR                 #");
    printf("\n########################################");
    data_processor_execute(json);
    
    printf("\n########################################");
    printf("\n#       TEXT PROCESSOR                 #");
    printf("\n########################################");
    data_processor_execute(text);
    
    /* Cleanup */
    processor_destroy(csv);
    processor_destroy(json);
    processor_destroy(text);
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}
```

**中文说明：**

完整示例代码实现了数据处理管道：

1. **模板方法（data_processor_execute）**：
   - 定义固定的处理流程：读取 → 验证 → 转换 → 写入
   - 每个步骤后调用钩子方法
   - 处理错误并统计

2. **具体实现**：
   - **CsvProcessor**：处理 CSV 数据，替换分隔符
   - **JsonProcessor**：处理 JSON 数据，可选压缩
   - **TextProcessor**：处理文本，转换为大写

3. **钩子方法**：
   - `on_phase_complete`：每个阶段完成后调用
   - CSV 和 Text 覆盖了钩子，JSON 使用默认实现

4. **优势体现**：
   - 算法骨架固定，步骤实现可变
   - 代码复用（模板方法）
   - 扩展灵活（新增处理器只需实现步骤）

