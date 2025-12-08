# C语言软件分层架构 - 层间交互方法详解

## 分层架构核心思想

```
+------------------------------------------------------------------+
|                      LAYERED ARCHITECTURE                         |
+------------------------------------------------------------------+

    +------------------+
    |  APPLICATION     |  Layer N (Highest)
    +--------+---------+
             |
             | depends on
             v
    +------------------+
    |    SERVICE       |  Layer N-1
    +--------+---------+
             |
             | depends on
             v
    +------------------+
    |    DRIVER        |  Layer N-2
    +--------+---------+
             |
             | depends on
             v
    +------------------+
    |   HARDWARE       |  Layer 0 (Lowest)
    +------------------+


    KEY PRINCIPLES:
    +-----------------------------------------------------------+
    |  1. Upper layers DEPEND ON lower layers (not reverse)     |
    |  2. Each layer has SINGLE RESPONSIBILITY                  |
    |  3. Layers communicate through WELL-DEFINED INTERFACES    |
    |  4. Changes in one layer should NOT affect other layers   |
    +-----------------------------------------------------------+
```

**分层核心思想说明：**
- **单向依赖**：上层依赖下层，下层不能依赖上层
- **职责分离**：每层只负责特定功能域
- **接口隔离**：层间通过明确定义的接口通信
- **变化隔离**：一层的修改不应影响其他层

---

## 方法一：直接调用 (Direct Call)

```
+------------------------------------------------------------------+
|                       DIRECT CALL PATTERN                         |
+------------------------------------------------------------------+

    +------------------+
    |   Application    |
    |                  |
    |  app_function()  |
    |       |          |
    +-------+----------+
            |
            | direct function call
            v
    +------------------+
    |     Service      |
    |                  |
    |  service_func()  |
    |       |          |
    +-------+----------+
            |
            | direct function call
            v
    +------------------+
    |     Driver       |
    |                  |
    |  driver_func()   |
    +------------------+


    Call Flow:
    app_function() ---> service_func() ---> driver_func()
                                                  |
                                                  v
                                            return value
                                                  |
    app_function() <--- service_func() <----------+
```

**直接调用说明：**
- 最简单的层间通信方式
- 上层直接调用下层函数，获取返回值
- 同步阻塞执行

### 完整代码示例

