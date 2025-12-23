# Topic 5: Zero-Cost Abstractions

## 1. Problem Statement

### What real engineering problem does this solve?

Systems programming faces a fundamental tension:

```
HIGH-LEVEL ABSTRACTION               LOW-LEVEL PERFORMANCE
+----------------------+             +----------------------+
| Readable code        |             | Maximum speed        |
| Type safety          |   VS        | Minimal overhead     |
| Maintainability      |             | Hardware control     |
+----------------------+             +----------------------+
```

Other languages force a choice. C++ aims to provide **both**.

### What goes wrong if abstractions have cost?

```cpp
// If iterators had overhead like Java iterators:
for (auto it = vec.begin(); it != vec.end(); ++it) {
    *it += 1;
    // Each operation: virtual call, bounds check, allocation?
    // Death by a thousand cuts
}

// If smart pointers had overhead:
unique_ptr<Widget> w = make_unique<Widget>();
w->process();  // Virtual dispatch? Atomic operations?
// Unusable in hot paths

// Engineers would be forced to write:
for (int i = 0; i < size; ++i) {
    raw_ptr[i] += 1;  // Back to C
}
```

**中文说明：**
"零开销抽象"是 C++ 的核心设计哲学：使用抽象不应比手写低级代码慢。如果 iterator 比原始指针慢，没人会用它。如果 unique_ptr 比裸指针慢，高性能代码就会回退到手动内存管理。零开销意味着"不用的功能不付费，用的功能不会比手写更慢"。

---

## 2. Core Idea

### The Zero-Overhead Principle

Bjarne Stroustrup's formulation:

> **"What you don't use, you don't pay for.
> What you do use, you couldn't hand-code any better."**

```
┌─────────────────────────────────────────────────────────────────┐
│  ABSTRACTION           │  OVERHEAD    │  EQUIVALENT TO          │
├─────────────────────────────────────────────────────────────────┤
│  vector<T>::iterator   │  Zero        │  T*                     │
│  unique_ptr<T>         │  Zero        │  T* (with delete)       │
│  array<T,N>            │  Zero        │  T[N]                   │
│  span<T>               │  Zero        │  T* + size              │
│  optional<T>           │  sizeof(T)+1 │  T + bool flag          │
│  string_view           │  Zero        │  const char* + size     │
│  std::function         │  Overhead!   │  (type erasure cost)    │
│  virtual functions     │  Overhead!   │  (indirect call cost)   │
└─────────────────────────────────────────────────────────────────┘
```

### How C++ Achieves Zero Cost

```
COMPILE-TIME RESOLUTION              RUNTIME DISPATCH
(Zero-cost)                          (Has cost)

template<typename T>                 class Base {
void sort(T* begin, T* end);         public:
                                         virtual void process();
                                     };

// Compiler generates:               // Runtime:
// sort_int(int*, int*);             // 1. Load vtable pointer
// sort_double(double*, double*);    // 2. Index into vtable
// sort_Widget(Widget*, Widget*);    // 3. Indirect call
                                     // 4. No inlining possible
// Each fully optimized for type
// All calls inlined if beneficial
```

**中文说明：**
C++ 实现零开销的关键机制是**编译时多态**（模板）而非**运行时多态**（虚函数）。模板在编译时为每个具体类型生成专门的代码，编译器可以完全内联和优化。虚函数则需要运行时查表和间接调用，阻止了内联优化。

---

## 3. Idiomatic C++ Techniques

### Templates: The Primary Tool

```cpp
// Zero-cost generic algorithm
template<typename Iterator, typename Predicate>
Iterator find_if(Iterator first, Iterator last, Predicate pred) {
    for (; first != last; ++first) {
        if (pred(*first)) {
            return first;
        }
    }
    return last;
}

// Usage - compiles to same code as hand-written loop
auto it = std::find_if(vec.begin(), vec.end(), 
    [](int x) { return x > 10; });

// Compiler sees:
// - Iterator = vector<int>::iterator (which is int*)
// - Predicate = lambda (inlined)
// - Loop = optimal machine code with no function calls
```

### Inline Everything

```cpp
// Small functions should be in headers for inlining
class Point {
    double x_, y_;
public:
    // Inline by default (defined in class)
    double x() const { return x_; }
    double y() const { return y_; }
    
    Point operator+(const Point& other) const {
        return Point{x_ + other.x_, y_ + other.y_};
    }
};

// After optimization: Point math compiles to just ADD instructions
// No function call overhead
```

### constexpr: Compile-Time Computation

```cpp
// Compute at compile time - zero runtime cost
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}

constexpr int fact10 = factorial(10);  // Computed at compile time

// Table generation at compile time
constexpr std::array<int, 256> generateLookupTable() {
    std::array<int, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = /* compute something */;
    }
    return table;
}

constexpr auto lookupTable = generateLookupTable();  // All at compile time
```

