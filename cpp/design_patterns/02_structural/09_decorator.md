# Pattern 9: Decorator

## 1. Problem the Pattern Solves

### Design Pressure
- Need to add responsibilities to objects dynamically
- Subclassing for every combination leads to class explosion
- Want to compose behaviors at runtime

### What Goes Wrong Without It
```cpp
// Subclass explosion for combinations:
class Coffee { virtual double cost(); };
class CoffeeWithMilk : public Coffee {};
class CoffeeWithSugar : public Coffee {};
class CoffeeWithMilkAndSugar : public Coffee {};
class CoffeeWithWhip : public Coffee {};
class CoffeeWithMilkAndWhip : public Coffee {};
// Every combination = new class!
```

### Symptoms Indicating Need
- Many small classes adding single features
- Combinations growing exponentially
- Features that can be added/removed dynamically
- Wrapper objects adding behavior before/after delegation

---

## 2. Core Idea (C++-Specific)

**Decorator attaches additional responsibilities to an object dynamically. It provides a flexible alternative to subclassing for extending functionality.**

```
+-------------+       +---------------+
|  Component  |<------| Decorator     |
| +operation()|       | +operation()  |
+-------------+       | -component    |
      ^               +---------------+
      |                      ^
+-------------+        +---------------+
| Concrete    |        | ConcreteDecor |
| Component   |        | +operation()  |
+-------------+        +---------------+
```

Key insight: **Decorator IS-A Component and HAS-A Component**. This allows recursive wrapping.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Inheritance | Decorator extends Component | Same interface |
| Composition | Decorator holds Component | Wrap behavior |
| `std::unique_ptr` | Own wrapped component | Clear ownership |
| `explicit` ctor | Take wrapped object | Prevent implicit wrapping |
| Forwarding | Pass-through methods | Preserve base behavior |

### Decorator Chain

```cpp
auto obj = make_unique<LoggingDecorator>(
             make_unique<CachingDecorator>(
               make_unique<ConcreteComponent>()));
// Onion layers: each wraps the inner one
```

---

## 4. Canonical C++ Implementation

### I/O Stream Example

```cpp
#include <memory>
#include <string>
#include <iostream>
#include <sstream>

// Component interface
class DataSource {
public:
    virtual ~DataSource() = default;
    virtual void write(const std::string& data) = 0;
    virtual std::string read() = 0;
};

// Concrete component
class FileDataSource : public DataSource {
public:
    explicit FileDataSource(std::string filename) 
        : filename_(std::move(filename)) {}
    
    void write(const std::string& data) override {
        buffer_ = data;  // Simulate file write
        std::cout << "Writing to " << filename_ << ": " << data << "\n";
    }
    
    std::string read() override {
        std::cout << "Reading from " << filename_ << "\n";
        return buffer_;  // Simulate file read
    }
    
private:
    std::string filename_;
    std::string buffer_;
};

// Base decorator
class DataSourceDecorator : public DataSource {
public:
    explicit DataSourceDecorator(std::unique_ptr<DataSource> source)
        : wrapped_(std::move(source)) {}
    
    void write(const std::string& data) override {
        wrapped_->write(data);
    }
    
    std::string read() override {
        return wrapped_->read();
    }
    
protected:
    std::unique_ptr<DataSource> wrapped_;
};

// Concrete decorators
class EncryptionDecorator : public DataSourceDecorator {
public:
    using DataSourceDecorator::DataSourceDecorator;
    
    void write(const std::string& data) override {
        std::string encrypted = encrypt(data);
        wrapped_->write(encrypted);
    }
    
    std::string read() override {
        std::string data = wrapped_->read();
        return decrypt(data);
    }
    
private:
    std::string encrypt(const std::string& s) {
        std::string result = s;
        for (char& c : result) c ^= 0x42;  // Simple XOR
        return "[ENC]" + result;
    }
    
    std::string decrypt(const std::string& s) {
        std::string result = s.substr(5);  // Remove [ENC]
        for (char& c : result) c ^= 0x42;
        return result;
    }
};

class CompressionDecorator : public DataSourceDecorator {
public:
    using DataSourceDecorator::DataSourceDecorator;
    
    void write(const std::string& data) override {
        std::string compressed = compress(data);
        wrapped_->write(compressed);
    }
    
    std::string read() override {
        std::string data = wrapped_->read();
        return decompress(data);
    }
    
private:
    std::string compress(const std::string& s) {
        return "[ZIP]" + s;  // Simulate compression
    }
    
    std::string decompress(const std::string& s) {
        return s.substr(5);  // Remove [ZIP]
    }
};

// Usage
int main() {
    // Plain file
    auto source = std::make_unique<FileDataSource>("data.txt");
    source->write("Hello");
    
    std::cout << "\n--- With encryption ---\n";
    auto encrypted = std::make_unique<EncryptionDecorator>(
        std::make_unique<FileDataSource>("data.txt")
    );
    encrypted->write("Hello");
    std::cout << "Read: " << encrypted->read() << "\n";
    
    std::cout << "\n--- With compression + encryption ---\n";
    auto full = std::make_unique<CompressionDecorator>(
        std::make_unique<EncryptionDecorator>(
            std::make_unique<FileDataSource>("data.txt")
        )
    );
    full->write("Hello World");
    std::cout << "Read: " << full->read() << "\n";
    
    return 0;
}
```

