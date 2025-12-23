# Pattern 2: Factory Method

## 1. Problem the Pattern Solves

### Design Pressure
- Object creation logic is complex or varies based on runtime conditions
- Client code should not know about concrete implementation classes
- Need to defer instantiation decisions to subclasses or configuration

### What Goes Wrong Without It
```cpp
// Client code tightly coupled to concrete types
void processDocument(const std::string& type) {
    Document* doc;
    if (type == "pdf") doc = new PdfDocument();      // Hardcoded
    else if (type == "word") doc = new WordDocument(); // Hardcoded
    else if (type == "txt") doc = new TextDocument();  // Hardcoded
    // Adding new type requires modifying this function!
}
```

### Symptoms Indicating Need
- Large `if/else` or `switch` chains for object creation
- `new ConcreteClass()` scattered throughout codebase
- Adding new types requires changing multiple files
- Testing requires creating concrete objects you don't want

---

## 2. Core Idea (C++-Specific)

**Factory Method defines an interface for creating objects but lets subclasses or factory functions decide which class to instantiate.**

```
+-------------------+         +-------------------+
|      Client       |         |    Product (ABC)  |
+-------------------+         +-------------------+
        |                            ^
        | uses                       | creates
        v                            |
+-------------------+         +-------------------+
|  Factory Method   |-------->| ConcreteProduct   |
| create() -> Product         +-------------------+
+-------------------+
```

In C++, factory methods:
1. Return `std::unique_ptr<Base>` (clear ownership)
2. Can be free functions, static methods, or virtual methods
3. Hide concrete types from client code

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `virtual` function | Polymorphic factory | Runtime type selection |
| `std::unique_ptr` | Return type | Clear ownership transfer |
| `std::make_unique` | Object creation | Exception-safe allocation |
| Free functions | `createProduct()` | Simpler than class hierarchy |
| Template factory | `create<T>()` | Compile-time type selection |
| `std::function` | Registered creators | Plugin architectures |

### Ownership Semantics

```cpp
// Factory returns unique_ptr: caller owns the object
std::unique_ptr<Product> create();  // ✓ Clear ownership

// AVOID: raw pointer - who deletes?
Product* create();  // ✗ Ambiguous ownership
```

---

## 4. Canonical C++ Implementation

### Simple Factory Function (Most Common)

```cpp
#include <memory>
#include <string>
#include <stdexcept>

// Abstract product
class Document {
public:
    virtual ~Document() = default;
    virtual void open() = 0;
    virtual void save() = 0;
};

// Concrete products
class PdfDocument : public Document {
public:
    void open() override { /* PDF logic */ }
    void save() override { /* PDF logic */ }
};

class WordDocument : public Document {
public:
    void open() override { /* Word logic */ }
    void save() override { /* Word logic */ }
};

// Factory function - the pattern
std::unique_ptr<Document> createDocument(const std::string& type) {
    if (type == "pdf") {
        return std::make_unique<PdfDocument>();
    } else if (type == "word") {
        return std::make_unique<WordDocument>();
    }
    throw std::invalid_argument("Unknown document type: " + type);
}

// Usage
int main() {
    auto doc = createDocument("pdf");
    doc->open();
    doc->save();
    // doc automatically destroyed
}
```

### Virtual Factory Method (Classic GoF)

```cpp
#include <memory>

class Button {
public:
    virtual ~Button() = default;
    virtual void render() = 0;
};

class WindowsButton : public Button {
public:
    void render() override { /* Windows style */ }
};

class MacButton : public Button {
public:
    void render() override { /* macOS style */ }
};

// Factory with virtual method
class Dialog {
public:
    virtual ~Dialog() = default;
    
    void renderDialog() {
        auto btn = createButton();  // Factory method call
        btn->render();
    }
    
protected:
    // The factory method - subclasses override
    virtual std::unique_ptr<Button> createButton() = 0;
};

class WindowsDialog : public Dialog {
protected:
    std::unique_ptr<Button> createButton() override {
        return std::make_unique<WindowsButton>();
    }
};

class MacDialog : public Dialog {
protected:
    std::unique_ptr<Button> createButton() override {
        return std::make_unique<MacButton>();
    }
};
```

### Registry-Based Factory (Extensible)

