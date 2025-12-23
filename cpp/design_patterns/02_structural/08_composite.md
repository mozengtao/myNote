# Pattern 8: Composite

## 1. Problem the Pattern Solves

### Design Pressure
- Need to represent part-whole hierarchies of objects
- Client should treat individual objects and compositions uniformly
- Tree structures where leaves and branches share operations

### What Goes Wrong Without It
```cpp
// Without composite: client must handle both cases
void render(Shape* s) {
    if (auto* group = dynamic_cast<ShapeGroup*>(s)) {
        for (auto* child : group->children()) {
            render(child);  // Recursive call
        }
    } else {
        s->draw();  // Leaf
    }
}
// Type-checking scattered throughout codebase
```

### Symptoms Indicating Need
- `dynamic_cast` or `typeid` to distinguish containers from items
- Recursive algorithms with special cases for composite types
- Duplicate logic for handling individual vs. grouped elements
- File system, GUI widget trees, document structures

---

## 2. Core Idea (C++-Specific)

**Composite composes objects into tree structures and lets clients treat individual objects and compositions uniformly.**

```
              +---------------+
              |   Component   |
              | + operation() |
              +---------------+
                     ^
                     |
         +-----------+-----------+
         |                       |
   +-----------+          +--------------+
   |   Leaf    |          |  Composite   |
   | operation |          | operation()  |
   +-----------+          | add(child)   |
                          | remove()     |
                          | children[]   |
                          +--------------+
                                |
                        holds Components
```

Key C++ considerations:
1. **Ownership**: Who owns children? (`unique_ptr`, `shared_ptr`, raw?)
2. **Type safety**: How to prevent adding children to leaves?
3. **Iteration**: How to traverse the tree?

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::vector<std::unique_ptr<>>` | Child storage | Ownership of children |
| Pure virtual method | `operation()` | Common interface |
| `override` | Implementations | Compiler verification |
| Range-based for | Traversal | Clean iteration |
| `std::accumulate` | Aggregate ops | Sum/reduce over children |

### Ownership Patterns

```cpp
// Exclusive ownership (most common)
std::vector<std::unique_ptr<Component>> children_;

// Shared ownership (rare, be careful)
std::vector<std::shared_ptr<Component>> children_;

// External ownership (viewer pattern)
std::vector<Component*> children_;  // Someone else owns
```

---

## 4. Canonical C++ Implementation

### File System Example

```cpp
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include <numeric>
#include <algorithm>

// Component interface
class FileSystemEntry {
public:
    explicit FileSystemEntry(std::string name) : name_(std::move(name)) {}
    virtual ~FileSystemEntry() = default;
    
    const std::string& name() const { return name_; }
    virtual size_t size() const = 0;
    virtual void print(int indent = 0) const = 0;
    
protected:
    std::string name_;
};

// Leaf
class File : public FileSystemEntry {
public:
    File(std::string name, size_t size)
        : FileSystemEntry(std::move(name)), size_(size) {}
    
    size_t size() const override { return size_; }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << name_ 
                  << " (" << size_ << " bytes)\n";
    }
    
private:
    size_t size_;
};

// Composite
class Directory : public FileSystemEntry {
public:
    explicit Directory(std::string name) 
        : FileSystemEntry(std::move(name)) {}
    
    void add(std::unique_ptr<FileSystemEntry> entry) {
        children_.push_back(std::move(entry));
    }
    
    size_t size() const override {
        return std::accumulate(
            children_.begin(), children_.end(), size_t{0},
            [](size_t sum, const auto& child) {
                return sum + child->size();
            });
    }
    
    void print(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "[" << name_ << "]\n";
        for (const auto& child : children_) {
            child->print(indent + 2);
        }
    }
    
private:
    std::vector<std::unique_ptr<FileSystemEntry>> children_;
};

