# C语言软件分层架构 - 层间交互方法详解 (续)

## 方法四：接口/抽象层 (Interface Abstraction)

```
+------------------------------------------------------------------+
|                   INTERFACE ABSTRACTION PATTERN                   |
+------------------------------------------------------------------+

    +------------------+
    |   Application    |
    |                  |
    | Uses interface   |
    | only, not impl   |
    +--------+---------+
             |
             | depends on abstraction
             v
    +--------------------------------------------------+
    |              ABSTRACT INTERFACE                   |
    |                 (storage.h)                       |
    |  +--------------------------------------------+  |
    |  | storage_init()                             |  |
    |  | storage_read()                             |  |
    |  | storage_write()                            |  |
    |  | storage_erase()                            |  |
    |  +--------------------------------------------+  |
    +--------------------------------------------------+
             ^                          ^
             |                          |
    +--------+--------+        +--------+--------+
    |  Flash Driver   |        |  EEPROM Driver  |
    | (storage_flash.c)|        |(storage_eeprom.c)|
    |  implements      |        |  implements      |
    |  interface       |        |  interface       |
    +-----------------+        +-----------------+


    COMPILE-TIME SELECTION:
    
    Makefile:
    ifeq ($(STORAGE_TYPE), FLASH)
        SRC += storage_flash.c
    else
        SRC += storage_eeprom.c
    endif
```

**接口抽象说明：**
- 定义抽象接口（头文件），声明操作契约
- 多个实现各自完成接口
- 上层只依赖接口，不关心具体实现
- 编译时或运行时选择具体实现

### 完整代码示例

