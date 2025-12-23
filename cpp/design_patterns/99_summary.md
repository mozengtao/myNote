# C++ Design Patterns Summary & Decision Guide

## Pattern Comparison Table

| # | Pattern | Problem Solved | Runtime Cost | Compile-Time Cost |
|---|---------|---------------|--------------|-------------------|
| **Creational** |
| 1 | Singleton | Single global instance | Static init | Minimal |
| 2 | Factory Method | Decouple object creation | Virtual call | Minimal |
| 3 | Abstract Factory | Create product families | Virtual calls | Minimal |
| 4 | Builder | Complex object construction | None | Minimal |
| 5 | Prototype | Clone without knowing type | Virtual clone | Minimal |
| **Structural** |
| 6 | Adapter | Interface mismatch | Delegation | Minimal |
| 7 | Bridge | Separate abstraction/impl | Pointer indirection | Minimal |
| 8 | Composite | Tree structures | Virtual calls | Minimal |
| 9 | Decorator | Add behavior dynamically | Wrapper layers | Minimal |
| 10 | Facade | Simplify complex subsystem | None | Minimal |
| 11 | Flyweight | Share common state | Pointer lookup | Minimal |
| 12 | Proxy | Control access | Indirection | Minimal |
| **Behavioral** |
| 13 | Strategy | Interchangeable algorithms | std::function / virtual | Minimal |
| 14 | State | State-dependent behavior | State object switch | Minimal |
| 15 | Command | Encapsulate operations | Command objects | Minimal |
| 16 | Observer | Event notification | Callback dispatch | Minimal |
| 17 | Iterator | Traverse collection | Iterator overhead | Minimal |
| 18 | Visitor | Add operations to hierarchy | Double dispatch | Minimal |
| 19 | Template Method | Algorithm skeleton | Virtual hooks | Minimal |
| 20 | Mediator | Centralize communication | Mediator lookup | Minimal |
| 21 | Memento | Capture/restore state | State copying | Minimal |
| 22 | Chain of Resp. | Pass request along chain | Chain traversal | Minimal |
| 23 | Interpreter | Evaluate expressions | Tree traversal | Minimal |
| **C++-Centric** |
| 24 | RAII | Resource management | None | Minimal |
| 25 | Type Erasure | Polymorphism without inheritance | Heap + virtual | Minimal |
| 26 | CRTP | Static polymorphism | None | Code bloat |
| 27 | Policy-Based | Compile-time customization | None | Code bloat |
| 28 | PIMPL | Compilation firewall | Pointer indirection | Reduced |
| 29 | Value Semantics | Simplify reasoning | Copy/move | Minimal |
| 30 | NVI | Control virtual interface | None extra | Minimal |

---

## Decision Guide

### "If You See X → Consider Y Pattern"

```
+---------------------------------------------------+---------------------------+
| If you see this problem...                        | Consider this pattern     |
+---------------------------------------------------+---------------------------+
| OBJECT CREATION                                   |                           |
+---------------------------------------------------+---------------------------+
| Need exactly one instance globally                | Singleton (sparingly!)    |
| Client shouldn't know concrete type               | Factory Method            |
| Creating families of related objects              | Abstract Factory          |
| Constructor has 5+ parameters                     | Builder                   |
| Need to copy polymorphic objects                  | Prototype                 |
+---------------------------------------------------+---------------------------+
| STRUCTURE & COMPOSITION                           |                           |
+---------------------------------------------------+---------------------------+
| Interface doesn't match what you need             | Adapter                   |
| Two dimensions of variation (what × how)          | Bridge                    |
| Part-whole tree hierarchy                         | Composite                 |
| Add features without subclassing                  | Decorator                 |
| Complex subsystem needs simple interface          | Facade                    |
| Many objects share identical state                | Flyweight                 |
| Control/lazy-load access to object                | Proxy                     |
+---------------------------------------------------+---------------------------+
| BEHAVIOR & ALGORITHMS                             |                           |
+---------------------------------------------------+---------------------------+
| Swap algorithms at runtime                        | Strategy                  |
| Behavior depends on internal state                | State                     |
| Queue/undo/log operations                         | Command                   |
| One-to-many event notification                    | Observer                  |
| Traverse collection without exposing internals    | Iterator                  |
| Add operations to stable class hierarchy          | Visitor                   |
| Algorithm skeleton with customizable steps        | Template Method           |
| Many-to-many object communication                 | Mediator                  |
| Capture/restore object state                      | Memento                   |
| Multiple handlers for a request                   | Chain of Responsibility   |
| Parse/evaluate a simple language                  | Interpreter               |
+---------------------------------------------------+---------------------------+
| C++ SPECIFIC                                      |                           |
+---------------------------------------------------+---------------------------+
| Resource cleanup guaranteed                       | RAII (always!)            |
| Polymorphism without inheritance                  | Type Erasure              |
| Static polymorphism, zero overhead                | CRTP                      |
| Compile-time customization                        | Policy-Based Design       |
| Reduce compile dependencies                       | PIMPL                     |
| Objects should copy independently                 | Value Semantics           |
| Control how virtual functions are called          | NVI                       |
+---------------------------------------------------+---------------------------+
```

