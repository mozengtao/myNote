# Pattern 20: Mediator

## 1. Problem the Pattern Solves

### Design Pressure
- Many objects communicate with each other
- Tight coupling makes system hard to modify
- Interaction logic scattered across objects

### What Goes Wrong Without It
```cpp
class Button {
    Dialog* dialog_;
    List* list_;
    TextBox* textBox_;
    void onClick() {
        list_->update();    // Button knows List
        textBox_->clear();  // Button knows TextBox
        dialog_->close();   // Tight coupling everywhere
    }
};
```

---

## 2. Core Idea (C++-Specific)

**Mediator defines an object that encapsulates how a set of objects interact, promoting loose coupling.**

```
+----------+     +----------+     +----------+
| ColleagueA|<-->| Mediator |<-->|ColleagueB|
+----------+     +----------+     +----------+
                      ^
                      |
                +----------+
                |ColleagueC|
                +----------+
```

Colleagues only know the mediator, not each other.

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| Weak references | Colleague → Mediator | Avoid cycles |
| `std::function` | Event callbacks | Flexible notifications |
| Forward declare | Break dependencies | Compile-time decoupling |

---

## 4. Canonical C++ Implementation

```cpp
#include <memory>
#include <string>
#include <iostream>

class DialogMediator;

class Widget {
public:
    explicit Widget(DialogMediator* mediator) : mediator_(mediator) {}
    virtual ~Widget() = default;
    virtual void changed();
protected:
    DialogMediator* mediator_;
};

class Button : public Widget {
public:
    using Widget::Widget;
    void click() { changed(); }
};

class TextBox : public Widget {
public:
    using Widget::Widget;
    std::string text;
    void setText(const std::string& t) { text = t; changed(); }
};

class List : public Widget {
public:
    using Widget::Widget;
    std::string selected;
    void select(const std::string& item) { selected = item; changed(); }
};

class DialogMediator {
public:
    virtual ~DialogMediator() = default;
    virtual void notify(Widget* sender, const std::string& event) = 0;
};

void Widget::changed() {
    mediator_->notify(this, "changed");
}

class SettingsDialog : public DialogMediator {
public:
    SettingsDialog() {
        okButton = std::make_unique<Button>(this);
        themeList = std::make_unique<List>(this);
        previewBox = std::make_unique<TextBox>(this);
    }
    
    void notify(Widget* sender, const std::string& event) override {
        if (sender == themeList.get()) {
            // When theme selected, update preview
            previewBox->setText("Preview: " + themeList->selected);
            std::cout << "Theme changed to: " << themeList->selected << "\n";
        } else if (sender == okButton.get()) {
            std::cout << "Applying settings...\n";
        }
    }
    
    std::unique_ptr<Button> okButton;
    std::unique_ptr<List> themeList;
    std::unique_ptr<TextBox> previewBox;
};

int main() {
    SettingsDialog dialog;
    
    dialog.themeList->select("Dark");
    dialog.okButton->click();
    
    return 0;
}
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| GUI | Dialog coordination |
| Chat | Chat room as mediator |
| ATC | Air traffic control |
| Games | Game controllers |

---

## 6. Common Mistakes

### ❌ Mediator Becomes God Object

```cpp
// BAD: Mediator knows too much
class GodMediator {
    void notify(Widget* w, Event e) {
        // 1000 lines of logic
    }
};
// Split into smaller mediators
```

---

## 7. Mediator vs Observer

| Aspect | Mediator | Observer |
|--------|----------|----------|
| Direction | Bidirectional | One-to-many |
| Centralization | Centralized logic | Distributed |

---

## 8. Mental Model Summary

**When Mediator "Clicks":**

Use Mediator when you have **many objects with complex interactions**. The mediator centralizes the interaction logic, making individual objects simpler and more reusable.

---

## 中文说明

### 中介者模式要点

1. **核心思想**：
   - 同事对象只知道中介者
   - 交互逻辑集中在中介者

2. **典型应用**：
   - GUI 对话框
   - 聊天室
   - 空中交通管制

3. **常见错误**：
   - 中介者变成上帝对象

