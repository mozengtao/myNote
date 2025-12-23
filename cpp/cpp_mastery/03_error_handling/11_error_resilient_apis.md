# Topic 11: Designing Error-Resilient APIs

## 1. Problem Statement

### What real engineering problem does this solve?

APIs can be misused, and bugs are expensive:

```
FRAGILE API                          RESILIENT API
┌─────────────────────┐              ┌─────────────────────┐
│ int* buffer = nullptr│             │ span<int> buffer;   │
│ size_t size = ???    │             │ // size embedded!   │
│ process(buffer, size)│             │ process(buffer);    │
└─────────────────────┘              └─────────────────────┘
        │                                    │
        v                                    v
   Buffer overrun!                   Bounds checked, safe
```

### What goes wrong with poor API design?

```cpp
// Easy to misuse
void drawRect(int x1, int y1, int x2, int y2);  // Which is which?
drawRect(100, 200, 50, 150);  // Wrong order? Who knows!

// Null pointer surprise
Widget* getWidget(int id);  // Can return null - easy to forget check

// State-dependent
File f;
f.read(buffer);  // Crash! Forgot to call f.open() first

// Error codes ignored
int result = dangerousOp();  // What if it failed?
```

**中文说明：**
脆弱的 API 容易被误用：参数顺序混淆、空指针返回被忽略、状态依赖未检查、错误码被丢弃。健壮的 API 通过类型系统、不变量和设计模式使错误使用无法编译或立即失败。"让正确的用法简单，错误的用法困难或不可能。"

---

## 2. Core Idea

### Make Wrong Code Hard to Write

```
PRINCIPLE                           TECHNIQUE
────────────────────────────────────────────────────────────
Wrong states unrepresentable        Strong types, enums
Wrong order impossible              Builder pattern, fluent API
Null impossible                     References, optional
Resources auto-managed              RAII
Errors must be handled              [[nodiscard]], expected
Invalid input rejected              Validate at boundary
```

### API Design Hierarchy

```
BEST:  Impossible to misuse (type system prevents it)
       ↓
GOOD:  Misuse detected at compile time (static_assert, concepts)
       ↓
OK:    Misuse detected at runtime immediately (precondition check)
       ↓
BAD:   Misuse causes undefined behavior or silent corruption
```

**中文说明：**
健壮 API 的层级：
1. **最佳**：通过类型系统使错误用法无法编译
2. **良好**：编译时静态检查捕获错误
3. **可接受**：运行时立即检测并报告错误
4. **糟糕**：静默失败或未定义行为

---

## 3. Idiomatic C++ Techniques

### Strong Types

```cpp
// BAD: Easy to confuse
void transfer(int from, int to, int amount);
transfer(100, 200, 50);  // Which is account, which is amount?

// GOOD: Strong types prevent confusion
struct AccountId { int value; };
struct Amount { int cents; };

void transfer(AccountId from, AccountId to, Amount amount);
transfer(AccountId{100}, AccountId{200}, Amount{5000});  // Clear!

// Even better: Named parameters via builder
class TransferBuilder {
public:
    TransferBuilder& from(AccountId id);
    TransferBuilder& to(AccountId id);
    TransferBuilder& amount(Amount a);
    void execute();
};

TransferBuilder()
    .from(AccountId{100})
    .to(AccountId{200})
    .amount(Amount{5000})
    .execute();
```

### [[nodiscard]] for Must-Check Returns

```cpp
[[nodiscard]] std::optional<int> parse(const std::string& s);

parse("123");  // WARNING: ignoring return value!

// Must handle result
if (auto val = parse("123")) {
    use(*val);
}
```

### Validated Construction

```cpp
class Email {
    std::string address_;
    
    // Private constructor
    Email(std::string addr) : address_(std::move(addr)) {}
    
public:
    // Factory validates input
    static std::optional<Email> create(std::string addr) {
        if (!isValidEmail(addr)) return std::nullopt;
        return Email(std::move(addr));
    }
    
    const std::string& get() const { return address_; }
};

// Cannot create invalid Email
// Email e("not-an-email");  // Won't compile - constructor private
auto email = Email::create("user@example.com");  // Must check optional
```

---

## 4. Complete C++ Example

