# Embedded C Software Reusability Engineering Techniques

## 1. Layered Architecture

```
+--------------------------------------------------+
|                APPLICATION LAYER                 |
|         (Business Logic, State Machines)         |
+--------------------------------------------------+
                        |
                        v
+--------------------------------------------------+
|                  SERVICE LAYER                   |
|    (Protocol Stack, Algorithm, Data Process)     |
+--------------------------------------------------+
                        |
                        v
+--------------------------------------------------+
|                  DRIVER LAYER                    |
|          (Hardware Abstraction Layer)            |
+--------------------------------------------------+
                        |
                        v
+--------------------------------------------------+
|                 HARDWARE LAYER                   |
|        (MCU Registers, Peripherals, GPIO)        |
+--------------------------------------------------+
```

**分层架构说明：**
- 上层只能调用下层，下层不能依赖上层
- 每层有明确的职责边界
- 更换硬件平台时，只需修改底层驱动，上层业务逻辑无需改动

### Complete Code Example

```c
/*============================================================================
 * File: layered_architecture_example.c
 * Description: Demonstrates layered architecture in embedded C
 *============================================================================*/

/*---------------------------------------------------------------------------
 * HARDWARE LAYER - MCU specific registers
 *---------------------------------------------------------------------------*/
// stm32f4xx.h (simplified)
#define GPIOA_BASE      0x40020000
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14))
#define GPIOA_IDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x10))

/*---------------------------------------------------------------------------
 * DRIVER LAYER - Hardware Abstraction
 *---------------------------------------------------------------------------*/
// hal_gpio.h
typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET   = 1
} gpio_state_t;

void hal_gpio_write(uint8_t pin, gpio_state_t state) {
    if (state == GPIO_PIN_SET) {
        GPIOA_ODR |= (1 << pin);
    } else {
        GPIOA_ODR &= ~(1 << pin);
    }
}

gpio_state_t hal_gpio_read(uint8_t pin) {
    return (GPIOA_IDR & (1 << pin)) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

/*---------------------------------------------------------------------------
 * SERVICE LAYER - Reusable Components
 *---------------------------------------------------------------------------*/
// led_service.h
typedef struct {
    uint8_t pin;
    uint8_t is_on;
} led_t;

void led_init(led_t *led, uint8_t pin) {
    led->pin = pin;
    led->is_on = 0;
    hal_gpio_write(pin, GPIO_PIN_RESET);
}

void led_on(led_t *led) {
    led->is_on = 1;
    hal_gpio_write(led->pin, GPIO_PIN_SET);
}

void led_off(led_t *led) {
    led->is_on = 0;
    hal_gpio_write(led->pin, GPIO_PIN_RESET);
}

void led_toggle(led_t *led) {
    led->is_on ? led_off(led) : led_on(led);
}

/*---------------------------------------------------------------------------
 * APPLICATION LAYER - Business Logic
 *---------------------------------------------------------------------------*/
// main.c
led_t status_led;
led_t error_led;

void app_init(void) {
    led_init(&status_led, 5);
    led_init(&error_led, 6);
}

void app_on_error(void) {
    led_on(&error_led);
}

void app_heartbeat(void) {
    led_toggle(&status_led);
}
```

**代码示例优势分析：**
1. `led_service` 模块可在任何项目中复用，只需提供 `hal_gpio` 实现
2. 更换 MCU 时，只需重写 `hal_gpio_write/read`，LED 服务代码无需修改
3. 应用层代码清晰表达业务意图，不涉及硬件细节

---

## 2. Hardware Abstraction Layer (HAL)

```
+------------------+    +------------------+    +------------------+
|   Application    |    |   Application    |    |   Application    |
|    (Reusable)    |    |    (Reusable)    |    |    (Reusable)    |
+--------+---------+    +--------+---------+    +--------+---------+
         |                       |                       |
         v                       v                       v
+------------------------------------------------------------------+
|                     HAL Interface (hal.h)                        |
|   hal_gpio_init()  hal_uart_send()  hal_spi_transfer()  ...      |
+------------------------------------------------------------------+
         |                       |                       |
         v                       v                       v
+------------------+    +------------------+    +------------------+
|   STM32 HAL      |    |   NRF52 HAL      |    |   ESP32 HAL      |
| (hal_stm32.c)    |    | (hal_nrf52.c)    |    | (hal_esp32.c)    |
+------------------+    +------------------+    +------------------+
         |                       |                       |
         v                       v                       v
+------------------+    +------------------+    +------------------+
|   STM32F4 MCU    |    |   NRF52840 MCU   |    |   ESP32 MCU      |
+------------------+    +------------------+    +------------------+
```

**硬件抽象层说明：**
- 定义统一的硬件操作接口（头文件）
- 每个平台提供各自的实现（源文件）
- 应用代码只依赖接口，实现编译时可切换

### Complete Code Example