---

## 4. Complete C++ Example

```cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <vector>

// ============================================================
// Example 1: unique_ptr is zero-cost
// ============================================================
void demonstrateUniquePtrCost() {
    constexpr int N = 10'000'000;
    
    // Raw pointer version
    auto start1 = std::chrono::high_resolution_clock::now();
    int* raw = new int[N];
    for (int i = 0; i < N; ++i) raw[i] = i;
    long sum1 = 0;
    for (int i = 0; i < N; ++i) sum1 += raw[i];
    delete[] raw;
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // unique_ptr version
    auto start2 = std::chrono::high_resolution_clock::now();
    auto smart = std::make_unique<int[]>(N);
    for (int i = 0; i < N; ++i) smart[i] = i;
    long sum2 = 0;
    for (int i = 0; i < N; ++i) sum2 += smart[i];
    // Automatic cleanup
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto time1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto time2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
    std::cout << "Raw pointer: " << time1.count() << " us\n";
    std::cout << "unique_ptr:  " << time2.count() << " us\n";
    std::cout << "(Should be nearly identical)\n";
}

// ============================================================
// Example 2: Iterator abstraction is zero-cost
// ============================================================
void demonstrateIteratorCost() {
    std::vector<int> vec(10'000'000);
    std::iota(vec.begin(), vec.end(), 0);
    
    // Index-based loop
    auto start1 = std::chrono::high_resolution_clock::now();
    long sum1 = 0;
    for (size_t i = 0; i < vec.size(); ++i) {
        sum1 += vec[i];
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // Iterator-based loop
    auto start2 = std::chrono::high_resolution_clock::now();
    long sum2 = 0;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        sum2 += *it;
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    // Range-based for
    auto start3 = std::chrono::high_resolution_clock::now();
    long sum3 = 0;
    for (int x : vec) {
        sum3 += x;
    }
    auto end3 = std::chrono::high_resolution_clock::now();
    
    // std::accumulate
    auto start4 = std::chrono::high_resolution_clock::now();
    long sum4 = std::accumulate(vec.begin(), vec.end(), 0L);
    auto end4 = std::chrono::high_resolution_clock::now();
    
    auto us = [](auto d) { 
        return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    };
    
    std::cout << "Index loop:   " << us(end1 - start1) << " us\n";
    std::cout << "Iterator:     " << us(end2 - start2) << " us\n";
    std::cout << "Range-for:    " << us(end3 - start3) << " us\n";
    std::cout << "accumulate:   " << us(end4 - start4) << " us\n";
    std::cout << "(All should be nearly identical)\n";
}

// ============================================================
// Example 3: Template vs Virtual - the cost difference
// ============================================================

// Virtual (has cost)
class ShapeVirtual {
public:
    virtual ~ShapeVirtual() = default;
    virtual double area() const = 0;
};

class CircleVirtual : public ShapeVirtual {
    double radius_;
public:
    CircleVirtual(double r) : radius_(r) {}
    double area() const override { return 3.14159 * radius_ * radius_; }
};

// Template (zero cost)
template<typename Shape>
double computeAreaTemplate(const Shape& s) {
    return s.area();  // Statically dispatched, inlined
}

class CircleCRTP {
    double radius_;
public:
    CircleCRTP(double r) : radius_(r) {}
    double area() const { return 3.14159 * radius_ * radius_; }
};

void demonstrateVirtualCost() {
    constexpr int N = 100'000'000;
    
    // Virtual dispatch
    std::unique_ptr<ShapeVirtual> shape = std::make_unique<CircleVirtual>(5.0);
    auto start1 = std::chrono::high_resolution_clock::now();
    double sum1 = 0;
    for (int i = 0; i < N; ++i) {
        sum1 += shape->area();  // Virtual call each time
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // Template (zero cost)
    CircleCRTP circle(5.0);
    auto start2 = std::chrono::high_resolution_clock::now();
    double sum2 = 0;
    for (int i = 0; i < N; ++i) {
        sum2 += computeAreaTemplate(circle);  // Inlined
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto us = [](auto d) { 
        return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
    };
    
    std::cout << "Virtual dispatch: " << us(end1 - start1) << " us\n";
    std::cout << "Template inline:  " << us(end2 - start2) << " us\n";
    std::cout << "(Template should be faster - no indirect calls)\n";
}

// ============================================================
// Example 4: Zero-cost optional check using static typing
// ============================================================

// Instead of runtime null checks:
// if (ptr != nullptr) { ptr->doSomething(); }

// Use types that enforce non-null at compile time:
template<typename T>
class NonNull {
    T* ptr_;
public:
    explicit NonNull(T& ref) : ptr_(&ref) {}
    
    // No null check needed - construction guarantees non-null
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    
    // Cannot construct from nullptr - compile error
    NonNull(std::nullptr_t) = delete;
};

void useWidget(NonNull<std::string> s) {
    // No null check needed - type system guarantees it
    std::cout << s->size() << "\n";
}

// ============================================================
// Example 5: Compile-time computation
// ============================================================
constexpr int popcount(unsigned int n) {
    int count = 0;
    while (n) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}

// Generate lookup table at compile time
constexpr std::array<int, 256> makePopcountTable() {
    std::array<int, 256> table{};
    for (unsigned int i = 0; i < 256; ++i) {
        table[i] = popcount(i);
    }
    return table;
}

constexpr auto popcountTable = makePopcountTable();

int fastPopcount(unsigned int n) {
    // Using compile-time generated table
    int count = 0;
    while (n) {
        count += popcountTable[n & 0xFF];
        n >>= 8;
    }
    return count;
}

int main() {
    std::cout << "=== unique_ptr Cost ===\n";
    demonstrateUniquePtrCost();
    
    std::cout << "\n=== Iterator Cost ===\n";
    demonstrateIteratorCost();
    
    std::cout << "\n=== Virtual vs Template ===\n";
    demonstrateVirtualCost();
    
    std::cout << "\n=== Compile-time Table ===\n";
    std::cout << "popcount(0xFF) = " << fastPopcount(0xFF) << "\n";
    std::cout << "popcount(0xAA) = " << fastPopcount(0xAA) << "\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Hidden costs in "zero-cost" features

```cpp
// Looks zero-cost but isn't!
std::function<void()> callback = []() { /* ... */ };
// std::function uses type erasure - has overhead

