# Pattern 16: Observer

## 1. Problem the Pattern Solves

### Design Pressure
- One-to-many dependency between objects
- When one object changes, dependents should be notified automatically
- Loose coupling between subject and observers

### What Goes Wrong Without It
```cpp
// Without observer: tight coupling
class DataModel {
    void update() {
        view1_.refresh();  // Knows all views
        view2_.refresh();
        chart_.redraw();
        // Adding new view = modify DataModel
    }
};
```

---

## 2. Core Idea (C++-Specific)

**Observer defines a one-to-many dependency so that when one object changes state, all dependents are notified automatically.**

```
+----------+       +------------+
| Subject  |------>| Observer   |
| attach() |       | update()   |
| notify() |       +------------+
+----------+            ^
     |           +------+------+
     |           |             |
 notifies   +--------+    +--------+
            | ObsA   |    | ObsB   |
            +--------+    +--------+
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::function` | Callback-based | Modern approach |
| `std::weak_ptr` | Safe observer ref | Avoid dangling |
| Virtual interface | Classic OOP | Type-erased |
| Signals/slots | Qt style | Higher-level |

---

## 4. Canonical C++ Implementation

### Modern: Callback-Based

```cpp
#include <functional>
#include <vector>
#include <algorithm>
#include <iostream>

template<typename... Args>
class Signal {
public:
    using Slot = std::function<void(Args...)>;
    using SlotId = size_t;
    
    SlotId connect(Slot slot) {
        slots_.push_back({nextId_, std::move(slot)});
        return nextId_++;
    }
    
    void disconnect(SlotId id) {
        slots_.erase(
            std::remove_if(slots_.begin(), slots_.end(),
                [id](const auto& p) { return p.first == id; }),
            slots_.end());
    }
    
    void emit(Args... args) {
        for (auto& [id, slot] : slots_) {
            slot(args...);
        }
    }
    
private:
    std::vector<std::pair<SlotId, Slot>> slots_;
    SlotId nextId_ = 0;
};

class DataModel {
public:
    Signal<int> valueChanged;
    
    void setValue(int v) {
        if (value_ != v) {
            value_ = v;
            valueChanged.emit(v);
        }
    }
    
private:
    int value_ = 0;
};

int main() {
    DataModel model;
    
    auto id1 = model.valueChanged.connect([](int v) {
        std::cout << "Observer 1: value = " << v << "\n";
    });
    
    model.valueChanged.connect([](int v) {
        std::cout << "Observer 2: value = " << v << "\n";
    });
    
    model.setValue(42);
    // Both observers notified
    
    model.valueChanged.disconnect(id1);
    model.setValue(100);
    // Only observer 2 notified
    
    return 0;
}
```

### With `weak_ptr` Safety

```cpp
#include <memory>
#include <vector>
#include <algorithm>

class Observer {
public:
    virtual ~Observer() = default;
    virtual void onUpdate(int value) = 0;
};

class Subject {
public:
    void attach(std::weak_ptr<Observer> obs) {
        observers_.push_back(obs);
    }
    
    void notify(int value) {
        // Clean up dead observers
        observers_.erase(
            std::remove_if(observers_.begin(), observers_.end(),
                [](const auto& w) { return w.expired(); }),
            observers_.end());
        
        for (auto& weak : observers_) {
            if (auto obs = weak.lock()) {
                obs->onUpdate(value);
            }
        }
    }
    
private:
    std::vector<std::weak_ptr<Observer>> observers_;
};
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| GUI | Model-View binding |
| Event systems | Event dispatchers |
| Reactive programming | Observable streams |
| Game engines | Entity component events |

---

## 6. Common Mistakes

### ❌ Observer Modifies Subject During Notification

```cpp
// BAD: Observer removes itself during notification
void Observer::onUpdate() {
    subject_.detach(this);  // Invalidates iteration!
}
// FIX: Defer modifications or copy observer list
```

### ❌ Memory Leaks with Raw Pointers

```cpp
// BAD: Subject holds raw pointer
std::vector<Observer*> observers_;
// Observer destroyed → dangling pointer
// FIX: Use weak_ptr
```

---

## 7. When NOT to Use

| Situation | Alternative |
|-----------|-------------|
| Simple callback | std::function |
| Compile-time binding | Template callback |

---

## 8. Mental Model Summary

**When Observer "Clicks":**

Use Observer when objects need to be notified of changes **without tight coupling**. In modern C++, use `std::function` callbacks with `weak_ptr` for safety.

---

## 中文说明

### 观察者模式要点

1. **现代实现**：
   - `std::function` 回调
   - `weak_ptr` 防止悬垂引用
   - 信号/槽机制

2. **常见错误**：
   - 通知期间修改观察者列表
   - 裸指针导致内存泄漏

3. **典型应用**：
   - GUI 数据绑定
   - 事件系统
   - 响应式编程

