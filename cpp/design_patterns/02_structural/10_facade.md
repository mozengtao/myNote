# Pattern 10: Facade

## 1. Problem the Pattern Solves

### Design Pressure
- Complex subsystem with many interacting classes
- Client code needs simple interface to common use cases
- Want to decouple clients from subsystem internals
- Reduce learning curve for subsystem usage

### What Goes Wrong Without It
```cpp
// Without facade: client must orchestrate multiple classes
void compileCode() {
    Lexer lexer;
    auto tokens = lexer.tokenize(source);
    
    Parser parser;
    auto ast = parser.parse(tokens);
    
    TypeChecker checker;
    checker.check(ast);
    
    CodeGenerator gen;
    auto code = gen.generate(ast);
    
    Optimizer opt;
    opt.optimize(code);
    
    Linker linker;
    linker.link(code, libraries);
}
// Complex sequence exposed to every client
```

### Symptoms Indicating Need
- Multiple classes needed for common operations
- Same initialization/setup code repeated
- Clients tightly coupled to many subsystem classes
- Subsystem hard to use without reading all documentation

---

## 2. Core Idea (C++-Specific)

**Facade provides a unified interface to a set of interfaces in a subsystem, making the subsystem easier to use.**

```
+--------+         +---------+
| Client | ------> | Facade  |
+--------+         +----+----+
                        |
          +-------------+-------------+
          |             |             |
     +----v----+   +----v----+   +----v----+
     | Class A |   | Class B |   | Class C |
     +---------+   +---------+   +---------+
           Subsystem Classes
```

Facade does NOT hide subsystem classes—clients can still access them directly for advanced use.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Aggregation | Hold subsystem refs | Coordinate classes |
| Simple methods | High-level operations | Easy API |
| `explicit` | Constructor | Clear initialization |
| No inheritance | Usually standalone | Not polymorphic |
| `noexcept` | Where appropriate | Performance hint |

### Facade Characteristics

```cpp
// Facade is typically:
- Non-polymorphic (no virtual methods)
- Stateless or minimal state
- Not hiding subsystems (just simplifying)
- Thread-safe if subsystems are
```

---

## 4. Canonical C++ Implementation

### Compiler Facade

```cpp
#include <string>
#include <vector>
#include <memory>
#include <iostream>

// Complex subsystem classes
class Lexer {
public:
    std::vector<std::string> tokenize(const std::string& source) {
        std::cout << "Lexer: tokenizing...\n";
        return {"token1", "token2", "token3"};
    }
};

class Parser {
public:
    struct AST { std::string data; };
    
    AST parse(const std::vector<std::string>& tokens) {
        std::cout << "Parser: parsing " << tokens.size() << " tokens...\n";
        return {"ast-root"};
    }
};

class Optimizer {
public:
    void optimize(Parser::AST& ast) {
        std::cout << "Optimizer: optimizing AST...\n";
    }
};

class CodeGenerator {
public:
    std::string generate(const Parser::AST& ast) {
        std::cout << "CodeGen: generating code...\n";
        return "compiled_binary";
    }
};

// Facade - simple interface for common use case
class Compiler {
public:
    std::string compile(const std::string& source) {
        // Orchestrate subsystem classes
        auto tokens = lexer_.tokenize(source);
        auto ast = parser_.parse(tokens);
        optimizer_.optimize(ast);
        return generator_.generate(ast);
    }
    
    // Expose subsystems for advanced use (optional)
    Lexer& lexer() { return lexer_; }
    Parser& parser() { return parser_; }
    
private:
    Lexer lexer_;
    Parser parser_;
    Optimizer optimizer_;
    CodeGenerator generator_;
};

int main() {
    Compiler compiler;
    
    // Simple: use facade
    auto binary = compiler.compile("int main() { return 0; }");
    std::cout << "Result: " << binary << "\n";
    
    // Advanced: access subsystem directly
    auto tokens = compiler.lexer().tokenize("int x = 5;");
    
    return 0;
}
```

### Home Automation Facade

