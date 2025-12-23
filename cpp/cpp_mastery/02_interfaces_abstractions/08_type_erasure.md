# Topic 8: Type Erasure (std::function, std::any, custom)

## 1. Problem Statement

### What real engineering problem does this solve?

You want polymorphism without requiring inheritance:

```
INHERITANCE-BASED                    TYPE ERASURE
┌─────────────────┐                 ┌─────────────────┐
│ Must inherit    │                 │ Any type that   │
│ from ICallable  │                 │ has operator()  │
└─────────────────┘                 └─────────────────┘
        │                                   │
        v                                   v
class MyFunc : ICallable {          auto f = [](int x) { return x*2; };
    void call() override;           std::function<int(int)> fn = f;
};                                  fn(5);  // Works!
```

### What goes wrong without type erasure?

```cpp
// Without type erasure: must define common base
class Callback {
public:
    virtual void invoke() = 0;
};

// Every callback type needs inheritance!
class MyCallback : public Callback { /* ... */ };

// Can't use lambdas directly
auto lambda = []() { std::cout << "Hi\n"; };
// lambda doesn't inherit from Callback!

// Type erasure solution:
std::function<void()> callback = lambda;  // Just works!
```

**中文说明：**
类型擦除允许"鸭子类型"多态——只要对象支持所需操作，就可以使用，无需继承公共基类。std::function 可以存储任何可调用对象（函数、lambda、函数对象），std::any 可以存储任何类型。类型擦除在内部使用多态，但对外隐藏了类型信息。

---

## 2. Core Idea

### How Type Erasure Works

```
EXTERNAL VIEW                       INTERNAL IMPLEMENTATION
┌────────────────────┐             ┌────────────────────────────────┐
│ std::function<     │             │ ┌─────────────────────┐        │
│   int(int)>        │             │ │ Concept (interface) │        │
│                    │             │ │  virtual invoke()   │        │
│ "I hold anything   │             │ └──────────┬──────────┘        │
│  callable as       │             │            │                   │
│  int(int)"         │             │ ┌──────────▼──────────┐        │
│                    │             │ │ Model<Lambda>       │        │
└────────────────────┘             │ │  Lambda lambda_;    │        │
                                   │ │  invoke() {         │        │
                                   │ │    return lambda_();│        │
                                   │ │  }                  │        │
                                   │ └─────────────────────┘        │
                                   └────────────────────────────────┘
```

### The Pattern

```cpp
// 1. Define concept (abstract interface)
class CallableConcept {
public:
    virtual ~CallableConcept() = default;
    virtual int invoke(int) = 0;
    virtual std::unique_ptr<CallableConcept> clone() = 0;
};

// 2. Define model (wraps any type satisfying concept)
template<typename T>
class CallableModel : public CallableConcept {
    T callable_;
public:
    CallableModel(T c) : callable_(std::move(c)) {}
    int invoke(int x) override { return callable_(x); }
    std::unique_ptr<CallableConcept> clone() override {
        return std::make_unique<CallableModel>(*this);
    }
};

// 3. Type-erased wrapper
class Function {
    std::unique_ptr<CallableConcept> impl_;
public:
    template<typename T>
    Function(T callable) 
        : impl_(std::make_unique<CallableModel<T>>(std::move(callable))) {}
    
    int operator()(int x) { return impl_->invoke(x); }
};
```

**中文说明：**
类型擦除的核心模式：
1. **Concept**：定义虚函数接口（内部使用）
2. **Model**：模板类，包装任何满足接口的类型
3. **Wrapper**：对外暴露的类型擦除容器

外部看到的是统一类型（如 std::function），内部通过多态实现不同类型的存储和调用。

---

## 3. Idiomatic C++ Techniques

### std::function

```cpp
#include <functional>

// Store any callable with matching signature
std::function<int(int, int)> op;

op = [](int a, int b) { return a + b; };
std::cout << op(2, 3);  // 5

op = [](int a, int b) { return a * b; };
std::cout << op(2, 3);  // 6

// Store member function
struct Calculator {
    int multiply(int a, int b) { return a * b; }
};
Calculator calc;
op = std::bind(&Calculator::multiply, &calc, 
               std::placeholders::_1, std::placeholders::_2);
```

### std::any (C++17)

```cpp
#include <any>

std::any value;

value = 42;
std::cout << std::any_cast<int>(value);

value = std::string("hello");
std::cout << std::any_cast<std::string>(value);

value = 3.14;
// std::any_cast<int>(value);  // throws std::bad_any_cast
```

