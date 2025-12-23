# Topic 23: PIMPL Idiom and ABI Stability

## 1. Problem Statement

### What real engineering problem does this solve?

Changing class internals requires recompiling all users:

```
WITHOUT PIMPL                        WITH PIMPL
class Widget {                       class Widget {
    int a_, b_, c_;  // Change this      struct Impl;
    std::string s_;  // or this          std::unique_ptr<Impl> pimpl_;
};                                   };
                                     
Changes require:                     Changes require:
- Recompile ALL users                - Recompile Widget.cpp ONLY
- Break binary compatibility         - Binary compatibility preserved
```

### ABI (Application Binary Interface)

- **ABI** defines how compiled code interacts
- Includes: struct layout, name mangling, calling conventions
- Changing class members breaks ABI
- Shared libraries need stable ABI

**中文说明：**
PIMPL（Pointer to Implementation）将类的实现细节隐藏在不透明指针后面。公共头文件只暴露接口，私有成员放在 .cpp 文件中。这带来：编译防火墙（减少重编译）、ABI 稳定性（共享库升级无需重编译用户）、信息隐藏（实现细节完全封装）。

---

## 2. Core Idea

### PIMPL Pattern

```cpp
// widget.h (public header)
class Widget {
public:
    Widget();
    ~Widget();
    
    // Public interface
    void doSomething();
    int getValue() const;
    
private:
    struct Impl;                    // Forward declaration
    std::unique_ptr<Impl> pimpl_;   // Pointer to implementation
};

// widget.cpp (implementation)
#include "widget.h"
#include <vector>
#include <string>
#include "heavy_dependency.h"

struct Widget::Impl {
    std::vector<std::string> data_;
    HeavyDependency dep_;
    int value_ = 0;
};

Widget::Widget() : pimpl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;  // Must be in .cpp where Impl is complete

void Widget::doSomething() {
    pimpl_->data_.push_back("item");
}

int Widget::getValue() const {
    return pimpl_->value_;
}
```

### ABI Stability

```
STABLE ABI:
┌─────────────────────────────────────────────────────────────────┐
│ Library v1.0            Library v2.0                            │
│ ┌───────────────┐       ┌───────────────┐                       │
│ │ Widget        │       │ Widget        │                       │
│ │ ├ pimpl_ (8B) │       │ ├ pimpl_ (8B) │  Same size!           │
│ └───────────────┘       └───────────────┘                       │
│                                                                 │
│ Impl v1:                Impl v2 (changed):                      │
│ ├ int a_               ├ int a_                                │
│ └ int b_               ├ int b_                                │
│                        └ int c_  (NEW)                         │
│                                                                 │
│ User code compiled against v1.0 works with v2.0 library!        │
└─────────────────────────────────────────────────────────────────┘
```

**中文说明：**
ABI 稳定性意味着升级共享库时，已编译的客户端代码无需重编译。PIMPL 保证公共类的大小（只有一个指针）永不改变，所有实现细节都在堆分配的 Impl 对象中。

---

## 3. Idiomatic C++ Techniques

### Basic PIMPL

```cpp
// Minimal PIMPL
class MyClass {
public:
    MyClass();
    ~MyClass();
    MyClass(MyClass&&) noexcept;
    MyClass& operator=(MyClass&&) noexcept;
    
    void doWork();
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};
```

### PIMPL with Copy Support

```cpp
class Copyable {
public:
    Copyable();
    ~Copyable();
    
    Copyable(const Copyable& other);
    Copyable& operator=(const Copyable& other);
    
    Copyable(Copyable&&) noexcept;
    Copyable& operator=(Copyable&&) noexcept;
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

// In .cpp
Copyable::Copyable(const Copyable& other)
    : pimpl_(std::make_unique<Impl>(*other.pimpl_)) {}

Copyable& Copyable::operator=(const Copyable& other) {
    *pimpl_ = *other.pimpl_;
    return *this;
}
```

### Fast PIMPL (Small Buffer Optimization)

```cpp
class FastPimpl {
public:
    FastPimpl();
    ~FastPimpl();
    
    void doWork();
    
private:
    struct Impl;
    static constexpr size_t ImplSize = 64;
    static constexpr size_t ImplAlign = 8;
    
    alignas(ImplAlign) char storage_[ImplSize];
    Impl* pimpl() { return reinterpret_cast<Impl*>(storage_); }
};
```

---

## 4. Complete C++ Example

