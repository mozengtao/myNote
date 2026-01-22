/*
场景说明
内核的中断控制器（如 APIC、GIC）是硬件级别的唯一设备，对应内核中的irq_chip实例也必须全局唯一，负责所有中断的注册、分发与处理。

核心说明
中断控制器是硬件唯一的，内核对应irq_chip实例也必须全局唯一；
实例为饿汉式初始化，内核启动时完成硬件适配后赋值；
所有中断操作（使能、禁用、分发）均通过该单例实例完成，保证硬件操作的一致性。
*/

#include <stdio.h>

// 1. 中断控制器单例结构体
typedef struct {
    int irq_max;                // 最大中断号
    void (*enable_irq)(int irq); // 使能中断函数
    void (*disable_irq)(int irq);// 禁用中断函数
} irq_chip;

// 2. 具体中断操作实现
static void apic_enable_irq(int irq) {
    printf("APIC控制器：使能中断 %d\n", irq);
}
static void apic_disable_irq(int irq) {
    printf("APIC控制器：禁用中断 %d\n", irq);
}

// 3. 全局唯一的中断控制器实例（饿汉式）
static irq_chip apic_chip = {
    .irq_max = 256,
    .enable_irq = apic_enable_irq,
    .disable_irq = apic_disable_irq
};

// 4. 唯一访问接口：获取中断控制器实例
static inline irq_chip* get_irq_chip(void) {
    return &apic_chip;
}

// 5. 业务接口：操作中断
void irq_manage(int irq, int enable) {
    irq_chip *chip = get_irq_chip();
    if (irq < 0 || irq >= chip->irq_max) return;
    if (enable) {
        chip->enable_irq(irq);
    } else {
        chip->disable_irq(irq);
    }
}

// 测试
int main() {
    printf("中断控制器实例地址：%p\n", get_irq_chip());
    irq_manage(10, 1); // 使能中断10
    irq_manage(10, 0); // 禁用中断10
    return 0;
}