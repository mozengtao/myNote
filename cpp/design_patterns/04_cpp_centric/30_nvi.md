# Pattern 30: Non-Virtual Interface (NVI)

## 1. Problem the Pattern Solves

### Design Pressure
- Control how virtual functions are called
- Add pre/post conditions to virtual methods
- Separate public interface from extension points

### What Goes Wrong Without It
```cpp
class Base {
public:
    virtual void process() {  // Public virtual = loose control
        // Client can call directly, bypassing invariants
    }
};

class Derived : public Base {
    void process() override {
        // Might forget to call pre/post checks
    }
};
```

---

## 2. Core Idea (C++-Specific)

**NVI uses public non-virtual functions that call private/protected virtual functions. The base class controls the interface; derived classes only customize behavior.**

```cpp
class Base {
public:
    void process() {           // Non-virtual, public
        preCondition();
        doProcess();           // Calls virtual
        postCondition();
    }
    
private:
    virtual void doProcess() = 0;  // Virtual, private
    
    void preCondition() { /* check invariants */ }
    void postCondition() { /* verify results */ }
};
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `private virtual` | Extension point | Derived customizes |
| Public non-virtual | Interface | Base controls |
| `final` | Lock implementation | Prevent override |

---

## 4. Canonical C++ Implementation

```cpp
#include <iostream>
#include <stdexcept>

class Document {
public:
    // Public non-virtual interface
    void save() {
        validate();
        log("Saving document...");
        doSave();  // Virtual hook
        log("Document saved.");
    }
    
    virtual ~Document() = default;
    
private:
    // Private virtual - derived classes customize
    virtual void doSave() = 0;
    
    void validate() {
        std::cout << "Validating...\n";
        // Invariant checks that ALWAYS happen
    }
    
    void log(const std::string& msg) {
        std::cout << "[LOG] " << msg << "\n";
    }
};

class TextDocument : public Document {
private:
    void doSave() override {
        std::cout << "Saving as .txt\n";
    }
};

class PDFDocument : public Document {
private:
    void doSave() override {
        std::cout << "Saving as .pdf\n";
    }
};

int main() {
    TextDocument txt;
    txt.save();
    // Output:
    // Validating...
    // [LOG] Saving document...
    // Saving as .txt
    // [LOG] Document saved.
    
    PDFDocument pdf;
    pdf.save();
    
    return 0;
}
```

### With Optional Hooks

```cpp
class Widget {
public:
    void draw() {
        setupGraphics();
        doDraw();            // Required override
        drawBorder();        // Optional hook
        cleanupGraphics();
    }
    
private:
    virtual void doDraw() = 0;           // Must override
    virtual void drawBorder() {}          // Optional, default empty
    
    void setupGraphics() { /* ... */ }
    void cleanupGraphics() { /* ... */ }
};
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Frameworks | Application lifecycle |
| Serialization | Pre/post processing |
| Validation | Input checking |
| Logging | Automatic instrumentation |

---

## 6. Common Mistakes

### ❌ Public Virtual Methods

```cpp
// BAD: No control over how derived class is called
class Base {
public:
    virtual void process();  // Anyone can call, can bypass checks
};
```

### ❌ Forgetting Base Implementation

```cpp
// With NVI, this is prevented:
class Derived : public Base {
    void doProcess() override {
        // No need to call base - NVI handles it
    }
};
```

---

## 7. NVI vs Template Method

| Aspect | NVI | Template Method |
|--------|-----|-----------------|
| Focus | Control | Algorithm structure |
| Override visibility | Private | Protected |
| Inheritance depth | Usually 1 level | Can be deep |

---

## 8. Mental Model Summary

**When NVI "Clicks":**

Use NVI when you want the **base class to control the interface** while derived classes only customize specific behavior. It's a way to enforce invariants and add cross-cutting concerns without trusting derived classes.

---

## 中文说明

### NVI 要点

1. **核心结构**：
   ```cpp
   public 非虚函数  →  控制接口
   private 虚函数   →  定制点
   ```

2. **好处**：
   - 基类控制调用流程
   - 自动应用前置/后置条件
   - 派生类无法绕过检查

3. **与模板方法区别**：
   - NVI：关注控制
   - 模板方法：关注算法结构

4. **常见应用**：
   - 框架生命周期
   - 序列化
   - 验证
   - 日志

