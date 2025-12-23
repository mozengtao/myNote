# Topic 26: API Design with Invariants

## 1. Problem Statement

### What real engineering problem does this solve?

APIs can be misused in ways that corrupt state:

```cpp
// FRAGILE API
class Account {
public:
    double balance;  // Anyone can set to -1000000!
    bool active;     // Can be active with balance < 0?
};

// ROBUST API
class Account {
public:
    bool deposit(double amount);   // Enforces amount > 0
    bool withdraw(double amount);  // Enforces balance >= amount
    double balance() const;        // Read-only access
private:
    double balance_ = 0;           // Always >= 0 (invariant)
    bool active_ = true;
};
```

### Class Invariants

- Properties that are ALWAYS true for valid objects
- Established by constructors
- Maintained by all member functions
- Verified by destructors (in debug)

**中文说明：**
类不变量是对象始终必须满足的条件，如"余额非负"、"指针非空"、"容器 size <= capacity"。好的 API 设计让不变量无法被外部代码破坏——通过封装数据成员、验证输入、提供受控的修改方法。

---

## 2. Core Idea

### Invariant Enforcement

```
CONSTRUCTION              MUTATION                  OBSERVATION
┌──────────────┐         ┌──────────────┐          ┌──────────────┐
│ Constructor  │         │ Mutating     │          │ Const        │
│ establishes  │         │ methods      │          │ methods      │
│ invariant    │ ──────► │ maintain     │ ──────►  │ rely on      │
│              │         │ invariant    │          │ invariant    │
└──────────────┘         └──────────────┘          └──────────────┘

If invariant can be violated, the class is BROKEN.
```

### Levels of Enforcement

```
STRONG ENFORCEMENT:           WEAK ENFORCEMENT:
• Type system prevents        • Runtime checks only
• Impossible to violate       • Assertions in debug
• Zero overhead               • Can still be violated

Example:                      Example:
class NonEmptyString {        class String {
    // Can't construct        public:
    // empty string              void clear() {
};                                  assert(!empty());
                                    data_.clear();
                                 }
                               };
```

**中文说明：**
不变量可以通过类型系统（编译时强制）或运行时检查（assert/exceptions）来维护。最好的设计是让违反不变量的操作无法编译。

---

## 3. Idiomatic C++ Techniques

### Private Data + Public Interface

```cpp
class BankAccount {
public:
    explicit BankAccount(std::string owner, double initial = 0)
        : owner_(std::move(owner))
        , balance_(initial < 0 ? 0 : initial)  // Enforce non-negative
    {}
    
    // Controlled mutation
    bool deposit(double amount) {
        if (amount <= 0) return false;
        balance_ += amount;
        return true;
    }
    
    bool withdraw(double amount) {
        if (amount <= 0 || amount > balance_) return false;
        balance_ -= amount;
        return true;
    }
    
    // Read-only access
    double balance() const { return balance_; }
    const std::string& owner() const { return owner_; }
    
private:
    std::string owner_;
    double balance_;  // INVARIANT: always >= 0
};
```

### Factory Functions for Validation

```cpp
class Email {
public:
    // Factory validates before construction
    static std::optional<Email> create(std::string address) {
        if (!isValid(address)) return std::nullopt;
        return Email(std::move(address));
    }
    
    const std::string& address() const { return address_; }
    
private:
    explicit Email(std::string addr) : address_(std::move(addr)) {}
    
    static bool isValid(const std::string& addr) {
        return addr.find('@') != std::string::npos;
    }
    
    std::string address_;  // INVARIANT: contains '@'
};
```

### Strong Types for Invariants

```cpp
// Type that can never be zero
class NonZero {
    int value_;
    explicit NonZero(int v) : value_(v) {}
    
public:
    static std::optional<NonZero> create(int v) {
        if (v == 0) return std::nullopt;
        return NonZero(v);
    }
    
    int get() const { return value_; }
};

// Division that never divides by zero
int safeDivide(int numerator, NonZero denominator) {
    return numerator / denominator.get();  // Always safe!
}
```

---

## 4. Complete C++ Example

