# Topic 7: Dynamic Polymorphism (Virtual Interfaces)

## 1. Problem Statement

### What real engineering problem does this solve?

You need to work with objects of different types through a common interface, where the concrete type is only known at runtime:

```
COMPILE TIME                         RUNTIME
┌─────────────────┐                 ┌─────────────────┐
│ Code written    │                 │ User selects    │
│ against Base*   │                 │ "Circle" or     │
│                 │                 │ "Rectangle"     │
└─────────────────┘                 └─────────────────┘
        │                                   │
        v                                   v
┌─────────────────────────────────────────────────────┐
│  base->draw();  // Which draw()? Decided at runtime │
└─────────────────────────────────────────────────────┘
```

### What goes wrong without virtual functions?

```cpp
// Without virtual: must know concrete type
void draw(Circle& c) { /* circle drawing */ }
void draw(Rectangle& r) { /* rectangle drawing */ }

// Can't do this:
std::vector<Shape*> shapes;  // What type to store?
for (auto* s : shapes) {
    draw(*s);  // Doesn't compile - which draw()?
}

// Without virtual, no heterogeneous containers
// Without virtual, no plugin systems
// Without virtual, no runtime configuration
```

**中文说明：**
动态多态解决的问题是"运行时才知道具体类型"。通过基类指针调用方法时，实际执行的是派生类的实现。这使得异构容器（不同类型放一起）、插件系统、运行时配置成为可能。代价是虚表查找和间接调用的开销。

---

## 2. Core Idea

### Virtual Function Mechanism

```
OBJECT LAYOUT                       VTABLE
┌─────────────────┐                ┌────────────────────┐
│ vptr ───────────┼───────────────>│ &Derived::func1    │
├─────────────────┤                ├────────────────────┤
│ Base members    │                │ &Derived::func2    │
├─────────────────┤                ├────────────────────┤
│ Derived members │                │ &Derived::func3    │
└─────────────────┘                └────────────────────┘

VIRTUAL CALL:
base->func1();
  1. Load vptr from object
  2. Index into vtable
  3. Indirect call through function pointer
```

### Interface-Based Design

```cpp
// Abstract interface (pure virtual = 0)
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(const std::string& msg) = 0;
    virtual void setLevel(int level) = 0;
};

// Concrete implementations
class FileLogger : public ILogger { /* ... */ };
class ConsoleLogger : public ILogger { /* ... */ };
class NetworkLogger : public ILogger { /* ... */ };

// Client code works with interface
void process(ILogger& logger) {
    logger.log("Processing started");
    // ...
}
```

**中文说明：**
虚函数通过 vtable（虚表）实现。每个有虚函数的对象包含一个 vptr（虚表指针），指向该类的虚表。虚表存储各虚函数的实际地址。调用虚函数时，通过 vptr 查表，再间接调用——这就是运行时多态的机制。

---

## 3. Idiomatic C++ Techniques

### Virtual Destructor Rule

```cpp
// ALWAYS make destructor virtual in base class with virtual functions
class Base {
public:
    virtual ~Base() = default;  // REQUIRED!
    virtual void doSomething() = 0;
};

// Without virtual destructor:
Base* b = new Derived();
delete b;  // Only calls ~Base(), Derived resources leaked!
```

### Override and Final

```cpp
class Base {
public:
    virtual void func() = 0;
    virtual void optional() {}
};

class Derived : public Base {
public:
    void func() override;           // Must override, compiler checks
    void optional() override final; // Override and prevent further override
};

class Further : public Derived {
    // void optional() override;  // ERROR: optional is final
};
```

### NVI (Non-Virtual Interface) Pattern

```cpp
class Shape {
public:
    // Public non-virtual interface
    double area() const {
        validateState();
        return doArea();  // Delegate to virtual
    }
    
protected:
    // Private virtual implementation
    virtual double doArea() const = 0;
    
private:
    void validateState() const { /* ... */ }
};
```

