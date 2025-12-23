# Topic 6: Static Polymorphism (Templates, CRTP)

## 1. Problem Statement

### What real engineering problem does this solve?

You want polymorphism (same interface, different implementations) but without runtime cost:

```
DYNAMIC POLYMORPHISM                 STATIC POLYMORPHISM
(Virtual Functions)                  (Templates/CRTP)

┌──────────────┐                    ┌──────────────┐
│  Base*       │                    │  T           │  (concrete type known)
│  ├─vtable ptr│                    │  └─no vtable │
└──────────────┘                    └──────────────┘
       │                                   │
       v                                   v
┌──────────────┐                    No indirection!
│  vtable      │                    Compiler generates
│  ├─func1 ptr │                    specialized code
│  ├─func2 ptr │                    for each type.
└──────────────┘
       │                            
       v (indirect call)            
┌──────────────┐
│  Derived::f1 │  
└──────────────┘

COST: Indirect call,                COST: Zero runtime!
no inlining possible                Full inlining, optimized
```

### What goes wrong without static polymorphism?

```cpp
// Virtual function version: Can't inline, always indirect call
class Shape {
public:
    virtual double area() const = 0;
};

double totalArea(const std::vector<Shape*>& shapes) {
    double sum = 0;
    for (auto* s : shapes) {
        sum += s->area();  // Indirect call, no inlining
    }
    return sum;
}

// In hot loops, this overhead adds up significantly
```

**中文说明：**
动态多态（虚函数）在运行时通过 vtable 查找函数指针，然后间接调用。这阻止了内联优化，在热路径上开销显著。静态多态通过模板在编译时确定具体类型，编译器为每个类型生成专门的优化代码，实现零运行时开销。

---

## 2. Core Idea

### Templates as Compile-Time Polymorphism

```cpp
// Template function: works with any type that has .area()
template<typename Shape>
double getArea(const Shape& s) {
    return s.area();  // Resolved at compile time
}

// Usage - compiler generates specialized versions
Circle c(5.0);
Rectangle r(3.0, 4.0);

double a1 = getArea(c);  // Compiles to: c.Circle::area() - inlined
double a2 = getArea(r);  // Compiles to: r.Rectangle::area() - inlined
```

### CRTP: The Curiously Recurring Template Pattern

```cpp
template<typename Derived>
class Base {
public:
    void interface() {
        // Call derived implementation - no virtual call!
        static_cast<Derived*>(this)->implementation();
    }
    
    void implementation() {
        // Default implementation (optional)
    }
};

class Concrete : public Base<Concrete> {
public:
    void implementation() {
        // Derived implementation
    }
};

// No vtable, interface() is inlined, implementation() is inlined
```

**中文说明：**
CRTP 是一种静态多态技术：派生类作为模板参数传递给基类。基类可以通过 static_cast 调用派生类的方法，因为编译器知道具体类型，所以可以内联，没有虚表开销。

---

## 3. Idiomatic C++ Techniques

### Template Duck Typing

```cpp
// No base class needed! Just satisfy the interface.
template<typename Logger>
void doWork(Logger& logger) {
    logger.log("Starting work");
    // ... do something ...
    logger.log("Work complete");
}

class FileLogger {
public:
    void log(std::string_view msg) { /* write to file */ }
};

class ConsoleLogger {
public:
    void log(std::string_view msg) { std::cout << msg << "\n"; }
};

// Both work - no inheritance needed
FileLogger file;
ConsoleLogger console;
doWork(file);     // Generates doWork<FileLogger>
doWork(console);  // Generates doWork<ConsoleLogger>
```

### CRTP for Static Interface Enforcement

```cpp
template<typename Derived>
class Addable {
public:
    Derived operator+(const Derived& other) const {
        Derived result = static_cast<const Derived&>(*this);
        result += other;  // Derived must implement +=
        return result;
    }
};

class Vector2D : public Addable<Vector2D> {
    double x_, y_;
public:
    Vector2D(double x, double y) : x_(x), y_(y) {}
    
    Vector2D& operator+=(const Vector2D& other) {
        x_ += other.x_;
        y_ += other.y_;
        return *this;
    }
};

// Now Vector2D has operator+ for free!
Vector2D v1(1, 2), v2(3, 4);
Vector2D v3 = v1 + v2;  // Uses CRTP-provided operator+
```

### Policy-Based Design

