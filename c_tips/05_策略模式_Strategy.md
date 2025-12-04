# 策略模式 (Strategy Pattern)

## 定义

策略模式是一种行为型设计模式，它定义了一系列算法，将每个算法封装起来，并使它们可以互相替换。策略模式让算法的变化独立于使用算法的客户端。

## 适用场景

- 需要在运行时选择不同算法的场景
- 有多种相似的行为，只是实现细节不同
- 避免使用大量条件语句选择算法
- 压缩算法选择（ZIP、GZIP、LZ4等）
- 排序算法选择
- 支付方式选择（信用卡、支付宝、微信等）
- 数据验证策略

## ASCII 图解

```
+------------------------------------------------------------------------+
|                        STRATEGY PATTERN                                 |
+------------------------------------------------------------------------+
|                                                                         |
|                       +------------------+                              |
|                       |     Context      |                              |
|                       +------------------+                              |
|                       | - strategy       |---+                          |
|                       +------------------+   |                          |
|                       | + setStrategy()  |   |                          |
|                       | + execute()      |   |                          |
|                       +------------------+   |                          |
|                                              |                          |
|                              +---------------+                          |
|                              |                                          |
|                              v                                          |
|                    +-------------------+                                |
|                    | <<interface>>     |                                |
|                    |    Strategy       |                                |
|                    +-------------------+                                |
|                    | + execute()       |                                |
|                    +-------------------+                                |
|                              ^                                          |
|                              |                                          |
|          +-------------------+-------------------+                      |
|          |                   |                   |                      |
|   +------+------+     +------+------+     +------+------+              |
|   | StrategyA   |     | StrategyB   |     | StrategyC   |              |
|   +-------------+     +-------------+     +-------------+              |
|   | + execute() |     | + execute() |     | + execute() |              |
|   +-------------+     +-------------+     +-------------+              |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   Runtime Strategy Selection:                                           |
|                                                                         |
|   +--------+                  +----------+                              |
|   | Client |                  | Context  |                              |
|   +---+----+                  +----+-----+                              |
|       |                            |                                    |
|       | 1. setStrategy(A)          |                                    |
|       |--------------------------->|                                    |
|       |                            |                                    |
|       | 2. execute()               |        +-----------+               |
|       |--------------------------->|------->| StrategyA |               |
|       |                            |        +-----------+               |
|       |                            |                                    |
|       | 3. setStrategy(B)          |                                    |
|       |--------------------------->|                                    |
|       |                            |                                    |
|       | 4. execute()               |        +-----------+               |
|       |--------------------------->|------->| StrategyB |               |
|       |                            |        +-----------+               |
|       |                            |                                    |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了策略模式的核心结构。Context（上下文）持有一个Strategy接口的引用，可以在运行时通过 `setStrategy()` 切换不同的策略实现。所有具体策略（StrategyA、B、C）都实现相同的接口，因此可以互相替换。下方的时序图展示了运行时切换策略的过程：客户端可以随时更换策略，每次调用 `execute()` 时会使用当前设置的策略。

## 实现方法

在C语言中实现策略模式：

1. 定义策略接口（函数指针结构体）
2. 实现各种具体策略
3. 创建上下文结构，包含当前策略指针
4. 提供策略切换函数

## C语言代码示例

### 策略接口定义

```c
// strategy.h
#ifndef STRATEGY_H
#define STRATEGY_H

#include <stddef.h>

// 策略接口
typedef struct Strategy Strategy;
typedef struct {
    int (*execute)(Strategy* self, void* data, size_t size);
    const char* (*get_name)(Strategy* self);
    void (*destroy)(Strategy* self);
} StrategyVTable;

struct Strategy {
    const StrategyVTable* vtable;
};

// 通用策略操作
int strategy_execute(Strategy* strategy, void* data, size_t size);
const char* strategy_get_name(Strategy* strategy);
void strategy_destroy(Strategy* strategy);

#endif
```

### 上下文实现

```c
// context.h
#ifndef CONTEXT_H
#define CONTEXT_H

#include "strategy.h"

typedef struct {
    Strategy* current_strategy;
    void* user_data;
} Context;

Context* context_create(void);
void context_set_strategy(Context* ctx, Strategy* strategy);
int context_execute(Context* ctx, void* data, size_t size);
void context_destroy(Context* ctx);

#endif
```

```c
// context.c
#include "context.h"
#include <stdio.h>
#include <stdlib.h>

Context* context_create(void) {
    Context* ctx = (Context*)malloc(sizeof(Context));
    if (ctx) {
        ctx->current_strategy = NULL;
        ctx->user_data = NULL;
    }
    return ctx;
}

