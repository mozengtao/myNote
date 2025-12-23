# Pattern 4: Builder

## 1. Problem the Pattern Solves

### Design Pressure
- Object construction requires many parameters
- Construction involves multiple steps with optional parts
- Same construction process should create different representations
- Need to prevent objects in partially-initialized state

### What Goes Wrong Without It
```cpp
// Constructor with too many parameters - unreadable
Pizza pizza("large", true, false, true, true, false, true, "tomato", "mozzarella");
//           size   pepperoni olives mushrooms onions bacon ham   sauce   cheese
// What do all these booleans mean?!
```

### Symptoms Indicating Need
- Constructors with 5+ parameters (especially booleans)
- Multiple constructor overloads for different configurations
- Complex objects with many optional parts
- Object construction requires validation across multiple fields

---

## 2. Core Idea (C++-Specific)

**Builder separates the construction of a complex object from its representation, allowing the same construction process to create different representations.**

```
+--------+     +------------+     +---------+
| Client | --> |  Director  | --> | Builder |
+--------+     +------------+     +---------+
                                       |
                                       v
                               +--------------+
                               |   Product    |
                               | (built step  |
                               |   by step)   |
                               +--------------+
```

In C++, Builder is often implemented with:
1. **Fluent interface** (method chaining)
2. **Move semantics** for final product extraction
3. **Optional Director** class for complex construction sequences

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `return *this` | Method chaining | Fluent interface |
| `&&` (rvalue ref) | `build() &&` | Consume builder, transfer ownership |
| `std::optional` | Optional fields | Handle unset values |
| `std::move` | Product extraction | Efficient ownership transfer |
| `private` constructor | Control creation | Force use of builder |
| `friend class` | Builder access | Allow builder to set private fields |

---

## 4. Canonical C++ Implementation

### Fluent Builder (Most Common in Modern C++)

```cpp
#include <string>
#include <optional>
#include <vector>
#include <stdexcept>
#include <iostream>

class Pizza {
public:
    // Only accessible via builder
    friend class PizzaBuilder;
    
    void describe() const {
        std::cout << "Pizza: " << size_ << ", sauce: " << sauce_;
        if (cheese_) std::cout << ", cheese: " << *cheese_;
        std::cout << ", toppings:";
        for (const auto& t : toppings_) std::cout << " " << t;
        std::cout << "\n";
    }
    
private:
    Pizza() = default;  // Private: force use of builder
    
    std::string size_;
    std::string sauce_;
    std::optional<std::string> cheese_;
    std::vector<std::string> toppings_;
};

class PizzaBuilder {
public:
    PizzaBuilder& size(std::string s) {
        pizza_.size_ = std::move(s);
        return *this;
    }
    
    PizzaBuilder& sauce(std::string s) {
        pizza_.sauce_ = std::move(s);
        return *this;
    }
    
    PizzaBuilder& cheese(std::string c) {
        pizza_.cheese_ = std::move(c);
        return *this;
    }
    
    PizzaBuilder& addTopping(std::string t) {
        pizza_.toppings_.push_back(std::move(t));
        return *this;
    }
    
    // Consume builder and return product
    Pizza build() && {
        if (pizza_.size_.empty()) {
            throw std::logic_error("Pizza size is required");
        }
        if (pizza_.sauce_.empty()) {
            throw std::logic_error("Pizza sauce is required");
        }
        return std::move(pizza_);
    }
    
private:
    Pizza pizza_;
};

// Usage
int main() {
    Pizza pizza = PizzaBuilder()
        .size("large")
        .sauce("tomato")
        .cheese("mozzarella")
        .addTopping("pepperoni")
        .addTopping("mushrooms")
        .build();
    
    pizza.describe();
    return 0;
}
```

### Separate Builder Types (Immutable Product)

```cpp
#include <string>
#include <memory>
#include <vector>

class HttpRequest {
public:
    const std::string& method() const { return method_; }
    const std::string& url() const { return url_; }
    const std::vector<std::pair<std::string, std::string>>& headers() const { 
        return headers_; 
    }
    
    class Builder;
    
private:
    HttpRequest(std::string method, std::string url,
                std::vector<std::pair<std::string, std::string>> headers)
        : method_(std::move(method))
        , url_(std::move(url))
        , headers_(std::move(headers)) {}
    
    std::string method_;
    std::string url_;
    std::vector<std::pair<std::string, std::string>> headers_;
};

class HttpRequest::Builder {
public:
    Builder& method(std::string m) {
        method_ = std::move(m);
        return *this;
    }
    
    Builder& url(std::string u) {
        url_ = std::move(u);
        return *this;
    }
    
    Builder& header(std::string key, std::string value) {
        headers_.emplace_back(std::move(key), std::move(value));
        return *this;
    }
    
    HttpRequest build() && {
        return HttpRequest(std::move(method_), std::move(url_), 
                          std::move(headers_));
    }
    
private:
    std::string method_ = "GET";
    std::string url_;
    std::vector<std::pair<std::string, std::string>> headers_;
};

// Usage
int main() {
    HttpRequest req = HttpRequest::Builder()
        .method("POST")
        .url("https://api.example.com/data")
        .header("Content-Type", "application/json")
        .header("Authorization", "Bearer token123")
        .build();
    return 0;
}
```

### Step Builder (Enforced Order)

