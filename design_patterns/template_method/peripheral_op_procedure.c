#include <stdio.h>
#include <string.h>

/* 步骤 1：定义「可变步骤的函数指针结构体」（对应 OOP 的抽象模板类的抽象方法） */
// 定义：外设操作的所有可变步骤的函数指针，对应模板方法的「抽象方法」
typedef struct {
    // 步骤1：硬件初始化
    void (*hw_init)(void);
    // 步骤2：数据采集
    int  (*data_collect)(void);
    // 步骤3：数据校准
    int  (*data_calibrate)(int raw_data);
    // 步骤4：硬件释放
    void (*hw_release)(void);
} PeripheralOps;

/* 步骤 2：实现「固定流程的模板函数」（核心！对应 OOP 的模板方法） */
// 模板方法核心：固定的算法骨架/执行流程，永不修改！
// 传入：外设的具体操作实现（函数指针结构体），执行固定流程
void peripheral_workflow(const PeripheralOps *ops) {
    int raw_data, cali_data;
    printf("===== 开始执行【统一外设操作流程】 =====\n");
    
    // 步骤1：硬件初始化 - 调用传入的实现
    ops->hw_init();
    // 步骤2：数据采集 - 调用传入的实现
    raw_data = ops->data_collect();
    printf("模板流程：采集到原始数据 = %d\n", raw_data);
    // 步骤3：数据校准 - 调用传入的实现
    cali_data = ops->data_calibrate(raw_data);
    printf("模板流程：校准后有效数据 = %d\n", cali_data);
    // 步骤4：硬件释放 - 调用传入的实现
    ops->hw_release();
    
    printf("===== 统一外设操作流程 执行结束 =====\n\n");
}

/* 步骤 3：实现「具体外设的细节函数」（对应 OOP 的具体子类） */

/* 实现 1：温湿度传感器 DHT11 的具体细节 */
// ==== 具体实现1：温湿度传感器 DHT11 的所有细节步骤 ====
void dht11_init(void) {
    printf("【DHT11】初始化温湿度传感器，配置GPIO引脚\n");
}
int dht11_collect(void) {
    return 256; // 模拟采集到的温湿度原始数据
}
int dht11_calibrate(int raw) {
    return raw / 10; // 模拟温湿度校准算法
}
void dht11_release(void) {
    printf("【DHT11】释放温湿度传感器GPIO引脚\n");
}
// 组装DHT11的操作集：给函数指针绑定具体实现
const PeripheralOps dht11_ops = {
    .hw_init = dht11_init,
    .data_collect = dht11_collect,
    .data_calibrate = dht11_calibrate,
    .hw_release = dht11_release
};

/* 实现 2：光照传感器 BH1750 的具体细节 */
// ==== 具体实现2：光照传感器 BH1750 的所有细节步骤 ====
void bh1750_init(void) {
    printf("【BH1750】初始化光照传感器，配置I2C通信\n");
}
int bh1750_collect(void) {
    return 890; // 模拟采集到的光照原始数据
}
int bh1750_calibrate(int raw) {
    return raw / 2; // 模拟光照校准算法
}
void bh1750_release(void) {
    printf("【BH1750】释放光照传感器I2C总线\n");
}
// 组装BH1750的操作集：给函数指针绑定具体实现
const PeripheralOps bh1750_ops = {
    .hw_init = bh1750_init,
    .data_collect = bh1750_collect,
    .data_calibrate = bh1750_calibrate,
    .hw_release = bh1750_release
};

/* 步骤 4：主函数调用（业务层使用，极简） */
int main(void) {
    // 调用温湿度传感器的流程：传入DHT11的实现，执行统一模板
    peripheral_workflow(&dht11_ops);
    
    // 调用光照传感器的流程：传入BH1750的实现，执行统一模板
    peripheral_workflow(&bh1750_ops);
    
    return 0;
}