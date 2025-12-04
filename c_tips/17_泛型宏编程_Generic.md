# 泛型宏编程 (Generic Macros with _Generic)

## 定义

泛型宏编程是利用C11引入的`_Generic`关键字实现编译时类型分发的技术。它允许根据表达式的类型在编译时选择不同的代码路径，实现类似C++函数重载的效果，且没有运行时开销。

## 适用场景

- 类型安全的打印函数
- 类型相关的数学运算（abs、max、min等）
- 序列化/反序列化函数的类型分发
- 类型安全的容器操作
- 调试输出的格式化
- 跨类型的统一API设计
- 替代不安全的void*泛型

## ASCII 图解

```
+------------------------------------------------------------------------+
|                    GENERIC MACROS (_Generic)                            |
+------------------------------------------------------------------------+
|                                                                         |
|   _Generic SYNTAX:                                                      |
|   +------------------------------------------------------------------+ |
|   | _Generic(controlling_expression,                                  | |
|   |          type1: result1,                                          | |
|   |          type2: result2,                                          | |
|   |          ...                                                      | |
|   |          default: default_result)                                 | |
|   +------------------------------------------------------------------+ |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   COMPILE-TIME TYPE DISPATCH:                                           |
|                                                                         |
|   print_value(x)                                                        |
|        |                                                                |
|        v                                                                |
|   +----------+                                                          |
|   | typeof(x)|                                                          |
|   +----+-----+                                                          |
|        |                                                                |
|   +----+--------+--------+--------+--------+                            |
|   |    |        |        |        |        |                            |
|   v    v        v        v        v        v                            |
|  int  float  double  char*    other    (compile-time)                   |
|   |    |        |        |        |                                     |
|   v    v        v        v        v                                     |
| "%d"  "%f"    "%lf"   "%s"   "<unknown>"                               |
|                                                                         |
|   Result: Type-appropriate format string, ZERO runtime overhead!        |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   TRADITIONAL APPROACH (Unsafe):     GENERIC APPROACH (Type-Safe):      |
|                                                                         |
|   +-------------------------+        +-------------------------+        |
|   | void print(void* x) {   |        | #define print(x)        |        |
|   |   // How to print?      |        | _Generic((x),           |        |
|   |   // What type is x?    |        |   int: print_int,       |        |
|   |   // Runtime check?     |        |   float: print_float,   |        |
|   |   // DANGEROUS!         |        |   char*: print_str      |        |
|   | }                       |        | )(x)                    |        |
|   +-------------------------+        +-------------------------+        |
|                                                                         |
|   Problem: No type info          Benefit: Compiler knows type           |
|   Problem: Easy to misuse        Benefit: Cannot misuse                 |
|                                                                         |
+------------------------------------------------------------------------+
|                                                                         |
|   MACRO EXPANSION EXAMPLE:                                              |
|                                                                         |
|   Source:                          After preprocessing:                 |
|   +--------------------+           +--------------------+               |
|   | int x = 42;        |           | int x = 42;        |               |
|   | print_value(x);    |    ==>    | print_int(x);      |               |
|   +--------------------+           +--------------------+               |
|                                                                         |
|   +--------------------+           +--------------------+               |
|   | double y = 3.14;   |           | double y = 3.14;   |               |
|   | print_value(y);    |    ==>    | print_double(y);   |               |
|   +--------------------+           +--------------------+               |
|                                                                         |
+------------------------------------------------------------------------+
```

**图解说明：**

上图展示了`_Generic`的工作原理。它在编译时检查控制表达式的类型，然后选择对应的分支。与传统的void*方式不同，_Generic是完全类型安全的，编译器知道每个分支处理的具体类型。宏展开示例显示，对于不同类型的变量，同一个宏会展开成不同的函数调用，这一切都在编译时完成，零运行时开销。

## 实现方法

1. 为每种类型实现具体函数
2. 使用`_Generic`构建类型选择宏
3. 可以组合多个`_Generic`实现复杂逻辑
4. 使用`default`处理未列出的类型