```c
/*============================================================================
 * 直接调用模式示例 - 温度监控系统
 *============================================================================*/

/*---------------------------------------------------------------------------
 * Driver Layer (驱动层) - driver_temp_sensor.h/c
 *---------------------------------------------------------------------------*/
/* driver_temp_sensor.h */
#ifndef DRIVER_TEMP_SENSOR_H
#define DRIVER_TEMP_SENSOR_H

#include <stdint.h>

typedef enum {
    SENSOR_OK = 0,
    SENSOR_ERROR,
    SENSOR_TIMEOUT
} sensor_status_t;

/* 驱动层接口：直接操作硬件 */
sensor_status_t driver_temp_init(void);
sensor_status_t driver_temp_read_raw(uint16_t *raw_value);

#endif

/* driver_temp_sensor.c */
#include "driver_temp_sensor.h"
#include "hal_i2c.h"

#define SENSOR_I2C_ADDR  0x48
#define REG_TEMPERATURE  0x00

sensor_status_t driver_temp_init(void) {
    /* 初始化 I2C 外设 */
    if (hal_i2c_init(I2C_PORT_1, 100000) != HAL_OK) {
        return SENSOR_ERROR;
    }
    return SENSOR_OK;
}

sensor_status_t driver_temp_read_raw(uint16_t *raw_value) {
    uint8_t data[2];
    
    /* 直接读取传感器寄存器 */
    if (hal_i2c_read_reg(SENSOR_I2C_ADDR, REG_TEMPERATURE, data, 2) != HAL_OK) {
        return SENSOR_ERROR;
    }
    
    *raw_value = (data[0] << 8) | data[1];
    return SENSOR_OK;
}

/*---------------------------------------------------------------------------
 * Service Layer (服务层) - service_temperature.h/c
 *---------------------------------------------------------------------------*/
/* service_temperature.h */
#ifndef SERVICE_TEMPERATURE_H
#define SERVICE_TEMPERATURE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float celsius;
    float fahrenheit;
    bool  is_valid;
} temperature_t;

/* 服务层接口：提供业务逻辑 */
bool service_temp_init(void);
temperature_t service_temp_get(void);
bool service_temp_is_overheating(float threshold);

#endif

/* service_temperature.c */
#include "service_temperature.h"
#include "driver_temp_sensor.h"  /* 服务层依赖驱动层 */

/* 内部状态 */
static bool initialized = false;
static temperature_t last_reading = {0};

bool service_temp_init(void) {
    /* 关键点：服务层直接调用驱动层初始化 */
    if (driver_temp_init() != SENSOR_OK) {
        return false;
    }
    initialized = true;
    return true;
}

temperature_t service_temp_get(void) {
    temperature_t result = {0, 0, false};
    uint16_t raw;
    
    if (!initialized) {
        return result;
    }
    
    /* 关键点：直接调用驱动层读取原始数据 */
    if (driver_temp_read_raw(&raw) != SENSOR_OK) {
        return result;
    }
    
    /* 服务层负责数据转换（业务逻辑） */
    result.celsius = (float)(raw >> 4) * 0.0625f;
    result.fahrenheit = result.celsius * 9.0f / 5.0f + 32.0f;
    result.is_valid = true;
    
    last_reading = result;
    return result;
}

bool service_temp_is_overheating(float threshold) {
    temperature_t temp = service_temp_get();
    return temp.is_valid && (temp.celsius > threshold);
}

/*---------------------------------------------------------------------------
 * Application Layer (应用层) - main.c
 *---------------------------------------------------------------------------*/
#include "service_temperature.h"  /* 应用层依赖服务层 */
#include <stdio.h>

#define OVERHEAT_THRESHOLD  45.0f

void app_temperature_monitor(void) {
    /* 关键点：应用层直接调用服务层接口 */
    temperature_t temp = service_temp_get();
    
    if (temp.is_valid) {
        printf("Temperature: %.1f C / %.1f F\n", 
               temp.celsius, temp.fahrenheit);
        
        /* 直接调用服务层判断逻辑 */
        if (service_temp_is_overheating(OVERHEAT_THRESHOLD)) {
            printf("WARNING: Overheating!\n");
            /* 触发报警 */
        }
    }
}

int main(void) {
    /* 初始化：调用链 app -> service -> driver */
    if (!service_temp_init()) {
        printf("Temperature sensor init failed!\n");
        return -1;
    }
    
    while (1) {
        app_temperature_monitor();
        delay_ms(1000);
    }
}
```

**直接调用优势分析：**

| 优势 | 说明 |
|------|------|
| **简单直观** | 函数调用关系清晰，易于理解和调试 |
| **类型安全** | 编译器检查参数类型和返回值 |
| **高效** | 无额外开销，直接跳转执行 |
| **可追踪** | 调用栈清晰，便于定位问题 |

| 局限 | 说明 |
|------|------|
| **强耦合** | 上层直接依赖下层具体实现 |
| **同步阻塞** | 调用者必须等待返回 |
| **难以测试** | 无法轻易替换下层实现 |

---

## 方法二：回调函数 (Callback)

```
+------------------------------------------------------------------+
|                      CALLBACK PATTERN                             |
+------------------------------------------------------------------+

    REGISTRATION PHASE:
    +------------------+                    +------------------+
    |   Application    |  register_callback |     Service      |
    |                  | -----------------> |                  |
    |  my_handler()    |                    | callback_ptr = & |
    |                  |                    |   my_handler     |
    +------------------+                    +------------------+

    EXECUTION PHASE:
    +------------------+                    +------------------+
    |   Application    |                    |     Service      |
    |                  |                    |                  |
    |  my_handler() <--|--------------------| (*callback_ptr)()|
    |      |           |  invoke callback   |   event occurred |
    |      v           |                    |                  |
    |  handle event    |                    |                  |
    +------------------+                    +------------------+


    Control Flow (Inversion of Control):
    
    Normal:     App ---calls---> Service
    Callback:   App <--called--- Service  (Service calls back to App)
```

**回调函数说明：**
- 下层定义回调接口（函数指针类型）
- 上层实现具体处理函数并注册
- 事件发生时，下层通过函数指针调用上层
- 实现"控制反转"：下层调用上层，但不依赖上层

### 完整代码示例

