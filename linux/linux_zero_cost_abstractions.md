# Linux Kernel Zero-Cost Abstractions (v3.2)

## Overview

This document explains how the Linux kernel achieves **abstraction without runtime overhead**, using techniques like inline functions, macros, and compile-time optimization.

---

## What is Zero-Cost Abstraction?

```
+------------------------------------------------------------------+
|  ZERO-COST ABSTRACTION PRINCIPLE                                 |
+------------------------------------------------------------------+

    "You don't pay for what you don't use,
     and what you do use, you couldn't hand-code better"
    
    +----------------------------------------------------------+
    | Abstraction should:                                       |
    | 1. NOT add runtime overhead                               |
    | 2. Be as efficient as hand-written code                   |
    | 3. Provide type safety and readability                    |
    | 4. Be optimized away at compile time                      |
    +----------------------------------------------------------+

    COST SPECTRUM:
    
    +----------------+
    | High overhead  |  Virtual functions (C++)
    |                |  Function pointers (indirect call)
    |                |  Dynamic dispatch
    +----------------+
            |
            v
    +----------------+
    | Medium overhead|  Regular function calls
    |                |  (call/ret instructions)
    +----------------+
            |
            v
    +----------------+
    | Zero overhead  |  Inline functions
    |                |  Macros
    |                |  static inline
    +----------------+
```

**中文解释：**
- 零开销抽象原则：不用的不付费，用的不比手写差
- 应该在编译期优化掉，提供类型安全和可读性
- 开销等级：虚函数 > 普通函数 > 内联函数/宏

---

## Inline Functions

### When to Use Inline

```c
/* Good candidate for inline: Small, frequently called */
static inline int atomic_read(const atomic_t *v)
{
    return (*(volatile int *)&(v)->counter);
}

/* Good candidate: Wrapper that adds type safety */
static inline struct task_struct *get_current(void)
{
    return current_thread_info()->task;
}

/* BAD candidate: Large function - inlining wastes icache */
static inline int large_function(void)
{
    /* 100 lines of code */
    /* This should NOT be inline! */
}
```

### Inline vs Regular Function

```
+------------------------------------------------------------------+
|  INLINE FUNCTION COMPILATION                                     |
+------------------------------------------------------------------+

    Source:
    
    static inline int add(int a, int b) { return a + b; }
    
    int compute(void) {
        return add(1, 2) + add(3, 4);
    }
    
    After inlining:
    
    int compute(void) {
        return (1 + 2) + (3 + 4);  // Inlined!
    }
    
    After constant folding:
    
    int compute(void) {
        return 10;  // Computed at compile time!
    }
    
    Assembly:
    
    compute:
        mov eax, 10    ; Single instruction!
        ret

+------------------------------------------------------------------+
|  REGULAR FUNCTION (not inlined)                                  |
+------------------------------------------------------------------+

    Assembly with function call:
    
    compute:
        push rbp           ; Setup
        mov rbp, rsp
        mov edi, 1         ; Args for add(1,2)
        mov esi, 2
        call add           ; CALL overhead
        mov ebx, eax       ; Save result
        mov edi, 3         ; Args for add(3,4)
        mov esi, 4
        call add           ; CALL overhead
        add eax, ebx       ; Combine
        pop rbp
        ret
```

**中文解释：**
- 内联函数在编译时展开，消除调用开销
- 编译器还可进行常量折叠，在编译期计算结果
- 大函数不应内联，会浪费指令缓存

---

## Ops Tables vs Virtual Dispatch

### Virtual Dispatch Cost (C++)

```cpp
// C++ virtual function - hidden cost
class Base {
    virtual void method();  // vtable pointer + indirect call
};

class Derived : public Base {
    void method() override;
};

void call(Base *b) {
    b->method();
    // Assembly:
    // mov rax, [rdi]        ; Load vtable pointer
    // mov rax, [rax + 0]    ; Load method pointer from vtable
    // call rax              ; Indirect call
}
```

### Ops Table Cost (Linux)

```c
/* Linux ops table - explicit, same cost but transparent */
struct ops {
    void (*method)(void *obj);
};

struct object {
    const struct ops *ops;
};

void call(struct object *obj) {
    obj->ops->method(obj);
    // Assembly:
    // mov rax, [rdi]        ; Load ops pointer
    // mov rax, [rax + 0]    ; Load method pointer
    // call rax              ; Indirect call
}
```

