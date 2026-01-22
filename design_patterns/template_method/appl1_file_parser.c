// 通用的文件解析框架
typedef struct {
    // 模板方法
    void (*parse)(const char* filename);
    
    // 可变步骤
    void (*open_file)(const char* filename);
    void (*parse_header)(void);
    void (*parse_body)(void);
    void (*parse_footer)(void);
    void (*close_file)(void);
} FileParser;

// 不同格式实现不同的步骤
// CSV: 打开 -> 解析CSV头 -> 逐行解析 -> 关闭
// XML: 打开 -> 解析XML声明 -> 解析节点 -> 关闭