```cpp
#include <cassert>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <variant>

// ============================================================
// Strong Types
// ============================================================
template<typename Tag, typename T = int>
class StrongType {
    T value_;
public:
    explicit StrongType(T v) : value_(v) {}
    T get() const { return value_; }
    
    bool operator==(const StrongType& other) const { 
        return value_ == other.value_; 
    }
};

struct UserIdTag {};
struct ProductIdTag {};
struct QuantityTag {};

using UserId = StrongType<UserIdTag>;
using ProductId = StrongType<ProductIdTag>;
using Quantity = StrongType<QuantityTag, unsigned>;

// ============================================================
// Non-Null Pointer Type
// ============================================================
template<typename T>
class NonNull {
    T* ptr_;
public:
    NonNull(T& ref) : ptr_(&ref) {}  // Only from reference
    NonNull(T* ptr) : ptr_(ptr) {
        if (!ptr) throw std::invalid_argument("null pointer");
    }
    
    // Cannot construct from nullptr
    NonNull(std::nullptr_t) = delete;
    
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    T* get() const { return ptr_; }
};

// ============================================================
// Validated Types
// ============================================================
class PositiveInt {
    int value_;
    
    explicit PositiveInt(int v) : value_(v) {}
    
public:
    [[nodiscard]]
    static std::optional<PositiveInt> create(int v) {
        if (v <= 0) return std::nullopt;
        return PositiveInt(v);
    }
    
    int get() const { return value_; }
};

class Percentage {
    double value_;  // 0.0 to 100.0
    
    explicit Percentage(double v) : value_(v) {}
    
public:
    [[nodiscard]]
    static std::optional<Percentage> create(double v) {
        if (v < 0.0 || v > 100.0) return std::nullopt;
        return Percentage(v);
    }
    
    double get() const { return value_; }
    double asFraction() const { return value_ / 100.0; }
};

// ============================================================
// State Machine with Type Safety
// ============================================================
// Connection states as types
class Disconnected {};
class Connecting {};
class Connected {
    int socket_;
public:
    explicit Connected(int sock) : socket_(sock) {}
    int socket() const { return socket_; }
};

class Connection {
    std::variant<Disconnected, Connecting, Connected> state_;
    
public:
    Connection() : state_(Disconnected{}) {}
    
    // Can only connect when disconnected
    void connect(const std::string& host) {
        if (!std::holds_alternative<Disconnected>(state_)) {
            throw std::logic_error("Already connected or connecting");
        }
        state_ = Connecting{};
        // Simulate connection...
        state_ = Connected{42};
    }
    
    // Can only send when connected
    void send(std::span<const char> data) {
        auto* conn = std::get_if<Connected>(&state_);
        if (!conn) {
            throw std::logic_error("Not connected");
        }
        // Use conn->socket()...
        std::cout << "Sent " << data.size() << " bytes\n";
    }
    
    void disconnect() {
        state_ = Disconnected{};
    }
};

// ============================================================
// Builder Pattern for Complex Construction
// ============================================================
class HttpRequest {
    std::string method_;
    std::string url_;
    std::string body_;
    int timeout_;
    
    HttpRequest() = default;
    friend class HttpRequestBuilder;
    
public:
    void execute() {
        std::cout << method_ << " " << url_ << "\n";
        if (!body_.empty()) std::cout << "Body: " << body_ << "\n";
    }
};

class HttpRequestBuilder {
    HttpRequest req_;
    bool methodSet_ = false;
    bool urlSet_ = false;
    
public:
    HttpRequestBuilder& method(std::string m) {
        req_.method_ = std::move(m);
        methodSet_ = true;
        return *this;
    }
    
    HttpRequestBuilder& url(std::string u) {
        req_.url_ = std::move(u);
        urlSet_ = true;
        return *this;
    }
    
    HttpRequestBuilder& body(std::string b) {
        req_.body_ = std::move(b);
        return *this;
    }
    
    HttpRequestBuilder& timeout(int ms) {
        if (ms <= 0) throw std::invalid_argument("timeout must be positive");
        req_.timeout_ = ms;
        return *this;
    }
    
    [[nodiscard]]
    HttpRequest build() {
        if (!methodSet_) throw std::logic_error("method required");
        if (!urlSet_) throw std::logic_error("url required");
        return std::move(req_);
    }
};

// ============================================================
// Span for Bounds-Safe Arrays
// ============================================================
// BAD: Raw pointer + size
void processBufferBad(const int* data, size_t size);

// GOOD: Span bundles pointer and size
void processBuffer(std::span<const int> data) {
    for (int x : data) {  // Range-for works
        std::cout << x << " ";
    }
    std::cout << "\n";
}

// ============================================================
// RAII Guard for Operations
// ============================================================
class Transaction {
    bool committed_ = false;
    
public:
    void execute(const std::string& sql) {
        if (committed_) {
            throw std::logic_error("Transaction already committed");
        }
        std::cout << "Execute: " << sql << "\n";
    }
    
    void commit() {
        committed_ = true;
        std::cout << "Committed\n";
    }
    
    ~Transaction() {
        if (!committed_) {
            std::cout << "Rolled back\n";
        }
    }
};

int main() {
    std::cout << "=== Strong Types ===\n";
    UserId user{123};
    ProductId product{456};
    // transfer(user, product);  // Type error - good!
    
    std::cout << "=== Validated Types ===\n";
    if (auto pct = Percentage::create(75.0)) {
        std::cout << "Valid: " << pct->get() << "%\n";
    }
    if (auto pct = Percentage::create(150.0)) {
        std::cout << "Should not see this\n";
    } else {
        std::cout << "Invalid percentage rejected\n";
    }
    
    std::cout << "\n=== Builder Pattern ===\n";
    auto request = HttpRequestBuilder()
        .method("POST")
        .url("https://api.example.com/data")
        .body("{\"key\": \"value\"}")
        .timeout(5000)
        .build();
    request.execute();
    
    std::cout << "\n=== Span for Safety ===\n";
    int arr[] = {1, 2, 3, 4, 5};
    processBuffer(arr);  // Array decays to span safely
    
    std::cout << "\n=== RAII Transaction ===\n";
    {
        Transaction txn;
        txn.execute("INSERT ...");
        txn.execute("UPDATE ...");
        // Oops, forgot commit!
    }  // Auto rollback
    
    {
        Transaction txn;
        txn.execute("INSERT ...");
        txn.commit();
    }  // Committed
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Too many parameters

```cpp
// BAD: 8 bool parameters
void configure(bool a, bool b, bool c, bool d, 
               bool e, bool f, bool g, bool h);
