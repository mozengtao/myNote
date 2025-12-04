# 契约式编程 (Design by Contract)

## 定义

契约式编程是一种软件设计方法，通过定义函数的前置条件（Preconditions）、后置条件（Postconditions）和不变式（Invariants）来明确组件之间的责任。调用者必须满足前置条件，被调用者保证满足后置条件，而不变式在操作前后始终为真。

## 适用场景

- API和库的边界验证
- 关键业务逻辑的正确性保证
- 调试阶段的错误检测
- 数据结构完整性验证
- 状态机的状态一致性检查
- 安全关键系统的断言
- 文档化代码的隐式假设

## ASCII 图解

```
+------------------------------------------------------------------------+
|                      DESIGN BY CONTRACT                                 |
+------------------------------------------------------------------------+
|                                                                         |
|   +---------------------------+                                         |
|   |     CALLER's DUTY         |                                         |
|   +---------------------------+                                         |
|   | Satisfy PRECONDITIONS:    |                                         |
|   | - Valid parameters        |                                         |
|   | - Object in valid state   |                                         |
|   | - Required resources      |                                         |
|   +-------------+-------------+                                         |
|                 |                                                       |
|                 v                                                       |
|   +---------------------------+                                         |
|   |     FUNCTION ENTRY        |                                         |
|   +---------------------------+                                         |
|   | REQUIRE(preconditions)    |<-- Verify caller's obligations          |
|   +-------------+-------------+                                         |
|                 |                                                       |
|                 v                                                       |
|   +---------------------------+                                         |
|   |    FUNCTION BODY          |                                         |
|   |    (Implementation)       |                                         |
|   +-------------+-------------+                                         |
|                 |                                                       |
|                 v                                                       |
|   +---------------------------+                                         |
|   |     FUNCTION EXIT         |                                         |
|   +---------------------------+                                         |
|   | ENSURE(postconditions)    |<-- Verify function's promises           |
|   +-------------+-------------+                                         |
|                 |                                                       |
|                 v                                                       |
|   +---------------------------+                                         |
|   |   FUNCTION's GUARANTEE    |                                         |
|   +---------------------------+                                         |
|   | Return value valid        |                                         |
|   | Side effects documented   |                                         |
|   | State modified correctly  |                                         |
|   +---------------------------+                                         |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   INVARIANT: Always True (Before AND After)                             |
|                                                                         |
|   +------------------+    +------------------+    +------------------+  |
|   |  INVARIANT TRUE  |--->|  Operation       |--->|  INVARIANT TRUE  |  |
|   |  (before call)   |    |  (may modify)    |    |  (after call)    |  |
|   +------------------+    +------------------+    +------------------+  |
|                                                                         |
|   Example: Binary Search Tree                                           |
|   INVARIANT: For all nodes: left.value < node.value < right.value       |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   CONTRACT VIOLATION HANDLING:                                          |
|                                                                         |
|   +-------------------+     +---------------------+                     |
|   | PRECONDITION FAIL |---->| CALLER's BUG        | (caller's fault)    |
|   +-------------------+     +---------------------+                     |
|                                                                         |
|   +-------------------+     +---------------------+                     |
|   | POSTCONDITION FAIL|--->| IMPLEMENTATION BUG  | (function's fault)  |
|   +-------------------+     +---------------------+                     |
|                                                                         |
|   +-------------------+     +---------------------+                     |
|   | INVARIANT FAIL    |---->| DATA CORRUPTION     | (serious bug)       |
|   +-------------------+     +---------------------+                     |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了契约式编程的三个核心概念。前置条件（REQUIRE）是调用者的义务，必须在调用函数前满足；后置条件（ENSURE）是函数的承诺，保证在正常返回时满足；不变式（INVARIANT）是始终为真的条件，在操作前后都必须成立。当契约被违反时，可以明确定位是调用者的错误（前置条件失败）还是实现的错误（后置条件失败），或者是数据损坏（不变式失败）。

## 实现方法

1. 定义契约检查宏（REQUIRE、ENSURE、INVARIANT）
2. 在发布版本中可以禁用检查（NDEBUG）
3. 违反契约时提供详细的诊断信息
4. 对数据结构定义不变式检查函数
5. 在关键操作前后调用不变式检查

## C语言代码示例

### 契约宏定义

```c
// contract.h
#ifndef CONTRACT_H
#define CONTRACT_H

#include <stdio.h>
#include <stdlib.h>

// ==================== 契约检查宏 ====================

