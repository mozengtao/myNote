# Topic 9: Exception Safety Guarantees

## 1. Problem Statement

### What real engineering problem does this solve?

When operations fail, what state is the program in?

```
BEFORE                    OPERATION                  AFTER (if exception)
┌──────────────┐         ┌─────────┐               ┌──────────────┐
│ Valid State  │────────>│ May     │──exception──>│ ??? State    │
│ Invariants OK│         │ Throw   │               │ Corrupted?   │
└──────────────┘         └─────────┘               │ Leaked?      │
                                                   │ Half-done?   │
                                                   └──────────────┘
```

Without exception safety guarantees:
- Resources may leak
- Data structures may be corrupted
- Invariants may be violated
- Recovery may be impossible

### What goes wrong if ignored?

```cpp
class BrokenContainer {
    T* data_;
    size_t size_;
    
public:
    void push_back(T value) {
        T* newData = new T[size_ + 1];  // May throw
        std::copy(data_, data_ + size_, newData);  // May throw
        delete[] data_;  // What if copy threw? data_ already freed!
        newData[size_] = std::move(value);  // May throw
        data_ = newData;
        ++size_;  // Never reached if anything threw
    }
};
// If exception thrown: possibly leaked, possibly double-freed, 
// size_ wrong, data_ dangling
```

**中文说明：**
异常安全性是关于"当操作失败时，程序处于什么状态"。没有异常安全保证的代码可能导致资源泄漏、数据结构损坏、不变量被破坏。C++ 定义了三个级别的异常安全保证，让程序员可以推理异常情况下的程序行为。

---

## 2. Core Idea

### The Three Guarantees

```
+====================================================================+
|  GUARANTEE      |  MEANING                                         |
+====================================================================+
|  NO-THROW       |  Operation NEVER throws                          |
|  (nothrow)      |  - Destructors should always be no-throw         |
|                 |  - swap() should be no-throw                     |
|                 |  - Move operations should be no-throw            |
+--------------------------------------------------------------------+
|  STRONG         |  If exception, state unchanged                   |
|  (commit or     |  - "Transactional" semantics                     |
|  rollback)      |  - Either fully succeeds or fully fails          |
|                 |  - Most expensive to provide                     |
+--------------------------------------------------------------------+
|  BASIC          |  If exception, no leaks and invariants hold      |
|  (valid state)  |  - Object may be in different valid state        |
|                 |  - Resources not leaked                          |
|                 |  - But exact state is unspecified                |
+====================================================================+
```

### Visual Representation

```
                       OPERATION EXECUTION
                              │
                              v
              ┌───────────────────────────────┐
              │       May throw exception      │
              └───────────────────────────────┘
                     /              \
                    /                \
            SUCCESS                  EXCEPTION
               │                         │
               v                         v
    ┌─────────────────┐      ┌─────────────────────┐
    │   New State     │      │  NO-THROW: n/a      │
    │   (expected)    │      │  STRONG: Old State  │
    └─────────────────┘      │  BASIC: Valid State │
                             └─────────────────────┘
```

**中文说明：**
- **No-throw**: 操作永不抛异常，用 `noexcept` 标记
- **Strong**: 事务语义——要么完全成功，要么状态完全不变
- **Basic**: 最低保证——不泄漏资源，对象保持有效状态（但可能改变）

---

## 3. Idiomatic C++ Techniques

### Achieving No-Throw

```cpp
class Widget {
public:
    // Destructor: MUST be no-throw
    ~Widget() noexcept {
        // Never throw here!
        cleanup();  // Must be no-throw
    }
    
    // Swap: should be no-throw
    friend void swap(Widget& a, Widget& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }
    
    // Move operations: should be no-throw
    Widget(Widget&& other) noexcept 
        : data_(other.data_), size_(other.size_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    
    Widget& operator=(Widget&& other) noexcept {
        Widget temp(std::move(other));
        swap(*this, temp);
        return *this;
    }
};
```

### Achieving Strong Guarantee: Copy-and-Swap

```cpp
class Container {
    T* data_;
    size_t size_;
    
public:
    // Strong guarantee via copy-and-swap
    Container& operator=(const Container& other) {
        Container temp(other);  // Copy (may throw)
        swap(*this, temp);      // No-throw swap
        return *this;
        // temp destroyed with old data
    }
    
    // Strong guarantee for modification
    void push_back(const T& value) {
        // Step 1: Prepare new state (may throw)
        size_t newSize = size_ + 1;
        T* newData = new T[newSize];
        
        try {
            std::copy(data_, data_ + size_, newData);
            newData[size_] = value;
        } catch (...) {
            delete[] newData;  // Clean up on failure
            throw;             // Re-throw
        }
        
        // Step 2: Commit (no-throw operations only)
        delete[] data_;
        data_ = newData;
        size_ = newSize;
    }
};
```

### Achieving Basic Guarantee

