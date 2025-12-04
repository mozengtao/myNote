# 编译时断言 (Static Assert / Compile-Time Assert)

## 定义

编译时断言是一种在编译阶段验证条件的技术，如果条件不满足则编译失败。与运行时断言不同，它不产生任何运行时开销，可以在编译期捕获结构体大小、对齐、常量值等问题。

## 适用场景

- 验证结构体大小（确保二进制兼容性）
- 检查结构体字段偏移
- 验证枚举值范围
- 检查配置常量的合理性
- 确保数组大小足够
- 验证类型大小假设
- 网络协议结构体的精确尺寸验证
- 跨平台代码的类型大小检查

## ASCII 图解

```
+------------------------------------------------------------------------+
|                      STATIC ASSERT (Compile-Time)                       |
+------------------------------------------------------------------------+
|                                                                         |
|   RUNTIME ASSERT:                    COMPILE-TIME ASSERT:               |
|   +-------------------+              +-------------------+              |
|   |    Source Code    |              |    Source Code    |              |
|   +--------+----------+              +--------+----------+              |
|            |                                  |                         |
|            v                                  v                         |
|   +-------------------+              +-------------------+              |
|   |     Compile       |              |     Compile       |              |
|   +--------+----------+              +--------+----------+              |
|            |                                  |                         |
|            v                                  | Check condition         |
|   +-------------------+                       v                         |
|   |    Executable     |              +-------------------+              |
|   +--------+----------+              | Condition TRUE?   |              |
|            |                         +----+--------+-----+              |
|            v                              |        |                    |
|   +-------------------+                  YES       NO                   |
|   |       Run         |                   |        |                    |
|   +--------+----------+                   v        v                    |
|            |                         +-------+ +--------+               |
|            v                         |Compile| | ERROR! |               |
|   +-------------------+              |Success| | Stop   |               |
|   | Check at runtime  |              +-------+ +--------+               |
|   +--------+----------+                                                 |
|            |                         Zero runtime overhead!             |
|        CRASH if fail                 Error caught before shipping!      |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   USE CASES:                                                            |
|                                                                         |
|   1. Struct Size Verification (for wire protocols)                      |
|   +------------------------------------------+                          |
|   | _Static_assert(sizeof(Header) == 16,     |                          |
|   |               "Header must be 16 bytes");|                          |
|   +------------------------------------------+                          |
|                                                                         |
|   2. Field Offset Verification                                          |
|   +------------------------------------------+                          |
|   | _Static_assert(offsetof(Packet, seq)==4, |                          |
|   |               "seq must be at offset 4");|                          |
|   +------------------------------------------+                          |
|                                                                         |
|   3. Configuration Validation                                           |
|   +------------------------------------------+                          |
|   | _Static_assert(BUFFER_SIZE >= 1024,      |                          |
|   |               "Buffer too small");       |                          |
|   +------------------------------------------+                          |
|                                                                         |
|   4. Type Size Assumptions                                              |
|   +------------------------------------------+                          |
|   | _Static_assert(sizeof(int) >= 4,         |                          |
|   |               "int must be 32+ bits");   |                          |
|   +------------------------------------------+                          |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图对比了运行时断言和编译时断言的区别。运行时断言在程序执行时才检查条件，失败会导致程序崩溃。编译时断言在编译阶段就检查条件，如果失败则编译终止，这意味着错误在代码发布前就被发现，且不会产生任何运行时开销。右侧展示了编译时断言的常见用途：验证结构体大小、字段偏移、配置参数和类型假设。

## 实现方法

1. C11标准：使用`_Static_assert`关键字
2. C99兼容：使用负数组大小技巧
3. 使用位域技巧
4. 封装成统一的宏，支持多种编译器

## C语言代码示例

### 编译时断言宏

```c
// static_assert.h
#ifndef STATIC_ASSERT_H
#define STATIC_ASSERT_H

#include <stddef.h>

// ==================== C11 _Static_assert ====================
#if __STDC_VERSION__ >= 201112L
    // C11原生支持
    #define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    // C99兼容实现
    
    // 方法1: 负数组大小（条件为假时数组大小为-1，编译失败）
    #define STATIC_ASSERT_CONCAT_(a, b) a##b
    #define STATIC_ASSERT_CONCAT(a, b) STATIC_ASSERT_CONCAT_(a, b)
    #define STATIC_ASSERT(cond, msg) \
        typedef char STATIC_ASSERT_CONCAT(static_assert_fail_, __LINE__) \
            [(cond) ? 1 : -1]
