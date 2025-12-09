# 策略模式 (Strategy Pattern)

## 核心思想

```
+------------------------------------------------------------------+
|                      STRATEGY PATTERN                             |
+------------------------------------------------------------------+

    PROBLEM: Multiple Algorithms with if-else
    
    process_payment(type, amount) {
        if (type == CREDIT_CARD) {
            // 100 lines of credit card logic
        } else if (type == PAYPAL) {
            // 100 lines of PayPal logic
        } else if (type == BITCOIN) {
            // 100 lines of Bitcoin logic
        }
        // More types = More if-else!
    }


    SOLUTION: Encapsulate each algorithm as Strategy
    
    +------------------+                    +------------------+
    |     Context      |      strategy      |     Strategy     |
    |  (PaymentProc)   | -----------------> |   (Interface)    |
    +------------------+                    +------------------+
    |                  |                    | execute(amount)  |
    | process() {      |                    +--------+---------+
    |   strategy->     |                             ^
    |     execute();   |                             |
    | }                |              +--------------+-------------+
    +------------------+              |              |             |
                                      |              |             |
                             +--------+---+ +--------+---+ +-------+----+
                             |CreditCard  | |  PayPal    | |  Bitcoin   |
                             | Strategy   | |  Strategy  | |  Strategy  |
                             +------------+ +------------+ +------------+


    RUNTIME SWITCH:
    
    Time -->
    
    context.set_strategy(CreditCard)    context.set_strategy(PayPal)
              |                                    |
              v                                    v
    context.process($100)               context.process($50)
              |                                    |
              v                                    v
    [Credit Card Logic]                 [PayPal Logic]
```

**核心思想说明：**
- 将算法封装成独立的策略类
- 策略之间可以互相替换
- 客户端通过接口使用策略，不关心具体实现
- 运行时可动态切换策略

## 实现思路

1. **定义策略接口**：所有策略实现相同的函数签名
2. **实现具体策略**：各算法独立封装
3. **上下文持有策略**：通过函数指针持有当前策略
4. **动态切换**：提供设置策略的函数

## 典型应用场景

- 支付方式选择
- 排序算法切换
- 压缩算法选择
- 日志级别处理
- 验证策略切换

## 完整代码示例