```cpp
#include <iostream>

class Lights {
public:
    void dim(int level) { 
        std::cout << "Lights dimmed to " << level << "%\n"; 
    }
    void on() { std::cout << "Lights on\n"; }
    void off() { std::cout << "Lights off\n"; }
};

class Thermostat {
public:
    void setTemp(int temp) { 
        std::cout << "Temperature set to " << temp << "°F\n"; 
    }
};

class AudioSystem {
public:
    void on() { std::cout << "Audio on\n"; }
    void off() { std::cout << "Audio off\n"; }
    void setVolume(int vol) { 
        std::cout << "Volume set to " << vol << "\n"; 
    }
    void playMusic(const std::string& playlist) {
        std::cout << "Playing: " << playlist << "\n";
    }
};

class SecuritySystem {
public:
    void arm() { std::cout << "Security armed\n"; }
    void disarm() { std::cout << "Security disarmed\n"; }
};

// Facade for common scenarios
class SmartHome {
public:
    void leaveHome() {
        lights_.off();
        thermostat_.setTemp(60);
        audio_.off();
        security_.arm();
        std::cout << "--- Home secured for departure ---\n";
    }
    
    void arriveHome() {
        security_.disarm();
        lights_.on();
        thermostat_.setTemp(72);
        std::cout << "--- Welcome home! ---\n";
    }
    
    void movieMode() {
        lights_.dim(20);
        audio_.on();
        audio_.setVolume(50);
        thermostat_.setTemp(70);
        std::cout << "--- Movie mode activated ---\n";
    }
    
    // Expose for custom control
    Lights& lights() { return lights_; }
    AudioSystem& audio() { return audio_; }
    
private:
    Lights lights_;
    Thermostat thermostat_;
    AudioSystem audio_;
    SecuritySystem security_;
};

int main() {
    SmartHome home;
    
    home.arriveHome();
    home.movieMode();
    
    // Custom: just the audio
    home.audio().playMusic("Jazz Favorites");
    
    home.leaveHome();
    
    return 0;
}
```

### Database Access Facade

```cpp
#include <memory>
#include <string>
#include <vector>

class ConnectionPool {
public:
    class Connection { /* ... */ };
    std::unique_ptr<Connection> acquire() { return nullptr; }
    void release(std::unique_ptr<Connection>) {}
};

class QueryBuilder {
public:
    std::string select(const std::string& table, 
                      const std::vector<std::string>& columns) {
        return "SELECT ...";
    }
};

class ResultMapper {
public:
    template<typename T>
    std::vector<T> mapResults(/* result set */) {
        return {};
    }
};

class TransactionManager {
public:
    void begin() {}
    void commit() {}
    void rollback() {}
};

// Facade for common database operations
class Database {
public:
    template<typename T>
    std::vector<T> query(const std::string& table,
                        const std::vector<std::string>& columns) {
        auto conn = pool_.acquire();
        auto sql = builder_.select(table, columns);
        // Execute sql...
        pool_.release(std::move(conn));
        return mapper_.mapResults<T>();
    }
    
    template<typename F>
    void transaction(F&& operation) {
        tx_.begin();
        try {
            operation();
            tx_.commit();
        } catch (...) {
            tx_.rollback();
            throw;
        }
    }
    
private:
    ConnectionPool pool_;
    QueryBuilder builder_;
    ResultMapper mapper_;
    TransactionManager tx_;
};
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| Compilers | Front-end API hiding lexer/parser/codegen |
| Databases | ORM hiding connection/query/mapping |
| GUI | Application class coordinating windows/menus |
| Networking | HTTP client hiding sockets/SSL/parsing |
| Game engines | Engine class for init/render/shutdown |

### Real-World Examples
- **SDL2**: `SDL_Init()` initializes multiple subsystems
- **OpenGL**: `glutInit()` and related functions
- **Qt**: `QApplication` coordinates event loop, widgets

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Facade Becomes God Object

```cpp
// BAD: Facade does too much
class EverythingFacade {
    void doNetworking();
    void doDatabaseStuff();
    void handleUI();
    void processAudio();
    void manageFiles();
    // 100 more methods...
};
// Split into multiple focused facades
```

### ❌ Mistake 2: Hiding Subsystems Completely

```cpp
// BAD: No access to underlying classes
class StrictFacade {
private:  // All subsystems hidden
    Lexer lexer_;
    Parser parser_;
};
// Users need advanced access sometimes
```

### ❌ Mistake 3: Facade With Business Logic

```cpp
// BAD: Facade adds new logic instead of just coordinating
class BadFacade {
    void process() {
        // 500 lines of business logic
        // This should be in subsystem classes
    }
};
```

### ❌ Mistake 4: Making Facade an Interface

```cpp
// BAD: Unnecessary abstraction
class IFacade {
    virtual void operation() = 0;
};
class ConcreteFacade : public IFacade {};
// Facades rarely need polymorphism
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Single class does the job | Just use that class |
| Need polymorphism | Different pattern |
| Hiding for encapsulation | Module/namespace |
| Dynamic behavior switching | Strategy pattern |