---

## 4. Complete C++ Example

```cpp
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ============================================================
// Abstract Interface
// ============================================================
class Shape {
public:
    virtual ~Shape() = default;
    
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual std::string name() const = 0;
    virtual std::unique_ptr<Shape> clone() const = 0;
    
    void describe() const {
        std::cout << name() << ": area=" << area() 
                  << ", perimeter=" << perimeter() << "\n";
    }
};

// ============================================================
// Concrete Implementations
// ============================================================
class Circle : public Shape {
    double radius_;
    
public:
    explicit Circle(double r) : radius_(r) {}
    
    double area() const override {
        return 3.14159 * radius_ * radius_;
    }
    
    double perimeter() const override {
        return 2 * 3.14159 * radius_;
    }
    
    std::string name() const override { return "Circle"; }
    
    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Circle>(*this);
    }
};

class Rectangle : public Shape {
    double width_, height_;
    
public:
    Rectangle(double w, double h) : width_(w), height_(h) {}
    
    double area() const override {
        return width_ * height_;
    }
    
    double perimeter() const override {
        return 2 * (width_ + height_);
    }
    
    std::string name() const override { return "Rectangle"; }
    
    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Rectangle>(*this);
    }
};

// ============================================================
// Factory Pattern with Virtual Construction
// ============================================================
class ShapeFactory {
public:
    static std::unique_ptr<Shape> create(const std::string& type) {
        if (type == "circle") return std::make_unique<Circle>(1.0);
        if (type == "rectangle") return std::make_unique<Rectangle>(1.0, 1.0);
        return nullptr;
    }
};

// ============================================================
// Heterogeneous Container
// ============================================================
class Canvas {
    std::vector<std::unique_ptr<Shape>> shapes_;
    
public:
    void add(std::unique_ptr<Shape> shape) {
        shapes_.push_back(std::move(shape));
    }
    
    double totalArea() const {
        double sum = 0;
        for (const auto& s : shapes_) {
            sum += s->area();  // Virtual dispatch
        }
        return sum;
    }
    
    void describeAll() const {
        for (const auto& s : shapes_) {
            s->describe();  // Virtual dispatch
        }
    }
    
    // Deep copy using virtual clone
    Canvas clone() const {
        Canvas copy;
        for (const auto& s : shapes_) {
            copy.add(s->clone());
        }
        return copy;
    }
};

// ============================================================
// Visitor Pattern (double dispatch)
// ============================================================
class ShapeVisitor;

class VisitableShape {
public:
    virtual ~VisitableShape() = default;
    virtual void accept(ShapeVisitor& visitor) = 0;
};

class VisitableCircle;
class VisitableRectangle;

class ShapeVisitor {
public:
    virtual ~ShapeVisitor() = default;
    virtual void visit(VisitableCircle& c) = 0;
    virtual void visit(VisitableRectangle& r) = 0;
};

class VisitableCircle : public VisitableShape {
public:
    double radius = 5.0;
    void accept(ShapeVisitor& v) override { v.visit(*this); }
};

class VisitableRectangle : public VisitableShape {
public:
    double width = 3.0, height = 4.0;
    void accept(ShapeVisitor& v) override { v.visit(*this); }
};

class AreaCalculator : public ShapeVisitor {
public:
    double total = 0;
    void visit(VisitableCircle& c) override {
        total += 3.14159 * c.radius * c.radius;
    }
    void visit(VisitableRectangle& r) override {
        total += r.width * r.height;
    }
};

int main() {
    std::cout << "=== Heterogeneous Container ===\n";
    Canvas canvas;
    canvas.add(std::make_unique<Circle>(5.0));
    canvas.add(std::make_unique<Rectangle>(3.0, 4.0));
    canvas.add(std::make_unique<Circle>(2.0));
    
    canvas.describeAll();
    std::cout << "Total area: " << canvas.totalArea() << "\n";
    
    std::cout << "\n=== Virtual Clone ===\n";
    Canvas copy = canvas.clone();
    copy.describeAll();
    
    std::cout << "\n=== Factory Pattern ===\n";
    auto shape = ShapeFactory::create("circle");
    if (shape) shape->describe();
    
    std::cout << "\n=== Visitor Pattern ===\n";
    std::vector<std::unique_ptr<VisitableShape>> shapes;
    shapes.push_back(std::make_unique<VisitableCircle>());
    shapes.push_back(std::make_unique<VisitableRectangle>());
    
    AreaCalculator calc;
    for (auto& s : shapes) {
        s->accept(calc);
    }
    std::cout << "Visitor total area: " << calc.total << "\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Forgetting virtual destructor

```cpp
class Base {
public:
    // ~Base() {}  // NOT virtual - BUG!
    virtual void foo() {}
};