```cpp
#include <string>

// Forward declarations
class EmailWithRecipient;
class EmailWithSubject;
class Email;

class EmailBuilder {
public:
    EmailWithRecipient to(std::string recipient);
private:
    std::string from_;
    std::string to_;
    std::string subject_;
    std::string body_;
    friend class EmailWithRecipient;
    friend class EmailWithSubject;
};

class EmailWithRecipient {
public:
    EmailWithSubject subject(std::string s);
private:
    EmailWithRecipient(EmailBuilder b) : builder_(std::move(b)) {}
    EmailBuilder builder_;
    friend class EmailBuilder;
    friend class EmailWithSubject;
};

class EmailWithSubject {
public:
    EmailWithSubject& body(std::string b) {
        builder_.body_ = std::move(b);
        return *this;
    }
    
    Email build() &&;
    
private:
    EmailWithSubject(EmailBuilder b) : builder_(std::move(b)) {}
    EmailBuilder builder_;
    friend class EmailWithRecipient;
};

// Usage forces correct order:
// EmailBuilder().to("...").subject("...").body("...").build();
// Cannot call subject() before to(), etc.
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| HTTP clients | Request builders (like libcurl) |
| SQL queries | Query builders (parameterized queries) |
| Protobuf/Thrift | Message builders |
| GUI | Complex widget configuration |
| Testing | Test fixture builders |
| Configuration | Application configuration objects |

### Real-World Examples
- **gRPC**: `RequestBuilder` for constructing RPC requests
- **Google Test**: `EXPECT_THAT` matchers use builder-like composition
- **Qt**: `QProcessBuilder` (hypothetical, but common pattern)

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Builder for Simple Objects

```cpp
// BAD: Overkill for 2-3 parameters
PointBuilder().x(10).y(20).build();

// GOOD: Just use constructor
Point{10, 20};
```

### ❌ Mistake 2: Not Validating in build()

```cpp
// BAD: Returns invalid object
Pizza build() {
    return pizza_;  // No validation!
}

// GOOD: Validate required fields
Pizza build() && {
    if (pizza_.size_.empty()) {
        throw std::logic_error("Size required");
    }
    return std::move(pizza_);
}
```

### ❌ Mistake 3: Allowing Reuse After build()

```cpp
// BAD: Builder reused after build
auto builder = PizzaBuilder().size("large");
auto pizza1 = builder.build();  // OK
auto pizza2 = builder.build();  // Bug: pizza1's state

// GOOD: Use && to consume builder
Pizza build() && { /* ... */ }  // Forces std::move(builder).build()
```

### ❌ Mistake 4: Excessive Method Chaining

```cpp
// BAD: 20+ chained calls become unreadable
auto obj = Builder()
    .setA(1).setB(2).setC(3).setD(4).setE(5)
    .setF(6).setG(7).setH(8).setI(9).setJ(10)
    // ... impossible to read
    .build();

// GOOD: Consider grouping or sub-builders
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Few parameters | Constructor with named parameters (C++20) |
| All parameters required | Regular constructor |
| Immutable value types | Aggregate initialization |
| Simple configuration | Struct with default values |

### C++20 Designated Initializers (Alternative)

```cpp
struct Config {
    std::string host = "localhost";
    int port = 8080;
    bool tls = false;
    int timeout_ms = 5000;
};

// No builder needed!
Config cfg{
    .host = "api.example.com",
    .port = 443,
    .tls = true
};
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### Named Parameter Idiom (Simplified Builder)

```cpp
class Request {
public:
    Request& method(std::string m) { method_ = std::move(m); return *this; }
    Request& url(std::string u) { url_ = std::move(u); return *this; }
    Request& timeout(int ms) { timeout_ = ms; return *this; }
    
    void send() { /* use configured values */ }
    
private:
    std::string method_ = "GET";
    std::string url_;
    int timeout_ = 30000;
};

// Object IS its own builder
Request().method("POST").url("/api/data").timeout(5000).send();
```

### `std::optional` for Optional Parameters

```cpp
struct ServerConfig {
    std::string host;
    int port;
    std::optional<std::string> ssl_cert;
    std::optional<int> max_connections;
};

ServerConfig cfg{
    .host = "localhost",
    .port = 8080,
    .ssl_cert = "cert.pem"
    // max_connections left as nullopt
};
```

### Expression Templates (Advanced)

```cpp
// SQL-like query builder using expression templates
auto query = select(users.name, users.email)
            .from(users)
            .where(users.age > 18)
            .order_by(users.name);
// Builds SQL string at compile time or runtime
```

---

## 9. Mental Model Summary

**When Builder "Clicks":**

Use Builder when object construction involves **many parameters** (especially optional ones), **multiple steps**, or when you want to **prevent incomplete objects**. Builder makes construction code readable and self-documenting. Think: "readable object configuration", "enforce construction order", "validate before creation".

**Code Review Recognition:**
- Method chaining with `return *this`
- Separate `Builder` class for complex objects
- `build()` method returns the final product
- `build() &&` prevents builder reuse

---

## 中文说明

### 建造者模式要点

1. **问题场景**：
   - 构造函数参数过多（超过 5 个）
   - 多个布尔参数难以区分
   - 对象有可选部分
   - 需要分步构建

2. **C++ 实现方式**：
   ```cpp
   // 流式接口
   Object obj = Builder()
       .setA(...)
       .setB(...)
       .build();
   ```

3. **关键技术**：
   - `return *this` 实现方法链
   - `build() &&` 消费 builder 防止重用
   - `std::optional` 处理可选字段
   - `friend class` 访问产品私有成员

4. **何时不使用**：
   - 参数少（直接用构造函数）
   - C++20 可用指定初始化器
   - 简单配置用结构体默认值

### 与工厂模式的区别

```
工厂方法：关注"创建什么类型"
建造者：关注"如何一步步构建"

工厂：隐藏具体类
建造者：隐藏构建细节
```

