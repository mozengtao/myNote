# Topic 2: Value Semantics vs Reference Semantics

## 1. Problem Statement

### What real engineering problem does this solve?

When designing types and APIs, you must decide how objects relate to each other:

```
VALUE SEMANTICS                      REFERENCE SEMANTICS
+--------+    +--------+             +--------+
|   A    |    |   B    |             |   A    |----+
| data=5 |    | data=5 |             | ptr    |    |
+--------+    +--------+             +--------+    |
 (independent copies)                              v
                                     +--------+    +--------+
                                     |   B    |--->| Shared |
                                     | ptr    |    | data=5 |
                                     +--------+    +--------+
                                      (aliased, shared state)
```

**The core question:** Does `B = A` create an independent copy or a shared reference?

### What goes wrong if this choice is unclear?

```cpp
// CONFUSION: Which semantics?
void process(Widget w);      // Value - receives copy
void process(Widget& w);     // Reference - modifies original
void process(Widget* w);     // Pointer - might alias, might be null

// BUG: Unintended sharing
std::vector<std::string*> names;  // Reference semantics
names.push_back(&localString);     // Dangling after scope exit!

// BUG: Unintended copying
void expensive(std::vector<int> v);  // Copies entire vector!
```

**中文说明：**
C++ 同时支持值语义和引用语义，这是其强大但也容易出错的来源。值语义（复制产生独立对象）提供安全性和简单性，引用语义（共享底层数据）提供效率。选择错误会导致：
- 意外的共享和竞态条件
- 悬挂引用和使用后释放
- 不必要的复制和性能问题

---

## 2. Core Idea

### Value Semantics

Objects are **independent values** that behave like integers:

```cpp
int a = 5;
int b = a;    // b is an independent copy
b = 10;       // a is still 5

std::string s1 = "hello";
std::string s2 = s1;  // s2 is an independent copy
s2 += " world";       // s1 is still "hello"
```

**Properties:**
- Copy creates independent object
- No aliasing between copies
- Safe to reason about in isolation
- Natural in multithreaded code

### Reference Semantics

Objects are **handles to shared state**:

```cpp
int* a = new int(5);
int* b = a;   // b points to same int
*b = 10;      // *a is now also 10

std::shared_ptr<Data> p1 = std::make_shared<Data>();
std::shared_ptr<Data> p2 = p1;  // Same underlying Data
p2->modify();                    // p1 sees the change
```

**Properties:**
- Copy creates another handle to same data
- Aliasing is the norm
- Must track lifetime carefully
- Requires synchronization in multithreaded code

### C++ Default Behavior

```
+-------------------+------------------------+
| Type              | Default Semantics      |
+-------------------+------------------------+
| Primitive (int)   | Value                  |
| Class/Struct      | Value (copy ctor)      |
| Raw pointer       | Reference (shallow)    |
| Reference (&)     | Reference (alias)      |
| std::shared_ptr   | Reference (shared)     |
| std::unique_ptr   | Value (move-only)      |
| STL containers    | Value (deep copy)      |
+-------------------+------------------------+
```

**中文说明：**
值语义的核心是"复制即独立"：复制后的对象与原对象完全无关。这符合数学中"值"的概念——5 就是 5，无论存储在哪里。引用语义则是"复制即共享"：多个句柄指向同一数据。C++ 的强大在于让程序员选择适合场景的语义。

---

## 3. Idiomatic C++ Techniques

### Achieving Value Semantics

```cpp
// 1. Use value types directly
std::string name;           // String has value semantics
std::vector<int> data;      // Vector has value semantics

// 2. Pass by value when you need a copy anyway
void process(std::string s) {  // Caller decides: copy or move
    // s is our own copy
}

// 3. Return by value (RVO/NRVO optimizes this)
std::vector<int> compute() {
    std::vector<int> result;
    // ... fill result ...
    return result;  // Moved or elided, never copied
}

// 4. Copy-on-write for large objects (advanced)
class Document {
    std::shared_ptr<const Data> data_;
public:
    void modify() {
        if (!data_.unique()) {
            data_ = std::make_shared<Data>(*data_);  // Copy now
        }
        // Now safe to modify
    }
};
```

### Achieving Reference Semantics

