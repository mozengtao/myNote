# Pattern 21: Memento

## 1. Problem the Pattern Solves

### Design Pressure
- Need to capture and restore object's internal state
- Must not violate encapsulation
- Support undo operations

### What Goes Wrong Without It
```cpp
// Without memento: exposing internal state
class Editor {
public:
    std::string content_;  // Public = breaks encapsulation
    int cursor_;
};
// Anyone can mess with state directly
```

---

## 2. Core Idea (C++-Specific)

**Memento captures and externalizes an object's internal state without violating encapsulation, so the object can be restored to this state later.**

```
+------------+      +---------+      +------------+
| Originator |----->| Memento |<-----| Caretaker  |
| save()     |      | (state) |      | (history)  |
| restore()  |      +---------+      +------------+
+------------+
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `friend class` | Memento access | Encapsulation |
| `private` nested class | Hide memento | Internal only |
| `std::stack` | History | Undo stack |
| Move semantics | Efficient state transfer | Performance |

---

## 4. Canonical C++ Implementation

```cpp
#include <memory>
#include <stack>
#include <string>
#include <iostream>

class Editor {
public:
    class Memento {
    public:
        // Only Editor can access internals
        friend class Editor;
    private:
        Memento(std::string content, int cursor)
            : content_(std::move(content)), cursor_(cursor) {}
        std::string content_;
        int cursor_;
    };
    
    void type(const std::string& text) {
        content_.insert(cursor_, text);
        cursor_ += text.length();
    }
    
    void moveCursor(int pos) { cursor_ = pos; }
    
    std::unique_ptr<Memento> save() const {
        return std::unique_ptr<Memento>(
            new Memento(content_, cursor_));
    }
    
    void restore(const Memento& m) {
        content_ = m.content_;
        cursor_ = m.cursor_;
    }
    
    void print() const {
        std::cout << "Content: \"" << content_ 
                  << "\" cursor: " << cursor_ << "\n";
    }
    
private:
    std::string content_;
    int cursor_ = 0;
};

class History {
public:
    void push(std::unique_ptr<Editor::Memento> m) {
        history_.push(std::move(m));
    }
    
    std::unique_ptr<Editor::Memento> pop() {
        if (history_.empty()) return nullptr;
        auto m = std::move(history_.top());
        history_.pop();
        return m;
    }
    
private:
    std::stack<std::unique_ptr<Editor::Memento>> history_;
};

int main() {
    Editor editor;
    History history;
    
    editor.type("Hello");
    history.push(editor.save());
    editor.print();
    
    editor.type(" World");
    history.push(editor.save());
    editor.print();
    
    editor.type("!");
    editor.print();
    
    // Undo
    if (auto m = history.pop()) {
        editor.restore(*m);
        editor.print();
    }
    
    // Undo again
    if (auto m = history.pop()) {
        editor.restore(*m);
        editor.print();
    }
    
    return 0;
}
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Text editors | Undo/redo |
| Games | Save points |
| Transactions | Rollback |
| UI | State snapshots |

---

## 6. Common Mistakes

### ❌ Memento Too Large

```cpp
// BAD: Saving entire document for small changes
class Memento { 
    std::vector<Page> allPages_;  // Could be gigabytes!
};
// Consider: incremental snapshots, command pattern
```

---

## 7. Memento vs Command

| Aspect | Memento | Command |
|--------|---------|---------|
| Stores | State snapshot | Action to perform |
| Undo via | Restore state | Inverse action |
| Size | Can be large | Typically small |

---

## 8. Mental Model Summary

**When Memento "Clicks":**

Use Memento when you need to **capture and restore state** without exposing internal details. The originator creates mementos, the caretaker stores them, and neither exposes the internal state publicly.

---

## 中文说明

### 备忘录模式要点

1. **三个角色**：
   - 发起人（Originator）：创建/恢复备忘录
   - 备忘录（Memento）：存储状态
   - 管理者（Caretaker）：保存历史

2. **与命令模式区别**：
   - 备忘录：保存状态快照
   - 命令：保存操作，通过逆操作撤销

3. **注意事项**：
   - 状态过大时考虑增量快照