```
+------------------------------------------------------------------+
|  COMPARISON: VIRTUAL vs OPS TABLE                                |
+------------------------------------------------------------------+

    COST: Same! Both are indirect function calls.
    
    DIFFERENCE: Transparency and control
    
    Virtual (C++):
    +----------------------------------------------------------+
    | - vtable hidden by compiler                               |
    | - RTTI adds extra memory                                  |
    | - May have destructor overhead                            |
    | - Compiler decides layout                                 |
    +----------------------------------------------------------+
    
    Ops Table (Linux):
    +----------------------------------------------------------+
    | - Table explicitly defined                                |
    | - No RTTI overhead                                        |
    | - No hidden destructor calls                              |
    | - Developer controls layout                               |
    | - Can have multiple ops tables per object                 |
    | - Can change ops at runtime                               |
    +----------------------------------------------------------+
```

**中文解释：**
- 虚函数和 ops 表的调用开销相同（都是间接调用）
- 区别在于透明度和控制：ops 表完全显式，无隐藏开销

---

## container_of and Type Recovery

### container_of Macro

From `include/linux/kernel.h`:

```c
#define container_of(ptr, type, member) ({                      \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);        \
    (type *)( (char *)__mptr - offsetof(type,member) );})
```

### Zero-Cost Type Recovery

```
+------------------------------------------------------------------+
|  container_of: ZERO RUNTIME COST                                 |
+------------------------------------------------------------------+

    Source:
    
    struct my_object {
        int field1;
        struct list_head list;  // offset = 4
        int field2;
    };
    
    struct my_object *get_object(struct list_head *l) {
        return container_of(l, struct my_object, list);
    }
    
    Expansion:
    
    struct my_object *get_object(struct list_head *l) {
        return (struct my_object *)((char *)l - 4);  // Compile-time constant!
    }
    
    Assembly:
    
    get_object:
        lea rax, [rdi - 4]   ; Single instruction: ptr - offset
        ret
    
    COST: One subtraction, computed at compile time!
```

### Comparison with Alternative Approaches

```c
/* Alternative 1: Store parent pointer (RUNTIME COST) */
struct bad_list_node {
    struct bad_list_node *next;
    void *parent;              /* Extra 8 bytes per node! */
};

/* Alternative 2: Wrapper structure (RUNTIME + MEMORY COST) */
struct wrapper {
    struct generic_list_node node;  /* Generic list */
    void *object;                   /* Pointer to actual object */
};

/* container_of approach: ZERO COST */
struct good_object {
    int data;
    struct list_head list;  /* Embedded, no extra pointer */
};
/* Recovery: compile-time pointer arithmetic */
```

**中文解释：**
- `container_of` 在编译期计算偏移量
- 生成代码仅一条减法指令
- 替代方案需要额外内存和运行时开销

---

## Performance-Critical Macros

### likely() and unlikely()

```c
/* Compiler hints for branch prediction */
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/* Usage */
if (likely(condition)) {
    /* Common case - CPU pipeline optimized */
}

if (unlikely(error)) {
    /* Rare case - cold code path */
}
```

### Branch Optimization Effect

```
+------------------------------------------------------------------+
|  BRANCH PREDICTION OPTIMIZATION                                  |
+------------------------------------------------------------------+

    Without hint:
    
        test eax, eax
        jz error_path        ; May mispredict
        ; common path
        ...
    error_path:
        ; error handling
    
    With likely():
    
        test eax, eax
        jz error_path        ; Predicted not-taken
        ; common path        ; Falls through (fast)
        ...
    
    error_path:              ; Placed after ret (cold)
        ; error handling
    
    BENEFIT:
    - Common path is straight-line code
    - CPU pipeline stays full
    - Branch predictor initialized correctly
```

### min/max Macros

```c
/* Type-safe min/max without function call */
#define min(x, y) ({                \
    typeof(x) _min1 = (x);          \
    typeof(y) _min2 = (y);          \
    (void) (&_min1 == &_min2);      /* Type check! */\
    _min1 < _min2 ? _min1 : _min2;  \
})

/* Usage */
int a = 5, b = 3;
int c = min(a, b);  /* No function call, just comparison */

/* Assembly */
cmp eax, ebx
cmovg eax, ebx      ; Conditional move, no branch!
```

**中文解释：**
- `likely()`/`unlikely()` 提示编译器优化分支预测
- `min()`/`max()` 宏无函数调用，仅生成比较指令
- 类型检查在编译期完成，无运行时开销

---

## Static Analysis Tricks

### BUILD_BUG_ON

```c
/* Compile-time assertion - zero runtime cost */
#define BUILD_BUG_ON(condition) \
    ((void)sizeof(char[1 - 2*!!(condition)]))

/* Usage */
BUILD_BUG_ON(sizeof(struct my_struct) != 64);
/* If condition is true, array has negative size = compile error! */

/* Runtime equivalent (HAS cost) */
if (sizeof(struct my_struct) != 64)
    BUG();  /* Runtime check - wastes cycles */
```