```cpp
class MinimalSafe {
    std::vector<Widget> widgets_;
    
public:
    // Basic guarantee: no leaks, valid state, but may be partial
    void loadMultiple(const std::vector<Config>& configs) {
        for (const auto& cfg : configs) {
            widgets_.push_back(Widget(cfg));  // May throw
            // If throws after some inserts, we have partial state
            // But: no leaks, widgets_ is valid, just incomplete
        }
    }
};
```

---

## 4. Complete C++ Example

```cpp
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// A class demonstrating all three guarantee levels
// ============================================================

class SafeBuffer {
    std::unique_ptr<int[]> data_;
    size_t size_;
    size_t capacity_;
    
public:
    // Constructor: Basic guarantee
    explicit SafeBuffer(size_t capacity = 0)
        : data_(capacity ? std::make_unique<int[]>(capacity) : nullptr)
        , size_(0)
        , capacity_(capacity)
    {}
    
    // Destructor: No-throw guarantee (implicit with unique_ptr)
    ~SafeBuffer() noexcept = default;
    
    // Move: No-throw guarantee
    SafeBuffer(SafeBuffer&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        other.size_ = 0;
        other.capacity_ = 0;
    }
    
    SafeBuffer& operator=(SafeBuffer&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }
    
    // Copy: Strong guarantee (copy-and-swap)
    SafeBuffer(const SafeBuffer& other)
        : data_(other.capacity_ ? std::make_unique<int[]>(other.capacity_) : nullptr)
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        std::copy(other.data_.get(), other.data_.get() + size_, data_.get());
    }
    
    SafeBuffer& operator=(const SafeBuffer& other) {
        SafeBuffer temp(other);  // May throw
        swap(*this, temp);       // No-throw
        return *this;
    }
    
    // Swap: No-throw guarantee
    friend void swap(SafeBuffer& a, SafeBuffer& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
        swap(a.capacity_, b.capacity_);
    }
    
    // push_back: Strong guarantee
    void push_back(int value) {
        if (size_ >= capacity_) {
            // Prepare new state (may throw)
            size_t newCapacity = capacity_ ? capacity_ * 2 : 8;
            auto newData = std::make_unique<int[]>(newCapacity);
            std::copy(data_.get(), data_.get() + size_, newData.get());
            
            // Commit (no-throw: just pointer/integer assignments)
            data_ = std::move(newData);
            capacity_ = newCapacity;
        }
        // Add element (no-throw: int assignment)
        data_[size_++] = value;
    }
    
    // at(): Strong guarantee (throws on invalid index)
    int& at(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("SafeBuffer::at");
        }
        return data_[index];
    }
    
    // operator[]: No-throw (undefined behavior on invalid index)
    int& operator[](size_t index) noexcept {
        return data_[index];  // No bounds check
    }
    
    // clear(): No-throw guarantee
    void clear() noexcept {
        size_ = 0;
        // Don't deallocate - just reset size
    }
    
    // resize(): Basic guarantee
    void resize(size_t newSize, int value = 0) {
        if (newSize > capacity_) {
            auto newData = std::make_unique<int[]>(newSize);  // May throw
            std::copy(data_.get(), data_.get() + size_, newData.get());
            data_ = std::move(newData);
            capacity_ = newSize;
        }
        // Fill new elements
        for (size_t i = size_; i < newSize; ++i) {
            data_[i] = value;
        }
        size_ = newSize;
    }
    
    size_t size() const noexcept { return size_; }
    size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }
};

// ============================================================
// Transaction pattern for strong guarantee
// ============================================================

class BankAccount {
    std::string name_;
    int balance_;
    std::vector<std::string> history_;
    
public:
    BankAccount(std::string name, int balance)
        : name_(std::move(name)), balance_(balance) {}
    
    // Strong guarantee: Transfer either fully succeeds or fully fails
    static void transfer(BankAccount& from, BankAccount& to, int amount) {
        if (amount <= 0) {
            throw std::invalid_argument("Amount must be positive");
        }
        if (from.balance_ < amount) {
            throw std::runtime_error("Insufficient funds");
        }
        
        // Prepare new state
        std::string fromLog = "Sent " + std::to_string(amount) + " to " + to.name_;
        std::string toLog = "Received " + std::to_string(amount) + " from " + from.name_;
        
        // This might throw (string allocation)
        std::vector<std::string> newFromHistory = from.history_;
        newFromHistory.push_back(fromLog);  // May throw
        
        std::vector<std::string> newToHistory = to.history_;
        newToHistory.push_back(toLog);  // May throw
        
        // Commit phase: all no-throw operations
        from.balance_ -= amount;
        to.balance_ += amount;
        from.history_ = std::move(newFromHistory);  // No-throw (move)
        to.history_ = std::move(newToHistory);      // No-throw (move)
    }
    
    int balance() const noexcept { return balance_; }
    const std::string& name() const noexcept { return name_; }
};

// ============================================================
// RAII guard for exception-safe resource management
// ============================================================

template<typename Func>
class ScopeGuard {
    Func cleanup_;
    bool active_;
    
public:
    explicit ScopeGuard(Func f)
        : cleanup_(std::move(f)), active_(true) {}
    
    ~ScopeGuard() noexcept {
        if (active_) {
            try {
                cleanup_();
            } catch (...) {
                // Swallow exceptions in destructor
            }
        }
    }
    
    void dismiss() noexcept { active_ = false; }
    
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
};

template<typename Func>
ScopeGuard<Func> makeScopeGuard(Func f) {
    return ScopeGuard<Func>(std::move(f));
}

void exampleWithScopeGuard() {
    FILE* file = fopen("test.txt", "w");
    if (!file) return;
    
    auto guard = makeScopeGuard([file]() { fclose(file); });
    
    // Do work that might throw...
    // fputs("data", file);
    
    // If we reach here, we might dismiss the guard
    // guard.dismiss();  // Don't close file, we'll handle it
}

// ============================================================
// Demonstration
// ============================================================

int main() {
    std::cout << "=== SafeBuffer Operations ===\n";
    
    SafeBuffer buf;
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    
    std::cout << "Buffer size: " << buf.size() << "\n";
    
    // Copy with strong guarantee
    SafeBuffer buf2 = buf;
    std::cout << "Copied buffer size: " << buf2.size() << "\n";
    
    // Move with no-throw guarantee
    SafeBuffer buf3 = std::move(buf);
    std::cout << "Moved-from buffer size: " << buf.size() << "\n";
    std::cout << "Moved-to buffer size: " << buf3.size() << "\n";
    
    std::cout << "\n=== Bank Transfer (Strong Guarantee) ===\n";
    
    BankAccount alice("Alice", 1000);
    BankAccount bob("Bob", 500);
    
    std::cout << "Before: Alice=" << alice.balance() 
              << ", Bob=" << bob.balance() << "\n";
    
    try {
        BankAccount::transfer(alice, bob, 300);
        std::cout << "After transfer: Alice=" << alice.balance()
                  << ", Bob=" << bob.balance() << "\n";
        
        // This should fail
        BankAccount::transfer(alice, bob, 2000);
    } catch (const std::exception& e) {
        std::cout << "Transfer failed: " << e.what() << "\n";
        std::cout << "Balances unchanged: Alice=" << alice.balance()
                  << ", Bob=" << bob.balance() << "\n";
    }
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Throwing in destructor

```cpp
class Bad {
public:
    ~Bad() {
        if (needsCleanup_) {
            throw std::runtime_error("Cleanup failed");  // DISASTER
        }
    }
};
// If destructor called during stack unwinding, std::terminate()!
```

### Mistake 2: Non-noexcept move operations

```cpp
class BadMove {
public:
    BadMove(BadMove&& other) {  // NOT noexcept
        data_ = new char[other.size_];  // May throw!
    }
};