// Usage
int main() {
    auto root = std::make_unique<Directory>("root");
    
    auto docs = std::make_unique<Directory>("docs");
    docs->add(std::make_unique<File>("readme.txt", 1024));
    docs->add(std::make_unique<File>("manual.pdf", 10240));
    
    auto src = std::make_unique<Directory>("src");
    src->add(std::make_unique<File>("main.cpp", 2048));
    src->add(std::make_unique<File>("utils.cpp", 1536));
    
    root->add(std::move(docs));
    root->add(std::move(src));
    root->add(std::make_unique<File>(".gitignore", 128));
    
    root->print();
    std::cout << "Total size: " << root->size() << " bytes\n";
    
    return 0;
}
```

### GUI Widget Tree

```cpp
#include <memory>
#include <vector>
#include <iostream>

class Widget {
public:
    virtual ~Widget() = default;
    virtual void render() = 0;
    virtual void handleEvent(int event) = 0;
};

class Button : public Widget {
public:
    explicit Button(std::string label) : label_(std::move(label)) {}
    
    void render() override {
        std::cout << "[Button: " << label_ << "]\n";
    }
    
    void handleEvent(int event) override {
        std::cout << "Button " << label_ << " handling event " << event << "\n";
    }
    
private:
    std::string label_;
};

class Panel : public Widget {
public:
    void add(std::unique_ptr<Widget> widget) {
        children_.push_back(std::move(widget));
    }
    
    void render() override {
        std::cout << "Panel {\n";
        for (const auto& child : children_) {
            child->render();
        }
        std::cout << "}\n";
    }
    
    void handleEvent(int event) override {
        // Propagate event to all children
        for (const auto& child : children_) {
            child->handleEvent(event);
        }
    }
    
private:
    std::vector<std::unique_ptr<Widget>> children_;
};

int main() {
    auto mainPanel = std::make_unique<Panel>();
    mainPanel->add(std::make_unique<Button>("OK"));
    mainPanel->add(std::make_unique<Button>("Cancel"));
    
    auto subPanel = std::make_unique<Panel>();
    subPanel->add(std::make_unique<Button>("Help"));
    mainPanel->add(std::move(subPanel));
    
    mainPanel->render();
    mainPanel->handleEvent(42);
    
    return 0;
}
```

### Transparent vs Safe Composite

```cpp
// Transparent: add() in Component (less type-safe)
class Component {
public:
    virtual void add(std::unique_ptr<Component>) {
        throw std::logic_error("Cannot add to leaf");
    }
};

// Safe: add() only in Composite (more type-safe)
class Composite : public Component {
public:
    void add(std::unique_ptr<Component> c);
};
// Client must know if it has a Composite to add children
```

---

## 5. Typical Usage in Real Projects

| Domain | Example |
|--------|---------|
| GUI frameworks | Widget hierarchies (Qt, wxWidgets) |
| Document editors | Sections, paragraphs, characters |
| Graphics | Scene graphs, SVG groups |
| Build systems | Targets, dependencies |
| Compilers | AST nodes |
| File systems | Directories and files |

### Real-World Examples
- **Qt**: `QObject` parent-child tree
- **HTML DOM**: Element nodes and text nodes
- **Unity**: `GameObject` with children

---

## 6. Common Mistakes & Misuse

### ❌ Mistake 1: Circular References

```cpp
// BAD: Child refers back to parent with shared_ptr
class Component {
    std::shared_ptr<Component> parent_;  // CYCLE if parent holds shared_ptr to child
    std::vector<std::shared_ptr<Component>> children_;
};

// FIX: Use weak_ptr or raw pointer for parent
std::weak_ptr<Component> parent_;
// Or: Component* parent_;  // Non-owning back-reference
```

### ❌ Mistake 2: Uniform Interface Bloat

```cpp
// BAD: Leaf forced to implement composite methods
class Leaf : public Component {
    void add(Component*) override { /* empty or throw */ }
    void remove(Component*) override { /* empty or throw */ }
    size_t childCount() override { return 0; }
};
// Violates ISP; consider separate interfaces
```

### ❌ Mistake 3: Deep Recursion Stack Overflow

```cpp
// BAD: Very deep trees cause stack overflow
void render() {
    for (auto& child : children_) {
        child->render();  // Recursive call
    }
}

