# Topic 4: Move Semantics and Ownership Transfer

## 1. Problem Statement

### What real engineering problem does this solve?

Before C++11, returning large objects was expensive:

```
COPY SEMANTICS (C++98)                 MOVE SEMANTICS (C++11)
+----------------+                     +----------------+
| createVector() |                     | createVector() |
|   vector v     |                     |   vector v     |
|   [data ptr]---+-->[ 1 2 3 4 5 ]    |   [data ptr]---+-->[ 1 2 3 4 5 ]
+----------------+                     +----------------+
        |                                      |
        | return v;                            | return std::move(v);
        v                                      v
+----------------+                     +----------------+
|   temp copy    |                     |   result       |
|   [data ptr]---+-->[ 1 2 3 4 5 ]    |   [data ptr]---+
+----------------+    (COPIED!)        +----------------+  |
        |                                      |          |
        v                                      v          |
+----------------+                             (pointer   |
|   result       |                              STOLEN!)  |
|   [data ptr]---+-->[ 1 2 3 4 5 ]                       |
+----------------+    (COPIED AGAIN!)                    |
                                               +---------+
                                               |
                                               v
                                          [ 1 2 3 4 5 ]
                                          (Zero copies!)
```

### What goes wrong without move semantics?

```cpp
// Pre-C++11: Expensive temporary copies
std::vector<int> createLargeVector() {
    std::vector<int> v(1000000);
    // fill v...
    return v;  // Copy all 1 million elements!
}

// Factory pattern was painful
std::unique_ptr<Widget> createWidget();  // Impossible before C++11!
// Had to use raw pointers or output parameters

// Container operations were slow
std::vector<std::string> names;
std::string name = "very long string...";
names.push_back(name);  // Must copy the string
```

**中文说明：**
C++98 的问题是临时对象的复制开销。当函数返回大对象时，可能发生多次深拷贝。Move 语义通过"转移"资源所有权而非复制来解决这个问题——旧对象的内部指针被"偷走"给新对象，无需复制底层数据。

---

## 2. Core Idea

### Lvalues vs Rvalues

```
LVALUE                               RVALUE
(has identity, persists)             (temporary, expiring)

int x = 10;       // x is lvalue    10 is rvalue
std::string s;    // s is lvalue    std::string("hi") is rvalue
                                     s + " world" is rvalue
                                     std::move(s) is rvalue

Can take address: &x is valid       Cannot take address: &10 is error
Lives beyond expression             Dies at end of expression
```

### The Move Operation

```cpp
// A move transfers resources, leaving source in valid-but-unspecified state
std::string a = "hello";
std::string b = std::move(a);  // Move from a to b

// After move:
// b == "hello"           (owns the data)
// a == ???               (valid but unspecified - probably empty)
// a.size() is safe       (still a valid string object)
```

### Move vs Copy

```
COPY                                 MOVE
+--------+     +--------+           +--------+     +--------+
| source |     |  dest  |           | source |     |  dest  |
|  ptr---+-->  |  ptr---+-->        |  ptr---+     |  ptr   |
+--------+   X +--------+   X       +--------+  \  +--------+
            /               /           |       \      |
           /               /            v        \     v
      [data]          [data]          [data]      -->[data]
      (allocate)      (copy!)         (no alloc)   (pointer
                                                    stolen!)
```

**中文说明：**
Move 的核心是区分"需要保留的对象"（左值）和"即将消亡的对象"（右值）。对于右值，我们可以安全地"窃取"其资源，因为它马上就会被销毁。`std::move` 本身不移动任何东西——它只是把左值转换为右值引用，表示"我不再需要这个对象的内容了"。

---

## 3. Idiomatic C++ Techniques

### Rvalue References

```cpp
void process(Widget& w);       // Lvalue reference: binds to lvalues
void process(Widget&& w);      // Rvalue reference: binds to rvalues
void process(const Widget& w); // Const lvalue ref: binds to both

Widget w;
process(w);              // Calls Widget&
process(Widget{});       // Calls Widget&&
process(std::move(w));   // Calls Widget&& (w is now "moved-from")
```

### Universal (Forwarding) References

```cpp
template<typename T>
void forward(T&& arg) {  // Not rvalue ref! It's a forwarding ref
    // T&& + template = forwarding reference
    // Can bind to lvalues AND rvalues
    actual_function(std::forward<T>(arg));  // Perfect forwarding
}

Widget w;
forward(w);              // T = Widget&, arg is lvalue ref
forward(Widget{});       // T = Widget, arg is rvalue ref
forward(std::move(w));   // T = Widget, arg is rvalue ref
```

### STL Move Support

