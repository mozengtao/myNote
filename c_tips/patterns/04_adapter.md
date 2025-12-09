# 适配器模式 (Adapter Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                       ADAPTER PATTERN                             |
+------------------------------------------------------------------+

    PROBLEM: Incompatible Interfaces
    
    +------------------+         +------------------+
    |     Client       |    ?    |   Third-Party    |
    |  expects:        |  <--->  |   Library        |
    |  send(data,len)  |         |  transmit(buf)   |
    |  recv(buf,len)   |         |  get_data()      |
    +------------------+         +------------------+
    
    Client cannot use library directly!


    SOLUTION: Adapter as Middle Layer
    
    +------------------+         +------------------+         +------------------+
    |     Client       |         |     ADAPTER      |         |   Third-Party    |
    |                  |         |                  |         |   Library        |
    |  send(data,len)--+-------->| send() {         |         |                  |
    |                  |         |   transmit(buf); +-------->| transmit(buf)    |
    |  recv(buf,len) <-+---------| }                |         |                  |
    |                  |         | recv() {         |<--------+ get_data()       |
    +------------------+         |   return get();  |         |                  |
                                 | }                |         +------------------+
                                 +------------------+


    ADAPTER STRUCTURE:
    
    +--------------------------------------------------+
    |                    ADAPTER                        |
    |  +--------------------------------------------+  |
    |  | target_ops_t   *target;   // 目标接口      |  |
    |  | adaptee_t      *adaptee;  // 被适配对象    |  |
    |  +--------------------------------------------+  |
    |                                                   |
    |  target->send() --> adaptee->transmit()          |
    |  target->recv() --> adaptee->get_data()          |
    +--------------------------------------------------+
```

**核心思想说明：**
- 在不兼容的接口之间建立桥梁
- 不修改已有代码，通过适配器转换
- 将一个接口转换为客户期望的另一个接口
- 使原本不兼容的类可以协同工作

## 实现思路

1. **定义目标接口**：客户期望的标准接口
2. **包装被适配者**：在适配器中持有被适配对象
3. **实现转换逻辑**：在目标接口方法中调用被适配者方法
4. **数据格式转换**：必要时转换参数和返回值格式

## 典型应用场景

- 集成第三方库
- 遗留代码对接新系统
- 不同协议转换（如 RS232 转 USB）
- 不同数据格式适配

## 完整代码示例

```c
/*============================================================================
 * 适配器模式示例 - 传感器接口适配
 *============================================================================*/

/*---------------------------------------------------------------------------
 * target_interface.h - 目标接口（系统期望的标准接口）
 *---------------------------------------------------------------------------*/
#ifndef TARGET_INTERFACE_H
#define TARGET_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

/* 关键点：系统期望的标准传感器接口 */
typedef struct sensor_interface sensor_interface_t;

typedef struct {
    bool (*init)(sensor_interface_t *sensor);
    bool (*read_temperature)(sensor_interface_t *sensor, float *temp_celsius);
    bool (*read_humidity)(sensor_interface_t *sensor, float *humidity_percent);
    bool (*read_all)(sensor_interface_t *sensor, float *temp, float *humidity);
    void (*shutdown)(sensor_interface_t *sensor);
} sensor_ops_t;

struct sensor_interface {
    const sensor_ops_t *ops;
    void *adapter_data;
};

#endif /* TARGET_INTERFACE_H */


/*---------------------------------------------------------------------------
 * legacy_sensor.h - 被适配者（遗留的传感器库，接口不同）
 *---------------------------------------------------------------------------*/
#ifndef LEGACY_SENSOR_H
#define LEGACY_SENSOR_H

#include <stdint.h>

/* 关键点：遗留库使用不同的接口和数据格式 */
typedef struct {
    uint8_t i2c_addr;
    uint8_t initialized;
} legacy_sensor_t;

/* 遗留库的函数（不能修改） */
int legacy_sensor_power_on(legacy_sensor_t *dev, uint8_t addr);
void legacy_sensor_power_off(legacy_sensor_t *dev);

/* 返回原始 ADC 值，不是温度 */
int legacy_sensor_get_raw_temp(legacy_sensor_t *dev, uint16_t *raw);

/* 返回原始 ADC 值，不是湿度 */  
int legacy_sensor_get_raw_humid(legacy_sensor_t *dev, uint16_t *raw);

/* 返回错误码，不是 bool */
int legacy_sensor_read_both(legacy_sensor_t *dev, uint16_t *temp, uint16_t *humid);

#endif /* LEGACY_SENSOR_H */


/*---------------------------------------------------------------------------
 * legacy_sensor.c - 遗留库实现（模拟）
 *---------------------------------------------------------------------------*/
