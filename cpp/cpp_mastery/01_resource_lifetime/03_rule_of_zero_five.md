# Topic 3: Rule of Zero / Rule of Five

## 1. Problem Statement

### What real engineering problem does this solve?

C++ has five special member functions that control object lifecycle:

```
+------------------+----------------------------------------+
| Function         | When Called                            |
+------------------+----------------------------------------+
| Destructor       | Object dies (scope exit, delete)       |
| Copy Constructor | Initialize from another object         |
| Copy Assignment  | Assign from another object             |
| Move Constructor | Initialize from expiring object        |
| Move Assignment  | Assign from expiring object            |
+------------------+----------------------------------------+
```

The compiler generates defaults, but they may be wrong for resource-managing types.

### What goes wrong if this is ignored?

```cpp
// DISASTER: Default copy leads to double-free
class BadBuffer {
    int* data_;
    size_t size_;
public:
    BadBuffer(size_t n) : data_(new int[n]), size_(n) {}
    ~BadBuffer() { delete[] data_; }
    // Compiler-generated copy: just copies the pointer!
};

void crash() {
    BadBuffer b1(100);
    BadBuffer b2 = b1;  // b2.data_ == b1.data_
}   // BOOM: double delete

// DISASTER: Resource leak from missing destructor
class LeakyBuffer {
    int* data_;
public:
    LeakyBuffer(size_t n) : data_(new int[n]) {}
    // No destructor - memory leaked!
};
```

**中文说明：**
C++ 编译器会自动生成五个特殊成员函数，但对于管理资源的类型，默认行为通常是错误的。Rule of Zero 说：如果不直接管理资源，就不要写任何特殊成员函数。Rule of Five 说：如果必须写其中一个，就要考虑写全部五个。

---

## 2. Core Idea

### The Two Rules

```
+================================================================+
|                      RULE OF ZERO                               |
+================================================================+
| If your class doesn't directly manage resources:                |
|   - Write NONE of the five special members                      |
|   - Let compiler generate all of them                           |
|   - Use RAII members (unique_ptr, vector, string) instead       |
+================================================================+

+================================================================+
|                      RULE OF FIVE                               |
+================================================================+
| If your class DOES directly manage resources:                   |
|   - You probably need a custom destructor                       |
|   - If you write destructor, also consider:                     |
|     • Copy constructor                                          |
|     • Copy assignment                                           |
|     • Move constructor                                          |
|     • Move assignment                                           |
+================================================================+
```

### Why These Rules Work

```
CLASS WITHOUT RESOURCES              CLASS WITH RESOURCES
(Rule of Zero)                       (Rule of Five)

struct Person {                      class Buffer {
    std::string name;                    char* data_;
    int age;                             size_t size_;
};                                   public:
                                         ~Buffer();           // 1
// Compiler generates perfect           Buffer(const Buffer&); // 2
// versions of all five:                 Buffer& operator=(const Buffer&); // 3
// - Copy: deep copies string            Buffer(Buffer&&);    // 4
// - Move: moves string                  Buffer& operator=(Buffer&&); // 5
// - Destroy: destroys string        };
// No code needed!
```

**中文说明：**
Rule of Zero 的核心洞察是：如果你的类只包含遵守 RAII 的成员（如 string, vector, unique_ptr），那么编译器生成的默认函数会正确地复制、移动和销毁这些成员。只有当你直接持有原始资源（如 new 分配的内存、文件句柄）时，才需要自己写特殊成员函数。

---

## 3. Idiomatic C++ Techniques

### Rule of Zero Implementation

```cpp
// GOOD: Rely on RAII members
class Document {
    std::string title_;
    std::vector<std::string> paragraphs_;
    std::unique_ptr<Image> cover_;
    
public:
    Document(std::string title) : title_(std::move(title)) {}
    
    // NO destructor needed - members clean themselves
    // NO copy/move needed - compiler generates correct ones
    // (Note: unique_ptr makes this move-only automatically)
};
```

### Rule of Five Implementation

```cpp
class Buffer {
    char* data_;
    size_t size_;
    
public:
    // Constructor
    explicit Buffer(size_t size) 
        : data_(size ? new char[size] : nullptr)
        , size_(size) {}
    
    // 1. Destructor
    ~Buffer() {
        delete[] data_;
    }
    
    // 2. Copy Constructor
    Buffer(const Buffer& other)
        : data_(other.size_ ? new char[other.size_] : nullptr)
        , size_(other.size_)
    {
        std::copy(other.data_, other.data_ + size_, data_);
    }
    
    // 3. Copy Assignment (copy-and-swap idiom)
    Buffer& operator=(const Buffer& other) {
        Buffer temp(other);    // Copy
        swap(*this, temp);     // Swap
        return *this;          // Old data destroyed with temp
    }
    
    // 4. Move Constructor
    Buffer(Buffer&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    
    // 5. Move Assignment
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    
    // Friend swap for copy-and-swap idiom
    friend void swap(Buffer& a, Buffer& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }
    
    // Accessors
    char* data() { return data_; }
    const char* data() const { return data_; }
    size_t size() const { return size_; }
};
```