```c
/*============================================================================
 * 接口抽象模式示例 - 存储抽象层
 *============================================================================*/

/*---------------------------------------------------------------------------
 * Abstract Interface (抽象接口层) - storage.h
 *---------------------------------------------------------------------------*/
#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 存储操作状态 */
typedef enum {
    STORAGE_OK = 0,
    STORAGE_ERROR,
    STORAGE_BUSY,
    STORAGE_INVALID_ADDR,
    STORAGE_WRITE_PROTECTED
} storage_status_t;

/* 存储信息结构 */
typedef struct {
    uint32_t total_size;
    uint32_t page_size;
    uint32_t sector_size;
    uint32_t write_unit;
} storage_info_t;

/*---------------------------------------------------------------------------
 * 关键点：抽象接口定义 - 所有实现必须提供这些函数
 *---------------------------------------------------------------------------*/
storage_status_t storage_init(void);
storage_status_t storage_deinit(void);
storage_status_t storage_read(uint32_t addr, void *data, size_t len);
storage_status_t storage_write(uint32_t addr, const void *data, size_t len);
storage_status_t storage_erase_sector(uint32_t sector_addr);
storage_status_t storage_erase_chip(void);
storage_info_t   storage_get_info(void);
bool             storage_is_busy(void);

#endif /* STORAGE_H */


/*---------------------------------------------------------------------------
 * Implementation 1: Internal Flash (内部 Flash 实现)
 *---------------------------------------------------------------------------*/
/* storage_internal_flash.c */
#include "storage.h"
#include "stm32f4xx_hal.h"

#define FLASH_START_ADDR    0x08060000  /* Sector 7 */
#define FLASH_SIZE          (128 * 1024)
#define FLASH_SECTOR_SIZE   (128 * 1024)
#define FLASH_PAGE_SIZE     256

static bool initialized = false;

storage_status_t storage_init(void) {
    /* Internal flash 通常无需特殊初始化 */
    initialized = true;
    return STORAGE_OK;
}

storage_status_t storage_deinit(void) {
    initialized = false;
    return STORAGE_OK;
}

storage_status_t storage_read(uint32_t addr, void *data, size_t len) {
    if (!initialized) return STORAGE_ERROR;
    if (addr + len > FLASH_SIZE) return STORAGE_INVALID_ADDR;
    
    /* 关键点：内部 Flash 可直接内存映射读取 */
    memcpy(data, (void *)(FLASH_START_ADDR + addr), len);
    return STORAGE_OK;
}

storage_status_t storage_write(uint32_t addr, const void *data, size_t len) {
    if (!initialized) return STORAGE_ERROR;
    if (addr + len > FLASH_SIZE) return STORAGE_INVALID_ADDR;
    
    HAL_FLASH_Unlock();
    
    const uint8_t *src = (const uint8_t *)data;
    uint32_t dest = FLASH_START_ADDR + addr;
    
    for (size_t i = 0; i < len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, dest + i, src[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return STORAGE_ERROR;
        }
    }
    
    HAL_FLASH_Lock();
    return STORAGE_OK;
}

storage_status_t storage_erase_sector(uint32_t sector_addr) {
    FLASH_EraseInitTypeDef erase_init = {
        .TypeErase = FLASH_TYPEERASE_SECTORS,
        .Sector = FLASH_SECTOR_7,
        .NbSectors = 1,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3
    };
    uint32_t error;
    
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &error);
    HAL_FLASH_Lock();
    
    return (status == HAL_OK) ? STORAGE_OK : STORAGE_ERROR;
}

storage_info_t storage_get_info(void) {
    return (storage_info_t){
        .total_size = FLASH_SIZE,
        .page_size = FLASH_PAGE_SIZE,
        .sector_size = FLASH_SECTOR_SIZE,
        .write_unit = 1
    };
}

bool storage_is_busy(void) {
    return (FLASH->SR & FLASH_SR_BSY) != 0;
}


/*---------------------------------------------------------------------------
 * Implementation 2: External SPI Flash (外部 SPI Flash 实现)
 *---------------------------------------------------------------------------*/
/* storage_spi_flash.c */
#include "storage.h"
#include "hal_spi.h"

#define W25Q128_SIZE         (16 * 1024 * 1024)
#define W25Q128_SECTOR_SIZE  (4 * 1024)
#define W25Q128_PAGE_SIZE    256

/* W25Q128 命令 */
#define CMD_READ_DATA        0x03
#define CMD_PAGE_PROGRAM     0x02
#define CMD_SECTOR_ERASE     0x20
#define CMD_CHIP_ERASE       0xC7
#define CMD_WRITE_ENABLE     0x06
#define CMD_READ_STATUS      0x05

static bool initialized = false;

static void wait_busy(void) {
    uint8_t status;
    do {
        hal_spi_cs_low();
        hal_spi_transfer(CMD_READ_STATUS);
        status = hal_spi_transfer(0xFF);
        hal_spi_cs_high();
    } while (status & 0x01);
}

static void write_enable(void) {
    hal_spi_cs_low();
    hal_spi_transfer(CMD_WRITE_ENABLE);
    hal_spi_cs_high();
}

storage_status_t storage_init(void) {
    /* 初始化 SPI 外设 */
    if (hal_spi_init(SPI_PORT_1, SPI_MODE_0, 10000000) != HAL_OK) {
        return STORAGE_ERROR;
    }
    
    /* 读取设备 ID 验证 */
    uint8_t id[3];
    hal_spi_cs_low();
    hal_spi_transfer(0x9F);
    id[0] = hal_spi_transfer(0xFF);
    id[1] = hal_spi_transfer(0xFF);
    id[2] = hal_spi_transfer(0xFF);
    hal_spi_cs_high();
    
    if (id[0] != 0xEF || id[1] != 0x40) {
        return STORAGE_ERROR;
    }
    
    initialized = true;
    return STORAGE_OK;
}

storage_status_t storage_read(uint32_t addr, void *data, size_t len) {
    if (!initialized) return STORAGE_ERROR;
    if (addr + len > W25Q128_SIZE) return STORAGE_INVALID_ADDR;
    
    wait_busy();
    
    hal_spi_cs_low();
    hal_spi_transfer(CMD_READ_DATA);
    hal_spi_transfer((addr >> 16) & 0xFF);
    hal_spi_transfer((addr >> 8) & 0xFF);
    hal_spi_transfer(addr & 0xFF);
    
    uint8_t *dst = (uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        dst[i] = hal_spi_transfer(0xFF);
    }
    hal_spi_cs_high();
    
    return STORAGE_OK;
}

storage_status_t storage_write(uint32_t addr, const void *data, size_t len) {
    if (!initialized) return STORAGE_ERROR;
    if (addr + len > W25Q128_SIZE) return STORAGE_INVALID_ADDR;
    
    const uint8_t *src = (const uint8_t *)data;
    
    /* 按页写入 */
    while (len > 0) {
        size_t page_offset = addr % W25Q128_PAGE_SIZE;
        size_t write_len = W25Q128_PAGE_SIZE - page_offset;
        if (write_len > len) write_len = len;
        
        wait_busy();
        write_enable();
        
        hal_spi_cs_low();
        hal_spi_transfer(CMD_PAGE_PROGRAM);
        hal_spi_transfer((addr >> 16) & 0xFF);
        hal_spi_transfer((addr >> 8) & 0xFF);
        hal_spi_transfer(addr & 0xFF);
        
        for (size_t i = 0; i < write_len; i++) {
            hal_spi_transfer(src[i]);
        }
        hal_spi_cs_high();
        
        addr += write_len;
        src += write_len;
        len -= write_len;
    }
    
    wait_busy();
    return STORAGE_OK;
}

storage_info_t storage_get_info(void) {
    return (storage_info_t){
        .total_size = W25Q128_SIZE,
        .page_size = W25Q128_PAGE_SIZE,
        .sector_size = W25Q128_SECTOR_SIZE,
        .write_unit = 1
    };
}


/*---------------------------------------------------------------------------
 * Application Layer - 使用抽象接口
 *---------------------------------------------------------------------------*/
/* config_manager.c */
#include "storage.h"  /* 关键点：只包含抽象接口，不关心具体实现 */
#include <string.h>
#include <stdio.h>

#define CONFIG_START_ADDR  0x0000
#define CONFIG_MAGIC       0xCAFEBABE

typedef struct {
    uint32_t magic;
    uint32_t version;
    char     device_name[32];
    uint8_t  volume;
    uint8_t  brightness;
    uint32_t crc;
} device_config_t;

static device_config_t current_config;

bool config_manager_init(void) {
    /* 关键点：调用抽象接口，不关心是 Flash 还是 EEPROM */
    if (storage_init() != STORAGE_OK) {
        printf("Storage init failed!\n");
        return false;
    }
    
    storage_info_t info = storage_get_info();
    printf("Storage: %lu KB, Sector: %lu B\n", 
           info.total_size / 1024, info.sector_size);
    
    return config_load();
}

bool config_load(void) {
    /* 使用抽象接口读取 */
    if (storage_read(CONFIG_START_ADDR, &current_config, 
                     sizeof(current_config)) != STORAGE_OK) {
        return false;
    }
    
    if (current_config.magic != CONFIG_MAGIC) {
        printf("No valid config, using defaults\n");
        config_reset_default();
        return config_save();
    }
    
    return true;
}

bool config_save(void) {
    current_config.magic = CONFIG_MAGIC;
    current_config.crc = calculate_crc(&current_config, 
                                        sizeof(current_config) - 4);
    
    /* 关键点：抽象接口自动处理 Flash/EEPROM 差异 */
    storage_info_t info = storage_get_info();
    
    /* 如果是 Flash，需要先擦除 */
    if (info.sector_size > 0) {
        storage_erase_sector(CONFIG_START_ADDR);
    }
    
    return storage_write(CONFIG_START_ADDR, &current_config, 
                         sizeof(current_config)) == STORAGE_OK;
}
```