```cpp
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

class Shape {
public:
    virtual ~Shape() = default;
    virtual void draw() = 0;
};

class ShapeFactory {
public:
    using Creator = std::function<std::unique_ptr<Shape>()>;
    
    static ShapeFactory& instance() {
        static ShapeFactory factory;
        return factory;
    }
    
    void registerShape(const std::string& name, Creator creator) {
        creators_[name] = std::move(creator);
    }
    
    std::unique_ptr<Shape> create(const std::string& name) {
        auto it = creators_.find(name);
        if (it == creators_.end()) {
            return nullptr;
        }
        return it->second();
    }
    
private:
    std::unordered_map<std::string, Creator> creators_;
};

// Auto-registration pattern
class Circle : public Shape {
public:
    void draw() override { /* circle drawing */ }
    
    static bool registered_;
};

bool Circle::registered_ = []() {
    ShapeFactory::instance().registerShape("circle", 
        []() { return std::make_unique<Circle>(); });
    return true;
}();
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| GUI frameworks | Widget creation (Qt's `QWidget::create`) |
| Serialization | Parser creation based on file format |
| Database | Connection factory for different DB engines |
| Networking | Protocol handler creation |
| Game engines | Entity spawning systems |
| Plugin systems | Dynamic object creation from DLLs |

### STL Examples
- `std::make_unique<T>()` - factory for unique_ptr
- `std::make_shared<T>()` - factory for shared_ptr
- `std::allocator_traits::construct()` - generic construction

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Returning Raw Pointers

```cpp
// BAD: Who owns this object?
Document* createDocument(const std::string& type);

// GOOD: Clear ownership transfer
std::unique_ptr<Document> createDocument(const std::string& type);
```

### ❌ Mistake 2: Over-Engineering Simple Creation

```cpp
// BAD: Factory for a simple value type
class PointFactory {
public:
    static Point create(int x, int y) { return Point{x, y}; }
};

// GOOD: Just use constructor
Point p{10, 20};
```

### ❌ Mistake 3: Virtual Factory Without Need

```cpp
// BAD: Complex hierarchy when simple function works
class DocumentCreator {
    virtual std::unique_ptr<Document> create() = 0;
};
class PdfCreator : public DocumentCreator { /* ... */ };
class WordCreator : public DocumentCreator { /* ... */ };

// GOOD: Simple function suffices
std::unique_ptr<Document> createDocument(const std::string& type);
```

### ❌ Mistake 4: Exposing Concrete Types

```cpp
// BAD: Defeats the purpose
std::unique_ptr<PdfDocument> createPdf();  // Concrete return type

// GOOD: Return abstract type
std::unique_ptr<Document> createDocument(DocType type);
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Single concrete type | Direct constructor |
| Value types | Constructor or aggregate init |
| Types known at compile time | Templates |
| No polymorphism needed | Simple object creation |

### Simple Alternative: Direct Construction

```cpp
// If there's only one type, just construct it
auto config = std::make_unique<JsonConfig>("config.json");
// No factory needed
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### Template Factory (Compile-Time Selection)

```cpp
template<typename T>
std::unique_ptr<Shape> createShape() {
    return std::make_unique<T>();
}

auto circle = createShape<Circle>();
```

### C++17: `std::variant` as Alternative to Polymorphism

```cpp
#include <variant>

using Document = std::variant<PdfDoc, WordDoc, TextDoc>;

Document createDocument(const std::string& type) {
    if (type == "pdf") return PdfDoc{};
    if (type == "word") return WordDoc{};
    return TextDoc{};
}

// No heap allocation, no virtual dispatch
std::visit([](auto& doc) { doc.save(); }, document);
```

### C++20: Concepts for Factory Constraints

```cpp
template<typename T>
concept Creatable = requires {
    { T::create() } -> std::convertible_to<std::unique_ptr<T>>;
};

template<Creatable T>
auto make() { return T::create(); }
```

---

## 9. Mental Model Summary

**When Factory Method "Clicks":**

Use Factory Method when **client code needs objects without knowing their concrete types**, or when **object creation logic is complex enough to warrant encapsulation**. The factory becomes the single point of change when adding new types.

**Code Review Recognition:**
- Functions named `create*()`, `make*()`, `build*()`
- Return type is `unique_ptr<AbstractBase>`
- Client code never uses `new` directly for polymorphic types
- Check: Is this factory adding value, or just indirection?

---

## 中文说明

### 工厂方法模式要点

1. **问题场景**：
   - 客户端代码不应知道具体实现类
   - 对象创建逻辑复杂或需要运行时决定
   - 添加新类型不应修改现有代码

2. **C++ 实现关键**：
   - 返回 `std::unique_ptr<Base>`（明确所有权）
   - 可以是自由函数、静态方法或虚方法
   - 使用 `std::make_unique` 创建对象

3. **三种形式**：
   ```
   简单工厂函数    →  最常用，适合大多数场景
   虚工厂方法      →  需要子类定制创建逻辑
   注册表工厂      →  支持运行时扩展和插件
   ```

4. **常见错误**：
   - 返回裸指针（所有权不明确）
   - 对简单类型过度设计
   - 暴露具体类型给客户端

### 决策流程

```
需要创建多态对象？
    ├── 否 → 直接使用构造函数
    └── 是 → 创建逻辑复杂吗？
              ├── 否 → 简单工厂函数
              └── 是 → 需要运行时扩展？
                        ├── 否 → 虚工厂方法
                        └── 是 → 注册表工厂
```

