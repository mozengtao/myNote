# Pattern 14: State

## 1. Problem the Pattern Solves

### Design Pressure
- Object behavior depends on its state
- State-specific code scattered across methods
- State transitions complex with many conditions

### What Goes Wrong Without It
```cpp
class Order {
    void process() {
        if (state == NEW) { /* ... */ }
        else if (state == PAID) { /* ... */ }
        else if (state == SHIPPED) { /* ... */ }
        // Every method has this switch
    }
    void cancel() {
        if (state == NEW) { /* ... */ }
        else if (state == PAID) { /* ... */ }
        // Repeated everywhere
    }
};
```

---

## 2. Core Idea (C++-Specific)

**State allows an object to alter its behavior when its internal state changes. The object appears to change its class.**

```
+----------+         +------------+
| Context  |-------->| State      |
| request()|         | handle()   |
+----------+         +------------+
                          ^
              +-----------+-----------+
              |                       |
        +-----------+           +-----------+
        | ConcreteA |           | ConcreteB |
        +-----------+           +-----------+
```

---

## 3. C++ Grammar & Language Features

| Feature | Usage | Purpose |
|---------|-------|---------|
| `std::variant` | Finite state | No virtual, type-safe |
| Virtual interface | Open states | Classic OOP |
| `std::unique_ptr` | State ownership | State objects |
| `std::visit` | Variant dispatch | Pattern matching |

---

## 4. Canonical C++ Implementation

### Modern: `std::variant` (Finite States)

```cpp
#include <variant>
#include <iostream>

// States as types
struct Idle {};
struct Running { int progress; };
struct Paused { int progress; };
struct Completed { int result; };

using TaskState = std::variant<Idle, Running, Paused, Completed>;

class Task {
public:
    void start() {
        std::visit([this](auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, Idle>) {
                state_ = Running{0};
            } else if constexpr (std::is_same_v<T, Paused>) {
                state_ = Running{s.progress};
            }
        }, state_);
    }
    
    void pause() {
        if (auto* running = std::get_if<Running>(&state_)) {
            state_ = Paused{running->progress};
        }
    }
    
    void complete(int result) {
        if (std::holds_alternative<Running>(state_)) {
            state_ = Completed{result};
        }
    }
    
    void printStatus() const {
        std::visit([](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, Idle>)
                std::cout << "Idle\n";
            else if constexpr (std::is_same_v<T, Running>)
                std::cout << "Running: " << s.progress << "%\n";
            else if constexpr (std::is_same_v<T, Paused>)
                std::cout << "Paused at: " << s.progress << "%\n";
            else
                std::cout << "Completed: " << s.result << "\n";
        }, state_);
    }
    
private:
    TaskState state_ = Idle{};
};

int main() {
    Task task;
    task.printStatus();  // Idle
    task.start();
    task.printStatus();  // Running: 0%
    task.pause();
    task.printStatus();  // Paused at: 0%
    task.start();
    task.complete(42);
    task.printStatus();  // Completed: 42
    return 0;
}
```

### Classic: Virtual Interface

```cpp
#include <memory>
#include <iostream>

class TCPConnection;

class TCPState {
public:
    virtual ~TCPState() = default;
    virtual void open(TCPConnection&) = 0;
    virtual void close(TCPConnection&) = 0;
    virtual void send(TCPConnection&, const std::string&) = 0;
};

class TCPConnection {
public:
    void setState(std::unique_ptr<TCPState> s) {
        state_ = std::move(s);
    }
    
    void open() { state_->open(*this); }
    void close() { state_->close(*this); }
    void send(const std::string& data) { state_->send(*this, data); }
    
private:
    std::unique_ptr<TCPState> state_;
};

class ClosedState : public TCPState {
public:
    void open(TCPConnection& conn) override {
        std::cout << "Opening connection...\n";
        conn.setState(std::make_unique<OpenState>());
    }
    void close(TCPConnection&) override {
        std::cout << "Already closed\n";
    }
    void send(TCPConnection&, const std::string&) override {
        std::cout << "Cannot send: connection closed\n";
    }
};
```

---

## 5. Typical Usage

| Domain | Example |
|--------|---------|
| Network | TCP connection states |
| UI | Button states (normal, hover, pressed) |
| Game | Character states (idle, walking, jumping) |
| Workflow | Document states (draft, review, published) |

---

## 6. Common Mistakes

### ❌ States Know Too Much About Each Other

```cpp
// BAD: Tight coupling
class StateA : public State {
    void handle(Context& ctx) override {
        ctx.setState(std::make_unique<StateB>());  // Knows about StateB
    }
};
// Consider state transition table
```

---

## 7. State vs Strategy

| Aspect | State | Strategy |
|--------|-------|----------|
| Intent | Change behavior with state | Choose algorithm |
| Transitions | Objects transition themselves | Client sets strategy |
| Awareness | States often know successors | Strategies independent |

---

## 8. Modern C++ Alternative

```cpp
// State machine with transition table
enum class State { Idle, Running, Done };
enum class Event { Start, Stop, Finish };

State transition(State s, Event e) {
    static const std::map<std::pair<State, Event>, State> table = {
        {{State::Idle, Event::Start}, State::Running},
        {{State::Running, Event::Stop}, State::Idle},
        {{State::Running, Event::Finish}, State::Done},
    };
    auto it = table.find({s, e});
    return it != table.end() ? it->second : s;
}
```

---

## 9. Mental Model Summary

**When State "Clicks":**

Use State when an object's behavior depends on its state and **state transitions are complex**. In C++, prefer `std::variant` for finite states (compile-time safety), virtual interface for open-ended states.

---

## 中文说明

### 状态模式要点

1. **两种实现**：
   - `std::variant`：有限状态，类型安全
   - 虚接口：开放状态，可扩展

2. **与策略模式区别**：
   - 状态：对象自己转换状态
   - 策略：客户端选择算法

3. **典型应用**：
   - 网络连接状态
   - 游戏角色状态
   - 工作流状态