```c
/*============================================================================
 * File: hal_uart.h
 * Description: UART Hardware Abstraction Layer Interface
 *============================================================================*/
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    HAL_UART_OK       = 0,
    HAL_UART_ERROR    = 1,
    HAL_UART_BUSY     = 2,
    HAL_UART_TIMEOUT  = 3
} hal_uart_status_t;

typedef struct {
    uint32_t baudrate;
    uint8_t  data_bits;    // 7, 8, 9
    uint8_t  stop_bits;    // 1, 2
    uint8_t  parity;       // 0=None, 1=Odd, 2=Even
} hal_uart_config_t;

/* Interface functions - implemented per platform */
hal_uart_status_t hal_uart_init(uint8_t port, const hal_uart_config_t *config);
hal_uart_status_t hal_uart_deinit(uint8_t port);
hal_uart_status_t hal_uart_send(uint8_t port, const uint8_t *data, size_t len);
hal_uart_status_t hal_uart_receive(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms);
size_t hal_uart_available(uint8_t port);

#endif /* HAL_UART_H */


/*============================================================================
 * File: hal_uart_stm32.c
 * Description: UART HAL Implementation for STM32
 *============================================================================*/
#include "hal_uart.h"
#include "stm32f4xx_hal.h"

static UART_HandleTypeDef huart_handles[3];

hal_uart_status_t hal_uart_init(uint8_t port, const hal_uart_config_t *config) {
    UART_HandleTypeDef *huart = &huart_handles[port];
    
    huart->Instance = (port == 0) ? USART1 : 
                      (port == 1) ? USART2 : USART3;
    huart->Init.BaudRate = config->baudrate;
    huart->Init.WordLength = (config->data_bits == 9) ? 
                              UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
    huart->Init.StopBits = (config->stop_bits == 2) ? 
                            UART_STOPBITS_2 : UART_STOPBITS_1;
    huart->Init.Parity = (config->parity == 1) ? UART_PARITY_ODD :
                         (config->parity == 2) ? UART_PARITY_EVEN : UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    
    if (HAL_UART_Init(huart) != HAL_OK) {
        return HAL_UART_ERROR;
    }
    return HAL_UART_OK;
}

hal_uart_status_t hal_uart_send(uint8_t port, const uint8_t *data, size_t len) {
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart_handles[port], 
                                                  (uint8_t *)data, len, 1000);
    return (status == HAL_OK) ? HAL_UART_OK : HAL_UART_ERROR;
}

hal_uart_status_t hal_uart_receive(uint8_t port, uint8_t *data, size_t len, uint32_t timeout_ms) {
    HAL_StatusTypeDef status = HAL_UART_Receive(&huart_handles[port], 
                                                 data, len, timeout_ms);
    if (status == HAL_OK) return HAL_UART_OK;
    if (status == HAL_TIMEOUT) return HAL_UART_TIMEOUT;
    return HAL_UART_ERROR;
}


/*============================================================================
 * File: hal_uart_nrf52.c
 * Description: UART HAL Implementation for NRF52
 *============================================================================*/
#include "hal_uart.h"
#include "nrf_uarte.h"
#include "nrfx_uarte.h"

static nrfx_uarte_t uart_instances[] = {
    NRFX_UARTE_INSTANCE(0),
    NRFX_UARTE_INSTANCE(1)
};

hal_uart_status_t hal_uart_init(uint8_t port, const hal_uart_config_t *config) {
    nrfx_uarte_config_t nrf_config = NRFX_UARTE_DEFAULT_CONFIG;
    
    /* Map baudrate */
    switch (config->baudrate) {
        case 9600:   nrf_config.baudrate = NRF_UARTE_BAUDRATE_9600;   break;
        case 115200: nrf_config.baudrate = NRF_UARTE_BAUDRATE_115200; break;
        default:     nrf_config.baudrate = NRF_UARTE_BAUDRATE_115200; break;
    }
    
    if (nrfx_uarte_init(&uart_instances[port], &nrf_config, NULL) != NRFX_SUCCESS) {
        return HAL_UART_ERROR;
    }
    return HAL_UART_OK;
}

hal_uart_status_t hal_uart_send(uint8_t port, const uint8_t *data, size_t len) {
    if (nrfx_uarte_tx(&uart_instances[port], data, len) != NRFX_SUCCESS) {
        return HAL_UART_ERROR;
    }
    return HAL_UART_OK;
}


/*============================================================================
 * File: protocol_parser.c
 * Description: Reusable Protocol Parser (Platform Independent)
 *============================================================================*/
#include "hal_uart.h"

#define FRAME_HEADER  0xAA
#define FRAME_TAIL    0x55

typedef struct {
    uint8_t port;
    uint8_t buffer[256];
    size_t  index;
} protocol_context_t;

void protocol_init(protocol_context_t *ctx, uint8_t uart_port) {
    ctx->port = uart_port;
    ctx->index = 0;
    
    hal_uart_config_t config = {
        .baudrate  = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity    = 0
    };
    hal_uart_init(uart_port, &config);  /* Uses HAL, platform independent */
}

void protocol_send_frame(protocol_context_t *ctx, const uint8_t *payload, size_t len) {
    uint8_t frame[260];
    frame[0] = FRAME_HEADER;
    frame[1] = (uint8_t)len;
    memcpy(&frame[2], payload, len);
    frame[2 + len] = FRAME_TAIL;
    
    hal_uart_send(ctx->port, frame, 3 + len);  /* Uses HAL */
}
```

**HAL 优势分析：**
1. **可移植性**：`protocol_parser.c` 可在 STM32、NRF52、ESP32 等任意平台运行
2. **编译时切换**：通过 Makefile 选择链接哪个 `hal_uart_xxx.c`
3. **单元测试**：可创建 `hal_uart_mock.c` 用于 PC 端测试
4. **团队协作**：硬件工程师负责 HAL 实现，软件工程师专注业务逻辑

---

## 3. Callback Functions and Function Pointers

```
+-------------------+
|   Button Module   |
|   (Reusable)      |
+-------------------+
         |
         | on_press(button_id)
         | on_release(button_id)
         | on_long_press(button_id)
         v
+-------------------+       +-------------------+       +-------------------+
|   Application A   |       |   Application B   |       |   Application C   |
|   Toggle LED      |       |   Send Command    |       |   Play Sound      |
+-------------------+       +-------------------+       +-------------------+


                    +---------------------------+
                    |      Function Pointer     |
                    |        Registration       |
                    +---------------------------+
                               |
         +---------------------+---------------------+
         |                     |                     |
         v                     v                     v
+----------------+    +----------------+    +----------------+
| button_set_    |    | button_set_    |    | button_set_    |
| callback(      |    | callback(      |    | callback(      |
|   ON_PRESS,    |    |   ON_PRESS,    |    |   ON_PRESS,    |
|   led_toggle   |    |   send_cmd     |    |   play_beep    |
| )              |    | )              |    | )              |
+----------------+    +----------------+    +----------------+
```

**回调函数说明：**
- 模块定义"事件发生时调用什么"的接口（函数指针）
- 应用层注入具体的处理函数
- 模块无需知道具体做什么，实现解耦

### Complete Code Example

