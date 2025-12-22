# Python Design Patterns - Complete Index

A comprehensive guide to design patterns in Python with runnable examples.

---

## Pattern Categories

### Creational Patterns
*Focus on object creation mechanisms*

| # | Pattern | Purpose | Python Feature |
|---|---------|---------|----------------|
| 01 | [Singleton](01_singleton.md) | Ensure single instance | `__new__`, module-level |
| 02 | [Factory Method](02_factory_method.md) | Defer instantiation to subclasses | Functions as factories |
| 03 | [Abstract Factory](03_abstract_factory.md) | Create families of related objects | Protocols, ABCs |
| 04 | [Builder](04_builder.md) | Step-by-step construction | Method chaining, dataclass |
| 05 | [Prototype](05_prototype.md) | Clone existing objects | `copy.deepcopy`, `replace()` |

### Structural Patterns
*Focus on object composition*

| # | Pattern | Purpose | Python Feature |
|---|---------|---------|----------------|
| 06 | [Adapter](06_adapter.md) | Convert interfaces | `__getattr__`, duck typing |
| 07 | [Decorator](07_decorator.md) | Add behavior dynamically | `@decorator` syntax |
| 11 | [Proxy](11_proxy.md) | Control access | `__getattr__`, lazy loading |
| 12 | [Facade](12_facade.md) | Simplify complex subsystems | Modules as facades |
| 16 | [Composite](16_composite.md) | Tree structures | Recursive iteration |

### Behavioral Patterns
*Focus on object communication*

| # | Pattern | Purpose | Python Feature |
|---|---------|---------|----------------|
| 08 | [Strategy](08_strategy.md) | Interchangeable algorithms | First-class functions |
| 09 | [Observer](09_observer.md) | Publish-subscribe | Callbacks, signals |
| 10 | [Command](10_command.md) | Encapsulate requests | Functions as commands |
| 13 | [Iterator](13_iterator.md) | Sequential access | `__iter__`, generators |
| 14 | [State](14_state.md) | State-dependent behavior | State classes |
| 15 | [Template Method](15_template_method.md) | Algorithm skeleton | ABCs, hooks |
| 17 | [Chain of Responsibility](17_chain_of_responsibility.md) | Pass along handlers | Middleware pattern |

### Additional Patterns

| # | Pattern | Purpose | Python Feature |
|---|---------|---------|----------------|
| 18 | [Dependency Injection](18_dependency_injection.md) | Loose coupling | Constructor params |

---

## Quick Reference: Python Idioms for Patterns

### Functions Over Classes
```python
# Many patterns don't need classes in Python
strategies = {
    "upper": str.upper,
    "lower": str.lower,
}
result = strategies["upper"]("hello")
```

### Decorators
```python
@lru_cache  # Built-in memoization (Flyweight-like)
@login_required  # Access control (Proxy-like)
@retry(3)  # Retry logic (Template Method-like)
```

### Generators
```python
# Iterator pattern in one line
def numbers(n):
    yield from range(n)
```

### Context Managers
```python
# Resource management (RAII pattern)
with open("file.txt") as f:
    data = f.read()
```

### Protocols
```python
# Duck typing with type hints
class Printable(Protocol):
    def __str__(self) -> str: ...
```

---

## Pattern Selection Guide

```
Need to create objects?
├── Single instance → Singleton
├── Factory returns different types → Factory Method
├── Family of related objects → Abstract Factory
├── Complex construction → Builder
└── Clone existing → Prototype

Need to compose objects?
├── Convert interface → Adapter
├── Add behavior → Decorator
├── Control access → Proxy
├── Simplify API → Facade
└── Tree structure → Composite

Need to manage behavior?
├── Swap algorithms → Strategy
├── React to changes → Observer
├── Queue/undo operations → Command
├── Traverse collection → Iterator
├── State-based behavior → State
├── Algorithm template → Template Method
└── Pass to handler → Chain of Responsibility
```

---

## How to Use These Files

1. **Read the pattern** - Understand purpose and diagram
2. **Study Python tips** - Learn which features make it natural
3. **Run the examples** - Each file is executable
4. **Apply to your code** - Look for matching problems

```bash
# Run any pattern example
python 01_singleton.md  # Won't work (markdown)
# Instead, copy the code block to a .py file or:
cd python_design_patterns
python -c "exec(open('01_singleton.md').read().split('```python')[1].split('```')[0])"
```

---

## Further Learning

- **Refactoring.Guru** - Visual pattern explanations
- **Python Patterns** - python-patterns.guide
- **Source Code Study** - Look at Flask, Django, SQLAlchemy
- **Practice** - Refactor existing code using patterns

---

## 中文概要

本目录包含Python设计模式的完整指南：

- **创建型模式**：单例、工厂、建造者、原型
- **结构型模式**：适配器、装饰器、代理、外观、组合
- **行为型模式**：策略、观察者、命令、迭代器、状态、模板方法、责任链

每个模式文件包含：
1. ASCII图解
2. 中文说明
3. Python语言特性对照
4. 可运行的代码示例
5. 适用场景和注意事项

