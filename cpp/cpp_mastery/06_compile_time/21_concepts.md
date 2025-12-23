# Topic 21: Concepts and Constraint-Based Design (C++20)

## 1. Problem Statement

### What real engineering problem does this solve?

Template error messages are notoriously bad:

```cpp
// Without concepts
template<typename T>
T add(T a, T b) { return a + b; }

add(std::string("hello"), std::string("world"));  // OK
add(std::mutex{}, std::mutex{});  // ERROR: pages of gibberish

// With concepts
template<typename T>
requires std::is_arithmetic_v<T>
T add(T a, T b) { return a + b; }

add(std::mutex{}, std::mutex{});  
// ERROR: constraint not satisfied: std::is_arithmetic_v<std::mutex>
```

### Benefits of Concepts

- Clear, readable error messages
- Document template requirements
- Overload resolution based on constraints
- Self-documenting interfaces

**中文说明：**
Concepts 是 C++20 引入的模板约束机制。它让模板的类型要求显式化，提供清晰的错误信息，并允许基于约束的函数重载。Concepts 是对 SFINAE 的现代化替代，更易读、更强大。

---

## 2. Core Idea

### Concept Syntax

```cpp
// Define a concept
template<typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
};

// Use with requires clause
template<typename T>
requires Addable<T>
T add(T a, T b) { return a + b; }

// Use as type constraint
template<Addable T>
T add2(T a, T b) { return a + b; }

// Abbreviated function template
auto add3(Addable auto a, Addable auto b) { return a + b; }
```

### Standard Library Concepts

```cpp
#include <concepts>

std::integral<T>         // int, long, etc.
std::floating_point<T>   // float, double
std::signed_integral<T>  // signed integers
std::same_as<T, U>       // T is same as U
std::derived_from<D, B>  // D derives from B
std::convertible_to<F,T> // F convertible to T
std::copyable<T>         // Copy constructible and assignable
std::movable<T>          // Move constructible and assignable
std::regular<T>          // Default constructible, copyable, equality comparable
```

**中文说明：**
Concept 定义了类型必须满足的语法和语义要求。requires 表达式描述类型必须支持的操作。标准库提供了常用的 concepts，如 integral、copyable、regular 等。

---

## 3. Idiomatic C++ Techniques

### Defining Custom Concepts

```cpp
// Simple predicate concept
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;

// Compound requirements
template<typename T>
concept Printable = requires(T t, std::ostream& os) {
    { os << t } -> std::same_as<std::ostream&>;
};

// Type requirements
template<typename C>
concept Container = requires(C c) {
    typename C::value_type;
    typename C::iterator;
    { c.begin() } -> std::same_as<typename C::iterator>;
    { c.end() } -> std::same_as<typename C::iterator>;
    { c.size() } -> std::convertible_to<std::size_t>;
};
```

### Concept-Based Overloading

```cpp
void process(std::integral auto value) {
    std::cout << "Integer: " << value << "\n";
}

void process(std::floating_point auto value) {
    std::cout << "Floating: " << value << "\n";
}

void process(auto value) {  // Least constrained, fallback
    std::cout << "Other: " << typeid(value).name() << "\n";
}

// Most constrained matching concept wins
process(42);      // Calls integral version
process(3.14);    // Calls floating version
process("hello"); // Calls fallback
```

### Combining Concepts

```cpp
// Conjunction
template<typename T>
concept SignedNumeric = std::integral<T> && std::signed_integral<T>;

// Disjunction
template<typename T>
concept Number = std::integral<T> || std::floating_point<T>;

// Nested requirements
template<typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

template<typename K>
concept MapKey = std::equality_comparable<K> && Hashable<K>;
```

---

## 4. Complete C++ Example