```cpp
// Containers support move
std::vector<std::string> v1 = {"a", "b", "c"};
std::vector<std::string> v2 = std::move(v1);  // O(1) move

// Algorithms support move
std::vector<Widget> source, dest;
std::move(source.begin(), source.end(), std::back_inserter(dest));

// Smart pointers are move-only
std::unique_ptr<Widget> p1 = std::make_unique<Widget>();
std::unique_ptr<Widget> p2 = std::move(p1);  // p1 is now null

// emplace avoids move/copy entirely
std::vector<std::pair<std::string, int>> v;
v.emplace_back("key", 42);  // Constructs in-place
```

---

## 4. Complete C++ Example

```cpp
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ============================================================
// A class with proper move semantics
// ============================================================
class Buffer {
    size_t size_;
    std::unique_ptr<char[]> data_;
    
    static int copyCount_;
    static int moveCount_;
    
public:
    explicit Buffer(size_t size) 
        : size_(size)
        , data_(std::make_unique<char[]>(size))
    {
        std::cout << "  Buffer(" << size << ") constructed\n";
    }
    
    // Copy constructor - expensive
    Buffer(const Buffer& other)
        : size_(other.size_)
        , data_(std::make_unique<char[]>(other.size_))
    {
        std::copy(other.data_.get(), other.data_.get() + size_, data_.get());
        ++copyCount_;
        std::cout << "  Buffer COPIED (expensive!)\n";
    }
    
    // Move constructor - cheap
    Buffer(Buffer&& other) noexcept
        : size_(other.size_)
        , data_(std::move(other.data_))  // Steal the unique_ptr
    {
        other.size_ = 0;
        ++moveCount_;
        std::cout << "  Buffer MOVED (cheap!)\n";
    }
    
    // Copy assignment
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            size_ = other.size_;
            data_ = std::make_unique<char[]>(other.size_);
            std::copy(other.data_.get(), other.data_.get() + size_, data_.get());
            ++copyCount_;
            std::cout << "  Buffer copy-assigned\n";
        }
        return *this;
    }
    
    // Move assignment
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            size_ = other.size_;
            data_ = std::move(other.data_);
            other.size_ = 0;
            ++moveCount_;
            std::cout << "  Buffer move-assigned\n";
        }
        return *this;
    }
    
    size_t size() const { return size_; }
    char* data() { return data_.get(); }
    
    static void printStats() {
        std::cout << "Copies: " << copyCount_ << ", Moves: " << moveCount_ << "\n";
    }
    
    static void resetStats() { copyCount_ = moveCount_ = 0; }
};

int Buffer::copyCount_ = 0;
int Buffer::moveCount_ = 0;

// ============================================================
// Factory function - return by value triggers move
// ============================================================
Buffer createBuffer(size_t size) {
    Buffer b(size);
    // Fill buffer...
    return b;  // Move (or copy elision)
}

// ============================================================
// Sink function - take ownership via move
// ============================================================
void consumeBuffer(Buffer b) {
    // b owns the buffer now
    std::cout << "  Consuming buffer of size " << b.size() << "\n";
}   // b destroyed here

// ============================================================
// Perfect forwarding example
// ============================================================
class Widget {
public:
    Widget() { std::cout << "  Widget default constructed\n"; }
    Widget(const Widget&) { std::cout << "  Widget COPIED\n"; }
    Widget(Widget&&) noexcept { std::cout << "  Widget MOVED\n"; }
};

template<typename T>
void wrapper(T&& arg) {
    // Without forward: always calls lvalue overload
    // process(arg);  // arg is lvalue (it has a name!)
    
    // With forward: preserves value category
    process(std::forward<T>(arg));
}

void process(Widget& w) { std::cout << "  process(Widget&)\n"; }
void process(Widget&& w) { std::cout << "  process(Widget&&)\n"; }

// ============================================================
// Demonstration
// ============================================================
int main() {
    std::cout << "=== Factory Pattern ===\n";
    Buffer::resetStats();
    Buffer b1 = createBuffer(1000);  // Move (or elided)
    Buffer::printStats();
    
    std::cout << "\n=== Explicit Move ===\n";
    Buffer::resetStats();
    Buffer b2 = std::move(b1);  // Explicit move
    std::cout << "b1 size after move: " << b1.size() << "\n";  // 0
    std::cout << "b2 size after move: " << b2.size() << "\n";  // 1000
    Buffer::printStats();
    
    std::cout << "\n=== Sink Pattern ===\n";
    Buffer::resetStats();
    Buffer b3(500);
    consumeBuffer(std::move(b3));  // Transfer ownership
    std::cout << "b3 size after consumption: " << b3.size() << "\n";  // 0
    Buffer::printStats();
    
    std::cout << "\n=== Vector Operations ===\n";
    Buffer::resetStats();
    std::vector<Buffer> vec;
    vec.reserve(3);  // Prevent reallocation moves
    
    std::cout << "push_back with copy:\n";
    Buffer b4(100);
    vec.push_back(b4);  // Copy (b4 still valid)
    
    std::cout << "push_back with move:\n";
    vec.push_back(std::move(b4));  // Move (b4 now empty)
    
    std::cout << "emplace_back:\n";
    vec.emplace_back(200);  // Construct in place (no copy/move)
    
    Buffer::printStats();
    
    std::cout << "\n=== Perfect Forwarding ===\n";
    Widget w;
    std::cout << "Passing lvalue:\n";
    wrapper(w);
    std::cout << "Passing rvalue:\n";
    wrapper(Widget{});
    
    return 0;
}
```