## C语言代码示例

### 基础泛型宏

```c
// generic_print.h
#ifndef GENERIC_PRINT_H
#define GENERIC_PRINT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// ==================== 类型特定的打印函数 ====================

static inline void print_int(int x) { 
    printf("int: %d\n", x); 
}

static inline void print_uint(unsigned int x) { 
    printf("uint: %u\n", x); 
}

static inline void print_long(long x) { 
    printf("long: %ld\n", x); 
}

static inline void print_ulong(unsigned long x) { 
    printf("ulong: %lu\n", x); 
}

static inline void print_llong(long long x) { 
    printf("llong: %lld\n", x); 
}

static inline void print_float(float x) { 
    printf("float: %f\n", x); 
}

static inline void print_double(double x) { 
    printf("double: %f\n", x); 
}

static inline void print_char(char x) { 
    printf("char: '%c' (%d)\n", x, x); 
}

static inline void print_str(const char* x) { 
    printf("string: \"%s\"\n", x ? x : "(null)"); 
}

static inline void print_ptr(const void* x) { 
    printf("pointer: %p\n", x); 
}

static inline void print_bool(_Bool x) { 
    printf("bool: %s\n", x ? "true" : "false"); 
}

// ==================== 泛型打印宏 ====================

#define print_value(x) _Generic((x),                \
    _Bool:              print_bool,                 \
    char:               print_char,                 \
    signed char:        print_int,                  \
    unsigned char:      print_uint,                 \
    short:              print_int,                  \
    unsigned short:     print_uint,                 \
    int:                print_int,                  \
    unsigned int:       print_uint,                 \
    long:               print_long,                 \
    unsigned long:      print_ulong,                \
    long long:          print_llong,                \
    unsigned long long: print_ulong,                \
    float:              print_float,                \
    double:             print_double,               \
    char*:              print_str,                  \
    const char*:        print_str,                  \
    default:            print_ptr                   \
)(x)

// ==================== 类型名称获取 ====================

#define type_name(x) _Generic((x),                  \
    _Bool:              "bool",                     \
    char:               "char",                     \
    signed char:        "signed char",              \
    unsigned char:      "unsigned char",            \
    short:              "short",                    \
    unsigned short:     "unsigned short",           \
    int:                "int",                      \
    unsigned int:       "unsigned int",             \
    long:               "long",                     \
    unsigned long:      "unsigned long",            \
    long long:          "long long",                \
    unsigned long long: "unsigned long long",       \
    float:              "float",                    \
    double:             "double",                   \
    long double:        "long double",              \
    char*:              "char*",                    \
    const char*:        "const char*",              \
    void*:              "void*",                    \
    default:            "unknown"                   \
)

#endif // GENERIC_PRINT_H
```

### 泛型数学函数

```c
// generic_math.h
#ifndef GENERIC_MATH_H
#define GENERIC_MATH_H

#include <math.h>
#include <stdlib.h>

// ==================== 绝对值 ====================

static inline int abs_int(int x) { return abs(x); }
static inline long abs_long(long x) { return labs(x); }
static inline long long abs_llong(long long x) { return llabs(x); }
static inline float abs_float(float x) { return fabsf(x); }
static inline double abs_double(double x) { return fabs(x); }

#define generic_abs(x) _Generic((x),                \
    int:        abs_int,                            \
    long:       abs_long,                           \
    long long:  abs_llong,                          \
    float:      abs_float,                          \
    double:     abs_double                          \
)(x)

// ==================== 最大值 ====================

static inline int max_int(int a, int b) { return a > b ? a : b; }
static inline long max_long(long a, long b) { return a > b ? a : b; }
static inline float max_float(float a, float b) { return a > b ? a : b; }
static inline double max_double(double a, double b) { return a > b ? a : b; }

#define generic_max(a, b) _Generic((a),             \
    int:    max_int,                                \
    long:   max_long,                               \
    float:  max_float,                              \
    double: max_double                              \
)(a, b)

// ==================== 最小值 ====================

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline long min_long(long a, long b) { return a < b ? a : b; }
static inline float min_float(float a, float b) { return a < b ? a : b; }
static inline double min_double(double a, double b) { return a < b ? a : b; }

#define generic_min(a, b) _Generic((a),             \
    int:    min_int,                                \
    long:   min_long,                               \
    float:  min_float,                              \
    double: min_double                              \
)(a, b)

// ==================== 范围限制 ====================

static inline int clamp_int(int x, int lo, int hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline float clamp_float(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline double clamp_double(double x, double lo, double hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

#define generic_clamp(x, lo, hi) _Generic((x),      \
    int:    clamp_int,                              \
    float:  clamp_float,                            \
    double: clamp_double                            \
)(x, lo, hi)

// ==================== 符号函数 ====================

static inline int sign_int(int x) { return (x > 0) - (x < 0); }
static inline int sign_float(float x) { return (x > 0) - (x < 0); }
static inline int sign_double(double x) { return (x > 0) - (x < 0); }

#define generic_sign(x) _Generic((x),               \
    int:    sign_int,                               \
    float:  sign_float,                             \
    double: sign_double                             \
)(x)

#endif // GENERIC_MATH_H
```