### Just Use the Subsystem

```cpp
// If client only needs one subsystem class:
Parser parser;
auto ast = parser.parse(tokens);
// No facade needed
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### Namespace as Facade

```cpp
namespace json {
    // Internal implementation classes
    namespace detail {
        class Parser { /* ... */ };
        class Serializer { /* ... */ };
        class Validator { /* ... */ };
    }
    
    // Facade functions
    Value parse(const std::string& text) {
        detail::Parser p;
        return p.parse(text);
    }
    
    std::string stringify(const Value& v) {
        detail::Serializer s;
        return s.serialize(v);
    }
}

// Client just uses:
auto data = json::parse(text);
```

### Free Functions as Facade

```cpp
// Header: compiler.h
std::string compile(const std::string& source);
std::string compileWithOptions(const std::string& source, 
                               const CompileOptions& opts);

// Implementation uses all subsystem classes internally
// Client never sees Lexer, Parser, etc.
```

### C++20 Modules

```cpp
// compiler.cppm
export module compiler;

// Only export the facade
export std::string compile(const std::string& source);

// Subsystems are module-private
class Lexer { /* ... */ };
class Parser { /* ... */ };
```

---

## 9. Mental Model Summary

**When Facade "Clicks":**

Use Facade when a **subsystem is complex** but **common use cases are simple**. The facade provides a "happy path" API while still allowing access to underlying classes for advanced users. Think: "simple interface to complex library", "one-liner for common operations".

**Code Review Recognition:**
- Class that aggregates/coordinates multiple subsystem classes
- Methods named for high-level operations (`compile()`, `initialize()`)
- Little to no business logic in the facade itself
- Subsystems often still accessible for advanced use
- Check: Is this just wrapping one class? (Then it's not really a facade)

---

## 中文说明

### 外观模式要点

1. **问题场景**：
   - 复杂子系统有很多交互类
   - 客户端需要简单接口处理常见用例
   - 需要解耦客户端与子系统内部

2. **核心设计**：
   ```
   外观提供简化接口
   但不隐藏子系统（高级用户仍可直接访问）
   ```

3. **C++ 实现方式**：
   - 聚合子系统对象
   - 提供高层方法协调子系统
   - 可选：暴露子系统引用供高级使用

4. **典型应用**：
   - 编译器前端
   - 数据库访问层
   - 游戏引擎初始化
   - 智能家居控制

5. **常见错误**：
   - 外观变成上帝对象（太多职责）
   - 完全隐藏子系统（不给高级访问）
   - 在外观中添加业务逻辑

### 与其他模式的区别

```
外观：简化复杂接口
适配器：改变接口形式
代理：控制访问
中介者：协调对等对象通信
```

### 现代 C++ 替代方案

```cpp
// 使用命名空间 + 自由函数
namespace compiler {
    std::string compile(const std::string& src);
}

// 使用 C++20 模块
export module compiler;
export std::string compile(...);
```