**接口抽象优势分析：**

| 优势 | 说明 |
|------|------|
| **实现可替换** | 内部Flash、SPI Flash、EEPROM 等任意切换 |
| **编译时选择** | Makefile 控制链接哪个实现文件 |
| **易于测试** | 可创建 Mock 实现用于单元测试 |
| **跨平台** | 不同硬件平台提供不同实现，上层代码不变 |
| **符合 SOLID** | 依赖倒置原则（DIP）的典型实践 |

---

## 方法五：依赖注入 (Dependency Injection)

```
+------------------------------------------------------------------+
|                  DEPENDENCY INJECTION PATTERN                     |
+------------------------------------------------------------------+

    WITHOUT INJECTION (Hard Dependency):
    
    +------------------+         +------------------+
    |    Logger        |  new    |   UartLogger     |
    |                  |-------->|   (concrete)     |
    |  log_init() {    |         |                  |
    |    uart_init();  |         +------------------+
    |  }               |
    +------------------+
    
    Problem: Logger is tightly coupled to UartLogger


    WITH INJECTION (Injected Dependency):
    
    +------------------+
    |    Application   |
    |                  |
    |  logger_init(    |
    |    &uart_ops     |----+
    |  )               |    |
    +------------------+    |
                            |  inject
                            v
    +------------------+         +------------------+
    |    Logger        |  uses   | logger_ops_t     |
    |                  |-------->| (interface)      |
    |  ops->write()    |         +------------------+
    |  ops->flush()    |              ^       ^
    +------------------+              |       |
                               +------+       +------+
                               |                     |
                    +------------------+  +------------------+
                    |   uart_ops       |  |   file_ops       |
                    | .write=uart_send |  | .write=file_write|
                    +------------------+  +------------------+
```

**依赖注入说明：**
- 模块不自己创建依赖，而是从外部"注入"
- 通过结构体（函数指针表）传递依赖
- 运行时可替换依赖实现
- 测试时注入 Mock 实现

### 完整代码示例