```c
/*============================================================================
 * 回调函数模式示例 - 按键事件处理系统
 *============================================================================*/

/*---------------------------------------------------------------------------
 * Service Layer - 按键服务（定义回调接口）
 *---------------------------------------------------------------------------*/
/* button_service.h */
#ifndef BUTTON_SERVICE_H
#define BUTTON_SERVICE_H

#include <stdint.h>

/* 按键事件类型 */
typedef enum {
    BTN_EVENT_PRESS,
    BTN_EVENT_RELEASE,
    BTN_EVENT_LONG_PRESS,
    BTN_EVENT_DOUBLE_CLICK
} button_event_t;

/* 关键点：定义回调函数类型（接口契约） */
typedef void (*button_callback_t)(uint8_t button_id, button_event_t event);

/* 服务层 API */
void button_service_init(void);
void button_service_process(void);  /* 在主循环中调用 */

/* 关键点：注册回调的接口 */
void button_register_callback(uint8_t button_id, button_callback_t callback);
void button_unregister_callback(uint8_t button_id);

#endif

/* button_service.c */
#include "button_service.h"
#include "hal_gpio.h"
#include "hal_tick.h"

#define MAX_BUTTONS      4
#define DEBOUNCE_MS      20
#define LONG_PRESS_MS    1500
#define DOUBLE_CLICK_MS  300

typedef struct {
    uint8_t  pin;
    uint8_t  last_state;
    uint8_t  stable_state;
    uint32_t state_change_time;
    uint32_t press_start_time;
    uint32_t last_click_time;
    uint8_t  click_count;
    button_callback_t callback;  /* 关键点：存储回调函数指针 */
} button_context_t;

static button_context_t buttons[MAX_BUTTONS];
static const uint8_t button_pins[MAX_BUTTONS] = {GPIO_PA0, GPIO_PA1, GPIO_PA2, GPIO_PA3};

void button_service_init(void) {
    for (int i = 0; i < MAX_BUTTONS; i++) {
        buttons[i].pin = button_pins[i];
        buttons[i].callback = NULL;  /* 初始无回调 */
        buttons[i].last_state = 1;
        buttons[i].stable_state = 1;
        hal_gpio_init(buttons[i].pin, GPIO_MODE_INPUT_PULLUP);
    }
}

/* 关键点：上层通过此函数注入自己的处理逻辑 */
void button_register_callback(uint8_t button_id, button_callback_t callback) {
    if (button_id < MAX_BUTTONS) {
        buttons[button_id].callback = callback;
    }
}

void button_unregister_callback(uint8_t button_id) {
    if (button_id < MAX_BUTTONS) {
        buttons[button_id].callback = NULL;
    }
}

/* 内部函数：触发回调 */
static void notify_event(uint8_t id, button_event_t event) {
    /* 关键点：检查回调是否已注册，然后调用 */
    if (buttons[id].callback != NULL) {
        buttons[id].callback(id, event);  /* 调用上层注册的函数 */
    }
}

void button_service_process(void) {
    uint32_t now = hal_tick_get_ms();
    
    for (uint8_t i = 0; i < MAX_BUTTONS; i++) {
        button_context_t *btn = &buttons[i];
        uint8_t current = hal_gpio_read(btn->pin);
        
        /* 消抖处理 */
        if (current != btn->last_state) {
            btn->state_change_time = now;
            btn->last_state = current;
        }
        
        if ((now - btn->state_change_time) < DEBOUNCE_MS) {
            continue;
        }
        
        /* 状态变化检测 */
        if (current != btn->stable_state) {
            btn->stable_state = current;
            
            if (current == 0) {  /* 按下 */
                btn->press_start_time = now;
                notify_event(i, BTN_EVENT_PRESS);  /* 触发按下事件 */
                
                /* 双击检测 */
                if ((now - btn->last_click_time) < DOUBLE_CLICK_MS) {
                    btn->click_count++;
                    if (btn->click_count >= 2) {
                        notify_event(i, BTN_EVENT_DOUBLE_CLICK);
                        btn->click_count = 0;
                    }
                } else {
                    btn->click_count = 1;
                }
                btn->last_click_time = now;
                
            } else {  /* 释放 */
                notify_event(i, BTN_EVENT_RELEASE);  /* 触发释放事件 */
            }
        }
        
        /* 长按检测 */
        if (btn->stable_state == 0 && btn->press_start_time > 0) {
            if ((now - btn->press_start_time) >= LONG_PRESS_MS) {
                notify_event(i, BTN_EVENT_LONG_PRESS);  /* 触发长按事件 */
                btn->press_start_time = 0;
            }
        }
    }
}

/*---------------------------------------------------------------------------
 * Application Layer - 应用层（实现并注册回调）
 *---------------------------------------------------------------------------*/
#include "button_service.h"
#include "led_service.h"
#include "buzzer_service.h"
#include <stdio.h>

/* 关键点：应用层实现具体的事件处理逻辑 */
void power_button_handler(uint8_t id, button_event_t event) {
    printf("[Power Button] Event: %d\n", event);
    
    switch (event) {
        case BTN_EVENT_PRESS:
            led_on(LED_POWER);
            break;
            
        case BTN_EVENT_RELEASE:
            led_off(LED_POWER);
            break;
            
        case BTN_EVENT_LONG_PRESS:
            printf("Entering sleep mode...\n");
            system_enter_sleep();
            break;
            
        case BTN_EVENT_DOUBLE_CLICK:
            printf("Showing battery status\n");
            show_battery_status();
            break;
    }
}

void volume_up_handler(uint8_t id, button_event_t event) {
    if (event == BTN_EVENT_PRESS) {
        audio_volume_increase();
        buzzer_beep(50);  /* 短促提示音 */
    }
}

void volume_down_handler(uint8_t id, button_event_t event) {
    if (event == BTN_EVENT_PRESS) {
        audio_volume_decrease();
        buzzer_beep(50);
    }
}

void menu_button_handler(uint8_t id, button_event_t event) {
    switch (event) {
        case BTN_EVENT_PRESS:
            menu_highlight_current();
            break;
        case BTN_EVENT_RELEASE:
            menu_select_current();
            break;
        case BTN_EVENT_LONG_PRESS:
            menu_go_back();
            break;
        default:
            break;
    }
}

int main(void) {
    /* 初始化服务 */
    button_service_init();
    led_service_init();
    buzzer_service_init();
    
    /* 关键点：注册回调函数 - 将应用逻辑注入到服务层 */
    button_register_callback(0, power_button_handler);
    button_register_callback(1, volume_up_handler);
    button_register_callback(2, volume_down_handler);
    button_register_callback(3, menu_button_handler);
    
    printf("Button demo started. Press buttons...\n");
    
    /* 主循环 */
    while (1) {
        button_service_process();  /* 服务层处理，会回调应用层 */
        delay_ms(10);
    }
}
```

