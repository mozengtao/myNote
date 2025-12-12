# 嵌入式设计模式 - 硬件访问模式

本文档介绍嵌入式系统中用于硬件访问的设计模式，包括完整的 C 语言代码示例。

---

## 目录

1. [硬件代理模式 (Hardware Proxy Pattern)](#1-硬件代理模式)
2. [硬件适配器模式 (Hardware Adapter Pattern)](#2-硬件适配器模式)
3. [中介者模式 (Mediator Pattern)](#3-中介者模式)
4. [观察者模式 (Observer Pattern)](#4-观察者模式)
5. [去抖动模式 (Debouncing Pattern)](#5-去抖动模式)
6. [中断模式 (Interrupt Pattern)](#6-中断模式)
7. [轮询模式 (Polling Pattern)](#7-轮询模式)

---

## 1. 硬件代理模式

### 架构图

```
+------------------------------------------------------------------+
|                    HARDWARE PROXY PATTERN                         |
+------------------------------------------------------------------+

    WITHOUT PROXY (Direct Access):
    
    +----------+     +----------+     +----------+
    | Client A |---->|          |     |          |
    +----------+     | Hardware |     | Hardware |
    +----------+     | Register |     | Device   |
    | Client B |---->|  (Raw)   |---->|          |
    +----------+     +----------+     +----------+
    +----------+           ^
    | Client C |-----------+
    +----------+
    
    Problem: All clients know hardware details!


    WITH PROXY (Encapsulated Access):
    
    +----------+
    | Client A |-----+
    +----------+     |     +------------------+     +----------+
    +----------+     |     |  Hardware Proxy  |     | Hardware |
    | Client B |-----+---->|  +------------+  |---->| Device   |
    +----------+     |     |  | Bit encode |  |     +----------+
    +----------+     |     |  | Encrypt    |  |
    | Client C |-----+     |  | Compress   |  |
    +----------+           |  +------------+  |
                           +------------------+
    
    Clients use clean API, proxy handles details.
```

**中文说明：**
- 硬件代理模式将所有硬件访问封装在一个代理类/结构体中
- 客户端无需了解硬件的位编码、加密、压缩等细节
- 当硬件变化时，只需修改代理，客户端代码无需改动

### 完整代码示例

```c
/*============================================================================
 * 硬件代理模式示例 - LED 驱动代理
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * 硬件寄存器定义（底层细节）
 *---------------------------------------------------------------------------*/
/* 关键点：这些硬件细节被代理隐藏 */
#define LED_PORT_BASE       0x40020000
#define LED_PORT_ODR        (*(volatile uint32_t *)(LED_PORT_BASE + 0x14))
#define LED_PORT_IDR        (*(volatile uint32_t *)(LED_PORT_BASE + 0x10))
#define LED_PORT_MODER      (*(volatile uint32_t *)(LED_PORT_BASE + 0x00))

/* LED 引脚映射（硬件相关） */
#define LED_RED_PIN         5
#define LED_GREEN_PIN       6
#define LED_BLUE_PIN        7

/*---------------------------------------------------------------------------
 * 硬件代理接口（对外暴露的清晰 API）
 *---------------------------------------------------------------------------*/
typedef enum {
    LED_COLOR_RED,
    LED_COLOR_GREEN,
    LED_COLOR_BLUE,
    LED_COLOR_MAX
} led_color_t;

typedef enum {
    LED_STATE_OFF,
    LED_STATE_ON,
    LED_STATE_BLINK
} led_state_t;

/* 关键点：代理结构体封装硬件状态 */
typedef struct {
    led_state_t states[LED_COLOR_MAX];
    uint8_t brightness[LED_COLOR_MAX];
    bool initialized;
} led_proxy_t;

/*---------------------------------------------------------------------------
 * 内部私有函数（处理硬件细节）
 *---------------------------------------------------------------------------*/
/* 关键点：将颜色映射到具体引脚（硬件细节封装在代理内部） */
static uint8_t color_to_pin(led_color_t color) {
    static const uint8_t pin_map[] = {
        [LED_COLOR_RED]   = LED_RED_PIN,
        [LED_COLOR_GREEN] = LED_GREEN_PIN,
        [LED_COLOR_BLUE]  = LED_BLUE_PIN
    };
    return pin_map[color];
}

/* 关键点：底层寄存器操作（客户端不可见） */
static void set_pin_output(uint8_t pin) {
    LED_PORT_MODER &= ~(3 << (pin * 2));
    LED_PORT_MODER |= (1 << (pin * 2));  /* 输出模式 */
}

static void set_pin_high(uint8_t pin) {
    LED_PORT_ODR |= (1 << pin);
}

static void set_pin_low(uint8_t pin) {
    LED_PORT_ODR &= ~(1 << pin);
}

/*---------------------------------------------------------------------------
 * 公开代理接口实现
 *---------------------------------------------------------------------------*/
static led_proxy_t g_led_proxy = {0};

/* 初始化代理 */
bool led_proxy_init(void) {
    /* 关键点：初始化硬件，客户端无需了解具体过程 */
    for (int i = 0; i < LED_COLOR_MAX; i++) {
        uint8_t pin = color_to_pin(i);
        set_pin_output(pin);
        set_pin_low(pin);
        g_led_proxy.states[i] = LED_STATE_OFF;
        g_led_proxy.brightness[i] = 100;
    }
    g_led_proxy.initialized = true;
    return true;
}

/* 关键点：清晰的 API，隐藏位操作细节 */
void led_proxy_set_state(led_color_t color, led_state_t state) {
    if (!g_led_proxy.initialized || color >= LED_COLOR_MAX) {
        return;
    }
    
    uint8_t pin = color_to_pin(color);
    
    switch (state) {
        case LED_STATE_ON:
            set_pin_high(pin);
            break;
        case LED_STATE_OFF:
            set_pin_low(pin);
            break;
        case LED_STATE_BLINK:
            /* 闪烁逻辑由代理管理 */
            break;
    }
    
    g_led_proxy.states[color] = state;
}

led_state_t led_proxy_get_state(led_color_t color) {
    if (color >= LED_COLOR_MAX) {
        return LED_STATE_OFF;
    }
    return g_led_proxy.states[color];
}

void led_proxy_set_brightness(led_color_t color, uint8_t percent) {
    if (color >= LED_COLOR_MAX || percent > 100) {
        return;
    }
    /* 关键点：PWM 控制细节隐藏在代理内部 */
    g_led_proxy.brightness[color] = percent;
    /* 实际 PWM 配置代码... */
}

/* 便捷函数 */
void led_proxy_all_off(void) {
    for (int i = 0; i < LED_COLOR_MAX; i++) {
        led_proxy_set_state(i, LED_STATE_OFF);
    }
}

void led_proxy_all_on(void) {
    for (int i = 0; i < LED_COLOR_MAX; i++) {
        led_proxy_set_state(i, LED_STATE_ON);
    }
}

/*---------------------------------------------------------------------------
 * 客户端使用示例
 *---------------------------------------------------------------------------*/
void client_code_example(void) {
    /* 关键点：客户端使用清晰的 API，不接触硬件寄存器 */
    led_proxy_init();
    
    led_proxy_set_state(LED_COLOR_RED, LED_STATE_ON);
    led_proxy_set_brightness(LED_COLOR_RED, 50);
    
    led_proxy_set_state(LED_COLOR_GREEN, LED_STATE_BLINK);
    
    led_proxy_all_off();
}
```

---

## 2. 硬件适配器模式

### 架构图

```
+------------------------------------------------------------------+
|                   HARDWARE ADAPTER PATTERN                        |
+------------------------------------------------------------------+

    +------------------+                    +------------------+
    |   Application    |                    |   New Hardware   |
    |   (Client)       |                    |   (Different     |
    |                  |                    |    Interface)    |
    | Expects:         |                    | Provides:        |
    | - read_temp()    |                    | - get_raw_adc()  |
    | - read_humidity()|                    | - read_register()|
    +--------+---------+                    +--------+---------+
             |                                       ^
             |                                       |
             v                                       |
    +--------------------------------------------------+
    |                    ADAPTER                        |
    |  +--------------------------------------------+  |
    |  | read_temp() {                              |  |
    |  |   raw = hw->get_raw_adc(TEMP_CH);         |  |
    |  |   return convert_to_celsius(raw);          |  |
    |  | }                                          |  |
    |  +--------------------------------------------+  |
    +--------------------------------------------------+
```

**中文说明：**
- 适配器模式在客户端期望的接口和实际硬件接口之间建立桥梁
- 当更换硬件时，只需修改适配器，不影响应用层代码
- 适配器负责数据格式转换和接口映射

### 完整代码示例

```c
/*============================================================================
 * 硬件适配器模式示例 - 温度传感器适配
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>

/*---------------------------------------------------------------------------
 * 应用层期望的标准接口（Target Interface）
 *---------------------------------------------------------------------------*/
typedef struct {
    bool (*init)(void);
    float (*read_temperature)(void);        /* 返回摄氏度 */
    float (*read_humidity)(void);           /* 返回百分比 */
    bool (*is_ready)(void);
} sensor_interface_t;

/*---------------------------------------------------------------------------
 * 旧硬件接口（Adaptee - 被适配者）
 *---------------------------------------------------------------------------*/
/* 旧传感器 LM75 - 返回原始 ADC 值 */
typedef struct {
    uint8_t i2c_addr;
} lm75_sensor_t;

uint16_t lm75_read_raw(lm75_sensor_t *sensor) {
    /* 模拟读取原始值 */
    (void)sensor;
    return 2500;  /* 原始 ADC 值 */
}

/*---------------------------------------------------------------------------
 * 新硬件接口（Another Adaptee）
 *---------------------------------------------------------------------------*/
/* 新传感器 BME280 - 完全不同的接口 */
typedef struct {
    uint8_t spi_cs_pin;
    uint8_t mode;
} bme280_sensor_t;

int bme280_init_device(bme280_sensor_t *sensor, uint8_t cs, uint8_t mode) {
    sensor->spi_cs_pin = cs;
    sensor->mode = mode;
    return 0;
}

int32_t bme280_get_temperature_raw(bme280_sensor_t *sensor) {
    (void)sensor;
    return 25000;  /* 原始值，需要除以 1000 */
}

int32_t bme280_get_humidity_raw(bme280_sensor_t *sensor) {
    (void)sensor;
    return 65000;  /* 原始值，需要除以 1000 */
}

/*---------------------------------------------------------------------------
 * LM75 适配器实现
 *---------------------------------------------------------------------------*/
static lm75_sensor_t lm75_device;

/* 关键点：适配器将旧接口转换为标准接口 */
static bool lm75_adapter_init(void) {
    lm75_device.i2c_addr = 0x48;
    return true;
}

static float lm75_adapter_read_temp(void) {
    uint16_t raw = lm75_read_raw(&lm75_device);
    /* 关键点：数据转换 - 原始值转摄氏度 */
    return (float)raw / 100.0f;
}

static float lm75_adapter_read_humidity(void) {
    /* LM75 不支持湿度，返回无效值 */
    return -1.0f;
}

static bool lm75_adapter_is_ready(void) {
    return true;
}

/* 关键点：适配器导出的标准接口 */
const sensor_interface_t lm75_adapter = {
    .init = lm75_adapter_init,
    .read_temperature = lm75_adapter_read_temp,
    .read_humidity = lm75_adapter_read_humidity,
    .is_ready = lm75_adapter_is_ready
};

/*---------------------------------------------------------------------------
 * BME280 适配器实现
 *---------------------------------------------------------------------------*/
static bme280_sensor_t bme280_device;

static bool bme280_adapter_init(void) {
    return bme280_init_device(&bme280_device, 10, 1) == 0;
}

static float bme280_adapter_read_temp(void) {
    int32_t raw = bme280_get_temperature_raw(&bme280_device);
    /* 关键点：不同的转换公式 */
    return (float)raw / 1000.0f;
}

static float bme280_adapter_read_humidity(void) {
    int32_t raw = bme280_get_humidity_raw(&bme280_device);
    return (float)raw / 1000.0f;
}

static bool bme280_adapter_is_ready(void) {
    return true;
}

const sensor_interface_t bme280_adapter = {
    .init = bme280_adapter_init,
    .read_temperature = bme280_adapter_read_temp,
    .read_humidity = bme280_adapter_read_humidity,
    .is_ready = bme280_adapter_is_ready
};

/*---------------------------------------------------------------------------
 * 应用层代码（不关心具体硬件）
 *---------------------------------------------------------------------------*/
void application_code(const sensor_interface_t *sensor) {
    /* 关键点：应用层只使用标准接口 */
    sensor->init();
    
    if (sensor->is_ready()) {
        float temp = sensor->read_temperature();
        float humidity = sensor->read_humidity();
        
        printf("Temperature: %.2f C\n", temp);
        if (humidity >= 0) {
            printf("Humidity: %.2f %%\n", humidity);
        }
    }
}

/* 使用示例 */
void main_example(void) {
    /* 关键点：更换硬件只需更换适配器 */
    
    /* 使用旧传感器 */
    application_code(&lm75_adapter);
    
    /* 切换到新传感器 */
    application_code(&bme280_adapter);
}
```

---

## 3. 中介者模式

### 架构图

```
+------------------------------------------------------------------+
|                      MEDIATOR PATTERN                             |
+------------------------------------------------------------------+

    WITHOUT MEDIATOR (Tight Coupling):
    
    +--------+     +--------+     +--------+
    | Motor1 |<--->| Motor2 |<--->| Motor3 |
    +----+---+     +---+----+     +----+---+
         |             |              |
         +-------------+--------------+
         |             |              |
    +----+---+     +---+----+     +---+----+
    | Sensor1|     | Sensor2|     | Sensor3|
    +--------+     +--------+     +--------+
    
    Problem: Every component knows about others!


    WITH MEDIATOR (Loose Coupling):
    
    +--------+     +--------+     +--------+
    | Motor1 |     | Motor2 |     | Motor3 |
    +----+---+     +----+---+     +----+---+
         |             |              |
         +-------------+----+---------+
                            |
                            v
                   +------------------+
                   |     MEDIATOR     |
                   |  (Coordinator)   |
                   +--------+---------+
                            |
         +-------------+----+---------+
         |             |              |
    +----+---+     +---+----+     +---+----+
    | Sensor1|     | Sensor2|     | Sensor3|
    +--------+     +--------+     +--------+
```

**中文说明：**
- 中介者模式使用一个中央协调者来管理多个设备的交互
- 设备之间不直接通信，而是通过中介者协调
- 简化了设备间的耦合关系，便于添加或修改设备

### 完整代码示例

```c
/*============================================================================
 * 中介者模式示例 - 机械臂控制系统
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 硬件设备定义
 *---------------------------------------------------------------------------*/
/* 电机 */
typedef struct {
    uint8_t id;
    int16_t current_angle;
    int16_t target_angle;
    bool is_moving;
} motor_t;

/* 传感器 */
typedef struct {
    uint8_t id;
    int16_t position;
    bool limit_reached;
} position_sensor_t;

/* 气缸（夹爪） */
typedef struct {
    bool is_open;
    bool has_object;
} gripper_t;

/*---------------------------------------------------------------------------
 * 中介者结构体
 *---------------------------------------------------------------------------*/
#define MAX_MOTORS 3
#define MAX_SENSORS 3

/* 关键点：中介者持有所有协作设备的引用 */
typedef struct {
    motor_t *motors[MAX_MOTORS];
    int motor_count;
    
    position_sensor_t *sensors[MAX_SENSORS];
    int sensor_count;
    
    gripper_t *gripper;
    
    bool emergency_stop;
} robot_arm_mediator_t;

/*---------------------------------------------------------------------------
 * 设备操作函数
 *---------------------------------------------------------------------------*/
void motor_set_angle(motor_t *motor, int16_t angle) {
    motor->target_angle = angle;
    motor->is_moving = true;
    printf("[Motor %d] Moving to angle %d\n", motor->id, angle);
}

void motor_stop(motor_t *motor) {
    motor->is_moving = false;
    motor->target_angle = motor->current_angle;
    printf("[Motor %d] Stopped at angle %d\n", motor->id, motor->current_angle);
}

void gripper_open(gripper_t *gripper) {
    gripper->is_open = true;
    printf("[Gripper] Opened\n");
}

void gripper_close(gripper_t *gripper) {
    gripper->is_open = false;
    printf("[Gripper] Closed\n");
}

/*---------------------------------------------------------------------------
 * 中介者实现
 *---------------------------------------------------------------------------*/
static robot_arm_mediator_t g_mediator;

void mediator_init(void) {
    g_mediator.motor_count = 0;
    g_mediator.sensor_count = 0;
    g_mediator.gripper = NULL;
    g_mediator.emergency_stop = false;
}

void mediator_register_motor(motor_t *motor) {
    if (g_mediator.motor_count < MAX_MOTORS) {
        g_mediator.motors[g_mediator.motor_count++] = motor;
    }
}

void mediator_register_sensor(position_sensor_t *sensor) {
    if (g_mediator.sensor_count < MAX_SENSORS) {
        g_mediator.sensors[g_mediator.sensor_count++] = sensor;
    }
}

void mediator_register_gripper(gripper_t *gripper) {
    g_mediator.gripper = gripper;
}

/* 关键点：中介者协调所有设备完成复杂动作 */
void mediator_move_to_position(int16_t x, int16_t y, int16_t z) {
    if (g_mediator.emergency_stop) {
        printf("[Mediator] Emergency stop active!\n");
        return;
    }
    
    printf("[Mediator] Coordinating move to (%d, %d, %d)\n", x, y, z);
    
    /* 关键点：中介者知道如何协调各个电机 */
    /* 先抬高Z轴，避免碰撞 */
    if (g_mediator.motor_count >= 3) {
        motor_set_angle(g_mediator.motors[2], z + 50);
        /* 等待Z轴完成... */
    }
    
    /* 移动X和Y轴 */
    if (g_mediator.motor_count >= 2) {
        motor_set_angle(g_mediator.motors[0], x);
        motor_set_angle(g_mediator.motors[1], y);
        /* 等待完成... */
    }
    
    /* 降低Z轴到目标位置 */
    if (g_mediator.motor_count >= 3) {
        motor_set_angle(g_mediator.motors[2], z);
    }
}

/* 关键点：复杂的抓取动作由中介者协调 */
void mediator_pick_object(int16_t x, int16_t y, int16_t z) {
    printf("[Mediator] === Pick Object Sequence ===\n");
    
    /* 1. 移动到目标上方 */
    mediator_move_to_position(x, y, z + 30);
    
    /* 2. 打开夹爪 */
    if (g_mediator.gripper) {
        gripper_open(g_mediator.gripper);
    }
    
    /* 3. 下降到目标位置 */
    if (g_mediator.motor_count >= 3) {
        motor_set_angle(g_mediator.motors[2], z);
    }
    
    /* 4. 关闭夹爪 */
    if (g_mediator.gripper) {
        gripper_close(g_mediator.gripper);
        g_mediator.gripper->has_object = true;
    }
    
    /* 5. 抬起 */
    if (g_mediator.motor_count >= 3) {
        motor_set_angle(g_mediator.motors[2], z + 50);
    }
    
    printf("[Mediator] === Pick Complete ===\n");
}

/* 关键点：传感器事件通知中介者 */
void mediator_on_sensor_event(position_sensor_t *sensor) {
    printf("[Mediator] Sensor %d event: limit=%d\n", 
           sensor->id, sensor->limit_reached);
    
    if (sensor->limit_reached) {
        /* 中介者决定如何响应 */
        for (int i = 0; i < g_mediator.motor_count; i++) {
            if (g_mediator.motors[i]->id == sensor->id) {
                motor_stop(g_mediator.motors[i]);
            }
        }
    }
}

/* 紧急停止 */
void mediator_emergency_stop(void) {
    printf("[Mediator] !!! EMERGENCY STOP !!!\n");
    g_mediator.emergency_stop = true;
    
    /* 停止所有电机 */
    for (int i = 0; i < g_mediator.motor_count; i++) {
        motor_stop(g_mediator.motors[i]);
    }
    
    /* 打开夹爪（安全释放） */
    if (g_mediator.gripper) {
        gripper_open(g_mediator.gripper);
    }
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void mediator_example(void) {
    /* 创建设备 */
    motor_t motor_x = {.id = 0, .current_angle = 0};
    motor_t motor_y = {.id = 1, .current_angle = 0};
    motor_t motor_z = {.id = 2, .current_angle = 0};
    position_sensor_t sensor_x = {.id = 0};
    gripper_t gripper = {.is_open = false};
    
    /* 初始化中介者并注册设备 */
    mediator_init();
    mediator_register_motor(&motor_x);
    mediator_register_motor(&motor_y);
    mediator_register_motor(&motor_z);
    mediator_register_sensor(&sensor_x);
    mediator_register_gripper(&gripper);
    
    /* 关键点：通过中介者协调复杂动作 */
    mediator_pick_object(100, 200, 50);
    
    /* 模拟传感器触发 */
    sensor_x.limit_reached = true;
    mediator_on_sensor_event(&sensor_x);
}
```

---

## 4. 观察者模式

### 架构图

```
+------------------------------------------------------------------+
|                     OBSERVER PATTERN                              |
+------------------------------------------------------------------+

    +------------------+
    |     Subject      |
    |   (Sensor)       |
    +------------------+
    | - observers[]    |
    | - data           |
    +------------------+
    | + subscribe()    |
    | + unsubscribe()  |
    | + notify()       |
    +--------+---------+
             |
             | notify all
             |
    +--------v-------------------------------------------+
    |              OBSERVER LIST                         |
    | +------------+ +------------+ +------------+       |
    | |  Display   | |   Logger   | |   Alarm    |       |
    | |  update()  | |  update()  | |  update()  |       |
    | +------------+ +------------+ +------------+       |
    +----------------------------------------------------+


    Subscribe/Unsubscribe:
    
    observer.subscribe(subject)    --> Add to list
    observer.unsubscribe(subject)  --> Remove from list
    
    Data Change:
    
    subject.data_changed() --> notify() --> observer1.update()
                                        --> observer2.update()
                                        --> observer3.update()
```

**中文说明：**
- 观察者模式实现了发布-订阅机制
- 客户端动态订阅感兴趣的数据或事件
- 当数据变化时，所有订阅者自动收到通知
- 实现了主题和观察者之间的松耦合

### 完整代码示例

```c
/*============================================================================
 * 观察者模式示例 - 温度传感器监控系统
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 观察者接口定义
 *---------------------------------------------------------------------------*/
#define MAX_OBSERVERS 8

typedef struct observer observer_t;
typedef struct subject subject_t;

/* 关键点：观察者回调函数类型 */
typedef void (*observer_update_fn)(observer_t *self, void *data);

struct observer {
    const char *name;
    observer_update_fn update;
    void *user_data;
};

/*---------------------------------------------------------------------------
 * 主题（被观察者）定义
 *---------------------------------------------------------------------------*/
struct subject {
    observer_t *observers[MAX_OBSERVERS];
    int observer_count;
    void *data;
    size_t data_size;
};

/* 关键点：订阅 */
bool subject_subscribe(subject_t *subject, observer_t *observer) {
    if (subject->observer_count >= MAX_OBSERVERS) {
        return false;
    }
    
    /* 检查是否已订阅 */
    for (int i = 0; i < subject->observer_count; i++) {
        if (subject->observers[i] == observer) {
            return true;
        }
    }
    
    subject->observers[subject->observer_count++] = observer;
    printf("[Subject] Observer '%s' subscribed\n", observer->name);
    return true;
}

/* 关键点：取消订阅 */
bool subject_unsubscribe(subject_t *subject, observer_t *observer) {
    for (int i = 0; i < subject->observer_count; i++) {
        if (subject->observers[i] == observer) {
            /* 移除观察者 */
            for (int j = i; j < subject->observer_count - 1; j++) {
                subject->observers[j] = subject->observers[j + 1];
            }
            subject->observer_count--;
            printf("[Subject] Observer '%s' unsubscribed\n", observer->name);
            return true;
        }
    }
    return false;
}

/* 关键点：通知所有观察者 */
void subject_notify(subject_t *subject) {
    printf("[Subject] Notifying %d observers\n", subject->observer_count);
    
    for (int i = 0; i < subject->observer_count; i++) {
        observer_t *obs = subject->observers[i];
        if (obs && obs->update) {
            obs->update(obs, subject->data);
        }
    }
}

/*---------------------------------------------------------------------------
 * 温度传感器（具体主题）
 *---------------------------------------------------------------------------*/
typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp;
} sensor_data_t;

typedef struct {
    subject_t base;
    sensor_data_t current_data;
    float temp_threshold;
} temperature_sensor_t;

void temp_sensor_init(temperature_sensor_t *sensor, float threshold) {
    memset(sensor, 0, sizeof(*sensor));
    sensor->base.data = &sensor->current_data;
    sensor->base.data_size = sizeof(sensor_data_t);
    sensor->temp_threshold = threshold;
}

/* 关键点：数据更新时自动通知观察者 */
void temp_sensor_update(temperature_sensor_t *sensor, float temp, float humidity) {
    sensor->current_data.temperature = temp;
    sensor->current_data.humidity = humidity;
    sensor->current_data.timestamp++;
    
    printf("\n[Sensor] New reading: %.1f°C, %.1f%%\n", temp, humidity);
    
    /* 关键点：通知所有订阅者 */
    subject_notify(&sensor->base);
}

/*---------------------------------------------------------------------------
 * 具体观察者实现
 *---------------------------------------------------------------------------*/

/* 观察者1：LCD 显示器 */
void display_update(observer_t *self, void *data) {
    sensor_data_t *sensor_data = (sensor_data_t *)data;
    printf("  [%s] Display: Temp=%.1f°C, Humidity=%.1f%%\n",
           self->name, sensor_data->temperature, sensor_data->humidity);
}

/* 观察者2：数据记录器 */
typedef struct {
    int log_count;
} logger_context_t;

void logger_update(observer_t *self, void *data) {
    sensor_data_t *sensor_data = (sensor_data_t *)data;
    logger_context_t *ctx = (logger_context_t *)self->user_data;
    
    ctx->log_count++;
    printf("  [%s] Log #%d: %.1f,%.1f,%u\n",
           self->name, ctx->log_count,
           sensor_data->temperature,
           sensor_data->humidity,
           sensor_data->timestamp);
}

/* 观察者3：温度报警器 */
typedef struct {
    float high_threshold;
    float low_threshold;
} alarm_context_t;

void alarm_update(observer_t *self, void *data) {
    sensor_data_t *sensor_data = (sensor_data_t *)data;
    alarm_context_t *ctx = (alarm_context_t *)self->user_data;
    
    if (sensor_data->temperature > ctx->high_threshold) {
        printf("  [%s] ALARM! High temp: %.1f > %.1f\n",
               self->name, sensor_data->temperature, ctx->high_threshold);
    } else if (sensor_data->temperature < ctx->low_threshold) {
        printf("  [%s] ALARM! Low temp: %.1f < %.1f\n",
               self->name, sensor_data->temperature, ctx->low_threshold);
    } else {
        printf("  [%s] Temperature normal\n", self->name);
    }
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void observer_example(void) {
    /* 创建传感器（主题） */
    temperature_sensor_t sensor;
    temp_sensor_init(&sensor, 30.0f);
    
    /* 创建观察者 */
    observer_t display = {
        .name = "Display",
        .update = display_update,
        .user_data = NULL
    };
    
    logger_context_t logger_ctx = {.log_count = 0};
    observer_t logger = {
        .name = "Logger",
        .update = logger_update,
        .user_data = &logger_ctx
    };
    
    alarm_context_t alarm_ctx = {.high_threshold = 35.0f, .low_threshold = 10.0f};
    observer_t alarm = {
        .name = "Alarm",
        .update = alarm_update,
        .user_data = &alarm_ctx
    };
    
    /* 关键点：动态订阅 */
    subject_subscribe(&sensor.base, &display);
    subject_subscribe(&sensor.base, &logger);
    subject_subscribe(&sensor.base, &alarm);
    
    /* 模拟传感器数据更新 */
    temp_sensor_update(&sensor, 25.0f, 60.0f);
    temp_sensor_update(&sensor, 32.0f, 55.0f);
    temp_sensor_update(&sensor, 38.0f, 45.0f);  /* 触发高温报警 */
    
    /* 关键点：动态取消订阅 */
    printf("\n--- Unsubscribe logger ---\n");
    subject_unsubscribe(&sensor.base, &logger);
    
    temp_sensor_update(&sensor, 28.0f, 50.0f);
}
```

---

## 5. 去抖动模式

### 架构图

```
+------------------------------------------------------------------+
|                    DEBOUNCING PATTERN                             |
+------------------------------------------------------------------+

    Button Signal with Bounce:
    
    Press                               Release
      |                                    |
      v                                    v
    --+  +-+  +--+  +------------------+  +-+  +--+  +-------------
      |  | |  |  |  |                  |  | |  |  |  |
      +--+ +--+  +--+                  +--+ +--+  +--+
      
      |<-- Bounce -->|                 |<-- Bounce -->|
           ~20ms                            ~20ms


    Debounce State Machine:
    
    +------------+      Button        +-------------+
    |   IDLE     |    Pressed         | DEBOUNCING  |
    | (Waiting)  |-------------------->| (Waiting)   |
    +------------+                    +------+------+
          ^                                  |
          |                                  | Timer expires
          |                                  | & still pressed
          |                                  v
          |                           +-------------+
          |      Button               |  PRESSED    |
          +---------------------------| (Confirmed) |
                Released              +-------------+
```

**中文说明：**
- 去抖动模式用于处理机械按钮、开关的信号抖动
- 检测到状态变化后，等待一段时间让抖动衰减
- 重新采样确认状态是否真正改变
- 避免单次按键产生多次触发

### 完整代码示例

```c
/*============================================================================
 * 去抖动模式示例 - 按键去抖动
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 去抖动状态定义
 *---------------------------------------------------------------------------*/
typedef enum {
    DEBOUNCE_IDLE,          /* 空闲，等待按键 */
    DEBOUNCE_PRESS_WAIT,    /* 检测到按下，等待去抖 */
    DEBOUNCE_PRESSED,       /* 确认按下 */
    DEBOUNCE_RELEASE_WAIT   /* 检测到释放，等待去抖 */
} debounce_state_t;

typedef enum {
    BUTTON_EVENT_NONE,
    BUTTON_EVENT_PRESSED,
    BUTTON_EVENT_RELEASED
} button_event_t;

/*---------------------------------------------------------------------------
 * 去抖动器结构体
 *---------------------------------------------------------------------------*/
typedef struct {
    uint8_t gpio_pin;
    bool active_low;                /* 是否低电平有效 */
    
    debounce_state_t state;
    uint32_t state_enter_time;      /* 进入当前状态的时间 */
    uint32_t debounce_time_ms;      /* 去抖时间 */
    
    bool last_raw_state;            /* 上次原始状态 */
    bool stable_state;              /* 稳定后的状态 */
    
    /* 回调函数 */
    void (*on_press)(uint8_t pin);
    void (*on_release)(uint8_t pin);
} debouncer_t;

/*---------------------------------------------------------------------------
 * 模拟硬件接口
 *---------------------------------------------------------------------------*/
static uint32_t g_tick_ms = 0;

uint32_t get_tick_ms(void) {
    return g_tick_ms;
}

void tick_advance(uint32_t ms) {
    g_tick_ms += ms;
}

/* 模拟 GPIO 读取 */
static bool g_simulated_button_state = false;

bool gpio_read(uint8_t pin) {
    (void)pin;
    return g_simulated_button_state;
}

void simulate_button_press(void) {
    g_simulated_button_state = true;
}

void simulate_button_release(void) {
    g_simulated_button_state = false;
}

/*---------------------------------------------------------------------------
 * 去抖动器实现
 *---------------------------------------------------------------------------*/
void debouncer_init(debouncer_t *db, uint8_t pin, bool active_low, uint32_t debounce_ms) {
    db->gpio_pin = pin;
    db->active_low = active_low;
    db->debounce_time_ms = debounce_ms;
    
    db->state = DEBOUNCE_IDLE;
    db->state_enter_time = get_tick_ms();
    db->last_raw_state = active_low;  /* 默认释放状态 */
    db->stable_state = false;
    
    db->on_press = NULL;
    db->on_release = NULL;
}

void debouncer_set_callbacks(debouncer_t *db, 
                             void (*on_press)(uint8_t),
                             void (*on_release)(uint8_t)) {
    db->on_press = on_press;
    db->on_release = on_release;
}

/* 关键点：去抖动状态机处理 */
button_event_t debouncer_process(debouncer_t *db) {
    button_event_t event = BUTTON_EVENT_NONE;
    uint32_t now = get_tick_ms();
    
    /* 读取当前原始状态 */
    bool raw = gpio_read(db->gpio_pin);
    bool pressed = db->active_low ? !raw : raw;
    
    switch (db->state) {
        case DEBOUNCE_IDLE:
            /* 关键点：检测到按下，开始去抖计时 */
            if (pressed) {
                db->state = DEBOUNCE_PRESS_WAIT;
                db->state_enter_time = now;
                printf("[Debounce] Press detected, waiting...\n");
            }
            break;
            
        case DEBOUNCE_PRESS_WAIT:
            /* 关键点：等待去抖时间后重新采样 */
            if (now - db->state_enter_time >= db->debounce_time_ms) {
                if (pressed) {
                    /* 确认按下 */
                    db->state = DEBOUNCE_PRESSED;
                    db->stable_state = true;
                    event = BUTTON_EVENT_PRESSED;
                    printf("[Debounce] Press CONFIRMED\n");
                    
                    if (db->on_press) {
                        db->on_press(db->gpio_pin);
                    }
                } else {
                    /* 抖动，返回空闲 */
                    db->state = DEBOUNCE_IDLE;
                    printf("[Debounce] Was just bounce, ignored\n");
                }
            }
            break;
            
        case DEBOUNCE_PRESSED:
            /* 关键点：检测到释放，开始去抖计时 */
            if (!pressed) {
                db->state = DEBOUNCE_RELEASE_WAIT;
                db->state_enter_time = now;
                printf("[Debounce] Release detected, waiting...\n");
            }
            break;
            
        case DEBOUNCE_RELEASE_WAIT:
            if (now - db->state_enter_time >= db->debounce_time_ms) {
                if (!pressed) {
                    /* 确认释放 */
                    db->state = DEBOUNCE_IDLE;
                    db->stable_state = false;
                    event = BUTTON_EVENT_RELEASED;
                    printf("[Debounce] Release CONFIRMED\n");
                    
                    if (db->on_release) {
                        db->on_release(db->gpio_pin);
                    }
                } else {
                    /* 抖动，返回按下状态 */
                    db->state = DEBOUNCE_PRESSED;
                    printf("[Debounce] Was just bounce, still pressed\n");
                }
            }
            break;
    }
    
    db->last_raw_state = pressed;
    return event;
}

bool debouncer_is_pressed(debouncer_t *db) {
    return db->stable_state;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void on_button_press(uint8_t pin) {
    printf(">>> Button %d PRESSED! <<<\n", pin);
}

void on_button_release(uint8_t pin) {
    printf(">>> Button %d RELEASED! <<<\n", pin);
}

void debounce_example(void) {
    printf("=== Debouncing Pattern Demo ===\n\n");
    
    /* 初始化去抖动器 */
    debouncer_t button;
    debouncer_init(&button, 0, true, 20);  /* 20ms 去抖时间 */
    debouncer_set_callbacks(&button, on_button_press, on_button_release);
    
    /* 模拟按键抖动 */
    printf("--- Simulating button press with bounce ---\n");
    
    /* 初始抖动 */
    simulate_button_press();
    debouncer_process(&button);
    tick_advance(5);
    
    simulate_button_release();  /* 抖动 */
    debouncer_process(&button);
    tick_advance(3);
    
    simulate_button_press();    /* 抖动 */
    debouncer_process(&button);
    tick_advance(15);           /* 等待足够时间 */
    
    debouncer_process(&button); /* 确认按下 */
    
    /* 保持按下 */
    printf("\n--- Button held ---\n");
    tick_advance(100);
    debouncer_process(&button);
    
    /* 模拟释放抖动 */
    printf("\n--- Simulating button release with bounce ---\n");
    simulate_button_release();
    debouncer_process(&button);
    tick_advance(5);
    
    simulate_button_press();    /* 抖动 */
    debouncer_process(&button);
    tick_advance(3);
    
    simulate_button_release();
    debouncer_process(&button);
    tick_advance(25);           /* 等待足够时间 */
    
    debouncer_process(&button); /* 确认释放 */
}
```

---

## 6. 中断模式

### 架构图

```
+------------------------------------------------------------------+
|                      INTERRUPT PATTERN                            |
+------------------------------------------------------------------+

    Normal Execution Flow:
    
    main() --> task1() --> task2() --> task3() --> ...
    
    
    Interrupted Flow:
    
    main() --> task1() --> task2() --+
                                      |  IRQ Signal!
                                      v
                              +-------------+
                              |     ISR     |
                              | (Handler)   |
                              +------+------+
                                     |
                                     v
                              task2() --> task3() --> ...
                              (resume)


    ISR Processing:
    
    +----------------------------------------------------------+
    |  1. Save context (automatic by hardware)                  |
    |  2. Identify interrupt source                             |
    |  3. Clear interrupt flag                                  |
    |  4. Handle event (keep SHORT!)                            |
    |  5. Set flag / queue message for main loop                |
    |  6. Restore context (automatic by hardware)               |
    +----------------------------------------------------------+
```

**中文说明：**
- 中断模式用于处理高紧急性事件
- 当中断发生时，暂停当前执行，跳转到 ISR 处理
- ISR 应该尽量简短，只做最必要的工作
- 复杂处理延迟到主循环中执行

### 完整代码示例

```c
/*============================================================================
 * 中断模式示例 - UART 接收中断
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 环形缓冲区（用于 ISR 和主循环之间的数据传递）
 *---------------------------------------------------------------------------*/
#define RING_BUFFER_SIZE 64

typedef struct {
    volatile uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} ring_buffer_t;

void ring_buffer_init(ring_buffer_t *rb) {
    rb->head = rb->tail = rb->count = 0;
}

/* 关键点：此函数在 ISR 中调用，必须简短 */
bool ring_buffer_put_isr(ring_buffer_t *rb, uint8_t data) {
    if (rb->count >= RING_BUFFER_SIZE) {
        return false;  /* 缓冲区满 */
    }
    
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RING_BUFFER_SIZE;
    rb->count++;
    return true;
}

/* 此函数在主循环中调用 */
bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data) {
    if (rb->count == 0) {
        return false;  /* 缓冲区空 */
    }
    
    /* 关键点：读取时禁用中断，防止竞态条件 */
    /* 在实际系统中：__disable_irq(); */
    
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    rb->count--;
    
    /* __enable_irq(); */
    return true;
}

uint16_t ring_buffer_available(ring_buffer_t *rb) {
    return rb->count;
}

/*---------------------------------------------------------------------------
 * UART 驱动（中断模式）
 *---------------------------------------------------------------------------*/
/* 模拟硬件寄存器 */
typedef struct {
    volatile uint32_t SR;   /* 状态寄存器 */
    volatile uint32_t DR;   /* 数据寄存器 */
    volatile uint32_t CR1;  /* 控制寄存器 */
} UART_TypeDef;

#define UART_SR_RXNE    (1 << 5)    /* 接收非空标志 */
#define UART_SR_TXE     (1 << 7)    /* 发送空标志 */
#define UART_CR1_RXNEIE (1 << 5)    /* 接收中断使能 */

static UART_TypeDef g_uart1;

/* 接收缓冲区 */
static ring_buffer_t uart_rx_buffer;

/* 关键点：标志变量（ISR 和主循环之间通信） */
static volatile bool uart_data_ready = false;
static volatile uint32_t uart_rx_count = 0;

/*---------------------------------------------------------------------------
 * 关键点：中断服务程序（ISR）
 *---------------------------------------------------------------------------*/
/* 
 * ISR 设计原则：
 * 1. 尽量简短
 * 2. 不要调用可能阻塞的函数
 * 3. 使用 volatile 变量与主循环通信
 * 4. 清除中断标志
 */
void UART1_IRQHandler(void) {
    /* 关键点：检查中断源 */
    if (g_uart1.SR & UART_SR_RXNE) {
        /* 读取数据（同时清除 RXNE 标志） */
        uint8_t data = (uint8_t)g_uart1.DR;
        
        /* 关键点：只做最少的工作 - 放入缓冲区 */
        ring_buffer_put_isr(&uart_rx_buffer, data);
        
        /* 设置标志通知主循环 */
        uart_data_ready = true;
        uart_rx_count++;
    }
}

/*---------------------------------------------------------------------------
 * UART 初始化和 API
 *---------------------------------------------------------------------------*/
void uart_init(void) {
    ring_buffer_init(&uart_rx_buffer);
    uart_data_ready = false;
    uart_rx_count = 0;
    
    /* 配置 UART（简化） */
    g_uart1.CR1 |= UART_CR1_RXNEIE;  /* 使能接收中断 */
    
    /* 实际系统中还需要：
     * - 配置波特率
     * - 配置 NVIC
     * - 使能 UART
     */
    
    printf("[UART] Initialized with RX interrupt\n");
}

/* 非阻塞读取 */
int uart_read_byte(uint8_t *data) {
    if (ring_buffer_get(&uart_rx_buffer, data)) {
        return 1;
    }
    return 0;
}

/* 读取多个字节 */
int uart_read(uint8_t *buffer, int max_len) {
    int count = 0;
    uint8_t byte;
    
    while (count < max_len && ring_buffer_get(&uart_rx_buffer, &byte)) {
        buffer[count++] = byte;
    }
    
    return count;
}

/* 检查是否有数据 */
bool uart_data_available(void) {
    return ring_buffer_available(&uart_rx_buffer) > 0;
}

/*---------------------------------------------------------------------------
 * 模拟中断触发
 *---------------------------------------------------------------------------*/
void simulate_uart_receive(const char *data) {
    printf("[Simulate] UART receiving: \"%s\"\n", data);
    
    while (*data) {
        /* 模拟硬件接收 */
        g_uart1.SR |= UART_SR_RXNE;
        g_uart1.DR = *data++;
        
        /* 触发中断处理 */
        UART1_IRQHandler();
    }
}

/*---------------------------------------------------------------------------
 * 主循环处理
 *---------------------------------------------------------------------------*/
void process_uart_data(void) {
    /* 关键点：主循环中处理数据，而不是在 ISR 中 */
    if (uart_data_ready) {
        uart_data_ready = false;
        
        uint8_t buffer[64];
        int len = uart_read(buffer, sizeof(buffer) - 1);
        
        if (len > 0) {
            buffer[len] = '\0';
            printf("[Main] Received %d bytes: \"%s\"\n", len, buffer);
            
            /* 复杂处理在这里进行 */
            /* 例如：解析命令、发送响应等 */
        }
    }
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void interrupt_example(void) {
    printf("=== Interrupt Pattern Demo ===\n\n");
    
    uart_init();
    
    /* 模拟主循环 */
    printf("\n--- Main loop iteration 1 ---\n");
    process_uart_data();  /* 无数据 */
    
    /* 模拟接收数据 */
    simulate_uart_receive("Hello");
    
    printf("\n--- Main loop iteration 2 ---\n");
    process_uart_data();  /* 处理数据 */
    
    /* 再次模拟接收 */
    simulate_uart_receive("World!");
    
    printf("\n--- Main loop iteration 3 ---\n");
    process_uart_data();
    
    printf("\n[Stats] Total bytes received: %u\n", uart_rx_count);
}
```

---

## 7. 轮询模式

### 架构图

```
+------------------------------------------------------------------+
|                      POLLING PATTERN                              |
+------------------------------------------------------------------+

    Periodic Polling:
    
    +-------+     +-------+     +-------+     +-------+
    | Timer |---->| Poll  |---->| Poll  |---->| Poll  |---->
    +-------+     +-------+     +-------+     +-------+
        |             |             |             |
        v             v             v             v
    [Sample]     [Sample]     [Sample]     [Sample]
        |             |             |             |
        No data      Data!        No data      Data!
                       |                          |
                       v                          v
                   [Process]                  [Process]


    Opportunistic Polling:
    
    main_loop() {
        while (1) {
            do_task_1();
            
            poll_sensor();   // Check when convenient
            
            do_task_2();
            
            poll_uart();     // Check when convenient
            
            do_task_3();
        }
    }
```

**中文说明：**
- 轮询模式周期性或机会性地检查硬件状态
- 适用于非紧急数据或可预测的采样间隔
- 实现简单，不需要中断配置
- 需要确保轮询频率足够快以不丢失数据

### 完整代码示例

```c
/*============================================================================
 * 轮询模式示例 - 多传感器轮询系统
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/*---------------------------------------------------------------------------
 * 时间管理
 *---------------------------------------------------------------------------*/
static uint32_t g_system_tick = 0;

uint32_t get_tick(void) {
    return g_system_tick;
}

void tick_increment(uint32_t ms) {
    g_system_tick += ms;
}

/*---------------------------------------------------------------------------
 * 轮询调度器
 *---------------------------------------------------------------------------*/
typedef void (*poll_callback_t)(void);

typedef struct {
    const char *name;
    poll_callback_t callback;
    uint32_t interval_ms;       /* 轮询间隔 */
    uint32_t last_poll_time;    /* 上次轮询时间 */
    uint32_t poll_count;        /* 轮询计数 */
    bool enabled;
} poll_task_t;

#define MAX_POLL_TASKS 8
static poll_task_t g_poll_tasks[MAX_POLL_TASKS];
static int g_poll_task_count = 0;

/* 注册轮询任务 */
int poll_register(const char *name, poll_callback_t callback, uint32_t interval_ms) {
    if (g_poll_task_count >= MAX_POLL_TASKS) {
        return -1;
    }
    
    poll_task_t *task = &g_poll_tasks[g_poll_task_count];
    task->name = name;
    task->callback = callback;
    task->interval_ms = interval_ms;
    task->last_poll_time = get_tick();
    task->poll_count = 0;
    task->enabled = true;
    
    printf("[Poller] Registered task '%s' with %ums interval\n", name, interval_ms);
    
    return g_poll_task_count++;
}

/* 关键点：轮询调度器 - 检查所有任务是否该执行 */
void poll_scheduler_run(void) {
    uint32_t now = get_tick();
    
    for (int i = 0; i < g_poll_task_count; i++) {
        poll_task_t *task = &g_poll_tasks[i];
        
        if (!task->enabled) {
            continue;
        }
        
        /* 关键点：检查是否到达轮询时间 */
        if (now - task->last_poll_time >= task->interval_ms) {
            task->callback();
            task->last_poll_time = now;
            task->poll_count++;
        }
    }
}

void poll_enable(int task_id, bool enable) {
    if (task_id >= 0 && task_id < g_poll_task_count) {
        g_poll_tasks[task_id].enabled = enable;
    }
}

/*---------------------------------------------------------------------------
 * 模拟传感器
 *---------------------------------------------------------------------------*/
/* 温度传感器 */
static float g_temperature = 25.0f;

bool temperature_sensor_has_new_data(void) {
    /* 模拟：每次都有新数据 */
    return true;
}

float temperature_sensor_read(void) {
    /* 模拟温度变化 */
    g_temperature += ((float)(get_tick() % 10) - 5.0f) * 0.1f;
    return g_temperature;
}

/* ADC */
static uint16_t g_adc_value = 2048;

bool adc_conversion_complete(void) {
    return true;
}

uint16_t adc_read(void) {
    g_adc_value = 2000 + (get_tick() % 100);
    return g_adc_value;
}

/* GPIO 输入 */
bool gpio_read_input(uint8_t pin) {
    /* 模拟每 500ms 状态翻转 */
    return (get_tick() / 500) % 2 == 0;
}

/*---------------------------------------------------------------------------
 * 轮询回调函数
 *---------------------------------------------------------------------------*/
void poll_temperature(void) {
    if (temperature_sensor_has_new_data()) {
        float temp = temperature_sensor_read();
        printf("  [Temp] %.2f°C\n", temp);
        
        /* 处理数据... */
    }
}

void poll_adc(void) {
    if (adc_conversion_complete()) {
        uint16_t value = adc_read();
        printf("  [ADC] %u (%.2fV)\n", value, value * 3.3f / 4096);
        
        /* 启动下一次转换... */
    }
}

void poll_gpio(void) {
    static bool last_state = false;
    bool current = gpio_read_input(0);
    
    if (current != last_state) {
        printf("  [GPIO] State changed to: %s\n", current ? "HIGH" : "LOW");
        last_state = current;
    }
}

void poll_heartbeat(void) {
    static uint32_t count = 0;
    printf("  [Heartbeat] #%u at %ums\n", ++count, get_tick());
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void polling_example(void) {
    printf("=== Polling Pattern Demo ===\n\n");
    
    /* 注册轮询任务 */
    poll_register("Temperature", poll_temperature, 100);  /* 100ms */
    poll_register("ADC", poll_adc, 50);                   /* 50ms */
    poll_register("GPIO", poll_gpio, 10);                 /* 10ms */
    poll_register("Heartbeat", poll_heartbeat, 500);      /* 500ms */
    
    printf("\n--- Starting polling loop ---\n\n");
    
    /* 模拟主循环运行 2 秒 */
    for (int i = 0; i < 40; i++) {
        printf("[Tick %u]\n", get_tick());
        
        /* 关键点：在主循环中调用轮询调度器 */
        poll_scheduler_run();
        
        /* 模拟时间流逝 */
        tick_increment(50);
        
        /* 其他任务... */
    }
    
    /* 打印统计 */
    printf("\n--- Polling Statistics ---\n");
    for (int i = 0; i < g_poll_task_count; i++) {
        printf("  %s: %u polls\n", 
               g_poll_tasks[i].name, 
               g_poll_tasks[i].poll_count);
    }
}
```

---

## 总结

| 模式 | 适用场景 | 核心优势 |
|------|----------|----------|
| 硬件代理 | 封装硬件访问细节 | 隔离硬件变化 |
| 硬件适配器 | 接口不兼容的硬件 | 无需修改客户端 |
| 中介者 | 多设备协调 | 简化设备间耦合 |
| 观察者 | 数据发布订阅 | 动态添加订阅者 |
| 去抖动 | 机械开关信号 | 消除抖动干扰 |
| 中断 | 高紧急性事件 | 及时响应 |
| 轮询 | 非紧急数据采集 | 实现简单 |