#include "legacy_sensor.h"
#include <stdio.h>

int legacy_sensor_power_on(legacy_sensor_t *dev, uint8_t addr) {
    if (dev == NULL) return -1;
    
    dev->i2c_addr = addr;
    dev->initialized = 1;
    printf("[Legacy] Sensor powered on at I2C addr 0x%02X\n", addr);
    return 0;  /* 0 = success */
}

void legacy_sensor_power_off(legacy_sensor_t *dev) {
    if (dev != NULL) {
        dev->initialized = 0;
        printf("[Legacy] Sensor powered off\n");
    }
}

int legacy_sensor_get_raw_temp(legacy_sensor_t *dev, uint16_t *raw) {
    if (dev == NULL || !dev->initialized || raw == NULL) {
        return -1;
    }
    
    /* 模拟：返回原始 ADC 值（需要转换才能得到摄氏度） */
    *raw = 2500;  /* 代表 25.00°C 的原始值 */
    printf("[Legacy] Raw temp ADC: %u\n", *raw);
    return 0;
}

int legacy_sensor_get_raw_humid(legacy_sensor_t *dev, uint16_t *raw) {
    if (dev == NULL || !dev->initialized || raw == NULL) {
        return -1;
    }
    
    /* 模拟：返回原始 ADC 值（需要转换才能得到百分比） */
    *raw = 6500;  /* 代表 65.00% 的原始值 */
    printf("[Legacy] Raw humid ADC: %u\n", *raw);
    return 0;
}

int legacy_sensor_read_both(legacy_sensor_t *dev, uint16_t *temp, uint16_t *humid) {
    int ret = legacy_sensor_get_raw_temp(dev, temp);
    if (ret != 0) return ret;
    
    ret = legacy_sensor_get_raw_humid(dev, humid);
    return ret;
}


/*---------------------------------------------------------------------------
 * sensor_adapter.c - 适配器实现（核心）
 *---------------------------------------------------------------------------*/
#include "target_interface.h"
#include "legacy_sensor.h"
#include <stdlib.h>
#include <stdio.h>

/* 适配器内部数据 */
typedef struct {
    legacy_sensor_t legacy_dev;  /* 关键点：持有被适配对象 */
    uint8_t i2c_address;
} adapter_data_t;

/*---------------------------------------------------------------------------
 * 关键点：转换函数 - 将原始值转换为标准单位
 *---------------------------------------------------------------------------*/
static float convert_raw_to_celsius(uint16_t raw) {
    /* 假设：raw / 100.0 = 摄氏度 */
    return (float)raw / 100.0f;
}

static float convert_raw_to_percent(uint16_t raw) {
    /* 假设：raw / 100.0 = 百分比 */
    return (float)raw / 100.0f;
}

/*---------------------------------------------------------------------------
 * 关键点：适配器方法 - 将目标接口转换为遗留接口调用
 *---------------------------------------------------------------------------*/
static bool adapter_init(sensor_interface_t *sensor) {
    adapter_data_t *data = (adapter_data_t *)sensor->adapter_data;
    
    /* 转换：init() --> legacy_sensor_power_on() */
    int ret = legacy_sensor_power_on(&data->legacy_dev, data->i2c_address);
    
    /* 转换返回值：0 --> true, 非0 --> false */
    return (ret == 0);
}

static bool adapter_read_temperature(sensor_interface_t *sensor, float *temp_celsius) {
    adapter_data_t *data = (adapter_data_t *)sensor->adapter_data;
    uint16_t raw;
    
    /* 转换：read_temperature() --> legacy_sensor_get_raw_temp() */
    int ret = legacy_sensor_get_raw_temp(&data->legacy_dev, &raw);
    if (ret != 0) {
        return false;
    }
    
    /* 关键点：数据格式转换 - 原始值 --> 摄氏度 */
    *temp_celsius = convert_raw_to_celsius(raw);
    printf("[Adapter] Converted: %u --> %.2f°C\n", raw, *temp_celsius);
    
    return true;
}

static bool adapter_read_humidity(sensor_interface_t *sensor, float *humidity_percent) {
    adapter_data_t *data = (adapter_data_t *)sensor->adapter_data;
    uint16_t raw;
    
    int ret = legacy_sensor_get_raw_humid(&data->legacy_dev, &raw);
    if (ret != 0) {
        return false;
    }
    
    /* 数据格式转换 */
    *humidity_percent = convert_raw_to_percent(raw);
    printf("[Adapter] Converted: %u --> %.2f%%\n", raw, *humidity_percent);
    
    return true;
}