### 泛型容器操作

```c
// generic_container.h
#ifndef GENERIC_CONTAINER_H
#define GENERIC_CONTAINER_H

#include <stdio.h>
#include <string.h>

// ==================== 泛型交换 ====================

// 使用typeof扩展（GCC/Clang）
#define generic_swap(a, b) do {                     \
    typeof(a) _tmp = (a);                           \
    (a) = (b);                                      \
    (b) = _tmp;                                     \
} while(0)

// ==================== 数组操作 ====================

// 安全数组长度
#define array_length(arr) (sizeof(arr) / sizeof((arr)[0]))

// 泛型数组打印
static inline void print_int_array(const int* arr, size_t n) {
    printf("[");
    for (size_t i = 0; i < n; i++) {
        printf("%d%s", arr[i], i < n-1 ? ", " : "");
    }
    printf("]\n");
}

static inline void print_float_array(const float* arr, size_t n) {
    printf("[");
    for (size_t i = 0; i < n; i++) {
        printf("%.2f%s", arr[i], i < n-1 ? ", " : "");
    }
    printf("]\n");
}

static inline void print_double_array(const double* arr, size_t n) {
    printf("[");
    for (size_t i = 0; i < n; i++) {
        printf("%.2f%s", arr[i], i < n-1 ? ", " : "");
    }
    printf("]\n");
}

// 注意：数组会退化为指针，需要显式传递长度
#define print_array(arr, n) _Generic((arr),         \
    int*:       print_int_array,                    \
    const int*: print_int_array,                    \
    float*:     print_float_array,                  \
    double*:    print_double_array                  \
)(arr, n)

// ==================== 泛型比较 ====================

static inline int compare_int(const void* a, const void* b) {
    return *(const int*)a - *(const int*)b;
}

static inline int compare_float(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static inline int compare_double(const void* a, const void* b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

static inline int compare_str(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// 获取类型对应的比较函数
#define get_comparator(type) _Generic((type){0},    \
    int:          compare_int,                      \
    float:        compare_float,                    \
    double:       compare_double,                   \
    char*:        compare_str,                      \
    const char*:  compare_str                       \
)

#endif // GENERIC_CONTAINER_H
```

### 泛型序列化

