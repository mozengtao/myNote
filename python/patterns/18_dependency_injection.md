# Dependency Injection Pattern in Python

---

## 1. Pattern Name: Dependency Injection (DI)

**Purpose / Problem Solved:**
A technique where an object receives its dependencies from external sources rather than creating them internally. Promotes loose coupling and testability.

---

## 2. ASCII Diagram

```
WITHOUT DI (tight coupling):
+------------------+
|    UserService   |
|------------------|
| __init__:        |
|   self.db = MySQL()  <-- Creates dependency
|   self.cache = Redis()
+------------------+

WITH DI (loose coupling):
+------------------+        +----------+
|    UserService   | <----- | Database | (injected)
|------------------|        +----------+
| __init__(db, cache): 
|   self.db = db          +----------+
|   self.cache = cache <--| Cache    | (injected)
+------------------+        +----------+

Injection Types:
1. Constructor Injection: __init__(dependency)
2. Setter Injection: service.set_db(db)
3. Interface Injection: implements interface that accepts dependency
```

**中文说明：**
依赖注入是一种将依赖项从外部传入对象的技术，而非让对象自己创建依赖。这实现了控制反转(IoC)——调用方控制依赖项，而非被调用方。优点：易于测试（可注入模拟对象）、松耦合、可配置。Python常用：构造函数注入、类型提示+工厂函数。

---

## 3. Python Module Example

```python
#!/usr/bin/env python3
"""Dependency Injection - Multiple Approaches"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from typing import Protocol


# ============== INTERFACES (Protocols) ==============

class Database(Protocol):
    def query(self, sql: str) -> list: ...
    def execute(self, sql: str) -> bool: ...


class Cache(Protocol):
    def get(self, key: str) -> str | None: ...
    def set(self, key: str, value: str) -> None: ...


class EmailSender(Protocol):
    def send(self, to: str, subject: str, body: str) -> bool: ...


# ============== IMPLEMENTATIONS ==============

class PostgresDB:
    def query(self, sql: str) -> list:
        return [{"id": 1, "name": "Alice"}]
    def execute(self, sql: str) -> bool:
        print(f"Postgres: {sql}")
        return True


class RedisCache:
    def __init__(self):
        self._data = {}
    def get(self, key: str) -> str | None:
        return self._data.get(key)
    def set(self, key: str, value: str):
        self._data[key] = value


class SMTPEmailSender:
    def send(self, to: str, subject: str, body: str) -> bool:
        print(f"SMTP: Sending '{subject}' to {to}")
        return True


# ============== MOCK IMPLEMENTATIONS (for testing) ==============

class MockDB:
    def query(self, sql: str) -> list:
        return [{"id": 999, "name": "Mock"}]
    def execute(self, sql: str) -> bool:
        return True


class MockCache:
    def get(self, key: str) -> str | None:
        return "cached"
    def set(self, key: str, value: str):
        pass


# ============== SERVICE WITH DI ==============

class UserService:
    """Service with constructor injection."""
    
    def __init__(self, db: Database, cache: Cache, email: EmailSender):
        self.db = db
        self.cache = cache
        self.email = email
    
    def get_user(self, user_id: int) -> dict:
        cached = self.cache.get(f"user:{user_id}")
        if cached:
            return {"cached": True, "name": cached}
        
        users = self.db.query(f"SELECT * FROM users WHERE id={user_id}")
        if users:
            self.cache.set(f"user:{user_id}", users[0]["name"])
            return users[0]
        return {}
    
    def create_user(self, name: str, email_addr: str) -> bool:
        self.db.execute(f"INSERT INTO users (name) VALUES ('{name}')")
        self.email.send(email_addr, "Welcome!", f"Hello {name}")
        return True


# ============== SIMPLE DI CONTAINER ==============

class Container:
    """Simple dependency injection container."""
    
    def __init__(self):
        self._singletons = {}
        self._factories = {}
    
    def register(self, interface, implementation, singleton=True):
        if singleton:
            self._singletons[interface] = None
            self._factories[interface] = implementation
        else:
            self._factories[interface] = implementation
    
    def resolve(self, interface):
        if interface in self._singletons:
            if self._singletons[interface] is None:
                factory = self._factories[interface]
                self._singletons[interface] = factory() if callable(factory) else factory
            return self._singletons[interface]
        
        factory = self._factories.get(interface)
        if factory:
            return factory() if callable(factory) else factory
        raise KeyError(f"No registration for {interface}")
    
    def build(self, cls):
        """Auto-resolve dependencies for a class."""
        import inspect
        sig = inspect.signature(cls.__init__)
        kwargs = {}
        for name, param in sig.parameters.items():
            if name == 'self':
                continue
            if param.annotation in self._factories:
                kwargs[name] = self.resolve(param.annotation)
        return cls(**kwargs)


# ============== USAGE ==============
if __name__ == "__main__":
    print("=== Manual Injection ===")
    
    # Production
    service = UserService(
        db=PostgresDB(),
        cache=RedisCache(),
        email=SMTPEmailSender()
    )
    print(f"User: {service.get_user(1)}")
    service.create_user("Bob", "bob@example.com")
    
    print("\n=== Testing with Mocks ===")
    
    test_service = UserService(
        db=MockDB(),
        cache=MockCache(),
        email=SMTPEmailSender()  # or MockEmailSender
    )
    print(f"Mock User: {test_service.get_user(1)}")
    
    print("\n=== Using Container ===")
    
    container = Container()
    container.register(Database, PostgresDB)
    container.register(Cache, RedisCache)
    container.register(EmailSender, SMTPEmailSender)
    
    # Auto-build with dependency resolution
    auto_service = container.build(UserService)
    print(f"Container User: {auto_service.get_user(1)}")
```

---

## 4. When to Use

**Use When:**
- Code should be testable (inject mocks)
- Want to swap implementations (e.g., databases)
- Following SOLID principles

**Python Idiom:**
Use constructor injection with type hints. For simple cases, default parameter values work well.

---

## 5. Related Patterns

| Pattern | Relationship |
|---------|--------------|
| **Factory** | Often creates dependencies to inject |
| **Strategy** | Injected strategies change behavior |
| **Service Locator** | Alternative (but often considered anti-pattern) |

