# Pattern 18: Visitor

## 1. Problem the Pattern Solves

### Design Pressure
- Need to perform operations on elements of a complex object structure
- Operations vary, but element classes are stable
- Want to add new operations without modifying element classes

### What Goes Wrong Without It
```cpp
// Adding new operation = modify every element class
class Circle { void draw(); void serialize(); void animate(); };
class Square { void draw(); void serialize(); void animate(); };
// Each new feature = modify all shapes
```

---

## 2. Core Idea (C++-Specific)

**Visitor lets you define new operations without changing the classes on which they operate, using double dispatch.**

```
+----------+         +-----------+
| Element  |         | Visitor   |
| accept(v)|-------->| visit(E)  |
+----------+         +-----------+
     ^                    ^
     |                    |
+---------+         +-----------+
| Circle  |         | DrawVisitor|
| accept()|         | visit(C)   |
+---------+         | visit(S)   |
                    +-----------+
```

**Double dispatch:** `element.accept(visitor)` → `visitor.visit(element)`

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Virtual `accept` | Element side | First dispatch |
| Overloaded `visit` | Visitor side | Second dispatch |
| `std::visit` | variant | Modern pattern matching |
| `std::variant` | Closed type set | Alternative to virtual |

---

## 4. Canonical C++ Implementation

### Classic OOP Visitor

```cpp
#include <memory>
#include <vector>
#include <iostream>

// Forward declarations
class Circle;
class Rectangle;

class ShapeVisitor {
public:
    virtual ~ShapeVisitor() = default;
    virtual void visit(Circle& c) = 0;
    virtual void visit(Rectangle& r) = 0;
};

class Shape {
public:
    virtual ~Shape() = default;
    virtual void accept(ShapeVisitor& v) = 0;
};

class Circle : public Shape {
public:
    double radius = 5.0;
    void accept(ShapeVisitor& v) override { v.visit(*this); }
};

class Rectangle : public Shape {
public:
    double width = 4.0, height = 3.0;
    void accept(ShapeVisitor& v) override { v.visit(*this); }
};

// Concrete visitors
class AreaVisitor : public ShapeVisitor {
public:
    double totalArea = 0.0;
    
    void visit(Circle& c) override {
        totalArea += 3.14159 * c.radius * c.radius;
    }
    void visit(Rectangle& r) override {
        totalArea += r.width * r.height;
    }
};

class DrawVisitor : public ShapeVisitor {
public:
    void visit(Circle& c) override {
        std::cout << "Drawing circle r=" << c.radius << "\n";
    }
    void visit(Rectangle& r) override {
        std::cout << "Drawing rect " << r.width << "x" << r.height << "\n";
    }
};

int main() {
    std::vector<std::unique_ptr<Shape>> shapes;
    shapes.push_back(std::make_unique<Circle>());
    shapes.push_back(std::make_unique<Rectangle>());
    
    AreaVisitor area;
    DrawVisitor draw;
    
    for (auto& s : shapes) {
        s->accept(area);
        s->accept(draw);
    }
    
    std::cout << "Total area: " << area.totalArea << "\n";
    return 0;
}
```

### Modern: `std::variant` + `std::visit`

```cpp
#include <variant>
#include <vector>
#include <iostream>

struct Circle { double radius; };
struct Rectangle { double width, height; };
using Shape = std::variant<Circle, Rectangle>;

// Visitor as overloaded lambdas
auto areaVisitor = [](const auto& shape) -> double {
    if constexpr (std::is_same_v<std::decay_t<decltype(shape)>, Circle>) {
        return 3.14159 * shape.radius * shape.radius;
    } else {
        return shape.width * shape.height;
    }
};

// Or using overload pattern
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

int main() {
    std::vector<Shape> shapes = { Circle{5.0}, Rectangle{4.0, 3.0} };
    
    double total = 0.0;
    for (const auto& s : shapes) {
        total += std::visit(areaVisitor, s);
    }
    
    // Or with overloaded pattern
    for (const auto& s : shapes) {
        std::visit(overloaded{
            [](const Circle& c) { std::cout << "Circle r=" << c.radius << "\n"; },
            [](const Rectangle& r) { std::cout << "Rect " << r.width << "x" << r.height << "\n"; }
        }, s);
    }
    
    return 0;
}
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Compilers | AST traversal |
| Document | XML/JSON processing |
| UI | Rendering, hit testing |
| Serialization | Multi-format export |

---

## 6. Common Mistakes

### ❌ Adding New Element Type

```cpp
// BAD: Adding new element breaks all visitors
class Triangle : public Shape { };
// ALL visitors must add visit(Triangle&)!
```

Visitor is **good for stable element hierarchies**, bad for frequently changing ones.

---

## 7. When NOT to Use

| Situation | Alternative |
|-----------|-------------|
| Frequently adding new types | Keep methods in elements |
| Simple operations | Direct virtual methods |

---

## 8. Mental Model Summary

**When Visitor "Clicks":**

Use Visitor when you have a **stable class hierarchy** and need to **add operations frequently**. The visitor pattern inverts the usual tradeoff: adding new operations is easy, but adding new types is hard.

---

## 中文说明

### 访问者模式要点

1. **双重分派**：
   - element.accept(visitor) → 第一次分派
   - visitor.visit(element) → 第二次分派

2. **两种实现**：
   - 经典 OOP：虚函数 + accept/visit
   - 现代 C++：`std::variant` + `std::visit`

3. **权衡**：
   - 添加新操作：容易
   - 添加新类型：困难

4. **典型应用**：
   - 编译器 AST
   - 文档处理
   - 序列化

