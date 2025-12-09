# 工厂模式 (Factory Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                       FACTORY PATTERN                             |
+------------------------------------------------------------------+

    WITHOUT FACTORY (Direct Creation):
    
    +------------------+         +------------------+
    |    Application   |  new    |  ConcreteClassA  |
    |                  |-------->|                  |
    |  if (type == A)  |         +------------------+
    |    new ClassA()  |         +------------------+
    |  else if (B)     |  new    |  ConcreteClassB  |
    |    new ClassB()  |-------->|                  |
    |                  |         +------------------+
    +------------------+
    
    Problem: Application knows all concrete classes!


    WITH FACTORY (Centralized Creation):
    
    +------------------+                    +------------------+
    |    Application   |    create(type)    |     Factory      |
    |                  |------------------->|                  |
    |  Only knows      |                    | if (type == A)   |
    |  abstract type   |                    |   return new A   |
    |                  |<-------------------|                  |
    +------------------+   returns Product  +--------+---------+
                                                     |
                               +---------------------+---------------------+
                               |                     |                     |
                               v                     v                     v
                    +------------------+  +------------------+  +------------------+
                    |    ProductA      |  |    ProductB      |  |    ProductC      |
                    +------------------+  +------------------+  +------------------+


    PRODUCT INTERFACE (Function Pointer Table):
    
    +------------------------------------------+
    |           product_ops_t                   |
    |  +------------------------------------+  |
    |  | init()    - Initialize product    |  |
    |  | process() - Do main work          |  |
    |  | destroy() - Clean up              |  |
    |  +------------------------------------+  |
    +------------------------------------------+
             ^              ^              ^
             |              |              |
    +--------+----+  +------+------+  +----+--------+
    | uart_ops    |  | spi_ops     |  | i2c_ops     |
    +-------------+  +-------------+  +-------------+
```

**核心思想说明：**
- 将对象创建逻辑集中到工厂函数中
- 应用层只依赖抽象接口，不关心具体实现
- 通过参数或类型标识决定创建哪种具体对象
- 新增产品类型时只需修改工厂，不影响应用层

## 实现思路

1. **定义抽象接口**：使用函数指针结构体定义产品接口
2. **实现具体产品**：各产品实现接口中的函数
3. **工厂函数**：根据类型创建对应产品并返回
4. **应用层**：只通过接口操作产品

## 典型应用场景

- 通信接口创建（UART/SPI/I2C）
- 存储驱动创建（Flash/EEPROM/SD卡）
- 协议解析器创建（JSON/XML/Binary）
- 图形元素创建（按钮/文本框/列表）

## 完整代码示例

```c
/*============================================================================
 * 工厂模式示例 - 通信接口工厂
 *============================================================================*/

/*---------------------------------------------------------------------------
 * comm_interface.h - 抽象接口定义
 *---------------------------------------------------------------------------*/
#ifndef COMM_INTERFACE_H
#define COMM_INTERFACE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 通信接口类型 */
typedef enum {
    COMM_TYPE_UART,
    COMM_TYPE_SPI,
    COMM_TYPE_I2C,
    COMM_TYPE_MAX
} comm_type_t;

/* 通信状态 */
typedef enum {
    COMM_OK = 0,
    COMM_ERROR,
    COMM_BUSY,
    COMM_TIMEOUT
} comm_status_t;

/* 关键点：抽象接口定义（函数指针表） */
typedef struct comm_interface comm_interface_t;

typedef struct {
    comm_status_t (*init)(comm_interface_t *comm, void *config);
    comm_status_t (*deinit)(comm_interface_t *comm);
    comm_status_t (*send)(comm_interface_t *comm, const uint8_t *data, size_t len);
    comm_status_t (*receive)(comm_interface_t *comm, uint8_t *data, size_t len, uint32_t timeout);
    bool (*is_ready)(comm_interface_t *comm);
} comm_ops_t;

/* 通信接口结构体 */
struct comm_interface {
    comm_type_t type;
    const comm_ops_t *ops;  /* 关键点：指向具体实现的操作表 */
    void *private_data;     /* 私有数据 */
    bool initialized;
};

/* 关键点：工厂函数声明 */
comm_interface_t* comm_factory_create(comm_type_t type, void *config);
void comm_factory_destroy(comm_interface_t *comm);

/* 便捷操作宏 */
#define COMM_INIT(comm, cfg)        ((comm)->ops->init((comm), (cfg)))
#define COMM_SEND(comm, data, len)  ((comm)->ops->send((comm), (data), (len)))
#define COMM_RECV(comm, data, len, timeout) \
    ((comm)->ops->receive((comm), (data), (len), (timeout)))

#endif /* COMM_INTERFACE_H */


/*---------------------------------------------------------------------------
 * comm_uart.c - UART 具体实现
 *---------------------------------------------------------------------------*/
#include "comm_interface.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* UART 私有数据 */
typedef struct {
    uint8_t port;
    uint32_t baudrate;
    uint8_t rx_buffer[256];
    uint16_t rx_head;
    uint16_t rx_tail;
} uart_private_t;