```c
/*============================================================================
 * File: button.h
 * Description: Reusable Button Module with Callback Support
 *============================================================================*/
#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

/* Callback function type definitions */
typedef void (*button_callback_t)(uint8_t button_id);

/* Button event types */
typedef enum {
    BUTTON_EVENT_PRESS,
    BUTTON_EVENT_RELEASE,
    BUTTON_EVENT_LONG_PRESS,
    BUTTON_EVENT_DOUBLE_CLICK,
    BUTTON_EVENT_MAX
} button_event_t;

/* Button configuration structure */
typedef struct {
    uint8_t  id;
    uint8_t  gpio_pin;
    uint8_t  active_low;           /* 1 = active low, 0 = active high */
    uint16_t debounce_ms;          /* Debounce time in milliseconds */
    uint16_t long_press_ms;        /* Long press threshold */
    uint16_t double_click_ms;      /* Double click window */
    button_callback_t callbacks[BUTTON_EVENT_MAX];
} button_config_t;

/* Public API */
void button_init(button_config_t *config);
void button_set_callback(button_config_t *config, 
                         button_event_t event, 
                         button_callback_t callback);
void button_process(button_config_t *config);  /* Call in main loop or timer ISR */

#endif /* BUTTON_H */


/*============================================================================
 * File: button.c
 * Description: Button Module Implementation
 *============================================================================*/
#include "button.h"
#include "hal_gpio.h"
#include "hal_tick.h"

typedef struct {
    uint8_t  last_state;
    uint8_t  stable_state;
    uint32_t last_change_time;
    uint32_t press_start_time;
    uint32_t last_click_time;
    uint8_t  click_count;
} button_state_t;

static button_state_t states[8];  /* Support up to 8 buttons */

void button_init(button_config_t *config) {
    hal_gpio_init(config->gpio_pin, GPIO_MODE_INPUT_PULLUP);
    
    button_state_t *state = &states[config->id];
    state->last_state = config->active_low ? 1 : 0;
    state->stable_state = state->last_state;
    state->last_change_time = 0;
    state->press_start_time = 0;
    state->last_click_time = 0;
    state->click_count = 0;
}

void button_set_callback(button_config_t *config, 
                         button_event_t event, 
                         button_callback_t callback) {
    if (event < BUTTON_EVENT_MAX) {
        config->callbacks[event] = callback;
    }
}

/* Internal: Invoke callback if registered */
static void invoke_callback(button_config_t *config, button_event_t event) {
    if (config->callbacks[event] != NULL) {
        config->callbacks[event](config->id);
    }
}

void button_process(button_config_t *config) {
    button_state_t *state = &states[config->id];
    uint32_t now = hal_tick_get_ms();
    
    /* Read current pin state */
    uint8_t current = hal_gpio_read(config->gpio_pin);
    uint8_t pressed = config->active_low ? (current == 0) : (current == 1);
    
    /* Debounce */
    if (current != state->last_state) {
        state->last_change_time = now;
        state->last_state = current;
    }
    
    if ((now - state->last_change_time) < config->debounce_ms) {
        return;  /* Still bouncing */
    }
    
    /* State changed after debounce */
    if (pressed != state->stable_state) {
        state->stable_state = pressed;
        
        if (pressed) {
            /* Button pressed */
            state->press_start_time = now;
            invoke_callback(config, BUTTON_EVENT_PRESS);
            
            /* Check double click */
            if ((now - state->last_click_time) < config->double_click_ms) {
                state->click_count++;
                if (state->click_count >= 2) {
                    invoke_callback(config, BUTTON_EVENT_DOUBLE_CLICK);
                    state->click_count = 0;
                }
            } else {
                state->click_count = 1;
            }
            state->last_click_time = now;
            
        } else {
            /* Button released */
            invoke_callback(config, BUTTON_EVENT_RELEASE);
        }
    }
    
    /* Check long press (while still pressed) */
    if (pressed && state->press_start_time > 0) {
        if ((now - state->press_start_time) >= config->long_press_ms) {
            invoke_callback(config, BUTTON_EVENT_LONG_PRESS);
            state->press_start_time = 0;  /* Only trigger once */
        }
    }
}


/*============================================================================
 * File: main.c (Application A - LED Control)
 * Description: Using button module with LED callbacks
 *============================================================================*/
#include "button.h"
#include "led.h"

static led_t status_led;
static button_config_t power_button;

/* Application-specific callbacks */
void on_power_press(uint8_t id) {
    led_on(&status_led);
}

void on_power_release(uint8_t id) {
    led_off(&status_led);
}

void on_power_long_press(uint8_t id) {
    led_blink(&status_led, 3);  /* Blink 3 times */
    system_enter_sleep();
}

void on_power_double_click(uint8_t id) {
    led_toggle(&status_led);
}

int main(void) {
    /* Initialize LED */
    led_init(&status_led, GPIO_PIN_5);
    
    /* Initialize button with configuration */
    power_button.id = 0;
    power_button.gpio_pin = GPIO_PIN_2;
    power_button.active_low = 1;
    power_button.debounce_ms = 20;
    power_button.long_press_ms = 2000;
    power_button.double_click_ms = 300;
    
    button_init(&power_button);
    
    /* Register callbacks - THIS IS THE KEY! */
    button_set_callback(&power_button, BUTTON_EVENT_PRESS, on_power_press);
    button_set_callback(&power_button, BUTTON_EVENT_RELEASE, on_power_release);
    button_set_callback(&power_button, BUTTON_EVENT_LONG_PRESS, on_power_long_press);
    button_set_callback(&power_button, BUTTON_EVENT_DOUBLE_CLICK, on_power_double_click);
    
    while (1) {
        button_process(&power_button);
    }
}


/*============================================================================
 * File: main_app_b.c (Application B - Remote Control)
 * Description: Same button module, different callbacks
 *============================================================================*/
#include "button.h"
#include "rf_transmitter.h"

static button_config_t remote_buttons[4];

void send_command_a(uint8_t id) { rf_send_code(0x01); }
void send_command_b(uint8_t id) { rf_send_code(0x02); }
void send_command_c(uint8_t id) { rf_send_code(0x03); }
void send_command_d(uint8_t id) { rf_send_code(0x04); }

int main(void) {
    /* Same button module, completely different behavior! */
    for (int i = 0; i < 4; i++) {
        remote_buttons[i].id = i;
        remote_buttons[i].gpio_pin = GPIO_PIN_2 + i;
        remote_buttons[i].active_low = 1;
        remote_buttons[i].debounce_ms = 50;
        button_init(&remote_buttons[i]);
    }
    
    button_set_callback(&remote_buttons[0], BUTTON_EVENT_PRESS, send_command_a);
    button_set_callback(&remote_buttons[1], BUTTON_EVENT_PRESS, send_command_b);
    button_set_callback(&remote_buttons[2], BUTTON_EVENT_PRESS, send_command_c);
    button_set_callback(&remote_buttons[3], BUTTON_EVENT_PRESS, send_command_d);
    
    while (1) {
        for (int i = 0; i < 4; i++) {
            button_process(&remote_buttons[i]);
        }
    }
}
```

**回调函数优势分析：**
1. **模块复用**：同一个 `button.c` 用于 LED 控制和遥控器，无需修改
2. **行为注入**：应用层决定"按钮按下做什么"，模块只负责检测
3. **运行时配置**：可动态更换回调函数，无需重新编译
4. **事件驱动**：清晰的事件-响应模型，代码易于理解和维护

---

## 4. Configuration Separation

```
+------------------------------------------------------------------+
|                        PROJECT STRUCTURE                          |
+------------------------------------------------------------------+

project_a/                           project_b/
+------------------+                 +------------------+
|  config/         |                 |  config/         |
|  +-------------+ |                 |  +-------------+ |
|  |board_cfg.h  | |                 |  |board_cfg.h  | |
|  |UART=115200  | |                 |  |UART=9600    | |
|  |LED_PIN=5    | |                 |  |LED_PIN=13   | |
|  +-------------+ |                 |  +-------------+ |
+--------+---------+                 +--------+---------+
         |                                    |
         +----------------+-------------------+
                          |
                          v
         +----------------------------------+
         |       SHARED COMPONENTS          |
         |  (Reusable across projects)      |
         |  +----------------------------+  |
         |  | led.c  | button.c | uart.c |  |
         |  +----------------------------+  |
         |  | #include "board_cfg.h"     |  |
         |  | Uses config macros         |  |
         |  +----------------------------+  |
         +----------------------------------+
```