```cpp
#include <cassert>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================
// INVARIANT: Vector-like container with size <= capacity
// ============================================================
template<typename T>
class Buffer {
    T* data_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
    
    // Check invariants (debug only)
    void checkInvariant() const {
        assert(size_ <= capacity_);
        assert((data_ == nullptr) == (capacity_ == 0));
    }
    
public:
    explicit Buffer(size_t cap = 0)
        : data_(cap ? new T[cap] : nullptr)
        , capacity_(cap)
    {
        checkInvariant();
    }
    
    ~Buffer() {
        checkInvariant();
        delete[] data_;
    }
    
    void push_back(const T& value) {
        if (size_ >= capacity_) {
            throw std::length_error("Buffer full");
        }
        data_[size_++] = value;
        checkInvariant();
    }
    
    void pop_back() {
        if (size_ == 0) {
            throw std::underflow_error("Buffer empty");
        }
        --size_;
        checkInvariant();
    }
    
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }
    
    T& operator[](size_t i) {
        assert(i < size_);  // Precondition
        return data_[i];
    }
};

// ============================================================
// INVARIANT: Range with begin <= end
// ============================================================
class DateRange {
    int startDay_;
    int endDay_;
    
    void checkInvariant() const {
        assert(startDay_ <= endDay_);
    }
    
public:
    // Factory ensures valid construction
    static std::optional<DateRange> create(int start, int end) {
        if (start > end) return std::nullopt;
        return DateRange(start, end);
    }
    
    int start() const { return startDay_; }
    int end() const { return endDay_; }
    int duration() const { return endDay_ - startDay_; }
    
    // Mutation maintains invariant
    bool extend(int days) {
        if (days < 0) return false;
        endDay_ += days;
        checkInvariant();
        return true;
    }
    
    bool shorten(int days) {
        if (days < 0 || days > duration()) return false;
        endDay_ -= days;
        checkInvariant();
        return true;
    }
    
private:
    DateRange(int start, int end) : startDay_(start), endDay_(end) {
        checkInvariant();
    }
};

// ============================================================
// INVARIANT: Non-empty collection
// ============================================================
template<typename T>
class NonEmptyVector {
    std::vector<T> data_;
    
    void checkInvariant() const {
        assert(!data_.empty());
    }
    
public:
    // Must be constructed with at least one element
    explicit NonEmptyVector(T first) : data_{std::move(first)} {
        checkInvariant();
    }
    
    NonEmptyVector(std::initializer_list<T> init) : data_(init) {
        if (data_.empty()) {
            throw std::invalid_argument("Cannot be empty");
        }
        checkInvariant();
    }
    
    void push_back(T value) {
        data_.push_back(std::move(value));
        checkInvariant();
    }
    
    // pop_back only if size > 1
    bool pop_back() {
        if (data_.size() <= 1) return false;
        data_.pop_back();
        checkInvariant();
        return true;
    }
    
    const T& front() const { return data_.front(); }
    const T& back() const { return data_.back(); }
    size_t size() const { return data_.size(); }
    
    // Guaranteed to have at least one element
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }
};

// ============================================================
// INVARIANT: State machine with valid transitions
// ============================================================
class Connection {
public:
    enum class State { Disconnected, Connecting, Connected };
    
private:
    State state_ = State::Disconnected;
    std::string address_;
    
public:
    // Only valid transitions allowed
    bool connect(const std::string& addr) {
        if (state_ != State::Disconnected) return false;
        address_ = addr;
        state_ = State::Connecting;
        return true;
    }
    
    bool onConnected() {
        if (state_ != State::Connecting) return false;
        state_ = State::Connected;
        return true;
    }
    
    bool disconnect() {
        state_ = State::Disconnected;
        address_.clear();
        return true;
    }
    
    // Operations only valid in certain states
    bool send(const std::string& data) {
        if (state_ != State::Connected) return false;
        // Send data...
        return true;
    }
    
    State state() const { return state_; }
};

int main() {
    std::cout << "=== Buffer Invariant ===\n";
    Buffer<int> buf(3);
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    // buf.push_back(4);  // Would throw
    std::cout << "Size: " << buf.size() << "/" << buf.capacity() << "\n";
    
    std::cout << "\n=== DateRange Invariant ===\n";
    if (auto range = DateRange::create(10, 20)) {
        std::cout << "Duration: " << range->duration() << "\n";
        range->extend(5);
        std::cout << "Extended: " << range->duration() << "\n";
    }
    
    std::cout << "\n=== NonEmptyVector Invariant ===\n";
    NonEmptyVector<int> vec{1, 2, 3};
    vec.push_back(4);
    std::cout << "Front: " << vec.front() << ", Size: " << vec.size() << "\n";
    
    std::cout << "\n=== State Machine Invariant ===\n";
    Connection conn;
    std::cout << "Send before connect: " << conn.send("data") << "\n";
    conn.connect("server.com");
    conn.onConnected();
    std::cout << "Send after connect: " << conn.send("data") << "\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Public data members

```cpp
// BAD: Anyone can violate invariants
class Circle {
public:
    double radius;  // Can be set to -1!
};

// GOOD: Controlled access
class Circle {
public:
    explicit Circle(double r) : radius_(r > 0 ? r : 0) {}
    void setRadius(double r) { if (r > 0) radius_ = r; }
    double radius() const { return radius_; }
private:
    double radius_;
};
```

### Mistake 2: Returning mutable reference

```cpp
// BAD: Invariant can be bypassed
class Config {
    std::vector<int> values_;
public:
    std::vector<int>& getValues() { return values_; }  // Can modify!
};

// GOOD: Return const reference
const std::vector<int>& getValues() const { return values_; }
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              INVARIANT DESIGN CHECKLIST                           |
+------------------------------------------------------------------+
|                                                                  |
|  ESTABLISH INVARIANTS:                                           |
|    □ Private data members                                        |
|    □ Constructor validates all inputs                            |
|    □ Factory functions for complex validation                    |
|                                                                  |
|  MAINTAIN INVARIANTS:                                            |
|    □ All mutators validate inputs                                |
|    □ No mutable references to internals                          |
|    □ State transitions are explicit and validated                |
|                                                                  |
|  VERIFY INVARIANTS:                                              |
|    □ Assert invariants after mutations (debug builds)            |
|    □ Document invariants in comments                             |
|    □ Unit tests for boundary conditions                          |
|                                                                  |
|  DESIGN PRINCIPLE:                                               |
|    "Make illegal states unrepresentable"                         |
|                                                                  |
+------------------------------------------------------------------+
```

