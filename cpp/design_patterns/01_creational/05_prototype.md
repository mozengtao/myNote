# Pattern 5: Prototype

## 1. Problem the Pattern Solves

### Design Pressure
- Creating new objects by copying existing instances is more convenient than from scratch
- Object creation is expensive, but cloning is cheap
- Need to create objects without knowing their exact types
- Runtime configuration of object "templates"

### What Goes Wrong Without It
```cpp
// Client code must know all concrete types to clone
Shape* cloneShape(Shape* original) {
    if (auto* c = dynamic_cast<Circle*>(original)) {
        return new Circle(*c);
    } else if (auto* r = dynamic_cast<Rectangle*>(original)) {
        return new Rectangle(*r);
    }
    // Must add case for every new shape!
}
```

### Symptoms Indicating Need
- Complex initialization that you want to do once then copy
- `dynamic_cast` chains to determine type before copying
- Factory becomes too complex with too many products
- Configuration-heavy objects that vary by parameters, not types

---

## 2. Core Idea (C++-Specific)

**Prototype creates new objects by cloning existing instances through a polymorphic `clone()` method.**

```
+----------------+       +------------------+
|    Client      | ----> | Prototype (ABC)  |
| clone(proto)   |       |   clone() = 0    |
+----------------+       +------------------+
                                ^
                                |
                  +-------------+-------------+
                  |                           |
         +--------+--------+         +--------+--------+
         | ConcreteProtoA  |         | ConcreteProtoB  |
         | clone() override|         | clone() override|
         +-----------------+         +-----------------+
```

In C++:
1. `clone()` returns `std::unique_ptr<Base>` for clear ownership
2. Concrete classes implement deep copy
3. Alternative: leverage C++ copy constructors directly

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `virtual clone()` | Polymorphic copy | Clone without knowing type |
| Copy constructor | Deep copy logic | Implement clone correctly |
| `std::unique_ptr` | Return type | Clear ownership of clone |
| `override` | Concrete clones | Compiler verification |
| `= delete` copy | Force clone usage | Prevent accidental slicing |

### Clone vs Copy Constructor

```cpp
// Copy constructor: same type
Circle c2 = c1;  // Works only if you know it's a Circle

// Clone: polymorphic
std::unique_ptr<Shape> s2 = s1->clone();  // Works for any Shape
```

---

## 4. Canonical C++ Implementation

### Virtual Clone Method

```cpp
#include <memory>
#include <string>
#include <iostream>

class Shape {
public:
    virtual ~Shape() = default;
    virtual std::unique_ptr<Shape> clone() const = 0;
    virtual void draw() const = 0;
    
    void setColor(const std::string& c) { color_ = c; }
    const std::string& color() const { return color_; }
    
protected:
    std::string color_ = "black";
};

class Circle : public Shape {
public:
    Circle(double radius) : radius_(radius) {}
    
    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Circle>(*this);  // Uses copy ctor
    }
    
    void draw() const override {
        std::cout << "Circle(r=" << radius_ << ", color=" << color_ << ")\n";
    }
    
    void setRadius(double r) { radius_ = r; }
    
private:
    double radius_;
};

class Rectangle : public Shape {
public:
    Rectangle(double w, double h) : width_(w), height_(h) {}
    
    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Rectangle>(*this);
    }
    
    void draw() const override {
        std::cout << "Rectangle(" << width_ << "x" << height_ 
                  << ", color=" << color_ << ")\n";
    }
    
private:
    double width_, height_;
};

// Prototype registry
#include <unordered_map>

class ShapeRegistry {
public:
    void registerPrototype(const std::string& name, 
                          std::unique_ptr<Shape> proto) {
        prototypes_[name] = std::move(proto);
    }
    
    std::unique_ptr<Shape> create(const std::string& name) const {
        auto it = prototypes_.find(name);
        if (it == prototypes_.end()) return nullptr;
        return it->second->clone();
    }
    
private:
    std::unordered_map<std::string, std::unique_ptr<Shape>> prototypes_;
};

int main() {
    // Set up prototypes
    ShapeRegistry registry;
    
    auto redCircle = std::make_unique<Circle>(10.0);
    redCircle->setColor("red");
    registry.registerPrototype("red-circle", std::move(redCircle));
    
    auto blueRect = std::make_unique<Rectangle>(5.0, 3.0);
    blueRect->setColor("blue");
    registry.registerPrototype("blue-rect", std::move(blueRect));
    
    // Create from prototypes
    auto shape1 = registry.create("red-circle");
    auto shape2 = registry.create("red-circle");  // Another copy
    auto shape3 = registry.create("blue-rect");
    
    shape1->draw();
    shape2->draw();
    shape3->draw();
    
    return 0;
}
```

### CRTP-Based Clone (Compile-Time)

```cpp
#include <memory>

template<typename Derived, typename Base>
class Cloneable : public Base {
public:
    std::unique_ptr<Base> clone() const override {
        return std::make_unique<Derived>(static_cast<const Derived&>(*this));
    }
};

class Shape {
public:
    virtual ~Shape() = default;
    virtual std::unique_ptr<Shape> clone() const = 0;
    virtual void draw() const = 0;
};

class Circle : public Cloneable<Circle, Shape> {
public:
    Circle(double r) : radius_(r) {}
    void draw() const override { /* ... */ }
private:
    double radius_;
};

class Rectangle : public Cloneable<Rectangle, Shape> {
public:
    Rectangle(double w, double h) : width_(w), height_(h) {}
    void draw() const override { /* ... */ }
private:
    double width_, height_;
};
```

