# Pattern 12: Proxy

## 1. Problem the Pattern Solves

### Design Pressure
- Need to control access to an object
- Object creation is expensive (defer until needed)
- Object is remote (hide network complexity)
- Need to add cross-cutting concerns (logging, caching)

### What Goes Wrong Without It
```cpp
// Without proxy: client handles remote access directly
void fetchData() {
    Socket socket;
    socket.connect(server);
    socket.send(request);
    auto response = socket.receive();
    socket.close();
    // Networking code scattered everywhere
}
```

### Symptoms Indicating Need
- Expensive initialization that may not be needed
- Access control checks scattered in code
- Remote objects accessed directly
- Caching logic duplicated around slow operations

---

## 2. Core Idea (C++-Specific)

**Proxy provides a surrogate or placeholder for another object to control access to it.**

```
+--------+       +----------+       +-------------+
| Client | ----> |  Proxy   | ----> | RealSubject |
+--------+       | (same    |       +-------------+
                 | interface)|
                 +----------+
```

**Proxy types:**
1. **Virtual Proxy**: Lazy initialization
2. **Protection Proxy**: Access control
3. **Remote Proxy**: Handle network access
4. **Smart Proxy**: Reference counting, logging

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Inheritance | Same interface | Substitutable |
| `std::unique_ptr` | Hold real subject | Lazy init |
| `mutable` | Lazy init in const | Cache real subject |
| `operator->` | Smart pointer proxy | Transparent access |
| `std::shared_ptr` | Reference counting proxy | Built-in |

---

## 4. Canonical C++ Implementation

### Virtual Proxy (Lazy Loading)

```cpp
#include <memory>
#include <iostream>

// Subject interface
class Image {
public:
    virtual ~Image() = default;
    virtual void display() = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

// Real subject - expensive to create
class HighResImage : public Image {
public:
    explicit HighResImage(std::string filename) 
        : filename_(std::move(filename)) {
        loadFromDisk();  // Expensive!
    }
    
    void display() override {
        std::cout << "Displaying " << filename_ 
                  << " (" << width_ << "x" << height_ << ")\n";
    }
    
    int width() const override { return width_; }
    int height() const override { return height_; }
    
private:
    void loadFromDisk() {
        std::cout << "Loading " << filename_ << " from disk...\n";
        width_ = 1920;
        height_ = 1080;
    }
    
    std::string filename_;
    int width_, height_;
};

// Virtual proxy - delays loading
class ImageProxy : public Image {
public:
    explicit ImageProxy(std::string filename)
        : filename_(std::move(filename)) {}
    
    void display() override {
        ensureLoaded();
        realImage_->display();
    }
    
    int width() const override {
        ensureLoaded();
        return realImage_->width();
    }
    
    int height() const override {
        ensureLoaded();
        return realImage_->height();
    }
    
private:
    void ensureLoaded() const {
        if (!realImage_) {
            realImage_ = std::make_unique<HighResImage>(filename_);
        }
    }
    
    std::string filename_;
    mutable std::unique_ptr<HighResImage> realImage_;
};

int main() {
    // Create proxy - no loading yet
    ImageProxy img("photo.jpg");
    std::cout << "Proxy created, image not loaded yet\n";
    
    // First access triggers load
    img.display();
    
    // Subsequent accesses use cached object
    std::cout << "Dimensions: " << img.width() << "x" << img.height() << "\n";
    
    return 0;
}
```

### Protection Proxy

```cpp
#include <memory>
#include <stdexcept>

class Document {
public:
    virtual ~Document() = default;
    virtual void read() = 0;
    virtual void write(const std::string& content) = 0;
};

class SecureDocument : public Document {
public:
    SecureDocument(std::unique_ptr<Document> doc, bool canWrite)
        : doc_(std::move(doc)), canWrite_(canWrite) {}
    
    void read() override {
        doc_->read();
    }
    
    void write(const std::string& content) override {
        if (!canWrite_) {
            throw std::runtime_error("Write access denied");
        }
        doc_->write(content);
    }
    
private:
    std::unique_ptr<Document> doc_;
    bool canWrite_;
};
```

### Smart Pointer as Proxy

```cpp
#include <memory>
#include <iostream>

template<typename T>
class LoggingPtr {
public:
    explicit LoggingPtr(std::unique_ptr<T> ptr) 
        : ptr_(std::move(ptr)) {}
    
    T* operator->() {
        std::cout << "Accessing object at " << ptr_.get() << "\n";
        return ptr_.get();
    }
    
    T& operator*() {
        std::cout << "Dereferencing object\n";
        return *ptr_;
    }
    
private:
    std::unique_ptr<T> ptr_;
};

struct MyClass {
    void doSomething() { std::cout << "Doing something\n"; }
};

int main() {
    LoggingPtr<MyClass> ptr(std::make_unique<MyClass>());
    ptr->doSomething();  // Logs access
    return 0;
}
```

---

## 5. Typical Usage in Real Projects

| Domain | Proxy Type | Example |
|--------|-----------|---------|
| ORM | Virtual | Lazy-loaded relations |
| RPC | Remote | gRPC stubs |
| Security | Protection | Access control wrappers |
| Caching | Smart | HTTP cache proxy |

### STL Examples
- `std::shared_ptr` - reference counting proxy
- `std::weak_ptr` - non-owning proxy

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Proxy with Different Interface

```cpp
// BAD: Proxy adds methods not in subject
class BadProxy : public Subject {
    void extraMethod();  // ✗ Breaks substitutability
};
```

### ❌ Mistake 2: Thread-Unsafe Lazy Init

```cpp
// BAD: Race condition on lazy init
void ensureLoaded() {
    if (!real_) {
        real_ = create();  // ✗ Two threads might create
    }
}

// FIX: Use std::call_once or mutex
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Simple delegation | Direct call |
| Just adding behavior | Decorator |
| Interface translation | Adapter |

---

## 8. Pattern Variations & Modern C++

### `std::optional` for Lazy Init

```cpp
#include <optional>

class LazyValue {
public:
    int get() {
        if (!value_) {
            value_ = computeExpensiveValue();
        }
        return *value_;
    }
private:
    mutable std::optional<int> value_;
    int computeExpensiveValue();
};
```

---

## 9. Mental Model Summary

**When Proxy "Clicks":**

Use Proxy when you need to **control access** to an object—whether for lazy loading, access control, or remote access. The proxy maintains the same interface, making it **transparent** to clients.

---

## 中文说明

### 代理模式要点

1. **代理类型**：
   - 虚代理：延迟加载
   - 保护代理：访问控制
   - 远程代理：网络访问
   - 智能代理：引用计数、日志

2. **与其他模式区别**：
   ```
   代理：控制访问，相同接口
   装饰器：添加行为
   适配器：改变接口
   ```

3. **C++ 智能指针即代理**：
   - `shared_ptr` = 引用计数代理
   - `weak_ptr` = 非拥有代理

