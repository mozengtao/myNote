# Topic 27: Expressing Ownership in Types

## 1. Problem Statement

### What real engineering problem does this solve?

In C and C++, pointer types don't express ownership:

```cpp
Widget* getWidget();      // Who owns the returned Widget?
void process(Widget* w);  // Does process() take ownership?
Widget* widgets_;         // Member variable - owning or observing?
```

This ambiguity causes:
- Memory leaks (everyone thinks someone else will delete)
- Double-frees (everyone deletes)
- Use-after-free (non-owner outlives owner)
- Endless documentation about "caller must delete"

```
RAW POINTER AMBIGUITY:
┌────────────────────────────────────────────────────────────────┐
│  T*  could mean:                                               │
│  • "I own this, you must delete it" (ownership transfer)       │
│  • "I own this, don't touch it" (lending)                      │
│  • "You own this, I'm just looking" (observing)                │
│  • "Here's an array" (container)                               │
│  • "Maybe null" (optional)                                     │
│  • "Definitely not null" (required reference)                  │
└────────────────────────────────────────────────────────────────┘
```

**中文说明：**
原始指针（T*）是 C++ 中最模糊的类型——它不表达所有权关系。谁应该释放内存？指针可以为空吗？这些问题只能通过文档和命名约定来回答，极易出错。现代 C++ 通过类型系统显式表达所有权，让编译器帮助检查。

---

## 2. Core Idea

### The Ownership Type System

```
TYPE                        MEANING
────────────────────────────────────────────────────────────────
std::unique_ptr<T>          "I own this exclusively. When I die, it dies."
std::shared_ptr<T>          "I share ownership. Last owner deletes."
std::weak_ptr<T>            "I observe shared ownership. May be dead."
T* / T&                     "I don't own this. Someone else does."
std::optional<T>            "I may or may not have a T value."
std::span<T>                "I'm a non-owning view of contiguous T's."
std::string_view            "I'm a non-owning view of characters."
T                           "I am the T. Value semantics."
```

### Visual Ownership Model

```
UNIQUE OWNERSHIP:
┌─────────────┐
│ unique_ptr  │───────►[ Widget ]
└─────────────┘         (dies with ptr)

SHARED OWNERSHIP:
┌─────────────┐
│ shared_ptr  │───┐
└─────────────┘   │
┌─────────────┐   ├────►[ Widget + RefCount ]
│ shared_ptr  │───┤     (dies when refcount = 0)
└─────────────┘   │
┌─────────────┐   │
│ shared_ptr  │───┘
└─────────────┘

NON-OWNING OBSERVATION:
┌─────────────┐
│ unique_ptr  │───────►[ Widget ]
└─────────────┘              ▲
                             │ (just looking)
┌─────────────┐              │
│  Widget*    │──────────────┘
└─────────────┘
(observer - must not outlive owner)
```

**中文说明：**
类型即文档：
- **unique_ptr<T>**：独占所有权，不可复制，只能移动
- **shared_ptr<T>**：共享所有权，引用计数，最后一个销毁时释放
- **T* / T&**：非拥有引用，只是观察，不负责生命周期
- **值类型 T**：没有间接，没有所有权问题

---

## 3. Idiomatic C++ Techniques

### Function Signatures That Express Ownership

```cpp
// Take exclusive ownership (sink)
void consume(std::unique_ptr<Widget> w);
// Caller must: consume(std::move(myWidget));
// Clear: ownership transferred to function

// Share ownership
void use(std::shared_ptr<Widget> w);
// Caller can: use(mySharedWidget);
// Clear: function may store, keeping Widget alive

// Just observe, don't own (most common)
void examine(Widget& w);              // Non-null reference
void examine(const Widget& w);        // Read-only
void examine(Widget* w);              // Nullable
void examine(std::span<Widget> ws);   // View of array

// Return ownership
std::unique_ptr<Widget> create();     // Caller owns result
std::shared_ptr<Widget> getShared();  // Caller shares result
Widget* find();                       // Non-owning, may be null
Widget& get();                        // Non-owning, never null
```

### Factory Pattern with Clear Ownership

```cpp
class WidgetFactory {
public:
    // Create and transfer ownership - unmistakable
    static std::unique_ptr<Widget> create() {
        return std::make_unique<Widget>();
    }
    
    // Factory with registration - shared ownership needed
    std::shared_ptr<Widget> createAndRegister() {
        auto widget = std::make_shared<Widget>();
        registry_.push_back(widget);
        return widget;  // Caller shares ownership with registry
    }
    
private:
    std::vector<std::shared_ptr<Widget>> registry_;
};
```

### Observer Pattern Without Ownership

```cpp
class Subject {
    std::vector<Observer*> observers_;  // Non-owning pointers
    
public:
    void addObserver(Observer* obs) {
        observers_.push_back(obs);
    }
    
    void removeObserver(Observer* obs) {
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), obs),
            observers_.end()
        );
    }
    
    void notify() {
        for (Observer* obs : observers_) {
            obs->update();
        }
    }
};

// Observers manage their own registration
class ConcreteObserver : public Observer {
    Subject* subject_;  // Non-owning
    
public:
    ConcreteObserver(Subject* s) : subject_(s) {
        subject_->addObserver(this);
    }
    
    ~ConcreteObserver() {
        subject_->removeObserver(this);
    }
};
```