```c
/*============================================================================
 * 依赖注入模式示例 - 可配置日志系统
 *============================================================================*/

/*---------------------------------------------------------------------------
 * Logger Interface Definition (日志接口定义)
 *---------------------------------------------------------------------------*/
/* logger.h */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

/* 日志级别 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

/*---------------------------------------------------------------------------
 * 关键点：定义依赖接口（函数指针结构体）
 *---------------------------------------------------------------------------*/
typedef struct {
    /* 初始化输出通道 */
    bool (*init)(void *config);
    
    /* 写入数据 */
    int (*write)(const char *data, size_t len);
    
    /* 刷新缓冲区 */
    void (*flush)(void);
    
    /* 关闭通道 */
    void (*close)(void);
    
    /* 可选：获取时间戳 */
    uint32_t (*get_timestamp)(void);
} logger_output_ops_t;

/* Logger API */
bool logger_init(const logger_output_ops_t *ops, void *config);
void logger_set_level(log_level_t level);
void logger_log(log_level_t level, const char *tag, const char *fmt, ...);
void logger_shutdown(void);

/* 便捷宏 */
#define LOG_D(tag, fmt, ...) logger_log(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOG_I(tag, fmt, ...) logger_log(LOG_LEVEL_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) logger_log(LOG_LEVEL_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_E(tag, fmt, ...) logger_log(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOG_F(tag, fmt, ...) logger_log(LOG_LEVEL_FATAL, tag, fmt, ##__VA_ARGS__)

#endif

/* logger.c */
#include "logger.h"
#include <stdio.h>
#include <string.h>

static const logger_output_ops_t *output_ops = NULL;
static log_level_t current_level = LOG_LEVEL_INFO;

static const char *level_names[] = {
    "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"
};

/* 关键点：注入依赖的初始化函数 */
bool logger_init(const logger_output_ops_t *ops, void *config) {
    if (ops == NULL || ops->write == NULL) {
        return false;
    }
    
    output_ops = ops;  /* 保存注入的依赖 */
    
    if (ops->init != NULL) {
        return ops->init(config);
    }
    return true;
}

void logger_set_level(log_level_t level) {
    current_level = level;
}

void logger_log(log_level_t level, const char *tag, const char *fmt, ...) {
    if (output_ops == NULL || level < current_level) {
        return;
    }
    
    char buffer[256];
    int offset = 0;
    
    /* 时间戳 */
    if (output_ops->get_timestamp != NULL) {
        uint32_t ts = output_ops->get_timestamp();
        offset = snprintf(buffer, sizeof(buffer), "[%lu] ", ts);
    }
    
    /* 级别和标签 */
    offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                       "[%s] [%s] ", level_names[level], tag);
    
    /* 消息内容 */
    va_list args;
    va_start(args, fmt);
    offset += vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    va_end(args);
    
    /* 换行符 */
    if (offset < sizeof(buffer) - 1) {
        buffer[offset++] = '\n';
    }
    
    /* 关键点：通过注入的依赖输出 */
    output_ops->write(buffer, offset);
    
    if (output_ops->flush != NULL) {
        output_ops->flush();
    }
}

void logger_shutdown(void) {
    if (output_ops != NULL && output_ops->close != NULL) {
        output_ops->close();
    }
    output_ops = NULL;
}


/*---------------------------------------------------------------------------
 * Concrete Implementation 1: UART Output (UART 输出实现)
 *---------------------------------------------------------------------------*/
/* logger_uart.c */
#include "logger.h"
#include "hal_uart.h"

typedef struct {
    uint8_t port;
    uint32_t baudrate;
} uart_logger_config_t;

static uint8_t uart_port;

static bool uart_output_init(void *config) {
    uart_logger_config_t *cfg = (uart_logger_config_t *)config;
    uart_port = cfg->port;
    
    hal_uart_config_t uart_cfg = {
        .baudrate = cfg->baudrate,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = 0
    };
    
    return hal_uart_init(uart_port, &uart_cfg) == HAL_UART_OK;
}

static int uart_output_write(const char *data, size_t len) {
    if (hal_uart_send(uart_port, (const uint8_t *)data, len) == HAL_UART_OK) {
        return (int)len;
    }
    return -1;
}

static void uart_output_flush(void) {
    while (hal_uart_is_busy(uart_port)) {
        /* Wait for transmission complete */
    }
}

static uint32_t uart_get_timestamp(void) {
    return hal_tick_get_ms();
}

/* 关键点：定义 UART 输出操作集 */
const logger_output_ops_t uart_logger_ops = {
    .init = uart_output_init,
    .write = uart_output_write,
    .flush = uart_output_flush,
    .close = NULL,
    .get_timestamp = uart_get_timestamp
};


/*---------------------------------------------------------------------------
 * Concrete Implementation 2: File Output (文件输出实现 - 用于 PC 测试)
 *---------------------------------------------------------------------------*/
/* logger_file.c */
#include "logger.h"
#include <stdio.h>
#include <time.h>

typedef struct {
    const char *filename;
} file_logger_config_t;

static FILE *log_file = NULL;

static bool file_output_init(void *config) {
    file_logger_config_t *cfg = (file_logger_config_t *)config;
    log_file = fopen(cfg->filename, "a");
    return log_file != NULL;
}

static int file_output_write(const char *data, size_t len) {
    if (log_file != NULL) {
        return (int)fwrite(data, 1, len, log_file);
    }
    return -1;
}

static void file_output_flush(void) {
    if (log_file != NULL) {
        fflush(log_file);
    }
}

static void file_output_close(void) {
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

static uint32_t file_get_timestamp(void) {
    return (uint32_t)time(NULL);
}

/* 文件输出操作集 */
const logger_output_ops_t file_logger_ops = {
    .init = file_output_init,
    .write = file_output_write,
    .flush = file_output_flush,
    .close = file_output_close,
    .get_timestamp = file_get_timestamp
};


/*---------------------------------------------------------------------------
 * Concrete Implementation 3: Mock Output (Mock 实现 - 用于单元测试)
 *---------------------------------------------------------------------------*/
/* logger_mock.c */
#include "logger.h"
#include <string.h>

#define MOCK_BUFFER_SIZE  4096

static char mock_buffer[MOCK_BUFFER_SIZE];
static size_t mock_buffer_len = 0;
static int write_count = 0;

static bool mock_output_init(void *config) {
    mock_buffer_len = 0;
    write_count = 0;
    return true;
}

static int mock_output_write(const char *data, size_t len) {
    if (mock_buffer_len + len < MOCK_BUFFER_SIZE) {
        memcpy(mock_buffer + mock_buffer_len, data, len);
        mock_buffer_len += len;
    }
    write_count++;
    return (int)len;
}

/* Mock 专用：验证函数 */
const char* mock_logger_get_buffer(void) {
    mock_buffer[mock_buffer_len] = '\0';
    return mock_buffer;
}

int mock_logger_get_write_count(void) {
    return write_count;
}

void mock_logger_reset(void) {
    mock_buffer_len = 0;
    write_count = 0;
}

const logger_output_ops_t mock_logger_ops = {
    .init = mock_output_init,
    .write = mock_output_write,
    .flush = NULL,
    .close = NULL,
    .get_timestamp = NULL
};


/*---------------------------------------------------------------------------
 * Application Layer - 注入依赖并使用
 *---------------------------------------------------------------------------*/
/* main.c */
#include "logger.h"

/* 声明外部定义的操作集 */
extern const logger_output_ops_t uart_logger_ops;
extern const logger_output_ops_t file_logger_ops;

int main(void) {
    /* 关键点：根据平台选择不同的依赖注入 */
    
#if defined(EMBEDDED_PLATFORM)
    /* 嵌入式平台：注入 UART 输出 */
    uart_logger_config_t uart_cfg = {
        .port = 0,
        .baudrate = 115200
    };
    logger_init(&uart_logger_ops, &uart_cfg);
    
#else
    /* PC 平台：注入文件输出 */
    file_logger_config_t file_cfg = {
        .filename = "application.log"
    };
    logger_init(&file_logger_ops, &file_cfg);
#endif
    
    logger_set_level(LOG_LEVEL_DEBUG);
    
    /* 使用日志 - 与具体输出方式解耦 */
    LOG_I("MAIN", "Application started");
    LOG_D("MAIN", "Debug information: value=%d", 42);
    LOG_W("MAIN", "Warning: low memory");
    LOG_E("MAIN", "Error occurred: code=%d", -1);
    
    /* 应用逻辑... */
    
    logger_shutdown();
    return 0;
}


/*---------------------------------------------------------------------------
 * Unit Test - 使用 Mock 依赖
 *---------------------------------------------------------------------------*/
/* test_logger.c */
#include "logger.h"
#include <assert.h>
#include <string.h>

extern const logger_output_ops_t mock_logger_ops;
extern const char* mock_logger_get_buffer(void);
extern int mock_logger_get_write_count(void);
extern void mock_logger_reset(void);

void test_logger_basic(void) {
    /* 关键点：测试时注入 Mock 依赖 */
    logger_init(&mock_logger_ops, NULL);
    mock_logger_reset();
    
    LOG_I("TEST", "Hello %s", "World");
    
    const char *output = mock_logger_get_buffer();
    assert(strstr(output, "INFO") != NULL);
    assert(strstr(output, "TEST") != NULL);
    assert(strstr(output, "Hello World") != NULL);
    assert(mock_logger_get_write_count() == 1);
    
    printf("test_logger_basic PASSED\n");
}

void test_logger_level_filter(void) {
    logger_init(&mock_logger_ops, NULL);
    mock_logger_reset();
    
    logger_set_level(LOG_LEVEL_WARN);
    
    LOG_D("TEST", "Debug");  /* Should be filtered */
    LOG_I("TEST", "Info");   /* Should be filtered */
    LOG_W("TEST", "Warn");   /* Should pass */
    LOG_E("TEST", "Error");  /* Should pass */
    
    assert(mock_logger_get_write_count() == 2);
    
    const char *output = mock_logger_get_buffer();
    assert(strstr(output, "Debug") == NULL);
    assert(strstr(output, "Warn") != NULL);
    
    printf("test_logger_level_filter PASSED\n");
}
```