### Coffee Example (Classic)

```cpp
#include <memory>
#include <string>
#include <iostream>

class Beverage {
public:
    virtual ~Beverage() = default;
    virtual double cost() const = 0;
    virtual std::string description() const = 0;
};

class Espresso : public Beverage {
public:
    double cost() const override { return 1.99; }
    std::string description() const override { return "Espresso"; }
};

class CondimentDecorator : public Beverage {
public:
    explicit CondimentDecorator(std::unique_ptr<Beverage> bev)
        : beverage_(std::move(bev)) {}
        
protected:
    std::unique_ptr<Beverage> beverage_;
};

class Milk : public CondimentDecorator {
public:
    using CondimentDecorator::CondimentDecorator;
    
    double cost() const override { 
        return beverage_->cost() + 0.50; 
    }
    std::string description() const override { 
        return beverage_->description() + ", Milk"; 
    }
};

class Whip : public CondimentDecorator {
public:
    using CondimentDecorator::CondimentDecorator;
    
    double cost() const override { 
        return beverage_->cost() + 0.70; 
    }
    std::string description() const override { 
        return beverage_->description() + ", Whip"; 
    }
};

int main() {
    auto drink = std::make_unique<Whip>(
                   std::make_unique<Milk>(
                     std::make_unique<Espresso>()));
    
    std::cout << drink->description() << " = $" << drink->cost() << "\n";
    // Output: Espresso, Milk, Whip = $3.19
    
    return 0;
}
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| I/O streams | `std::iostream` with buffering, encoding |
| GUI | Borders, scrollbars on windows |
| Networking | Compression, encryption layers |
| Logging | Log level filtering, formatting |
| Caching | Cache wrapper around service |

### STL Examples
- `std::istream` with `std::filebuf`, `std::stringbuf`
- Algorithm decorators (not in STL, but common pattern)

### Real-World
- **Java I/O**: `BufferedInputStream`, `DataInputStream`
- **Python**: `@decorator` syntax
- **Qt**: `QProxyStyle` for style customization

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Decorator Changes Interface

```cpp
// BAD: Adding new methods breaks uniformity
class CachingDecorator : public DataSource {
    void write(const std::string& data) override;
    void clearCache();  // ✗ Not in DataSource!
};
// Client can't treat decorators uniformly
```

### ❌ Mistake 2: Decorator Order Matters Silently

```cpp
// Encryption then compression vs compression then encryption
// have VERY different behaviors!
auto a = Compress(Encrypt(source));  // Encrypt first, then compress
auto b = Encrypt(Compress(source));  // Compress first, then encrypt
// Document order requirements clearly!
```

### ❌ Mistake 3: Too Many Small Decorators

```cpp
// BAD: 10 decorators each adding tiny behavior
auto obj = D10(D9(D8(D7(D6(D5(D4(D3(D2(D1(base))))))))));
// Hard to read, debug, and trace
// Consider Builder pattern for complex decoration
```

### ❌ Mistake 4: Identity Comparison Fails

```cpp
// Original object identity lost
DataSource* original = new FileSource();
auto decorated = std::make_unique<Encrypted>(original);
// decorated != original, even though same data
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Single additional behavior | Simple subclass |
| Behavior at compile time | Policy-based design |
| Changing core behavior | Strategy pattern |
| Many optional parameters | Builder pattern |

