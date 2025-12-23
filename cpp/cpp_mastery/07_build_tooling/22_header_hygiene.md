# Topic 22: Header Hygiene and Compile-Time Reduction

## 1. Problem Statement

### What real engineering problem does this solve?

C++ compilation is slow because of header inclusion:

```
POOR HEADER HYGIENE                  GOOD HEADER HYGIENE
every_header.h includes:             focused_header.h includes:
  - <iostream>                         - Forward declarations
  - <vector>                           - Only what's needed
  - <algorithm>                        
  - <memory>                         Implementation file:
  - <string>                           - Full includes
  - ... everything                   
                                     
100 .cpp files include it            Result: 10x faster builds
→ Parse same headers 100x
```

### Symptoms of Header Problems

- Long compile times
- Small change triggers full rebuild
- Circular dependencies
- Include order matters

**中文说明：**
C++ 头文件在每个包含它的 .cpp 文件中重复解析。一个"重量级"头文件被包含 100 次，就解析 100 次。头文件卫生的目标是：最小化依赖、使用前置声明、将实现细节从头文件移到 .cpp 文件。

---

## 2. Core Idea

### Forward Declarations

```cpp
// widget.h - BEFORE (heavy)
#include <memory>
#include <string>
#include "database.h"
#include "logger.h"
#include "config.h"

class Widget {
    std::unique_ptr<Database> db_;
    Logger logger_;
    Config config_;
};

// widget.h - AFTER (light)
#include <memory>
class Database;  // Forward declaration
class Logger;
class Config;

class Widget {
    std::unique_ptr<Database> db_;  // OK: pointer/ref to incomplete type
    Logger* logger_;                 // OK: pointer
    Config* config_;                 // OK: pointer
};

// widget.cpp
#include "widget.h"
#include "database.h"  // Full include only here
#include "logger.h"
#include "config.h"
```

### Include What You Use (IWYU)

```cpp
// WRONG: Rely on transitive includes
#include "a.h"  // a.h includes <vector>
std::vector<int> v;  // Works by accident

// RIGHT: Include directly what you use
#include <vector>
std::vector<int> v;
```

**中文说明：**
前置声明允许引用不完整类型的指针/引用，无需包含完整头文件。IWYU 原则要求直接包含你使用的东西，不依赖传递包含。这减少了编译依赖，也使代码更清晰。

---

## 3. Idiomatic C++ Techniques

### Minimize Public Header Includes

```cpp
// public_api.h (included by users)
#include <cstdint>  // Minimal, widely-used

class PublicClass {
public:
    void doSomething(int32_t value);
    // Return types that don't need full definition
    const char* getName() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;  // Only need <memory>
};

// public_api.cpp
#include "public_api.h"
#include <string>          // Only needed in implementation
#include <unordered_map>   // Only needed in implementation
#include "internal_lib.h"  // Only needed in implementation
```

### Precompiled Headers

```cpp
// pch.h - Precompiled header
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <iostream>
// All the heavy STL headers

// CMakeLists.txt
target_precompile_headers(mylib PRIVATE pch.h)
```

### Module (C++20)

```cpp
// math.cppm - Module interface
export module math;

export int add(int a, int b) {
    return a + b;
}

// main.cpp
import math;

int main() {
    return add(1, 2);
}
```

---

## 4. Complete Example

### Bad Header Structure

```cpp
// === bad_widget.h ===
#ifndef BAD_WIDGET_H
#define BAD_WIDGET_H

#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <map>
#include <iostream>  // Never include in headers!

#include "heavy_library.h"
#include "another_heavy_lib.h"

class BadWidget {
    std::vector<std::string> names_;
    std::map<int, std::string> mapping_;
    HeavyLibraryType heavy_;
    AnotherHeavyType another_;
    
public:
    void process() {
        // Implementation in header!
        for (auto& name : names_) {
            std::cout << name << "\n";  // I/O in header
        }
    }
};

#endif
```

### Good Header Structure

```cpp
// === good_widget.h ===
#ifndef GOOD_WIDGET_H
#define GOOD_WIDGET_H

#include <memory>
#include <cstddef>

// Forward declarations
class HeavyLibraryType;
class AnotherHeavyType;

namespace std {
    template<class T> class vector;
    template<class K, class V> class map;
}

class GoodWidget {
public:
    GoodWidget();
    ~GoodWidget();
    
    GoodWidget(GoodWidget&&) noexcept;
    GoodWidget& operator=(GoodWidget&&) noexcept;
    
    void process();
    size_t count() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif

// === good_widget.cpp ===
#include "good_widget.h"

#include <vector>
#include <string>
#include <map>
#include <iostream>
#include "heavy_library.h"
#include "another_heavy_lib.h"

struct GoodWidget::Impl {
    std::vector<std::string> names_;
    std::map<int, std::string> mapping_;
    HeavyLibraryType heavy_;
    AnotherHeavyType another_;
};

GoodWidget::GoodWidget() : pimpl_(std::make_unique<Impl>()) {}
GoodWidget::~GoodWidget() = default;
GoodWidget::GoodWidget(GoodWidget&&) noexcept = default;
GoodWidget& GoodWidget::operator=(GoodWidget&&) noexcept = default;

void GoodWidget::process() {
    for (auto& name : pimpl_->names_) {
        std::cout << name << "\n";
    }
}

size_t GoodWidget::count() const {
    return pimpl_->names_.size();
}
```

---

## 5. Failure Modes

### Mistake 1: Including implementation headers

```cpp
// BAD
#include <iostream>  // Huge, rarely needed in headers

// GOOD: Use <iosfwd> for forward declarations
#include <iosfwd>
std::ostream& operator<<(std::ostream& os, const MyClass& obj);
```

### Mistake 2: Circular includes

```cpp
// a.h
#include "b.h"
class A { B* b; };

// b.h
#include "a.h"  // Circular!
class B { A* a; };

// FIX: Use forward declarations
// a.h
class B;
class A { B* b; };

// b.h
class A;
class B { A* a; };
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              HEADER HYGIENE CHECKLIST                             |
+------------------------------------------------------------------+
|                                                                  |
|  REDUCE INCLUDES:                                                |
|    □ Forward declare when pointer/reference is enough            |
|    □ Never include <iostream> in headers                         |
|    □ Use <iosfwd> for stream forward declarations                |
|    □ Move implementation to .cpp files                           |
|                                                                  |
|  ORGANIZE:                                                       |
|    □ Include what you use (IWYU)                                 |
|    □ Use include guards or #pragma once                          |
|    □ Order: own header, project headers, external, std           |
|                                                                  |
|  OPTIMIZE:                                                       |
|    □ Use precompiled headers for stable dependencies             |
|    □ Consider PIMPL for heavy classes                            |
|    □ Use C++20 modules when available                            |
|                                                                  |
+------------------------------------------------------------------+
```