// Zero-cost alternative:
auto callback = []() { /* ... */ };  // Exact type preserved

// Or template parameter:
template<typename F>
void setCallback(F&& f);  // No type erasure
```

### Mistake 2: Debug build vs Release build

```cpp
// In debug mode, vector::operator[] may have bounds checking
vec[i];  // Debug: O(1) with check. Release: true O(1)

// Always profile in release/optimized builds
// Debug build performance is meaningless
```

### Mistake 3: Preventing inlining

```cpp
// Too much code in header prevents inlining
class Widget {
public:
    void process() {
        // 200 lines of code...
        // Compiler may refuse to inline
    }
};

// Split hot path and cold path
class Widget {
public:
    void process() {
        if (likely(fastPath())) return;
        slowPath();  // Outlined
    }
private:
    bool fastPath();  // Inline
    void slowPath();  // In .cpp file
};
```

---

## 6. When Zero-Cost Isn't Achievable

### Abstractions with Inherent Cost

| Abstraction | Why It Has Cost |
|-------------|-----------------|
| `std::function` | Type erasure requires indirection |
| Virtual functions | vtable lookup, no inlining |
| `std::shared_ptr` | Atomic reference counting |
| Dynamic containers | Heap allocation |
| RTTI (`dynamic_cast`) | Runtime type information |

### When to Accept the Cost

```
ACCEPT COST WHEN:
┌─────────────────────────────────────────────────────────────┐
│ • The abstraction is not in a hot path                      │
│ • The benefit (flexibility, safety) outweighs the cost      │
│ • The alternative would be more complex and error-prone     │
│ • Profiling shows the cost is negligible in your context    │
└─────────────────────────────────────────────────────────────┘
```

**中文说明：**
并非所有抽象都能零开销：
- **std::function**: 类型擦除需要间接调用
- **虚函数**: vtable 查找阻止内联
- **shared_ptr**: 原子操作引用计数
- **动态容器**: 堆分配有固定开销

但这不意味着要避免它们。关键是**在热路径上追求零开销，在非热路径上接受适当开销换取安全性和可维护性**。

---

## Summary

```
+------------------------------------------------------------------+
|                  ZERO-COST ABSTRACTION TOOLKIT                    |
+------------------------------------------------------------------+
|                                                                  |
|  TOOL                    │  USE FOR                              |
|  ────────────────────────│───────────────────────────────────────|
|  Templates               │  Type-generic code, no vtable         |
|  Inline functions        │  Eliminating call overhead            |
|  constexpr               │  Compile-time computation             |
|  unique_ptr              │  Ownership without overhead           |
|  array/span              │  Bounds-safe arrays, zero cost        |
|  string_view             │  Non-owning string reference          |
|  Concepts (C++20)        │  Compile-time constraints             |
|                                                                  |
|  AVOID IN HOT PATHS:                                             |
|  ────────────────────────────────────────────────────────────────|
|  • std::function (use templates or auto lambdas)                 |
|  • Virtual functions (use CRTP or templates)                     |
|  • shared_ptr (use unique_ptr if possible)                       |
|  • dynamic_cast (redesign if possible)                           |
|                                                                  |
+------------------------------------------------------------------+
```