/* UART 配置 */
typedef struct {
    uint8_t port;
    uint32_t baudrate;
} uart_config_t;

static comm_status_t uart_init(comm_interface_t *comm, void *config) {
    uart_config_t *cfg = (uart_config_t *)config;
    uart_private_t *priv = (uart_private_t *)comm->private_data;
    
    priv->port = cfg->port;
    priv->baudrate = cfg->baudrate;
    priv->rx_head = priv->rx_tail = 0;
    
    /* 模拟硬件初始化 */
    printf("[UART] Initialized port %d @ %u baud\n", priv->port, priv->baudrate);
    
    comm->initialized = true;
    return COMM_OK;
}

static comm_status_t uart_deinit(comm_interface_t *comm) {
    uart_private_t *priv = (uart_private_t *)comm->private_data;
    printf("[UART] Deinitialized port %d\n", priv->port);
    comm->initialized = false;
    return COMM_OK;
}

static comm_status_t uart_send(comm_interface_t *comm, const uint8_t *data, size_t len) {
    uart_private_t *priv = (uart_private_t *)comm->private_data;
    
    if (!comm->initialized) return COMM_ERROR;
    
    printf("[UART%d] TX (%zu bytes): ", priv->port, len);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 16) printf("...");
    printf("\n");
    
    return COMM_OK;
}

static comm_status_t uart_receive(comm_interface_t *comm, uint8_t *data, 
                                   size_t len, uint32_t timeout) {
    uart_private_t *priv = (uart_private_t *)comm->private_data;
    
    if (!comm->initialized) return COMM_ERROR;
    
    printf("[UART%d] RX request: %zu bytes, timeout=%ums\n", 
           priv->port, len, timeout);
    
    /* 模拟接收数据 */
    memset(data, 0xAA, len);
    return COMM_OK;
}

static bool uart_is_ready(comm_interface_t *comm) {
    return comm->initialized;
}

/* 关键点：UART 操作表 */
static const comm_ops_t uart_ops = {
    .init = uart_init,
    .deinit = uart_deinit,
    .send = uart_send,
    .receive = uart_receive,
    .is_ready = uart_is_ready
};

/* UART 创建函数（工厂内部使用） */
comm_interface_t* uart_create(void) {
    comm_interface_t *comm = malloc(sizeof(comm_interface_t));
    if (comm == NULL) return NULL;
    
    uart_private_t *priv = malloc(sizeof(uart_private_t));
    if (priv == NULL) {
        free(comm);
        return NULL;
    }
    
    memset(priv, 0, sizeof(uart_private_t));
    
    comm->type = COMM_TYPE_UART;
    comm->ops = &uart_ops;  /* 关键点：绑定操作表 */
    comm->private_data = priv;
    comm->initialized = false;
    
    return comm;
}


/*---------------------------------------------------------------------------
 * comm_spi.c - SPI 具体实现
 *---------------------------------------------------------------------------*/
/* SPI 私有数据 */
typedef struct {
    uint8_t port;
    uint32_t speed_hz;
    uint8_t mode;
} spi_private_t;

typedef struct {
    uint8_t port;
    uint32_t speed_hz;
    uint8_t mode;
} spi_config_t;

static comm_status_t spi_init(comm_interface_t *comm, void *config) {
    spi_config_t *cfg = (spi_config_t *)config;
    spi_private_t *priv = (spi_private_t *)comm->private_data;
    
    priv->port = cfg->port;
    priv->speed_hz = cfg->speed_hz;
    priv->mode = cfg->mode;
    
    printf("[SPI] Initialized port %d @ %u Hz, mode %d\n", 
           priv->port, priv->speed_hz, priv->mode);
    
    comm->initialized = true;
    return COMM_OK;
}

static comm_status_t spi_deinit(comm_interface_t *comm) {
    spi_private_t *priv = (spi_private_t *)comm->private_data;
    printf("[SPI] Deinitialized port %d\n", priv->port);
    comm->initialized = false;
    return COMM_OK;
}