```cpp
// ============================================================
// database_client.h - Public header
// ============================================================
#ifndef DATABASE_CLIENT_H
#define DATABASE_CLIENT_H

#include <memory>
#include <string>
#include <vector>

class DatabaseClient {
public:
    // Construction
    DatabaseClient(const std::string& connectionString);
    ~DatabaseClient();
    
    // Move only (non-copyable)
    DatabaseClient(DatabaseClient&&) noexcept;
    DatabaseClient& operator=(DatabaseClient&&) noexcept;
    DatabaseClient(const DatabaseClient&) = delete;
    DatabaseClient& operator=(const DatabaseClient&) = delete;
    
    // Public interface
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    std::vector<std::string> query(const std::string& sql);
    int execute(const std::string& sql);
    
    // Transaction support
    void beginTransaction();
    void commit();
    void rollback();
    
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif

// ============================================================
// database_client.cpp - Implementation
// ============================================================
#include "database_client.h"

// Heavy includes only in .cpp
#include <unordered_map>
#include <queue>
#include <mutex>
// #include "mysql_driver.h"  // Imagine this is heavy
// #include "connection_pool.h"

struct DatabaseClient::Impl {
    std::string connectionString_;
    bool connected_ = false;
    bool inTransaction_ = false;
    
    // Simulated heavy members
    std::unordered_map<std::string, std::string> cache_;
    std::queue<std::string> queryLog_;
    std::mutex mutex_;
    
    explicit Impl(std::string connStr) 
        : connectionString_(std::move(connStr)) {}
};

DatabaseClient::DatabaseClient(const std::string& connectionString)
    : pimpl_(std::make_unique<Impl>(connectionString)) {}

DatabaseClient::~DatabaseClient() = default;

DatabaseClient::DatabaseClient(DatabaseClient&&) noexcept = default;
DatabaseClient& DatabaseClient::operator=(DatabaseClient&&) noexcept = default;

bool DatabaseClient::connect() {
    // Simulate connection
    pimpl_->connected_ = true;
    return true;
}

void DatabaseClient::disconnect() {
    pimpl_->connected_ = false;
}

bool DatabaseClient::isConnected() const {
    return pimpl_->connected_;
}

std::vector<std::string> DatabaseClient::query(const std::string& sql) {
    std::lock_guard lock(pimpl_->mutex_);
    pimpl_->queryLog_.push(sql);
    
    // Simulate query result
    return {"row1", "row2", "row3"};
}

int DatabaseClient::execute(const std::string& sql) {
    std::lock_guard lock(pimpl_->mutex_);
    pimpl_->queryLog_.push(sql);
    return 1;  // Affected rows
}

void DatabaseClient::beginTransaction() {
    pimpl_->inTransaction_ = true;
}

void DatabaseClient::commit() {
    pimpl_->inTransaction_ = false;
}

void DatabaseClient::rollback() {
    pimpl_->inTransaction_ = false;
}

// ============================================================
// main.cpp - Usage
// ============================================================
#include <iostream>
#include "database_client.h"

int main() {
    DatabaseClient db("host=localhost;db=mydb");
    
    if (db.connect()) {
        std::cout << "Connected!\n";
        
        auto results = db.query("SELECT * FROM users");
        for (const auto& row : results) {
            std::cout << row << "\n";
        }
        
        db.disconnect();
    }
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Destructor in header

```cpp
class Bad {
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
public:
    Bad();
    ~Bad() = default;  // ERROR: Impl is incomplete here!
};

// FIX: Define destructor in .cpp
```

### Mistake 2: Inline methods accessing pimpl

```cpp
class Bad {
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
public:
    int getValue() const { return pimpl_->value_; }  // ERROR!
};

// FIX: Define in .cpp where Impl is complete
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              PIMPL IMPLEMENTATION CHECKLIST                       |
+------------------------------------------------------------------+
|                                                                  |
|  HEADER FILE:                                                    |
|    □ Forward declare: struct Impl;                               |
|    □ unique_ptr<Impl> member                                     |
|    □ Declare (don't define) special members                      |
|    □ No inline methods that access pimpl_                        |
|                                                                  |
|  CPP FILE:                                                       |
|    □ Define struct Impl with all data members                    |
|    □ Define constructor(s) with make_unique<Impl>()              |
|    □ Define destructor: ~Class() = default;                      |
|    □ Define move operations: = default;                          |
|    □ Define copy operations if needed                            |
|                                                                  |
|  WHEN TO USE PIMPL:                                              |
|    □ Public library APIs                                         |
|    □ Stable ABI required                                         |
|    □ Heavy implementation dependencies                           |
|    □ Compile firewall needed                                     |
|                                                                  |
|  TRADEOFFS:                                                      |
|    ✓ ABI stability                                               |
|    ✓ Faster compilation                                          |
|    ✗ Extra indirection (heap access)                             |
|    ✗ Extra memory (heap allocation)                              |
|    ✗ More boilerplate code                                       |
|                                                                  |
+------------------------------------------------------------------+
```