**回调函数优势分析：**

| 优势 | 说明 |
|------|------|
| **解耦** | 服务层不依赖应用层的具体实现 |
| **控制反转** | 事件驱动，服务层通知应用层 |
| **可扩展** | 不同应用可注册不同处理逻辑 |
| **运行时配置** | 可动态更换回调函数 |
| **复用性高** | 同一按键服务可用于完全不同的应用场景 |

| 对比 | 直接调用 | 回调函数 |
|------|----------|----------|
| 调用方向 | 上层 → 下层 | 下层 → 上层 |
| 耦合性 | 上层依赖下层 | 下层不依赖上层 |
| 适用场景 | 请求-响应 | 事件通知 |

---

## 方法三：消息/事件队列 (Message Queue)

```
+------------------------------------------------------------------+
|                    MESSAGE QUEUE PATTERN                          |
+------------------------------------------------------------------+

    +------------------+                    +------------------+
    |    Producer      |                    |    Consumer      |
    |   (Lower Layer)  |                    |  (Upper Layer)   |
    +--------+---------+                    +--------+---------+
             |                                       ^
             | post_message()                        | get_message()
             v                                       |
    +----------------------------------------------------------+
    |                     MESSAGE QUEUE                         |
    |  +------+  +------+  +------+  +------+  +------+        |
    |  | MSG1 |  | MSG2 |  | MSG3 |  | MSG4 |  | .... |        |
    |  +------+  +------+  +------+  +------+  +------+        |
    |  <-- tail                               head -->          |
    +----------------------------------------------------------+


    ASYNCHRONOUS COMMUNICATION:
    
    Time -->
    
    Producer:  [post]     [post]          [post]
                 |          |               |
                 v          v               v
    Queue:     [M1]       [M1,M2]        [M1,M2,M3]
                                              |
    Consumer:              [get M1]      [get M2]   [get M3]
                              |              |          |
                              v              v          v
                          [process]     [process]  [process]
```

