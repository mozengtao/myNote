# Pattern 3: Abstract Factory

## 1. Problem the Pattern Solves

### Design Pressure
- Need to create **families of related objects** that must work together
- System must be independent of how objects are created and composed
- Must ensure objects from one family aren't mixed with another

### What Goes Wrong Without It
```cpp
// Without abstract factory: mixed families cause bugs
WindowsButton* btn = new WindowsButton();
MacScrollbar* scroll = new MacScrollbar();  // WRONG FAMILY!
// Runtime crashes or visual inconsistencies
```

### Symptoms Indicating Need
- Multiple `switch/if` chains selecting related object types
- Objects must be used in consistent "sets" (UI themes, database drivers)
- Platform-specific code scattered throughout codebase
- Test fixtures need to swap entire object families

---

## 2. Core Idea (C++-Specific)

**Abstract Factory provides an interface for creating families of related objects without specifying their concrete classes.**

```
+------------------+       +------------------+       +------------------+
|   Client Code    |       | AbstractFactory  |       | AbstractProduct  |
+------------------+       +------------------+       +------------------+
        |                         ^                         ^
        | uses                    |                         |
        v                         |                         |
+------------------+       +------+------+           +------+------+
| ConcreteFactory  |       | WinFactory  |           | WinButton   |
| createButton()   |------>| MacFactory  |           | MacButton   |
| createScrollbar()|       +-------------+           +-------------+
+------------------+
```

Key insight: **The factory itself is polymorphic**, returning consistent product families.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Pure virtual methods | Factory interface | Define creation contract |
| `std::unique_ptr` | Return type | Ownership transfer |
| Abstract base classes | Product families | Common interfaces |
| `override` | Factory implementations | Compiler-checked override |
| Templates | Alternative to virtual | Compile-time family selection |

### Ownership Model

```
AbstractFactory (owned by client)
    │
    └── creates → unique_ptr<ProductA>  (client takes ownership)
    └── creates → unique_ptr<ProductB>  (client takes ownership)
```

---

## 4. Canonical C++ Implementation

### Classic Implementation

```cpp
#include <memory>
#include <iostream>

// ============ Abstract Products ============
class Button {
public:
    virtual ~Button() = default;
    virtual void render() = 0;
};

class Checkbox {
public:
    virtual ~Checkbox() = default;
    virtual void render() = 0;
};

// ============ Concrete Products: Windows Family ============
class WindowsButton : public Button {
public:
    void render() override {
        std::cout << "Rendering Windows button\n";
    }
};

class WindowsCheckbox : public Checkbox {
public:
    void render() override {
        std::cout << "Rendering Windows checkbox\n";
    }
};

// ============ Concrete Products: macOS Family ============
class MacButton : public Button {
public:
    void render() override {
        std::cout << "Rendering macOS button\n";
    }
};

class MacCheckbox : public Checkbox {
public:
    void render() override {
        std::cout << "Rendering macOS checkbox\n";
    }
};

// ============ Abstract Factory ============
class GUIFactory {
public:
    virtual ~GUIFactory() = default;
    virtual std::unique_ptr<Button> createButton() = 0;
    virtual std::unique_ptr<Checkbox> createCheckbox() = 0;
};

// ============ Concrete Factories ============
class WindowsFactory : public GUIFactory {
public:
    std::unique_ptr<Button> createButton() override {
        return std::make_unique<WindowsButton>();
    }
    std::unique_ptr<Checkbox> createCheckbox() override {
        return std::make_unique<WindowsCheckbox>();
    }
};

class MacFactory : public GUIFactory {
public:
    std::unique_ptr<Button> createButton() override {
        return std::make_unique<MacButton>();
    }
    std::unique_ptr<Checkbox> createCheckbox() override {
        return std::make_unique<MacCheckbox>();
    }
};

// ============ Client Code ============
class Application {
public:
    explicit Application(std::unique_ptr<GUIFactory> factory)
        : factory_(std::move(factory)) {}
    
    void createUI() {
        button_ = factory_->createButton();
        checkbox_ = factory_->createCheckbox();
    }
    
    void render() {
        button_->render();
        checkbox_->render();
    }
    
private:
    std::unique_ptr<GUIFactory> factory_;
    std::unique_ptr<Button> button_;
    std::unique_ptr<Checkbox> checkbox_;
};

// ============ Factory Selection ============
std::unique_ptr<GUIFactory> createFactory(const std::string& os) {
    if (os == "windows") return std::make_unique<WindowsFactory>();
    if (os == "macos") return std::make_unique<MacFactory>();
    throw std::runtime_error("Unknown OS: " + os);
}

int main() {
    auto factory = createFactory("macos");
    Application app(std::move(factory));
    app.createUI();
    app.render();
    return 0;
}
```

### Template-Based Alternative (Compile-Time)

