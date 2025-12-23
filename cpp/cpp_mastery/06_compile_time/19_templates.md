# Topic 19: Templates as a Compile-Time Language

## 1. Problem Statement

### What real engineering problem does this solve?

Write code once, use with any type:

```cpp
// Without templates: Write N versions
int max_int(int a, int b) { return a > b ? a : b; }
double max_double(double a, double b) { return a > b ? a : b; }
// ... repeat for every type

// With templates: Write once
template<typename T>
T max(T a, T b) { return a > b ? a : b; }
// Works with int, double, string, anything comparable
```

### What goes wrong without templates?

- Code duplication
- No type safety (void* and macros)
- No compile-time optimization
- Can't express generic algorithms

**中文说明：**
模板是 C++ 的元编程系统——在编译时生成代码。与运行时多态（虚函数）不同，模板在编译时为每个使用的类型生成专门的代码，实现零开销抽象。模板让你写一次算法，用于任意类型。

---

## 2. Core Idea

### Templates Generate Code at Compile Time

```
SOURCE CODE                         COMPILER GENERATES
template<typename T>                int max(int a, int b) {
T max(T a, T b) {                       return a > b ? a : b;
    return a > b ? a : b;           }
}                                   
                                    double max(double a, double b) {
max(1, 2);       // int version         return a > b ? a : b;
max(1.0, 2.0);   // double version  }
```

### Two-Phase Lookup

```cpp
template<typename T>
void foo(T x) {
    bar(x);      // Dependent: looked up when T is known
    baz();       // Non-dependent: looked up immediately
}

// Phase 1: Parse template, check non-dependent names
// Phase 2: Instantiate with specific T, check dependent names
```

**中文说明：**
模板编译分两阶段：
1. **模板定义时**：检查语法和不依赖模板参数的名字
2. **模板实例化时**：为具体类型生成代码，检查依赖的名字

这意味着模板错误可能在定义时发现，也可能在使用时才发现。

---

## 3. Idiomatic C++ Techniques

### Function Templates

```cpp
// Type deduction
template<typename T>
void print(const T& value) {
    std::cout << value << "\n";
}

print(42);      // T = int
print("hello"); // T = const char*

// Explicit template arguments
template<typename To, typename From>
To convert(From value) {
    return static_cast<To>(value);
}

int x = convert<int>(3.14);  // Must specify To
```

### Class Templates

```cpp
template<typename T, size_t N>
class Array {
    T data_[N];
public:
    T& operator[](size_t i) { return data_[i]; }
    constexpr size_t size() const { return N; }
};

Array<int, 10> arr;  // Fixed-size array of 10 ints
```

### Template Specialization

```cpp
// Primary template
template<typename T>
class Serializer {
public:
    static std::string serialize(const T& obj) {
        return obj.toString();
    }
};

// Full specialization for int
template<>
class Serializer<int> {
public:
    static std::string serialize(int value) {
        return std::to_string(value);
    }
};

// Partial specialization for pointers
template<typename T>
class Serializer<T*> {
public:
    static std::string serialize(T* ptr) {
        return ptr ? Serializer<T>::serialize(*ptr) : "null";
    }
};
```

---

## 4. Complete C++ Example

