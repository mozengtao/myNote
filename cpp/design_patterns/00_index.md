# C++ Design Patterns Mastery Guide

## Overview

This guide covers 30 essential design patterns from a **C++ systems engineering perspective**.
Focus is on understanding the engineering problems each pattern solves, the C++ language
features that enable them, and practical application in real-world projects.

---

## Pattern Categories

### I. Creational Patterns (1-5)
Patterns that deal with object creation mechanisms.

| # | Pattern | File | Core C++ Feature |
|---|---------|------|------------------|
| 1 | Singleton | [01_singleton.md](01_creational/01_singleton.md) | Static local, `std::call_once` |
| 2 | Factory Method | [02_factory_method.md](01_creational/02_factory_method.md) | Virtual functions, `unique_ptr` |
| 3 | Abstract Factory | [03_abstract_factory.md](01_creational/03_abstract_factory.md) | Interface inheritance |
| 4 | Builder | [04_builder.md](01_creational/04_builder.md) | Method chaining, move semantics |
| 5 | Prototype | [05_prototype.md](01_creational/05_prototype.md) | Virtual clone, `unique_ptr` |

### II. Structural Patterns (6-12)
Patterns that deal with object composition.

| # | Pattern | File | Core C++ Feature |
|---|---------|------|------------------|
| 6 | Adapter | [06_adapter.md](02_structural/06_adapter.md) | Composition, inheritance |
| 7 | Bridge | [07_bridge.md](02_structural/07_bridge.md) | PIMPL, `unique_ptr` |
| 8 | Composite | [08_composite.md](02_structural/08_composite.md) | Recursive composition |
| 9 | Decorator | [09_decorator.md](02_structural/09_decorator.md) | Wrapper delegation |
| 10 | Facade | [10_facade.md](02_structural/10_facade.md) | Simplified interface |
| 11 | Flyweight | [11_flyweight.md](02_structural/11_flyweight.md) | Shared state, `shared_ptr` |
| 12 | Proxy | [12_proxy.md](02_structural/12_proxy.md) | Indirection, smart pointers |

### III. Behavioral Patterns (13-23)
Patterns that deal with algorithms and object communication.

| # | Pattern | File | Core C++ Feature |
|---|---------|------|------------------|
| 13 | Strategy | [13_strategy.md](03_behavioral/13_strategy.md) | `std::function`, templates |
| 14 | State | [14_state.md](03_behavioral/14_state.md) | State objects, `variant` |
| 15 | Command | [15_command.md](03_behavioral/15_command.md) | Callable objects, undo stacks |
| 16 | Observer | [16_observer.md](03_behavioral/16_observer.md) | Callbacks, weak references |
| 17 | Iterator | [17_iterator.md](03_behavioral/17_iterator.md) | STL iterator concepts |
| 18 | Visitor | [18_visitor.md](03_behavioral/18_visitor.md) | Double dispatch, `variant` |
| 19 | Template Method | [19_template_method.md](03_behavioral/19_template_method.md) | NVI, virtual hooks |
| 20 | Mediator | [20_mediator.md](03_behavioral/20_mediator.md) | Central coordinator |
| 21 | Memento | [21_memento.md](03_behavioral/21_memento.md) | State snapshots |
| 22 | Chain of Responsibility | [22_chain_of_responsibility.md](03_behavioral/22_chain_of_responsibility.md) | Handler chains |
| 23 | Interpreter | [23_interpreter.md](03_behavioral/23_interpreter.md) | AST, recursive evaluation |

### IV. C++-Centric / Modern Patterns (24-30)
Patterns that leverage C++-specific features.

| # | Pattern | File | Core C++ Feature |
|---|---------|------|------------------|
| 24 | RAII | [24_raii.md](04_cpp_centric/24_raii.md) | Destructors, scope |
| 25 | Type Erasure | [25_type_erasure.md](04_cpp_centric/25_type_erasure.md) | `std::function`, `std::any` |
| 26 | CRTP | [26_crtp.md](04_cpp_centric/26_crtp.md) | Static polymorphism |
| 27 | Policy-Based Design | [27_policy_based.md](04_cpp_centric/27_policy_based.md) | Template parameters |
| 28 | PIMPL | [28_pimpl.md](04_cpp_centric/28_pimpl.md) | Compilation firewall |
| 29 | Value Semantics | [29_value_semantics.md](04_cpp_centric/29_value_semantics.md) | Copy/move, Rule of Zero |
| 30 | Non-Virtual Interface | [30_nvi.md](04_cpp_centric/30_nvi.md) | Public non-virtual + private virtual |

---

## Quick Reference

### Compile-Time vs Runtime Polymorphism

```
+------------------+-------------------+-------------------+
|   Mechanism      |   Compile-Time    |     Runtime       |
+------------------+-------------------+-------------------+
| Dispatch         | Templates, CRTP   | virtual functions |
| Flexibility      | Fixed at compile  | Changeable        |
| Performance      | Zero overhead     | vtable indirection|
| Binary size      | Code bloat risk   | Smaller           |
| Error messages   | Complex           | Clear             |
+------------------+-------------------+-------------------+
```

### Ownership Quick Guide

```
+------------------+-------------------+-------------------+
|   Ownership      |   Smart Pointer   |   Use Case        |
+------------------+-------------------+-------------------+
| Exclusive        | unique_ptr        | Factory returns   |
| Shared           | shared_ptr        | Observer subjects |
| Non-owning       | raw ptr / ref     | Function params   |
| Weak reference   | weak_ptr          | Caches, observers |
+------------------+-------------------+-------------------+
```

---

## Summary & Decision Guide

See [99_summary.md](99_summary.md) for:
- Complete pattern comparison table
- "If you see X → use Y pattern" decision guide
- Must-master vs use-sparingly recommendations

---

## 学习顺序建议 (Recommended Learning Order)

### 第一阶段：基础模式 (Foundation)
1. **RAII** (#24) - C++ 的核心资源管理模式
2. **Value Semantics** (#29) - 理解 C++ 值语义
3. **Strategy** (#13) - 最常用的行为模式
4. **Factory Method** (#2) - 对象创建解耦

### 第二阶段：系统设计 (System Design)
5. **PIMPL** (#28) - 编译防火墙
6. **Observer** (#16) - 事件驱动架构
7. **Command** (#15) - 操作封装
8. **Decorator** (#9) - 功能组合

### 第三阶段：高级技术 (Advanced)
9. **Type Erasure** (#25) - 类型抹除
10. **CRTP** (#26) - 静态多态
11. **Policy-Based Design** (#27) - 策略组合
12. **Visitor** (#18) - 双重分派

### 第四阶段：按需学习 (On-Demand)
- Singleton, Builder, Adapter, Facade - 场景驱动
- Composite, Flyweight, Proxy - 结构优化
- State, Mediator, Memento - 复杂状态管理

