/*
场景说明
内核调试时，会给中断处理函数添加 “调用次数统计、执行时长计时” 等装饰功能：核心功能是中断处理逻辑，装饰器不修改核心逻辑，仅在中断处理前后添加统计附加功能，调试完成后可移除。

核心说明
中断统计装饰器持有原生中断处理接口，处理前后添加计时和次数统计；
调试时可通过统计数据定位 “中断风暴”（调用次数过高）、“中断耗时过长” 等问题；
生产模式下直接使用原生中断接口，无性能损耗。
*/

#include <stdio.h>
#include <stdint.h>

// 模拟中断处理核心接口（Component）
typedef struct {
    void (*handler)(int irq); // 核心中断处理接口
} irq_ops;

// ---------------------- 具体组件：原生中断处理（无统计） ----------------------
static void irq_base_handler(int irq) {
    printf("[核心功能] 处理IRQ%d中断：执行硬件逻辑\n", irq);
    // 模拟中断处理耗时逻辑
    for (int i=0; i<1000; i++);
}

static irq_ops irq_base_ops = {
    .handler = irq_base_handler
};

// ---------------------- 具体装饰器：中断统计装饰器 ----------------------
typedef struct {
    irq_ops core_ops;        // 持有核心中断接口
    uint64_t call_count;     // 附加统计：调用次数
    uint64_t total_duration; // 附加统计：总执行时长（模拟）
} irq_stat_decorator;

// 统计装饰器-中断处理：计时+统计+调用核心处理
static void irq_stat_handler(irq_stat_decorator *decorator, int irq) {
    // 附加功能：计时开始
    uint64_t start = 100000; // 模拟时间戳
    // 调用核心中断处理
    decorator->core_ops.handler(irq);
    // 附加功能：计时结束+统计
    uint64_t end = 100123; // 模拟时间戳
    decorator->call_count++;
    decorator->total_duration += (end - start);
    printf("[统计装饰] IRQ%d：调用次数=%lu，本次时长=%luus，总时长=%luus\n",
           irq, decorator->call_count, end-start, decorator->total_duration);
}

// 初始化中断统计装饰器
static void irq_stat_decorator_init(irq_stat_decorator *decorator, irq_ops core) {
    decorator->core_ops = core;
    decorator->call_count = 0;
    decorator->total_duration = 0;
    decorator->core_ops.handler = (void (*)(int))irq_stat_handler;
}

// ---------------------- 上层内核：无感知使用统计装饰后的中断接口 ----------------------
int main() {
    // 初始化中断统计装饰器，包裹原生中断接口
    irq_stat_decorator irq_dec;
    irq_stat_decorator_init(&irq_dec, irq_base_ops);
    irq_ops irq_ops = irq_dec.core_ops;
    
    // 模拟中断触发：透明统计
    printf("\n=== 执行统计装饰后的中断处理 ===\n");
    irq_ops.handler(19); // 网卡中断
    irq_ops.handler(19); // 再次触发网卡中断
    
    return 0;
}