```cpp
// 1. Use references for non-owning access
void print(const Widget& w);   // Read-only access
void modify(Widget& w);        // Read-write access

// 2. Use smart pointers for shared ownership
std::shared_ptr<Resource> resource;

// 3. Use raw pointers for non-owning observers
class Observer {
    Widget* target_;  // Does not own, just observes
};

// 4. Use reference_wrapper for rebindable references
std::vector<std::reference_wrapper<Widget>> refs;
```

---

## 4. Complete C++ Example

```cpp
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ============================================================
// Value Semantics Example: A 2D Point
// ============================================================
class Point {
    double x_, y_;
    
public:
    Point(double x = 0, double y = 0) : x_(x), y_(y) {}
    
    // Value semantics: default copy is perfect
    // Point(const Point&) = default;
    // Point& operator=(const Point&) = default;
    
    double x() const { return x_; }
    double y() const { return y_; }
    
    Point operator+(const Point& other) const {
        return Point(x_ + other.x_, y_ + other.y_);
    }
    
    bool operator==(const Point& other) const {
        return x_ == other.x_ && y_ == other.y_;
    }
};

void demonstrateValueSemantics() {
    Point p1(3, 4);
    Point p2 = p1;      // p2 is independent copy
    
    Point p3 = p1 + p2; // Creates new Point, no aliasing
    
    std::vector<Point> points;
    points.push_back(p1);  // Copy into vector
    points[0] = Point(0, 0);  // p1 unchanged!
    
    std::cout << "p1: (" << p1.x() << ", " << p1.y() << ")\n";
    // Output: p1: (3, 4) - unaffected by vector modification
}

// ============================================================
// Reference Semantics Example: Shared Document
// ============================================================
class DocumentContent {
    std::string text_;
public:
    explicit DocumentContent(std::string text) : text_(std::move(text)) {}
    
    const std::string& text() const { return text_; }
    void append(const std::string& s) { text_ += s; }
};

class DocumentHandle {
    std::shared_ptr<DocumentContent> content_;
    
public:
    explicit DocumentHandle(std::string text)
        : content_(std::make_shared<DocumentContent>(std::move(text))) {}
    
    // Reference semantics: copies share content
    // DocumentHandle(const DocumentHandle&) = default;
    
    const std::string& text() const { return content_->text(); }
    void append(const std::string& s) { content_->append(s); }
    
    // Check if we share with others
    bool isShared() const { return content_.use_count() > 1; }
};

void demonstrateReferenceSemantics() {
    DocumentHandle doc1("Hello");
    DocumentHandle doc2 = doc1;  // Shares same content!
    
    std::cout << "Before: doc1='" << doc1.text() << "', doc2='" << doc2.text() << "'\n";
    
    doc2.append(" World");  // Modifies shared content
    
    std::cout << "After:  doc1='" << doc1.text() << "', doc2='" << doc2.text() << "'\n";
    // Output: Both show "Hello World"
}

// ============================================================
// Hybrid: Value Semantics with Efficient Copying
// ============================================================
class Image {
    struct Data {
        std::vector<uint8_t> pixels;
        int width, height;
    };
    std::shared_ptr<const Data> data_;  // Immutable shared data
    
    void ensureUnique() {
        if (!data_.unique()) {
            data_ = std::make_shared<Data>(*data_);
        }
    }
    
public:
    Image(int w, int h) 
        : data_(std::make_shared<Data>(Data{{}, w, h})) 
    {
        const_cast<Data*>(data_.get())->pixels.resize(w * h);
    }
    
    // Copy is O(1) - just share the pointer
    Image(const Image&) = default;
    Image& operator=(const Image&) = default;
    
    // But modification triggers copy-on-write
    void setPixel(int x, int y, uint8_t value) {
        // This is where value semantics is enforced
        if (data_.use_count() > 1) {
            // Someone else has a reference - copy first
            data_ = std::make_shared<Data>(*data_);
        }
        const_cast<Data*>(data_.get())->pixels[y * data_->width + x] = value;
    }
    
    uint8_t getPixel(int x, int y) const {
        return data_->pixels[y * data_->width + x];
    }
    
    bool isShared() const { return data_.use_count() > 1; }
};

void demonstrateCopyOnWrite() {
    Image img1(100, 100);
    img1.setPixel(0, 0, 255);
    
    Image img2 = img1;  // O(1) copy - shares data
    std::cout << "After copy, shared: " << img2.isShared() << "\n";
    
    img2.setPixel(1, 1, 128);  // Triggers actual copy
    std::cout << "After modify, shared: " << img2.isShared() << "\n";
    
    // img1 and img2 are now independent (value semantics preserved)
    std::cout << "img1[0,0]=" << (int)img1.getPixel(0, 0) << "\n";
    std::cout << "img2[0,0]=" << (int)img2.getPixel(0, 0) << "\n";
}

// ============================================================
// When to use each in APIs
// ============================================================

// Value: Small, cheap to copy, logically independent
void movePoint(Point p, double dx, double dy);

// Const reference: Read-only access, avoid copy
void printDocument(const DocumentHandle& doc);

// Non-const reference: Modify caller's object
void appendToDocument(DocumentHandle& doc, const std::string& text);

// Pointer: Optional (nullable) or observer
void setCallback(void (*callback)(int));

// unique_ptr: Transfer ownership
void takeOwnership(std::unique_ptr<Resource> r);

// shared_ptr: Shared ownership
void shareResource(std::shared_ptr<Resource> r);

int main() {
    std::cout << "=== Value Semantics ===\n";
    demonstrateValueSemantics();
    
    std::cout << "\n=== Reference Semantics ===\n";
    demonstrateReferenceSemantics();
    
    std::cout << "\n=== Copy-on-Write ===\n";
    demonstrateCopyOnWrite();
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Accidental slicing

```cpp
class Base { int x; };
class Derived : public Base { int y; };