#ifdef NDEBUG
    // 发布版本：禁用所有契约检查
    #define REQUIRE(cond, msg)      ((void)0)
    #define ENSURE(cond, msg)       ((void)0)
    #define INVARIANT(cond, msg)    ((void)0)
    #define ASSERT_UNREACHABLE()    ((void)0)
#else
    // 调试版本：启用契约检查
    
    #define REQUIRE(cond, msg) do {                                 \
        if (!(cond)) {                                              \
            fprintf(stderr,                                         \
                "\n╔══════════════════════════════════════════╗\n"  \
                "║        PRECONDITION VIOLATED             ║\n"    \
                "╠══════════════════════════════════════════╣\n"    \
                "║ Condition: %s\n"                                 \
                "║ Message:   %s\n"                                 \
                "║ Location:  %s:%d\n"                              \
                "║ Function:  %s()\n"                               \
                "╚══════════════════════════════════════════╝\n\n", \
                #cond, msg, __FILE__, __LINE__, __func__);          \
            abort();                                                \
        }                                                           \
    } while(0)
    
    #define ENSURE(cond, msg) do {                                  \
        if (!(cond)) {                                              \
            fprintf(stderr,                                         \
                "\n╔══════════════════════════════════════════╗\n"  \
                "║        POSTCONDITION VIOLATED            ║\n"    \
                "╠══════════════════════════════════════════╣\n"    \
                "║ Condition: %s\n"                                 \
                "║ Message:   %s\n"                                 \
                "║ Location:  %s:%d\n"                              \
                "║ Function:  %s()\n"                               \
                "╚══════════════════════════════════════════╝\n\n", \
                #cond, msg, __FILE__, __LINE__, __func__);          \
            abort();                                                \
        }                                                           \
    } while(0)
    
    #define INVARIANT(cond, msg) do {                               \
        if (!(cond)) {                                              \
            fprintf(stderr,                                         \
                "\n╔══════════════════════════════════════════╗\n"  \
                "║         INVARIANT VIOLATED               ║\n"    \
                "╠══════════════════════════════════════════╣\n"    \
                "║ Condition: %s\n"                                 \
                "║ Message:   %s\n"                                 \
                "║ Location:  %s:%d\n"                              \
                "║ Function:  %s()\n"                               \
                "╚══════════════════════════════════════════╝\n\n", \
                #cond, msg, __FILE__, __LINE__, __func__);          \
            abort();                                                \
        }                                                           \
    } while(0)
    
    #define ASSERT_UNREACHABLE() do {                               \
        fprintf(stderr,                                             \
            "\n╔══════════════════════════════════════════╗\n"      \
            "║       UNREACHABLE CODE REACHED           ║\n"        \
            "╠══════════════════════════════════════════╣\n"        \
            "║ Location:  %s:%d\n"                                  \
            "║ Function:  %s()\n"                                   \
            "╚══════════════════════════════════════════╝\n\n",     \
            __FILE__, __LINE__, __func__);                          \
        abort();                                                    \
    } while(0)
#endif

// ==================== 范围检查宏 ====================

