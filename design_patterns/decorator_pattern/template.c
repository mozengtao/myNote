/*
C 语言无 “继承”“类”，但通过「函数指针结构体（统一接口）+ 嵌套封装（装饰器持有核心接口指针）」可完美实现装饰器模式。以下是内核风格的通用范式（串口读写功能扩展）：

代码说明
接口一致性：日志 / 校验装饰器均实现uart_ops接口，上层调用时无需区分是基础组件还是装饰器；
动态嵌套：先包裹校验装饰器，再包裹日志装饰器，实现 “核心 + 校验 + 日志” 的组合功能；
无侵入性：基础串口操作uart_base_write/read未做任何修改，仅通过装饰器扩展；
灵活扩展：可随时移除某层装饰器（如注释掉日志装饰器，仅保留核心 + 校验）
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

// ---------------------- 1. 定义核心接口（Component）：串口操作统一接口 ----------------------
typedef struct {
    // 核心接口：串口写数据
    int (*write)(const char *data, int len);
    // 核心接口：串口读数据
    int (*read)(char *buf, int len);
} uart_ops;

// ---------------------- 2. 具体组件（ConcreteComponent）：基础串口操作（无附加功能） ----------------------
// 基础串口写：仅完成数据发送，无任何附加功能
static int uart_base_write(const char *data, int len) {
    if (!data || len <= 0) return -1;
    printf("[核心功能] 串口发送原始数据：%.*s（长度=%d）\n", len, data, len);
    return len; // 模拟发送成功
}

// 基础串口读：仅完成数据接收，无任何附加功能
static int uart_base_read(char *buf, int len) {
    if (!buf || len <= 0) return -1;
    strncpy(buf, "hello decorator", len-1); // 模拟接收数据
    printf("[核心功能] 串口接收原始数据：%s（长度=%ld）\n", buf, strlen(buf));
    return strlen(buf);
}

// 基础串口操作实例（纯核心逻辑）
static uart_ops uart_base = {
    .write = uart_base_write,
    .read = uart_base_read
};

// ---------------------- 3. 装饰器基类（Decorator）：封装核心接口，实现统一接口 ----------------------
// 日志装饰器：给串口操作添加日志功能
typedef struct {
    uart_ops core_ops; // 持有核心接口指针（可指向基础组件或其他装饰器）
} uart_log_decorator;

// 日志装饰器-写：调用核心写+添加日志
static int uart_log_write(uart_log_decorator *decorator, const char *data, int len) {
    // 附加功能：写前日志
    printf("[日志装饰] 准备发送数据，长度=%d\n", len);
    // 调用核心功能（基础组件/其他装饰器）
    int ret = decorator->core_ops.write(data, len);
    // 附加功能：写后日志
    printf("[日志装饰] 发送完成，返回值=%d\n", ret);
    return ret;
}

// 日志装饰器-读：调用核心读+添加日志
static int uart_log_read(uart_log_decorator *decorator, char *buf, int len) {
    // 附加功能：读前日志
    printf("[日志装饰] 准备接收数据，缓冲区长度=%d\n", len);
    // 调用核心功能
    int ret = decorator->core_ops.read(buf, len);
    // 附加功能：读后日志
    printf("[日志装饰] 接收完成，读取长度=%d，数据=%s\n", ret, buf);
    return ret;
}

// 初始化日志装饰器（绑定核心接口）
static void uart_log_decorator_init(uart_log_decorator *decorator, uart_ops core) {
    decorator->core_ops = core;
    // 替换为装饰后的接口（保持接口一致性）
    decorator->core_ops.write = (int (*)(const char*, int))uart_log_write;
    decorator->core_ops.read = (int (*)(char*, int))uart_log_read;
}

// ---------------------- 4. 具体装饰器2：校验和装饰器 ----------------------
typedef struct {
    uart_ops core_ops;
} uart_checksum_decorator;

// 计算校验和（附加功能）
static uint8_t calc_checksum(const char *data, int len) {
    uint8_t sum = 0;
    for (int i=0; i<len; i++) sum += data[i];
    return sum;
}

// 校验和装饰器-写：调用核心写+添加校验和
static int uart_checksum_write(uart_checksum_decorator *decorator, const char *data, int len) {
    // 附加功能：计算并添加校验和
    char buf[256] = {0};
    memcpy(buf, data, len);
    buf[len] = calc_checksum(data, len); // 末尾添加校验和
    // 调用核心功能（基础组件/日志装饰器）
    int ret = decorator->core_ops.write(buf, len+1);
    printf("[校验装饰] 附加校验和=0x%02X，发送总长度=%d\n", buf[len], len+1);
    return ret;
}

// 校验和装饰器-读：调用核心读+校验和验证
static int uart_checksum_read(uart_checksum_decorator *decorator, char *buf, int len) {
    // 调用核心功能
    int ret = decorator->core_ops.read(buf, len);
    if (ret <= 1) return -1; // 无校验和
    // 附加功能：验证校验和
    uint8_t recv_sum = buf[ret-1];
    uint8_t calc_sum = calc_checksum(buf, ret-1);
    if (recv_sum != calc_sum) {
        printf("[校验装饰] 校验和错误！接收=0x%02X，计算=0x%02X\n", recv_sum, calc_sum);
        return -1;
    }
    printf("[校验装饰] 校验和验证通过=0x%02X\n", recv_sum);
    buf[ret-1] = '\0'; // 移除校验和
    return ret-1;
}

// 初始化校验和装饰器
static void uart_checksum_decorator_init(uart_checksum_decorator *decorator, uart_ops core) {
    decorator->core_ops = core;
    decorator->core_ops.write = (int (*)(const char*, int))uart_checksum_write;
    decorator->core_ops.read = (int (*)(char*, int))uart_checksum_read;
}

// ---------------------- 5. 上层使用：嵌套装饰器（核心+校验+日志） ----------------------
int main() {
    // 步骤1：基础组件（纯核心逻辑）
    uart_ops ops = uart_base;
    
    // 步骤2：用校验和装饰器包裹基础组件
    uart_checksum_decorator check_decor;
    uart_checksum_decorator_init(&check_decor, ops);
    ops = check_decor.core_ops; // 接口替换为校验装饰后的接口
    
    // 步骤3：用日志装饰器包裹校验装饰器（嵌套装饰）
    uart_log_decorator log_decor;
    uart_log_decorator_init(&log_decor, ops);
    ops = log_decor.core_ops; // 最终接口：核心+校验+日志
    
    // 上层调用：完全无感知装饰器，仅调用统一接口
    printf("\n=== 执行装饰后的串口写操作 ===\n");
    ops.write("test data", 9);
    
    printf("\n=== 执行装饰后的串口读操作 ===\n");
    char buf[32] = {0};
    ops.read(buf, sizeof(buf));
    
    return 0;
}