---

## Patterns Every C++ Engineer MUST Master

### 🟢 Tier 1: Fundamental (Use Daily)

| Pattern | Why Essential |
|---------|---------------|
| **RAII** | Core C++ idiom for resource management |
| **Value Semantics** | Default to this; simplifies reasoning |
| **Factory Method** | Clean object creation |
| **Strategy** | Flexible algorithm selection |
| **Observer** | Event-driven systems |
| **Iterator** | STL compatibility |

### 🟡 Tier 2: Important (Use Regularly)

| Pattern | Why Important |
|---------|---------------|
| **Builder** | Complex object construction |
| **Decorator** | Runtime feature composition |
| **Facade** | API simplification |
| **Command** | Undo/redo, task queues |
| **PIMPL** | Library development, ABI stability |
| **NVI** | Controlled polymorphism |

### 🟠 Tier 3: Advanced (Use When Needed)

| Pattern | When to Use |
|---------|-------------|
| **CRTP** | Zero-overhead polymorphism |
| **Type Erasure** | Heterogeneous containers |
| **Policy-Based** | Highly configurable libraries |
| **Visitor** | Stable class hierarchies |
| **Composite** | Tree structures |

---

## Patterns to Use Sparingly

### ⚠️ Use with Caution

| Pattern | Why Caution |
|---------|-------------|
| **Singleton** | Global state, testing nightmares, hidden dependencies |
| **Abstract Factory** | Often overkill; adds complexity |
| **Flyweight** | Only when memory is proven bottleneck |
| **Mediator** | Can become god object |
| **Interpreter** | Use parser generators for complex grammars |

### ❌ Often Overused/Misused

| Anti-Pattern | Better Alternative |
|--------------|-------------------|
| Singleton for "convenience" | Dependency injection |
| Deep decorator chains | Composition or builder |
| Virtual for everything | Templates or CRTP |
| Factory for simple objects | Direct construction |

---

## Runtime vs Compile-Time Polymorphism

```
Choose Runtime (virtual) when:
├── Types determined at runtime (plugins, user input)
├── Need heterogeneous containers with base*
├── Library boundary (ABI stability)
└── Type set is open/extensible

Choose Compile-Time (templates/CRTP) when:
├── Types known at compile time
├── Zero runtime overhead required
├── Hot paths, performance critical
└── Type set is closed
```

---

## Pattern Relationships

```
                    +------------------+
                    |      RAII        |  ← Foundation
                    +------------------+
                           |
         +-----------------+-----------------+
         |                 |                 |
    +--------+        +--------+        +--------+
    | Smart  |        | Lock   |        | File   |
    | Ptr    |        | Guard  |        | Handle |
    +--------+        +--------+        +--------+

    Strategy ←→ Template Method ←→ State
        ↓             ↓              ↓
    (runtime)    (inheritance)   (state obj)

    Factory Method → Abstract Factory → Builder
         ↓                 ↓              ↓
    (one product)   (family)      (complex ctor)

    Adapter ←→ Bridge ←→ Decorator ←→ Proxy
        ↓         ↓           ↓         ↓
    (interface) (impl)    (behavior)  (access)
```

---

## Quick Reference: Modern C++ Alternatives

| Classic Pattern | Modern C++ Alternative |
|----------------|------------------------|
| Singleton | `inline static`, module-level static |
| Factory | `std::make_unique<T>()` |
| Strategy | `std::function` + lambda |
| Observer | Signals/slots, `std::function` |
| Iterator | Range-based for, `std::ranges` |
| Visitor | `std::variant` + `std::visit` |
| State | `std::variant` with state types |
| Prototype | Copy constructor, `clone()` |
| Command | `std::function<void()>` |

---

## 中文总结

### 必须掌握的模式（每天使用）

1. **RAII** - C++ 资源管理的基础
2. **值语义** - 默认设计方式
3. **工厂方法** - 对象创建解耦
4. **策略** - 灵活的算法选择
5. **观察者** - 事件驱动系统
6. **迭代器** - STL 兼容

### 谨慎使用的模式

1. **单例** - 全局状态，测试困难
2. **抽象工厂** - 常常过度设计
3. **中介者** - 容易变成上帝对象

### 决策流程

```
需要创建对象？
    ├── 单一类型 → 工厂方法
    ├── 产品家族 → 抽象工厂
    └── 复杂构造 → 建造者

需要多态？
    ├── 运行时决定 → 虚函数
    ├── 编译时决定 → 模板/CRTP
    └── 无继承 → 类型擦除

需要扩展行为？
    ├── 运行时组合 → 装饰器
    ├── 编译时组合 → 基于策略设计
    └── 算法替换 → 策略模式
```

