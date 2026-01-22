// 更完整的实现
#include <stdio.h>
#include <stdlib.h>

// 1. 定义算法上下文结构
typedef struct {
    char* filename;
    void* buffer;
    size_t size;
} ProcessContext;

// 2. 定义处理器接口（虚函数表）
typedef struct ProcessorVTable {
    int (*validate)(ProcessContext* ctx);
    int (*transform)(ProcessContext* ctx);
    void (*cleanup)(ProcessContext* ctx);
} ProcessorVTable;

// 3. 处理器基类
typedef struct Processor {
    ProcessContext* context;
    ProcessorVTable* vtable;
    
    // 固定步骤
    void (*load)(struct Processor* self);
    void (*save)(struct Processor* self);
} Processor;

// 4. 模板方法实现
void process_template(Processor* processor) {
    printf("=== 开始处理 ===\n");
    
    // 固定步骤1
    processor->load(processor);
    
    // 可变步骤1
    if (processor->vtable->validate(processor->context) != 0) {
        printf("验证失败！\n");
        return;
    }
    
    // 可变步骤2
    if (processor->vtable->transform(processor->context) != 0) {
        printf("转换失败！\n");
        return;
    }
    
    // 固定步骤2
    processor->save(processor);
    
    // 可选步骤
    if (processor->vtable->cleanup) {
        processor->vtable->cleanup(processor->context);
    }
    
    printf("=== 处理完成 ===\n\n");
}

// 5. 固定步骤的实现
void default_load(Processor* self) {
    printf("1. 从文件 [%s] 加载数据...\n", self->context->filename);
    // 实际加载逻辑
}

void default_save(Processor* self) {
    printf("4. 保存处理结果...\n");
    // 实际保存逻辑
}

// 6. 具体处理器实现
// 6.1 CSV处理器
int csv_validate(ProcessContext* ctx) {
    printf("2. 验证CSV格式...\n");
    // CSV特定验证
    return 0; // 成功
}

int csv_transform(ProcessContext* ctx) {
    printf("3. 转换CSV数据...\n");
    // CSV特定转换
    return 0;
}

void csv_cleanup(ProcessContext* ctx) {
    printf("5. CSV清理工作...\n");
}

// 6.2 JSON处理器
int json_validate(ProcessContext* ctx) {
    printf("2. 验证JSON语法...\n");
    // JSON特定验证
    return 0;
}

int json_transform(ProcessContext* ctx) {
    printf("3. 解析JSON并转换...\n");
    // JSON特定转换
    return 0;
}

// 7. 工厂函数创建具体处理器
Processor* create_csv_processor(const char* filename) {
    static ProcessorVTable csv_vtable = {
        .validate = csv_validate,
        .transform = csv_transform,
        .cleanup = csv_cleanup
    };
    
    Processor* p = malloc(sizeof(Processor));
    p->context = malloc(sizeof(ProcessContext));
    p->context->filename = (char*)filename;
    p->vtable = &csv_vtable;
    p->load = default_load;
    p->save = default_save;
    
    return p;
}

Processor* create_json_processor(const char* filename) {
    static ProcessorVTable json_vtable = {
        .validate = json_validate,
        .transform = json_transform,
        .cleanup = NULL  // 没有清理步骤
    };
    
    Processor* p = malloc(sizeof(Processor));
    p->context = malloc(sizeof(ProcessContext));
    p->context->filename = (char*)filename;
    p->vtable = &json_vtable;
    p->load = default_load;
    p->save = default_save;
    
    return p;
}

// 8. 使用
int main() {
    // 创建并处理CSV
    Processor* csv_processor = create_csv_processor("data.csv");
    process_template(csv_processor);
    
    // 创建并处理JSON
    Processor* json_processor = create_json_processor("data.json");
    process_template(json_processor);
    
    // 清理
    free(csv_processor->context);
    free(csv_processor);
    free(json_processor->context);
    free(json_processor);
    
    return 0;
}