void bug() {
    Derived d;
    Base b = d;  // SLICING: y is lost!
    
    std::vector<Base> items;
    items.push_back(Derived{});  // Sliced!
}

// FIX: Use pointers/references for polymorphism
std::vector<std::unique_ptr<Base>> items;
items.push_back(std::make_unique<Derived>());
```

### Mistake 2: Reference to temporary

```cpp
const std::string& getName() {
    return "temporary";  // BUG: Dangling reference!
}

// FIX: Return by value
std::string getName() {
    return "temporary";  // Caller receives valid object
}
```

### Mistake 3: Unexpected aliasing

```cpp
void process(std::vector<int>& v, const int& elem) {
    v.push_back(elem);  // BUG if elem is reference to v's element!
    // push_back may reallocate, invalidating elem
}

std::vector<int> v = {1, 2, 3};
process(v, v[0]);  // Undefined behavior!

// FIX: Take by value
void process(std::vector<int>& v, int elem) {
    v.push_back(elem);  // elem is a copy, safe
}
```

---

## 6. When NOT to Use Value Semantics

### When Reference Semantics is Better

| Scenario | Reason |
|----------|--------|
| Large objects frequently copied | Performance |
| Identity matters (not just value) | Entity vs Value distinction |
| Polymorphic hierarchies | Slicing prevention |
| Observable/mutable shared state | Intentional aliasing |
| Interfacing with C/external code | Pointers expected |

### Decision Guide

```
                     Is the object small?
                      (< 2-3 pointers)
                           /    \
                         YES     NO
                         /        \
                    Value       Is copying common?
                   Semantics      /        \
                               YES          NO
                               /             \
                          Consider         Value + Move
                          Reference        Semantics
                          Semantics
```

**中文说明：**
选择原则：
1. **小对象 + 简单复制 → 值语义**（如 Point, Date, Color）
2. **大对象 + 频繁复制 → 引用语义或 Copy-on-Write**
3. **多态层次 → 指针 + 引用语义**（避免切片）
4. **需要共享状态 → shared_ptr（显式引用语义）**
5. **独占所有权 + 转移 → unique_ptr（值语义 + 移动）**

---

## Summary

```
+------------------------------------------------------------------+
|              VALUE VS REFERENCE: DECISION MATRIX                  |
+------------------------------------------------------------------+
|                                                                  |
|  VALUE SEMANTICS          │  REFERENCE SEMANTICS                 |
|  ─────────────────────────│──────────────────────────────────────|
|  Copy = Independent       │  Copy = Shared handle                |
|  No aliasing worries      │  Must track aliasing                 |
|  Thread-safe by default   │  Needs synchronization               |
|  Simple reasoning         │  Complex lifetime                    |
|  May be expensive         │  Cheap copy, complex ownership       |
|                           │                                      |
|  USE FOR:                 │  USE FOR:                            |
|  - Small data types       │  - Large shared objects              |
|  - Immutable objects      │  - Polymorphic hierarchies           |
|  - Function returns       │  - Observer patterns                 |
|  - STL containers         │  - Caches, pools                     |
|                                                                  |
+------------------------------------------------------------------+
```