### Constant Expression Optimization

```c
/* Compiler evaluates constant expressions */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int array[10];
int size = ARRAY_SIZE(array);  /* Computed at compile time: 10 */

/* Assembly */
mov eax, 10   ; Direct constant, no division!
```

### Type Checking Without Cost

```c
/* Ensure types match at compile time */
#define typecheck(type, x) \
({  type __dummy; \
    typeof(x) __dummy2; \
    (void)(&__dummy == &__dummy2); /* Compile error if mismatch */ \
    1; \
})

/* Usage */
void foo(unsigned long x) {
    typecheck(unsigned long, x);  /* Validates type */
    /* Generates no code! Optimizer removes it */
}
```

**中文解释：**
- `BUILD_BUG_ON`：编译期断言，失败时编译错误
- `ARRAY_SIZE`：编译期计算数组大小
- `typecheck`：编译期类型检查，生成零代码

---

## When Macros Beat Functions

```
+------------------------------------------------------------------+
|  MACROS vs FUNCTIONS                                             |
+------------------------------------------------------------------+

    USE MACROS WHEN:
    +----------------------------------------------------------+
    | 1. Type-generic operations (min, max, container_of)       |
    | 2. Compile-time constants needed                          |
    | 3. Token pasting required (##)                            |
    | 4. Stringification needed (#)                             |
    | 5. Statement expressions (GNU extension)                  |
    +----------------------------------------------------------+
    
    USE INLINE FUNCTIONS WHEN:
    +----------------------------------------------------------+
    | 1. Type safety is important                               |
    | 2. Debugging with source-level debugger                   |
    | 3. Taking address of function needed                      |
    | 4. Recursive calls                                        |
    | 5. Multiple evaluation of arguments is problematic        |
    +----------------------------------------------------------+
```

### Macro Dangers

```c
/* DANGER: Multiple evaluation */
#define SQUARE(x) ((x) * (x))

int y = SQUARE(++i);  /* BUG: ++i evaluated twice! */

/* SAFE: Inline function */
static inline int square(int x) { return x * x; }

int y = square(++i);  /* Safe: ++i evaluated once */
```

### When Both Are Zero-Cost

```c
/* Both are zero-cost when used correctly */

/* Macro version */
#define get_field(ptr, type, field) \
    (((type *)(ptr))->field)

/* Inline version */
static inline int get_field_inline(struct obj *ptr) {
    return ptr->field;
}

/* Both generate same code:
   mov eax, [rdi + offset]
*/
```

**中文解释：**
- 宏适用场景：泛型操作、编译期常量、token 拼接
- 内联函数适用场景：类型安全、调试、避免多次求值
- 两者都可实现零开销

---

## User-Space Application

