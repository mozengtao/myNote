# Pattern 1: Singleton

## 1. Problem the Pattern Solves

### Design Pressure
- Need exactly ONE instance of a class to coordinate actions across the system
- Global access point required for shared resources (logger, config, connection pool)
- Instance creation must be controlled and lazy

### What Goes Wrong Without It
```cpp
// Multiple instances created accidentally
Logger log1;  // in file A
Logger log2;  // in file B
// Both write to same file → corruption
```

### Symptoms Indicating Need
- Multiple instances cause resource conflicts
- Configuration data duplicated and inconsistent
- System services (logging, metrics) scattered across code

---

## 2. Core Idea (C++-Specific)

**Singleton guarantees a class has only one instance and provides global access to it.**

In C++, the key mechanisms are:
1. **Private constructor** prevents external instantiation
2. **Static local variable** (C++11) provides thread-safe lazy initialization
3. **Deleted copy/move operations** prevent duplication

```
+------------------------------------------+
|            Client Code                   |
+------------------------------------------+
          |
          | Singleton::instance()
          v
+------------------------------------------+
|   static local instance (created once)   |
|   +----------------------------------+   |
|   |   Private Constructor            |   |
|   |   Private Members (state)        |   |
|   +----------------------------------+   |
+------------------------------------------+
```

**C++11 Magic:** Static local variables are initialized exactly once, even under concurrent access (guaranteed by the standard).

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `static` local variable | `static Singleton inst;` | Thread-safe lazy init (C++11) |
| `= delete` | Copy/move constructors | Prevent duplication |
| `private` constructor | Hide instantiation | Force single creation point |
| `inline` static method | Avoid ODR issues | Header-only singleton |
| `std::call_once` | Alternative init | Explicit thread synchronization |

### Compile-Time vs Runtime

| Aspect | Behavior |
|--------|----------|
| Instance creation | Runtime (lazy) |
| Thread safety | Guaranteed by compiler (static local) |
| Destruction order | Reverse of construction (LIFO) |

---

## 4. Canonical C++ Implementation

### Modern Meyers Singleton (C++11+)

```cpp
#include <iostream>
#include <string>

class Logger {
public:
    // Global access point
    static Logger& instance() {
        static Logger inst;  // Thread-safe in C++11+
        return inst;
    }

    // Delete copy and move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void log(const std::string& msg) {
        std::cout << "[LOG] " << msg << "\n";
    }

private:
    Logger() {
        std::cout << "Logger initialized\n";
    }
    ~Logger() {
        std::cout << "Logger destroyed\n";
    }
};

// Usage
int main() {
    Logger::instance().log("Application started");
    Logger::instance().log("Processing data");
    
    // Compile error: copy deleted
    // Logger copy = Logger::instance();
    
    return 0;
}
```

### Alternative: `std::call_once` for Complex Initialization

```cpp
#include <mutex>
#include <memory>

class Database {
public:
    static Database& instance() {
        std::call_once(init_flag_, []() {
            instance_.reset(new Database());
        });
        return *instance_;
    }

private:
    Database() { /* complex initialization */ }
    
    static std::unique_ptr<Database> instance_;
    static std::once_flag init_flag_;
};

std::unique_ptr<Database> Database::instance_;
std::once_flag Database::init_flag_;
```

---

## 5. Typical Usage in Real Projects

### Common Applications

| Domain | Example |
|--------|---------|
| Logging | `spdlog::get()`, application-wide logger |
| Configuration | Runtime config loaded once |
| Connection pools | Database connection manager |
| Hardware access | Device driver interfaces |
| Thread pools | Worker thread management |

### Real-World Examples

- **Qt**: `QCoreApplication::instance()`
- **Vulkan/OpenGL**: Context managers
- **Game engines**: Asset managers, audio systems
- **Linux kernel**: Equivalent `static` module-level state

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Using Singleton for Convenience (Not Necessity)

```cpp
// BAD: Singleton just for global access
class UserPreferences : public Singleton<UserPreferences> {
    // This should probably be passed as a parameter
};
```

### ❌ Mistake 2: Hidden Dependencies

```cpp
void processOrder(Order& o) {
    // Hidden dependency on Logger singleton
    Logger::instance().log("Processing order");
    
    // Better: inject logger
    // void processOrder(Order& o, Logger& log);
}
```

### ❌ Mistake 3: Static Initialization Order Fiasco

```cpp
// file1.cpp
static ConfigManager& cfg = ConfigManager::instance();

// file2.cpp
static Logger& log = Logger::instance();
// If Logger uses ConfigManager, order is undefined!
```

**Solution:** Use function-local statics (Meyers Singleton) or explicit initialization order.

### ❌ Mistake 4: Destruction Order Issues

```cpp
class ResourceA {
    ~ResourceA() {
        // ResourceB might already be destroyed!
        ResourceB::instance().cleanup();
    }
};
```

---

## 7. When NOT to Use This Pattern

### Anti-Use Cases

| Situation | Better Alternative |
|-----------|-------------------|
| Testing with mocks | Dependency injection |
| Multiple configurations | Factory + injection |
| Stateless utilities | Free functions or `static` methods |
| Short-lived objects | Regular construction |

### Simpler Alternatives

```cpp
// Instead of Logger singleton, use dependency injection:
class Service {
    Logger& logger_;
public:
    explicit Service(Logger& log) : logger_(log) {}
};

// Or module-level static (if you control the module)
namespace {
    Logger module_logger;
}
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### Template-Based Singleton (CRTP)

```cpp
template<typename T>
class Singleton {
public:
    static T& instance() {
        static T inst;
        return inst;
    }
protected:
    Singleton() = default;
    ~Singleton() = default;
};

class MyService : public Singleton<MyService> {
    friend class Singleton<MyService>;
    MyService() = default;
};
```

### C++17: `inline static` for Header-Only

```cpp
class Config {
public:
    static Config& instance() {
        static Config inst;
        return inst;
    }
    
    inline static std::string app_name = "MyApp";  // C++17
};
```

### Modern Alternative: Dependency Injection Container

```cpp
// Use a DI framework instead of singletons
struct Services {
    std::unique_ptr<Logger> logger;
    std::unique_ptr<Database> database;
};

void run(Services& svc) {
    svc.logger->log("Starting...");
}
```

---

## 9. Mental Model Summary

**When Singleton "Clicks":**

Use Singleton when you genuinely need **exactly one instance** that is **shared across the entire application** and the **instance represents a unique physical or logical resource** (hardware device, thread pool, global configuration).

**Code Review Recognition:**
- Look for `::instance()` or `::get()` calls
- Check if the singleton is hiding dependencies (bad)
- Verify thread safety of the implementation
- Question: "Does this truly need to be global?"

---

## 中文说明

### 单例模式要点

1. **问题场景**：系统中某资源只能有一个实例（如日志、配置、连接池）
2. **C++ 实现关键**：
   - 私有构造函数
   - `static` 局部变量（C++11 保证线程安全）
   - 删除拷贝/移动操作
3. **常见错误**：
   - 为了方便全局访问而滥用单例
   - 隐藏依赖关系，难以测试
   - 静态初始化顺序问题
4. **替代方案**：依赖注入、模块级静态变量

### 何时使用

```
是否需要全局唯一实例？
    ├── 否 → 不要使用单例
    └── 是 → 这个实例代表唯一资源吗（硬件、系统服务）？
              ├── 否 → 考虑依赖注入
              └── 是 → 可以使用单例
```