```cpp
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// ============================================================
// Basic function template
// ============================================================
template<typename T>
T minimum(T a, T b) {
    return (a < b) ? a : b;
}

// ============================================================
// Class template with default argument
// ============================================================
template<typename T, typename Allocator = std::allocator<T>>
class SimpleVector {
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
    Allocator alloc_;
    
public:
    SimpleVector() = default;
    
    ~SimpleVector() {
        for (size_t i = 0; i < size_; ++i) {
            std::allocator_traits<Allocator>::destroy(alloc_, &data_[i]);
        }
        if (data_) {
            std::allocator_traits<Allocator>::deallocate(alloc_, data_, capacity_);
        }
    }
    
    void push_back(const T& value) {
        if (size_ >= capacity_) {
            size_t newCap = capacity_ ? capacity_ * 2 : 8;
            T* newData = std::allocator_traits<Allocator>::allocate(alloc_, newCap);
            for (size_t i = 0; i < size_; ++i) {
                std::allocator_traits<Allocator>::construct(alloc_, &newData[i], std::move(data_[i]));
                std::allocator_traits<Allocator>::destroy(alloc_, &data_[i]);
            }
            if (data_) {
                std::allocator_traits<Allocator>::deallocate(alloc_, data_, capacity_);
            }
            data_ = newData;
            capacity_ = newCap;
        }
        std::allocator_traits<Allocator>::construct(alloc_, &data_[size_++], value);
    }
    
    T& operator[](size_t i) { return data_[i]; }
    size_t size() const { return size_; }
};

// ============================================================
// Variadic templates
// ============================================================
// Base case
void print() {
    std::cout << "\n";
}

// Recursive case
template<typename T, typename... Args>
void print(T first, Args... rest) {
    std::cout << first;
    if constexpr (sizeof...(rest) > 0) {
        std::cout << ", ";
    }
    print(rest...);
}

// ============================================================
// SFINAE (Substitution Failure Is Not An Error)
// ============================================================
// Enable only for integral types
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
double_it(T value) {
    return value * 2;
}

// Enable only for floating point
template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
double_it(T value) {
    return value * 2.0;
}

// ============================================================
// Template metaprogramming: compile-time factorial
// ============================================================
template<int N>
struct Factorial {
    static constexpr int value = N * Factorial<N-1>::value;
};

template<>
struct Factorial<0> {
    static constexpr int value = 1;
};

// ============================================================
// Type traits
// ============================================================
template<typename T>
void describe() {
    std::cout << "Type properties:\n";
    std::cout << "  is_integral: " << std::is_integral<T>::value << "\n";
    std::cout << "  is_floating_point: " << std::is_floating_point<T>::value << "\n";
    std::cout << "  is_pointer: " << std::is_pointer<T>::value << "\n";
    std::cout << "  is_const: " << std::is_const<T>::value << "\n";
}

int main() {
    std::cout << "=== Basic Templates ===\n";
    std::cout << "min(3, 5) = " << minimum(3, 5) << "\n";
    std::cout << "min(3.14, 2.71) = " << minimum(3.14, 2.71) << "\n";
    
    std::cout << "\n=== Class Template ===\n";
    SimpleVector<std::string> vec;
    vec.push_back("Hello");
    vec.push_back("World");
    std::cout << "vec[0] = " << vec[0] << "\n";
    
    std::cout << "\n=== Variadic Templates ===\n";
    print(1, 2.5, "three", 'c');
    
    std::cout << "\n=== SFINAE ===\n";
    std::cout << "double_it(5) = " << double_it(5) << "\n";
    std::cout << "double_it(2.5) = " << double_it(2.5) << "\n";
    
    std::cout << "\n=== Compile-time Computation ===\n";
    std::cout << "Factorial<5> = " << Factorial<5>::value << "\n";
    std::cout << "Factorial<10> = " << Factorial<10>::value << "\n";
    
    std::cout << "\n=== Type Traits ===\n";
    std::cout << "int:\n";
    describe<int>();
    std::cout << "\ndouble*:\n";
    describe<double*>();
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Confusing error messages

```cpp
template<typename T>
void process(T x) {
    x.foo();  // Requires foo() method
}

process(42);  // Error: int has no member 'foo'
// Error message may be pages long, pointing to template internals
```

### Mistake 2: Code bloat

```cpp
// Each instantiation generates new code
template<typename T>
void bigFunction(T x) { /* 500 lines */ }

bigFunction(1);     // Generates bigFunction<int>
bigFunction(1.0);   // Generates bigFunction<double>
bigFunction(1.0f);  // Generates bigFunction<float>
// Binary size increases 3x!
```

### Mistake 3: ODR violations

```cpp
// header.h
template<typename T>
T getValue() { return T(); }

// file1.cpp
#include "header.h"
template<> int getValue<int>() { return 1; }

// file2.cpp
#include "header.h"
template<> int getValue<int>() { return 2; }  // ODR violation!
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              TEMPLATE BEST PRACTICES                              |
+------------------------------------------------------------------+
|                                                                  |
|  DESIGN:                                                         |
|    □ Use concepts (C++20) to constrain template parameters       |
|    □ Factor out non-generic code to reduce bloat                 |
|    □ Prefer function templates over macros                       |
|    □ Use if constexpr for compile-time branches                  |
|                                                                  |
|  AVOID:                                                          |
|    □ Very deep template nesting                                  |
|    □ Complex SFINAE (use concepts instead)                       |
|    □ Templates for non-generic code                              |
|                                                                  |
|  MODERN ALTERNATIVES:                                            |
|    □ auto for type deduction                                     |
|    □ Concepts for constraints (C++20)                            |
|    □ constexpr for compile-time computation                      |
|                                                                  |
+------------------------------------------------------------------+
```

