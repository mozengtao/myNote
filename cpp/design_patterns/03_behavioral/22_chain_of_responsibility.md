# Pattern 22: Chain of Responsibility

## 1. Problem the Pattern Solves

### Design Pressure
- Multiple objects may handle a request
- Handler isn't known in advance
- Request should be passed along until handled
- Decouple sender from receivers

### What Goes Wrong Without It
```cpp
// Without chain: sender knows all handlers
void handleRequest(Request r) {
    if (auth.canHandle(r)) auth.handle(r);
    else if (logging.canHandle(r)) logging.handle(r);
    else if (cache.canHandle(r)) cache.handle(r);
    // Sender coupled to all handlers
}
```

---

## 2. Core Idea (C++-Specific)

**Chain of Responsibility passes a request along a chain of handlers. Each handler decides either to process the request or pass it to the next handler.**

```
+----------+    +----------+    +----------+
| Handler1 |--->| Handler2 |--->| Handler3 |
|  next_   |    |  next_   |    |  next_   |
+----------+    +----------+    +----------+
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::unique_ptr` | Own next handler | Chain ownership |
| Virtual method | Handle or pass | Polymorphic handling |
| `std::optional` | Optional result | Indicate handling |

---

## 4. Canonical C++ Implementation

```cpp
#include <memory>
#include <string>
#include <iostream>

struct Request {
    std::string type;
    int priority;
};

class Handler {
public:
    virtual ~Handler() = default;
    
    Handler* setNext(std::unique_ptr<Handler> next) {
        next_ = std::move(next);
        return next_.get();
    }
    
    virtual void handle(const Request& req) {
        if (next_) {
            next_->handle(req);
        }
    }
    
protected:
    std::unique_ptr<Handler> next_;
};

class AuthHandler : public Handler {
public:
    void handle(const Request& req) override {
        if (req.type == "auth") {
            std::cout << "AuthHandler: Handling auth request\n";
        } else {
            std::cout << "AuthHandler: Passing to next\n";
            Handler::handle(req);
        }
    }
};

class CacheHandler : public Handler {
public:
    void handle(const Request& req) override {
        if (req.type == "cache") {
            std::cout << "CacheHandler: Handling cache request\n";
        } else {
            std::cout << "CacheHandler: Passing to next\n";
            Handler::handle(req);
        }
    }
};

class DefaultHandler : public Handler {
public:
    void handle(const Request& req) override {
        std::cout << "DefaultHandler: Handling request\n";
    }
};

int main() {
    auto auth = std::make_unique<AuthHandler>();
    auto cache = std::make_unique<CacheHandler>();
    auto def = std::make_unique<DefaultHandler>();
    
    auth->setNext(std::move(cache))->setNext(std::move(def));
    
    auth->handle({"auth", 1});
    auth->handle({"cache", 2});
    auth->handle({"other", 3});
    
    return 0;
}
```

### Middleware Style (Modern)

```cpp
#include <functional>
#include <vector>

using Handler = std::function<void(Request&, std::function<void()>)>;

class Pipeline {
public:
    void use(Handler h) {
        handlers_.push_back(std::move(h));
    }
    
    void handle(Request& req) {
        size_t idx = 0;
        std::function<void()> next;
        next = [&]() {
            if (idx < handlers_.size()) {
                handlers_[idx++](req, next);
            }
        };
        next();
    }
    
private:
    std::vector<Handler> handlers_;
};

int main() {
    Pipeline pipeline;
    
    pipeline.use([](Request& r, auto next) {
        std::cout << "Auth check\n";
        next();
    });
    
    pipeline.use([](Request& r, auto next) {
        std::cout << "Logging\n";
        next();
    });
    
    Request req{"test", 1};
    pipeline.handle(req);
    
    return 0;
}
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Web servers | Middleware (Express.js style) |
| GUI | Event bubbling |
| Logging | Log level handlers |
| Help systems | Context-sensitive help |

---

## 6. Mental Model Summary

**When Chain of Responsibility "Clicks":**

Use Chain of Responsibility when you have **multiple potential handlers** and want to **decouple sender from receivers**. Each handler either handles the request or passes it on.

---

## 中文说明

### 责任链模式要点

1. **核心思想**：
   - 请求沿链传递
   - 每个处理器决定处理或传递

2. **典型应用**：
   - HTTP 中间件
   - 事件冒泡
   - 日志过滤

3. **现代实现**：
   - 函数式中间件管道