### Explicit Control Syntax

```cpp
class Widget {
public:
    // Explicitly defaulted - use compiler version
    Widget() = default;
    Widget(const Widget&) = default;
    Widget& operator=(const Widget&) = default;
    Widget(Widget&&) = default;
    Widget& operator=(Widget&&) = default;
    ~Widget() = default;
    
    // Explicitly deleted - cannot be called
    // Widget(const Widget&) = delete;
    // Widget& operator=(const Widget&) = delete;
};

// Move-only type (like unique_ptr)
class MoveOnly {
public:
    MoveOnly() = default;
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
    
    // Disable copying
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
};
```

---

## 4. Complete C++ Example

```cpp
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// RULE OF ZERO: Let RAII members do the work
// ============================================================
class Employee {
    std::string name_;
    std::string department_;
    std::vector<std::string> skills_;
    std::unique_ptr<std::string> bio_;  // Optional, move-only
    
public:
    Employee(std::string name, std::string dept)
        : name_(std::move(name))
        , department_(std::move(dept))
    {}
    
    void setBio(std::string bio) {
        bio_ = std::make_unique<std::string>(std::move(bio));
    }
    
    void addSkill(std::string skill) {
        skills_.push_back(std::move(skill));
    }
    
    void print() const {
        std::cout << name_ << " (" << department_ << ")\n";
        std::cout << "Skills: ";
        for (const auto& s : skills_) std::cout << s << " ";
        std::cout << "\n";
        if (bio_) std::cout << "Bio: " << *bio_ << "\n";
    }
    
    // NO special members needed!
    // Compiler generates:
    // - Destructor: destroys all members
    // - Move ctor/assign: moves all members (works with unique_ptr)
    // - Copy is implicitly deleted due to unique_ptr
};

// ============================================================
// RULE OF FIVE: Manage raw resource directly
// ============================================================
class DynamicArray {
    int* data_;
    size_t size_;
    size_t capacity_;
    
public:
    // Constructor
    explicit DynamicArray(size_t capacity = 0)
        : data_(capacity ? new int[capacity] : nullptr)
        , size_(0)
        , capacity_(capacity)
    {}
    
    // 1. Destructor
    ~DynamicArray() {
        delete[] data_;
    }
    
    // 2. Copy Constructor
    DynamicArray(const DynamicArray& other)
        : data_(other.capacity_ ? new int[other.capacity_] : nullptr)
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        std::copy(other.data_, other.data_ + size_, data_);
    }
    
    // 3. Copy Assignment (copy-and-swap)
    DynamicArray& operator=(DynamicArray other) {  // Note: by value!
        swap(*this, other);
        return *this;
    }
    
    // 4. Move Constructor
    DynamicArray(DynamicArray&& other) noexcept
        : data_(other.data_)
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    
    // 5. Move Assignment
    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }
    
    // Swap function (enables copy-and-swap)
    friend void swap(DynamicArray& a, DynamicArray& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
        swap(a.capacity_, b.capacity_);
    }
    
    // Interface
    void push_back(int value) {
        if (size_ >= capacity_) {
            size_t newCap = capacity_ ? capacity_ * 2 : 8;
            int* newData = new int[newCap];
            std::copy(data_, data_ + size_, newData);
            delete[] data_;
            data_ = newData;
            capacity_ = newCap;
        }
        data_[size_++] = value;
    }
    
    int& operator[](size_t i) { return data_[i]; }
    const int& operator[](size_t i) const { return data_[i]; }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
};

// ============================================================
// BETTER: Convert Rule of Five to Rule of Zero
// ============================================================
class BetterDynamicArray {
    std::unique_ptr<int[]> data_;  // RAII handles memory
    size_t size_;
    size_t capacity_;
    
public:
    explicit BetterDynamicArray(size_t capacity = 0)
        : data_(capacity ? std::make_unique<int[]>(capacity) : nullptr)
        , size_(0)
        , capacity_(capacity)
    {}
    
    // Move operations: compiler-generated are correct
    BetterDynamicArray(BetterDynamicArray&&) = default;
    BetterDynamicArray& operator=(BetterDynamicArray&&) = default;
    
    // Copy operations: must implement (unique_ptr can't copy)
    BetterDynamicArray(const BetterDynamicArray& other)
        : data_(other.capacity_ ? std::make_unique<int[]>(other.capacity_) : nullptr)
        , size_(other.size_)
        , capacity_(other.capacity_)
    {
        std::copy(other.data_.get(), other.data_.get() + size_, data_.get());
    }
    
    BetterDynamicArray& operator=(const BetterDynamicArray& other) {
        if (this != &other) {
            auto newData = other.capacity_ 
                ? std::make_unique<int[]>(other.capacity_) : nullptr;
            std::copy(other.data_.get(), other.data_.get() + other.size_, newData.get());
            data_ = std::move(newData);
            size_ = other.size_;
            capacity_ = other.capacity_;
        }
        return *this;
    }
    
    // Destructor: not needed! unique_ptr handles it
    // ~BetterDynamicArray() = default;  // implicit
    
    void push_back(int value) {
        if (size_ >= capacity_) {
            size_t newCap = capacity_ ? capacity_ * 2 : 8;
            auto newData = std::make_unique<int[]>(newCap);
            std::copy(data_.get(), data_.get() + size_, newData.get());
            data_ = std::move(newData);
            capacity_ = newCap;
        }
        data_[size_++] = value;
    }
    
    int& operator[](size_t i) { return data_[i]; }
    size_t size() const { return size_; }
};

// ============================================================
// Demonstration
// ============================================================
int main() {
    // Rule of Zero example
    std::cout << "=== Rule of Zero ===\n";
    Employee e1("Alice", "Engineering");
    e1.addSkill("C++");
    e1.addSkill("Python");
    e1.setBio("Senior engineer");
    
    Employee e2 = std::move(e1);  // Move works!
    e2.print();
    
    // Rule of Five example
    std::cout << "\n=== Rule of Five ===\n";
    DynamicArray arr1;
    arr1.push_back(1);
    arr1.push_back(2);
    arr1.push_back(3);
    
    DynamicArray arr2 = arr1;  // Copy
    arr2.push_back(4);
    
    DynamicArray arr3 = std::move(arr1);  // Move
    
    std::cout << "arr2 size: " << arr2.size() << "\n";  // 4
    std::cout << "arr3 size: " << arr3.size() << "\n";  // 3
    std::cout << "arr1 size: " << arr1.size() << "\n";  // 0 (moved-from)
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Forgetting one of the five

```cpp
class Broken {
    char* data_;
public:
    Broken(const char* s) : data_(strdup(s)) {}
    ~Broken() { free(data_); }
    // FORGOT copy ctor and copy assignment!
    // Compiler generates shallow copies -> double free
};
```

### Mistake 2: Non-noexcept move operations

```cpp
class BadMove {
public:
    // WRONG: Move can throw
    BadMove(BadMove&& other) {
        data_ = new char[other.size_];  // May throw!
        // ...
    }
};