**依赖注入优势分析：**

| 优势 | 说明 |
|------|------|
| **运行时可替换** | 不重新编译即可切换实现 |
| **易于测试** | 注入 Mock 依赖进行单元测试 |
| **松耦合** | Logger 不依赖具体输出方式 |
| **可配置** | 同一程序可配置不同输出目标 |
| **跨平台** | 嵌入式和 PC 可用同一套日志代码 |

---

## 方法六：观察者模式 / 发布-订阅 (Observer / Pub-Sub)

```
+------------------------------------------------------------------+
|                    OBSERVER / PUB-SUB PATTERN                     |
+------------------------------------------------------------------+

    +------------------+
    |     Subject      |
    |   (Publisher)    |
    +--------+---------+
             |
             | notify_all()
             |
    +--------v-------------------------------------------+
    |              OBSERVER LIST                         |
    | +------------+ +------------+ +------------+       |
    | | Observer 1 | | Observer 2 | | Observer 3 |       |
    | +------------+ +------------+ +------------+       |
    +----------------------------------------------------+
             |              |              |
             v              v              v
    +------------+   +------------+   +------------+
    | Display    |   |   Logger   |   |   Alarm    |
    | update()   |   |  update()  |   |  update()  |
    +------------+   +------------+   +------------+


    Sequence:
    
    Subject                Observer1        Observer2        Observer3
       |                      |                |                |
       | subscribe(obs1)      |                |                |
       |<---------------------|                |                |
       | subscribe(obs2)      |                |                |
       |<------------------------------------- |                |
       | subscribe(obs3)      |                |                |
       |<---------------------------------------------------- -|
       |                      |                |                |
       | notify(data)         |                |                |
       |--------------------->|                |                |
       |      update(data)    |                |                |
       |------------------------------------>  |                |
       |               update(data)            |                |
       |---------------------------------------------------- ->|
       |                               update(data)            |
```

