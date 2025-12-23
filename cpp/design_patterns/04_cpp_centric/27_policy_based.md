# Pattern 27: Policy-Based Design

## 1. Problem the Pattern Solves

### Design Pressure
- Need to vary multiple aspects of a class independently
- Avoid combinatorial explosion of subclasses
- Want compile-time flexibility and zero overhead

### What Goes Wrong Without It
```cpp
// Class explosion:
class ThreadSafeLoggerToFile {};
class ThreadSafeLoggerToConsole {};
class SingleThreadLoggerToFile {};
class SingleThreadLoggerToConsole {};
// 2 threading × 2 output = 4 classes
```

---

## 2. Core Idea (C++-Specific)

**Policy-Based Design uses template parameters to inject behavior, allowing independent variation of multiple aspects.**

```cpp
template<typename ThreadingPolicy, typename OutputPolicy>
class Logger : public ThreadingPolicy, public OutputPolicy {
    void log(const std::string& msg) {
        ThreadingPolicy::lock();
        OutputPolicy::write(msg);
        ThreadingPolicy::unlock();
    }
};
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Template parameters | Policy injection | Compile-time config |
| Multiple inheritance | Combine policies | Orthogonal aspects |
| `static` methods | Stateless policies | No object needed |
| Concepts (C++20) | Constrain policies | Type safety |

---

## 4. Canonical C++ Implementation

```cpp
#include <iostream>
#include <mutex>
#include <string>

// Threading policies
struct SingleThreaded {
    void lock() {}
    void unlock() {}
};

struct MultiThreaded {
    std::mutex mtx_;
    void lock() { mtx_.lock(); }
    void unlock() { mtx_.unlock(); }
};

// Output policies
struct ConsoleOutput {
    static void write(const std::string& msg) {
        std::cout << msg << "\n";
    }
};

struct FileOutput {
    static void write(const std::string& msg) {
        // Write to file
    }
};

// Host class
template<typename ThreadingPolicy, typename OutputPolicy>
class Logger : private ThreadingPolicy {
public:
    void log(const std::string& msg) {
        this->lock();
        OutputPolicy::write(msg);
        this->unlock();
    }
};

// Usage
int main() {
    Logger<SingleThreaded, ConsoleOutput> simpleLogger;
    simpleLogger.log("Hello");
    
    Logger<MultiThreaded, ConsoleOutput> safeLogger;
    safeLogger.log("Thread-safe");
    
    return 0;
}
```

### Smart Pointer with Policies

```cpp
template<
    typename T,
    typename OwnershipPolicy,   // unique, shared, intrusive
    typename CheckingPolicy,    // none, null-check, bounds
    typename ThreadingPolicy    // single, multi
>
class SmartPtr;

// Like Loki library or Alexandrescu's Modern C++ Design
```

---

## 5. Typical Usage

| Use Case | Policies |
|----------|----------|
| Smart pointers | Ownership, checking, threading |
| Containers | Allocation, growth |
| Logging | Output, filtering, threading |
| Serialization | Format, encoding |

---

## 6. Common Mistakes

### ❌ Too Many Policies

```cpp
template<typename P1, typename P2, typename P3, 
         typename P4, typename P5, typename P6>
class TooManyPolicies;  // Unreadable!
// Keep to 2-4 policies max
```

### ❌ Policies with Dependencies

```cpp
// BAD: Policy A needs Policy B's types
// Policies should be orthogonal
```

---

## 7. Policy vs Strategy

| Aspect | Policy | Strategy |
|--------|--------|----------|
| Binding | Compile-time | Runtime |
| Overhead | Zero | Virtual call |
| Flexibility | Fixed | Dynamic |

---

## 8. Mental Model Summary

**When Policy-Based Design "Clicks":**

Use Policy-Based Design when you have **multiple orthogonal aspects** that vary independently and you want **compile-time configuration with zero overhead**. It's Strategy pattern at compile time.

---

## 中文说明

### 基于策略的设计要点

1. **核心思想**：
   - 模板参数注入行为
   - 独立变化的方面

2. **与策略模式区别**：
   - 基于策略设计：编译时
   - 策略模式：运行时

3. **使用建议**：
   - 策略应正交（相互独立）
   - 限制策略数量（2-4个）

4. **典型应用**：
   - 智能指针
   - 容器分配器
   - 日志系统

