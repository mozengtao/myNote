# Pattern 6: Adapter

## 1. Problem the Pattern Solves

### Design Pressure
- Existing class interface doesn't match what client code expects
- Want to reuse a third-party or legacy library with incompatible interface
- Need to integrate systems with different APIs

### What Goes Wrong Without It
```cpp
// Client expects this interface
void render(Renderer& r) {
    r.drawCircle(x, y, radius);
}

// But legacy library has this:
class OldGraphicsLib {
    void renderEllipse(int cx, int cy, int rx, int ry);  // Different!
};

// Without adapter: modify client code everywhere or fork library
```

### Symptoms Indicating Need
- Wrapper functions scattered throughout codebase
- `if/else` branches handling different API versions
- Client code aware of multiple incompatible interfaces
- Need to use class that "almost" fits

---

## 2. Core Idea (C++-Specific)

**Adapter converts the interface of a class into another interface clients expect, allowing incompatible classes to work together.**

```
+---------+       +----------+       +----------+
| Client  | ----> | Target   | <---- | Adapter  |
| code    |       | Interface|       +----+-----+
+---------+       +----------+            |
                                          | wraps/inherits
                                          v
                                    +----------+
                                    | Adaptee  |
                                    | (legacy) |
                                    +----------+
```

Two forms in C++:
1. **Object Adapter**: Composition (holds adaptee reference)
2. **Class Adapter**: Multiple inheritance (inherits both)

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Composition | Object adapter | Hold adaptee by pointer/reference |
| Multiple inheritance | Class adapter | Inherit interface + implementation |
| `explicit` constructor | Prevent implicit conversion | Clear adapter construction |
| `override` | Implement target interface | Compiler verification |
| `std::reference_wrapper` | Store reference in container | Adapter collections |

### Object vs Class Adapter

| Aspect | Object Adapter | Class Adapter |
|--------|---------------|---------------|
| Coupling | Looser | Tighter |
| Flexibility | Can adapt subclasses | Fixed to one adaptee |
| C++ feature | Composition | Multiple inheritance |
| Preferred | ✓ Usually | Rarely |

---

## 4. Canonical C++ Implementation

### Object Adapter (Preferred)

```cpp
#include <iostream>
#include <memory>
#include <string>

// Target interface (what client expects)
class MediaPlayer {
public:
    virtual ~MediaPlayer() = default;
    virtual void play(const std::string& filename) = 0;
    virtual void stop() = 0;
};

// Adaptee (legacy/third-party library)
class AdvancedMediaLib {
public:
    void loadFile(const char* path) {
        std::cout << "AdvancedLib: Loading " << path << "\n";
    }
    void startPlayback() {
        std::cout << "AdvancedLib: Playback started\n";
    }
    void stopPlayback() {
        std::cout << "AdvancedLib: Playback stopped\n";
    }
};

// Adapter - converts AdvancedMediaLib to MediaPlayer interface
class MediaPlayerAdapter : public MediaPlayer {
public:
    explicit MediaPlayerAdapter(std::unique_ptr<AdvancedMediaLib> lib)
        : lib_(std::move(lib)) {}
    
    void play(const std::string& filename) override {
        lib_->loadFile(filename.c_str());
        lib_->startPlayback();
    }
    
    void stop() override {
        lib_->stopPlayback();
    }
    
private:
    std::unique_ptr<AdvancedMediaLib> lib_;
};

// Client code - only knows MediaPlayer interface
void playMusic(MediaPlayer& player, const std::string& file) {
    player.play(file);
    // ... later
    player.stop();
}

int main() {
    auto adapter = std::make_unique<MediaPlayerAdapter>(
        std::make_unique<AdvancedMediaLib>()
    );
    
    playMusic(*adapter, "song.mp3");
    return 0;
}
```

### Class Adapter (Multiple Inheritance)

```cpp
#include <iostream>

// Target interface
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void drawCircle(int x, int y, int r) = 0;
};

// Adaptee
class LegacyGraphics {
public:
    void drawEllipse(int x, int y, int rx, int ry) {
        std::cout << "Legacy ellipse at (" << x << "," << y 
                  << ") rx=" << rx << " ry=" << ry << "\n";
    }
};

// Class adapter - inherits both
class GraphicsAdapter : public Renderer, private LegacyGraphics {
public:
    void drawCircle(int x, int y, int r) override {
        // Translate circle to ellipse with equal radii
        drawEllipse(x, y, r, r);
    }
};

int main() {
    GraphicsAdapter adapter;
    adapter.drawCircle(100, 100, 50);
    return 0;
}
```

### Function Adapter (Modern C++)

