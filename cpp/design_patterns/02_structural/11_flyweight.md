# Pattern 11: Flyweight

## 1. Problem the Pattern Solves

### Design Pressure
- Need to support a large number of fine-grained objects efficiently
- Objects share common state that can be factored out
- Memory usage is a critical concern

### What Goes Wrong Without It
```cpp
// Without flyweight: each character stores font data
class Character {
    char value_;
    std::string fontFamily_;    // "Arial" repeated 1 million times!
    int fontSize_;              // 12 repeated 1 million times!
    std::string color_;         // "#000000" repeated 1 million times!
};
// Document with 1M characters = massive memory waste
```

### Symptoms Indicating Need
- Many objects with identical/similar internal state
- Memory profiler shows redundant data
- Object count in millions or billions
- State naturally divides into intrinsic (shared) and extrinsic (unique)

---

## 2. Core Idea (C++-Specific)

**Flyweight uses sharing to support large numbers of fine-grained objects efficiently by separating intrinsic (shared) state from extrinsic (context-specific) state.**

```
                    +---------------+
                    |FlyweightFactory|
                    | getFlyweight() |
                    +-------+-------+
                            |
              +-------------+-------------+
              |             |             |
        +-----v-----+ +-----v-----+ +-----v-----+
        | Flyweight | | Flyweight | | Flyweight |
        | (shared)  | | (shared)  | | (shared)  |
        |  Arial    | |  Times    | |  Courier  |
        +-----------+ +-----------+ +-----------+
              ^             ^             ^
              |             |             |
        used by many objects with extrinsic state (position, etc.)
```

**Key distinction:**
- **Intrinsic state**: Stored in flyweight, shared (font, texture)
- **Extrinsic state**: Passed in, not stored (position, context)

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::shared_ptr` | Share flyweights | Automatic sharing |
| `std::unordered_map` | Flyweight factory | Fast lookup |
| `const` | Intrinsic state | Immutable after creation |
| `static` factory | `get()` method | Global access point |
| `std::hash` | Map key | Efficient lookup |

### Memory Layout

```cpp
// Without flyweight
struct Particle {
    float x, y, z;           // 12 bytes (extrinsic)
    Texture texture;         // 1000 bytes (intrinsic)
    Color color;             // 16 bytes (intrinsic)
};
// 1M particles = 1M × 1028 bytes = ~1 GB

// With flyweight
struct ParticleType {       // Shared flyweight
    Texture texture;        // 1000 bytes
    Color color;            // 16 bytes
};
struct Particle {
    float x, y, z;                    // 12 bytes
    std::shared_ptr<ParticleType> type; // 8 bytes
};
// 1M particles + 10 types = 1M × 20 + 10 × 1016 ≈ 20 MB
```

---

## 4. Canonical C++ Implementation

### Text Rendering Example

```cpp
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

// Flyweight: shared character formatting
class CharacterFormat {
public:
    CharacterFormat(std::string font, int size, std::string color)
        : font_(std::move(font))
        , size_(size)
        , color_(std::move(color)) {}
    
    const std::string& font() const { return font_; }
    int size() const { return size_; }
    const std::string& color() const { return color_; }
    
private:
    const std::string font_;
    const int size_;
    const std::string color_;
};

// Flyweight factory
class FormatFactory {
public:
    static FormatFactory& instance() {
        static FormatFactory factory;
        return factory;
    }
    
    std::shared_ptr<CharacterFormat> getFormat(
        const std::string& font, int size, const std::string& color) 
    {
        std::string key = font + ":" + std::to_string(size) + ":" + color;
        
        auto it = formats_.find(key);
        if (it != formats_.end()) {
            return it->second;
        }
        
        auto format = std::make_shared<CharacterFormat>(font, size, color);
        formats_[key] = format;
        return format;
    }
    
    size_t formatCount() const { return formats_.size(); }
    
private:
    FormatFactory() = default;
    std::unordered_map<std::string, std::shared_ptr<CharacterFormat>> formats_;
};

// Client object with extrinsic state
class Character {
public:
    Character(char value, int x, int y, 
              std::shared_ptr<CharacterFormat> format)
        : value_(value), x_(x), y_(y), format_(std::move(format)) {}
    
    void render() const {
        std::cout << "Char '" << value_ << "' at (" << x_ << "," << y_ 
                  << ") with " << format_->font() << " " 
                  << format_->size() << "pt " << format_->color() << "\n";
    }
    
private:
    char value_;          // Unique per character
    int x_, y_;           // Extrinsic state
    std::shared_ptr<CharacterFormat> format_;  // Shared flyweight
};

