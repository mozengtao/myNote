# Topic 25: Composition over Inheritance

## 1. Problem Statement

### What real engineering problem does this solve?

Inheritance creates tight coupling between classes:

```
INHERITANCE HIERARCHY                COMPOSITION
(Rigid, tightly coupled)             (Flexible, loosely coupled)

       ┌─────────┐                   ┌─────────┐
       │  Base   │                   │ Client  │
       └────┬────┘                   └────┬────┘
            │                             │ has-a
       ┌────┴────┐                   ┌────▼────┐
       │ Derived │                   │Component│
       └─────────┘                   └─────────┘

• Derived is ALWAYS a Base         • Client can CHANGE component
• Can't change at runtime          • Runtime flexibility
• Exposes Base internals           • Clean interface
• Breaks with Base changes         • Stable interface
```

### What goes wrong with excessive inheritance?

```cpp
// The "fragile base class" problem
class Animal {
protected:
    int legs_ = 4;
public:
    virtual void move() { walk(); }
    virtual void walk() { /* use legs_ */ }
};

class Bird : public Animal {
public:
    void fly() { /* ... */ }
    void move() override { fly(); }  // Override default
};

class Penguin : public Bird {
    // Problem: Penguins can't fly!
    // But Bird::move() calls fly()
    // And Animal::move() calls walk() which assumes legs_
    // Inheritance hierarchy doesn't match reality
};
```

**中文说明：**
继承创建了类之间的强耦合——派生类依赖基类的实现细节。"脆弱基类"问题：修改基类可能破坏所有派生类。组合通过"有一个"而非"是一个"关系实现灵活的代码复用，可以在运行时更换组件，接口更清晰。

---

## 2. Core Idea

### When to Use Inheritance vs Composition

```
USE INHERITANCE (is-a):             USE COMPOSITION (has-a):
────────────────────────────────────────────────────────────────
• True subtype relationship         • Reusing functionality
• Need polymorphism                 • Combining behaviors
• LSP is satisfied                  • Delegation patterns
• Stable interface                  • Runtime flexibility

例子:                                例子:
• std::exception hierarchy          • Car has-a Engine
• Stream classes                    • Logger has-a Formatter
• Widget base class                 • Controller has-a Model
```

### Liskov Substitution Principle (LSP)

```cpp
// LSP: If B is subtype of A, objects of type A can be replaced with B
// without breaking the program

class Rectangle {
public:
    virtual void setWidth(int w) { width_ = w; }
    virtual void setHeight(int h) { height_ = h; }
    int area() const { return width_ * height_; }
protected:
    int width_, height_;
};

class Square : public Rectangle {
public:
    void setWidth(int w) override { width_ = height_ = w; }
    void setHeight(int h) override { width_ = height_ = h; }
};

void test(Rectangle& r) {
    r.setWidth(5);
    r.setHeight(10);
    assert(r.area() == 50);  // Fails for Square!
}

// Square violates LSP - can't substitute for Rectangle
// SOLUTION: Use composition instead
```

**中文说明：**
继承应该只用于真正的"是一个"关系，且必须满足里氏替换原则：用派生类替换基类不应破坏程序行为。如果继承只是为了复用代码而非表达类型关系，应该用组合代替。

---

## 3. Idiomatic C++ Techniques

### Composition with Delegation

```cpp
// Instead of inheriting from Logger
class Service {
    Logger logger_;       // Composition
    Database db_;         // Composition
    
public:
    Service(Logger log, Database db) 
        : logger_(std::move(log)), db_(std::move(db)) {}
    
    void process() {
        logger_.log("Processing...");
        db_.execute("SELECT ...");
    }
};
```

### Strategy Pattern (Behavioral Composition)