```cpp
#include <memory>
#include <type_traits>

// Products
struct WindowsButton { void render() { /* ... */ } };
struct WindowsCheckbox { void render() { /* ... */ } };
struct MacButton { void render() { /* ... */ } };
struct MacCheckbox { void render() { /* ... */ } };

// Factory as template parameter
template<typename Factory>
class Application {
public:
    void createUI() {
        button_ = Factory::createButton();
        checkbox_ = Factory::createCheckbox();
    }
    
    void render() {
        button_.render();
        checkbox_.render();
    }
    
private:
    typename Factory::ButtonType button_;
    typename Factory::CheckboxType checkbox_;
};

// Concrete factories
struct WindowsFactory {
    using ButtonType = WindowsButton;
    using CheckboxType = WindowsCheckbox;
    
    static ButtonType createButton() { return {}; }
    static CheckboxType createCheckbox() { return {}; }
};

struct MacFactory {
    using ButtonType = MacButton;
    using CheckboxType = MacCheckbox;
    
    static ButtonType createButton() { return {}; }
    static CheckboxType createCheckbox() { return {}; }
};

// Usage
int main() {
    Application<MacFactory> app;
    app.createUI();
    app.render();
}
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| GUI toolkits | Qt, wxWidgets platform abstraction |
| Database access | ADO.NET provider factories |
| Game engines | Rendering backend (OpenGL/Vulkan/DirectX) |
| Testing | Mock object families |
| Cross-platform | File system, networking abstractions |

### Real-World Example: Database Drivers

```cpp
class DatabaseFactory {
public:
    virtual std::unique_ptr<Connection> createConnection() = 0;
    virtual std::unique_ptr<Command> createCommand() = 0;
    virtual std::unique_ptr<ResultSet> createResultSet() = 0;
};

class PostgresFactory : public DatabaseFactory { /* ... */ };
class MySQLFactory : public DatabaseFactory { /* ... */ };
class SQLiteFactory : public DatabaseFactory { /* ... */ };
```

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Using When Only One Product Type Exists

```cpp
// BAD: Abstract Factory for single product
class LoggerFactory {
    virtual std::unique_ptr<Logger> createLogger() = 0;
};
// Just use simple Factory Method instead!
```

### ❌ Mistake 2: Products From Different Families

```cpp
// BAD: Factory allows mixing families
class BadFactory {
    std::unique_ptr<Button> createButton(bool windows);  // ✗
    std::unique_ptr<Checkbox> createCheckbox(bool mac);  // ✗
};
```

### ❌ Mistake 3: Leaking Concrete Types

```cpp
// BAD: Factory returns concrete type
class WindowsFactory {
    std::unique_ptr<WindowsButton> createButton();  // ✗ Exposes WindowsButton
};
// Should return std::unique_ptr<Button>
```

### ❌ Mistake 4: Adding New Product Types

Abstract Factory makes adding new products hard—every factory must change:

```cpp
class GUIFactory {
    virtual std::unique_ptr<Button> createButton() = 0;
    virtual std::unique_ptr<Slider> createSlider() = 0;  // New: ALL factories must update
};
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Single product type | Factory Method |
| Product family known at compile time | Template-based factory |
| No polymorphism needed | Direct construction |
| Frequently adding new product types | Consider Visitor pattern |

### Factory Method vs Abstract Factory

```
Factory Method:
    - Creates ONE product type
    - Single virtual method
    - Simpler

Abstract Factory:
    - Creates FAMILY of products
    - Multiple related methods
    - Ensures consistency
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### C++17: `std::variant` for Fixed Families

```cpp
#include <variant>

using Button = std::variant<WindowsButton, MacButton, LinuxButton>;
using Checkbox = std::variant<WindowsCheckbox, MacCheckbox, LinuxCheckbox>;

struct GUIFamily {
    Button button;
    Checkbox checkbox;
};

GUIFamily createWindowsFamily() {
    return { WindowsButton{}, WindowsCheckbox{} };
}
```

### Dependency Injection Container

```cpp
class DIContainer {
public:
    template<typename T>
    void bind(std::function<std::unique_ptr<T>()> creator);
    
    template<typename T>
    std::unique_ptr<T> resolve();
};

// Configuration determines entire family
container.bind<Button>([]{ return std::make_unique<MacButton>(); });
container.bind<Checkbox>([]{ return std::make_unique<MacCheckbox>(); });
```

### C++20: Concepts for Product Constraints

```cpp
template<typename T>
concept UIProduct = requires(T t) {
    { t.render() } -> std::same_as<void>;
};

template<typename Factory>
concept UIFactory = requires(Factory f) {
    { f.createButton() } -> UIProduct;
    { f.createCheckbox() } -> UIProduct;
};
```

---

## 9. Mental Model Summary

**When Abstract Factory "Clicks":**

Use Abstract Factory when you need to create **families of related objects that must be used together** and **never mixed**. The factory guarantees family consistency. Think: "platform abstraction", "themed UI components", "database driver sets".

**Code Review Recognition:**
- Factory class with multiple `create*()` methods
- Products from same factory used together
- Factory injected into client at construction
- No direct instantiation of concrete products

---

## 中文说明

### 抽象工厂模式要点

1. **问题场景**：
   - 需要创建一组相关对象（产品家族）
   - 这些对象必须配套使用，不能混用
   - 系统需要独立于具体产品类

2. **与工厂方法的区别**：
   ```
   工厂方法：创建单一产品类型
   抽象工厂：创建产品家族（多个相关产品）
   ```

3. **典型应用**：
   - 跨平台 UI 组件（Windows/Mac/Linux）
   - 数据库驱动（MySQL/PostgreSQL/SQLite）
   - 游戏渲染后端（OpenGL/Vulkan/DirectX）

4. **C++ 实现要点**：
   - 工厂返回 `unique_ptr<AbstractProduct>`
   - 工厂本身通过依赖注入传入客户端
   - 客户端只依赖抽象接口

5. **权衡**：
   - 优点：保证产品家族一致性，易于切换整个家族
   - 缺点：添加新产品类型需修改所有工厂

### 决策流程

```
需要创建多个相关产品？
    ├── 否 → 使用工厂方法
    └── 是 → 这些产品必须配套使用？
              ├── 否 → 分开使用多个工厂方法
              └── 是 → 使用抽象工厂
```