void context_set_strategy(Context* ctx, Strategy* strategy) {
    if (ctx) {
        ctx->current_strategy = strategy;
        printf("[Context] Strategy changed to: %s\n", 
               strategy ? strategy_get_name(strategy) : "(none)");
    }
}

int context_execute(Context* ctx, void* data, size_t size) {
    if (!ctx || !ctx->current_strategy) {
        printf("[Context] No strategy set!\n");
        return -1;
    }
    return strategy_execute(ctx->current_strategy, data, size);
}

void context_destroy(Context* ctx) {
    free(ctx);
}
```

### 具体策略实现：压缩算法

```c
// compression_strategies.h
#ifndef COMPRESSION_STRATEGIES_H
#define COMPRESSION_STRATEGIES_H

#include "strategy.h"

// 创建不同的压缩策略
Strategy* create_no_compression(void);
Strategy* create_rle_compression(void);      // Run-Length Encoding
Strategy* create_simple_compression(void);   // 简单压缩

#endif
```

```c
// compression_strategies.c
#include "compression_strategies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==================== No Compression ====================

static int no_compress_execute(Strategy* self, void* data, size_t size) {
    (void)self;
    printf("  [NoCompress] Processing %zu bytes (no compression)\n", size);
    printf("  [NoCompress] Output size: %zu bytes (ratio: 100%%)\n", size);
    return 0;
}

static const char* no_compress_name(Strategy* self) {
    (void)self;
    return "NoCompression";
}

static void no_compress_destroy(Strategy* self) {
    free(self);
}

static const StrategyVTable no_compress_vtable = {
    .execute = no_compress_execute,
    .get_name = no_compress_name,
    .destroy = no_compress_destroy
};

Strategy* create_no_compression(void) {
    Strategy* s = (Strategy*)malloc(sizeof(Strategy));
    if (s) s->vtable = &no_compress_vtable;
    return s;
}

// ==================== RLE Compression ====================

static int rle_execute(Strategy* self, void* data, size_t size) {
    (void)self;
    unsigned char* bytes = (unsigned char*)data;
    size_t compressed = 0;
    
    printf("  [RLE] Processing %zu bytes with Run-Length Encoding\n", size);
    
    // 模拟RLE压缩计算
    size_t i = 0;
    while (i < size) {
        unsigned char current = bytes[i];
        size_t run = 1;
        while (i + run < size && bytes[i + run] == current && run < 255) {
            run++;
        }
        compressed += 2;  // count + byte
        i += run;
    }
    
    double ratio = (double)compressed / size * 100.0;
    printf("  [RLE] Output size: %zu bytes (ratio: %.1f%%)\n", compressed, ratio);
    return 0;
}

static const char* rle_name(Strategy* self) {
    (void)self;
    return "RLE";
}

static void rle_destroy(Strategy* self) {
    free(self);
}

static const StrategyVTable rle_vtable = {
    .execute = rle_execute,
    .get_name = rle_name,
    .destroy = rle_destroy
};

Strategy* create_rle_compression(void) {
    Strategy* s = (Strategy*)malloc(sizeof(Strategy));
    if (s) s->vtable = &rle_vtable;
    return s;
}

// ==================== Simple Compression ====================

static int simple_execute(Strategy* self, void* data, size_t size) {
    (void)self;
    (void)data;
    
    printf("  [Simple] Processing %zu bytes with dictionary encoding\n", size);
    
    // 模拟简单字典压缩
    size_t compressed = (size_t)(size * 0.7);
    double ratio = (double)compressed / size * 100.0;
    printf("  [Simple] Output size: %zu bytes (ratio: %.1f%%)\n", compressed, ratio);
    return 0;
}

static const char* simple_name(Strategy* self) {
    (void)self;
    return "SimpleCompress";
}

static void simple_destroy(Strategy* self) {
    free(self);
}

static const StrategyVTable simple_vtable = {
    .execute = simple_execute,
    .get_name = simple_name,
    .destroy = simple_destroy
};

Strategy* create_simple_compression(void) {
    Strategy* s = (Strategy*)malloc(sizeof(Strategy));
    if (s) s->vtable = &simple_vtable;
    return s;
}
```

### 实际应用示例：排序策略

```c
// sort_strategies.h
#ifndef SORT_STRATEGIES_H
#define SORT_STRATEGIES_H

typedef void (*SortFunction)(int* arr, int size);

typedef struct {
    SortFunction sort;
    const char* name;
} SortStrategy;

// 排序策略
extern SortStrategy bubble_sort_strategy;
extern SortStrategy quick_sort_strategy;
extern SortStrategy insertion_sort_strategy;