```cpp
// Policies are template parameters that inject behavior
template<typename StoragePolicy, typename ValidationPolicy>
class Container : private StoragePolicy, private ValidationPolicy {
public:
    void add(int value) {
        if (ValidationPolicy::validate(value)) {
            StoragePolicy::store(value);
        }
    }
};

struct HeapStorage {
    std::vector<int> data_;
    void store(int v) { data_.push_back(v); }
};

struct StackStorage {
    std::array<int, 100> data_;
    size_t size_ = 0;
    void store(int v) { data_[size_++] = v; }
};

struct NoValidation {
    static bool validate(int) { return true; }
};

struct PositiveOnly {
    static bool validate(int v) { return v > 0; }
};

// Different containers via template parameters
using HeapContainer = Container<HeapStorage, NoValidation>;
using StackPositive = Container<StackStorage, PositiveOnly>;
```

---

## 4. Complete C++ Example

```cpp
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

// ============================================================
// CRTP Base: Provides interface and common functionality
// ============================================================
template<typename Derived>
class Shape {
public:
    double area() const {
        return static_cast<const Derived*>(this)->areaImpl();
    }
    
    double perimeter() const {
        return static_cast<const Derived*>(this)->perimeterImpl();
    }
    
    void describe() const {
        std::cout << "Shape with area=" << area() 
                  << ", perimeter=" << perimeter() << "\n";
    }
};

class Circle : public Shape<Circle> {
    double radius_;
    friend class Shape<Circle>;
    
    double areaImpl() const {
        return 3.14159265359 * radius_ * radius_;
    }
    
    double perimeterImpl() const {
        return 2 * 3.14159265359 * radius_;
    }
    
public:
    explicit Circle(double r) : radius_(r) {}
};

class Rectangle : public Shape<Rectangle> {
    double width_, height_;
    friend class Shape<Rectangle>;
    
    double areaImpl() const {
        return width_ * height_;
    }
    
    double perimeterImpl() const {
        return 2 * (width_ + height_);
    }
    
public:
    Rectangle(double w, double h) : width_(w), height_(h) {}
};

// ============================================================
// Template function: Static polymorphism
// ============================================================
template<typename ShapeT>
double totalArea(const std::vector<ShapeT>& shapes) {
    double sum = 0;
    for (const auto& s : shapes) {
        sum += s.area();  // Fully inlined
    }
    return sum;
}

// ============================================================
// Policy-based design example
// ============================================================
template<typename ComparePolicy>
class Sorter {
public:
    template<typename Iter>
    void sort(Iter begin, Iter end) {
        std::sort(begin, end, ComparePolicy::compare);
    }
};

struct Ascending {
    static bool compare(int a, int b) { return a < b; }
};

struct Descending {
    static bool compare(int a, int b) { return a > b; }
};

struct ByAbsoluteValue {
    static bool compare(int a, int b) { return std::abs(a) < std::abs(b); }
};

// ============================================================
// CRTP mixin: Add functionality to any class
// ============================================================
template<typename Derived>
class Cloneable {
public:
    std::unique_ptr<Derived> clone() const {
        return std::make_unique<Derived>(
            static_cast<const Derived&>(*this)
        );
    }
};

template<typename Derived>
class Printable {
public:
    friend std::ostream& operator<<(std::ostream& os, const Derived& d) {
        d.print(os);
        return os;
    }
};

class Point : public Cloneable<Point>, public Printable<Point> {
    double x_, y_;
    
public:
    Point(double x, double y) : x_(x), y_(y) {}
    
    void print(std::ostream& os) const {
        os << "(" << x_ << ", " << y_ << ")";
    }
};

// ============================================================
// Performance comparison
// ============================================================
class VirtualShape {
public:
    virtual ~VirtualShape() = default;
    virtual double area() const = 0;
};

class VirtualCircle : public VirtualShape {
    double radius_;
public:
    explicit VirtualCircle(double r) : radius_(r) {}
    double area() const override {
        return 3.14159265359 * radius_ * radius_;
    }
};

void benchmarkComparison() {
    constexpr int N = 10'000'000;
    
    // Static polymorphism
    std::vector<Circle> circles;
    circles.reserve(N);
    for (int i = 0; i < N; ++i) {
        circles.emplace_back(1.0 + i * 0.001);
    }
    
    auto start1 = std::chrono::high_resolution_clock::now();
    double sum1 = totalArea(circles);
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // Dynamic polymorphism
    std::vector<std::unique_ptr<VirtualShape>> vshapes;
    vshapes.reserve(N);
    for (int i = 0; i < N; ++i) {
        vshapes.push_back(std::make_unique<VirtualCircle>(1.0 + i * 0.001));
    }
    
    auto start2 = std::chrono::high_resolution_clock::now();
    double sum2 = 0;
    for (const auto& s : vshapes) {
        sum2 += s->area();  // Virtual call
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    
    auto us1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto us2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
    std::cout << "Static polymorphism:  " << us1.count() << " us (sum=" << sum1 << ")\n";
    std::cout << "Dynamic polymorphism: " << us2.count() << " us (sum=" << sum2 << ")\n";
}

int main() {
    std::cout << "=== CRTP Shapes ===\n";
    Circle c(5.0);
    Rectangle r(3.0, 4.0);
    
    c.describe();
    r.describe();
    
    std::cout << "\n=== Template Polymorphism ===\n";
    std::vector<Circle> circles = { Circle(1), Circle(2), Circle(3) };
    std::cout << "Total circle area: " << totalArea(circles) << "\n";
    
    std::cout << "\n=== Policy-Based Sorting ===\n";
    std::vector<int> data1 = {3, -1, 4, -1, 5, -9, 2, 6};
    std::vector<int> data2 = data1;
    std::vector<int> data3 = data1;
    
    Sorter<Ascending>{}.sort(data1.begin(), data1.end());
    Sorter<Descending>{}.sort(data2.begin(), data2.end());
    Sorter<ByAbsoluteValue>{}.sort(data3.begin(), data3.end());
    
    std::cout << "Ascending: ";
    for (int x : data1) std::cout << x << " ";
    std::cout << "\nDescending: ";
    for (int x : data2) std::cout << x << " ";
    std::cout << "\nBy |value|: ";
    for (int x : data3) std::cout << x << " ";
    std::cout << "\n";
    
    std::cout << "\n=== CRTP Mixins ===\n";
    Point p(3.0, 4.0);
    std::cout << "Point: " << p << "\n";
    
    auto p2 = p.clone();
    std::cout << "Cloned: " << *p2 << "\n";
    
    std::cout << "\n=== Performance Comparison ===\n";
    benchmarkComparison();
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Forgetting to use static_cast in CRTP

```cpp
template<typename D>
class Base {
public:
    void interface() {
        implementation();  // WRONG: Calls Base::implementation!
        // Should be: static_cast<D*>(this)->implementation();
    }
};
```

### Mistake 2: CRTP with wrong derived type

```cpp
class Derived1 : public Base<Derived1> {};  // OK
class Derived2 : public Base<Derived1> {};  // BUG: Wrong type parameter!
// static_cast to wrong type -> undefined behavior
```

### Mistake 3: Template code bloat

```cpp
// Each instantiation generates new code
template<typename T>
void bigFunction(T t) {
    // 500 lines of code...
}