```c
/* user_space_zero_cost.c */

#include <stdio.h>
#include <stddef.h>

/*---------------------------------------------------------
 * container_of - Zero-cost type recovery
 *---------------------------------------------------------*/
#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})

/*---------------------------------------------------------
 * likely/unlikely - Branch hints
 *---------------------------------------------------------*/
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

/*---------------------------------------------------------
 * Type-safe min/max
 *---------------------------------------------------------*/
#define min(x, y) ({                        \
    typeof(x) _min1 = (x);                  \
    typeof(y) _min2 = (y);                  \
    (void) (&_min1 == &_min2);              \
    _min1 < _min2 ? _min1 : _min2;          \
})

#define max(x, y) ({                        \
    typeof(x) _max1 = (x);                  \
    typeof(y) _max2 = (y);                  \
    (void) (&_max1 == &_max2);              \
    _max1 > _max2 ? _max1 : _max2;          \
})

/*---------------------------------------------------------
 * Compile-time assertions
 *---------------------------------------------------------*/
#define BUILD_BUG_ON(cond) \
    ((void)sizeof(char[1 - 2*!!(cond)]))

#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0]))

/*---------------------------------------------------------
 * Example: Zero-cost intrusive list
 *---------------------------------------------------------*/
struct list_head {
    struct list_head *next, *prev;
};

struct my_object {
    int id;
    char name[32];
    struct list_head list;  /* Embedded list node */
};

/* Compile-time computation of offset */
void demonstrate_container_of(void)
{
    struct my_object obj = { .id = 42, .name = "test" };
    struct list_head *list_ptr = &obj.list;
    
    /* Zero-cost recovery - just pointer arithmetic */
    struct my_object *recovered = container_of(list_ptr, 
                                               struct my_object, 
                                               list);
    
    printf("Recovered object id: %d\n", recovered->id);
    printf("offset of list: %zu\n", offsetof(struct my_object, list));
    
    /* The subtraction is computed at compile time! */
}

/*---------------------------------------------------------
 * Example: Branch optimization
 *---------------------------------------------------------*/
int process_data(int *data, int len)
{
    int sum = 0;
    
    /* Compiler optimizes for the common case */
    for (int i = 0; i < len; i++) {
        if (likely(data[i] >= 0)) {
            sum += data[i];  /* Fast path */
        } else {
            /* Slow path - error handling */
            if (unlikely(data[i] < -1000)) {
                return -1;  /* Very rare case */
            }
            sum += -data[i];
        }
    }
    
    return sum;
}

/*---------------------------------------------------------
 * Example: Compile-time checks
 *---------------------------------------------------------*/
struct config {
    int flags;
    char data[60];
};

void check_struct_layout(void)
{
    /* These are compile-time checks - no runtime cost! */
    BUILD_BUG_ON(sizeof(struct config) != 64);
    BUILD_BUG_ON(offsetof(struct config, data) != 4);
    
    printf("Struct size verified at compile time: %zu\n", 
           sizeof(struct config));
}

/*---------------------------------------------------------
 * Example: Type-safe operations
 *---------------------------------------------------------*/
void demonstrate_min_max(void)
{
    int a = 5, b = 3;
    float x = 1.5f, y = 2.5f;
    
    /* Type-safe, zero-cost min/max */
    int min_int = min(a, b);
    float min_float = min(x, y);
    
    printf("min(5, 3) = %d\n", min_int);
    printf("min(1.5, 2.5) = %.1f\n", min_float);
    
    /* This would cause compile error (type mismatch):
     * int bad = min(a, x);
     */
}

int main(void)
{
    printf("=== container_of demo ===\n");
    demonstrate_container_of();
    
    printf("\n=== Compile-time checks ===\n");
    check_struct_layout();
    
    printf("\n=== min/max demo ===\n");
    demonstrate_min_max();
    
    printf("\n=== Array size (compile-time) ===\n");
    int arr[] = {1, 2, 3, 4, 5};
    printf("Array has %zu elements\n", ARRAY_SIZE(arr));
    
    return 0;
}
```

**中文解释：**
- 用户态零开销抽象示例：
  1. `container_of`：编译期计算偏移量
  2. `likely()`/`unlikely()`：分支预测优化
  3. `min()`/`max()`：类型安全的编译期展开
  4. `BUILD_BUG_ON`：编译期断言

---

## Summary

```
+------------------------------------------------------------------+
|  ZERO-COST ABSTRACTION SUMMARY                                   |
+------------------------------------------------------------------+

    TECHNIQUES:
    +----------------------------------------------------------+
    | 1. INLINE FUNCTIONS                                       |
    |    - Eliminate call/ret overhead                          |
    |    - Enable constant folding                              |
    |    - Use for small, frequently called functions           |
    +----------------------------------------------------------+
    | 2. MACROS                                                 |
    |    - Type-generic operations                              |
    |    - Compile-time computation                             |
    |    - Token manipulation                                   |
    +----------------------------------------------------------+
    | 3. COMPILE-TIME CONSTANTS                                 |
    |    - offsetof(), sizeof()                                 |
    |    - Constant expressions                                 |
    |    - Computed by compiler, not at runtime                 |
    +----------------------------------------------------------+
    | 4. BRANCH HINTS                                           |
    |    - likely()/unlikely()                                  |
    |    - Optimize CPU pipeline                                |
    |    - No runtime cost, just code layout                    |
    +----------------------------------------------------------+
    
    KERNEL EXAMPLES:
    +----------------------------------------------------------+
    | Construct         | Zero-Cost Technique                  |
    |-------------------|--------------------------------------|
    | container_of      | Compile-time pointer arithmetic      |
    | atomic_read       | Inline volatile read                 |
    | list_entry        | Macro + offsetof                     |
    | BUILD_BUG_ON      | Negative array size trick            |
    | min/max           | Statement expression macro           |
    | get_current()     | Inline thread-local access           |
    +----------------------------------------------------------+
    
    RULES:
    +----------------------------------------------------------+
    | 1. Measure before optimizing                              |
    | 2. Prefer inline functions for type safety                |
    | 3. Use macros for generics and compile-time               |
    | 4. Don't inline large functions (icache pressure)         |
    | 5. Use compiler explorer to verify optimization           |
    +----------------------------------------------------------+
```

**中文总结：**
零开销抽象技术：
1. **内联函数**：消除调用开销，启用常量折叠
2. **宏**：泛型操作，编译期计算
3. **编译期常量**：`offsetof`、`sizeof` 在编译期求值
4. **分支提示**：优化 CPU 流水线，无运行时开销

原则：先测量再优化、内联用于类型安全、宏用于泛型、大函数不内联