// 排序上下文
void sort_with_strategy(SortStrategy* strategy, int* arr, int size);
void print_array(const char* label, int* arr, int size);

#endif
```

```c
// sort_strategies.c
#include "sort_strategies.h"
#include <stdio.h>

// ==================== Bubble Sort ====================

static void bubble_sort(int* arr, int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                int temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

SortStrategy bubble_sort_strategy = {
    .sort = bubble_sort,
    .name = "Bubble Sort"
};

// ==================== Quick Sort ====================

static void quick_sort_impl(int* arr, int low, int high) {
    if (low < high) {
        int pivot = arr[high];
        int i = low - 1;
        
        for (int j = low; j < high; j++) {
            if (arr[j] < pivot) {
                i++;
                int temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
            }
        }
        
        int temp = arr[i + 1];
        arr[i + 1] = arr[high];
        arr[high] = temp;
        
        int pi = i + 1;
        quick_sort_impl(arr, low, pi - 1);
        quick_sort_impl(arr, pi + 1, high);
    }
}

static void quick_sort(int* arr, int size) {
    quick_sort_impl(arr, 0, size - 1);
}

SortStrategy quick_sort_strategy = {
    .sort = quick_sort,
    .name = "Quick Sort"
};

// ==================== Insertion Sort ====================

static void insertion_sort(int* arr, int size) {
    for (int i = 1; i < size; i++) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

SortStrategy insertion_sort_strategy = {
    .sort = insertion_sort,
    .name = "Insertion Sort"
};

// ==================== Context ====================

void sort_with_strategy(SortStrategy* strategy, int* arr, int size) {
    if (strategy && strategy->sort) {
        printf("[Sorting] Using %s algorithm\n", strategy->name);
        strategy->sort(arr, size);
    }
}

void print_array(const char* label, int* arr, int size) {
    printf("%s: [", label);
    for (int i = 0; i < size; i++) {
        printf("%d%s", arr[i], (i < size - 1) ? ", " : "");
    }
    printf("]\n");
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include <string.h>
#include "context.h"
#include "compression_strategies.h"
#include "sort_strategies.h"

int main() {
    printf("=== Strategy Pattern Demo ===\n\n");
    
    // ========== 压缩策略示例 ==========
    printf("--- Compression Strategies ---\n\n");
    
    Context* ctx = context_create();
    char data[] = "AAAAAABBBBCCCCCCCCDDDDDD";
    size_t data_size = strlen(data);
    
    printf("Data: \"%s\" (%zu bytes)\n\n", data, data_size);
    
    // 策略1: 无压缩
    Strategy* no_comp = create_no_compression();
    context_set_strategy(ctx, no_comp);
    context_execute(ctx, data, data_size);
    
    printf("\n");
    
    // 策略2: RLE压缩
    Strategy* rle = create_rle_compression();
    context_set_strategy(ctx, rle);
    context_execute(ctx, data, data_size);
    
    printf("\n");
    
    // 策略3: 简单压缩
    Strategy* simple = create_simple_compression();
    context_set_strategy(ctx, simple);
    context_execute(ctx, data, data_size);
    
    // 清理
    strategy_destroy(no_comp);
    strategy_destroy(rle);
    strategy_destroy(simple);
    context_destroy(ctx);
    
    // ========== 排序策略示例 ==========
    printf("\n--- Sort Strategies ---\n\n");
    
    int arr1[] = {64, 34, 25, 12, 22, 11, 90};
    int arr2[] = {64, 34, 25, 12, 22, 11, 90};
    int arr3[] = {64, 34, 25, 12, 22, 11, 90};
    int size = 7;
    
    // 使用冒泡排序
    print_array("Before", arr1, size);
    sort_with_strategy(&bubble_sort_strategy, arr1, size);
    print_array("After ", arr1, size);
    
    printf("\n");
    
    // 使用快速排序
    print_array("Before", arr2, size);
    sort_with_strategy(&quick_sort_strategy, arr2, size);
    print_array("After ", arr2, size);
    
    printf("\n");
    
    // 使用插入排序
    print_array("Before", arr3, size);
    sort_with_strategy(&insertion_sort_strategy, arr3, size);
    print_array("After ", arr3, size);
    
    return 0;
}
```

## 优缺点

### 优点
- 算法可以自由切换，运行时动态选择
- 避免使用多重条件判断
- 扩展性好，增加新策略无需修改上下文
- 符合开闭原则

### 缺点
- 客户端必须了解所有策略的差异
- 策略类数量可能很多
- 所有策略必须对外暴露相同接口

