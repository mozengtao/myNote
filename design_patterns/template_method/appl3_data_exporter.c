// 数据导出框架
typedef struct DataExporter {
    // 模板方法
    void (*export)(struct DataExporter* self, DataSet* data);
    
    // 可变步骤
    void (*begin_export)(struct DataExporter* self);
    void (*export_header)(struct DataExporter* self);
    void (*export_row)(struct DataExporter* self, Row* row);
    void (*export_footer)(struct DataExporter* self);
    void (*end_export)(struct DataExporter* self);
} DataExporter;

// CSV导出器、JSON导出器、Excel导出器实现不同的步骤