**配置分离说明：**
- 可变参数（引脚、波特率、缓冲区大小等）提取到配置头文件
- 可复用模块通过 `#include` 引用配置
- 不同项目只需修改配置文件，无需改动模块代码

### Complete Code Example

```c
/*============================================================================
 * File: board_config.h (Project A - STM32 Development Board)
 * Description: Project-specific configuration
 *============================================================================*/
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*---------------------------------------------------------------------------
 * System Configuration
 *---------------------------------------------------------------------------*/
#define SYSTEM_CLOCK_HZ         72000000
#define SYSTICK_FREQ_HZ         1000

/*---------------------------------------------------------------------------
 * UART Configuration
 *---------------------------------------------------------------------------*/
#define UART_DEBUG_PORT         0
#define UART_DEBUG_BAUDRATE     115200
#define UART_DEBUG_TX_PIN       GPIO_PA9
#define UART_DEBUG_RX_PIN       GPIO_PA10
#define UART_RX_BUFFER_SIZE     256
#define UART_TX_BUFFER_SIZE     256

/*---------------------------------------------------------------------------
 * LED Configuration
 *---------------------------------------------------------------------------*/
#define LED_STATUS_PIN          GPIO_PC13
#define LED_ERROR_PIN           GPIO_PC14
#define LED_ACTIVE_LOW          1

/*---------------------------------------------------------------------------
 * Button Configuration
 *---------------------------------------------------------------------------*/
#define BUTTON_USER_PIN         GPIO_PA0
#define BUTTON_ACTIVE_LOW       1
#define BUTTON_DEBOUNCE_MS      20

/*---------------------------------------------------------------------------
 * I2C Configuration
 *---------------------------------------------------------------------------*/
#define I2C_SENSOR_PORT         0
#define I2C_SENSOR_SPEED        100000
#define I2C_SENSOR_SDA_PIN      GPIO_PB7
#define I2C_SENSOR_SCL_PIN      GPIO_PB6

/*---------------------------------------------------------------------------
 * Feature Flags
 *---------------------------------------------------------------------------*/
#define FEATURE_ENABLE_DEBUG_LOG    1
#define FEATURE_ENABLE_WATCHDOG     1
#define FEATURE_ENABLE_SLEEP_MODE   0

#endif /* BOARD_CONFIG_H */


/*============================================================================
 * File: board_config.h (Project B - Custom Product)
 * Description: Different project, different configuration
 *============================================================================*/
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define SYSTEM_CLOCK_HZ         48000000
#define SYSTICK_FREQ_HZ         1000

#define UART_DEBUG_PORT         1
#define UART_DEBUG_BAUDRATE     9600      /* Different baudrate */
#define UART_DEBUG_TX_PIN       GPIO_PB10
#define UART_DEBUG_RX_PIN       GPIO_PB11
#define UART_RX_BUFFER_SIZE     128       /* Smaller buffer */
#define UART_TX_BUFFER_SIZE     128

#define LED_STATUS_PIN          GPIO_PA5  /* Different pin */
#define LED_ERROR_PIN           GPIO_PA6
#define LED_ACTIVE_LOW          0         /* Active high this time */

#define BUTTON_USER_PIN         GPIO_PC13
#define BUTTON_ACTIVE_LOW       0
#define BUTTON_DEBOUNCE_MS      30

#define FEATURE_ENABLE_DEBUG_LOG    0     /* Disabled for production */
#define FEATURE_ENABLE_WATCHDOG     1
#define FEATURE_ENABLE_SLEEP_MODE   1

#endif /* BOARD_CONFIG_H */


/*============================================================================
 * File: uart_driver.c
 * Description: UART driver using configuration macros
 *============================================================================*/
#include "uart_driver.h"
#include "board_config.h"

/* Buffers sized by configuration */
static uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
static uint8_t tx_buffer[UART_TX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

void uart_debug_init(void) {
    hal_uart_config_t config = {
        .baudrate  = UART_DEBUG_BAUDRATE,   /* From config */
        .data_bits = 8,
        .stop_bits = 1,
        .parity    = 0
    };
    
    hal_gpio_init(UART_DEBUG_TX_PIN, GPIO_MODE_AF_PP);  /* From config */
    hal_gpio_init(UART_DEBUG_RX_PIN, GPIO_MODE_INPUT);  /* From config */
    hal_uart_init(UART_DEBUG_PORT, &config);            /* From config */
}

void uart_debug_send(const char *str) {
    hal_uart_send(UART_DEBUG_PORT, (const uint8_t *)str, strlen(str));
}


/*============================================================================
 * File: debug_log.c
 * Description: Debug logging with feature flag
 *============================================================================*/
#include "debug_log.h"
#include "board_config.h"
#include "uart_driver.h"
#include <stdio.h>
#include <stdarg.h>

void debug_log(const char *fmt, ...) {
#if FEATURE_ENABLE_DEBUG_LOG    /* Compile-time feature toggle */
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    uart_debug_send(buffer);
#else
    (void)fmt;  /* Suppress unused parameter warning */
    /* Function compiles to nothing when disabled - zero overhead */
#endif
}


/*============================================================================
 * File: led_driver.c
 * Description: LED driver using configuration
 *============================================================================*/
#include "led_driver.h"
#include "board_config.h"

void led_status_init(void) {
    hal_gpio_init(LED_STATUS_PIN, GPIO_MODE_OUTPUT_PP);
    led_status_off();  /* Start in known state */
}

void led_status_on(void) {
#if LED_ACTIVE_LOW
    hal_gpio_write(LED_STATUS_PIN, GPIO_PIN_RESET);
#else
    hal_gpio_write(LED_STATUS_PIN, GPIO_PIN_SET);
#endif
}

void led_status_off(void) {
#if LED_ACTIVE_LOW
    hal_gpio_write(LED_STATUS_PIN, GPIO_PIN_SET);
#else
    hal_gpio_write(LED_STATUS_PIN, GPIO_PIN_RESET);
#endif
}

void led_error_init(void) {
    hal_gpio_init(LED_ERROR_PIN, GPIO_MODE_OUTPUT_PP);
    led_error_off();
}

/* Similar implementation for error LED... */
```

**配置分离优势分析：**
1. **一处修改**：改引脚/波特率只需修改 `board_config.h`
2. **项目复用**：同一套驱动代码，不同项目用不同配置文件
3. **编译时优化**：`FEATURE_ENABLE_DEBUG_LOG=0` 时，调试代码完全不编译
4. **文档化**：配置文件集中展示硬件分配，便于review

---

## 5. Opaque Pointer (Information Hiding)

