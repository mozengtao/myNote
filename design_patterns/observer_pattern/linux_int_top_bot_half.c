/*
场景说明
内核中断处理分为顶半部（Top Half）和底半部（Bottom Half）：顶半部是主题，快速处理中断（禁用中断、保存上下文）后触发底半部；底半部是观察者，在中断开启的情况下异步处理耗时逻辑（如数据拷贝、协议解析），是观察者模式在中断处理中的核心应用。

核心说明
顶半部是主题，仅负责 “触发事件”（标记底半部待处理），不处理耗时逻辑；
底半部是观察者，通过tasklet_schedule被通知，异步处理耗时操作；
内核中tasklet/workqueue均是此模式的实现，解耦了 “快速中断响应” 和 “耗时数据处理”。
*/

#include <stdio.h>

// 模拟内核底半部回调（观察者接口）
typedef void (*bh_callback)(void *data);

// 模拟内核底半部结构体（观察者）
typedef struct {
    bh_callback cb;       // 底半部回调函数
    void *data;           // 回调数据
    int pending;          // 是否待处理（事件状态）
} tasklet_struct;

// 模拟内核顶半部（主题：中断处理快速路径）
void irq_handler_top_half(int irq_num, tasklet_struct *tasklet, void *data) {
    printf("\n顶半部[IRQ%d]：快速处理（禁用中断、保存上下文）\n", irq_num);
    // 标记底半部待处理（触发事件）
    tasklet->pending = 1;
    tasklet->data = data;
    printf("顶半部[IRQ%d]：触发底半部异步处理\n", irq_num);
}

// 模拟内核底半部调度（通知观察者）
void tasklet_schedule(tasklet_struct *tasklet) {
    if (!tasklet || !tasklet->pending || !tasklet->cb) return;
    printf("底半部调度器：执行异步处理\n");
    tasklet->cb(tasklet->data); // 调用观察者回调
    tasklet->pending = 0; // 清除待处理标记
}

// ---------------------- 具体观察者（底半部）实现 ----------------------
// 底半部1：网卡数据处理
void net_bh_handler(void *data) {
    printf("[网卡底半部] 处理数据：%s → 解析IP包、转发至协议栈\n", (char*)data);
}

// 主函数测试（模拟网卡中断）
int main() {
    // 1. 初始化底半部（观察者）
    tasklet_struct net_tasklet = {
        .cb = net_bh_handler,
        .data = NULL,
        .pending = 0
    };
    
    // 2. 触发网卡中断（IRQ19）
    char pkt_data[] = "来自192.168.1.1的TCP包";
    irq_handler_top_half(19, &net_tasklet, pkt_data);
    
    // 3. 调度底半部（异步处理）
    tasklet_schedule(&net_tasklet);
    
    return 0;
}