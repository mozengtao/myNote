# Topic 20: constexpr and Compile-Time Evaluation

## 1. Problem Statement

### What real engineering problem does this solve?

Move computation from runtime to compile time:

```
RUNTIME COMPUTATION                  COMPILE-TIME COMPUTATION
┌─────────────────────────────────┐  ┌─────────────────────────────────┐
│ int factorial(int n) {          │  │ constexpr int factorial(int n) {│
│     int result = 1;             │  │     return n <= 1 ? 1           │
│     for (int i = 2; i <= n; ++i)│  │            : n * factorial(n-1);│
│         result *= i;            │  │ }                               │
│     return result;              │  │                                 │
│ }                               │  │ constexpr int fact10 = factorial│
│                                 │  │                          (10);  │
│ int x = factorial(10);          │  │ // fact10 = 3628800 at compile  │
│ // Computed every time          │  │ // time - zero runtime cost!    │
└─────────────────────────────────┘  └─────────────────────────────────┘
```

### Benefits

- Zero runtime cost for constant data
- Compile-time error detection
- Smaller binary (computed values embedded)
- Enable template metaprogramming with normal syntax

**中文说明：**
constexpr 让函数和变量在编译时求值。编译期计算的结果直接嵌入可执行文件，运行时零开销。这对于查找表、数学常量、配置值等非常有用。C++14/17/20 持续扩展了 constexpr 的能力。

---

## 2. Core Idea

### constexpr Variables

```cpp
constexpr int max_size = 100;        // Must be known at compile time
constexpr double pi = 3.14159265359;
constexpr int arr[] = {1, 2, 3, 4, 5};

// Can be used in compile-time contexts
std::array<int, max_size> buffer;    // OK: max_size is constexpr
static_assert(max_size > 0);         // OK: compile-time check
```

### constexpr Functions

```cpp
constexpr int square(int x) {
    return x * x;
}

// Called at compile time if possible
constexpr int s = square(5);         // Computed at compile time

// Can also be called at runtime
int n;
std::cin >> n;
int result = square(n);              // Computed at runtime
```

**中文说明：**
- **constexpr 变量**：必须在编译时初始化
- **constexpr 函数**：如果输入是编译时常量，则编译时求值；否则运行时求值
- C++14 允许 constexpr 函数中使用循环和局部变量
- C++17 允许 constexpr if
- C++20 允许 constexpr 动态内存分配和虚函数

---

## 3. Idiomatic C++ Techniques

### Compile-Time Lookup Tables

```cpp
constexpr std::array<int, 256> makePopCountTable() {
    std::array<int, 256> table{};
    for (int i = 0; i < 256; ++i) {
        int count = 0;
        int n = i;
        while (n) {
            count += n & 1;
            n >>= 1;
        }
        table[i] = count;
    }
    return table;
}

constexpr auto popCountTable = makePopCountTable();  // Compile-time!

int popCount(unsigned int n) {
    return popCountTable[n & 0xFF] 
         + popCountTable[(n >> 8) & 0xFF]
         + popCountTable[(n >> 16) & 0xFF]
         + popCountTable[(n >> 24) & 0xFF];
}
```

### if constexpr (C++17)

```cpp
template<typename T>
auto process(T value) {
    if constexpr (std::is_integral_v<T>) {
        return value * 2;        // Only compiled for integers
    } else if constexpr (std::is_floating_point_v<T>) {
        return value * 2.0;      // Only compiled for floats
    } else {
        return value.process();  // Only compiled for other types
    }
}
```

### consteval (C++20) - Must Be Compile-Time

```cpp
consteval int must_be_compile_time(int x) {
    return x * x;
}

constexpr int a = must_be_compile_time(5);  // OK
int b = must_be_compile_time(5);            // OK: literal argument

int n = 5;
int c = must_be_compile_time(n);            // ERROR: n is not constexpr
```

---

## 4. Complete C++ Example