**消息队列说明：**
- 生产者（通常是下层/中断）将消息放入队列
- 消费者（通常是上层/主循环）从队列取出处理
- 异步通信：生产者不等待消费者处理完成
- 缓冲作用：平滑处理速率差异

### 完整代码示例

```c
/*============================================================================
 * 消息队列模式示例 - 多事件处理系统
 *============================================================================*/

/*---------------------------------------------------------------------------
 * Message Queue Infrastructure (基础设施层)
 *---------------------------------------------------------------------------*/
/* message_queue.h */
#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

/* 消息类型定义 */
typedef enum {
    MSG_TYPE_BUTTON,
    MSG_TYPE_UART_RX,
    MSG_TYPE_TIMER,
    MSG_TYPE_SENSOR,
    MSG_TYPE_NETWORK,
    MSG_TYPE_MAX
} msg_type_t;

/* 通用消息结构 */
typedef struct {
    msg_type_t type;
    uint32_t   timestamp;
    union {
        struct { uint8_t id; uint8_t event; } button;
        struct { uint8_t *data; uint16_t len; } uart;
        struct { uint8_t timer_id; } timer;
        struct { uint8_t sensor_id; int32_t value; } sensor;
        struct { uint8_t event; uint32_t param; } network;
    } payload;
} message_t;

/* 消息队列 API */
void msg_queue_init(void);
bool msg_queue_post(const message_t *msg);      /* 发送消息（可在 ISR 中调用） */
bool msg_queue_get(message_t *msg);             /* 获取消息（阻塞） */
bool msg_queue_try_get(message_t *msg);         /* 尝试获取（非阻塞） */
uint16_t msg_queue_count(void);
bool msg_queue_is_empty(void);
bool msg_queue_is_full(void);

#endif

/* message_queue.c */
#include "message_queue.h"
#include "critical_section.h"
#include <string.h>

#define QUEUE_SIZE  32

static message_t queue_buffer[QUEUE_SIZE];
static volatile uint16_t head = 0;
static volatile uint16_t tail = 0;
static volatile uint16_t count = 0;

void msg_queue_init(void) {
    head = tail = count = 0;
}

/* 关键点：发送消息，可在中断中调用 */
bool msg_queue_post(const message_t *msg) {
    bool success = false;
    
    CRITICAL_ENTER();  /* 进入临界区，防止竞争 */
    {
        if (count < QUEUE_SIZE) {
            memcpy(&queue_buffer[head], msg, sizeof(message_t));
            head = (head + 1) % QUEUE_SIZE;
            count++;
            success = true;
        }
    }
    CRITICAL_EXIT();
    
    return success;
}

/* 关键点：获取消息，在主循环中调用 */
bool msg_queue_get(message_t *msg) {
    bool success = false;
    
    CRITICAL_ENTER();
    {
        if (count > 0) {
            memcpy(msg, &queue_buffer[tail], sizeof(message_t));
            tail = (tail + 1) % QUEUE_SIZE;
            count--;
            success = true;
        }
    }
    CRITICAL_EXIT();
    
    return success;
}

bool msg_queue_try_get(message_t *msg) {
    return msg_queue_get(msg);  /* 简化实现 */
}

uint16_t msg_queue_count(void) {
    return count;
}

bool msg_queue_is_empty(void) {
    return count == 0;
}

bool msg_queue_is_full(void) {
    return count >= QUEUE_SIZE;
}

/*---------------------------------------------------------------------------
 * Driver Layer - 消息生产者（中断服务程序）
 *---------------------------------------------------------------------------*/
#include "message_queue.h"
#include "hal_tick.h"

/* UART 接收中断 - 生产消息 */
void UART1_IRQHandler(void) {
    static uint8_t rx_buffer[64];
    static uint8_t rx_index = 0;
    
    uint8_t byte = UART1->DR;  /* 读取接收数据 */
    
    if (byte == '\n' || rx_index >= 63) {
        /* 收到完整帧，发送消息 */
        message_t msg = {
            .type = MSG_TYPE_UART_RX,
            .timestamp = hal_tick_get_ms(),
            .payload.uart.data = rx_buffer,
            .payload.uart.len = rx_index
        };
        
        /* 关键点：在中断中发送消息到队列 */
        msg_queue_post(&msg);
        rx_index = 0;
    } else {
        rx_buffer[rx_index++] = byte;
    }
}

/* 定时器中断 - 生产消息 */
void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~TIM_SR_UIF;
        
        message_t msg = {
            .type = MSG_TYPE_TIMER,
            .timestamp = hal_tick_get_ms(),
            .payload.timer.timer_id = 2
        };
        
        msg_queue_post(&msg);
    }
}

/* 按键中断 - 生产消息 */
void EXTI0_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR0) {
        EXTI->PR = EXTI_PR_PR0;
        
        message_t msg = {
            .type = MSG_TYPE_BUTTON,
            .timestamp = hal_tick_get_ms(),
            .payload.button.id = 0,
            .payload.button.event = (GPIOA->IDR & 0x01) ? 0 : 1
        };
        
        msg_queue_post(&msg);
    }
}

/*---------------------------------------------------------------------------
 * Application Layer - 消息消费者（主循环）
 *---------------------------------------------------------------------------*/
#include "message_queue.h"
#include <stdio.h>

/* 各类型消息的处理函数 */
static void handle_button_message(const message_t *msg) {
    printf("[%lu] Button %d: %s\n", 
           msg->timestamp,
           msg->payload.button.id,
           msg->payload.button.event ? "pressed" : "released");
    
    if (msg->payload.button.event) {
        led_toggle(msg->payload.button.id);
    }
}

static void handle_uart_message(const message_t *msg) {
    printf("[%lu] UART received %d bytes: ", 
           msg->timestamp, msg->payload.uart.len);
    
    /* 处理接收到的数据 */
    process_uart_command(msg->payload.uart.data, msg->payload.uart.len);
}

static void handle_timer_message(const message_t *msg) {
    printf("[%lu] Timer %d expired\n", 
           msg->timestamp, msg->payload.timer.timer_id);
    
    /* 定时任务处理 */
    if (msg->payload.timer.timer_id == 2) {
        sensor_read_periodic();
    }
}

static void handle_sensor_message(const message_t *msg) {
    printf("[%lu] Sensor %d value: %ld\n",
           msg->timestamp,
           msg->payload.sensor.sensor_id,
           msg->payload.sensor.value);
    
    /* 传感器数据处理 */
    data_logger_record(msg->payload.sensor.sensor_id, 
                       msg->payload.sensor.value);
}

/* 关键点：消息分发处理器 */
static void message_dispatcher(const message_t *msg) {
    switch (msg->type) {
        case MSG_TYPE_BUTTON:
            handle_button_message(msg);
            break;
        case MSG_TYPE_UART_RX:
            handle_uart_message(msg);
            break;
        case MSG_TYPE_TIMER:
            handle_timer_message(msg);
            break;
        case MSG_TYPE_SENSOR:
            handle_sensor_message(msg);
            break;
        default:
            printf("Unknown message type: %d\n", msg->type);
            break;
    }
}

int main(void) {
    /* 初始化 */
    msg_queue_init();
    uart_init();
    timer_init();
    gpio_interrupt_init();
    
    printf("Message queue demo started.\n");
    
    /* 关键点：主循环 - 消息消费者 */
    while (1) {
        message_t msg;
        
        /* 从队列获取消息并处理 */
        if (msg_queue_get(&msg)) {
            message_dispatcher(&msg);
        }
        
        /* 可以在这里做其他低优先级任务 */
        idle_task();
    }
}
```

**消息队列优势分析：**

| 优势 | 说明 |
|------|------|
| **异步解耦** | 生产者和消费者独立运行，互不阻塞 |
| **缓冲削峰** | 队列缓冲突发事件，平滑处理 |
| **中断安全** | 中断只需快速投递消息，处理延后 |
| **时序解耦** | 事件产生和处理的时间解耦 |
| **优先级处理** | 可实现优先级队列，重要消息优先处理 |
| **调试友好** | 消息可记录、回放、分析 |

| 对比 | 回调函数 | 消息队列 |
|------|----------|----------|
| 执行时机 | 事件发生时立即执行 | 主循环中延后执行 |
| 执行上下文 | 可能在中断中 | 总是在主循环中 |
| 实时性 | 高 | 略低（取决于队列深度） |
| 复杂度 | 低 | 中等 |
| 适用场景 | 简单事件通知 | 复杂多事件协调 |