**Output:**
```
=== Factory Pattern ===
  Buffer(1000) constructed
  Buffer MOVED (cheap!)
Copies: 0, Moves: 1

=== Explicit Move ===
  Buffer MOVED (cheap!)
b1 size after move: 0
b2 size after move: 1000
Copies: 0, Moves: 1

=== Sink Pattern ===
  Buffer(500) constructed
  Buffer MOVED (cheap!)
  Consuming buffer of size 500
b3 size after consumption: 0
Copies: 0, Moves: 1

=== Vector Operations ===
push_back with copy:
  Buffer(100) constructed
  Buffer COPIED (expensive!)
push_back with move:
  Buffer MOVED (cheap!)
emplace_back:
  Buffer(200) constructed
Copies: 1, Moves: 1

=== Perfect Forwarding ===
  Widget default constructed
Passing lvalue:
  process(Widget&)
Passing rvalue:
  Widget default constructed
  process(Widget&&)
```

---

## 5. Failure Modes

### Mistake 1: Using object after move

```cpp
std::string s = "hello";
std::vector<std::string> v;
v.push_back(std::move(s));

std::cout << s.size();   // OK: valid operation
std::cout << s[0];       // DANGEROUS: s might be empty
s = "new value";         // OK: can reassign
```

### Mistake 2: Forgetting noexcept on move operations

```cpp
class BadMove {
public:
    BadMove(BadMove&& other) {  // No noexcept!
        // ...
    }
};

std::vector<BadMove> v;
v.reserve(10);
v.push_back(BadMove{});
// Vector may COPY instead of MOVE for exception safety!
```

### Mistake 3: std::move on const objects

```cpp
const std::string s = "hello";
std::string t = std::move(s);  // Compiles but COPIES!
// std::move(s) returns const std::string&&
// This matches copy ctor better than move ctor
```

### Mistake 4: Returning std::move from function

```cpp
std::string bad() {
    std::string s = "hello";
    return std::move(s);  // WRONG: prevents copy elision!
}

std::string good() {
    std::string s = "hello";
    return s;  // RIGHT: allows NRVO (no copy, no move)
}
```

---

## 6. When NOT to Use Move

### When Move Doesn't Help

| Situation | Why Move Doesn't Help |
|-----------|----------------------|
| Small objects | Move is same cost as copy (just copying a few bytes) |
| Objects without resources | Nothing to steal (e.g., Point{x, y}) |
| Const objects | Cannot move from const |
| Need original to remain valid | Copy is required |

### Performance Anti-Pattern

```cpp
// DON'T: Unnecessary move
int x = 5;
int y = std::move(x);  // Pointless: int has no resources to move

// DON'T: Pessimizing return
Widget createWidget() {
    Widget w;
    return std::move(w);  // Inhibits RVO! Just return w;
}

// DON'T: Move from return value
Widget w = std::move(createWidget());  // Pointless: already rvalue
```

**中文说明：**
不应使用 move 的情况：
1. **小对象**：如 int、Point，没有堆资源可窃取
2. **const 对象**：无法移动，会退化为复制
3. **函数返回局部变量**：直接 return 让编译器做 RVO/NRVO
4. **需要保留原对象**：移动后原对象状态不确定

---

## Summary

```
+------------------------------------------------------------------+
|                    MOVE SEMANTICS CHECKLIST                       |
+------------------------------------------------------------------+
|                                                                  |
|  1. Mark move ctor/assignment as noexcept                        |
|     (Required for vector optimization)                           |
|                                                                  |
|  2. Leave moved-from object in valid state                       |
|     (Empty, default, or otherwise safe to destroy)               |
|                                                                  |
|  3. Don't use std::move in return statements                     |
|     (Prevents copy elision)                                      |
|                                                                  |
|  4. Don't access moved-from objects                              |
|     (Except to reassign or destroy)                              |
|                                                                  |
|  5. Use std::forward for perfect forwarding                      |
|     (In template functions with T&&)                             |
|                                                                  |
|  6. Prefer emplace over push_back                                |
|     (Avoids temporary altogether)                                |
|                                                                  |
+------------------------------------------------------------------+
```

