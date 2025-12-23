# Pattern 26: CRTP (Curiously Recurring Template Pattern)

## 1. Problem the Pattern Solves

### Design Pressure
- Want polymorphic behavior without virtual function overhead
- Reuse code across derived classes
- Implement "static interfaces"

### What Goes Wrong Without It
```cpp
// Virtual dispatch has runtime cost
class Base { virtual void impl() = 0; };
// Every call goes through vtable
```

---

## 2. Core Idea (C++-Specific)

**CRTP is a C++ idiom where a class derives from a template instantiated with itself as the argument.**

```cpp
template<typename Derived>
class Base {
    void interface() {
        static_cast<Derived*>(this)->implementation();
    }
};

class Concrete : public Base<Concrete> {
    void implementation() { /* ... */ }
};
```

All dispatch resolved at **compile time** → zero runtime overhead.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `static_cast<Derived*>` | Downcast | Access derived |
| Template class | Base class | Parameterized |
| No `virtual` | Static dispatch | Zero overhead |

---

## 4. Canonical C++ Implementation

### Static Polymorphism

```cpp
#include <iostream>

template<typename Derived>
class Shape {
public:
    void draw() {
        // Compile-time "virtual" call
        static_cast<Derived*>(this)->drawImpl();
    }
    
    double area() {
        return static_cast<Derived*>(this)->areaImpl();
    }
};

class Circle : public Shape<Circle> {
public:
    double radius = 5.0;
    
    void drawImpl() {
        std::cout << "Drawing circle r=" << radius << "\n";
    }
    
    double areaImpl() {
        return 3.14159 * radius * radius;
    }
};

class Rectangle : public Shape<Rectangle> {
public:
    double width = 4.0, height = 3.0;
    
    void drawImpl() {
        std::cout << "Drawing rect " << width << "x" << height << "\n";
    }
    
    double areaImpl() {
        return width * height;
    }
};

// Usage (note: not polymorphic via base pointer!)
template<typename T>
void render(Shape<T>& shape) {
    shape.draw();
    std::cout << "Area: " << shape.area() << "\n";
}

int main() {
    Circle c;
    Rectangle r;
    
    render(c);
    render(r);
    
    return 0;
}
```

### Mixin Pattern with CRTP

```cpp
template<typename Derived>
class Counter {
    static inline int count_ = 0;
public:
    Counter() { ++count_; }
    ~Counter() { --count_; }
    static int count() { return count_; }
};

class Widget : public Counter<Widget> {
    // Automatically counts Widget instances
};

class Button : public Counter<Button> {
    // Separate count for Button instances
};
```

### Enable-from-this (std Pattern)

```cpp
#include <memory>

class Widget : public std::enable_shared_from_this<Widget> {
public:
    std::shared_ptr<Widget> getShared() {
        return shared_from_this();
    }
};
```

---

## 5. Typical Usage

| Use Case | Example |
|----------|---------|
| Static polymorphism | Shape::draw |
| Mixins | Counter, Printable |
| enable_shared_from_this | Shared pointer |
| Compile-time interfaces | Policy injection |

---

## 6. Common Mistakes

### ❌ Calling Derived Before Construction

```cpp
template<typename D>
class Base {
    Base() {
        static_cast<D*>(this)->method();  // BUG: D not constructed!
    }
};
```

### ❌ Wrong Derived Type

```cpp
class A : public Base<B> { };  // Oops, should be Base<A>!
```

---

## 7. CRTP vs Virtual

| Aspect | CRTP | Virtual |
|--------|------|---------|
| Dispatch | Compile-time | Runtime |
| Overhead | Zero | Vtable lookup |
| Flexibility | Fixed at compile | Changeable |
| Containers | Need template | Homogeneous via base* |

---

## 8. Mental Model Summary

**When CRTP "Clicks":**

Use CRTP for **static polymorphism**—when you want polymorphic behavior with **zero runtime overhead** and type is known at compile time. It's not a replacement for virtual when you need runtime flexibility.

---

## 中文说明

### CRTP 要点

1. **核心结构**：
   ```cpp
   class Derived : public Base<Derived>
   ```

2. **用途**：
   - 静态多态（无虚函数开销）
   - Mixin（混入功能）
   - `enable_shared_from_this`

3. **与虚函数区别**：
   - CRTP：编译时分派，零开销
   - 虚函数：运行时分派，可替换

4. **常见错误**：
   - 构造函数中调用派生类方法
   - 继承错误的特化类型