```cpp
// Behavior injected via composition
class Sorter {
    std::function<bool(int, int)> comparator_;
    
public:
    explicit Sorter(std::function<bool(int, int)> cmp)
        : comparator_(std::move(cmp)) {}
    
    void sort(std::vector<int>& data) {
        std::sort(data.begin(), data.end(), comparator_);
    }
};

// Usage - swap behavior at runtime
Sorter ascending([](int a, int b) { return a < b; });
Sorter descending([](int a, int b) { return a > b; });
```

### Mixins via CRTP (Compile-time Composition)

```cpp
// Add functionality without deep inheritance
template<typename Derived>
class Serializable {
public:
    std::string toJson() const {
        return static_cast<const Derived*>(this)->serializeImpl();
    }
};

template<typename Derived>
class Comparable {
public:
    bool operator==(const Derived& other) const {
        return static_cast<const Derived*>(this)->equals(other);
    }
    bool operator!=(const Derived& other) const {
        return !(*this == other);
    }
};

// "Compose" multiple capabilities
class Person : public Serializable<Person>, public Comparable<Person> {
    std::string name_;
    int age_;
    
public:
    std::string serializeImpl() const {
        return "{\"name\":\"" + name_ + "\",\"age\":" + std::to_string(age_) + "}";
    }
    
    bool equals(const Person& other) const {
        return name_ == other.name_ && age_ == other.age_;
    }
};
```

---

## 4. Complete C++ Example