```c
/*============================================================================
 * 策略模式示例 - 数据压缩策略
 *============================================================================*/

/*---------------------------------------------------------------------------
 * compression_strategy.h - 策略接口定义
 *---------------------------------------------------------------------------*/
#ifndef COMPRESSION_STRATEGY_H
#define COMPRESSION_STRATEGY_H

#include <stdint.h>
#include <stddef.h>

/* 关键点：策略接口 - 所有压缩算法实现此接口 */
typedef struct {
    const char *name;
    
    /* 压缩函数 */
    size_t (*compress)(const uint8_t *input, size_t input_len,
                       uint8_t *output, size_t output_capacity);
    
    /* 解压函数 */
    size_t (*decompress)(const uint8_t *input, size_t input_len,
                         uint8_t *output, size_t output_capacity);
    
    /* 获取压缩后最大可能大小 */
    size_t (*get_max_compressed_size)(size_t input_len);
} compression_strategy_t;

/* 预定义策略 */
extern const compression_strategy_t strategy_no_compression;
extern const compression_strategy_t strategy_rle;
extern const compression_strategy_t strategy_lz;

#endif /* COMPRESSION_STRATEGY_H */


/*---------------------------------------------------------------------------
 * no_compression.c - 无压缩策略（直接复制）
 *---------------------------------------------------------------------------*/
#include "compression_strategy.h"
#include <string.h>
#include <stdio.h>

static size_t no_compress(const uint8_t *input, size_t input_len,
                          uint8_t *output, size_t output_capacity) {
    if (output_capacity < input_len) {
        return 0;
    }
    
    printf("[NoCompress] Copying %zu bytes\n", input_len);
    memcpy(output, input, input_len);
    return input_len;
}

static size_t no_decompress(const uint8_t *input, size_t input_len,
                            uint8_t *output, size_t output_capacity) {
    if (output_capacity < input_len) {
        return 0;
    }
    
    memcpy(output, input, input_len);
    return input_len;
}

static size_t no_max_size(size_t input_len) {
    return input_len;
}

const compression_strategy_t strategy_no_compression = {
    .name = "No Compression",
    .compress = no_compress,
    .decompress = no_decompress,
    .get_max_compressed_size = no_max_size
};


/*---------------------------------------------------------------------------
 * rle_compression.c - RLE 压缩策略（游程编码）
 *---------------------------------------------------------------------------*/
#include "compression_strategy.h"
#include <stdio.h>

/* 简单的 RLE 压缩 */
static size_t rle_compress(const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t output_capacity) {
    if (input_len == 0) return 0;
    
    size_t out_pos = 0;
    size_t i = 0;
    
    while (i < input_len) {
        uint8_t current = input[i];
        size_t count = 1;
        
        /* 统计连续相同字节 */
        while (i + count < input_len && 
               input[i + count] == current && 
               count < 255) {
            count++;
        }
        
        /* 输出：计数 + 字节值 */
        if (out_pos + 2 > output_capacity) {
            return 0;  /* 缓冲区不足 */
        }
        
        output[out_pos++] = (uint8_t)count;
        output[out_pos++] = current;
        
        i += count;
    }
    
    printf("[RLE] Compressed: %zu -> %zu bytes (%.1f%%)\n", 
           input_len, out_pos, (float)out_pos / input_len * 100);
    
    return out_pos;
}

static size_t rle_decompress(const uint8_t *input, size_t input_len,
                             uint8_t *output, size_t output_capacity) {
    size_t out_pos = 0;
    size_t i = 0;
    
    while (i + 1 < input_len) {
        uint8_t count = input[i++];
        uint8_t value = input[i++];
        
        if (out_pos + count > output_capacity) {
            return 0;
        }
        
        for (uint8_t j = 0; j < count; j++) {
            output[out_pos++] = value;
        }
    }
    
    return out_pos;
}

static size_t rle_max_size(size_t input_len) {
    return input_len * 2;  /* 最坏情况 */
}

const compression_strategy_t strategy_rle = {
    .name = "RLE (Run-Length Encoding)",
    .compress = rle_compress,
    .decompress = rle_decompress,
    .get_max_compressed_size = rle_max_size
};


/*---------------------------------------------------------------------------
 * lz_compression.c - 简化的 LZ 压缩策略
 *---------------------------------------------------------------------------*/
#include "compression_strategy.h"
#include <string.h>
#include <stdio.h>

/* 简化的 LZ 压缩（示例用） */
static size_t lz_compress(const uint8_t *input, size_t input_len,
                          uint8_t *output, size_t output_capacity) {
    /* 简化实现：添加头部标记 + 原始数据 */
    if (output_capacity < input_len + 4) {
        return 0;
    }
    
    /* 添加 "LZ" 头部 */
    output[0] = 'L';
    output[1] = 'Z';
    output[2] = (input_len >> 8) & 0xFF;
    output[3] = input_len & 0xFF;
    memcpy(output + 4, input, input_len);
    
    size_t out_len = input_len + 4;
    
    printf("[LZ] Compressed: %zu -> %zu bytes\n", input_len, out_len);
    
    return out_len;
}

static size_t lz_decompress(const uint8_t *input, size_t input_len,
                            uint8_t *output, size_t output_capacity) {
    if (input_len < 4 || input[0] != 'L' || input[1] != 'Z') {
        return 0;
    }
    
    size_t original_len = (input[2] << 8) | input[3];
    
    if (output_capacity < original_len) {
        return 0;
    }
    
    memcpy(output, input + 4, original_len);
    return original_len;
}

static size_t lz_max_size(size_t input_len) {
    return input_len + 4;
}

const compression_strategy_t strategy_lz = {
    .name = "LZ (Lempel-Ziv)",
    .compress = lz_compress,
    .decompress = lz_decompress,
    .get_max_compressed_size = lz_max_size
};


/*---------------------------------------------------------------------------
 * compressor.h - 上下文定义
 *---------------------------------------------------------------------------*/
#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "compression_strategy.h"

typedef struct {
    const compression_strategy_t *strategy;  /* 关键点：持有当前策略 */
    uint8_t *buffer;
    size_t buffer_size;
} compressor_t;

compressor_t* compressor_create(size_t buffer_size);
void compressor_destroy(compressor_t *comp);

/* 关键点：设置/切换策略 */
void compressor_set_strategy(compressor_t *comp, 
                             const compression_strategy_t *strategy);

/* 使用当前策略处理数据 */
size_t compressor_compress(compressor_t *comp, 
                           const uint8_t *data, size_t len,
                           uint8_t *output, size_t output_capacity);
size_t compressor_decompress(compressor_t *comp,
                             const uint8_t *data, size_t len,
                             uint8_t *output, size_t output_capacity);

#endif /* COMPRESSOR_H */


/*---------------------------------------------------------------------------
 * compressor.c - 上下文实现
 *---------------------------------------------------------------------------*/
#include "compressor.h"
#include <stdlib.h>
#include <stdio.h>

compressor_t* compressor_create(size_t buffer_size) {
    compressor_t *comp = malloc(sizeof(compressor_t));
    if (comp == NULL) return NULL;
    
    comp->buffer = malloc(buffer_size);
    if (comp->buffer == NULL) {
        free(comp);
        return NULL;
    }
    
    comp->buffer_size = buffer_size;
    comp->strategy = &strategy_no_compression;  /* 默认策略 */
    
    return comp;
}

void compressor_destroy(compressor_t *comp) {
    if (comp != NULL) {
        free(comp->buffer);
        free(comp);
    }
}

/* 关键点：动态切换策略 */
void compressor_set_strategy(compressor_t *comp,
                             const compression_strategy_t *strategy) {
    if (comp != NULL && strategy != NULL) {
        printf("[Compressor] Strategy changed to: %s\n", strategy->name);
        comp->strategy = strategy;
    }
}

/* 关键点：通过策略接口调用，不关心具体实现 */
size_t compressor_compress(compressor_t *comp,
                           const uint8_t *data, size_t len,
                           uint8_t *output, size_t output_capacity) {
    if (comp == NULL || comp->strategy == NULL) {
        return 0;
    }
    
    printf("[Compressor] Using strategy: %s\n", comp->strategy->name);
    return comp->strategy->compress(data, len, output, output_capacity);
}

size_t compressor_decompress(compressor_t *comp,
                             const uint8_t *data, size_t len,
                             uint8_t *output, size_t output_capacity) {
    if (comp == NULL || comp->strategy == NULL) {
        return 0;
    }
    
    return comp->strategy->decompress(data, len, output, output_capacity);
}


/*---------------------------------------------------------------------------
 * 使用示例 - main.c
 *---------------------------------------------------------------------------*/
#include "compressor.h"
#include <stdio.h>
#include <string.h>

void print_hex(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02X ", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

int main(void) {
    printf("=== Strategy Pattern Demo ===\n\n");
    
    /* 创建压缩器 */
    compressor_t *comp = compressor_create(4096);
    
    /* 测试数据：重复字节（RLE 压缩效果好） */
    uint8_t test_data[] = {
        'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A',
        'B', 'B', 'B', 'B', 'B',
        'C', 'C', 'C', 'C', 'C', 'C', 'C',
        'D', 'D', 'D'
    };
    size_t test_len = sizeof(test_data);
    
    uint8_t compressed[256];
    uint8_t decompressed[256];
    size_t comp_len, decomp_len;
    
    printf("Original data (%zu bytes): ", test_len);
    print_hex(test_data, test_len);
    
    /* 关键点：使用不同策略压缩同一数据 */
    
    printf("\n--- Strategy 1: No Compression ---\n");
    compressor_set_strategy(comp, &strategy_no_compression);
    comp_len = compressor_compress(comp, test_data, test_len, 
                                    compressed, sizeof(compressed));
    printf("Compressed (%zu bytes): ", comp_len);
    print_hex(compressed, comp_len);
    
    printf("\n--- Strategy 2: RLE ---\n");
    compressor_set_strategy(comp, &strategy_rle);
    comp_len = compressor_compress(comp, test_data, test_len,
                                    compressed, sizeof(compressed));
    printf("Compressed (%zu bytes): ", comp_len);
    print_hex(compressed, comp_len);
    
    /* 验证解压 */
    decomp_len = compressor_decompress(comp, compressed, comp_len,
                                        decompressed, sizeof(decompressed));
    printf("Decompressed (%zu bytes): ", decomp_len);
    print_hex(decompressed, decomp_len);
    printf("Verify: %s\n", 
           memcmp(test_data, decompressed, test_len) == 0 ? "OK" : "FAILED");
    
    printf("\n--- Strategy 3: LZ ---\n");
    compressor_set_strategy(comp, &strategy_lz);
    comp_len = compressor_compress(comp, test_data, test_len,
                                    compressed, sizeof(compressed));
    printf("Compressed (%zu bytes): ", comp_len);
    print_hex(compressed, comp_len);
    
    /* 清理 */
    compressor_destroy(comp);
    
    printf("\nDone!\n");
    return 0;
}
```

