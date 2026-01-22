/*
策略思想映射
策略接口：进程调度函数指针；
具体策略：CFS（普通进程）、RT（实时进程）、Batch（批处理）；
上下文：进程调度器，通过task_struct.policy动态选择策略。
*/

#include <stdio.h>

// 模拟进程结构体
typedef struct {
    int pid;     // 进程ID
    int policy;  // 策略：0=CFS,1=RT,2=Batch
} task_struct;

// 1. 策略接口：进程调度函数指针
typedef void (*SchedStrategy)(task_struct *task);

// 2. 具体策略实现：三种调度算法
void cfs_schedule(task_struct *t) { // CFS：完全公平调度
    printf("[CFS策略] 调度进程%d → 按虚拟时间公平调度\n", t->pid);
}
void rt_schedule(task_struct *t) { // RT：实时抢占调度
    printf("[RT策略] 调度进程%d → 实时抢占式调度\n", t->pid);
}
void batch_schedule(task_struct *t) { // Batch：批处理调度
    printf("[Batch策略] 调度进程%d → 减少调度频率\n", t->pid);
}

// 3. 上下文：调度器上下文（策略表）
typedef struct {
    SchedStrategy sched_table[3]; // 策略映射表
} sched_context;

// 初始化策略表
void sched_init(sched_context *ctx) {
    ctx->sched_table[0] = cfs_schedule;
    ctx->sched_table[1] = rt_schedule;
    ctx->sched_table[2] = batch_schedule;
}

// 上下文方法：执行调度（内核核心逻辑）
void schedule(sched_context *ctx, task_struct *t) {
    ctx->sched_table[t->policy](t);
}

// 调用测试
int main() {
    sched_context sched_ctx;
    sched_init(&sched_ctx);

    // 普通进程（CFS策略）
    task_struct task1 = {100, 0};
    schedule(&sched_ctx, &task1);
    printf("\n");

    // 实时进程（RT策略）
    task_struct task2 = {200, 1};
    schedule(&sched_ctx, &task2);

    return 0;
}