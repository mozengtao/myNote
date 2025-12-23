# Pattern 25: Type Erasure

## 1. Problem the Pattern Solves

### Design Pressure
- Want polymorphism without inheritance
- Store different types in same container
- Avoid virtual function overhead when possible
- Hide template types from interfaces

### What Goes Wrong Without It
```cpp
// Without type erasure: forced inheritance
class Drawable { virtual void draw() = 0; };
class Circle : public Drawable { void draw() override; };
// Every drawable MUST inherit from Drawable
```

---

## 2. Core Idea (C++-Specific)

**Type Erasure hides concrete types behind a uniform interface without requiring inheritance from a common base class.**

```
+------------------+       +------------------+
| Type-Erased      |       | Any Type with    |
| Container        |------>| required concept |
| (AnyDrawable)    |       | (Circle, Square) |
+------------------+       +------------------+
        │
        │ wraps
        ▼
+------------------+
| Internal Model   |
| (virtual calls)  |
+------------------+
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::function` | Callable erasure | Hide callable type |
| `std::any` | Value erasure | Store any copyable |
| Templates | Capture concrete type | Type-specific model |
| Virtual interface | Internal | Runtime dispatch |

---

## 4. Canonical C++ Implementation

### Custom Type Erasure

```cpp
#include <memory>
#include <iostream>

class AnyDrawable {
public:
    template<typename T>
    AnyDrawable(T obj) 
        : self_(std::make_unique<Model<T>>(std::move(obj))) {}
    
    void draw() const { self_->draw(); }
    
    // Value semantics
    AnyDrawable(const AnyDrawable& other) 
        : self_(other.self_->clone()) {}
    
    AnyDrawable& operator=(AnyDrawable other) {
        std::swap(self_, other.self_);
        return *this;
    }
    
private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void draw() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };
    
    template<typename T>
    struct Model : Concept {
        T data_;
        Model(T d) : data_(std::move(d)) {}
        void draw() const override { data_.draw(); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(*this);
        }
    };
    
    std::unique_ptr<Concept> self_;
};

// No inheritance needed!
struct Circle {
    void draw() const { std::cout << "Circle\n"; }
};

struct Square {
    void draw() const { std::cout << "Square\n"; }
};

int main() {
    std::vector<AnyDrawable> shapes;
    shapes.push_back(Circle{});
    shapes.push_back(Square{});
    
    for (const auto& s : shapes) {
        s.draw();
    }
    return 0;
}
```

### Using `std::function`

```cpp
#include <functional>
#include <vector>

// std::function IS type erasure
std::vector<std::function<void()>> callbacks;

callbacks.push_back([]() { std::cout << "Lambda\n"; });
callbacks.push_back(std::bind(&MyClass::method, &obj));

for (const auto& cb : callbacks) {
    cb();
}
```

---

## 5. Typical Usage

| Use Case | Example |
|----------|---------|
| Callbacks | `std::function` |
| Any value | `std::any` |
| Pimpl | Hide implementation |
| Plugin systems | Dynamic loading |

---

## 6. Common Mistakes

### ❌ Performance Overhead

```cpp
// Type erasure has cost (heap allocation, virtual calls)
// Don't use when templates suffice:
template<typename Drawable>
void draw(const Drawable& d) { d.draw(); }  // Zero overhead
```

---

## 7. Mental Model Summary

**When Type Erasure "Clicks":**

Use Type Erasure when you need **polymorphism without inheritance**—storing heterogeneous types in containers or hiding template types from APIs. Accept the performance cost for flexibility.

---

## 中文说明

### 类型擦除要点

1. **核心思想**：
   - 隐藏具体类型
   - 不需要公共基类

2. **实现方式**：
   - 内部虚接口（Concept/Model）
   - 外部非虚接口

3. **标准库类型擦除**：
   - `std::function`
   - `std::any`

4. **权衡**：
   - 灵活性 vs 性能开销