## 运行输出示例

```
=== Strategy Pattern Demo ===

Original data (25 bytes): 41 41 41 41 41 41 41 41 41 41 42 42 42 42 42 43 43 43 43 43 43 43 44 44 44 

--- Strategy 1: No Compression ---
[Compressor] Strategy changed to: No Compression
[Compressor] Using strategy: No Compression
[NoCompress] Copying 25 bytes
Compressed (25 bytes): 41 41 41 41 41 41 41 41 41 41 42 42 42 42 42 43 43 43 43 43 43 43 44 44 44 

--- Strategy 2: RLE ---
[Compressor] Strategy changed to: RLE (Run-Length Encoding)
[Compressor] Using strategy: RLE (Run-Length Encoding)
[RLE] Compressed: 25 -> 8 bytes (32.0%)
Compressed (8 bytes): 0A 41 05 42 07 43 03 44 
Decompressed (25 bytes): 41 41 41 41 41 41 41 41 41 41 42 42 42 42 42 43 43 43 43 43 43 43 44 44 44 
Verify: OK

--- Strategy 3: LZ ---
[Compressor] Strategy changed to: LZ (Lempel-Ziv)
[Compressor] Using strategy: LZ (Lempel-Ziv)
[LZ] Compressed: 25 -> 29 bytes
Compressed (29 bytes): 4C 5A 00 19 41 41 41 41 41 41 41 41 41 41 42 42 42 42 42 43 43 43 43 43 43 43 44 44 44 ...

Done!
```

## 优势分析

| 优势 | 说明 |
|------|------|
| **算法封装** | 每个策略独立封装，职责单一 |
| **运行时切换** | 可动态选择最合适的算法 |
| **消除条件语句** | 避免大量 if-else/switch |
| **易于扩展** | 新增策略不影响现有代码 |
| **易于测试** | 策略可独立测试 |

