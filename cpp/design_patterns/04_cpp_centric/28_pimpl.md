# Pattern 28: PIMPL (Pointer to Implementation)

## 1. Problem the Pattern Solves

### Design Pressure
- Reduce compilation dependencies
- Hide implementation details from headers
- Maintain ABI stability across library versions

### What Goes Wrong Without It
```cpp
// widget.h
#include <string>        // Changes here →
#include <vector>        // Recompile ALL users
#include "database.h"    // of widget.h

class Widget {
    std::string name_;   // Private details in header!
    std::vector<int> data_;
    Database db_;
};
```

---

## 2. Core Idea (C++-Specific)

**PIMPL hides implementation details behind an opaque pointer, creating a compilation firewall.**

```
widget.h                    widget.cpp
+------------+              +------------------+
| class Widget              | struct Widget::Impl
| unique_ptr<Impl> pimpl;   |   std::string name;
+------------+              |   std::vector data;
                            +------------------+
```

Header only forward-declares `Impl`; definition is in `.cpp`.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Forward declaration | `struct Impl;` | Incomplete type |
| `std::unique_ptr` | Hold impl | Automatic cleanup |
| Destructor in .cpp | `~Widget()` | Complete type needed |

---

## 4. Canonical C++ Implementation

### Header (widget.h)

```cpp
#ifndef WIDGET_H
#define WIDGET_H

#include <memory>
#include <string>

class Widget {
public:
    Widget();
    ~Widget();  // Must be in .cpp
    
    // Move operations (in .cpp)
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
    
    // No copy (or implement in .cpp)
    Widget(const Widget&) = delete;
    Widget& operator=(const Widget&) = delete;
    
    void setName(const std::string& name);
    std::string getName() const;
    
private:
    struct Impl;                       // Forward declaration
    std::unique_ptr<Impl> pimpl_;
};

#endif
```

### Implementation (widget.cpp)

```cpp
#include "widget.h"
#include <vector>    // Heavy includes only here!
#include <algorithm>

struct Widget::Impl {
    std::string name;
    std::vector<int> data;
    // ... more members
};

Widget::Widget() : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;  // Impl complete here
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::setName(const std::string& name) {
    pimpl_->name = name;
}

std::string Widget::getName() const {
    return pimpl_->name;
}
```

---

## 5. Typical Usage

| Use Case | Benefit |
|----------|---------|
| Library APIs | ABI stability |
| Large classes | Compile time |
| Platform abstraction | Hide OS details |

---

## 6. Common Mistakes

### ❌ Destructor in Header

```cpp
// BAD: unique_ptr needs complete type for deletion
class Widget {
    ~Widget() = default;  // ERROR if Impl incomplete
};
// FIX: Define destructor in .cpp
```

### ❌ Extra Indirection Cost

```cpp
// Every member access goes through pointer
pimpl_->member;  // Indirection
// Don't use PIMPL for hot paths
```

---

## 7. PIMPL vs Bridge

| Aspect | PIMPL | Bridge |
|--------|-------|--------|
| Purpose | Hide impl / ABI | Vary abstraction & impl |
| Polymorphism | No | Yes (both sides) |

---

## 8. Mental Model Summary

**When PIMPL "Clicks":**

Use PIMPL for **compilation firewall** and **ABI stability** in library interfaces. Accept the indirection cost for better encapsulation and faster incremental builds.

---

## 中文说明

### PIMPL 要点

1. **核心好处**：
   - 减少编译依赖
   - 隐藏实现细节
   - 保持 ABI 稳定

2. **实现要点**：
   - 前向声明 `struct Impl`
   - 析构函数在 .cpp 中定义
   - 移动操作在 .cpp 中定义

3. **权衡**：
   - 优点：编译隔离
   - 缺点：指针间接访问开销

