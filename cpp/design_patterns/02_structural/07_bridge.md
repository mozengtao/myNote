# Pattern 7: Bridge

## 1. Problem the Pattern Solves

### Design Pressure
- Abstraction and implementation need to vary independently
- Avoiding "class explosion" from combining multiple dimensions
- Want to switch implementations at runtime
- Need to hide implementation details completely

### What Goes Wrong Without It
```cpp
// Class explosion problem:
class Window { ... };
class XWindow : public Window { ... };      // X11 implementation
class WinWindow : public Window { ... };    // Windows implementation

class IconWindow : public Window { ... };
class XIconWindow : public IconWindow { ... };     // X11 + Icon
class WinIconWindow : public IconWindow { ... };   // Windows + Icon

class TransientWindow : public Window { ... };
class XTransientWindow : public TransientWindow { ... };
class WinTransientWindow : public TransientWindow { ... };
// M window types × N platforms = M×N classes!
```

### Symptoms Indicating Need
- Inheritance hierarchy growing in two or more dimensions
- Concrete classes needed for every combination
- Implementation changes require modifying abstraction
- Want to share implementation across different abstractions

---

## 2. Core Idea (C++-Specific)

**Bridge separates an abstraction from its implementation so that the two can vary independently.**

```
    Abstraction                          Implementation
+------------------+                 +------------------+
|   Abstraction    |  has-a          | Implementor      |
|   + operation()  |---------------->| + operationImpl()|
+------------------+                 +------------------+
        ^                                    ^
        |                                    |
+------------------+             +-----------+-----------+
| RefinedAbstraction|            |                       |
+------------------+    +------------------+  +------------------+
                        | ConcreteImplA   |  | ConcreteImplB   |
                        +------------------+  +------------------+
```

The key insight: **composition over inheritance**. Instead of subclassing for every combination, compose abstraction with implementation.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::unique_ptr` | Hold impl pointer | Manage impl lifetime |
| Pure virtual class | Implementor interface | Define impl contract |
| `std::make_unique` | Create impl | Exception-safe creation |
| `explicit` constructor | Take impl | Clear ownership transfer |
| PIMPL idiom | Related pattern | ABI stability |

### Bridge vs PIMPL

```
Bridge: Abstraction and impl are both polymorphic hierarchies
PIMPL:  Single class hiding implementation details
```

---

## 4. Canonical C++ Implementation

### Classic Bridge

```cpp
#include <memory>
#include <iostream>
#include <string>

// ============ Implementor Interface ============
class DrawingAPI {
public:
    virtual ~DrawingAPI() = default;
    virtual void drawCircle(double x, double y, double radius) = 0;
    virtual void drawRectangle(double x, double y, double w, double h) = 0;
};

// ============ Concrete Implementors ============
class OpenGLAPI : public DrawingAPI {
public:
    void drawCircle(double x, double y, double radius) override {
        std::cout << "OpenGL: circle at (" << x << "," << y 
                  << ") radius=" << radius << "\n";
    }
    void drawRectangle(double x, double y, double w, double h) override {
        std::cout << "OpenGL: rect at (" << x << "," << y 
                  << ") " << w << "x" << h << "\n";
    }
};

class VulkanAPI : public DrawingAPI {
public:
    void drawCircle(double x, double y, double radius) override {
        std::cout << "Vulkan: circle at (" << x << "," << y 
                  << ") radius=" << radius << "\n";
    }
    void drawRectangle(double x, double y, double w, double h) override {
        std::cout << "Vulkan: rect at (" << x << "," << y 
                  << ") " << w << "x" << h << "\n";
    }
};

// ============ Abstraction ============
class Shape {
public:
    explicit Shape(std::unique_ptr<DrawingAPI> api)
        : api_(std::move(api)) {}
    
    virtual ~Shape() = default;
    virtual void draw() = 0;
    virtual void resize(double factor) = 0;
    
protected:
    DrawingAPI& api() { return *api_; }
    
private:
    std::unique_ptr<DrawingAPI> api_;
};

// ============ Refined Abstractions ============
class Circle : public Shape {
public:
    Circle(double x, double y, double radius, 
           std::unique_ptr<DrawingAPI> api)
        : Shape(std::move(api))
        , x_(x), y_(y), radius_(radius) {}
    
    void draw() override {
        api().drawCircle(x_, y_, radius_);
    }
    
    void resize(double factor) override {
        radius_ *= factor;
    }
    
private:
    double x_, y_, radius_;
};

class Rectangle : public Shape {
public:
    Rectangle(double x, double y, double w, double h,
              std::unique_ptr<DrawingAPI> api)
        : Shape(std::move(api))
        , x_(x), y_(y), width_(w), height_(h) {}
    
    void draw() override {
        api().drawRectangle(x_, y_, width_, height_);
    }
    
    void resize(double factor) override {
        width_ *= factor;
        height_ *= factor;
    }
    
private:
    double x_, y_, width_, height_;
};

// ============ Usage ============
int main() {
    // Same abstraction, different implementations
    Circle c1(10, 20, 5, std::make_unique<OpenGLAPI>());
    Circle c2(10, 20, 5, std::make_unique<VulkanAPI>());
    
    c1.draw();  // Uses OpenGL
    c2.draw();  // Uses Vulkan
    
    Rectangle r(0, 0, 100, 50, std::make_unique<VulkanAPI>());
    r.draw();
    r.resize(2.0);
    r.draw();
    
    return 0;
}
```

### Template-Based Bridge (Compile-Time)

```cpp
#include <iostream>

// Implementors as types (no virtual)
struct OpenGLImpl {
    static void drawCircle(double x, double y, double r) {
        std::cout << "OpenGL circle\n";
    }
};