// Document using flyweights
class Document {
public:
    void addCharacter(char c, int x, int y, 
                     const std::string& font, int size, 
                     const std::string& color) {
        auto format = FormatFactory::instance().getFormat(font, size, color);
        characters_.emplace_back(c, x, y, format);
    }
    
    void render() const {
        for (const auto& ch : characters_) {
            ch.render();
        }
    }
    
    size_t characterCount() const { return characters_.size(); }
    
private:
    std::vector<Character> characters_;
};

int main() {
    Document doc;
    
    // All these share the same format object
    doc.addCharacter('H', 0, 0, "Arial", 12, "black");
    doc.addCharacter('e', 10, 0, "Arial", 12, "black");
    doc.addCharacter('l', 20, 0, "Arial", 12, "black");
    doc.addCharacter('l', 30, 0, "Arial", 12, "black");
    doc.addCharacter('o', 40, 0, "Arial", 12, "black");
    
    // Different format
    doc.addCharacter('!', 50, 0, "Arial", 24, "red");
    
    doc.render();
    
    std::cout << "\nCharacters: " << doc.characterCount() << "\n";
    std::cout << "Unique formats: " << FormatFactory::instance().formatCount() << "\n";
    
    return 0;
}
```

### Game Particle System

```cpp
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

// Flyweight: shared particle type data
class ParticleType {
public:
    ParticleType(std::string texturePath, float maxSpeed, float lifetime)
        : texturePath_(std::move(texturePath))
        , maxSpeed_(maxSpeed)
        , lifetime_(lifetime) {
        // Expensive: load texture
        // textureId_ = loadTexture(texturePath_);
    }
    
    const std::string& texture() const { return texturePath_; }
    float maxSpeed() const { return maxSpeed_; }
    float lifetime() const { return lifetime_; }
    
private:
    std::string texturePath_;
    float maxSpeed_;
    float lifetime_;
    // unsigned textureId_;  // OpenGL texture handle
};