// FIX: Use iterative traversal with explicit stack
void renderIterative() {
    std::stack<Component*> pending;
    pending.push(this);
    while (!pending.empty()) {
        auto* node = pending.top(); pending.pop();
        // ... render node ...
        // Push children
    }
}
```

---

## 7. When NOT to Use This Pattern

| Situation | Alternative |
|-----------|-------------|
| No shared operations | Simple container |
| Fixed structure | Direct composition |
| No recursive operations | Flat collection |
| Performance critical | Data-oriented design |

### Alternative: Separate Types

```cpp
// If operations differ significantly:
class File {
    void read();
    void write();
};

class Directory {
    std::vector<std::variant<File, Directory>> entries;
    void list();
};
// Clearer when behaviors are very different
```

---

## 8. Pattern Variations & Modern C++ Alternatives

### `std::variant` for Closed Hierarchy

```cpp
#include <variant>
#include <vector>

struct File { size_t size; };
struct Directory;

using Entry = std::variant<File, Directory>;

struct Directory {
    std::vector<Entry> children;
};

size_t totalSize(const Entry& e) {
    return std::visit([](const auto& x) -> size_t {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, File>) {
            return x.size;
        } else {
            size_t sum = 0;
            for (const auto& child : x.children) {
                sum += totalSize(child);
            }
            return sum;
        }
    }, e);
}
```

### Visitor Pattern Integration

```cpp
class ComponentVisitor {
public:
    virtual void visit(Leaf&) = 0;
    virtual void visit(Composite&) = 0;
};

class Component {
public:
    virtual void accept(ComponentVisitor&) = 0;
};

class Composite : public Component {
public:
    void accept(ComponentVisitor& v) override {
        v.visit(*this);
        for (auto& child : children_) {
            child->accept(v);
        }
    }
};
```

### C++20 Concepts for Components

```cpp
template<typename T>
concept CompositeComponent = requires(T t) {
    { t.size() } -> std::convertible_to<size_t>;
    { t.name() } -> std::convertible_to<std::string>;
};
```

---

## 9. Mental Model Summary

**When Composite "Clicks":**

Use Composite when you have **tree structures** where you want to **treat individual elements and groups uniformly**. The pattern lets you "forget" whether you're working with a leaf or a branch—just call `operation()` and it does the right thing recursively.

**Code Review Recognition:**
- Base class with `operation()` implemented by both leaf and composite
- Composite holds `vector<unique_ptr<Base>>`
- Recursive method calls in composite that delegate to children
- Check ownership: who deletes children? How are cycles prevented?

---

## 中文说明

### 组合模式要点

1. **问题场景**：
   - 需要表示"部分-整体"层次结构
   - 客户端应统一处理单个对象和组合对象
   - 树形结构中叶子和分支共享操作

2. **核心设计**：
   ```
   Component（组件接口）
       ├── Leaf（叶子）：实现具体操作
       └── Composite（组合）：包含子组件，递归调用
   ```

3. **所有权管理**：
   ```cpp
   // 独占所有权（最常用）
   std::vector<std::unique_ptr<Component>> children_;
   
   // 父节点引用用 weak_ptr 或裸指针
   std::weak_ptr<Component> parent_;
   ```

4. **典型应用**：
   - 文件系统（目录和文件）
   - GUI 控件树
   - 文档结构（章节、段落）
   - AST 语法树

5. **常见错误**：
   - 循环引用（parent 用 shared_ptr）
   - 强迫叶子实现组合方法
   - 深层递归导致栈溢出

### 透明 vs 安全组合

```
透明组合：add() 在基类，叶子抛异常
安全组合：add() 只在 Composite，类型更安全
```