```c
// generic_serialize.h
#ifndef GENERIC_SERIALIZE_H
#define GENERIC_SERIALIZE_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// ==================== 序列化到缓冲区 ====================

static inline size_t serialize_int(void* buf, int val) {
    memcpy(buf, &val, sizeof(val));
    return sizeof(val);
}

static inline size_t serialize_float(void* buf, float val) {
    memcpy(buf, &val, sizeof(val));
    return sizeof(val);
}

static inline size_t serialize_double(void* buf, double val) {
    memcpy(buf, &val, sizeof(val));
    return sizeof(val);
}

static inline size_t serialize_str(void* buf, const char* val) {
    size_t len = val ? strlen(val) : 0;
    uint32_t len32 = (uint32_t)len;
    memcpy(buf, &len32, sizeof(len32));
    if (len > 0) {
        memcpy((char*)buf + sizeof(len32), val, len);
    }
    return sizeof(len32) + len;
}

#define serialize(buf, val) _Generic((val),         \
    int:          serialize_int,                    \
    float:        serialize_float,                  \
    double:       serialize_double,                 \
    char*:        serialize_str,                    \
    const char*:  serialize_str                     \
)(buf, val)

// ==================== 从缓冲区反序列化 ====================

static inline size_t deserialize_int(const void* buf, int* val) {
    memcpy(val, buf, sizeof(*val));
    return sizeof(*val);
}

static inline size_t deserialize_float(const void* buf, float* val) {
    memcpy(val, buf, sizeof(*val));
    return sizeof(*val);
}

static inline size_t deserialize_double(const void* buf, double* val) {
    memcpy(val, buf, sizeof(*val));
    return sizeof(*val);
}

// 字符串反序列化需要分配内存，这里简化处理
static inline size_t deserialize_str(const void* buf, char* val, size_t max_len) {
    uint32_t len;
    memcpy(&len, buf, sizeof(len));
    size_t copy_len = (len < max_len - 1) ? len : max_len - 1;
    memcpy(val, (const char*)buf + sizeof(len), copy_len);
    val[copy_len] = '\0';
    return sizeof(len) + len;
}

#endif // GENERIC_SERIALIZE_H
```

### 使用示例