#define REQUIRE_NOT_NULL(ptr) \
    REQUIRE((ptr) != NULL, #ptr " must not be NULL")

#define REQUIRE_IN_RANGE(val, min, max) \
    REQUIRE((val) >= (min) && (val) <= (max), \
            #val " must be in range [" #min ", " #max "]")

#define REQUIRE_POSITIVE(val) \
    REQUIRE((val) > 0, #val " must be positive")

#define REQUIRE_NON_NEGATIVE(val) \
    REQUIRE((val) >= 0, #val " must be non-negative")

#endif // CONTRACT_H
```

### 带契约的栈实现

```c
// stack_contract.h
#ifndef STACK_CONTRACT_H
#define STACK_CONTRACT_H

#include <stddef.h>

#define STACK_MAX_CAPACITY 1024

typedef struct {
    int* data;
    size_t capacity;
    size_t size;
} Stack;

// 生命周期
Stack* stack_create(size_t capacity);
void stack_destroy(Stack* stack);

// 操作
void stack_push(Stack* stack, int value);
int stack_pop(Stack* stack);
int stack_peek(const Stack* stack);

// 查询
size_t stack_size(const Stack* stack);
int stack_is_empty(const Stack* stack);
int stack_is_full(const Stack* stack);

// 调试
void stack_print(const Stack* stack);

#endif
```

```c
// stack_contract.c
#include "stack_contract.h"
#include "contract.h"
#include <stdio.h>
#include <stdlib.h>

// ==================== 不变式检查 ====================

static void stack_check_invariant(const Stack* stack) {
    INVARIANT(stack != NULL, 
              "Stack pointer must not be NULL");
    INVARIANT(stack->data != NULL, 
              "Stack data must be allocated");
    INVARIANT(stack->capacity > 0 && stack->capacity <= STACK_MAX_CAPACITY,
              "Stack capacity must be in valid range");
    INVARIANT(stack->size <= stack->capacity,
              "Stack size must not exceed capacity");
}

// ==================== 实现 ====================

Stack* stack_create(size_t capacity) {
    // 前置条件
    REQUIRE(capacity > 0, "Capacity must be positive");
    REQUIRE(capacity <= STACK_MAX_CAPACITY, "Capacity exceeds maximum");
    
    Stack* stack = (Stack*)malloc(sizeof(Stack));
    REQUIRE_NOT_NULL(stack);
    
    stack->data = (int*)malloc(capacity * sizeof(int));
    if (!stack->data) {
        free(stack);
        REQUIRE(0, "Failed to allocate stack data");
    }
    
    stack->capacity = capacity;
    stack->size = 0;
    
    // 后置条件
    ENSURE(stack->size == 0, "New stack must be empty");
    ENSURE(stack->capacity == capacity, "Capacity must be set correctly");
    
    stack_check_invariant(stack);
    
    printf("[Stack] Created with capacity %zu\n", capacity);
    return stack;
}

void stack_destroy(Stack* stack) {
    // 前置条件
    REQUIRE_NOT_NULL(stack);
    stack_check_invariant(stack);
    
    printf("[Stack] Destroyed (had %zu elements)\n", stack->size);
    
    free(stack->data);
    free(stack);
}

void stack_push(Stack* stack, int value) {
    // 前置条件
    REQUIRE_NOT_NULL(stack);
    REQUIRE(!stack_is_full(stack), "Cannot push to full stack");
    stack_check_invariant(stack);
    
    size_t old_size = stack->size;
    
    // 操作
    stack->data[stack->size++] = value;
    
    // 后置条件
    ENSURE(stack->size == old_size + 1, "Size must increase by 1");
    ENSURE(stack->data[stack->size - 1] == value, "Value must be at top");
    
    stack_check_invariant(stack);
}

int stack_pop(Stack* stack) {
    // 前置条件
    REQUIRE_NOT_NULL(stack);
    REQUIRE(!stack_is_empty(stack), "Cannot pop from empty stack");
    stack_check_invariant(stack);
    
    size_t old_size = stack->size;
    
    // 操作
    int value = stack->data[--stack->size];
    
    // 后置条件
    ENSURE(stack->size == old_size - 1, "Size must decrease by 1");
    
    stack_check_invariant(stack);
    
    return value;
}

int stack_peek(const Stack* stack) {
    // 前置条件
    REQUIRE_NOT_NULL(stack);
    REQUIRE(!stack_is_empty(stack), "Cannot peek empty stack");
    stack_check_invariant(stack);
    
    return stack->data[stack->size - 1];
}

size_t stack_size(const Stack* stack) {
    REQUIRE_NOT_NULL(stack);
    return stack->size;
}

int stack_is_empty(const Stack* stack) {
    REQUIRE_NOT_NULL(stack);
    return stack->size == 0;
}

int stack_is_full(const Stack* stack) {
    REQUIRE_NOT_NULL(stack);
    return stack->size >= stack->capacity;
}

void stack_print(const Stack* stack) {
    REQUIRE_NOT_NULL(stack);
    
    printf("Stack[%zu/%zu]: ", stack->size, stack->capacity);
    if (stack->size == 0) {
        printf("(empty)");
    } else {
        printf("[ ");
        for (size_t i = 0; i < stack->size; i++) {
            printf("%d ", stack->data[i]);
        }
        printf("] <- top");
    }
    printf("\n");
}
```

### 带契约的二分查找

```c
// binary_search.c
#include "contract.h"
#include <stdio.h>

// 验证数组已排序（不变式辅助函数）
static int is_sorted(const int* arr, size_t size) {
    for (size_t i = 1; i < size; i++) {
        if (arr[i] < arr[i-1]) return 0;
    }
    return 1;
}

// 带契约的二分查找
// 返回: 找到返回索引，未找到返回-1
int binary_search(const int* arr, size_t size, int target) {
    // 前置条件
    REQUIRE_NOT_NULL(arr);
    REQUIRE(size > 0, "Array must not be empty");
    REQUIRE(is_sorted(arr, size), "Array must be sorted");
    
    size_t left = 0;
    size_t right = size;
    
    while (left < right) {
        // 循环不变式
        INVARIANT(left <= right, "Search range must be valid");
        INVARIANT(right <= size, "Right bound must not exceed size");
        
        size_t mid = left + (right - left) / 2;
        
        // 防止越界
        INVARIANT(mid < size, "Mid index must be valid");
        
        if (arr[mid] == target) {
            // 后置条件：找到目标
            ENSURE(arr[mid] == target, "Found element must equal target");
            return (int)mid;
        } else if (arr[mid] < target) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    // 后置条件：未找到时，目标确实不在数组中
    #ifndef NDEBUG
    for (size_t i = 0; i < size; i++) {
        ENSURE(arr[i] != target, "Target must not exist if not found");
    }
    #endif
    
    return -1;
}

// 带契约的安全除法
int safe_divide(int dividend, int divisor, int* result) {
    // 前置条件
    REQUIRE(divisor != 0, "Divisor must not be zero");
    REQUIRE_NOT_NULL(result);
    
    *result = dividend / divisor;
    
    // 后置条件
    ENSURE(*result * divisor <= dividend, "Result must be correct floor");
    ENSURE((*result + 1) * divisor > dividend || *result * divisor == dividend,
           "Result must be exact or floor");
    
    return 0;
}
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include "stack_contract.h"
#include "contract.h"

// 带契约的函数示例
int calculate_factorial(int n) {
    REQUIRE_IN_RANGE(n, 0, 12);  // 防止溢出
    
    int result = 1;
    for (int i = 2; i <= n; i++) {
        result *= i;
    }
    
    ENSURE(result > 0, "Factorial must be positive");
    return result;
}

int main() {
    printf("=== Design by Contract Demo ===\n\n");
    
    // 测试栈
    printf("--- Stack with Contracts ---\n");
    Stack* stack = stack_create(5);
    
    stack_push(stack, 10);
    stack_push(stack, 20);
    stack_push(stack, 30);
    stack_print(stack);
    
    printf("Peek: %d\n", stack_peek(stack));
    printf("Pop: %d\n", stack_pop(stack));
    stack_print(stack);
    
    // 测试二分查找
    printf("\n--- Binary Search with Contracts ---\n");
    int sorted_arr[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19};
    size_t arr_size = sizeof(sorted_arr) / sizeof(sorted_arr[0]);
    
    int targets[] = {7, 1, 19, 10, 0};
    for (size_t i = 0; i < sizeof(targets)/sizeof(targets[0]); i++) {
        int idx = binary_search(sorted_arr, arr_size, targets[i]);
        if (idx >= 0) {
            printf("Found %d at index %d\n", targets[i], idx);
        } else {
            printf("%d not found\n", targets[i]);
        }
    }
    
    // 测试阶乘
    printf("\n--- Factorial with Contracts ---\n");
    for (int i = 0; i <= 10; i += 2) {
        printf("%d! = %d\n", i, calculate_factorial(i));
    }
    
    // 清理
    stack_destroy(stack);
    
    // 演示契约违反（取消注释以测试）
    printf("\n--- Contract Violations (uncomment to test) ---\n");
    
    // 违反前置条件：空栈pop
    // Stack* empty = stack_create(3);
    // stack_pop(empty);  // 会触发前置条件失败
    
    // 违反前置条件：超出范围
    // calculate_factorial(15);  // 会触发前置条件失败
    
    // 违反前置条件：空指针
    // stack_push(NULL, 10);  // 会触发前置条件失败
    
    printf("All tests passed!\n");
    
    return 0;
}

/* 输出示例:
=== Design by Contract Demo ===

--- Stack with Contracts ---
[Stack] Created with capacity 5
Stack[3/5]: [ 10 20 30 ] <- top
Peek: 30
Pop: 30
Stack[2/5]: [ 10 20 ] <- top

--- Binary Search with Contracts ---
Found 7 at index 3
Found 1 at index 0
Found 19 at index 9
10 not found
0 not found

--- Factorial with Contracts ---
0! = 1
2! = 2
4! = 24
6! = 720
8! = 40320
10! = 3628800

[Stack] Destroyed (had 2 elements)

--- Contract Violations (uncomment to test) ---
All tests passed!
*/
```

## 优缺点

### 优点
- **明确责任边界**：调用者和实现者职责清晰
- **早期错误检测**：在开发阶段就能发现bug
- **自文档化**：契约本身就是文档
- **可禁用**：发布版本可移除检查，零开销
- **易于定位bug**：知道是哪一方违反了契约

### 缺点
- 增加代码量
- 调试版本性能下降
- 需要团队遵守约定
- 不变式检查可能代价高昂
- 与其他错误处理机制需要配合使用