```
+------------------------------------------------------------------+
|                         PUBLIC HEADER                             |
|                       (state_machine.h)                           |
+------------------------------------------------------------------+
|                                                                    |
|   typedef struct sm_context sm_context_t;  <-- Forward declaration |
|                                                                    |
|   sm_context_t* sm_create(void);                                   |
|   void sm_destroy(sm_context_t *ctx);                              |
|   void sm_process(sm_context_t *ctx, uint8_t event);               |
|   uint8_t sm_get_state(sm_context_t *ctx);                         |
|                                                                    |
+------------------------------------------------------------------+
                              |
                              | User only sees pointer
                              | Cannot access internals
                              v
+------------------------------------------------------------------+
|                      PRIVATE IMPLEMENTATION                       |
|                       (state_machine.c)                           |
+------------------------------------------------------------------+
|                                                                    |
|   struct sm_context {          <-- Full definition hidden here     |
|       uint8_t current_state;                                       |
|       uint8_t previous_state;                                      |
|       uint32_t state_enter_time;                                   |
|       uint8_t transition_count;                                    |
|       state_handler_t handlers[MAX_STATES];                        |
|       void *user_data;                                             |
|   };                                                               |
|                                                                    |
+------------------------------------------------------------------+
```

**不透明指针说明：**
- 头文件只声明结构体类型名（前向声明），不暴露成员
- 用户只能通过指针和公开 API 操作对象
- 内部实现可自由修改，不影响使用方

### Complete Code Example

```c
/*============================================================================
 * File: ring_buffer.h
 * Description: Opaque Ring Buffer - Public Interface
 *============================================================================*/
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Opaque type - users cannot see internal structure */
typedef struct ring_buffer ring_buffer_t;

/* Public API */
ring_buffer_t* ring_buffer_create(size_t capacity);
void ring_buffer_destroy(ring_buffer_t *rb);

bool ring_buffer_push(ring_buffer_t *rb, uint8_t data);
bool ring_buffer_pop(ring_buffer_t *rb, uint8_t *data);
bool ring_buffer_peek(ring_buffer_t *rb, uint8_t *data);

size_t ring_buffer_count(ring_buffer_t *rb);
size_t ring_buffer_capacity(ring_buffer_t *rb);
bool ring_buffer_is_empty(ring_buffer_t *rb);
bool ring_buffer_is_full(ring_buffer_t *rb);
void ring_buffer_clear(ring_buffer_t *rb);

#endif /* RING_BUFFER_H */


/*============================================================================
 * File: ring_buffer.c
 * Description: Ring Buffer - Private Implementation
 *============================================================================*/
#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>

/* Private structure definition - hidden from users */
struct ring_buffer {
    uint8_t *buffer;        /* User cannot access this */
    size_t capacity;        /* User cannot access this */
    size_t head;            /* User cannot access this */
    size_t tail;            /* User cannot access this */
    size_t count;           /* User cannot access this */
    
    /* We can add fields without breaking user code! */
    uint32_t push_count;    /* Statistics - added later */
    uint32_t pop_count;     /* Statistics - added later */
    uint32_t overflow_count;/* Statistics - added later */
};

ring_buffer_t* ring_buffer_create(size_t capacity) {
    ring_buffer_t *rb = (ring_buffer_t *)malloc(sizeof(ring_buffer_t));
    if (rb == NULL) {
        return NULL;
    }
    
    rb->buffer = (uint8_t *)malloc(capacity);
    if (rb->buffer == NULL) {
        free(rb);
        return NULL;
    }
    
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->push_count = 0;
    rb->pop_count = 0;
    rb->overflow_count = 0;
    
    return rb;
}

void ring_buffer_destroy(ring_buffer_t *rb) {
    if (rb != NULL) {
        if (rb->buffer != NULL) {
            free(rb->buffer);
        }
        free(rb);
    }
}

bool ring_buffer_push(ring_buffer_t *rb, uint8_t data) {
    if (rb == NULL || ring_buffer_is_full(rb)) {
        if (rb != NULL) {
            rb->overflow_count++;  /* Track overflows */
        }
        return false;
    }
    
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count++;
    rb->push_count++;
    
    return true;
}

bool ring_buffer_pop(ring_buffer_t *rb, uint8_t *data) {
    if (rb == NULL || data == NULL || ring_buffer_is_empty(rb)) {
        return false;
    }
    
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count--;
    rb->pop_count++;
    
    return true;
}

bool ring_buffer_peek(ring_buffer_t *rb, uint8_t *data) {
    if (rb == NULL || data == NULL || ring_buffer_is_empty(rb)) {
        return false;
    }
    
    *data = rb->buffer[rb->tail];
    return true;
}

size_t ring_buffer_count(ring_buffer_t *rb) {
    return (rb != NULL) ? rb->count : 0;
}

size_t ring_buffer_capacity(ring_buffer_t *rb) {
    return (rb != NULL) ? rb->capacity : 0;
}

bool ring_buffer_is_empty(ring_buffer_t *rb) {
    return (rb == NULL) || (rb->count == 0);
}

bool ring_buffer_is_full(ring_buffer_t *rb) {
    return (rb != NULL) && (rb->count >= rb->capacity);
}

void ring_buffer_clear(ring_buffer_t *rb) {
    if (rb != NULL) {
        rb->head = 0;
        rb->tail = 0;
        rb->count = 0;
    }
}


/*============================================================================
 * File: uart_receiver.c
 * Description: Using the opaque ring buffer
 *============================================================================*/
#include "ring_buffer.h"
#include "hal_uart.h"

static ring_buffer_t *uart_rx_buffer = NULL;

void uart_receiver_init(void) {
    /* User creates buffer without knowing internal structure */
    uart_rx_buffer = ring_buffer_create(256);
}

void uart_rx_isr(void) {
    uint8_t byte = hal_uart_read_byte();
    
    /* User cannot do: uart_rx_buffer->buffer[0] = byte; */
    /* Must use public API */
    ring_buffer_push(uart_rx_buffer, byte);
}

bool uart_get_byte(uint8_t *byte) {
    return ring_buffer_pop(uart_rx_buffer, byte);
}

size_t uart_bytes_available(void) {
    return ring_buffer_count(uart_rx_buffer);
}
```

**不透明指针优势分析：**
1. **封装性**：用户无法直接访问 `rb->buffer`，必须通过 API
2. **ABI 稳定**：添加 `overflow_count` 字段不影响已编译的用户代码
3. **防止误用**：不可能写出 `rb->count = -1` 这种错误代码
4. **易于调试**：所有修改都经过 API，便于添加断点/日志

---

## 6. Compile-Time Polymorphism (Macros)

```
+------------------------------------------------------------------+
|                      PLATFORM SELECTION                           |
|                       (Compile Time)                              |
+------------------------------------------------------------------+
                              |
         +--------------------+--------------------+
         |                    |                    |
         v                    v                    v
   PLATFORM_STM32       PLATFORM_NRF52       PLATFORM_LINUX
         |                    |                    |
         v                    v                    v
+----------------+    +----------------+    +----------------+
| CRITICAL_ENTER |    | CRITICAL_ENTER |    | CRITICAL_ENTER |
| __disable_irq()|    | sd_nvic_       |    | pthread_       |
|                |    | critical_      |    | mutex_lock()   |
| CRITICAL_EXIT  |    | region_enter() |    |                |
| __enable_irq() |    |                |    | CRITICAL_EXIT  |
+----------------+    +----------------+    | pthread_       |
                                           | mutex_unlock() |
                                           +----------------+

                    Same source code compiles to
                    different implementations!
```