```cpp
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ============================================================
// BAD: Deep inheritance hierarchy
// ============================================================
namespace BadDesign {

class Employee {
protected:
    std::string name_;
    double baseSalary_;
    
public:
    Employee(std::string name, double salary) 
        : name_(std::move(name)), baseSalary_(salary) {}
    
    virtual double calculatePay() const { return baseSalary_; }
    virtual ~Employee() = default;
};

class Manager : public Employee {
protected:
    double bonus_;
    
public:
    Manager(std::string name, double salary, double bonus)
        : Employee(std::move(name), salary), bonus_(bonus) {}
    
    double calculatePay() const override { return baseSalary_ + bonus_; }
};

class SeniorManager : public Manager {
    double stockOptions_;
    
public:
    SeniorManager(std::string name, double salary, double bonus, double options)
        : Manager(std::move(name), salary, bonus), stockOptions_(options) {}
    
    double calculatePay() const override {
        return baseSalary_ + bonus_ + stockOptions_;
    }
};

// Problem: Adding new compensation types requires new classes
// Problem: What if someone is both a Manager AND a Contractor?
// Problem: Can't change employee type at runtime

}  // namespace BadDesign

// ============================================================
// GOOD: Composition-based design
// ============================================================
namespace GoodDesign {

// Compensation strategy (behavior as component)
class CompensationPolicy {
public:
    virtual ~CompensationPolicy() = default;
    virtual double calculate(double baseSalary) const = 0;
    virtual std::unique_ptr<CompensationPolicy> clone() const = 0;
};

class SalariedCompensation : public CompensationPolicy {
public:
    double calculate(double baseSalary) const override {
        return baseSalary;
    }
    std::unique_ptr<CompensationPolicy> clone() const override {
        return std::make_unique<SalariedCompensation>(*this);
    }
};

class BonusCompensation : public CompensationPolicy {
    double bonusPercent_;
    
public:
    explicit BonusCompensation(double percent) : bonusPercent_(percent) {}
    
    double calculate(double baseSalary) const override {
        return baseSalary * (1 + bonusPercent_);
    }
    std::unique_ptr<CompensationPolicy> clone() const override {
        return std::make_unique<BonusCompensation>(*this);
    }
};

class CommissionCompensation : public CompensationPolicy {
    double commissionRate_;
    double sales_;
    
public:
    CommissionCompensation(double rate, double sales) 
        : commissionRate_(rate), sales_(sales) {}
    
    double calculate(double baseSalary) const override {
        return baseSalary + (sales_ * commissionRate_);
    }
    std::unique_ptr<CompensationPolicy> clone() const override {
        return std::make_unique<CommissionCompensation>(*this);
    }
};

// Benefits as component
class BenefitsPackage {
    bool healthInsurance_;
    bool retirement_;
    int vacationDays_;
    
public:
    BenefitsPackage(bool health = true, bool retire = true, int vacation = 20)
        : healthInsurance_(health), retirement_(retire), vacationDays_(vacation) {}
    
    void describe() const {
        std::cout << "Benefits: Health=" << healthInsurance_ 
                  << ", Retirement=" << retirement_
                  << ", Vacation=" << vacationDays_ << " days\n";
    }
};

// Employee uses composition - no inheritance for behavior
class Employee {
    std::string name_;
    double baseSalary_;
    std::unique_ptr<CompensationPolicy> compensation_;
    BenefitsPackage benefits_;
    
public:
    Employee(std::string name, double salary,
             std::unique_ptr<CompensationPolicy> comp,
             BenefitsPackage benefits = {})
        : name_(std::move(name))
        , baseSalary_(salary)
        , compensation_(std::move(comp))
        , benefits_(std::move(benefits))
    {}
    
    // Can copy
    Employee(const Employee& other)
        : name_(other.name_)
        , baseSalary_(other.baseSalary_)
        , compensation_(other.compensation_->clone())
        , benefits_(other.benefits_)
    {}
    
    // Change compensation at runtime!
    void setCompensation(std::unique_ptr<CompensationPolicy> comp) {
        compensation_ = std::move(comp);
    }
    
    double calculatePay() const {
        return compensation_->calculate(baseSalary_);
    }
    
    void describe() const {
        std::cout << name_ << ": $" << calculatePay() << "\n";
        benefits_.describe();
    }
};

}  // namespace GoodDesign

// ============================================================
// Composition for behavior injection (Logger example)
// ============================================================

class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(const std::string& msg) = 0;
};

class ConsoleLogger : public ILogger {
public:
    void log(const std::string& msg) override {
        std::cout << "[LOG] " << msg << "\n";
    }
};

class NullLogger : public ILogger {
public:
    void log(const std::string&) override {}  // Do nothing
};

// Service uses logger via composition
class OrderService {
    std::unique_ptr<ILogger> logger_;
    
public:
    explicit OrderService(std::unique_ptr<ILogger> logger)
        : logger_(std::move(logger)) {}
    
    void processOrder(int orderId) {
        logger_->log("Processing order " + std::to_string(orderId));
        // ... actual processing ...
        logger_->log("Order " + std::to_string(orderId) + " completed");
    }
};

// ============================================================
// Demonstration
// ============================================================

int main() {
    using namespace GoodDesign;
    
    std::cout << "=== Composition-based Employees ===\n";
    
    // Create employees with different compensation policies
    Employee regular(
        "Alice",
        50000,
        std::make_unique<SalariedCompensation>()
    );
    
    Employee manager(
        "Bob",
        70000,
        std::make_unique<BonusCompensation>(0.15),  // 15% bonus
        BenefitsPackage(true, true, 25)
    );
    
    Employee sales(
        "Charlie",
        40000,
        std::make_unique<CommissionCompensation>(0.10, 100000),
        BenefitsPackage(true, false, 15)
    );
    
    regular.describe();
    manager.describe();
    sales.describe();
    
    std::cout << "\n=== Changing compensation at runtime ===\n";
    // Promote regular employee
    regular.setCompensation(std::make_unique<BonusCompensation>(0.10));
    std::cout << "After promotion:\n";
    regular.describe();
    
    std::cout << "\n=== Service with injected logger ===\n";
    
    OrderService service(std::make_unique<ConsoleLogger>());
    service.processOrder(12345);
    
    std::cout << "\n(With null logger - silent)\n";
    OrderService silentService(std::make_unique<NullLogger>());
    silentService.processOrder(67890);  // No output
    
    return 0;
}
```

---

## 5. Failure Modes

### Mistake 1: Inheriting for code reuse only

