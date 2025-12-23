# Pattern 29: Value-Semantic Design

## 1. Problem the Pattern Solves

### Design Pressure
- Simplify reasoning about object identity and lifetime
- Avoid dangling pointers and shared state bugs
- Enable efficient copying with move semantics

### What Goes Wrong Without It
```cpp
// Reference semantics nightmare
Person* p1 = new Person("Alice");
Person* p2 = p1;  // Same object!
p2->setName("Bob");
std::cout << p1->getName();  // "Bob" - surprise!
delete p1;
p2->method();  // CRASH: dangling pointer
```

---

## 2. Core Idea (C++-Specific)

**Value Semantics means objects behave like values—copies are independent, assignment creates a copy, and lifetime is deterministic.**

```cpp
std::string s1 = "hello";
std::string s2 = s1;      // Copy
s2[0] = 'H';
// s1 is still "hello", s2 is "Hello"
```

Value types:
- **Copy** creates independent object
- **Assignment** replaces value
- **Comparison** compares content
- **Lifetime** tied to scope

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Copy constructor | Deep copy | Independent copies |
| Copy assignment | Replace value | Assignment semantics |
| Move constructor | Efficient transfer | Avoid copies |
| Rule of Zero | Let compiler generate | Simplicity |
| `std::variant` | Polymorphic values | No heap |

---

## 4. Canonical C++ Implementation

### Value-Semantic Polygon

```cpp
#include <vector>
#include <iostream>

struct Point {
    double x, y;
};

class Polygon {
public:
    Polygon() = default;
    
    void addPoint(Point p) {
        points_.push_back(p);
    }
    
    size_t size() const { return points_.size(); }
    
    // Rule of Zero: compiler generates correct copy/move
    
private:
    std::vector<Point> points_;  // Value type member
};

int main() {
    Polygon p1;
    p1.addPoint({0, 0});
    p1.addPoint({1, 0});
    
    Polygon p2 = p1;  // Copy - independent object
    p2.addPoint({1, 1});
    
    std::cout << p1.size() << "\n";  // 2
    std::cout << p2.size() << "\n";  // 3
    
    return 0;
}
```

### Polymorphic Value with variant

```cpp
#include <variant>
#include <vector>

struct Circle { double radius; };
struct Rectangle { double width, height; };

using Shape = std::variant<Circle, Rectangle>;

int main() {
    std::vector<Shape> shapes;
    shapes.push_back(Circle{5.0});
    shapes.push_back(Rectangle{4.0, 3.0});
    
    // All on stack, no heap allocation
    // Copies are deep, independent
    
    auto copy = shapes;  // Independent copy
    
    return 0;
}
```

### Clone for Polymorphic Hierarchy

```cpp
class Animal {
public:
    virtual ~Animal() = default;
    virtual std::unique_ptr<Animal> clone() const = 0;
};

class Dog : public Animal {
public:
    std::unique_ptr<Animal> clone() const override {
        return std::make_unique<Dog>(*this);
    }
};

// Value wrapper with deep copy
class AnimalValue {
public:
    template<typename T>
    AnimalValue(T animal) 
        : ptr_(std::make_unique<T>(std::move(animal))) {}
    
    AnimalValue(const AnimalValue& other)
        : ptr_(other.ptr_->clone()) {}
    
    // ... rest of value semantics
    
private:
    std::unique_ptr<Animal> ptr_;
};
```

---

## 5. Typical Usage

| Use Case | Approach |
|----------|----------|
| Simple data | struct/class with value members |
| Polymorphism | `std::variant` or clone |
| Large objects | Move semantics |
| Configuration | Immutable value objects |

---

## 6. Value vs Reference Semantics

| Aspect | Value | Reference |
|--------|-------|-----------|
| Copy | Independent | Shared |
| Identity | Content | Address |
| Lifetime | Automatic | Manual |
| Reasoning | Simple | Complex |

---

## 7. Mental Model Summary

**When Value Semantics "Clicks":**

Default to value semantics in C++. Objects should **copy like ints**—independently. Use pointers only when **sharing is intentional**. Combine with move semantics for efficiency.

---

## 中文说明

### 值语义设计要点

1. **核心原则**：
   - 拷贝创建独立对象
   - 赋值替换值
   - 生命周期由作用域决定

2. **实现方式**：
   - 规则零：让编译器生成
   - `std::variant`：多态值类型
   - Clone：多态继承层次

3. **值 vs 引用语义**：
   - 值：独立拷贝，简单推理
   - 引用：共享状态，需要管理

4. **默认使用值语义**，只在需要共享时用指针