---

## 4. Complete C++ Example

```cpp
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// ============================================================
// Domain types with clear ownership semantics
// ============================================================

class Resource {
    std::string name_;
public:
    explicit Resource(std::string name) : name_(std::move(name)) {
        std::cout << "  Resource '" << name_ << "' created\n";
    }
    ~Resource() {
        std::cout << "  Resource '" << name_ << "' destroyed\n";
    }
    const std::string& name() const { return name_; }
};

// ============================================================
// Ownership expressed in function signatures
// ============================================================

// 1. SINK: Takes exclusive ownership
void consume(std::unique_ptr<Resource> r) {
    std::cout << "Consuming " << r->name() << "\n";
    // r dies at end of function
}

// 2. SHARE: Participates in shared ownership
void share(std::shared_ptr<Resource> r) {
    std::cout << "Sharing " << r->name() << " (refcount: " 
              << r.use_count() << ")\n";
    // May store r, keeping Resource alive
}

// 3. OBSERVE: Non-owning reference
void observe(const Resource& r) {
    std::cout << "Observing " << r.name() << "\n";
    // Must not store &r or assume it lives beyond this call
}

// 4. OPTIONAL OBSERVE: Nullable non-owning
void maybeObserve(const Resource* r) {
    if (r) {
        std::cout << "Observing " << r->name() << "\n";
    } else {
        std::cout << "Nothing to observe\n";
    }
}

// 5. FACTORY: Creates and transfers ownership
std::unique_ptr<Resource> createResource(std::string name) {
    return std::make_unique<Resource>(std::move(name));
}

// 6. SHARED FACTORY: Creates with shared ownership
std::shared_ptr<Resource> createShared(std::string name) {
    return std::make_shared<Resource>(std::move(name));
}

// 7. CONTAINER VIEW: Non-owning view of range
void processAll(std::span<const std::unique_ptr<Resource>> resources) {
    std::cout << "Processing " << resources.size() << " resources:\n";
    for (const auto& r : resources) {
        std::cout << "  - " << r->name() << "\n";
    }
}

// ============================================================
// Class with clear member ownership
// ============================================================

class ResourceManager {
    // Owned resources
    std::vector<std::unique_ptr<Resource>> ownedResources_;
    
    // Shared resources (participates in ownership)
    std::vector<std::shared_ptr<Resource>> sharedResources_;
    
    // Observed resources (non-owning)
    std::vector<Resource*> observedResources_;
    
    // Optional owned resource
    std::optional<std::unique_ptr<Resource>> cachedResource_;
    
public:
    // Add and take ownership
    void addOwned(std::unique_ptr<Resource> r) {
        std::cout << "Taking ownership of " << r->name() << "\n";
        ownedResources_.push_back(std::move(r));
    }
    
    // Add and share ownership
    void addShared(std::shared_ptr<Resource> r) {
        std::cout << "Sharing ownership of " << r->name() << "\n";
        sharedResources_.push_back(std::move(r));
    }
    
    // Add observation (caller retains ownership)
    void observe(Resource& r) {
        std::cout << "Observing " << r.name() << "\n";
        observedResources_.push_back(&r);
    }
    
    // Return non-owning pointer (may be null)
    Resource* findOwned(std::string_view name) {
        for (auto& r : ownedResources_) {
            if (r->name() == name) return r.get();
        }
        return nullptr;
    }
    
    // Return non-owning reference (must exist)
    Resource& getOwned(size_t index) {
        return *ownedResources_.at(index);
    }
    
    // Transfer ownership out
    std::unique_ptr<Resource> releaseOwned(size_t index) {
        auto it = ownedResources_.begin() + index;
        auto r = std::move(*it);
        ownedResources_.erase(it);
        return r;
    }
    
    // View all owned
    std::span<const std::unique_ptr<Resource>> allOwned() const {
        return ownedResources_;
    }
    
    void printStatus() const {
        std::cout << "Manager status:\n";
        std::cout << "  Owned: " << ownedResources_.size() << "\n";
        std::cout << "  Shared: " << sharedResources_.size() << "\n";
        std::cout << "  Observed: " << observedResources_.size() << "\n";
    }
};

// ============================================================
// Demonstrating ownership patterns
// ============================================================

int main() {
    std::cout << "=== Sink Pattern (transfer ownership) ===\n";
    {
        auto r = createResource("Sinkable");
        consume(std::move(r));  // Explicit transfer
        // r is now null - ownership transferred
    }
    
    std::cout << "\n=== Shared Pattern ===\n";
    {
        auto shared = createShared("Shareable");
        std::cout << "Before share, refcount: " << shared.use_count() << "\n";
        share(shared);  // Copy shared_ptr, bump refcount
        std::cout << "After share, refcount: " << shared.use_count() << "\n";
    }
    
    std::cout << "\n=== Observe Pattern ===\n";
    {
        auto r = createResource("Observable");
        observe(*r);  // Non-owning reference
        maybeObserve(r.get());  // Nullable observe
        maybeObserve(nullptr);
    }
    
    std::cout << "\n=== Manager Pattern ===\n";
    {
        ResourceManager mgr;
        
        // Transfer ownership
        mgr.addOwned(createResource("Managed1"));
        mgr.addOwned(createResource("Managed2"));
        
        // View without owning
        processAll(mgr.allOwned());
        
        // Get non-owning reference
        Resource& ref = mgr.getOwned(0);
        std::cout << "Got reference to: " << ref.name() << "\n";
        
        // Release ownership
        auto released = mgr.releaseOwned(0);
        std::cout << "Released: " << released->name() << "\n";
        
        mgr.printStatus();
    }
    
    std::cout << "\n=== All resources should be destroyed above ===\n";
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: shared_ptr when unique_ptr suffices

```cpp
// WRONG: Unnecessary overhead
class Node {
    std::shared_ptr<Node> children_[2];  // 48 bytes!
};