**编译时多态说明：**
- 使用宏和条件编译实现平台相关代码切换
- 编译时确定具体实现，零运行时开销
- 相比函数指针，更适合资源受限的嵌入式系统

### Complete Code Example

```c
/*============================================================================
 * File: platform.h
 * Description: Platform abstraction using compile-time polymorphism
 *============================================================================*/
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

/*---------------------------------------------------------------------------
 * Platform Detection (can be set by build system)
 *---------------------------------------------------------------------------*/
#if defined(STM32F4)
    #define PLATFORM_STM32
#elif defined(NRF52840_XXAA)
    #define PLATFORM_NRF52
#elif defined(__linux__)
    #define PLATFORM_LINUX
#else
    #error "Unknown platform! Define STM32F4, NRF52840_XXAA, or __linux__"
#endif

/*---------------------------------------------------------------------------
 * Critical Section Macros
 *---------------------------------------------------------------------------*/
#if defined(PLATFORM_STM32)
    #include "stm32f4xx.h"
    #define CRITICAL_ENTER()    __disable_irq()
    #define CRITICAL_EXIT()     __enable_irq()

#elif defined(PLATFORM_NRF52)
    #include "nrf_nvic.h"
    static uint8_t __cr_nested = 0;
    #define CRITICAL_ENTER()    do { \
                                    uint8_t __CR_NESTED; \
                                    sd_nvic_critical_region_enter(&__CR_NESTED); \
                                    __cr_nested = __CR_NESTED; \
                                } while(0)
    #define CRITICAL_EXIT()     sd_nvic_critical_region_exit(__cr_nested)

#elif defined(PLATFORM_LINUX)
    #include <pthread.h>
    extern pthread_mutex_t critical_mutex;
    #define CRITICAL_ENTER()    pthread_mutex_lock(&critical_mutex)
    #define CRITICAL_EXIT()     pthread_mutex_unlock(&critical_mutex)
#endif

/*---------------------------------------------------------------------------
 * Memory Barrier Macros
 *---------------------------------------------------------------------------*/
#if defined(PLATFORM_STM32) || defined(PLATFORM_NRF52)
    #define MEMORY_BARRIER()    __DMB()
#elif defined(PLATFORM_LINUX)
    #define MEMORY_BARRIER()    __sync_synchronize()
#endif

/*---------------------------------------------------------------------------
 * Delay Functions
 *---------------------------------------------------------------------------*/
#if defined(PLATFORM_STM32)
    #define DELAY_MS(ms)        HAL_Delay(ms)
    #define DELAY_US(us)        do { \
                                    uint32_t start = DWT->CYCCNT; \
                                    uint32_t cycles = (SystemCoreClock / 1000000) * (us); \
                                    while ((DWT->CYCCNT - start) < cycles); \
                                } while(0)

#elif defined(PLATFORM_NRF52)
    #include "nrf_delay.h"
    #define DELAY_MS(ms)        nrf_delay_ms(ms)
    #define DELAY_US(us)        nrf_delay_us(us)

#elif defined(PLATFORM_LINUX)
    #include <unistd.h>
    #define DELAY_MS(ms)        usleep((ms) * 1000)
    #define DELAY_US(us)        usleep(us)
#endif

/*---------------------------------------------------------------------------
 * Debug Output
 *---------------------------------------------------------------------------*/
#if defined(PLATFORM_STM32) || defined(PLATFORM_NRF52)
    /* Implemented in platform-specific file */
    void platform_debug_print(const char *str);
    #define DEBUG_PRINT(str)    platform_debug_print(str)

#elif defined(PLATFORM_LINUX)
    #include <stdio.h>
    #define DEBUG_PRINT(str)    printf("%s", str)
#endif

/*---------------------------------------------------------------------------
 * Atomic Operations
 *---------------------------------------------------------------------------*/
#if defined(PLATFORM_STM32)
    #define ATOMIC_LOAD(ptr)        (*(volatile typeof(*(ptr)) *)(ptr))
    #define ATOMIC_STORE(ptr, val)  (*(volatile typeof(*(ptr)) *)(ptr) = (val))
    #define ATOMIC_INC(ptr)         do { CRITICAL_ENTER(); (*(ptr))++; CRITICAL_EXIT(); } while(0)
    #define ATOMIC_DEC(ptr)         do { CRITICAL_ENTER(); (*(ptr))--; CRITICAL_EXIT(); } while(0)

#elif defined(PLATFORM_NRF52)
    #include "nrf_atomic.h"
    #define ATOMIC_LOAD(ptr)        nrf_atomic_u32_fetch_add((nrf_atomic_u32_t *)(ptr), 0)
    #define ATOMIC_STORE(ptr, val)  nrf_atomic_u32_store((nrf_atomic_u32_t *)(ptr), val)
    #define ATOMIC_INC(ptr)         nrf_atomic_u32_add((nrf_atomic_u32_t *)(ptr), 1)
    #define ATOMIC_DEC(ptr)         nrf_atomic_u32_sub((nrf_atomic_u32_t *)(ptr), 1)

#elif defined(PLATFORM_LINUX)
    #define ATOMIC_LOAD(ptr)        __atomic_load_n(ptr, __ATOMIC_SEQ_CST)
    #define ATOMIC_STORE(ptr, val)  __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST)
    #define ATOMIC_INC(ptr)         __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
    #define ATOMIC_DEC(ptr)         __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#endif

#endif /* PLATFORM_H */


/*============================================================================
 * File: thread_safe_counter.c
 * Description: Platform-independent code using polymorphic macros
 *============================================================================*/
#include "platform.h"

typedef struct {
    volatile uint32_t value;
    volatile uint32_t max_value;
} safe_counter_t;

void counter_init(safe_counter_t *counter, uint32_t initial, uint32_t max) {
    ATOMIC_STORE(&counter->value, initial);
    ATOMIC_STORE(&counter->max_value, max);
}

bool counter_increment(safe_counter_t *counter) {
    bool success = false;
    
    CRITICAL_ENTER();
    {
        uint32_t current = counter->value;
        if (current < counter->max_value) {
            counter->value = current + 1;
            success = true;
        }
    }
    CRITICAL_EXIT();
    
    return success;
}

uint32_t counter_get(safe_counter_t *counter) {
    return ATOMIC_LOAD(&counter->value);
}


/*============================================================================
 * File: sensor_reader.c
 * Description: Using platform macros for timing
 *============================================================================*/
#include "platform.h"
#include "hal_i2c.h"

#define SENSOR_ADDR     0x48
#define CONVERSION_TIME_US  100

uint16_t read_temperature_sensor(void) {
    uint8_t cmd = 0x01;
    uint8_t data[2];
    
    /* Start conversion */
    hal_i2c_write(SENSOR_ADDR, &cmd, 1);
    
    /* Wait for conversion - platform-independent delay */
    DELAY_US(CONVERSION_TIME_US);
    
    /* Read result */
    hal_i2c_read(SENSOR_ADDR, data, 2);
    
    return (data[0] << 8) | data[1];
}
```