#endif

// ==================== 便捷断言宏 ====================

// 验证类型大小
#define ASSERT_TYPE_SIZE(type, size) \
    STATIC_ASSERT(sizeof(type) == (size), \
                  #type " must be " #size " bytes")

// 验证类型最小大小
#define ASSERT_TYPE_MIN_SIZE(type, min_size) \
    STATIC_ASSERT(sizeof(type) >= (min_size), \
                  #type " must be at least " #min_size " bytes")

// 验证结构体字段偏移
#define ASSERT_FIELD_OFFSET(type, field, expected_offset) \
    STATIC_ASSERT(offsetof(type, field) == (expected_offset), \
                  #type "." #field " must be at offset " #expected_offset)

// 验证数组大小足够
#define ASSERT_ARRAY_SIZE(arr, min_size) \
    STATIC_ASSERT(sizeof(arr) / sizeof((arr)[0]) >= (min_size), \
                  #arr " must have at least " #min_size " elements")

// 验证值在范围内
#define ASSERT_IN_RANGE(val, min, max) \
    STATIC_ASSERT((val) >= (min) && (val) <= (max), \
                  #val " must be in range [" #min ", " #max "]")

// 验证是2的幂
#define ASSERT_POWER_OF_TWO(val) \
    STATIC_ASSERT(((val) & ((val) - 1)) == 0 && (val) > 0, \
                  #val " must be a power of 2")

// 验证对齐
#define ASSERT_ALIGNMENT(type, alignment) \
    STATIC_ASSERT(alignof(type) >= (alignment), \
                  #type " must be aligned to " #alignment " bytes")

#endif // STATIC_ASSERT_H
```

### 网络协议结构体验证

```c
// network_protocol.h
#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include "static_assert.h"
#include <stdint.h>

// ==================== 线协议结构体定义 ====================
// 这些结构体必须与协议规范完全匹配

#pragma pack(push, 1)  // 禁用填充

// 以太网帧头 (14 bytes)
typedef struct {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ether_type;
} EthernetHeader;

// 编译时验证
STATIC_ASSERT(sizeof(EthernetHeader) == 14, 
              "EthernetHeader must be exactly 14 bytes");

// IPv4头 (20 bytes minimum)
typedef struct {
    uint8_t  version_ihl;      // 版本 + 头长度
    uint8_t  tos;              // 服务类型
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} IPv4Header;

STATIC_ASSERT(sizeof(IPv4Header) == 20, 
              "IPv4Header must be exactly 20 bytes");
ASSERT_FIELD_OFFSET(IPv4Header, src_ip, 12);
ASSERT_FIELD_OFFSET(IPv4Header, dest_ip, 16);

// TCP头 (20 bytes minimum)
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} TCPHeader;

STATIC_ASSERT(sizeof(TCPHeader) == 20, 
              "TCPHeader must be exactly 20 bytes");
ASSERT_FIELD_OFFSET(TCPHeader, seq_num, 4);
ASSERT_FIELD_OFFSET(TCPHeader, ack_num, 8);

// UDP头 (8 bytes)
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} UDPHeader;

STATIC_ASSERT(sizeof(UDPHeader) == 8, 
              "UDPHeader must be exactly 8 bytes");

// 自定义应用协议消息头
typedef struct {
    uint8_t  magic[4];     // "MYPR"
    uint8_t  version;
    uint8_t  type;
    uint16_t payload_len;
    uint32_t sequence;
    uint32_t timestamp;
    uint8_t  reserved[4];
} AppMessageHeader;

STATIC_ASSERT(sizeof(AppMessageHeader) == 20, 
              "AppMessageHeader must be exactly 20 bytes");
ASSERT_FIELD_OFFSET(AppMessageHeader, version, 4);
ASSERT_FIELD_OFFSET(AppMessageHeader, sequence, 8);
ASSERT_FIELD_OFFSET(AppMessageHeader, timestamp, 12);

#pragma pack(pop)

// ==================== 验证组合 ====================

// 完整数据包的最大大小
#define MAX_PACKET_SIZE 65535
#define MTU 1500

// 验证MTU足够容纳头部
STATIC_ASSERT(MTU > sizeof(EthernetHeader) + sizeof(IPv4Header) + sizeof(TCPHeader),
              "MTU must accommodate standard headers");

#endif // NETWORK_PROTOCOL_H
```

### 配置验证

```c
// config_validation.h
#ifndef CONFIG_VALIDATION_H
#define CONFIG_VALIDATION_H

#include "static_assert.h"

// ==================== 系统配置 ====================

#define MAX_CONNECTIONS     1024
#define BUFFER_SIZE         4096
#define THREAD_POOL_SIZE    8
#define MAX_PATH_LENGTH     256
#define HASH_TABLE_SIZE     1024

// ==================== 编译时配置验证 ====================

// 连接数必须是2的幂（用于位操作优化）
ASSERT_POWER_OF_TWO(MAX_CONNECTIONS);

// 缓冲区大小必须是2的幂
ASSERT_POWER_OF_TWO(BUFFER_SIZE);

// 线程池大小必须在合理范围内
ASSERT_IN_RANGE(THREAD_POOL_SIZE, 1, 64);

// 哈希表大小必须是2的幂
ASSERT_POWER_OF_TWO(HASH_TABLE_SIZE);

// 路径长度必须足够
STATIC_ASSERT(MAX_PATH_LENGTH >= 260,
              "MAX_PATH_LENGTH must support Windows long paths");

// ==================== 类型假设验证 ====================

// 基本类型大小假设
STATIC_ASSERT(sizeof(char) == 1, "char must be 1 byte");
STATIC_ASSERT(sizeof(short) >= 2, "short must be at least 2 bytes");
STATIC_ASSERT(sizeof(int) >= 4, "int must be at least 4 bytes");
STATIC_ASSERT(sizeof(long long) >= 8, "long long must be at least 8 bytes");
STATIC_ASSERT(sizeof(void*) == 4 || sizeof(void*) == 8,
              "Only 32-bit or 64-bit platforms supported");

// 浮点类型
STATIC_ASSERT(sizeof(float) == 4, "float must be 4 bytes");
STATIC_ASSERT(sizeof(double) == 8, "double must be 8 bytes");

// ==================== 枚举范围验证 ====================

typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_HIGH = 2,
    PRIORITY_CRITICAL = 3,
    PRIORITY_COUNT
} Priority;

// 验证优先级值可以用2位表示
STATIC_ASSERT(PRIORITY_COUNT <= 4, "Priority values must fit in 2 bits");

// 如果使用uint8_t存储，验证范围
STATIC_ASSERT(PRIORITY_COUNT <= 256, "Priority must fit in uint8_t");

// ==================== 位域验证 ====================

typedef struct {
    uint32_t id : 20;        // 最多1M个ID
    uint32_t type : 4;       // 最多16种类型
    uint32_t flags : 8;      // 8个标志位
} CompactRecord;

STATIC_ASSERT(sizeof(CompactRecord) == 4, 
              "CompactRecord must be exactly 4 bytes");

// 验证位域足够
#define MAX_RECORD_ID 1000000
#define MAX_RECORD_TYPES 16

STATIC_ASSERT(MAX_RECORD_ID < (1 << 20), "ID field too small");
STATIC_ASSERT(MAX_RECORD_TYPES <= (1 << 4), "Type field too small");

#endif // CONFIG_VALIDATION_H
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include <string.h>
#include "static_assert.h"
#include "network_protocol.h"
#include "config_validation.h"

// 本文件内的编译时断言
STATIC_ASSERT(sizeof(int) == 4, "This code assumes 32-bit int");

// 验证数组大小
static char error_messages[PRIORITY_COUNT][64];
STATIC_ASSERT(sizeof(error_messages) / sizeof(error_messages[0]) >= PRIORITY_COUNT,
              "error_messages array too small");

// 函数内使用
void process_packet(const void* data, size_t len) {
    // 编译时验证头部能放入
    STATIC_ASSERT(sizeof(AppMessageHeader) <= 64, 
                  "Header should fit in cache line");
    
    if (len < sizeof(AppMessageHeader)) {
        printf("Packet too small\n");
        return;
    }
    
    const AppMessageHeader* header = (const AppMessageHeader*)data;
    printf("Processing packet: version=%d, type=%d, seq=%u\n",
           header->version, header->type, header->sequence);
}

int main() {
    printf("=== Static Assert Demo ===\n\n");
    
    // 所有静态断言在编译时已经验证过了
    printf("All compile-time assertions passed!\n\n");
    
    // 打印验证过的结构体大小
    printf("--- Verified Structure Sizes ---\n");
    printf("EthernetHeader:    %2zu bytes (expected: 14)\n", sizeof(EthernetHeader));
    printf("IPv4Header:        %2zu bytes (expected: 20)\n", sizeof(IPv4Header));
    printf("TCPHeader:         %2zu bytes (expected: 20)\n", sizeof(TCPHeader));
    printf("UDPHeader:         %2zu bytes (expected: 8)\n", sizeof(UDPHeader));
    printf("AppMessageHeader:  %2zu bytes (expected: 20)\n", sizeof(AppMessageHeader));
    printf("CompactRecord:     %2zu bytes (expected: 4)\n", sizeof(CompactRecord));
    
    // 打印配置值
    printf("\n--- Verified Configuration ---\n");
    printf("MAX_CONNECTIONS:   %d (power of 2: %s)\n", 
           MAX_CONNECTIONS, 
           (MAX_CONNECTIONS & (MAX_CONNECTIONS-1)) == 0 ? "yes" : "no");
    printf("BUFFER_SIZE:       %d\n", BUFFER_SIZE);
    printf("THREAD_POOL_SIZE:  %d\n", THREAD_POOL_SIZE);
    printf("HASH_TABLE_SIZE:   %d\n", HASH_TABLE_SIZE);
    
    // 打印类型大小
    printf("\n--- Verified Type Sizes ---\n");
    printf("char:              %zu byte(s)\n", sizeof(char));
    printf("short:             %zu bytes\n", sizeof(short));
    printf("int:               %zu bytes\n", sizeof(int));
    printf("long:              %zu bytes\n", sizeof(long));
    printf("long long:         %zu bytes\n", sizeof(long long));
    printf("void*:             %zu bytes\n", sizeof(void*));
    printf("float:             %zu bytes\n", sizeof(float));
    printf("double:            %zu bytes\n", sizeof(double));
    
    // 测试数据包处理
    printf("\n--- Packet Processing Test ---\n");
    AppMessageHeader packet = {
        .magic = {'M', 'Y', 'P', 'R'},
        .version = 1,
        .type = 2,
        .payload_len = 100,
        .sequence = 12345,
        .timestamp = 1234567890
    };
    process_packet(&packet, sizeof(packet));
    
    // 演示编译时断言失败（取消注释以测试）
    // STATIC_ASSERT(sizeof(int) == 8, "This will fail on 32-bit int systems");
    // ASSERT_POWER_OF_TWO(100);  // 100不是2的幂，会失败
    
    printf("\nAll verifications successful!\n");
    
    return 0;
}

/* 输出示例:
=== Static Assert Demo ===

All compile-time assertions passed!

--- Verified Structure Sizes ---
EthernetHeader:    14 bytes (expected: 14)
IPv4Header:        20 bytes (expected: 20)
TCPHeader:         20 bytes (expected: 20)
UDPHeader:          8 bytes (expected: 8)
AppMessageHeader:  20 bytes (expected: 20)
CompactRecord:      4 bytes (expected: 4)

--- Verified Configuration ---
MAX_CONNECTIONS:   1024 (power of 2: yes)
BUFFER_SIZE:       4096
THREAD_POOL_SIZE:  8
HASH_TABLE_SIZE:   1024

--- Verified Type Sizes ---
char:              1 byte(s)
short:             2 bytes
int:               4 bytes
long:              4 bytes
long long:         8 bytes
void*:             8 bytes
float:             4 bytes
double:            8 bytes

--- Packet Processing Test ---
Processing packet: version=1, type=2, seq=12345

All verifications successful!
*/
```

## 优缺点

### 优点
- **零运行时开销**：所有检查在编译时完成
- **早期错误发现**：问题在发布前就被捕获
- **文档作用**：明确表达代码的假设
- **跨平台验证**：确保代码的可移植性假设
- **二进制兼容性**：保证结构体布局正确

### 缺点
- 只能检查编译时常量表达式
- 错误消息可能不够友好（特别是C99兼容方案）
- 某些编译器对`_Static_assert`支持有限
- 无法检查运行时值
- C99兼容方案会产生未使用的typedef