static bool adapter_read_all(sensor_interface_t *sensor, float *temp, float *humidity) {
    adapter_data_t *data = (adapter_data_t *)sensor->adapter_data;
    uint16_t raw_temp, raw_humid;
    
    /* 转换：read_all() --> legacy_sensor_read_both() */
    int ret = legacy_sensor_read_both(&data->legacy_dev, &raw_temp, &raw_humid);
    if (ret != 0) {
        return false;
    }
    
    *temp = convert_raw_to_celsius(raw_temp);
    *humidity = convert_raw_to_percent(raw_humid);
    
    return true;
}

static void adapter_shutdown(sensor_interface_t *sensor) {
    adapter_data_t *data = (adapter_data_t *)sensor->adapter_data;
    
    /* 转换：shutdown() --> legacy_sensor_power_off() */
    legacy_sensor_power_off(&data->legacy_dev);
}

/* 关键点：适配器操作表 */
static const sensor_ops_t adapter_ops = {
    .init = adapter_init,
    .read_temperature = adapter_read_temperature,
    .read_humidity = adapter_read_humidity,
    .read_all = adapter_read_all,
    .shutdown = adapter_shutdown
};

/*---------------------------------------------------------------------------
 * 创建适配器
 *---------------------------------------------------------------------------*/
sensor_interface_t* sensor_adapter_create(uint8_t i2c_address) {
    sensor_interface_t *sensor = malloc(sizeof(sensor_interface_t));
    if (sensor == NULL) return NULL;
    
    adapter_data_t *data = malloc(sizeof(adapter_data_t));
    if (data == NULL) {
        free(sensor);
        return NULL;
    }
    
    data->i2c_address = i2c_address;
    
    sensor->ops = &adapter_ops;
    sensor->adapter_data = data;
    
    printf("[Adapter] Created for legacy sensor at 0x%02X\n", i2c_address);
    return sensor;
}

void sensor_adapter_destroy(sensor_interface_t *sensor) {
    if (sensor != NULL) {
        if (sensor->adapter_data != NULL) {
            free(sensor->adapter_data);
        }
        free(sensor);
    }
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "target_interface.h"
#include <stdio.h>

/* 外部声明 */
sensor_interface_t* sensor_adapter_create(uint8_t i2c_address);
void sensor_adapter_destroy(sensor_interface_t *sensor);

/* 关键点：应用代码只使用标准接口，不知道底层是遗留库 */
void monitor_environment(sensor_interface_t *sensor) {
    float temperature, humidity;
    
    printf("\n--- Environment Monitor ---\n");
    
    /* 使用标准接口 */
    if (sensor->ops->read_temperature(sensor, &temperature)) {
        printf("Temperature: %.2f°C\n", temperature);
    }
    
    if (sensor->ops->read_humidity(sensor, &humidity)) {
        printf("Humidity: %.2f%%\n", humidity);
    }
    
    printf("\n--- Read All ---\n");
    if (sensor->ops->read_all(sensor, &temperature, &humidity)) {
        printf("Temp: %.2f°C, Humid: %.2f%%\n", temperature, humidity);
    }
}

int main(void) {
    printf("=== Adapter Pattern Demo ===\n\n");
    
    /* 关键点：通过适配器使用遗留库 */
    sensor_interface_t *sensor = sensor_adapter_create(0x48);
    
    if (sensor == NULL) {
        printf("Failed to create sensor adapter!\n");
        return -1;
    }
    
    /* 初始化（内部调用遗留库） */
    if (!sensor->ops->init(sensor)) {
        printf("Failed to init sensor!\n");
        sensor_adapter_destroy(sensor);
        return -1;
    }
    
    /* 使用标准接口读取数据 */
    monitor_environment(sensor);
    
    /* 关闭 */
    sensor->ops->shutdown(sensor);
    sensor_adapter_destroy(sensor);
    
    printf("\nDone!\n");
    return 0;
}
```

## 运行输出示例

```
=== Adapter Pattern Demo ===

[Adapter] Created for legacy sensor at 0x48
[Legacy] Sensor powered on at I2C addr 0x48

--- Environment Monitor ---
[Legacy] Raw temp ADC: 2500
[Adapter] Converted: 2500 --> 25.00°C
Temperature: 25.00°C
[Legacy] Raw humid ADC: 6500
[Adapter] Converted: 6500 --> 65.00%%
Humidity: 65.00%

--- Read All ---
[Legacy] Raw temp ADC: 2500
[Legacy] Raw humid ADC: 6500
Temp: 25.00°C, Humid: 65.00%
[Legacy] Sensor powered off

Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **无侵入** | 不修改遗留代码或第三方库 |
| **接口统一** | 应用层使用标准接口 |
| **解耦** | 隔离遗留实现的变化 |
| **复用** | 可复用已有的库/代码 |
| **渐进迁移** | 逐步替换遗留实现 |