**编译时多态优势分析：**
1. **零运行时开销**：宏在编译时展开，无函数调用开销
2. **代码统一**：业务代码不变，编译时自动选择正确实现
3. **易于测试**：`PLATFORM_LINUX` 允许在 PC 上运行单元测试
4. **类型安全**：相比函数指针，编译器可以做更多检查

---

## 7. Modular Directory Structure

```
embedded_project/
|
+-- app/                          <-- Application specific
|   +-- main.c
|   +-- app_config.h
|   +-- state_machine.c
|
+-- components/                   <-- Reusable modules
|   |
|   +-- button/
|   |   +-- button.h
|   |   +-- button.c
|   |   +-- README.md
|   |
|   +-- led/
|   |   +-- led.h
|   |   +-- led.c
|   |
|   +-- ring_buffer/
|   |   +-- ring_buffer.h
|   |   +-- ring_buffer.c
|   |
|   +-- protocol/
|   |   +-- protocol_parser.h
|   |   +-- protocol_parser.c
|   |
|   +-- scheduler/
|       +-- task_scheduler.h
|       +-- task_scheduler.c
|
+-- drivers/                      <-- Hardware interface
|   |
|   +-- hal/                      <-- Abstract interface
|   |   +-- hal_gpio.h
|   |   +-- hal_uart.h
|   |   +-- hal_spi.h
|   |   +-- hal_i2c.h
|   |   +-- hal_timer.h
|   |
|   +-- stm32f4/                  <-- STM32 implementation
|   |   +-- hal_gpio_stm32.c
|   |   +-- hal_uart_stm32.c
|   |   +-- hal_spi_stm32.c
|   |   +-- startup_stm32f4.s
|   |   +-- system_stm32f4.c
|   |
|   +-- nrf52/                    <-- NRF52 implementation
|   |   +-- hal_gpio_nrf52.c
|   |   +-- hal_uart_nrf52.c
|   |   +-- hal_spi_nrf52.c
|   |
|   +-- mock/                     <-- Mock for unit testing
|       +-- hal_gpio_mock.c
|       +-- hal_uart_mock.c
|
+-- config/                       <-- Build configurations
|   +-- stm32f4_board_a/
|   |   +-- board_config.h
|   |   +-- linker_script.ld
|   |
|   +-- stm32f4_board_b/
|   |   +-- board_config.h
|   |   +-- linker_script.ld
|   |
|   +-- nrf52_board/
|       +-- board_config.h
|       +-- linker_script.ld
|
+-- lib/                          <-- Third-party libraries
|   +-- cmsis/
|   +-- freertos/
|   +-- printf/
|
+-- test/                         <-- Unit tests
|   +-- test_ring_buffer.c
|   +-- test_protocol_parser.c
|   +-- test_button.c
|
+-- docs/                         <-- Documentation
|   +-- architecture.md
|   +-- coding_standard.md
|
+-- tools/                        <-- Build tools and scripts
|   +-- Makefile
|   +-- CMakeLists.txt
|   +-- flash.sh
|
+-- README.md
```

**模块化目录结构说明：**
- `components/` 可直接复制到其他项目使用
- `drivers/hal/` 定义接口，`drivers/stm32f4/` 等提供实现
- `config/` 按硬件板型组织配置文件
- `test/` 配合 `drivers/mock/` 可在 PC 上运行单元测试

---

## 8. Weak Symbol Override

```
+------------------------------------------------------------------+
|                    FRAMEWORK / LIBRARY                            |
+------------------------------------------------------------------+
|                                                                    |
|   __attribute__((weak))                                            |
|   void app_error_handler(uint32_t error_code) {                    |
|       /* Default implementation: infinite loop */                  |
|       while(1) { }                                                 |
|   }                                                                |
|                                                                    |
|   void framework_critical_error(uint32_t code) {                   |
|       log_error(code);                                             |
|       app_error_handler(code);   <-- Calls weak or strong          |
|   }                                                                |
|                                                                    |
+------------------------------------------------------------------+
                              |
                              | User can override
                              v
+------------------------------------------------------------------+
|                    USER APPLICATION                               |
+------------------------------------------------------------------+
|                                                                    |
|   /* Strong symbol overrides weak symbol */                        |
|   void app_error_handler(uint32_t error_code) {                    |
|       save_crash_dump(error_code);                                 |
|       send_error_report();                                         |
|       system_reset();                                              |
|   }                                                                |
|                                                                    |
+------------------------------------------------------------------+
```

**弱符号说明：**
- 框架/库提供默认实现（weak）
- 用户可选择性覆盖（strong symbol 自动替换 weak symbol）
- 不覆盖则使用默认行为，覆盖则使用自定义行为

### Complete Code Example