```cpp
#include <concepts>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// ============================================================
// Basic concepts
// ============================================================
template<typename T>
concept Arithmetic = std::is_arithmetic_v<T>;

template<typename T>
concept Printable = requires(std::ostream& os, T t) {
    { os << t };
};

// ============================================================
// Container concept
// ============================================================
template<typename C>
concept Container = requires(C c) {
    typename C::value_type;
    typename C::size_type;
    { c.size() } -> std::convertible_to<typename C::size_type>;
    { c.empty() } -> std::same_as<bool>;
    { c.begin() };
    { c.end() };
};

template<typename C>
concept SizedContainer = Container<C> && requires(C c) {
    { c.size() } -> std::same_as<typename C::size_type>;
};

// ============================================================
// Iterator concept (simplified)
// ============================================================
template<typename I>
concept Iterator = requires(I i) {
    { *i };           // Dereferenceable
    { ++i } -> std::same_as<I&>;  // Incrementable
};

template<typename I>
concept RandomAccessIterator = Iterator<I> && requires(I i, int n) {
    { i + n } -> std::same_as<I>;
    { i - n } -> std::same_as<I>;
    { i[n] };
};

// ============================================================
// Using concepts in functions
// ============================================================
template<Arithmetic T>
T square(T x) {
    return x * x;
}

template<Container C>
void printSize(const C& container) {
    std::cout << "Size: " << container.size() << "\n";
}

// Concept-based overloading
void describe(std::integral auto x) {
    std::cout << "Integral: " << x << "\n";
}

void describe(std::floating_point auto x) {
    std::cout << "Floating: " << x << "\n";
}

void describe(Printable auto x) {
    std::cout << "Printable: " << x << "\n";
}

// ============================================================
// Constrained class template
// ============================================================
template<std::regular T>
class Box {
    T value_;
public:
    Box() = default;
    explicit Box(T v) : value_(std::move(v)) {}
    
    const T& get() const { return value_; }
    void set(T v) { value_ = std::move(v); }
    
    bool operator==(const Box&) const = default;
};

// ============================================================
// requires expression for complex constraints
// ============================================================
template<typename T>
concept Serializable = requires(T t) {
    { t.serialize() } -> std::same_as<std::string>;
    { T::deserialize(std::string{}) } -> std::same_as<T>;
};

struct Message {
    std::string data;
    
    std::string serialize() const { return data; }
    static Message deserialize(std::string s) { return Message{s}; }
};

template<Serializable T>
void save(const T& obj) {
    std::string data = obj.serialize();
    std::cout << "Saved: " << data << "\n";
}

// ============================================================
// Abbreviated function templates (C++20)
// ============================================================
auto multiply(Arithmetic auto a, Arithmetic auto b) {
    return a * b;
}

// ============================================================
// Subsumption: more constrained wins
// ============================================================
template<typename T>
void handle(T t) {
    std::cout << "Generic handler\n";
}

template<std::integral T>
void handle(T t) {
    std::cout << "Integral handler: " << t << "\n";
}

template<std::signed_integral T>
void handle(T t) {
    std::cout << "Signed integral handler: " << t << "\n";
}

// signed_integral subsumes integral, which subsumes unconstrained

int main() {
    std::cout << "=== Basic Concepts ===\n";
    std::cout << "square(5) = " << square(5) << "\n";
    std::cout << "square(2.5) = " << square(2.5) << "\n";
    // square("hello");  // Would not compile
    
    std::cout << "\n=== Container Concept ===\n";
    std::vector<int> vec = {1, 2, 3};
    printSize(vec);
    
    std::cout << "\n=== Overloading ===\n";
    describe(42);
    describe(3.14);
    describe("hello");
    
    std::cout << "\n=== Constrained Class ===\n";
    Box<int> box(42);
    std::cout << "Box value: " << box.get() << "\n";
    
    std::cout << "\n=== Serializable ===\n";
    Message msg{"Hello, Concepts!"};
    save(msg);
    
    std::cout << "\n=== Subsumption ===\n";
    handle(42);           // Signed integral handler
    handle(42u);          // Integral handler (unsigned)
    handle("hello");      // Generic handler
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Incorrect concept definition

```cpp
// BAD: No return type constraint
template<typename T>
concept Addable = requires(T a, T b) {
    a + b;  // Just checks if valid, not the type
};

// GOOD: Constrain return type
template<typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
};
```

### Mistake 2: Over-constraining

```cpp
// Too strict: requires exact same type
template<typename T>
requires std::same_as<T, int>  // Only int!
void foo(T t);

// Better: use broader concept
template<std::integral T>
void foo(T t);
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              CONCEPTS QUICK REFERENCE                             |
+------------------------------------------------------------------+
|                                                                  |
|  DEFINE CONCEPT:                                                 |
|    template<typename T>                                          |
|    concept Name = requires(T t) { ... };                         |
|                                                                  |
|  USE CONCEPT:                                                    |
|    template<Name T> void foo(T t);     // Type constraint        |
|    template<typename T> requires Name<T> void foo(T t);          |
|    void foo(Name auto t);              // Abbreviated            |
|                                                                  |
|  REQUIRES EXPRESSION:                                            |
|    { expr };                   // expr must be valid             |
|    { expr } -> concept;        // expr must satisfy concept      |
|    typename T::type;           // Type requirement               |
|                                                                  |
|  STANDARD CONCEPTS:                                              |
|    std::integral, std::floating_point, std::regular              |
|    std::copyable, std::movable, std::same_as, std::derived_from  |
|                                                                  |
+------------------------------------------------------------------+
```