class ParticleTypeFactory {
public:
    std::shared_ptr<ParticleType> getType(const std::string& name) {
        auto it = types_.find(name);
        if (it != types_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    void registerType(const std::string& name, 
                     std::shared_ptr<ParticleType> type) {
        types_[name] = std::move(type);
    }
    
private:
    std::unordered_map<std::string, std::shared_ptr<ParticleType>> types_;
};

// Individual particle with extrinsic state
struct Particle {
    float x, y, z;              // Position (extrinsic)
    float vx, vy, vz;           // Velocity (extrinsic)
    float age;                  // Current age (extrinsic)
    std::shared_ptr<ParticleType> type;  // Shared (flyweight)
    
    void update(float dt) {
        x += vx * dt;
        y += vy * dt;
        z += vz * dt;
        age += dt;
    }
    
    bool isAlive() const {
        return age < type->lifetime();
    }
};

class ParticleSystem {
public:
    explicit ParticleSystem(std::shared_ptr<ParticleType> type)
        : type_(std::move(type)) {}
    
    void emit(float x, float y, float z) {
        particles_.push_back({x, y, z, 0, 1, 0, 0, type_});
    }
    
    void update(float dt) {
        for (auto& p : particles_) {
            p.update(dt);
        }
        // Remove dead particles
        particles_.erase(
            std::remove_if(particles_.begin(), particles_.end(),
                          [](const Particle& p) { return !p.isAlive(); }),
            particles_.end());
    }
    
    size_t count() const { return particles_.size(); }
    
private:
    std::shared_ptr<ParticleType> type_;
    std::vector<Particle> particles_;
};
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| Text editors | Character formatting, fonts |
| Games | Particle systems, sprites, tiles |
| Graphics | Texture atlases, materials |
| Databases | Connection strings, prepared statements |
| Caching | Immutable cached objects |

### Real-World Examples
- **Java**: `String.intern()` - string flyweight pool
- **Game engines**: Sprite sheets, tile maps
- **Browsers**: DOM node style sharing
- **Python**: Small integer caching (-5 to 256)

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Mutable Flyweight State

```cpp
// BAD: Flyweight with mutable state
class Flyweight {
    std::string sharedData_;
    mutable int useCount_;  // ✗ Not thread-safe!
public:
    void use() { ++useCount_; }
};
// Flyweights should be immutable
```

### ❌ Mistake 2: Storing Extrinsic State in Flyweight

```cpp
// BAD: Position is extrinsic!
class TreeType {
    std::string species_;
    Texture texture_;
    int x_, y_;  // ✗ Should not be in flyweight
};
// Each tree at different position should share the type
```

### ❌ Mistake 3: Overhead Exceeds Savings

```cpp
// BAD: Flyweight for simple value
class Point {
    int x, y;  // 8 bytes
};
// With flyweight:
class PointFlyweight { int x, y; };  // 8 bytes
class PointRef { shared_ptr<PointFlyweight> p; };  // 16 bytes!
// Made it worse!
```

### ❌ Mistake 4: Not Considering Cache Locality

```cpp
// Flyweight via indirection hurts cache
for (auto& p : particles) {
    p.type->texture();  // Pointer chase on every particle!
}
// Consider data-oriented design for hot loops
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| Few objects | Don't bother |
| No shared state | Value types |
| Performance critical loops | Data-oriented design |
| Mutable shared state | Object pool |

### Alternative: Data-Oriented Design

```cpp
// Instead of flyweight with pointer chasing:
struct ParticleData {
    std::vector<float> x, y, z;       // SOA layout
    std::vector<float> vx, vy, vz;
    std::vector<int> typeIndex;       // Index, not pointer
};
// Better cache utilization for hot loops
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### `std::string_view` as Lightweight Reference

```cpp
#include <string_view>

class SharedStrings {
public:
    std::string_view intern(std::string_view s) {
        auto [it, inserted] = strings_.insert(std::string(s));
        return *it;
    }
private:
    std::set<std::string> strings_;
};
// string_view is a flyweight reference to string data
```

### `std::pmr` for Custom Memory

```cpp
#include <memory_resource>

// Use pool allocator for flyweights
std::pmr::monotonic_buffer_resource pool;
std::pmr::vector<ParticleType> types{&pool};
// All types in contiguous memory
```

### `constexpr` Compile-Time Flyweight

```cpp
// Compile-time flyweight (no runtime sharing needed)
constexpr CharFormat BOLD_RED{"Arial", 12, "red", true};
constexpr CharFormat PLAIN{"Arial", 12, "black", false};

// Zero runtime cost - embedded in binary
```

### Object Pool (Related Pattern)

```cpp
template<typename T>
class ObjectPool {
public:
    T* acquire() {
        if (pool_.empty()) {
            return new T();
        }
        T* obj = pool_.back();
        pool_.pop_back();
        return obj;
    }
    
    void release(T* obj) {
        pool_.push_back(obj);
    }
    
private:
    std::vector<T*> pool_;
};
// Reuses mutable objects instead of sharing immutable ones
```

---

## 9. Mental Model Summary

**When Flyweight "Clicks":**

Use Flyweight when you have **millions of objects** with **common intrinsic state** that can be **shared immutably**. The pattern trades pointer indirection for memory savings. Think: "string interning", "sprite sheets", "character formatting".

**Code Review Recognition:**
- Factory returning `shared_ptr` to existing objects
- Cache/map of immutable objects
- Client objects holding references to shared objects
- Clear separation of intrinsic (in flyweight) vs extrinsic (passed in)
- Check: Is the sharing actually saving memory? (Measure!)

---

## 中文说明

### 享元模式要点

1. **问题场景**：
   - 需要支持大量细粒度对象
   - 对象有可共享的内在状态
   - 内存使用是关键问题

2. **核心概念**：
   ```
   内在状态（Intrinsic）：存在享元中，可共享
   外在状态（Extrinsic）：由客户端传入，不存储
   ```

3. **内存节省示例**：
   ```
   不用享元：100万粒子 × 1KB = 1GB
   使用享元：100万粒子 × 20B + 10种类型 × 1KB = 20MB
   ```

4. **C++ 实现要点**：
   - 享元工厂使用 `unordered_map` 缓存
   - 返回 `shared_ptr` 共享对象
   - 享元状态必须是不可变的（`const`）

5. **常见错误**：
   - 享元包含可变状态
   - 外在状态存入享元
   - 对小对象使用享元（开销超过节省）
   - 忽略缓存局部性问题

### 何时使用

```
对象数量是否巨大（百万级）？
    ├── 否 → 不需要享元
    └── 是 → 是否有可共享的不变状态？
              ├── 否 → 考虑对象池
              └── 是 → 共享后指针开销是否小于状态大小？
                        ├── 否 → 不划算
                        └── 是 → 使用享元
```

