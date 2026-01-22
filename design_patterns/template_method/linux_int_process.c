#include <stdio.h>
// 1. 中断处理函数指针类型：抽象可变步骤
typedef int (*irq_handler_t)(int irq_num);

// 2. 【抽象步骤】中断核心结构体：绑定 中断号 + 处理函数
typedef struct {
    int irq_num;            // 中断号
    irq_handler_t handler;  // 中断处理函数指针
} irqaction;

// 3. 【固定骨架】内核模板函数1：中断注册流程
int request_irq(irqaction *irq, int irq_num, irq_handler_t handler) {
    printf("[内核模板] 注册中断 → 中断号：%d\n", irq_num);
    irq->irq_num = irq_num;
    irq->handler = handler;
    return 0;
}

// 4. 【固定骨架】内核模板函数2：中断分发流程（硬件触发后内核自动执行）
void irq_dispatch(const irqaction *irq) {
    printf("[内核模板] 检测到中断触发 → 中断号：%d\n", irq->irq_num);
    irq->handler(irq->irq_num); // 调用具体中断处理逻辑
}

// ===================== 具体实现：网卡中断 =====================
int eth_irq_handler(int irq_num) {
    printf("[网卡驱动] 处理中断 → 接收以太网数据包\n");
    return 0;
}

// ===================== 具体实现：定时器中断 =====================
int timer_irq_handler(int irq_num) {
    printf("[定时器驱动] 处理中断 → 刷新系统时间\n");
    return 0;
}

// 调用测试
int main() {
    irqaction eth_irq, timer_irq;
    request_irq(&eth_irq, 5, eth_irq_handler);    // 注册网卡中断
    request_irq(&timer_irq, 10, timer_irq_handler);//注册定时器中断
    irq_dispatch(&eth_irq); // 模拟网卡中断触发
    return 0;
}