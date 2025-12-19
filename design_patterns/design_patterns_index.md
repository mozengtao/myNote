# Design Patterns Complete Reference (设计模式完整参考)

A comprehensive guide to 23 classic design patterns with English explanations,
Chinese details, ASCII diagrams, and Python code examples.

---

## Document Structure

This design patterns documentation is organized into four files:

| File | Patterns | Count |
|------|----------|-------|
| [design_patterns_creational.md](design_patterns_creational.md) | Creational Patterns (创建型模式) | 6 |
| [design_patterns_structural.md](design_patterns_structural.md) | Structural Patterns (结构型模式) | 7 |
| [design_patterns_behavioral_1.md](design_patterns_behavioral_1.md) | Behavioral Patterns Part 1 (行为型模式 第一部分) | 6 |
| [design_patterns_behavioral_2.md](design_patterns_behavioral_2.md) | Behavioral Patterns Part 2 (行为型模式 第二部分) | 5 |

---

## Quick Reference Table

### Creational Patterns (创建型模式)

| # | Pattern | 中文名称 | Purpose | File |
|---|---------|----------|---------|------|
| 1 | Singleton | 单例模式 | Ensure single instance | [Link](design_patterns_creational.md#1-singleton-pattern-单例模式) |
| 2 | Simple Factory | 简单工厂模式 | Centralized creation | [Link](design_patterns_creational.md#2-simple-factory-pattern-简单工厂模式) |
| 3 | Factory Method | 工厂方法模式 | Subclass decides creation | [Link](design_patterns_creational.md#3-factory-method-pattern-工厂方法模式) |
| 4 | Abstract Factory | 抽象工厂模式 | Create object families | [Link](design_patterns_creational.md#4-abstract-factory-pattern-抽象工厂模式) |
| 5 | Builder | 生成器模式 | Step-by-step construction | [Link](design_patterns_creational.md#5-builder-pattern-生成器模式) |
| 6 | Prototype | 原型模式 | Clone existing objects | [Link](design_patterns_creational.md#6-prototype-pattern-原型模式) |

### Structural Patterns (结构型模式)

| # | Pattern | 中文名称 | Purpose | File |
|---|---------|----------|---------|------|
| 7 | Adapter | 适配器模式 | Interface conversion | [Link](design_patterns_structural.md#1-adapter-pattern-适配器模式) |
| 8 | Bridge | 桥接模式 | Separate abstraction/implementation | [Link](design_patterns_structural.md#2-bridge-pattern-桥接模式) |
| 9 | Composite | 组合模式 | Tree structures | [Link](design_patterns_structural.md#3-composite-pattern-组合模式) |
| 10 | Decorator | 装饰器模式 | Add responsibilities dynamically | [Link](design_patterns_structural.md#4-decorator-pattern-装饰器模式) |
| 11 | Facade | 外观模式 | Simplified interface | [Link](design_patterns_structural.md#5-facade-pattern-外观模式) |
| 12 | Flyweight | 享元模式 | Share fine-grained objects | [Link](design_patterns_structural.md#6-flyweight-pattern-享元模式) |
| 13 | Proxy | 代理模式 | Control access | [Link](design_patterns_structural.md#7-proxy-pattern-代理模式) |

### Behavioral Patterns (行为型模式)

| # | Pattern | 中文名称 | Purpose | File |
|---|---------|----------|---------|------|
| 14 | Chain of Responsibility | 责任链模式 | Pass request along chain | [Link](design_patterns_behavioral_1.md#1-chain-of-responsibility-pattern-责任链模式) |
| 15 | Command | 命令模式 | Encapsulate request as object | [Link](design_patterns_behavioral_1.md#2-command-pattern-命令模式) |
| 16 | Iterator | 迭代器模式 | Sequential collection access | [Link](design_patterns_behavioral_1.md#3-iterator-pattern-迭代器模式) |
| 17 | Mediator | 中介者模式 | Centralize communication | [Link](design_patterns_behavioral_1.md#4-mediator-pattern-中介者模式) |
| 18 | Memento | 备忘录模式 | Save/restore object state | [Link](design_patterns_behavioral_1.md#5-memento-pattern-备忘录模式) |
| 19 | Observer | 观察者模式 | State change notification | [Link](design_patterns_behavioral_1.md#6-observer-pattern-观察者模式) |
| 20 | State | 状态模式 | Behavior varies with state | [Link](design_patterns_behavioral_2.md#7-state-pattern-状态模式) |
| 21 | Strategy | 策略模式 | Interchangeable algorithms | [Link](design_patterns_behavioral_2.md#8-strategy-pattern-策略模式) |
| 22 | Template Method | 模板方法模式 | Define algorithm skeleton | [Link](design_patterns_behavioral_2.md#9-template-method-pattern-模板方法模式) |
| 23 | Visitor | 访问者模式 | Operations on object structure | [Link](design_patterns_behavioral_2.md#10-visitor-pattern-访问者模式) |
| 24 | Interpreter | 解释器模式 | Language interpretation | [Link](design_patterns_behavioral_2.md#11-interpreter-pattern-解释器模式) |

---

## Pattern Categories Overview

```
+================================================================+
|                    DESIGN PATTERNS                              |
+================================================================+
|                                                                 |
|  CREATIONAL (创建型)        STRUCTURAL (结构型)                   |
|  How objects are created   How objects are composed            |
|  ----------------------    ---------------------               |
|  • Singleton               • Adapter                           |
|  • Simple Factory          • Bridge                            |
|  • Factory Method          • Composite                         |
|  • Abstract Factory        • Decorator                         |
|  • Builder                 • Facade                            |
|  • Prototype               • Flyweight                         |
|                            • Proxy                             |
|                                                                 |
|  BEHAVIORAL (行为型)                                             |
|  How objects interact and communicate                          |
|  ------------------------------------                          |
|  • Chain of Responsibility  • Observer                         |
|  • Command                  • State                            |
|  • Iterator                 • Strategy                         |
|  • Mediator                 • Template Method                  |
|  • Memento                  • Visitor                          |
|                             • Interpreter                      |
|                                                                 |
+================================================================+
```

**图解说明：**
- 创建型模式：关注对象的创建机制，提供创建对象的最佳方式
- 结构型模式：关注类和对象的组合，形成更大的结构
- 行为型模式：关注对象之间的职责分配和通信

---

## Pattern Selection Guide

### When to Use Each Pattern

| Situation | Recommended Pattern |
|-----------|-------------------|
| Need only one instance | **Singleton** |
| Need to create objects without specifying exact class | **Factory Method** |
| Need to create families of related objects | **Abstract Factory** |
| Need to build complex objects step by step | **Builder** |
| Need to clone existing objects | **Prototype** |
| Need to make incompatible interfaces work together | **Adapter** |
| Need to separate abstraction from implementation | **Bridge** |
| Need to treat individual objects and compositions uniformly | **Composite** |
| Need to add responsibilities dynamically | **Decorator** |
| Need to provide a simplified interface | **Facade** |
| Need to share objects to reduce memory | **Flyweight** |
| Need to control access to an object | **Proxy** |
| Need to pass requests along a chain | **Chain of Responsibility** |
| Need to encapsulate requests as objects | **Command** |
| Need to traverse a collection without exposing internals | **Iterator** |
| Need to reduce coupling between many objects | **Mediator** |
| Need to save and restore object state | **Memento** |
| Need to notify multiple objects of state changes | **Observer** |
| Need behavior to change based on state | **State** |
| Need to swap algorithms at runtime | **Strategy** |
| Need to define algorithm skeleton with customizable steps | **Template Method** |
| Need to add operations without changing elements | **Visitor** |
| Need to interpret a simple language | **Interpreter** |

---

## Document Format

Each pattern in this documentation includes:

1. **Pattern Name in English**
2. **One-line English explanation**
3. **Detailed Chinese explanation (中文详解)**
   - 适用场景 (Use cases)
   - 优点 (Advantages)
   - 缺点 (Disadvantages)
4. **ASCII Structure Diagram**
5. **Chinese diagram explanation (图解说明)**
6. **Complete Python code example**

---

## Related Resources

- [Boundaries and Contracts in C](boundaries_and_contracts_in_c.md) - Architecture patterns for C
- GoF "Design Patterns: Elements of Reusable Object-Oriented Software"
- "Head First Design Patterns" by Freeman & Robson

---

*Total: 24 patterns (including Simple Factory which is not part of GoF 23)*

*Document version: 1.0*
*Created: December 2024*