configure(true, false, true, true, false, true, false, true);  // ???

// GOOD: Flags or builder
enum class ConfigFlags {
    None = 0,
    EnableLogging = 1 << 0,
    EnableCaching = 1 << 1,
    // ...
};
void configure(ConfigFlags flags);
```

### Mistake 2: Out parameters

```cpp
// BAD: Out parameters easy to forget
bool parse(const std::string& s, int& result);

int x;
parse("123", x);  // Did I check return value?

// GOOD: Return optional
std::optional<int> parse(const std::string& s);
```

### Mistake 3: Implicit conversions

```cpp
// BAD: Implicit conversion allows nonsense
class Milliseconds {
public:
    Milliseconds(int ms) {}  // Implicit!
};

void sleep(Milliseconds ms);
sleep(42);  // 42 what? Seconds? Milliseconds?

// GOOD: Explicit constructor
explicit Milliseconds(int ms) {}
sleep(Milliseconds{42});  // Clear
```

---

## 6. Summary

```
+------------------------------------------------------------------+
|              ERROR-RESILIENT API CHECKLIST                        |
+------------------------------------------------------------------+
|                                                                  |
|  TYPE SAFETY:                                                    |
|    □ Use strong types for domain concepts                        |
|    □ Use explicit constructors                                   |
|    □ Use span<T> instead of (T*, size)                           |
|    □ Use optional for nullable                                   |
|                                                                  |
|  CONSTRUCTION:                                                   |
|    □ Validate in factory functions                               |
|    □ Use builder for complex objects                             |
|    □ Make invalid states unrepresentable                         |
|                                                                  |
|  RETURN VALUES:                                                  |
|    □ Use [[nodiscard]] for must-check returns                    |
|    □ Return optional/expected for fallible operations            |
|    □ Avoid out parameters                                        |
|                                                                  |
|  RESOURCE MANAGEMENT:                                            |
|    □ Use RAII for all resources                                  |
|    □ Return unique_ptr for ownership transfer                    |
|    □ Use references for non-nullable parameters                  |
|                                                                  |
+------------------------------------------------------------------+
```