### Alternative: Policy-Based Design

```cpp
template<typename EncryptionPolicy, typename CompressionPolicy>
class DataSource {
    void write(const std::string& data) {
        auto compressed = CompressionPolicy::compress(data);
        auto encrypted = EncryptionPolicy::encrypt(compressed);
        // ... write encrypted
    }
};

// Compile-time composition, no runtime overhead
using SecureSource = DataSource<AESEncryption, ZLibCompression>;
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### Lambda-Based Decorator

```cpp
#include <functional>

using Handler = std::function<std::string(std::string)>;

Handler addLogging(Handler next) {
    return [next](std::string input) {
        std::cout << "Input: " << input << "\n";
        auto output = next(input);
        std::cout << "Output: " << output << "\n";
        return output;
    };
}

Handler addPrefix(std::string prefix, Handler next) {
    return [prefix, next](std::string input) {
        return next(prefix + input);
    };
}

// Compose decorators
Handler pipeline = addLogging(addPrefix(">>", [](std::string s) {
    return s + " processed";
}));

auto result = pipeline("data");
```

### Mixins (Multiple Inheritance)

```cpp
template<typename Base>
class Logging : public Base {
public:
    void write(const std::string& data) {
        std::cout << "Writing: " << data << "\n";
        Base::write(data);
    }
};

template<typename Base>
class Encrypted : public Base {
public:
    void write(const std::string& data) {
        Base::write(encrypt(data));
    }
};

// Compose at compile time
using SecureSource = Logging<Encrypted<FileSource>>;
```

### C++20 Ranges (Pipeline Style)

```cpp
#include <ranges>

auto result = data 
    | std::views::transform(encrypt)
    | std::views::transform(compress);
// Range adaptors are decorators for lazy evaluation
```

---

## 9. Mental Model Summary

**When Decorator "Clicks":**

Use Decorator when you need to **add behavior to individual objects** without affecting others, and when **subclassing would lead to combinatorial explosion**. Each decorator wraps and extends, like layers of an onion. Think: "wrapping", "layers", "middleware", "filters".

**Code Review Recognition:**
- Class implements interface AND holds instance of same interface
- Constructor takes same type it extends
- Methods call wrapped object then add behavior (or vice versa)
- Check: Is order of decorators documented? Are there too many layers?

---

## 中文说明

### 装饰器模式要点

1. **问题场景**：
   - 需要动态添加对象职责
   - 子类组合会导致类爆炸
   - 需要在运行时组合行为

2. **核心结构**：
   ```
   装饰器 IS-A 组件 AND HAS-A 组件
   → 可以递归包装，形成"洋葱"结构
   ```

3. **C++ 实现要点**：
   - 装饰器继承接口 + 持有同类型指针
   - 使用 `unique_ptr` 管理所有权
   - 方法中调用被包装对象再添加行为

4. **典型应用**：
   - I/O 流（缓冲、加密、压缩）
   - GUI 窗口装饰（边框、滚动条）
   - HTTP 中间件
   - 日志过滤和格式化

5. **常见错误**：
   - 装饰器添加新方法（破坏统一接口）
   - 忽略装饰器顺序的重要性
   - 装饰器层数过多

### 与其他模式的区别

```
装饰器：添加职责，保持接口
适配器：改变接口
代理  ：控制访问
策略  ：改变核心算法
```