```cpp
// BAD: Inheritance just to reuse code
class Stack : public std::vector<int> {
    // Exposes ALL vector methods - can break stack invariants!
    // Someone can call insert(), which makes no sense for a stack
};

// GOOD: Composition with controlled interface
class Stack {
    std::vector<int> data_;  // Hidden implementation
    
public:
    void push(int v) { data_.push_back(v); }
    int pop() {
        int v = data_.back();
        data_.pop_back();
        return v;
    }
    bool empty() const { return data_.empty(); }
};
```

### Mistake 2: God class from over-composition

```cpp
// BAD: Too many responsibilities
class Application {
    Logger logger_;
    Database db_;
    HttpServer server_;
    Cache cache_;
    AuthService auth_;
    EmailService email_;
    MetricsCollector metrics_;
    // ... 20 more components
    
    // Doing too much!
};

// BETTER: Smaller, focused classes
class OrderHandler {
    OrderRepository& orders_;
    InventoryService& inventory_;
    NotificationService& notifier_;
    // Just what's needed
};
```

### Mistake 3: Circular dependencies in composition

```cpp
// BAD: A has B, B has A
class A {
    B* b_;  // A depends on B
};

class B {
    A* a_;  // B depends on A - circular!
};

// FIX: Introduce interface to break cycle
class IB {
public:
    virtual void doSomething() = 0;
};

class A {
    IB* b_;  // A depends on interface
};

class B : public IB {
    A* a_;  // OK: concrete depends on concrete
};
```

---

## 6. When to Use Inheritance

### Legitimate Uses of Inheritance

| Use Case | Example |
|----------|---------|
| True subtype (is-a) | Circle is-a Shape |
| Interface implementation | Logger implements ILogger |
| Framework extension points | Override template methods |
| Mix-ins via CRTP | Add serialization capability |
| Exception hierarchies | DomainError extends Error |

### Decision Framework

```
                    Need polymorphism?
                           │
               ┌───────────┴───────────┐
              YES                      NO
               │                        │
               v                        v
        Is it true is-a?           Use composition
               │
       ┌───────┴───────┐
      YES              NO
       │                │
       v                v
  Use inheritance   Use composition +
  (public)          interface (has-a)
```

**中文说明：**
使用继承的场景：
1. **真正的"是一个"关系**：Circle 是一个 Shape
2. **实现接口**：提供虚函数的具体实现
3. **框架扩展点**：如 Qt 的 QWidget 派生
4. **CRTP 混入**：编译时组合功能

默认选择组合。只有当继承明确表达类型层次且满足 LSP 时才用继承。

---

## Summary

```
+------------------------------------------------------------------+
|              COMPOSITION VS INHERITANCE                           |
+------------------------------------------------------------------+
|                                                                  |
|  PREFER COMPOSITION WHEN:                                        |
|  ─────────────────────────────────────────────────────────────── |
|  • Reusing implementation, not defining subtype                  |
|  • Need runtime flexibility (swap components)                    |
|  • Multiple inheritance would be needed                          |
|  • Base class is unstable or complex                             |
|  • Relationship is has-a, not is-a                               |
|                                                                  |
|  USE INHERITANCE WHEN:                                           |
|  ─────────────────────────────────────────────────────────────── |
|  • True subtype relationship (is-a)                              |
|  • Need polymorphism (virtual functions)                         |
|  • LSP is satisfied                                              |
|  • Interface is stable                                           |
|  • Framework requires it                                         |
|                                                                  |
|  COMPOSITION TECHNIQUES:                                         |
|  ─────────────────────────────────────────────────────────────── |
|  • Dependency injection (constructor)                            |
|  • Strategy pattern (inject behavior)                            |
|  • Decorator pattern (wrap and extend)                           |
|  • Aggregate components (has-a members)                          |
|  • CRTP mixins (compile-time composition)                        |
|                                                                  |
+------------------------------------------------------------------+
```

