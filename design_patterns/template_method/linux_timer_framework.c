// file: timer_template.c
#include <stdio.h>
#include <unistd.h>

// 1. 定时器回调函数类型
typedef void (*timer_callback)(unsigned long data);

// 2. 定时器结构
typedef struct timer_list {
    unsigned long expires;        // 到期时间
    timer_callback function;      // 回调函数
    unsigned long data;           // 回调参数
} timer_t;

// 3. 定时器核心框架
static void __run_timers(timer_t *timer) {
    printf("定时器框架: 检查定时器到期\n");
    
    // 固定步骤1: 检查是否到期
    if (1) { // 简化：假设总是到期
        printf("定时器框架: 定时器到期，准备执行回调\n");
        
        // 可变步骤: 执行具体回调函数
        timer->function(timer->data);
        
        // 固定步骤2: 更新统计
        printf("定时器框架: 回调执行完成\n");
    }
}

// 4. 具体使用示例
void my_timer_callback(unsigned long data) {
    printf("定时器回调: 处理数据 %lu\n", data);
}

void heartbeat_callback(unsigned long data) {
    printf("心跳回调: 发送心跳包\n");
}

int main() {
    // 创建不同的定时器
    timer_t my_timer = {
        .expires = 1000,
        .function = my_timer_callback,
        .data = 12345
    };
    
    timer_t heartbeat = {
        .expires = 5000,
        .function = heartbeat_callback,
        .data = 0
    };
    
    printf("=== 启动自定义定时器 ===\n");
    __run_timers(&my_timer);
    
    printf("\n=== 启动心跳定时器 ===\n");
    __run_timers(&heartbeat);
    
    return 0;
}