struct VulkanImpl {
    static void drawCircle(double x, double y, double r) {
        std::cout << "Vulkan circle\n";
    }
};

// Abstraction parameterized by implementor
template<typename Impl>
class Circle {
public:
    Circle(double x, double y, double r) : x_(x), y_(y), r_(r) {}
    
    void draw() {
        Impl::drawCircle(x_, y_, r_);
    }
    
private:
    double x_, y_, r_;
};

int main() {
    Circle<OpenGLImpl> c1(10, 20, 5);
    Circle<VulkanImpl> c2(10, 20, 5);
    
    c1.draw();  // Compile-time binding
    c2.draw();
    
    return 0;
}
```

### Runtime-Switchable Bridge

```cpp
#include <memory>

class Shape {
public:
    explicit Shape(std::shared_ptr<DrawingAPI> api)
        : api_(std::move(api)) {}
    
    void setAPI(std::shared_ptr<DrawingAPI> api) {
        api_ = std::move(api);
    }
    
    virtual void draw() = 0;
    
protected:
    std::shared_ptr<DrawingAPI> api_;
};

// Can switch renderer at runtime
shape->setAPI(std::make_shared<VulkanAPI>());
shape->draw();  // Now uses Vulkan
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| GUI toolkits | Widget abstraction + platform impl |
| Graphics | Shape/scene + rendering backend |
| Database | Query builder + driver |
| Networking | Connection + protocol |
| Device drivers | High-level API + hardware impl |

### Real-World Examples
- **Qt**: `QPaintDevice` (abstraction) + `QPaintEngine` (impl)
- **LLVM**: IR (abstraction) + Target backends (impl)
- **Java JDBC**: Statement (abstraction) + Driver (impl)

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Single Dimension of Variation

```cpp
// BAD: Only implementation varies, no abstraction hierarchy
class Logger {
    std::unique_ptr<LoggerImpl> impl_;
};
// Just use Strategy pattern or simple polymorphism
```

### ❌ Mistake 2: Leaking Implementor Interface

```cpp
// BAD: Abstraction exposes implementor
class Shape {
public:
    DrawingAPI& getAPI() { return *api_; }  // ✗ Leaks impl
};
// Client should only use Shape interface
```

### ❌ Mistake 3: Over-Engineering

```cpp
// BAD: Bridge for a single abstraction + single impl
class OnlyOneShape { ... };
class OnlyOneImpl { ... };
// Just use simple composition
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Single dimension varies | Simple inheritance or Strategy |
| Implementation fixed at compile time | Template parameter |
| No implementation hierarchy | Direct composition |
| Hiding impl details only | PIMPL idiom |

### Bridge vs Strategy

```
Bridge:   Abstractions have their own hierarchy
          impl changes structure of abstraction
          
Strategy: Single class with swappable algorithm
          impl is interchangeable behavior
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### Type Erasure Bridge

```cpp
#include <memory>
#include <functional>

class AnyDrawable {
public:
    template<typename T>
    AnyDrawable(T drawable)
        : self_(std::make_unique<Model<T>>(std::move(drawable))) {}
    
    void draw() { self_->draw(); }
    
private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void draw() = 0;
    };
    
    template<typename T>
    struct Model : Concept {
        T data;
        Model(T d) : data(std::move(d)) {}
        void draw() override { data.draw(); }
    };
    
    std::unique_ptr<Concept> self_;
};
```

### C++20 Concepts

```cpp
template<typename T>
concept DrawingBackend = requires(T t, double x, double y, double r) {
    t.drawCircle(x, y, r);
};

template<DrawingBackend Backend>
class Circle {
    Backend backend_;
public:
    void draw() { backend_.drawCircle(x_, y_, r_); }
};
```

### `std::variant` for Fixed Implementations

```cpp
#include <variant>

using DrawingBackend = std::variant<OpenGLAPI, VulkanAPI, DirectXAPI>;

class Shape {
    DrawingBackend backend_;
public:
    void draw() {
        std::visit([this](auto& api) {
            api.drawCircle(x_, y_, r_);
        }, backend_);
    }
};
```

---

## 9. Mental Model Summary

**When Bridge "Clicks":**

Use Bridge when you have **two orthogonal dimensions of variation** that would otherwise lead to class explosion. One dimension is the "what" (abstraction), the other is the "how" (implementation). Think: "shapes × renderers", "widgets × platforms", "messages × protocols".

**Code Review Recognition:**
- Abstraction class holds pointer to implementation interface
- Both abstraction and implementation have subclass hierarchies
- Implementation is injected at construction
- Check: Are there really two independent dimensions?

---

## 中文说明

### 桥接模式要点

1. **问题场景**：
   - 抽象和实现需要独立变化
   - 避免"类爆炸"问题（M×N 个类）
   - 需要在运行时切换实现

2. **核心思想**：
   ```
   不使用桥接：Shape × Platform = N×M 个类
   使用桥接：  N 个 Shape + M 个 Platform = N+M 个类
   ```

3. **与其他模式的区别**：
   ```
   桥接：两个独立变化的维度
   策略：单一类的可替换算法
   PIMPL：隐藏实现细节（无多态）
   ```

4. **C++ 实现要点**：
   - 抽象持有 `unique_ptr<Implementor>`
   - 实现通过构造函数注入
   - 两边都可以有继承层次

5. **典型应用**：
   - 图形系统：形状 × 渲染器
   - GUI 框架：控件 × 平台
   - 数据库：查询 × 驱动

### 决策流程

```
是否有两个正交的变化维度？
    ├── 否 → 使用简单继承或策略模式
    └── 是 → 两个维度都需要多态吗？
              ├── 否 → 使用模板参数化
              └── 是 → 使用桥接模式
```