```cpp
#include <array>
#include <iostream>
#include <string_view>
#include <type_traits>

// ============================================================
// Basic constexpr functions
// ============================================================
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

constexpr int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        int c = a + b;
        a = b;
        b = c;
    }
    return b;
}

// ============================================================
// Compile-time string operations
// ============================================================
constexpr size_t strlen_ct(const char* s) {
    size_t len = 0;
    while (s[len]) ++len;
    return len;
}

constexpr bool streq_ct(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return false;
        ++a; ++b;
    }
    return *a == *b;
}

// ============================================================
// Compile-time lookup table
// ============================================================
constexpr std::array<int, 20> makeFibTable() {
    std::array<int, 20> table{};
    for (int i = 0; i < 20; ++i) {
        table[i] = fibonacci(i);
    }
    return table;
}

constexpr auto fibTable = makeFibTable();

// ============================================================
// if constexpr for type-based branching
// ============================================================
template<typename T>
constexpr auto typeInfo() {
    if constexpr (std::is_integral_v<T>) {
        return "integral";
    } else if constexpr (std::is_floating_point_v<T>) {
        return "floating";
    } else if constexpr (std::is_pointer_v<T>) {
        return "pointer";
    } else {
        return "other";
    }
}

// ============================================================
// constexpr class
// ============================================================
class Point {
    double x_, y_;
public:
    constexpr Point(double x = 0, double y = 0) : x_(x), y_(y) {}
    
    constexpr double x() const { return x_; }
    constexpr double y() const { return y_; }
    
    constexpr Point operator+(const Point& other) const {
        return Point(x_ + other.x_, y_ + other.y_);
    }
    
    constexpr double distanceSquared(const Point& other) const {
        double dx = x_ - other.x_;
        double dy = y_ - other.y_;
        return dx * dx + dy * dy;
    }
};

// ============================================================
// Compile-time validation
// ============================================================
template<size_t N>
constexpr bool isValidConfig(const std::array<int, N>& config) {
    for (size_t i = 0; i < N; ++i) {
        if (config[i] < 0) return false;
        if (config[i] > 100) return false;
    }
    return true;
}

constexpr std::array<int, 4> config = {10, 20, 30, 40};
static_assert(isValidConfig(config), "Invalid configuration!");

int main() {
    // Compile-time computation
    constexpr int fact10 = factorial(10);
    constexpr int fib15 = fibonacci(15);
    
    std::cout << "=== Compile-Time Values ===\n";
    std::cout << "factorial(10) = " << fact10 << "\n";
    std::cout << "fibonacci(15) = " << fib15 << "\n";
    
    // Compile-time string
    constexpr auto len = strlen_ct("Hello, World!");
    std::cout << "\n=== String at Compile Time ===\n";
    std::cout << "strlen_ct(\"Hello, World!\") = " << len << "\n";
    
    static_assert(streq_ct("abc", "abc"), "Strings should match");
    static_assert(!streq_ct("abc", "abd"), "Strings should differ");
    
    // Lookup table
    std::cout << "\n=== Fibonacci Table (compile-time) ===\n";
    for (int i = 0; i < 10; ++i) {
        std::cout << "fib(" << i << ") = " << fibTable[i] << "\n";
    }
    
    // Type info
    std::cout << "\n=== Type Info ===\n";
    std::cout << "int: " << typeInfo<int>() << "\n";
    std::cout << "double: " << typeInfo<double>() << "\n";
    std::cout << "int*: " << typeInfo<int*>() << "\n";
    
    // Constexpr class
    constexpr Point p1(3, 4);
    constexpr Point p2(6, 8);
    constexpr Point p3 = p1 + p2;
    constexpr double dist = p1.distanceSquared(p2);
    
    std::cout << "\n=== Constexpr Class ===\n";
    std::cout << "p3 = (" << p3.x() << ", " << p3.y() << ")\n";
    std::cout << "distSquared = " << dist << "\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Non-constexpr operations in constexpr function

```cpp
constexpr int bad(int n) {
    std::cout << n;  // ERROR: I/O is not constexpr
    return n * 2;
}

// FIX: Remove I/O or make function non-constexpr
```

### Mistake 2: Expecting constexpr at runtime

```cpp
constexpr int square(int x) { return x * x; }

int n;
std::cin >> n;
constexpr int result = square(n);  // ERROR: n is not constexpr
```

### Mistake 3: Overflow in constexpr

```cpp
constexpr int overflow() {
    int x = INT_MAX;
    return x + 1;  // ERROR: Undefined behavior detected at compile time!
}
// Compile-time UB is a compile error (good!)
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              CONSTEXPR QUICK REFERENCE                            |
+------------------------------------------------------------------+
|                                                                  |
|  constexpr variable:  Must be compile-time initialized           |
|  constexpr function:  Can be compile-time or runtime             |
|  consteval (C++20):   MUST be compile-time                       |
|  constinit (C++20):   Compile-time init, runtime mutable         |
|                                                                  |
|  USE CASES:                                                      |
|    • Lookup tables (zero runtime cost)                           |
|    • Configuration validation (static_assert)                    |
|    • Type traits and compile-time logic                          |
|    • Embedded constants (pi, buffer sizes)                       |
|                                                                  |
|  LIMITATIONS (relaxed over time):                                |
|    • C++11: Very limited (basically only return statements)      |
|    • C++14: Loops, local variables                               |
|    • C++17: if constexpr                                         |
|    • C++20: Virtual functions, dynamic allocation, try/catch     |
|                                                                  |
+------------------------------------------------------------------+
```