### Value-Semantic Clone (Modern C++ Alternative)

```cpp
#include <variant>
#include <memory>

struct Circle { double radius; };
struct Rectangle { double width, height; };
struct Triangle { double a, b, c; };

using Shape = std::variant<Circle, Rectangle, Triangle>;

// No clone() needed! std::variant is copyable
Shape original = Circle{10.0};
Shape copy = original;  // Automatic deep copy
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| Document editors | Template documents |
| Game development | Enemy/item spawning from templates |
| CAD systems | Object duplication with modifications |
| GUI builders | Widget templates |
| Networking | Message template objects |

### Real-World Examples
- **Protobuf**: `CopyFrom()` method
- **Game engines**: Prefab instantiation
- **IDE**: Code snippet templates

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Shallow Copy When Deep Copy Needed

```cpp
class Document : public Prototype {
    std::vector<Page*> pages_;  // Raw pointers!
    
    std::unique_ptr<Prototype> clone() const override {
        auto copy = std::make_unique<Document>(*this);
        // BUG: pages_ are shallow copied (same pointers!)
        return copy;
    }
};

// FIX: Use value types or clone nested objects
class Document : public Prototype {
    std::vector<std::unique_ptr<Page>> pages_;
    
    std::unique_ptr<Prototype> clone() const override {
        auto copy = std::make_unique<Document>();
        for (const auto& page : pages_) {
            copy->pages_.push_back(page->clone());
        }
        return copy;
    }
};
```

### ❌ Mistake 2: Forgetting to Clone Base Class Parts

```cpp
class ColoredShape : public Shape {
    std::string color_;
    
    std::unique_ptr<Shape> clone() const override {
        auto copy = std::make_unique<ColoredShape>();
        copy->color_ = color_;
        // BUG: Forgot to copy Shape's members!
        return copy;
    }
};

// FIX: Use copy constructor
std::unique_ptr<Shape> clone() const override {
    return std::make_unique<ColoredShape>(*this);
}
```

### ❌ Mistake 3: Object Slicing

```cpp
Shape s = *circle;  // SLICING: only Shape part copied
// Always use pointers/references for polymorphism
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Simple value types | Copy constructor |
| Types known at compile time | `std::variant` |
| No polymorphism needed | Direct copy |
| Circular references | Careful manual copy |

### Just Use Copy Constructors

```cpp
// If you know the concrete type:
Circle c1(10.0);
Circle c2 = c1;  // Simple, no pattern needed
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### `std::variant` (C++17)

```cpp
#include <variant>

using Shape = std::variant<Circle, Rectangle, Triangle>;

// Variant handles copying automatically
Shape s1 = Circle{5.0};
Shape s2 = s1;  // Deep copy, type-safe
```

### Smart Pointer with Clone

```cpp
template<typename T>
class ClonePtr {
public:
    ClonePtr(std::unique_ptr<T> ptr) : ptr_(std::move(ptr)) {}
    
    ClonePtr(const ClonePtr& other) 
        : ptr_(other.ptr_ ? other.ptr_->clone() : nullptr) {}
    
    ClonePtr& operator=(const ClonePtr& other) {
        ptr_ = other.ptr_ ? other.ptr_->clone() : nullptr;
        return *this;
    }
    
    T* operator->() { return ptr_.get(); }
    const T* operator->() const { return ptr_.get(); }
    
private:
    std::unique_ptr<T> ptr_;
};

// Now containers work with polymorphic copying
std::vector<ClonePtr<Shape>> shapes;
shapes.push_back(ClonePtr<Shape>(std::make_unique<Circle>(5.0)));
auto copy = shapes;  // Deep copies all shapes!
```

### C++20: Concepts for Cloneable

```cpp
template<typename T>
concept Cloneable = requires(const T& t) {
    { t.clone() } -> std::convertible_to<std::unique_ptr<T>>;
};

template<Cloneable T>
std::unique_ptr<T> duplicate(const T& obj) {
    return obj.clone();
}
```

---

## 9. Mental Model Summary

**When Prototype "Clicks":**

Use Prototype when you need **polymorphic copying**—creating new objects by cloning existing ones without knowing their concrete types. It's also useful when object setup is expensive but cloning is cheap, allowing you to preconfigure "template" objects.

**Code Review Recognition:**
- `clone()` method returning `unique_ptr<Base>`
- Prototype registry/cache holding template objects
- Check for proper deep copy (especially with nested pointers)
- Consider if `std::variant` is simpler for closed type sets

---

## 中文说明

### 原型模式要点

1. **问题场景**：
   - 需要多态拷贝（不知道具体类型时拷贝对象）
   - 对象初始化复杂，拷贝更简单
   - 运行时配置对象模板

2. **C++ 实现方式**：
   ```cpp
   virtual std::unique_ptr<Base> clone() const = 0;
   ```

3. **与拷贝构造函数的区别**：
   ```
   拷贝构造函数：需要知道具体类型
   Circle c2 = c1;  // 只能拷贝 Circle
   
   原型 clone()：多态拷贝
   auto s2 = s1->clone();  // 可以拷贝任何 Shape
   ```

4. **常见错误**：
   - 浅拷贝（忘记深拷贝嵌套对象）
   - 忘记拷贝基类成员
   - 对象切片

5. **现代替代方案**：
   - `std::variant`（类型集合固定时）
   - 值语义设计（避免多态拷贝需求）

### 决策流程

```
需要运行时多态拷贝？
    ├── 否 → 使用拷贝构造函数
    └── 是 → 类型集合固定吗？
              ├── 是 → 考虑 std::variant
              └── 否 → 使用原型模式
```