std::vector<BadMove> v;
v.push_back(BadMove{});
// Vector uses COPY instead of move for exception safety!
```

### Mistake 3: Partial state modification

```cpp
void badResize(std::vector<Widget>& v, size_t newSize) {
    v.clear();           // Clear old elements
    v.reserve(newSize);  // May throw! Now v is empty
    // ...
}
// If reserve throws, v is empty - data lost!
```

---

## 6. When NOT to Provide Strong Guarantee

### When Basic is Acceptable

| Situation | Reason |
|-----------|--------|
| Performance-critical code | Strong guarantee may require copy |
| Batch operations | Partial completion may be useful |
| Simple types | No complex invariants |
| Internal helpers | Called only with valid input |

### The Cost of Strong Guarantee

```cpp
// STRONG but slow: Makes copy first
void strongPushBack(std::vector<T>& v, const T& value) {
    std::vector<T> temp = v;  // Full copy!
    temp.push_back(value);
    swap(v, temp);
}

// BASIC but fast: Modifies in place
void basicPushBack(std::vector<T>& v, const T& value) {
    v.push_back(value);  // May leave v in larger state on throw
}
```

**中文说明：**
强异常安全保证通常需要"先复制后交换"，这可能很慢。基本保证（不泄漏、保持有效）通常足够，尤其是对于批量操作——部分完成可能比完全回滚更有用。关键是记录你提供的保证级别。

---

## Summary

```
+------------------------------------------------------------------+
|              EXCEPTION SAFETY CHECKLIST                           |
+------------------------------------------------------------------+
|                                                                  |
|  DESTRUCTOR:                                                     |
|    □ Never throws (mark noexcept)                                |
|    □ Handles all cleanup paths                                   |
|                                                                  |
|  MOVE OPERATIONS:                                                |
|    □ Mark noexcept                                               |
|    □ Leave source in valid state                                 |
|                                                                  |
|  SWAP:                                                           |
|    □ Mark noexcept                                               |
|    □ Use member swap or std::swap                                |
|                                                                  |
|  MODIFYING OPERATIONS:                                           |
|    □ Document guarantee level                                    |
|    □ Do all throwing work before modifying state                 |
|    □ Use copy-and-swap for strong guarantee                      |
|                                                                  |
|  RAII:                                                           |
|    □ Acquire in constructor                                      |
|    □ Release in destructor (no-throw)                            |
|    □ Consider scope guards for complex cleanup                   |
|                                                                  |
+------------------------------------------------------------------+
```