**观察者模式说明：**
- Subject（主题）维护观察者列表
- 状态变化时通知所有观察者
- 观察者自行决定如何响应
- 一对多的解耦通知机制

### 完整代码示例

```c
/*============================================================================
 * 观察者模式示例 - 温度监控系统（多模块响应）
 *============================================================================*/

/*---------------------------------------------------------------------------
 * Observer Interface (观察者接口)
 *---------------------------------------------------------------------------*/
/* observer.h */
#ifndef OBSERVER_H
#define OBSERVER_H

#include <stdint.h>

/* 事件类型 */
typedef enum {
    EVENT_TEMP_CHANGED,
    EVENT_TEMP_ALERT,
    EVENT_SENSOR_ERROR
} event_type_t;

/* 事件数据 */
typedef struct {
    event_type_t type;
    union {
        struct { float value; float delta; } temp;
        struct { float threshold; float current; } alert;
        struct { int code; const char *msg; } error;
    } data;
} event_t;

/* 关键点：观察者接口定义 */
typedef void (*observer_callback_t)(const event_t *event, void *user_data);

typedef struct {
    observer_callback_t callback;
    void *user_data;
    uint8_t event_mask;  /* 订阅的事件类型掩码 */
} observer_t;

#endif


/*---------------------------------------------------------------------------
 * Subject (发布者 / 被观察主题)
 *---------------------------------------------------------------------------*/
/* temperature_subject.h */
#ifndef TEMPERATURE_SUBJECT_H
#define TEMPERATURE_SUBJECT_H

#include "observer.h"

#define MAX_OBSERVERS  8

/* 主题 API */
void temp_subject_init(void);
bool temp_subject_subscribe(observer_t *observer);
bool temp_subject_unsubscribe(observer_t *observer);
void temp_subject_set_value(float new_temp);
void temp_subject_notify_error(int code, const char *msg);
float temp_subject_get_value(void);

#endif

/* temperature_subject.c */
#include "temperature_subject.h"
#include <string.h>
#include <math.h>

static observer_t *observers[MAX_OBSERVERS];
static uint8_t observer_count = 0;
static float current_temp = 0.0f;
static float alert_threshold = 45.0f;

void temp_subject_init(void) {
    observer_count = 0;
    current_temp = 0.0f;
    memset(observers, 0, sizeof(observers));
}

/* 关键点：订阅 - 将观察者加入列表 */
bool temp_subject_subscribe(observer_t *observer) {
    if (observer_count >= MAX_OBSERVERS) {
        return false;
    }
    observers[observer_count++] = observer;
    return true;
}

bool temp_subject_unsubscribe(observer_t *observer) {
    for (uint8_t i = 0; i < observer_count; i++) {
        if (observers[i] == observer) {
            /* 移动后续元素 */
            for (uint8_t j = i; j < observer_count - 1; j++) {
                observers[j] = observers[j + 1];
            }
            observer_count--;
            return true;
        }
    }
    return false;
}

/* 关键点：通知所有订阅者 */
static void notify_observers(const event_t *event) {
    for (uint8_t i = 0; i < observer_count; i++) {
        observer_t *obs = observers[i];
        
        /* 检查是否订阅了此事件类型 */
        if (obs->event_mask & (1 << event->type)) {
            obs->callback(event, obs->user_data);
        }
    }
}

void temp_subject_set_value(float new_temp) {
    float old_temp = current_temp;
    current_temp = new_temp;
    
    /* 温度变化事件 */
    event_t event = {
        .type = EVENT_TEMP_CHANGED,
        .data.temp.value = new_temp,
        .data.temp.delta = new_temp - old_temp
    };
    notify_observers(&event);
    
    /* 检查是否触发告警 */
    if (new_temp > alert_threshold && old_temp <= alert_threshold) {
        event_t alert = {
            .type = EVENT_TEMP_ALERT,
            .data.alert.threshold = alert_threshold,
            .data.alert.current = new_temp
        };
        notify_observers(&alert);
    }
}

void temp_subject_notify_error(int code, const char *msg) {
    event_t event = {
        .type = EVENT_SENSOR_ERROR,
        .data.error.code = code,
        .data.error.msg = msg
    };
    notify_observers(&event);
}


/*---------------------------------------------------------------------------
 * Concrete Observers (具体观察者实现)
 *---------------------------------------------------------------------------*/

/* 观察者1：LCD 显示模块 */
#include "lcd_driver.h"

void display_observer_callback(const event_t *event, void *user_data) {
    switch (event->type) {
        case EVENT_TEMP_CHANGED:
            lcd_printf(0, 0, "Temp: %.1f C", event->data.temp.value);
            break;
            
        case EVENT_TEMP_ALERT:
            lcd_set_backlight(LCD_RED);
            lcd_printf(1, 0, "ALERT! >%.0fC", event->data.alert.threshold);
            break;
            
        case EVENT_SENSOR_ERROR:
            lcd_printf(1, 0, "ERR: %s", event->data.error.msg);
            break;
    }
}

static observer_t display_observer = {
    .callback = display_observer_callback,
    .user_data = NULL,
    .event_mask = (1 << EVENT_TEMP_CHANGED) | 
                  (1 << EVENT_TEMP_ALERT) | 
                  (1 << EVENT_SENSOR_ERROR)
};


/* 观察者2：数据记录模块 */
#include "data_logger.h"

typedef struct {
    uint32_t log_interval_ms;
    uint32_t last_log_time;
} logger_context_t;

void logger_observer_callback(const event_t *event, void *user_data) {
    logger_context_t *ctx = (logger_context_t *)user_data;
    uint32_t now = hal_tick_get_ms();
    
    if (event->type == EVENT_TEMP_CHANGED) {
        /* 按间隔记录，避免过于频繁 */
        if (now - ctx->last_log_time >= ctx->log_interval_ms) {
            data_logger_record("TEMP", event->data.temp.value);
            ctx->last_log_time = now;
        }
    } else if (event->type == EVENT_TEMP_ALERT) {
        /* 告警立即记录 */
        data_logger_record_alert("TEMP_ALERT", event->data.alert.current);
    }
}

static logger_context_t logger_ctx = {
    .log_interval_ms = 1000,
    .last_log_time = 0
};

static observer_t logger_observer = {
    .callback = logger_observer_callback,
    .user_data = &logger_ctx,
    .event_mask = (1 << EVENT_TEMP_CHANGED) | (1 << EVENT_TEMP_ALERT)
};


/* 观察者3：报警模块 */
#include "buzzer.h"
#include "led.h"

void alarm_observer_callback(const event_t *event, void *user_data) {
    if (event->type == EVENT_TEMP_ALERT) {
        led_blink_start(LED_RED, 500);  /* 红灯闪烁 */
        buzzer_beep_pattern(PATTERN_ALARM);
    } else if (event->type == EVENT_SENSOR_ERROR) {
        led_on(LED_YELLOW);
        buzzer_beep(100);
    }
}

static observer_t alarm_observer = {
    .callback = alarm_observer_callback,
    .user_data = NULL,
    .event_mask = (1 << EVENT_TEMP_ALERT) | (1 << EVENT_SENSOR_ERROR)
};


/*---------------------------------------------------------------------------
 * Application - 组装并运行
 *---------------------------------------------------------------------------*/
int main(void) {
    /* 初始化主题 */
    temp_subject_init();
    
    /* 关键点：注册观察者 - 模块间解耦 */
    temp_subject_subscribe(&display_observer);
    temp_subject_subscribe(&logger_observer);
    temp_subject_subscribe(&alarm_observer);
    
    printf("Temperature monitor started.\n");
    printf("Subscribed observers: Display, Logger, Alarm\n");
    
    /* 模拟温度变化 */
    float temps[] = {25.0, 30.0, 35.0, 40.0, 46.0, 48.0, 44.0};
    
    for (int i = 0; i < sizeof(temps)/sizeof(temps[0]); i++) {
        printf("\n--- Setting temperature to %.1f ---\n", temps[i]);
        
        /* 关键点：设置值会自动通知所有观察者 */
        temp_subject_set_value(temps[i]);
        
        delay_ms(2000);
    }
    
    /* 模拟错误 */
    temp_subject_notify_error(-1, "Sensor disconnected");
    
    return 0;
}
```