bigFunction(1);        // Generates bigFunction<int>
bigFunction(1.0);      // Generates bigFunction<double>
bigFunction(1.0f);     // Generates bigFunction<float>
// 3x code size!

// FIX: Factor out non-generic parts
void bigFunctionCommon(/* ... */);  // Non-template

template<typename T>
void bigFunction(T t) {
    auto converted = preprocess(t);  // Small template part
    bigFunctionCommon(converted);     // Big non-template part
}
```

---

## 6. When NOT to Use Static Polymorphism

### When Dynamic Polymorphism is Better

| Situation | Why Dynamic is Better |
|-----------|----------------------|
| Heterogeneous containers | Can't have vector<Circle, Rectangle> |
| Plugin systems | Types not known at compile time |
| Runtime configuration | Need to switch implementations |
| Reducing compile time | Templates increase compilation |
| ABI stability | Templates require recompilation |

### The Key Trade-off

```
STATIC POLYMORPHISM         DYNAMIC POLYMORPHISM
───────────────────────────────────────────────────
✓ Zero runtime cost         ✗ Vtable + indirect call
✓ Full inlining             ✗ Can't inline virtual
✓ Compile-time errors       ✗ Runtime errors possible
✗ Code bloat                ✓ Shared code
✗ Long compile times        ✓ Faster compilation
✗ No heterogeneous types    ✓ Different types in container
✗ Types must be known       ✓ Works with plugins
```

**中文说明：**
静态多态的代价是编译时间长和代码膨胀（每个类型实例化一份）。当需要异构容器（不同类型放一起）、插件系统或运行时配置时，动态多态更合适。关键是分析热路径——在高频调用点使用静态多态，在灵活性需求高的地方使用动态多态。

---

## Summary

```
+------------------------------------------------------------------+
|              STATIC POLYMORPHISM TOOLKIT                          |
+------------------------------------------------------------------+
|                                                                  |
|  TECHNIQUE           │  USE CASE                                 |
|  ────────────────────│───────────────────────────────────────────|
|  Template functions  │  Algorithm that works with any type       |
|  CRTP                │  Add interface/mixins without virtual     |
|  Policy classes      │  Inject configurable behavior             |
|  Concepts (C++20)    │  Constrain template parameters cleanly    |
|                                                                  |
|  BEST PRACTICES:                                                 |
|  ────────────────────────────────────────────────────────────────|
|  • Use in hot paths where inlining matters                       |
|  • Factor out non-generic code to reduce bloat                   |
|  • Consider concepts for better error messages                   |
|  • Use virtual for heterogeneous containers                      |
|                                                                  |
+------------------------------------------------------------------+
```

