#include <stdio.h>
// 1. 工作函数指针类型：抽象可变步骤
typedef void (*work_func_t)(void *work);

// 2. 【抽象步骤】工作项核心结构体：绑定 工作函数 + 私有数据
typedef struct {
    work_func_t func; // 工作处理函数指针
    void *data;       // 私有数据
} work_struct;

// 3. 【固定骨架】内核模板函数：工作队列调度流程
void queue_work(work_struct *work) {
    printf("[内核模板] 工作项入队 → 唤醒worker线程\n");
    work->func(work->data); // 执行具体工作逻辑
    printf("[内核模板] 工作执行完成 → 清理工作项\n");
}

// ===================== 具体实现：异步传感器数据上报 =====================
void sensor_work_handler(void *data) {
    printf("[传感器驱动] 异步处理 → 上报温湿度数据：%s\n", (char*)data);
}

// ===================== 具体实现：异步磁盘写回 =====================
void disk_work_handler(void *data) {
    printf("[磁盘驱动] 异步处理 → 缓存数据刷写到磁盘：%s\n", (char*)data);
}

// 调用测试
int main() {
    work_struct sensor_work = {.func=sensor_work_handler, .data="temp:25°C"};
    work_struct disk_work = {.func=disk_work_handler, .data="block:0x100"};
    queue_work(&sensor_work); // 调度传感器工作
    queue_work(&disk_work);   // 调度磁盘工作
    return 0;
}