```c
/*============================================================================
 * File: framework.h
 * Description: Framework public interface with overridable hooks
 *============================================================================*/
#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <stdint.h>

/* Error codes */
typedef enum {
    ERR_NONE = 0,
    ERR_MEMORY,
    ERR_HARDWARE,
    ERR_TIMEOUT,
    ERR_INVALID_PARAM,
    ERR_STACK_OVERFLOW
} error_code_t;

/* Framework initialization */
void framework_init(void);
void framework_run(void);

/* User-overridable hooks (weak symbols) */
void app_init_hook(void);
void app_idle_hook(void);
void app_error_handler(error_code_t error);
void app_stack_overflow_handler(const char *task_name);
void app_malloc_failed_handler(size_t requested_size);

#endif /* FRAMEWORK_H */


/*============================================================================
 * File: framework.c
 * Description: Framework implementation with default weak handlers
 *============================================================================*/
#include "framework.h"
#include "hal_gpio.h"
#include <stdlib.h>

/*---------------------------------------------------------------------------
 * Weak symbol definitions - User can override these
 *---------------------------------------------------------------------------*/

__attribute__((weak))
void app_init_hook(void) {
    /* Default: Do nothing */
    /* User can override to add custom initialization */
}

__attribute__((weak))
void app_idle_hook(void) {
    /* Default: Enter low power mode */
    __WFI();  /* Wait for interrupt */
}

__attribute__((weak))
void app_error_handler(error_code_t error) {
    /* Default: Blink LED and hang */
    (void)error;
    while (1) {
        hal_gpio_toggle(LED_ERROR_PIN);
        for (volatile int i = 0; i < 100000; i++);
    }
}

__attribute__((weak))
void app_stack_overflow_handler(const char *task_name) {
    /* Default: Just call error handler */
    (void)task_name;
    app_error_handler(ERR_STACK_OVERFLOW);
}

__attribute__((weak))
void app_malloc_failed_handler(size_t requested_size) {
    /* Default: Just call error handler */
    (void)requested_size;
    app_error_handler(ERR_MEMORY);
}

/*---------------------------------------------------------------------------
 * Framework core functions
 *---------------------------------------------------------------------------*/

static volatile uint8_t error_occurred = 0;
static volatile error_code_t last_error = ERR_NONE;

void framework_init(void) {
    /* Initialize hardware */
    hal_gpio_init(LED_STATUS_PIN, GPIO_MODE_OUTPUT_PP);
    hal_gpio_init(LED_ERROR_PIN, GPIO_MODE_OUTPUT_PP);
    
    /* Call user hook */
    app_init_hook();
}

void framework_run(void) {
    while (1) {
        if (error_occurred) {
            app_error_handler(last_error);
        }
        
        /* Do main work here... */
        
        /* No work to do, call idle hook */
        app_idle_hook();
    }
}

void framework_report_error(error_code_t error) {
    last_error = error;
    error_occurred = 1;
}

/* Called by memory allocator on failure */
void *framework_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL && size > 0) {
        app_malloc_failed_handler(size);
    }
    return ptr;
}


/*============================================================================
 * File: main.c (User Application - Overriding weak symbols)
 * Description: Application that overrides default handlers
 *============================================================================*/
#include "framework.h"
#include "uart_driver.h"
#include "flash_storage.h"
#include "watchdog.h"

/* Application state */
static uint32_t error_count = 0;

/*---------------------------------------------------------------------------
 * Override weak symbols with strong (custom) implementations
 *---------------------------------------------------------------------------*/

/* Override: Custom initialization */
void app_init_hook(void) {
    uart_debug_init();
    flash_storage_init();
    watchdog_init(5000);  /* 5 second timeout */
    
    uart_debug_print("Application started!\r\n");
}

/* Override: Custom idle behavior */
void app_idle_hook(void) {
    watchdog_feed();  /* Feed watchdog in idle */
    
    /* Enter sleep mode but wake on UART */
    hal_uart_enable_rx_interrupt();
    __WFI();
}

/* Override: Custom error handler */
void app_error_handler(error_code_t error) {
    /* Disable interrupts */
    __disable_irq();
    
    /* Save error info to flash for post-mortem analysis */
    error_info_t info = {
        .code = error,
        .count = ++error_count,
        .timestamp = hal_tick_get_ms(),
        .stack_pointer = __get_MSP()
    };
    flash_storage_save_error(&info);
    
    /* Send error report if UART available */
    char msg[64];
    snprintf(msg, sizeof(msg), "FATAL ERROR: %d\r\n", error);
    uart_debug_print(msg);
    
    /* Wait a bit for UART to finish */
    for (volatile int i = 0; i < 1000000; i++);
    
    /* Reset system */
    NVIC_SystemReset();
}

/* Override: Custom stack overflow handler */
void app_stack_overflow_handler(const char *task_name) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Stack overflow in: %s\r\n", task_name);
    uart_debug_print(msg);
    
    app_error_handler(ERR_STACK_OVERFLOW);
}

/* Override: Custom malloc failure handler */
void app_malloc_failed_handler(size_t requested_size) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Malloc failed: %u bytes\r\n", requested_size);
    uart_debug_print(msg);
    
    /* Try to free some memory */
    cache_flush_all();
    
    /* Still call error handler */
    app_error_handler(ERR_MEMORY);
}

/*---------------------------------------------------------------------------
 * Main function
 *---------------------------------------------------------------------------*/
int main(void) {
    framework_init();  /* Will call our app_init_hook() */
    framework_run();   /* Will call our app_idle_hook() and app_error_handler() */
    return 0;
}
```

**弱符号优势分析：**
1. **可选覆盖**：用户只覆盖需要自定义的函数，其他使用默认实现
2. **框架友好**：框架提供扩展点，用户无需修改框架代码
3. **渐进式开发**：先用默认实现快速启动，后续再逐步定制
4. **链接时决定**：编译器自动选择 strong symbol，无运行时开销

---

## Summary: Reusability Principles

```
+------------------------------------------------------------------+
|                  SOFTWARE REUSABILITY PRINCIPLES                  |
+------------------------------------------------------------------+

+------------------+    +------------------+    +------------------+
|  HIGH COHESION   |    |  LOW COUPLING    |    |    ABSTRACTION   |
|                  |    |                  |    |                  |
|  One module,     |    |  Communicate     |    |  Hide details    |
|  one purpose     |    |  via interfaces  |    |  behind APIs     |
+------------------+    +------------------+    +------------------+

+------------------+    +------------------+    +------------------+
| SEPARATION OF    |    |  DEPENDENCY      |    |  OPEN-CLOSED     |
| CONCERNS         |    |  INVERSION       |    |  PRINCIPLE       |
|                  |    |                  |    |                  |
| Config vs Code   |    | Depend on        |    | Open to extend   |
| Interface vs     |    | abstractions,    |    | Closed to modify |
| Implementation   |    | not concretions  |    | (use callbacks)  |
+------------------+    +------------------+    +------------------+

+------------------------------------------------------------------+
|                     IMPLEMENTATION TECHNIQUES                     |
+------------------------------------------------------------------+
|                                                                    |
|  1. Layered Architecture    - Organize code in layers             |
|  2. HAL (Hardware Abstract) - Isolate hardware dependencies       |
|  3. Callback Functions      - Inject behavior at runtime          |
|  4. Configuration Files     - Externalize variable parameters     |
|  5. Opaque Pointers         - Hide implementation details         |
|  6. Compile-time Macros     - Zero-overhead platform abstraction  |
|  7. Modular Structure       - Organize files for reuse            |
|  8. Weak Symbols            - Provide overridable defaults        |
|                                                                    |
+------------------------------------------------------------------+
```

**总结说明：**

| 原则 | 实现技巧 | 核心收益 |
|------|----------|----------|
| 高内聚 | 分层架构、模块化目录 | 模块职责单一，易于理解和测试 |
| 低耦合 | HAL、不透明指针 | 修改一处不影响其他模块 |
| 抽象 | 函数指针、接口头文件 | 隐藏实现细节，只暴露必要接口 |
| 配置分离 | 配置头文件、条件编译 | 同一代码适配不同硬件/需求 |
| 依赖倒置 | HAL 接口 + 平台实现 | 上层不依赖下层具体实现 |
| 开闭原则 | 回调函数、弱符号 | 扩展功能无需修改现有代码 |

**应用这些技巧后，你的嵌入式 C 代码可以：**
- 跨项目复用（不同产品）
- 跨平台移植（不同 MCU）
- 易于单元测试（PC 端模拟）
- 团队协作开发（清晰边界）