// RIGHT: Clear single ownership
class Node {
    std::unique_ptr<Node> children_[2];  // 16 bytes
    Node* parent_;                        // Non-owning back-pointer
};
```

### Mistake 2: Returning non-owning pointer to owned object

```cpp
class Container {
    std::vector<std::unique_ptr<Widget>> widgets_;
    
public:
    // DANGEROUS: What if vector reallocates?
    Widget* add(std::unique_ptr<Widget> w) {
        widgets_.push_back(std::move(w));
        return widgets_.back().get();  // May be invalidated!
    }
    
    // SAFER: Return reference to unique_ptr, or index
    const std::unique_ptr<Widget>& add(std::unique_ptr<Widget> w) {
        widgets_.push_back(std::move(w));
        return widgets_.back();
    }
};
```

### Mistake 3: Confusing weak_ptr lifetime

```cpp
void wrong() {
    std::weak_ptr<Widget> weak;
    {
        auto shared = std::make_shared<Widget>();
        weak = shared;
    }  // shared dies, Widget destroyed
    
    auto ptr = weak.lock();  // Returns nullptr!
    if (ptr) {
        // This code never runs
    }
}
```

---

## 6. When NOT to Use Smart Pointers

### Raw Pointers Are Still Appropriate For:

| Scenario | Why Raw Pointer |
|----------|-----------------|
| Non-owning function parameters | No ownership transfer |
| Iterators into containers | Container owns elements |
| Optional non-owning reference | `T*` can be null |
| Interfacing with C APIs | C doesn't have smart pointers |
| Performance-critical hot paths | Measure first! |

### The Non-Owning Pointer Convention

```cpp
// Modern C++ convention:
// T* = non-owning, possibly null
// T& = non-owning, never null
// unique_ptr<T> = owning, exclusive
// shared_ptr<T> = owning, shared

// If you follow this, T* is unambiguous:
// "This is NOT an owning pointer"
void process(Widget* w);  // Clearly non-owning
void consume(std::unique_ptr<Widget> w);  // Clearly owning
```

**中文说明：**
原始指针并非禁用——在现代 C++ 中，它们有明确的语义："非拥有指针"。配合 unique_ptr 和 shared_ptr 的使用，原始指针的含义变得清晰：它只是观察，不负责生命周期。关键是团队要遵循统一约定。

---

## Summary

```
+------------------------------------------------------------------+
|                  OWNERSHIP TYPE CHEAT SHEET                       |
+------------------------------------------------------------------+
|                                                                  |
|  PARAMETER TYPES:                                                |
|  ────────────────────────────────────────────────────────────────|
|  void f(T);                  // Copy (small T) or move           |
|  void f(const T&);           // Read-only access                 |
|  void f(T&);                 // Read-write access                |
|  void f(T*);                 // Optional non-owning access       |
|  void f(unique_ptr<T>);      // Take ownership (sink)            |
|  void f(shared_ptr<T>);      // Share ownership                  |
|  void f(span<T>);            // View into array                  |
|                                                                  |
|  RETURN TYPES:                                                   |
|  ────────────────────────────────────────────────────────────────|
|  T f();                      // Return value                     |
|  T& f();                     // Return reference (must exist)    |
|  T* f();                     // Return optional non-owning ptr   |
|  unique_ptr<T> f();          // Return owned object              |
|  shared_ptr<T> f();          // Return shared ownership          |
|  optional<T> f();            // Return maybe-value               |
|                                                                  |
|  MEMBER TYPES:                                                   |
|  ────────────────────────────────────────────────────────────────|
|  unique_ptr<T> member_;      // Owns T exclusively               |
|  shared_ptr<T> member_;      // Shares T with others             |
|  T* member_;                 // Observes T (non-owning)          |
|  T member_;                  // Contains T (value)               |
|                                                                  |
+------------------------------------------------------------------+
```