```c
// main.c
#include <stdio.h>
#include <stdlib.h>
#include "generic_print.h"
#include "generic_math.h"
#include "generic_container.h"
#include "generic_serialize.h"

int main() {
    printf("=== Generic Macros Demo ===\n\n");
    
    // ========== 泛型打印 ==========
    printf("--- Generic Print ---\n");
    
    int i = 42;
    float f = 3.14159f;
    double d = 2.71828;
    char c = 'X';
    const char* s = "Hello, Generics!";
    _Bool b = 1;
    
    print_value(i);
    print_value(f);
    print_value(d);
    print_value(c);
    print_value(s);
    print_value(b);
    print_value(&i);  // 指针使用default
    
    // ========== 类型名称 ==========
    printf("\n--- Type Names ---\n");
    printf("i is: %s\n", type_name(i));
    printf("f is: %s\n", type_name(f));
    printf("d is: %s\n", type_name(d));
    printf("s is: %s\n", type_name(s));
    
    // ========== 泛型数学 ==========
    printf("\n--- Generic Math ---\n");
    
    printf("abs(-5) = %d\n", generic_abs(-5));
    printf("abs(-3.14f) = %f\n", generic_abs(-3.14f));
    printf("abs(-2.718) = %f\n", generic_abs(-2.718));
    
    printf("max(10, 20) = %d\n", generic_max(10, 20));
    printf("max(1.5f, 2.5f) = %f\n", generic_max(1.5f, 2.5f));
    
    printf("min(10, 20) = %d\n", generic_min(10, 20));
    printf("min(1.5, 2.5) = %f\n", generic_min(1.5, 2.5));
    
    printf("clamp(15, 0, 10) = %d\n", generic_clamp(15, 0, 10));
    printf("clamp(-5.0, 0.0, 10.0) = %f\n", generic_clamp(-5.0, 0.0, 10.0));
    
    printf("sign(-42) = %d\n", generic_sign(-42));
    printf("sign(0) = %d\n", generic_sign(0));
    printf("sign(3.14) = %d\n", generic_sign(3.14));
    
    // ========== 泛型交换 ==========
    printf("\n--- Generic Swap ---\n");
    
    int x = 10, y = 20;
    printf("Before: x=%d, y=%d\n", x, y);
    generic_swap(x, y);
    printf("After:  x=%d, y=%d\n", x, y);
    
    double a = 1.1, b2 = 2.2;
    printf("Before: a=%.1f, b=%.1f\n", a, b2);
    generic_swap(a, b2);
    printf("After:  a=%.1f, b=%.1f\n", a, b2);
    
    // ========== 泛型数组打印 ==========
    printf("\n--- Generic Array Print ---\n");
    
    int int_arr[] = {1, 2, 3, 4, 5};
    float float_arr[] = {1.1f, 2.2f, 3.3f};
    double double_arr[] = {1.11, 2.22, 3.33, 4.44};
    
    printf("int array: ");
    print_array(int_arr, array_length(int_arr));
    
    printf("float array: ");
    print_array(float_arr, array_length(float_arr));
    
    printf("double array: ");
    print_array(double_arr, array_length(double_arr));
    
    // ========== 泛型排序 ==========
    printf("\n--- Generic Sort (using qsort) ---\n");
    
    int nums[] = {64, 34, 25, 12, 22, 11, 90};
    printf("Before: ");
    print_array(nums, array_length(nums));
    
    qsort(nums, array_length(nums), sizeof(nums[0]), get_comparator(int));
    
    printf("After:  ");
    print_array(nums, array_length(nums));
    
    // ========== 泛型序列化 ==========
    printf("\n--- Generic Serialize ---\n");
    
    char buffer[256];
    size_t offset = 0;
    
    int val_i = 12345;
    float val_f = 3.14159f;
    const char* val_s = "Hello";
    
    offset += serialize(buffer + offset, val_i);
    offset += serialize(buffer + offset, val_f);
    offset += serialize(buffer + offset, val_s);
    
    printf("Serialized %zu bytes\n", offset);
    
    // 反序列化
    int out_i;
    float out_f;
    char out_s[64];
    size_t read_offset = 0;
    
    read_offset += deserialize_int(buffer + read_offset, &out_i);
    read_offset += deserialize_float(buffer + read_offset, &out_f);
    read_offset += deserialize_str(buffer + read_offset, out_s, sizeof(out_s));
    
    printf("Deserialized: int=%d, float=%f, str=\"%s\"\n", out_i, out_f, out_s);
    
    printf("\n=== All tests complete ===\n");
    
    return 0;
}

/* 输出示例:
=== Generic Macros Demo ===

--- Generic Print ---
int: 42
float: 3.141590
double: 2.718280
char: 'X' (88)
string: "Hello, Generics!"
bool: true
pointer: 0x7ffd12345678

--- Type Names ---
i is: int
f is: float
d is: double
s is: const char*

--- Generic Math ---
abs(-5) = 5
abs(-3.14f) = 3.141590
abs(-2.718) = 2.718000
max(10, 20) = 20
max(1.5f, 2.5f) = 2.500000
min(10, 20) = 10
min(1.5, 2.5) = 1.500000
clamp(15, 0, 10) = 10
clamp(-5.0, 0.0, 10.0) = 0.000000
sign(-42) = -1
sign(0) = 0
sign(3.14) = 1

--- Generic Swap ---
Before: x=10, y=20
After:  x=20, y=10
Before: a=1.1, b=2.2
After:  a=2.2, b=1.1

--- Generic Array Print ---
int array: [1, 2, 3, 4, 5]
float array: [1.10, 2.20, 3.30]
double array: [1.11, 2.22, 3.33, 4.44]

--- Generic Sort (using qsort) ---
Before: [64, 34, 25, 12, 22, 11, 90]
After:  [11, 12, 22, 25, 34, 64, 90]

--- Generic Serialize ---
Serialized 17 bytes
Deserialized: int=12345, float=3.141590, str="Hello"

=== All tests complete ===
*/
```

## 优缺点

### 优点
- **类型安全**：编译时类型检查
- **零运行时开销**：完全在编译时解析
- **类似函数重载**：C语言也能有重载体验
- **代码简洁**：统一的API处理多种类型
- **标准C11**：不需要编译器扩展

### 缺点
- 需要为每种类型提供实现
- 宏展开错误信息不友好
- 不能处理用户自定义类型（除非用default）
- 需要C11支持
- 复杂的_Generic表达式难以阅读

