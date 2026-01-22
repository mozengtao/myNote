// 1. 定义算法步骤的函数指针类型
typedef struct {
    void (*validate)(void* data);
    void (*transform)(void* data);
    void (*cleanup)(void* data);  // 可选钩子
} ProcessorOps;

// 2. 定义模板函数
void process_template(void* data, ProcessorOps* ops) {
    printf("1. 加载数据...\n");
    // 固定步骤1
    load_data(data);
    
    printf("2. 验证数据...\n");
    // 可变步骤1
    if (ops->validate) ops->validate(data);
    
    printf("3. 转换数据...\n");
    // 可变步骤2
    if (ops->transform) ops->transform(data);
    
    printf("4. 保存结果...\n");
    // 固定步骤2
    save_result(data);
    
    printf("5. 清理...\n");
    // 可选步骤
    if (ops->cleanup) ops->cleanup(data);
}

// 3. 具体实现
void csv_validate(void* data) {
    printf("   CSV验证逻辑\n");
}

void csv_transform(void* data) {
    printf("   CSV转换逻辑\n");
}

void xml_validate(void* data) {
    printf("   XML验证逻辑\n");
}

void xml_transform(void* data) {
    printf("   XML转换逻辑\n");
}

// 4. 使用
int main() {
    // CSV处理器
    ProcessorOps csv_ops = {
        .validate = csv_validate,
        .transform = csv_transform,
        .cleanup = NULL
    };
    
    // XML处理器
    ProcessorOps xml_ops = {
        .validate = xml_validate,
        .transform = xml_transform,
        .cleanup = NULL
    };
    
    Data csv_data, xml_data;
    process_template(&csv_data, &csv_ops);
    printf("\n");
    process_template(&xml_data, &xml_ops);
    
    return 0;
}