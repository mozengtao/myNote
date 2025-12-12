# 嵌入式设计模式 - 安全和可靠性模式

本文档介绍嵌入式系统中用于保证安全和可靠性的设计模式，包括完整的 C 语言代码示例。

---

## 目录

1. [一补码模式 (One's Complement Pattern)](#1-一补码模式)
2. [CRC 模式 (CRC Pattern)](#2-crc-模式)
3. [智能数据模式 (Smart Data Pattern)](#3-智能数据模式)
4. [通道模式 (Channel Pattern)](#4-通道模式)
5. [受保护单通道模式 (Protected Single Channel Pattern)](#5-受保护单通道模式)
6. [双通道模式 (Dual Channel Pattern)](#6-双通道模式)

---

## 1. 一补码模式

### 架构图

```
+------------------------------------------------------------------+
|                   ONE'S COMPLEMENT PATTERN                        |
+------------------------------------------------------------------+

    Memory Storage:
    
    +-------------+-------------+
    |    Value    |  Inverted   |
    |  (Normal)   | (Complement)|
    +-------------+-------------+
    |  0x55AA     |  0xAA55     |
    +-------------+-------------+
    
    Write: value -> memory[0]
           ~value -> memory[1]


    Read Verification:
    
    memory[0] = 0x55AA (value)
    memory[1] = 0xAA55 (inverted)
    
    Check: value == ~inverted ?
           0x55AA == ~0xAA55 ?
           0x55AA == 0x55AA ? --> Valid!
    
    
    Corruption Detection:
    
    Original:    0x55AA | 0xAA55  --> Valid
    
    Bit flip:    0x55AB | 0xAA55  --> Invalid!
                 0x55AB != ~0xAA55
    
    Both corrupt: Very unlikely (requires
                  perfectly complementary
                  bit flips)
```

**中文说明：**
- 一补码模式将关键数据存储两份：正常值和取反值
- 读取时比较两者是否互补，检测内存损坏
- 可以检测单个或多个位翻转错误
- 适用于保护关键配置参数和状态变量

### 完整代码示例

```c
/*============================================================================
 * 一补码模式示例 - 关键数据保护
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * 一补码保护数据结构
 *---------------------------------------------------------------------------*/
/* 关键点：同时存储值和其一补码 */
typedef struct {
    uint32_t value;
    uint32_t complement;    /* ~value */
} protected_u32_t;

typedef struct {
    uint16_t value;
    uint16_t complement;
} protected_u16_t;

typedef struct {
    uint8_t value;
    uint8_t complement;
} protected_u8_t;

/*---------------------------------------------------------------------------
 * 一补码操作函数
 *---------------------------------------------------------------------------*/
/* 关键点：写入时同时存储值和补码 */
void protected_u32_write(protected_u32_t *p, uint32_t value) {
    p->value = value;
    p->complement = ~value;  /* 一补码 */
}

void protected_u16_write(protected_u16_t *p, uint16_t value) {
    p->value = value;
    p->complement = ~value;
}

void protected_u8_write(protected_u8_t *p, uint8_t value) {
    p->value = value;
    p->complement = ~value;
}

/* 关键点：读取时验证值和补码是否匹配 */
bool protected_u32_read(const protected_u32_t *p, uint32_t *value) {
    /* 检查：value 应该等于 complement 的取反 */
    if (p->value == ~p->complement) {
        *value = p->value;
        return true;  /* 数据有效 */
    }
    return false;  /* 数据损坏 */
}

bool protected_u16_read(const protected_u16_t *p, uint16_t *value) {
    if (p->value == (uint16_t)~p->complement) {
        *value = p->value;
        return true;
    }
    return false;
}

bool protected_u8_read(const protected_u8_t *p, uint8_t *value) {
    if (p->value == (uint8_t)~p->complement) {
        *value = p->value;
        return true;
    }
    return false;
}

/* 验证但不读取 */
bool protected_u32_verify(const protected_u32_t *p) {
    return p->value == ~p->complement;
}

/*---------------------------------------------------------------------------
 * 应用示例：关键系统参数
 *---------------------------------------------------------------------------*/
typedef struct {
    protected_u32_t firmware_version;
    protected_u32_t boot_count;
    protected_u16_t calibration_offset;
    protected_u8_t  safety_flags;
    protected_u32_t checksum;  /* 整体校验 */
} critical_params_t;

static critical_params_t g_params;

void params_init(void) {
    protected_u32_write(&g_params.firmware_version, 0x01020003);  /* v1.2.3 */
    protected_u32_write(&g_params.boot_count, 0);
    protected_u16_write(&g_params.calibration_offset, 512);
    protected_u8_write(&g_params.safety_flags, 0x00);
    protected_u32_write(&g_params.checksum, 0);
    
    printf("[Params] Initialized\n");
}

bool params_verify_all(void) {
    bool valid = true;
    
    if (!protected_u32_verify(&g_params.firmware_version)) {
        printf("  [ERROR] firmware_version corrupted!\n");
        valid = false;
    }
    if (!protected_u32_verify(&g_params.boot_count)) {
        printf("  [ERROR] boot_count corrupted!\n");
        valid = false;
    }
    if (!protected_u16_read(&g_params.calibration_offset, &(uint16_t){0})) {
        printf("  [ERROR] calibration_offset corrupted!\n");
        valid = false;
    }
    if (!protected_u8_read(&g_params.safety_flags, &(uint8_t){0})) {
        printf("  [ERROR] safety_flags corrupted!\n");
        valid = false;
    }
    
    return valid;
}

void params_increment_boot_count(void) {
    uint32_t count;
    if (protected_u32_read(&g_params.boot_count, &count)) {
        protected_u32_write(&g_params.boot_count, count + 1);
    }
}

void params_print(void) {
    uint32_t version, boot_count;
    uint16_t offset;
    uint8_t flags;
    
    printf("\n=== Critical Parameters ===\n");
    
    if (protected_u32_read(&g_params.firmware_version, &version)) {
        printf("  Firmware: v%u.%u.%u\n",
               (version >> 24) & 0xFF,
               (version >> 16) & 0xFF,
               version & 0xFFFF);
    } else {
        printf("  Firmware: CORRUPTED!\n");
    }
    
    if (protected_u32_read(&g_params.boot_count, &boot_count)) {
        printf("  Boot count: %u\n", boot_count);
    } else {
        printf("  Boot count: CORRUPTED!\n");
    }
    
    if (protected_u16_read(&g_params.calibration_offset, &offset)) {
        printf("  Calibration offset: %d\n", offset);
    } else {
        printf("  Calibration offset: CORRUPTED!\n");
    }
    
    if (protected_u8_read(&g_params.safety_flags, &flags)) {
        printf("  Safety flags: 0x%02X\n", flags);
    } else {
        printf("  Safety flags: CORRUPTED!\n");
    }
}

/*---------------------------------------------------------------------------
 * 模拟位翻转（测试用）
 *---------------------------------------------------------------------------*/
void simulate_bit_flip(void *addr, int bit) {
    uint8_t *ptr = (uint8_t *)addr;
    int byte_idx = bit / 8;
    int bit_idx = bit % 8;
    
    ptr[byte_idx] ^= (1 << bit_idx);
    
    printf("[Simulate] Bit flip at byte %d, bit %d\n", byte_idx, bit_idx);
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void ones_complement_example(void) {
    printf("=== One's Complement Pattern Demo ===\n\n");
    
    /* 初始化参数 */
    params_init();
    params_print();
    
    /* 正常操作 */
    printf("\n--- Normal Operations ---\n");
    params_increment_boot_count();
    params_increment_boot_count();
    params_increment_boot_count();
    
    printf("After 3 boots:\n");
    params_print();
    
    /* 验证完整性 */
    printf("\n--- Integrity Check ---\n");
    if (params_verify_all()) {
        printf("All parameters valid!\n");
    }
    
    /* 模拟内存损坏 */
    printf("\n--- Simulating Memory Corruption ---\n");
    
    /* 只翻转 value，不翻转 complement */
    printf("\nCorrupting boot_count value:\n");
    simulate_bit_flip(&g_params.boot_count.value, 3);
    
    /* 验证检测 */
    printf("\nIntegrity check after corruption:\n");
    if (!params_verify_all()) {
        printf("Corruption detected! System should enter safe mode.\n");
    }
    
    /* 尝试读取损坏的数据 */
    uint32_t boot_count;
    if (!protected_u32_read(&g_params.boot_count, &boot_count)) {
        printf("Cannot read boot_count - data corrupted!\n");
    }
    
    /* 修复（重新写入） */
    printf("\n--- Recovery ---\n");
    protected_u32_write(&g_params.boot_count, 0);  /* 重置为安全值 */
    
    if (params_verify_all()) {
        printf("System recovered, parameters valid.\n");
    }
    params_print();
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 2. CRC 模式

### 架构图

```
+------------------------------------------------------------------+
|                        CRC PATTERN                                |
+------------------------------------------------------------------+

    CRC Calculation:
    
    +--------+--------+--------+--------+
    | Data 0 | Data 1 | Data 2 | Data 3 |  --> Input data
    +--------+--------+--------+--------+
         |
         v
    +-----------------+
    | CRC Algorithm   |
    | (Polynomial)    |
    +-----------------+
         |
         v
    +--------+
    |  CRC   |  --> 16 or 32 bit value
    +--------+


    Storage Format:
    
    +------------------------------------------+--------+
    |              Data Payload                |  CRC   |
    +------------------------------------------+--------+
    
    
    Verification:
    
    1. Calculate CRC of data
    2. Compare with stored CRC
    3. Match = Data valid, Mismatch = Data corrupted
    
    
    CRC-16 Polynomial Example (CCITT):
    
    x^16 + x^12 + x^5 + 1 = 0x1021
```

**中文说明：**
- CRC（循环冗余校验）用于检测大数据块中的错误
- 计算数据的 CRC 值并与数据一起存储
- 读取时重新计算 CRC 并比较，检测数据损坏
- 广泛用于通信协议和数据存储

### 完整代码示例

```c
/*============================================================================
 * CRC 模式示例 - 数据完整性保护
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * CRC-16 (CCITT) 实现
 *---------------------------------------------------------------------------*/
#define CRC16_INIT  0xFFFF
#define CRC16_POLY  0x1021  /* x^16 + x^12 + x^5 + 1 */

/* 关键点：CRC-16 计算（按位计算） */
uint16_t crc16_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ CRC16_POLY;
        } else {
            crc = crc << 1;
        }
    }
    
    return crc;
}

uint16_t crc16_calculate(const uint8_t *data, size_t length) {
    uint16_t crc = CRC16_INIT;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc16_update(crc, data[i]);
    }
    
    return crc;
}

/*---------------------------------------------------------------------------
 * CRC-32 实现（使用查表法提高效率）
 *---------------------------------------------------------------------------*/
#define CRC32_INIT  0xFFFFFFFF
#define CRC32_POLY  0xEDB88320  /* 反转的多项式 */

static uint32_t crc32_table[256];
static bool crc32_table_ready = false;

/* 关键点：生成 CRC-32 查找表 */
void crc32_init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLY;
            } else {
                crc = crc >> 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_ready = true;
}

/* 关键点：CRC-32 计算（查表法） */
uint32_t crc32_calculate(const uint8_t *data, size_t length) {
    if (!crc32_table_ready) {
        crc32_init_table();
    }
    
    uint32_t crc = CRC32_INIT;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return crc ^ CRC32_INIT;  /* 最终异或 */
}

/*---------------------------------------------------------------------------
 * CRC 保护的数据块
 *---------------------------------------------------------------------------*/
#define MAX_DATA_SIZE 256

typedef struct {
    uint8_t data[MAX_DATA_SIZE];
    size_t length;
    uint32_t crc32;
} crc_protected_block_t;

/* 关键点：写入数据并计算 CRC */
bool block_write(crc_protected_block_t *block, const uint8_t *data, size_t length) {
    if (length > MAX_DATA_SIZE) {
        return false;
    }
    
    memcpy(block->data, data, length);
    block->length = length;
    block->crc32 = crc32_calculate(data, length);
    
    printf("[Block] Written %zu bytes, CRC32: 0x%08X\n", length, block->crc32);
    return true;
}

/* 关键点：读取数据并验证 CRC */
bool block_read(const crc_protected_block_t *block, uint8_t *data, size_t *length) {
    /* 重新计算 CRC */
    uint32_t calculated_crc = crc32_calculate(block->data, block->length);
    
    if (calculated_crc != block->crc32) {
        printf("[Block] CRC MISMATCH! Stored: 0x%08X, Calculated: 0x%08X\n",
               block->crc32, calculated_crc);
        return false;
    }
    
    memcpy(data, block->data, block->length);
    *length = block->length;
    
    printf("[Block] Read %zu bytes, CRC verified\n", *length);
    return true;
}

/* 仅验证不读取 */
bool block_verify(const crc_protected_block_t *block) {
    uint32_t calculated_crc = crc32_calculate(block->data, block->length);
    return calculated_crc == block->crc32;
}

/*---------------------------------------------------------------------------
 * 应用示例：配置存储
 *---------------------------------------------------------------------------*/
typedef struct {
    char device_name[32];
    uint32_t serial_number;
    uint16_t config_version;
    uint8_t network_settings[16];
    float calibration_values[4];
} device_config_t;

typedef struct {
    device_config_t config;
    uint32_t crc32;
} stored_config_t;

static stored_config_t g_stored_config;

void config_save(const device_config_t *config) {
    memcpy(&g_stored_config.config, config, sizeof(device_config_t));
    g_stored_config.crc32 = crc32_calculate((uint8_t *)config, sizeof(device_config_t));
    
    printf("[Config] Saved, CRC32: 0x%08X\n", g_stored_config.crc32);
}

bool config_load(device_config_t *config) {
    uint32_t calc_crc = crc32_calculate((uint8_t *)&g_stored_config.config, 
                                         sizeof(device_config_t));
    
    if (calc_crc != g_stored_config.crc32) {
        printf("[Config] CRC verification FAILED!\n");
        return false;
    }
    
    memcpy(config, &g_stored_config.config, sizeof(device_config_t));
    printf("[Config] Loaded and verified\n");
    return true;
}

/*---------------------------------------------------------------------------
 * 应用示例：通信协议
 *---------------------------------------------------------------------------*/
#define FRAME_HEADER 0xAA55
#define MAX_PAYLOAD  64

typedef struct {
    uint16_t header;
    uint8_t cmd;
    uint8_t length;
    uint8_t payload[MAX_PAYLOAD];
    uint16_t crc16;
} protocol_frame_t;

/* 关键点：构建带 CRC 的通信帧 */
size_t frame_build(protocol_frame_t *frame, uint8_t cmd, 
                   const uint8_t *payload, uint8_t length) {
    frame->header = FRAME_HEADER;
    frame->cmd = cmd;
    frame->length = length;
    
    if (length > 0 && payload != NULL) {
        memcpy(frame->payload, payload, length);
    }
    
    /* 关键点：CRC 计算包括 header、cmd、length 和 payload */
    uint16_t crc = crc16_calculate((uint8_t *)&frame->header, 2);
    crc = crc16_update(crc, frame->cmd);
    crc = crc16_update(crc, frame->length);
    for (int i = 0; i < length; i++) {
        crc = crc16_update(crc, frame->payload[i]);
    }
    frame->crc16 = crc;
    
    printf("[Frame] Built cmd=0x%02X, len=%d, CRC16: 0x%04X\n",
           cmd, length, crc);
    
    return 4 + length + 2;  /* header(2) + cmd(1) + len(1) + payload + crc(2) */
}

/* 关键点：验证接收到的帧 */
bool frame_verify(const protocol_frame_t *frame) {
    if (frame->header != FRAME_HEADER) {
        printf("[Frame] Invalid header\n");
        return false;
    }
    
    /* 重新计算 CRC */
    uint16_t crc = crc16_calculate((uint8_t *)&frame->header, 2);
    crc = crc16_update(crc, frame->cmd);
    crc = crc16_update(crc, frame->length);
    for (int i = 0; i < frame->length; i++) {
        crc = crc16_update(crc, frame->payload[i]);
    }
    
    if (crc != frame->crc16) {
        printf("[Frame] CRC mismatch: expected 0x%04X, got 0x%04X\n",
               frame->crc16, crc);
        return false;
    }
    
    printf("[Frame] Verified successfully\n");
    return true;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void crc_example(void) {
    printf("=== CRC Pattern Demo ===\n\n");
    
    /* 初始化 CRC 表 */
    crc32_init_table();
    
    /* 基本 CRC 计算 */
    printf("--- Basic CRC Calculation ---\n");
    const char *test_data = "Hello, World!";
    uint16_t crc16 = crc16_calculate((uint8_t *)test_data, strlen(test_data));
    uint32_t crc32 = crc32_calculate((uint8_t *)test_data, strlen(test_data));
    printf("Data: \"%s\"\n", test_data);
    printf("CRC-16: 0x%04X\n", crc16);
    printf("CRC-32: 0x%08X\n", crc32);
    
    /* 数据块保护 */
    printf("\n--- Protected Data Block ---\n");
    crc_protected_block_t block;
    uint8_t write_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    block_write(&block, write_data, sizeof(write_data));
    
    uint8_t read_data[MAX_DATA_SIZE];
    size_t read_len;
    if (block_read(&block, read_data, &read_len)) {
        printf("Read back %zu bytes successfully\n", read_len);
    }
    
    /* 模拟数据损坏 */
    printf("\n--- Simulating Data Corruption ---\n");
    block.data[2] ^= 0x10;  /* 翻转一位 */
    printf("Corrupted byte 2\n");
    
    if (!block_verify(&block)) {
        printf("Corruption detected!\n");
    }
    
    /* 配置存储 */
    printf("\n--- Configuration Storage ---\n");
    device_config_t config = {
        .device_name = "Sensor_001",
        .serial_number = 12345678,
        .config_version = 0x0102,
        .calibration_values = {1.0f, 2.0f, 3.0f, 4.0f}
    };
    
    config_save(&config);
    
    device_config_t loaded_config;
    if (config_load(&loaded_config)) {
        printf("Device: %s, SN: %u\n", 
               loaded_config.device_name, 
               loaded_config.serial_number);
    }
    
    /* 通信协议 */
    printf("\n--- Communication Protocol ---\n");
    protocol_frame_t frame;
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    frame_build(&frame, 0x10, payload, sizeof(payload));
    
    if (frame_verify(&frame)) {
        printf("Frame is valid\n");
    }
    
    /* 模拟传输错误 */
    printf("\nSimulating transmission error...\n");
    frame.payload[1] ^= 0x01;
    
    if (!frame_verify(&frame)) {
        printf("Transmission error detected!\n");
    }
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 3. 智能数据模式

### 架构图

```
+------------------------------------------------------------------+
|                    SMART DATA PATTERN                             |
+------------------------------------------------------------------+

    Traditional Data (No Checking):
    
    int value = 50;
    value = 999;    // No check! May be invalid.


    Smart Data (Self-Validating):
    
    +-------------------+
    |    Smart Data     |
    +-------------------+
    | - value           |
    | - min_limit       |
    | - max_limit       |
    | - valid           |
    +-------------------+
    | + set(v)          | --> Check range, set valid flag
    | + get()           | --> Return only if valid
    | + is_valid()      |
    +-------------------+


    Validation Chain:
    
    Input --> Range Check --> Consistency Check --> Set Value
              |                |                    |
              v                v                    v
           [Error]          [Error]              [Success]
              |                |                    |
              v                v                    v
         Mark Invalid     Mark Invalid         Mark Valid
```

**中文说明：**
- 智能数据模式将数据与其验证逻辑封装在一起
- 每次操作都自动检查数据的有效性
- 包括范围检查、合理性检查和一致性检查
- 确保数据始终处于有效状态

### 完整代码示例

```c
/*============================================================================
 * 智能数据模式示例 - 自验证数据类型
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/*---------------------------------------------------------------------------
 * 智能整数（带范围检查）
 *---------------------------------------------------------------------------*/
typedef struct {
    int32_t value;
    int32_t min_limit;
    int32_t max_limit;
    bool valid;
    const char *name;       /* 用于调试 */
} smart_int_t;

/* 关键点：初始化时设定范围 */
void smart_int_init(smart_int_t *si, const char *name, 
                    int32_t min, int32_t max, int32_t initial) {
    si->name = name;
    si->min_limit = min;
    si->max_limit = max;
    si->valid = false;
    
    /* 设置初始值（会验证） */
    if (initial >= min && initial <= max) {
        si->value = initial;
        si->valid = true;
    } else {
        si->value = min;  /* 使用最小值作为安全默认 */
        printf("[SmartInt] %s: Initial value %d out of range [%d, %d]\n",
               name, initial, min, max);
    }
}

/* 关键点：设置值时自动验证范围 */
bool smart_int_set(smart_int_t *si, int32_t value) {
    if (value < si->min_limit || value > si->max_limit) {
        printf("[SmartInt] %s: Value %d out of range [%d, %d] - REJECTED\n",
               si->name, value, si->min_limit, si->max_limit);
        si->valid = false;
        return false;
    }
    
    si->value = value;
    si->valid = true;
    return true;
}

/* 关键点：获取值时检查有效性 */
bool smart_int_get(const smart_int_t *si, int32_t *value) {
    if (!si->valid) {
        printf("[SmartInt] %s: Cannot get - data invalid!\n", si->name);
        return false;
    }
    *value = si->value;
    return true;
}

/* 强制获取（即使无效） */
int32_t smart_int_get_unsafe(const smart_int_t *si) {
    return si->value;
}

bool smart_int_is_valid(const smart_int_t *si) {
    /* 关键点：重新验证当前值 */
    return si->valid && 
           si->value >= si->min_limit && 
           si->value <= si->max_limit;
}

/* 增量操作（带溢出检查） */
bool smart_int_increment(smart_int_t *si, int32_t delta) {
    if (!si->valid) return false;
    
    int64_t new_value = (int64_t)si->value + delta;
    
    if (new_value < si->min_limit || new_value > si->max_limit) {
        printf("[SmartInt] %s: Increment would overflow\n", si->name);
        return false;
    }
    
    si->value = (int32_t)new_value;
    return true;
}

/*---------------------------------------------------------------------------
 * 智能浮点数（带范围和 NaN 检查）
 *---------------------------------------------------------------------------*/
typedef struct {
    float value;
    float min_limit;
    float max_limit;
    float max_rate_of_change;   /* 最大变化率 */
    float previous_value;
    bool valid;
    const char *name;
} smart_float_t;

void smart_float_init(smart_float_t *sf, const char *name,
                      float min, float max, float max_rate, float initial) {
    sf->name = name;
    sf->min_limit = min;
    sf->max_limit = max;
    sf->max_rate_of_change = max_rate;
    sf->previous_value = initial;
    sf->valid = false;
    
    /* 关键点：检查 NaN 和无穷大 */
    if (isnan(initial) || isinf(initial)) {
        printf("[SmartFloat] %s: Invalid initial value (NaN/Inf)\n", name);
        sf->value = min;
        return;
    }
    
    if (initial >= min && initial <= max) {
        sf->value = initial;
        sf->valid = true;
    }
}

/* 关键点：设置时进行多重检查 */
bool smart_float_set(smart_float_t *sf, float value) {
    /* 检查 NaN 和无穷大 */
    if (isnan(value) || isinf(value)) {
        printf("[SmartFloat] %s: NaN/Inf rejected\n", sf->name);
        sf->valid = false;
        return false;
    }
    
    /* 范围检查 */
    if (value < sf->min_limit || value > sf->max_limit) {
        printf("[SmartFloat] %s: %.3f out of range [%.3f, %.3f]\n",
               sf->name, value, sf->min_limit, sf->max_limit);
        sf->valid = false;
        return false;
    }
    
    /* 关键点：变化率检查（检测传感器故障） */
    if (sf->valid && sf->max_rate_of_change > 0) {
        float rate = fabsf(value - sf->previous_value);
        if (rate > sf->max_rate_of_change) {
            printf("[SmartFloat] %s: Rate of change %.3f exceeds limit %.3f\n",
                   sf->name, rate, sf->max_rate_of_change);
            sf->valid = false;
            return false;
        }
    }
    
    sf->previous_value = sf->value;
    sf->value = value;
    sf->valid = true;
    return true;
}

bool smart_float_get(const smart_float_t *sf, float *value) {
    if (!sf->valid) {
        printf("[SmartFloat] %s: Cannot get - data invalid!\n", sf->name);
        return false;
    }
    *value = sf->value;
    return true;
}

/*---------------------------------------------------------------------------
 * 智能枚举（带有效值检查）
 *---------------------------------------------------------------------------*/
typedef enum {
    MOTOR_STATE_STOPPED = 0,
    MOTOR_STATE_STARTING = 1,
    MOTOR_STATE_RUNNING = 2,
    MOTOR_STATE_STOPPING = 3,
    MOTOR_STATE_ERROR = 4,
    MOTOR_STATE_COUNT
} motor_state_t;

typedef struct {
    motor_state_t value;
    motor_state_t allowed_transitions[MOTOR_STATE_COUNT];
    bool transition_allowed[MOTOR_STATE_COUNT][MOTOR_STATE_COUNT];
    bool valid;
    const char *name;
} smart_motor_state_t;

/* 状态名称 */
const char* motor_state_name(motor_state_t state) {
    static const char *names[] = {
        "STOPPED", "STARTING", "RUNNING", "STOPPING", "ERROR"
    };
    return (state < MOTOR_STATE_COUNT) ? names[state] : "UNKNOWN";
}

void smart_motor_state_init(smart_motor_state_t *sms, const char *name) {
    sms->name = name;
    sms->value = MOTOR_STATE_STOPPED;
    sms->valid = true;
    
    /* 关键点：定义允许的状态转换 */
    /* 初始化所有转换为不允许 */
    for (int i = 0; i < MOTOR_STATE_COUNT; i++) {
        for (int j = 0; j < MOTOR_STATE_COUNT; j++) {
            sms->transition_allowed[i][j] = false;
        }
        sms->transition_allowed[i][i] = true;  /* 允许保持当前状态 */
    }
    
    /* 允许的转换 */
    sms->transition_allowed[MOTOR_STATE_STOPPED][MOTOR_STATE_STARTING] = true;
    sms->transition_allowed[MOTOR_STATE_STOPPED][MOTOR_STATE_ERROR] = true;
    sms->transition_allowed[MOTOR_STATE_STARTING][MOTOR_STATE_RUNNING] = true;
    sms->transition_allowed[MOTOR_STATE_STARTING][MOTOR_STATE_ERROR] = true;
    sms->transition_allowed[MOTOR_STATE_STARTING][MOTOR_STATE_STOPPED] = true;
    sms->transition_allowed[MOTOR_STATE_RUNNING][MOTOR_STATE_STOPPING] = true;
    sms->transition_allowed[MOTOR_STATE_RUNNING][MOTOR_STATE_ERROR] = true;
    sms->transition_allowed[MOTOR_STATE_STOPPING][MOTOR_STATE_STOPPED] = true;
    sms->transition_allowed[MOTOR_STATE_STOPPING][MOTOR_STATE_ERROR] = true;
    sms->transition_allowed[MOTOR_STATE_ERROR][MOTOR_STATE_STOPPED] = true;
}

/* 关键点：状态转换时验证合法性 */
bool smart_motor_state_set(smart_motor_state_t *sms, motor_state_t new_state) {
    /* 检查枚举值有效性 */
    if (new_state >= MOTOR_STATE_COUNT) {
        printf("[SmartState] %s: Invalid state value %d\n", sms->name, new_state);
        sms->valid = false;
        return false;
    }
    
    /* 关键点：检查转换是否允许 */
    if (!sms->transition_allowed[sms->value][new_state]) {
        printf("[SmartState] %s: Transition %s -> %s NOT ALLOWED\n",
               sms->name,
               motor_state_name(sms->value),
               motor_state_name(new_state));
        return false;
    }
    
    printf("[SmartState] %s: %s -> %s\n",
           sms->name,
           motor_state_name(sms->value),
           motor_state_name(new_state));
    
    sms->value = new_state;
    return true;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void smart_data_example(void) {
    printf("=== Smart Data Pattern Demo ===\n\n");
    
    /* 智能整数 */
    printf("--- Smart Integer ---\n");
    smart_int_t speed;
    smart_int_init(&speed, "MotorSpeed", 0, 3000, 0);
    
    smart_int_set(&speed, 1500);        /* 有效 */
    smart_int_set(&speed, 5000);        /* 超出范围 */
    smart_int_set(&speed, -100);        /* 负值，无效 */
    
    int32_t current_speed;
    if (smart_int_get(&speed, &current_speed)) {
        printf("Current speed: %d RPM\n", current_speed);
    }
    
    /* 智能浮点数 */
    printf("\n--- Smart Float ---\n");
    smart_float_t temperature;
    smart_float_init(&temperature, "Temperature", -40.0f, 125.0f, 10.0f, 25.0f);
    
    smart_float_set(&temperature, 30.0f);   /* 有效 */
    smart_float_set(&temperature, 32.0f);   /* 有效 */
    smart_float_set(&temperature, 100.0f);  /* 变化太快！ */
    smart_float_set(&temperature, 150.0f);  /* 超出范围 */
    
    /* 尝试设置 NaN */
    smart_float_set(&temperature, NAN);
    
    float temp_value;
    if (smart_float_get(&temperature, &temp_value)) {
        printf("Current temperature: %.1f°C\n", temp_value);
    }
    
    /* 智能状态 */
    printf("\n--- Smart Motor State ---\n");
    smart_motor_state_t motor;
    smart_motor_state_init(&motor, "Motor1");
    
    smart_motor_state_set(&motor, MOTOR_STATE_STARTING);    /* 有效 */
    smart_motor_state_set(&motor, MOTOR_STATE_RUNNING);     /* 有效 */
    smart_motor_state_set(&motor, MOTOR_STATE_STOPPED);     /* 无效！必须先 STOPPING */
    smart_motor_state_set(&motor, MOTOR_STATE_STOPPING);    /* 有效 */
    smart_motor_state_set(&motor, MOTOR_STATE_STOPPED);     /* 有效 */
    
    /* 尝试无效的枚举值 */
    smart_motor_state_set(&motor, (motor_state_t)99);
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 4. 通道模式

### 架构图

```
+------------------------------------------------------------------+
|                      CHANNEL PATTERN                              |
+------------------------------------------------------------------+

    Processing Pipeline (Single Channel):
    
    +--------+   +--------+   +--------+   +--------+   +--------+
    | Sensor |-->| Filter |-->|Process |-->| Check  |-->|Actuator|
    +--------+   +--------+   +--------+   +--------+   +--------+
         |            |            |            |            |
    Raw Data    Filtered     Processed    Validated     Output
    Acquisition   Data          Data         Data       Action


    Channel Structure:
    
    +----------------------------------------------------------+
    |                    CHANNEL                                |
    |  +----------+  +----------+  +----------+  +----------+  |
    |  | Stage 1  |->| Stage 2  |->| Stage 3  |->| Stage 4  |  |
    |  | (Input)  |  | (Process)|  | (Verify) |  | (Output) |  |
    |  +----------+  +----------+  +----------+  +----------+  |
    |       ^                                          |       |
    |       |                                          |       |
    |   [Sensor]                                  [Actuator]   |
    +----------------------------------------------------------+


    Data Transformation:
    
    Raw ADC --> Calibration --> Scaling --> Limit Check --> Output
    (0-4095)     (+offset)     (*factor)   (min/max)     (float)
```

**中文说明：**
- 通道模式将处理过程组织为数据转换的流水线
- 每个阶段执行特定的数据转换或验证
- 提供独立、自包含的功能单元
- 是构建冗余系统的基础

### 完整代码示例

```c
/*============================================================================
 * 通道模式示例 - 传感器数据处理通道
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/*---------------------------------------------------------------------------
 * 通道阶段定义
 *---------------------------------------------------------------------------*/
typedef enum {
    STAGE_OK,
    STAGE_ERROR,
    STAGE_OUT_OF_RANGE,
    STAGE_SENSOR_FAULT
} stage_status_t;

/* 通道数据包 */
typedef struct {
    float value;
    uint32_t timestamp;
    stage_status_t status;
    uint8_t stage_passed;       /* 已通过的阶段数 */
    uint32_t error_code;
} channel_data_t;

/* 阶段处理函数类型 */
typedef stage_status_t (*stage_fn)(channel_data_t *data, void *context);

/* 阶段定义 */
typedef struct {
    const char *name;
    stage_fn process;
    void *context;
} channel_stage_t;

/*---------------------------------------------------------------------------
 * 通道结构
 *---------------------------------------------------------------------------*/
#define MAX_STAGES 8

typedef struct {
    const char *name;
    channel_stage_t stages[MAX_STAGES];
    int stage_count;
    
    /* 统计 */
    uint32_t total_processed;
    uint32_t total_errors;
    uint32_t stage_errors[MAX_STAGES];
} channel_t;

void channel_init(channel_t *ch, const char *name) {
    ch->name = name;
    ch->stage_count = 0;
    ch->total_processed = 0;
    ch->total_errors = 0;
    
    for (int i = 0; i < MAX_STAGES; i++) {
        ch->stage_errors[i] = 0;
    }
}

void channel_add_stage(channel_t *ch, const char *name, 
                        stage_fn process, void *context) {
    if (ch->stage_count >= MAX_STAGES) return;
    
    channel_stage_t *stage = &ch->stages[ch->stage_count];
    stage->name = name;
    stage->process = process;
    stage->context = context;
    ch->stage_count++;
    
    printf("[Channel] %s: Added stage '%s'\n", ch->name, name);
}

/* 关键点：数据流经所有阶段 */
stage_status_t channel_process(channel_t *ch, channel_data_t *data) {
    data->stage_passed = 0;
    data->status = STAGE_OK;
    data->error_code = 0;
    
    /* 关键点：顺序执行每个阶段 */
    for (int i = 0; i < ch->stage_count; i++) {
        channel_stage_t *stage = &ch->stages[i];
        
        stage_status_t result = stage->process(data, stage->context);
        
        if (result != STAGE_OK) {
            printf("[Channel] %s: Stage '%s' failed with status %d\n",
                   ch->name, stage->name, result);
            data->status = result;
            ch->stage_errors[i]++;
            ch->total_errors++;
            return result;
        }
        
        data->stage_passed++;
    }
    
    ch->total_processed++;
    return STAGE_OK;
}

/*---------------------------------------------------------------------------
 * 具体阶段实现
 *---------------------------------------------------------------------------*/

/* 阶段1：ADC 采集 */
typedef struct {
    uint16_t *adc_value;
    float adc_max;
    float vref;
} adc_stage_ctx_t;

stage_status_t stage_adc_acquire(channel_data_t *data, void *context) {
    adc_stage_ctx_t *ctx = (adc_stage_ctx_t *)context;
    
    /* 转换 ADC 值到电压 */
    data->value = (*ctx->adc_value / ctx->adc_max) * ctx->vref;
    
    printf("  [ADC] Raw: %u -> %.3f V\n", *ctx->adc_value, data->value);
    return STAGE_OK;
}

/* 阶段2：校准 */
typedef struct {
    float offset;
    float gain;
} calibration_ctx_t;

stage_status_t stage_calibrate(channel_data_t *data, void *context) {
    calibration_ctx_t *ctx = (calibration_ctx_t *)context;
    
    float calibrated = (data->value - ctx->offset) * ctx->gain;
    
    printf("  [Calibrate] %.3f -> %.3f\n", data->value, calibrated);
    
    data->value = calibrated;
    return STAGE_OK;
}

/* 阶段3：滤波 */
typedef struct {
    float alpha;        /* 滤波系数 */
    float last_value;
    bool initialized;
} filter_ctx_t;

stage_status_t stage_filter(channel_data_t *data, void *context) {
    filter_ctx_t *ctx = (filter_ctx_t *)context;
    
    if (!ctx->initialized) {
        ctx->last_value = data->value;
        ctx->initialized = true;
    }
    
    /* 关键点：低通滤波 */
    float filtered = ctx->alpha * data->value + 
                     (1.0f - ctx->alpha) * ctx->last_value;
    
    printf("  [Filter] %.3f -> %.3f (alpha=%.2f)\n", 
           data->value, filtered, ctx->alpha);
    
    ctx->last_value = filtered;
    data->value = filtered;
    return STAGE_OK;
}

/* 阶段4：范围检查 */
typedef struct {
    float min_value;
    float max_value;
} range_check_ctx_t;

stage_status_t stage_range_check(channel_data_t *data, void *context) {
    range_check_ctx_t *ctx = (range_check_ctx_t *)context;
    
    printf("  [RangeCheck] %.3f in [%.3f, %.3f]? ",
           data->value, ctx->min_value, ctx->max_value);
    
    if (data->value < ctx->min_value) {
        printf("TOO LOW!\n");
        data->error_code = 1;
        return STAGE_OUT_OF_RANGE;
    }
    
    if (data->value > ctx->max_value) {
        printf("TOO HIGH!\n");
        data->error_code = 2;
        return STAGE_OUT_OF_RANGE;
    }
    
    printf("OK\n");
    return STAGE_OK;
}

/* 阶段5：合理性检查 */
typedef struct {
    float max_rate;
    float last_value;
    bool initialized;
} plausibility_ctx_t;

stage_status_t stage_plausibility(channel_data_t *data, void *context) {
    plausibility_ctx_t *ctx = (plausibility_ctx_t *)context;
    
    if (!ctx->initialized) {
        ctx->last_value = data->value;
        ctx->initialized = true;
        printf("  [Plausibility] First sample, OK\n");
        return STAGE_OK;
    }
    
    float rate = fabsf(data->value - ctx->last_value);
    
    printf("  [Plausibility] Rate: %.3f (max: %.3f) ",
           rate, ctx->max_rate);
    
    if (rate > ctx->max_rate) {
        printf("SENSOR FAULT!\n");
        data->error_code = 3;
        return STAGE_SENSOR_FAULT;
    }
    
    printf("OK\n");
    ctx->last_value = data->value;
    return STAGE_OK;
}

/* 阶段6：单位转换 */
typedef struct {
    float scale;
    float offset;
    const char *unit;
} unit_convert_ctx_t;

stage_status_t stage_unit_convert(channel_data_t *data, void *context) {
    unit_convert_ctx_t *ctx = (unit_convert_ctx_t *)context;
    
    float converted = data->value * ctx->scale + ctx->offset;
    
    printf("  [UnitConvert] %.3f -> %.3f %s\n",
           data->value, converted, ctx->unit);
    
    data->value = converted;
    return STAGE_OK;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void channel_example(void) {
    printf("=== Channel Pattern Demo ===\n\n");
    
    /* 创建温度传感器通道 */
    channel_t temp_channel;
    channel_init(&temp_channel, "Temperature");
    
    /* 阶段上下文 */
    static uint16_t adc_value = 2048;
    static adc_stage_ctx_t adc_ctx = {
        .adc_value = &adc_value,
        .adc_max = 4095.0f,
        .vref = 3.3f
    };
    
    static calibration_ctx_t cal_ctx = {
        .offset = 0.1f,
        .gain = 1.02f
    };
    
    static filter_ctx_t filter_ctx = {
        .alpha = 0.3f,
        .initialized = false
    };
    
    static range_check_ctx_t range_ctx = {
        .min_value = 0.0f,
        .max_value = 3.0f
    };
    
    static plausibility_ctx_t plaus_ctx = {
        .max_rate = 0.5f,
        .initialized = false
    };
    
    static unit_convert_ctx_t unit_ctx = {
        .scale = 100.0f,    /* V to °C */
        .offset = -50.0f,
        .unit = "°C"
    };
    
    /* 关键点：构建处理流水线 */
    channel_add_stage(&temp_channel, "ADC Acquire", stage_adc_acquire, &adc_ctx);
    channel_add_stage(&temp_channel, "Calibrate", stage_calibrate, &cal_ctx);
    channel_add_stage(&temp_channel, "Filter", stage_filter, &filter_ctx);
    channel_add_stage(&temp_channel, "Range Check", stage_range_check, &range_ctx);
    channel_add_stage(&temp_channel, "Plausibility", stage_plausibility, &plaus_ctx);
    channel_add_stage(&temp_channel, "Unit Convert", stage_unit_convert, &unit_ctx);
    
    printf("\n--- Processing Normal Samples ---\n");
    
    /* 模拟多次采样 */
    uint16_t samples[] = {2048, 2100, 2150, 2200, 2180};
    
    for (int i = 0; i < 5; i++) {
        printf("\n[Sample %d] ADC = %u\n", i, samples[i]);
        adc_value = samples[i];
        
        channel_data_t data = {0};
        data.timestamp = i * 100;
        
        if (channel_process(&temp_channel, &data) == STAGE_OK) {
            printf("  Result: %.2f °C\n", data.value);
        }
    }
    
    /* 模拟传感器故障（值跳变） */
    printf("\n--- Simulating Sensor Fault ---\n");
    adc_value = 4000;  /* 突然跳变 */
    
    channel_data_t fault_data = {0};
    if (channel_process(&temp_channel, &fault_data) != STAGE_OK) {
        printf("  Fault detected at stage %d, error code: %u\n",
               fault_data.stage_passed, fault_data.error_code);
    }
    
    /* 模拟超出范围 */
    printf("\n--- Simulating Out of Range ---\n");
    
    /* 重置 plausibility */
    plaus_ctx.initialized = false;
    adc_value = 4090;  /* 高值 */
    
    channel_data_t oor_data = {0};
    if (channel_process(&temp_channel, &oor_data) != STAGE_OK) {
        printf("  Out of range at stage %d\n", oor_data.stage_passed);
    }
    
    /* 统计 */
    printf("\n=== Channel Statistics ===\n");
    printf("  Total processed: %u\n", temp_channel.total_processed);
    printf("  Total errors: %u\n", temp_channel.total_errors);
    for (int i = 0; i < temp_channel.stage_count; i++) {
        printf("  Stage '%s': %u errors\n",
               temp_channel.stages[i].name,
               temp_channel.stage_errors[i]);
    }
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 5. 受保护单通道模式

### 架构图

```
+------------------------------------------------------------------+
|              PROTECTED SINGLE CHANNEL PATTERN                     |
+------------------------------------------------------------------+

    Enhanced Channel with Internal Checks:
    
    +--------------------------------------------------------------+
    |                   PROTECTED CHANNEL                           |
    |                                                               |
    |  +-------+   +--------+   +-------+   +--------+   +-------+  |
    |  | Input |-->| Check1 |-->|Process|-->| Check2 |-->|Output |  |
    |  +-------+   +--------+   +-------+   +--------+   +-------+  |
    |      |           |            |            |            |     |
    |      v           v            v            v            v     |
    |  [Validate]  [Verify]    [Monitor]    [Verify]    [Limit]    |
    |                                                               |
    +--------------------------------------------------------------+
                          |
                          v
                   +------------+
                   | SAFE STATE |
                   +------------+
    

    Protection Points:
    
    1. Input Validation (range, type, timing)
    2. Processing Watchdog (timeout, stuck detection)
    3. Output Verification (expected vs actual)
    4. Output Limiting (clamp to safe values)
```

**中文说明：**
- 受保护单通道模式在处理链的关键检查点添加验证
- 包括范围检查、合理性检查、数据有效性检查等
- 检测到故障时进入故障安全状态
- 适用于只需检测故障而不需要完全冗余的系统

### 完整代码示例

```c
/*============================================================================
 * 受保护单通道模式示例 - 增强安全性的处理通道
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/*---------------------------------------------------------------------------
 * 保护检查类型
 *---------------------------------------------------------------------------*/
typedef enum {
    CHECK_RANGE,            /* 范围检查 */
    CHECK_RATE,             /* 变化率检查 */
    CHECK_STUCK,            /* 卡住检测 */
    CHECK_TIMING,           /* 时序检查 */
    CHECK_CONSISTENCY       /* 一致性检查 */
} check_type_t;

typedef struct {
    check_type_t type;
    const char *name;
    bool enabled;
    uint32_t fail_count;
    uint32_t total_checks;
} protection_check_t;

/*---------------------------------------------------------------------------
 * 受保护通道结构
 *---------------------------------------------------------------------------*/
#define MAX_PROTECTIONS 10

typedef enum {
    CHANNEL_NORMAL,
    CHANNEL_WARNING,
    CHANNEL_FAULT,
    CHANNEL_SAFE_STATE
} channel_health_t;

typedef struct {
    const char *name;
    channel_health_t health;
    
    /* 输入保护 */
    float input_min;
    float input_max;
    float input_max_rate;
    
    /* 输出保护 */
    float output_min;
    float output_max;
    float safe_output;      /* 故障安全输出值 */
    
    /* 处理参数 */
    float gain;
    float offset;
    
    /* 状态跟踪 */
    float last_input;
    float last_output;
    uint32_t last_update_time;
    uint32_t stuck_counter;
    
    /* 保护检查 */
    protection_check_t checks[MAX_PROTECTIONS];
    int check_count;
    
    /* 故障阈值 */
    uint32_t warning_threshold;
    uint32_t fault_threshold;
    uint32_t consecutive_failures;
    
    /* 统计 */
    uint32_t total_samples;
    uint32_t warnings;
    uint32_t faults;
} protected_channel_t;

/*---------------------------------------------------------------------------
 * 初始化
 *---------------------------------------------------------------------------*/
void protected_channel_init(protected_channel_t *ch, const char *name) {
    ch->name = name;
    ch->health = CHANNEL_NORMAL;
    
    ch->input_min = -1000.0f;
    ch->input_max = 1000.0f;
    ch->input_max_rate = 100.0f;
    
    ch->output_min = 0.0f;
    ch->output_max = 100.0f;
    ch->safe_output = 0.0f;
    
    ch->gain = 1.0f;
    ch->offset = 0.0f;
    
    ch->last_input = 0.0f;
    ch->last_output = 0.0f;
    ch->last_update_time = 0;
    ch->stuck_counter = 0;
    
    ch->check_count = 0;
    ch->warning_threshold = 3;
    ch->fault_threshold = 5;
    ch->consecutive_failures = 0;
    
    ch->total_samples = 0;
    ch->warnings = 0;
    ch->faults = 0;
}

void protected_channel_add_check(protected_channel_t *ch, 
                                  check_type_t type, const char *name) {
    if (ch->check_count >= MAX_PROTECTIONS) return;
    
    protection_check_t *check = &ch->checks[ch->check_count++];
    check->type = type;
    check->name = name;
    check->enabled = true;
    check->fail_count = 0;
    check->total_checks = 0;
}

/*---------------------------------------------------------------------------
 * 保护检查实现
 *---------------------------------------------------------------------------*/
/* 关键点：输入范围检查 */
bool check_input_range(protected_channel_t *ch, float input) {
    if (input < ch->input_min || input > ch->input_max) {
        printf("  [CHECK] Input range: %.2f NOT in [%.2f, %.2f] - FAIL\n",
               input, ch->input_min, ch->input_max);
        return false;
    }
    return true;
}

/* 关键点：变化率检查 */
bool check_input_rate(protected_channel_t *ch, float input) {
    float rate = fabsf(input - ch->last_input);
    
    if (rate > ch->input_max_rate) {
        printf("  [CHECK] Rate: %.2f > %.2f - FAIL\n", rate, ch->input_max_rate);
        return false;
    }
    return true;
}

/* 关键点：卡住检测 */
bool check_stuck(protected_channel_t *ch, float input) {
    const float epsilon = 0.001f;
    const uint32_t stuck_limit = 10;
    
    if (fabsf(input - ch->last_input) < epsilon) {
        ch->stuck_counter++;
        if (ch->stuck_counter > stuck_limit) {
            printf("  [CHECK] Stuck detected: %u samples unchanged - FAIL\n",
                   ch->stuck_counter);
            return false;
        }
    } else {
        ch->stuck_counter = 0;
    }
    return true;
}

/* 关键点：时序检查 */
bool check_timing(protected_channel_t *ch, uint32_t current_time) {
    const uint32_t max_interval = 1000;  /* 最大允许间隔 */
    
    if (ch->last_update_time > 0) {
        uint32_t interval = current_time - ch->last_update_time;
        if (interval > max_interval) {
            printf("  [CHECK] Timing: interval %u > %u - FAIL\n",
                   interval, max_interval);
            return false;
        }
    }
    return true;
}

/* 关键点：输出范围限制 */
float limit_output(protected_channel_t *ch, float output) {
    if (output < ch->output_min) {
        printf("  [LIMIT] Output clamped: %.2f -> %.2f\n", output, ch->output_min);
        return ch->output_min;
    }
    if (output > ch->output_max) {
        printf("  [LIMIT] Output clamped: %.2f -> %.2f\n", output, ch->output_max);
        return ch->output_max;
    }
    return output;
}

/*---------------------------------------------------------------------------
 * 核心处理函数
 *---------------------------------------------------------------------------*/
/* 关键点：进入安全状态 */
void enter_safe_state(protected_channel_t *ch) {
    ch->health = CHANNEL_SAFE_STATE;
    ch->last_output = ch->safe_output;
    ch->faults++;
    
    printf("  [SAFE] Channel '%s' entered SAFE STATE, output = %.2f\n",
           ch->name, ch->safe_output);
}

/* 关键点：受保护的处理 */
float protected_channel_process(protected_channel_t *ch, float input, 
                                  uint32_t timestamp) {
    ch->total_samples++;
    
    printf("\n[%s] Processing input: %.2f (sample #%u)\n",
           ch->name, input, ch->total_samples);
    
    /* 如果已在安全状态，返回安全值 */
    if (ch->health == CHANNEL_SAFE_STATE) {
        printf("  Channel in SAFE STATE, returning safe output\n");
        return ch->safe_output;
    }
    
    bool all_passed = true;
    
    /* 关键点：执行所有保护检查 */
    for (int i = 0; i < ch->check_count; i++) {
        protection_check_t *check = &ch->checks[i];
        if (!check->enabled) continue;
        
        bool passed = true;
        check->total_checks++;
        
        switch (check->type) {
            case CHECK_RANGE:
                passed = check_input_range(ch, input);
                break;
            case CHECK_RATE:
                passed = check_input_rate(ch, input);
                break;
            case CHECK_STUCK:
                passed = check_stuck(ch, input);
                break;
            case CHECK_TIMING:
                passed = check_timing(ch, timestamp);
                break;
            default:
                break;
        }
        
        if (!passed) {
            check->fail_count++;
            all_passed = false;
        }
    }
    
    /* 更新健康状态 */
    if (!all_passed) {
        ch->consecutive_failures++;
        
        if (ch->consecutive_failures >= ch->fault_threshold) {
            enter_safe_state(ch);
            return ch->safe_output;
        } else if (ch->consecutive_failures >= ch->warning_threshold) {
            ch->health = CHANNEL_WARNING;
            ch->warnings++;
            printf("  [WARNING] Consecutive failures: %u\n", 
                   ch->consecutive_failures);
        }
    } else {
        ch->consecutive_failures = 0;
        if (ch->health == CHANNEL_WARNING) {
            ch->health = CHANNEL_NORMAL;
            printf("  [RECOVERED] Channel back to NORMAL\n");
        }
    }
    
    /* 处理 */
    float output = input * ch->gain + ch->offset;
    
    /* 关键点：输出限制 */
    output = limit_output(ch, output);
    
    /* 更新状态 */
    ch->last_input = input;
    ch->last_output = output;
    ch->last_update_time = timestamp;
    
    printf("  Result: %.2f (health: %d)\n", output, ch->health);
    
    return output;
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void protected_channel_example(void) {
    printf("=== Protected Single Channel Pattern Demo ===\n\n");
    
    /* 创建受保护通道 */
    protected_channel_t motor_cmd;
    protected_channel_init(&motor_cmd, "MotorCommand");
    
    /* 配置 */
    motor_cmd.input_min = 0.0f;
    motor_cmd.input_max = 100.0f;
    motor_cmd.input_max_rate = 20.0f;
    motor_cmd.output_min = 0.0f;
    motor_cmd.output_max = 1000.0f;
    motor_cmd.safe_output = 0.0f;
    motor_cmd.gain = 10.0f;
    motor_cmd.offset = 0.0f;
    
    motor_cmd.warning_threshold = 2;
    motor_cmd.fault_threshold = 4;
    
    /* 添加保护检查 */
    protected_channel_add_check(&motor_cmd, CHECK_RANGE, "InputRange");
    protected_channel_add_check(&motor_cmd, CHECK_RATE, "RateLimit");
    protected_channel_add_check(&motor_cmd, CHECK_STUCK, "StuckDetect");
    protected_channel_add_check(&motor_cmd, CHECK_TIMING, "Timing");
    
    printf("--- Normal Operation ---\n");
    
    uint32_t time = 0;
    float inputs[] = {10.0f, 15.0f, 20.0f, 25.0f, 30.0f};
    
    for (int i = 0; i < 5; i++) {
        protected_channel_process(&motor_cmd, inputs[i], time);
        time += 100;
    }
    
    printf("\n--- Rate Violation ---\n");
    protected_channel_process(&motor_cmd, 80.0f, time);  /* 跳变太大 */
    time += 100;
    
    printf("\n--- Range Violation ---\n");
    protected_channel_process(&motor_cmd, 150.0f, time);  /* 超出范围 */
    time += 100;
    
    printf("\n--- Continued Violations (trigger safe state) ---\n");
    for (int i = 0; i < 5; i++) {
        protected_channel_process(&motor_cmd, 200.0f, time);  /* 持续超范围 */
        time += 100;
    }
    
    /* 尝试在安全状态下处理 */
    printf("\n--- Processing in Safe State ---\n");
    protected_channel_process(&motor_cmd, 50.0f, time);
    
    /* 统计 */
    printf("\n=== Channel Statistics ===\n");
    printf("  Total samples: %u\n", motor_cmd.total_samples);
    printf("  Warnings: %u\n", motor_cmd.warnings);
    printf("  Faults: %u\n", motor_cmd.faults);
    printf("  Final health: %d\n", motor_cmd.health);
    
    for (int i = 0; i < motor_cmd.check_count; i++) {
        printf("  Check '%s': %u/%u failures\n",
               motor_cmd.checks[i].name,
               motor_cmd.checks[i].fail_count,
               motor_cmd.checks[i].total_checks);
    }
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 6. 双通道模式

### 架构图

```
+------------------------------------------------------------------+
|                    DUAL CHANNEL PATTERN                           |
+------------------------------------------------------------------+

    Homogeneous Redundancy (Same Implementation):
    
    Input ----+----> [Channel A] ----+
              |                      |
              |                      v
              |              +---------------+
              |              |   Comparator  |---> Output
              |              +---------------+
              |                      ^
              |                      |
              +----> [Channel B] ----+


    Heterogeneous Redundancy (Different Implementation):
    
    Input ----+----> [Channel A: Algo 1] ----+
              |                              |
              |                              v
              |                      +---------------+
              |                      |   Comparator  |---> Output
              |                      |   & Voter     |
              |                      +---------------+
              |                              ^
              |                              |
              +----> [Channel B: Algo 2] ----+


    Failover Strategy:
    
    Channel A: Active    Channel B: Standby
         |                    |
         v                    |
    [Processing]              |
         |                    |
    (Fault detected)          |
         |                    |
         v                    v
    Channel A: Failed    Channel B: Active
         X                    |
                              v
                         [Processing]
```

**中文说明：**
- 双通道模式通过复制通道实现架构冗余
- 同质冗余：两个通道使用相同实现
- 异质冗余：两个通道使用不同实现
- 可以检测故障并可选地继续提供服务

### 完整代码示例

```c
/*============================================================================
 * 双通道模式示例 - 冗余处理系统
 *============================================================================*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

/*---------------------------------------------------------------------------
 * 通道定义
 *---------------------------------------------------------------------------*/
typedef enum {
    CH_STATUS_OK,
    CH_STATUS_DEGRADED,
    CH_STATUS_FAILED
} channel_status_t;

typedef float (*process_fn)(float input, void *context);

typedef struct {
    const char *name;
    channel_status_t status;
    process_fn process;
    void *context;
    
    float last_output;
    uint32_t error_count;
    uint32_t total_processed;
    bool active;
} redundant_channel_t;

/*---------------------------------------------------------------------------
 * 双通道系统
 *---------------------------------------------------------------------------*/
typedef enum {
    DUAL_MODE_COMPARE,      /* 比较模式：两通道必须一致 */
    DUAL_MODE_PRIMARY,      /* 主备模式：主通道优先 */
    DUAL_MODE_AVERAGE       /* 平均模式：取两通道平均值 */
} dual_channel_mode_t;

typedef struct {
    const char *name;
    redundant_channel_t channel_a;
    redundant_channel_t channel_b;
    
    dual_channel_mode_t mode;
    float tolerance;            /* 比较容差 */
    
    bool both_active;           /* 两通道都在运行 */
    uint32_t mismatch_count;
    uint32_t failover_count;
    
    float last_output;
    bool output_valid;
} dual_channel_system_t;

/*---------------------------------------------------------------------------
 * 初始化
 *---------------------------------------------------------------------------*/
void channel_init(redundant_channel_t *ch, const char *name,
                  process_fn process, void *context) {
    ch->name = name;
    ch->status = CH_STATUS_OK;
    ch->process = process;
    ch->context = context;
    ch->last_output = 0.0f;
    ch->error_count = 0;
    ch->total_processed = 0;
    ch->active = true;
}

void dual_system_init(dual_channel_system_t *sys, const char *name,
                      process_fn process_a, void *ctx_a,
                      process_fn process_b, void *ctx_b) {
    sys->name = name;
    
    channel_init(&sys->channel_a, "Channel_A", process_a, ctx_a);
    channel_init(&sys->channel_b, "Channel_B", process_b, ctx_b);
    
    sys->mode = DUAL_MODE_COMPARE;
    sys->tolerance = 0.01f;
    sys->both_active = true;
    sys->mismatch_count = 0;
    sys->failover_count = 0;
    sys->last_output = 0.0f;
    sys->output_valid = true;
}

/*---------------------------------------------------------------------------
 * 通道处理函数示例
 *---------------------------------------------------------------------------*/
/* 通道 A：标准算法 */
typedef struct {
    float gain;
    float offset;
} algo_standard_ctx_t;

float algo_standard(float input, void *context) {
    algo_standard_ctx_t *ctx = (algo_standard_ctx_t *)context;
    return input * ctx->gain + ctx->offset;
}

/* 通道 B：替代算法（异质冗余） */
typedef struct {
    float coefficients[3];  /* 多项式系数 */
} algo_polynomial_ctx_t;

float algo_polynomial(float input, void *context) {
    algo_polynomial_ctx_t *ctx = (algo_polynomial_ctx_t *)context;
    /* y = a0 + a1*x + a2*x^2 */
    return ctx->coefficients[0] + 
           ctx->coefficients[1] * input +
           ctx->coefficients[2] * input * input;
}

/* 同质冗余版本 */
float algo_standard_v2(float input, void *context) {
    /* 相同算法，但独立实现 */
    algo_standard_ctx_t *ctx = (algo_standard_ctx_t *)context;
    float result = input;
    result = result * ctx->gain;
    result = result + ctx->offset;
    return result;
}

/*---------------------------------------------------------------------------
 * 双通道处理
 *---------------------------------------------------------------------------*/
/* 关键点：处理单个通道 */
float process_channel(redundant_channel_t *ch, float input) {
    if (ch->status == CH_STATUS_FAILED || !ch->active) {
        return ch->last_output;  /* 返回上次有效值 */
    }
    
    float output = ch->process(input, ch->context);
    ch->last_output = output;
    ch->total_processed++;
    
    return output;
}

/* 关键点：比较两通道输出 */
bool compare_outputs(dual_channel_system_t *sys, float out_a, float out_b) {
    float diff = fabsf(out_a - out_b);
    float max_val = fmaxf(fabsf(out_a), fabsf(out_b));
    
    if (max_val < 0.001f) {
        return diff < sys->tolerance;
    }
    
    float relative_diff = diff / max_val;
    return relative_diff < sys->tolerance;
}

/* 关键点：双通道系统处理 */
float dual_system_process(dual_channel_system_t *sys, float input) {
    float output_a = process_channel(&sys->channel_a, input);
    float output_b = process_channel(&sys->channel_b, input);
    
    printf("[%s] Input: %.3f -> A: %.3f, B: %.3f\n",
           sys->name, input, output_a, output_b);
    
    /* 检查通道状态 */
    bool a_ok = sys->channel_a.status != CH_STATUS_FAILED && sys->channel_a.active;
    bool b_ok = sys->channel_b.status != CH_STATUS_FAILED && sys->channel_b.active;
    
    if (!a_ok && !b_ok) {
        /* 关键点：两通道都失效 */
        printf("  [CRITICAL] Both channels failed!\n");
        sys->output_valid = false;
        return sys->last_output;  /* 返回最后已知的好值 */
    }
    
    float output;
    
    switch (sys->mode) {
        case DUAL_MODE_COMPARE:
            /* 关键点：比较模式 - 两通道必须一致 */
            if (a_ok && b_ok) {
                if (compare_outputs(sys, output_a, output_b)) {
                    output = (output_a + output_b) / 2.0f;
                    printf("  [COMPARE] Match, output: %.3f\n", output);
                } else {
                    sys->mismatch_count++;
                    printf("  [COMPARE] MISMATCH! Count: %u\n", sys->mismatch_count);
                    
                    /* 选择一个输出（或使用其他策略） */
                    output = output_a;  /* 默认信任 A */
                    
                    /* 标记可能有问题的通道 */
                    if (sys->mismatch_count > 3) {
                        sys->channel_b.status = CH_STATUS_DEGRADED;
                    }
                }
            } else {
                /* 单通道运行 */
                output = a_ok ? output_a : output_b;
                printf("  [COMPARE] Single channel: %.3f\n", output);
            }
            break;
            
        case DUAL_MODE_PRIMARY:
            /* 关键点：主备模式 */
            if (a_ok) {
                output = output_a;
                printf("  [PRIMARY] Using Channel A: %.3f\n", output);
            } else {
                output = output_b;
                sys->failover_count++;
                printf("  [FAILOVER] Using Channel B: %.3f (failover #%u)\n",
                       output, sys->failover_count);
            }
            break;
            
        case DUAL_MODE_AVERAGE:
            /* 关键点：平均模式 */
            if (a_ok && b_ok) {
                output = (output_a + output_b) / 2.0f;
                printf("  [AVERAGE] %.3f\n", output);
            } else {
                output = a_ok ? output_a : output_b;
                printf("  [SINGLE] %.3f\n", output);
            }
            break;
            
        default:
            output = output_a;
    }
    
    sys->last_output = output;
    sys->output_valid = true;
    
    return output;
}

/*---------------------------------------------------------------------------
 * 故障注入和恢复
 *---------------------------------------------------------------------------*/
void channel_inject_fault(redundant_channel_t *ch) {
    ch->status = CH_STATUS_FAILED;
    ch->active = false;
    ch->error_count++;
    printf("[FAULT] %s marked as FAILED\n", ch->name);
}

void channel_recover(redundant_channel_t *ch) {
    ch->status = CH_STATUS_OK;
    ch->active = true;
    printf("[RECOVER] %s recovered to OK\n", ch->name);
}

/*---------------------------------------------------------------------------
 * 使用示例
 *---------------------------------------------------------------------------*/
void dual_channel_example(void) {
    printf("=== Dual Channel Pattern Demo ===\n\n");
    
    /* 创建处理上下文 */
    algo_standard_ctx_t ctx_a = {.gain = 2.0f, .offset = 10.0f};
    algo_standard_ctx_t ctx_b = {.gain = 2.0f, .offset = 10.0f};
    
    /* 创建双通道系统（同质冗余） */
    dual_channel_system_t system;
    dual_system_init(&system, "HomogeneousSystem",
                     algo_standard, &ctx_a,
                     algo_standard_v2, &ctx_b);
    
    system.mode = DUAL_MODE_COMPARE;
    system.tolerance = 0.01f;
    
    printf("--- Homogeneous Redundancy (Compare Mode) ---\n\n");
    
    /* 正常处理 */
    float inputs[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    for (int i = 0; i < 5; i++) {
        dual_system_process(&system, inputs[i]);
    }
    
    /* 模拟通道 B 漂移 */
    printf("\n--- Simulating Channel B Drift ---\n\n");
    ctx_b.gain = 2.1f;  /* 轻微漂移 */
    
    for (int i = 0; i < 3; i++) {
        dual_system_process(&system, inputs[i]);
    }
    
    /* 恢复并测试主备模式 */
    printf("\n--- Primary/Backup Mode ---\n\n");
    ctx_b.gain = 2.0f;  /* 恢复 */
    system.mode = DUAL_MODE_PRIMARY;
    
    dual_system_process(&system, 10.0f);
    
    /* 模拟主通道故障 */
    printf("\n--- Primary Channel Failure ---\n");
    channel_inject_fault(&system.channel_a);
    
    dual_system_process(&system, 15.0f);
    dual_system_process(&system, 20.0f);
    
    /* 恢复主通道 */
    printf("\n--- Primary Channel Recovery ---\n");
    channel_recover(&system.channel_a);
    
    dual_system_process(&system, 25.0f);
    
    /* 异质冗余示例 */
    printf("\n--- Heterogeneous Redundancy ---\n\n");
    
    algo_polynomial_ctx_t poly_ctx = {
        .coefficients = {10.0f, 2.0f, 0.0f}  /* 等效于 y = 10 + 2x */
    };
    
    dual_channel_system_t hetero_system;
    dual_system_init(&hetero_system, "HeterogeneousSystem",
                     algo_standard, &ctx_a,
                     algo_polynomial, &poly_ctx);
    
    hetero_system.mode = DUAL_MODE_COMPARE;
    hetero_system.tolerance = 0.01f;
    
    for (int i = 0; i < 3; i++) {
        dual_system_process(&hetero_system, inputs[i]);
    }
    
    /* 统计 */
    printf("\n=== System Statistics ===\n");
    printf("Homogeneous System:\n");
    printf("  Mismatches: %u\n", system.mismatch_count);
    printf("  Failovers: %u\n", system.failover_count);
    printf("  Channel A processed: %u\n", system.channel_a.total_processed);
    printf("  Channel B processed: %u\n", system.channel_b.total_processed);
    
    printf("Heterogeneous System:\n");
    printf("  Mismatches: %u\n", hetero_system.mismatch_count);
    
    printf("\n=== Demo Complete ===\n");
}
```

---

## 总结

| 模式 | 适用场景 | 检测能力 | 复杂度 | 开销 |
|------|----------|----------|--------|------|
| 一补码 | 关键变量保护 | 位翻转 | 低 | 2x 存储 |
| CRC | 数据块/通信 | 多位错误 | 中 | 计算+存储 |
| 智能数据 | 数据验证 | 范围/一致性 | 中 | 运行时检查 |
| 通道 | 数据处理流 | 阶段错误 | 中 | 结构化设计 |
| 受保护单通道 | 安全系统 | 多重检查 | 中高 | 多重验证 |
| 双通道 | 高可靠性系统 | 通道故障 | 高 | 2x 资源 |