**观察者模式优势分析：**

| 优势 | 说明 |
|------|------|
| **一对多通知** | 一次状态变化，多个模块响应 |
| **松耦合** | Subject 不需要知道 Observer 的具体类型 |
| **动态订阅** | 运行时可增删观察者 |
| **事件过滤** | 观察者只接收关心的事件 |
| **易于扩展** | 新增响应模块无需修改 Subject |

---

## 总结对比

```
+------------------------------------------------------------------+
|              LAYER INTERACTION METHODS COMPARISON                 |
+------------------------------------------------------------------+

    +------------------+------------------+------------------+
    |   Method         |   Direction      |   Coupling       |
    +------------------+------------------+------------------+
    | Direct Call      | Upper -> Lower   | Tight            |
    | Callback         | Lower -> Upper   | Loose            |
    | Message Queue    | Async Both Ways  | Very Loose       |
    | Interface        | Upper -> Abstract| Loose            |
    | Dependency Inj.  | External -> Both | Very Loose       |
    | Observer         | Subject -> Many  | Very Loose       |
    +------------------+------------------+------------------+


    SELECTION GUIDE:
    
    +------------------------------------------+
    |  Use Direct Call when:                   |
    |  - Simple request-response              |
    |  - Synchronous operation needed          |
    |  - Performance critical                  |
    +------------------------------------------+
    
    +------------------------------------------+
    |  Use Callback when:                      |
    |  - Lower layer notifies upper layer      |
    |  - Event-driven design                   |
    |  - Need inversion of control             |
    +------------------------------------------+
    
    +------------------------------------------+
    |  Use Message Queue when:                 |
    |  - Async communication needed            |
    |  - ISR to main loop communication        |
    |  - Need to buffer events                 |
    +------------------------------------------+
    
    +------------------------------------------+
    |  Use Interface Abstraction when:         |
    |  - Multiple implementations possible     |
    |  - Platform portability needed           |
    |  - Compile-time selection                |
    +------------------------------------------+
    
    +------------------------------------------+
    |  Use Dependency Injection when:          |
    |  - Runtime configuration needed          |
    |  - Unit testing important                |
    |  - Complex dependencies                  |
    +------------------------------------------+
    
    +------------------------------------------+
    |  Use Observer when:                      |
    |  - One-to-many notification              |
    |  - Multiple modules respond to event     |
    |  - Dynamic subscription needed           |
    +------------------------------------------+
```

**总结对比说明：**

| 方法 | 典型场景 | 优势 | 局限 |
|------|----------|------|------|
| **直接调用** | 服务请求 | 简单高效 | 强耦合 |
| **回调函数** | 事件通知 | 控制反转 | 回调地狱 |
| **消息队列** | 中断通信 | 异步解耦 | 复杂度高 |
| **接口抽象** | 驱动封装 | 可替换 | 编译时绑定 |
| **依赖注入** | 配置化 | 运行时替换 | 初始化复杂 |
| **观察者** | 广播通知 | 一对多 | 调试困难 |

**实践建议：**
- 简单系统：直接调用 + 回调函数
- 中等系统：+ 接口抽象
- 复杂系统：+ 消息队列 + 依赖注入 + 观察者
- 可测试性要求高：必用依赖注入