// WHY IT MATTERS:
// vector::push_back uses move only if noexcept
// Otherwise it copies (for strong exception safety)
std::vector<BadMove> v;
v.push_back(BadMove{});  // Will COPY, not move!
```

### Mistake 3: Self-assignment bug

```cpp
class SelfAssignBug {
    char* data_;
public:
    SelfAssignBug& operator=(const SelfAssignBug& other) {
        delete[] data_;              // Deletes our data
        data_ = new char[other.size_]; // other.size_ is garbage!
        // ...
        return *this;
    }
};

SelfAssignBug x;
x = x;  // BOOM!

// FIX: Check for self-assignment or use copy-and-swap
```

---

## 6. When NOT to Use Rule of Zero

### When You Need Custom Behavior

| Scenario | Solution |
|----------|----------|
| Logging on destruction | Custom destructor only (Rule of One?) |
| Non-owning raw pointers | May need Rule of Zero (no cleanup needed) |
| Interfacing with C APIs | Often need Rule of Five |
| Performance-critical copy | Custom copy operations |

### The Rule of "Declare All or None"

```cpp
// If you declare ANY of the five, declare ALL explicitly
class Explicit {
public:
    Explicit() = default;
    ~Explicit() = default;  // Declared one...
    
    Explicit(const Explicit&) = default;      // ...so declare all
    Explicit& operator=(const Explicit&) = default;
    Explicit(Explicit&&) = default;
    Explicit& operator=(Explicit&&) = default;
};
```

**中文说明：**
何时不用 Rule of Zero：
1. **性能关键路径**：可能需要自定义的高效复制
2. **C 接口**：必须手动管理 C 库分配的资源
3. **观察者模式**：持有非拥有指针时可能不需要特殊处理
4. **部分自定义**：只需要添加日志或统计时可能只写析构函数

---

## Summary

```
+------------------------------------------------------------------+
|                    DECISION FLOWCHART                             |
+------------------------------------------------------------------+
|                                                                  |
|     Does your class directly manage resources?                   |
|     (new/delete, malloc/free, file handles, etc.)                |
|                        |                                         |
|               +--------+--------+                                |
|               |                 |                                |
|              YES               NO                                |
|               |                 |                                |
|               v                 v                                |
|     +------------------+    +------------------+                 |
|     | RULE OF FIVE     |    | RULE OF ZERO     |                 |
|     |------------------|    |------------------|                 |
|     | Write all five:  |    | Write none.      |                 |
|     | - Destructor     |    | Use RAII members:|                 |
|     | - Copy ctor      |    | - unique_ptr     |                 |
|     | - Copy assign    |    | - shared_ptr     |                 |
|     | - Move ctor      |    | - vector         |                 |
|     | - Move assign    |    | - string         |                 |
|     +------------------+    +------------------+                 |
|                                                                  |
|     PREFER RULE OF ZERO: Convert resources to RAII wrappers      |
|                                                                  |
+------------------------------------------------------------------+
```