### std::variant (Closed Type Set)

```cpp
#include <variant>

std::variant<int, double, std::string> v;

v = 42;
v = 3.14;
v = "hello";

// Type-safe visitation
std::visit([](auto&& arg) {
    std::cout << arg << "\n";
}, v);
```

---

## 4. Complete C++ Example

```cpp
#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ============================================================
// Custom Type Erasure: Drawable
// ============================================================
class Drawable {
    // Concept: internal interface
    struct Concept {
        virtual ~Concept() = default;
        virtual void draw() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };
    
    // Model: wraps any drawable type
    template<typename T>
    struct Model : Concept {
        T data_;
        Model(T d) : data_(std::move(d)) {}
        void draw() const override { data_.draw(); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(*this);
        }
    };
    
    std::unique_ptr<Concept> impl_;
    
public:
    // Accept any type with draw() method
    template<typename T>
    Drawable(T x) : impl_(std::make_unique<Model<T>>(std::move(x))) {}
    
    // Copy support
    Drawable(const Drawable& other) : impl_(other.impl_->clone()) {}
    Drawable& operator=(const Drawable& other) {
        impl_ = other.impl_->clone();
        return *this;
    }
    
    // Move support
    Drawable(Drawable&&) = default;
    Drawable& operator=(Drawable&&) = default;
    
    void draw() const { impl_->draw(); }
};

// Types that satisfy Drawable concept (no inheritance!)
struct Circle {
    double radius;
    void draw() const { 
        std::cout << "Circle(r=" << radius << ")\n"; 
    }
};

struct Square {
    double side;
    void draw() const { 
        std::cout << "Square(s=" << side << ")\n"; 
    }
};

struct Text {
    std::string content;
    void draw() const { 
        std::cout << "Text: " << content << "\n"; 
    }
};

// ============================================================
// Small Buffer Optimization (SBO) Type Erasure
// ============================================================
class SmallDrawable {
    static constexpr size_t BufferSize = 32;
    
    struct Concept {
        virtual ~Concept() = default;
        virtual void draw() const = 0;
        virtual void copyTo(void* buffer) const = 0;
        virtual void moveTo(void* buffer) = 0;
    };
    
    template<typename T>
    struct Model : Concept {
        T data_;
        Model(T d) : data_(std::move(d)) {}
        void draw() const override { data_.draw(); }
        void copyTo(void* buffer) const override {
            new (buffer) Model(*this);
        }
        void moveTo(void* buffer) override {
            new (buffer) Model(std::move(*this));
        }
    };
    
    alignas(std::max_align_t) char buffer_[BufferSize];
    bool useHeap_ = false;
    
    Concept* ptr() { 
        return useHeap_ 
            ? *reinterpret_cast<Concept**>(buffer_)
            : reinterpret_cast<Concept*>(buffer_);
    }
    const Concept* ptr() const {
        return useHeap_
            ? *reinterpret_cast<Concept* const*>(buffer_)
            : reinterpret_cast<const Concept*>(buffer_);
    }
    
public:
    template<typename T>
    SmallDrawable(T x) {
        if constexpr (sizeof(Model<T>) <= BufferSize) {
            new (buffer_) Model<T>(std::move(x));
            useHeap_ = false;
        } else {
            *reinterpret_cast<Concept**>(buffer_) = 
                new Model<T>(std::move(x));
            useHeap_ = true;
        }
    }
    
    ~SmallDrawable() {
        if (useHeap_) {
            delete ptr();
        } else {
            ptr()->~Concept();
        }
    }
    
    void draw() const { ptr()->draw(); }
};

// ============================================================
// std::function usage examples
// ============================================================
void demonstrateStdFunction() {
    std::cout << "=== std::function ===\n";
    
    // Store lambda
    std::function<int(int)> f = [](int x) { return x * 2; };
    std::cout << "Lambda: " << f(5) << "\n";
    
    // Store free function
    f = [](int x) { return x + 10; };
    std::cout << "Changed: " << f(5) << "\n";
    
    // Callbacks
    std::vector<std::function<void()>> callbacks;
    callbacks.push_back([]() { std::cout << "Callback 1\n"; });
    callbacks.push_back([]() { std::cout << "Callback 2\n"; });
    
    for (auto& cb : callbacks) cb();
}

// ============================================================
// std::any usage examples
// ============================================================
void demonstrateStdAny() {
    std::cout << "\n=== std::any ===\n";
    
    std::vector<std::any> heterogeneous;
    heterogeneous.push_back(42);
    heterogeneous.push_back(3.14);
    heterogeneous.push_back(std::string("hello"));
    heterogeneous.push_back(Circle{5.0});
    
    for (const auto& item : heterogeneous) {
        if (item.type() == typeid(int)) {
            std::cout << "int: " << std::any_cast<int>(item) << "\n";
        } else if (item.type() == typeid(double)) {
            std::cout << "double: " << std::any_cast<double>(item) << "\n";
        } else if (item.type() == typeid(std::string)) {
            std::cout << "string: " << std::any_cast<std::string>(item) << "\n";
        } else if (item.type() == typeid(Circle)) {
            std::any_cast<Circle>(item).draw();
        }
    }
}

int main() {
    std::cout << "=== Custom Type Erasure ===\n";
    
    // Heterogeneous container without inheritance!
    std::vector<Drawable> shapes;
    shapes.push_back(Circle{5.0});
    shapes.push_back(Square{3.0});
    shapes.push_back(Text{"Hello, Type Erasure!"});
    
    for (const auto& s : shapes) {
        s.draw();
    }
    
    std::cout << "\n=== SBO Type Erasure ===\n";
    SmallDrawable d1 = Circle{2.5};
    SmallDrawable d2 = Square{4.0};
    d1.draw();
    d2.draw();
    
    demonstrateStdFunction();
    demonstrateStdAny();
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Performance overhead of std::function

```cpp
// std::function has overhead - avoid in hot loops
for (int i = 0; i < 1000000; ++i) {
    std::function<int(int)> f = [](int x) { return x * 2; };
    sum += f(i);  // Slow: allocation + virtual call
}