```cpp
#include <functional>
#include <iostream>

// Modern alternative: adapt with lambda/function
class LegacySort {
public:
    // Old interface: returns negative/zero/positive
    using Comparator = int(*)(const void*, const void*);
    void sort(void* data, size_t count, size_t size, Comparator cmp);
};

// Adapt to modern std::function
template<typename T>
auto adaptComparator(std::function<bool(const T&, const T&)> modern) {
    return [modern](const void* a, const void* b) -> int {
        const T& ta = *static_cast<const T*>(a);
        const T& tb = *static_cast<const T*>(b);
        if (modern(ta, tb)) return -1;
        if (modern(tb, ta)) return 1;
        return 0;
    };
}
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| GUI frameworks | Widget wrappers |
| Database access | Driver adapters |
| Networking | Protocol converters |
| File I/O | Stream adapters |
| Testing | Mock adapters |

### STL Examples
- `std::stack` adapts `std::deque`
- `std::queue` adapts `std::deque`
- `std::priority_queue` adapts `std::vector`
- Iterator adapters (`std::reverse_iterator`)

### Real-World
- **Qt**: `QAbstractItemModel` adapts various data sources
- **Boost.Asio**: Socket adapters for different protocols
- **SQLite**: C API wrapped in C++ classes

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Adapter That Changes Behavior

```cpp
// BAD: Adapter adds caching (not just interface translation)
class CachingAdapter : public DataSource {
    void fetchData() override {
        if (!cache_.empty()) return cache_;  // New behavior!
        cache_ = adaptee_->getData();
        return cache_;
    }
};
// This is Decorator, not Adapter
```

### ❌ Mistake 2: Adapting Too Many Methods

```cpp
// BAD: Adapter exposes entire adaptee interface
class FullAdapter : public Target {
    void method1() override { adaptee_->a(); }
    void method2() override { adaptee_->b(); }
    void method3() override { adaptee_->c(); }
    // ... 50 more methods
};
// Consider if Facade is more appropriate
```

### ❌ Mistake 3: Leaking Adaptee

```cpp
// BAD: Exposes adaptee
class LeakyAdapter : public Target {
public:
    LegacyLib& getAdaptee() { return *adaptee_; }  // ✗
};
// Client becomes dependent on adaptee interface
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Simplifying complex interface | Facade |
| Adding functionality | Decorator |
| Converting data, not interfaces | Converter function |
| Complete rewrite possible | Direct implementation |

### Adapter vs Facade vs Decorator

```
Adapter:   Makes INCOMPATIBLE interface COMPATIBLE
Facade:    Makes COMPLEX interface SIMPLER
Decorator: ADDS behavior to existing interface
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### Lambda Adapter

```cpp
#include <functional>

// Old API
void legacyProcess(int(*callback)(int));

// Modern usage with adapter lambda
std::function<int(int)> modern = [](int x) { return x * 2; };

// Can't pass std::function directly, so adapt:
legacyProcess([](int x) -> int {
    // Capture modern lambda in static or use trampoline
    static auto fn = [](int x) { return x * 2; };
    return fn(x);
});
```

### Template Adapter

```cpp
template<typename Adaptee>
class GenericAdapter : public Target {
public:
    explicit GenericAdapter(Adaptee adaptee) : adaptee_(std::move(adaptee)) {}
    
    void targetMethod() override {
        adaptee_.legacyMethod();
    }
    
private:
    Adaptee adaptee_;
};

// Works with any adaptee that has legacyMethod()
```

### C++20 Concepts for Adaptable Types

```cpp
template<typename T>
concept Drawable = requires(T t, int x, int y, int r) {
    t.drawCircle(x, y, r);
};

template<typename T>
concept LegacyDrawable = requires(T t, int x, int y, int rx, int ry) {
    t.drawEllipse(x, y, rx, ry);
};

template<LegacyDrawable T>
class DrawableAdapter {
public:
    explicit DrawableAdapter(T& adaptee) : adaptee_(adaptee) {}
    
    void drawCircle(int x, int y, int r) {
        adaptee_.drawEllipse(x, y, r, r);
    }
    
private:
    T& adaptee_;
};
```

---

## 9. Mental Model Summary

**When Adapter "Clicks":**

Use Adapter when you have **existing code with an incompatible interface** that you **cannot or should not modify**. The adapter translates between interfaces without adding new behavior. Think: "I have X, I need Y interface."

**Code Review Recognition:**
- Class implementing one interface while holding another
- Method implementations that delegate to wrapped object
- Named with suffixes like `Adapter`, `Wrapper`
- Check: Is this only translating interface, or also adding behavior?

---

## 中文说明

### 适配器模式要点

1. **问题场景**：
   - 现有类接口与期望接口不兼容
   - 想复用第三方或遗留代码
   - 不能或不应修改原有代码

2. **两种形式**：
   ```
   对象适配器（推荐）：组合方式，持有被适配者引用
   类适配器：多重继承，同时继承目标接口和被适配者
   ```

3. **与其他模式的区别**：
   ```
   适配器：使不兼容接口变兼容
   外观：简化复杂接口
   装饰器：添加新功能
   ```

4. **STL 中的适配器**：
   - `std::stack` 适配 `std::deque`
   - `std::queue` 适配 `std::deque`
   - `std::reverse_iterator` 适配正向迭代器

5. **常见错误**：
   - 适配器中添加新行为（应该用装饰器）
   - 暴露被适配者给客户端
   - 适配方法过多（考虑外观模式）

### 何时使用

```
需要使用接口不兼容的类？
    ├── 可以修改源码 → 直接修改
    └── 不能修改 → 使用适配器
                    └── 只需转换接口，不添加功能
```