class Derived : public Base {
    std::vector<int> data_;  // Has resources
};

Base* b = new Derived();
delete b;  // Only ~Base() called, data_ leaked!
```

### Mistake 2: Calling virtual in constructor/destructor

```cpp
class Base {
public:
    Base() {
        init();  // Calls Base::init(), not Derived::init()!
    }
    virtual void init() { std::cout << "Base\n"; }
};

class Derived : public Base {
public:
    void init() override { std::cout << "Derived\n"; }
};

Derived d;  // Prints "Base", not "Derived"!
```

### Mistake 3: Object slicing

```cpp
void process(Shape shape) {  // Takes by VALUE
    shape.draw();  // Calls Shape::draw(), not derived!
}

Circle c;
process(c);  // Circle sliced to Shape - derived part lost!

// FIX: Take by reference or pointer
void process(Shape& shape);
void process(Shape* shape);
```

---

## 6. When NOT to Use Virtual Functions

### Performance-Critical Code

| Situation | Alternative |
|-----------|-------------|
| Tight loops | Templates (static polymorphism) |
| Known types at compile time | CRTP |
| Simple dispatch | std::variant + std::visit |
| Hot path | Type erasure with SBO |

### Cost Analysis

```
VIRTUAL CALL COST:
┌─────────────────────────────────────────────────────────────────┐
│ 1. Load vptr from object (~1 cycle if cached)                   │
│ 2. Load function pointer from vtable (~1 cycle if cached)       │
│ 3. Indirect call (~2-5 cycles, branch misprediction penalty)    │
│ 4. No inlining possible                                         │
│                                                                 │
│ Total: ~5-20 cycles per call (depends on cache state)           │
│ Compare: Direct call + inline = 0 cycles (optimized away)       │
└─────────────────────────────────────────────────────────────────┘
```

**中文说明：**
虚函数的开销包括：vptr 加载、vtable 查找、间接调用、无法内联。在热路径上每次调用多几十个周期可能很显著。替代方案：模板（编译时多态）、CRTP、std::variant。但在非热路径，虚函数的灵活性优势通常大于性能损失。

---

## Summary

```
+------------------------------------------------------------------+
|              DYNAMIC POLYMORPHISM CHECKLIST                       |
+------------------------------------------------------------------+
|                                                                  |
|  DESIGN:                                                         |
|    □ Virtual destructor in base class                            |
|    □ Pure virtual (= 0) for abstract interface                   |
|    □ Use override keyword on all overrides                       |
|    □ Consider NVI pattern for invariant enforcement              |
|                                                                  |
|  AVOID:                                                          |
|    □ Calling virtual functions in constructor/destructor         |
|    □ Passing polymorphic objects by value (slicing)              |
|    □ Deep inheritance hierarchies (prefer composition)           |
|                                                                  |
|  PERFORMANCE:                                                    |
|    □ Acceptable for non-hot paths                                |
|    □ Use templates/CRTP for performance-critical code            |
|    □ Consider std::variant for closed type sets                  |
|                                                                  |
+------------------------------------------------------------------+
```

