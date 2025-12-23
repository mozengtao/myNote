# Pattern 15: Command

## 1. Problem the Pattern Solves

### Design Pressure
- Need to parameterize objects with operations
- Queue, log, or undo operations
- Support undo/redo functionality
- Decouple invoker from executor

### What Goes Wrong Without It
```cpp
// Without command: directly calling methods
button.onClick = []() { document.save(); };
// Cannot undo, cannot queue, cannot log
```

---

## 2. Core Idea (C++-Specific)

**Command encapsulates a request as an object, letting you parameterize clients with queues, requests, and operations.**

```
+----------+     +---------+     +----------+
| Invoker  |---->| Command |---->| Receiver |
| (button) |     | execute |     | (actual  |
+----------+     | undo    |     |  logic)  |
                 +---------+     +----------+
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::function` | Simple command | Callable wrapper |
| Virtual class | Complex command | Undo support |
| Lambda | Inline command | Concise |
| `std::stack` | Undo stack | History |

---

## 4. Canonical C++ Implementation

### With Undo Support

```cpp
#include <memory>
#include <stack>
#include <string>
#include <iostream>

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class TextEditor {
public:
    std::string text;
    void insertText(size_t pos, const std::string& s) {
        text.insert(pos, s);
    }
    void deleteText(size_t pos, size_t len) {
        text.erase(pos, len);
    }
};

class InsertCommand : public Command {
public:
    InsertCommand(TextEditor& editor, size_t pos, std::string text)
        : editor_(editor), pos_(pos), text_(std::move(text)) {}
    
    void execute() override {
        editor_.insertText(pos_, text_);
    }
    
    void undo() override {
        editor_.deleteText(pos_, text_.length());
    }
    
private:
    TextEditor& editor_;
    size_t pos_;
    std::string text_;
};

class CommandHistory {
public:
    void execute(std::unique_ptr<Command> cmd) {
        cmd->execute();
        undoStack_.push(std::move(cmd));
        while (!redoStack_.empty()) redoStack_.pop();
    }
    
    void undo() {
        if (undoStack_.empty()) return;
        auto cmd = std::move(undoStack_.top());
        undoStack_.pop();
        cmd->undo();
        redoStack_.push(std::move(cmd));
    }
    
    void redo() {
        if (redoStack_.empty()) return;
        auto cmd = std::move(redoStack_.top());
        redoStack_.pop();
        cmd->execute();
        undoStack_.push(std::move(cmd));
    }
    
private:
    std::stack<std::unique_ptr<Command>> undoStack_;
    std::stack<std::unique_ptr<Command>> redoStack_;
};

int main() {
    TextEditor editor;
    CommandHistory history;
    
    history.execute(std::make_unique<InsertCommand>(editor, 0, "Hello"));
    std::cout << editor.text << "\n";  // "Hello"
    
    history.execute(std::make_unique<InsertCommand>(editor, 5, " World"));
    std::cout << editor.text << "\n";  // "Hello World"
    
    history.undo();
    std::cout << editor.text << "\n";  // "Hello"
    
    history.redo();
    std::cout << editor.text << "\n";  // "Hello World"
    
    return 0;
}
```

### Simple: `std::function`

```cpp
#include <functional>
#include <vector>

class Button {
public:
    using Action = std::function<void()>;
    
    void setAction(Action a) { action_ = std::move(a); }
    void click() { if (action_) action_(); }
    
private:
    Action action_;
};

class MacroCommand {
public:
    void add(std::function<void()> cmd) {
        commands_.push_back(std::move(cmd));
    }
    
    void execute() {
        for (auto& cmd : commands_) cmd();
    }
    
private:
    std::vector<std::function<void()>> commands_;
};
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Text editors | Undo/redo |
| GUI | Button actions |
| Games | Replay systems |
| Transactions | Database operations |

---

## 6. Common Mistakes

### ❌ Command Captures Reference to Dead Object

```cpp
// BAD: Lambda captures reference
auto cmd = [&editor]() { editor.save(); };
// If editor destroyed, crash!

// FIX: Capture shared_ptr or validate
```

---

## 7. When NOT to Use

| Situation | Alternative |
|-----------|-------------|
| No undo needed | Direct callback |
| Simple action | Lambda |

---

## 8. Mental Model Summary

**When Command "Clicks":**

Use Command when you need to **treat operations as objects**—to queue, undo, log, or compose them. The command encapsulates all information needed to perform the action later.

---

## 中文说明

### 命令模式要点

1. **核心能力**：
   - 撤销/重做
   - 操作队列
   - 宏命令（组合多个命令）

2. **C++ 实现**：
   - 简单命令：`std::function`
   - 需要撤销：虚基类 + undo()

3. **常见错误**：
   - Lambda 捕获悬垂引用
   - 命令对象过于复杂