// Better: use auto or template
auto f = [](int x) { return x * 2; };
for (int i = 0; i < 1000000; ++i) {
    sum += f(i);  // Fast: inlined
}
```

### Mistake 2: std::any_cast to wrong type

```cpp
std::any a = 42;
// auto s = std::any_cast<std::string>(a);  // throws!

// Safe pattern:
if (auto* p = std::any_cast<int>(&a)) {
    std::cout << *p << "\n";
}
```

### Mistake 3: Dangling references in std::function

```cpp
std::function<int()> createBadFunction() {
    int x = 42;
    return [&x]() { return x; };  // x dies when function returns!
}

// FIX: Capture by value
std::function<int()> createGoodFunction() {
    int x = 42;
    return [x]() { return x; };  // Copy of x captured
}
```

---

## 6. When NOT to Use Type Erasure

### Performance Considerations

| Situation | Alternative |
|-----------|-------------|
| Hot path, type known | Templates/auto |
| Closed type set | std::variant |
| Only a few types | Manual dispatch |
| No heap allocation | SBO or variant |

### Comparison

```
┌─────────────────────────────────────────────────────────────────┐
│ TYPE ERASURE      │ OVERHEAD           │ USE WHEN               │
├─────────────────────────────────────────────────────────────────┤
│ std::function     │ Heap + virtual     │ Callbacks, event system│
│ std::any          │ Heap + RTTI        │ Plugin values          │
│ std::variant      │ Largest type       │ Closed type sets       │
│ Custom (SBO)      │ Virtual only       │ Perf-sensitive APIs    │
│ Templates         │ Zero               │ Known types            │
└─────────────────────────────────────────────────────────────────┘
```

**中文说明：**
类型擦除有开销：std::function 可能堆分配 + 虚调用，std::any 有 RTTI 开销。当类型在编译时已知，用模板/auto 更高效。当类型集合固定，用 std::variant 避免堆分配。自定义类型擦除可以用 SBO（小缓冲区优化）避免堆分配。

---

## Summary

```
+------------------------------------------------------------------+
|              TYPE ERASURE DECISION GUIDE                          |
+------------------------------------------------------------------+
|                                                                  |
|  std::function<Sig>:                                             |
|    • Callbacks and event handlers                                |
|    • When callable signature is known                            |
|    • Beware: heap allocation possible                            |
|                                                                  |
|  std::any:                                                       |
|    • Truly heterogeneous storage                                 |
|    • Plugin systems, property bags                               |
|    • Beware: type checking at runtime                            |
|                                                                  |
|  std::variant<Ts...>:                                            |
|    • Closed set of types known at compile time                   |
|    • No heap allocation                                          |
|    • Exhaustive visit handling                                   |
|                                                                  |
|  Custom type erasure:                                            |
|    • When you need specific interface                            |
|    • Can optimize with SBO                                       |
|    • Full control over behavior                                  |
|                                                                  |
+------------------------------------------------------------------+
```