static comm_status_t spi_send(comm_interface_t *comm, const uint8_t *data, size_t len) {
    spi_private_t *priv = (spi_private_t *)comm->private_data;
    
    if (!comm->initialized) return COMM_ERROR;
    
    printf("[SPI%d] TX (%zu bytes): ", priv->port, len);
    for (size_t i = 0; i < len && i < 16; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    return COMM_OK;
}

static comm_status_t spi_receive(comm_interface_t *comm, uint8_t *data, 
                                  size_t len, uint32_t timeout) {
    spi_private_t *priv = (spi_private_t *)comm->private_data;
    (void)timeout;
    
    if (!comm->initialized) return COMM_ERROR;
    
    printf("[SPI%d] RX (%zu bytes)\n", priv->port, len);
    memset(data, 0xBB, len);
    return COMM_OK;
}

static bool spi_is_ready(comm_interface_t *comm) {
    return comm->initialized;
}

static const comm_ops_t spi_ops = {
    .init = spi_init,
    .deinit = spi_deinit,
    .send = spi_send,
    .receive = spi_receive,
    .is_ready = spi_is_ready
};

comm_interface_t* spi_create(void) {
    comm_interface_t *comm = malloc(sizeof(comm_interface_t));
    if (comm == NULL) return NULL;
    
    spi_private_t *priv = malloc(sizeof(spi_private_t));
    if (priv == NULL) {
        free(comm);
        return NULL;
    }
    
    memset(priv, 0, sizeof(spi_private_t));
    
    comm->type = COMM_TYPE_SPI;
    comm->ops = &spi_ops;
    comm->private_data = priv;
    comm->initialized = false;
    
    return comm;
}


/*---------------------------------------------------------------------------
 * comm_factory.c - 工厂实现
 *---------------------------------------------------------------------------*/
#include "comm_interface.h"

/* 外部声明具体创建函数 */
extern comm_interface_t* uart_create(void);
extern comm_interface_t* spi_create(void);

/* 关键点：工厂函数 - 根据类型创建对应产品 */
comm_interface_t* comm_factory_create(comm_type_t type, void *config) {
    comm_interface_t *comm = NULL;
    
    /* 根据类型调用对应的创建函数 */
    switch (type) {
        case COMM_TYPE_UART:
            comm = uart_create();
            break;
            
        case COMM_TYPE_SPI:
            comm = spi_create();
            break;
            
        case COMM_TYPE_I2C:
            /* TODO: i2c_create() */
            printf("[Factory] I2C not implemented\n");
            return NULL;
            
        default:
            printf("[Factory] Unknown type: %d\n", type);
            return NULL;
    }
    
    /* 自动初始化 */
    if (comm != NULL && config != NULL) {
        if (comm->ops->init(comm, config) != COMM_OK) {
            comm_factory_destroy(comm);
            return NULL;
        }
    }
    
    return comm;
}

/* 销毁接口 */
void comm_factory_destroy(comm_interface_t *comm) {
    if (comm == NULL) return;
    
    if (comm->initialized && comm->ops->deinit != NULL) {
        comm->ops->deinit(comm);
    }
    
    if (comm->private_data != NULL) {
        free(comm->private_data);
    }
    
    free(comm);
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "comm_interface.h"
#include <stdio.h>

/* 传感器读取函数 - 只依赖抽象接口 */
void read_sensor(comm_interface_t *comm) {
    uint8_t cmd[] = {0x01, 0x02, 0x03};
    uint8_t response[8];
    
    /* 关键点：通过抽象接口操作，不关心具体是 UART 还是 SPI */
    if (COMM_SEND(comm, cmd, sizeof(cmd)) == COMM_OK) {
        if (COMM_RECV(comm, response, sizeof(response), 1000) == COMM_OK) {
            printf("Sensor response received\n");
        }
    }
}

int main(void) {
    printf("=== Factory Pattern Demo ===\n\n");
    
    /* 配置 */
    uart_config_t uart_cfg = {.port = 1, .baudrate = 115200};
    spi_config_t spi_cfg = {.port = 0, .speed_hz = 1000000, .mode = 0};
    
    /* 关键点：通过工厂创建不同类型的通信接口 */
    printf("Creating UART interface...\n");
    comm_interface_t *uart = comm_factory_create(COMM_TYPE_UART, &uart_cfg);
    
    printf("\nCreating SPI interface...\n");
    comm_interface_t *spi = comm_factory_create(COMM_TYPE_SPI, &spi_cfg);
    
    if (uart == NULL || spi == NULL) {
        printf("Failed to create interfaces!\n");
        return -1;
    }
    
    /* 关键点：同样的函数，不同的接口实现 */
    printf("\n--- Reading sensor via UART ---\n");
    read_sensor(uart);
    
    printf("\n--- Reading sensor via SPI ---\n");
    read_sensor(spi);
    
    /* 清理 */
    printf("\nCleaning up...\n");
    comm_factory_destroy(uart);
    comm_factory_destroy(spi);
    
    printf("\nDone!\n");
    return 0;
}
```

## 运行输出示例

```
=== Factory Pattern Demo ===

Creating UART interface...
[UART] Initialized port 1 @ 115200 baud

Creating SPI interface...
[SPI] Initialized port 0 @ 1000000 Hz, mode 0

--- Reading sensor via UART ---
[UART1] TX (3 bytes): 01 02 03 
[UART1] RX request: 8 bytes, timeout=1000ms
Sensor response received

--- Reading sensor via SPI ---
[SPI0] TX (3 bytes): 01 02 03 
[SPI0] RX (8 bytes)
Sensor response received

Cleaning up...
[UART] Deinitialized port 1
[SPI] Deinitialized port 0

Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **解耦** | 应用层不依赖具体实现类 |
| **易扩展** | 新增产品只需添加实现和工厂分支 |
| **统一接口** | 所有产品使用相同的操作方式 |
| **集中管理** | 创建逻辑集中在工厂中 |
| **运行时选择** | 可根据配置动态选择产品类型 |

