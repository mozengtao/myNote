/*
策略思想映射
    策略接口：I/O 请求调度函数指针；
    具体策略：CFQ（公平队列）、Deadline（截止时间）、NOOP（无操作，适合 SSD）；
    上下文：块设备 I/O 队列，用户可通过echo cfq > /sys/block/sda/queue/scheduler动态切换
*/

#include <stdio.h>

// 模拟I/O请求结构体
typedef struct { int req_id; int sector; char op; } io_request;

// 1. 策略接口：I/O调度函数指针类型
typedef void (*IOSchedStrategy)(io_request *req);

// 2. 具体策略实现：三种I/O调度算法
void cfq_sched(io_request *req) { // CFQ：按进程公平调度
    printf("[CFQ策略] 处理请求%d → 公平分配I/O带宽\n", req->req_id);
}
void deadline_sched(io_request *req) { // Deadline：优先保证延迟
    printf("[Deadline策略] 处理请求%d → 截止时间优先\n", req->req_id);
}
void noop_sched(io_request *req) { // NOOP：仅排序，适合SSD
    printf("[NOOP策略] 处理请求%d → 无复杂调度\n", req->req_id);
}

// 3. 上下文：块设备I/O队列
typedef struct {
    IOSchedStrategy sched; // 当前调度策略
    char dev_name[16];     // 设备名（如sda）
} io_queue;

// 上下文方法：切换调度策略（对应sysfs接口）
void io_set_scheduler(io_queue *q, const char *name, IOSchedStrategy s) {
    q->sched = s;
    printf("设备%s：切换为%s调度策略\n", q->dev_name, name);
}

// 上下文方法：执行I/O调度（内核核心逻辑）
void io_dispatch(io_queue *q, io_request *req) {
    printf("设备%s：调度请求（扇区：%d）\n", q->dev_name, req->sector);
    q->sched(req);
}

// 调用测试
int main() {
    io_queue sda = {NULL, "sda"};
    io_request req = {1001, 0x1000, 'R'};

    // 切换为CFQ策略
    io_set_scheduler(&sda, "cfq", cfq_sched);
    io_dispatch(&sda, &req);
    printf("\n");

    // 切换为Deadline策略
    io_set_scheduler(&sda, "deadline", deadline_sched);
    io_dispatch(&sda, &req);

    return 0;
}